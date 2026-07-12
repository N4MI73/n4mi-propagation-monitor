// N4MI Desktop Instrument Series - Propagation Monitor
// main.cpp -- Phase 3: real screen navigation
//
// Replaces the temporary Phase 2 long-press 4-way cycle and
// mock-scenario-stepper with the brief's actual decided interaction
// model:
//
//   ROTATE_CW / ROTATE_CCW -- cycle Overview -> Bands -> Solar ->
//                             Alerts -> (wraps). CW advances, CCW goes
//                             back -- confirmed correct on real
//                             hardware 2026-07-12 (the initial mapping
//                             had this backward; fixed by swapping
//                             which physical direction calls
//                             next_screen()/prev_screen(), see below).
//   SHORT_PRESS             -- force refresh of the current screen.
//                             Since Phase 3's real HTTP fetch from
//                             PropMon doesn't exist yet, this also
//                             advances the mock scenario index as a
//                             stand-in for "new data arrived" -- keeps
//                             all four screens' rendering testable
//                             under the new navigation without a fifth
//                             gesture. Remove the scenario-advance line
//                             once a real data_client_fetch_live() (or
//                             similar) exists.
//   LONG_PRESS               -- toggle into/out of the placeholder
//                             Config screen (see screen_config.cpp).
//                             Not the full editable config menu the
//                             brief describes -- that's separate,
//                             future work once Wi-Fi/NVS exist.
//
// Also adds, both previously documented as "decided" but never
// actually implemented in the Phase 2 mock-testing code:
//   - 10s idle timeout, returns to Overview from any other screen
//     (including Config).
//   - Long-press hold-progress bar (see ui_common.cpp), so a hold in
//     progress is visible instead of giving zero feedback until it
//     fires. This also settled whether the long-press-feels-like-2.5s
//     report (see encoder.cpp) was a real button-debounce issue --
//     confirmed it wasn't (hold duration climbed smoothly with no
//     stutters or resets).
//
// The Bug #1 redraw-coalescing fix (drain every pending encoder event
// in one loop() pass, redraw at most once per pass) carries over
// unchanged -- real navigation has the same "a fast multi-detent turn
// shouldn't queue up N redraws" requirement the mock-stepper did.
//
// USB-CDC note: uses while(!Serial) to wait for monitor connection
// before printing anything -- required for ESP32-S3 native USB-CDC
// which must re-enumerate after every reset.

#include <Arduino.h>
#include "config.h"
#include "display_driver.h"
#include "encoder.h"
#include "data_client.h"
#include "ui_common.h"
#include "screens/screen_overview.h"
#include "screens/screen_bands.h"
#include "screens/screen_solar.h"
#include "screens/screen_alerts.h"
#include "screens/screen_config.h"

// Set to 1 to compile in diagnostic Serial output. Set to 0 (default)
// to compile it out of the binary entirely -- not just skip it at
// runtime via if(Serial), but remove it altogether. See Lessons
// Learned in the project brief: on this ESP32-S3 native USB-CDC setup,
// Serial writes can stall for real time when nothing is connected to
// read them, and even the runtime if(Serial) check itself costs
// something measurable when evaluated many times per second with
// nothing attached. Flip to 1 only when actively debugging with a
// monitor attached.
#define DEBUG_VERBOSE 0

enum class ActiveScreen { OVERVIEW, BANDS, SOLAR, ALERTS, CONFIG };

static PropMonData current_data;
static uint8_t scenario_index = 0;
static ActiveScreen active_screen = ActiveScreen::OVERVIEW;

// Which non-Config screen was active right before a long-press entered
// Config -- rotating (or long-pressing again) while on Config returns
// here, rather than jumping to a fixed screen. The separate idle
// timeout below always returns to Overview specifically, matching the
// brief's long-decided "idle -> Overview" behavior; that's a distinct
// rule from "how do I back out of Config on purpose."
static ActiveScreen screen_before_config = ActiveScreen::OVERVIEW;

// Updated on every resolved encoder event (rotation, short press, or
// long press). Drives the 10s idle timeout.
static uint32_t last_interaction_ms = 0;

// ---------------------------------------------------------------------
// Ambient alert banner + persistent badge (added 2026-07-13)
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

// Tracks the worst active alert as of the last refresh, so the next
// refresh can detect a real change (new alert, escalation, or
// resolution) rather than re-triggering on every redraw.
static AlertLevel last_worst_level = ALERT_NONE;
static AlertCategory last_worst_category = ALERT_CAT_TOWER;

// Set by the SHORT_PRESS case below, consumed once per
// refresh_and_draw() call -- distinguishes "this redraw is a real
// data refresh" from ordinary navigation redraws (rotation, long
// press, idle timeout), which also call refresh_and_draw() but
// shouldn't re-trigger an unchanged banner on their own.
static bool force_alert_reshow = false;

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
// data_client.h). Returns false if no alert is active (alert_count==0
// or every present entry is ALERT_NONE, matching PropMon's real
// steady-state placeholder entry). Kept local to main.cpp rather than
// added to data_client.cpp/.h -- that file's current contents weren't
// available this session, and this logic is simple enough (a direct
// AlertLevel enum comparison) that it's very likely equivalent to
// whatever screen_alerts.cpp already computes for the same purpose,
// but flagging that as an assumption, not a confirmed match. Worth
// consolidating into one shared helper later if it's ever handed over.
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

// Evaluates current_data's alert state after every refresh and
// decides whether the ambient banner should (re-)appear. See the
// banner state comment block above for the decided rules.
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
// whatever the current screen just drew. Called after every redraw,
// not just alert-related ones, so both persist correctly across plain
// screen navigation.
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
        case ActiveScreen::OVERVIEW: screen_overview_draw(current_data); break;
        case ActiveScreen::BANDS:    screen_bands_draw(current_data);    break;
        case ActiveScreen::SOLAR:    screen_solar_draw(current_data);    break;
        case ActiveScreen::ALERTS:   screen_alerts_draw(current_data);   break;
        case ActiveScreen::CONFIG:   screen_config_draw();               break;
    }
}

static void refresh_and_draw() {
    data_client_get_mock(current_data, scenario_index);
    check_alert_state(force_alert_reshow);
    force_alert_reshow = false;
    draw_active_screen();
    draw_alert_overlays();
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
        Serial.println("=== N4MI Propagation Monitor -- Phase 3 ===");
        Serial.println("Real screen navigation");
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
        display_show_boot_message("N4MI PropMon", "Phase 3 - Navigation");
        delay(1500);
        last_interaction_ms = millis();
        refresh_and_draw();
    } else {
#if DEBUG_VERBOSE
        if (Serial) Serial.println("[setup] display_init() FAILED");
#endif
    }
}

// See file header for the full Phase 3 interaction model. The Bug #1
// redraw-coalescing pattern (drain every pending event first, redraw
// at most once per pass) is unchanged from Phase 2's mock-stepper --
// this need is identical for real navigation.
void loop() {
    bool need_redraw = false;
    EncoderEvent ev;

    while ((ev = encoder_poll()) != EncoderEvent::NONE) {
        last_interaction_ms = millis();

        // Any resolved event means the button is no longer mid-hold in
        // a state the progress ring needs to keep reflecting -- clear
        // it unconditionally. Cheap even when nothing was drawn (same
        // partial-ring cost either way, never a full-screen clear).
        ui_clear_hold_progress();

        switch (ev) {
            case EncoderEvent::ROTATE_CW:
            case EncoderEvent::ROTATE_CCW: {
                if (active_screen == ActiveScreen::CONFIG) {
                    // Rotating away from Config just returns to
                    // wherever you were before -- this rotation event
                    // doesn't also count as a further cycle step.
                    active_screen = screen_before_config;
                } else {
                    // UPDATED 2026-07-12: confirmed on real hardware
                    // that CW physically produced Overview->Alerts->
                    // Solar->Bands (backward) against the original
                    // assumption. Swapped which physical direction
                    // maps to next_screen()/prev_screen() here --
                    // next_screen()/prev_screen() themselves are
                    // unchanged (still define the logical forward
                    // order Overview->Bands->Solar->Alerts), only the
                    // mapping from physical rotation to that logical
                    // order was backward.
                    active_screen = (ev == EncoderEvent::ROTATE_CW)
                        ? prev_screen(active_screen)
                        : next_screen(active_screen);
                }
                need_redraw = true;
                break;
            }
            case EncoderEvent::SHORT_PRESS:
                // Stand-in for a real PropMon fetch -- see file header.
                scenario_index = (scenario_index + 1) % data_client_mock_scenario_count();
                force_alert_reshow = true;
#if DEBUG_VERBOSE
                if (Serial) Serial.println("[encoder] SHORT PRESS -- force refresh");
#endif
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
    // progress hold duration. Independent of the event-drain above,
    // since encoder_poll() only reports SHORT_PRESS/LONG_PRESS as
    // completed events after the press is already over; this needs to
    // see the hold *while* it's still building. Throttled to ~25
    // updates/sec so this doesn't add per-loop() draw overhead on
    // every single iteration.
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
    // ALERT_BANNER_DURATION_MS even with no further user interaction,
    // by forcing one redraw once the timer elapses. A full screen
    // redraw is required to actually erase it (it's a full-width
    // overlay strip, not a small isolated region like the
    // hold-progress bar, so it can't be cleared in place).
    if (banner_active && (millis() - banner_started_ms) >= ALERT_BANNER_DURATION_MS) {
        banner_active = false;
        need_redraw = true;
    }

    // Idle timeout -- any screen other than Overview returns there
    // after IDLE_TIMEOUT_MS with no interaction (rotation or either
    // press). Documented as decided since the project's initial
    // planning session, but never actually implemented until this
    // Phase 3 rewrite -- the Phase 2 mock-testing main.cpp had no
    // idle-timeout logic at all.
    if (active_screen != ActiveScreen::OVERVIEW &&
        (millis() - last_interaction_ms) >= IDLE_TIMEOUT_MS) {
#if DEBUG_VERBOSE
        if (Serial) Serial.println("[nav] idle timeout -- returning to Overview");
#endif
        active_screen = ActiveScreen::OVERVIEW;
        need_redraw = true;
        last_interaction_ms = millis();
    }

    if (need_redraw) {
#if DEBUG_VERBOSE
        if (Serial) {
            Serial.printf("[main] t=%lu redraw (screen=%s, scenario=%d)\n",
                (unsigned long)millis(), active_screen_name(), scenario_index);
        }
#endif
        refresh_and_draw();
    }

    delay(2);
}
