#pragma once

#include <Arduino.h>
#include "led/LedHelpers.h"
#include "led/LedDriver.h"
#include "led/Animations.h"
#include "led/AnimationEngine.h"
#include "audio/AudioFrame.h"
#include <Preferences.h>
#include <atomic>

/**
 * Coordinates LED configuration, control state and rendering.
 *
 * Owns the pixel buffer + the RMT/DMA driver, loads the strip config and the
 * persisted control state (brightness / mode / colour / animation), and drives
 * the AnimationEngine each tick. The animation *implementations* live in
 * AnimationEngine; the animation *vocabulary* (AnimationType + labels) lives in
 * Animations.h. This class is the thin coordinator over those pieces.
 */
class LEDManager {
public:
    LEDManager();
    ~LEDManager();

    LEDManager(const LEDManager&) = delete;
    LEDManager& operator=(const LEDManager&) = delete;

    bool initialize();
    void update();

    /// Strip frames pushed since the last call, and zero the counter. Callable
    /// from any task. Diagnostic only: it counts the steady-state path in
    /// update(), not the OTA or error-flash transients. Reported as LED fps
    /// beside the render stats — see .claude/rules/testing.md.
    uint32_t takeShowCount() { return showCount_.exchange(0, std::memory_order_relaxed); }

    void setBrightness(uint8_t newBrightness);

    /**
     * Blocking boot animation: a diagonal rainbow-field "dispersion wipe"
     * (top-left to bottom-right across the strip grid) that then cross-fades
     * from the final frame into the saved state. Runs once in setup(), before
     * the render task takes over LVGL.
     */
    void performStartupDispersion();

    void showOTAProgress(uint8_t progress);
    uint8_t getBrightness() const { return brightness_; }
    void setAnimationEnabled(bool enabled);

    /// While in OTA mode update() is a no-op, so the progress display is not
    /// fought by the normal brightness tick.
    void setOTAMode(bool enabled);

    /// Brief non-blocking red flash (OTA failure feedback). Rendered over the
    /// next ~1.2 s by update(); safe to call from any task.
    void flashError();

    bool isAnimationEnabled() const { return showAnimation_; }
    void setCurrentAnimation(AnimationType animation);
    AnimationType getCurrentAnimation() const { return currentAnimation_; }
    void setVuMode(bool enabled);
    bool isVuModeEnabled() const { return vuMode_; }
    void setWhiteMode(bool enabled);
    bool isWhiteModeEnabled() const { return whiteMode_; }
    void setSolidColor(CRGB color);
    CRGB getSolidColor() const { return solidColor_; }

    /// Immediate, and does NOT change stored state.
    void fillColor(CRGB color);

    /// Starts the save debounce; the write happens in update().
    void markStateDirty();

    bool hasLoadedState() const { return stateLoaded_; }
    void clearSavedState();
    void fillWhite();

    int getNumStrips() const;
    int getLedsPerStrip() const;
    int getTotalLeds() const;
    bool isConfigValid() const;

private:
    // Incremented on loopTask, read+zeroed from the render task's report.
    std::atomic<uint32_t> showCount_{0};

    CRGB* leds_;
    int numStrips_;
    int ledsPerStrip_;
    int totalLeds_;
    bool configLoaded_;
    bool initialized_;

    uint8_t brightness_;
    bool showAnimation_;
    bool vuMode_;
    bool whiteMode_;
    AnimationType currentAnimation_;
    CRGB solidColor_;

    bool stateDirty_;
    bool stateLoaded_;
    unsigned long stateChangedTime_;
    static const unsigned long STATE_SAVE_DEBOUNCE_MS = 5000;

    /// Overall level, used for VU-mode brightness.
    int audioLevel_;

    uint8_t lastOTAProgress_;
    // While true, update() is suspended so showOTAProgress() can own the strips
    // without the normal brightness/animation tick fighting it.
    bool otaMode_;
    // Non-zero while a failure flash is active: millis() deadline to flash until.
    unsigned long errorFlashUntilMs_;

    Preferences preferences_;
    unsigned long lastAnimationUpdate_;

    LedDriver driver_;
    AnimationEngine engine_;

    void loadConfiguration();
    void loadState();
    void saveState();
    void saveStateIfNeeded();
    bool allocateLedArrays();
    void deallocateLedArrays();
    void updateBrightness();
};

extern LEDManager* g_ledManager;
