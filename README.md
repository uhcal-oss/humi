<div align="center">

# 🌱 HUMI v2

**Intelligent Plant Management System**

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT)
[![Arduino](https://img.shields.io/badge/Arduino-Uno%20R4%20WiFi-00979D?logo=arduino&logoColor=white)](https://store.arduino.cc/products/uno-r4-wifi)
[![Platform](https://img.shields.io/badge/Platform-Renesas%20RA4M1-blue)](https://www.renesas.com/products/microcontrollers-microprocessors/ra-cortex-m-mcus/ra4m1-32-bit-microcontrollers-48mhz-arm-cortex-m4-and-lcd-controller-and-cap-touch-hmi)
[![API](https://img.shields.io/badge/API-REST%20JSON-orange)](https://github.com/uhcal-oss/humi)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](https://github.com/uhcal-oss/humi/pulls)
![Status](https://img.shields.io/badge/Status-Active-success)
![Plants](https://img.shields.io/badge/Plants%20Supported-3-brightgreen)

*Automated soil moisture monitoring, watering, and ventilation for your plants — powered by Arduino Uno R4 WiFi.*

---

</div>

## Overview

HUMI v2 is a fully autonomous plant care system that monitors soil moisture levels, automatically controls water pumps and ventilation fans, and provides a REST API for remote monitoring and control. It sends email alerts via MailerSend when your plants need attention.

### Key Features

- **Automated Plant Care** — Pumps and fans activate/deactivate based on configurable moisture thresholds
- **REST API** — Full JSON API for remote monitoring, control, and configuration
- **Email Alerts** — Automatic notifications for low/high moisture, sensor disconnection, and daily status reports via MailerSend
- **SD Card Persistence** — Credentials, thresholds, and logs saved to SD card and survive power cycles
- **Sensor Detection** — Standard deviation-based algorithm distinguishes connected vs. disconnected sensors
- **NTP Time Sync** — Real-time clock via WiFi with hourly resync
- **Watchdog Timer** — Automatic recovery from system hangs (16s timeout)
- **Security** — Bearer token authentication, configurable CORS, rate limiting (10 req/s)

## Hardware

| Component | Specification |
|-----------|---------------|
| **Board** | Arduino Uno R4 WiFi |
| **Processor** | 48 MHz ARM Cortex-M4 (Renesas RA4M1) |
| **WiFi** | ESP32-S3 module (built-in) |
| **RAM / Flash** | 32 KB / 256 KB |
| **ADC** | 12-bit resolution |
| **Soil Sensors** | 3× capacitive moisture sensors (A0, A1, A2) |
| **Pumps** | 3× water pumps (D2, D4, D6) |
| **Fans** | 3× ventilation fans (D3, D5, D7) |
| **Storage** | SD card module (SPI: MOSI→11, MISO→12, SCK→13, CS→10) |

### Wiring Diagram

```
Arduino Uno R4 WiFi
├── A0 ─── Soil Sensor 0 (Martha)
├── A1 ─── Soil Sensor 1 (Menekşe)
├── A2 ─── Soil Sensor 2 (Orkide)
├── D2 ─── Pump 0
├── D3 ─── Fan 0
├── D4 ─── Pump 1
├── D5 ─── Fan 1
├── D6 ─── Pump 2
├── D7 ─── Fan 2
└── SPI ─── SD Card Module (CS: D10)
```

## Getting Started

### Prerequisites

- [Arduino IDE](https://www.arduino.cc/en/software) 2.x or [Arduino CLI](https://arduino.github.io/arduino-cli/)
- Board package: **Arduino UNO R4 Boards** (`arduino:renesas_uno`)

### Required Libraries

| Library | Purpose |
|---------|---------|
| `WiFiS3` | WiFi connectivity (included with board package) |
| `SD` | SD card read/write |
| `SPI` | SPI communication |
| `WiFiSSLClient` | HTTPS for MailerSend API |
| `ArduinoHttpClient` | HTTP client for email sending |
| `ArduinoJson` | JSON parsing for API requests |
| `WDT` | Watchdog timer (included with board package) |

### Installation

1. **Clone the repository**
   ```bash
   git clone https://github.com/uhcal-oss/humi.git
   cd humi
   ```

2. **Open in Arduino IDE**
   Open `main.ino` in Arduino IDE.

3. **Install libraries**
   Via Arduino Library Manager, install:
   - `ArduinoHttpClient`
   - `ArduinoJson`

4. **Configure SD card** (recommended)
   Create a `config.txt` file on your SD card:
   ```ini
   # HUMI v2 Configuration
   WIFI_SSID=YourNetworkName
   WIFI_PASSWORD=YourPassword
   MAILERSEND_TOKEN=mlsn.your_token_here
   FROM_EMAIL=humi@yourdomain.com
   TO_EMAIL=you@email.com
   API_TOKEN=your-secret-api-token
   CORS_ORIGIN=https://your-website.com
   ```

5. **Upload**
   Select board **Arduino Uno R4 WiFi** and upload.

## API Reference

All endpoints return JSON. POST endpoints require `Authorization: Bearer <token>` if `API_TOKEN` is configured.

### Status & Health

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/` or `/health` | Health check |
| `GET` | `/api/status` | Complete system status with sensor data, logs, and uptime |
| `GET` | `/api/status/fast` | Minimal status for quick polling |

### Plant Control

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/api/plant/{id}/status` | Individual plant status with logs (`id`: 0–2) |
| `POST` | `/api/plant/{id}/pump` | Toggle water pump |
| `POST` | `/api/plant/{id}/fan` | Toggle ventilation fan |

### Configuration

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/api/thresholds` | Get moisture thresholds |
| `POST` | `/api/thresholds` | Update thresholds (saved to SD) |

**Example — Update thresholds:**
```bash
curl -X POST http://<device-ip>/api/thresholds \
  -H "Authorization: Bearer your-token" \
  -H "Content-Type: application/json" \
  -d '{"moisture_too_low": 25, "moisture_too_high": 75}'
```

### System

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST` | `/api/system/restart` | Restart the device |
| `POST` | `/api/system/testmode` | Toggle test mode (bypasses sensor detection) |
| `GET` | `/api/logs/export` | Export logs to SD card |

### Email

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/api/email/settings` | Get email notification settings |
| `POST` | `/api/email/settings` | Enable/disable notifications |
| `POST` | `/api/email/test` | Send a test email |
| `POST` | `/api/email/status` | Send system status email |

## SD Card Files

| File | Purpose | Format |
|------|---------|--------|
| `config.txt` | Credentials & settings | `KEY=VALUE` per line, `#` for comments |
| `thresh.txt` | Persisted moisture thresholds | Auto-managed, `KEY=VALUE` |
| `system.txt` | System boot info | Auto-generated |
| `plant_log_N.txt` | Per-plant log files | Timestamped entries |
| `all_logs.txt` | Exported log archive | Created via `/api/logs/export` |

## Moisture Thresholds

The system uses a hysteresis-based control scheme:

```
0%                    30%    40%         60%    70%                   100%
├─── TOO DRY ─────────┤      ├── IDEAL ──┤      ├─── TOO WET ────────┤
     Pump turns ON ────┘      └───────────┘      └── Fan turns ON
                        Pump turns OFF ──┘  Fan turns OFF ──┘
```

| Threshold | Default | Description |
|-----------|---------|-------------|
| `moisture_too_low` | 30% | Pump activates below this |
| `moisture_ideal_low` | 40% | Pump deactivates at this level |
| `moisture_ideal_high` | 60% | Fan deactivates at this level |
| `moisture_too_high` | 70% | Fan activates above this |

## Security

- **Authentication** — POST endpoints require `Authorization: Bearer <token>` when `API_TOKEN` is set in `config.txt`
- **CORS** — Configurable via `CORS_ORIGIN` in `config.txt` (defaults to `*`)
- **Rate Limiting** — 10 requests per second; returns `429 Too Many Requests` when exceeded
- **Credentials** — All secrets loaded from SD card; defaults in code as fallback

## Plants

| ID | Name | Sensor | Pump | Fan |
|----|------|--------|------|-----|
| 0 | Martha (Yaprak Güzeli) | A0 | D2 | D3 |
| 1 | Menekşe | A1 | D4 | D5 |
| 2 | Orkide | A2 | D6 | D7 |

## Contributors

| Contributor | Seasons | GitHub |
|-------------|---------|--------|
| **Batuhan Türkoğlu** | 2024–2025–2026 | [@Batuhantrkgl](https://github.com/Batuhantrkgl) |
| **Melike Ebru** | 2024–2025 | — |
| **Rüzgar Burak Tunç** | 2024–2025 | [@DrRuzgar](https://github.com/DrRuzgar) |

## License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.

---

<div align="center">

*Built with care for TÜBİTAK*

</div>
