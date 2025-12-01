# VAG Freiburg Tram Departure Display

> Real-time tram departure information on an ESP32-powered OLED display

<div align="center">

![ESP32](https://img.shields.io/badge/ESP32-DOIT_DevKit_v1-blue?style=flat-square&logo=espressif)
![PlatformIO](https://img.shields.io/badge/PlatformIO-Arduino-orange?style=flat-square&logo=platformio)
![License](https://img.shields.io/badge/License-MIT-green?style=flat-square)

</div>

---

## Overview

A compact embedded system that displays live departure times for German public transit. The display shows the next departures for your configured station and directions, complete with delay information and countdown timers.

```
┌────────────────────────────┐
│  17:26+3          in 4'    │
│  17:34            in 12'   │
│  17:42+1          in 19'   │
└────────────────────────────┘
```

## Features

- **Real-time data** from the German Rail transport API
- **Smart filtering** — shows only relevant directions
- **Delay indicators** — see if your tram is running late
- **Countdown display** — minutes until departure
- **Power efficient** — deep sleep between updates

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
                    │                                         │
    ┌───────────┐   │  3V3 ○────────────────┬─────○ VCC      │
    │  RESET    │   │                       │                │
    │  BUTTON   │   │  EN  ○───┐            │    SSD1306     │
    │   ┌───┐   │   │          │            │    ┌──────┐    │
    │   │   │───┼───┼──────────┤            ├────│ VCC  │    │
    │   └───┘   │   │          │            │    │      │    │
    └───────────┘   │  GND ○───┴────────────┴────│ GND  │    │
                    │                            │      │    │
                    │  GPIO21 ○──────────────────│ SDA  │    │
                    │                            │      │    │
                    │  GPIO22 ○──────────────────│ SCL  │    │
                    │                            └──────┘    │
                    └─────────────────────────────────────────┘

           POWER SUPPLY (3× AAA → 3.3V)
    ┌─────────────────────────────────────────────────────────┐
    │                                                         │
    │   3× AAA          MCP1700-3302E                        │
    │   (4.5V)          ┌─────────┐                          │
    │     +             │         │                          │
    │     ├────┬────────┤ VIN     │                          │
    │     │   ─┴─       │      VOUT├───────────→ ESP32 3V3   │
    │    ─┴─  ───  1µF  │         │        │                 │
    │    ───   │        │     GND │       ─┴─                │
    │     │    │        └────┬────┘       ───  1µF           │
    │     └────┴─────────────┴─────────────┴──→ ESP32 GND    │
    │     -                                                   │
    └─────────────────────────────────────────────────────────┘
```

### Component Details

| Part | Description |
|------|-------------|
| **Reset Button** | Momentary push button between EN and GND — wakes ESP32 from deep sleep |
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

// Station ID - find yours at https://v6.db.transport.rest/locations?query=YOUR_STATION
const char* STATION_ID = "YOUR_STATION_ID";

// Comma-separated list of direction keywords to filter for
// Only departures containing one of these strings will be shown
// Leave empty ("") to show all departures
const char* DIRECTION_FILTER = "Hauptbahnhof,Marktplatz";
```

### Finding Your Station ID

1. Open your browser and go to:
   ```
   https://v6.db.transport.rest/locations?query=YOUR_STATION_NAME
   ```
   Replace `YOUR_STATION_NAME` with your station (e.g., `Hauptbahnhof` or `Münsterplatz`).

2. Find your station in the JSON response and copy the `id` field.

### Filtering Directions

The `DIRECTION_FILTER` setting lets you show only departures heading in specific directions:

- Use partial matches: `"Haupt"` will match `"Hauptbahnhof"`
- Separate multiple filters with commas: `"Hauptbahnhof,Marktplatz"`
- Leave empty to show all departures: `""`

### Other Settings

In `src/main.cpp`:

| Setting | Default | Description |
|---------|---------|-------------|
| `awakeTimeMs` | `30000` | Display on-time before sleep (ms) |

## Dependencies

Managed automatically by PlatformIO:

- `ArduinoJson` — JSON parsing
- `Adafruit GFX Library` — Graphics primitives
- `Adafruit SSD1306` — Display driver

## How It Works

```
┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐
│  Boot   │───►│  WiFi   │───►│  Fetch  │───►│ Display │
│         │    │ Connect │    │  Data   │    │  30sec  │
└─────────┘    └─────────┘    └─────────┘    └────┬────┘
                                                  │
                    ┌─────────┐                   │
                    │  Deep   │◄──────────────────┘
                    │  Sleep  │
                    └─────────┘
```

1. **Boot** — Initialize display, show progress
2. **Connect** — Join WiFi network
3. **Sync** — Get current time via NTP
4. **Fetch** — Query departures from API
5. **Display** — Show next 3 matching trams
6. **Sleep** — Enter deep sleep to save power

---

<div align="center">

Made with caffeine and public transit enthusiasm

</div>
