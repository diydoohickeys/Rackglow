#pragma once

#include <lvgl.h>
#include <stdint.h>

// =============================================================================
// PatchCable — a modular-synth patch lead (a plug at each end, a sagging wavy
// cable between them) as a live LVGL widget that doubles as a PROGRESS
// indicator: the cable lights one segment at a time from the left plug to the
// right, so boot reads as signal arriving at the far end.
// =============================================================================
// Geometry is expressed as thousandths of the widget WIDTH, so any panel width
// reproduces the same lead. Everything is integer math and `constexpr` on
// purpose — the caller lays text out under the cable, so it needs the height
// at compile time (HeightForWidth).
//
// The curve is y = yc - A * sin(pi t) * sin(3 pi t): zero value AND zero slope
// at both ends, so the cable leaves each plug perfectly horizontally however
// the amplitude is tuned, with two small rises either side of a deep sag.
//
// The spectrum comes from DoohickeyMark — one copy of the brand's hue order,
// not a second table (see .claude/rules/theming.md).
//
// Progress model, inherited from the brand mark's ring and load-bearing for the
// same reasons: segments change on THRESHOLD CROSSINGS, never per frame, and
// only the segments that actually changed get a style write. The one continuous
// motion is TickShimmer(), which mixes exactly ONE object's colour between the
// sleeve grey and its lit hue, and is PUMPED BY THE CALLER's own loop — an
// indicator that keeps animating after a hang is worse than none.
//
// Each segment is its own lv_line sized to ITS OWN bounding box rather than a
// full-width overlay. That is a redraw budget, not tidiness: lighting one
// segment then invalidates ~13x40 px instead of the whole lead, which is what
// keeps the fill sweep (one segment per refresh — see BootScreen) quick.
// =============================================================================

class PatchCable {
public:
    static constexpr int SEGMENTS = 16;

    // --- geometry, in thousandths of the widget width -------------------------
    // A plug is a metal SHAFT (one piece, with a dark insulator band near the
    // tip), a housing, and a strain relief the cable emerges from. Drawing the
    // tip and sleeve as two separate pills instead reads as two loose dots at
    // this size — the band is what says "jack".
    static constexpr int AMP_K       = 150;  // wave amplitude
    static constexpr int CABLE_K     = 42;   // cable thickness
    static constexpr int JACK_K      = 200;  // whole plug, tip to strain relief
    static constexpr int SHAFT_K     = 90;   // metal shaft length
    static constexpr int SHAFT_H_K   = 34;   // metal shaft diameter
    static constexpr int BAND_OFF_K  = 30;   // insulator band, from the tip
    static constexpr int BAND_W_K    = 11;   // insulator band width
    static constexpr int HOUSE_K     = 80;   // housing length
    static constexpr int HOUSE_H_K   = 88;  // housing height
    static constexpr int EDGE_PAD    = 2;    // clear space for the rounded caps

    static constexpr int Scaled(int widthPx, int k) { return (widthPx * k + 500) / 1000; }
    static constexpr int CableWidthPx(int widthPx) {
        return Scaled(widthPx, CABLE_K) < 3 ? 3 : Scaled(widthPx, CABLE_K);
    }
    /// The widget height this width needs. `constexpr` so callers can lay out
    /// against it without building the cable first.
    static constexpr int HeightForWidth(int widthPx) {
        return 2 * (Scaled(widthPx, AMP_K) + CableWidthPx(widthPx) / 2 + EDGE_PAD);
    }

    // Build at an explicit top-left with the given width; the height is
    // HeightForWidth(widthPx). Safe to call once only; Cleanup() before a rebuild.
    void BuildAt(lv_obj_t* parent, int x, int y, int widthPx);

    // Light the first `lit` segments (clamped 0..SEGMENTS). No-op when unchanged.
    void SetLitCount(int lit);

    // 0-100 -> round(pct * 16 / 100) segments. Derived from the REAL progress
    // value: one segment is NOT one boot step.
    void SetProgressPct(int pct);

    int LitCount() const { return _lit; }

    // Breathe the next-to-light segment between sleeve grey and its lit hue.
    // One style write on one object; no-op once the cable is fully lit.
    void TickShimmer(uint32_t nowMs);

    void SetHidden(bool hidden);

    // Drop retained references. The objects are freed with the parent, so this
    // never deletes.
    void Cleanup();

    bool IsBuilt() const { return _root != nullptr; }
    lv_obj_t* Root() const { return _root; }

private:
    // 4 points per segment = 48 chords across the lead; the shared endpoint is
    // carried by both neighbours so the rounded caps close the joins.
    static constexpr int PTS_PER_SEG = 4;

    void buildSegments(int widthPx, int heightPx);
    void buildPlug(bool rightHand, int widthPx, int heightPx);
    void applySegment(int i, bool lit);
    void applyPlug(int which, bool live);

    lv_obj_t* _root = nullptr;
    lv_obj_t* _segments[SEGMENTS] = { nullptr };
    lv_obj_t* _plugBody[2] = { nullptr, nullptr };  // 0 = left (source), 1 = right

    // lv_line does NOT copy its point array — it must outlive the widget.
    lv_point_precise_t _pts[SEGMENTS][PTS_PER_SEG] = {};

    int _lit = 0;
    int _shimmerIndex = -1;
    bool _plugLive[2] = { false, false };
};
