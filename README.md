# Project Multiverse Toons (working title)

A 3D open-world cartoon crossover action RPG in Unreal Engine 5. Jimmy Neutron's rift generator
breaks, the cartoon multiverse spills into itself, and eight heroes travel between their worlds
from the Nexus Observatory.

- **Design:** [docs/GAME_BIBLE.md](docs/GAME_BIBLE.md)
- **Architecture, world scale, system specs, status:** [docs/TECH_DESIGN.md](docs/TECH_DESIGN.md)
- **Cel-shading and per-show line art:** [docs/RENDERING.md](docs/RENDERING.md)

## Play the prototype (Linux)

Download [`Builds/MultiverseToons-Prototype-0.1-linux-x86_64.tar.gz`](Builds/), then:

```bash
tar xzf MultiverseToons-Prototype-0.1-linux-x86_64.tar.gz
cd MultiverseToons-Prototype-0.1-linux-x86_64
./play.sh
```

It's a greybox build (blocky stand-in art) with all 8 heroes, the Nexus Observatory, the
Retroville slice, the hover car, Danny's phasing, the Omnitrix, rift patrols and gate courses.
It runs on any x86_64 distro with glibc 2.28+ and OpenGL 3.3. Controls are in the bundled
`README.txt` (press F1 in game). The prototype lives in `Prototype/` (C++17 + raylib). It plays
the same JSON greybox layouts and uses the same `OmnitrixStateMachine.h` / `HoverMath.h` as the
Unreal project. Rebuild with `Prototype/tools/package.sh`; CI rebuilds and smoke-tests it on
every push.

## What's here

| Area | Contents |
|---|---|
| `Source/MultiverseToons` | Hero base class, ability loadout, Danny's ghost flight + intangibility, Jimmy's hover car, Ben's Omnitrix, Nexus terminals and hero swapping |
| `Plugins/ToonRender` | Cel-shade lighting quantization and Sobel outline HLSL with per-universe line styles driven by custom stencil |
| `Content/Python/greybox` | JSON blockouts for the Nexus Observatory and the 1 km² Retroville slice, plus an editor script that builds them |
| `Prototype/` | Playable Linux greybox build (raylib) sharing the engine-independent rules and layouts |
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
