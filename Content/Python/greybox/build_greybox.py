"""Builds greybox levels in the Unreal Editor from layouts/*.json.

In the editor's Python console (Content/Python is on the path):

    import greybox.build_greybox as gb
    gb.build("nexus_observatory")
    gb.build("retroville_slice")
    gb.build_all()

Re-running replaces every actor tagged "Greybox" in the target map, so the
JSON stays the source of truth until art takes over a level.
"""

import os

import unreal

from greybox import layout_schema

LAYOUT_DIR = os.path.join(os.path.dirname(__file__), "layouts")
GREYBOX_TAG = "Greybox"
SHAPE_MESHES = {
    "cube": "/Engine/BasicShapes/Cube.Cube",
    "cylinder": "/Engine/BasicShapes/Cylinder.Cylinder",
    "sphere": "/Engine/BasicShapes/Sphere.Sphere",
    "cone": "/Engine/BasicShapes/Cone.Cone",
    "plane": "/Engine/BasicShapes/Plane.Plane",
}
NATIVE_MARKERS = {
    "DimensionalTerminal": "/Script/MultiverseToons.DimensionalTerminal",
    "HoverCarPawn": "/Script/MultiverseToons.HoverCarPawn",
}


def _actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _levels():
    return unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def _open_or_create(map_path):
    if unreal.EditorAssetLibrary.does_asset_exist(map_path):
        _levels().load_level(map_path)
        for actor in _actors().get_all_level_actors():
            if actor.actor_has_tag(GREYBOX_TAG):
                _actors().destroy_actor(actor)
    elif not _levels().new_level(map_path):
        raise RuntimeError("could not create level %s" % map_path)


def _tag(actor, label, folder):
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    actor.set_editor_property("tags", [unreal.Name(GREYBOX_TAG)])


def _spawn_block(block, meshes):
    shape = block.get("shape", "cube")
    loc = block["location"]
    size = block["size"]
    rot = unreal.Rotator(roll=0.0, pitch=float(block.get("pitch", 0.0)), yaw=float(block.get("yaw", 0.0)))
    actor = _actors().spawn_actor_from_object(meshes[shape], unreal.Vector(*loc), rot)
    # Basic shapes are 100 uu; the plane has no thickness so Z scale is irrelevant.
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    _tag(actor, block["label"], "Greybox/" + block.get("folder", "Misc"))

    component = actor.static_mesh_component
    if block.get("phase_blocker"):
        component.set_collision_object_type(unreal.CollisionChannel.ECC_GAME_TRACE_CHANNEL1)
    if block.get("no_collision"):
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    return actor


def _spawn_marker(marker):
    cls_name = marker["class"]
    if cls_name == "PlayerStart":
        cls = unreal.PlayerStart
    elif cls_name == "TargetPoint":
        cls = unreal.TargetPoint
    else:
        cls = unreal.load_class(None, NATIVE_MARKERS[cls_name])
        if cls is None:
            unreal.log_warning("greybox: %s not compiled yet, placing TargetPoint for %s" % (cls_name, marker["label"]))
            cls = unreal.TargetPoint
    rot = unreal.Rotator(roll=0.0, pitch=0.0, yaw=float(marker.get("yaw", 0.0)))
    actor = _actors().spawn_actor_from_class(cls, unreal.Vector(*marker["location"]), rot)
    _tag(actor, marker["label"], "Greybox/" + marker.get("folder", "Markers"))
    return actor


def _spawn_lighting(layout):
    light = layout.get("lighting", {})
    sun = _actors().spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0, 0, 1000),
        unreal.Rotator(roll=0.0, pitch=float(light.get("sun_pitch", -45.0)), yaw=float(light.get("sun_yaw", 35.0))))
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    _tag(sun, "Sun", "Greybox/Lighting")
    for cls, label in ((unreal.SkyAtmosphere, "SkyAtmosphere"), (unreal.SkyLight, "SkyLight"),
                       (unreal.ExponentialHeightFog, "HeightFog")):
        actor = _actors().spawn_actor_from_class(cls, unreal.Vector(0, 0, 0), unreal.Rotator())
        _tag(actor, label, "Greybox/Lighting")
    sky = [a for a in _actors().get_all_level_actors() if a.get_actor_label() == "SkyLight"]
    if sky:
        sky[0].light_component.set_editor_property("real_time_capture", True)


def build(layout_name):
    path = os.path.join(LAYOUT_DIR, layout_name + ".json")
    layout = layout_schema.load_layout(path)
    blocks, markers = layout_schema.validate_layout(layout)

    meshes = {shape: unreal.load_asset(p) for shape, p in SHAPE_MESHES.items()}
    _open_or_create(layout["map_path"])

    with unreal.ScopedSlowTask(len(blocks) + len(markers), "Building greybox: " + layout["name"]) as task:
        task.make_dialog(True)
        for block in blocks:
            if task.should_cancel():
                return
            task.enter_progress_frame(1)
            _spawn_block(block, meshes)
        for marker in markers:
            task.enter_progress_frame(1)
            _spawn_marker(marker)

    ps = layout["player_start"]
    _spawn_marker({"label": "PlayerStart", "class": "PlayerStart", "location": ps["location"], "yaw": ps.get("yaw", 0.0)})
    _spawn_lighting(layout)

    _levels().save_current_level()
    unreal.log("greybox: built %s (%d blocks, %d markers) -> %s" % (layout["name"], len(blocks), len(markers), layout["map_path"]))


def build_all():
    for name in sorted(os.listdir(LAYOUT_DIR)):
        if name.endswith(".json"):
            build(name[:-5])
