#include "diag/ScreenshotRoute.h"

#include "diag/PngStream.h"
#include "diag/ScreenMirror.h"

#include <Arduino.h>
#include <Logger.h>
#include <AccessGuard.h>
#include <HttpdRoute.h>
#include <esp_heap_caps.h>
#include <memory>

namespace {

// Owns everything one screenshot needs, so the mirror is released however the
// request ends — a client disconnect mid-stream included. Leaving the mirror held
// would silently stop the screenshot endpoint working ever again.
struct ScreenshotJob {
    PngStream png;
    ~ScreenshotJob() { ScreenMirror::thaw(); }
};

struct FreeDeleter {
    void operator()(uint8_t* p) const { heap_caps_free(p); }
};

// How long to wait for the render task to deliver one complete frame before
// giving up. A capture can never complete while the screensaver is up (it renders
// through Display::pushStrip, bypassing the LVGL flush), so this has to end
// rather than retry forever.
constexpr uint32_t CAPTURE_TIMEOUT_MS = 3000;
constexpr uint32_t CAPTURE_POLL_MS = 10;
constexpr size_t CHUNK_BYTES = 4096;

esp_err_t sendText(httpd_req_t* req, const char* status, const char* text) {
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_sendstr(req, text);
}

esp_err_t handleScreenshot(httpd_req_t* req) {
    if (!Access.allow(req)) {
        return ESP_OK;
    }
    if (!ScreenMirror::isReady()) {
        return sendText(req, "503 Service Unavailable",
                        "Screen mirror unavailable (PSRAM allocation failed at boot)");
    }
    // One at a time: the mirror holds a single captured frame, and a second
    // reader would release it out from under the first when its job died.
    // requestCapture() only succeeds from Idle, so this IS the mutual
    // exclusion — not a check before it.
    if (!ScreenMirror::requestCapture()) {
        return sendText(req, "409 Conflict", "A screenshot is already being served");
    }

    ScreenshotJob job;
    if (!job.png.begin()) {
        return sendText(req, "503 Service Unavailable", "Out of PSRAM for the PNG encoder");
    }

    // The render task has to paint a whole frame into the mirror first.
    const uint32_t requestedMs = millis();
    while (!ScreenMirror::isCaptureReady()) {
        if (millis() - requestedMs >= CAPTURE_TIMEOUT_MS) {
            Logger.warning("Screenshot: no frame within %u ms "
                           "(screensaver up, or the render task is stalled)",
                           (unsigned)CAPTURE_TIMEOUT_MS);
            return sendText(req, "504 Gateway Timeout", "No frame captured (is the screensaver up?)");
        }
        vTaskDelay(pdMS_TO_TICKS(CAPTURE_POLL_MS));
    }

    std::unique_ptr<uint8_t, FreeDeleter> chunk(static_cast<uint8_t*>(
        heap_caps_malloc(CHUNK_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
    if (!chunk) {
        return sendText(req, "503 Service Unavailable", "Out of memory");
    }

    httpd_resp_set_type(req, "image/png");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    for (;;) {
        const size_t n = job.png.read(chunk.get(), CHUNK_BYTES);
        if (n == 0) {
            break;
        }
        // A failed chunk means the client went away; stop, and let httpd close the socket.
        if (httpd_resp_send_chunk(req, reinterpret_cast<const char*>(chunk.get()), n) != ESP_OK) {
            return ESP_FAIL;
        }
    }
    if (job.png.failed()) {
        // Ending the body early truncates the PNG, which is honest: a client gets a
        // broken image rather than a plausible but stale one, and the reason is here.
        Logger.warning("Screenshot: PNG encode failed mid-stream");
        return ESP_FAIL;
    }
    return httpd_resp_send_chunk(req, nullptr, 0);
}

}  // namespace

void ScreenshotRoute::registerRoutes(httpd_handle_t server) {
    if (!server) return;
    const httpd_uri_t route = wifisetup::route("/screenshot.png", HTTP_GET, handleScreenshot, nullptr);
    httpd_register_uri_handler(server, &route);
}
