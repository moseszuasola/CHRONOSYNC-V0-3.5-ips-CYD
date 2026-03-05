---
description: Edit the ChronoSync web admin UI and rebuild the embedded header file
---

# Update Web UI Workflow

The web UI is a single-page app in `www/index.html`. It gets compressed and embedded into the firmware as `src/web_index.h`. After any UI changes, you must regenerate this header and rebuild firmware.

## Steps

1. Edit the web UI source file
```
www/index.html
```
Make your HTML/CSS/JS changes here. This is the source of truth for the admin panel.

2. Navigate to the project root
```bash
cd "/Volumes/4tb Mac Hdd/AI Projects/Chronosync/ChronoSync V0 - ESP32-035 3_5in IPS - JAN 2026 Update"
```

3. Regenerate the `web_index.h` header from the updated HTML
```bash
python3 tools/generate_web_index.py
```
This script gzip-compresses `www/index.html` and encodes it as a C byte array in `src/web_index.h`.

4. Verify the header was updated (check the file timestamp)
```bash
ls -la src/web_index.h
```

5. Build and flash firmware with the new web UI embedded
```bash
pio run --target upload
```

6. Open a browser and navigate to the ESP32's IP address to verify the updated UI is live.

## Notes
- The web server serves `web_index.h` content with gzip encoding — no SD card required for the UI
- If the browser shows the old UI, hard refresh with `Cmd+Shift+R` (Mac) to bypass cache
- Keep `www/index.html` as the editable source — never manually edit `src/web_index.h`
