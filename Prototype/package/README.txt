PROJECT MULTIVERSE TOONS - Prototype Build @VERSION@
====================================================

HOW TO PLAY

  WINDOWS 10 / 11
    1. Right-click the .zip > Extract All.
    2. Open the extracted folder and double-click MultiverseToons.exe.
    If SmartScreen says "Windows protected your PC" (the game isn't signed
    by a paid certificate): click "More info" > "Run anyway". Only needed once.

  macOS 10.15 or newer (Apple Silicon and Intel)
    1. Double-click the .zip to extract it.
    2. Right-click "Multiverse Toons.app" > Open > Open.
    The right-click is only needed the first time: the app isn't notarized
    by Apple, so a plain double-click is blocked on first launch. If macOS
    says the app "is damaged", run this once in Terminal and try again:
        xattr -cr ~/Downloads/MultiverseToons-Prototype-*/"Multiverse Toons.app"
    (adjust the path to wherever you extracted it).

  LINUX (64-bit, glibc 2.28+: Ubuntu 20.04+, Debian 10+, Fedora 29+, Arch,
  Mint, Pop!_OS, SteamOS...; X11 or Wayland via XWayland)
    tar xzf MultiverseToons-Prototype-@VERSION@-linux-x86_64.tar.gz
    cd MultiverseToons-Prototype-@VERSION@-linux-x86_64
    ./play.sh

  Any platform needs a graphics driver with OpenGL 3.3 (any GPU from roughly
  2010 on). No install and no extra downloads; nothing is written outside the
  game folder. Command-line options: --fullscreen   --size 1280x720

WHAT'S IN THIS BUILD
  This is an early prototype with real mechanics. The heroes are cel-shaded
  3D models designed from the production-bible concept sheet: they wear their
  sheet faces and are fully animated (walk/run cycles, idle breathing, jumps,
  landings, flying, gliding, attacks, hit reactions). Press M to switch to
  the hand-drawn cut-outs taken straight from the sheet. The sheet's
  portraits appear on the hero cards and the terminal menu.
  It plays the same level layouts and runs the same Omnitrix and hover-car
  rules as the Unreal Engine project in this repository.

  NEXUS OBSERVATORY (start)
    - 8 dimensional terminals: walk up, press E, pick a hero (1-8).
    - The Retroville terminal can also travel there (press T).
    - Jimmy's hover car is parked on the platform.
    - Flight Ring Challenge: fly through the 12 glowing rings in order.
    - Scale gallery next to the start (jump-height test blocks).

  RETROVILLE (1 km x 1 km)
    - Suburbs with lawns, fences, mailboxes and trees; roads with street
      lamps; the Neutron house and lab with Jimmy's rocket; Lindbergh
      Elementary with its school bus; the Candy Bar; the park and pond;
      downtown towers with neon; Retroland (ferris wheel, coaster, carousel).
    - 3 Rift Patrols: get close to a purple rift, beat 3 waves to close it.
    - Hover Race: drive the car through the perimeter gates in order.
    - Press P to return to the Nexus.

  HEROES
    1 Jimmy Neutron  - shrink-ray blaster, Q Goddard jetpack, drives the hover
                       car; Goddard follows him around
    2 Timmy Turner   - magic stars, Q anvil drop, Tab fairy cloud flight
    3 Danny Phantom  - ecto blasts, Tab ghost flight, Q intangible (phase
                       through walls), R Ghost Wail
    4 Jake Long      - dragon claws, hold Q fire breath, Tab dragon form flight
    5 XJ9 (Jenny)    - rapid arm blaster, Q homing missiles, Tab rocket flight
    6 El Tigre       - steel claws, hold Q/right-click to grapple, double jump
    7 Ben Tennyson   - Omnitrix: 1 Heatblast, 2 Four Arms, 3 XLR8 (Q opens the
                       dial with slow-mo), G Master Control Glitch (Cannonbolt),
                       F with a full meter = Way Big
    8 Aang           - air blast, double jump, hold Space in the air to glide,
                       Q air scooter, R earth pillar (launches you up)
    Everyone: F unleashes their ultimate when the meter is full.

MOVES AND FLIGHT
  - Flight has momentum: you carve turns and bank into them, diving builds
    speed, Shift boosts (uses energy, with a sonic boom), V barrel-rolls, and
    holding Ctrl into the ground lands. Every flyer handles differently:
    Danny is agile and stops on a dime, XJ9's rockets are fastest but turn
    wide, Jake's wings bank hard and fold back for dives, Timmy surfs a
    floaty fairy cloud.
  - Aang's glider: look down to dive and gain speed, look up to climb; fly
    too slowly and you stall. S air-brakes.
  - Attacks chain into 3-hit combos; the third hit is a finisher (spins,
    slams, kicks, double strikes). Dodges make you untouchable.
  - Jumps have coyote time, early-release short hops, wall jumps, and
    flips on double jumps.
  - Rift shards: 24 crystals in Retroville and 10 in the Nexus, on rooftops
    and up in the sky. Each one refills health, energy and ultimate.
  - Every hero has their own idle fidgets, walk style, attack moves, dodge,
    flight pose and emote.

CONTROLS
  WASD move            Mouse look (arrow keys also work)
  Space jump / ascend  Ctrl or C descend      Shift sprint / boost
  Left click attack    Q signature ability    F ultimate
  Tab fly (flyers)     E interact             P return to Nexus
  V or Alt dodge: roll on the ground, dash in the air, barrel roll in flight
  Ctrl in the air: ground pound     B emote (each hero's signature pose)
  Jump while touching a wall: wall jump (El Tigre clings and slides)
  F1 controls overlay  F11 fullscreen         Esc pause (X to quit)
  M art style: 3D models / concept-sheet cut-outs
  Hover car: W/S throttle, A/D steer, Space/Ctrl altitude, Shift boost, E exit

  Debug keys: F5 spawn enemies, F6 fill ultimate meter, F7 next hero.

TROUBLESHOOTING
  Linux "Permission denied":   chmod +x play.sh MultiverseToons
  Error about OpenGL 3.3 / black window: update your graphics driver.
  Self-check (all platforms): run the game with --smoke-test, e.g.
      Windows:  MultiverseToons.exe --smoke-test
      macOS:    "Multiverse Toons.app/Contents/MacOS/MultiverseToons" --smoke-test --shots ~/Desktop/mt-shots
      Linux:    ./MultiverseToons --smoke-test
  It plays through every system on its own, saves screenshots and a
  smoke-test.log (ok/FAIL lines) to the smoke-test-shots folder.

All characters belong to their respective owners (Nickelodeon/Paramount,
Cartoon Network/Warner Bros. Discovery, Disney). Non-commercial fan prototype.
