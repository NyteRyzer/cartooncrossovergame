PROJECT MULTIVERSE TOONS - Prototype Build 0.1 (Linux x86_64)
=============================================================

HOW TO PLAY
  tar xzf MultiverseToons-Prototype-0.1-linux-x86_64.tar.gz
  cd MultiverseToons-Prototype-0.1-linux-x86_64
  ./play.sh                    (or double-click MultiverseToons)

  Options:  ./play.sh --fullscreen      ./play.sh --size 1280x720

REQUIREMENTS
  - 64-bit Linux with glibc 2.28 or newer (Ubuntu 20.04+, Debian 10+,
    Fedora 29+, Arch, Mint, Pop!_OS, SteamOS...). Works on X11 and on
    Wayland through XWayland.
  - A GPU driver with OpenGL 3.3 (anything from roughly 2010 onward).
  - No install, no dependencies, nothing written outside this folder.

WHAT'S IN THIS BUILD
  This is a greybox prototype: blocky stand-in art, but real mechanics.
  It plays the same level layouts and runs the same Omnitrix and hover-car
  rules as the Unreal Engine project in this repository.

  NEXUS OBSERVATORY (start)
    - 8 dimensional terminals: walk up, press E, pick a hero (1-8).
    - The Retroville terminal can also travel there (press T).
    - Jimmy's hover car is parked on the platform.
    - Flight Ring Challenge: fly through the 12 glowing rings in order.
    - Scale gallery next to the start (jump-height test blocks).

  RETROVILLE (1 km x 1 km)
    - Suburbs, the Neutron house, downtown towers, school, Retroland.
    - 3 Rift Patrols: get close to a purple rift, beat 3 waves to close it.
    - Hover Race: drive the car through the perimeter gates in order.
    - Press P to return to the Nexus.

  HEROES
    1 Jimmy Neutron  - laser blaster, Q Goddard jetpack, drives the hover car
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
                       Q air scooter
    Everyone: F unleashes their ultimate when the meter is full.

CONTROLS
  WASD move            Mouse look (arrow keys also work)
  Space jump / ascend  Ctrl or C descend      Shift sprint / boost
  Left click attack    Q signature ability    F ultimate
  Tab fly (flyers)     E interact             P return to Nexus
  F1 controls overlay  F11 fullscreen         Esc pause (X to quit)
  Hover car: W/S throttle, A/D steer, Space/Ctrl altitude, Shift boost, E exit

  Debug keys: F5 spawn enemies, F6 fill ultimate meter, F7 next hero.

TROUBLESHOOTING
  "Permission denied"   chmod +x play.sh MultiverseToons
  Black window / GL error: update your GPU driver; check
                        `glxinfo | grep "OpenGL version"` shows 3.3 or higher.
  Self-check:           ./MultiverseToons --smoke-test
                        plays through every system on its own, saves
                        screenshots to ./smoke-test-shots, prints ok/FAIL.

All characters belong to their respective owners (Nickelodeon/Paramount,
Cartoon Network/Warner Bros. Discovery, Disney). Non-commercial fan prototype.
