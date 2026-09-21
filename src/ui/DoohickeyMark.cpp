#include "ui/DoohickeyMark.h"

#include <math.h>

namespace {
// Local, so nothing depends on M_PI being visible (it isn't guaranteed under a
// strict-ISO <cmath>, and Arduino's math macros are a known trap).
constexpr float kPi = 3.14159265358979f;

// The mark's hue-ordered spectrum, lifted verbatim from the artwork generator's
// SPECTRUM list (segment 0 = 12 o'clock, running clockwise). Artwork, not theme —
// see the header.
constexpr uint32_t kSpectrum[DoohickeyMark::SEGMENTS] = {
    0xE24B4A, 0xEE6A35, 0xF08A20, 0xEFA820, 0xE0C21E, 0xB9CB22,
    0x8ABF4E, 0x4FB97F, 0x2FB2A0, 0x2FA2C4, 0x3B8BD4, 0x5C79DC,
    0x7F77DD, 0xA06FD6, 0xC062C0, 0xD4548F,
};

// Scale an artwork unit (of the 2*OUT_R = 160-unit ring) to pixels.
inline int unitsToPx(float units, int diameterPx) {
    const float k = static_cast<float>(diameterPx) / (2.0f * DoohickeyMark::OUT_R);
    return static_cast<int>(lroundf(units * k));
}

// LVGL measures arc angles from 3 o'clock, clockwise; the mark measures from
// 12 o'clock, clockwise. Normalised to [0,360) because LVGL rejects negatives.
inline int32_t markToLvglAngle(float markDegrees) {
    float a = markDegrees - 90.0f;
    while (a < 0.0f) a += 360.0f;
    while (a >= 360.0f) a -= 360.0f;
    return static_cast<int32_t>(lroundf(a));
}
}  // namespace

uint32_t DoohickeyMark::SegmentColor(int index) {
    if (index < 0 || index >= SEGMENTS) return kSpectrum[0];
    return kSpectrum[index];
}

void DoohickeyMark::SegmentRGB(int index, uint8_t& r, uint8_t& g, uint8_t& b) {
    const uint32_t c = SegmentColor(index);
    r = static_cast<uint8_t>((c >> 16) & 0xFF);
    g = static_cast<uint8_t>((c >> 8) & 0xFF);
    b = static_cast<uint8_t>(c & 0xFF);
}

void DoohickeyMark::Build(lv_obj_t* parent, int diameterPx, int yOffset) {
    if (!parent || _root) return;
    buildInto(parent, diameterPx);
    if (_root) lv_obj_align(_root, LV_ALIGN_CENTER, 0, yOffset);
}

void DoohickeyMark::BuildAt(lv_obj_t* parent, int x, int y, int diameterPx) {
    if (!parent || _root) return;
    buildInto(parent, diameterPx);
    if (_root) lv_obj_set_pos(_root, x, y);
}

void DoohickeyMark::buildInto(lv_obj_t* parent, int diameterPx) {
    if (diameterPx < 24) diameterPx = 24;  // below this the band is sub-pixel
    _diameter = diameterPx;

    _root = lv_obj_create(parent);
    lv_obj_remove_style_all(_root);
    lv_obj_set_size(_root, diameterPx, diameterPx);
    lv_obj_remove_flag(_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(_root, LV_OBJ_FLAG_CLICKABLE);

    int bandPx = unitsToPx(THICK, diameterPx);
    if (bandPx < 1) bandPx = 1;
    int strokePx = unitsToPx(STROKE, diameterPx);
    if (strokePx < 2) strokePx = 2;  // a sub-pixel stroke dissolves

    // --- the 16 spectrum segments -------------------------------------------
    // One lv_arc per segment: the band is drawn INWARD from the widget edge, so a
    // widget of `diameterPx` with arc_width = THICK gives outer radius = D/2 exactly.
    for (int i = 0; i < SEGMENTS; ++i) {
        const float centre = i * PITCH;
        lv_obj_t* arc = lv_arc_create(_root);
        lv_obj_remove_style_all(arc);
        lv_obj_set_size(arc, diameterPx, diameterPx);
        lv_obj_center(arc);
        lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);

        // Whole-degree rounding turns the 17.5/5.0 span/gap into ~18/4.5 — invisible
        // at these sizes, and LVGL angles are integers.
        const int32_t a0 = markToLvglAngle(centre - SPAN * 0.5f);
        const int32_t a1 = markToLvglAngle(centre + SPAN * 0.5f);
        lv_arc_set_bg_angles(arc, a0, a1);

        lv_obj_set_style_arc_width(arc, bandPx, LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, lv_color_hex(kSpectrum[i]), LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(arc, false, LV_PART_MAIN);  // the artwork's ends are square
        lv_obj_set_style_arc_opa(arc, UNLIT_OPA, LV_PART_MAIN);
        // The value indicator and the draggable knob are both unwanted — this is a
        // static band, not a slider.
        lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);

        _segments[i] = arc;
    }

    // --- the knob: an OUTLINED circle (fill="none" + stroke in the artwork) ---
    const int knobPx = unitsToPx(2.0f * KNOB_R, diameterPx);
    _knob = lv_obj_create(_root);
    lv_obj_remove_style_all(_knob);
    lv_obj_set_size(_knob, knobPx, knobPx);
    lv_obj_center(_knob);
    lv_obj_remove_flag(_knob, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(_knob, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(_knob, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(_knob, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_knob, strokePx, 0);
    lv_obj_set_style_border_color(_knob, lv_color_hex(INK), 0);
    lv_obj_set_style_border_opa(_knob, LV_OPA_COVER, 0);

    // --- the detent pointer: centre -> r=POINTER, round cap -------------------
    _pointer = lv_line_create(_root);
    lv_obj_remove_style_all(_pointer);
    lv_obj_set_size(_pointer, diameterPx, diameterPx);
    lv_obj_center(_pointer);
    lv_obj_set_style_line_width(_pointer, strokePx, 0);
    lv_obj_set_style_line_color(_pointer, lv_color_hex(INK), 0);
    lv_obj_set_style_line_rounded(_pointer, true, 0);
    updatePointerPoints();

    _lit = 0;
    _shimmerIndex = -1;
}

void DoohickeyMark::updatePointerPoints() {
    if (!_pointer || _diameter <= 0) return;

    // Screen coords (y down) match the artwork's own p(a,r) = (r sin a, -r cos a).
    const float a = _pointerAngle * kPi / 180.0f;
    const int32_t c = _diameter / 2;
    const int len = unitsToPx(POINTER, _diameter);

    _pointerPts[0].x = c;
    _pointerPts[0].y = c;
    _pointerPts[1].x = c + static_cast<int32_t>(lroundf(len * sinf(a)));
    _pointerPts[1].y = c - static_cast<int32_t>(lroundf(len * cosf(a)));

    lv_line_set_points(_pointer, _pointerPts, 2);
}

void DoohickeyMark::SetPointerAngle(float markDegrees) {
    if (_pointerAngle == markDegrees) return;
    _pointerAngle = markDegrees;
    updatePointerPoints();
}

void DoohickeyMark::applySegment(int i, bool lit) {
    if (i < 0 || i >= SEGMENTS || !_segments[i]) return;
    lv_obj_set_style_arc_opa(_segments[i], lit ? LIT_OPA : UNLIT_OPA, LV_PART_MAIN);
}

void DoohickeyMark::SetLitCount(int lit) {
    if (!_root) return;
    if (lit < 0) lit = 0;
    if (lit > SEGMENTS) lit = SEGMENTS;
    if (lit == _lit) return;

    // Only the segments that actually changed get a style write.
    if (lit > _lit) {
        for (int i = _lit; i < lit; ++i) applySegment(i, true);
    } else {
        for (int i = lit; i < _lit; ++i) applySegment(i, false);
    }
    _lit = lit;

    // The old shimmer target has been overtaken (or rolled back) — re-baseline it
    // so a stale pulse opacity can't stick.
    if (_shimmerIndex >= 0 && _shimmerIndex != _lit) {
        applySegment(_shimmerIndex, _shimmerIndex < _lit);
        _shimmerIndex = -1;
    }
}

void DoohickeyMark::SetProgressPct(int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    SetLitCount((pct * SEGMENTS + 50) / 100);
}

void DoohickeyMark::TickShimmer(uint32_t nowMs) {
    if (!_root || _lit >= SEGMENTS) {
        if (_shimmerIndex >= 0) {  // fully lit: retire the pulse
            applySegment(_shimmerIndex, true);
            _shimmerIndex = -1;
        }
        return;
    }

    const int target = _lit;  // the next segment to light
    if (_shimmerIndex != target) {
        if (_shimmerIndex >= 0) applySegment(_shimmerIndex, _shimmerIndex < _lit);
        _shimmerIndex = target;
    }

    // ~1.4 s breath between the unlit floor and a clearly-visible mid opacity.
    constexpr uint32_t PERIOD_MS = 1400;
    const float phase = (nowMs % PERIOD_MS) / static_cast<float>(PERIOD_MS);
    const float wave = 0.5f * (1.0f - cosf(phase * 2.0f * kPi));  // 0..1
    const lv_opa_t opa = static_cast<lv_opa_t>(UNLIT_OPA + wave * (200 - UNLIT_OPA));
    lv_obj_set_style_arc_opa(_segments[target], opa, LV_PART_MAIN);
}

void DoohickeyMark::SetHidden(bool hidden) {
    if (!_root) return;
    if (hidden) lv_obj_add_flag(_root, LV_OBJ_FLAG_HIDDEN);
    else        lv_obj_remove_flag(_root, LV_OBJ_FLAG_HIDDEN);
}

void DoohickeyMark::Cleanup() {
    // Objects are owned by the parent; only drop our references.
    _root = nullptr;
    _knob = nullptr;
    _pointer = nullptr;
    for (int i = 0; i < SEGMENTS; ++i) _segments[i] = nullptr;
    _diameter = 0;
    _lit = 0;
    _shimmerIndex = -1;
}
