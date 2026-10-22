"""Exports the greybox JSON layouts into the prototype's plain-text .lvl format.

Uses the same expansion/validation code as the Unreal greybox builder, so the
prototype always plays the exact blockout that would be generated in-editor.

    python3 -I Prototype/tools/export_levels.py <out_dir>
"""

import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "Content", "Python", "greybox"))

import layout_schema as ls  # noqa: E402

LAYOUT_DIR = os.path.join(ROOT, "Content", "Python", "greybox", "layouts")


def _num(v):
    return ("%.3f" % float(v)).rstrip("0").rstrip(".")


def _token(text):
    return str(text).replace(" ", "_") or "-"


def export(layout_path, out_path):
    layout = ls.load_layout(layout_path)
    blocks, markers = ls.validate_layout(layout)
    b = layout["bounds"]
    ps = layout["player_start"]
    lines = [
        "level %s" % _token(layout["name"]),
        "bounds %s" % " ".join(_num(v) for v in b["min"] + b["max"]),
        "start %s %s" % (" ".join(_num(v) for v in ps["location"]), _num(ps.get("yaw", 0))),
    ]
    for blk in blocks:
        lines.append("block %s %s %s %s %s %s %d %d %s" % (
            _token(blk["label"]), blk.get("shape", "cube"),
            " ".join(_num(v) for v in blk["location"]),
            " ".join(_num(v) for v in blk["size"]),
            _num(blk.get("yaw", 0)), _num(blk.get("pitch", 0)),
            0 if blk.get("no_collision") else 1,
            1 if blk.get("phase_blocker") else 0,
            _token(blk.get("folder", "Misc"))))
    for m in markers:
        lines.append("marker %s %s %s %s" % (
            _token(m["label"]), m["class"],
            " ".join(_num(v) for v in m["location"]), _num(m.get("yaw", 0))))
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    return len(blocks), len(markers)


def main(argv):
    if len(argv) != 2:
        print("usage: export_levels.py <out_dir>")
        return 2
    out_dir = argv[1]
    os.makedirs(out_dir, exist_ok=True)
    for name in sorted(os.listdir(LAYOUT_DIR)):
        if name.endswith(".json"):
            out = os.path.join(out_dir, name[:-5] + ".lvl")
            nb, nm = export(os.path.join(LAYOUT_DIR, name), out)
            print("exported %s (%d blocks, %d markers)" % (out, nb, nm))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
