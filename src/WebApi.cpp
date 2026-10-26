#include "WebApi.h"

#include <WiFi.h>
#include <Preferences.h>
#include <Logger.h>
#include <AccessGuard.h>
#include <FormBody.h>
#include <HttpdRoute.h>
#include <SameOrigin.h>
#include "WebAssets.h"
#include "diag/ScreenshotRoute.h"
#include "led/LEDManager.h"
#include "ui/UIManager.h"
#include "ui/ui.h"
#include "MqttManager.h"
#include <string>

// Defined in main.cpp — set true to schedule a deferred restart from a handler
// (the loop performs the actual ESP.restart() after a short delay).
extern bool g_restartRequested;

// Minimal JSON string escaper for user-entered values (broker host/user/password).
static String jsonEscape(const String& in) {
    String out;
    out.reserve(in.length() + 4);
    for (size_t i = 0; i < in.length(); i++) {
        char c = in[i];
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c == '\t') {
            out += "\\t";
        } else {
            out += c;
        }
    }
    return out;
}

namespace {

esp_err_t sendJson(httpd_req_t* req, const char* status, const String& json) {
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json.c_str(), json.length());
}

// Streams one embedded asset straight from flash — nothing is copied into RAM.
esp_err_t sendAsset(httpd_req_t* req, const WebAssets::Asset* asset) {
    if (!asset) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
        return ESP_OK;
    }
    httpd_resp_set_type(req, asset->contentType);
    if (asset->gzip) {
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    }
    return httpd_resp_send(req, reinterpret_cast<const char*>(asset->data), asset->len);
}

// Every POST below reads a form body; refusals before it is read return ESP_FAIL so httpd
// closes the socket instead of draining it.
bool admitPost(httpd_req_t* req, std::string& body) {
    if (!wifisetup::isSameOrigin(req)) {
        wifisetup::sendCrossOriginRefused(req);
        return false;
    }
    return Access.allow(req) && wifisetup::readFormBody(req, body);
}

bool formHas(const std::string& body, const char* key) {
    const std::string prefix = std::string(key) + "=";
    return body.compare(0, prefix.size(), prefix) == 0 || body.find("&" + prefix) != std::string::npos;
}

String formString(const std::string& body, const char* key) {
    return String(wifisetup::formValue(body, key).c_str());
}

bool formBool(const std::string& body, const char* key) {
    const std::string v = wifisetup::formValue(body, key);
    return v == "true" || v == "1" || v == "on";
}

esp_err_t handleTest(httpd_req_t* req) {
    if (!Access.allow(req)) return ESP_OK;
    const String text = "Server is working! IP: " + WiFi.localIP().toString();
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, text.c_str(), text.length());
}

// LED Configuration page — served by the SPA. We send index.html and let the
// client-side router show the LedConfig view based on window.location.pathname.
esp_err_t handleLedConfigPage(httpd_req_t* req) {
    if (!Access.allow(req)) return ESP_OK;
    return sendAsset(req, WebAssets::find("/index.html"));
}

esp_err_t handleGetLedConfig(httpd_req_t* req) {
    if (!Access.allow(req)) return ESP_OK;
    Preferences ledPrefs;
    ledPrefs.begin("led-config", true); // Read-only

    bool hasLedSettings = ledPrefs.isKey("num_strips");
    int numStrips = ledPrefs.getInt("num_strips", 0);
    int ledsPerStrip = ledPrefs.getInt("leds_per_strip", 0);
    int totalLeds = numStrips * ledsPerStrip;

    ledPrefs.end();

    String json = "{";
    json += "\"hasLedSettings\":" + String(hasLedSettings ? "true" : "false") + ",";
    json += "\"numStrips\":" + String(numStrips) + ",";
    json += "\"ledsPerStrip\":" + String(ledsPerStrip) + ",";
    json += "\"totalLeds\":" + String(totalLeds);
    json += "}";

    return sendJson(req, "200 OK", json);
}

esp_err_t handleSaveLedConfig(httpd_req_t* req) {
    std::string body;
    if (!admitPost(req, body)) return ESP_FAIL;

    const int numStrips = formString(body, "num_strips").toInt();
    const int ledsPerStrip = formString(body, "leds_per_strip").toInt();

    if (numStrips <= 0 || ledsPerStrip <= 0) {
        return sendJson(req, "400 Bad Request", "{\"ok\":false,\"error\":\"Invalid LED configuration\"}");
    }

    Preferences ledPrefs;
    ledPrefs.begin("led-config", false);
    ledPrefs.putInt("num_strips", numStrips);
    ledPrefs.putInt("leds_per_strip", ledsPerStrip);
    ledPrefs.end();

    Logger.info("Saved LED config: %d strips, %d LEDs per strip", numStrips, ledsPerStrip);

    const esp_err_t err = sendJson(req, "200 OK", "{\"ok\":true}");
    // Schedule restart — the web client shows the "Restarting..." view.
    g_restartRequested = true;
    return err;
}

// Settings: idle screensaver (enable + idle timeout). Applied live, no restart.
esp_err_t handleGetSettings(httpd_req_t* req) {
    if (!Access.allow(req)) return ESP_OK;
    bool enabled = g_uiManager ? g_uiManager->isScreensaverEnabled() : true;
    uint32_t idleSec = g_uiManager ? g_uiManager->getScreensaverIdleMs() / 1000 : 30;

    // MQTT / Home Assistant broker config (defaults when the manager isn't up,
    // e.g. captive-portal setup mode).
    bool mqttEnabled = g_mqttManager ? g_mqttManager->isEnabled() : false;
    String mqttHost = g_mqttManager ? g_mqttManager->host() : String("homeassistant.local");
    uint16_t mqttPort = g_mqttManager ? g_mqttManager->port() : 1883;
    bool mqttTls = g_mqttManager ? g_mqttManager->tls() : false;
    String mqttUser = g_mqttManager ? g_mqttManager->user() : String("");
    String mqttPrefix = g_mqttManager ? g_mqttManager->prefix() : String("rackglow");
    // Password is write-only: never sent back. Expose only whether one is set so
    // the UI can show a "leave blank to keep" affordance.
    bool mqttHasPass = g_mqttManager ? (g_mqttManager->pass().length() > 0) : false;

    // Scene list comes from the device so the picker can't drift out of step
    // with what is actually registered in ScenePlayer.
    String scenes = "[";
    const int sceneCount = g_uiManager ? g_uiManager->screensaverSceneCount() : 0;
    for (int i = 0; i < sceneCount; i++) {
        if (i) scenes += ",";
        scenes += "\"" + jsonEscape(g_uiManager->screensaverSceneName(i)) + "\"";
    }
    scenes += "]";
    const int sceneSel = g_uiManager ? g_uiManager->getScreensaverScene() : 0;

    String json = "{";
    json += "\"screensaverEnabled\":" + String(enabled ? "true" : "false") + ",";
    json += "\"screensaverTimeoutSec\":" + String(idleSec) + ",";
    json += "\"screensaverScene\":" + String(sceneSel) + ",";
    json += "\"screensaverScenes\":" + scenes + ",";
    json += "\"mqttEnabled\":" + String(mqttEnabled ? "true" : "false") + ",";
    json += "\"mqttHost\":\"" + jsonEscape(mqttHost) + "\",";
    json += "\"mqttPort\":" + String(mqttPort) + ",";
    json += "\"mqttTls\":" + String(mqttTls ? "true" : "false") + ",";
    json += "\"mqttUser\":\"" + jsonEscape(mqttUser) + "\",";
    json += "\"mqttHasPass\":" + String(mqttHasPass ? "true" : "false") + ",";
    json += "\"mqttPrefix\":\"" + jsonEscape(mqttPrefix) + "\"";
    json += "}";

    return sendJson(req, "200 OK", json);
}

esp_err_t handleSaveSettings(httpd_req_t* req) {
    std::string body;
    if (!admitPost(req, body)) return ESP_FAIL;

    if (!g_uiManager) {
        return sendJson(req, "503 Service Unavailable", "{\"ok\":false,\"error\":\"UI not ready\"}");
    }

    // Default to the current values so a partial form doesn't clobber them.
    bool enabled = g_uiManager->isScreensaverEnabled();
    uint32_t idleSec = g_uiManager->getScreensaverIdleMs() / 1000;

    if (formHas(body, "screensaver_enabled")) {
        enabled = formBool(body, "screensaver_enabled");
    }
    if (formHas(body, "screensaver_timeout_sec")) {
        idleSec = formString(body, "screensaver_timeout_sec").toInt();
    }

    g_uiManager->setScreensaverConfig(enabled, idleSec * 1000);

    if (formHas(body, "screensaver_scene")) {
        g_uiManager->setScreensaverScene(formString(body, "screensaver_scene").toInt());
    }

    // MQTT / Home Assistant broker config (applied live via a deferred restart
    // inside MqttManager). Only touched when the form submits the mqtt fields.
    if (g_mqttManager && formHas(body, "mqtt_enabled")) {
        const bool mqttEnabled = formBool(body, "mqtt_enabled");

        String host = g_mqttManager->host();
        uint16_t port = g_mqttManager->port();
        bool tls = g_mqttManager->tls();
        String user = g_mqttManager->user();
        String pass = g_mqttManager->pass();
        String prefix = g_mqttManager->prefix();

        if (formHas(body, "mqtt_host")) host = formString(body, "mqtt_host");
        if (formHas(body, "mqtt_port")) port = formString(body, "mqtt_port").toInt();
        if (formHas(body, "mqtt_tls")) tls = formBool(body, "mqtt_tls");
        if (formHas(body, "mqtt_user")) user = formString(body, "mqtt_user");
        if (formHas(body, "mqtt_prefix")) prefix = formString(body, "mqtt_prefix");
        // Password is write-only: a blank field means "keep the stored one", so we
        // only overwrite when the user actually typed a new password.
        const String typed = formString(body, "mqtt_pass");
        if (typed.length() > 0) pass = typed;

        g_mqttManager->applyConfig(mqttEnabled, host, port, tls, user, pass, prefix);
    }

    return sendJson(req, "200 OK", "{\"ok\":true}");
}

// Clear saved LED state (for testing first-boot experience)
esp_err_t handleClearLedState(httpd_req_t* req) {
    if (!wifisetup::isSameOrigin(req)) {
        return wifisetup::sendCrossOriginRefused(req);
    }
    if (!Access.allow(req)) return ESP_OK;
    if (g_ledManager) {
        g_ledManager->clearSavedState();
    }

    Logger.info("LED state preferences cleared via web UI");
    return sendJson(req, "200 OK", "{\"ok\":true}");
}

// Legacy get-message endpoint (mostly unused)
esp_err_t handleGetMessage(httpd_req_t* req) {
    if (!Access.allow(req)) return ESP_OK;
    return sendJson(req, "200 OK", "");
}

esp_err_t handleAsset(httpd_req_t* req) {
    const auto* asset = static_cast<const WebAssets::Asset*>(req->user_ctx);
    // The page itself signs in first; its scripts, styles and icons stay open.
    if (strncmp(asset->contentType, "text/html", 9) == 0 && !Access.allow(req)) {
        return ESP_OK;
    }
    return sendAsset(req, asset);
}

}  // namespace

void WebApi::registerRoutes(httpd_handle_t server) {
    Logger.debug("=== WebApi: Setting up routes ===");

    // NOTE: WiFi setup routes (/setup, /factory-reset, /get-networks, /save-wifi)
    // are handled by the WiFiSetupManager library, not here.
    const httpd_uri_t routes[] = {
        wifisetup::route("/test", HTTP_GET, handleTest, nullptr),
        wifisetup::route("/led-config", HTTP_GET, handleLedConfigPage, nullptr),
        wifisetup::route("/get-led-config", HTTP_GET, handleGetLedConfig, nullptr),
        wifisetup::route("/save-led-config", HTTP_POST, handleSaveLedConfig, nullptr),
        wifisetup::route("/get-settings", HTTP_GET, handleGetSettings, nullptr),
        wifisetup::route("/save-settings", HTTP_POST, handleSaveSettings, nullptr),
        wifisetup::route("/clear-led-state", HTTP_POST, handleClearLedState, nullptr),
        wifisetup::route("/get-message", HTTP_GET, handleGetMessage, nullptr),
    };
    for (const httpd_uri_t& r : routes) {
        if (httpd_register_uri_handler(server, &r) != ESP_OK) {
            Logger.error("WebApi: could not register %s", r.uri);
        }
    }

    ScreenshotRoute::registerRoutes(server);

    // The Vue bundle is linked into the firmware (see include/WebAssets.h), so
    // it is served from flash rather than a filesystem. One route per asset
    // instead of a wildcard on purpose: an unmatched URL has to keep falling
    // through to the WiFi library's 404 handler, which redirects to /setup and is
    // what makes the captive portal work. WebApi's routes are registered in
    // captive mode too.
    for (size_t i = 0; i < WebAssets::count(); ++i) {
        const WebAssets::Asset* asset = WebAssets::at(i);
        const httpd_uri_t route = wifisetup::route(asset->path, HTTP_GET, handleAsset,
                                                   const_cast<WebAssets::Asset*>(asset));
        httpd_register_uri_handler(server, &route);
    }
    Logger.debug("WebApi: %u embedded assets served from flash",
                 (unsigned)WebAssets::count());

    Logger.debug("=== WebApi: Routes setup complete ===");
}
