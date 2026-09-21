#pragma once

#include <lvgl.h>
#include <Arduino.h>
#include <functional>

/// Toggle button for VU mode. Owns its LVGL objects and deletes them on destruction.
class VuButton {
public:
    using StateChangeCallback = std::function<void(bool isActive)>;

    VuButton();
    ~VuButton();

    VuButton(VuButton&& other) noexcept;
    VuButton& operator=(VuButton&& other) noexcept;
    VuButton(const VuButton&) = delete;
    VuButton& operator=(const VuButton&) = delete;

    /// Layout is the parent's job; this only creates the button inside it.
    bool initialize(lv_obj_t* parent);

    void setCallback(StateChangeCallback callback);
    void setState(bool isActive);
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
