// N4MI Desktop Instrument Series - Propagation Monitor
// screen_solar.cpp
//
// Full-detail solar screen -- surfaces two fields Overview has no room
// for (X-ray flare class, solar wind speed) alongside the four stats
// Overview already shows in compact form (SFI, sunspots, K-index,
// A-index), sized more generously here.
//
// K-index is colored using PropMon's own existing geomagnetic penalty
// thresholds (documented in the project brief: K<=2 full strength,
// K<=4 penalized, K>4 heavily penalized) -- this is the first screen
// to surface that logic visually rather than just folding it into a
// number. X-ray class coloring is new logic this screen introduces:
// PropMon sends the raw string (e.g. "C1.3"), so the leading letter is
// parsed here to classify A/B as quiet, C as minor, M/X as severe --
// PropMon itself does not pre-classify this field.
//
// Layout approved against an SVG mockup 2026-07-11 before writing this
// file, following the same mockup-first workflow as Overview and Bands.
//
// UPDATED 2026-07-17: footer color is now staleness-aware via the
// shared ui_staleness_color() helper -- shifts from muted to amber
// once data crosses STALE_DATA_THRESHOLD_MS (config.h), a lightweight
// signal that the live fetch might be failing without needing a
// dedicated error screen.

#include "screens/screen_solar.h"
#include "display_driver.h"
#include "ui_common.h"
#include <Arduino.h>

// See main.cpp for the full explanation -- set to 1 to compile in
// diagnostic Serial output, 0 (default) to compile it out entirely.
#define DEBUG_VERBOSE 0

static uint8_t k_index_tier(int k) {
    if (k <= 2) return BAND_GOOD;
    if (k <= 4) return BAND_FAIR;
    return BAND_POOR;
}

static uint8_t xray_tier(const char *xray) {
    if (!xray || xray[0] == '\0') return BAND_GOOD;
    char c = xray[0];
    if (c == 'A' || c == 'B') return BAND_GOOD;
    if (c == 'C') return BAND_FAIR;
    return BAND_POOR; // M or X class
}

void screen_solar_draw(const PropMonData &data) {
    if (!gfx) return;
    display_clear();

    ui_draw_centered_text_bold("SOLAR", 70, 2, UI_COLOR_LABEL);

    int col_x[2] = {133, 257};
    char valbuf[8];

    ui_draw_text_at("SFI", col_x[0], 114, 2, UI_COLOR_LABEL);
    snprintf(valbuf, sizeof(valbuf), "%d", data.sfi);
    ui_draw_text_at_bold(valbuf, col_x[0], 140, 3, UI_COLOR_VALUE);

    ui_draw_text_at("SUNSPOTS", col_x[1], 114, 2, UI_COLOR_LABEL);
    snprintf(valbuf, sizeof(valbuf), "%d", data.sunspots);
    ui_draw_text_at_bold(valbuf, col_x[1], 140, 3, UI_COLOR_VALUE);

    uint8_t k_tier = k_index_tier(data.k_index);
    ui_draw_text_at("K-INDEX", col_x[0], 180, 2, UI_COLOR_LABEL);
    snprintf(valbuf, sizeof(valbuf), "%d", data.k_index);
    ui_draw_text_at_bold(valbuf, col_x[0], 207, 3, ui_tier_color(k_tier));

    ui_draw_text_at("A-INDEX", col_x[1], 180, 2, UI_COLOR_LABEL);
    snprintf(valbuf, sizeof(valbuf), "%d", data.a_index);
    ui_draw_text_at_bold(valbuf, col_x[1], 207, 3, UI_COLOR_VALUE);

    uint8_t x_tier = xray_tier(data.xray);
    ui_draw_text_at("X-RAY", col_x[0], 246, 2, UI_COLOR_LABEL);
    ui_draw_text_at_bold(data.xray, col_x[0], 273, 3, ui_tier_color(x_tier));

    ui_draw_text_at("WIND KM/S", col_x[1], 246, 2, UI_COLOR_LABEL);
    snprintf(valbuf, sizeof(valbuf), "%d", (int)(data.solar_wind + 0.5f));
    ui_draw_text_at_bold(valbuf, col_x[1], 273, 3, UI_COLOR_VALUE);

#if DEBUG_VERBOSE
    if (Serial) {
        Serial.printf("[solar] k=%d (tier=%d) xray=%s (tier=%d) wind=%.1f\n",
            data.k_index, k_tier, data.xray, x_tier, data.solar_wind);
    }
#endif

    gfx->drawLine(98, 293, 293, 293, UI_COLOR_DIVIDER);

    struct { int cx; const char *label; uint16_t dot; uint16_t text; } legend[3] = {
        {75,  "QUIET",  UI_COLOR_GOOD, UI_COLOR_GOOD_LIGHT},
        {170, "MINOR",  UI_COLOR_FAIR, UI_COLOR_FAIR_LIGHT},
        {265, "SEVERE", UI_COLOR_POOR, UI_COLOR_POOR_LIGHT},
    };
    for (int i = 0; i < 3; i++) {
        gfx->fillCircle(legend[i].cx, 312, 5, legend[i].dot);
        gfx->setTextSize(2);
        gfx->setTextColor(legend[i].text);
        gfx->setCursor(legend[i].cx + 11, 304);
        gfx->print(legend[i].label);
    }

    char footer[32];
    ui_format_age(data.last_updated_ms, footer, sizeof(footer));
    ui_draw_centered_text(footer, 346, 2, ui_staleness_color(data.last_updated_ms));
}
