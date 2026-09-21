#include "diag/ScreenMirror.h"

#include <Logger.h>
#include <atomic>
#include <esp_heap_caps.h>
#include <string.h>

namespace {

uint16_t* s_buf = nullptr;
int32_t   s_w = 0;
int32_t   s_h = 0;

// Capture state. Plain atomic rather than a mutex: the flush path must never wait
// on a web request. See the header for the sequence.
enum : uint8_t { CAP_IDLE = 0, CAP_REQUESTED, CAP_ARMING, CAP_READY };
std::atomic<uint8_t> s_cap{CAP_IDLE};

// Rows copied since arming. Render task only — takeArmRequest() and blit() both
// run there — so it needs no synchronisation.
int32_t s_rowsCovered = 0;

}  // namespace

bool ScreenMirror::begin(int32_t width, int32_t height) {
    if (s_buf) return true;
    if (width <= 0 || height <= 0) return false;

    const size_t bytes = (size_t)width * (size_t)height * sizeof(uint16_t);
    s_buf = (uint16_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    if (!s_buf) {
        Logger.warning("ScreenMirror: %u byte PSRAM alloc failed - screenshots unavailable",
                       (unsigned)bytes);
        return false;
    }

    memset(s_buf, 0, bytes);
    s_w = width;
    s_h = height;
    Logger.info("ScreenMirror: %ldx%ld mirror ready (%u KB PSRAM)",
                (long)s_w, (long)s_h, (unsigned)(bytes / 1024));
    return true;
}

bool ScreenMirror::isReady()  { return s_buf != nullptr; }
int32_t ScreenMirror::width()  { return s_w; }
int32_t ScreenMirror::height() { return s_h; }
bool ScreenMirror::isCaptureReady() {
    return s_cap.load(std::memory_order_acquire) == CAP_READY;
}

bool ScreenMirror::isCaptureBusy() {
    return s_cap.load(std::memory_order_acquire) != CAP_IDLE;
}

bool ScreenMirror::requestCapture() {
    if (!s_buf) return false;
    // Only ever starts from Idle, so a second caller cannot steal a capture that
    // is already arming or ready.
    uint8_t expected = CAP_IDLE;
    return s_cap.compare_exchange_strong(expected, CAP_REQUESTED,
                                         std::memory_order_acq_rel,
                                         std::memory_order_acquire);
}

bool ScreenMirror::takeArmRequest() {
    if (s_cap.load(std::memory_order_acquire) != CAP_REQUESTED) return false;
    s_rowsCovered = 0;
    s_cap.store(CAP_ARMING, std::memory_order_release);
    return true;
}

void ScreenMirror::thaw() { s_cap.store(CAP_IDLE, std::memory_order_release); }

void ScreenMirror::blit(const lv_area_t* area, const uint8_t* pixels) {
    // The steady-state early-out: one relaxed load, no copy, no branch on the
    // geometry. Everything below runs only while a screenshot is arming.
    if (s_cap.load(std::memory_order_acquire) != CAP_ARMING) return;
    if (!s_buf || !area || !pixels) return;

    // Clip rather than trust the area: a rect off the edge would walk the heap.
    const int32_t x1 = area->x1 < 0 ? 0 : area->x1;
    const int32_t y1 = area->y1 < 0 ? 0 : area->y1;
    const int32_t x2 = area->x2 >= s_w ? s_w - 1 : area->x2;
    const int32_t y2 = area->y2 >= s_h ? s_h - 1 : area->y2;
    if (x2 < x1 || y2 < y1) return;

    // The source is the full flushed rect, so its stride is the rect's width even
    // when clipping trimmed what we copy.
    const int32_t srcStride = area->x2 - area->x1 + 1;
    const uint16_t* src = (const uint16_t*)pixels;
    const size_t runBytes = (size_t)(x2 - x1 + 1) * sizeof(uint16_t);

    for (int32_t y = y1; y <= y2; ++y) {
        const uint16_t* s = src + (size_t)(y - area->y1) * srcStride + (x1 - area->x1);
        memcpy(s_buf + (size_t)y * s_w + x1, s, runBytes);
    }

    // A full-screen invalidate emits every row exactly once, so counting rows is
    // enough to know the frame is complete — no per-row bookkeeping needed. It
    // also works if LVGL spreads the refresh over several handler calls.
    s_rowsCovered += (y2 - y1 + 1);
    if (s_rowsCovered >= s_h) {
        // Ready is also frozen: the guard at the top stops any further copying,
        // so what the encoder reads cannot be torn by the next refresh.
        s_cap.store(CAP_READY, std::memory_order_release);
    }
}

const uint16_t* ScreenMirror::row(int32_t y) {
    if (!s_buf || y < 0 || y >= s_h) return nullptr;
    return s_buf + (size_t)y * s_w;
}
