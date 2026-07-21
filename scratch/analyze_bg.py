from PIL import Image

img = Image.open(r'C:\Users\danme\.gemini\antigravity-ide\brain\fe9ba4f7-98fc-498e-91f8-7aecaf2f2d7b\media__1784634238167.jpg').convert('RGB')

# Top-Left Box (Time)
tl_ys = [y for y in range(130, 165) if any(sum(img.getpixel((x, y))) < 30 for x in range(35, 115))]
print(f"Top-Left Box Y range: [{min(tl_ys)}..{max(tl_ys)}] (height={max(tl_ys)-min(tl_ys)+1})")

# Top-Right Box (Date)
tr_ys = [y for y in range(130, 165) if any(sum(img.getpixel((x, y))) < 30 for x in range(130, 200))]
print(f"Top-Right Box Y range: [{min(tr_ys)}..{max(tr_ys)}] (height={max(tr_ys)-min(tr_ys)+1})")

# Bottom Box (Metric)
b_ys = [y for y in range(165, 190) if any(sum(img.getpixel((x, y))) < 30 for x in range(35, 205))]
print(f"Bottom Box Y range: [{min(b_ys)}..{max(b_ys)}] (height={max(b_ys)-min(b_ys)+1})")
