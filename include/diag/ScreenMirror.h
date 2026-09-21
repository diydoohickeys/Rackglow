#pragma once

#include <lvgl.h>
#include <stdint.h>

/**
 * @brief A full-screen RGB565 copy of what LVGL drew, held in PSRAM.
 *
 * LVGL runs in PARTIAL render mode here, so there is no framebuffer anywhere to
 * read a screenshot out of, and the ST7796's read-back would fight the render
 * task for the parallel bus. The mirror sidesteps both: flushed rects are copied
 * here, and reading it touches neither LVGL nor the panel — which is what lets
 * the screenshot route serve from the async web task.
 *
 * 🚨 Capture is ARMED, not continuous. Copying every flush unconditionally put a
 * full-frame (300 KB) PSRAM write in the steady-state render path, paid on every
 * animation frame, for an endpoint used occasionally. blit() now costs one atomic
 * load unless a capture is actually in progress.
 *
 * The sequence is a state machine, not a flag:
 *   requestCapture()   any task   Idle -> Requested
 *   takeArmRequest()   RENDER     Requested -> Arming; the caller then invalidates
 *   blit()             RENDER     copies + counts rows; full coverage -> Ready
 *   isCaptureReady()   any task   poll; Ready means frozen and safe to read
 *   thaw()             any task   -> Idle, releasing it for the next caller
 *
 * Ready IS frozen: blit() only copies while Arming, so a completed frame cannot be
 * torn by the next refresh. There is no separate freeze flag any more.
 *
 * ⚠ The screensaver renders procedurally through Display::pushStrip, bypassing the
 * LVGL flush entirely, so no capture can complete while it is up. That was already
 * true (a screenshot showed the last LVGL frame instead); it now surfaces as the
 * route's timeout rather than as silently stale pixels.
 */
namespace ScreenMirror {

/// Allocate the mirror. False if PSRAM is too fragmented — screenshots are then
/// unavailable and everything else carries on unaffected.
bool begin(int32_t width, int32_t height);

bool isReady();
int32_t width();
int32_t height();

/// Copy one flushed area in. Called from the LVGL flush callback (render task).
/// A no-op unless a capture is armed, and while the mirror isn't allocated.
void blit(const lv_area_t* area, const uint8_t* pixels);

/// Ask for one fresh full frame. Returns false if a capture is already in flight
/// (the caller should 409) or the mirror isn't allocated.
bool requestCapture();

/// RENDER TASK ONLY, immediately before lv_timer_handler(). Returns true exactly
/// once per request, meaning "invalidate the whole screen now".
bool takeArmRequest();

/// True once a complete frame has landed since requestCapture().
bool isCaptureReady();

/// True while a capture is requested, arming or ready — i.e. not available.
bool isCaptureBusy();

/// Release the mirror for the next caller. Safe to call in any state, which is
/// what lets the route's job destructor cover an aborted download.
void thaw();

/// Row accessor for the encoder. Only meaningful once isCaptureReady(). Null if
/// the row is out of range or the mirror isn't allocated.
const uint16_t* row(int32_t y);

}  // namespace ScreenMirror
