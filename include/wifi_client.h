// N4MI Desktop Instrument Series - Propagation Monitor
// wifi_client.h -- Wi-Fi connection management
//
// Uses hardcoded credentials from wifi_credentials.h (gitignored --
// see wifi_credentials.h.example) as a temporary stand-in for the real
// captive-portal Wi-Fi setup, which doesn't exist yet. This file's
// interface is deliberately small so swapping the credential source
// later (NVS instead of a compiled-in header) shouldn't require
// touching any caller of these two functions.

#pragma once

#include <Arduino.h>

// Attempts to connect to Wi-Fi using the hardcoded credentials in
// wifi_credentials.h. Blocks up to WIFI_CONNECT_TIMEOUT_MS (config.h)
// waiting for association. Returns true if connected by the time it
// returns, false on timeout. Safe to call again later to retry --
// returns immediately (true) if already connected.
bool wifi_client_connect();

// Returns true if currently associated to Wi-Fi.
bool wifi_client_is_connected();
