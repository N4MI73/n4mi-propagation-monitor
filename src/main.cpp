// N4MI Desktop Instrument Series - Propagation Monitor
// main.cpp -- Phase 3+: real live PropMon data (proof of concept)
//
// UPDATED 2026-07-15: replaces the mock-scenario stand-in with a real
// HTTP fetch against PropMon's live endpoint. Wi-Fi uses hardcoded
// credentials (wifi_credentials.h, gitignored -- see
// wifi_credentials.h.example) as a temporary stand-in for the real
// captive-portal Wi-Fi setup, which isn't built yet. Once that exists,
// only wifi_client.cpp's credential source changes -- nothing in this
// file's fetch/redraw logic should need to.
//
// Deliberately out of scope for this pass (see project brief): no
// dedicated Wi-Fi-unreachable or PropMon-unreachable screens yet. On
// any fetch failure, the device just keeps showing whatever data it
// last successfully got, mirroring PropMon's own "serve last known-
// good data on failure" design -- the existing "Updated Xs ago"
// footer is the only (passive) staleness indicator for now.
//
// Interaction model (screens/short-press/long-press meanings unchanged
// from the mock-data version -- only what SHORT_PRESS and the
// automatic timer actually fetch has changed):
//   ROTATE_CW / ROTATE_CCW -- cycle Overview -> Bands -> Solar ->
//                             Alerts -> (wraps). CW advances, CCW goes
//                             back -- confirmed correct on real
//                             hardware 2026-07-13.
//   SHORT_PRESS             -- force a real fetch from PropMon right
//                             now, in addition to the automatic
//                             periodic fetch below.
//   LONG_PRESS               -- toggle into/out of the placeholder
//                             Config screen (see screen_config.cpp).
//
// Also, independent of user interaction: an automatic fetch runs every
// LIVE_FETCH_INTERVAL_MS (config.h) so the device stays current without
// requiring a manual short press.
//
// The Bug #1 redraw-coalescing fix (drain every pending encoder event
// in one loop() pass, redraw at most once per pass) carries over
// unchanged from Phase 2/3.
//
// USB-CDC note: uses while(!Serial) to wait for monitor connection
// before printing anything -- required for ESP32-S3 native USB-CDC
// which must re-enumerate after every reset.

#include <Arduino.h>
#include "config.h"
#include "display_driver.h"
#include "encoder.h"
#include "data_client.h"
#include "wifi_client.h"
#include "ui_common.h"
#include "screens/screen_overview.h"
#include "screens/screen_bands.h"
#include "screens/screen_solar.h"
#include "screens/screen_alerts.h"
#include "screens/screen_config.h"

// Set to 1 to compile in diagnostic Serial output. Set to 0 (default)
// to compile it out of the binary entirely. Strongly recommended to
// flip to 1 for the first flash of any real network-facing change
// (like this one) -- Wi-Fi/HTTP failures are otherwise a black box
// with no clue why. See Lessons Learned in the project brief for why
// this is a compile-time toggle, not a runtime if(Serial) check.
#define DEBUG_VERBOSE 0

enum class ActiveScreen { OVERVIEW, BANDS, SOLAR, ALERTS, CONFIG };

static PropMonData current_data;
static ActiveScreen active_screen = ActiveScreen::OVERVIEW;

// Which non-Config screen was active right before a long-press entered
// Config -- rotating (or long-pressing again) while on Config returns
// here, rather than jumping to a fixed screen. The separate idle
// timeout below always returns to Overview specifically.
static ActiveScreen screen_before_config = ActiveScreen::OVERVIEW;

// Updated on every resolved encoder event (rotation, short press, or
// long press). Drives the 10s idle timeout.
static uint32_t last_interaction_ms = 0;

// True once at least one live PropMon fetch has ever succeeded.
// Deliberately never reverts to false just because a later fetch
// fails -- matches "keep showing last known-good data" applied to the
// Config screen's own Data Source label too.
static bool live_data_ever_fetched = false;

// Drives the automatic periodic fetch, independent of user interaction.
static uint32_t last_fetch_attempt_ms = 0;

// Drives the footer's "just refresh the elapsed-time text" tick.
// Updated by every redraw, regardless of what triggered it, so the
// tick only fires when nothing else has refreshed the screen recently.
static uint32_t last_redraw_ms = 0;

// ---------------------------------------------------------------------
// Ambient alert banner + persistent badge
// ---------------------------------------------------------------------
// Per Dan's decisions: the banner fires on any CHANGE in the worst
// active alert (including severity escalation, not just a brand-new
// alert from a clear state), persists across screen navigation
// (rotation doesn't dismiss it early -- the screen changes underneath
// it), and a force refresh (short press) re-shows it for a fresh
// duration even when the alert is unchanged. The persistent badge
// shows independently of the banner's own on/off state, for as long
// as any alert remains active.
#define ALERT_BANNER_DURATION_MS 9000 // ~8-10s per the brief's decided ambient-alert behavior

static bool banner_active = false;
static uint32_t banner_started_ms = 0;
static AlertCategory banner_category = ALERT_CAT_TOWER;
static AlertLevel banner_level = ALERT_NONE;
static char banner_message[48] = "";

// Tracks the worst active alert as of the last check, so the next
// check can detect a real change (new alert, escalation, or
// resolution) rather than re-triggering on every redraw.
static AlertLevel last_worst_level = ALERT_NONE;
static AlertCategory last_worst_category = ALERT_CAT_TOWER;

static const char *active_screen_name() {
    switch (active_screen) {
        case ActiveScreen::OVERVIEW: return "Overview";
        case ActiveScreen::BANDS:    return "Bands";
        case ActiveScreen::SOLAR:    return "Solar";
        case ActiveScreen::ALERTS:   return "Alerts";
        case ActiveScreen::CONFIG:   return "Config";
    }
    return "?";
}

static ActiveScreen next_screen(ActiveScreen s) {
    switch (s) {
        case ActiveScreen::OVERVIEW: return ActiveScreen::BANDS;
        case ActiveScreen::BANDS:    return ActiveScreen::SOLAR;
        case ActiveScreen::SOLAR:    return ActiveScreen::ALERTS;
        case ActiveScreen::ALERTS:   return ActiveScreen::OVERVIEW;
        default:                     return ActiveScreen::OVERVIEW; // CONFIG never reaches here
    }
}

static ActiveScreen prev_screen(ActiveScreen s) {
    switch (s) {
        case ActiveScreen::OVERVIEW: return ActiveScreen::ALERTS;
        case ActiveScreen::BANDS:    return ActiveScreen::OVERVIEW;
        case ActiveScreen::SOLAR:    return ActiveScreen::BANDS;
        case ActiveScreen::ALERTS:   return ActiveScreen::SOLAR;
        default:                     return ActiveScreen::OVERVIEW; // CONFIG never reaches here
    }
}

// Finds the single worst-severity active alert in `data.alerts[]` (by
// AlertLevel, where higher enum values are more severe -- see
// data_client.h). Returns false if no alert is active. Kept local to
// main.cpp rather than added to data_client.cpp/.h -- see prior
// session notes for why (screen_alerts.cpp's own internal logic
// wasn't available when this was first written); flagged as a likely
// but unconfirmed match with whatever that file computes internally.
static bool find_worst_alert(const PropMonData &data, AlertEntry &out_worst) {
    bool found = false;
    for (uint8_t i = 0; i < data.alert_count && i < PROPMON_MAX_ALERTS; i++) {
        if (data.alerts[i].level == ALERT_NONE) continue;
        if (!found || data.alerts[i].level > out_worst.level) {
            out_worst = data.alerts[i];
            found = true;
        }
    }
    return found;
}

// Evaluates current_data's alert state and decides whether the
// ambient banner should (re-)appear. See the banner state comment
// block above for the decided rules. `force_reshow` should only be
// true for a deliberate, successful, user-initiated fetch (short
// press) -- not for the automatic periodic fetch, which should only
// show the banner on a real change (otherwise it would resurface every
// LIVE_FETCH_INTERVAL_MS for as long as any alert stays active, which
// would be naggy rather than useful).
static void check_alert_state(bool force_reshow) {
    AlertEntry worst;
    bool found = find_worst_alert(current_data, worst);
    bool changed = found && (worst.level != last_worst_level || worst.category != last_worst_category);

    if (changed || (found && force_reshow)) {
        banner_active = true;
        banner_started_ms = millis();
        banner_category = worst.category;
        banner_level = worst.level;
        strncpy(banner_message, worst.message, sizeof(banner_message) - 1);
        banner_message[sizeof(banner_message) - 1] = '\0';
#if DEBUG_VERBOSE
        if (Serial) {
            Serial.printf("[alert] banner triggered: cat=%d level=%d msg=%s\n",
                (int)banner_category, (int)banner_level, banner_message);
        }
#endif
    }

    last_worst_level = found ? worst.level : ALERT_NONE;
    if (found) last_worst_category = worst.category;
}

// Draws the persistent badge (if any alert is currently active) and
// the temporary banner (if still within its window) on top of
// whatever the current screen just drew.
static void draw_alert_overlays() {
    if (last_worst_level != ALERT_NONE) {
        ui_draw_alert_badge((uint8_t)last_worst_category, (uint8_t)last_worst_level);
    }
    if (banner_active) {
        ui_draw_alert_banner((uint8_t)banner_category, (uint8_t)banner_level, banner_message);
    }
}

static void draw_active_screen() {
    switch (active_screen) {
        case ActiveScreen::OVERVIEW: screen_overview_draw(current_data);        break;
        case ActiveScreen::BANDS:    screen_bands_draw(current_data);           break;
        case ActiveScreen::SOLAR:    screen_solar_draw(current_data);           break;
        case ActiveScreen::ALERTS:   screen_alerts_draw(current_data);          break;
        case ActiveScreen::CONFIG:   screen_config_draw(live_data_ever_fetched); break;
    }
}

// Redraws whatever the current screen is, using whatever current_data
// already holds -- no fetch attempt. Used for plain navigation
// (rotation, long press, idle timeout, banner expiring) where the data
// itself hasn't changed, only what's being shown.
static void redraw_current() {
    draw_active_screen();
    draw_alert_overlays();
}

// Attempts a real PropMon fetch and updates current_data on success.
// On failure, current_data is left completely untouched (see
// data_client_fetch_live()'s own contract) -- the device just keeps
// showing whatever it last had, with the "Updated Xs ago" footer aging
// naturally as the passive signal that something's stale.
// `force_reshow` is passed through to check_alert_state() -- see that
// function's comment for why it's true only for a deliberate,
// successful, user-initiated fetch.
static void perform_fetch_and_update(bool force_reshow) {
    if (!wifi_client_is_connected()) {
        wifi_client_connect(); // blocks up to WIFI_CONNECT_TIMEOUT_MS
    }

    PropMonData fetched;
    bool ok = data_client_fetch_live(fetched);
    if (ok) {
        current_data = fetched;
        live_data_ever_fetched = true;
    }

    check_alert_state(ok && force_reshow);
    last_fetch_attempt_ms = millis();

#if DEBUG_VERBOSE
    if (Serial) {
        Serial.printf("[fetch] t=%lu result=%s\n", (unsigned long)millis(), ok ? "OK" : "FAILED");
    }
#endif
}

void setup() {
    Serial.begin(115200);
    unsigned long start = millis();
    while (!Serial && (millis() - start) < 3000) {
        delay(10);
    }
    delay(200);
#if DEBUG_VERBOSE
    if (Serial) {
        Serial.println();
        Serial.println("=== N4MI Propagation Monitor -- live data (proof of concept) ===");
    }
#endif

    encoder_init();
#if DEBUG_VERBOSE
    if (Serial) Serial.println("[setup] encoder_init() done");
#endif

    if (display_init()) {
#if DEBUG_VERBOSE
        if (Serial) Serial.println("[setup] display_init() OK");
#endif
        display_show_boot_message("N4MI PropMon", "Connecting Wi-Fi...");

        // Seed with a reasonable-looking placeholder before any real
        // fetch completes -- Wi-Fi connect + first fetch can take
        // several seconds, and showing all-zero/blank data during
        // that window would look broken rather than "still starting
        // up". Known rough edge: if Wi-Fi or the first fetch never
        // succeeds, this placeholder is all that ever shows, with no
        // dedicated error screen yet to say so -- deliberately
        // deferred alongside the other error-screen work in the brief.
        data_client_get_mock(current_data, 0);

        bool wifi_ok = wifi_client_connect();
        if (wifi_ok) {
            display_show_boot_message("N4MI PropMon", "Fetching data...");
            PropMonData fetched;
            if (data_client_fetch_live(fetched)) {
                current_data = fetched;
                live_data_ever_fetched = true;
            }
        }
#if DEBUG_VERBOSE
        if (Serial) {
            Serial.printf("[setup] Wi-Fi %s, live_data=%s\n",
                wifi_ok ? "connected" : "FAILED",
                live_data_ever_fetched ? "yes" : "no (showing placeholder)");
        }
#endif

        last_interaction_ms = millis();
        last_fetch_attempt_ms = millis();
        check_alert_state(false);
        redraw_current();
    } else {
#if DEBUG_VERBOSE
        if (Serial) Serial.println("[setup] display_init() FAILED");
#endif
    }
}

// See file header for the full interaction model. The Bug #1 redraw-
// coalescing pattern (drain every pending event first, redraw at most
// once per pass) is unchanged from Phase 2/3.
void loop() {
    bool need_redraw = false;
    EncoderEvent ev;

    while ((ev = encoder_poll()) != EncoderEvent::NONE) {
        last_interaction_ms = millis();

        // Any resolved event means the button is no longer mid-hold in
        // a state the progress bar needs to keep reflecting -- clear
        // it unconditionally. Cheap even when nothing was drawn.
        ui_clear_hold_progress();

        switch (ev) {
            case EncoderEvent::ROTATE_CW:
            case EncoderEvent::ROTATE_CCW: {
                if (active_screen == ActiveScreen::CONFIG) {
                    active_screen = screen_before_config;
                } else {
                    active_screen = (ev == EncoderEvent::ROTATE_CW)
                        ? next_screen(active_screen)
                        : prev_screen(active_screen);
                }
                need_redraw = true;
                break;
            }
            case EncoderEvent::SHORT_PRESS:
#if DEBUG_VERBOSE
                if (Serial) Serial.println("[encoder] SHORT PRESS -- force fetch");
#endif
                perform_fetch_and_update(true);
                need_redraw = true;
                break;
            case EncoderEvent::LONG_PRESS:
                if (active_screen == ActiveScreen::CONFIG) {
                    active_screen = screen_before_config;
                } else {
                    screen_before_config = active_screen;
                    active_screen = ActiveScreen::CONFIG;
                }
#if DEBUG_VERBOSE
                if (Serial) {
                    Serial.printf("[nav] t=%lu LONG PRESS -- now on %s\n",
                        (unsigned long)millis(), active_screen_name());
                }
#endif
                need_redraw = true;
                break;
            default:
                break;
        }
    }

    // Hold-progress bar -- reflects the button's current, still-in-
    // progress hold duration. Independent of the event-drain above.
    // Throttled to ~25 updates/sec so this doesn't add per-loop() draw
    // overhead on every single iteration.
    {
        uint32_t hold_ms = encoder_get_hold_ms();
        static uint32_t last_progress_draw_ms = 0;
        if (hold_ms >= LONG_PRESS_PROGRESS_MIN_MS && hold_ms < LONG_PRESS_MS &&
            (millis() - last_progress_draw_ms) >= 40) {
            float fraction = (float)(hold_ms - LONG_PRESS_PROGRESS_MIN_MS) /
                              (float)(LONG_PRESS_MS - LONG_PRESS_PROGRESS_MIN_MS);
            ui_draw_hold_progress(fraction);
            last_progress_draw_ms = millis();
        }
    }

    // Ambient alert banner -- clears itself after
    // ALERT_BANNER_DURATION_MS even with no further user interaction.
    if (banner_active && (millis() - banner_started_ms) >= ALERT_BANNER_DURATION_MS) {
        banner_active = false;
        need_redraw = true;
    }

    // Idle timeout -- any screen other than Overview returns there
    // after IDLE_TIMEOUT_MS with no interaction (rotation or either
    // press).
    if (active_screen != ActiveScreen::OVERVIEW &&
        (millis() - last_interaction_ms) >= IDLE_TIMEOUT_MS) {
#if DEBUG_VERBOSE
        if (Serial) Serial.println("[nav] idle timeout -- returning to Overview");
#endif
        active_screen = ActiveScreen::OVERVIEW;
        need_redraw = true;
        last_interaction_ms = millis();
    }

    // Automatic periodic fetch -- independent of user interaction, so
    // the device stays current without requiring a manual short press.
    // Note: wifi_client_connect() (called inside perform_fetch_and_update
    // if not already connected) blocks up to WIFI_CONNECT_TIMEOUT_MS,
    // which will freeze the UI for that long if Wi-Fi has dropped --
    // a known limitation, not yet fixed. Non-blocking reconnect is
    // real work better suited to the eventual captive-portal pass.
    if ((millis() - last_fetch_attempt_ms) >= LIVE_FETCH_INTERVAL_MS) {
        perform_fetch_and_update(false);
        need_redraw = true;
    }

    // Footer tick -- redraws purely to refresh the "Updated Xs ago"
    // elapsed-time text when nothing else has triggered a redraw
    // recently. Skipped on Config, which has no elapsed-time footer
    // and nothing else that benefits from redrawing on a timer.
    if (active_screen != ActiveScreen::CONFIG &&
        (millis() - last_redraw_ms) >= UI_TICK_INTERVAL_MS) {
        need_redraw = true;
    }

    if (need_redraw) {
        redraw_current();
        last_redraw_ms = millis();
    }

    delay(2);
}
