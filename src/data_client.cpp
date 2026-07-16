// N4MI Desktop Instrument Series - Propagation Monitor
// data_client.cpp -- mock data source (Phase 2) + real live HTTP
// fetch (added 2026-07-15)
//
// The mock scenarios below are unchanged from Phase 2 -- still useful
// for future testing -- but main.cpp's normal operation now calls
// data_client_fetch_live() instead.

#include "data_client.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// See main.cpp for the full explanation -- set to 1 to compile in
// diagnostic Serial output, 0 (default) to compile it out entirely.
#define DEBUG_VERBOSE 0

#define NUM_MOCK_SCENARIOS 4

// Scenario 0: mirrors the real sample captured in the project brief --
// mixed conditions, GOOD is the best tier present.
static BandReading scenario0_bands[PROPMON_BAND_COUNT] = {
    {"160m", BAND_GOOD}, {"80m", BAND_FAIR}, {"40m", BAND_GOOD},
    {"30m",  BAND_GOOD}, {"20m", BAND_FAIR}, {"17m", BAND_FAIR},
    {"15m",  BAND_POOR}, {"12m", BAND_POOR}, {"10m", BAND_POOR},
    {"6m",   BAND_POOR},
};

// Scenario 1: geomagnetic storm -- mostly poor, tower on CAUTION.
// Exercises the tower-status color mapping and a POOR-dominant tier.
static BandReading scenario1_bands[PROPMON_BAND_COUNT] = {
    {"160m", BAND_FAIR}, {"80m", BAND_POOR}, {"40m", BAND_FAIR},
    {"30m",  BAND_POOR}, {"20m", BAND_POOR}, {"17m", BAND_POOR},
    {"15m",  BAND_POOR}, {"12m", BAND_POOR}, {"10m", BAND_POOR},
    {"6m",   BAND_POOR},
};

// Scenario 2: excellent day -- every band GOOD, no secondary tier at
// all. Exercises the "no secondary line" edge case.
static BandReading scenario2_bands[PROPMON_BAND_COUNT] = {
    {"160m", BAND_GOOD}, {"80m", BAND_GOOD}, {"40m", BAND_GOOD},
    {"30m",  BAND_GOOD}, {"20m", BAND_GOOD}, {"17m", BAND_GOOD},
    {"15m",  BAND_GOOD}, {"12m", BAND_GOOD}, {"10m", BAND_GOOD},
    {"6m",   BAND_GOOD},
};

// Scenario 3: dead band day -- everything POOR. Headline itself
// should render in the POOR color, not just a secondary line.
static BandReading scenario3_bands[PROPMON_BAND_COUNT] = {
    {"160m", BAND_POOR}, {"80m", BAND_POOR}, {"40m", BAND_POOR},
    {"30m",  BAND_POOR}, {"20m", BAND_POOR}, {"17m", BAND_POOR},
    {"15m",  BAND_POOR}, {"12m", BAND_POOR}, {"10m", BAND_POOR},
    {"6m",   BAND_POOR},
};

// Alert sets, one per scenario. Scenario 0 and 2 use PropMon's real
// steady-state shape (a single NONE-level placeholder entry, per the
// live JSON sample captured in the project brief -- not an empty
// array). Scenario 1 has two simultaneous alerts of different
// categories and severities, to exercise "pick the worst one" logic
// and the "+N more" indicator together. Scenario 3 is the only mock
// case reaching CRITICAL -- nothing else in this file has tested that
// tier yet.
static AlertEntry scenario0_alerts[1] = {
    {ALERT_CAT_TOWER, ALERT_NONE, "No active alerts"},
};

static AlertEntry scenario1_alerts[2] = {
    {ALERT_CAT_TOWER, ALERT_CAUTION, "Lightning detected within 10mi"},
    {ALERT_CAT_PROPAGATION, ALERT_WARNING, "Geomagnetic storm in progress (Kp=6)"},
};

static AlertEntry scenario2_alerts[1] = {
    {ALERT_CAT_TOWER, ALERT_NONE, "No active alerts"},
};

static AlertEntry scenario3_alerts[1] = {
    {ALERT_CAT_TOWER, ALERT_CRITICAL, "Wind gust 62mph - secure equipment"},
};

void data_client_get_mock(PropMonData &out, uint8_t scenario_index) {
    scenario_index %= NUM_MOCK_SCENARIOS;

    out.last_updated_ms = millis();

    switch (scenario_index) {
        case 0:
            out.sfi = 163; out.a_index = 12; out.k_index = 3; out.sunspots = 99;
            out.solar_wind = 654.6;
            strncpy(out.xray, "C1.3", sizeof(out.xray));
            strncpy(out.tower_status, "NONE", sizeof(out.tower_status));
            memcpy(out.bands, scenario0_bands, sizeof(scenario0_bands));
            memcpy(out.alerts, scenario0_alerts, sizeof(scenario0_alerts));
            out.alert_count = 1;
            break;
        case 1:
            // Geomagnetic storm scenario -- also given a moderate (M-class)
            // flare and elevated wind speed so this is the one mock case
            // exercising the "severe" tier on both K-index and X-ray at
            // once, alongside the tower CAUTION status. Two simultaneous
            // alerts here too (tower CAUTION + propagation WARNING) --
            // the worst-alert-wins headline logic should pick WARNING.
            out.sfi = 98; out.a_index = 28; out.k_index = 6; out.sunspots = 41;
            out.solar_wind = 780.2;
            strncpy(out.xray, "M5.2", sizeof(out.xray));
            strncpy(out.tower_status, "CAUTION", sizeof(out.tower_status));
            memcpy(out.bands, scenario1_bands, sizeof(scenario1_bands));
            memcpy(out.alerts, scenario1_alerts, sizeof(scenario1_alerts));
            out.alert_count = 2;
            break;
        case 2:
            out.sfi = 210; out.a_index = 4; out.k_index = 1; out.sunspots = 187;
            out.solar_wind = 380.0;
            strncpy(out.xray, "A0.5", sizeof(out.xray));
            strncpy(out.tower_status, "NONE", sizeof(out.tower_status));
            memcpy(out.bands, scenario2_bands, sizeof(scenario2_bands));
            memcpy(out.alerts, scenario2_alerts, sizeof(scenario2_alerts));
            out.alert_count = 1;
            break;
        case 3:
            // K-index here is GOOD (2) despite the dead-band day -- low
            // SFI, not geomagnetic activity, is why the bands are poor.
            // X-ray given a middling C-class so this scenario also tests
            // K and X-ray landing in *different* tiers from each other.
            // Tower alert is CRITICAL -- the only mock scenario reaching
            // this severity.
            out.sfi = 68; out.a_index = 6; out.k_index = 2; out.sunspots = 12;
            out.solar_wind = 410.0;
            strncpy(out.xray, "C4.5", sizeof(out.xray));
            strncpy(out.tower_status, "CRITICAL", sizeof(out.tower_status));
            memcpy(out.bands, scenario3_bands, sizeof(scenario3_bands));
            memcpy(out.alerts, scenario3_alerts, sizeof(scenario3_alerts));
            out.alert_count = 1;
            break;
    }
}

uint8_t data_client_mock_scenario_count() {
    return NUM_MOCK_SCENARIOS;
}

// ---------------------------------------------------------------------
// Live data fetch (added 2026-07-15)
// ---------------------------------------------------------------------
// Fixed band order matching PropMon's own JSON ordering exactly (see
// the live sample in the project brief) -- the "band" field in each
// JSON entry is checked against this expected order as a sanity check
// (see the loop below) rather than trusted blindly, so a silent
// name/position mismatch fails the fetch instead of quietly
// mislabeling a band's tier.
static const char *EXPECTED_BAND_ORDER[PROPMON_BAND_COUNT] = {
    "160m", "80m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m"
};

static bool parse_alert_level(const char *s, AlertLevel &out) {
    if (strcmp(s, "NONE") == 0)     { out = ALERT_NONE;     return true; }
    if (strcmp(s, "CAUTION") == 0)  { out = ALERT_CAUTION;  return true; }
    if (strcmp(s, "WARNING") == 0)  { out = ALERT_WARNING;  return true; }
    if (strcmp(s, "CRITICAL") == 0) { out = ALERT_CRITICAL; return true; }
    return false;
}

static bool parse_alert_category(const char *s, AlertCategory &out) {
    if (strcmp(s, "tower") == 0)       { out = ALERT_CAT_TOWER;       return true; }
    if (strcmp(s, "propagation") == 0) { out = ALERT_CAT_PROPAGATION; return true; }
    return false;
}

static bool parse_band_condition(const char *s, BandCondition &out) {
    if (strcmp(s, "good") == 0) { out = BAND_GOOD; return true; }
    if (strcmp(s, "fair") == 0) { out = BAND_FAIR; return true; }
    if (strcmp(s, "poor") == 0) { out = BAND_POOR; return true; }
    return false;
}

bool data_client_fetch_live(PropMonData &out) {
    if (WiFi.status() != WL_CONNECTED) {
#if DEBUG_VERBOSE
        if (Serial) Serial.println("[fetch] Wi-Fi not connected, aborting");
#endif
        return false;
    }

    char url[64];
    snprintf(url, sizeof(url), "http://%s:%d/api/instrument/propagation",
        PROPMON_DEFAULT_HOST, PROPMON_DEFAULT_PORT);

    HTTPClient http;
    http.setTimeout(5000);
    if (!http.begin(url)) {
#if DEBUG_VERBOSE
        if (Serial) Serial.println("[fetch] http.begin() failed");
#endif
        return false;
    }

    int status = http.GET();
    if (status != HTTP_CODE_OK) {
#if DEBUG_VERBOSE
        if (Serial) Serial.printf("[fetch] HTTP GET failed, status=%d\n", status);
#endif
        http.end();
        return false;
    }

    // Parse directly from the response stream rather than buffering
    // the whole body into a String first -- one less full copy of the
    // JSON sitting in RAM at once.
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();

    if (err) {
#if DEBUG_VERBOSE
        if (Serial) Serial.printf("[fetch] JSON parse failed: %s\n", err.c_str());
#endif
        return false;
    }

    // Parse into a local temporary first -- only commit to `out` if
    // every required field parses successfully, so a partial or
    // malformed response never corrupts whatever `out` already held
    // (mirrors PropMon's own "serve last known-good data on failure"
    // design, carried down to the device).
    PropMonData tmp;
    memset(&tmp, 0, sizeof(tmp));

    tmp.sfi        = doc["sfi"] | 0;
    tmp.a_index    = doc["a_index"] | 0;
    tmp.k_index    = doc["k_index"] | 0;
    tmp.sunspots   = doc["sunspots"] | 0;
    tmp.solar_wind = doc["solar_wind"] | 0.0f;

    const char *xray = doc["xray"] | "";
    strncpy(tmp.xray, xray, sizeof(tmp.xray) - 1);

    const char *tower = doc["tower_status"] | "NONE";
    strncpy(tmp.tower_status, tower, sizeof(tmp.tower_status) - 1);

    JsonArray bands = doc["bands"];
    if (bands.size() != PROPMON_BAND_COUNT) {
#if DEBUG_VERBOSE
        if (Serial) Serial.printf("[fetch] unexpected band count: %d\n", (int)bands.size());
#endif
        return false;
    }

    for (int i = 0; i < PROPMON_BAND_COUNT; i++) {
        const char *name = bands[i]["band"] | "";
        if (strcmp(name, EXPECTED_BAND_ORDER[i]) != 0) {
#if DEBUG_VERBOSE
            if (Serial) {
                Serial.printf("[fetch] band order mismatch at %d: expected %s got %s\n",
                    i, EXPECTED_BAND_ORDER[i], name);
            }
#endif
            return false;
        }
        const char *status_str = bands[i]["status"] | "";
        BandCondition cond;
        if (!parse_band_condition(status_str, cond)) {
#if DEBUG_VERBOSE
            if (Serial) Serial.printf("[fetch] unrecognized band status: %s\n", status_str);
#endif
            return false;
        }
        tmp.bands[i].name = EXPECTED_BAND_ORDER[i];
        tmp.bands[i].condition = cond;
    }

    JsonArray alerts = doc["alerts"];
    uint8_t count = 0;
    for (JsonObject a : alerts) {
        if (count >= PROPMON_MAX_ALERTS) break;

        const char *cat_str   = a["category"] | "";
        const char *level_str = a["level"] | "";
        const char *msg       = a["message"] | "";

        AlertCategory cat;
        AlertLevel level;
        if (!parse_alert_category(cat_str, cat) || !parse_alert_level(level_str, level)) {
            // Skip just this entry rather than failing the whole
            // fetch -- one unrecognized alert shouldn't take down
            // solar/band data that parsed fine.
#if DEBUG_VERBOSE
            if (Serial) {
                Serial.printf("[fetch] skipping unrecognized alert category/level: %s/%s\n",
                    cat_str, level_str);
            }
#endif
            continue;
        }
        tmp.alerts[count].category = cat;
        tmp.alerts[count].level = level;
        strncpy(tmp.alerts[count].message, msg, sizeof(tmp.alerts[count].message) - 1);
        count++;
    }
    tmp.alert_count = count;

    tmp.last_updated_ms = millis();

    out = tmp;
#if DEBUG_VERBOSE
    if (Serial) Serial.println("[fetch] success");
#endif
    return true;
}
