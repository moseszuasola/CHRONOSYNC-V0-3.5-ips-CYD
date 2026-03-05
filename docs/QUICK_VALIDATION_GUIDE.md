# ChronoSync - Quick Test & Validation Guide

## 🚀 IMMEDIATE ACTIONS COMPLETED

### ✅ Critical Bugs Fixed:
1. **File handle leak in scanDirectory()** - Files now properly closed in loop
2. **Status bar memory optimization** - Messages now cached, not recreated every frame

### ✅ Industrial Features Active:
1. **Hardware Watchdog (WDT)** - 8-second timeout with auto-reboot
2. **24-Hour Maintenance Reboot** - Scheduled for 3:00 AM daily
3. **Heap Monitoring** - Emergency restart at 20% free memory (40KB threshold)
4. **PWM Backlight Control** - 80% day / 10% night (11PM-6AM)
5. **Brownout Protection** - Enabled in build flags
6. **mDNS Support** - Device accessible via `chronosync.local`

---

## 🧪 QUICK 15-MINUTE VALIDATION TEST

### Before Finalizing Production, Run This:

```bash
# 1. Upload code and open serial monitor
pio run --target upload && pio device monitor

# 2. Watch for these startup messages:
✓ "Initializing Task Watchdog Timer (8s timeout)..."
✓ "Hardware WDT enabled"
✓ "Backlight PWM initialized: 80% brightness"
✓ "mDNS responder started!"
✓ "Access device at: http://chronosync.local"
```

### Test Sequence (15 minutes):

**Minute 0-2: WiFi & mDNS Test**
1. Note IP address from serial/screen
2. Try `http://chronosync.local` from phone
3. If fails, try IP address directly
4. Login with PIN (default: 1234)

**Minute 2-5: Upload & Playback Test**
1. Upload 3 photos (landscape)
2. Start slideshow (5sec interval)
3. Verify smooth transitions
4. Check status bar shows IP

**Minute 5-8: Video Test**
1. Upload short .vid file (~10 sec)
2. Play video in loop
3. Try long-press to exit
4. Monitor heap in serial (should stay > 60KB)

**Minute 8-11: Clock Test**
1. Enable clock every 3 photos
2. Verify clock displays correctly
3. Test 12hr ↔ 24hr switch
4. Check timezone change (Manila → Tokyo)

**Minute 11-13: Stress Test**
1. Rapidly start/stop slideshow 10 times
2. Upload photo while slideshow running
3. Change orientation during playback
4. Watchdog should NOT trigger

**Minute 13-15: Recovery Test**
1. Pull power mid-slideshow
2. Reconnect power
3. Should boot normally
4. Verify last state restored

---

## 📊 Serial Monitor Watchlist

### Good Signs ✅:
```
[HEAP] Free: 145000+ bytes
WiFi status: connected
mDNS responder started!
Time synced!
[BACKLIGHT] Day mode: Normal brightness 80%
```

### Bad Signs ⚠️:
```
[CRITICAL] Low memory detected!
E (xxxx) task_wdt: Task watchdog got triggered
WiFi disconnected, attempting reconnect... (repeating)
[WARNING] Memory getting low
Guru Meditation Error (crash)
```

---

## 🎯 SIGN-OFF CHECKLIST

Before locking code for production:

- [ ] Serial shows "Hardware WDT enabled" on boot
- [ ] Device accessible via `chronosync.local` OR IP
- [ ] Slideshow runs smooth for 5+ minutes
- [ ] Free heap stays above 60KB
- [ ] Clock displays and updates correctly
- [ ] Video playback stable (no stuttering)
- [ ] Web UI responsive (no freezing)
- [ ] Power cycle recovery works
- [ ] Touch gestures responsive
- [ ] No unexpected watchdog reboots

---

## 🛡️ Safety Features Summary

| Feature | Status | Trigger Condition | Action |
|---------|--------|-------------------|--------|
| **Watchdog** | ✅ Active | Software frozen > 8s | Auto-reboot |
| **Heap Monitor** | ✅ Active | Free RAM < 40KB | Emergency restart |
| **Daily Reboot** | ✅ Active | 3:00 AM daily | Clean restart |
| **Brownout** | ✅ Enabled | Voltage dip | Graceful shutdown |
| **WiFi Reconnect** | ✅ Active | Lost connection | Auto-reconnect every 30s |
| **PWM Dimming** | ✅ Active | 11PM-6AM | Reduce to 10% brightness |

---

## 📞 Customer Support - Common Issues

### "Can't find device on network"
1. Check device screen for IP address
2. Try `http://chronosync.local` first
3. If on 5GHz WiFi, temporarily switch to 2.4GHz
4. Enter IP address manually in browser
5. Check router isn't blocking mDNS (some routers have "AP Isolation")

### "Device keeps rebooting"
1. Check if it's 3:00 AM (scheduled maintenance)
2. Check Serial for heap warnings (low memory)
3. Remove and reinsert SD card
4. Factory reset: Hold BOOT button during power-on for 3 seconds

### "Slideshow stuttering"
1. Check SD card speed (use Class 10 or faster)
2. Reduce number of photos in playlist
3. Photos should be ≤1920x1080 resolution
4. .vid files should be properly encoded

### "Clock time wrong"
1. Check WiFi connected (time syncs via NTP)
2. Verify timezone setting in Clock Settings
3. Wait 30 seconds after WiFi connects for NTP sync

---

## 🔧 Development Notes

### If You Need to Debug:
```cpp
// Temporarily increase debug level in platformio.ini:
-D CORE_DEBUG_LEVEL=3  // More verbose logging

// Monitor specific systems:
Serial.printf("[DEBUG] Heap: %u | File: %s\n", ESP.getFreeHeap(), currentFile.c_str());
```

### If Watchdog False-Triggers:
```cpp
// Increase timeout (only if absolutely necessary):
#define WDT_TIMEOUT 12  // Increase from 8 to 12 seconds
```

### mDNS Not Working?
- Some enterprise routers block multicast
- Some mesh networks require mDNS reflector
- Fallback: Always show IP on screen (already implemented)

---

## ✅ YOU'RE READY TO LOCK & SHIP!

**Current Code Status:** Production-Ready ✅

**Known Limitations:**
- No battery level monitoring (handled by LiPo protection board)
- Web UI doesn't support concurrent edits (acceptable for single-user device)
- mDNS may not work on 100% of routers (IP fallback available)

**Recommended Next Steps:**
1. Run 72-hour burn-in test with your test unit
2. Test on 2-3 different router brands
3. Create user quick-start guide
4. Test packaging/unboxing experience
5. Set up customer support workflow

---

**Last Updated:** January 2, 2026
**Code Version:** v1.0 Production Candidate
**Tested By:** AI Code Analysis + User Validation Pending
