// N4MI Desktop Instrument Series - Propagation Monitor
// screen_config.cpp
//
// Matches the SVG mockup approved by Dan 2026-07-12: bold teal header,
// four label/value rows, muted footer hint.
//
// UPDATED 2026-07-13: footer text ("ROTATE TO EXIT") was clipped at
// the bottom edge -- the bottom three rows (Wi-Fi, IP address, footer)
// were tightened and pulled up to fix it. Header through Data Source
// untouched, since those were never reported as a problem.
//
// UPDATED 2026-07-15: Wi-Fi and IP address are no longer "Not yet
// built" placeholders -- real Wi-Fi now exists (hardcoded credentials
// as a temporary stand-in for the real captive-portal setup, which
// isn't built yet), so this screen shows the actual connection status
// and IP directly from WiFi.status()/WiFi.localIP(). Data source line
// is dynamic: "MOCK DATA" (amber, matching the original placeholder
// treatment) until the first successful live fetch, "LIVE" (teal)
// after -- see screen_config.h for why that state lives in main.cpp
// rather than here.

#include "screens/screen_config.h"
#include "display_driver.h"
#include "ui_common.h"
#include "config.h"
#include <WiFi.h>

void screen_config_draw(bool live_data_active) {
    if (!gfx) return;
    display_clear();

    ui_draw_centered_text_bold("CONFIG", 82, 3, UI_COLOR_GOOD);
    gfx->drawLine(130, 98, 260, 98, UI_COLOR_DIVIDER);

    char url[40];
    snprintf(url, sizeof(url), "%s:%d", PROPMON_DEFAULT_HOST, PROPMON_DEFAULT_PORT);

    ui_draw_centered_text("PROPMON URL", 138, 2, UI_COLOR_LABEL);
    ui_draw_centered_text_bold(url, 163, 2, UI_COLOR_VALUE);

    ui_draw_centered_text("DATA SOURCE", 200, 2, UI_COLOR_LABEL);
    if (live_data_active) {
        ui_draw_centered_text_bold("LIVE", 225, 2, UI_COLOR_GOOD);
    } else {
        ui_draw_centered_text_bold("MOCK DATA", 225, 2, UI_COLOR_FAIR);
    }

    bool wifi_connected = (WiFi.status() == WL_CONNECTED);

    ui_draw_centered_text("WI-FI", 262, 2, UI_COLOR_LABEL);
    if (wifi_connected) {
        ui_draw_centered_text("Connected", 280, 2, UI_COLOR_GOOD_LIGHT);
    } else {
        ui_draw_centered_text("Disconnected", 280, 2, UI_COLOR_POOR_LIGHT);
    }

    ui_draw_centered_text("IP ADDRESS", 302, 2, UI_COLOR_LABEL);
    if (wifi_connected) {
        char ip_str[16];
        IPAddress ip = WiFi.localIP();
        snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        ui_draw_centered_text(ip_str, 320, 2, UI_COLOR_VALUE);
    } else {
        ui_draw_centered_text("N/A", 320, 2, UI_COLOR_MUTED);
    }

    ui_draw_centered_text("ROTATE TO EXIT", 342, 2, UI_COLOR_MUTED);
}
