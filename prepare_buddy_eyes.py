"""
prepare_buddy_eyes.py
Converts the 4 device-retrieved RLE eye files from root directory to 100x100 px RLE format in data/.
80x80 RLEs are placed in a 100x100 canvas centered on a pure black background.
"""

import os
import sys
import struct
from PIL import Image

def decompress_rle(rpath):
    with open(rpath, 'rb') as f: data = f.read()
    w, h = struct.unpack('<HH', data[:4])
    pixels = []
    i, n = 4, len(data)
    while i < n:
        hdr = data[i]; i += 1
        cnt = (hdr & 0x7F) + 1
        if hdr & 0x80:
            if i + 2 > n: break
            col = struct.unpack('<H', data[i:i+2])[0]; i += 2
            pixels.extend([col] * cnt)
        else:
            for _ in range(cnt):
                if i + 2 > n: break
                col = struct.unpack('<H', data[i:i+2])[0]; i += 2
                pixels.append(col)
    rgb = []
    for p in pixels:
        r = ((p >> 11) & 0x1F) << 3
        g = ((p >> 5) & 0x3F) << 2
        b = (p & 0x1F) << 3
        rgb.append((r, g, b, 255))
    img = Image.new('RGBA', (w, h))
    img.putdata(rgb)
    return img

def compress_rle_565(img_rgb, out_rle_path):
    img_q = img_rgb.quantize(colors=64).convert('RGB')
    w, h = img_q.size
    pixels = list(img_q.getdata())
    p565 = [((r>>3)<<11)|((g>>2)<<5)|(b>>3) for r,g,b in pixels]
    rle = bytearray()
    rle.extend(struct.pack('<HH', w, h))
    i, n = 0, len(p565)
    while i < n:
        run = 1
        while i+run < n and p565[i+run] == p565[i] and run < 128: run += 1
        if run >= 2:
            rle.append((run-1)|0x80)
            rle.extend(struct.pack('<H', p565[i]))
            i += run
        else:
            raw = 1
            while i+raw < n and raw < 128:
                if i+raw+1 < n and p565[i+raw] == p565[i+raw+1]: break
                raw += 1
            rle.append(raw-1)
            for j in range(raw): rle.extend(struct.pack('<H', p565[i+j]))
            i += raw
    with open(out_rle_path, 'wb') as f: f.write(rle)
    return len(rle)

def main():
    root_dir = os.path.dirname(os.path.abspath(__file__))
    data_dir = os.path.join(root_dir, "data")
    rle_files = ['buddy_eye_o.rle', 'buddy_eye_h.rle', 'buddy_eye_c.rle', 'buddy_eye_s.rle']

    total_bytes = 0
    for rname in rle_files:
        src_path = os.path.join(root_dir, rname)
        dst_path = os.path.join(data_dir, rname)
        if not os.path.exists(src_path):
            print(f"Skipping {src_path} (not found)")
            continue
        
        img = decompress_rle(src_path)
        w, h = img.size
        if w == 100 and h == 100:
            img_100 = img.convert('RGB')
        else:
            img_100 = Image.new('RGB', (100, 100), (0, 0, 0))
            offset_x = (100 - w) // 2
            offset_y = (100 - h) // 2
            img_100.paste(img, (offset_x, offset_y), img.split()[3])

        sz = compress_rle_565(img_100, dst_path)
        total_bytes += sz
        print(f"[OK] {rname}: {w}x{h} -> 100x100 ({sz:,} bytes RLE)")

    print(f"\nAll 4 100x100 eye assets generated! Total size: {total_bytes:,} bytes ({total_bytes/1024:.1f} KB)")

if __name__ == '__main__':
    main()
