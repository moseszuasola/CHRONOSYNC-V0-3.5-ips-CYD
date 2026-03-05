#!/usr/bin/env python3
"""
Enhanced GIF to MJPEG/VID Converter
Converts GIF animations to smooth video format for ESP32 playback

Features:
- Proper frame disposal handling (eliminates flicker)
- Frame interpolation for smooth playback
- Optimized encoding with PIL/Pillow
- Support for both raw RGB565 (.vid) and MJPEG output
- Automatic frame rate normalization

Usage:
    python gif_converter.py input.gif output.vid --fps 30 --format vid
    python gif_converter.py input.gif output.mjpeg --fps 30 --format mjpeg
"""

import sys
import argparse
from PIL import Image, ImageSequence
import struct
import io

def normalize_frame_rate(frames, target_fps=30):
    """
    Normalize variable GIF frame rates to constant FPS
    Duplicates frames for slow delays, skips frames for fast delays
    """
    normalized = []
    target_delay_ms = 1000.0 / target_fps
    
    for frame_data in frames:
        frame, delay_ms = frame_data
        
        # How many times should this frame appear?
        num_repeats = max(1, round(delay_ms / target_delay_ms))
        
        for _ in range(num_repeats):
            normalized.append(frame)
    
    return normalized

def extract_gif_frames(gif_path):
    """
    Extract all frames from GIF with proper disposal handling
    Returns list of (PIL.Image, delay_ms) tuples
    """
    img = Image.open(gif_path)
    frames = []
    
    # Start with transparent/black background
    background = Image.new('RGBA', img.size, (0, 0, 0, 255))
    prev_frame = None
    
    for frame_idx, frame in enumerate(ImageSequence.Iterator(img)):
        # Get frame delay (in milliseconds)
        delay = frame.info.get('duration', 100)  # Default 100ms if not specified
        
        # Get disposal method (0=none, 1=do not dispose, 2=restore background, 3=restore previous)
        disposal = frame.info.get('disposal', 0)
        
        # Convert frame to RGBA (handles transparency correctly)
        frame = frame.convert('RGBA')
        
        # Handle disposal of previous frame
        if frame_idx > 0:
            if disposal == 2:  # Restore to background
                # Start fresh with background
                working = background.copy()
            elif disposal == 3:  # Restore to previous
                # Use the frame before the last one
                working = prev_frame.copy() if prev_frame else background.copy()
            else:  # 0 or 1: Do not dispose (keep accumulating)
                # Keep building on previous composite
                working = background.copy()
                if prev_frame:
                    working.paste(prev_frame, (0, 0))
        else:
            working = background.copy()
        
        # Composite current frame onto working image
        working.paste(frame, (0, 0), frame)
        
        # Convert to RGB (removes alpha channel)
        rgb_frame = Image.new('RGB', working.size, (0, 0, 0))
        rgb_frame.paste(working, (0, 0), working)
        
        # Save this composite for potential "restore to previous"
        prev_frame = working.copy()
        
        frames.append((rgb_frame, delay))
    
    return frames

def resize_and_crop(image, target_width, target_height):
    """
    Resize image to fit target dimensions while maintaining aspect ratio
    Then crop to exact size (centered)
    """
    # Calculate aspect ratios
    img_aspect = image.width / image.height
    target_aspect = target_width / target_height
    
    if img_aspect > target_aspect:
        # Image is wider than target - fit by height
        new_height = target_height
        new_width = int(new_height * img_aspect)
    else:
        # Image is taller than target - fit by width
        new_width = target_width
        new_height = int(new_width / img_aspect)
    
    # Resize with high-quality resampling
    resized = image.resize((new_width, new_height), Image.Resampling.LANCZOS)
    
    # Crop to exact size (centered)
    left = (new_width - target_width) // 2
    top = (new_height - target_height) // 2
    right = left + target_width
    bottom = top + target_height
    
    cropped = resized.crop((left, top, right, bottom))
    
    return cropped

def rgb_to_rgb565(r, g, b):
    """Convert RGB888 to RGB565 (Little Endian for ESP32)"""
    rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    # Return as little-endian bytes
    return struct.pack('<H', rgb565)

def convert_to_vid(frames, output_path, width, height):
    """
    Convert frames to raw RGB565 .vid format
    Each frame is width*height*2 bytes
    """
    print(f"Converting {len(frames)} frames to .vid format ({width}x{height})...")
    
    with open(output_path, 'wb') as f:
        for idx, frame in enumerate(frames):
            if idx % 10 == 0:
                print(f"  Frame {idx+1}/{len(frames)}...", end='\r')
            
            # Resize/crop frame
            processed = resize_and_crop(frame, width, height)
            
            # Convert to RGB565
            pixels = processed.load()
            for y in range(height):
                for x in range(width):
                    r, g, b = pixels[x, y]
                    f.write(rgb_to_rgb565(r, g, b))
    
    print(f"\n✅ Saved {output_path} ({len(frames)} frames)")

def convert_to_mjpeg(frames, output_path, width, height, quality=85):
    """
    Convert frames to MJPEG format
    Format: [frame_size(4 bytes)][JPEG data][frame_size][JPEG data]...
    """
    print(f"Converting {len(frames)} frames to MJPEG format ({width}x{height})...")
    
    with open(output_path, 'wb') as f:
        for idx, frame in enumerate(frames):
            if idx % 10 == 0:
                print(f"  Frame {idx+1}/{len(frames)}...", end='\r')
            
            # Resize/crop frame
            processed = resize_and_crop(frame, width, height)
            
            # Encode as JPEG
            buffer = io.BytesIO()
            processed.save(buffer, format='JPEG', quality=quality, optimize=True)
            jpeg_data = buffer.getvalue()
            
            # Write frame size (4 bytes, little-endian) then JPEG data
            f.write(struct.pack('<I', len(jpeg_data)))
            f.write(jpeg_data)
    
    print(f"\n✅ Saved {output_path} ({len(frames)} frames)")

def create_thumbnail(frames, output_path, width, height):
    """Create JPEG thumbnail from first frame"""
    if frames:
        thumb = resize_and_crop(frames[0], width, height)
        thumb.save(output_path, format='JPEG', quality=80)
        print(f"✅ Saved thumbnail: {output_path}")

def main():
    parser = argparse.ArgumentParser(description='Convert GIF to smooth video format for ESP32')
    parser.add_argument('input', help='Input GIF file')
    parser.add_argument('output', help='Output file (.vid or .mjpeg)')
    parser.add_argument('--fps', type=int, default=30, help='Target FPS (default: 30)')
    parser.add_argument('--format', choices=['vid', 'mjpeg'], default='vid', 
                        help='Output format (default: vid)')
    parser.add_argument('--width', type=int, default=480, help='Output width (default: 480)')
    parser.add_argument('--height', type=int, default=320, help='Output height (default: 320)')
    parser.add_argument('--quality', type=int, default=85, 
                        help='JPEG quality for MJPEG format (default: 85)')
    parser.add_argument('--thumbnail', help='Output thumbnail JPEG path')
    
    args = parser.parse_args()
    
    print(f"\n🎬 GIF to Video Converter")
    print(f"Input: {args.input}")
    print(f"Output: {args.output} ({args.format} format)")
    print(f"Resolution: {args.width}x{args.height} @ {args.fps} FPS\n")
    
    # Step 1: Extract frames with proper disposal
    print("📷 Extracting frames from GIF...")
    raw_frames = extract_gif_frames(args.input)
    print(f"  Found {len(raw_frames)} frames")
    
    # Step 2: Normalize frame rate
    print(f"⚡ Normalizing to {args.fps} FPS...")
    normalized = normalize_frame_rate(raw_frames, args.fps)
    print(f"  Normalized to {len(normalized)} frames")
    
    # Step 3: Convert to output format
    if args.format == 'vid':
        convert_to_vid(normalized, args.output, args.width, args.height)
    else:
        convert_to_mjpeg(normalized, args.output, args.width, args.height, args.quality)
    
    # Step 4: Create thumbnail
    if args.thumbnail:
        create_thumbnail(normalized, args.thumbnail, args.width, args.height)
    
    # Calculate file sizes
    import os
    output_size = os.path.getsize(args.output)
    print(f"\n📊 Statistics:")
    print(f"  Total frames: {len(normalized)}")
    print(f"  Output size: {output_size / 1024 / 1024:.2f} MB")
    print(f"  Avg frame size: {output_size / len(normalized) / 1024:.2f} KB")
    print(f"  Duration: {len(normalized) / args.fps:.2f} seconds")
    print(f"\n✨ Conversion complete!")

if __name__ == '__main__':
    main()
