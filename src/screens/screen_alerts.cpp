// N4MI Desktop Instrument Series - Propagation Monitor
// screen_alerts.cpp
//
// Deliberately mirrors Overview's visual grammar rather than building a
// dense scrollable list: headline the single worst active alert (by
// severity), show its category and message underneath, note "+N more"
// if others exist. Full detail for those additional alerts lives in
// PropMon's own JSON endpoint today, and will live in the Phase 3 web
// config portal later -- not on this screen. Decided 2026-07-11 after
// weighing the alternative of letting the knob page through alerts on
// this screen only, which would have broken the "rotation always
// cycles screens" rule the other three screens rely on.
//
// PropMon's steady state (no real alerts active) is a single NONE-level
// placeholder entry (e.g. "No active alerts"), per the real JSON sample
// in the project brief -- not an empty array. That case renders as a
// calm "ALL CLEAR" headline, matching the project's established
// "deliberately calm, not alarming" tone for non-problem states.
//
// Severity-to-color mapping reuses screen_overview.cpp's existing
// tower_status_color() logic exactly (NONE=good, CAUTION=fair,
// WARNING/CRITICAL=poor) for consistency -- WARNING and CRITICAL both
// render in the POOR/red color, distinguished by the headline word
// itself rather than a fourth color tier.
//
// Coordinates deliberately reuse Overview's exact proven layout rhythm
// (header y=35, headline y=95 size5 bold, secondary lines at 145/175,
// footer y=320) since this screen plays the same "single dynamic
// headline + supporting detail" role Overview does, just without a
// stat row -- so there's extra vertical room used here for the
// wrapped message text instead.
//
// UPDATED 2026-07-13: the local two-line message wrap (previously a
// static split_message() helper here) moved to ui_common.cpp as
// ui_wrap_two_lines() once the ambient alert banner needed the exact
// same wrap behavior -- logic itself is unchanged, just relocated so
// both consumers share one proven implementation instead of risking
// two that quietly diverge.

#include "screens/screen_alerts.h"
#include "display_driver.h"
#include "ui_common.h"
#include <Arduino.h>

// See main.cpp for the full explanation -- set to 1 to compile in
// diagnostic Serial output, 0 (default) to compile it out entirely.
#define DEBUG_VERBOSE 0

static uint8_t alert_level_tier(AlertLevel level) {
    switch (level) {
        case ALERT_NONE:     return BAND_GOOD;
        case ALERT_CAUTION:  return BAND_FAIR;
        default:             return BAND_POOR; // WARNING or CRITICAL
    }
}

static const char *alert_level_label(AlertLevel level) {
    switch (level) {
        case ALERT_NONE:     return "ALL CLEAR";
        case ALERT_CAUTION:  return "CAUTION";
        case ALERT_WARNING:  return "WARNING";
        default:              return "CRITICAL";
    }
}

static const char *alert_category_label(AlertCategory cat) {
    return (cat == ALERT_CAT_TOWER) ? "TOWER" : "PROPAGATION";
}

void screen_alerts_draw(const PropMonData &data) {
    if (!gfx) return;
    display_clear();

    ui_draw_centered_text_bold("ALERTS", 35, 2, UI_COLOR_LABEL);

    // Find the worst (highest-severity) active alert.
    int worst_idx = -1;
    AlertLevel worst_level = ALERT_NONE;
    for (int i = 0; i < data.alert_count; i++) {
        if (data.alerts[i].level >= worst_level) {
            worst_level = data.alerts[i].level;
            worst_idx = i;
        }
    }

    if (worst_idx < 0 || worst_level == ALERT_NONE) {
#if DEBUG_VERBOSE
        if (Serial) Serial.println("[alerts] all clear");
#endif
        ui_draw_centered_text_bold("ALL CLEAR", 95, 5, UI_COLOR_GOOD);
        ui_draw_centered_text("No active alerts", 145, 2, UI_COLOR_GOOD_LIGHT);
    } else {
        const AlertEntry &worst = data.alerts[worst_idx];
        uint8_t tier = alert_level_tier(worst.level);

#if DEBUG_VERBOSE
        if (Serial) {
            Serial.printf("[alerts] worst=%s category=%s msg=\"%s\" count=%d\n",
                alert_level_label(worst.level), alert_category_label(worst.category),
                worst.message, data.alert_count);
        }
#endif

        ui_draw_centered_text_bold(alert_level_label(worst.level), 95, 5, ui_tier_color(tier));
        ui_draw_centered_text(alert_category_label(worst.category), 145, 2, ui_tier_color_light(tier));

        char line1[24], line2[24];
        ui_wrap_two_lines(worst.message, line1, sizeof(line1), line2, sizeof(line2));
        ui_draw_centered_text(line1, 180, 2, UI_COLOR_VALUE);
        if (line2[0] != '\0') {
            ui_draw_centered_text(line2, 205, 2, UI_COLOR_VALUE);
        }

        if (data.alert_count > 1) {
            char more[24];
            snprintf(more, sizeof(more), "+%d more alert%s", data.alert_count - 1,
                (data.alert_count - 1) == 1 ? "" : "s");
            ui_draw_centered_text(more, 245, 2, UI_COLOR_MUTED);
        }
    }

    char footer[32];
    ui_format_age(data.last_updated_ms, footer, sizeof(footer));
    ui_draw_centered_text(footer, 320, 2, UI_COLOR_MUTED);
}
