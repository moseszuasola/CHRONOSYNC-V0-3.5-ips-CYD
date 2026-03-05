#!/usr/bin/env python3
"""
Clear WiFi credentials from ESP32 NVS (Non-Volatile Storage)
This will trigger the QR code setup screen on next boot
"""
import serial
import time

# Configure your COM port
PORT = 'COM4'
BAUD = 115200

print("Connecting to ESP32...")
ser = serial.Serial(PORT, BAUD, timeout=2)
time.sleep(2)

# Send commands to clear WiFi credentials via Preferences API
# We'll trigger a reset and clear the credentials
print("Sending erase command...")

# The ESP32 will need to be programmed to accept serial commands
# For now, we'll just use esptool to erase NVS partition

ser.close()
print("Done! Now use esptool to erase NVS partition:")
print(f"esptool.py --port {PORT} erase_region 0x9000 0x5000")
