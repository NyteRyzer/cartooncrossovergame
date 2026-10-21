"""Unit tests for Content/Python/greybox/layout_schema.py (no Unreal needed)."""

import copy
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "Content", "Python", "greybox"))

import layout_schema as ls  # noqa: E402

BASE = {
    "name": "Test",
    "map_path": "/Game/Maps/Test",
    "bounds": {"min": [-1000, -1000, -100], "max": [1000, 1000, 1000]},
    "player_start": {"location": [0, 0, 100]},
    "blocks": [{"label": "Floor", "location": [0, 0, -50], "size": [2000, 2000, 100]}],
}


class LayoutSchemaTests(unittest.TestCase):
    def layout(self, **changes):
        data = copy.deepcopy(BASE)
        data.update(changes)
        return data

    def test_minimal_layout_validates(self):
        blocks, markers = ls.validate_layout(self.layout())
        self.assertEqual(len(blocks), 1)
        self.assertEqual(markers, [])

    def test_ring_expansion_faces_center(self):
        ring = {"type": "ring", "label": "Post", "count": 4, "radius": 500, "size": [10, 10, 10],
                "names": ["E", "N", "W", "S"]}
        blocks = ls.expand_layout(self.layout(arrays=[ring]))[1:]
        self.assertEqual([b["label"] for b in blocks], ["Post_E", "Post_N", "Post_W", "Post_S"])
        self.assertAlmostEqual(blocks[0]["location"][0], 500)
        self.assertAlmostEqual(blocks[1]["location"][1], 500)
        self.assertAlmostEqual(blocks[0]["yaw"], 180)  # east post looks west, at the centre
        self.assertNotIn("names", blocks[0])

    def test_grid_is_deterministic_and_skip_is_stable(self):
        grid = {"type": "grid", "label": "H", "origin": [0, 0, 0], "count": [3, 3], "spacing": [100, 100],
                "size": [50, 50, 40], "size_jitter": [10, 10, 10], "seed": 3}
        full = ls.expand_layout(self.layout(arrays=[grid]))
        again = ls.expand_layout(self.layout(arrays=[grid]))
        self.assertEqual(full, again)

        skipped = dict(grid, skip=[[1, 1]])
        partial = ls.expand_layout(self.layout(arrays=[skipped]))
        self.assertEqual(len(partial), len(full) - 1)
        by_label = {b["label"]: b for b in full}
        for b in partial:
            self.assertEqual(b, by_label[b["label"]])
        # Grid cells rest on the origin plane.
        cell = by_label["H_0_0"]
        self.assertAlmostEqual(cell["location"][2], cell["size"][2] / 2.0)

    def test_duplicate_labels_rejected(self):
        dup = self.layout(blocks=BASE["blocks"] + [dict(BASE["blocks"][0])])
        with self.assertRaisesRegex(ls.LayoutError, "duplicate"):
            ls.validate_layout(dup)

    def test_out_of_bounds_rejected(self):
        bad = self.layout(blocks=[{"label": "Far", "location": [5000, 0, 0], "size": [1, 1, 1]}])
        with self.assertRaisesRegex(ls.LayoutError, "outside bounds"):
            ls.validate_layout(bad)

    def test_bad_shape_and_size_rejected(self):
        with self.assertRaisesRegex(ls.LayoutError, "unknown shape"):
            ls.validate_layout(self.layout(blocks=[{"label": "X", "shape": "torus", "location": [0, 0, 0], "size": [1, 1, 1]}]))
        with self.assertRaisesRegex(ls.LayoutError, "positive"):
            ls.validate_layout(self.layout(blocks=[{"label": "X", "location": [0, 0, 0], "size": [1, 0, 1]}]))

    def test_buried_spawn_rejected(self):
        tower = {"label": "Tower", "location": [0, 0, 500], "size": [200, 200, 1000]}
        with self.assertRaisesRegex(ls.LayoutError, "player_start is inside solid block Tower"):
            ls.validate_layout(self.layout(blocks=BASE["blocks"] + [tower]))
        ghost = dict(tower, no_collision=True)
        ls.validate_layout(self.layout(blocks=BASE["blocks"] + [ghost]))

    def test_unknown_marker_class_rejected(self):
        markers = [{"label": "M", "class": "Spaceship", "location": [0, 0, 100]}]
        with self.assertRaisesRegex(ls.LayoutError, "unknown marker class"):
            ls.validate_layout(self.layout(markers=markers))

    def test_shipped_layouts_validate(self):
        folder = os.path.join(ROOT, "Content", "Python", "greybox", "layouts")
        names = [n for n in os.listdir(folder) if n.endswith(".json")]
        self.assertGreaterEqual(len(names), 2)
        for name in names:
            with self.subTest(layout=name):
                ls.validate_layout(ls.load_layout(os.path.join(folder, name)))


if __name__ == "__main__":
    unittest.main()
