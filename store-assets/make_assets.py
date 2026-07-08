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

# Center "W" (Wikipedia) at the radar's heart, in a serif to echo the
# Wikipedia wordmark
def serif_font(size):
    for p in ('/System/Library/Fonts/Supplemental/Georgia Bold.ttf',
              '/System/Library/Fonts/Supplemental/Times New Roman Bold.ttf'):
        try:
            return ImageFont.truetype(p, size)
        except OSError:
            continue
    return font(size, bold=True)

d.text((cx, cy), 'W', font=serif_font(64), fill=(255, 255, 255),
       anchor='mm')

# Title + tagline
d.text((378, 96), 'WikiRadar', font=font(60, bold=True), fill=(255, 255, 255))
d.text((380, 176), 'Wikipedia, around you.', font=font(28),
       fill=(150, 175, 190))
d.text((380, 218), 'Nearby articles · compass · offline',
       font=font(20), fill=(90, 115, 130))

img.convert('RGB').save('store-assets/banner.png')

# ---- Icons: radar scope with sweep and blips --------------------------------

def radar_icon(size, corner, ring_w, on_cerulean=True):
    """Radar scope: range rings, crosshairs, sweep trail, and blips."""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    fg = (255, 255, 255, 255) if on_cerulean else (0, 0, 0, 255)
    faint = fg[:3] + (110,) if on_cerulean else fg
    if on_cerulean:
        d.rounded_rectangle([0, 0, size - 1, size - 1], radius=corner,
                            fill=CERULEAN)
    c = size / 2
    r = size * 0.38
    thin = max(ring_w // 2, 1)
    # Crosshairs, clipped to the scope
    d.line([c - r, c, c + r, c], fill=faint, width=thin)
    d.line([c, c - r, c, c + r], fill=faint, width=thin)
    # Range rings: inner ring(s) + outer ring on top
    d.ellipse([c - r * 0.55, c - r * 0.55, c + r * 0.55, c + r * 0.55],
              outline=faint, width=thin)
    if size >= 96:
        d.ellipse([c - r * 0.78, c - r * 0.78, c + r * 0.78, c + r * 0.78],
                  outline=faint, width=thin)
    d.ellipse([c - r, c - r, c + r, c + r], outline=fg, width=ring_w)
    # Sweep: fading trail wedge, thin leading edge (not a clock hand)
    if size >= 48 and on_cerulean:
        for spread, alpha in ((55, 55), (30, 100)):
            d.pieslice([c - r, c - r, c + r, c + r],
                       start=-60 - spread, end=-60,
                       fill=fg[:3] + (alpha,))
        edge = math.radians(-60)
        d.line([c, c, c + r * math.cos(edge), c + r * math.sin(edge)],
               fill=fg, width=thin)
    # Center dot
    cd = max(size // 26, 1)
    d.ellipse([c - cd, c - cd, c + cd, c + cd], fill=fg)
    # Blips: centered in the open annuli between rings (ring radii are
    # 0.21 / [0.30 at >=96px] / 0.38 of size), on diagonals clear of the
    # crosshair lines
    if size >= 96:
        # One inside the innermost ring, one per annulus
        blips = ((-140, 0.13), (35, 0.25), (115, 0.34))
    else:
        blips = ((-140, 0.13), (35, 0.28), (115, 0.28))
    for ang, dist in blips:
        a = math.radians(ang)
        bx = c + dist * size * math.cos(a)
        by = c + dist * size * math.sin(a)
        br = max(size // 22, 1)
        d.ellipse([bx - br, by - br, bx + br, by + br], fill=fg)
    return img

radar_icon(144, 30, 7).save('store-assets/icon-large.png')
radar_icon(80, 17, 4).save('store-assets/icon-small.png')
# Menu icon: black on transparent for the launcher (B&W friendly)
radar_icon(25, 0, 2, on_cerulean=False).save('resources/images/icon.png')
print('icons written')
