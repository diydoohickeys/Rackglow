#include "WebUIManager.h"

#include "ui/EffectGrid.h"
#include "WebApi.h"
#include "ui/UIManager.h"
#include "ui/ui.h"
#include <WiFiSetupManager.h>
#include <OTAManager.h>
#include <string>
#include <Logger.h>
#include "led/LEDManager.h"   // g_ledManager
#include "ui/ColourWheel.h"
#include "audio/AudioTask.h"  // pause/resume audio task around OTA

// Global WebUI manager instance
WebUIManager* g_webUIManager = nullptr;

// Global OTA manager instance
OTAManager* g_otaManager = nullptr;

WebUIManager::WebUIManager(httpd_handle_t server)
    : initialized_(false)
    , server_(server)
{
}

bool WebUIManager::initialize() {
    if (initialized_) {
        return true;
    }

    if (!server_) {
        Logger.error("WebUIManager: No web server provided!");
        return false;
    }

    if (!wsHub_.begin(server_, [this](const char* text, size_t len) { handleWebSocketMessage(text, len); })) {
        Logger.error("WebUIManager: could not register /ws");
    }
    WebApi::registerRoutes(server_);

    // The /logs page live-tails over the same socket.
    Logger.setOutputCallback([this](LogLevel level, uint32_t timestamp, const char* message) {
        if (Logger.isWebSocketEnabled()) {
            const std::string line = Logger.liveTailJson(level, timestamp, message);
            wsHub_.broadcast(line.data(), line.size());
        }
    });

    // Initialize OTA Manager with callbacks
    if (!g_otaManager) {
        g_otaManager = new OTAManager();
    }

    if (g_otaManager) {
        // Set start callback
        g_otaManager->setStartCallback([]() {
            // Take the LED strips out of normal update() so the OTA progress
            // display owns them — otherwise the main loop slams brightness
            // back to the user's saved value between progress ticks and the
            // bar flickers the whole way.
            if (g_ledManager) {
                g_ledManager->setAnimationEnabled(false);
                g_ledManager->setOTAMode(true);  // stops LEDManager reading g_audioBus
            }
            // Suspend the core-0 audio task: its ADC2 (WiFi-shared) reads contend
            // with the OTA traffic and help starve IDLE0 (task watchdog). Done
            // after setOTAMode so no one is consuming audio frames.
            pauseAudioTask();
        });

        // Set LED progress callback
        g_otaManager->setLEDProgressCallback([](uint8_t progress) {
            if (g_ledManager) {
                g_ledManager->showOTAProgress(progress);
            }
        });

        // Set screen progress callback
        g_otaManager->setScreenProgressCallback([](uint8_t progress, OTAManager::Stage stage) {
            if (g_uiManager) {
                if (stage == OTAManager::Stage::STARTING) {
                    g_uiManager->showOTAScreen();
                } else if (stage == OTAManager::Stage::IN_PROGRESS || stage == OTAManager::Stage::COMPLETE) {
                    g_uiManager->updateOTAProgress(progress);
                } else if (stage == OTAManager::Stage::FAILED) {
                    g_uiManager->hideOTAScreen();
                }
            }
        });

        // Set end callback for failure handling
        g_otaManager->setEndCallback([](bool success) {
            if (!success) {
                // OTA failed (no reboot). Release the strips back to normal and
                // ask for a non-blocking red flash — do NOT block here (this runs
                // on the httpd task; the old delay()-based blink stalled it ~1.2s
                // and never even rendered, since update() is suspended in OTA mode).
                if (g_ledManager) {
                    g_ledManager->setOTAMode(false);
                    g_ledManager->flashError();
                }
                // Bring the audio task back.
                resumeAudioTask();
            }
            // On success the device restarts, so the flag resets implicitly.
        });

        // Initialize OTA with the shared server
        // Blank partition label = no filesystem OTA. The web UI is linked into
        // the firmware now (WebAssets.h), so there is no filesystem to update and
        // the option would only be a way to wipe a partition by accident.
        g_otaManager->begin(server_, "");
    }

    Logger.info("Web UI Manager initialized successfully");

    initialized_ = true;
    return true;
}

void WebUIManager::notifyClients() {
    if (!initialized_) {
        return;
    }
    broadcast(generateStateResponse());
}

void WebUIManager::broadcast(const String& message) {
    wsHub_.broadcast(message.c_str(), message.length());
}

String WebUIManager::generateAnimationsResponse() {
    JsonDocument doc;
    doc["message"] = "animations";
    JsonArray animations = doc["animations"].to<JsonArray>();

    for (int i = 0; i < ANIMATION_COUNT; i++) {
        AnimationType current = static_cast<AnimationType>(i);
        JsonObject anim = animations.add<JsonObject>();
        anim["name"] = animationDescription(current);
        anim["value"] = i;
        // The colour the device paints this effect's tile with. Sent rather than
        // duplicated in the web app, so the two pickers can't drift apart.
        anim["colour"] = EffectGrid::signatureColor(i);
        anim["colour2"] = EffectGrid::signatureColorEnd(i);
        anim["audio"] = (i >= ICEWAVES);
    }

    String output;
    serializeJson(doc, output);
    return output;
}

String WebUIManager::generateStateResponse() {
    JsonDocument doc;
    doc["message"] = "states";
    JsonArray controls = doc["controls"].to<JsonArray>();
    
    JsonObject vu_ctrl = controls.add<JsonObject>();
    vu_ctrl["name"] = "vu";
    vu_ctrl["state"] = g_ledManager ? g_ledManager->isVuModeEnabled() : false;

    JsonObject white_ctrl = controls.add<JsonObject>();
    white_ctrl["name"] = "white";
    white_ctrl["state"] = g_ledManager ? g_ledManager->isWhiteModeEnabled() : false;

    JsonObject anim_ctrl = controls.add<JsonObject>();
    anim_ctrl["name"] = "animation";
    anim_ctrl["state"] = g_ledManager ? g_ledManager->isAnimationEnabled() : false;
    anim_ctrl["animation"] = g_ledManager ? static_cast<int>(g_ledManager->getCurrentAnimation()) : 0;

    JsonObject color_ctrl = controls.add<JsonObject>();
    color_ctrl["name"] = "colour";
    color_ctrl["state"] = g_colourWheel ? g_colourWheel->getColorHex() : "#000000";

    JsonObject brightness_ctrl = controls.add<JsonObject>();
    brightness_ctrl["name"] = "brightness";
    brightness_ctrl["state"] = g_ledManager ? g_ledManager->getBrightness() : 128;

    String output;
    serializeJson(doc, output);
    return output;
}

void WebUIManager::handleWebSocketMessage(const char* text, size_t len) {
    JsonDocument request;
    if (deserializeJson(request, text, len)) {
        return;
    }
    const char* messageType = request["message"] | "";

    if (strcmp(messageType, "connect") == 0) {
        handleConnectMessage();
    } else if (strcmp(messageType, "vu") == 0) {
        handleVuMessage(request);
    } else if (strcmp(messageType, "white") == 0) {
        handleWhiteMessage(request);
    } else if (strcmp(messageType, "brightness") == 0) {
        handleBrightnessMessage(request);
    } else if (strcmp(messageType, "animation") == 0) {
        handleAnimationMessage(request);
    } else if (strcmp(messageType, "colour") == 0) {
        handleColorMessage(request);
    }
}

void WebUIManager::handleConnectMessage() {
    broadcast(generateAnimationsResponse());
    broadcast(generateStateResponse());
}

// These handlers run on the httpd task. They must NOT touch LVGL directly —
// instead they post a POD command to the UI owner, which applies it on the task
// that owns lv_* (see UIManager::applyUiCommand). notifyClients() is issued
// there too, after the state has actually changed.
void WebUIManager::handleVuMessage(const JsonDocument& request) {
    if (g_uiManager) {
        UiCommand cmd;
        cmd.type = UiCommandType::SetVu;
        cmd.boolValue = (bool)request["value"];
        g_uiManager->postUiCommand(cmd);
    }
}

void WebUIManager::handleWhiteMessage(const JsonDocument& request) {
    if (g_uiManager) {
        UiCommand cmd;
        cmd.type = UiCommandType::SetWhite;
        cmd.boolValue = (bool)request["value"];
        g_uiManager->postUiCommand(cmd);
    }
}

void WebUIManager::handleBrightnessMessage(const JsonDocument& request) {
    if (g_uiManager) {
        UiCommand cmd;
        cmd.type = UiCommandType::SetBrightness;
        cmd.intValue = (int)request["value"];
        g_uiManager->postUiCommand(cmd);
    }
}

void WebUIManager::handleAnimationMessage(const JsonDocument& request) {
    if (g_uiManager) {
        UiCommand cmd;
        cmd.type = UiCommandType::SetAnimation;
        cmd.boolValue = (bool)request["value"];
        cmd.intValue = (int)request["animation"];
        g_uiManager->postUiCommand(cmd);
    }
}

void WebUIManager::handleColorMessage(const JsonDocument& request) {
    if (g_uiManager) {
        String hexValue = (const char*)request["value"];
        // Deliberately not logged: the web wheel sends one of these per pointer
        // move, and a line each buried the whole log buffer in a single drag —
        // and pushed every one out to all WebSocket clients.
        UiCommand cmd;
        cmd.type = UiCommandType::SetColour;
        // cmd.colour is sized exactly for "#RRGGBB" + null. strlcpy is bounded and
        // always null-terminates (and, unlike snprintf("%s"), doesn't trip
        // -Wformat-truncation on the unbounded source string).
        strlcpy(cmd.colour, hexValue.c_str(), sizeof(cmd.colour));
        g_uiManager->postUiCommand(cmd);
    }
}


// Web UI notification helper
void updateWebUi() {
    if (g_webUIManager) {
        g_webUIManager->notifyClients();
    }
}