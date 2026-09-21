#pragma once

#include <lvgl.h>
#include <functional>

/**
 * Horizontal brightness fader in a small card, with a live percentage beside
 * the caption.
 *
 * Horizontal rather than vertical on purpose: a tall fader pinned to the left of
 * the Colour tab costs a full column of width and leaves the wheel cramped.
 */
class BrightnessSlider {
public:
    using BrightnessCallback = std::function<void(int brightness)>;

private:
    lv_obj_t* card_;                ///< Container: header row + track
    lv_obj_t* slider_;
    lv_obj_t* label_;               ///< Live percentage readout
    lv_obj_t* parentTab_;
    bool initialized_;
    int currentBrightness_;
    int maxBrightness_;
    BrightnessCallback callback_;

    static lv_style_t indicatorStyle_;
    static lv_style_t knobStyle_;
    static bool stylesInitialized_;

    static void initializeStyles();
    void createSlider();
    /// Creates the card and its header row (caption + percentage).
    void createLabel();
    void updateValueLabel();
    void applySliderStyling();
    static void eventHandlerWrapper(lv_event_t* e);
    void handleSliderEvent(lv_event_t* e);

public:
    explicit BrightnessSlider(int maxBrightness = 255);
    ~BrightnessSlider();

    BrightnessSlider(const BrightnessSlider&) = delete;
    BrightnessSlider& operator=(const BrightnessSlider&) = delete;
    BrightnessSlider(BrightnessSlider&& other) noexcept;
    BrightnessSlider& operator=(BrightnessSlider&& other) noexcept;

    /// Layout is the parent's job; this only creates the card inside it.
    bool initialize(lv_obj_t* parent, int initialBrightness = 100);

    /// triggerCallback defaults to FALSE because the usual programmatic call is
    /// reflecting the LEDs' existing state, where firing it would echo back out.
    /// Pass true when the change must propagate onward (e.g. from the web).
    void setBrightness(int brightness, bool animate = true, bool triggerCallback = false);

    int getBrightness() const { return currentBrightness_; }

    /// Reads the widget rather than the cached value.
    int getSliderValue() const;

    /// Adopts the widget's value into currentBrightness_, after anything that
    /// moved the slider without going through setBrightness().
    void syncFromSlider();

    int getMaxBrightness() const { return maxBrightness_; }
    void setCallback(BrightnessCallback callback) { callback_ = callback; }
    bool isInitialized() const { return initialized_; }
    lv_obj_t* getSliderWidget() const { return slider_; }

    /// Also called by the destructor.
    void cleanup();
};
