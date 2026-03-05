---
description: Create a timestamped backup of the current ChronoSync firmware before making changes
---

# Backup Firmware Workflow

Always create a backup before making significant changes to `main.cpp` or `web_index.h`.

## Steps

1. Navigate to the project root
```bash
cd "/Volumes/4tb Mac Hdd/AI Projects/Chronosync/ChronoSync V0 - ESP32-035 3_5in IPS - JAN 2026 Update"
```

2. Create a timestamped backup of the main firmware file
```bash
cp src/main.cpp backups/main_$(date +%Y%m%d_%H%M%S).cpp
```

3. (Optional) Also back up the web UI header
```bash
cp src/web_index.h backups/web_index_$(date +%Y%m%d_%H%M%S).h
```

4. (Optional) Back up the compiled binary if available
```bash
cp .pio/build/esp32dev/firmware.bin backups/firmware_$(date +%Y%m%d_%H%M%S).bin
```

5. Verify the backup was created
```bash
ls -lh backups/ | tail -5
```

## Restoring from a Backup

To restore a previous version:
```bash
cp backups/main_YYYYMMDD_HHMMSS.cpp src/main.cpp
pio run --target upload
```

## Notes
- The `backups/` folder already contains historical snapshots — check there before starting new features
- Backup filenames include a timestamp so you can identify them by date/time
- Consider creating a git commit as well: `git add -A && git commit -m "Backup before XYZ changes"`
