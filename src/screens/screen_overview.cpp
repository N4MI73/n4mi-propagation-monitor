// N4MI Desktop Instrument Series - Propagation Monitor
// screen_overview.cpp
//
// Tier selection is dynamic, not hardcoded to GOOD-as-headline: the
// screen finds whichever tier (GOOD/FAIR/POOR) actually has the most
// favorable bands present in the data and headlines that one, in that
// tier's color. A secondary line shows the next-best tier that also
// has at least one band, if any -- omitted entirely if every band
// landed in a single tier (see mock scenario 2/3 for that edge case).

#include "screens/screen_overview.h"
#include "display_driver.h"
#include "ui_common.h"

static void build_band_list(const PropMonData &data, BandCondition tier, char *out, size_t out_len) {
    out[0] = '\0';
    for (int i = 0; i < PROPMON_BAND_COUNT; i++) {
        if (data.bands[i].condition != tier) continue;
        size_t used = strlen(out);
        if (used + strlen(data.bands[i].name) + 2 >= out_len) break;
        if (used > 0) strcat(out, " ");
        strcat(out, data.bands[i].name);
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

    ui_draw_centered_text("N4MI PROPMON", 30, 1, UI_COLOR_LABEL);

    build_band_list(data, primary_tier, band_list, sizeof(band_list));
    ui_draw_centered_text(ui_tier_label(primary_tier), 95, 4, ui_tier_color(primary_tier));
    ui_draw_centered_text(band_list, 145, 1, ui_tier_color_light(primary_tier));

    if (has_secondary) {
        build_band_list(data, secondary_tier, band_list, sizeof(band_list));
        char secondary_line[56];
        snprintf(secondary_line, sizeof(secondary_line), "%s: %s", ui_tier_label(secondary_tier), band_list);
        ui_draw_centered_text(secondary_line, 170, 1, ui_tier_color_light(secondary_tier));
    }

    gfx->drawLine(95, 195, 295, 195, UI_COLOR_DIVIDER);

    int col_x[4] = {98, 163, 228, 293};
    char valbuf[8];
    int16_t x1, y1;
    uint16_t w, h;

    ui_draw_centered_text("SFI", 215, 1, UI_COLOR_LABEL);
    snprintf(valbuf, sizeof(valbuf), "%d", data.sfi);
    ui_draw_centered_text(valbuf, 235, 2, UI_COLOR_VALUE);

    ui_draw_sun_icon(col_x[1], 212, UI_COLOR_SUN);
    snprintf(valbuf, sizeof(valbuf), "%d", data.sunspots);
    gfx->setTextSize(2);
    gfx->setTextColor(UI_COLOR_VALUE);
    gfx->getTextBounds(valbuf, 0, 0, &x1, &y1, &w, &h);
    gfx->setCursor(col_x[1] - w / 2, 235);
    gfx->print(valbuf);

    gfx->setTextSize(1);
    gfx->setTextColor(UI_COLOR_LABEL);
    gfx->getTextBounds("K", 0, 0, &x1, &y1, &w, &h);
    gfx->setCursor(col_x[2] - w / 2, 215);
    gfx->print("K");
    snprintf(valbuf, sizeof(valbuf), "%d", data.k_index);
    gfx->setTextSize(2);
    gfx->setTextColor(UI_COLOR_VALUE);
    gfx->getTextBounds(valbuf, 0, 0, &x1, &y1, &w, &h);
    gfx->setCursor(col_x[2] - w / 2, 235);
    gfx->print(valbuf);

    gfx->setTextSize(1);
    gfx->setTextColor(UI_COLOR_LABEL);
    gfx->getTextBounds("A", 0, 0, &x1, &y1, &w, &h);
    gfx->setCursor(col_x[3] - w / 2, 215);
    gfx->print("A");
    snprintf(valbuf, sizeof(valbuf), "%d", data.a_index);
    gfx->setTextSize(2);
    gfx->setTextColor(UI_COLOR_VALUE);
    gfx->getTextBounds(valbuf, 0, 0, &x1, &y1, &w, &h);
    gfx->setCursor(col_x[3] - w / 2, 235);
    gfx->print(valbuf);

    uint16_t tower_color = tower_status_color(data.tower_status);
    gfx->fillCircle(140, 275, 4, tower_color);
    gfx->setTextSize(1);
    gfx->setTextColor(tower_color);
    gfx->setCursor(150, 271);
    gfx->print("TOWER ");
    gfx->print(data.tower_status);

    uint32_t age_sec = (millis() - data.last_updated_ms) / 1000;
    char footer[32];
    if (age_sec < 60) {
        snprintf(footer, sizeof(footer), "Updated %lus ago", (unsigned long)age_sec);
    } else {
        snprintf(footer, sizeof(footer), "Updated %lum ago", (unsigned long)(age_sec / 60));
    }
    ui_draw_centered_text(footer, 305, 1, UI_COLOR_MUTED);
}
