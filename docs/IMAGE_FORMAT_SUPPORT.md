# Image Format Support & Automatic Conversion

## Overview
ChronoSync automatically converts all image formats to JPEG before storing on the device. This ensures:
- **Universal compatibility** with the TJpg_Decoder library (JPEG-only)
- **Memory efficiency** for ESP32 (no PSRAM)
- **Consistent quality** at 90% JPEG compression

## Supported Upload Formats
The web interface accepts ALL standard image formats:

### Fully Supported
- **JPEG/JPG** - Native format, no conversion needed
- **PNG** - Converted to JPEG (transparency removed, white background)
- **BMP** - Converted to JPEG
- **WEBP** - Converted to JPEG
- **GIF** - First frame extracted and converted to JPEG

### Browser-Dependent
- **HEIC/HEIF** - Supported on Safari/iOS, converted to JPEG
- **AVIF** - Supported on modern browsers, converted to JPEG
- **TIFF** - Limited support, may fail on some browsers

## Conversion Process

### 1. Upload Detection (uploadPhoto function)
```javascript
// Detect format and notify user
const fileType = file.type.toLowerCase();
if (fileType !== 'image/jpeg' && fileType !== 'image/jpg') {
    const formatName = fileType.replace('image/', '').toUpperCase();
    showToast(`Converting ${formatName} to JPEG...`, 'info');
}
```

### 2. Browser Processing
- File loaded via FileReader as data URL
- Image decoded to canvas in browser memory
- NO processing done on ESP32 (saves memory)

### 3. Crop & Convert (applyCrop function)
```javascript
// After cropping, convert to JPEG at 90% quality
croppedCanvas.toBlob(async (blob) => {
    formData.append('photo', blob, 'cropped_photo.jpg');
    // Upload to ESP32
}, 'image/jpeg', 0.9);
```

### 4. ESP32 Storage
- Receives pre-converted JPEG data
- Stores directly to SD card
- No additional processing needed

## Quality Settings
- **Compression**: 90% quality (`0.9` parameter)
- **Dimensions**: 480x320 pixels (landscape) or 320x480 (portrait)
- **Color Space**: RGB (24-bit)

## Size Limits
- **Maximum dimensions**: 8000x8000 pixels (browser memory protection)
- **Recommended upload size**: <10MB per file
- **SD card storage**: Limited by card capacity

## Format-Specific Behavior

### PNG with Transparency
- **Alpha channel removed** - replaced with white background
- **Recommendation**: Use solid background images for best results
- **Example**: PNG logo with transparent background → JPEG with white background

### Animated GIF
- **Only first frame extracted** - animation not preserved
- **Recommendation**: Use video format for motion content
- **Example**: Animated GIF → Static JPEG of first frame

### HEIC (iPhone Photos)
- **iOS Safari**: Automatic conversion, works seamlessly
- **Other browsers**: May require manual export from Photos app
- **Recommendation**: Enable "Transfer to Mac/PC: Automatic" in iPhone settings

### BMP (Bitmap)
- **Large file size**: BMPs are uncompressed, may take longer to upload
- **Conversion benefit**: JPEG typically 90% smaller than BMP
- **Example**: 5MB BMP → 500KB JPEG

## User Experience

### Upload Flow
1. **Select any image** via file picker (shows all formats)
2. **Automatic detection** - toast notification if non-JPEG
3. **Format conversion** - happens transparently in browser
4. **Crop interface** - same workflow for all formats
5. **Upload to device** - receives optimized JPEG

### Notifications
- **JPEG files**: No notification (native format)
- **PNG files**: "Converting PNG to JPEG..."
- **BMP files**: "Converting BMP to JPEG..."
- **Other formats**: "Converting [FORMAT] to JPEG..."
- **Errors**: "Failed to load image - unsupported format"

## Error Handling

### Invalid Files
```javascript
if (!fileType.startsWith('image/')) {
    showToast('Please select a valid image file', 'error');
    return;
}
```

### Oversized Images
```javascript
if (img.width > 8000 || img.height > 8000) {
    showToast('Image too large (max 8000x8000 pixels)', 'error');
    return;
}
```

### Failed Conversion
```javascript
img.onerror = function() {
    showToast('Failed to load image - unsupported format', 'error');
};
```

## Testing Recommendations

### Format Compatibility Test
Upload one file of each format:
- [ ] JPEG - should upload instantly, no conversion notice
- [ ] PNG - should show conversion toast, upload successfully
- [ ] BMP - should show conversion toast, upload successfully
- [ ] WEBP - should show conversion toast, upload successfully
- [ ] GIF - should extract first frame, upload successfully

### Quality Validation
After upload, verify:
- [ ] Colors accurate (no banding/artifacts)
- [ ] Sharp details preserved at 90% quality
- [ ] No visible compression artifacts on screen

### Edge Cases
- [ ] PNG with transparency → white background added
- [ ] Animated GIF → first frame only
- [ ] Large BMP (>10MB) → successful conversion and upload
- [ ] Portrait vs landscape orientation preserved

## Technical Details

### Why Browser-Side Conversion?
1. **Memory constraints**: ESP32 has ~200KB RAM, no PSRAM
2. **Processing power**: Modern browsers handle image decode efficiently
3. **Network efficiency**: Upload already-compressed JPEG vs raw PNG
4. **Compatibility**: TJpg_Decoder library only supports JPEG

### Code Location
- **Upload function**: `www/index.html` line 1244 (`uploadPhoto()`)
- **Conversion**: `www/index.html` line 1467 (`applyCrop()`)
- **Format detection**: Lines 1253-1260
- **Quality setting**: Line 1522 (`toBlob(..., 'image/jpeg', 0.9)`)

### Dependencies
- **Browser APIs**: FileReader, Canvas, toBlob
- **ESP32 library**: TJpg_Decoder v1.0.8
- **File system**: SD card (FAT32)

## Troubleshooting

### "Failed to load image - unsupported format"
**Cause**: Browser doesn't support the format (rare)
**Solution**: Convert file to JPEG/PNG using desktop software before upload

### "Image too large (max 8000x8000 pixels)"
**Cause**: Protection against browser memory crash
**Solution**: Resize image to <8000px in largest dimension

### Upload works but image doesn't display
**Cause**: SD card write error or corruption
**Solution**: Check SD card health, reformat if needed

### PNG transparency shows black background
**Cause**: Browser-specific canvas behavior
**Solution**: This is expected - device uses white background by default

## Future Enhancements
- **Configurable quality**: Allow user to set compression level
- **Background color selection**: Choose transparency replacement color
- **Multi-frame GIF**: Extract and convert all frames as separate photos
- **RAW format support**: Camera RAW conversion (requires library)
- **WebAssembly decoder**: Server-side conversion for unsupported browsers

## Summary
✅ **Any image format** → automatically converted to JPEG at 90% quality  
✅ **Browser-side processing** → no ESP32 memory overhead  
✅ **Transparent workflow** → user doesn't need to know about conversion  
✅ **Quality preserved** → minimal visible compression artifacts  
✅ **Commercial-grade** → handles all common formats customers may use  
