# N4MI Desktop Instrument Series
**Propagation Monitor** — N4MI | Dan Marshall

A small family of ESP32-based desktop hardware instruments for the ham shack. The first
instrument — **Propagation Monitor** — is a round-AMOLED desk display showing live HF band
conditions, solar data, and tower/weather alerts, pulled from a self-hosted backend service
called [PropMon](https://github.com/N4MI73/streamdeck-hamradio/tree/main/propmon).

This repo covers the firmware only. PropMon (deployment, environment variables, data sources,
API shape) is documented in its own repo — see [Related Projects](#related-projects) below.

---

## What this is

Inspired by [FlightScnr](https://github.com/yashmulgaonkar/FlightScnr) — built clean-room on the
same hardware platform, referencing it only for what's proven to work (display library, captive
portal UX, idle timeout values), no code copied. No credentials or personal configuration are
committed anywhere in this repo; see [Wi-Fi Setup](#wi-fi-setup) for how that's handled.

Four data screens, cycled by turning the knob:

| Screen | Shows |
|---|---|
| **Overview** | Dynamic best-tier headline (GOOD/FAIR/POOR), band list, compact SFI/SS/K/A stats, tower status |
| **Bands** | All 10 bands (160m–6m) individually, fixed order, color-coded by tier |
| **Solar** | Full-detail SFI/sunspots/K-index/A-index/X-ray/solar-wind, larger stat grid |
| **Alerts** | Worst active alert (tower or propagation), full message, "+N more" if applicable |

Plus a **Config** screen (long-press) showing live Wi-Fi status, IP address, PropMon URL, and
data source (live vs. mock), and a **Setup** screen (hold 3s+) for connecting to Wi-Fi without
ever touching source code — see below.

An ambient alert system layers on top of all four data screens: a full-width banner appears when
the worst active alert changes (new alert, or a severity escalation), and a small persistent
badge (position indicates category — tower or propagation) stays visible on every screen for as
long as any alert remains active.

---

## Hardware

| Item | Detail |
|---|---|
| Board | [LilyGO T-Encoder Pro](https://github.com/Xinyuan-LilyGO/T-Encoder-Pro) |
| Chip | ESP32-S3-R8 (8MB PSRAM), 16MB flash |
| Display | 390×390 round AMOLED — **CO5300 or SH8601 driver, varies by unit** (see [Toolchain](#toolchain-notes)) |
| Input | Rotary encoder with push button. A capacitive touch controller is physically present but not used in this firmware. |
| Connectivity | Wi-Fi (station mode + a temporary AP for first-time setup) |

---

## Prerequisites

- A running instance of **[PropMon](https://github.com/N4MI73/streamdeck-hamradio/tree/main/propmon)**,
  reachable on your local network. This firmware is a pure API consumer — see that repo's README
  for deploying it (Portainer/Docker Compose). **This README assumes you're already comfortable
  standing up a Portainer stack**; it doesn't walk through Docker or NAS basics from zero.
- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- A phone or laptop with a Wi-Fi radio, for the on-device setup flow

---

## Toolchain notes

- **Platform: [pioarduino](https://github.com/pioarduino/platform-espressif32) fork, not
  official PlatformIO `espressif32`.** The display library requires ESP32 Arduino core 3.x;
  official PlatformIO's `espressif32` platform only supports core 2.x as of this writing.
  `platformio.ini` already points at the correct platform URL — no action needed beyond a normal
  `pio run`.
- **Windows users:** enable long path support (`LongPathsEnabled=1` in the registry) before
  building — the pioarduino toolchain's file paths can exceed Windows' default limit.
- **Display driver varies by unit.** LilyGO has shipped this board with two different
  panel/touch revisions (SH8601+CHSC5816 and CO5300+CST816). This firmware is built against
  CO5300. If your unit shows a blank screen despite a clean build and flash, try swapping the
  driver class in `display_driver.cpp`/`.h` to `Arduino_SH8601`.

---

## Building & flashing

```
git clone https://github.com/N4MI73/n4mi-desktop-instruments
cd n4mi-desktop-instruments
```

Copy `include/wifi_credentials.h.example` to `include/wifi_credentials.h` — this file is
gitignored and never committed. **As of the real captive-portal setup below, this file is only a
fallback**, used once on a genuinely fresh device (no credentials stored yet) before the on-device
setup flow has ever run. If you'd rather skip the on-device flow entirely for a first build, fill
in real values here; otherwise the placeholder values are fine, since setup will happen on-device.

Build and flash via PlatformIO (`pio run -t upload`, or the PlatformIO IDE's Upload button),
targeting the `tencoder-pro` environment.

---

## Wi-Fi Setup

**On first boot** (or any time credentials aren't yet stored), or **by holding the knob for 3+
seconds** from any screen (a continued hold past the regular 1.5s long-press, which opens
Config):

1. The device opens its own temporary Wi-Fi network, `N4MI-PropMon-Setup` (open, no password)
2. Connect your phone to that network — most phones will auto-prompt a sign-in page; if not,
   open a browser to `192.168.4.1`
3. The page shows a scan of nearby networks — pick yours, enter its password, submit
4. The device validates the credentials (this one step is deliberately blocking — both your
   phone's page and the device's own screen/knob pause for up to ~15 seconds while it confirms
   the password actually works)
5. On success, credentials are saved to the device's onboard flash (NVS) and it immediately
   fetches live data

Setup runs independently of which screen is currently displayed — rotating the knob to check
Overview/Bands/etc. mid-setup doesn't cancel it; only a long-press while actually looking at the
Setup screen does, or a 5-minute timeout if nobody finishes.

**Credentials never touch this repo.** They're either entered on-device (above, stored in NVS
only) or, for the fallback path, kept in your local `include/wifi_credentials.h`, which is
gitignored.

---

## PropMon API expectations

This firmware polls PropMon's `GET /api/instrument/propagation` endpoint — see PropMon's own
README for the full response shape, data sources, and refresh cadence. Two things this firmware
specifically depends on, worth knowing if you're running a different or modified backend:

- **Bands must appear in a fixed order** (`160m, 80m, 40m, 30m, 20m, 17m, 15m, 12m, 10m, 6m`) —
  the firmware validates this on every fetch and **rejects the entire response** if the order (or
  count) doesn't match, rather than risk silently mislabeling a band's condition.
- **The `alerts` array is expected to contain at least one entry even in steady state** — a
  single `{"category": "tower", "level": "NONE", "message": "..."}` placeholder, not an empty
  array. This is what PropMon itself returns; a from-scratch backend should match this shape.

**PropMon's host/port are currently a compile-time constant** (`config.h`,
`PROPMON_DEFAULT_HOST`/`PROPMON_DEFAULT_PORT`) — not yet part of the on-device setup flow. If
you're pointing this at your own PropMon instance, edit those two values and reflash.

---

## Known limitations (current state, not roadmap)

- **No dedicated "Wi-Fi unreachable" or "PropMon unreachable" screens yet.** A failed fetch just
  keeps showing the last successfully-fetched data, with the footer's "Updated Xs ago" text as
  the only (passive) signal that something might be stale.
- **If Wi-Fi drops mid-operation, the automatic reconnect attempt blocks the UI for up to 15
  seconds.** Self-recovers, doesn't corrupt data — just a brief freeze.
- **PropMon's URL isn't configurable on-device** — see above, it's a compile-time constant for
  now.
- **Touch/swipe is not implemented.** The panel's touch controller is present but unused; all
  interaction is via the knob.
- **Manual navigation only** — no auto-cycling between screens.

---

## Project structure

```
n4mi-desktop-instruments/
├── platformio.ini
├── include/
│   ├── config.h                 Pin assignments, timing constants, PropMon host/port
│   ├── display_driver.h
│   ├── encoder.h
│   ├── data_client.h            PropMonData model, mock data, live HTTP fetch
│   ├── ui_common.h              Shared colors, text helpers, alert banner/badge, hold-progress bar
│   ├── wifi_client.h            Wi-Fi connect/status, NVS credential storage
│   ├── wifi_portal.h            Captive portal (AP, DNS redirect, web server)
│   ├── wifi_credentials.h.example
│   └── screens/                 One header per screen
└── src/
    ├── main.cpp                 Screen state machine, event handling, fetch/redraw loop
    ├── display_driver.cpp
    ├── encoder.cpp
    ├── data_client.cpp
    ├── ui_common.cpp
    ├── wifi_client.cpp
    ├── wifi_portal.cpp
    └── screens/                 One .cpp per screen
```

---

## Related Projects

**[PropMon](https://github.com/N4MI73/streamdeck-hamradio/tree/main/propmon)** — the backend
this firmware consumes. Deployment (Portainer/Docker Compose), environment variables, data
sources, refresh cadence, and the full band-rating model are documented in that repo. PropMon is
general-purpose — anything that can make an HTTP GET request can use it; this firmware is simply
its first consumer.

---

## Licensing

Built clean-room, referencing [FlightScnr](https://github.com/yashmulgaonkar/FlightScnr) (CC
BY-NC-SA 4.0) only for hardware feasibility — no code copied. LilyGO's own vendor sample code is
separately GPL-3 and also not used directly.
