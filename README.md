# HAssPanel – ESP32-S3 LVGL Dashboard for Home Assistant

A fully configurable 7" touch dashboard for Home Assistant, running on the ESP32-S3 with an 800×480 RGB panel. Configure entities, topics, colors, fonts, and layout from the built-in web wizard or a plain-text file on the SD card — no recompiling needed.

![LVGL Dark Theme Dashboard](https://img.shields.io/badge/LVGL-8.3-blue) ![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32--S3-orange) ![License](https://img.shields.io/badge/License-MIT-green)

---

## Hardware

| Component | Details |
|-----------|---------|
| Board | **ESP32-8048S070** (ESP32-S3, 16 MB Flash, 8 MB OPI-PSRAM) |
| Display | 7" RGB panel, 800×480 px |
| Touch | GT911 (I²C, auto-detect 0x5D / 0x14) |
| SD Card | SPI, for `/config.txt` |
| Backlight | GPIO 2 (active HIGH) |

### Pin Assignments (RGB Panel)

| Signal | GPIO |
|--------|------|
| DE | 41 |
| VSYNC | 40 |
| HSYNC | 39 |
| PCLK | 42 |
| R0–R4 | 14, 21, 47, 48, 45 |
| G0–G5 | 9, 46, 3, 8, 16, 1 |
| B0–B4 | 15, 7, 6, 5, 4 |
| Touch SDA | 19 |
| Touch SCL | 20 |
| Touch RST | 38 |

---

## Features

- Up to **12 configurable entities** — Sensor, Switch, or Group
- **Group entities** with up to 8 sub-values per tile
- Sub-values can be **read-only** or **toggle switches** (MQTT publish on tap)
- **Web configuration wizard** with an 800×480 live preview
- Automatic **MQTT topic discovery** with live values in the wizard
- **Custom tile layout** by defining the tile count for each row, for example `layout=2,3,2`
- Configurable **theme colors** plus per-entity and per-sub-value colors and font sizes
- Optional title-free group tiles for compact value-only layouts
- **LVGL 8.3** dark theme, Montserrat fonts
- **MQTT** subscribe (sensors) and publish (switches)
- Automatic **WiFi recovery** every 5 seconds with MQTT reconnect and topic resubscription
- **SD card config** — change entities without reflashing
- **NTP clock** in the header bar
- **Configurable panel title** (`panel_title=`) and sub-label color (`sub_label_color=`)
- **Boot screen** with live status (WiFi, OTA, MQTT)
- **FreeRTOS** dual-core: MQTT on Core 0, LVGL on Core 1
- **OTA firmware update** via ArduinoOTA (PlatformIO `ha_panel_ota` environment)
- **Web firmware upload** at `/firmware` — flash `.bin` directly from any browser
- **HTTP SD-card upload** — update `config.txt` via browser

---

## Dependencies

Managed via PlatformIO (`platformio.ini`):

| Library | Version |
|---------|---------|
| [GFX Library for Arduino](https://github.com/moononournation/Arduino_GFX) | 1.4.9 |
| [PubSubClient](https://github.com/knolleary/pubsubclient) | ^2.8 |
| [ArduinoJson](https://arduinojson.org/) | ^7.0 |
| [TAMC_GT911](https://github.com/TAMCTec/gt911-arduino) | ^1.0.2 |
| [lvgl](https://lvgl.io/) | ^8.3 |

---

## Getting Started

### 1 – Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- SD card formatted as FAT32
- ESP32-8048S070 connected via USB-C

### 2 – First-time USB Flash

```bash
git clone https://github.com/Evohl/HAssPanel-ESP32-S3-LVGL-dashboard.git
cd HAssPanel-ESP32-S3-LVGL-dashboard/espdisplay/HAssPanel

# Copy the config template to the SD card root and fill in your credentials
cp config_template.txt /path/to/sdcard/config.txt   # edit before inserting!

# Flash via USB (first time only)
pio run -e ha_panel -t upload
```

Insert the SD card, power on — the display connects to WiFi and shows the dashboard.

### 3 – Web Management Interface

Once online, open **`http://<hostname>.local/`** (or the IP shown on the display) in any browser:

| Page | URL | What it does |
|------|-----|--------------|
| Status | `/` | IP, uptime, heap, WiFi RSSI, MQTT state |
| Wizard | `/wizard` | Configure tiles, topics, colors, fonts, and layout with a live display preview |
| Config | `/config` | Edit `config.txt` directly or upload a replacement file |
| Log | `/log` | Live log, refreshed every second with the latest 80 entries |
| Firmware | `/firmware` | Upload and flash a PlatformIO `.bin` file from the browser |
| Restart | `/restart` | Restart the panel remotely |
| MQTT API | `/api/topics` | Return the MQTT connection state and discovered topic values as JSON |

The wizard discovers MQTT topics received by the panel and uses their current values in the preview. It supports Sensor, Switch, and Group tiles, read-only or switchable sub-values, predefined color themes, custom colors, font sizes, and custom row layouts.

Saving through the wizard or config editor writes `/config.txt`, keeps the previous file as `/config.txt.bak`, and restarts the panel automatically.

### 4 – OTA Firmware Update (all subsequent flashes — no USB needed)

Edit `platformio.ini` and set `upload_port` to the display's IP:

```ini
[env:ha_panel_ota]
extends = env:ha_panel
upload_protocol = espota
upload_port = <panel-ip>    # ← your panel's IP
upload_flags = --host_ip=<your-pc-ip>  # ← your PC's IP on the same subnet
```

Then flash wirelessly:

```bash
pio run -e ha_panel_ota -t upload
```

> **Linux firewall note:** During OTA the ESP opens a TCP connection *back* to your PC.
> If you use `firewalld` (common on Fedora/RHEL), mark your local subnet as trusted so
> incoming connections from the ESP are not rejected:
> ```bash
> sudo firewall-cmd --zone=trusted --add-source=<your-local-subnet/mask> --permanent
> sudo firewall-cmd --reload
> ```
> Also fix a Python 3 incompatibility in the bundled `espota.py` (line 220):
> ```python
> # change:  except e:
> # to:      except Exception as e:
> ```
> File: `~/.platformio/packages/framework-arduinoespressif32/tools/espota.py`

### 5 – Multiple Panels

Each panel needs a unique hostname in its `config.txt`:

```ini
hostname=hassPanel1   # first display  → http://hassPanel1.local/
hostname=hassPanel2   # second display → http://hassPanel2.local/
```

Set the hostname via the web editor before the first OTA, or place a pre-configured `config.txt` on each SD card.

---

## Configuration (`config.txt`)

Copy `config_template.txt` to the SD card root as `config.txt` and edit it. Lines starting with `//` or `#` are ignored.

### Global Settings

```ini
WiFi_ssid=YOUR_SSID
WiFi_password=YOUR_PASSWORD
hostname=hassPanel1
MQTT_server=192.168.1.x
MQTT_port=1883
MQTT_user=
MQTT_passwd=
NTP_server=pool.ntp.org
NTP_timezone=CET-1CEST,M3.5.0/02,M10.5.0/03
panel_title=HOME ASSISTANT PANEL   // header text on boot screen
bg_color=0D1117                    // screen background
header_bg_color=0D1117             // header background
tile_bg_color=161B22               // tile background
accent_color=58A6FF                // panel title and progress bar
title_color=8B949E                 // tile title color
sub_label_color=8B949E             // sub-label text color (6-digit hex)
```

### Layout

```ini
// 2 tiles in row 1, 3 tiles in row 2
layout=2,3
```

Each number is the number of tiles in that row. Leave `layout` empty or omit it for automatic layout.

### Entity Types

#### Sensor
```ini
entity1_type=sensor
entity1_label=Solar
entity1_topic=homeassistant/sensor/solar_power/state
entity1_unit=W
entity1_color=FEA020
entity1_font_size=28
```

#### Switch
```ini
entity2_type=switch
entity2_label=Pumpe
entity2_topic=homeassistant/switch/pumpe/state
entity2_cmd_topic=homeassistant/switch/pumpe/set
entity2_color=58A6FF
```

#### Group (sensor + sub-values)
```ini
entity3_type=group
entity3_label=Batterie
entity3_topic=homeassistant/sensor/battery_level/state
entity3_unit=%
entity3_color=3FB950
entity3_sub_count=2
entity3_sub1_label=Laden
entity3_sub1_topic=homeassistant/sensor/battery_charge/state
entity3_sub1_unit=W
entity3_sub1_color=58A6FF
entity3_sub1_font_size=16
entity3_sub2_label=Entladen
entity3_sub2_topic=homeassistant/sensor/battery_discharge/state
entity3_sub2_unit=W
```

A group with no `entityN_topic` shows only sub-values (no main value).

Set `entityN_hide_title=1` to hide a group tile's title and use the available space for its sub-values.

#### Sub-value as toggle switch
```ini
entity3_sub1_cmd=homeassistant/switch/fan/set
```
When `sub_cmd` is set, the sub renders as a tap-to-toggle switch instead of a label.

### Font Sizes

Set `entityN_font_size` or `entityN_subM_font_size` to `12`, `14`, `16`, `20`, `24`, or `28`. Omit the setting or use `0` to select a size automatically based on the tile dimensions.

### Colors

All colors are 6-digit hex without `#`, e.g. `FEA020`.

| Preset | Hex |
|--------|-----|
| Orange (Solar) | `FEA020` |
| Green (OK) | `3FB950` |
| Red (Alert) | `F85149` |
| Blue (Power) | `58A6FF` |
| Grey (Title) | `8B949E` |

---

## Architecture

```
Core 0                      Core 1 (lvgl_task)
──────────────────────      ───────────────────────────
mqtt_task:                  lv_timer_handler()  ← suspended during OTA
  WiFi reconnect            1-sec entity flush
  MQTT reconnect            ui_update_entity()
  mqttClient.loop()         ui_update_header()
ota_task:               ↑   boot screen (ui_boot_show/status)
  ArduinoOTA.handle()   │
  httpServer.handle()   │
  /firmware upload      │
          └── g_lvgl_mutex (FreeRTOS) ──┘

During OTA flash:
  lvgl_task  → vTaskSuspend (no PSRAM framebuffer writes)
  mqtt_task  → vTaskSuspend
  backlight  → off  (no visible DMA flicker)
  ota_task   → ArduinoOTA.handle() at 1ms polling
```

If WiFi drops, `mqtt_task` closes the stale MQTT connection and explicitly retries WiFi every 5 seconds. Once WiFi is back, MQTT reconnects and subscribes to all configured topics again.

MQTT sets `entity.dirty = true`; the LVGL task flushes all dirty entities once per second.

---

## File Overview

| File | Purpose |
|------|---------|
| `HAssPanel.ino` | Main sketch, globals, FreeRTOS tasks, OTA task-suspend logic |
| `display.ino` | LVGL display + touch driver init |
| `ui.ino` | Tile construction, UI updates, boot screen, OTA overlay |
| `mqtt.ino` | MQTT callback, subscribe, reconnect |
| `config.ino` | SD card config parser |
| `ota.ino` | ArduinoOTA + HTTP web server (config, log, firmware upload) |
| `config_template.txt` | Config template (copy to SD as `config.txt`) |
| `platformio.ini` | PlatformIO build config (`ha_panel` USB, `ha_panel_ota` OTA) |
| `lv_conf.h` | LVGL configuration |

---

## License

MIT — see [LICENSE](LICENSE)
