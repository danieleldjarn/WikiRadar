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

# ---- Icons: radar scope with sweep and blips --------------------------------

def radar_icon(size, corner, ring_w, on_cerulean=True):
    """Radar scope: ring, sweep wedge with leading edge, and blips."""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    fg = (255, 255, 255, 255) if on_cerulean else (0, 0, 0, 255)
    if on_cerulean:
        d.rounded_rectangle([0, 0, size - 1, size - 1], radius=corner,
                            fill=CERULEAN)
    c = size / 2
    r = size * 0.36
    d.ellipse([c - r, c - r, c + r, c + r], outline=fg, width=ring_w)
    # Sweep wedge, pointing up-right, fading trail
    if size >= 48:
        for spread, alpha in ((50, 60), (28, 110)):
            d.pieslice([c - r, c - r, c + r, c + r],
                       start=-60 - spread, end=-60,
                       fill=fg[:3] + (alpha,))
    # Leading edge of the sweep
    edge = math.radians(-60)
    d.line([c, c, c + r * math.cos(edge), c + r * math.sin(edge)],
           fill=fg, width=max(ring_w - 1, 2))
    # Center dot
    cd = max(size // 24, 1)
    d.ellipse([c - cd, c - cd, c + cd, c + cd], fill=fg)
    # Blips
    for fx, fy in ((-0.16, -0.14), (0.13, 0.17)):
        bx, by = c + fx * size, c + fy * size
        br = max(size // 20, 1)
        d.ellipse([bx - br, by - br, bx + br, by + br], fill=fg)
    return img

radar_icon(144, 30, 7).save('store-assets/icon-large.png')
radar_icon(48, 10, 3).save('store-assets/icon-small.png')
# Menu icon: black on transparent for the launcher (B&W friendly)
radar_icon(25, 0, 2, on_cerulean=False).save('resources/images/icon.png')
print('icons written')
