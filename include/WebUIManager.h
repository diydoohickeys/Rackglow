#pragma once

#include <Arduino.h>
#include <esp_http_server.h>
#include <ArduinoJson.h>

#include "WsHub.h"

/// WebSocket control channel and API endpoints, hung off Preflight's server.
class WebUIManager {
public:
    /// server is Preflight's shared instance — this does not own it.
    explicit WebUIManager(httpd_handle_t server);

    WebUIManager(const WebUIManager&) = delete;
    WebUIManager& operator=(const WebUIManager&) = delete;

    bool initialize();

    /// Pushes the current state to every connected client.
    void notifyClients();

    bool isInitialized() const { return initialized_; }

private:
    bool initialized_;
    httpd_handle_t server_;  // Preflight's, not owned
    WsHub wsHub_;

    void broadcast(const String& message);
    void handleWebSocketMessage(const char* text, size_t len);

    String generateAnimationsResponse();
    String generateStateResponse();

    void handleConnectMessage();
    void handleVuMessage(const JsonDocument& request);
    void handleWhiteMessage(const JsonDocument& request);
    void handleBrightnessMessage(const JsonDocument& request);
    void handleAnimationMessage(const JsonDocument& request);
    void handleColorMessage(const JsonDocument& request);
};

extern WebUIManager* g_webUIManager;

class OTAManager;
extern OTAManager* g_otaManager;

void updateWebUi();
