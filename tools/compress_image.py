import os
import sys
import struct
from PIL import Image

def compress_image_to_rle(image_path, rle_path, resize_dim=None):
    if not os.path.exists(image_path):
        print(f"Error: {image_path} not found.")
        return False
        
    print(f"Loading {image_path}...")
    img = Image.open(image_path)
    
    # Convert to RGB, handling transparency
    if img.mode in ('RGBA', 'LA') or (img.mode == 'P' and 'transparency' in img.info):
        img_rgba = img.convert('RGBA')
        img_rgb = Image.new("RGB", img_rgba.size, (0, 0, 0))
        img_rgb.paste(img_rgba, mask=img_rgba.split()[3])
    else:
        img_rgb = img.convert("RGB")
        
    # Resize if dimension is provided
    if resize_dim:
        print(f"Resizing to {resize_dim[0]}x{resize_dim[1]} (LANCZOS resampling)...")
        img_processed = img_rgb.resize(resize_dim, Image.Resampling.LANCZOS)
    else:
        img_processed = img_rgb
        
    print("Quantizing to 64 colors to optimize compression...")
    img_quantized = img_processed.quantize(colors=64).convert("RGB")
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
            
    dest_dir = os.path.dirname(rle_path)
    if dest_dir:
        os.makedirs(dest_dir, exist_ok=True)
        
    with open(rle_path, 'wb') as f:
        f.write(rle_data)
        
    print(f"Compressed image saved to {rle_path} (Size: {len(rle_data)} bytes, Dimensions: {width}x{height})")
    return True

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python compress_image.py <image_path>")
        sys.exit(1)
        
    image_path = sys.argv[1]
    
    # Save the output directly to the project's data/ directory
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = script_dir if os.path.basename(script_dir) != 'tools' else os.path.dirname(script_dir)
    data_dir = os.path.join(project_root, "data")
    
    # Resolve image_path relative to project root if it doesn't exist locally
    if not os.path.isabs(image_path) and not os.path.exists(image_path):
        resolved_path = os.path.join(project_root, image_path)
        if os.path.exists(resolved_path):
            image_path = resolved_path

    filename = os.path.basename(image_path)
    base, _ = os.path.splitext(filename)
    rle_path = os.path.join(data_dir, base + ".rle")
    
    compress_image_to_rle(image_path, rle_path)
