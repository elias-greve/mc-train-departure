<div align="center">

# Tram Departure Display

**Real-time tram departures on a battery-powered ESP32 OLED display**

[![ESP32](https://img.shields.io/badge/ESP32-DOIT_DevKit_v1-000000?style=for-the-badge&logo=espressif&logoColor=E7352C)](https://www.espressif.com/)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-Arduino-FF7F00?style=for-the-badge&logo=platformio&logoColor=white)](https://platformio.org/)
[![License](https://img.shields.io/badge/License-MIT-22C55E?style=for-the-badge)](LICENSE)

<br>

<img src="images/mc-train-departures.gif" alt="OLED Display showing tram departures" width="320">

<br>

**Live departure times** · **Delay tracking** · **Press-to-power** · **Months of battery life**

*Configurable for any German train, tram, or bus station*

---

</div>

## Overview

A compact embedded system that fetches live departure times for German public transit and displays them on a crisp OLED screen. Configure your station, filter by direction, and never miss your tram again.

<table>
<tr>
<td width="50%">

### Features

- **Real-time data** from VAG Freiburg's EFA system
- **Smart filtering** — shows only relevant directions
- **Delay indicators** — see if your tram is running late
- **Countdown display** — minutes until departure
- **Press-to-power** — momentary switch cuts the battery completely between checks; zero quiescent draw

</td>
<td width="50%">

```
┌────────────────────────────┐
│  17:26+3          in 4'    │
│  17:34            in 12'   │
│  17:42+1          in 19'   │
└────────────────────────────┘
```

</td>
</tr>
</table>

## Hardware

| Component | Specification |
|-----------|---------------|
| Microcontroller | ESP32 DOIT DevKit v1 |
| Display | SSD1306 OLED 128×64 |
| Interface | I2C (SDA: GPIO 21, SCL: GPIO 22) |
| Power | 3× AAA batteries + MCP1700 regulator |

### Wiring Diagram

```
                    ┌─────────────────────────────────────────┐
                    │              ESP32 DevKit               │
                    │                                SSD1306  │
                    │                               ┌──────┐  │
                    │  3V3 ○────────────────────────│ VCC  │  │
                    │                               │      │  │
                    │  GND ○────────────────────────│ GND  │  │
                    │                               │      │  │
                    │  GPIO21 ○─────────────────────│ SDA  │  │
                    │                               │      │  │
                    │  GPIO22 ○─────────────────────│ SCL  │  │
                    │                               └──────┘  │
                    └─────────────────────────────────────────┘

       POWER SUPPLY (3× AAA → momentary switch → MCP1700 → 3.3V)
    ┌─────────────────────────────────────────────────────────┐
    │                                                         │
    │   3× AAA      MOMENTARY      MCP1700-3302E             │
    │   (4.5V)       SWITCH        ┌─────────┐               │
    │     +     ╱○                 │         │               │
    │     ├────○ ──────┬───────────┤ VIN     │               │
    │     │           ─┴─          │      VOUT├──→ ESP32 3V3 │
    │    ─┴─          ───  1µF     │         │     │         │
    │    ───           │           │     GND │    ─┴─        │
    │     │            │           └────┬────┘    ───  1µF   │
    │     └────────────┴────────────────┴─────────┴──→ ESP32 GND │
    │     -                                                   │
    └─────────────────────────────────────────────────────────┘

  Hold the switch to power the device. Releasing it cuts the battery
  completely — no quiescent draw, no deep sleep needed.
```

### Component Details

| Part | Description |
|------|-------------|
| **Momentary Switch** | Press-and-hold push button in series with the battery's positive lead. Releasing fully disconnects the battery |
| **MCP1700-3302E** | Low-dropout 3.3V regulator (250mA max, ideal for battery operation) |
| **1µF Capacitors** | Ceramic caps on VIN and VOUT for regulator stability |
| **3× AAA** | Provides ~4.5V nominal (works down to ~3.6V with MCP1700) |

## Quick Start

### 1. Clone & Configure

```bash
git clone https://github.com/elias-greve/mc-train-departure.git
cd mc-train-departure
```

Copy the secrets template and configure it for your station:

```bash
cp src/secrets.h.example src/secrets.h
```

Edit `src/secrets.h` with your settings (see [Customization](#customization) below).

### 2. Build & Upload

Using **PlatformIO CLI**:

```bash
pio run -t upload
```

Or use the PlatformIO IDE upload button in VS Code.

### 3. Monitor

```bash
pio device monitor
```

## Project Structure

```
mc-train-departure/
├── src/
│   ├── main.cpp              # Application logic
│   ├── secrets.h             # WiFi credentials (git-ignored)
│   └── secrets.h.example     # Credentials template
├── include/                  # Header files
├── lib/                      # Custom libraries
├── platformio.ini            # Build configuration
└── README.md
```

## Customization

All user settings are in `src/secrets.h`:

```cpp
// WiFi credentials
const char* WIFI_SSID = "your-network";
const char* WIFI_PASSWORD = "your-password";

// VAG Stop ID - find yours at:
// https://efa.vagfr.de/vagfr3/XSLT_STOPFINDER_REQUEST?outputFormat=JSON&type_sf=any&name_sf=YOUR_STOP_NAME
const char* STATION_ID = "YOUR_STOP_ID";  // see lookup link below

// Comma-separated list of direction keywords to filter for
// Only departures containing one of these strings will be shown
// Leave empty ("") to show all departures
const char* DIRECTION_FILTER = "YOUR_DIRECTION";
```

### Finding Your Stop ID

The firmware queries VAG Freiburg's EFA system directly — no API key, no
intermediary server, no rate limits.

1. Open your browser and go to:
   ```
   https://efa.vagfr.de/vagfr3/XSLT_STOPFINDER_REQUEST?outputFormat=JSON&type_sf=any&name_sf=YOUR_STOP_NAME
   ```
   Replace `YOUR_STOP_NAME` with your stop name.

2. Find your stop in the JSON response and copy the stop's numeric ID.
   Pick a specific stop (e.g. `STOP_ID` — "Stop name from response"),
   not a station area.

### Filtering Directions

The `DIRECTION_FILTER` setting lets you show only departures heading in specific directions:

- Use partial matches: `"Haupt"` will match `"Hauptbahnhof"`
- Separate multiple filters with commas: `"Hauptbahnhof,Marktplatz"`
- Leave empty to show all departures: `""`

### Other Cities

The firmware currently targets VAG Freiburg's EFA endpoint. If you'd like
support for a different city's transit system, open an issue — I might
take a stab at adapting the code so it works for your provider too.

### Other Settings

In `src/main.cpp`:

| Setting | Default | Description |
|---------|---------|-------------|
| `awakeTimeMs` | `10000` | Maximum display on-time after data is loaded (ms). The switch cuts power on release, so this is just an upper bound before the firmware would enter deep sleep on its own. |

## Prerequisites

Install the required tools via [Homebrew](https://brew.sh/):

```bash
brew install platformio clang-format
```

| Tool | Purpose |
|------|---------|
| **PlatformIO** | Build, upload, and test firmware |
| **clang-format** | Auto-format C/C++ source files |

A `Makefile` is provided for convenience:

| Command | Description |
|---------|-------------|
| `make test` | Run unit tests (native) |
| `make format` | Format all source files |
| `make upload` | Build and flash to ESP32 |
| `make monitor` | Open serial monitor |

## Dependencies

Managed automatically by PlatformIO:

- `ArduinoJson` — JSON parsing
- `Adafruit GFX Library` — Graphics primitives
- `Adafruit SSD1306` — Display driver

## How It Works

```
   ┌────────┐     ┌─────────┐     ┌─────────┐     ┌─────────┐     ┌────────┐
   │ Press  │────►│  Boot   │────►│  WiFi   │────►│  Fetch  │────►│Display │
   │ switch │     │ + spin  │     │ Connect │     │  Data   │     │results │
   └────────┘     └─────────┘     └─────────┘     └─────────┘     └────┬───┘
                                                                       │
                                                                  Release switch
                                                                       │
                                                                       ▼
                                                                  Power off
                                                                  (zero draw)
```

1. **Press the switch** — battery connects to the regulator, ESP32 boots
2. **Boot + spinner** — display lights up immediately, an animated spinner runs while WiFi and the HTTP fetch happen on the main core
3. **Connect** — Join WiFi network
4. **Fetch** — Query VAG Freiburg EFA over plain HTTP for the next departures
5. **Display results** — Spinner is replaced by up to three matching trams
6. **Release the switch** — battery is physically disconnected; nothing runs, nothing drains

## Power Profile

Because the switch cuts the battery completely on release, there is no
"sleep current" to dominate battery life — only the energy spent during
each button press.

### Component Current Draw

| Component | While powered | While switch open |
|-----------|---------------|-------------------|
| **ESP32 (active)** | ~80 mA | 0 mA |
| **WiFi TX peaks** | up to ~240 mA | 0 mA |
| **SSD1306 OLED** | ~20 mA (lit) | 0 mA |
| **MCP1700 Regulator** | ~1.6 µA quiescent | 0 mA |
| **Total** | **~100 mA average** | **0 mA** |

### Energy Per Activation

Each press is roughly:

| Phase | Duration | Current | Energy (mAh) |
|-------|----------|---------|--------------|
| Boot + WiFi connect | ~3 s | ~120 mA | 0.10 |
| HTTP fetch + parse | ~0.5 s | ~110 mA | 0.015 |
| Display lit (`awakeTimeMs`) | ~10 s | ~25 mA | 0.07 |
| **Total per activation** | **~13 s** | — | **~0.19 mAh** |

> The switchover from HTTPS to plain HTTP shaves roughly 1–2 s of TLS
> handshake from each cycle.

### Estimated Battery Life

Battery life scales linearly with how often you press the button:

| Usage pattern | Activations/day | Daily energy | Life on 900 mAh AAA pack |
|---------------|-----------------|--------------|--------------------------|
| Minimal | 1 | 0.19 mAh | ~13 years (limited by shelf life) |
| Typical | 3 | 0.57 mAh | ~4 years (limited by shelf life) |
| Moderate | 5 | 0.95 mAh | ~2.6 years (limited by shelf life) |
| Heavy | 20 | 3.8 mAh | ~8 months |

> **Note:** Real-world battery life will usually be capped by alkaline
> shelf life (~5–10 years) or NiMH self-discharge, not by the device.
> Assumes ~900 mAh usable capacity from 3× AAA alkalines down to the
> MCP1700's ~3.6 V dropout point.

### Tips

1. **Don't hold the switch longer than you need to read the screen** — the display draws ~25 mA continuously while powered.
2. **Use quality alkaline batteries** for the longest shelf life. NiMH works too but self-discharges faster, so its calendar life is shorter even though its capacity is similar.

---

<div align="center">

Made with caffeine and public transit enthusiasm

</div>
