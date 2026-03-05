#!/usr/bin/env python3
from PIL import Image, ImageDraw, ImageFont
import os

OUT = 'lib/fonts/SplashText.h'
TTF_BOLD = 'lib/fonts/Montserrat-Bold.ttf'
TTF_REGULAR = 'lib/fonts/Montserrat-Regular.ttf'

# Check if Bold exists, otherwise download it
if not os.path.exists(TTF_BOLD):
    print("Downloading Montserrat-Bold.ttf...")
    import requests
    url = 'https://github.com/JulietaUla/Montserrat/raw/master/fonts/ttf/Montserrat-Bold.ttf'
    r = requests.get(url)
    os.makedirs('lib/fonts', exist_ok=True)
    with open(TTF_BOLD, 'wb') as f:
        f.write(r.content)
    print(f"Downloaded {TTF_BOLD}")

# Text items to render
texts = [
    ("CHRONOSYNC", TTF_BOLD, 60, (255, 165, 0)),      # Title - Bold 60pt orange
    ("Digital Memory Vault", TTF_REGULAR, 28, (255, 255, 255)),  # Subtitle - Regular 28pt white
]

all_bitmaps = []
text_info = []

for text, ttf_path, size, color in texts:
    font = ImageFont.truetype(ttf_path, size)
    
    # Render text to get bounding box
    img = Image.new('RGBA', (600, 150), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Draw text and get actual bounds
    draw.text((10, 10), text, font=font, fill=(*color, 255))
    bbox = img.getbbox()
    
    if bbox:
        left, upper, right, lower = bbox
        cropped = img.crop((left, upper, right, lower))
        w, h = cropped.size
        
        # Convert RGBA to RGB565 with alpha blending over black
        pixels_565 = []
        for y in range(h):
            for x in range(w):
                r, g, b, a = cropped.getpixel((x, y))
                # Blend over black background using alpha
                r = (r * a) // 255
                g = (g * a) // 255
                b = (b * a) // 255
                rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                pixels_565.append(rgb565)
        
        text_info.append({
            'name': text.replace(' ', '_').upper(),
            'width': w,
            'height': h,
            'offset': len(all_bitmaps),
            'pixels': pixels_565
        })
        
        all_bitmaps.extend(pixels_565)

# Write header file
with open(OUT, 'w', encoding='utf8') as f:
    f.write('// Generated Montserrat splash screen text bitmaps\n')
    f.write('#ifndef SPLASH_TEXT_H\n')
    f.write('#define SPLASH_TEXT_H\n\n')
    f.write('#include <stdint.h>\n')
    f.write('#include <pgmspace.h>\n\n')
    
    # Write combined bitmap array
    f.write('const uint16_t SplashTextBitmaps[] PROGMEM = {\n')
    for i, v in enumerate(all_bitmaps):
        if i % 12 == 0:
            f.write('  ')
        f.write('0x%04X' % v)
        if i < len(all_bitmaps) - 1:
            f.write(', ')
        if (i + 1) % 12 == 0:
            f.write('\n')
    f.write('\n};\n\n')
    
    # Write individual text metadata
    for info in text_info:
        f.write(f'// {info["name"]}: {info["width"]}x{info["height"]} pixels\n')
        f.write(f'const uint16_t {info["name"]}_WIDTH = {info["width"]};\n')
        f.write(f'const uint16_t {info["name"]}_HEIGHT = {info["height"]};\n')
        f.write(f'const uint32_t {info["name"]}_OFFSET = {info["offset"]};\n\n')
    
    f.write('#endif // SPLASH_TEXT_H\n')

print(f'Wrote {OUT}')
print('Generated text bitmaps:')
for info in text_info:
    print(f'  {info["name"]}: {info["width"]}x{info["height"]} pixels, offset {info["offset"]}')
