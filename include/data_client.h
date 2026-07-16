// N4MI Desktop Instrument Series - Propagation Monitor
// data_client.h -- PropMon data model, mock data source, and (new)
// real live HTTP fetch
//
// Field names/types mirror PropMon's real JSON shape (see project brief)
// so that swapping this mock source for a live HTTP fetch is a drop-in
// change -- no screen code needed to change, only how PropMonData gets
// populated. That's now proven true: data_client_fetch_live() populates
// the exact same PropMonData struct the mock source always has.
//
// UPDATED 2026-07-11: xray and solar_wind added when the Solar screen
// needed them. alerts[] added now that the Alerts screen needs it --
// both were previously deferred per the project's "no premature
// abstraction" framework scope decision.
//
// UPDATED 2026-07-15: added data_client_fetch_live() -- a real HTTP
// GET + JSON parse against PropMon's live endpoint, using hardcoded
// Wi-Fi credentials as a temporary stand-in for the real captive-
// portal Wi-Fi setup (not yet built). The mock functions below are
// unchanged and still compiled in -- useful for future testing --
// but main.cpp's normal operation no longer calls them.

#pragma once

#include <Arduino.h>

#define PROPMON_BAND_COUNT 10
#define PROPMON_MAX_ALERTS 4

enum BandCondition : uint8_t {
    BAND_POOR = 0,
    BAND_FAIR = 1,
    BAND_GOOD = 2,
};

struct BandReading {
    const char *name;
    BandCondition condition;
};

// Mirrors PropMon's alert level strings exactly: "NONE", "CAUTION",
// "WARNING", "CRITICAL". A single NONE-level entry with a placeholder
// message (e.g. "No active alerts") is PropMon's normal steady-state
// response, per the real JSON sample in the project brief -- not an
// empty array.
enum AlertLevel : uint8_t {
    ALERT_NONE     = 0,
    ALERT_CAUTION  = 1,
    ALERT_WARNING  = 2,
    ALERT_CRITICAL = 3,
};

// Mirrors PropMon's alert category strings: "tower" or "propagation".
enum AlertCategory : uint8_t {
    ALERT_CAT_TOWER       = 0,
    ALERT_CAT_PROPAGATION = 1,
};

struct AlertEntry {
    AlertCategory category;
    AlertLevel level;
    char message[48];
};

struct PropMonData {
    // millis() timestamp of when this data was considered "fresh" --
    // used to compute "Updated Xm ago" without needing NTP/Wi-Fi time
    // sync. data_client_fetch_live() sets this to millis() on every
    // successful parse.
    uint32_t last_updated_ms;

    int sfi;
    int a_index;
    int k_index;
    int sunspots;
    float solar_wind;

    // Mirrors PropMon's raw xray string exactly, e.g. "C1.3" -- the
    // Solar screen parses the leading letter (A/B/C/M/X) itself to
    // pick a severity color; PropMon doesn't pre-classify this field.
    char xray[8];

    // Mirrors PropMon's tower_status field exactly: "NONE", "CAUTION",
    // "WARNING", or "CRITICAL".
    char tower_status[16];

    BandReading bands[PROPMON_BAND_COUNT];

    AlertEntry alerts[PROPMON_MAX_ALERTS];
    uint8_t alert_count;
};

// Populates `out` with one of a small set of hardcoded mock scenarios,
// selected by scenario_index (wraps via data_client_mock_scenario_count()).
// Scenarios are deliberately varied -- mixed conditions, all-good,
// all-poor, and a tower alert -- to exercise the screen's tier-selection
// and color logic before any live data exists. Kept available for
// future testing even though main.cpp's normal operation now uses
// data_client_fetch_live() instead.
void data_client_get_mock(PropMonData &out, uint8_t scenario_index);

uint8_t data_client_mock_scenario_count();

// Performs a real HTTP GET against PropMon's live endpoint
// (config.h's PROPMON_DEFAULT_HOST/PORT) and parses the JSON response.
// Parses into a local temporary first and only copies into `out` on
// full success -- a failed or partial/malformed response never
// corrupts whatever `out` already held, mirroring PropMon's own
// "serve last known-good data on fetch failure" design (see project
// brief). Returns false on any failure: Wi-Fi not connected, HTTP
// error, unexpected band count/order, or a JSON parse error. Any
// individual malformed alert entry is skipped rather than failing the
// whole fetch. Caller is responsible for ensuring Wi-Fi is connected
// first (see wifi_client.h) -- this function checks but does not
// itself attempt to connect.
bool data_client_fetch_live(PropMonData &out);
