#!/usr/bin/env python3
from PIL import Image, ImageDraw, ImageFont
import os

OUT = 'lib/fonts/MontserratDigits56.h'
TTF = 'lib/fonts/Montserrat-Regular.ttf'
CHARS = ['0','1','2','3','4','5','6','7','8','9',':']
SIZE = 56
FG_RGB = (0,255,255)  # cyan digits
BG_RGB = (0,0,0)      # black background

os.makedirs('lib/fonts', exist_ok=True)
font = ImageFont.truetype(TTF, SIZE)

glyph_bitmaps = []
widths = []
heights = []
offsets = []
all_pixels = []

offset = 0
for ch in CHARS:
    # render glyph onto image with margin
    img = Image.new('L', (SIZE*2, SIZE*2), 0)
    draw = ImageDraw.Draw(img)
    # draw the char at (0,0)
    draw.text((0,0), ch, font=font, fill=255)
    bbox = img.getbbox()
    if not bbox:
        w,h = (int(font.getmask(ch).size[0]), 1)
        # create 1x1 empty
        cropped = Image.new('L',(w,h),0)
    else:
        left,upper,right,lower = bbox
        cropped = img.crop((left,upper,right,lower))
        w,h = cropped.size
    widths.append(w)
    heights.append(h)
    offsets.append(offset)
    # create RGB565 buffer using alpha=grayscale value
    pixels_565 = []
    for y in range(h):
        for x in range(w):
            a = cropped.getpixel((x,y))  # 0-255
            # blend FG over black background
            r = (FG_RGB[0] * a) // 255
            g = (FG_RGB[1] * a) // 255
            b = (FG_RGB[2] * a) // 255
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            pixels_565.append(rgb565)
    all_pixels.extend(pixels_565)
    offset += len(pixels_565)

# Write header
with open(OUT, 'w', encoding='utf8') as f:
    f.write('// Generated Montserrat digit bitmaps (56pt) - digits 0-9 and :\n')
    f.write('#ifndef MONTSERRAT_DIGITS_56_H\n')
    f.write('#define MONTSERRAT_DIGITS_56_H\n\n')
    f.write('#include <stdint.h>\n')
    f.write('#include <pgmspace.h>\n\n')
    f.write('const uint16_t MontserratDigits56Bitmaps[] PROGMEM = {\n')
    for i,v in enumerate(all_pixels):
        if i % 12 == 0:
            f.write('  ')
        f.write('0x%04X, ' % v)
        if (i+1) % 12 == 0:
            f.write('\n')
    f.write('\n};\n\n')
    # glyph metadata
    f.write('const uint16_t MontserratDigits56Offsets[] PROGMEM = { ')
    f.write(', '.join(str(o) for o in offsets))
    f.write(' };\n')
    f.write('\nconst uint8_t MontserratDigits56Width[] PROGMEM = { ')
    f.write(', '.join(str(w) for w in widths))
    f.write(' };\n')
    f.write('\nconst uint8_t MontserratDigits56Height[] PROGMEM = { ')
    f.write(', '.join(str(h) for h in heights))
    f.write(' };\n')
    f.write('\nconst char* MontserratDigits56Chars = "0123456789:";\n')
    f.write('\n#endif // MONTSERRAT_DIGITS_56_H\n')

print('Wrote', OUT)
