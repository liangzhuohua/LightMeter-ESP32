# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-S3 AMOLED smart device with LVGL UI — a photography exposure calculator with weather display, WiFi connectivity, OTA updates, and deep sleep support.

## Version Management

After every code change, increment the patch version in `CMakeLists.txt` line 8:
```
project(ESP32S3-IDF_AMOLED_LVGL-V8 VERSION X.Y.Z)
```
Increment Z for bug fixes and small changes, Y for new features, X for major reworks.

## Documentation

After every code change, update `README.md` to reflect the new behavior or feature. If the change affects architecture, hardware behavior, or user-facing functionality, the README must stay in sync. This includes: feature additions/removals, behavior changes (like keypress duration), API or dependency changes, and hardware configuration changes.

## Build Commands

Uses ESP-IDF 5.2.0. Target: `esp32s3` with Octal PSRAM.

```bash
# Build
idf.py build

# Flash to device (serial port /dev/ttyACM0)
idf.py -p /dev/ttyACM0 flash

# Flash and monitor
idf.py -p /dev/ttyACM0 flash monitor

# Monitor only
idf.py -p /dev/ttyACM0 monitor

# Menuconfig
idf.py menuconfig

# Clean build
idf.py clean

# Set target (first time setup)
idf.py set-target esp32s3
```

Shortcut script: `./idf.sh B` (build+flash), `./idf.sh BM` (build+flash+monitor), `./idf.sh M` (menuconfig), `./idf.sh C <name> <path>` (create component), `./idf.sh H` (help).

## External Dependencies (idf_component.yml)

- `lvgl/lvgl` ^8 — LVGL graphics library
- `espressif/esp_lcd_touch` ^1.1.2 — touch controller driver
- `esp-idf-lib/veml7700` ^1.0.7 — VEML7700 ambient light sensor driver

## Key sdkconfig.defaults Settings

- Flash: QIO mode, 16MB, custom partition table
- PSRAM: Octal, 80MHz, instruction fetch + rodata in PSRAM
- CPU: 240MHz, FreeRTOS 1000Hz tick
- mbedTLS: dynamic buffers + peer cert in PSRAM
- LVGL: color 16-bit swap, custom memory allocator, perf monitor enabled
- Optimization: `-O2` (CONFIG_COMPILER_OPTIMIZATION_PERF)

## Architecture

### Layer Structure

```
main/
├── main.c                  # Entry point — I2C, VEML7700, NVS, OLED, LVGL init
├── app_controller.c/h      # Central orchestrator — all tasks, queues, semaphores
├── app_ui.c/h              # LVGL UI implementation (large file — main UI)
├── app_ui_*_port.c/h       # UI port layers (bridge LVGL callbacks to business logic)
├── app_exposure_calc.c/h   # Photography exposure calculation engine
├── app_location.c/h        # Location via cellocation API (WiFi AP BSSID scan)
├── app_weather.c/h         # Weather via QWeather API v7 (now + 3d forecast + moon phase)
├── app_time.c/h            # SNTP time sync + RTC persistence
├── app_http_requests.c/h   # HTTP client helpers (async + sync, Gzip auto-decompress)
├── app_nvs_storage.c/h     # NVS persistence (WiFi, location, weather, sync timestamps, camera/lens cards, UI state)
├── app_battery.c/h         # Unified battery API (orchestrates MAX17055 + TP4056)
├── hw_oled.c/h             # QSPI AMOLED display driver (460x460)
├── hw_wifi.c/h             # WiFi init, scan, connect, state machine
├── hw_veml7700.c/h         # VEML7700 ambient light sensor (5-level adaptive range, AN 84323 polynomial correction)
├── hw_max17055.c/h         # MAX17055 battery fuel gauge (SOC, voltage)
├── hw_tp4056.c/h           # TP4056 charge detector (CHRG/STDBY pins)
├── hw_ota.c/h              # OTA firmware update (AP hotspot + web upload page)
├── hw_wakeup_key.c/h       # GPIO9 wake-up key (short press to sleep/wake)
├── hw_nvs.c/h              # Low-level NVS helpers (WiFi config CRUD)
├── bsp_i2c_init.c/h        # I2C bus initialization
├── img_*.c                 # LVGL image assets (embedded C arrays)
├── clock_icon.c            # Analog clock face icon
├── qweather_icons.c        # Weather condition icons
└── SourceHanSansCN_Regular.c  # Chinese font (思源黑体)
```

### Application Startup Flow (app_controller_init)

1. Create queues/semaphores (lux_value_queue, wifi_operation_queue, location_Sem, time_sync_Sem, weather_Sem)
2. Register WiFi state callback
3. Load all NVS data (WiFi config, cached location, cached weather)
4. Restore time from RTC memory (survives deep sleep)
5. Display cached location, weather on UI
6. If WiFi was enabled at last shutdown: init WiFi, scan, auto-connect to saved SSID (up to 3 retries)
7. Create LVGL periodic sync timer (every 30 min) — checks if time/weather thresholds exceeded
8. Create FreeRTOS tasks (see Tasks section below)

### Serial Request Chain

WiFi connection triggers a serial chain (location → time → weather) via semaphores:

```
WiFi Connected
  → give(location_Sem)
    → task_get_location: cellocation API (WiFi AP BSSID scan)
    → On result: init SNTP, give(time_sync_Sem)
      → task_time_sync_and_update: SNTP sync, update RTC
      → On success: give(weather_Sem)
        → task_weather_update: QWeather API (now + 3d forecast + moon phase)
```

Each step retries up to 3 times on failure. Cached data is displayed immediately on boot so the UI is never blank.

### Tasks (FreeRTOS)

| Task | Stack | Core | Purpose |
|------|-------|------|---------|
| task_get_lux_value | 4KB | any | Read VEML7700 ALS every 1s, push to queue |
| task_calc_exposure | 4KB | any | Consume lux, run exposure calc, update UI rollers |
| task_wifi_operation | 4KB | 0 | Process WiFi command queue (scan/connect/disconnect) |
| task_get_location | 8KB | 0 | Wait on location_Sem, call cellocation API |
| task_time_sync | 6KB | 0 | Wait on time_sync_Sem, SNTP sync, update RTC |
| task_time_update | 2KB | any | Update UI clock display every minute |
| task_weather | 16KB | 0 | Wait on weather_Sem, call QWeather API |
| task_battery | 3KB | any | Read battery SOC/voltage/status every 3s |
| task_power_manage | 4KB | any | Wait on sleep_sem, trigger deep sleep on key press |

### Deep Sleep Sequence (app_controller_enter_deep_sleep)

1. Save current time to RTC memory
2. OLED enter sleep mode → touch enter sleep mode
3. Shut down VEML7700 light sensor
4. Put MAX17055/TP4056 to sleep
5. Disconnect WiFi, stop and deinit WiFi driver
6. Configure GPIO9 as wake-up source
7. Release OLED, touch, I2C pins (prevent leakage)
8. `esp_deep_sleep_start()`

On wake: `main.c` re-initializes everything. `app_controller_init` restores time from RTC.

### Key Design Patterns

1. **Controller Pattern**: `app_controller.c` is the central orchestrator — all inter-task communication goes through its queues/semaphores.

2. **Port Layer**: `app_ui_*_port.c` files bridge LVGL UI to business logic. UI callbacks call port functions, which call app_controller public API.

3. **Hardware Abstraction**: `hw_*.c` files encapsulate hardware details. Each has a corresponding header with public API.

4. **NVS Persistence**: `app_nvs_storage.c` handles all persistent storage with typed structures (`camera_data_t`, `lens_data_t`, `wifi_data_t`, `weather_data_t`, `location_data_t`, `sync_timestamp_t`).

5. **LVGL Thread Safety**: All UI updates from non-LVGL tasks MUST wrap in `example_lvgl_lock(-1)` / `example_lvgl_unlock()`.

### Hardware Configuration

- Display: 460×460 QSPI AMOLED (SPI3_HOST)
- Touch: CST820 I2C touch controller
- Light Sensor: VEML7700 (ambient light for exposure calculation)
- Battery: MAX17055 fuel gauge + TP4056 charge detector (pins CHRG/STDBY)
- Wake-up Key: GPIO9 (short press to enter/exit deep sleep)

### Reference Files
- `Datasheet/` — chip datasheets (PDF)
- `hardware/` — PCB project file (.epro2)
- `docs/schematic.png` — schematic diagram

### Battery Management Architecture

Three-layer architecture:

```
┌─────────────────────────────────────────┐
│         app_battery.c/h                 │  ← Application layer (unified API)
│   app_battery_get_info()                │
│   app_battery_init() / app_battery_sleep()│
├─────────────────────────────────────────┤
│  hw_max17055.c/h    │  hw_tp4056.c/h    │  ← Hardware drivers
│  (SOC, voltage)     │  (charge status)  │
└─────────────────────────────────────────┘
```

Battery status enum: `BATTERY_STATUS_DISCHARGING`, `BATTERY_STATUS_CHARGING`, `BATTERY_STATUS_FULL`.

When TP4056 detects charge complete, it recalibrates MAX17055 SOC to 100% to resolve drift between the two detection mechanisms.

### Exposure Calculator Core

`app_exposure_calc.c` implements photography exposure logic:
- Supports 1/3 EV and 1/2 EV step increments
- Modes: Manual, Auto, Landscape, Portrait
- Uses standard shutter/aperture tables
- `exposure_auto()` calculates aperture/shutter based on lux, ISO, and mode

### OTA Updates

`hw_ota.c` implements web-based OTA: the ESP32 creates a WiFi AP hotspot (`ESP32S3_OTA`), starts an HTTP server at `192.168.4.1`, and serves an HTML upload page. The browser uploads a `.bin` firmware file via POST to `/upload`. `app_controller_request_ota()` registers a progress callback and starts the OTA task. Progress is forwarded to `app_ui_ota_port.c` for UI display. `app_controller_cancel_ota()` stops the AP and restores STA mode.

### VEML7700 Adaptive Range

`hw_veml7700.c` implements a 5-level auto-switching strategy following Vishay AN 84323, starting from the lowest sensitivity (GAIN_1/8, IT=100ms):

| Level | Gain | Integration | Resolution | Range |
|-------|------|-------------|------------|-------|
| WEAK (L0) | GAIN_2 | 800ms | 0.0042 lx/cnt | ~0–84 lx |
| MODERATE (L1) | GAIN_1/4 | 100ms | 0.2688 lx/cnt | ~27–17k lx |
| BRIGHT (L2) | GAIN_1/8 | 100ms | 0.5376 lx/cnt | ~54–35k lx (default start) |
| VBRIGHT (L3) | GAIN_1/8 | 50ms | 1.0752 lx/cnt | ~108–70k lx |
| EXTREME (L4) | GAIN_1/8 | 25ms | 2.1504 lx/cnt | ~215–141k lx |

Switching is based on raw counts (not lux), matching the AN flowchart: ≤100 counts → increase sensitivity, >10000 counts (at GAIN_1/8) → shorten integration time. A raw saturation safety net (55000 counts) forces immediate upshift. Non-linear correction uses the AN 4th-order polynomial (a·x⁴ + b·x³ + c·x² + d·x) for L1–L4; L0 is linear. A transmission factor compensates for cover glass attenuation.

### T9 Keyboard

`app_ui.c` contains a full T9 (multi-tap) predictive text keyboard for camera/lens name input:
- 12-key layout with Chinese/English character support
- Multi-tap cycling with 800ms confirmation timeout
- Toggle between text mode, numeric/symbol mode, and uppercase
- Associated textarea tracking via `t9_kb_set_textarea()` / `t9_kb_get_textarea()`

## Partition Table (partitions.csv)

| Name | Type | Size |
|------|------|------|
| nvs | data, nvs | 24KB |
| phy_init | data, phy | 4KB |
| ota_0 | app, ota_0 | 3MB |
| ota_1 | app, ota_1 | 3MB |
| ota_data | data, ota | 8KB |

## Calibration

`docs/calibrate_veml7700.py` — Python script for VEML7700 lux calibration. Updated for AN 84323 polynomial correction approach.

## Commit Style

Chinese commit messages, focused on what changed and why. No conventional commit format enforced.
