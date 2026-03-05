# Display Configuration Fix Guide
## ESP32-3248S035 (3.5" IPS CYD) - Text & Color Rendering

This document outlines all changes made to fix text rendering and color display issues on the ESP32-3248S035 with ST7796 driver (480x320 IPS display).

---

## Problem Summary

**Original Issues:**
1. Text would not appear on screen (blank display)
2. Colors were inverted (white→black, red→cyan, etc.)
3. Red and Blue channels were swapped

**Root Causes:**
1. Display required `TFT_INVERSION_OFF` (was using default `ON`)
2. RGB channel order needed to be `TFT_BGR` instead of `TFT_RGB`
3. Incorrect text rendering approach with `drawString()` and datum settings

---

## Solution: Critical Configuration Changes

### 1. platformio.ini Settings

**Display Driver Configuration:**
```ini
build_flags = 
    -D ST7796_DRIVER=1              ; 3.5" IPS uses ST7796, not ST7789
    -D TFT_WIDTH=320
    -D TFT_HEIGHT=480
    -D TFT_INVERSION_OFF            ; CRITICAL: Fixes inverted colors
    -D TFT_RGB_ORDER=TFT_BGR        ; CRITICAL: Fixes red/blue swap
    
    ; SPI Pins for ESP32-3248S035
    -D TFT_MISO=12
    -D TFT_MOSI=13
    -D TFT_SCLK=14
    -D TFT_CS=15
    -D TFT_DC=2
    -D TFT_RST=-1
    -D TFT_BL=27                    ; Backlight pin
    -D TFT_BACKLIGHT_ON=HIGH
    
    ; SPI Speed
    -D SPI_FREQUENCY=40000000
    -D SPI_READ_FREQUENCY=20000000
    
    ; Enable Fonts
    -D LOAD_GLCD=1
    -D LOAD_FONT2=1
    -D LOAD_FONT4=1
    -D LOAD_GFXFF=1
    -D SMOOTH_FONT=1
```

**Key Points:**
- `TFT_INVERSION_OFF` is **mandatory** for correct colors on this display
- `TFT_RGB_ORDER=TFT_BGR` swaps R/B channels to fix color mapping
- Without these two settings, colors will be wrong

---

### 2. Code Changes for Text Rendering

#### A. Backlight Initialization (Critical!)
```cpp
void setup() {
    // --- BACKLIGHT MUST BE ENABLED FIRST ---
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    delay(100);  // Let backlight stabilize
    
    tft.begin();
    tft.setRotation(1);  // Landscape mode
    tft.fillScreen(TFT_BLACK);
}
```

**Why:** Display won't show anything without backlight enabled.

---

#### B. Text Rendering Method (Use setCursor + print)

**❌ AVOID this approach (causes blank text):**
```cpp
tft.setTextDatum(MC_DATUM);  // DON'T USE
tft.drawString("Text", x, y);  // DON'T USE
```

**✅ USE this approach instead:**
```cpp
void drawSystemText(const char* text, int16_t x, int16_t y, uint16_t color, uint8_t font) {
    tft.setTextColor(color, TFT_BLACK);  // IMPORTANT: Include background color
    tft.setTextSize(1);
    tft.setCursor(x, y);
    tft.print(text);  // Use print() with setCursor()
}
```

**Key Points:**
- Always use `setCursor()` + `print()` instead of `drawString()` + datum
- Always specify **both** text color and background color: `setTextColor(color, background)`
- Background color ensures pixels are properly overwritten

---

#### C. Byte Swapping Management

**For Text:**
```cpp
tft.setSwapBytes(false);  // DISABLE for text rendering
tft.setTextColor(TFT_WHITE, TFT_BLACK);
tft.setCursor(10, 10);
tft.print("Hello World");
```

**For Images (JPEG/GIF):**
```cpp
tft.setSwapBytes(true);  // ENABLE for image rendering
TJpgDec.drawSdJpg(0, 0, filename);
```

**Toggle as needed:**
```cpp
// Before drawing text
tft.setSwapBytes(false);
drawSystemText("Status", 10, 10, TFT_GREEN, 2);

// Before drawing images
tft.setSwapBytes(true);
TJpgDec.drawSdJpg(0, 0, "/photo.jpg");
```

---

### 3. Complete Splash Screen Example

```cpp
void showSplashScreen() {
    // Ensure swapBytes OFF for text
    tft.setSwapBytes(false);
    tft.fillScreen(TFT_BLACK);
    delay(100);
    
    // Title
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(3);
    tft.setCursor(120, 120);
    tft.print("CHRONOSYNC");
    
    // Subtitle
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(100, 170);
    tft.print("Photo Frame");
    
    // Status
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(180, 250);
    tft.print("Initializing...");
    
    delay(3000);
}
```

---

### 4. NTP Time Sync (Bonus Feature)

**Add to includes:**
```cpp
#include <time.h>

// NTP Configuration for Manila, Philippines (UTC+8)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600;  // UTC+8
const int daylightOffset_sec = 0;     // No DST
```

**In setup() after WiFi connects:**
```cpp
if (WiFi.status() == WL_CONNECTED) {
    // Sync time with NTP
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    // Wait for time sync
    struct tm timeinfo;
    int syncAttempts = 0;
    while(!getLocalTime(&timeinfo) && syncAttempts < 10) {
        delay(500);
        syncAttempts++;
    }
    
    if(getLocalTime(&timeinfo)) {
        Serial.println("Time synced successfully!");
    }
}
```

**Display clock:**
```cpp
tft.setSwapBytes(false);  // Disable for text
struct tm timeinfo;
if(getLocalTime(&timeinfo)) {
    char timeStr[10];
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(5);
    tft.setCursor(150, 100);
    tft.print(timeStr);
}
```

---

## Testing Procedure

### 1. Color Test
```cpp
void testColors() {
    tft.fillScreen(TFT_BLACK);
    
    tft.setTextSize(2);
    int y = 10;
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(10, y); tft.print("WHITE"); y += 30;
    
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setCursor(10, y); tft.print("RED"); y += 30;
    
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(10, y); tft.print("GREEN"); y += 30;
    
    tft.setTextColor(TFT_BLUE, TFT_BLACK);
    tft.setCursor(10, y); tft.print("BLUE"); y += 30;
    
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(10, y); tft.print("YELLOW"); y += 30;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(10, y); tft.print("CYAN"); y += 30;
    
    tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
    tft.setCursor(10, y); tft.print("MAGENTA"); y += 30;
}
```

**Expected Results:**
- WHITE should show as white
- RED should show as red (not blue/cyan)
- BLUE should show as blue (not red/yellow)
- YELLOW should show as yellow (not cyan)
- All colors should be correct, not inverted

### 2. Text Visibility Test
```cpp
void testText() {
    tft.fillScreen(TFT_BLACK);
    tft.setSwapBytes(false);
    
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setCursor(10, 10);
    tft.print("TEST");
    
    // If you see "TEST" in red, text rendering works!
}
```

---

## Applying to Other Microcontrollers

### For Other ESP32 Boards with ST7796:

1. **Update pin definitions** in `platformio.ini`:
   ```ini
   -D TFT_MISO=XX  ; Your board's pins
   -D TFT_MOSI=XX
   -D TFT_SCLK=XX
   -D TFT_CS=XX
   -D TFT_DC=XX
   -D TFT_RST=XX   ; Use -1 if no reset pin
   -D TFT_BL=XX    ; Your backlight pin
   ```

2. **Keep these settings** (they're display-specific, not board-specific):
   ```ini
   -D ST7796_DRIVER=1
   -D TFT_INVERSION_OFF
   -D TFT_RGB_ORDER=TFT_BGR
   ```

3. **Use the same text rendering approach**:
   - `setCursor()` + `print()`
   - `setTextColor(color, background)`
   - `setSwapBytes(false)` before text

### For Different Display Drivers (e.g., ST7789, ILI9341):

1. Change driver:
   ```ini
   -D ST7789_DRIVER=1  ; or ILI9341_DRIVER, etc.
   ```

2. **Test inversion setting:**
   - Try `TFT_INVERSION_OFF` first
   - If colors inverted, switch to `TFT_INVERSION_ON`

3. **Test RGB order:**
   - Try `TFT_RGB_ORDER=TFT_BGR` first
   - If red/blue swapped, switch to `TFT_RGB_ORDER=TFT_RGB`

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| **No display at all** | Enable backlight: `digitalWrite(TFT_BL, HIGH)` |
| **Text doesn't show** | Use `setCursor()` + `print()`, not `drawString()` |
| **Colors inverted** | Set `TFT_INVERSION_OFF` in platformio.ini |
| **Red/Blue swapped** | Set `TFT_RGB_ORDER=TFT_BGR` in platformio.ini |
| **Images look wrong** | Set `tft.setSwapBytes(true)` before image display |
| **Text background visible** | Use `setTextColor(color, TFT_BLACK)` with background |

---

## Summary Checklist

- [ ] `TFT_INVERSION_OFF` set in platformio.ini
- [ ] `TFT_RGB_ORDER=TFT_BGR` set in platformio.ini
- [ ] Backlight enabled: `digitalWrite(TFT_BL, HIGH)`
- [ ] Using `setCursor()` + `print()` for text
- [ ] Using `setTextColor(color, background)` with background
- [ ] `setSwapBytes(false)` before text rendering
- [ ] `setSwapBytes(true)` before image rendering
- [ ] Correct SPI pins defined for your board

---

## Files Modified

1. **platformio.ini** - Display driver and build flags
2. **src/main.cpp** - Text rendering methods, NTP sync
3. **lib/TFT_eSPI/User_Setup_420.h** - Custom display configuration (optional)

---

## Reference Configuration

**Working Configuration for ESP32-3248S035 (3.5" IPS CYD):**
- **Display:** ST7796, 480x320, IPS
- **Inversion:** OFF
- **RGB Order:** BGR
- **SPI Speed:** 40MHz
- **Backlight:** GPIO27, Active HIGH
- **Text Method:** setCursor() + print()
- **Image Swap:** true
- **Text Swap:** false

---

**Date Created:** December 7, 2025  
**Board:** ESP32-3248S035 (3.5" IPS CYD)  
**Display Driver:** ST7796  
**Resolution:** 480x320  
**Status:** ✅ Fully Working
