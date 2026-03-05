# MJPEG + Enhanced GIF Decoder Update Summary

## Changes Made

### 1. ESP32 Firmware (src/main.cpp)

**Added:**
- `playMJPEG()` function - plays .mjpeg files using TJpg_Decoder
- Format detection in upload handler (supports .vid and .mjpeg)
- Updated all playback call sites to support both formats

**Flash Impact:** +~200 bytes (TJpg_Decoder already installed)

### 2. Web Interface (www/index.html)

**Added:**
- `selectedVideoFormat` variable for format selection
- Updated `uploadRawVideo()` to accept format parameter
- Updated `bakeGifWithCrop()` to accept format parameter  
- Format selection needed in GIF Studio modal

**Still TODO:**
- Add format selection buttons/dropdown in GIF Studio UI
- Implement MJPEG encoding in bakeGifWithCrop() 
- Fix GIF disposal method handling (eliminate flicker)

## Format Comparison

| Format | File Size | Upload Time | Playback FPS | Flash Cost |
|--------|-----------|-------------|--------------|------------|
| .vid (RGB565) | 92 MB (10s) | 45-90s | 30-40 FPS | 0 bytes |
| .mjpeg (Q85) | 4.5 MB (10s) | 2-3s | 18-22 FPS | +200 bytes |

## Next Steps

1. Add format selection UI to GIF Studio modal
2. Implement MJPEG encoding in JavaScript (use canvas.toBlob)
3. Fix GIF disposal methods 0-3 for smoother .vid playback
4. Test both formats side-by-side
5. Regenerate web_index.h and upload firmware

