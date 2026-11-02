#!/usr/bin/env python3
"""Syntax-checks every Luau file and unit-tests the engine-independent shared
modules (Omnitrix state machine, procedural Pose) outside Roblox, using Lua 5.1
through the `lupa` package. The game code sticks to Lua 5.1-compatible syntax
(no type annotations, compound assignment or `continue`) so this works.

  pip install lupa
  python3 Roblox/tools/test_logic.py
"""

import glob
import os
import sys

from lupa import lua51

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src")


def main():
    lua = lua51.LuaRuntime(unpack_returned_tuples=True)
    failures = 0

    check = lua.eval("function(s, n) local f, e = loadstring(s, n) return f ~= nil, e end")
    files = sorted(glob.glob(os.path.join(SRC, "**", "*.luau"), recursive=True))
    for path in files:
        ok, err = check(open(path).read(), os.path.relpath(path, SRC))
        if not ok:
            print("SYNTAX", err)
            failures += 1
    print("syntax: %d files checked" % len(files))

    def load(name):
        src = open(os.path.join(SRC, "shared", name + ".luau")).read()
        return lua.execute(src)

    # Minimal Roblox stubs for Config.
    lua.execute("""
        Color3 = { fromRGB = function(r, g, b) return { r, g, b } end, new = function(r, g, b) return { r, g, b } end }
        Vector3 = { new = function(x, y, z) return { x, y, z } end }
    """)
    config = load("Config")
    omni_mod = load("Omnitrix")
    pose = load("Pose")

    tests = lua.execute(r"""
      return function(Config, Omnitrix, Pose)
        local fails = {}
        local function expect(cond, what)
            if not cond then table.insert(fails, what) end
        end
        local S = Omnitrix.State

        -- Mirrors Tools/tests/omnitrix_test.cpp.
        local m = Omnitrix.new(Config.Omnitrix)
        expect(m.State == S.Human, "starts human")
        expect(m:OpenDial() and m.State == S.Selecting, "dial opens")
        expect(m:Confirm(1) and m.State == S.Transforming, "confirm transforms")
        expect(m:IsInvulnerable() and m:IsInputLocked(), "transform i-frames + lock")
        m:Tick(Config.Omnitrix.TransformTime + 0.01)
        expect(m.State == S.Alien and m.ActiveForm == 1, "becomes alien slot 1")
        expect(Omnitrix.FormIndex(m.ActiveForm, Config) == 2, "slot 1 -> Four Arms index")
        expect(Config.Forms[Omnitrix.FormIndex(m.ActiveForm, Config)].Id == "FourArms", "form is Four Arms")
        local before = m.AlienTimeRemaining
        expect(m:QuickSwap(0), "quick swap")
        expect(math.abs(m.AlienTimeRemaining - (before - Config.Omnitrix.SwapTimePenalty)) < 1e-6, "swap costs alien time")
        expect(not m:QuickSwap(0), "no swap while transforming")
        m:Tick(1)
        expect(m.State == S.Alien and m.ActiveForm == 0, "swapped to Heatblast")
        expect(not m:QuickSwap(0), "cannot swap to the same form")
        for _ = 1, 100 do m:Tick(0.5) end
        expect(m.State == S.Human, "times out, reverts, recharges, back to human")

        local g = Omnitrix.new(Config.Omnitrix)
        expect(g:TriggerGlitch() and g.State == S.Glitched and g:IsInvulnerable(), "glitch")
        expect(not g:TriggerGlitch(), "glitch cooldown")
        g:Tick(Config.Omnitrix.GlitchDuration + 0.01)
        expect(g.State == S.Reverting, "glitch ends in revert")
        expect(Omnitrix.FormIndex(Omnitrix.GlitchForm, Config) == 4, "glitch -> Cannonbolt")

        local o = Omnitrix.new(Config.Omnitrix)
        o:Tick(0.1)
        expect(o:TriggerOverclock(), "overclock from human")
        o:Tick(1)
        expect(o.State == S.Overclocked and o.ActiveForm == Omnitrix.OverclockForm, "overclocked")
        expect(Config.Forms[Omnitrix.FormIndex(o.ActiveForm, Config)].Id == "WayBig", "Way Big")
        o:Tick(Config.Omnitrix.OverclockDuration + 0.1)
        expect(o.State == S.Reverting, "overclock ends")
        expect(not Omnitrix.new():Confirm(0), "confirm needs the dial")
        expect(not Omnitrix.new():QuickSwap(5), "invalid slot")

        -- Pose: every hero and form, in every movement state, gives finite angles.
        local heroes = {}
        for _, id in ipairs(Config.HeroOrder) do table.insert(heroes, { id, nil }) end
        for _, f in ipairs(Config.Forms) do table.insert(heroes, { "Ben", f.Id }) end
        local states = {
            { Grounded = true },
            { Grounded = true, Speed = 20, WalkPhase = 1.3 },
            { Grounded = true, Speed = 40, WalkPhase = 2.1 },
            { Grounded = true, IdleTime = 6 },
            { Grounded = true, EmoteTime = 1.0 },
            { Grounded = true, AttackAnim = 0.1, AttackDur = 0.25, AttackStep = 0 },
            { Grounded = true, AttackAnim = 0.2, AttackDur = 0.38, AttackStep = 2 },
            { Grounded = true, Dodge = 0.2 },
            { Grounded = false, Dodge = 0.2, DodgeAir = true },
            { Grounded = false, VelY = 20 },
            { Grounded = false, VelY = -20, FlipTime = 0.3 },
            { Grounded = false, Pounding = true },
            { Grounded = true, PoundLand = 0.2 },
            { Grounded = false, WallSlide = 0.1 },
            { Grounded = false, Flying = true, Speed3 = 60, FlyBank = 30, FlyPitch = -20, Boosting = true },
            { Grounded = false, Flying = true, Speed3 = 60, Dodge = 0.2, DodgeAir = true, DodgeSide = -1 },
            { Grounded = false, Gliding = true, Speed3 = 40, FlyBank = -20 },
            { Grounded = true, Hurt = 0.2, LandTimer = 0.1 },
        }
        local count = 0
        local function finite(v) return v == v and v ~= math.huge and v ~= -math.huge end
        for _, h in ipairs(heroes) do
            for i, st in ipairs(states) do
                st.Hero, st.Form, st.Time = h[1], h[2], 3.7
                local O = Pose.Compute(st)
                for k, v in pairs(O) do
                    if type(v) == "number" then
                        expect(finite(v), h[1] .. "/" .. tostring(h[2]) .. " state " .. i .. " " .. k)
                    elseif type(v) == "table" then
                        for s = 1, 2 do expect(finite(v[s]), h[1] .. " state " .. i .. " " .. k .. s) end
                    end
                end
                count = count + 1
            end
        end
        -- Signature behaviour checks.
        local tim = Pose.Compute({ Hero = "Timmy", Flying = true, Grounded = false, Speed3 = 40, Time = 1 })
        expect(tim.Spin == -65, "Timmy surfs sideways on the cloud")
        local dan = Pose.Compute({ Hero = "Danny", Flying = true, Grounded = false, Speed3 = 60, Time = 1 })
        expect(dan.BodyPitch > 60 and dan.Tail, "Danny flies superhero-style with his ghost tail")
        local roll = Pose.Compute({ Hero = "Jimmy", Grounded = true, Dodge = 0.2, Time = 1 })
        expect(roll.BodyPitch > 90, "dodge is a forward roll")
        local tig = Pose.Compute({ Hero = "ElTigre", Grounded = true, AttackAnim = 0.19, AttackDur = 0.38, AttackStep = 2, Time = 1 })
        expect(tig.Spin > 0, "El Tigre's finisher spins")
        return fails, count
      end
    """)
    fails, count = tests(config, omni_mod, pose)
    fails = list(fails.values())
    for f in fails:
        print("FAIL", f)
    print("pose: %d hero/state combinations evaluated" % count)
    failures += len(fails)
    print("%d failure(s)" % failures)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
