// N4MI Desktop Instrument Series - Propagation Monitor
// screen_bands.cpp
//
// Shows all 10 bands individually, in fixed 160m->6m order (matching
// PropMon's own JSON ordering everywhere else in the project), each
// band name colored by its own tier -- unlike Overview, there's no
// truncation or "+N" here since showing every band individually is
// this screen's whole purpose. A small color-key legend and the same
// "Updated Xs ago" footer used on Overview keep it visually consistent
// with the rest of the instrument.
//
// Layout approved against an SVG mockup 2026-07-10 before writing this
// file -- fixed band order (not grouped by tier) was Dan's explicit
// choice, so a given band's position on screen never moves session to
// session regardless of current conditions.

#include "screens/screen_bands.h"
#include "display_driver.h"
#include "ui_common.h"
#include <Arduino.h>

// See main.cpp for the full explanation -- set to 1 to compile in
// diagnostic Serial output, 0 (default) to compile it out entirely.
#define DEBUG_VERBOSE 0

void screen_bands_draw(const PropMonData &data) {
    if (!gfx) return;

    uint32_t t_start = millis();
    display_clear();
    uint32_t t_clear = millis();

    ui_draw_centered_text("BANDS", 70, 2, UI_COLOR_LABEL);

    int row_y[5] = {115, 155, 195, 235, 275};
    int col_x[2] = {142, 248};

    for (int i = 0; i < PROPMON_BAND_COUNT; i++) {
        int row = i / 2;
        int col = i % 2;
        uint16_t color = ui_tier_color(data.bands[i].condition);
        ui_draw_text_at(data.bands[i].name, col_x[col], row_y[row], 2, color);
    }

    gfx->drawLine(107, 301, 283, 301, UI_COLOR_DIVIDER);

    struct { int cx; const char *label; uint16_t dot; uint16_t text; } legend[3] = {
        {75,  "GOOD", UI_COLOR_GOOD, UI_COLOR_GOOD_LIGHT},
        {170, "FAIR", UI_COLOR_FAIR, UI_COLOR_FAIR_LIGHT},
        {265, "POOR", UI_COLOR_POOR, UI_COLOR_POOR_LIGHT},
    };
    for (int i = 0; i < 3; i++) {
        gfx->fillCircle(legend[i].cx, 323, 5, legend[i].dot);
        gfx->setTextSize(2);
        gfx->setTextColor(legend[i].text);
        gfx->setCursor(legend[i].cx + 11, 315);
        gfx->print(legend[i].label);
    }

    char footer[32];
    ui_format_age(data.last_updated_ms, footer, sizeof(footer));
    ui_draw_centered_text(footer, 354, 2, UI_COLOR_MUTED);

    uint32_t t_end = millis();
#if DEBUG_VERBOSE
    if (Serial) {
        Serial.printf("[timing][bands] clear=%lums total=%lums\n",
            (unsigned long)(t_clear - t_start),
            (unsigned long)(t_end - t_start));
    }
#endif
}
