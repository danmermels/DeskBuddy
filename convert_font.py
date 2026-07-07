import os
import sys
import struct
from PIL import Image, ImageDraw, ImageFont

def generate_vlw_bytes(font_path, font_size, allowed_chars, font_class_name):
    # Load font
    try:
        font = ImageFont.truetype(font_path, font_size)
    except Exception as e:
        print(f"Error loading font {font_path}: {e}")
        return None

    # Get general font metrics
    # font.getmetrics() -> (ascent, descent)
    ascent, descent = font.getmetrics()
    
    # Render glyphs and gather metrics
    glyphs_data = []
    bitmaps_data = bytearray()
    
    # We sort characters by ASCII code
    chars = sorted(list(set(allowed_chars)))
    
    for char in chars:
        unicode_val = ord(char)
        # Get bbox relative to baseline (0,0)
        left, top, right, bottom = font.getbbox(char)
        g_width = right - left
        g_height = bottom - top
        
        # Calculate advance width
        gx_advance = int(round(font.getlength(char)))
        
        # Handle empty/whitespace characters
        if g_width <= 0 or g_height <= 0:
            glyphs_data.append({
                "unicode": unicode_val,
                "gHeight": 0,
                "gWidth": 0,
                "gxAdvance": gx_advance,
                "gdY": 0,
                "gdX": 0,
            })
            continue
            
        # Draw glyph in anti-aliased grayscale (L mode)
        img = Image.new("L", (g_width, g_height), 0)
        draw = ImageDraw.Draw(img)
        draw.text((-left, -top), char, font=font, fill=255)
        
        # gdY is distance from baseline to top of glyph (positive = up)
        gdY = -top
        gdX = left
        
        glyphs_data.append({
            "unicode": unicode_val,
            "gHeight": g_height,
            "gWidth": g_width,
            "gxAdvance": gx_advance,
            "gdY": gdY,
            "gdX": gdX,
        })
        
        # Append raw 8-bit alpha pixels
        bitmaps_data.extend(img.tobytes())
        
    # Assemble VLW format
    vlw = bytearray()
    
    # 1. Header (6 x uint32_t big-endian)
    g_count = len(glyphs_data)
    vlw.extend(struct.pack('>IIIIII', g_count, 11, font_size, 0, ascent, descent))
    
    # 2. Glyph Index (gCount * 7 x int32_t big-endian)
    for g in glyphs_data:
        vlw.extend(struct.pack('>iiiiiii', 
            g["unicode"], 
            g["gHeight"], 
            g["gWidth"], 
            g["gxAdvance"], 
            g["gdY"], 
            g["gdX"], 
            0 # padding
        ))
        
    # 3. Bitmaps
    vlw.extend(bitmaps_data)
    
    # 4. Trailer
    font_name_bytes = (font_class_name + f"_{font_size}").encode('utf-8')
    vlw.append(len(font_name_bytes))
    vlw.extend(font_name_bytes)
    vlw.append(0) # Null terminator for Font Name
    
    ps_name_bytes = (font_class_name + f"_{font_size}-Regular").encode('utf-8')
    vlw.append(len(ps_name_bytes))
    vlw.extend(ps_name_bytes)
    vlw.append(1) # Anti-aliased / smoothed flag (1 = True)
    
    return vlw

def write_header(font_path, output_header_path, allowed_chars, sizes, font_class_name):
    header_guard = f"{font_class_name.upper()}_FONT_H"
    
    header_content = []
    header_content.append(f"// This file is auto-generated from convert_font.py using {os.path.basename(font_path)}")
    header_content.append(f"#ifndef {header_guard}")
    header_content.append(f"#define {header_guard}")
    header_content.append("")
    header_content.append("#include <pgmspace.h>")
    header_content.append("")

    for size in sizes:
        print(f"Generating VLW font data for size {size}...")
        vlw_bytes = generate_vlw_bytes(font_path, size, allowed_chars, font_class_name)
        if not vlw_bytes:
            print(f"Failed to generate size {size}")
            continue
            
        var_name = f"{font_class_name}{size}"
        header_content.append(f"// Size: {size}pt, Glyphs: {len(allowed_chars)}, Total size: {len(vlw_bytes)} bytes")
        header_content.append(f"const uint8_t {var_name}[] PROGMEM = {{")
        
        # Format bytes as hex array
        hex_lines = []
        for i in range(0, len(vlw_bytes), 16):
            chunk = vlw_bytes[i:i+16]
            hex_str = ", ".join(f"0x{b:02X}" for b in chunk)
            hex_lines.append("  " + hex_str + ",")
        header_content.extend(hex_lines)
        header_content.append("};")
        header_content.append("")
        
    header_content.append(f"#endif // {header_guard}")
    
    with open(output_header_path, "w", encoding="utf-8") as f:
        f.write("\n".join(header_content))
        
    print(f"Successfully generated font header at {output_header_path}")

if __name__ == "__main__":
    if len(sys.argv) < 5 or len(sys.argv) > 6:
        print("Usage: python convert_font.py <font_filepath> <size1> <size2> <size3> [char_limiter]")
        print("Example: python convert_font.py \"good timing bd.otf\" 15 20 42 \" 0123456789:ACDEFHIMNORSTUW\"")
        sys.exit(1)
        
    font_file = sys.argv[1]
    if not os.path.exists(font_file):
        print(f"Error: Font file '{font_file}' not found.")
        sys.exit(1)
        
    try:
        size1 = int(sys.argv[2])
        size2 = int(sys.argv[3])
        size3 = int(sys.argv[4])
    except ValueError:
        print("Error: Font sizes must be integers.")
        sys.exit(1)
        
    # Character limiter (default if not supplied)
    if len(sys.argv) == 6:
        char_set = sys.argv[5]
        print(f"Using custom character limiter: '{char_set}'")
    else:
        char_set = " 0123456789:ACDEFHIMNORSTUW"
        print(f"Using default character limiter: '{char_set}'")
        
    # Generate a clean font class name from base name
    # e.g., good timing bd -> GoodTiming
    base_name = os.path.splitext(os.path.basename(font_file))[0]
    base_clean = base_name.lower().replace("bd", "").replace("bold", "").replace("regular", "").strip()
    words = [w.capitalize() for w in base_clean.replace('-', ' ').replace('_', ' ').split()]
    font_class_name = "".join(words)
    
    # Save inside the src directory
    output_header = f"src/{font_class_name}Font.h"
    font_sizes = [size1, size2, size3]
    
    write_header(font_file, output_header, char_set, font_sizes, font_class_name)
