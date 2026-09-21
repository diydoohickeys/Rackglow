#include "ui/BootScreen.h"
#include "rackglow.h"

#include <Arduino.h>
#include <Logger.h>

namespace {

// Fixed-pixel layout for the 320x480 portrait panel. Horizontal placement is
// LV_ALIGN_TOP_MID throughout, so only the vertical budget is hard-coded.
constexpr int NAME_Y = 88;
constexpr int SUB_Y  = 140;

constexpr int CABLE_Y           = 186;
constexpr int CABLE_MAX_W       = 296;
constexpr int CABLE_SIDE_MARGIN = 12;   // minimum clear space either side
constexpr int CABLE_GAP         = 42;   // clear space under the lead

// The text rows are DERIVED from the lead's MAXIMUM height rather than its actual
// one, so a narrower panel that clamps the lead just gets extra clearance instead
// of silently overlapping the status line.
constexpr int CABLE_MAX_H = PatchCable::HeightForWidth(CABLE_MAX_W);

constexpr int STATUS_Y  = CABLE_Y + CABLE_MAX_H + CABLE_GAP;
constexpr int IP_Y      = STATUS_Y + 32;
constexpr int DETAILS_Y = IP_Y + 24;

// All-caps at these sizes reads cramped without tracking; the tagline is tracked
// harder still so it reads as a rule under the name rather than as a second line
// of equal weight (which the dimmed opacity reinforces).
constexpr int NAME_TRACKING = 8;
constexpr int SUB_TRACKING  = 7;
constexpr lv_opa_t TAGLINE_OPA = 160;

// The WiFi phase is the whole progress budget — see the header. Connection
// attempts creep toward CONNECT_MAX so a slow associate still shows movement
// without the cable reaching the far plug before it actually has.
constexpr int PROGRESS_SCAN_START  = 6;
constexpr int PROGRESS_SCAN_DONE   = 19;
constexpr int PROGRESS_CONNECTING  = 31;
constexpr int PROGRESS_CONNECT_STEP = 6;
constexpr int PROGRESS_CONNECT_MAX = 88;
constexpr int PROGRESS_READY       = 100;

// 🚨 The final fill must go one segment PER REFRESH, never as a batch.
// The partial draw buffer (Display::DRAW_BUF_ROWS) repaints an invalidated region as bands
// strictly top to bottom, so several segments lit inside one refresh appear in
// SCANLINE order rather than along the cable — the two crests light before the
// sag between them, and the lead fills in three places at once instead of
// running end to end. A fast join makes that an 11-segment batch.
// Cost is ~(16 - lit) x (one segment repaint + this delay); it is the one number
// to tune if the flourish feels slow.
constexpr uint32_t FILL_STEP_MS = 18;

int cableWidth(int screenWidth) {
    const int available = screenWidth - 2 * CABLE_SIDE_MARGIN;
    return (available < CABLE_MAX_W) ? available : CABLE_MAX_W;
}

}  // namespace

BootScreen::~BootScreen() {
    cleanup();
}

bool BootScreen::initialize() {
    if (_initialized) {
        return true;
    }

    createUI();
    _initialized = true;
    Logger.info("BootScreen: branded boot screen up");
    return true;
}

void BootScreen::cleanup() {
    if (!_initialized) {
        return;
    }

    // Drop the lead's references BEFORE the container goes — its objects are
    // children of _root, so they die with it and a retained pointer would dangle.
    _cable.Cleanup();

    if (_root) {
        lv_obj_delete(_root);
        _root = nullptr;
    }

    _nameLabel = nullptr;
    _taglineLabel = nullptr;
    _statusLabel = nullptr;
    _ipLabel = nullptr;
    _detailsLabel = nullptr;
    _initialized = false;
}

lv_obj_t* BootScreen::addRow(const char* text, const lv_font_t* font, uint32_t color, int y) {
    lv_obj_t* label = lv_label_create(_root);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
    return label;
}

void BootScreen::createUI() {
    lv_obj_t* screen = lv_screen_active();
    const int32_t width = lv_display_get_horizontal_resolution(lv_display_get_default());

    // One full-screen container holds the lot, so cleanup() is a single delete and
    // no styling is stranded on the screen the tabview is built on afterwards.
    _root = lv_obj_create(screen);
    lv_obj_remove_style_all(_root);
    lv_obj_set_size(_root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(_root, 0, 0);
    lv_obj_remove_flag(_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(_root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(_root, lv_color_hex(UI_COLOR_BACKGROUND), 0);
    lv_obj_set_style_bg_opa(_root, LV_OPA_COVER, 0);

    _nameLabel = addRow("RACKGLOW", &lv_font_montserrat_32, UI_COLOR_TEXT, NAME_Y);
    lv_obj_set_style_text_letter_space(_nameLabel, NAME_TRACKING, 0);

    _taglineLabel = addRow("LIGHT & SOUND", &lv_font_montserrat_14, UI_COLOR_PRIMARY, SUB_Y);
    lv_obj_set_style_text_letter_space(_taglineLabel, SUB_TRACKING, 0);
    // text_opa, NOT whole-object opa: object opacity would make the label a
    // composited layer (lvgl.md).
    lv_obj_set_style_text_opa(_taglineLabel, TAGLINE_OPA, 0);

    const int leadWidth = cableWidth(static_cast<int>(width));
    _cable.BuildAt(_root, (static_cast<int>(width) - leadWidth) / 2, CABLE_Y, leadWidth);

    _statusLabel  = addRow("Starting...", &lv_font_montserrat_22, UI_COLOR_TEXT, STATUS_Y);
    _ipLabel      = addRow("", &lv_font_montserrat_14, UI_COLOR_PRIMARY, IP_Y);
    _detailsLabel = addRow("", &lv_font_montserrat_12, UI_COLOR_TEXT_MUTED, DETAILS_Y);

    refresh();
}

void BootScreen::refresh() {
    // begin() blocks the loop that would drive lv_timer_handler, so every update
    // during the WiFi phase has to force its own redraw.
    lv_refr_now(NULL);
}

void BootScreen::updateStatus(const String& status) {
    if (!_statusLabel) return;
    lv_label_set_text(_statusLabel, status.c_str());
    // Re-align after the text change so the row re-centres at its new width.
    lv_obj_align(_statusLabel, LV_ALIGN_TOP_MID, 0, STATUS_Y);
}

void BootScreen::updateDetails(const String& details) {
    if (!_detailsLabel) return;
    lv_label_set_text(_detailsLabel, details.c_str());
    lv_obj_align(_detailsLabel, LV_ALIGN_TOP_MID, 0, DETAILS_Y);
}

void BootScreen::setProgress(int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    _progress = pct;
    _cable.SetProgressPct(pct);
}

void BootScreen::sweepProgressTo(int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    const int target = (pct * PatchCable::SEGMENTS + 50) / 100;
    while (_cable.LitCount() < target) {
        _cable.SetLitCount(_cable.LitCount() + 1);
        refresh();
        delay(FILL_STEP_MS);
    }

    // Lands exactly, and covers a downward move (which the loop above can't make).
    _cable.SetProgressPct(pct);
    _progress = pct;
    refresh();
}

void BootScreen::setStatus(const char* status) {
    if (!_initialized || !status) return;
    updateStatus(status);
    _cable.TickShimmer(millis());
    refresh();
}

void BootScreen::tick() {
    if (!_initialized) return;
    _cable.TickShimmer(millis());
}

void BootScreen::onScanStart() {
    setProgress(PROGRESS_SCAN_START);
    updateStatus("Scanning");
    updateDetails("Looking for networks");
    refresh();
}

void BootScreen::onScanComplete(int networks) {
    setProgress(PROGRESS_SCAN_DONE);
    updateDetails(networks > 0 ? String(networks) + " networks found" : String("No networks found"));
    refresh();
}

void BootScreen::onConnecting(const String& ssid) {
    setProgress(PROGRESS_CONNECTING);
    updateStatus("Connecting");
    updateDetails(ssid);
    refresh();
}

void BootScreen::onConnectionProgress() {
    if (_progress < PROGRESS_CONNECT_MAX) {
        int next = _progress + PROGRESS_CONNECT_STEP;
        if (next > PROGRESS_CONNECT_MAX) next = PROGRESS_CONNECT_MAX;
        setProgress(next);
    }
    _cable.TickShimmer(millis());
    refresh();
}

void BootScreen::onConnected(IPAddress ip) {
    // Text first, then the cable runs home under it — the sweep's own refreshes
    // paint both, and "Connected" landing before the flourish reads right.
    updateStatus("Connected");
    if (_ipLabel) {
        lv_label_set_text(_ipLabel, ip.toString().c_str());
        lv_obj_align(_ipLabel, LV_ALIGN_TOP_MID, 0, IP_Y);
    }
    updateDetails(_deviceName.length() > 0 ? _deviceName + ".local" : String(""));
    sweepProgressTo(PROGRESS_READY);
}

void BootScreen::onAPMode(const String& apName, IPAddress ip) {
    updateStatus("Setup Mode");
    if (_ipLabel) {
        lv_label_set_text(_ipLabel, ip.toString().c_str());
        lv_obj_align(_ipLabel, LV_ALIGN_TOP_MID, 0, IP_Y);
    }
    // String() on the left: Arduino has no operator+(const char*, const String&).
    updateDetails(String("Join \"") + apName + "\" to configure");
    // Not progress but a stable terminal state — a lead patched end to end reads
    // as "ready, waiting for you" rather than as a stalled boot.
    sweepProgressTo(PROGRESS_READY);
}
