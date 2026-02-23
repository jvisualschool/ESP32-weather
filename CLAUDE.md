# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32 weather display app targeting a QSPI-connected AXS15231B LCD (320×480). Shows Korean city weather from the OpenWeatherMap API using an LVGL-based UI.

## Build & Flash

This is an **Arduino sketch** (`.ino`). Build and flash via Arduino IDE or `arduino-cli`:

```bash
# Select board: "ESP32S3 Dev Module" or equivalent
arduino-cli compile --fqbn esp32:esp32:esp32s3 ESP32_STOCK.ino
arduino-cli upload -p /dev/cu.usbserial-* --fqbn esp32:esp32:esp32s3 ESP32_STOCK.ino
arduino-cli monitor -p /dev/cu.usbserial-* --config baudrate=115200
```

Required libraries (install via Arduino Library Manager):
- `lvgl` (LVGL graphics)
- `ArduinoJson`
- `esp32` board package (Espressif)

## Configuration

`config.h` holds real credentials and is **not committed**. Copy `config.example.h` to `config.h` and fill in:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* weather_api_key = "YOUR_OPENWEATHERMAP_KEY";
```

## Architecture

### File Roles

| File | Purpose |
|---|---|
| `ESP32_STOCK.ino` | Main sketch: WiFi setup, weather fetch loop, LVGL UI construction |
| `config.h` | Credentials (gitignored) |
| `weather_cities.h` | `k_cities[]` array of Korean cities with lat/lon; `city_count` constant |
| `esp_bsp.h` / `esp_bsp.c` | Board Support Package: initializes QSPI SPI bus, AXS15231B LCD panel, backlight PWM (LEDC), I2C touch, and LVGL port |
| `display.h` | LCD resolution constants (`EXAMPLE_LCD_QSPI_H_RES=320`, `EXAMPLE_LCD_QSPI_V_RES=480`) and raw display API |
| `lv_port.h` / `lv_port.c` | LVGL porting layer — task/timer setup, display/touch registration |
| `esp_lcd_axs15231b.*` | Vendor LCD panel driver |
| `esp_lcd_touch.*` | Touch controller driver |
| `lv_conf.h` | LVGL compile-time configuration |
| `bsp_err_check.h` | BSP error-check macros (`BSP_ERROR_CHECK_RETURN_ERR`, etc.) |

### UI Structure

The UI is a 2-tile `lv_tileview`:
- **Tile (0,0)** — City list: scrollable list of cities from `k_cities[]`; tapping fetches weather and navigates to tile (1,0)
- **Tile (1,0)** — Weather detail: temperature, condition, humidity/wind; background color shifts red/blue/deep-blue based on temperature (>25°C / >10°C / cold)

### Key Runtime Details

- **LVGL task stack**: 16 KB (increased from the 4 KB default in `ESP_LVGL_PORT_INIT_CONFIG`)
- **Display buffer**: `EXAMPLE_LCD_QSPI_H_RES * 80` pixels (80 scan lines), stored in PSRAM (`buff_spiram = true`)
- **Rotation**: `LV_DISP_ROT_90` — physical 320×480 is presented as 480×320 landscape
- **Thread safety**: All LVGL calls must be wrapped in `bsp_display_lock(0)` / `bsp_display_unlock()`
- **Weather refresh**: Every 10 minutes (`weather_interval = 600000 ms`) via `loop()`
- **HTTPS**: `WiFiClientSecure` with `setInsecure()` (no certificate validation)

### Pin Assignments (from `esp_bsp.h`)

| Signal | GPIO |
|---|---|
| QSPI CS | 45 |
| QSPI CLK | 47 |
| QSPI D0–D3 | 21, 48, 40, 39 |
| DC | 8 |
| TE (tear) | 38 |
| Backlight | 1 |
| Touch SDA | 4 |
| Touch SCL | 8 |
