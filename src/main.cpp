// N4MI Desktop Instrument Series - Propagation Monitor
// main.cpp -- Phase 2: Overview and Bands screens against mock data
//
// USB-CDC note: uses while(!Serial) to wait for monitor connection
// before printing anything -- required for ESP32-S3 native USB-CDC
// which must re-enumerate after every reset.
//
// Encoder mapping for this phase:
//   ROTATE_CW/CCW -- step through mock data scenarios (acceptance
//                    testing aid, not the final Phase 3+ behavior),
//                    applies to whichever screen is currently shown
//   SHORT_PRESS    -- force refresh of the current screen
//   LONG_PRESS     -- TEMPORARY testing toggle between Overview and
//                     Bands. NOT the final navigation design -- the
//                     brief's decided behavior is screen cycling via
//                     knob rotation once all four screens (Overview,
//                     Bands, Solar, Alerts) exist. This toggle exists
//                     only so Bands can be seen on real hardware
//                     before Solar/Alerts and real navigation are
//                     built; remove/replace when that happens.

#include <Arduino.h>
#include "config.h"
#include "display_driver.h"
#include "encoder.h"
#include "data_client.h"
#include "screens/screen_overview.h"
#include "screens/screen_bands.h"

// Set to 1 to compile in the diagnostic Serial output added this session
// for the lag investigation. Set to 0 (default) to compile it out
// entirely -- not just skip it at runtime via if(Serial), but remove it
// from the binary altogether. Added specifically to test whether the
// if(Serial) runtime check itself was contributing measurable overhead
// when checked repeatedly per frame with nothing connected, since lag
// was still reported after that guard was added. Flip to 1 only when
// actively debugging with a monitor attached.
#define DEBUG_VERBOSE 0

enum class ActiveScreen { OVERVIEW, BANDS };

static PropMonData current_data;
static uint8_t scenario_index = 0;
static ActiveScreen active_screen = ActiveScreen::OVERVIEW;

static void refresh_and_draw() {
    data_client_get_mock(current_data, scenario_index);
    if (active_screen == ActiveScreen::OVERVIEW) {
        screen_overview_draw(current_data);
    } else {
        screen_bands_draw(current_data);
    }
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
        Serial.println("=== N4MI Propagation Monitor -- Phase 2 ===");
        Serial.println("Overview screen against mock data");
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
        display_show_boot_message("N4MI PropMon", "Phase 2 - Overview");
        delay(1500);
#if DEBUG_VERBOSE
        if (Serial) Serial.printf("[setup] t=%lu about to call first refresh_and_draw()\n", (unsigned long)millis());
#endif
        refresh_and_draw();
#if DEBUG_VERBOSE
        if (Serial) Serial.printf("[setup] t=%lu first refresh_and_draw() returned\n", (unsigned long)millis());
#endif
    } else {
#if DEBUG_VERBOSE
        if (Serial) Serial.println("[setup] display_init() FAILED");
#endif
    }
}

// UPDATED 2026-07-10: loop() previously called refresh_and_draw() once per
// individual encoder event. A fast multi-detent turn would queue several
// events, each triggering its own full (expensive) redraw in sequence --
// the encoder itself never missed a transition (that was fixed in Phase 1),
// but the display visibly lagged behind while working through the backlog
// of redraws one at a time. Fix: drain every pending event in one loop()
// pass first, then redraw at most once per pass reflecting wherever things
// ended up. This is the same fix real screen navigation will need later,
// not just this temporary mock-scenario stepper.
//
// UPDATED 2026-07-10 (later same day): all Serial output in this file (and
// in every screen's draw function) is now guarded with `if (Serial)`.
// Confirmed by testing: the lag symptom this guards against only appeared
// when no Serial Monitor was connected -- with one attached, everything ran
// fast; unplugged, it lagged noticeably. Root cause: on this ESP32-S3 native
// USB-CDC setup, Serial.print()/printf() calls can stall for real time when
// nothing is reading the other end, since the internal TX buffer has
// nowhere to drain to. This session added many debug prints (one or more
// per redraw/per encoder event) specifically to diagnose the *original*
// lag bug -- which meant the diagnostic instrumentation itself became a
// real-world performance bug for the actual deployed use case, since a
// finished desk instrument spends effectively all its life with no
// computer attached at all. `if (Serial)` is a cheap, non-blocking check
// (reads USB-CDC connection/DTR state) that skips the print entirely when
// nothing's listening, rather than attempting -- and possibly stalling on
// -- a write with no reader. This guard should be standard practice for
// any future debug output added to this project, not just this fix.
void loop() {
    bool need_redraw = false;
    EncoderEvent ev;

    while ((ev = encoder_poll()) != EncoderEvent::NONE) {
        switch (ev) {
            case EncoderEvent::ROTATE_CW:
                scenario_index = (scenario_index + 1) % data_client_mock_scenario_count();
                need_redraw = true;
                break;
            case EncoderEvent::ROTATE_CCW:
                scenario_index = (scenario_index + data_client_mock_scenario_count() - 1) % data_client_mock_scenario_count();
                need_redraw = true;
                break;
            case EncoderEvent::SHORT_PRESS:
#if DEBUG_VERBOSE
                if (Serial) Serial.println("[encoder] SHORT PRESS -- force refresh");
#endif
                need_redraw = true;
                break;
            case EncoderEvent::LONG_PRESS:
                active_screen = (active_screen == ActiveScreen::OVERVIEW)
                    ? ActiveScreen::BANDS : ActiveScreen::OVERVIEW;
#if DEBUG_VERBOSE
                if (Serial) {
                    Serial.printf("[nav] t=%lu LONG PRESS -- toggled to %s (temporary testing toggle)\n",
                        (unsigned long)millis(),
                        active_screen == ActiveScreen::OVERVIEW ? "Overview" : "Bands");
                }
#endif
                need_redraw = true;
                break;
            default:
                break;
        }
    }

    if (need_redraw) {
#if DEBUG_VERBOSE
        if (Serial) {
            Serial.printf("[mock] t=%lu scenario -> %d (screen=%s)\n", (unsigned long)millis(), scenario_index,
                active_screen == ActiveScreen::OVERVIEW ? "Overview" : "Bands");
        }
#endif
        refresh_and_draw();
    }

    delay(2);
}
