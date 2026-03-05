# ChronoSync Documentation

This folder contains detailed technical documentation, testing guides, and reference materials for the ChronoSync ESP32-035 Digital Photo Frame project.

---

## 📋 Production Documentation

### [FINAL_PRODUCTION_TESTS.md](FINAL_PRODUCTION_TESTS.md)
Complete production validation checklist with 9 comprehensive test procedures covering all critical systems (display, WiFi, SD card, stability, authentication, format support, orientation handling).

**When to use:** Before final deployment or after major code changes

---

## 🧪 Testing Guides

### [PRODUCTION_TEST_PLAN.md](PRODUCTION_TEST_PLAN.md)
Comprehensive 8-step testing strategy covering:
- Industrial stability features (watchdog, scheduled reboot, heap monitoring)
- Network reliability (WiFi reconnection, mDNS)
- SD card monitoring and recovery
- Thread safety validation
- Upload protection mechanisms

**When to use:** During development to validate individual features

### [QUICK_VALIDATION_GUIDE.md](QUICK_VALIDATION_GUIDE.md)
15-minute rapid validation checklist for quick smoke testing after builds.

**When to use:** After every firmware upload to catch regressions quickly

---

## 🎨 Feature Documentation

### [IMAGE_FORMAT_SUPPORT.md](IMAGE_FORMAT_SUPPORT.md)
Detailed guide on PNG/BMP auto-conversion to JPEG:
- Browser-side conversion implementation
- File extension handling
- Cropper.js integration
- Troubleshooting common upload issues

**When to use:** Reference for image upload functionality or debugging format issues

### [PNG_BMP_UPLOAD_SUMMARY.md](PNG_BMP_UPLOAD_SUMMARY.md)
Quick reference for PNG/BMP support features and verification steps.

**When to use:** Quick lookup for format support capabilities

---

## 🔒 Security & Stability

### [THREAD_SAFETY_SD_MONITORING.md](THREAD_SAFETY_SD_MONITORING.md)
Technical documentation for:
- FreeRTOS mutex implementation (race condition prevention)
- SD card removal detection and graceful error handling
- Edge case scenarios and recovery mechanisms

**When to use:** Understanding thread safety architecture or debugging SD card issues

---

## 🛠️ Hardware & Display

### [DISPLAY_FIX_README.md](DISPLAY_FIX_README.md)
Display configuration reference for ESP32-035 with ST7796 driver.

**When to use:** Display troubleshooting or hardware setup reference

---

## 📂 Important Files in Main Folder

The following critical files remain in the root directory:

### `PRODUCTION_READY_SUMMARY.md`
**Executive summary** of the entire project:
- All implemented features
- Memory status (RAM/Flash usage)
- Known limitations
- Production sign-off checklist

**Audience:** Project managers, stakeholders, decision makers

### `SECURITY_IMPLEMENTATION_GUIDE.md`
**Critical security implementation guide** for mass production:
- 3 recommended security measures (symbol stripping, device binding, OTA signature)
- Step-by-step implementation with code snippets
- 40-minute implementation timeline
- Testing procedures and troubleshooting

**Audience:** Developers implementing security before online sales

**⚠️ MUST READ before mass production!**

---

## 📁 Project Structure

```
ChronoSync V0 - ESP32-035 3_5in IPS/
├── PRODUCTION_READY_SUMMARY.md       ← Executive summary (KEEP IN ROOT)
├── SECURITY_IMPLEMENTATION_GUIDE.md  ← Security guide (KEEP IN ROOT)
├── docs/                             ← Detailed documentation
│   ├── README.md                     ← This file
│   ├── FINAL_PRODUCTION_TESTS.md
│   ├── PRODUCTION_TEST_PLAN.md
│   ├── QUICK_VALIDATION_GUIDE.md
│   ├── IMAGE_FORMAT_SUPPORT.md
│   ├── PNG_BMP_UPLOAD_SUMMARY.md
│   ├── THREAD_SAFETY_SD_MONITORING.md
│   └── DISPLAY_FIX_README.md
├── src/                              ← Source code
│   ├── main.cpp
│   ├── web_index.h
│   ├── lv_conf.h
│   └── sd_loader.h
├── backups/                          ← Production backups
│   └── production_ready_20260103_000547/
├── platformio.ini                    ← Build configuration
└── tools/                            ← Build scripts
```

---

## 🚀 Quick Navigation

**First time setup?** → Start with root `PRODUCTION_READY_SUMMARY.md`

**Running tests?** → Use `QUICK_VALIDATION_GUIDE.md` (15 min) or `FINAL_PRODUCTION_TESTS.md` (comprehensive)

**Debugging features?** → Check specific docs:
- Image uploads → `IMAGE_FORMAT_SUPPORT.md`
- SD card issues → `THREAD_SAFETY_SD_MONITORING.md`
- Display problems → `DISPLAY_FIX_README.md`

**Preparing for production?** → Read `SECURITY_IMPLEMENTATION_GUIDE.md` (CRITICAL!)

---

**Last updated:** January 3, 2026  
**Project status:** Production-ready, security implementation deferred until casing/battery finalized
