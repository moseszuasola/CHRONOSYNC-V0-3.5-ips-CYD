#!/usr/bin/env python3
from PIL import Image, ImageDraw, ImageFont
import os

OUT_LARGE = 'lib/fonts/MontserratBoldClock56.h'
OUT_SMALL = 'lib/fonts/MontserratBoldAMPM.h'
TTF = 'lib/fonts/Montserrat-Bold.ttf'
FONT_SIZE_LARGE = 90  # Much larger for HH:MM
FONT_SIZE_SMALL = 40  # Smaller for AM/PM

# Generate large font for digits and colon
chars_large = '0123456789:'
font_large = ImageFont.truetype(TTF, FONT_SIZE_LARGE)

glyphs_large = []
for ch in chars_large:
    img = Image.new('RGBA', (150, 150), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw.text((10, 10), ch, font=font_large, fill=(255, 255, 255, 255))
    
    bbox = img.getbbox()
    if bbox:
        left, upper, right, lower = bbox
        cropped = img.crop((left, upper, right, lower))
        w, h = cropped.size
        
        pixels_565 = []
        for y in range(h):
            for x in range(w):
                r, g, b, a = cropped.getpixel((x, y))
                gray = a
                r5 = (gray * 31) // 255
                g6 = (gray * 63) // 255
                b5 = (gray * 31) // 255
                rgb565 = (r5 << 11) | (g6 << 5) | b5
                pixels_565.append(rgb565)
        
        glyphs_large.append({
            'char': ch,
            'width': w,
            'height': h,
            'pixels': pixels_565
        })

# Write large font header
with open(OUT_LARGE, 'w', encoding='utf8') as f:
    f.write('// Generated Montserrat Bold clock digits (90pt) - digits 0-9 and :\n')
    f.write('#ifndef MONTSERRAT_BOLD_CLOCK_56_H\n')
    f.write('#define MONTSERRAT_BOLD_CLOCK_56_H\n\n')
    f.write('#include <stdint.h>\n')
    f.write('#include <pgmspace.h>\n\n')
    
    all_pixels = []
    offsets = []
    for g in glyphs_large:
        offsets.append(len(all_pixels))
        all_pixels.extend(g['pixels'])
    
    f.write('const uint16_t MontserratBoldClock56Bitmaps[] PROGMEM = {\n')
    for i, v in enumerate(all_pixels):
        if i % 12 == 0:
            f.write('  ')
        f.write('0x%04X' % v)
        if i < len(all_pixels) - 1:
            f.write(', ')
        if (i + 1) % 12 == 0:
            f.write('\n')
    f.write('\n};\n\n')
    
    f.write('const char MontserratBoldClock56Chars[] = "')
    f.write(''.join(g['char'] for g in glyphs_large))
    f.write('";\n\n')
    
    f.write('const uint16_t MontserratBoldClock56Offsets[] PROGMEM = {\n  ')
    f.write(', '.join(str(off) for off in offsets))
    f.write('\n};\n\n')
    
    f.write('const uint8_t MontserratBoldClock56Width[] PROGMEM = {\n  ')
    f.write(', '.join(str(g['width']) for g in glyphs_large))
    f.write('\n};\n\n')
    
    f.write('const uint8_t MontserratBoldClock56Height[] PROGMEM = {\n  ')
    f.write(', '.join(str(g['height']) for g in glyphs_large))
    f.write('\n};\n\n')
    
    f.write('#endif // MONTSERRAT_BOLD_CLOCK_56_H\n')

print(f'Wrote {OUT_LARGE}')
print('Generated large bold clock digits (90pt):')
for g in glyphs_large:
    print(f"  '{g['char']}': {g['width']}x{g['height']} pixels")

# Generate small font for AM/PM
chars_small = 'AMP'
font_small = ImageFont.truetype(TTF, FONT_SIZE_SMALL)

glyphs_small = []
for ch in chars_small:
    img = Image.new('RGBA', (80, 80), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw.text((5, 5), ch, font=font_small, fill=(255, 255, 255, 255))
    
    bbox = img.getbbox()
    if bbox:
        left, upper, right, lower = bbox
        cropped = img.crop((left, upper, right, lower))
        w, h = cropped.size
        
        pixels_565 = []
        for y in range(h):
            for x in range(w):
                r, g, b, a = cropped.getpixel((x, y))
                gray = a
                r5 = (gray * 31) // 255
                g6 = (gray * 63) // 255
                b5 = (gray * 31) // 255
                rgb565 = (r5 << 11) | (g6 << 5) | b5
                pixels_565.append(rgb565)
        
        glyphs_small.append({
            'char': ch,
            'width': w,
            'height': h,
            'pixels': pixels_565
        })

# Write small font header
with open(OUT_SMALL, 'w', encoding='utf8') as f:
    f.write('// Generated Montserrat Bold AM/PM (40pt)\n')
    f.write('#ifndef MONTSERRAT_BOLD_AMPM_H\n')
    f.write('#define MONTSERRAT_BOLD_AMPM_H\n\n')
    f.write('#include <stdint.h>\n')
    f.write('#include <pgmspace.h>\n\n')
    
    all_pixels = []
    offsets = []
    for g in glyphs_small:
        offsets.append(len(all_pixels))
        all_pixels.extend(g['pixels'])
    
    f.write('const uint16_t MontserratBoldAMPMBitmaps[] PROGMEM = {\n')
    for i, v in enumerate(all_pixels):
        if i % 12 == 0:
            f.write('  ')
        f.write('0x%04X' % v)
        if i < len(all_pixels) - 1:
            f.write(', ')
        if (i + 1) % 12 == 0:
            f.write('\n')
    f.write('\n};\n\n')
    
    f.write('const char MontserratBoldAMPMChars[] = "')
    f.write(''.join(g['char'] for g in glyphs_small))
    f.write('";\n\n')
    
    f.write('const uint16_t MontserratBoldAMPMOffsets[] PROGMEM = {\n  ')
    f.write(', '.join(str(off) for off in offsets))
    f.write('\n};\n\n')
    
    f.write('const uint8_t MontserratBoldAMPMWidth[] PROGMEM = {\n  ')
    f.write(', '.join(str(g['width']) for g in glyphs_small))
    f.write('\n};\n\n')
    
    f.write('const uint8_t MontserratBoldAMPMHeight[] PROGMEM = {\n  ')
    f.write(', '.join(str(g['height']) for g in glyphs_small))
    f.write('\n};\n\n')
    
    f.write('#endif // MONTSERRAT_BOLD_AMPM_H\n')

print(f'\nWrote {OUT_SMALL}')
print('Generated small bold AM/PM (40pt):')
for g in glyphs_small:
    print(f"  '{g['char']}': {g['width']}x{g['height']} pixels")

font = ImageFont.truetype(TTF, FONT_SIZE)

glyphs = []
for ch in chars:
    # Render character on transparent background
    img = Image.new('RGBA', (100, 100), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Draw with white color for maximum contrast
    draw.text((10, 10), ch, font=font, fill=(255, 255, 255, 255))
    
    # Crop to actual bounds
    bbox = img.getbbox()
    if bbox:
        left, upper, right, lower = bbox
        cropped = img.crop((left, upper, right, lower))
        w, h = cropped.size
        
        # Convert RGBA to RGB565 with white text on black background
        pixels_565 = []
        for y in range(h):
            for x in range(w):
                r, g, b, a = cropped.getpixel((x, y))
                # Use alpha channel for anti-aliasing
                # Create grayscale based on alpha (white text on black)
                gray = a
                # Convert to RGB565 grayscale
                r5 = (gray * 31) // 255
                g6 = (gray * 63) // 255
                b5 = (gray * 31) // 255
                rgb565 = (r5 << 11) | (g6 << 5) | b5
                pixels_565.append(rgb565)
        
        glyphs.append({
            'char': ch,
            'width': w,
            'height': h,
            'pixels': pixels_565
        })

# Write header file
with open(OUT, 'w', encoding='utf8') as f:
    f.write('// Generated Montserrat Bold clock digits (56pt) - digits 0-9 and :\n')
    f.write('#ifndef MONTSERRAT_BOLD_CLOCK_56_H\n')
    f.write('#define MONTSERRAT_BOLD_CLOCK_56_H\n\n')
    f.write('#include <stdint.h>\n')
    f.write('#include <pgmspace.h>\n\n')
    
    # Combine all bitmaps
    all_pixels = []
    offsets = []
    for g in glyphs:
        offsets.append(len(all_pixels))
        all_pixels.extend(g['pixels'])
    
    # Write bitmap array
    f.write('const uint16_t MontserratBoldClock56Bitmaps[] PROGMEM = {\n')
    for i, v in enumerate(all_pixels):
        if i % 12 == 0:
            f.write('  ')
        f.write('0x%04X' % v)
        if i < len(all_pixels) - 1:
            f.write(', ')
        if (i + 1) % 12 == 0:
            f.write('\n')
    f.write('\n};\n\n')
    
    # Character set
    f.write('const char MontserratBoldClock56Chars[] = "')
    f.write(''.join(g['char'] for g in glyphs))
    f.write('";\n\n')
    
    # Offsets array
    f.write('const uint16_t MontserratBoldClock56Offsets[] PROGMEM = {\n  ')
    f.write(', '.join(str(off) for off in offsets))
    f.write('\n};\n\n')
    
    # Width array
    f.write('const uint8_t MontserratBoldClock56Width[] PROGMEM = {\n  ')
    f.write(', '.join(str(g['width']) for g in glyphs))
    f.write('\n};\n\n')
    
    # Height array
    f.write('const uint8_t MontserratBoldClock56Height[] PROGMEM = {\n  ')
    f.write(', '.join(str(g['height']) for g in glyphs))
    f.write('\n};\n\n')
    
    f.write('#endif // MONTSERRAT_BOLD_CLOCK_56_H\n')

print(f'Wrote {OUT}')
print('Generated bold clock digits:')
for g in glyphs:
    print(f"  '{g['char']}': {g['width']}x{g['height']} pixels")
