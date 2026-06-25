import os
import struct
from PIL import Image

def compress_png_to_rle(png_path, rle_path):
    if not os.path.exists(png_path):
        print(f"Error: {png_path} not found.")
        return False
        
    print(f"Loading {png_path}...")
    img = Image.open(png_path)
    
    # Convert to RGB
    img_rgb = Image.new("RGB", img.size, (0, 0, 0))
    if img.mode == 'RGBA':
        img_rgb.paste(img, mask=img.split()[3])
    else:
        img_rgb.paste(img)
        
    # Resize to 240x240
    print("Resizing to 240x240 (LANCZOS resampling)...")
    img_resized = img_rgb.resize((240, 240), Image.Resampling.LANCZOS)
    print("Quantizing to 64 colors to optimize compression...")
    img_quantized = img_resized.quantize(colors=64).convert("RGB")
    width, height = img_quantized.size
    
    pixels_rgb = list(img_quantized.getdata())
    pixels_565 = []
    for r, g, b in pixels_rgb:
        r5 = (r >> 3) & 0x1F
        g6 = (g >> 2) & 0x3F
        b5 = (b >> 3) & 0x1F
        rgb565 = (r5 << 11) | (g6 << 5) | b5
        pixels_565.append(rgb565)
        
    rle_data = bytearray()
    
    # Write header: width and height
    rle_data.extend(struct.pack('<HH', width, height))
    
    # PackBits compression
    i = 0
    n = len(pixels_565)
    
    while i < n:
        # Search for a run of repeating pixels
        run_len = 1
        while i + run_len < n and pixels_565[i + run_len] == pixels_565[i] and run_len < 128:
            run_len += 1
            
        if run_len >= 2:
            # Repeating packet: high bit is 1, count is run_len - 1
            header = (run_len - 1) | 0x80
            rle_data.append(header)
            rle_data.extend(struct.pack('<H', pixels_565[i]))
            i += run_len
        else:
            # Non-repeating raw packet
            raw_len = 1
            while i + raw_len < n and raw_len < 128:
                # Stop if we find a repeating run of length >= 2
                if i + raw_len + 1 < n and pixels_565[i + raw_len] == pixels_565[i + raw_len + 1]:
                    break
                raw_len += 1
                
            # Raw packet: high bit is 0, count is raw_len - 1
            header = raw_len - 1
            rle_data.append(header)
            for j in range(raw_len):
                rle_data.extend(struct.pack('<H', pixels_565[i + j]))
            i += raw_len
            
    os.makedirs(os.path.dirname(rle_path), exist_ok=True)
    with open(rle_path, 'wb') as f:
        f.write(rle_data)
        
    print(f"Compressed image saved to {rle_path} (Size: {len(rle_data)} bytes)")
    return True

if __name__ == '__main__':
    compress_png_to_rle('DeskBuddyFace1Small.png', 'data/away.rle')
