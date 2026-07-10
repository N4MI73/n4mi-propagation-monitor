// N4MI Desktop Instrument Series - Propagation Monitor
// screen_overview.cpp
//
// Tier selection is dynamic, not hardcoded to GOOD-as-headline: the
// screen finds whichever tier (GOOD/FAIR/POOR) actually has the most
// favorable bands present in the data and headlines that one, in that
// tier's color. A secondary line shows the next-best tier that also
// has at least one band, if any -- omitted entirely if every band
// landed in a single tier (see mock scenario 2/3 for that edge case).
//
// UPDATED 2026-07-10: real-hardware photos showed every textSize(1)
// element (header, stat labels, band lists, tower text, footer) was
// invisible -- confirmed by the pattern across three test scenarios
// (only textSize(2)+ text and primitive shapes rendered). Rewritten
// with textSize(2) as the minimum everywhere, and the headline bumped
// to textSize(5) with a fake-bold double-draw. Band name lists are now
// capped at 4 names + "+N" so a bigger font can never overflow the
// display width even when many bands share one tier. Serial debug
// prints added so the next flash confirms exactly what's being drawn.

#include "screens/screen_overview.h"
#include "display_driver.h"
#include "ui_common.h"
#include <Arduino.h>

#define MAX_BAND_NAMES_SHOWN 4

static void build_band_list(const PropMonData &data, BandCondition tier, char *out, size_t out_len) {
    out[0] = '\0';
    int shown = 0;
    int total = 0;
    for (int i = 0; i < PROPMON_BAND_COUNT; i++) {
        if (data.bands[i].condition != tier) continue;
        total++;
        if (shown >= MAX_BAND_NAMES_SHOWN) continue;
        size_t used = strlen(out);
        if (used + strlen(data.bands[i].name) + 2 >= out_len) continue;
        if (used > 0) strcat(out, " ");
        strcat(out, data.bands[i].name);
        shown++;
    }
    if (total > shown) {
        char suffix[8];
        snprintf(suffix, sizeof(suffix), " +%d", total - shown);
        size_t used = strlen(out);
        if (used + strlen(suffix) < out_len) strcat(out, suffix);
    }
}

static uint16_t tower_status_color(const char *status) {
    if (strcmp(status, "NONE") == 0)     return UI_COLOR_GOOD;
    if (strcmp(status, "CAUTION") == 0)  return UI_COLOR_FAIR;
    return UI_COLOR_POOR; // WARNING or CRITICAL
}

void screen_overview_draw(const PropMonData &data) {
    if (!gfx) return;
    display_clear();

    int counts[3] = {0, 0, 0};
    for (int i = 0; i < PROPMON_BAND_COUNT; i++) counts[data.bands[i].condition]++;

    BandCondition tier_priority[3] = {BAND_GOOD, BAND_FAIR, BAND_POOR};
    BandCondition primary_tier = BAND_POOR;
    int primary_idx = -1;
    for (int i = 0; i < 3; i++) {
        if (counts[tier_priority[i]] > 0) { primary_tier = tier_priority[i]; primary_idx = i; break; }
    }

    bool has_secondary = false;
    BandCondition secondary_tier = BAND_POOR;
    for (int i = primary_idx + 1; i < 3; i++) {
        if (counts[tier_priority[i]] > 0) { secondary_tier = tier_priority[i]; has_secondary = true; break; }
    }

    char band_list[48];

    ui_draw_centered_text("N4MI PROPMON", 35, 2, UI_COLOR_LABEL);

    build_band_list(data, primary_tier, band_list, sizeof(band_list));
    Serial.printf("[overview] primary=%s list=\"%s\"\n", ui_tier_label(primary_tier), band_list);
    ui_draw_centered_text_bold(ui_tier_label(primary_tier), 95, 5, ui_tier_color(primary_tier));
    ui_draw_centered_text(band_list, 145, 2, ui_tier_color_light(primary_tier));

    if (has_secondary) {
        build_band_list(data, secondary_tier, band_list, sizeof(band_list));
        char secondary_line[56];
        snprintf(secondary_line, sizeof(secondary_line), "%s: %s", ui_tier_label(secondary_tier), band_list);
        Serial.printf("[overview] secondary=\"%s\"\n", secondary_line);
        ui_draw_centered_text(secondary_line, 175, 2, ui_tier_color_light(secondary_tier));
    } else {
        Serial.println("[overview] no secondary tier this scenario");
    }

    gfx->drawLine(95, 200, 295, 200, UI_COLOR_DIVIDER);

    int col_x[4] = {98, 163, 228, 293};
    char valbuf[8];

    ui_draw_text_at("SFI", col_x[0], 215, 2, UI_COLOR_LABEL);
    snprintf(valbuf, sizeof(valbuf), "%d", data.sfi);
    ui_draw_text_at(valbuf, col_x[0], 240, 2, UI_COLOR_VALUE);

    ui_draw_text_at("SS", col_x[1], 215, 2, UI_COLOR_LABEL);
    snprintf(valbuf, sizeof(valbuf), "%d", data.sunspots);
    ui_draw_text_at(valbuf, col_x[1], 240, 2, UI_COLOR_VALUE);

    ui_draw_text_at("K", col_x[2], 215, 2, UI_COLOR_LABEL);
    snprintf(valbuf, sizeof(valbuf), "%d", data.k_index);
    ui_draw_text_at(valbuf, col_x[2], 240, 2, UI_COLOR_VALUE);

    ui_draw_text_at("A", col_x[3], 215, 2, UI_COLOR_LABEL);
    snprintf(valbuf, sizeof(valbuf), "%d", data.a_index);
    ui_draw_text_at(valbuf, col_x[3], 240, 2, UI_COLOR_VALUE);

    uint16_t tower_color = tower_status_color(data.tower_status);
    gfx->fillCircle(140, 284, 5, tower_color);
    gfx->setTextSize(2);
    gfx->setTextColor(tower_color);
    gfx->setCursor(152, 276);
    gfx->print(data.tower_status);

    uint32_t age_sec = (millis() - data.last_updated_ms) / 1000;
    char footer[32];
    if (age_sec < 60) {
        snprintf(footer, sizeof(footer), "Updated %lus ago", (unsigned long)age_sec);
    } else {
        snprintf(footer, sizeof(footer), "Updated %lum ago", (unsigned long)(age_sec / 60));
    }
    ui_draw_centered_text(footer, 320, 2, UI_COLOR_MUTED);
}
