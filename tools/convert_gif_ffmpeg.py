#!/usr/bin/env python3
"""
Convert GIF to optimized MJPEG for ESP32 playback using ffmpeg.
Based on the approach from ESP32 video player projects.

Usage:
    python3 convert_gif_ffmpeg.py input.gif [orientation] [quality]
    
    orientation: 'landscape' (480x320) or 'portrait' (320x480)
    quality: 1-31 (lower = better quality, larger file. Default: 10)

Output format:
    [4 bytes frame size][JPEG data][4 bytes frame size][JPEG data]...
"""

import sys
import os
import subprocess
import struct
import tempfile
import shutil

def check_ffmpeg():
    """Check if ffmpeg is installed"""
    try:
        subprocess.run(['ffmpeg', '-version'], capture_output=True, check=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False

def convert_gif_to_mjpeg(input_file, output_file, width=480, height=320, quality=10):
    """
    Convert GIF to custom MJPEG format for ESP32.
    
    Args:
        input_file: Path to input GIF
        output_file: Path to output .mjpeg file
        width: Target width
        height: Target height  
        quality: JPEG quality (1-31, lower = better)
    """
    
    if not os.path.exists(input_file):
        print(f"Error: Input file '{input_file}' not found")
        return False
    
    # Create temp directory for frames
    temp_dir = tempfile.mkdtemp()
    
    try:
        # Step 1: Extract frames from GIF using ffmpeg with optimized settings
        print(f"Extracting frames from {input_file}...")
        
        # Use ffmpeg to extract frames as optimized JPEGs
        # -qscale:v sets JPEG quality (2-31, lower = better)
        # -vf scale with force_original_aspect_ratio=decrease and pad to center
        ffmpeg_cmd = [
            'ffmpeg', '-y',
            '-i', input_file,
            '-vf', f'scale={width}:{height}:force_original_aspect_ratio=decrease,pad={width}:{height}:(ow-iw)/2:(oh-ih)/2:black',
            '-qscale:v', str(quality),
            '-f', 'image2',
            os.path.join(temp_dir, 'frame_%05d.jpg')
        ]
        
        result = subprocess.run(ffmpeg_cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"FFmpeg error: {result.stderr}")
            return False
        
        # Step 2: Get list of frame files
        frame_files = sorted([f for f in os.listdir(temp_dir) if f.endswith('.jpg')])
        
        if not frame_files:
            print("Error: No frames extracted")
            return False
        
        print(f"Extracted {len(frame_files)} frames")
        
        # Step 3: Build MJPEG file with our custom format
        # Format: [4 bytes size LE][JPEG data][4 bytes size LE][JPEG data]...
        total_size = 0
        with open(output_file, 'wb') as outfile:
            for i, frame_file in enumerate(frame_files):
                frame_path = os.path.join(temp_dir, frame_file)
                
                with open(frame_path, 'rb') as f:
                    jpeg_data = f.read()
                
                frame_size = len(jpeg_data)
                total_size += 4 + frame_size
                
                # Write frame size (4 bytes, little-endian)
                outfile.write(struct.pack('<I', frame_size))
                # Write JPEG data
                outfile.write(jpeg_data)
                
                if (i + 1) % 10 == 0:
                    print(f"  Processed {i + 1}/{len(frame_files)} frames...")
        
        print(f"\nSuccess! Created {output_file}")
        print(f"  Frames: {len(frame_files)}")
        print(f"  Size: {total_size / 1024:.1f} KB")
        print(f"  Avg frame: {total_size / len(frame_files) / 1024:.1f} KB")
        
        # Also create thumbnail from first frame
        thumb_file = output_file.rsplit('.', 1)[0] + '.jpg'
        first_frame = os.path.join(temp_dir, frame_files[0])
        shutil.copy(first_frame, thumb_file)
        print(f"  Thumbnail: {thumb_file}")
        
        return True
        
    finally:
        # Cleanup temp directory
        shutil.rmtree(temp_dir, ignore_errors=True)

def main():
    if not check_ffmpeg():
        print("Error: ffmpeg not found. Please install ffmpeg:")
        print("  macOS: brew install ffmpeg")
        print("  Ubuntu: sudo apt install ffmpeg")
        print("  Windows: Download from https://ffmpeg.org/download.html")
        sys.exit(1)
    
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    
    input_file = sys.argv[1]
    orientation = sys.argv[2] if len(sys.argv) > 2 else 'landscape'
    quality = int(sys.argv[3]) if len(sys.argv) > 3 else 10
    
    # Set dimensions based on orientation
    if orientation == 'portrait':
        width, height = 320, 480
    else:
        width, height = 480, 320
    
    # Generate output filename
    base_name = os.path.splitext(os.path.basename(input_file))[0]
    output_file = f"{base_name}_{orientation}.mjpeg"
    
    print(f"Converting {input_file} to {output_file}")
    print(f"  Resolution: {width}x{height}")
    print(f"  Quality: {quality} (lower = better)")
    print()
    
    success = convert_gif_to_mjpeg(input_file, output_file, width, height, quality)
    
    if success:
        print(f"\nCopy {output_file} and {base_name}_{orientation}.jpg to your SD card's /gifs/{orientation}/ folder")
    else:
        sys.exit(1)

if __name__ == '__main__':
    main()
