#include "WsHub.h"

#include <AccessGuard.h>
#include <HttpdRoute.h>
#include <memory>
#include <new>
#include <string>
#include <vector>

#if !CONFIG_HTTPD_WS_PRE_HANDSHAKE_CB_SUPPORT
#error "WsHub signs the WebSocket in before the upgrade: set CONFIG_HTTPD_WS_PRE_HANDSHAKE_CB_SUPPORT=y (custom_sdkconfig in platformio.ini)"
#endif

namespace {

struct BroadcastJob {
    httpd_handle_t server;
    std::string text;
};

// At least the server's max_open_sockets.
constexpr size_t MAX_CLIENTS = 16;
// The web UI's largest message is a colour or animation command; anything bigger is not ours.
constexpr size_t MAX_FRAME = 1024;

}  // namespace

bool WsHub::begin(httpd_handle_t server, MessageHandler onMessage) {
    if (!server) {
        return false;
    }
    server_ = server;
    onMessage_ = std::move(onMessage);

    httpd_uri_t ws = wifisetup::route("/ws", HTTP_GET, handleFrame, this);
    ws.is_websocket = true;
    ws.ws_pre_handshake_cb = authorizeHandshake;
    return httpd_register_uri_handler(server, &ws) == ESP_OK;
}

// Browsers don't apply same-origin to WebSockets, so without this any page could drive the LEDs.
// The session cookie is SameSite=Lax, so a cross-site page's handshake never carries it.
esp_err_t WsHub::authorizeHandshake(httpd_req_t* req) {
    return Access.isAuthorized(req) ? ESP_OK : ESP_FAIL;
}

esp_err_t WsHub::handleFrame(httpd_req_t* req) {
    if (req->method == HTTP_GET) {
        return ESP_OK;  // the upgrade itself, already authorised
    }
    auto* self = static_cast<WsHub*>(req->user_ctx);

    httpd_ws_frame_t frame = {};
    esp_err_t err = httpd_ws_recv_frame(req, &frame, 0);
    if (err != ESP_OK || frame.len == 0) {
        return err;
    }
    if (frame.len > MAX_FRAME) {
        return ESP_FAIL;  // closes the socket: the unread payload would desync the stream
    }

    std::vector<uint8_t> payload(frame.len + 1);
    frame.payload = payload.data();
    err = httpd_ws_recv_frame(req, &frame, frame.len);
    if (err != ESP_OK) {
        return err;
    }
    payload[frame.len] = 0;

    if (frame.type == HTTPD_WS_TYPE_TEXT && self && self->onMessage_) {
        self->onMessage_(reinterpret_cast<const char*>(payload.data()), frame.len);
    }
    return ESP_OK;
}

void WsHub::broadcast(const char* text, size_t len) {
    if (!server_ || !text) {
        return;
    }
    std::unique_ptr<BroadcastJob> job(new (std::nothrow) BroadcastJob());
    if (!job) {
        return;
    }
    try {
        job->server = server_;
        job->text.assign(text, len);
    } catch (...) {
        return;
    }
    if (httpd_queue_work(server_, sendToAll, job.get()) == ESP_OK) {
        job.release();  // sendToAll owns it now
    }
}

// On the server task. Logs nothing: a log line here would be broadcast too, and a failing send
// would feed itself.
void WsHub::sendToAll(void* arg) {
    std::unique_ptr<BroadcastJob> job(static_cast<BroadcastJob*>(arg));

    size_t count = MAX_CLIENTS;
    int fds[MAX_CLIENTS];
    if (httpd_get_client_list(job->server, &count, fds) != ESP_OK) {
        return;
    }

    httpd_ws_frame_t frame = {};
    frame.type = HTTPD_WS_TYPE_TEXT;
    frame.payload = reinterpret_cast<uint8_t*>(&job->text[0]);
    frame.len = job->text.size();
    for (size_t i = 0; i < count; i++) {
        if (httpd_ws_get_fd_info(job->server, fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET) {
            httpd_ws_send_frame_async(job->server, fds[i], &frame);
        }
    }
}
