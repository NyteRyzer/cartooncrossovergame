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
#   feet: (x0, x1, y_top, y_floor, y_shoes) sheet coords; below y_top only
#         x0..x1 is kept, below y_shoes the turntable surface is removed (the
#         shoes aren't bluish), and nothing below y_floor (characters with blue legs)
SPRITES = {
    "jimmy":     (84, 59, 190, 196, {"ring": 0.22, "feet": (111, 153, 180, 194, 186)}),
    "timmy":     (440, 72, 566, 201, {"ring": 0.18, "feet": (483, 518, 183, 197, 188)}),
    "danny":     (720, 60, 842, 200, {"ring": 0.15}),
    "jake":      (40, 248, 150, 368, {"ring": 0.15, "feet": (76, 111, 354, 367, 358)}),
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
        fx0, fx1, fy_top, fy_floor, fy_shoes = opts["feet"]
        rows = np.arange(h)[:, None] + box[1]
        cols = np.arange(w)[None, :] + box[0]
        passable |= (rows >= fy_floor) | ((rows >= fy_top) & ((cols < fx0) | (cols > fx1)))
        passable |= (rows >= fy_shoes) & turntable_surface_like(crop)

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


# Cut-out puppet rigs. Coordinates are normalised to the trimmed sprite
# (u right, v down). Parts are taken in order, each claiming the sprite pixels
# inside its polygon that no earlier part claimed; whatever is left is "body".
# "a"/"b" = image-left / image-right side. z: draw order (body is 0).
def R(u0, v0, u1, v1):
    return [(u0, v0), (u1, v0), (u1, v1), (u0, v1)]

RIGS = {
    "jimmy": [
        ("arm_a", R(0.0, 0.55, 0.33, 0.70), (0.33, 0.615), 2),
        ("arm_b", R(0.66, 0.55, 1.0, 0.70), (0.66, 0.615), 2),
        ("leg_a", R(0.0, 0.80, 0.49, 1.0), (0.43, 0.80), -1),
        ("leg_b", R(0.49, 0.80, 1.0, 1.0), (0.56, 0.80), -1),
        ("head", R(0.0, 0.0, 1.0, 0.575), (0.49, 0.57), 3),
    ],
    "timmy": [
        ("arm_a", [(0.0, 0.42), (0.17, 0.42), (0.38, 0.64), (0.38, 0.80), (0.0, 0.80)], (0.37, 0.68), 2),
        ("arm_b", [(0.62, 0.45), (1.0, 0.45), (1.0, 0.82), (0.62, 0.82)], (0.61, 0.68), 2),
        ("leg_a", R(0.0, 0.79, 0.50, 1.0), (0.46, 0.79), -1),
        ("leg_b", R(0.50, 0.79, 1.0, 1.0), (0.54, 0.79), -1),
        ("head", R(0.0, 0.0, 1.0, 0.63), (0.50, 0.62), 3),
    ],
    "danny": [
        ("arm_a", R(0.0, 0.40, 0.36, 0.82), (0.37, 0.45), 2),
        ("arm_b", R(0.64, 0.40, 1.0, 0.82), (0.63, 0.45), 2),
        ("tail", R(0.0, 0.62, 1.0, 1.0), (0.50, 0.62), -1),
        ("head", R(0.0, 0.0, 1.0, 0.385), (0.50, 0.38), 3),
    ],
    "jake": [
        ("arm_a", [(0.0, 0.30), (0.32, 0.30), (0.34, 0.40), (0.25, 0.66), (0.0, 0.66)], (0.33, 0.33), 2),
        ("arm_b", [(0.68, 0.30), (1.0, 0.30), (1.0, 0.66), (0.75, 0.66), (0.66, 0.40)], (0.67, 0.33), 2),
        ("leg_a", R(0.0, 0.58, 0.50, 1.0), (0.45, 0.58), -1),
        ("leg_b", R(0.50, 0.58, 1.0, 1.0), (0.55, 0.58), -1),
        ("head", R(0.0, 0.0, 1.0, 0.275), (0.50, 0.27), 3),
    ],
    "xj9": [
        ("arm_a", [(0.0, 0.33), (0.30, 0.33), (0.34, 0.40), (0.25, 0.68), (0.0, 0.68)], (0.32, 0.36), 2),
        ("arm_b", [(0.70, 0.33), (1.0, 0.33), (1.0, 0.68), (0.75, 0.68), (0.66, 0.40)], (0.68, 0.36), 2),
        ("leg_a", R(0.0, 0.61, 0.50, 1.0), (0.42, 0.60), -1),
        ("leg_b", R(0.50, 0.61, 1.0, 1.0), (0.58, 0.60), -1),
        ("head", R(0.0, 0.0, 1.0, 0.335), (0.50, 0.33), 3),
    ],
    "eltigre": [
        ("arm_a", R(0.0, 0.50, 0.34, 0.90), (0.36, 0.58), 2),
        ("arm_b", R(0.63, 0.47, 1.0, 0.90), (0.62, 0.58), 2),
        ("leg_a", R(0.0, 0.82, 0.50, 1.0), (0.43, 0.80), -1),
        ("leg_b", R(0.50, 0.82, 1.0, 1.0), (0.57, 0.80), -1),
        ("head", R(0.0, 0.0, 1.0, 0.50), (0.50, 0.50), 3),
    ],
    "ben": [
        ("arm_a", [(0.0, 0.31), (0.30, 0.31), (0.33, 0.38), (0.26, 0.64), (0.0, 0.64)], (0.31, 0.34), 2),
        ("arm_b", [(0.69, 0.31), (1.0, 0.31), (1.0, 0.64), (0.74, 0.64), (0.67, 0.38)], (0.69, 0.34), 2),
        ("leg_a", R(0.0, 0.60, 0.50, 1.0), (0.42, 0.60), -1),
        ("leg_b", R(0.50, 0.60, 1.0, 1.0), (0.58, 0.60), -1),
        ("head", R(0.0, 0.0, 1.0, 0.305), (0.50, 0.30), 3),
    ],
    "aang": [
        ("leg_a", R(0.38, 0.66, 0.50, 1.0), (0.45, 0.66), -1),
        ("leg_b", R(0.50, 0.66, 0.64, 1.0), (0.55, 0.66), -1),
        ("staff", [(0.0, 0.36), (1.0, 0.60), (1.0, 0.76), (0.0, 0.52)], (0.50, 0.55), 2),
        ("head", R(0.30, 0.0, 0.70, 0.305), (0.50, 0.30), 3),
    ],
    "heatblast": [
        ("arm_a", [(0.0, 0.28), (0.32, 0.28), (0.35, 0.36), (0.28, 0.70), (0.0, 0.70)], (0.33, 0.33), 2),
        ("arm_b", [(0.66, 0.28), (1.0, 0.28), (1.0, 0.70), (0.72, 0.70), (0.64, 0.36)], (0.66, 0.33), 2),
        ("leg_a", R(0.0, 0.62, 0.50, 1.0), (0.42, 0.60), -1),
        ("leg_b", R(0.50, 0.62, 1.0, 1.0), (0.58, 0.60), -1),
        ("head", R(0.0, 0.0, 1.0, 0.285), (0.50, 0.28), 3),
    ],
    "fourarms": [
        ("arm_a", R(0.0, 0.22, 0.23, 0.95), (0.25, 0.33), 2),
        ("arm_b", R(0.77, 0.22, 1.0, 0.95), (0.75, 0.33), 2),
        ("leg_a", R(0.25, 0.82, 0.50, 1.0), (0.44, 0.80), -1),
        ("leg_b", R(0.50, 0.82, 0.75, 1.0), (0.56, 0.80), -1),
        ("head", R(0.30, 0.0, 0.70, 0.20), (0.50, 0.20), 3),
    ],
    "dragon": [
        ("wing", [(0.47, 0.0), (1.0, 0.0), (1.0, 0.50), (0.66, 0.50), (0.52, 0.42)], (0.55, 0.42), -1),
        ("head", [(0.0, 0.0), (0.46, 0.0), (0.46, 0.50), (0.0, 0.62)], (0.42, 0.48), 2),
        ("tail", [(0.66, 0.55), (1.0, 0.36), (1.0, 0.86), (0.73, 0.80)], (0.68, 0.70), -1),
    ],
}


# Face decals for the 3D models: (u0, v0, u1, v1) on the trimmed sprite.
FACES = {
    "jimmy": (0.32, 0.375, 0.66, 0.585),
    "timmy": (0.27, 0.24, 0.78, 0.64),
    "danny": (0.36, 0.15, 0.65, 0.33),
    "jake": (0.41, 0.045, 0.60, 0.255),
    "xj9": (0.31, 0.13, 0.70, 0.33),
    "eltigre": (0.30, 0.17, 0.84, 0.50),
    "ben": (0.38, 0.10, 0.62, 0.295),
    "aang": (0.43, 0.07, 0.575, 0.265),
    "heatblast": (0.41, 0.10, 0.60, 0.275),
    "fourarms": (0.43, 0.03, 0.57, 0.18),
}


def export_face(name, sprite):
    """Oval face decal (eyes, nose, mouth) for the 3D head."""
    u0, v0, u1, v1 = FACES[name]
    w, h = sprite.size
    box = (int(u0 * w), int(v0 * h), int(u1 * w), int(v1 * h))
    face = sprite.crop(box).resize(((box[2] - box[0]) * 2, (box[3] - box[1]) * 2), Image.LANCZOS)
    fw, fh = face.size
    yy, xx = np.mgrid[0:fh, 0:fw]
    oval = ((xx - fw / 2) / (fw / 2)) ** 2 + ((yy - fh / 2) / (fh / 2)) ** 2 <= 1.0
    rgba = np.asarray(face).copy()
    rgba[..., 3] = np.where(oval, rgba[..., 3], 0)
    os.makedirs(os.path.join(OUT_DIR, "faces"), exist_ok=True)
    Image.fromarray(rgba, "RGBA").save(os.path.join(OUT_DIR, "faces", name + ".png"))


def polygon_mask(poly, w, h):
    from PIL import ImageDraw
    m = Image.new("L", (w, h), 0)
    ImageDraw.Draw(m).polygon([(u * w, v * h) for u, v in poly], fill=255)
    return np.asarray(m) > 0


def export_rig(name, sprite):
    """Split a sprite into puppet parts; writes <name>/<part>.png + rig.txt."""
    rgba = np.asarray(sprite)
    h, w = rgba.shape[:2]
    alpha = rgba[..., 3] > 0
    claimed = np.zeros((h, w), bool)
    out_dir = os.path.join(OUT_DIR, name)
    os.makedirs(out_dir, exist_ok=True)
    lines = ["sprite %d %d" % (w, h)]
    joint_r = 0.035 * max(w, h)
    yy, xx = np.mgrid[0:h, 0:w]
    joints = np.zeros((h, w), bool)
    parts = []
    for part, poly, pivot, z in RIGS[name]:
        mask = alpha & polygon_mask(poly, w, h) & ~claimed
        claimed |= mask
        px, py = pivot[0] * w, pivot[1] * h
        joints |= (xx - px) ** 2 + (yy - py) ** 2 < joint_r ** 2
        parts.append((part, mask, (px, py), z))
    # The body keeps joint discs so rotating limbs never open a gap.
    body = alpha & (~claimed | joints)
    parts.append(("body", body, (w * 0.5, h), 0))
    for part, mask, (px, py), z in parts:
        ys, xs = np.where(mask)
        if len(xs) == 0:
            continue
        x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
        piece = rgba[y0:y1, x0:x1].copy()
        piece[..., 3] = np.where(mask[y0:y1, x0:x1], piece[..., 3], 0)
        Image.fromarray(piece, "RGBA").save(os.path.join(out_dir, part + ".png"))
        lines.append("part %s %d %d %d %d %.1f %.1f %d" % (part, x0, y0, x1 - x0, y1 - y0, px, py, z))
    with open(os.path.join(out_dir, "rig.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")


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
        if name in RIGS:
            export_rig(name, sprites[name])
        if name in FACES:
            export_face(name, sprites[name])
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
