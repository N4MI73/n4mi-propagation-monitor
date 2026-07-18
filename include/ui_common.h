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
//
// UPDATED Phase 3 (real navigation): added ui_draw_hold_progress() /
// ui_clear_hold_progress() for the long-press hold-progress bar.
// First implementation (a ring of many small drawLine() ticks) never
// rendered on real hardware -- see ui_common.cpp for the diagnosis
// (same root cause as the documented sun-icon non-rendering bug).
// Redesigned as a single-fillRect() bar to match the primitive
// pattern already proven reliable elsewhere in this codebase.

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

// Bold version of ui_draw_text_at -- centers on an arbitrary x (for
// columns/grids) with the same double-draw bold effect. Added
// 2026-07-11 after Dan reported the Solar screen's header and stat
// values were hard to read at a couple feet on the real display's
// dotted bitmap font -- the same legibility problem the Overview
// headline solved with bold, just needed here at column positions
// instead of screen-center.
void ui_draw_text_at_bold(const char *text, int16_t cx, int16_t y, uint8_t text_size, uint16_t color);

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

// Formats "Updated Xs ago" / "Updated Xm ago" from a millis()-based
// timestamp. Shared by any screen that shows a data-freshness footer --
// added here once a second screen (Bands) needed the same formatting
// Overview already had, per the project's "extract what's actually
// shared once it's actually shared" framework scope principle.
void ui_format_age(uint32_t last_updated_ms, char *out, size_t out_len);

// Returns UI_COLOR_MUTED if `last_updated_ms` is recent, or
// UI_COLOR_FAIR if it's older than STALE_DATA_THRESHOLD_MS (config.h)
// -- a lightweight visual signal that the live fetch might be failing,
// without needing a dedicated error screen. Shared by every screen's
// "Updated Xs ago" footer so the threshold lives in one place.
uint16_t ui_staleness_color(uint32_t last_updated_ms);

// Splits `msg` across two output buffers, breaking at the last space
// at or before (line1_cap-1) characters rather than mid-word. If the
// remainder still doesn't fit in line2's capacity, it's truncated with
// "...". Extracted here 2026-07-13 once the ambient alert banner
// needed the exact same two-line wrap behavior screen_alerts.cpp
// already had (as a local static helper there) -- same pattern as
// every other shared helper in this file: extract once a second
// consumer actually needs it, not before.
void ui_wrap_two_lines(const char *msg, char *line1, size_t line1_cap, char *line2, size_t line2_cap);

// Draws (or extends) the long-press hold-progress bar, positioned
// above every screen's header. `fraction` is 0.0-1.0 of the way from
// LONG_PRESS_PROGRESS_MIN_MS to LONG_PRESS_MS (see config.h). Drawn as
// a single fillRect() call per update -- deliberately not many small
// primitives (see ui_common.cpp for why: that was the first design,
// and it silently failed to render on real hardware). Cheap
// partial-region draw, never a full-screen redraw -- see Bug #1 in the
// project brief.
void ui_draw_hold_progress(float fraction);

// Erases the hold-progress bar's screen region. Safe to call even if
// no bar was ever drawn (idempotent, same draw cost either way). Call
// this once a hold resolves -- release, or a LONG_PRESS event firing --
// so a partial bar never lingers on screen into whatever's drawn next.
void ui_clear_hold_progress();

// ---------------------------------------------------------------------
// Ambient alert banner + persistent badge (added 2026-07-13)
// ---------------------------------------------------------------------
// `category` and `level` are AlertCategory's / AlertLevel's enum
// values (data_client.h) passed as plain uint8_t, matching
// ui_tier_color()'s existing pattern of keeping this header decoupled
// from data_client.h.

// Draws the ambient alert banner as a single full-width fillRect()
// band across the display's vertical center -- the widest, most
// edge-safe region of the round display, and a single primitive call
// per the "single calls render reliably, many small ones in a loop do
// not" lesson from the hold-progress bar investigation. Overlays
// whatever screen is currently shown; the caller must trigger a full
// screen redraw to erase it later (a full-width strip can't be
// cleanly cleared in place the way the small hold-progress bar can).
void ui_draw_alert_banner(uint8_t category, uint8_t level, const char *message);

// Draws a small persistent badge (single fillCircle() call) showing
// only the single worst currently-active alert -- matching how the
// Alerts screen already treats multiple simultaneous alerts
// (worst-wins), rather than showing one indicator per category.
// Position (left vs. right of center) indicates category so the
// "visually distinct by category" requirement is met without needing
// two badges at once.
void ui_draw_alert_badge(uint8_t category, uint8_t level);
