// N4MI Desktop Instrument Series - Propagation Monitor
// screen_config.h
//
// Phase 3 placeholder config screen. Deliberately read-only and
// deliberately honest about what is and isn't real yet: PropMon URL
// is a live, compiled-in value; Wi-Fi and IP address have no backing
// implementation at all yet (no Wi-Fi stack exists in this firmware),
// so they're shown as explicit "Not yet built" placeholders rather
// than faked or omitted. The real editable config menu (Wi-Fi reset,
// PropMon URL field) is separate future work once Phase 3's Wi-Fi
// portal exists -- see the project brief's Backlog.
//
// Entered via long-press from any of the four data screens; exited by
// knob rotation (returns to whichever screen was active before the
// long-press) or by the shared idle timeout (returns to Overview).
// See main.cpp for that state handling -- this file only draws.

#pragma once

// No PropMonData needed -- every value this screen shows is either a
// compile-time constant (PropMon URL) or a static "not yet built"
// placeholder, not live instrument data.
void screen_config_draw();
