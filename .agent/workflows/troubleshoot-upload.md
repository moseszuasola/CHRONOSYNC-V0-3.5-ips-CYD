---
description: Diagnose and fix firmware upload failures when flashing ChronoSync
---

# Troubleshoot Upload Workflow

## Step 1 — Confirm the device is connected

```bash
ls /dev/cu.usb*
```
If nothing appears → try a different USB cable (data cable, not charge-only) or a different port.

## Step 2 — Confirm PlatformIO sees the port

```bash
pio device list
```
Look for a serial device matching the ESP32 (CP210x, CH340, or FTDI).

## Step 3 — Try forcing the port manually

```bash
pio run --target upload --upload-port /dev/cu.usbserial-XXXX
```
Replace `XXXX` with the actual port name from step 1.

## Step 4 — Enter bootloader mode manually

If upload hangs at `Connecting......____`:
1. Hold the **BOOT** (GPIO0) button on the ESP32
2. While holding BOOT, press and release **RESET/EN**
3. Release BOOT after 1–2 seconds
4. Re-run the upload command immediately

## Step 5 — Reduce upload speed

In `platformio.ini`, temporarily reduce the upload speed:
```ini
upload_speed = 460800
```
Or try `115200` for the most reliable connection.

## Step 6 — Check for permission issues (macOS)

```bash
ls -la /dev/cu.usbserial-*
```
If your user doesn't have read/write access:
```bash
sudo chmod 666 /dev/cu.usbserial-XXXX
```

## Step 7 — Reinstall USB serial driver

For CP2102 chips (common on CYD boards):
- Download and reinstall: [CP210x driver from Silicon Labs](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)

For CH340 chips:
```bash
brew install --cask wch-ch34x-usb-serial-driver
```
Restart Mac after installing.

## Step 8 — Erase flash and retry

If the device is in a bad state:
```bash
pip install esptool
esptool.py --port /dev/cu.usbserial-XXXX erase_flash
```
Then re-upload normally:
```bash
pio run --target upload
```

## Common Error Messages

| Error | Cause | Fix |
|-------|-------|-----|
| `Connecting......____` | Not in bootloader mode | Hold BOOT button |
| `No such file /dev/cu.*` | Device not detected | Check cable/driver |
| `Permission denied` | Port permissions | `chmod 666 /dev/cu.*` |
| `A fatal error occurred: Failed to connect` | Wrong port or bad cable | Use `--upload-port` flag |
| `MD5 mismatch` | Upload corruption | Lower `upload_speed` |
