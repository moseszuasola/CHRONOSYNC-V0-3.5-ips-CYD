#!/usr/bin/env python3
"""
Generate a WiFi QR code and export it as C++ code for embedding in ESP32 firmware.
"""

import qrcode
import sys

def generate_wifi_qr_code(ssid, password="", security="nopass"):
    """
    Generate a WiFi QR code.
    
    Args:
        ssid: WiFi network name
        password: WiFi password (empty for open network)
        security: Security type (nopass, WPA, WEP)
    """
    # Create WiFi connection string
    wifi_string = f"WIFI:T:{security};S:{ssid};P:{password};H:false;;"
    
    # Generate QR code
    qr = qrcode.QRCode(
        version=1,  # Version 1 = 21x21 modules
        error_correction=qrcode.constants.ERROR_CORRECT_L,  # Low error correction for simplicity
        box_size=1,
        border=0,  # No border, we'll add it manually
    )
    qr.add_data(wifi_string)
    qr.make(fit=True)
    
    # Get the matrix
    matrix = qr.get_matrix()
    size = len(matrix)
    
    print(f"// Generated WiFi QR Code for SSID: {ssid}")
    print(f"// QR Code Size: {size}x{size}")
    print(f"// WiFi String: {wifi_string}")
    print()
    print(f"#define QR_SIZE {size}")
    print()
    print("// QR code matrix: true = black module, false = white module")
    print(f"const bool qrCodeMatrix[{size}][{size}] = {{")
    
    for i, row in enumerate(matrix):
        print("  {", end="")
        for j, cell in enumerate(row):
            print("1" if cell else "0", end="")
            if j < len(row) - 1:
                print(",", end="")
        print("}" + ("," if i < len(matrix) - 1 else ""))
    
    print("};")
    print()
    print(f"// Total modules: {size}x{size} = {size*size}")
    
    return matrix, size

if __name__ == "__main__":
    # Generate QR code for ChronoSync-Setup (open network)
    ssid = "ChronoSync-Setup"
    password = ""
    security = "nopass"
    
    if len(sys.argv) > 1:
        ssid = sys.argv[1]
    if len(sys.argv) > 2:
        password = sys.argv[2]
        security = "WPA" if password else "nopass"
    
    generate_wifi_qr_code(ssid, password, security)
