#pragma once

#include <lvgl.h>
#include <Arduino.h>

#define NUM_VU_CHANNELS 7
#define SEGMENTS_PER_BAR 10
#define SEGMENT_WIDTH 26
#define SEGMENT_HEIGHT 18
#define SEGMENT_GAP 2
#define BAR_SPACING 10
#define LEFT_ALIGNMENT 260
#define BAR_TOTAL_HEIGHT ((SEGMENT_HEIGHT + SEGMENT_GAP) * SEGMENTS_PER_BAR)
// Height of the frequency-label row drawn just below the bars (montserrat_14 at
// BAR_TOTAL_HEIGHT + 5). Used when centring the whole block on the screensaver.
#define VU_LABEL_BAND_HEIGHT 24

/// Segmented VU meter over the seven MSGEQ7 bands. Owns its LVGL objects.
class VuGraph {
public:
    VuGraph();
    ~VuGraph();

    VuGraph(VuGraph&& other) noexcept;
    VuGraph& operator=(VuGraph&& other) noexcept;
    VuGraph(const VuGraph&) = delete;
    VuGraph& operator=(const VuGraph&) = delete;

    bool initialize(lv_obj_t* parent);

    /// Redraws from the latest frame in g_audioBus. Runs on the render task and
    /// PULLS — it never samples the hardware itself.
    void update();

    /**
     * Re-centres the meter within an @p areaW × @p areaH region.
     *
     * The bars and labels are absolutely positioned for the VU tab's geometry,
     * so this moves the underlying canvas instead. Used by the idle screensaver;
     * the VU tab never calls it, which is what keeps the tab layout untouched.
     * Call after initialize().
     */
    void centerContentIn(lv_coord_t areaW, lv_coord_t areaH);

    bool isInitialized() const { return initialized_; }
    lv_obj_t* getLvglObject() const { return canvas_; }

private:
    lv_obj_t* canvas_;
    lv_obj_t* segments_[NUM_VU_CHANNELS][SEGMENTS_PER_BAR];
    lv_obj_t* peakSegments_[NUM_VU_CHANNELS];
    bool initialized_;

    /// Per-band levels 0-255, pulled from g_audioBus.
    int vuValues_[NUM_VU_CHANNELS];

    int peakLevels_[NUM_VU_CHANNELS];
    unsigned long peakTimers_[NUM_VU_CHANNELS];

    /// Last drawn segment count per band, so a frame only touches what changed.
    int prevLitSegments_[NUM_VU_CHANNELS];

    void updateVuBars();
    void cleanup();
    void createFrequencyLabels();

    /// segmentIndex counts from the BOTTOM (0) up to SEGMENTS_PER_BAR-1.
    lv_color_t getSegmentColor(int segmentIndex, bool lit);
};
