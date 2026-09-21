#pragma once

#include <lvgl.h>
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <memory>
#include "UiCommand.h"
#include "ui/ColourTab.h"
#include "ui/EffectsTab.h"
#include "ui/VuTab.h"
#include "ui/AudioVisualiser.h"
#include "ui/OtaScreen.h"

/// Owns the display, the tabview and the render task that drives LVGL.
class UIManager {
public:
    UIManager();
    ~UIManager();

    // Non-copyable and non-movable: a singleton owned via a global pointer in
    // main.cpp. A running render task captures `this`, so moving it would dangle.
    UIManager(const UIManager&) = delete;
    UIManager& operator=(const UIManager&) = delete;
    UIManager(UIManager&&) = delete;
    UIManager& operator=(UIManager&&) = delete;

    bool initializeScreen();
    bool initializeUI();
    void update();

    /**
     * Queues a UI mutation from another task (e.g. AsyncTCP web handlers).
     *
     * Non-blocking and safe from any task. Applied on the UI owner task inside
     * update(), and silently DROPPED if the queue is full — these are
     * latest-state controls, so losing a stale intermediate value is fine.
     */
    void postUiCommand(const UiCommand& cmd);

    /**
     * Spins up the render task as the sole owner of all lv_* calls.
     *
     * Pinned to core 1 above the Arduino loopTask so it preempts it. From here
     * on NOTHING else may call lv_*: the loop runs only the non-UI managers and
     * web handlers hand work over via postUiCommand(). Call once, after the UI
     * is initialised and all startup lv_* has completed. Normal mode only — in
     * setup mode the boot UI keeps driving LVGL on the loop and this never runs.
     */
    void startRenderTask();

    bool isRenderTaskRunning() const { return renderTaskHandle_ != nullptr; }

    void applyCurrentColor();
    void setVuState(bool newState);
    void setWhiteState(bool newState);
    void logAndUpdateVuState(bool newState);
    void logAndUpdateWhiteState(bool newState);
    void setAnimationState(bool newState);
    void setAnimation(int animation);

    /// Reflects LEDManager's NVS-loaded state. Call after the UI is initialised.
    void syncWithLEDState();

    bool isInitialized() const { return initialized_; }

    /**
     * Configures the idle screensaver and persists it to NVS.
     *
     * Safe from any task: it writes POD config + NVS, never lv_*. The render
     * task picks the values up on its next update() tick, including
     * live-dismissing the screensaver if it is disabled while showing.
     */
    void setScreensaverConfig(bool enabled, uint32_t idleMs);

    /**
     * Chooses which visual the screensaver shows, and persists it.
     *
     * 0 is the VU meter; 1..N are the procedural scenes. Swiping on the
     * screensaver changes the live visual too, but does NOT persist — only this.
     */
    void setScreensaverScene(int index);
    int getScreensaverScene() const { return screensaverScene_; }

    /// Names for the settings picker, in selection order.
    int screensaverSceneCount() const;
    const char* screensaverSceneName(int index) const;

    bool isScreensaverEnabled() const { return screensaverEnabled_; }
    uint32_t getScreensaverIdleMs() const { return screensaverIdleMs_; }

    /// Brings the Effects tab to the front (the Colour tab's effect bar).
    void showEffectsTab();

    void showOTAScreen();
    void updateOTAProgress(uint8_t progress);
    void hideOTAScreen();

    /// Report LVGL's pool usage. See the definition for why this is not optional.
    /// Public so boot can bracket a free and show the fragmentation it leaves.
    static void logLvglMemory(const char* when);

private:
    // Tab view components (each owns its widgets + layout) + the screensaver.
    std::unique_ptr<ColourTab> colourTab_;
    std::unique_ptr<EffectsTab> effectsTab_;
    std::unique_ptr<VuTab> vuTab_;
    std::unique_ptr<AudioVisualiser> audioVisualiser_;

    lv_obj_t* tabview_;
    lv_obj_t* tab1_;  // Colour
    lv_obj_t* tab2_;  // Effects
    lv_obj_t* tab3_;  // VU

    // Full-screen OTA progress overlay (owns its own lv_* objects + cross-task
    // flags; applied on the render task from update()).
    OtaScreen otaScreen_;

    bool initialized_;
    bool screenInitialized_;

    // Screensaver config, loaded from NVS in initializeComponents() and
    // updatable live from the web. volatile: written from the AsyncTCP task,
    // read on the render task in update() — aligned bool/uint32_t accesses are
    // atomic on ESP32, same pattern as the OTA flags.
    volatile bool screensaverEnabled_;
    volatile uint32_t screensaverIdleMs_;
    volatile int screensaverScene_ = 0;

    // Producers: web handlers on AsyncTCP. Consumer: update() on the UI owner task.
    QueueHandle_t uiCommandQueue_;

    // Sole lv_* owner once started; nullptr until startRenderTask().
    TaskHandle_t renderTaskHandle_;

    static void renderTaskTrampoline(void* arg);

    /// One-shot guard for the post-settle heap report in update().
    bool heapSettledLogged_ = false;

    void drainUiCommands();
    void applyUiCommand(const UiCommand& cmd);
    void applySynthTheme();
    bool createTabview();
    bool initializeComponents();
    void loadScreensaverConfig();
    void cleanup();
    static void scrollBeginEvent(lv_event_t* e);
};
