// N4MI Desktop Instrument Series - Propagation Monitor
// wifi_client.cpp

#include "wifi_client.h"
#include "wifi_credentials.h"
#include "config.h"
#include <WiFi.h>

// See main.cpp for the full explanation -- set to 1 to compile in
// diagnostic Serial output, 0 (default) to compile it out entirely.
#define DEBUG_VERBOSE 0

bool wifi_client_connect() {
    if (WiFi.status() == WL_CONNECTED) return true;

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

#if DEBUG_VERBOSE
    if (Serial) Serial.printf("[wifi] connecting to \"%s\"...\n", WIFI_SSID);
#endif

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
        delay(250);
    }

    bool connected = (WiFi.status() == WL_CONNECTED);
#if DEBUG_VERBOSE
    if (Serial) {
        if (connected) {
            Serial.printf("[wifi] connected, IP=%s\n", WiFi.localIP().toString().c_str());
        } else {
            Serial.printf("[wifi] connect FAILED after %lums, status=%d\n",
                (unsigned long)(millis() - start), (int)WiFi.status());
        }
    }
#endif
    return connected;
}

bool wifi_client_is_connected() {
    return WiFi.status() == WL_CONNECTED;
}
