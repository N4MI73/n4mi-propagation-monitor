// N4MI Desktop Instrument Series - Propagation Monitor
// encoder.h -- rotary encoder rotation + knob button reading
//
// Rotation decoding is interrupt-driven (see encoder.cpp) so that
// quadrature transitions are never missed while loop() is blocked on
// something else (e.g. a display redraw). Button press/release is
// still polled from loop() via encoder_poll() -- press timing doesn't
// need interrupt precision the way rotation does.

#pragma once

#include <Arduino.h>

enum class EncoderEvent {
    NONE,
    ROTATE_CW,
    ROTATE_CCW,
    SHORT_PRESS,
    LONG_PRESS,
};

void encoder_init();
EncoderEvent encoder_poll();

// Returns milliseconds the button has been continuously held, or 0 if
// not currently pressed. Read-only -- does not affect the press/
// release event state tracked internally by encoder_poll(). Added
// Phase 3 (real navigation) so UI code can show hold-progress feedback
// while a long-press is still building -- encoder_poll() itself only
// reports SHORT_PRESS/LONG_PRESS as completed, discrete events after
// the fact, which isn't enough to draw a live progress indicator.
uint32_t encoder_get_hold_ms();
