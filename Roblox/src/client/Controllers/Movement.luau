--!nonstrict
-- The local hero's movement, ported from the prototype (Prototype/src/Game.cpp):
-- momentum flight with per-hero handling, banking, boost and barrel rolls;
-- Aang's glider (dive for speed, climb to slow, stall); dodge roll / air dash;
-- ground pound; double jumps with flips; wall jumps and El Tigre's wall cling;
-- coyote time and short hops; Jimmy's jetpack; El Tigre's grappling chain;
-- Danny's intangibility; Aang's air scooter and earth-pillar launch; emotes.
--
-- The client owns its character's physics on Roblox, so this drives the
-- HumanoidRootPart with a LinearVelocity + AlignOrientation pair when the
-- normal Humanoid walking isn't in charge. The server only hears intent and a
-- compact state report (Remotes "State") used for animation and immunity.

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))

local Movement = {}
local player = Players.LocalPlayer
local Input, CameraRig, Hud
local remotes

local S = {} -- local movement state (also read by Animator, CameraRig, Hud)
local mover -- { Att, Lin, Align }
local lastSent = {}
local sendClock = 0

local function reset()
	S = {
		Flying = false, Gliding = false, Pounding = false, PoundLand = 0, Dodge = 0, DodgeAir = false, DodgeSide = 1,
		DodgeDir = Vector3.new(0, 0, -1), DodgeCd = 0, AirDashUsed = false, AirJumpsLeft = 0, FlipTime = 0,
		WallSlide = 0, WallTimer = 0, WallNormal = Vector3.zero, EmoteTime = 0, IdleTime = 0, LandTimer = 0,
		Boosting = false, WasBoosting = false, BoostFx = 0, FlyBank = 0, FlyPitch = 0, TakeoffTime = 0, FlyVel = Vector3.zero,
		GlideSpeed = 0, Energy = Config.Player.MaxEnergy, Jetpack = 0, Scooter = 0, Grapple = nil, Intangible = false,
		WalkPhase = 0, Coyote = 0, Jumped = false, JumpCut = false, Grounded = true, WasGrounded = true,
		AttackAnim = 0, AttackDur = 0.25, AttackStep = 0, LastAttack = -10, AttackCd = 0, SigCd = 0, Sig2Cd = 0,
		Breath = false, LastSafe = nil, FaceYaw = nil,
	}
end
reset()

function Movement.state()
	return S
end

local function charParts()
	local character = player.Character
	local humanoid = character and character:FindFirstChildOfClass("Humanoid")
	local root = character and character:FindFirstChild("HumanoidRootPart")
	if humanoid and root and humanoid.Health > 0 then
		return character, humanoid, root
	end
	return nil
end

local function heroId()
	local character = player.Character
	return character and character:GetAttribute("Hero") or "Jimmy"
end

local function hero()
	return Config.Heroes[heroId()] or Config.Heroes.Jimmy
end

local function formId()
	local character = player.Character
	return character and character:GetAttribute("Form")
end

local function formDef()
	local id = formId()
	if not id then
		return nil
	end
	for _, f in ipairs(Config.Forms) do
		if f.Id == id then
			return f
		end
	end
	return nil
end

local function scale()
	local _, humanoid, root = charParts()
	if not humanoid then
		return 1
	end
	return math.max(0.5, (humanoid.HipHeight + root.Size.Y * 0.5) / 3)
end

local function ensureMover(root)
	if mover and mover.Att.Parent == root then
		return mover
	end
	local att = Instance.new("Attachment")
	att.Name = "MoveAttachment"
	att.Parent = root
	local lin = Instance.new("LinearVelocity")
	lin.Name = "MoveVelocity"
	lin.Attachment0 = att
	lin.RelativeTo = Enum.ActuatorRelativeTo.World
	lin.MaxForce = 0
	lin.Parent = root
	local align = Instance.new("AlignOrientation")
	align.Name = "MoveOrientation"
	align.Mode = Enum.OrientationAlignmentMode.OneAttachment
	align.Attachment0 = att
	align.MaxTorque = 0
	align.Responsiveness = 30
	align.Parent = root
	mover = { Att = att, Lin = lin, Align = align }
	return mover
end

-- mode: nil (humanoid walks), "Vector" (full velocity), "Plane" (horizontal only).
local function drive(root, mode, velocity, faceYaw)
	local m = ensureMover(root)
	if not mode then
		m.Lin.MaxForce = 0
		m.Align.MaxTorque = 0
		return
	end
	local mass = root.AssemblyMass
	if mode == "Plane" then
		m.Lin.VelocityConstraintMode = Enum.VelocityConstraintMode.Plane
		m.Lin.PrimaryTangentAxis = Vector3.new(1, 0, 0)
		m.Lin.SecondaryTangentAxis = Vector3.new(0, 0, 1)
		m.Lin.PlaneVelocity = Vector2.new(velocity.X, velocity.Z)
	else
		m.Lin.VelocityConstraintMode = Enum.VelocityConstraintMode.Vector
		m.Lin.VectorVelocity = velocity
	end
	m.Lin.MaxForce = mass * 4000
	if faceYaw then
		m.Align.CFrame = CFrame.Angles(0, math.rad(faceYaw), 0)
		m.Align.MaxTorque = mass * 5000
	else
		m.Align.MaxTorque = 0
	end
end

local function yawOf(v)
	return math.deg(math.atan2(-v.X, -v.Z))
end

local function angleDelta(from, to)
	return (to - from + 540) % 360 - 180
end

local function approach(cur, target, step)
	local d = target - cur
	if math.abs(d) <= step then
		return target
	end
	return cur + (d > 0 and step or -step)
end

-- Turns unit vector `from` toward unit vector `to` by at most maxRad.
local function rotateToward(from, to, maxRad)
	local c = math.clamp(from:Dot(to), -1, 1)
	local angle = math.acos(c)
	if angle <= maxRad or angle < 1e-4 then
		return to
	end
	local axis = from:Cross(to)
	if axis.Magnitude < 1e-4 then
		axis = from:Cross(Vector3.new(0, 1, 0))
	end
	if axis.Magnitude < 1e-4 then
		axis = Vector3.new(1, 0, 0)
	end
	local perp = axis.Unit:Cross(from).Unit
	return (from * math.cos(maxRad) + perp * math.sin(maxRad)).Unit
end

local function fire(name, ...)
	local r = remotes and remotes:FindFirstChild(name)
	if r then
		r:FireServer(...)
	end
end

local function say(text, color)
	if Hud then
		Hud.message(text, color)
	end
end

local function groundBelow(root, humanoid, extra)
	local params = RaycastParams.new()
	params.FilterType = Enum.RaycastFilterType.Exclude
	params.FilterDescendantsInstances = { player.Character }
	local reach = humanoid.HipHeight + root.Size.Y * 0.5 + (extra or 0.6)
	return workspace:Raycast(root.Position, Vector3.new(0, -reach, 0), params)
end

-- Abilities --------------------------------------------------------------------------

local function startFlight(root, humanoid)
	S.Flying = true
	S.Gliding = false
	S.Pounding = false
	S.EmoteTime = 0
	S.TakeoffTime = 0.45
	local v = root.AssemblyLinearVelocity
	S.FlyVel = Vector3.new(v.X, math.max(v.Y, S.Grounded and 32 or 10), v.Z)
	humanoid.AutoRotate = false
	humanoid:ChangeState(Enum.HumanoidStateType.Freefall)
end

local function stopFlight(humanoid)
	S.Flying = false
	S.Boosting = false
	humanoid.AutoRotate = true
end

local function toggleIntangible(root)
	local _, humanoid = charParts()
	if not S.Intangible then
		if S.Energy < 15 then
			say("Not enough ecto energy", Color3.fromRGB(120, 255, 150))
			return
		end
		S.Intangible = true
		if not S.Flying then
			startFlight(root, humanoid)
		end
		say("Going ghost... intangible!", Color3.fromRGB(120, 255, 150))
	else
		S.WantSolid = true
	end
end

local function insideGeometry(root)
	local params = OverlapParams.new()
	params.FilterType = Enum.RaycastFilterType.Exclude
	params.FilterDescendantsInstances = { player.Character, workspace:FindFirstChild("Shards"), workspace:FindFirstChild("Enemies") }
	local parts = workspace:GetPartBoundsInBox(root.CFrame, root.Size * Vector3.new(1, 2.4, 1), params)
	for _, p in ipairs(parts) do
		if p.CanCollide then
			return true
		end
	end
	return false
end

local function aimDir(root)
	local target = CameraRig.aimPoint()
	local chest = root.Position + Vector3.new(0, 0.8 * scale(), 0)
	local d = target - chest
	if d.Magnitude < 0.01 then
		return CameraRig.lookVector()
	end
	return d.Unit
end

local function attackDef()
	local f = formDef()
	if f then
		return f.Attack
	end
	return hero().Attack
end

local function tryAttack()
	local character, humanoid, root = charParts()
	if not character or S.Intangible or humanoid.SeatPart then
		return
	end
	local a = attackDef()
	if not a or S.AttackCd > 0 then
		return
	end
	local now = os.clock()
	local combo = a.Cooldown >= 0.2
	if combo and now - S.LastAttack < a.Cooldown + Config.Moves.ComboWindow then
		S.AttackStep = (S.AttackStep + 1) % 3
	else
		S.AttackStep = 0
	end
	S.LastAttack = now
	local fin = combo and S.AttackStep == 2
	S.AttackDur = fin and 0.38 or 0.25
	S.AttackAnim = S.AttackDur
	S.AttackCd = a.Cooldown + (fin and 0.15 or 0)
	S.EmoteTime = 0
	local dir = aimDir(root)
	-- Face the target for the swing; melee lunges a little.
	S.FaceYaw = yawOf(Vector3.new(dir.X, 0, dir.Z))
	S.FaceTime = S.AttackDur
	if a.Kind == "Melee" and S.Grounded and not S.Flying then
		local flat = Vector3.new(dir.X, 0, dir.Z)
		if flat.Magnitude > 0.01 then
			root.AssemblyLinearVelocity = root.AssemblyLinearVelocity + flat.Unit * (fin and 17 or 10)
		end
	end
	fire("Attack", dir)
end

local function onSig(down)
	local character, humanoid, root = charParts()
	if not character then
		return
	end
	local id, f = heroId(), formId()
	if id == "Ben" then
		if down then
			if Hud and Hud.dialOpen() then
				fire("Omnitrix", "cancel")
				Hud.setDial(false)
			else
				fire("Omnitrix", "dial")
				Hud.setDial(true)
			end
		end
		return
	end
	if f then
		return
	end
	if id == "ElTigre" then
		if down then
			local cam = workspace.CurrentCamera.CFrame
			local params = RaycastParams.new()
			params.FilterType = Enum.RaycastFilterType.Exclude
			params.FilterDescendantsInstances = { character, workspace:FindFirstChild("Shards") }
			local hit = workspace:Raycast(cam.Position, cam.LookVector * 230, params)
			if hit and (hit.Position - root.Position).Magnitude > 7 then
				S.Grapple = hit.Position
				local v = root.AssemblyLinearVelocity
				root.AssemblyLinearVelocity = Vector3.new(v.X, math.max(v.Y, 13), v.Z)
			else
				say("Nothing to grapple in range", Color3.fromRGB(255, 200, 120))
			end
		else
			if S.Grapple and not S.Grounded then
				S.FlipTime = 0.5
			end
			S.Grapple = nil
		end
		return
	end
	if id == "Jake" then
		S.Breath = down
		fire("Ability", "Breath", aimDir(root), down)
		return
	end
	if not down then
		return
	end
	if id == "Jimmy" then
		if S.SigCd <= 0 and S.Energy >= 25 then
			S.Energy = S.Energy - 25
			S.SigCd = 0.6
			S.Jetpack = 0.8
			local v = root.AssemblyLinearVelocity
			root.AssemblyLinearVelocity = Vector3.new(v.X, 45, v.Z)
		end
	elseif id == "Danny" then
		toggleIntangible(root)
	elseif id == "Aang" then
		if S.SigCd <= 0 then
			S.SigCd = 1
			S.Scooter = 5
			say("Air Scooter!", Color3.fromRGB(200, 230, 255))
		end
	else
		fire("Ability", "Sig", aimDir(root))
	end
end

local function onSig2(down)
	if not down then
		return
	end
	local character, _, root = charParts()
	if not character or formId() then
		return
	end
	local id = heroId()
	if id == "Aang" then
		if S.Sig2Cd <= 0 and S.Grounded then
			S.Sig2Cd = 2
			fire("Ability", "Sig2", aimDir(root))
			task.delay(0.08, function()
				if root.Parent then
					local v = root.AssemblyLinearVelocity
					root.AssemblyLinearVelocity = Vector3.new(v.X, 57, v.Z)
				end
			end)
		end
	elseif id == "Danny" then
		fire("Ability", "Sig2", aimDir(root))
	end
end

local function onUlt(down)
	if not down then
		return
	end
	local character, _, root = charParts()
	if not character then
		return
	end
	if heroId() == "Ben" then
		fire("Omnitrix", "overclock")
	elseif (character:GetAttribute("Meter") or 0) >= 100 then
		fire("Ability", "Ult", aimDir(root))
	else
		say("Ultimate meter not full yet (fight to charge it)", Color3.fromRGB(200, 200, 255))
	end
end

local function onFly(down)
	if not down then
		return
	end
	local character, humanoid, root = charParts()
	if not character or humanoid.SeatPart then
		return
	end
	if not hero().CanFly or formId() then
		return
	end
	if S.Flying then
		if S.Intangible then
			say("Turn solid first (Q) to land", Color3.fromRGB(200, 255, 200))
			return
		end
		stopFlight(humanoid)
	else
		startFlight(root, humanoid)
	end
end

local function onDodge(down)
	if not down then
		return
	end
	local character, humanoid, root = charParts()
	if not character or humanoid.SeatPart or S.DodgeCd > 0 or S.Grapple then
		return
	end
	local f = formId()
	if f == "Cannonbolt" or f == "WayBig" then
		return
	end
	local cam = workspace.CurrentCamera.CFrame
	local fwd = Vector3.new(cam.LookVector.X, 0, cam.LookVector.Z).Unit
	local right = Vector3.new(cam.RightVector.X, 0, cam.RightVector.Z).Unit
	local mv = Input.move()
	if S.Flying then
		-- Barrel roll: a quick sidestep with a full roll.
		S.DodgeSide = mv.X < -0.1 and -1 or 1
		S.FlyVel = S.FlyVel + right * S.DodgeSide * 53
		S.Dodge = Config.Moves.BarrelRollTime
		S.DodgeAir = true
		S.DodgeCd = 0.55
	elseif S.Grounded or not S.AirDashUsed then
		local wish = fwd * mv.Y + right * mv.X
		if wish.Magnitude < 0.1 then
			wish = root.CFrame.LookVector * Vector3.new(1, 0, 1)
		end
		S.DodgeDir = wish.Unit
		S.DodgeAir = not S.Grounded
		if S.DodgeAir then
			S.AirDashUsed = true
			local v = root.AssemblyLinearVelocity
			root.AssemblyLinearVelocity = Vector3.new(v.X, math.max(v.Y, 6), v.Z)
		end
		S.Dodge = S.DodgeAir and Config.Moves.AirDodgeTime or Config.Moves.DodgeTime
		S.DodgeCd = Config.Moves.DodgeCooldown
		S.EmoteTime = 0
		S.Pounding = false
		S.FaceYaw = yawOf(S.DodgeDir)
		S.FaceTime = S.Dodge
	end
end

local function onDown(down)
	if not down then
		return
	end
	local character, humanoid, root = charParts()
	if not character or S.Grounded or S.Flying or S.Pounding or S.Intangible or S.Grapple or humanoid.SeatPart then
		return
	end
	if formId() == "Cannonbolt" then
		return
	end
	if not groundBelow(root, humanoid, 5) then
		S.Pounding = true
		S.Gliding = false
		S.FlipTime = 0
		S.Dodge = 0
	end
end

local function onEmote(down)
	if not down or not S.Grounded or S.Flying or S.EmoteTime > 0 or formId() == "Cannonbolt" then
		return
	end
	S.EmoteTime = Config.Moves.EmoteTime
	local lines = {
		Jimmy = "Brain blast!", Timmy = "Cosmo! Wanda!", Danny = "Going ghost!", Jake = "Dragon up!",
		XJ9 = "Hi! XJ9, ready to save the day!", ElTigre = "TIGRE!", Ben = "It's hero time!", Aang = "Yip yip!",
	}
	say(formId() or lines[heroId()] or "", hero().Accent)
end

local function onJumpRequest()
	local character, humanoid, root = charParts()
	if not character or humanoid.SeatPart or S.Flying then
		return
	end
	local v = root.AssemblyLinearVelocity
	local jumpV = math.sqrt(2 * workspace.Gravity * humanoid.JumpHeight)
	if S.Grounded then
		S.JumpCut = true
		S.Jumped = true
		S.EmoteTime = 0
		return
	end
	-- Coyote time: just walked off a ledge.
	if not S.Jumped and S.Coyote > 0 then
		root.AssemblyLinearVelocity = Vector3.new(v.X, jumpV, v.Z)
		S.Jumped = true
		S.JumpCut = true
		S.Coyote = 0
		return
	end
	local f = formId()
	if S.WallTimer > 0 and (not f or f == "XLR8") and not S.JustJumped then
		local out = S.WallNormal * Config.Moves.WallJumpOut
		root.AssemblyLinearVelocity = Vector3.new(out.X, jumpV * 0.95, out.Z)
		S.WallTimer = 0
		S.WallSlide = 0
		S.AirDashUsed = false
		S.JumpCut = true
		S.FaceYaw = yawOf(S.WallNormal)
		S.FaceTime = 0.25
		if heroId() == "ElTigre" or heroId() == "Aang" then
			S.FlipTime = 0.5
		end
		return
	end
	if S.AirJumpsLeft > 0 and not f then
		S.AirJumpsLeft = S.AirJumpsLeft - 1
		root.AssemblyLinearVelocity = Vector3.new(v.X, jumpV * 0.9, v.Z)
		S.FlipTime = 0.5
		S.JumpCut = false
	end
end

-- Per-frame --------------------------------------------------------------------------

local function updateFlight(dt, humanoid, root)
	local id = heroId()
	local h = hero()
	local style = Config.Flight[id] or Config.Flight.Danny
	local cam = workspace.CurrentCamera.CFrame
	local right = Vector3.new(cam.RightVector.X, 0, cam.RightVector.Z).Unit
	local mv = S.EmoteTime > 0 and Vector2.zero or Input.move()
	local wish = cam.LookVector * mv.Y + right * mv.X
	local vertical = (Input.jumpHeld() and 1 or 0) - (Input.held("Down") and 1 or 0)
	wish = wish + Vector3.new(0, vertical * 0.75, 0)
	if wish.Magnitude > 1 then
		wish = wish.Unit
	end
	local wishLen = wish.Magnitude
	local boost = Input.held("Boost") and S.Energy > 1 and wishLen > 0.1
	if boost then
		S.Energy = math.max(0, S.Energy - style.BoostCost * dt)
	end
	S.Boosting = boost

	local old = S.FlyVel
	local spd = old.Magnitude
	local dir = spd > 0.05 and old / spd or Vector3.new(0, 1, 0)
	local v
	if wishLen > 0.05 then
		local wishDir = wish.Unit
		local top = (boost and h.FlyBoost or h.FlySpeed) * wishLen - dir.Y * style.DiveGain
		local slow = math.clamp(1 - spd / h.FlySpeed, 0, 1)
		local turn = math.rad(style.TurnRate * (1 + 4 * slow) * (boost and 0.8 or 1)) * dt
		local newDir = spd > 5 and rotateToward(dir, wishDir, turn) or wishDir
		local against = math.max(0, -dir:Dot(wishDir))
		spd = approach(spd, top * (1 - 0.5 * against), (spd < top and style.Accel or style.Accel * 0.7) * dt)
		v = newDir * spd
	else
		spd = approach(spd, 0, style.Brake * dt)
		v = dir * spd
	end
	if S.TakeoffTime > 0 then
		v = Vector3.new(v.X, math.max(v.Y, 23 * S.TakeoffTime / 0.45), v.Z)
	end
	S.FlyVel = v

	-- Visual bank from the turn rate and strafing, pitch from the climb angle.
	local flat = Vector3.new(v.X, 0, v.Z)
	local oldFlat = Vector3.new(old.X, 0, old.Z)
	local yawRate = 0
	if flat.Magnitude > 3 and oldFlat.Magnitude > 3 and dt > 0 then
		yawRate = -angleDelta(yawOf(oldFlat), yawOf(flat)) / dt
	end
	local bankTarget = math.clamp(yawRate * 0.22 + mv.X * 20, -style.MaxBank, style.MaxBank) * math.clamp(flat.Magnitude / 17, 0, 1)
	S.FlyBank = S.FlyBank + (bankTarget - S.FlyBank) * math.min(1, dt * 5)
	local pitchTarget = spd > 5 and math.deg(math.atan2(v.Y, flat.Magnitude)) or 0
	S.FlyPitch = S.FlyPitch + (pitchTarget - S.FlyPitch) * math.min(1, dt * 6)

	if boost and not S.WasBoosting and spd > 13 then
		S.BoostFx = 0.4
		CameraRig.shake(0.35)
		if Movement.onBoom then
			Movement.onBoom(root.Position, dir)
		end
	end
	S.WasBoosting = boost
	local face = flat.Magnitude > 2 and yawOf(flat) or (S.FaceYaw or CameraRig.yaw())
	if S.AttackAnim > 0 then
		face = CameraRig.yaw()
	end
	drive(root, "Vector", v, face)

	-- Land by holding descend into the ground.
	if Input.held("Down") and not S.Intangible and groundBelow(root, humanoid, 1.2) then
		stopFlight(humanoid)
		S.LandTimer = 0.18
	end
end

local function updateGlide(dt, root)
	local G = Config.Glide
	local flat = Vector3.new(root.AssemblyLinearVelocity.X, 0, root.AssemblyLinearVelocity.Z)
	if not S.Gliding then
		S.Gliding = true
		S.GlideSpeed = math.max(flat.Magnitude, 32)
	end
	local camPitch = math.deg(math.asin(math.clamp(CameraRig.lookVector().Y, -1, 1)))
	local pitch = math.rad(math.clamp(camPitch + 8, -55, 25))
	S.GlideSpeed = math.clamp(S.GlideSpeed + (-math.sin(pitch) * G.DiveAccel - (S.GlideSpeed - G.Cruise) * 0.35) * dt, G.MinSpeed, G.MaxSpeed)
	local mv = Input.move()
	local travel = flat.Magnitude > 2 and yawOf(flat) or CameraRig.yaw()
	local target = CameraRig.yaw() - mv.X * 40
	local maxTurn = (mv.Y < -0.5 and 160 or 110) * dt
	local step = math.clamp(angleDelta(travel, target), -maxTurn, maxTurn)
	local newYaw = travel + step
	if mv.Y < -0.5 then
		S.GlideSpeed = math.max(G.MinSpeed, S.GlideSpeed - 30 * dt)
	end
	local forward = CFrame.Angles(0, math.rad(newYaw), 0).LookVector
	local horizontal = forward * S.GlideSpeed * math.cos(pitch)
	local vy = math.sin(pitch) * S.GlideSpeed * 0.7 - 5.7
	if S.GlideSpeed < G.StallSpeed then
		vy = vy - (G.StallSpeed - S.GlideSpeed) * 1.6
	end
	local curY = root.AssemblyLinearVelocity.Y
	vy = approach(curY, vy, 100 * dt)
	drive(root, "Vector", Vector3.new(horizontal.X, vy, horizontal.Z), newYaw)
	local yawRate = dt > 0 and -step / dt or 0
	S.FlyBank = S.FlyBank + (math.clamp(yawRate * 0.35, -45, 45) - S.FlyBank) * math.min(1, dt * 5)
	S.FlyPitch = S.FlyPitch + (math.deg(math.atan2(vy, math.max(1, horizontal.Magnitude))) - S.FlyPitch) * math.min(1, dt * 6)
end

local function updateWalls(root, humanoid)
	local mv = Input.move()
	local cam = workspace.CurrentCamera.CFrame
	local wish = Vector3.new(cam.LookVector.X, 0, cam.LookVector.Z).Unit * mv.Y + Vector3.new(cam.RightVector.X, 0, cam.RightVector.Z).Unit * mv.X
	local probe = wish.Magnitude > 0.2 and wish.Unit or root.CFrame.LookVector * Vector3.new(1, 0, 1)
	if probe.Magnitude < 0.1 then
		return
	end
	local params = RaycastParams.new()
	params.FilterType = Enum.RaycastFilterType.Exclude
	params.FilterDescendantsInstances = { player.Character, workspace:FindFirstChild("Shards"), workspace:FindFirstChild("Enemies") }
	local hit = workspace:Raycast(root.Position, probe.Unit * (root.Size.X * 0.5 + 1.2), params)
	if hit and math.abs(hit.Normal.Y) < 0.5 and hit.Instance.CanCollide then
		S.WallTimer = 0.22
		S.WallNormal = Vector3.new(hit.Normal.X, 0, hit.Normal.Z).Unit
		if wish.Magnitude > 0.3 and wish.Unit:Dot(S.WallNormal) < -0.3 and root.AssemblyLinearVelocity.Y < 0 then
			S.WallSlide = 0.15
		end
	end
end

local function step(dt)
	local character, humanoid, root = charParts()
	if not character then
		return
	end
	local f = formDef()
	local h = hero()
	local id = heroId()

	-- Timers.
	S.AttackCd = S.AttackCd - dt
	S.SigCd = S.SigCd - dt
	S.Sig2Cd = S.Sig2Cd - dt
	S.AttackAnim = math.max(0, S.AttackAnim - dt)
	S.FlipTime = math.max(0, S.FlipTime - dt)
	S.PoundLand = math.max(0, S.PoundLand - dt)
	S.TakeoffTime = math.max(0, S.TakeoffTime - dt)
	S.LandTimer = math.max(0, S.LandTimer - dt)
	S.Jetpack = math.max(0, S.Jetpack - dt)
	S.Scooter = math.max(0, S.Scooter - dt)
	S.BoostFx = math.max(0, S.BoostFx - dt)
	S.DodgeCd = S.DodgeCd - dt
	S.WallTimer = S.WallTimer - dt
	S.WallSlide = S.WallSlide - dt
	S.FaceTime = (S.FaceTime or 0) - dt
	if S.Dodge > 0 then
		S.Dodge = math.max(0, S.Dodge - dt)
	end
	if not S.Intangible and not S.Boosting and not S.Breath then
		S.Energy = math.min(Config.Player.MaxEnergy, S.Energy + Config.Player.EnergyRegen * dt)
	end
	if humanoid.SeatPart then
		if S.Flying then
			stopFlight(humanoid)
		end
		drive(root, nil)
		return
	end
	if (not h.CanFly or f) and S.Flying then
		stopFlight(humanoid)
	end
	if Input.held("Attack") then
		local a = attackDef()
		if a and (a.Cooldown < 0.25 or not S.AttackHeldBefore) then
			tryAttack()
		end
		S.AttackHeldBefore = true
	else
		S.AttackHeldBefore = false
	end

	-- Ground contact.
	S.WasGrounded = S.Grounded
	local floor = humanoid.FloorMaterial ~= Enum.Material.Air
	S.Grounded = floor and not S.Flying
	local vel = root.AssemblyLinearVelocity
	if S.Grounded and not S.WasGrounded then
		if vel.Y < -10 or S.Pounding then
			S.LandTimer = 0.18
		end
		S.AirDashUsed = false
		S.FlipTime = 0
		S.Jumped = false
		if S.Pounding then
			S.Pounding = false
			S.PoundLand = 0.35
			fire("Pound")
			CameraRig.shake(0.8)
			if Movement.onPound then
				Movement.onPound(root.Position - Vector3.new(0, humanoid.HipHeight + root.Size.Y * 0.5, 0))
			end
		end
	end
	if S.Grounded then
		S.AirJumpsLeft = f and 0 or (h.AirJumps or 0)
		S.Coyote = Config.Moves.CoyoteTime
		S.WallSlide = 0
		if not S.Intangible then
			S.LastSafe = root.CFrame
		end
	else
		S.Coyote = S.Coyote - dt
		if humanoid:GetState() == Enum.HumanoidStateType.Jumping then
			S.Jumped = true
		end
	end
	-- Short hop: let go of jump early.
	if S.JumpCut and not Input.jumpHeld() and vel.Y > 0 and not S.Grounded then
		root.AssemblyLinearVelocity = Vector3.new(vel.X, vel.Y * 0.5, vel.Z)
		S.JumpCut = false
	end
	if vel.Y <= 0 and not S.Grounded then
		S.JumpCut = false
	end

	-- Danny: intangible = noclip, energy drain, back to safety if it runs out inside a wall.
	if S.Intangible then
		S.Energy = math.max(0, S.Energy - 12 * dt)
		local inside = insideGeometry(root)
		if not inside then
			S.LastSafe = root.CFrame
		end
		if S.Energy <= 0 then
			if inside and S.LastSafe then
				root.CFrame = S.LastSafe
				S.FlyVel = Vector3.zero
				say("Out of energy - pulled back to safety", Color3.fromRGB(255, 200, 120))
			end
			S.Intangible = false
			S.WantSolid = false
		elseif S.WantSolid and not inside then
			S.Intangible = false
			S.WantSolid = false
		elseif S.WantSolid then
			say("Still inside something - fly clear to turn solid", Color3.fromRGB(200, 255, 200))
			S.WantSolid = false
			S.WantSolidRetry = true
		end
		if S.WantSolidRetry and not inside then
			S.Intangible = false
			S.WantSolidRetry = false
		end
	end

	-- Walk speed: sprint, scooter, emote freeze.
	local base = character:GetAttribute("WalkSpeedBase") or humanoid.WalkSpeed
	if not character:GetAttribute("WalkSpeedBase") then
		character:SetAttribute("WalkSpeedBase", humanoid.WalkSpeed)
		base = humanoid.WalkSpeed
	end
	local walkTarget = (f and f.Walk or h.Walk) * (Input.held("Boost") and not f and 1.45 or 1) * (S.Scooter > 0 and 2.4 or 1)
	humanoid.WalkSpeed = S.EmoteTime > 0 and 0 or walkTarget

	-- Movement modes.
	local canGlide = f and f.Glide or (not f and h.CanGlide)
	if S.Flying then
		S.Gliding = false
		updateFlight(dt, humanoid, root)
	elseif S.Pounding then
		drive(root, "Vector", Vector3.new(0, -Config.Moves.PoundSpeed, 0), nil)
	elseif S.Dodge > 0 then
		local speed = Config.Moves.DodgeSpeed * (f and f.Id == "XLR8" and 1.9 or 1) * (f and f.Id == "FourArms" and 0.85 or 1)
		drive(root, "Plane", S.DodgeDir * speed, yawOf(S.DodgeDir))
	elseif not S.Grounded and canGlide and Input.jumpHeld() and (vel.Y < 0 or S.Gliding) and not S.Grapple then
		updateGlide(dt, root)
	else
		S.Gliding = false
		if S.Grapple then
			local to = S.Grapple - root.Position
			if to.Magnitude < 5 then
				S.Grapple = nil
				S.FlipTime = 0.5
			else
				local nv = vel + to.Unit * 150 * dt + Vector3.new(0, workspace.Gravity * 0.65 * dt, 0)
				if nv.Magnitude > 107 then
					nv = nv.Unit * 107
				end
				root.AssemblyLinearVelocity = nv
			end
			drive(root, nil)
		elseif S.FaceTime > 0 and S.FaceYaw then
			humanoid.AutoRotate = false
			drive(root, nil)
			local m = ensureMover(root)
			m.Align.CFrame = CFrame.Angles(0, math.rad(S.FaceYaw), 0)
			m.Align.MaxTorque = root.AssemblyMass * 5000
		else
			humanoid.AutoRotate = true
			drive(root, nil)
		end
		if not S.Grounded then
			updateWalls(root, humanoid)
			if S.WallSlide > 0 and vel.Y < 0 then
				local cling = (id == "ElTigre" and not f) and -7 or -22
				if vel.Y < cling then
					root.AssemblyLinearVelocity = Vector3.new(vel.X, cling, vel.Z)
				end
				local m = ensureMover(root)
				m.Align.CFrame = CFrame.Angles(0, math.rad(yawOf(-S.WallNormal)), 0)
				m.Align.MaxTorque = root.AssemblyMass * 5000
				humanoid.AutoRotate = false
			end
		end
	end

	-- Emotes end when you move or leave the ground.
	if S.EmoteTime > 0 then
		local before = S.EmoteTime
		S.EmoteTime = math.max(0, S.EmoteTime - dt)
		if Input.move().Magnitude > 0.1 or not S.Grounded then
			S.EmoteTime = 0
		elseif before > 1.2 and S.EmoteTime <= 1.2 and Movement.onEmotePeak then
			Movement.onEmotePeak(root.Position, formDef() and formDef().Accent or h.Accent)
		end
	end

	-- Walk cycle phase and idle time for the Animator.
	local flatSpeed = Vector3.new(vel.X, 0, vel.Z).Magnitude
	S.WalkPhase = S.WalkPhase + flatSpeed * dt * 0.75 / scale()
	if S.Grounded and flatSpeed < 1 and S.AttackAnim <= 0 and S.Dodge <= 0 then
		S.IdleTime = S.IdleTime + dt
	else
		S.IdleTime = 0
	end

	-- Report state for animation replication and server immunity checks (max 12 Hz, on change).
	sendClock = sendClock - dt
	local report = {
		Flying = S.Flying, Gliding = S.Gliding, Dodge = S.Dodge > 0, DodgeAir = S.DodgeAir, Pounding = S.Pounding,
		Emote = S.EmoteTime > 0, WallSlide = S.WallSlide > 0, Boosting = S.Boosting, Jetpack = S.Jetpack > 0,
		Scooter = S.Scooter > 0, Intangible = S.Intangible, Takeoff = S.TakeoffTime > 0, Grapple = S.Grapple ~= nil,
		Breath = S.Breath, DodgeSide = S.DodgeSide, FlyBank = math.floor(S.FlyBank + 0.5), FlyPitch = math.floor(S.FlyPitch + 0.5),
	}
	local changed = false
	for k, v in pairs(report) do
		if lastSent[k] ~= v then
			changed = true
			break
		end
	end
	if changed and sendClock <= 0 then
		sendClock = 1 / 12
		lastSent = report
		fire("State", report)
	end
end

function Movement.init(controllers)
	Input, CameraRig, Hud = controllers.Input, controllers.CameraRig, controllers.Hud
	Input.on("Sig", onSig)
	Input.on("Sig2", onSig2)
	Input.on("Ult", onUlt)
	Input.on("Fly", onFly)
	Input.on("Dodge", onDodge)
	Input.on("Down", onDown)
	Input.on("Emote", onEmote)
	Input.on("Attack", function(down)
		if down then
			tryAttack()
			S.AttackHeldBefore = true
		end
	end)
	for i = 1, 3 do
		Input.on("Omni" .. i, function(down)
			if down and heroId() == "Ben" then
				fire("Omnitrix", "slot", i - 1)
				if Hud then
					Hud.setDial(false)
				end
			end
		end)
	end
	Input.on("Glitch", function(down)
		if down and heroId() == "Ben" then
			fire("Omnitrix", "glitch")
		end
	end)
	Input.on("Recall", function(down)
		if down then
			fire("Travel", Config.Levels.Nexus)
		end
	end)
end

function Movement.start()
	remotes = ReplicatedStorage:WaitForChild("Remotes")
	game:GetService("UserInputService").JumpRequest:Connect(onJumpRequest)
	player.CharacterAdded:Connect(function()
		reset()
		mover = nil
		lastSent = {}
	end)
	RunService.Heartbeat:Connect(step)
	-- Intangibility: switch collisions off every physics step (the Humanoid re-enables some).
	local ghosted = {}
	RunService.Stepped:Connect(function()
		if S.Intangible and player.Character then
			for _, p in ipairs(player.Character:GetDescendants()) do
				if p:IsA("BasePart") and p.CanCollide then
					p.CanCollide = false
					ghosted[p] = true
				end
			end
		elseif next(ghosted) then
			-- Solid again: restore exactly the collisions we switched off.
			for p in pairs(ghosted) do
				if p.Parent then
					p.CanCollide = true
				end
			end
			ghosted = {}
		end
	end)
end

return Movement
