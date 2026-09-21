#include "ui/Display.h"
#include "diag/ScreenMirror.h"
#include "pins.h"

#include <lvgl.h>
#include <LovyanGFX.hpp>
#include <Arduino.h>
#include <atomic>

namespace {

// LovyanGFX device for the WT32-SC01 Plus (ST7796 8-bit parallel + FT5x06 touch).
// Pin map is SETTLED — do not change without a hardware reason.
class MyLGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7796 _panel_instance;
    lgfx::Bus_Parallel8 _bus_instance;
    lgfx::Light_PWM _light_instance;
    lgfx::Touch_FT5x06 _touch_instance;

public:
    MyLGFX(void) {
        {
            auto cfg = _bus_instance.config();
            cfg.freq_write = 40000000;
            cfg.pin_wr = pins::LCD_WR;
            cfg.pin_rd = pins::LCD_RD;
            cfg.pin_rs = pins::LCD_RS;
            cfg.pin_d0 = pins::LCD_D0;
            cfg.pin_d1 = pins::LCD_D1;
            cfg.pin_d2 = pins::LCD_D2;
            cfg.pin_d3 = pins::LCD_D3;
            cfg.pin_d4 = pins::LCD_D4;
            cfg.pin_d5 = pins::LCD_D5;
            cfg.pin_d6 = pins::LCD_D6;
            cfg.pin_d7 = pins::LCD_D7;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs = pins::LCD_CS;
            cfg.pin_rst = pins::LCD_RST;
            cfg.pin_busy = pins::LCD_BUSY;
            cfg.memory_width = 320;
            cfg.memory_height = 480;
            cfg.panel_width = 320;
            cfg.panel_height = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits = 1;
            cfg.readable = true;
            cfg.invert = true;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = true;

            _panel_instance.config(cfg);
        }

        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = pins::LCD_BL;
            cfg.invert = false;
            cfg.freq = 44100;
            cfg.pwm_channel = 7;

            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        {
            auto cfg = _touch_instance.config();
            cfg.i2c_port = 1;
            cfg.i2c_addr = 0x38;
            cfg.pin_sda = pins::TOUCH_SDA;
            cfg.pin_scl = pins::TOUCH_SCL;
            cfg.freq = 400000;
            cfg.x_min = 0;
            cfg.x_max = 320;
            cfg.y_min = 0;
            cfg.y_max = 480;

            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

        setPanel(&_panel_instance);
    }
};

MyLGFX lcd;

// LVGL display configuration
const uint16_t screenWidth = 320;
const uint16_t screenHeight = 480;

// 🚨 Sized in BYTES, and deliberately not an lv_color_t array. In LVGL 9
// `lv_color_t` is a 3-byte {blue,green,red} struct WHATEVER `LV_COLOR_DEPTH` says,
// while the render format here is RGB565 at 2 bytes/px — and
// lv_display_set_buffers takes a byte count. The old `lv_color_t buf[320*10]`
// therefore bought 15 rows, not the 10 it reads as. Size by rows x 2, never by
// element count.
// Rows are a redraw-cost lever: every band costs a startWrite + setAddrWindow
// command sequence on the 8-bit bus + endWrite + a flush_cb dispatch, so a
// full-screen repaint is ceil(480 / rows) bands — 32 at the original 15 rows,
// 12 here, 8 at 60.
// ⚠ They are also an INTERNAL RAM lever, and that is the binding constraint: this
// array competes with WiFi, AsyncTCP, MQTT and OTA. At 60 rows the settled free
// internal heap measured 30 KB, which holds in steady state and fails under load;
// 40 gives ~12.8 KB of it back for 4 extra bands. Check `internal heap free` in
// the "LVGL heap settled" line before raising it again.
// alignas(4) for LV_DRAW_BUF_ALIGN and the LovyanGFX DMA path.
constexpr int DRAW_BUF_ROWS = 40;
alignas(4) uint8_t buf[(size_t)screenWidth * DRAW_BUF_ROWS * 2];

// Bands pushed since the last read. Differenced by the render task's frame report
// so "bands per frame" is visible — which is the only direct evidence that a
// DRAW_BUF_ROWS change actually took effect.
uint32_t s_flushCount = 0;

// Last screen-touch timestamp, used to trigger the idle screensaver. Written
// in touchpadRead (render task, inside lv_timer_handler) and read by UIManager
// on the same task — no synchronisation needed.
uint32_t s_lastTouchMs = 0;

// Screen sleep. Written on the render task, read from the MQTT publisher on the loop.
std::atomic<bool> s_asleep{false};
// Set by a waking touch; cleared on release so the waking finger never presses a widget.
bool s_swallowTouch = false;
// The touch read runs inside lv_timer_handler, which wake()'s 120 ms SLPOUT wait
// would stall, so it only requests the wake; serviceWake() performs it before the next handler.
bool s_wakeRequested = false;

// Display flush callback
void displayFlush(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    lcd.startWrite();
    lcd.setAddrWindow(area->x1, area->y1, w, h);
    lcd.pushPixels((uint16_t*)px_map, w * h, true);
    lcd.endWrite();
    s_flushCount++;

    // Tee the same pixels into the PSRAM mirror — but ONLY while a screenshot is
    // armed. This used to run on every flush, which put a full-frame (300 KB)
    // PSRAM copy in the steady-state render path for a docs endpoint nobody was
    // calling. pushPixels() swaps for the panel and leaves px_map alone, so what
    // lands in the mirror is LVGL's native little-endian RGB565.
    ScreenMirror::blit(area, px_map);

    lv_display_flush_ready(disp);
}

// Touch read callback
void touchpadRead(lv_indev_t* indev_driver, lv_indev_data_t* data) {
    uint16_t touchX, touchY;
    bool touched = lcd.getTouch(&touchX, &touchY);

    if (touched && s_asleep) {
        s_wakeRequested = true;
        s_swallowTouch = true;
    }
    if (s_swallowTouch) {
        if (!touched) s_swallowTouch = false;
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    if (!touched) {
        data->state = LV_INDEV_STATE_RELEASED;
    } else {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = touchX;
        data->point.y = touchY;
        s_lastTouchMs = millis();  // any screen touch resets the idle timer
    }
}

}  // namespace

void Display::initPanel() {
    lcd.init();
    lcd.setRotation(2);
}

void Display::setupLvglDisplay() {
    // Best effort: without it /screenshot.png returns 503 and nothing else cares.
    ScreenMirror::begin(screenWidth, screenHeight);

    lv_display_t* disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, displayFlush);
}

void Display::setupLvglTouch() {
    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touchpadRead);
}

uint32_t Display::lastTouchMs() {
    return s_lastTouchMs;
}

uint32_t Display::takeFlushCount() {
    const uint32_t n = s_flushCount;
    s_flushCount = 0;
    return n;
}

// LovyanGFX's sleep()/wakeup() drop and restore the backlight around SLPIN/SLPOUT.
// The panel keeps accepting writes while asleep, so LVGL's GRAM stays current.
void Display::sleep() {
    if (s_asleep) return;
    lcd.sleep();
    s_asleep = true;
}

void Display::wake() {
    if (!s_asleep) return;
    lcd.wakeup();
    // ST7796: 120 ms after SLPOUT before the next command. Once per wake.
    vTaskDelay(pdMS_TO_TICKS(120));
    lv_obj_invalidate(lv_screen_active());
    s_lastTouchMs = millis();  // a fresh idle window, not an instant screensaver
    s_asleep = false;
}

bool Display::isAsleep() {
    return s_asleep;
}

void Display::serviceWake() {
    if (!s_wakeRequested) return;
    s_wakeRequested = false;
    wake();
}

void Display::beginDirect() {
    lcd.startWrite();
}

void Display::pushStrip(int32_t y, int32_t height, const uint16_t* pixels) {
    if (!pixels || height <= 0) return;
    lcd.setAddrWindow(0, y, screenWidth, height);
    // swap=true to match displayFlush(): the panel wants big-endian RGB565 and
    // scenes build pixels in native order, exactly like LVGL's buffer.
    lcd.pushPixels(const_cast<uint16_t*>(pixels), (uint32_t)screenWidth * height, true);

    // Feed the mirror here too. While a scene owns the panel LVGL never flushes,
    // so without this /screenshot.png returns whatever was on screen before the
    // screensaver started — which is exactly when you most want a screenshot.
    const lv_area_t area = { 0, y, (int32_t)screenWidth - 1, y + height - 1 };
    ScreenMirror::blit(&area, reinterpret_cast<const uint8_t*>(pixels));
}

void Display::endDirect() {
    lcd.endWrite();
}
