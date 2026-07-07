"""WikiRadar store assets: 720x320 banner + 144x144 large icon."""
import math
from PIL import Image, ImageDraw, ImageFont

CERULEAN = (0, 170, 255)
BG = (8, 20, 30)
RING = (28, 62, 78)

def font(size, bold=False):
    candidates = [
        '/System/Library/Fonts/Supplemental/Arial Bold.ttf' if bold else
        '/System/Library/Fonts/Supplemental/Arial.ttf',
        '/System/Library/Fonts/Helvetica.ttc',
    ]
    for path in candidates:
        try:
            return ImageFont.truetype(path, size)
        except OSError:
            continue
    return ImageFont.load_default()

# ---- Banner ----------------------------------------------------------------
W, H = 720, 320
img = Image.new('RGBA', (W, H), BG + (255,))
d = ImageDraw.Draw(img)

cx, cy = 200, 160

# Radar rings
for r in (45, 90, 135, 180):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=RING, width=2)
# Crosshairs
d.line([cx - 185, cy, cx + 185, cy], fill=RING, width=1)
d.line([cx, cy - 160, cx, cy + 160], fill=RING, width=1)

# Sweep wedge (three fading layers), pointing up-right
overlay = Image.new('RGBA', (W, H), (0, 0, 0, 0))
od = ImageDraw.Draw(overlay)
for spread, alpha in ((34, 28), (22, 46), (10, 70)):
    od.pieslice([cx - 180, cy - 180, cx + 180, cy + 180],
                start=-60 - spread, end=-60, fill=CERULEAN + (alpha,))
img = Image.alpha_composite(img, overlay)
d = ImageDraw.Draw(img)

# Sweep leading edge
edge = math.radians(-60)
d.line([cx, cy, cx + 180 * math.cos(edge), cy + 180 * math.sin(edge)],
       fill=CERULEAN, width=2)

# Blips (articles on the radar)
for bx, by, r in ((265, 95, 6), (150, 210, 5), (110, 120, 4)):
    d.ellipse([bx - r, by - r, bx + r, by + r], fill=CERULEAN)
    d.ellipse([bx - r - 4, by - r - 4, bx + r + 4, by + r + 4],
              outline=CERULEAN + (90,), width=2)

# Center needle (the app glyph)
d.polygon([(cx + 16, cy - 22), (cx - 8, cy + 20), (cx - 2, cy - 1),
           (cx - 24, cy - 4)], fill=(255, 255, 255))

# Title + tagline
d.text((378, 96), 'WikiRadar', font=font(60, bold=True), fill=(255, 255, 255))
d.text((380, 176), 'Wikipedia, around you.', font=font(28),
       fill=(150, 175, 190))
d.text((380, 218), 'Nearby articles · compass · offline',
       font=font(20), fill=(90, 115, 130))

img.convert('RGB').save('store-assets/banner.png')

# ---- Large icon (144x144) ---------------------------------------------------
icon = Image.new('RGBA', (144, 144), (0, 0, 0, 0))
di = ImageDraw.Draw(icon)
di.rounded_rectangle([0, 0, 143, 143], radius=30, fill=CERULEAN)
di.ellipse([22, 22, 121, 121], outline=(255, 255, 255), width=8)
di.polygon([(94, 42), (62, 100), (69, 71), (40, 74)], fill=(255, 255, 255))
icon.save('store-assets/icon-large.png')
print('assets written')
