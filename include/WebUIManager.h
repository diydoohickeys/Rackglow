#pragma once

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>


#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoJson.h>

/// WebSocket control channel and API endpoints, hung off Preflight's server.
class WebUIManager {
public:
    /// webServer is Preflight's shared instance — this does not own it.
    WebUIManager(AsyncWebServer* webServer);
    ~WebUIManager();

    WebUIManager(const WebUIManager&) = delete;
    WebUIManager& operator=(const WebUIManager&) = delete;

    bool initialize();

    /// Call from the loop; reaps closed WebSocket clients.
    void update();

    /// Pushes the current state to every connected client.
    void notifyClients();

    bool isInitialized() const { return initialized_; }

    /// Shared with the Logger, which live-tails over the same socket.
    AsyncWebSocket* getWebSocket() { return &webSocket_; }

private:
    bool initialized_;
    AsyncWebServer* server_;  // Preflight's, not owned
    AsyncWebSocket webSocket_;

    void initializeWebSocket();

    void handleWebSocketMessage(void* arg, uint8_t* data, size_t len);
    void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                         AwsEventType type, void* arg, uint8_t* data, size_t len);

    String generateAnimationsResponse();
    String generateStateResponse();

    void handleConnectMessage();
    void handleVuMessage(const JsonDocument& request);
    void handleWhiteMessage(const JsonDocument& request);
    void handleBrightnessMessage(const JsonDocument& request);
    void handleAnimationMessage(const JsonDocument& request);
    void handleColorMessage(const JsonDocument& request);

    /// Static: AsyncWebSocket takes a C-style callback.
    static void staticWebSocketEventHandler(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                          AwsEventType type, void* arg, uint8_t* data, size_t len);
};

extern WebUIManager* g_webUIManager;

class OTAManager;
extern OTAManager* g_otaManager;

void updateWebUi();
