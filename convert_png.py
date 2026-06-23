import os
from PIL import Image

image_path = 'DeskBuddyFace1Small.png'
header_path = 'src/AwayImage.h'

if not os.path.exists(image_path):
    print(f"Error: {image_path} not found.")
    exit(1)

print(f"Loading {image_path}...")
img = Image.open(image_path)
width, height = img.size
print(f"Original image size: {width}x{height}")

# Convert to RGB (in case of RGBA/transparency, this pads alpha with black)
img_rgb = Image.new("RGB", img.size, (0, 0, 0))
if img.mode == 'RGBA':
    img_rgb.paste(img, mask=img.split()[3]) # paste using alpha channel as mask
else:
    img_rgb.paste(img)

# Resize to exactly 240x240 to fill/fit the GC9A01 TFT screen
print("Resizing to 240x240...")
img_resized = img_rgb.resize((240, 240), Image.Resampling.LANCZOS)

print(f"Writing to {header_path}...")
with open(header_path, 'w') as f:
    f.write('#ifndef AWAY_IMAGE_H\n')
    f.write('#define AWAY_IMAGE_H\n\n')
    f.write('#include <pgmspace.h>\n\n')
    f.write('#define AWAY_IMG_WIDTH 240\n')
    f.write('#define AWAY_IMG_HEIGHT 240\n\n')
    f.write('const uint16_t away_img_data[] PROGMEM = {\n')
    
    pixels = list(img_resized.getdata())
    hex_vals = []
    for r, g, b in pixels:
        # Convert RGB888 to RGB565 (16-bit)
        r5 = (r >> 3) & 0x1F
        g6 = (g >> 2) & 0x3F
        b5 = (b >> 3) & 0x1F
        rgb565 = (r5 << 11) | (g6 << 5) | b5
        hex_vals.append(f"0x{rgb565:04X}")
    
    # Write in rows of 12 values for formatting
    for i in range(0, len(hex_vals), 12):
        f.write("  " + ", ".join(hex_vals[i:i+12]) + ",\n")
        
    f.write('};\n\n')
    f.write('#endif // AWAY_IMAGE_H\n')

print("AwayImage.h generated successfully!")
