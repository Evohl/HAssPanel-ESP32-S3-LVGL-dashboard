# HAssPanel – ESP32-S3 LVGL Dashboard for Home Assistant

A fully configurable 7" touch dashboard for Home Assistant, running on the ESP32-S3 with an 800×480 RGB panel. All entities, topics, colors, and layout are defined via a plain-text config file on the SD card — no recompiling needed.

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

- Up to **8 configurable entities** — Sensor, Switch, or Group
- **Group entities** with up to 5 sub-values per tile
- Sub-values can be **read-only** or **toggle switches** (MQTT publish on tap)
- **Color thresholds** per entity (up to 3 breakpoints)
- **Custom tile layout** via `layout=rows,cols` in config
- **LVGL 8.3** dark theme, Montserrat fonts
- **MQTT** subscribe (sensors) and publish (switches)
- **SD card config** — change entities without reflashing
- **NTP clock** in the header bar
- **FreeRTOS** dual-core: MQTT on Core 0, LVGL on Core 1
- **OTA firmware update** via ArduinoOTA (PlatformIO `ha_panel_ota` environment)
- **HTTP SD-card upload** — update `config.txt` via browser at `http://hassPanel.local/`

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

## Build & Flash

1. Install [PlatformIO](https://platformio.org/)
2. Clone this repo
3. Copy `config_template.txt` to your SD card root as **`/config.txt`** and fill in your credentials
4. Build and flash (first time via USB):

```bash
pio run -e ha_panel -t upload
```

### OTA Firmware Update (after first USB flash)

```bash
pio run -e ha_panel_ota -t upload
```

The device is reachable as `hassPanel.local` via mDNS. The `upload_port` in `platformio.ini` can also be set to the device's IP address.

### Update `config.txt` via Browser

Open `http://hassPanel.local/` in a browser, select the new `config.txt` and click **Hochladen**. The display restarts automatically. A backup of the previous config is kept as `/config.txt.bak` on the SD card.

---

## Configuration (`config.txt`)

Copy `config_template.txt` to the SD card root as `config.txt` and edit it. Lines starting with `//` or `#` are ignored.

### Global Settings

```ini
WiFi_ssid=YOUR_SSID
WiFi_password=YOUR_PASSWORD
MQTT_server=192.168.1.x
MQTT_port=1883
MQTT_user=
MQTT_passwd=
NTP_server=pool.ntp.org
NTP_timezone=CET-1CEST,M3.5.0/02,M10.5.0/03
title_color=8B949E
```

### Layout

```ini
// 2 tiles in row 1, 3 tiles in row 2
layout=2,3
```

Leave empty for automatic layout (2-column grid).

### Entity Types

#### Sensor
```ini
entity1_type=sensor
entity1_label=Solar
entity1_topic=homeassistant/sensor/solar_power/state
entity1_unit=W
entity1_color=FEA020
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
entity3_sub2_label=Entladen
entity3_sub2_topic=homeassistant/sensor/battery_discharge/state
entity3_sub2_unit=W
```

A group with no `entityN_topic` shows only sub-values (no main value).

#### Sub-value as toggle switch
```ini
entity3_sub1_cmd=homeassistant/switch/fan/set
```
When `sub_cmd` is set, the sub renders as a tap-to-toggle switch instead of a label.

### Color Thresholds

Up to 3 breakpoints, evaluated in order (first match wins):

```ini
entity3_thresh_count=2
entity3_thresh1_val=20
entity3_thresh1_color=F85149
entity3_thresh2_val=50
entity3_thresh2_color=FEA020
// >= 50 → falls back to entity base color
```

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
Core 0                 Core 1 (lvgl_task)
──────────────────     ─────────────────────
mqtt_task:             lv_timer_handler()
  WiFi reconnect       1-sec entity flush
  MQTT reconnect       ui_update_entity()
  mqttClient.loop()    ui_update_header()
ota_task:
  ArduinoOTA.handle()
  httpServer.handle()
          └── g_lvgl_mutex (FreeRTOS) ──┘
```

MQTT sets `entity.dirty = true`; the LVGL task flushes all dirty entities once per second.

---

## File Overview

| File | Purpose |
|------|---------|
| `HAssPanel.ino` | Main sketch, globals, FreeRTOS tasks |
| `display.ino` | LVGL display + touch driver init |
| `ui.ino` | Tile construction and UI updates |
| `mqtt.ino` | MQTT callback, subscribe, reconnect |
| `config.ino` | SD card config parser |
| `ota.ino` | ArduinoOTA + HTTP SD-card upload server |
| `config_template.txt` | Config template (copy to SD as `config.txt`) |
| `platformio.ini` | PlatformIO build config (`ha_panel` USB, `ha_panel_ota` OTA) |
| `lv_conf.h` | LVGL configuration |

---

## License

MIT — see [LICENSE](LICENSE)
