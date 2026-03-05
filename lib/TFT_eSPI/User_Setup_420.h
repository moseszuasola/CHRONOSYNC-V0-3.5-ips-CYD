// =======================================================================================
// === User Setup for ESP32-3248S035 (3.5" IPS CYD - ST7796 480x320) ===
// =======================================================================================
#define USER_SETUP_LOADED 1

// --- Define the Driver and Resolution ---
// 3.5" CYD uses ST7796 driver chip
#define ST7796_DRIVER

// The ST7796 is 320x480 (portrait native)
#define TFT_WIDTH  320
#define TFT_HEIGHT 480

// --- Define the SPI Pins (VSPI Bus for TFT) ---
#define TFT_MOSI 13
#define TFT_MISO 12
#define TFT_SCLK 14

// --- Control Pins (3.5" CYD Pinout) ---
#define TFT_CS   15  // Chip Select
#define TFT_DC    2  // Data/Command pin
#define TFT_RST  -1  // Reset pin (use -1 if connected to ESP32 EN)
#define TFT_BL   27  // Backlight pin

// --- Performance and Fixes ---
#define SPI_FREQUENCY  65000000 // 65 MHz SPI speed (original stable)
#define TFT_INVERSION_ON       // Required for correct colors
#define TFT_RGB_ORDER TFT_BGR  // Correct color order for ST7796

// --- Font/Graphics Features ---
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// --- Touch Screen Configuration (XPT2046 Resistive Touch) ---
#define TOUCH_CS 33     // Touch screen chip select pin