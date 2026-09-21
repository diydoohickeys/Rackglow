#pragma once

#include <memory>
#include <stdint.h>
#include "ui/saver/IScene.h"

/**
 * @brief Drives a screensaver scene straight to the panel, bypassing LVGL.
 *
 * While a scene is running this owns the display: it renders one strip at a time
 * into a ~10 KB internal-RAM buffer and pushes each strip over the parallel bus
 * itself. LVGL keeps running on the same task so touch still works and a tap
 * still dismisses — it simply isn't asked to repaint, and the opaque overlay it
 * drew once underneath is what we paint over.
 *
 * 🚨 This is a deliberate departure from "LVGL owns the display", and it is only
 * safe because a screensaver is a full-screen modal state on the render task,
 * which is also the only task that talks to the panel. If LVGL does repaint for
 * some other reason the next frame simply overwrites it. On stop() the screen is
 * invalidated so LVGL redraws the real UI.
 *
 * Holds a list of scenes so a picker (or a swipe) is a small step from here:
 * add a class, add it in begin(), and next() already cycles.
 */
class ScenePlayer {
public:
    ~ScenePlayer();

    /// Register the scenes. Cheap — nothing is allocated until start().
    bool begin();

    /// Allocate the strip buffer and start the current scene.
    bool start();
    /// Free the strip buffer and hand the screen back to LVGL.
    void stop();

    /**
     * Advance the current frame by one batch of strips. Call from the render task
     * while active.
     *
     * 🚨 Deliberately does NOT paint a whole frame per call. It used to, and a
     * full-screen procedural render plus a ~300 KB bus push is tens of ms during
     * which this task — priority 2 on core 1 — never yields. `LEDManager::update()`
     * runs on loopTask at priority 1 on the SAME core, so it got only what was
     * left, and the strips visibly slowed whenever a scene screensaver was up.
     * A batch per call hands the render task's own vTaskDelay back to loopTask
     * several times per frame instead of once.
     */
    void update();

    /// Move to the next scene, wrapping. Safe while running.
    void next();

    const char* currentName() const;
    bool isRunning() const { return running_; }

    /// The registered scenes, for the settings picker.
    int count() const { return sceneCount_; }
    const char* nameAt(int index) const;
    int current() const { return current_; }
    /// Select by index. Restarts the scene if one is running.
    void setCurrent(int index);

private:
    /// Rows per push. 320x16x2 = 10 KB — small enough for internal RAM, big
    /// enough that per-strip overhead disappears against the pixel work.
    static constexpr int32_t STRIP_ROWS = 16;
    /// Fallback pacing when a scene doesn't ask for its own; the render task
    /// ticks at 200 Hz, so this paces it down. See IScene::frameIntervalMs().
    static constexpr uint32_t FRAME_MS = 33;
    /// Strips painted per update() call. The whole point is the gap BETWEEN
    /// calls, so this trades scene frame rate against how often the LED task
    /// gets the core — raise it for a smoother screensaver, lower it for
    /// smoother strips. 30 strips per screen at STRIP_ROWS=16.
    static constexpr int32_t STRIPS_PER_CALL = 5;
    static constexpr int MAX_SCENES = 6;   // headroom; each slot is a pointer

    void startCurrentScene();

    std::unique_ptr<IScene> scenes_[MAX_SCENES];
    int sceneCount_ = 0;
    int current_ = 0;

    uint16_t* strip_ = nullptr;
    uint32_t lastFrameMs_ = 0;
    int32_t width_ = 0;
    int32_t height_ = 0;
    /// Next row to paint. 0 means "between frames" — the only point at which the
    /// frame-interval gate applies and the scene is allowed to advance, so a
    /// sweep always paints ONE coherent moment however many calls it spans.
    int32_t cursorY_ = 0;
    bool running_ = false;
};
