import gzip
import os

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INPUT_FILE = os.path.join(PROJECT_DIR, 'www', 'index.html')
OUTPUT_FILE = os.path.join(PROJECT_DIR, 'src', 'web_index.h')

def compress_web():
    print(f"Reading: {INPUT_FILE}")
    with open(INPUT_FILE, 'rb') as f:
        html_content = f.read()

    print(f"Original size: {len(html_content)} bytes")
    
    # Compress with maximum compression (level 9)
    compressed_content = gzip.compress(html_content, compresslevel=9)
    print(f"Compressed size: {len(compressed_content)} bytes")
    
    # Calculate savings
    savings = 100 - (len(compressed_content) / len(html_content) * 100)
    print(f"Savings: {savings:.2f}%")

    # Generate C header
    header_content = []
    header_content.append("// Auto-generated from www/index.html")
    header_content.append("#ifndef WEB_INDEX_H")
    header_content.append("#define WEB_INDEX_H")
    header_content.append("\n#include <Arduino.h>\n")
    
    header_content.append(f"const size_t MAIN_page_len = {len(compressed_content)};")
    header_content.append("const uint8_t MAIN_page[] PROGMEM = {")
    
    # Format bytes as hex array
    hex_bytes = [f"0x{b:02X}" for b in compressed_content]
    
    # Chunk into lines of 16 bytes for readability
    for i in range(0, len(hex_bytes), 16):
        chunk = hex_bytes[i:i+16]
        line = "    " + ", ".join(chunk)
        # Add comma to all but the last byte of the last chunk
        if i + 16 < len(hex_bytes):
            line += ","
        header_content.append(line)
        
    header_content.append("};")
    header_content.append("\n#endif // WEB_INDEX_H")
    
    with open(OUTPUT_FILE, 'w') as f:
        f.write("\n".join(header_content))
        
    print(f"Successfully wrote compressed header to: {OUTPUT_FILE}")

if __name__ == "__main__":
    compress_web()
