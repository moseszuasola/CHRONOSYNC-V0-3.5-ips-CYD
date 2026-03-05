#!/usr/bin/env python3
import os

# Read the HTML file
with open('www/index.html', 'r', encoding='utf-8') as f:
    html_content = f.read()

# Write the header file
with open('src/web_index.h', 'w', encoding='utf-8') as f:
    f.write('// Auto-generated from www/index.html\n')
    f.write('#ifndef WEB_INDEX_H\n')
    f.write('#define WEB_INDEX_H\n\n')
    f.write('const char MAIN_page[] PROGMEM = R"rawliteral(\n')
    f.write(html_content)
    f.write('\n)rawliteral";\n\n')
    f.write('#endif // WEB_INDEX_H\n')

print('Generated src/web_index.h from www/index.html')
print(f'HTML size: {len(html_content)} bytes')
