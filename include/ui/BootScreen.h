#pragma once

// ⚠ Not rackglow.h — that header is an umbrella that includes WifiBootManager.h,
// which needs this one. Colours come from it in the .cpp, as in every other ui/.
#include "ui/PatchCable.h"

#include <Arduino.h>
#include <WiFiSetupManager.h>  // WiFiStatusCallback
#include <lvgl.h>

/**
 * @brief Rackglow's branded boot screen: the wordmark over a modular-synth patch
 *        lead, with the cable lighting segment by segment as progress.
 *
 * Replaces Preflight's generic `WiFiSetupBootUI` (a title over a scrolling text
 * log). Implements the same `WiFiStatusCallback` interface, so it drops into
 * `WiFiSetupConfig::statusCallback` unchanged.
 *
 * Everything is built into ONE full-screen container on the active screen — the
 * tabview is created on that same screen afterwards and covers it, and cleanup()
 * is a single delete that leaves no styles behind.
 *
 * Threading: Preflight fires every status callback synchronously from the task
 * that called `WiFiSetupManager::begin()` (see connectToNetwork / scanNetworks),
 * which is `setup()`. So there is no cross-task queue here — updates paint
 * directly and force a redraw, because `begin()` blocks the loop that would
 * otherwise drive `lv_timer_handler`.
 *
 * Progress is the WIFI phase only, which is the variable-length part the user
 * actually waits on: the cable reaches the far plug when WiFi connects, and the
 * later fixed-cost steps (UI build, LEDs, web server) just change the status line.
 */
class BootScreen : public WiFiStatusCallback {
public:
    BootScreen() = default;
    ~BootScreen();

    BootScreen(const BootScreen&) = delete;
    BootScreen& operator=(const BootScreen&) = delete;

    bool initialize();
    void cleanup();
    bool isInitialized() const { return _initialized; }

    /// Shown in the connected ("<name>.local") line. Set before begin().
    void setDeviceName(const String& deviceName) { _deviceName = deviceName; }

    /// Status line for a step Preflight knows nothing about (LEDs, UI build).
    void setStatus(const char* status);

    /**
     * Pump the cable's shimmer. Deliberately caller-driven rather than self-timed:
     * an animation on its own timer keeps running after a hang, which is exactly
     * backwards for a "still working" indicator.
     */
    void tick();

    // WiFiStatusCallback
    void onScanStart() override;
    void onScanComplete(int networks) override;
    void onConnecting(const String& ssid) override;
    void onConnectionProgress() override;
    void onConnected(IPAddress ip) override;
    void onAPMode(const String& apName, IPAddress ip) override;

private:
    void createUI();
    void updateStatus(const String& status);
    void updateDetails(const String& details);
    void setProgress(int pct);
    /// Fill up to `pct` one segment PER REFRESH. Load-bearing — see the .cpp.
    void sweepProgressTo(int pct);
    void refresh();

    // Styled + placed at `y`, centred horizontally.
    lv_obj_t* addRow(const char* text, const lv_font_t* font, uint32_t color, int y);

    bool _initialized = false;

    lv_obj_t* _root = nullptr;        // full-screen container; owns everything below
    lv_obj_t* _nameLabel = nullptr;   // "RACKGLOW"
    lv_obj_t* _taglineLabel = nullptr;
    lv_obj_t* _statusLabel = nullptr;
    lv_obj_t* _ipLabel = nullptr;     // persistent once set
    lv_obj_t* _detailsLabel = nullptr;

    PatchCable _cable;

    String _deviceName;
    int _progress = 0;
};
