# ChronoSync Production Test Plan & Code Analysis
**Date:** January 2, 2026
**Device:** ESP32-035 3.5" IPS (No PSRAM)
**Purpose:** Pre-production validation for commercial release

---

## ✅ CODE ANALYSIS RESULTS

### **CRITICAL ISSUES FOUND & FIXED:**

#### 1. **File Handle Leak in scanDirectory()** ⚠️ CRITICAL
**Location:** Line 2467-2479
**Issue:** `File` objects not properly closed in while loop
**Impact:** Memory leak during directory scans (happens frequently)
**Status:** **NEEDS FIX**

```cpp
// CURRENT (BROKEN):
File file = root.openNextFile();
while(file){
    // ... process file
    file = root.openNextFile();  // ❌ Previous file never closed!
}

// SHOULD BE:
File file = root.openNextFile();
while(file){
    // ... process file
    file.close();  // ✅ Must close before reassignment
    file = root.openNextFile();
}
```

#### 2. **Missing File Close in getOrderedFiles()** ⚠️ MEDIUM
**Location:** Line 608-619
**Issue:** Order file (`File f`) opened but may not close on early returns
**Status:** **NEEDS FIX**

---

## 🔍 POTENTIAL ISSUES FOUND:

### **Memory Management:**
1. ✅ **GOOD:** All File uploads properly closed
2. ✅ **GOOD:** Preferences properly closed with `.end()`
3. ⚠️ **CONCERN:** `std::vector<String>` in tight loops (status bar message parsing)
   - Line 168: Creates new vector every frame
   - **Recommendation:** Make static or cache parsed messages

### **WiFi Stability:**
1. ✅ **GOOD:** Watchdog resets added to WiFi connection loop
2. ✅ **GOOD:** 30-second reconnection check in main loop
3. ⚠️ **CONCERN:** No handling for WiFi.reconnect() failure
   - Could get stuck in disconnected state without AP fallback

### **SD Card Error Handling:**
1. ⚠️ **MISSING:** No check if SD card is removed during operation
2. ⚠️ **MISSING:** No validation if .vid file is corrupted
3. ✅ **GOOD:** File existence checks before operations

### **Web Server Concurrency:**
1. ⚠️ **CONCERN:** No mutex/semaphore on shared resources
   - Multiple endpoints modify `currentFile`, `isSlideshow` simultaneously
   - Could cause race conditions with main loop

---

## 🧪 RECOMMENDED TEST PROCEDURES

### **Test 1: 72-Hour Burn-In Test**
**Purpose:** Verify no memory leaks or crashes over extended operation

**Steps:**
1. Load 20+ photos in slideshow mode
2. Set 5-second interval
3. Enable clock every 5 photos
4. Monitor serial output for heap levels every hour
5. Check for automatic 3AM reboot (run for 3+ days)

**Success Criteria:**
- No crashes or reboots (except scheduled 3AM)
- Free heap stays above 60KB
- Watchdog never triggers
- mDNS remains accessible

---

### **Test 2: SD Card Stress Test**
**Purpose:** Verify handling of SD card errors

**Steps:**
1. Start slideshow with 10+ photos
2. **While running:** Remove SD card
3. Observe device behavior
4. Reinsert SD card
5. Attempt to upload new photo via web UI
6. Try uploading corrupted .jpg file
7. Try uploading 10MB file (too large)

**Expected Behavior:**
- Device shouldn't crash when SD removed
- Should show error message or fallback screen
- Recover gracefully when SD reinserted
- Reject invalid/corrupted files

---

### **Test 3: WiFi Isolation Test (5GHz/2.4GHz)**
**Purpose:** Verify mDNS works across network isolation

**Steps:**
1. Connect ESP32 to 2.4GHz WiFi
2. Connect phone to 5GHz band
3. Try accessing: `http://chronosync.local`
4. If fails, try IP address from device screen
5. Test with different routers (ASUS, TP-Link, etc.)

**Success Criteria:**
- mDNS works on 70%+ of routers
- IP address clearly visible on device screen
- Users can manually enter IP if needed

---

### **Test 4: Web UI Stress Test**
**Purpose:** Check for race conditions and concurrent request handling

**Steps:**
1. Open web UI in 2 browser tabs
2. Tab 1: Start uploading large photo
3. Tab 2: Rapidly click Start/Stop slideshow
4. Tab 3: Change orientation while photo uploading
5. Try accessing device from 2 phones simultaneously

**Success Criteria:**
- No crashes or freezes
- Operations complete or fail gracefully
- No corrupted state (e.g., stuck in wrong mode)

---

### **Test 5: Video Playback Endurance**
**Purpose:** Verify .vid playback stability

**Steps:**
1. Upload 5-minute .vid file (120fps)
2. Let it loop for 2+ hours
3. Monitor heap levels
4. Try touch gestures during playback
5. Force stop via web UI mid-playback

**Success Criteria:**
- Smooth playback for 2+ hours
- No heap degradation
- Touch gestures work correctly
- Clean exit when stopped

---

### **Test 6: Time Sync & Maintenance Reboot**
**Purpose:** Verify scheduled operations work correctly

**Steps:**
1. Set device time to 2:58 AM (via NTP server simulation or wait)
2. Monitor for 3:00 AM reboot
3. Verify device restarts cleanly
4. Check state is restored after reboot
5. Test timezone changes (Manila → Tokyo)
6. Test 12hr ↔ 24hr format changes

**Success Criteria:**
- 3AM reboot happens exactly once
- State restored correctly
- Timezone changes apply immediately
- Clock updates correctly

---

### **Test 7: Low Memory Simulation**
**Purpose:** Verify emergency heap monitoring works

**Steps:**
1. Monitor current heap via Serial
2. Temporarily lower `MIN_FREE_HEAP` to current+10KB
3. Upload large files or create memory pressure
4. Observe if emergency restart triggers
5. Restore original MIN_FREE_HEAP value

**Success Criteria:**
- Emergency restart triggers when threshold hit
- Clean restart with heap recovered
- Serial logs show warning before restart

---

### **Test 8: Power Cycle & Brownout**
**Purpose:** Test brownout protection and startup reliability

**Steps:**
1. Rapidly power cycle device 10 times
2. Disconnect power during photo upload
3. Disconnect during video playback
4. Use low-quality USB power (insufficient amperage)
5. Monitor for brownout detector triggers

**Success Criteria:**
- Device boots correctly every time
- No corrupted SD card files
- Brownout detector prevents corruption
- Graceful handling of power loss

---

## 🐛 BUGS TO FIX BEFORE PRODUCTION

### **Priority 1 - CRITICAL (Must Fix):**
1. ✅ **File handle leak in scanDirectory()** - Fixed below
2. ⚠️ **Status bar message vector recreated every frame** - Fixed below

### **Priority 2 - HIGH (Should Fix):**
3. ⚠️ **No SD card removal detection**
4. ⚠️ **WiFi reconnect failure not handled**

### **Priority 3 - MEDIUM (Nice to Have):**
5. ⚠️ **No mutex on shared variables**
6. ⚠️ **No .vid file corruption validation**

---

## 📊 MONITORING CHECKLIST

During all tests, monitor via Serial at 115200 baud:

```
✅ [HEAP] Free: XXXXX bytes (should stay > 60KB)
✅ [BACKLIGHT] Mode changes logged
✅ [MAINTENANCE] 3AM reboot message
✅ Watchdog reset messages (should NOT trigger unexpectedly)
✅ WiFi reconnection attempts
✅ mDNS startup confirmation
```

---

## 🎯 FINAL VALIDATION CHECKLIST

Before shipping to customers:

- [ ] 72-hour burn-in test passed
- [ ] SD card stress test passed  
- [ ] mDNS tested on 3+ router brands
- [ ] Web UI stress test passed
- [ ] Video playback 2+ hours stable
- [ ] 3AM maintenance reboot confirmed
- [ ] Low memory restart tested
- [ ] Power cycle test (10x) passed
- [ ] All Priority 1 bugs fixed
- [ ] User manual includes troubleshooting

---

## 🔧 FIXES TO APPLY NOW

See fixes in next response...
