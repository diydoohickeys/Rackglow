#pragma once

#include <stdint.h>

/**
 * @brief Board display + touch hardware glue, wired to LVGL.
 *
 * The physical panel is a WT32-SC01 Plus: an ST7796 320x480 driven over an
 * 8-bit 8080 parallel bus, with an FT5x06 capacitive touch, all via LovyanGFX.
 * This module owns the LovyanGFX device (pin map, bus/panel/backlight/touch
 * config) and the two LVGL callbacks (flush + touch read). It is split out of
 * UIManager so the manager isn't carrying ~120 lines of board pin configuration.
 *
 * Pin map lives in Display.cpp (see pins.h once item 8 lands). Pins are SETTLED.
 */
namespace Display {

/// Initialise the LCD panel hardware. Call once, before lv_init().
void initPanel();

/// Create the LVGL display object (render buffer + flush callback).
void setupLvglDisplay();

/// Create the LVGL pointer input device (touch read callback).
void setupLvglTouch();

/// millis() timestamp of the last screen touch (drives the idle screensaver).
uint32_t lastTouchMs();

/// Bands pushed to the panel since the last call, and zero the counter. Render
/// task only. Differenced against the frame count, it gives bands-per-frame.
uint32_t takeFlushCount();

/**
 * Screen sleep: backlight off + panel SLPIN, independent of the screensaver.
 *
 * sleep()/wake()/serviceWake() are render-task only and run OUTSIDE
 * lv_timer_handler (wake blocks 120 ms for the panel). A touch while asleep requests a wake that
 * serviceWake() performs, and that touch is swallowed until the finger lifts
 * so it never lands on a widget the user could not see. isAsleep() is safe
 * from any task.
 */
void sleep();
void wake();
bool isAsleep();

/// Performs a wake requested by a touch. Call each render tick before lv_timer_handler.
void serviceWake();

/**
 * Direct panel access for the screensaver, which renders procedurally and
 * pushes strips itself instead of going through LVGL (see ui/saver/ScenePlayer.h
 * for why). Wrapped here rather than exposing the LovyanGFX device, so the panel
 * stays owned by this module.
 *
 * Only legal from the render task, which is the only task that talks to the
 * panel. Bracket pushes with beginDirect()/endDirect() so the bus transaction is
 * held open across the whole frame.
 */
void beginDirect();
void pushStrip(int32_t y, int32_t height, const uint16_t* pixels);
void endDirect();

}  // namespace Display
