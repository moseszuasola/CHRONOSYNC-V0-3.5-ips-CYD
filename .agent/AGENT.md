# ChronoSync — Antigravity Agent

## Project Overview
**ChronoSync** is an ESP32-based smart clock/photo-frame device built on the **ESP32-3248S035** (3.5" IPS CYD) board.  
It features an ST7796 display, XPT2046 capacitive touch, Wi-Fi connectivity, MJPEG playback, SD card support, and a full web-based admin UI served directly from the ESP32.

> **Project Root:**  
> `/Volumes/4tb Mac Hdd/AI Projects/Chronosync/ChronoSync V0 - ESP32-035 3_5in IPS - JAN 2026 Update/`

---

## Hardware Specification

| Component        | Details                                      |
|-----------------|----------------------------------------------|
| **MCU**          | ESP32 (ESP32-3248S035 "Cheap Yellow Display") |
| **Display**      | 3.5" IPS ST7796 — 320×480 px                 |
| **Touch**        | XPT2046 (SPI)                                |
| **Storage**      | MicroSD card (SPI)                           |
| **Connectivity** | Wi-Fi 802.11 b/g/n                           |
| **Framework**    | Arduino via PlatformIO                       |
| **Upload Speed** | 115200 baud                                  |
| **Monitor Speed**| 115200 baud                                  |

---

## Pin Configuration

| GPIO | Function        |
|------|-----------------|
| 12   | TFT_MISO        |
| 13   | TFT_MOSI        |
| 14   | TFT_SCLK        |
| 15   | TFT_CS          |
| 2    | TFT_DC          |
| 27   | TFT_BL (Backlight) |
| 33   | TOUCH_CS        |

---

## Project Structure

```
ChronoSync V0 - ESP32-035 3_5in IPS - JAN 2026 Update/
├── src/
│   ├── main.cpp          # Main firmware — clock, UI, web server, MJPEG
│   ├── web_index.h       # Embedded web UI (gzip compressed HTML/JS/CSS)
│   ├── wifi_qr_data.h    # QR code data for Wi-Fi provisioning
│   ├── sd_loader.h       # SD card loading utilities
│   └── lv_conf.h         # LVGL configuration (if used)
├── include/              # Shared header files
├── lib/
│   ├── TFT_eSPI/         # Display driver (local copy)
│   ├── lvgl/             # LVGL UI library (if used)
│   └── fonts/            # Custom bitmap/GFX fonts
├── www/
│   └── index.html        # Web UI source (built → web_index.h via tools)
├── tools/
│   ├── generate_web_index.py       # Compresses www/index.html → web_index.h
│   ├── convert_gif_ffmpeg.py       # GIF → MJPEG conversion
│   ├── gif_converter.py            # Alternate GIF → frame converter
│   ├── generate_bold_clock_digits.py # Generate custom digit bitmaps
│   ├── generate_montserrat_digits.py # Montserrat font digit generator
│   ├── generate_gfx_font.py        # GFX font header generator
│   ├── generate_splash_text.py     # Splash screen text asset generator
│   └── generate_wifi_qr.py         # Generate Wi-Fi QR code header
├── docs/                 # Technical documentation
├── backups/              # Firmware backup snapshots
├── platformio.ini        # PlatformIO config (board, libs, build flags)
├── clear_wifi.py         # Utility: erase saved Wi-Fi credentials
└── .agent/               # This Antigravity agent config
    ├── AGENT.md          # This file
    └── workflows/        # Reusable workflow definitions
```

---

## Key Libraries & Dependencies

| Library            | Source              | Purpose                         |
|--------------------|---------------------|---------------------------------|
| **TFT_eSPI**       | bodmer (local+pio)  | ST7796 display driver           |
| **TJpg_Decoder**   | bodmer              | JPEG/MJPEG frame decoding       |
| **ArduinoJson**    | bblanchon           | JSON parsing for web API calls  |

---

## Build Flags Summary (platformio.ini)

- `ST7796_DRIVER=1` — ST7796 display chip
- `TFT_WIDTH=320`, `TFT_HEIGHT=480`
- `TFT_RGB_ORDER=TFT_BGR`
- `SPI_FREQUENCY=40000000` (40 MHz display SPI)
- `SPI_READ_FREQUENCY=20000000`
- `TOUCH_CS=33`
- Fonts: GLCD, Font2, Font4, GFXFF, SMOOTH_FONT

---

## Quick Reference Commands

### Find connected ESP32 port
```bash
ls /dev/cu.usb*
```

### Build firmware
```bash
cd "/Volumes/4tb Mac Hdd/AI Projects/Chronosync/ChronoSync V0 - ESP32-035 3_5in IPS - JAN 2026 Update"
pio run
```

### Upload firmware
```bash
pio run --target upload
```

### Upload to specific port
```bash
pio run --target upload --upload-port /dev/cu.usbserial-XXXX
```

### Open serial monitor
```bash
pio device monitor --baud 115200
```

### Clean build
```bash
pio run --target clean
```

### Rebuild web UI header from www/index.html
```bash
cd "/Volumes/4tb Mac Hdd/AI Projects/Chronosync/ChronoSync V0 - ESP32-035 3_5in IPS - JAN 2026 Update"
python3 tools/generate_web_index.py
```

### Convert GIF to MJPEG
```bash
python3 tools/convert_gif_ffmpeg.py <input.gif> <output.mjpeg>
```

### Clear saved Wi-Fi credentials on device
```bash
python3 clear_wifi.py
```

---

## Agent Responsibilities

This agent handles all development on ChronoSync, including:

- ✅ **Firmware** — C/C++ Arduino framework development in `src/main.cpp`
- ✅ **Display** — ST7796 TFT via TFT_eSPI, touch via XPT2046
- ✅ **MJPEG** — Animated image playback from SD card or web upload
- ✅ **Web UI** — Admin panel in `www/index.html`, compiled to `web_index.h`
- ✅ **Wi-Fi** — Station + AP mode, OTA updates, captive portal
- ✅ **SD Card** — Image/gif/font loading from MicroSD
- ✅ **Build & Upload** — PlatformIO build, clean, flash via USB-C
- ✅ **Tools** — Python tool scripts for asset generation
- ✅ **Debugging** — Serial monitor, stack traces, crash analysis
- ✅ **Security** — Wi-Fi credential management, OTA auth

---

## Important Notes

- Always check the USB port before uploading: `ls /dev/cu.usb*`
- If upload fails → hold **BOOT** button on the ESP32 while starting upload
- SPI frequency set to **40 MHz** for display stability (reduce to 27 MHz if display glitches)
- Web UI changes in `www/index.html` must be compiled to `web_index.h` before building: `python3 tools/generate_web_index.py`
- `main.cpp` is large (~120KB) — it contains the full firmware: clock rendering, MJPEG player, web server, Wi-Fi manager, settings, and OTA handler
- `backups/` folder contains timestamped snapshots — create one before major changes

---

## Workflows

See `.agent/workflows/` for detailed step-by-step procedures:

| Workflow File              | Purpose                                     |
|---------------------------|---------------------------------------------|
| `build-and-flash.md`       | Full build → upload → monitor cycle         |
| `update-web-ui.md`         | Edit web UI and rebuild the header file      |
| `add-mjpeg-animation.md`   | Convert and add a new MJPEG animation        |
| `backup-firmware.md`       | Create a timestamped backup of firmware      |
| `troubleshoot-upload.md`   | Debug and fix firmware upload failures       |
