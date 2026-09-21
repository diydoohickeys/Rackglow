#include "ui/PatchCable.h"

#include "rackglow.h"
#include "ui/DoohickeyMark.h"

#include <math.h>

namespace {
// Local, so nothing depends on M_PI being visible (it isn't guaranteed under a
// strict-ISO <cmath>, and Arduino's math macros are a known trap).
constexpr float kPi = 3.14159265358979f;

// The pending segment breathes between the sleeve grey and its lit hue. A COLOUR
// mix rather than an opacity pulse: fading opacity over the near-black
// background would open a visible gap in the cable at the trough.
constexpr uint32_t SHIMMER_PERIOD_MS = 1400;
constexpr uint8_t  SHIMMER_PEAK_MIX  = 190;  // 255 would read as already lit

int clampInt(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}
}  // namespace

void PatchCable::BuildAt(lv_obj_t* parent, int x, int y, int widthPx) {
    if (!parent || _root) return;
    if (widthPx < 120) widthPx = 120;  // below this the plug details are sub-pixel

    const int heightPx = HeightForWidth(widthPx);

    _root = lv_obj_create(parent);
    lv_obj_remove_style_all(_root);
    lv_obj_set_size(_root, widthPx, heightPx);
    lv_obj_set_pos(_root, x, y);
    lv_obj_remove_flag(_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(_root, LV_OBJ_FLAG_CLICKABLE);

    // Cable first, plugs after: the plugs then cover the rounded cap the cable
    // leaves where it enters each housing.
    buildSegments(widthPx, heightPx);
    buildPlug(false, widthPx, heightPx);
    buildPlug(true, widthPx, heightPx);

    _lit = 0;
    _shimmerIndex = -1;
}

void PatchCable::buildSegments(int widthPx, int heightPx) {
    const int   cableW = CableWidthPx(widthPx);
    const int   jack   = Scaled(widthPx, JACK_K);
    const float x0     = static_cast<float>(jack);
    const float x1     = static_cast<float>(widthPx - jack);
    const float yc     = heightPx * 0.5f;
    const float amp    = static_cast<float>(Scaled(widthPx, AMP_K));

    // One shared polyline, then split into per-segment slices. The boundary point
    // belongs to BOTH neighbours, so their rounded caps close the join.
    constexpr int CURVE_PTS = SEGMENTS * (PTS_PER_SEG - 1) + 1;
    lv_point_t curve[CURVE_PTS];
    for (int i = 0; i < CURVE_PTS; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(CURVE_PTS - 1);
        curve[i].x = static_cast<int32_t>(lroundf(x0 + (x1 - x0) * t));
        curve[i].y = static_cast<int32_t>(lroundf(yc - amp * sinf(kPi * t) * sinf(3.0f * kPi * t)));
    }

    for (int s = 0; s < SEGMENTS; ++s) {
        const int base = s * (PTS_PER_SEG - 1);

        int minX = curve[base].x, maxX = minX;
        int minY = curve[base].y, maxY = minY;
        for (int p = 1; p < PTS_PER_SEG; ++p) {
            const int32_t px = curve[base + p].x;
            const int32_t py = curve[base + p].y;
            if (px < minX) minX = px;
            if (px > maxX) maxX = px;
            if (py < minY) minY = py;
            if (py > maxY) maxY = py;
        }
        for (int p = 0; p < PTS_PER_SEG; ++p) {
            _pts[s][p].x = curve[base + p].x - minX;
            _pts[s][p].y = curve[base + p].y - minY;
        }

        lv_obj_t* line = lv_line_create(_root);
        lv_obj_remove_style_all(line);
        // Sized to the slice's own extent; the rounded caps overhang it, which
        // lv_line covers by reporting line_width as its extra draw size.
        lv_obj_set_size(line, (maxX - minX) > 0 ? (maxX - minX) : 1,
                              (maxY - minY) > 0 ? (maxY - minY) : 1);
        lv_obj_set_pos(line, minX, minY);
        lv_obj_set_style_line_width(line, cableW, 0);
        lv_obj_set_style_line_rounded(line, true, 0);
        lv_obj_set_style_line_color(line, lv_color_hex(UI_COLOR_CABLE_SLEEVE), 0);
        lv_line_set_points(line, _pts[s], PTS_PER_SEG);

        _segments[s] = line;
    }
}

void PatchCable::buildPlug(bool rightHand, int widthPx, int heightPx) {
    const int jack    = Scaled(widthPx, JACK_K);
    const int shaftL  = Scaled(widthPx, SHAFT_K)   < 4 ? 4 : Scaled(widthPx, SHAFT_K);
    const int shaftH  = Scaled(widthPx, SHAFT_H_K) < 3 ? 3 : Scaled(widthPx, SHAFT_H_K);
    const int bandOff = Scaled(widthPx, BAND_OFF_K);
    const int bandW   = Scaled(widthPx, BAND_W_K)  < 1 ? 1 : Scaled(widthPx, BAND_W_K);
    const int houseL  = Scaled(widthPx, HOUSE_K)   < 5 ? 5 : Scaled(widthPx, HOUSE_K);
    const int houseH  = Scaled(widthPx, HOUSE_H_K) < 7 ? 7 : Scaled(widthPx, HOUSE_H_K);
    const int cableW  = CableWidthPx(widthPx);
    // The relief takes whatever the plug's total length leaves, so the parts can
    // never disagree with JACK_K about where the cable starts.
    const int reliefL = (jack - shaftL - houseL) < 3 ? 3 : (jack - shaftL - houseL);
    const int reliefH = cableW + 6;
    const int radius  = houseH / 6 < 2 ? 2 : houseH / 6;
    const int yc      = heightPx / 2;

    // Everything is measured from the plug's OUTER edge inward; the right-hand
    // plug is the same run of parts mirrored, so there is one description of a
    // plug rather than two.
    auto part = [&](int fromOuter, int len, int partH, uint32_t fill, int rad) {
        const int x = rightHand ? (widthPx - fromOuter - len) : fromOuter;
        lv_obj_t* o = lv_obj_create(_root);
        lv_obj_remove_style_all(o);
        lv_obj_set_size(o, len, partH);
        lv_obj_set_pos(o, x, yc - partH / 2);
        lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(o, lv_color_hex(fill), 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(o, rad, 0);
        return o;
    };

    // A vertical gradient into a shadow tone is what makes a flat rectangle read
    // as a turned cylinder, and costs no extra objects.
    auto shade = [](lv_obj_t* o, uint32_t shadow) {
        lv_obj_set_style_bg_grad_color(o, lv_color_hex(shadow), 0);
        lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER, 0);
    };

    // Strain relief, pushed BEHIND the cable so the cable disappears into it
    // rather than having a dark blob sit on top of the lit end. It runs back
    // under the housing by one corner radius so no gap opens between them.
    lv_obj_t* relief = part(jack - reliefL - radius, reliefL + radius, reliefH, UI_COLOR_PLUG_BODY, radius);
    lv_obj_move_to_index(relief, 0);

    // The shaft likewise runs on under the housing, which is built last and
    // covers the overlap — its rounded end would otherwise leave dark wedges.
    lv_obj_t* shaft = part(0, shaftL + radius, shaftH, UI_COLOR_PLUG_METAL, shaftH / 2);
    shade(shaft, UI_COLOR_PLUG_METAL_DIM);

    // The insulator band across the shaft — the one detail that says "jack" at
    // this size, so it sits near the tip where it splits off a visible tip.
    part(bandOff, bandW, shaftH, UI_COLOR_PLUG_BAND, 0);

    // The housing carries the "signal is here" state: its border takes the hue of
    // the cable end it terminates.
    lv_obj_t* body = part(shaftL, houseL, houseH, UI_COLOR_PLUG_BODY, radius);
    shade(body, UI_COLOR_PLUG_BODY_DIM);
    lv_obj_set_style_border_width(body, UI_BORDER_NORMAL, 0);
    lv_obj_set_style_border_opa(body, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(body, lv_color_hex(UI_COLOR_BORDER), 0);

    _plugBody[rightHand ? 1 : 0] = body;
    _plugLive[rightHand ? 1 : 0] = false;
}

void PatchCable::applyPlug(int which, bool live) {
    if (which < 0 || which > 1 || !_plugBody[which]) return;
    if (_plugLive[which] == live) return;
    _plugLive[which] = live;

    // Muted against the raw hue: at full saturation the rim out-shouts the cable
    // it is reporting on.
    const uint32_t hue = DoohickeyMark::SegmentColor(which == 0 ? 0 : SEGMENTS - 1);
    const lv_color_t rim = live ? lv_color_mix(lv_color_hex(hue), lv_color_hex(UI_COLOR_BORDER), 165)
                                : lv_color_hex(UI_COLOR_BORDER);
    lv_obj_set_style_border_color(_plugBody[which], rim, 0);
}

void PatchCable::applySegment(int i, bool lit) {
    if (i < 0 || i >= SEGMENTS || !_segments[i]) return;
    const uint32_t colour = lit ? DoohickeyMark::SegmentColor(i) : UI_COLOR_CABLE_SLEEVE;
    lv_obj_set_style_line_color(_segments[i], lv_color_hex(colour), 0);
}

void PatchCable::SetLitCount(int lit) {
    if (!_root) return;
    lit = clampInt(lit, 0, SEGMENTS);
    if (lit == _lit) return;

    // Only the segments that actually changed get a style write.
    if (lit > _lit) {
        for (int i = _lit; i < lit; ++i) applySegment(i, true);
    } else {
        for (int i = lit; i < _lit; ++i) applySegment(i, false);
    }
    _lit = lit;

    // The old shimmer target has been overtaken (or rolled back) — re-baseline it
    // so a stale mix colour can't stick.
    if (_shimmerIndex >= 0 && _shimmerIndex != _lit) {
        applySegment(_shimmerIndex, _shimmerIndex < _lit);
        _shimmerIndex = -1;
    }

    applyPlug(0, _lit >= 1);
    applyPlug(1, _lit >= SEGMENTS);
}

void PatchCable::SetProgressPct(int pct) {
    pct = clampInt(pct, 0, 100);
    SetLitCount((pct * SEGMENTS + 50) / 100);
}

void PatchCable::TickShimmer(uint32_t nowMs) {
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

    const float phase = (nowMs % SHIMMER_PERIOD_MS) / static_cast<float>(SHIMMER_PERIOD_MS);
    const float wave  = 0.5f * (1.0f - cosf(phase * 2.0f * kPi));  // 0..1
    const uint8_t mix = static_cast<uint8_t>(lroundf(wave * SHIMMER_PEAK_MIX));

    const lv_color_t colour = lv_color_mix(lv_color_hex(DoohickeyMark::SegmentColor(target)),
                                           lv_color_hex(UI_COLOR_CABLE_SLEEVE), mix);
    lv_obj_set_style_line_color(_segments[target], colour, 0);
}

void PatchCable::SetHidden(bool hidden) {
    if (!_root) return;
    if (hidden) lv_obj_add_flag(_root, LV_OBJ_FLAG_HIDDEN);
    else        lv_obj_remove_flag(_root, LV_OBJ_FLAG_HIDDEN);
}

void PatchCable::Cleanup() {
    // Objects are owned by the parent; only drop our references.
    _root = nullptr;
    for (int i = 0; i < SEGMENTS; ++i) _segments[i] = nullptr;
    _plugBody[0] = _plugBody[1] = nullptr;
    _plugLive[0] = _plugLive[1] = false;
    _lit = 0;
    _shimmerIndex = -1;
}
