"""Cuts character sprites out of the production-bible concept sheet.

    python3 Prototype/tools/make_sprites.py [--preview out.png]
    (needs numpy, scipy, pillow)

Each sprite is a crop box on Prototype/assets/concept/production_bible_sheet.jpg.
The sheet background is dark navy with grid lines; characters have black ink
outlines. Background pixels are told apart from ink by their blue tint, then
flood-filled inward from the crop border, so dark areas enclosed by outlines
(Danny's suit, Ben's stripe) survive. The turntable glow under each character
is removed by colour below the knees, thin label text/callout lines are opened
away, and the result is upscaled with a smoothed alpha edge.

Output: Prototype/assets/sprites/<name>.png (RGBA, feet at the bottom edge).
"""

import argparse
import os

import numpy as np
from PIL import Image, ImageFilter
from scipy import ndimage

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SHEET = os.path.join(ROOT, "Prototype", "assets", "concept", "production_bible_sheet.jpg")
OUT_DIR = os.path.join(ROOT, "Prototype", "assets", "sprites")
SCALE = 3

# name: (left, top, right, bottom, options)
#   ring: remove turntable glow in the bottom fraction of the box (0 = off)
#   keep: also keep extra components at least this fraction of the largest one
#   erase: sheet-space rectangles to treat as background (label text)
#   surface: also remove the turntable's dark top surface in the ring band
#   feet: (x0, x1, y_top, y_floor) sheet coords; below y_top only x0..x1 is
#         kept, and nothing below y_floor (for characters with blue legs)
SPRITES = {
    "jimmy":     (84, 59, 190, 196, {"ring": 0.22, "feet": (111, 153, 180, 194)}),
    "timmy":     (440, 72, 566, 201, {"ring": 0.18, "feet": (483, 518, 183, 197)}),
    "danny":     (720, 60, 842, 200, {"ring": 0.15}),
    "jake":      (40, 248, 150, 368, {"ring": 0.15, "feet": (76, 111, 354, 367)}),
    "xj9":       (463, 240, 550, 376, {"ring": 0.0, "keep": 0.03, "erase": [(460, 266, 482, 282), (450, 334, 487, 357)]}),
    "eltigre":   (795, 240, 888, 340, {"ring": 0.0}),
    "ben":       (54, 415, 131, 530, {"ring": 0.15, "surface": True}),
    "aang":      (432, 407, 602, 530, {"ring": 0.15, "surface": True}),
    "heatblast": (179, 402, 292, 512, {"ring": 0.14, "erase": [(254, 488, 293, 512)]}),
    "fourarms":  (300, 412, 410, 515, {"ring": 0.14, "erase": [(368, 490, 411, 516)]}),
    "dragon":    (154, 236, 358, 376, {"ring": 0.0, "keep": 0.02}),
    "fairies":   (568, 146, 663, 205, {"ring": 0.0, "keep": 0.2}),
}


def background_like(rgb):
    """Navy background: bluish and dark. Sampled background never exceeds
    ~60 in any channel, while dark denim folds start around 72."""
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    return ((b - r) >= 8) & (np.maximum.reduce([r, g, b]) < 66)


def turntable_like(rgb):
    """Bright cyan/blue glow of the turntable rings (denim is much darker)."""
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    lum = 0.3 * r + 0.59 * g + 0.11 * b
    return (b > r + 35) & (g > r + 10) & (b > 90) & (lum > 110)


def turntable_surface_like(rgb):
    """The turntable's dark-blue top surface (only for characters without blue legs)."""
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    return ((b - r) >= 8) & (np.maximum.reduce([r, g, b]) < 140)


def cut(img, box, opts):
    crop = np.asarray(img.crop(box[:4]), dtype=np.float32)
    h, w, _ = crop.shape
    passable = background_like(crop)
    for (el, et, er, eb) in opts.get("erase", []):
        passable[max(0, et - box[1]):max(0, eb - box[1]), max(0, el - box[0]):max(0, er - box[0])] = True
    ring = opts.get("ring", 0.0)
    if ring > 0:
        band = np.zeros((h, w), bool)
        band[int(h * (1 - ring)):, :] = True
        passable |= band & turntable_like(crop)
        if opts.get("surface", False):
            passable |= band & turntable_surface_like(crop)
    if "feet" in opts:
        fx0, fx1, fy_top, fy_floor = opts["feet"]
        rows = np.arange(h)[:, None] + box[1]
        cols = np.arange(w)[None, :] + box[0]
        passable |= (rows >= fy_floor) | ((rows >= fy_top) & ((cols < fx0) | (cols > fx1)))

    # Flood the background inward from every border pixel.
    seeds = np.zeros((h, w), bool)
    seeds[0, :] = seeds[-1, :] = seeds[:, 0] = seeds[:, -1] = True
    labels, _ = ndimage.label(passable)
    border_labels = np.unique(labels[seeds & passable])
    background = np.isin(labels, border_labels[border_labels > 0])
    fg = ~background

    # Drop thin text and callout lines, then keep the character itself.
    fg = ndimage.binary_opening(fg, structure=np.ones((3, 3)))
    labels, n = ndimage.label(fg)
    if n == 0:
        raise RuntimeError("nothing left after masking")
    sizes = ndimage.sum(fg, labels, range(1, n + 1))
    largest = sizes.max()
    keep = opts.get("keep", 0.0)
    kept = [i + 1 for i, s in enumerate(sizes) if s == largest or (keep > 0 and s >= largest * keep)]
    fg = np.isin(labels, kept)
    fg = ndimage.binary_fill_holes(fg) if opts.get("fill", False) else fg

    # Trim to content, upscale, smooth the alpha edge.
    ys, xs = np.where(fg)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    rgb = Image.fromarray(crop[y0:y1, x0:x1].astype(np.uint8), "RGB")
    alpha = Image.fromarray((fg[y0:y1, x0:x1] * 255).astype(np.uint8), "L")
    size = (rgb.width * SCALE, rgb.height * SCALE)
    rgb = rgb.resize(size, Image.LANCZOS).filter(ImageFilter.UnsharpMask(radius=2, percent=50))
    alpha = alpha.resize(size, Image.BILINEAR).filter(ImageFilter.GaussianBlur(1.2))
    alpha = alpha.point(lambda v: 0 if v < 100 else (255 if v > 170 else int((v - 100) * 255 / 70)))
    out = rgb.convert("RGBA")
    out.putalpha(alpha)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview", help="write a contact sheet of all sprites here")
    args = ap.parse_args()

    img = Image.open(SHEET).convert("RGB")
    os.makedirs(OUT_DIR, exist_ok=True)
    sprites = {}
    for name, (l, t, r, b, opts) in SPRITES.items():
        sprites[name] = cut(img, (l, t, r, b), opts)
        sprites[name].save(os.path.join(OUT_DIR, name + ".png"))
        print("%-10s %dx%d" % (name, sprites[name].width, sprites[name].height))

    if args.preview:
        cols, cell = 5, 330
        rows = (len(sprites) + cols - 1) // cols
        sheet = Image.new("RGBA", (cols * cell, rows * cell), (255, 0, 255, 255))
        for i, (name, s) in enumerate(sprites.items()):
            t = s.copy()
            t.thumbnail((cell - 10, cell - 10))
            sheet.alpha_composite(t, ((i % cols) * cell + 5, (i // cols) * cell + 5))
        sheet.convert("RGB").save(args.preview)


if __name__ == "__main__":
    main()
