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

def write_vlw_files(font_path, output_dir, allowed_chars, sizes, font_class_name):
    os.makedirs(output_dir, exist_ok=True)
    
    for size in sizes:
        print(f"Generating VLW font data for size {size}...")
        vlw_bytes = generate_vlw_bytes(font_path, size, allowed_chars, font_class_name)
        if not vlw_bytes:
            print(f"Failed to generate size {size}")
            continue
            
        output_file = os.path.join(output_dir, f"{font_class_name}{size}.vlw")
        with open(output_file, "wb") as f:
            f.write(vlw_bytes)
        print(f"Successfully generated VLW font at {output_file} ({len(vlw_bytes)} bytes)")

if __name__ == "__main__":
    if len(sys.argv) < 3 or len(sys.argv) > 4:
        print("Usage: python convert_font.py <font_filepath> <size> [preset_or_string_or_filepath]")
        print("\nPresets for [char_limiter]:")
        print("  1 - Time & Numbers only (0123456789:/)")
        print("  2 - Abbreviations (Numbers, separators, & Uppercase Letters)")
        print("  3 - Full text support (All printable ASCII characters)")
        print("\nExamples:")
        print("  python convert_font.py \"good timing bd.otf\" 15 1")
        print("  python convert_font.py \"good timing bd.otf\" 15 2")
        print("  python convert_font.py \"good timing bd.otf\" 15 3")
        print("  python convert_font.py \"good timing bd.otf\" 15 charset.txt (from file)")
        print("  python convert_font.py \"good timing bd.otf\" 15 \"custom raw string\"")
        sys.exit(1)
        
    font_file = sys.argv[1]
    if not os.path.exists(font_file):
        print(f"Error: Font file '{font_file}' not found.")
        sys.exit(1)
        
    try:
        font_size = int(sys.argv[2])
    except ValueError:
        print("Error: Font size must be an integer.")
        sys.exit(1)
        
    # Character limiter (default if not supplied)
    if len(sys.argv) == 4:
        arg_val = sys.argv[3].strip()
        if arg_val == "1":
            char_set = "0123456789:/"
            print(f"Using Preset 1 (Time/Numeric): '{char_set}'")
        elif arg_val == "2":
            char_set = " 0123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZ/h"
            print(f"Using Preset 2 (Abbreviation/Uppercase): '{char_set}'")
        elif arg_val == "3":
            char_set = "".join(chr(i) for i in range(32, 127))
            print("Using Preset 3 (Full Printable ASCII): All standard letters, numbers, and symbols.")
        elif os.path.exists(arg_val):
            try:
                with open(arg_val, "r", encoding="utf-8") as f:
                    char_set = f.read()
                print(f"Loaded custom character limiter from file '{arg_val}' ({len(char_set)} chars)")
            except Exception as e:
                print(f"Error reading file '{arg_val}': {e}. Using raw argument string.")
                char_set = arg_val
        else:
            char_set = arg_val
            print(f"Using custom character limiter: '{char_set}'")
            
    else:
        char_set = " 0123456789:ACDEFHIMNORSTUW/h"
        print(f"Using default character limiter: '{char_set}'")
        
    # Strip matching outermost quotes if they were double-escaped or passed literally by the shell
    if len(char_set) >= 2:
        if (char_set.startswith('"') and char_set.endswith('"')) or (char_set.startswith("'") and char_set.endswith("'")):
            char_set = char_set[1:-1]
            print(f"Stripped outer quotes. Character set now: '{char_set}'")
        
    # Generate a clean font class name from base name
    # e.g., good timing bd -> GoodTiming
    base_name = os.path.splitext(os.path.basename(font_file))[0]
    base_clean = base_name.lower().replace("bd", "").replace("bold", "").replace("regular", "").strip()
    words = [w.capitalize() for w in base_clean.replace('-', ' ').replace('_', ' ').split()]
    font_class_name = "".join(words)
    
    # Save directly to data directory
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = script_dir if os.path.basename(script_dir) != 'tools' else os.path.dirname(script_dir)
    output_dir = os.path.join(project_root, "data")
    
    # Resolve font_file relative to project root if it doesn't exist locally
    if not os.path.isabs(font_file) and not os.path.exists(font_file):
        resolved_path = os.path.join(project_root, font_file)
        if os.path.exists(resolved_path):
            font_file = resolved_path

    font_sizes = [font_size]
    
    write_vlw_files(font_file, output_dir, char_set, font_sizes, font_class_name)
