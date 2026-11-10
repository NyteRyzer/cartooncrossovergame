--!nonstrict
-- Animates every hero in the server with Shared/Pose (the prototype's
-- procedural animation): each frame, each character's movement state becomes
-- joint angles on its R15 Motor6Ds. The local hero reads Movement's state
-- directly; other players use the attributes the server relays. Also shows
-- or hides state-dependent gear (Jake's dragon, Danny's ghost tail, Aang's
-- glider, rocket flames...), blinks the faces, and spins Cannonbolt.

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Pose = require(Shared:WaitForChild("Pose"))

local Animator = {}
local Movement
local player = Players.LocalPlayer
local rigs = {} -- [character] = cache

local JOINTS = {
	Root = { "LowerTorso", "Root" }, Waist = { "UpperTorso", "Waist" }, Neck = { "Head", "Neck" },
	LShoulder = { "LeftUpperArm", "LeftShoulder" }, LElbow = { "LeftLowerArm", "LeftElbow" },
	RShoulder = { "RightUpperArm", "RightShoulder" }, RElbow = { "RightLowerArm", "RightElbow" },
	LHip = { "LeftUpperLeg", "LeftHip" }, LKnee = { "LeftLowerLeg", "LeftKnee" }, LAnkle = { "LeftFoot", "LeftAnkle" },
	RHip = { "RightUpperLeg", "RightHip" }, RKnee = { "RightLowerLeg", "RightKnee" }, RAnkle = { "RightFoot", "RightAnkle" },
}
local BODY = {
	"UpperTorso", "LowerTorso", "LeftUpperArm", "LeftLowerArm", "LeftHand", "RightUpperArm", "RightLowerArm",
	"RightHand", "LeftUpperLeg", "LeftLowerLeg", "LeftFoot", "RightUpperLeg", "RightLowerLeg", "RightFoot",
}
local LEGS = { LeftUpperLeg = true, LeftLowerLeg = true, LeftFoot = true, RightUpperLeg = true, RightLowerLeg = true, RightFoot = true }

local rad = math.rad
local function A(x, y, z)
	return CFrame.Angles(rad(x), rad(y), rad(z))
end

local function cacheFor(character)
	local c = rigs[character]
	if c and c.Gear and c.Gear.Parent then
		return c
	end
	-- No gear yet (still being dressed): retry a few times a second, not every frame.
	if c and not c.Gear and os.clock() < (c.RetryAt or 0) then
		return c
	end
	local humanoid = character:FindFirstChildOfClass("Humanoid")
	if not humanoid or humanoid.RigType ~= Enum.HumanoidRigType.R15 then
		return nil
	end
	c = c or { WalkPhase = 0, IdleTime = 0, Emote = 0, Dodge = 0, Blink = math.random() * 3 }
	c.Humanoid = humanoid
	c.Root = character:FindFirstChild("HumanoidRootPart")
	c.Joints = {}
	for key, spec in pairs(JOINTS) do
		local part = character:FindFirstChild(spec[1])
		c.Joints[key] = part and part:FindFirstChild(spec[2])
	end
	c.Gear = character:FindFirstChild("HeroGear")
	c.RetryAt = os.clock() + 0.3
	c.Spinner, c.SpinBase = nil, nil
	c.Toggles = {}
	c.Extra = {}
	if c.Gear then
		for _, p in ipairs(c.Gear:GetChildren()) do
			if p:IsA("BasePart") then
				local when = p:GetAttribute("ShowWhen")
				if when then
					table.insert(c.Toggles, { Part = p, When = when, Base = p.Transparency })
				end
				if p:GetAttribute("Spin") then
					c.Spinner = p:FindFirstChild("GearWeld")
					c.SpinBase = c.Spinner and c.Spinner.C0
				end
			end
		end
		local head = c.Gear:FindFirstChild("ToonHead")
		c.Face = head and head:FindFirstChild("Face")
	end
	for _, d in ipairs(character:GetDescendants()) do
		if d:IsA("Motor6D") and (d.Name == "StaffJoint" or d.Name:sub(1, 8) == "ExtraArm" or d.Name:sub(1, 10) == "DragonWing") then
			c.Extra[d.Name] = d
		end
	end
	rigs[character] = c
	return c
end

local function setVisible(part, visible, base)
	part.Transparency = visible and base or 1
end

local function bodyVisibility(character, c, mode)
	-- mode: nil (normal), "Dragon" (hide human), "Ball" (Cannonbolt), "Ghost" (Danny phasing), "Tail" (hide legs)
	if c.BodyMode == mode then
		return
	end
	c.BodyMode = mode
	for _, name in ipairs(BODY) do
		local p = character:FindFirstChild(name)
		if p then
			local hide = mode == "Dragon" or mode == "Ball" or (mode == "Tail" and LEGS[name])
			p.LocalTransparencyModifier = hide and 1 or (mode == "Ghost" and 0.6 or 0)
		end
	end
	if c.Gear then
		for _, p in ipairs(c.Gear:GetChildren()) do
			if p:IsA("BasePart") and not p:GetAttribute("ShowWhen") then
				p.LocalTransparencyModifier = (mode == "Dragon" or mode == "Ball") and 1 or (mode == "Ghost" and 0.6 or 0)
			end
		end
	end
end

-- Movement state for another player's character, from relayed attributes.
local function remoteState(character, c, dt)
	local root = c.Root
	local v = root.AssemblyLinearVelocity
	local flat = Vector3.new(v.X, 0, v.Z).Magnitude
	local params = RaycastParams.new()
	params.FilterType = Enum.RaycastFilterType.Exclude
	params.FilterDescendantsInstances = { character }
	local reach = c.Humanoid.HipHeight + root.Size.Y * 0.5 + 0.8
	local grounded = workspace:Raycast(root.Position, Vector3.new(0, -reach, 0), params) ~= nil
	local emoting = character:GetAttribute("MvEmote")
	if emoting and c.Emote <= 0 then
		c.Emote = Pose.EmoteTime
	elseif not emoting then
		c.Emote = 0
	end
	c.Emote = math.max(0, c.Emote - dt)
	local dodging = character:GetAttribute("MvDodge")
	if dodging and not c.WasDodging then
		c.Dodge = character:GetAttribute("MvDodgeAir") and 0.32 or 0.4
	end
	c.WasDodging = dodging
	c.Dodge = math.max(0, c.Dodge - dt)
	c.WalkPhase = c.WalkPhase + flat * dt * 0.75 / c.Scale
	if grounded and flat < 1 then
		c.IdleTime = c.IdleTime + dt
	else
		c.IdleTime = 0
	end
	local flying = character:GetAttribute("MvFlying") == true
	return {
		Speed = flat, Speed3 = v.Magnitude, VelY = v.Y, Grounded = grounded and not flying,
		Flying = flying, Gliding = character:GetAttribute("MvGliding") == true,
		Pounding = character:GetAttribute("MvPounding") == true, Dodge = c.Dodge, DodgeAir = character:GetAttribute("MvDodgeAir"),
		DodgeSide = character:GetAttribute("MvDodgeSide") or 1, EmoteTime = c.Emote, WallSlide = character:GetAttribute("MvWallSlide") and 1 or 0,
		FlyBank = character:GetAttribute("MvFlyBank") or 0, FlyPitch = character:GetAttribute("MvFlyPitch") or 0,
		Boosting = character:GetAttribute("MvBoosting"), Jetpack = character:GetAttribute("MvJetpack"),
		Scooter = character:GetAttribute("MvScooter"), Intangible = character:GetAttribute("MvIntangible"),
		TakeoffTime = character:GetAttribute("MvTakeoff") and 0.3 or 0, Breath = character:GetAttribute("MvBreath"),
		WalkPhase = c.WalkPhase, IdleTime = c.IdleTime,
		Slide = character:GetAttribute("MvSlide"), WallRun = character:GetAttribute("MvWallRun"), WallSide = character:GetAttribute("MvWallSide") or 0,
		Climb = character:GetAttribute("MvClimb"), Swing = character:GetAttribute("MvSwing"),
	}
end

local function localState(c)
	local S = Movement.state()
	local root = c.Root
	local v = root.AssemblyLinearVelocity
	return {
		Speed = Vector3.new(v.X, 0, v.Z).Magnitude, Speed3 = v.Magnitude, VelY = v.Y, Grounded = S.Grounded,
		Flying = S.Flying, Gliding = S.Gliding, Pounding = S.Pounding, PoundLand = S.PoundLand,
		Dodge = S.Dodge, DodgeAir = S.DodgeAir, DodgeSide = S.DodgeSide, AttackAnim = S.AttackAnim, AttackDur = S.AttackDur,
		AttackStep = S.AttackStep, LandTimer = S.LandTimer, IdleTime = S.IdleTime, EmoteTime = S.EmoteTime,
		FlipTime = S.FlipTime, WallSlide = S.WallSlide, FlyBank = S.FlyBank, FlyPitch = S.FlyPitch, Boosting = S.Boosting,
		TakeoffTime = S.TakeoffTime, Breath = S.Breath, Jetpack = S.Jetpack > 0, Scooter = S.Scooter > 0,
		Intangible = S.Intangible, WalkPhase = S.WalkPhase,
		Slide = S.Slide > 0, WallRun = S.WallRun ~= nil, WallSide = S.WallRun and S.WallRun.Side or 0, Climb = S.Climb,
		Swing = S.Swing ~= nil, Counter = S.CounterAnim, Block = S.BlockAnim, Takedown = S.TakedownAnim,
	}
end

local function animate(character, c, dt, now, serverNow)
	local isLocal = character == player.Character
	c.Scale = math.max(0.5, (c.Humanoid.HipHeight + c.Root.Size.Y * 0.5) / 3)
	local st = isLocal and localState(c) or remoteState(character, c, dt)
	st.Time = now
	st.Hero = character:GetAttribute("Hero") or "Ben"
	st.Form = character:GetAttribute("Form")
	if not isLocal then
		-- Attacks come from the server's record of the swing.
		local at = character:GetAttribute("AttackAt")
		if at then
			local dur = character:GetAttribute("AttackDur") or 0.25
			st.AttackAnim = math.max(0, dur - (serverNow - at))
			st.AttackDur = dur
			st.AttackStep = character:GetAttribute("AttackStep") or 0
		end
	end
	if not isLocal then
		-- Counters and blocks come from the server's record.
		local counterAt = character:GetAttribute("CounterAt")
		if counterAt then
			st.Counter = math.max(0, 0.3 - (serverNow - counterAt))
		end
		local blockAt = character:GetAttribute("BlockAt")
		if blockAt then
			st.Block = math.max(0, 0.5 - (serverNow - blockAt))
		end
	end
	local hurtAt = character:GetAttribute("HurtAt")
	if hurtAt then
		st.Hurt = math.max(0, 0.25 - (serverNow - hurtAt))
	end
	local O = Pose.Compute(st)

	-- Prototype space -> R15 joint space (see Shared/Pose for conventions).
	local k = c.Scale * 3 * 1.73 / 150
	local J = c.Joints
	if J.Root then
		J.Root.Transform = CFrame.new(-O.ShiftX * k, O.Bob * k, 0)
			* A(0, 0, -O.BodyRoll) * A(-O.BodyPitch, 0, 0) * A(-O.Lean, 0, 0) * A(0, 0, -O.HipRoll) * A(0, O.HipTwist + O.Spin, 0)
	end
	if J.Waist then
		J.Waist.Transform = A(0, O.ChestTwist - O.HipTwist, 0)
	end
	if J.Neck then
		J.Neck.Transform = A(-O.HeadPitch, O.HeadYaw, -O.HeadRoll)
	end
	if J.LShoulder then
		J.LShoulder.Transform = A(O.ShFwd[1], 0, 0) * A(0, 0, -O.ShAbd[1])
	end
	if J.RShoulder then
		J.RShoulder.Transform = A(O.ShFwd[2], 0, 0) * A(0, 0, O.ShAbd[2])
	end
	if J.LElbow then
		J.LElbow.Transform = A(O.Elbow[1], 0, 0)
	end
	if J.RElbow then
		J.RElbow.Transform = A(O.Elbow[2], 0, 0)
	end
	if J.LHip then
		J.LHip.Transform = A(O.Thigh[1], 0, 0) * A(0, 0, -(2 + O.ThighAbd[1]))
	end
	if J.RHip then
		J.RHip.Transform = A(O.Thigh[2], 0, 0) * A(0, 0, 2 + O.ThighAbd[2])
	end
	if J.LKnee then
		J.LKnee.Transform = A(-O.Knee[1], 0, 0)
	end
	if J.RKnee then
		J.RKnee.Transform = A(-O.Knee[2], 0, 0)
	end
	if J.LAnkle then
		J.LAnkle.Transform = A(O.Ankle[1], 0, 0)
	end
	if J.RAnkle then
		J.RAnkle.Transform = A(O.Ankle[2], 0, 0)
	end
	local X = c.Extra
	if X.ExtraArmL then
		X.ExtraArmL.Transform = A(O.ShFwd[2] * 0.8, 0, 0) * A(0, 0, -25)
	end
	if X.ExtraArmR then
		X.ExtraArmR.Transform = A(O.ShFwd[1] * 0.8, 0, 0) * A(0, 0, 25)
	end
	if X.StaffJoint then
		X.StaffJoint.Transform = A(0, O.StaffSpin, 0)
	end
	local flap
	if st.Flying then
		local dive = (st.VelY or 0) < -23 and not st.Boosting
		c.FlapPhase = (c.FlapPhase or 0) + dt * (dive and 1 or 6 + (st.Boosting and 5 or 0) + math.max(0, st.VelY or 0) / 10)
		flap = dive and -28 or math.sin(c.FlapPhase) * 38
	end
	if X.DragonWingL and flap then
		X.DragonWingL.Transform = A(0, 0, -flap)
	end
	if X.DragonWingR and flap then
		X.DragonWingR.Transform = A(0, 0, flap)
	end

	-- State-dependent gear and body visibility.
	local hero, form = st.Hero, st.Form
	local mode = nil
	if hero == "Jake" and not form and st.Flying then
		mode = "Dragon"
	elseif character:GetAttribute("HideBody") then
		mode = "Ball"
	elseif hero == "Danny" and (st.Intangible or ((st.Dodge or 0) > 0 and not st.Flying)) then
		mode = "Ghost"
	elseif hero == "Danny" and st.Flying then
		mode = "Tail"
	end
	bodyVisibility(character, c, mode)
	for _, t in ipairs(c.Toggles) do
		local w = t.When
		local show = (w == "Always")
			or (w == "Flying" and st.Flying)
			or (w == "Gliding" and st.Gliding)
			or (w == "NotGliding" and not st.Gliding)
			or (w == "Scooter" and st.Scooter)
			or (w == "Jetpack" and st.Jetpack)
			or (w == "FlyingOrDodge" and (st.Flying or (st.Dodge or 0) > 0))
		setVisible(t.Part, show, t.Base)
	end
	-- Jake's dragon is the whole body in flight: bank and pitch it with the flight.
	if mode == "Dragon" and J.Root then
		J.Root.Transform = A(0, 0, -(st.FlyBank or 0)) * A((st.FlyPitch or 0) * 0.7 - 8, 0, 0)
	end
	-- Cannonbolt rolls.
	if c.Spinner and c.SpinBase then
		c.Roll = (c.Roll or 0) + (st.Speed or 0) * dt * 0.5
		c.Spinner.C0 = c.SpinBase * CFrame.Angles(-c.Roll, 0, 0)
	end
	-- Blink.
	if c.Face then
		c.Blink = c.Blink - dt
		local closed = c.Blink < 0.12
		if c.Blink < 0 then
			c.Blink = 2.5 + math.random() * 3
		end
		for _, name in ipairs({ "EyeL", "EyeR" }) do
			local eye = c.Face:FindFirstChild(name)
			if eye then
				local base = eye:GetAttribute("BaseY")
				if not base then
					base = eye.Size.Y.Scale
					eye:SetAttribute("BaseY", base)
				end
				eye.Size = UDim2.fromScale(eye.Size.X.Scale, closed and base * 0.12 or base)
			end
		end
	end
end

function Animator.init(controllers)
	Movement = controllers.Movement
end

function Animator.start()
	RunService.Stepped:Connect(function(time, dt)
		local now = os.clock()
		local serverNow = workspace:GetServerTimeNow()
		for _, p in ipairs(Players:GetPlayers()) do
			local character = p.Character
			if character and character.Parent then
				local c = cacheFor(character)
				if c and c.Root and c.Root.Parent then
					local ok, err = pcall(animate, character, c, dt, now, serverNow)
					if not ok then
						warn("[Animator] " .. tostring(err))
						rigs[character] = nil
					end
				end
			end
		end
		for character in pairs(rigs) do
			if not character.Parent then
				rigs[character] = nil
			end
		end
	end)
	-- Rebuild a rig's cache when its gear is rebuilt (hero or form change).
	workspace.DescendantAdded:Connect(function(d)
		if d.Name == "HeroGear" and d.Parent and rigs[d.Parent] then
			task.delay(0.2, function()
				local c = rigs[d.Parent]
				if c then
					c.Gear = nil
					c.BodyMode = "reset"
				end
			end)
		end
	end)
end

return Animator
