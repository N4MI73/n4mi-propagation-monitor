// N4MI Desktop Instrument Series - Propagation Monitor
// ui_common.h -- shared display colors and drawing helpers
//
// Colors approved 2026-07-05 against a visual mockup of the Overview
// screen. Each tier (GOOD/FAIR/POOR) has a medium shade for headline
// use and a lighter shade for supporting/secondary text, so any screen
// can render "this tier is primary" vs. "this tier is secondary" with
// the same two-color pattern.
//
// UPDATED 2026-07-10: real-hardware testing showed every textSize(1)
// element (header, stat labels, band lists, tower text, footer) was
// completely invisible on the actual AMOLED, while textSize(2)+ and
// primitive shape draws (circles/lines) rendered fine. Minimum text
// size going forward is 2. UI_COLOR_LABEL also brightened slightly
// since it's now doing more visual work at the larger size.

#pragma once

#include "display_driver.h"

// GOOD tier (teal)
static const uint16_t UI_COLOR_GOOD       = RGB565(93, 202, 165);
static const uint16_t UI_COLOR_GOOD_LIGHT = RGB565(159, 225, 203);

// FAIR tier (amber)
static const uint16_t UI_COLOR_FAIR       = RGB565(239, 159, 39);
static const uint16_t UI_COLOR_FAIR_LIGHT = RGB565(250, 199, 117);

// POOR tier (red)
static const uint16_t UI_COLOR_POOR       = RGB565(226, 75, 74);
static const uint16_t UI_COLOR_POOR_LIGHT = RGB565(240, 149, 149);

// Chrome / neutral
static const uint16_t UI_COLOR_LABEL   = RGB565(150, 150, 150);
static const uint16_t UI_COLOR_VALUE   = RGB565(232, 232, 232);
static const uint16_t UI_COLOR_MUTED   = RGB565(110, 110, 110);
static const uint16_t UI_COLOR_DIVIDER = RGB565(50, 50, 50);
static const uint16_t UI_COLOR_SUN     = RGB565(239, 159, 39);

// Draws text horizontally centered on the display at the given cursor y.
// Minimum text_size is 2 -- size 1 has been confirmed invisible on the
// real hardware/library combination in use.
void ui_draw_centered_text(const char *text, int16_t y, uint8_t text_size, uint16_t color);

// Same as ui_draw_centered_text, but centers on an arbitrary x rather
// than assuming the full display width -- use this for anything laid
// out in columns (e.g. the stat row) so every label/value in a column
// shares one exact center point instead of each being centered
// independently on the whole screen.
void ui_draw_text_at(const char *text, int16_t cx, int16_t y, uint8_t text_size, uint16_t color);

// Same as above, but simulates a bold weight by drawing the text twice,
// offset one pixel horizontally -- this font library has no true bold.
// Intended for headline-weight text (tier word).
void ui_draw_centered_text_bold(const char *text, int16_t y, uint8_t text_size, uint16_t color);

// Draws a small sun glyph (circle + rays) centered at (cx, cy). Size-
// independent since it's primitive shape drawing, not text.
void ui_draw_sun_icon(int16_t cx, int16_t cy, uint16_t color);

// Returns the headline/medium color for a given band tier. Takes an
// int rather than BandCondition to keep this header decoupled from
// data_client.h -- callers pass BAND_GOOD/BAND_FAIR/BAND_POOR values.
uint16_t ui_tier_color(uint8_t tier);

// Returns the lighter/supporting-text color for a given band tier.
uint16_t ui_tier_color_light(uint8_t tier);

// Returns the display label for a given band tier ("GOOD"/"FAIR"/"POOR").
const char *ui_tier_label(uint8_t tier);
