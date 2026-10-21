# Technical Design

Engine: Unreal Engine 5.4 (C++ plus Blueprint). Single game module `MultiverseToons` and a
rendering plugin `ToonRender`.

## Code layout

```
Source/MultiverseToons/
  Public|Private/
    Core/        ToonTypes (EToonUniverse, EToonAbilitySlot, collision channels), ToonHeroData
    Characters/  AToonCharacterBase: camera, Enhanced Input, loadout, ultimate meter, immunity
    Abilities/   UToonAbility: cooldown + meter-cost ability object, gameplay in BP
    Traversal/   UToonTraversalComponent base, UGhostFlightComponent (Danny),
                 AHoverCarPawn (Jimmy), HoverMath.h (engine-free spring/drive math)
    Omnitrix/    OmnitrixStateMachine.h (engine-free), UOmnitrixComponent, UAlienFormData
    Hub/         AToonGameModeBase, AToonPlayerController (hero swap),
                 ADimensionalTerminal, IToonInteractable
Plugins/ToonRender/       Cel-shade + outline HLSL, stencil tagging (docs/RENDERING.md)
Content/Python/greybox/   JSON layouts and the editor script that builds greybox maps
Tools/                    Logic tests that run without Unreal (run_logic_tests.sh)
```

### Conventions

- **Rules that can be tested stay out of UObjects.** State machines and physics math live in plain
  headers (`OmnitrixStateMachine.h`, `HoverMath.h`) that compile with a stock C++17 compiler.
  The UObject wrappers only feed them input and apply results. That way the rules are covered by
  tests in CI without an engine install.
- **One hero = one `AToonCharacterBase` Blueprint** plus a traversal component and optional hero
  components (Omnitrix, element stance...). Roster entries are `UToonHeroData` primary assets
  under `/Game/Data/Heroes`.
- **Damage immunity and ability blocks are named sources** (`AddDamageImmunity("GhostIntangibility")`),
  so overlapping effects like intangibility, Omnitrix i-frames and Aang's Elemental Sphere can't
  clear each other.
- **Heavy assets are soft references**, streamed through `UAssetManager`.

## World scale

These numbers set the physics scale for every open world. Units are Unreal centimetres.

| Thing | Value | Notes |
|---|---|---|
| Gravity | -1960 cm/s² (2× real) | Snappier cartoon arcs; set in `DefaultEngine.ini` |
| Kid hero capsule | r 32, half-height 70 (≈140 cm) | Most of the roster; Aang/Jenny tune per BP |
| Walk speed | 650 cm/s | |
| Jump | 900 cm/s → ~2.0 m apex | Nexus scale gallery: 200 cm block reachable, 250 cm not |
| Danny cruise / boost | 1800 / 3500 cm/s (65 / 126 km/h) | Crosses Retroville (1 km) in ~30 s at boost |
| Hover car top / boost | 2500 / 4200 cm/s (90 / 151 km/h) | Cruise altitude 1.2 m – 25 m via Lift |
| Hover spring | k = 60 /s², c = 10 /s | ~33 cm sag under gravity, damping ratio ≈ 0.65 |
| Nexus platform | 120 m diameter | Terminals on 40 m ring |
| Retroville slice | 1 km × 1 km | Downtown towers 20–100 m |

## Systems

### Hover car (`AHoverCarPawn`)

- Physics body (900 kg) held up by 4 hover points. Each point line-traces down and applies
  `HoverMath::SpringAcceleration × mass / N` at its location, so pitch and roll come out of the physics.
- Self-righting torque keeps it level. Lateral grip removes sideways drift at a frame-rate-independent
  rate, and yaw is driven directly for arcade handling.
- `Lift` input raises the cruise altitude (up to 25 m) so the car can cross rooftops and
  downtown. The ray length grows with it.
- Any hero can interact to drive it: the driver attaches to the `DriverSeat` socket and the controller
  possesses the car.

### Ghost flight (`UGhostFlightComponent`)

- Flight uses `MOVE_Flying` on the character movement component, with cruise/boost speeds and a
  vertical axis.
- Intangibility sets the capsule to ignore all channels except `PhaseBlocker`
  (`ECC_GameTraceChannel1`), grants damage immunity, blocks abilities and drains Ecto energy.
- **Safety:** releasing intangibility inside geometry is deferred until a shrunk-capsule overlap
  test comes back clear. If energy runs out while stuck, Danny teleports to the last clear location.
  Phasing forces flight so he can't fall through floors. World bounds and Fenton ghost shields
  use the `PhaseBlocker` object type.

### Omnitrix (`Omnitrix::FStateMachine` + `UOmnitrixComponent`)

```
Human ─OpenDial─► Selecting ─Confirm─► Transforming ─(timer ∧ assets ready)─► Alien
  ▲                  │Cancel               ▲  │                                │ timeout / ForceRevert
  │                  ▼                     │  └─────────── QuickSwap ◄─────────┤
  │                Human/Alien             │                                   ▼
Recharging ◄──────────────────────── Reverting ◄── Glitched (MasterControl) / Overclocked (Ultimate)
```

- The 3 playlist forms and the glitch form (Cannonbolt) are pre-streamed at BeginPlay. The overclock
  form (Way Big) streams on demand. `Transforming` holds until the target form's assets are resident,
  so a swap never shows a missing mesh.
- An in-combat **QuickSwap** costs 4 s of alien time. The alien clock keeps running while the dial is
  open; the dial slows world time to 25%, but Omnitrix timers run in real time.
- `Transforming` and `Glitched` grant i-frames. `Transforming`, `Reverting` and `Selecting` block
  abilities.
- Swapping a form applies its mesh, anim class, capsule (growing upward so feet stay planted),
  movement stats and ability slot overrides, then re-tags the universe stencil.
- Rules are covered by `Tools/tests/omnitrix_test.cpp`.

### Hero swapping (Nexus)

`ADimensionalTerminal` → Blueprint UI (`OnTerminalOpened`) → `SelectHero` →
`AToonPlayerController::SwapToHero`. That async-loads the hero class, spawns it at the current pawn's
transform, possesses it and destroys the old pawn.

### Greybox pipeline

`Content/Python/greybox/layouts/*.json` is the source of truth for blockouts. `layout_schema.py`
expands ring/grid generators and validates them: bounds, unique labels, and spawns not buried in solid
blocks. It runs in CI with plain Python. In the editor, run:

```python
import greybox.build_greybox as gb
gb.build_all()   # → /Game/Maps/Greybox/L_Nexus_Greybox, L_Retroville_Greybox
```

Re-running replaces everything tagged `Greybox`, so iterate in JSON until art takes over a level.

## Status of the immediate next steps

| # | Step | State |
|---|---|---|
| 1 | UE5 project + cel-shading/line-art pipeline | Project, config, `ToonRender` plugin, HLSL library and stencil tagging done. Post-process materials and the style LUT need building in the editor ([RENDERING.md](RENDERING.md)). |
| 2 | Hover car + Danny flight prototypes | `AHoverCarPawn` and `UGhostFlightComponent` written; scale numbers above. Needs BP setup (meshes, input assets) and feel-tuning in the greybox. |
| 3 | Omnitrix state machine + asset swap | State machine written and unit-tested; `UOmnitrixComponent` handles streaming and swapping. Needs alien data assets. |
| 4 | Greybox: Nexus + Retroville slice | Layouts written and validated; run `gb.build_all()` in the editor to generate the maps. |

## Editor setup checklist (first open)

1. Right-click `MultiverseToons.uproject` → *Generate Visual Studio project files*, build `MultiverseToonsEditor`.
2. In the Python console: `import greybox.build_greybox as gb; gb.build_all()`.
3. Create Enhanced Input assets under `/Game/Input`: `IMC_Hero`, `IMC_Vehicle`, and actions
   `IA_Move` (Axis2D), `IA_Look` (Axis2D), `IA_Jump`, `IA_Vertical` (Axis1D), `IA_Traversal`,
   `IA_Interact`, `IA_Light`, `IA_Heavy`, `IA_Sig1-3`, `IA_Ultimate`, `IA_Throttle`, `IA_Steer`,
   `IA_Lift`, `IA_Boost`, `IA_ExitVehicle`.
4. Create `BP_Danny` (parent `AToonCharacterBase`, add `GhostFlightComponent`), `BP_Ben` (add
   `OmnitrixComponent`), `BP_HoverCar` (parent `AHoverCarPawn`, add a body mesh with a `DriverSeat` socket).
5. Build `PP_ToonLighting`, `PP_ToonOutline` and `T_UniverseLineStyles` per [RENDERING.md](RENDERING.md).

## Known gaps / next up

- Combat is a thin ability-object layer. Move to the Gameplay Ability System once hit reactions,
  status effects and networking needs are clear.
- No networking or replication yet. Everything assumes a single local player.
- Ghost flight forward input is yaw-only. Try camera-pitch-relative flight during feel-tuning.
- `UOmnitrixComponent` doesn't yet auto-`ForceRevert` on lethal damage. Hook `OnDefeated` or add a
  pre-damage callback.
