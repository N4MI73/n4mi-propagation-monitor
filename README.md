# N4MI Propagation Monitor

A small desk instrument that shows live HF propagation conditions at a glance — built on
the LilyGO T-Encoder Pro (ESP32-S3, 390×390 round AMOLED, rotary encoder). Cycle through
four screens with a twist of the knob to see band conditions, solar indices, and active
weather/tower alerts, without opening a laptop or a browser tab.

Part of the **N4MI Desktop Instrument Series** — a growing family of small, single-purpose
ham radio desk instruments on the same hardware platform. This repo covers the
Propagation Monitor specifically; additional instruments in the series live in their own
dedicated repos as they're built.

---

## What it does

- **Overview** — headline band-condition summary at a glance (which tier — GOOD/FAIR/POOR
  — has the most bands right now, plus a secondary line for the next-best tier)
- **Bands** — all ten HF bands (160m–6m), fixed screen order, each colored by its own
  condition tier
- **Solar** — SFI, sunspot number, K-index, A-index, X-ray flare class, solar wind speed
- **Alerts** — the single worst active tower/weather or propagation alert, with an ambient
  banner and persistent badge that surface new or escalating alerts on any screen
- **Staleness indicator** — footer text shifts color if data hasn't refreshed recently,
  so you always know whether you're looking at current conditions

Navigation is knob-only: rotate to cycle screens, short-press to force an immediate data
refresh, long-press to open a Config screen (Wi-Fi status, data source, IP address).

---

## Hardware

| Item | Detail |
|---|---|
| Board | LilyGO T-Encoder Pro |
| Chip | ESP32-S3 (8MB PSRAM) |
| Display | 390×390 round AMOLED (CO5300 driver — see note below) |
| Input | Rotary encoder with push button |
| Connectivity | Wi-Fi, USB-C |

**Display driver note:** the T-Encoder Pro has shipped with two different panel
revisions. This project uses `Arduino_CO5300`. If you build this on your own unit and get
a blank screen despite clean serial output, your panel may be the older SH8601 revision —
try swapping the display driver class first.

---

## Getting started

### Toolchain
- [PlatformIO](https://platformio.org/) with the **pioarduino** platform fork — official
  PlatformIO's `espressif32` platform does not currently support the ESP32 Arduino core
  version this project requires, so the community fork is necessary, not optional.
- Arduino framework, `Arduino_GFX` library (leave the version unpinned — pinned older
  versions don't include CO5300/SH8601 support).

### Build & flash
1. Clone this repo and open it in VS Code with the PlatformIO extension installed.
2. Copy `include/wifi_credentials.h.example` to `include/wifi_credentials.h` — **this file
   is a one-time fallback only** and is gitignored; normal setup uses the on-device
   captive portal described below, not this file.
3. Build and upload via PlatformIO as usual.

### Wi-Fi setup
On first boot (or after a factory-reset-style long hold), the device opens its own Wi-Fi
access point with a captive portal. Connect to it from your phone, select your home
network from the scanned list, and enter your password. Credentials are stored on-device
and reused on every subsequent boot.

---

## Data source

This instrument does **not** fetch propagation data directly from external sources —
it polls a small, self-hosted backend service called **PropMon**, which handles fetching,
rating, and caching HF propagation and local weather/tower alert data, and serves it as a
single flat JSON endpoint.

PropMon is a separate project, maintained here:
**[github.com/N4MI73/streamdeck-hamradio](https://github.com/N4MI73/streamdeck-hamradio)**
(see the `propmon/` folder). If you want to run this firmware yourself, you'll need a
working PropMon deployment first — its README covers deployment via Docker/Portainer.

This firmware treats PropMon strictly as an external API and expects:
- A fixed, validated band order in every response
- A non-empty `alerts` array (a steady-state "no active alerts" placeholder entry, not an
  empty array)

---

## Known limitations

- No dedicated "Wi-Fi unreachable" or "PropMon unreachable" screens — a failed fetch just
  keeps showing the last successfully retrieved data, with the staleness indicator as the
  passive signal that something's wrong.
- If Wi-Fi drops mid-operation, reconnect attempts block the UI for up to 15 seconds. This
  is a known, accepted rough edge — it self-recovers and doesn't corrupt anything.
- Touch input (the panel has a capacitive touch controller) is not currently used —
  navigation is knob-only.

---

## License & credits

This project is built clean-room, inspired by
[FlightScnr](https://github.com/yashmulgaonkar/FlightScnr) (CC BY-NC-SA 4.0) — FlightScnr
was a reference for what's proven to work on this exact hardware (display library family,
captive-portal UX pattern, idle timeout value), but no code is copied from it.

Propagation and weather data ultimately originates from HamQSL, NOAA SWPC, WeatherFlow
Tempest, and NWS — fetched and digested by the companion PropMon service (see above).

---

## Blog Post

I added a post about PropMon to my n4mi.tech blog: [PropMon - a Desktop Propagation Monitor](https://n4mi.tech/propmon-a-desktop-propagation-monitor/

---

## Related projects

- **[streamdeck-hamradio](https://github.com/N4MI73/streamdeck-hamradio)** — PropMon, the
  backend data service this firmware depends on.
- More instruments in the N4MI Desktop Instrument Series are in progress, each in their
  own dedicated repo following the naming pattern `n4mi-[instrument]-monitor`.
