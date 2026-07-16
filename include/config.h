// N4MI Desktop Instrument Series - Propagation Monitor
// config.h -- pin assignments and non-secret constants
//
// All pin numbers confirmed directly from LilyGO's official
// Xinyuan-LilyGO/T-Encoder-Pro repository PinOverview table.

#pragma once

// ---------------------------------------------------------------------
// Display (SH8601 AMOLED, QSPI bus)
// ---------------------------------------------------------------------
#define PIN_LCD_SDIO0   11
#define PIN_LCD_SDIO1   13
#define PIN_LCD_SDIO2   7
#define PIN_LCD_SDIO3   14
#define PIN_LCD_SCLK    12
#define PIN_LCD_RST     4
#define PIN_LCD_CS      10
#define PIN_LCD_VCI_EN  3

#define DISPLAY_WIDTH   390
#define DISPLAY_HEIGHT  390

// ---------------------------------------------------------------------
// Touch (CHSC5816, I2C) -- not used in v1
// ---------------------------------------------------------------------
#define PIN_TOUCH_RST   8
#define PIN_TOUCH_INT   9
#define PIN_TOUCH_SDA   5
#define PIN_TOUCH_SCL   6

// ---------------------------------------------------------------------
// Rotary encoder + knob push-button
// ---------------------------------------------------------------------
#define PIN_ENCODER_A   1
#define PIN_ENCODER_B   2

// IMPORTANT: GPIO0 is also the ESP32-S3 boot-mode strap pin.
// Holding the knob while powering on forces bootloader mode.
#define PIN_ENCODER_KEY 0

// ---------------------------------------------------------------------
// Buzzer (not used in v1)
// ---------------------------------------------------------------------
#define PIN_BUZZER      17

// ---------------------------------------------------------------------
// Interaction timing
// ---------------------------------------------------------------------
#define IDLE_TIMEOUT_MS      10000
#define LONG_PRESS_MS        1500
#define KNOB_RESET_HOLD_MS   3000

// ---------------------------------------------------------------------
// Long-press hold-progress feedback (added Phase 3 -- real navigation)
// ---------------------------------------------------------------------
// Minimum continuous hold time (ms) before the hold-progress bar
// starts drawing -- avoids a visible flicker on ordinary short presses
// that happen to still read as "pressed" for a couple of
// encoder_poll() cycles before release. Chosen well below
// LONG_PRESS_MS so there's still a meaningful amount of bar fill to
// watch before the long-press actually fires. See ui_common.h's
// ui_draw_hold_progress() / ui_clear_hold_progress().
#define LONG_PRESS_PROGRESS_MIN_MS   300

// ---------------------------------------------------------------------
// Live data fetch (added for the real PropMon HTTP fetch proof of
// concept -- temporarily using hardcoded Wi-Fi credentials, see
// wifi_credentials.h / wifi_credentials.h.example)
// ---------------------------------------------------------------------
// Max time to wait for Wi-Fi to associate before giving up (wifi_client.cpp).
#define WIFI_CONNECT_TIMEOUT_MS   15000

// How often the device automatically re-fetches from PropMon, in
// addition to the short-press force-refresh. PropMon's own server-side
// cache refreshes every 2-5 minutes (see project brief), so this is
// deliberately shorter than that only to keep the "Updated Xs ago"
// footer feeling responsive -- not because faster polling gets fresher
// data from PropMon itself.
#define LIVE_FETCH_INTERVAL_MS    60000

// How often to redraw purely to refresh the "Updated Xs ago" footer's
// elapsed-time text, independent of any actual data change. Without
// this, the footer only updates as a side effect of some other redraw
// (rotation, a press, a fetch) -- confirmed on real hardware 2026-07-15
// that it otherwise sits frozen between those events even though real
// time is passing. Skipped entirely on the Config screen (see main.cpp)
// since nothing there needs live-ticking. Much lower frequency than the
// redraws that caused the earlier lag bugs (#1/#2 in the brief), which
// were dozens per second during fast interaction, not one every 5s.
#define UI_TICK_INTERVAL_MS       5000

// ---------------------------------------------------------------------
// PropMon service
// ---------------------------------------------------------------------
#define PROPMON_DEFAULT_HOST  "192.168.6.29"
#define PROPMON_DEFAULT_PORT  8076
