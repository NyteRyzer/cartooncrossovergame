--!nonstrict
-- Procedural character animation, ported from the prototype
-- (Prototype/src/GameCharacters.cpp). Pure: takes a character's movement
-- state, returns joint angles in degrees. Client/Controllers/Animator applies
-- them to the R15 Motor6Ds of every character, so everyone sees everyone's
-- moves without uploaded animation assets.
--
-- Angle conventions (prototype model space, converted in the Animator):
--   ShFwd/Thigh/Elbow positive = forward / bend, Knee positive = bend,
--   ShAbd positive = away from the body, Lean/BodyPitch/HeadPitch positive =
--   forward/down, Twist/Spin/HeadYaw positive = turn left, Roll positive = tilt right.
--
-- State fields (all optional): Hero, Form, Speed (horizontal studs/s), Speed3,
-- VelY, Grounded, Flying, Gliding, Pounding, PoundLand, Dodge, DodgeAir,
-- DodgeSide, AttackAnim, AttackDur, AttackStep, Hurt, LandTimer, IdleTime,
-- EmoteTime, FlipTime, WallSlide, FlyBank, FlyPitch, Boosting, TakeoffTime,
-- Breath, Jetpack, WalkPhase, Time.

local Pose = {}

local PI = math.pi
local sin, cos, abs, min, max, floor = math.sin, math.cos, math.abs, math.min, math.max, math.floor

local function clamp(v, a, b)
	if v < a then
		return a
	end
	if v > b then
		return b
	end
	return v
end

local function round(v)
	return floor(v + 0.5)
end

local function fmod(a, b)
	return a - floor(a / b) * b
end

Pose.EmoteTime = 2.4

-- Gait per hero: stride, cadence, arm swing, knee lift, bounce, hip twist, run lean.
local GAITS = {
	FourArms = { 0.85, 0.8, 0.7, 0.8, 1.8, 0.6, 0.6 },
	XLR8 = { 1.3, 1.3, 0.5, 1.4, 0.5, 0.6, 3.0 },
	WayBig = { 0.8, 0.75, 0.6, 0.8, 1.2, 0.5, 0.5 },
	Jimmy = { 0.85, 1.15, 0.8, 0.9, 0.9, 0.8, 1.4 },
	Timmy = { 1.0, 1.05, 1.5, 1.15, 1.5, 1.2, 0.8 },
	Danny = { 1.05, 0.95, 0.7, 1.0, 0.8, 1.0, 1.0 },
	Jake = { 1.1, 0.95, 1.1, 1.0, 1.4, 1.8, 0.9 },
	XJ9 = { 0.9, 1.0, 0.6, 0.35, 0.4, 0.0, 0.6 },
	ElTigre = { 1.1, 1.05, 0.8, 1.3, 0.9, 1.0, 1.6 },
	Aang = { 1.2, 0.95, 0.9, 1.25, 1.6, 1.0, 1.1 },
}

function Pose.Compute(st)
	local t = st.Time or 0
	local form = st.Form
	local human = form == nil
	local id = st.Hero or "Ben"
	local function is(h)
		return human and id == h
	end
	local speed = st.Speed or 0
	local speed3 = st.Speed3 or speed
	local grounded = st.Grounded ~= false
	local flying = st.Flying == true
	local gliding = st.Gliding == true
	local pounding = st.Pounding == true
	local dodgeT = st.Dodge or 0
	local dodge = dodgeT > 0 and not flying
	local wall = (st.WallSlide or 0) > 0 and not grounded

	local g = GAITS[form or id] or { 1, 1, 1, 1, 1, 1, 1 }
	local gStride, gCad, gArms, gKnees, gBounce, gTwist, gLean = g[1], g[2], g[3], g[4], g[5], g[6], g[7]

	local walk, run = 0, 0
	if grounded and not dodge then
		walk = min(1, speed / 20)
		run = clamp((speed - 24) / 14, 0, 1)
	end
	local ph = (st.WalkPhase or 0) * gCad
	local sw, cw = sin(ph), cos(ph)
	local idle = (1 - walk) * (grounded and 1 or 0)
	local atkDur = st.AttackDur or 0.25
	local atkAnim = st.AttackAnim or 0
	local atkA, atk = 0, 0
	if atkAnim > 0 then
		atkA = clamp(1 - atkAnim / atkDur, 0, 1)
		atk = sin(PI * atkA)
	end
	local step = st.AttackStep or 0
	local hurt = min(1, (st.Hurt or 0) / 0.25)
	local land = (st.LandTimer or 0) / 0.18
	local air = not grounded and not flying and not gliding and not pounding and not dodge and not wall
	local A = (1 + run * 0.7) * gStride

	local fid, fidT = 0, 0
	local idleTime = st.IdleTime or 0
	if idleTime > 2 then
		local f = fmod(idleTime - 2, 7)
		if f > 3 and f < 5.5 then
			fidT = (f - 3) / 2.5
			fid = min(1, sin(PI * fidT) * 1.6)
		end
	end
	if atkAnim > 0 then
		fid = 0
	end
	fid = fid * idle
	local em, emW = 0, 0
	local emoteTime = st.EmoteTime or 0
	if emoteTime > 0 then
		em = 1 - emoteTime / Pose.EmoteTime
		emW = clamp(min(em, 1 - em) * 6, 0, 1)
	end

	local O = {
		Lean = 0, HipTwist = 0, ChestTwist = 0, Bob = 0, ShiftX = 0, HipRoll = 0,
		HeadYaw = 0, HeadPitch = 0, HeadRoll = 0,
		ShFwd = { 0, 0 }, ShAbd = { 0, 0 }, Elbow = { 0, 0 },
		Thigh = { 0, 0 }, ThighAbd = { 0, 0 }, Knee = { 0, 0 }, Ankle = { 0, 0 },
		Breath = 1, BodyPitch = 0, BodyRoll = 0, Spin = 0, StaffSpin = 0,
		Tail = flying and is("Danny"),
	}
	-- Index 1 = left, 2 = right.
	local function blend(tbl, key, target, w)
		tbl[key] = tbl[key] + (target - tbl[key]) * w
	end
	local function arm(s, fwd, abd, elb, w)
		blend(O.ShFwd, s, fwd, w)
		blend(O.ShAbd, s, abd, w)
		blend(O.Elbow, s, elb, w)
	end
	local function leg(s, th, kn, w)
		blend(O.Thigh, s, th, w)
		blend(O.Knee, s, kn, w)
	end

	-- Base idle: breathing, weight shift, looking around.
	O.Breath = 1 + 0.025 * sin(t * 2.2) * idle
	O.ShiftX = sin(t * 0.8) * 1.5 * idle
	O.HipRoll = sin(t * 0.8) * 2 * idle
	O.HeadYaw = sin(t * 0.45) * 14 * idle
	O.HeadPitch = sin(t * 0.7) * 3 * idle
	for s = 1, 2 do
		O.ShAbd[s] = 9 + sin(t * 2.2) * 2 * idle
		O.Elbow[s] = 12 + 6 * idle
		O.Knee[s] = 4
	end
	-- Walk / run cycle shaped by the gait.
	O.Thigh[1] = 30 * walk * A * sw
	O.Thigh[2] = -30 * walk * A * sw
	O.Knee[1] = O.Knee[1] + (10 + 50 * A * max(0, cw)) * walk * gKnees
	O.Knee[2] = O.Knee[2] + (10 + 50 * A * max(0, -cw)) * walk * gKnees
	O.Ankle[1] = (O.Knee[1] - O.Thigh[1]) * 0.35 * walk
	O.Ankle[2] = (O.Knee[2] - O.Thigh[2]) * 0.35 * walk
	O.ShFwd[1] = -32 * walk * A * sw * gArms
	O.ShFwd[2] = 32 * walk * A * sw * gArms
	O.Elbow[1] = O.Elbow[1] + (18 + 55 * run) * walk
	O.Elbow[2] = O.Elbow[2] + (18 + 55 * run) * walk
	O.Bob = (-3 * A * abs(sw) + 1.5) * walk * gBounce
	O.HipTwist = 8 * walk * sw * gTwist
	O.ChestTwist = -10 * walk * sw * gTwist
	O.Lean = (4 * walk + 12 * run) * gLean
	O.HeadYaw = O.HeadYaw - O.ChestTwist * 0.6

	-- Personality: idle stance, walk style, fidget.
	if form == "Heatblast" then
		arm(1, 10, 28, 35, idle)
		arm(2, 10, 28, 35, idle)
		arm(1, 100, 15, 95, fid)
		arm(2, 100, 15, 95, fid)
	elseif form == "FourArms" then
		O.Breath = 1 + 0.06 * sin(t * 1.6) * idle
		blend(O.ThighAbd, 1, 10, idle)
		blend(O.ThighAbd, 2, 10, idle)
		arm(1, 5, 26, 45, idle)
		arm(2, 5, 26, 45, idle)
		O.HipRoll = O.HipRoll + sin(ph) * 6 * walk
		arm(1, 0, 95, 125, fid)
		arm(2, 0, 95, 125, fid)
	elseif form == "XLR8" then
		O.Lean = O.Lean + 15 * idle
		leg(1, 12, 20 + max(0, sin(t * 14)) * 22, idle)
		arm(1, -50, 18, 20, run)
		arm(2, -50, 18, 20, run)
	elseif form == "WayBig" then
		O.Breath = 1 + 0.04 * sin(t) * idle
		arm(1, 80, -20, 90, fid)
		arm(2, 80, -20, 90, fid)
	elseif human then
		if id == "Jimmy" then
			arm(1, 0, 32, 95, idle * 0.7)
			O.HeadPitch = O.HeadPitch + 6 * run
			if fidT < 0.6 then
				arm(2, 95, -18, 135, fid)
				O.HeadPitch = O.HeadPitch + (-10 - O.HeadPitch) * fid
				O.HeadYaw = O.HeadYaw + (12 - O.HeadYaw) * fid
			else
				arm(2, 165, 8, 10, fid)
				O.HeadPitch = O.HeadPitch + (-14 - O.HeadPitch) * fid
				O.Bob = O.Bob + 2 * fid
			end
		elseif id == "Timmy" then
			O.Bob = O.Bob + abs(sin(t * 4.5)) * 2.5 * idle
			O.HeadYaw = sin(t * 0.9) * 26 * idle + O.HeadYaw * walk
			O.ShFwd[1] = O.ShFwd[1] + sin(t * 2.2) * 10 * idle
			O.ShFwd[2] = O.ShFwd[2] + sin(t * 2.2 + PI) * 10 * idle
			O.HeadRoll = sin(ph) * 6 * walk
			arm(2, 160, 20 + sin(t * 10) * 25, 25, fid)
			O.HeadPitch = O.HeadPitch + (-10 - O.HeadPitch) * fid
		elseif id == "Danny" then
			O.ShiftX = O.ShiftX + 3 * idle
			O.HipRoll = O.HipRoll + 5 * idle
			blend(O.ThighAbd, 1, 6, idle)
			blend(O.Knee, 2, 14, idle)
			O.HeadPitch = O.HeadPitch + 4
			arm(1, 65, -38, 115, fid)
			arm(2, 65, -38, 115, fid)
			O.HeadYaw = O.HeadYaw + (-15 - O.HeadYaw) * fid
		elseif id == "Jake" then
			local beat = t * 4
			O.Bob = O.Bob - abs(sin(beat)) * 3 * idle
			O.HeadPitch = O.HeadPitch + sin(beat * 2) * 5 * idle
			O.ChestTwist = O.ChestTwist + sin(t * 2) * 8 * idle
			O.Knee[1] = O.Knee[1] + (8 + abs(sin(beat)) * 10) * idle
			O.Knee[2] = O.Knee[2] + (8 + abs(sin(beat)) * 10) * idle
			O.ShAbd[1] = O.ShAbd[1] + 10 * walk
			O.ShAbd[2] = O.ShAbd[2] + 10 * walk
			O.HeadRoll = sin(ph) * 5 * walk
			arm(2, 175, 10, 40 + sin(t * 14) * 15, fid)
			arm(1, 60, -10, 110, fid)
		elseif id == "XJ9" then
			O.Breath = 1
			O.ShiftX = O.ShiftX * (1 - idle)
			O.HipRoll = O.HipRoll * (1 - idle)
			O.HeadYaw = round(sin(t * 0.45) * 2) * 15 * idle
			arm(1, -20, 42, 95, idle)
			arm(2, -20, 42, 95, idle)
			O.Elbow[1] = O.Elbow[1] * (1 - walk) + 5 * walk
			O.Elbow[2] = O.Elbow[2] * (1 - walk) + 5 * walk
			O.Bob = O.Bob + sin(ph * 2) * walk
			arm(2, 10, 150, 30 + sin(t * 12) * 28, fid)
			O.HeadRoll = O.HeadRoll + (15 - O.HeadRoll) * fid
		elseif id == "ElTigre" then
			leg(1, 22, 38, idle)
			leg(2, 22, 38, idle)
			blend(O.ThighAbd, 1, 8, idle)
			blend(O.ThighAbd, 2, 8, idle)
			O.Ankle[1] = O.Ankle[1] + 14 * idle
			O.Ankle[2] = O.Ankle[2] + 14 * idle
			O.Bob = O.Bob - 6 * idle
			O.Lean = O.Lean + 10 * idle
			arm(1, 55, 22, 95, idle)
			arm(2, 55, 22, 95, idle)
			O.ShiftX = O.ShiftX + sin(t * 2.5) * 3 * idle
			O.Knee[1] = O.Knee[1] + 15 * walk
			O.Knee[2] = O.Knee[2] + 15 * walk
			arm(1, -55, 15, 20, run)
			arm(2, -55, 15, 20, run)
			O.Lean = O.Lean + 22 * run
			arm(1, 30, 85, 70 + sin(t * 20) * 8, fid)
			arm(2, 30, 85, 70 + sin(t * 20) * 8, fid)
			O.HeadPitch = O.HeadPitch + (-12 - O.HeadPitch) * fid
		elseif id == "Ben" then
			arm(1, 0, 6, 20, idle * 0.5)
			arm(2, 0, 6, 20, idle * 0.5)
			arm(1, 75, -25, 105, fid)
			O.HeadPitch = O.HeadPitch + (18 - O.HeadPitch) * fid
			O.HeadYaw = O.HeadYaw + (20 - O.HeadYaw) * fid
			if fidT > 0.45 then
				arm(2, 60 + sin(t * 12) * 6, -30, 95, fid)
			end
		elseif id == "Aang" then
			O.Bob = O.Bob + (2 + sin(t * 2) * 2) * idle
			arm(2, 12, 22, 40, idle)
			arm(1, -40 * run, 15, 10, run)
			arm(2, -55 * run, 15, 10, run)
			O.Lean = O.Lean + 14 * run
			arm(2, 80, 10, 30, fid)
			if fid > 0.05 then
				O.StaffSpin = t * 720
			end
		end
	end

	-- Airborne.
	if air then
		local up = (st.VelY or 0) > 0 and 1 or 0.7
		O.Thigh[1], O.Thigh[2] = 50, 18
		O.Knee[1], O.Knee[2] = 80, 35
		for s = 1, 2 do
			O.ShAbd[s] = 55 * up
			O.ShFwd[s] = -15
			O.Elbow[s] = 35
		end
		O.Lean = 6
		if is("Timmy") and (st.VelY or 0) < 0 then
			O.ShAbd[1] = 120 + sin(t * 16) * 30
			O.ShAbd[2] = 120 + cos(t * 16) * 30
			O.Thigh[1] = 30 + sin(t * 14) * 25
			O.Thigh[2] = 30 - sin(t * 14) * 25
		elseif is("XJ9") then
			O.Thigh[1], O.Thigh[2], O.Knee[1], O.Knee[2] = 5, 5, 5, 5
		elseif is("Danny") or is("Aang") then
			O.Thigh[2] = 0
			O.Knee[2] = 15
			O.ShAbd[1], O.ShAbd[2] = 25, 25
		elseif is("Jimmy") and st.Jetpack then
			O.Thigh[1], O.Thigh[2], O.Knee[1], O.Knee[2] = 6, -4, 12, 12
			arm(1, 10, 38, 20, 1)
			arm(2, 10, 38, 20, 1)
			O.HeadPitch = -10
		end
		local flip = st.FlipTime or 0
		if flip > 0 then
			local f = 1 - flip / 0.5
			O.BodyPitch = 360 * (f * f * (3 - 2 * f))
			O.Thigh[1], O.Thigh[2], O.Knee[1], O.Knee[2] = 95, 95, 115, 115
			arm(1, 60, 20, 90, 1)
			arm(2, 60, 20, 90, 1)
			O.Bob = O.Bob + sin(PI * f) * 10
		end
	end
	if (st.LandTimer or 0) > 0 then
		O.Knee[1] = O.Knee[1] + 50 * land
		O.Knee[2] = O.Knee[2] + 50 * land
		O.Thigh[1] = O.Thigh[1] + 30 * land
		O.Thigh[2] = O.Thigh[2] + 30 * land
		O.Bob = O.Bob - 10 * land
		O.Lean = O.Lean + 12 * land
	end

	-- Wall slide: facing the wall, hands (or claws) on it.
	if wall then
		arm(1, 150, 25, 40, 1)
		arm(2, 150, 25, 40, 1)
		leg(1, 40, 50, 1)
		leg(2, 20, 30, 1)
		O.Lean = -5
		O.HeadPitch = -15
	end

	-- Ground pound.
	if pounding then
		leg(1, 75, 105, 1)
		leg(2, 75, 105, 1)
		arm(1, 170, 10, 30, 1)
		arm(2, 170, 10, 30, 1)
		O.Lean = 8
	end
	local pl = (st.PoundLand or 0) / 0.35
	if pl > 0 then
		leg(1, 70, 100, pl)
		leg(2, 40, 90, pl)
		arm(1, 55, 10, 10, pl)
		arm(2, 70, 5, 5, pl)
		O.Bob = O.Bob - 20 * pl
		O.Lean = O.Lean + 30 * pl
	end

	-- Dodge.
	if dodge then
		local dur = st.DodgeAir and 0.32 or 0.4
		local d = clamp(1 - dodgeT / dur, 0, 1)
		local s = sin(PI * d)
		local tuck = min(1, s * 1.8)
		if is("Danny") then
			O.Lean = 35
			arm(1, -40, 15, 10, 1)
			arm(2, -40, 15, 10, 1)
			leg(1, -20, 40, 1)
			leg(2, -10, 30, 1)
		elseif is("XJ9") then
			O.Lean = 15
			arm(1, 0, 40, 0, 1)
			arm(2, 0, 40, 0, 1)
			leg(1, 0, 0, 1)
			leg(2, 0, 0, 1)
		elseif is("Aang") then
			O.Spin = 360 * d
			arm(1, 10, 80, 10, 1)
			arm(2, 10, 80, 10, 1)
			leg(1, 50, 80, 1)
			leg(2, 0, 10, 1)
			O.Bob = O.Bob + 12 * s
		elseif is("ElTigre") then
			O.Lean = 50 * s
			arm(1, 150, 15, 20, 1)
			arm(2, 150, 15, 20, 1)
			leg(1, -30, 50, 1)
			leg(2, -15, 40, 1)
			O.Bob = O.Bob + 10 * s
		elseif form == "FourArms" then
			O.Lean = 30
			O.ChestTwist = 30
			arm(1, 60, 10, 110, 1)
			arm(2, 60, 10, 110, 1)
		elseif form == "XLR8" then
			O.Lean = 55
			arm(1, -60, 15, 15, 1)
			arm(2, -60, 15, 15, 1)
			O.Thigh[1] = 50 * sin(t * 40)
			O.Thigh[2] = -O.Thigh[1]
		else
			-- Forward roll / air somersault: tuck tight, rotate about the hips.
			O.BodyPitch = 360 * d
			leg(1, 100, 115, tuck)
			leg(2, 100, 115, tuck)
			arm(1, 70, 15, 100, tuck)
			arm(2, 70, 15, 100, tuck)
			if not st.DodgeAir then
				O.Bob = O.Bob - 27 * s
			end
		end
	end

	-- Flight.
	if flying then
		local fwd = clamp(speed3 / 50, 0, 1)
		local hover = sin(t * 3)
		local travel = clamp(80 - (st.FlyPitch or 0) * 0.9, 0, 170)
		local roll = st.FlyBank or 0
		if dodgeT > 0 and st.DodgeAir then
			roll = roll + 360 * (1 - dodgeT / 0.45) * (st.DodgeSide or 1)
		end
		local lift = (st.TakeoffTime or 0) / 0.45
		O.Lean = 0
		O.BodyRoll = roll
		if is("Timmy") then
			-- Surfing the cloud-board.
			O.Spin = -65
			O.HeadYaw = 55
			O.BodyPitch = 14 * fwd
			O.BodyRoll = (st.FlyBank or 0) * 0.8
			leg(1, 25, 40, 1)
			leg(2, 25, 40, 1)
			O.ThighAbd[1], O.ThighAbd[2] = 16, 16
			arm(1, 10, 75 + sin(t * 1.5) * 10 - (st.FlyBank or 0) * 0.6, 20, 1)
			arm(2, 10, 75 - sin(t * 1.5) * 10 + (st.FlyBank or 0) * 0.6, 20, 1)
			O.Bob = O.Bob + hover * 2
		elseif is("XJ9") then
			-- Rocket pose.
			O.BodyPitch = travel * fwd
			O.HeadPitch = -O.BodyPitch * 0.55
			arm(1, -20 * fwd + 10 * (1 - fwd), 14 + 22 * (1 - fwd), 5, 1)
			arm(2, -20 * fwd + 10 * (1 - fwd), 14 + 22 * (1 - fwd), 5, 1)
			leg(1, 0, 2, 1)
			leg(2, 0, 2, 1)
			O.ThighAbd[1], O.ThighAbd[2] = -2, -2
			O.Bob = O.Bob + hover * 3 * (1 - fwd)
			if lift > 0 then
				arm(2, 175, 5, 5, lift)
			end
		else
			-- Superhero flight (Danny; Jake's dragon is a separate model).
			O.BodyPitch = travel * fwd
			O.HeadPitch = -O.BodyPitch * 0.6
			leg(1, -8 + hover * 6, 25, 1)
			leg(2, -2 - hover * 6, 15, 1)
			arm(2, 40 + 130 * fwd, 8, 5, 1)
			arm(1, -25, 14, 20, 1)
			if st.Boosting then
				arm(1, 40 + 130 * fwd, 8, 5, 1)
			end
			arm(1, 10, 30, 30, 1 - fwd)
			arm(2, 10, 30, 30, 1 - fwd)
			O.Bob = O.Bob + hover * 4 * (1 - fwd)
			if lift > 0 then
				arm(2, 178, 5, 5, lift)
			end
		end
	end
	if gliding then
		local fwd = clamp(speed3 / 40, 0, 1)
		if form == "Heatblast" then
			arm(1, -10, 22, 0, 1)
			arm(2, -10, 22, 0, 1)
			leg(1, 10, 20, 1)
			leg(2, -5, 10, 1)
			O.BodyRoll = (st.FlyBank or 0) * 0.5
		else
			-- Aang lies prone under the glider.
			O.BodyPitch = (70 - (st.FlyPitch or 0) * 0.5) * fwd
			O.BodyRoll = st.FlyBank or 0
			O.HeadPitch = -O.BodyPitch * 0.6
			arm(1, 170, 18, 15, 1)
			arm(2, 170, 18, 15, 1)
			leg(1, -6, 10, 1)
			leg(2, 4, 15, 1)
		end
	end

	-- Attacks: each hero, each combo hit.
	if atk > 0 then
		local S = (step % 2 == 0) and 2 or 1
		local sd = (S == 2) and 1 or -1
		local fin = step == 2
		local sweep = 1 - 2 * atkA
		if form == "FourArms" then
			if fin then
				local v = 170 - 120 * atkA
				arm(1, v, 10, 20, atk)
				arm(2, v, 10, 20, atk)
				O.Lean = O.Lean + 20 * atk
			else
				arm(S, 95, 0, 8, atk)
				O.ChestTwist = O.ChestTwist + sd * 28 * atk
				O.Lean = O.Lean + 8 * atk
			end
		elseif form == "Heatblast" then
			local throw
			if atkA < 0.3 then
				throw = -40 * (atkA / 0.3)
			else
				throw = 140 * sin(PI * (atkA - 0.3) / 0.7)
			end
			arm(S, throw, 10, 30, 1)
			if fin then
				arm(3 - S, throw, 10, 30, 1)
			end
			O.ChestTwist = O.ChestTwist + sd * 20 * atk
		elseif form == "XLR8" then
			local q = (floor(atkA * 4) % 2) + 1
			arm(q, 90 * sin(PI * fmod(atkA * 4, 1)), 0, 5, 1)
			O.Lean = O.Lean + 15 * atk
		elseif form == "WayBig" then
			leg(2, 70 * atk, 60 * atk, 1)
			arm(1, 30, 40, 20, atk)
			arm(2, 30, 40, 20, atk)
		elseif human then
			if id == "Jimmy" then
				local aim = min(1, atk * 2)
				local recoil = 0
				if atkA < 0.3 then
					recoil = sin(PI * atkA / 0.3)
				end
				arm(2, 88 + 15 * recoil, 2, 4 + 20 * recoil, aim)
				O.ChestTwist = O.ChestTwist + 18 * aim
				if fin then
					arm(1, 80, -30, 45, aim)
					O.Lean = O.Lean - 10 * recoil
				end
			elseif id == "Timmy" then
				if fin then
					arm(2, 170, sin(atkA * 2 * PI) * 40, 20, atk)
					O.Bob = O.Bob + 6 * atk
				elseif step == 0 then
					arm(2, 120, 70 - 100 * atkA, 25, atk)
				else
					arm(2, 120, -30 + 100 * atkA, 25, atk)
				end
			elseif id == "Danny" then
				if fin then
					arm(1, 92, 0, 5, atk)
					arm(2, 92, 0, 5, atk)
					O.Lean = O.Lean + 10 * atk
				else
					arm(S, 92, 0, 5, atk)
					O.ChestTwist = O.ChestTwist + sd * 22 * atk
				end
			elseif id == "Jake" then
				if fin then
					local v = 170 - 130 * atkA
					arm(1, v, 15, 15, min(1, atk * 2))
					arm(2, v, 15, 15, min(1, atk * 2))
					O.Lean = O.Lean + 18 * atk
				else
					arm(S, 80, 70 * sweep, 15, atk)
					O.ChestTwist = O.ChestTwist + sd * 30 * sweep * atk
					O.Lean = O.Lean + 8 * atk
				end
			elseif id == "ElTigre" then
				if fin then
					O.Spin = 360 * atkA
					arm(1, 20, 90, 10, atk)
					arm(2, 20, 90, 10, atk)
				else
					arm(S, 70, 60 * sweep, 20, atk)
					O.ChestTwist = O.ChestTwist + sd * 30 * sweep * atk
					O.Lean = O.Lean + 12 * atk
				end
			elseif id == "XJ9" then
				arm(2, 90, 2 + sin(t * 70) * 2, 0, min(1, atk * 3))
				O.ChestTwist = O.ChestTwist + 20 * atk
			elseif id == "Ben" then
				if fin then
					leg(2, 95, 15, atk)
					O.Lean = O.Lean - 12 * atk
					arm(1, 10, 40, 30, atk)
					arm(2, 10, 40, 30, atk)
				else
					arm(S, 95, 0, 5, atk)
					O.ChestTwist = O.ChestTwist + sd * 25 * atk
					O.Lean = O.Lean + 8 * atk
				end
			elseif id == "Aang" then
				if fin then
					O.Spin = 360 * atkA
					arm(1, 30, 70, 10, atk)
					arm(2, 30, 70, 10, atk)
					O.StaffSpin = 720 * atkA
				else
					arm(S, 90, -5, 5, atk)
					O.ChestTwist = O.ChestTwist + sd * 20 * atk
					O.Lean = O.Lean + 6 * atk
				end
			end
		end
	end
	if st.Breath then
		O.HeadPitch = O.HeadPitch - 12
	end

	-- Emote: the hero's signature pose.
	if emW > 0 then
		local w = emW
		local function head(key, v)
			O[key] = O[key] + (v - O[key]) * w
		end
		if form == "FourArms" then
			arm(1, 0, 95, 130, w)
			arm(2, 0, 95, 130, w)
			head("HeadPitch", -10)
		elseif form == "Heatblast" then
			arm(1, 10, 140, 30, w)
			arm(2, 10, 140, 30, w)
		elseif form == "XLR8" then
			arm(2, 70, 10, 10, w)
			head("Lean", 20)
		elseif form == "WayBig" then
			arm(1, 80, -20, 90, w)
			arm(2, 80, -20, 90, w)
		elseif human then
			if id == "Jimmy" then
				arm(2, 170, -5, 10, w)
				arm(1, 0, 35, 100, w)
				head("HeadPitch", -15)
				head("HeadRoll", 8)
				O.Bob = O.Bob + 2 * w
			elseif id == "Timmy" then
				arm(1, 10, 150 + sin(t * 14) * 12, 20, w)
				arm(2, 10, 150 + cos(t * 14) * 12, 20, w)
				O.Bob = O.Bob + abs(sin(t * 7)) * 9 * w
			elseif id == "Danny" then
				if em < 0.45 then
					arm(1, 70, -40, 60, w)
					arm(2, 70, -40, 60, w)
					head("HeadPitch", 10)
				else
					arm(1, 10, 110, 5, w)
					arm(2, 10, 110, 5, w)
					head("HeadPitch", -10)
					O.Bob = O.Bob + 3 * w
				end
			elseif id == "Jake" then
				arm(2, 175, 10, 25 + sin(t * 12) * 15, w)
				arm(1, 0, 35, 100, w)
				leg(1, 15, 20, w)
				leg(2, 15, 20, w)
				head("HeadPitch", -10)
			elseif id == "XJ9" then
				arm(2, 10, 150, 35 + sin(t * 12) * 28, w)
				arm(1, -20, 42, 95, w)
				head("HeadRoll", 15)
			elseif id == "ElTigre" then
				arm(1, 30, 120, 70, w)
				arm(2, 30, 120, 70, w)
				leg(1, 30, 40, w)
				leg(2, 30, 40, w)
				blend(O.ThighAbd, 1, 15, w)
				blend(O.ThighAbd, 2, 15, w)
				head("HeadPitch", -18)
			elseif id == "Ben" then
				arm(1, 80, -30, 100, w)
				if em < 0.5 then
					arm(2, 110, -10, 100, w)
				else
					arm(2, 70, -35, 95, w)
				end
				head("HeadPitch", 15)
			elseif id == "Aang" then
				local bow = sin(PI * clamp((em - 0.1) / 0.6, 0, 1))
				head("Lean", 45 * bow)
				arm(1, 75, -32, 110, w)
				arm(2, 75, -32, 110, w)
				head("HeadPitch", 15)
				if em > 0.7 then
					O.StaffSpin = t * 900
				end
			end
		end
	end
	if hurt > 0 then
		O.Lean = O.Lean - 15 * hurt
		O.HeadPitch = O.HeadPitch - 14 * hurt
		O.ShAbd[1] = O.ShAbd[1] + 30 * hurt
		O.ShAbd[2] = O.ShAbd[2] + 30 * hurt
	end
	return O
end

return Pose
