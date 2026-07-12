// N4MI Desktop Instrument Series - Propagation Monitor
// ui_common.cpp

#include <math.h>
#include "ui_common.h"
#include "data_client.h"

void ui_draw_text_at(const char *text, int16_t cx, int16_t y, uint8_t text_size, uint16_t color) {
    if (!gfx) return;
    gfx->setTextSize(text_size);
    gfx->setTextColor(color);
    int16_t x1, y1;
    uint16_t w, h;
    gfx->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    gfx->setCursor(cx - w / 2, y);
    gfx->print(text);
}

void ui_draw_centered_text(const char *text, int16_t y, uint8_t text_size, uint16_t color) {
    ui_draw_text_at(text, DISPLAY_WIDTH / 2, y, text_size, color);
}

void ui_draw_text_at_bold(const char *text, int16_t cx, int16_t y, uint8_t text_size, uint16_t color) {
    if (!gfx) return;
    gfx->setTextSize(text_size);
    gfx->setTextColor(color);
    int16_t x1, y1;
    uint16_t w, h;
    gfx->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    int16_t x = cx - w / 2;
    gfx->setCursor(x, y);
    gfx->print(text);
    gfx->setCursor(x + 1, y);
    gfx->print(text);
}

void ui_draw_centered_text_bold(const char *text, int16_t y, uint8_t text_size, uint16_t color) {
    ui_draw_text_at_bold(text, DISPLAY_WIDTH / 2, y, text_size, color);
}

void ui_draw_sun_icon(int16_t cx, int16_t cy, uint16_t color) {
    if (!gfx) return;
    gfx->drawCircle(cx, cy, 4, color);
    gfx->drawLine(cx, cy - 8, cx, cy - 6, color);
    gfx->drawLine(cx, cy + 6, cx, cy + 8, color);
    gfx->drawLine(cx - 8, cy, cx - 6, cy, color);
    gfx->drawLine(cx + 6, cy, cx + 8, cy, color);
    gfx->drawLine(cx - 6, cy - 6, cx - 5, cy - 5, color);
    gfx->drawLine(cx + 5, cy + 5, cx + 6, cy + 6, color);
    gfx->drawLine(cx + 6, cy - 6, cx + 5, cy - 5, color);
    gfx->drawLine(cx - 5, cy + 5, cx - 6, cy + 6, color);
}

uint16_t ui_tier_color(uint8_t tier) {
    switch (tier) {
        case BAND_GOOD: return UI_COLOR_GOOD;
        case BAND_FAIR: return UI_COLOR_FAIR;
        default:        return UI_COLOR_POOR;
    }
}

uint16_t ui_tier_color_light(uint8_t tier) {
    switch (tier) {
        case BAND_GOOD: return UI_COLOR_GOOD_LIGHT;
        case BAND_FAIR: return UI_COLOR_FAIR_LIGHT;
        default:        return UI_COLOR_POOR_LIGHT;
    }
}

const char *ui_tier_label(uint8_t tier) {
    switch (tier) {
        case BAND_GOOD: return "GOOD";
        case BAND_FAIR: return "FAIR";
        default:        return "POOR";
    }
}

void ui_format_age(uint32_t last_updated_ms, char *out, size_t out_len) {
    uint32_t age_sec = (millis() - last_updated_ms) / 1000;
    if (age_sec < 60) {
        snprintf(out, out_len, "Updated %lus ago", (unsigned long)age_sec);
    } else {
        snprintf(out, out_len, "Updated %lum ago", (unsigned long)(age_sec / 60));
    }
}

// ---------------------------------------------------------------------
// Long-press hold-progress bar (added Phase 3 -- real navigation)
// ---------------------------------------------------------------------
// UPDATED 2026-07-12 (second pass): the original design drew the ring
// as up to 90 short drawLine() calls fired in a tight loop per update.
// Confirmed via Serial diagnostic that the draw calls were executing
// correctly (fraction climbed smoothly 0.00 -> 0.97 every time, no
// stutters -- also incidentally confirming the earlier hold-timing
// concern in encoder.cpp was not a real debounce bug), yet nothing
// ever appeared on the physical display. This matches the project's
// own previously-documented sun-icon non-rendering bug exactly --
// ui_draw_sun_icon() is one drawCircle() + eight drawLine() calls
// fired back-to-back, and it silently failed to render too (worked
// around with a text label instead). Everything that *does* render
// reliably across every screen shares one trait: it's always a single
// primitive call, not a rapid burst of many -- the Config screen's
// divider line (one drawLine call, confirmed visible in real hardware
// photos), display_clear()'s fillScreen(), and all text glyphs.
// Rather than continue chasing many-small-primitive rendering (which
// may be a genuine QSPI-transaction-rate quirk in this display/library
// combination, still unconfirmed but now supported by two independent
// occurrences), this redesigns the indicator as a single growing bar
// -- one fillRect() call per update, matching the exact primitive
// pattern already proven reliable everywhere else in this codebase.
//
// Positioned above every screen's header (all screens' headers start
// around y=82 per the shared visual grammar) rather than near the
// outer rim -- deliberately not near the display edge this time,
// since the edge-visibility question from the first pass is now moot
// if this location clears every screen's own content with margin.
// Flag to Dan if this overlaps anything on real hardware; position is
// reasoned, not yet hardware-confirmed.
static const int16_t HOLD_BAR_X = 145;
static const int16_t HOLD_BAR_Y = 30;
static const int16_t HOLD_BAR_W = 100;
static const int16_t HOLD_BAR_H = 6;

void ui_draw_hold_progress(float fraction) {
    if (!gfx) return;
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    int16_t fill_w = (int16_t)(HOLD_BAR_W * fraction);
    if (fill_w > 0) {
        gfx->fillRect(HOLD_BAR_X, HOLD_BAR_Y, fill_w, HOLD_BAR_H, UI_COLOR_GOOD);
    }
}

void ui_clear_hold_progress() {
    if (!gfx) return;
    gfx->fillRect(HOLD_BAR_X, HOLD_BAR_Y, HOLD_BAR_W, HOLD_BAR_H, RGB565(0, 0, 0));
}
