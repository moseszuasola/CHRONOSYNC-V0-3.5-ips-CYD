---
description: Build ChronoSync firmware and flash it to the ESP32 via USB-C
---

# Build & Flash Workflow

## Prerequisites
- ESP32 device connected via USB-C cable
- PlatformIO installed (`pip install platformio` or via VSCode extension)
- Correct USB driver installed (CP210x or CH340)

## Steps

1. Navigate to the project directory
```bash
cd "/Volumes/4tb Mac Hdd/AI Projects/Chronosync/ChronoSync V0 - ESP32-035 3_5in IPS - JAN 2026 Update"
```

2. Find the connected ESP32 USB port
```bash
ls /dev/cu.usb*
```
Look for something like `/dev/cu.usbserial-10` or `/dev/cu.SLAB_USBtoUART`.

3. (Optional) Clean previous build artifacts if changing build flags
```bash
pio run --target clean
```

4. Build the firmware
```bash
pio run
```
Expected: `[SUCCESS]` with `.pio/build/esp32dev/firmware.bin` generated.

5. Upload firmware to device
```bash
pio run --target upload
```
If the upload fails to detect the port, specify it explicitly:
```bash
pio run --target upload --upload-port /dev/cu.usbserial-XXXX
```
> **If upload hangs at "Connecting..."** — hold the **BOOT** button on the ESP32, then release after upload begins.

6. Open serial monitor to verify boot
```bash
pio device monitor --baud 115200
```
You should see boot logs including Wi-Fi connection status and display initialization.

7. Press `Ctrl+C` to exit the serial monitor when done.

## Expected Serial Output on Boot
```
ChronoSync booting...
Display initialized (ST7796 320x480)
Connecting to Wi-Fi...
Wi-Fi connected. IP: 192.168.x.x
Web server started on port 80
```
