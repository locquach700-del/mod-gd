from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter
import math

SIZE = 512
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "logo.png"

# Dark blue-violet gradient background.
base = Image.new("RGB", (SIZE, SIZE))
pixels = base.load()
for y in range(SIZE):
    for x in range(SIZE):
        dx = (x - SIZE * 0.5) / (SIZE * 0.72)
        dy = (y - SIZE * 0.46) / (SIZE * 0.72)
        glow = max(0.0, 1.0 - math.sqrt(dx * dx + dy * dy))
        pixels[x, y] = (
            int(8 + 13 * glow),
            int(12 + 18 * glow),
            int(34 + 40 * glow),
        )

# Soft cyan / violet halo behind the cube.
halo = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
hd = ImageDraw.Draw(halo)
hd.ellipse((90, 76, 424, 410), fill=(40, 225, 255, 105))
hd.ellipse((150, 130, 390, 380), fill=(170, 65, 255, 95))
halo = halo.filter(ImageFilter.GaussianBlur(54))
base = Image.alpha_composite(base.convert("RGBA"), halo)

# Build an original isometric cube from three gradient-filled polygons.
cube = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
draw = ImageDraw.Draw(cube)
top = [(256, 105), (390, 178), (256, 252), (122, 178)]
left = [(122, 178), (256, 252), (256, 410), (122, 335)]
right = [(256, 252), (390, 178), (390, 335), (256, 410)]

def gradient_polygon(points, top_color, bottom_color):
    mask = Image.new("L", (SIZE, SIZE), 0)
    ImageDraw.Draw(mask).polygon(points, fill=255)
    grad = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    gp = grad.load()
    for yy in range(SIZE):
        t = yy / (SIZE - 1)
        col = tuple(int(top_color[i] * (1-t) + bottom_color[i] * t) for i in range(4))
        for xx in range(SIZE):
            gp[xx, yy] = col
    grad.putalpha(ImageChops_multiply(grad.getchannel("A"), mask))
    return grad

def ImageChops_multiply(a, b):
    from PIL import ImageChops
    return ImageChops.multiply(a, b)

cube.alpha_composite(gradient_polygon(top, (85, 250, 255, 255), (50, 115, 245, 255)))
cube.alpha_composite(gradient_polygon(left, (49, 220, 255, 255), (24, 90, 205, 255)))
cube.alpha_composite(gradient_polygon(right, (195, 88, 255, 255), (85, 53, 200, 255)))

draw = ImageDraw.Draw(cube)
edge = (221, 250, 255, 255)
draw.line(top + [top[0]], fill=edge, width=10, joint="curve")
draw.line(left + [left[0]], fill=edge, width=10, joint="curve")
draw.line(right + [right[0]], fill=edge, width=10, joint="curve")
draw.line((256, 252, 256, 410), fill=(220, 245, 255, 235), width=7)
draw.line((122, 178, 256, 252, 390, 178), fill=(225, 250, 255, 210), width=7)

# Face details and small motion sparks.
draw.rounded_rectangle((189, 292, 218, 322), radius=7, fill=(7, 26, 66, 255))
draw.rounded_rectangle((294, 292, 323, 322), radius=7, fill=(7, 26, 66, 255))
draw.arc((222, 320, 292, 367), start=15, end=165, fill=(7, 26, 66, 255), width=8)

for cx, cy, r in [(82, 150, 8), (426, 140, 7), (95, 360, 6), (430, 365, 9), (255, 58, 5)]:
    draw.ellipse((cx-r, cy-r, cx+r, cy+r), fill=(180, 250, 255, 220))

base = Image.alpha_composite(base, cube)
base = base.convert("RGB").resize((512, 512), Image.Resampling.LANCZOS)
base.save(OUT, "PNG", optimize=True)
print(f"Generated {OUT} ({OUT.stat().st_size} bytes)")
