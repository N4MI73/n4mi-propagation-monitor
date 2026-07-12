// N4MI Desktop Instrument Series - Propagation Monitor
// screen_config.cpp
//
// Matches the SVG mockup approved by Dan 2026-07-12: bold teal header,
// four label/value rows, muted footer hint. PropMon URL and "MOCK
// DATA" render in normal/bright colors since they're real; Wi-Fi and
// IP address render dim since there's nothing real behind them yet.
//
// UPDATED 2026-07-12 (later same day): the footer's "ROTATE TO EXIT"
// text was clipped at the bottom edge in real-hardware photos. At
// y=368 with text_size 2, the glyph's bottom edge lands around y=384
// -- inside the same tight physical edge margin the hold-progress bar
// investigation identified (roughly the outer ~15-20px of the 390px
// display is not reliably visible). The "IP ADDRESS: Not yet built"
// line just above it, ending around y=355, was never reported as
// clipped -- so the bottom three rows (Wi-Fi, IP address, footer)
// were tightened and pulled up to keep the footer's bottom edge well
// inside that confirmed-safe boundary. Header through Data Source
// unchanged -- those were never reported as a problem.

#include "screens/screen_config.h"
#include "display_driver.h"
#include "ui_common.h"
#include "config.h"

void screen_config_draw() {
    if (!gfx) return;
    display_clear();

    ui_draw_centered_text_bold("CONFIG", 82, 3, UI_COLOR_GOOD);
    gfx->drawLine(130, 98, 260, 98, UI_COLOR_DIVIDER);

    char url[40];
    snprintf(url, sizeof(url), "%s:%d", PROPMON_DEFAULT_HOST, PROPMON_DEFAULT_PORT);

    ui_draw_centered_text("PROPMON URL", 138, 2, UI_COLOR_LABEL);
    ui_draw_centered_text_bold(url, 163, 2, UI_COLOR_VALUE);

    ui_draw_centered_text("DATA SOURCE", 200, 2, UI_COLOR_LABEL);
    ui_draw_centered_text_bold("MOCK DATA", 225, 2, UI_COLOR_FAIR);

    ui_draw_centered_text("WI-FI", 262, 2, UI_COLOR_LABEL);
    ui_draw_centered_text("Not yet built", 280, 2, UI_COLOR_MUTED);

    ui_draw_centered_text("IP ADDRESS", 302, 2, UI_COLOR_LABEL);
    ui_draw_centered_text("Not yet built", 320, 2, UI_COLOR_MUTED);

    ui_draw_centered_text("ROTATE TO EXIT", 342, 2, UI_COLOR_MUTED);
}
