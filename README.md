# Project Multiverse Toons (working title)

A 3D open-world cartoon crossover action RPG in Unreal Engine 5. Jimmy Neutron's rift generator
breaks, the cartoon multiverse spills into itself, and eight heroes travel between their worlds
from the Nexus Observatory.

- **Design:** [docs/GAME_BIBLE.md](docs/GAME_BIBLE.md)
- **Architecture, world scale, system specs, status:** [docs/TECH_DESIGN.md](docs/TECH_DESIGN.md)
- **Cel-shading and per-show line art:** [docs/RENDERING.md](docs/RENDERING.md)

## What's here

| Area | Contents |
|---|---|
| `Source/MultiverseToons` | Hero base class, ability loadout, Danny's ghost flight + intangibility, Jimmy's hover car, Ben's Omnitrix, Nexus terminals and hero swapping |
| `Plugins/ToonRender` | Cel-shade lighting quantization and Sobel outline HLSL with per-universe line styles driven by custom stencil |
| `Content/Python/greybox` | JSON blockouts for the Nexus Observatory and the 1 km² Retroville slice, plus an editor script that builds them |
| `Tools/` | Tests for the engine-independent logic (Omnitrix rules, hover physics, layout validation) |

## Quick start

```bash
# Logic tests: no Unreal needed (C++17 compiler + Python 3)
Tools/run_logic_tests.sh
```

In Unreal Engine 5.4: generate project files, build `MultiverseToonsEditor`, open the project, then
in the Python console run:

```python
import greybox.build_greybox as gb
gb.build_all()
```

The full first-open checklist is in [docs/TECH_DESIGN.md](docs/TECH_DESIGN.md#editor-setup-checklist-first-open).

## Assets

Binary assets (`.uasset`, `.umap`, textures, audio) go through Git LFS (`.gitattributes`). Run
`git lfs install` before committing content.

## IP note

All characters and worlds belong to their respective rights holders (Nickelodeon/Paramount,
Cartoon Network/Warner Bros. Discovery, Disney). This repository is a non-commercial fan project
and design prototype. Don't commit ripped show assets. A public or commercial release would need
licensing.
