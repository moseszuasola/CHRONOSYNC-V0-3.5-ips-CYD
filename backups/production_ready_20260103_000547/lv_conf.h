/**
 * @file lv_conf.h
 * Configuration file for LVGL.
 * This is the "nervous system" for our graphics.
 */

#ifndef LV_CONF_H
#define LV_CONF_H

// --- 1. System Settings ---
#define LV_USE_LOG 0            // Disable LVGL's internal logging
#define LV_TICK_CUSTOM 1        // We'll tell it when to "tick"

// --- 2. Color Settings ---
#define LV_COLOR_DEPTH 16       // 16-bit color (RGB565)
#define LV_COLOR_16_SWAP 1      // Our screen needs this
#define LV_COLOR_CHROMA_KEY lv_color_hex(0x000000) // Disable "green screen" effect

// --- 3. Font Settings ---
// We will enable the built-in "Montserrat" font at various sizes
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1 // For header and website
#define LV_FONT_MONTSERRAT_24 1 // For date/day
#define LV_FONT_MONTSERRAT_36 1 // For clock time
#define LV_FONT_MONTSERRAT_48 1 // For larger bold clock
#define LV_FONT_DEFAULT &lv_font_montserrat_16 // This is our default

// --- 4. Enable the "Widgets" we will need ---
// This is like a "shopping list" for our UI.
#define LV_USE_LABEL 1          // For text (our "Hello World")
#define LV_USE_CALENDAR 1       // For our future calendar
#define LV_USE_CALENDAR_HEADER_ARROW 1

// --- 5. Enable the "Extras" we will need ---
#define LV_USE_FADE_IN 1        // For our transitions
#define LV_USE_ANIMIMG 1        // For our animated GIFs

// --- 6. Hardware Settings ---
// We need to tell LVGL how "big" our screen is.
#define LV_HOR_RES_MAX 320
#define LV_VER_RES_MAX 240

#endif