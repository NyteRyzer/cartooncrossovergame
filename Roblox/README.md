# Multiverse Toons on Roblox

A Roblox version of the prototype: the same heroes, worlds and mechanics, rebuilt in Luau.
It's built from parts and code only, so it needs no uploaded assets.

## Open it in Roblox Studio

1. Download **`Builds/MultiverseToons.rbxlx`** from this repository.
2. Double-click it, or in Roblox Studio use **File > Open from File...** and pick it.
3. Press **Play** (F5). To test multiplayer, use **Test > Clients and Servers**.
4. To put it online, use **File > Publish to Roblox**. In **Game Settings > Avatar**,
   check that the avatar type is **R15**. The procedural animation needs R15; R6 avatars
   still play, but without the custom animations.

That place file is complete: both worlds, about 17,000 parts, are baked in (so you can
see and edit them in Studio), along with every script.

## What's in it

- **8 heroes**, each picked at a dimensional terminal in the Nexus (walk up, press **E**):
  Jimmy Neutron, Timmy Turner, Danny Phantom, Jake Long, XJ9, El Tigre, Ben Tennyson and Aang.
  Each one gets:
  - cartoon proportions with a big round head and a drawn face that blinks;
  - their colours and gear (Jimmy's hair swirl and shrink ray, Timmy's hat and wand, Danny's
    white hair and glowing hands, Jake's spikes, XJ9's pigtails and skirt, El Tigre's ears and
    claws, Ben's Omnitrix, Aang's arrow and staff);
  - sidekicks: Cosmo & Wanda orbit Timmy, and Goddard follows Jimmy.
- **Ben's Omnitrix:** Heatblast, Four Arms (with a second pair of arms), XLR8, Cannonbolt and
  Way Big. It uses the same state machine as the Unreal project and the prototype.
- **Flight with momentum and per-hero handling:**
  - Danny is agile.
  - XJ9 is fastest but turns wide.
  - Jake turns into a dragon with flapping wings.
  - Timmy surfs a cloud.
  - Everyone gets banking, boost with a sonic boom, barrel rolls, speed lines and a wider FOV at speed.
  - **Aang's glider:** dive to gain speed, climb to slow down, and you stall if you go too slow.
- **Moves for everyone:**
  - dodge roll, air dash or barrel roll (with a short invulnerability window);
  - ground pound with a shockwave;
  - double jumps with flips, and wall jumps (El Tigre can cling to walls);
  - coyote time and short hops;
  - 3-hit combos with finishers;
  - emotes (**B**).
- **Hero abilities:**
  - Jimmy's jetpack;
  - Timmy's anvil drop;
  - Danny's intangibility (fly through walls) and Ghost Wail;
  - Jake's fire breath;
  - XJ9's homing missiles;
  - El Tigre's grappling chain;
  - Aang's air scooter and earth pillar;
  - an ultimate for every hero.
- **Procedural animation** ported from the prototype. Each hero has their own walk, idle
  fidgets, attack moves, dodge, flight pose and emote, and every player sees everyone's moves.
- **Worlds:**
  - **Nexus Observatory**, the hub: terminals and the Flight Ring Challenge.
  - **Retroville**: suburbs, downtown, the Neutron lab, Lindbergh Elementary, the Candy Bar,
    the park and Retroland. It also has three rift patrols (3 waves of rift gremlins each),
    the Hover Race in Jimmy's hover car, and 24 rift shards to collect (10 more in the Nexus).
- **Multiplayer and server authority:** the server decides all damage, cooldowns and the
  ultimate meter. Every remote is rate-limited and validated.
- **Mobile and gamepad:** on-screen buttons and touch-drag camera on mobile; gamepad mappings
  for every action.

## Open-world action mechanics

These borrow the best-known ideas from the last decade's superhero, action and open-world games.
The code is original.

**Freeflow combat**
- When an enemy winds up to hit you, a red **!** appears over it and a COUNTER prompt flashes.
  Counter in time (right click / X) to reverse the attack. Pressing counter with no attack
  coming blocks instead.
- Melee attacks lunge across the gap to the targeted enemy. Lock on with **Z** to keep the
  camera on one enemy.
- Hits build up an enemy's posture. A full posture bar staggers it ("FINISH IT!"), and the
  next melee hit is a finisher that heals you.
- Shots fired at you can be countered back at the shooter.
- An 8-hit combo unlocks a special **takedown** (T). Taking a hit breaks your combo.

**Enemies**
- **Gremlin**: basic melee.
- **Brute**: heavy, with a ground-slam shockwave.
- **Spitter**: ranged; its shots can be parried.
- **Hunter**: sent by the wanted level.
- Rift waves mix these types.

**Boss: the Rift Titan** has three phases:
1. A ground shockwave you jump over, and a charge you dodge.
2. Adds projectile volleys you can counter back.
3. Enraged: faster, and summons gremlins.

It appears when all three rifts are closed, or from the Titan Hunt mission. It can be
posture-broken for big finisher damage.

**Parkour**
- **Wall-run:** hold Shift alongside a wall in the air, and jump to kick off it.
- **Wall-climb:** hold Shift running into a wall. It costs energy, and you vault over the top.
- **Mantle:** automatic onto ledges.
- **Slide:** Ctrl while sprinting.
- **Rope swing:** El Tigre swings from anything above him (Q in the air). Pump the swing and let go to fling.

**Open world**
- **Wanted level:** smashing monsters in Retroville raises threat stars, and Hunter squads chase
  you. Lie low (no fighting, no hunters near) and the stars flash and cool off.
- **Missions:** walk into a yellow marker and press E. Each has a waypoint, a timer and rewards:
  - **Rift Rush:** three combat waves Downtown.
  - **Express Delivery:** drive a hover car to the Neutron house against the clock.
  - **Rooftop Run:** reach tower-top checkpoints.
  - **Titan Hunt:** the boss fight.
- **Traffic:** hover cars drive Retroville's streets and stop for people in the road. Press E
  to hijack one.
- **Day/night cycle:** a full day every 12 minutes.
- **Minimap:** rotates with the camera and shows roads, missions, rifts, shards, enemies, players,
  cars, the boss, and an arrow to your objective.

**Progression (saved between sessions)**
- XP and cash from kills, rifts, missions, courses and shards.
- Level-ups give skill points. Spend them with **K** or at the Upgrade Bench in the Nexus on
  Vitality, Power, Reserves, Aerodynamics, Agility or Focus.
- The Gear Locker in the Nexus sells health packs, ultimate charge and extra skill points.
- Saving uses Roblox DataStores, with a session lock so data can't be duplicated or lost when
  switching servers. Enable **Game Settings > Security > Enable Studio Access to API Services**
  to test saving in Studio. Without it, progress just doesn't save.

## Controls

| Action | Keyboard / mouse |
|---|---|
| Move / look | WASD / mouse (cursor locked; **M** or Esc frees it) |
| Jump / ascend | Space (jump at a wall to wall-jump; hold while falling to glide as Aang) |
| Descend / ground pound | Ctrl or C (in the air: ground pound) |
| Sprint / boost | Shift |
| Attack | Left click (3-hit combos) |
| Signature ability | Q |
| Second ability | R (Danny: Ghost Wail, Aang: Earth Pillar) |
| Ultimate | F (when the meter is full) |
| Fly (flyers) | Tab |
| Dodge / air dash / barrel roll | V or Left Alt |
| Emote | B |
| Ben's Omnitrix | 1 / 2 / 3 transform, Q dial, G Master Control Glitch, F Way Big |
| Terminals, hover car | E |
| Back to the Nexus | P |
| Counter / block | Right click or X (on a red **!** to counter) |
| Takedown | T (with an 8+ combo) |
| Lock-on | Z or middle mouse |
| Wall-run / wall-climb | Hold Shift alongside / into a wall |
| Slide | Ctrl while sprinting |
| Skills menu | K |
| Missions, shop, skills bench | E at the marker or service |
| Controls overlay | F1 |

## Project layout

The folders follow the [Rojo](https://rojo.space) layout, and `default.project.json` maps them,
so you can also live-sync the code into Studio with `rojo serve`.

```text
Roblox/
  src/shared/   -> ReplicatedStorage.Shared   Config (heroes, tuning), Omnitrix, Pose (animation), Remotes, MapData (generated)
  src/server/   -> ServerScriptService.Server init + Systems: Net, Players, Heroes, World, Enemies, Combat, Collectibles, Courses; Lib/Looks
  src/client/   -> StarterPlayerScripts.Client init + Controllers: Input, Hud, CameraRig, Movement, Animator, Effects, Companions, Vehicle
  world/        the two levels exported from the prototype (blocks, set dressing, markers)
  tools/        build_place.py (writes the .rbxlx), test_logic.py (syntax check + unit tests)
```

To rebuild the place after changing code or levels:

```sh
# only after changing the prototype's levels or set dressing:
Prototype/build/MultiverseToons --export-world Roblox/world
python3 Roblox/tools/build_place.py          # -> Builds/MultiverseToons.rbxlx
pip install lupa && python3 Roblox/tools/test_logic.py
```

## Notes

- Each hero's face is drawn with GUI frames. To use the faces from the concept sheet instead,
  upload `Prototype/assets/sprites/faces/*.png` to Roblox. Then set `Look.FaceImage =
  "rbxassetid://..."` for that hero in `src/shared/Config.luau`, or `Portrait` for the
  terminal menu card.
- The look is inspired by the shows, but it is an original fan recreation. It doesn't use any
  ripped show assets, and publishing it publicly would need the rights holders' permission.
