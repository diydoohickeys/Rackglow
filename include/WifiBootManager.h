#pragma once

#include <Arduino.h>
#include <WiFiSetupManager.h>

// Forward-declared, not included: rackglow.h pulls this header in, so including
// ui/BootScreen.h here would make that a cycle. Only a pointer is held.
class BootScreen;

/// Wraps Preflight's WiFi setup, the branded boot screen and the shared theme
/// into one boot-time unit.
class WifiBootManager {
public:
    WifiBootManager();
    ~WifiBootManager();

    WifiBootManager(const WifiBootManager&) = delete;
    WifiBootManager& operator=(const WifiBootManager&) = delete;

    /// apName/apPassword are the captive portal's credentials in setup mode.
    bool initialize(const char* apName = "Rackglow-Setup",
                    const char* apPassword = "rackglow123");

    /// Call from the loop.
    void update();

    bool isInSetupMode() const;

    /// Preflight owns the server; other components register routes on it.
    AsyncWebServer* getWebServer();

    WiFiSetupTheme* getTheme() { return &theme_; }

    /// Status line for a boot step Preflight's callbacks don't cover.
    void setBootStatus(const char* status);

    /// Tears the boot screen down once the real UI takes the screen.
    void cleanupBootUI();

private:
    WiFiSetupManager* wifiManager_;
    BootScreen* bootScreen_;
    WiFiSetupTheme theme_;
    bool initialized_;

    void initializeTheme();
    void loadHostname();
};

extern WifiBootManager* g_wifiBootManager;
