import os
import sys
from PIL import Image
import compress_image

def process_hand_image(image_path):
    if not os.path.exists(image_path):
        print(f"File {image_path} not found. Skipping.")
        return None

    img = Image.open(image_path).convert('RGBA')
    width, height = img.size
    pixels = img.load()

    pivot_x = None
    pivot_y = None

    # Search for a red-ish pixel with high saturation
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            # Match high-red, low green/blue, and mostly opaque pixel
            if r > 200 and g < 80 and b < 80 and a > 128:
                pivot_x = x
                pivot_y = y
                break
        if pivot_x is not None:
            break

    if pivot_x is None:
        print(f"Warning: No red pivot pixel (255, 0, 0) found in {image_path}. Defaulting pivot to center bottom.")
        pivot_x = width // 2
        pivot_y = height - 1

    print(f"Processed {image_path}:")
    print(f"  - Size: {width}x{height}")
    print(f"  - Pivot found at: ({pivot_x}, {pivot_y})")

    # Compress to RLE directly into the data/ directory
    script_dir = os.path.dirname(os.path.abspath(__file__))
    data_dir = os.path.join(script_dir, "data")
    filename = os.path.basename(image_path)
    base, _ = os.path.splitext(filename)
    rle_path = os.path.join(data_dir, base + ".rle")

    compress_image.compress_image_to_rle(image_path, rle_path)

    return pivot_x, pivot_y, width, height

if __name__ == '__main__':
    prefix = 'aviator'
    if len(sys.argv) >= 2:
        prefix = sys.argv[1]

    hour_name = f"{prefix}_hour.png"
    min_name = f"{prefix}_minute.png"
    sec_name = f"{prefix}_second.png"

    hands = [hour_name, min_name, sec_name]
    results = {}

    for hand in hands:
        res = process_hand_image(hand)
        if res:
            results[hand] = res

    if results:
        print(f"\n=== Copy the following code into your Faceplate init routine ===")
        print("void initWatchHandSprites() {")
        print(f"  Serial.println(\"[SPRITES] Allocating {prefix.capitalize()} watch hands and center canvas sprite...\");\n")
        
        # Hour Hand
        if hour_name in results:
            px, py, w, h = results[hour_name]
            print(f"  hourHandSprite.createSprite({w}, {h});")
            print(f"  hourHandSprite.fillSprite(COLOR_TRANSPARENT);")
            print(f"  drawFullRLEToSprite(hourHandSprite, \"/{prefix}_hour.rle\");")
            print(f"  hourHandSprite.setPivot({px}, {py});\n")

        # Minute Hand
        if min_name in results:
            px, py, w, h = results[min_name]
            print(f"  minuteHandSprite.createSprite({w}, {h});")
            print(f"  minuteHandSprite.fillSprite(COLOR_TRANSPARENT);")
            print(f"  drawFullRLEToSprite(minuteHandSprite, \"/{prefix}_minute.rle\");")
            print(f"  minuteHandSprite.setPivot({px}, {py});\n")

        # Second Hand
        if sec_name in results:
            px, py, w, h = results[sec_name]
            print(f"  secondHandSprite.createSprite({w}, {h});")
            print(f"  secondHandSprite.fillSprite(COLOR_TRANSPARENT);")
            print(f"  drawFullRLEToSprite(secondHandSprite, \"/{prefix}_second.rle\");")
            print(f"  secondHandSprite.setPivot({px}, {py});\n")

        print("  centerBgSprite.createSprite(160, 160);")
        print("  centerBgSprite.fillSprite(TFT_BLACK);")
        print("}")
