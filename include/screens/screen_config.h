// N4MI Desktop Instrument Series - Propagation Monitor
// screen_config.h
//
// UPDATED 2026-07-15: Wi-Fi and IP address are no longer placeholder
// "Not yet built" text -- real Wi-Fi now exists (hardcoded credentials,
// see wifi_client.h), so this screen shows real connection status and
// IP directly from the Wi-Fi stack. Data source line is now dynamic:
// shows "MOCK DATA" until the first successful live fetch, "LIVE"
// after -- `live_data_active` is passed in from main.cpp, which is the
// only place that knows whether a fetch has ever actually succeeded.
//
// Entered via long-press from any of the four data screens; exited by
// knob rotation (returns to whichever screen was active before the
// long-press) or by the shared idle timeout (returns to Overview).
// See main.cpp for that state handling -- this file only draws.

#pragma once

// live_data_active: true once at least one successful live PropMon
// fetch has occurred (main.cpp tracks this -- it never reverts to
// false just because a later fetch fails, matching the "keep showing
// last known-good" philosophy carried down from PropMon itself).
void screen_config_draw(bool live_data_active);
