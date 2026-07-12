// N4MI Desktop Instrument Series - Propagation Monitor
// main.cpp -- Phase 3: real screen navigation
//
// Replaces the temporary Phase 2 long-press 4-way cycle and
// mock-scenario-stepper with the brief's actual decided interaction
// model:
//
//   ROTATE_CW / ROTATE_CCW -- cycle Overview -> Bands -> Solar ->
//                             Alerts -> (wraps). CW advances, CCW goes
//                             back -- this direction mapping is an
//                             assumption, not yet confirmed on real
//                             hardware; easy to swap if it feels
//                             backwards.
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
//   - Long-press hold-progress ring (see ui_common.cpp), so a hold in
//     progress is visible instead of giving zero feedback until it
//     fires. This should also help settle whether the long-press-
//     feels-like-2.5s report (see encoder.cpp) is pure perception or
//     a real button-debounce issue -- watch whether the ring climbs
//     smoothly or stutters/resets mid-hold.
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
    draw_active_screen();
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
                    active_screen = (ev == EncoderEvent::ROTATE_CW)
                        ? next_screen(active_screen)
                        : prev_screen(active_screen);
                }
                need_redraw = true;
                break;
            }
            case EncoderEvent::SHORT_PRESS:
                // Stand-in for a real PropMon fetch -- see file header.
                scenario_index = (scenario_index + 1) % data_client_mock_scenario_count();
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
