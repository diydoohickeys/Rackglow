#pragma once

#include <lvgl.h>
#include <stdint.h>

// =============================================================================
// DoohickeyMark — the DIY Doohickeys brand mark (16-segment spectrum ring +
// outlined knob + detent pointer) as a live LVGL widget that doubles as a
// PROGRESS indicator.
// =============================================================================
// The artwork generator `Icons/create-icons.py` in the DeckDoohickey repo is the
// SOURCE OF TRUTH — it EMITS the SVGs, so never read geometry out of an SVG, and
// ignore any older description of a tapering band or THICK=14/KNOB_R=44 (both
// superseded). Mirrored here as ratios of the artwork's 162-unit viewBox so any
// pixel size reproduces the mark:
//     N=16 segments, PITCH=22.5 deg, SPAN=17.5 deg (5 deg gap),
//     OUT_R=80, THICK=20 (constant band), KNOB_R=46 (OUTLINE, not a filled disc),
//     pointer from the centre to r=31 at 60 deg (2 o'clock), ink #7D7C75.
//
// ⚠ There is no build-time link between the generator (Python, another repo) and
// this copy. A geometry change has to be made here by hand as well.
//
// The spectrum and the ink are ARTWORK, not theme — they are the same on every
// product that carries the mark, so they deliberately do not come from
// rackglow.h (see .claude/rules/theming.md).
//
// Progress model: segments light on THRESHOLD CROSSINGS, not per frame.
// SetLitCount/SetProgressPct restyle only the segments that actually changed
// (~16 style writes across a whole boot), because per-frame re-styling of 16
// objects is the documented LVGL freeze pattern (see .claude/rules/lvgl.md).
// The only continuous motion is TickShimmer(), which pulses exactly ONE object
// (the next segment to light) so a stall still reads as "working", not "hung".
// It is PUMPED BY THE WORK — the caller ticks it from whatever loop or callback
// it already has — so an animation that keeps running after a hang is impossible.
// =============================================================================

class DoohickeyMark {
public:
    static constexpr int SEGMENTS = 16;

    // Artwork constants (units of the 162 viewBox) — see the header comment.
    static constexpr float OUT_R   = 80.0f;
    static constexpr float THICK   = 20.0f;
    static constexpr float KNOB_R  = 46.0f;
    static constexpr float POINTER = 31.0f;
    static constexpr float SPAN    = 17.5f;
    static constexpr float PITCH   = 360.0f / SEGMENTS;
    static constexpr float STROKE  = 7.5f;   // knob outline + pointer width
    static constexpr float POINTER_ANGLE = 60.0f;  // 2 o'clock, the mark's detent
    static constexpr uint32_t INK = 0x7D7C75;

    // Unlit segments stay faintly visible so the ring always reads AS the mark
    // while it fills, rather than appearing out of nothing.
    static constexpr lv_opa_t UNLIT_OPA = 45;
    static constexpr lv_opa_t LIT_OPA   = LV_OPA_COVER;

    // Build the mark on `parent`, centred, with `diameterPx` = the ring's OUTER
    // diameter. Safe to call once only; call Cleanup() before rebuilding.
    void Build(lv_obj_t* parent, int diameterPx, int yOffset = 0);

    // Build at an explicit top-left, for a caller that owns the layout.
    void BuildAt(lv_obj_t* parent, int x, int y, int diameterPx);

    // Light the first `lit` segments (clamped 0..SEGMENTS). No-op when unchanged.
    void SetLitCount(int lit);

    // 0-100 -> round(pct * 16 / 100) segments. Deliberately derived from the REAL
    // progress value: one segment is NOT one boot step.
    void SetProgressPct(int pct);

    int LitCount() const { return _lit; }

    // Pointer angle in MARK degrees (0 = 12 o'clock, increasing clockwise).
    void SetPointerAngle(float markDegrees);

    // Pulse the next-to-light segment. Cheap: one style write per call, on one
    // object. Call from whatever loop or callback already exists; no-op once
    // fully lit.
    void TickShimmer(uint32_t nowMs);

    // Show/hide the whole mark without destroying it.
    void SetHidden(bool hidden);

    // Drop retained references. The objects themselves are freed with the parent,
    // so this never deletes.
    void Cleanup();

    bool IsBuilt() const { return _root != nullptr; }
    lv_obj_t* Root() const { return _root; }

    // The mark's hue-ordered spectrum (segment 0 at 12 o'clock, clockwise).
    static uint32_t SegmentColor(int index);
    static void SegmentRGB(int index, uint8_t& r, uint8_t& g, uint8_t& b);

private:
    void buildInto(lv_obj_t* parent, int diameterPx);
    void applySegment(int i, bool lit);
    void updatePointerPoints();

    lv_obj_t* _root = nullptr;
    lv_obj_t* _segments[SEGMENTS] = { nullptr };
    lv_obj_t* _knob = nullptr;
    lv_obj_t* _pointer = nullptr;

    // lv_line does NOT copy its point array — it must outlive the widget.
    lv_point_precise_t _pointerPts[2] = {};

    int   _diameter = 0;
    int   _lit = 0;
    int   _shimmerIndex = -1;
    float _pointerAngle = POINTER_ANGLE;
};
