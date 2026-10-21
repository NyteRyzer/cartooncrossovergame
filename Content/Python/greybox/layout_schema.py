"""Greybox layout format: loading, expansion and validation.

Pure Python (no `unreal` import) so layouts can be checked in CI:

    python3 -I Content/Python/greybox/layout_schema.py Content/Python/greybox/layouts

A layout is JSON with:
  name, map_path, bounds{min,max}, player_start{location,yaw}
  blocks:   explicit primitives
  arrays:   generators ("ring" / "grid") expanded into blocks
  markers:  native actors to place (e.g. DimensionalTerminal, PlayerStart)

All units are Unreal centimetres. Block `location` is the shape's centre,
`size` is its full extent (engine basic shapes are 100 uu, so scale = size/100).
"""

import json
import math
import os
import random
import sys

SHAPES = ("cube", "cylinder", "sphere", "cone", "plane")
MARKER_CLASSES = ("DimensionalTerminal", "HoverCarPawn", "PlayerStart", "TargetPoint")


class LayoutError(ValueError):
    pass


def _vec(value, what, n=3):
    if not isinstance(value, (list, tuple)) or len(value) != n:
        raise LayoutError("%s must be a list of %d numbers, got %r" % (what, n, value))
    for v in value:
        if not isinstance(v, (int, float)) or isinstance(v, bool) or not math.isfinite(v):
            raise LayoutError("%s has a non-numeric component: %r" % (what, value))
    return [float(v) for v in value]


def load_layout(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def _expand_ring(gen):
    count = int(gen["count"])
    radius = float(gen["radius"])
    center = _vec(gen.get("center", [0, 0, 0]), "ring center")
    start = float(gen.get("start_angle", 0.0))
    names = gen.get("names") or []
    blocks = []
    for i in range(count):
        angle = math.radians(start + 360.0 * i / count)
        x = center[0] + radius * math.cos(angle)
        y = center[1] + radius * math.sin(angle)
        yaw = math.degrees(angle) + 180.0 if gen.get("face_center", True) else math.degrees(angle)
        suffix = names[i] if i < len(names) else "%02d" % i
        block = {k: v for k, v in gen.items() if k not in ("type", "count", "radius", "center", "start_angle", "names", "face_center", "label")}
        block.update({
            "label": "%s_%s" % (gen["label"], suffix),
            "location": [x, y, center[2]],
            "yaw": round(yaw % 360.0, 4),
        })
        blocks.append(block)
    return blocks


def _expand_grid(gen):
    nx, ny = (int(c) for c in gen["count"])
    sx, sy = _vec(gen["spacing"], "grid spacing", 2)
    origin = _vec(gen["origin"], "grid origin")
    size = _vec(gen["size"], "grid size")
    jitter = _vec(gen.get("size_jitter", [0, 0, 0]), "grid size_jitter")
    skip = {tuple(c) for c in gen.get("skip", [])}
    rng = random.Random(gen.get("seed", 0))
    blocks = []
    for ix in range(nx):
        for iy in range(ny):
            # Draw jitter even for skipped cells so skipping doesn't reshuffle the rest.
            j = [rng.uniform(-jitter[a], jitter[a]) for a in range(3)]
            if (ix, iy) in skip:
                continue
            s = [max(1.0, size[a] + j[a]) for a in range(3)]
            block = {k: v for k, v in gen.items() if k not in ("type", "count", "spacing", "origin", "size", "size_jitter", "seed", "skip", "label")}
            block.update({
                "label": "%s_%d_%d" % (gen["label"], ix, iy),
                # Grid cells sit on the origin's Z plane: centre is half the height above it.
                "location": [origin[0] + ix * sx, origin[1] + iy * sy, origin[2] + s[2] / 2.0],
                "size": s,
            })
            blocks.append(block)
    return blocks


def expand_layout(layout):
    """Return a flat list of block dicts (explicit blocks + expanded arrays)."""
    blocks = [dict(b) for b in layout.get("blocks", [])]
    for gen in layout.get("arrays", []):
        kind = gen.get("type")
        if kind == "ring":
            blocks.extend(_expand_ring(gen))
        elif kind == "grid":
            blocks.extend(_expand_grid(gen))
        else:
            raise LayoutError("array %r has unknown type %r" % (gen.get("label"), kind))
    return blocks


def expand_markers(layout):
    markers = [dict(m) for m in layout.get("markers", [])]
    for gen in layout.get("marker_arrays", []):
        if gen.get("type") != "ring":
            raise LayoutError("marker array %r must be a ring" % gen.get("label"))
        for m in _expand_ring(gen):
            markers.append(m)
    return markers


def _inside(point, bounds_min, bounds_max):
    return all(bounds_min[a] <= point[a] <= bounds_max[a] for a in range(3))


def _buried_in(point, block):
    """True if point is strictly inside a solid, axis-aligned block.

    Blocks rotated off 90-degree steps (or pitched) are skipped: this is a
    sanity check for spawn points, not a physics query.
    """
    if block.get("no_collision"):
        return False
    yaw = float(block.get("yaw", 0.0)) % 360.0
    if float(block.get("pitch", 0.0)) % 360.0 != 0.0 or yaw % 90.0 != 0.0:
        return False
    loc, size = block["location"], list(block["size"])
    if yaw in (90.0, 270.0):
        size[0], size[1] = size[1], size[0]
    return all(abs(point[a] - loc[a]) < size[a] / 2.0 for a in range(3))


def validate_layout(layout):
    """Raise LayoutError on the first problem; return (blocks, markers) on success."""
    for key in ("name", "map_path", "bounds", "player_start"):
        if key not in layout:
            raise LayoutError("missing top-level key %r" % key)
    if not str(layout["map_path"]).startswith("/Game/"):
        raise LayoutError("map_path must start with /Game/")
    bmin = _vec(layout["bounds"].get("min"), "bounds.min")
    bmax = _vec(layout["bounds"].get("max"), "bounds.max")
    if any(bmin[a] >= bmax[a] for a in range(3)):
        raise LayoutError("bounds.min must be below bounds.max on every axis")

    start = _vec(layout["player_start"].get("location"), "player_start.location")
    if not _inside(start, bmin, bmax):
        raise LayoutError("player_start is outside bounds")

    blocks = expand_layout(layout)
    markers = expand_markers(layout)
    if not blocks:
        raise LayoutError("layout has no blocks")

    labels = set()
    for b in blocks:
        label = b.get("label")
        if not label:
            raise LayoutError("block without a label: %r" % b)
        if label in labels:
            raise LayoutError("duplicate label %r" % label)
        labels.add(label)
        if b.get("shape", "cube") not in SHAPES:
            raise LayoutError("%s: unknown shape %r" % (label, b.get("shape")))
        loc = _vec(b.get("location"), "%s.location" % label)
        size = _vec(b.get("size"), "%s.size" % label)
        if any(s <= 0 for s in size):
            raise LayoutError("%s: size must be positive" % label)
        if not _inside(loc, bmin, bmax):
            raise LayoutError("%s: location %r outside bounds" % (label, loc))

    for m in markers:
        label = m.get("label")
        if not label or label in labels:
            raise LayoutError("marker label missing or duplicated: %r" % label)
        labels.add(label)
        if m.get("class") not in MARKER_CLASSES:
            raise LayoutError("%s: unknown marker class %r" % (label, m.get("class")))
        if not _inside(_vec(m.get("location"), "%s.location" % label), bmin, bmax):
            raise LayoutError("%s: outside bounds" % label)

    spawn_points = [("player_start", start)] + [(m["label"], m["location"]) for m in markers]
    for label, point in spawn_points:
        for b in blocks:
            if _buried_in(point, b):
                raise LayoutError("%s is inside solid block %s" % (label, b["label"]))

    return blocks, markers


def main(argv):
    if len(argv) != 2:
        print("usage: layout_schema.py <layouts_dir>")
        return 2
    folder = argv[1]
    failed = False
    for name in sorted(os.listdir(folder)):
        if not name.endswith(".json"):
            continue
        path = os.path.join(folder, name)
        try:
            blocks, markers = validate_layout(load_layout(path))
            print("ok   %-28s %4d blocks  %3d markers" % (name, len(blocks), len(markers)))
        except (LayoutError, KeyError, ValueError) as e:
            failed = True
            print("FAIL %-28s %s" % (name, e))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
