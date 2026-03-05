---
description: Convert a GIF file to MJPEG format and deploy it for ChronoSync playback
---

# Add MJPEG Animation Workflow

ChronoSync plays animations as MJPEG files stored on the SD card. GIFs must be converted to MJPEG before use.

## Prerequisites
- `ffmpeg` installed: `brew install ffmpeg`
- Python 3 with Pillow: `pip install Pillow`

## Steps

1. Place your source GIF in a convenient location, e.g. `/tmp/animation.gif`

2. Navigate to the project root
```bash
cd "/Volumes/4tb Mac Hdd/AI Projects/Chronosync/ChronoSync V0 - ESP32-035 3_5in IPS - JAN 2026 Update"
```

3. Convert GIF → MJPEG using the project tool
```bash
python3 tools/convert_gif_ffmpeg.py /tmp/animation.gif /tmp/animation.mjpeg
```
Or use the alternate converter:
```bash
python3 tools/gif_converter.py /tmp/animation.gif /tmp/animation.mjpeg
```

4. Check the output dimensions — ChronoSync display is **320×480**.  
   Resize if needed using ffmpeg directly:
```bash
ffmpeg -i /tmp/animation.gif -vf "scale=320:480:force_original_aspect_ratio=decrease,pad=320:480:(ow-iw)/2:(oh-ih)/2" -q:v 5 /tmp/animation.mjpeg
```

5. Copy the `.mjpeg` file to your MicroSD card's animations folder:
```
SD Card: /animations/animation.mjpeg
```

6. Insert the SD card into the ESP32 and reboot the device.

7. In the ChronoSync web admin panel, navigate to **Media → Animations** and select the new animation.

## Notes
- MJPEG quality (`-q:v`): lower = better quality, larger file. Range 1–31. Use `5` for good balance.
- Target frame rate: 10–15 fps for smooth playback without overloading the ESP32
- Max recommended file size: ~5MB per animation for responsive playback
- Supported SD card formats: FAT32
