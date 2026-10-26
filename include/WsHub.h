#pragma once

#include <esp_http_server.h>
#include <functional>
#include <stddef.h>

/// The /ws control channel on Preflight's server (esp_http_server, HTTPS when enabled).
///
/// A client is admitted only when signed in, checked before the upgrade in the pre-handshake
/// hook. That makes every open WebSocket on the server an authorised one, so a broadcast walks
/// the server's own client list instead of keeping a second one that could drift from it.
class WsHub {
public:
    /// Runs on the httpd task: queue app work, never block (see WebUIManager's handlers).
    using MessageHandler = std::function<void(const char* text, size_t len)>;

    bool begin(httpd_handle_t server, MessageHandler onMessage);

    /// Callable from any task: the payload is copied and the send runs on the server task.
    /// Dropped silently if httpd's work queue is full, like a slow client missing a frame.
    void broadcast(const char* text, size_t len);

private:
    httpd_handle_t server_ = nullptr;
    MessageHandler onMessage_;

    static esp_err_t authorizeHandshake(httpd_req_t* req);
    static esp_err_t handleFrame(httpd_req_t* req);
    static void sendToAll(void* job);
};
