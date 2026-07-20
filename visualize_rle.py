import os
import sys
import struct
from PIL import Image

def decompress_rle_to_png(rle_path, png_path):
    if not os.path.exists(rle_path):
        print(f"Error: {rle_path} not found.")
        return False
        
    print(f"Decompressing {rle_path}...")
    with open(rle_path, 'rb') as f:
        data = f.read()
        
    if len(data) < 4:
        print("Error: Invalid RLE file (too short).")
        return False
        
    width, height = struct.unpack('<HH', data[:4])
    print(f"Dimensions: {width}x{height}")
    
    pixels = []
    i = 4
    n = len(data)
    
    while i < n:
        header = data[i]
        i += 1
        count = (header & 0x7F) + 1
        if header & 0x80:
            if i + 2 > n:
                break
            color = struct.unpack('<H', data[i:i+2])[0]
            i += 2
            pixels.extend([color] * count)
        else:
            for _ in range(count):
                if i + 2 > n:
                    break
                color = struct.unpack('<H', data[i:i+2])[0]
                i += 2
                pixels.append(color)
                
    # Convert RGB565 back to RGB888
    rgb_pixels = []
    for p in pixels:
        r = ((p >> 11) & 0x1F) << 3
        g = ((p >> 5) & 0x3F) << 2
        b = (p & 0x1F) << 3
        # Handle alpha: if black (0,0,0), make it transparent
        if r == 0 and g == 0 and b == 0:
            rgb_pixels.append((0, 0, 0, 0)) # Transparent
        else:
            rgb_pixels.append((r, g, b, 255)) # Opaque
            
    img = Image.new("RGBA", (width, height))
    img.putdata(rgb_pixels)
    img.save(png_path)
    print(f"Saved decompressed PNG to {png_path}")
    return True

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python visualize_rle.py <rle_file_path> [output_png_path]")
        sys.exit(1)
        
    rle_path = sys.argv[1]
    if len(sys.argv) >= 3:
        png_path = sys.argv[2]
    else:
        base, _ = os.path.splitext(rle_path)
        png_path = base + "_preview.png"
        
    decompress_rle_to_png(rle_path, png_path)
