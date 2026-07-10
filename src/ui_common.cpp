// N4MI Desktop Instrument Series - Propagation Monitor
// ui_common.cpp

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

void ui_draw_centered_text_bold(const char *text, int16_t y, uint8_t text_size, uint16_t color) {
    if (!gfx) return;
    gfx->setTextSize(text_size);
    gfx->setTextColor(color);
    int16_t x1, y1;
    uint16_t w, h;
    gfx->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    int16_t x = (DISPLAY_WIDTH - w) / 2;
    gfx->setCursor(x, y);
    gfx->print(text);
    gfx->setCursor(x + 1, y);
    gfx->print(text);
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
