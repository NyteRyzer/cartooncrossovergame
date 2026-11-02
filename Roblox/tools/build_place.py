#!/usr/bin/env python3
"""Builds the Roblox place file (Builds/MultiverseToons.rbxlx) for Studio.

Inputs:
  Roblox/world/*.world  - every level's blocks, set dressing and markers,
                          exported from the prototype with
                          `MultiverseToons --export-world Roblox/world`
  Roblox/src/**         - the Luau game code, laid out like a Rojo project
                          (default.project.json maps the same folders)

The worlds are baked into Workspace as parts (so they show in Studio's edit
view), the code becomes Script/LocalScript/ModuleScript instances, and
Shared/MapData is generated with spawn points, terminals, rifts, cars and
course gates in Roblox coordinates.

Unreal/prototype space is centimetres, Z up, X forward, Y right; Roblox is
studs, Y up. 1 stud = 30 cm and (x, y, z)_ue -> (x, z, y) / 30, which also
flips handedness correctly. A UE yaw of psi is a Roblox Y rotation of -psi.

  python3 Roblox/tools/build_place.py [out.rbxlx]
"""

import math
import os
import random
import sys
from xml.sax.saxutils import escape

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ROBLOX = os.path.join(ROOT, "Roblox")
CM = 30.0
MAX_PART = 2000.0  # Roblox parts are limited to 2048 studs per axis

LEVELS = [
    # world file stem, Workspace folder, display name, offset (studs), blurb
    ("nexus_observatory", "NexusObservatory", "Nexus Observatory", (0.0, 0.0, 0.0),
     "Pick a hero at the terminals, fly the ring challenge, or take Jimmy's hover car for a spin."),
    ("retroville_slice", "Retroville", "Retroville", (0.0, 0.0, 8000.0),
     "Close the three dimensional rifts, race the hover-car loop, find the rift shards. P returns to the Nexus."),
]

TERMINAL_HEROES = {
    "Retroville": ("Jimmy", "Retroville"), "Dimmsdale": ("Timmy", "Dimmsdale"), "AmityPark": ("Danny", "Amity Park"),
    "Chinatown": ("Jake", "NYC Chinatown"), "Tremorton": ("XJ9", "Tremorton"), "MiracleCity": ("ElTigre", "Miracle City"),
    "Bellwood": ("Ben", "Bellwood"), "AirTemple": ("Aang", "Air Temple"),
}

MATERIAL = {"SmoothPlastic": 272, "Neon": 288, "Glass": 1568, "Slate": 800}

# --------------------------------------------------------------------------
# Small math helpers (rotation matrices as row-major 3x3 lists)
# --------------------------------------------------------------------------

def mat_mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def rot_x(t):
    c, s = math.cos(t), math.sin(t)
    return [[1, 0, 0], [0, c, -s], [0, s, c]]


def rot_y(t):
    c, s = math.cos(t), math.sin(t)
    return [[c, 0, s], [0, 1, 0], [-s, 0, c]]


def rot_z(t):
    c, s = math.cos(t), math.sin(t)
    return [[c, -s, 0], [s, c, 0], [0, 0, 1]]


IDENTITY = [[1, 0, 0], [0, 1, 0], [0, 0, 1]]


class CF:
    """A CFrame: position + rotation (columns = right, up, back)."""

    def __init__(self, pos=(0.0, 0.0, 0.0), rot=None):
        self.p = tuple(pos)
        self.r = rot or IDENTITY

    def __mul__(self, other):
        rp = [sum(self.r[i][k] * other.p[k] for k in range(3)) for i in range(3)]
        return CF((self.p[0] + rp[0], self.p[1] + rp[1], self.p[2] + rp[2]), mat_mul(self.r, other.r))


def T(x, y, z):
    return CF((x, y, z))


def R(m):
    return CF((0, 0, 0), m)


def ue_to_studs(x, y, z, offset):
    return (x / CM + offset[0], z / CM + offset[1], y / CM + offset[2])


def roblox_yaw(ue_yaw_deg):
    """Facing yaw (deg) for CFrame.Angles(0, yaw, 0) so -Z points along the UE yaw."""
    return -90.0 - ue_yaw_deg


# --------------------------------------------------------------------------
# XML writer
# --------------------------------------------------------------------------

class Writer:
    def __init__(self):
        self.out = []
        self.ref = 0

    def open(self, cls, props):
        self.ref += 1
        self.out.append('<Item class="%s" referent="RBX%d"><Properties>' % (cls, self.ref))
        for p in props:
            self.out.append(p)
        self.out.append("</Properties>")

    def close(self):
        self.out.append("</Item>")

    def leaf(self, cls, props):
        self.open(cls, props)
        self.close()

    def text(self):
        return ('<roblox xmlns:xmime="http://www.w3.org/2005/05/xmlmime" '
                'xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" '
                'xsi:noNamespaceSchemaLocation="http://www.roblox.com/roblox.xsd" version="4">\n'
                + "\n".join(self.out) + "\n</roblox>\n")


def p_string(name, v):
    return '<string name="%s">%s</string>' % (name, escape(v))


def p_bool(name, v):
    return '<bool name="%s">%s</bool>' % (name, "true" if v else "false")


def p_float(name, v):
    return '<float name="%s">%.4f</float>' % (name, v)


def p_int(name, v):
    return '<int name="%s">%d</int>' % (name, v)


def p_token(name, v):
    return '<token name="%s">%d</token>' % (name, v)


def p_vec(name, v):
    return '<Vector3 name="%s"><X>%.4f</X><Y>%.4f</Y><Z>%.4f</Z></Vector3>' % (name, v[0], v[1], v[2])


def p_color3(name, c):
    return '<Color3 name="%s"><R>%.4f</R><G>%.4f</G><B>%.4f</B></Color3>' % (name, c[0], c[1], c[2])


def p_color(c):
    r, g, b = (max(0, min(255, int(v))) for v in c)
    return '<Color3uint8 name="Color3uint8">%d</Color3uint8>' % (0xFF000000 | (r << 16) | (g << 8) | b)


def p_cframe(name, cf):
    r = cf.r
    return ('<CoordinateFrame name="%s"><X>%.4f</X><Y>%.4f</Y><Z>%.4f</Z>'
            '<R00>%.6f</R00><R01>%.6f</R01><R02>%.6f</R02>'
            '<R10>%.6f</R10><R11>%.6f</R11><R12>%.6f</R12>'
            '<R20>%.6f</R20><R21>%.6f</R21><R22>%.6f</R22></CoordinateFrame>') % (
        name, cf.p[0], cf.p[1], cf.p[2], r[0][0], r[0][1], r[0][2], r[1][0], r[1][1], r[1][2], r[2][0], r[2][1], r[2][2])


def p_source(src):
    if "]]>" in src:
        raise ValueError("script contains ']]>'")
    return '<ProtectedString name="Source"><![CDATA[%s]]></ProtectedString>' % src


# --------------------------------------------------------------------------
# Parts
# --------------------------------------------------------------------------

class PartStats:
    count = 0


def part(w, name, cf, size, color, cls="Part", shape=None, material="SmoothPlastic", transparency=0.0,
         collide=True, query=True, shadow=True, mesh=None, children=None):
    size = tuple(max(0.05, s) for s in size)
    props = [
        p_string("Name", name), p_bool("Anchored", True), p_bool("CanCollide", collide), p_bool("CanQuery", query),
        p_bool("CanTouch", False), p_bool("CastShadow", shadow), p_bool("Locked", False), p_cframe("CFrame", cf),
        p_vec("size", size), p_color(color), p_token("Material", MATERIAL[material]), p_float("Transparency", transparency),
        p_token("TopSurface", 0), p_token("BottomSurface", 0),
    ]
    if shape is not None:
        props.append(p_token("shape", shape))
    w.open(cls, props)
    if mesh:
        w.leaf("SpecialMesh", [p_string("Name", "Mesh"), p_token("MeshType", mesh), p_vec("Scale", (1, 1, 1))])
    if children:
        children()
    w.close()
    PartStats.count += 1


def cone_parts(w, name, cf, height, radius, color, **kw):
    """Cone along cf's +Y from its centre: three tapering cylinders + a tip."""
    tiers = 3
    for i in range(tiers):
        r = radius * (1 - i / tiers)
        h = height / tiers
        y = -height / 2 + h * (i + 0.5)
        part(w, name, cf * T(0, y, 0) * R(rot_z(math.pi / 2)), (h, r * 2, r * 2), color, shape=2, **kw)


def gable_roof(w, name, cf, a, b, h, color, **kw):
    """Gable roof on footprint a (local X) by b (local Z), height h, base at cf."""
    frame = cf
    if b > a:
        frame = cf * R(rot_y(math.pi / 2))
        a, b = b, a
    for side in (-1, 1):
        rot = IDENTITY if side < 0 else rot_y(math.pi)
        part(w, name, frame * T(0, h / 2, side * b / 4) * R(rot), (a, h, b / 2), color, cls="WedgePart", **kw)


def ring_parts(w, name, cf, axis_u, axis_v, radius, color, segments=16, thick=1.4, material="Neon", transparency=0.0):
    """A ring of boxes in the plane spanned by unit local axes u, v (tuples) of cf."""
    for i in range(segments):
        a = 2 * math.pi * i / segments
        off = tuple(axis_u[k] * math.cos(a) * radius + axis_v[k] * math.sin(a) * radius for k in range(3))
        seg_len = 2 * math.pi * radius / segments * 1.15
        # orient each segment's local Z along the tangent
        tangent = tuple(-axis_u[k] * math.sin(a) + axis_v[k] * math.cos(a) for k in range(3))
        normal = tuple(axis_u[k] * math.cos(a) + axis_v[k] * math.sin(a) for k in range(3))
        up = (normal[1] * tangent[2] - normal[2] * tangent[1], normal[2] * tangent[0] - normal[0] * tangent[2],
              normal[0] * tangent[1] - normal[1] * tangent[0])
        rot = [[normal[0], up[0], tangent[0]], [normal[1], up[1], tangent[1]], [normal[2], up[2], tangent[2]]]
        part(w, name, cf * T(*off) * R(rot), (thick, thick, seg_len), color, material=material,
             transparency=transparency, collide=False, query=False, shadow=False)


# --------------------------------------------------------------------------
# Worlds
# --------------------------------------------------------------------------

def load_world(path):
    data = {"blocks": [], "markers": [], "start": None, "bounds": None, "name": ""}
    with open(path) as f:
        for line in f:
            t = line.split()
            if not t:
                continue
            if t[0] == "level":
                data["name"] = " ".join(t[1:])
            elif t[0] == "start":
                data["start"] = tuple(float(v) for v in t[1:5])
            elif t[0] == "bounds":
                data["bounds"] = tuple(float(v) for v in t[1:7])
            elif t[0] == "marker":
                data["markers"].append({"cls": t[1], "pos": tuple(float(v) for v in t[2:5]), "yaw": float(t[5]), "label": t[6]})
            elif t[0] == "block":
                v = t[1:]
                data["blocks"].append({
                    "kind": int(v[0]), "center": tuple(float(x) for x in v[1:4]), "size": tuple(float(x) for x in v[4:7]),
                    "yaw": float(v[7]), "pitch": float(v[8]), "color": tuple(int(x) for x in v[9:12]), "alpha": int(v[12]),
                    "emissive": float(v[13]), "solid": v[14] == "1", "roof": float(v[15]), "roof_color": tuple(int(x) for x in v[16:19]),
                    "flags": v[19], "label": v[20] if v[20] != "-" else "Block",
                })
    return data


def block_frame(b, offset):
    pos = ue_to_studs(*b["center"], offset)
    rot = mat_mul(rot_y(math.radians(-b["yaw"])), rot_z(math.radians(b["pitch"])))
    return CF(pos, rot)


def write_block(w, b, offset):
    sx, sy, sz = b["size"]
    a, h, d = sx / CM, sz / CM, sy / CM  # Roblox local X, Y, Z extents
    cf = block_frame(b, offset)
    color = b["color"]
    label = b["label"]
    flags = b["flags"]
    neon = b["emissive"] >= 0.5
    material = "Neon" if neon else "SmoothPlastic"
    transparency = 1.0 - b["alpha"] / 255.0
    solid = b["solid"]
    big = max(a, h, d) > 3.0
    kw = dict(material=material, transparency=transparency, collide=solid, query=solid or big, shadow=big and transparency < 0.5)

    if "h" in flags:
        if label == "Retroland_FerrisWheel":
            write_ferris_wheel(w, cf, a, h, d)
        return
    if "g" in flags:
        # Course gate: an invisible marker part (what the server measures) holding a glowing ring.
        dims = [a, h, d]
        thin = dims.index(min(dims))
        axes = [(1, 0, 0), (0, 1, 0), (0, 0, 1)]
        plane = [axes[i] for i in range(3) if i != thin]
        radius = max(dims) * 0.45

        def ring():
            ring_parts(w, label + "_Ring", cf, plane[0], plane[1], radius, (120, 255, 210), segments=20, thick=1.2)

        part(w, label, cf, (a, h, d), color, transparency=1.0, collide=False, query=False, shadow=False, children=ring)
        return
    if label.startswith("Boundary"):
        kw.update(material="SmoothPlastic", transparency=0.97, shadow=False, query=False)

    kind = b["kind"]
    if kind in (0, 4):  # cube, plane
        if kind == 4:
            h = 0.1
        tiles_x = max(1, math.ceil(a / MAX_PART))
        tiles_z = max(1, math.ceil(d / MAX_PART))
        for ix in range(tiles_x):
            for iz in range(tiles_z):
                ox = (ix + 0.5) * a / tiles_x - a / 2
                oz = (iz + 0.5) * d / tiles_z - d / 2
                part(w, label, cf * T(ox, 0, oz), (a / tiles_x, min(h, MAX_PART), d / tiles_z), color, **kw)
    elif kind == 1:  # vertical cylinder
        diameter = (a + d) / 2
        part(w, label, cf * R(rot_z(math.pi / 2)), (h, diameter, diameter), color, shape=2, **kw)
    elif kind == 2:  # sphere / ellipsoid
        if abs(a - h) < 0.01 and abs(a - d) < 0.01:
            part(w, label, cf, (a, a, a), color, shape=0, **kw)
        else:
            part(w, label, cf, (a, h, d), color, mesh=3, **kw)
    elif kind == 3:  # cone
        cone_parts(w, label, cf, h, (a + d) / 4, color, **kw)
    elif kind == 5:  # pyramid -> gable
        gable_roof(w, label, cf * T(0, -h / 2, 0), a, d, h, color, **kw)

    if b["roof"] > 0:
        rh = b["roof"] / CM
        gable_roof(w, label + "_Roof", cf * T(0, h / 2, 0), a * 1.12, d * 1.12, rh, b["roof_color"],
                   material="SmoothPlastic", collide=True, query=True, shadow=True)


def write_ferris_wheel(w, cf, a, h, d):
    radius = max(a, h, d) * 0.5
    # The wheel turns in the plane of local Y/Z (as in the prototype).
    ring_parts(w, "FerrisWheel_Rim", cf, (0, 1, 0), (0, 0, 1), radius, (255, 90, 100), segments=24, thick=2, material="SmoothPlastic")
    colors = [(120, 200, 255), (255, 220, 80), (140, 230, 140)]
    for i in range(12):
        ang = 2 * math.pi * i / 12
        y, z = math.cos(ang) * radius, math.sin(ang) * radius
        part(w, "FerrisWheel_Spoke", cf * T(0, y / 2, z / 2) * R(rot_x(-ang)), (1, radius, 1), (250, 250, 250), collide=False)
        part(w, "FerrisWheel_Gondola", cf * T(4, y - 7, z), (7, 6.5, 6), colors[i % 3], collide=False)
    part(w, "FerrisWheel_Hub", cf * R(rot_z(0)), (17, 8.5, 8.5), (220, 220, 230), shape=2)
    for side in (-1, 1):
        part(w, "FerrisWheel_Leg", cf * T(5, -radius / 2, side * radius * 0.45) * R(rot_x(side * math.radians(20))),
             (2, radius + 4, 2), (200, 200, 210))


def write_stars(w):
    rng = random.Random(99)
    for i in range(220):
        yaw = rng.random() * 2 * math.pi
        pitch = math.asin(rng.uniform(-1, 1))
        r = 1600
        pos = (math.cos(pitch) * math.cos(yaw) * r, math.sin(pitch) * r, math.cos(pitch) * math.sin(yaw) * r)
        s = rng.uniform(2, 5)
        part(w, "Star", T(*pos), (s, s, s), (255, 250, 230), shape=0, material="Neon", collide=False, query=False, shadow=False)


def level_mapdata(stem, folder, display, offset, blurb, data):
    sx, sy, sz, syaw = data["start"]
    start = ue_to_studs(sx, sy, sz, offset)
    b = data["bounds"]
    lo = ue_to_studs(b[0], b[1], b[2], offset)
    hi = ue_to_studs(b[3], b[4], b[5], offset)
    mn = tuple(min(lo[i], hi[i]) for i in range(3))
    mx = tuple(max(lo[i], hi[i]) for i in range(3))
    terminals, cars, rifts = [], [], []
    for m in data["markers"]:
        pos = ue_to_studs(*m["pos"], offset)
        if m["cls"] == "DimensionalTerminal":
            key = m["label"].split("_", 1)[1]
            hero, universe = TERMINAL_HEROES.get(key, ("Jimmy", key))
            terminals.append({"Name": m["label"], "Pos": pos, "Hero": hero, "Universe": universe,
                              "Travel": "Retroville" if key == "Retroville" else None})
        elif m["cls"] == "HoverCarPawn":
            cars.append({"Pos": pos, "Yaw": roblox_yaw(m["yaw"])})
        elif m["cls"] == "TargetPoint" and m["label"].startswith("Rift_"):
            rifts.append({"Name": m["label"], "Pos": pos})
    if folder == "NexusObservatory":
        gates = ["FlightGate_%02d" % i for i in range(12)]
        course = ("Flight Ring Challenge", gates)
    else:
        gates = (["RaceGate_S_%d_0" % i for i in range(8)] + ["RaceGate_E_0_%d" % i for i in range(8)]
                 + ["RaceGate_N_%d_0" % i for i in range(7, -1, -1)] + ["RaceGate_W_0_%d" % i for i in range(7, -1, -1)])
        course = ("Hover Race", gates)
    labels = {bl["label"] for bl in data["blocks"]}
    course = (course[0], [g for g in course[1] if g in labels])

    def v3(p):
        return "Vector3.new(%.2f, %.2f, %.2f)" % p

    lines = ['\t%s = {' % folder,
             '\t\tName = "%s",' % display,
             '\t\tBlurb = "%s",' % blurb,
             '\t\tStart = %s,' % v3(start),
             '\t\tStartYaw = %.1f,' % roblox_yaw(syaw),
             '\t\tBounds = { Min = %s, Max = %s },' % (v3(mn), v3(mx)),
             '\t\tTerminals = {']
    for t in terminals:
        travel = '"%s"' % t["Travel"] if t["Travel"] else "nil"
        lines.append('\t\t\t{ Name = "%s", Pos = %s, Hero = "%s", Universe = "%s", Travel = %s },' % (
            t["Name"], v3(t["Pos"]), t["Hero"], t["Universe"], travel))
    lines.append('\t\t},')
    lines.append('\t\tHoverCars = {')
    for c in cars:
        lines.append('\t\t\t{ Pos = %s, Yaw = %.1f },' % (v3(c["Pos"]), c["Yaw"]))
    lines.append('\t\t},')
    lines.append('\t\tRifts = {')
    for r in rifts:
        lines.append('\t\t\t{ Name = "%s", Pos = %s },' % (r["Name"], v3(r["Pos"])))
    lines.append('\t\t},')
    lines.append('\t\tCourse = { Name = "%s", Gates = { %s } },' % (course[0], ", ".join('"%s"' % g for g in course[1])))
    lines.append('\t},')
    return "\n".join(lines), start, roblox_yaw(syaw)


# --------------------------------------------------------------------------
# Scripts
# --------------------------------------------------------------------------

def script_item(w, path, name, cls):
    with open(path) as f:
        src = f.read()
    props = [p_string("Name", name), p_source(src)]
    if cls == "Script":
        props.append(p_token("RunContext", 0))
    w.leaf(cls, props)


def write_tree(w, directory):
    """Rojo-style: *.luau = ModuleScript, *.server.luau = Script, *.client.luau = LocalScript, dirs = Folders."""
    for entry in sorted(os.listdir(directory)):
        path = os.path.join(directory, entry)
        if os.path.isdir(path):
            w.open("Folder", [p_string("Name", entry)])
            write_tree(w, path)
            w.close()
        elif entry.endswith(".server.luau"):
            script_item(w, path, entry[: -len(".server.luau")], "Script")
        elif entry.endswith(".client.luau"):
            script_item(w, path, entry[: -len(".client.luau")], "LocalScript")
        elif entry.endswith(".luau"):
            script_item(w, path, entry[: -len(".luau")], "ModuleScript")


def write_init_container(w, directory, name, init_file, cls):
    """A folder whose init script becomes the container (Rojo init.*.luau)."""
    with open(os.path.join(directory, init_file)) as f:
        src = f.read()
    props = [p_string("Name", name), p_source(src)]
    if cls == "Script":
        props.append(p_token("RunContext", 0))
    w.open(cls, props)
    for entry in sorted(os.listdir(directory)):
        path = os.path.join(directory, entry)
        if entry == init_file:
            continue
        if os.path.isdir(path):
            w.open("Folder", [p_string("Name", entry)])
            write_tree(w, path)
            w.close()
        elif entry.endswith(".luau"):
            script_item(w, path, entry[: -len(".luau")], "ModuleScript")
    w.close()


# --------------------------------------------------------------------------
# Main
# --------------------------------------------------------------------------

def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "Builds", "MultiverseToons.rbxlx")
    w = Writer()
    levels = []
    mapdata_parts = []
    nexus_start = None
    for stem, folder, display, offset, blurb in LEVELS:
        data = load_world(os.path.join(ROBLOX, "world", stem + ".world"))
        levels.append((folder, offset, data))
        text, start, yaw = level_mapdata(stem, folder, display, offset, blurb, data)
        mapdata_parts.append(text)
        if nexus_start is None:
            nexus_start = (start, yaw)

    mapdata = ("--!nonstrict\n-- GENERATED by Roblox/tools/build_place.py from Roblox/world/*.world. Do not edit.\n"
               "-- Spawn points, terminals, hover cars, rifts and course gates in Roblox studs.\n"
               "return {\n\tLevels = {\n" + "\n".join(mapdata_parts) + "\n\t},\n}\n")
    mapdata_path = os.path.join(ROBLOX, "src", "shared", "MapData.luau")
    with open(mapdata_path, "w") as f:
        f.write(mapdata)

    # Workspace with both worlds.
    w.open("Workspace", [p_string("Name", "Workspace"), p_float("Gravity", 196.2), p_bool("StreamingEnabled", False),
                         p_float("FallenPartsDestroyHeight", -500)])
    for folder, offset, data in levels:
        w.open("Folder", [p_string("Name", folder)])
        for b in data["blocks"]:
            write_block(w, b, offset)
        if folder == "NexusObservatory":
            w.open("Folder", [p_string("Name", "Stars")])
            write_stars(w)
            w.close()
        w.close()
    (sx, sy, sz), yaw = nexus_start
    w.leaf("SpawnLocation", [
        p_string("Name", "NexusSpawn"), p_bool("Anchored", True), p_bool("CanCollide", True), p_bool("Neutral", True),
        p_int("Duration", 0), p_cframe("CFrame", T(sx, sy + 0.2, sz) * R(rot_y(math.radians(yaw)))), p_vec("size", (8, 0.4, 8)),
        p_color((255, 230, 120)), p_token("Material", MATERIAL["Neon"]), p_float("Transparency", 0.4),
        p_token("TopSurface", 0), p_token("BottomSurface", 0),
    ])
    w.close()

    # Lighting: bright cartoon daylight.
    w.open("Lighting", [p_string("Name", "Lighting"), p_float("ClockTime", 14.5), p_float("Brightness", 2.6),
                        p_color3("Ambient", (0.45, 0.45, 0.5)), p_color3("OutdoorAmbient", (0.62, 0.62, 0.68)),
                        p_bool("GlobalShadows", True), p_float("EnvironmentDiffuseScale", 0.4), p_float("EnvironmentSpecularScale", 0.2)])
    w.leaf("Sky", [p_string("Name", "Sky"), p_int("StarCount", 3000)])
    w.leaf("ColorCorrectionEffect", [p_string("Name", "ToonColor"), p_float("Saturation", 0.25), p_float("Contrast", 0.08), p_float("Brightness", 0.02)])
    w.leaf("BloomEffect", [p_string("Name", "Glow"), p_float("Intensity", 0.6), p_float("Size", 24), p_float("Threshold", 1.6)])
    w.close()

    # Code.
    w.open("ReplicatedStorage", [p_string("Name", "ReplicatedStorage")])
    w.open("Folder", [p_string("Name", "Shared")])
    write_tree(w, os.path.join(ROBLOX, "src", "shared"))
    w.close()
    w.close()
    w.open("ServerScriptService", [p_string("Name", "ServerScriptService")])
    write_init_container(w, os.path.join(ROBLOX, "src", "server"), "Server", "init.server.luau", "Script")
    w.close()
    w.open("StarterPlayer", [
        p_string("Name", "StarterPlayer"), p_bool("CharacterUseJumpPower", False), p_float("CharacterJumpHeight", 7.5),
        p_float("CharacterWalkSpeed", 21), p_bool("LoadCharacterAppearance", False), p_bool("EnableMouseLockOption", False),
        p_float("CameraMaxZoomDistance", 60), p_token("GameSettingsAvatar", 1),
    ])
    w.open("StarterPlayerScripts", [p_string("Name", "StarterPlayerScripts")])
    write_init_container(w, os.path.join(ROBLOX, "src", "client"), "Client", "init.client.luau", "LocalScript")
    w.close()
    # Empty placeholders replace Roblox's default Animate (procedural animation
    # drives the joints) and Health (Combat handles regeneration).
    w.open("StarterCharacterScripts", [p_string("Name", "StarterCharacterScripts")])
    w.leaf("LocalScript", [p_string("Name", "Animate"), p_source("-- Replaced by Client/Controllers/Animator (procedural animation).\n")])
    w.leaf("Script", [p_string("Name", "Health"), p_source("-- Health regeneration is handled by Server/Systems/Combat.\n"), p_token("RunContext", 0)])
    w.close()
    w.close()
    w.leaf("StarterGui", [p_string("Name", "StarterGui")])

    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w") as f:
        f.write(w.text())
    print("Wrote %s (%d parts, %.1f MB)" % (out, PartStats.count, os.path.getsize(out) / 1e6))


if __name__ == "__main__":
    main()
