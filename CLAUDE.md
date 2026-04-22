# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-S3 AMOLED smart device with LVGL UI - a photography exposure calculator with weather display, WiFi connectivity, OTA updates, and deep sleep support.

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

Shortcut script available: `./idf.sh B` (build+flash), `./idf.sh BM` (build+flash+monitor), `./idf.sh M` (menuconfig).

## Architecture

### Layer Structure

```
main/
├── main.c              # Entry point - initializes all subsystems
├── app_controller.c    # Central orchestrator - coordinates all app logic
├── app_ui.c            # LVGL UI implementation (large file - main UI)
├── app_*_port.c        # UI port layers (bridge UI to business logic)
├── app_*.c             # Application logic (exposure calc, weather, time, location)
├── hw_*.c              # Hardware abstraction (OLED, WiFi, sensors, OTA)
├── bsp_*.c             # Board support (I2C init)
└── app_nvs_storage.c   # Non-volatile storage persistence

components/             # External/hardware drivers
├── esp_lcd_qspi_amoled/    # QSPI AMOLED display driver (460x460)
├── esp_lcd_touch_cst820/   # CST820 touch controller
├── max17055/               # Battery fuel gauge
├── veml7700/               # Ambient light sensor (for exposure calculation)
├── i2cdev/                 # I2C device helpers
└── esp_idf_lib_helpers/    # ESP-IDF library helpers
```

### Key Design Patterns

1. **Controller Pattern**: `app_controller.c` is the central coordinator using FreeRTOS queues/semaphores for inter-task communication.

2. **Port Layer**: `app_ui_*_port.c` files bridge LVGL UI to business logic - use these to access functionality from UI callbacks.

3. **Hardware Abstraction**: `hw_*.c` files encapsulate hardware details. Each has a corresponding header with public API.

4. **NVS Persistence**: `app_nvs_storage.c` handles all persistent storage with typed structures (`camera_data_t`, `lens_data_t`, `wifi_data_t`, etc.).

### FreeRTOS Resources

- `wifi_operation_queue` - WiFi commands (scan, connect, disconnect)
- `lux_value_queue` - Ambient light sensor readings
- `calc_data_queue` - Exposure calculation results
- `location_Sem`, `time_sync_Sem`, `weather_Sem` - Synchronization semaphores

### Hardware Configuration

- Display: 460x460 QSPI AMOLED (SPI3_HOST)
- Touch: CST820 I2C touch controller
- Light Sensor: VEML7700 (ambient light for exposure)
- Battery: MAX17055 fuel gauge
- Wake-up Key: GPIO9 (long press = 3s)

### Exposure Calculator Core

`app_exposure_calc.c` implements the photography exposure logic:
- Supports 1/3 EV and 1/2 EV step increments
- Modes: Manual, Auto, Landscape, Portrait
- Uses standard shutter/aperture tables
- `exposure_auto()` calculates aperture/shutter based on lux, ISO, and mode

## Partition Table

Custom partitions with OTA support:
- `ota_0`: 3MB
- `ota_1`: 3MB
- NVS: 24KB

## LVGL Configuration

- Color depth: 16-bit with swap
- Double-buffered with PSRAM
- Task priority: 7, stack: 6KB
- Use `example_lvgl_lock()` / `example_lvgl_unlock()` for thread-safe UI access
