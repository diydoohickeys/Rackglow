#include "diag/ScreenshotRoute.h"

#include "diag/PngStream.h"
#include "diag/ScreenMirror.h"

#include <Logger.h>
#include <memory>

namespace {

// Owns everything one screenshot needs, so the mirror is released no matter how
// the response ends. AsyncWebServer destroys the response — and with it the
// lambda holding this — on completion AND on a client disconnect, so the thaw in
// the destructor covers the aborted-download case too. Leaving the mirror held
// would silently stop the screenshot endpoint working ever again.
struct ScreenshotJob {
    PngStream png;
    uint32_t requestedMs = 0;
    bool streaming = false;
    ~ScreenshotJob() { ScreenMirror::thaw(); }
};

// How long to wait for the render task to deliver one complete frame before
// giving up. A capture can never complete while the screensaver is up (it renders
// through Display::pushStrip, bypassing the LVGL flush), so this has to end
// rather than retry forever.
constexpr uint32_t CAPTURE_TIMEOUT_MS = 3000;

}  // namespace

void ScreenshotRoute::registerRoutes(AsyncWebServer* server) {
    if (!server) return;

    server->on("/screenshot.png", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!ScreenMirror::isReady()) {
            request->send(503, "text/plain",
                          "Screen mirror unavailable (PSRAM allocation failed at boot)");
            return;
        }
        // One at a time: the mirror holds a single captured frame, and a second
        // reader would release it out from under the first when its job died.
        // requestCapture() only succeeds from Idle, so this IS the mutual
        // exclusion — not a check before it.
        if (!ScreenMirror::requestCapture()) {
            request->send(409, "text/plain", "A screenshot is already being served");
            return;
        }

        auto job = std::make_shared<ScreenshotJob>();
        job->requestedMs = millis();
        if (!job->png.begin()) {
            // begin() only allocates — it reads no rows — so it is safe to do here,
            // before the frame has landed. job dies and releases on the way out.
            request->send(503, "text/plain", "Out of PSRAM for the PNG encoder");
            return;
        }

        AsyncWebServerResponse* response = request->beginChunkedResponse(
            "image/png",
            [job](uint8_t* buffer, size_t maxLen, size_t /*index*/) -> size_t {
                // The render task has to paint a whole frame into the mirror first.
                // Never block here — this runs on the AsyncTCP task — so hand the
                // socket back and get asked again.
                if (!job->streaming) {
                    if (ScreenMirror::isCaptureReady()) {
                        job->streaming = true;
                    } else if (millis() - job->requestedMs < CAPTURE_TIMEOUT_MS) {
                        return RESPONSE_TRY_AGAIN;
                    } else {
                        // Ending the body early truncates the PNG, which is honest:
                        // a client gets a broken image rather than a plausible but
                        // stale one, and the reason is in the log.
                        Logger.warning("Screenshot: no frame within %u ms "
                                       "(screensaver up, or the render task is stalled)",
                                       (unsigned)CAPTURE_TIMEOUT_MS);
                        return 0;
                    }
                }

                const size_t n = job->png.read(buffer, maxLen);
                if (n == 0 && job->png.failed()) {
                    Logger.warning("Screenshot: PNG encode failed mid-stream");
                }
                return n;
            });

        response->addHeader("Cache-Control", "no-store");
        request->send(response);
    });

    Logger.debug("ScreenshotRoute: /screenshot.png registered");
}
