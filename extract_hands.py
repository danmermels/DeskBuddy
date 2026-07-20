from PIL import Image
import numpy as np

# Load original image
img = Image.open('FaceplateAviatorwithOrange.png').convert('RGBA')
w, h = img.size
center = (120, 120)

# Rotate helper
def rotate_image(image, angle, center):
    # PIL rotate rotates counter-clockwise.
    # To rotate clockwise by angle: use -angle.
    # center is specified as a tuple.
    return image.rotate(-angle, resample=Image.BICUBIC, center=center)

# 1. Extract Hour Hand
# Rotate clockwise by 284.5 degrees to make it point straight up (0 degrees / 12 o'clock)
# That means we want to rotate by -284.5 degrees counter-clockwise
rot_hour = rotate_image(img, 284.5, center)
# Crop 18x62 around center X=120, Y=58..120
# Box: (left, upper, right, lower)
hour_hand = rot_hour.crop((111, 58, 129, 120))

# 2. Extract Minute Hand
# Rotate clockwise by 174.0 degrees to make it point straight up (0 degrees / 12 o'clock)
rot_min = rotate_image(img, 174.0, center)
# Crop 18x92 around center X=120, Y=28..120
min_hand = rot_min.crop((111, 28, 129, 120))

# 3. Extract Second Hand (Orange)
# Isolate orange pixels (R is high, G/B are low-ish, or specific orange hue)
arr = np.array(img)
r, g, b, a = arr[:,:,0], arr[:,:,1], arr[:,:,2], arr[:,:,3]
# Orange mask: R > 180, G is between 60 and 140, B < 80
orange_mask = (r > 180) & (g > 60) & (g < 140) & (b < 80)

# Create pure orange image with alpha from mask
orange_arr = np.zeros_like(arr)
orange_arr[:, :, 0] = 235 # R
orange_arr[:, :, 1] = 94  # G
orange_arr[:, :, 2] = 40  # B
orange_arr[:, :, 3] = np.where(orange_mask, 255, 0).astype(np.uint8)

orange_img = Image.fromarray(orange_arr, 'RGBA')
rot_sec = rotate_image(orange_img, 174.0, center)
# Crop 8x95 around X=120. Crop X=116..124, Y=25..120
sec_hand = rot_sec.crop((116, 25, 124, 120))

# Save cropped hand images
hour_hand.save('hour_hand.png')
min_hand.save('min_hand.png')
sec_hand.save('sec_hand.png')

print("Hands extracted successfully with PIL:")
print("hour_hand.png size:", hour_hand.size)
print("min_hand.png size:", min_hand.size)
print("sec_hand.png size:", sec_hand.size)
