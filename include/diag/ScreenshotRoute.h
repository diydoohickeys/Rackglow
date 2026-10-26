#pragma once

#include <esp_http_server.h>

/**
 * @brief `GET /screenshot.png` — the current screen, as a PNG, for help docs.
 *
 * Serves straight from ScreenMirror, so it neither touches LVGL nor competes for
 * the panel bus. Encoding is streamed a batch of rows at a time (see PngStream).
 * The request holds the shared server task for the capture wait plus the encode
 * (a second or two); acceptable for a docs tool, and other requests just queue.
 */
namespace ScreenshotRoute {

void registerRoutes(httpd_handle_t server);

}  // namespace ScreenshotRoute
