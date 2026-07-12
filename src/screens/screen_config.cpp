// N4MI Desktop Instrument Series - Propagation Monitor
// screen_config.cpp
//
// Matches the SVG mockup approved by Dan 2026-07-12: bold teal header,
// four label/value rows, muted footer hint. PropMon URL and "MOCK
// DATA" render in normal/bright colors since they're real; Wi-Fi and
// IP address render dim + italicized-in-spirit (this font library has
// no italic, so dim + the literal text "Not yet built" carries that
// job instead) since there's nothing real behind them yet.

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
    ui_draw_centered_text("Not yet built", 285, 2, UI_COLOR_MUTED);

    ui_draw_centered_text("IP ADDRESS", 316, 2, UI_COLOR_LABEL);
    ui_draw_centered_text("Not yet built", 339, 2, UI_COLOR_MUTED);

    ui_draw_centered_text("ROTATE TO EXIT", 368, 2, UI_COLOR_MUTED);
}
