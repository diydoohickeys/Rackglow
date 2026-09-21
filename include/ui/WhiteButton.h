#pragma once

#include <lvgl.h>
#include <Arduino.h>
#include <functional>

/// Toggle button for white mode. Owns its LVGL objects and deletes them on destruction.
class WhiteButton {
public:
    using StateChangeCallback = std::function<void(bool isWhite)>;

    WhiteButton();
    ~WhiteButton();

    WhiteButton(WhiteButton&& other) noexcept;
    WhiteButton& operator=(WhiteButton&& other) noexcept;
    WhiteButton(const WhiteButton&) = delete;
    WhiteButton& operator=(const WhiteButton&) = delete;

    /// Layout is the parent's job; this only creates the button inside it.
    bool initialize(lv_obj_t* parent);

    void setCallback(StateChangeCallback callback);

    /// triggerCallback = false when ANOTHER control is clearing this one (VU or
    /// an animation being selected). Firing the callback there re-enters the
    /// mode logic and undoes the change that triggered it.
    void setState(bool isWhite, bool triggerCallback = true);

    bool getState() const;
    bool isInitialized() const { return initialized_; }
    lv_obj_t* getLvglObject() const { return button_; }

private:
    lv_obj_t* button_;
    StateChangeCallback callback_;
    bool initialized_;
    bool currentState_;

    static void eventHandler(lv_event_t* event);
    void handleStateChange();
    void cleanup();
    void applyButtonStyling();
};
