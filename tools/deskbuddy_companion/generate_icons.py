import struct
import zlib
import os

def create_png(width, height, r, g, b, a=255):
    def chunk(chunk_type, data):
        c = chunk_type + data
        crc = struct.pack('>I', zlib.crc32(c) & 0xFFFFFFFF)
        return struct.pack('>I', len(data)) + c + crc

    header = b'\x89PNG\r\n\x1a\n'
    ihdr = chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))

    radius = width // 2 - 1
    cx, cy = width // 2, height // 2

    raw = b''
    for y in range(height):
        raw += b'\x00'  # filter none
        for x in range(width):
            dx, dy = x - cx + 0.5, y - cy + 0.5
            dist = (dx * dx + dy * dy) ** 0.5
            if dist <= radius:
                # Inside circle - smooth edges
                if dist > radius - 1.5:
                    alpha = int(a * max(0, min(1, radius - dist)))
                    raw += struct.pack('BBBB', r, g, b, alpha)
                else:
                    raw += struct.pack('BBBB', r, g, b, a)
            else:
                raw += struct.pack('BBBB', 0, 0, 0, 0)

    idat = chunk(b'IDAT', zlib.compress(raw))
    iend = chunk(b'IEND', b'')

    return header + ihdr + idat + iend

def create_app_icon(size, r, g, b):
    def chunk(chunk_type, data):
        c = chunk_type + data
        crc = struct.pack('>I', zlib.crc32(c) & 0xFFFFFFFF)
        return struct.pack('>I', len(data)) + c + crc

    header = b'\x89PNG\r\n\x1a\n'
    ihdr = chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0))

    cx, cy = size // 2, size // 2
    radius = size // 2 - 2
    inner_radius = radius - 5
    bar_width = size // 6
    letter_left = size // 4
    letter_right = size // 4 + bar_width

    raw = b''
    for y in range(size):
        raw += b'\x00'
        for x in range(size):
            dx, dy = x - cx + 0.5, y - cy + 0.5
            dist = (dx * dx + dy * dy) ** 0.5
            if dist <= radius:
                is_bar = (letter_left <= x <= letter_right and
                          cy - radius + 6 <= y <= cy + radius - 6)
                is_hole = (letter_left + 3 <= x <= radius and
                           cy - inner_radius <= y <= cy + inner_radius and
                           (x - cx) ** 2 + (y - cy) ** 2 < inner_radius ** 2)
                if is_bar:
                    raw += struct.pack('BBBB', 255, 255, 255, 255)  # white bar
                elif is_hole:
                    raw += struct.pack('BBBB', 0, 0, 0, 0)  # transparent
                elif dist > radius - 1.5:
                    alpha = int(255 * max(0, min(1, radius - dist)))
                    raw += struct.pack('BBBB', r, g, b, alpha)
                else:
                    raw += struct.pack('BBBB', r, g, b, 255)
            else:
                raw += struct.pack('BBBB', 0, 0, 0, 0)

    idat = chunk(b'IDAT', zlib.compress(raw))
    iend = chunk(b'IEND', b'')

    return header + ihdr + idat + iend

base = os.path.dirname(os.path.abspath(__file__))
icons = os.path.join(base, 'src-tauri', 'icons')
os.makedirs(icons, exist_ok=True)

green = create_png(32, 32, 46, 204, 113)   # #2ecc71 green
gray = create_png(32, 32, 149, 165, 166)    # #95a5a6 gray
app_icon = create_app_icon(128, 46, 204, 113)
app_icon_32 = create_app_icon(32, 46, 204, 113)

for name, data in [
    ('tray-green.png', green),
    ('tray-gray.png', gray),
    ('icon.png', app_icon_32),
    ('128x128.png', app_icon),
    ('32x32.png', app_icon_32),
]:
    path = os.path.join(icons, name)
    with open(path, 'wb') as f:
        f.write(data)
    print(f'Created {path} ({len(data)} bytes)')

print('Done!')
