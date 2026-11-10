--!nonstrict
-- The Rift Titan: a three-phase boss fight in the classic action-game mould.
--   Phase 1  Stomp (a ground shockwave: jump over it) and Charge (dodge it).
--   Phase 2  adds a Volley of shots (parry them back with a counter).
--   Phase 3  enraged: faster, and summons gremlins.
-- Every attack is telegraphed. Enough hits break its posture: it staggers and
-- finishers deal triple damage. It appears when all three rifts are closed,
-- or from the "Titan Hunt" mission. Health scales with the players nearby.

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))

local Boss = {}
local Net, Enemies, Combat, Progression, PlayerState
local active = nil
local B = Config.Boss

local function rootOf(player)
	local character = player and player.Character
	local humanoid = character and character:FindFirstChildOfClass("Humanoid")
	if humanoid and humanoid.Health > 0 then
		return character:FindFirstChild("HumanoidRootPart")
	end
	return nil
end

local function onGround(root)
	local params = RaycastParams.new()
	params.FilterType = Enum.RaycastFilterType.Exclude
	params.FilterDescendantsInstances = { root.Parent }
	return workspace:Raycast(root.Position, Vector3.new(0, -5.5, 0), params) ~= nil
end

local function buildModel(pos)
	local model = Instance.new("Model")
	model.Name = "RiftTitan"
	local s = B.Size
	local root = Instance.new("Part")
	root.Name = "Body"
	root.Shape = Enum.PartType.Ball
	root.Size = Vector3.new(s, s, s)
	root.Color = B.Color
	root.Material = Enum.Material.SmoothPlastic
	root.CFrame = CFrame.new(pos)
	root.CustomPhysicalProperties = PhysicalProperties.new(2, 0.3, 0.3)
	root.Parent = model
	local function add(size, offset, color, material, shape)
		local p = Instance.new("Part")
		p.Size = size
		p.Color = color
		p.Material = material or Enum.Material.SmoothPlastic
		p.Shape = shape or Enum.PartType.Ball
		p.CanCollide = false
		p.CanQuery = false
		p.Massless = true
		p.CFrame = root.CFrame * offset
		p.Parent = model
		local w = Instance.new("WeldConstraint")
		w.Part0 = root
		w.Part1 = p
		w.Parent = p
		return p
	end
	for _, x in ipairs({ -1, 1 }) do
		add(Vector3.new(6, 6, 6), CFrame.new(x * 4.5, 3, -8.5), Color3.new(1, 1, 1))
		add(Vector3.new(3, 3, 3), CFrame.new(x * 4.5, 3, -11), Color3.fromRGB(255, 60, 90), Enum.Material.Neon)
		add(Vector3.new(2.5, 12, 2.5), CFrame.new(x * 7, 11, 0) * CFrame.Angles(0, 0, -x * 0.5), Color3.fromRGB(250, 230, 200), nil, Enum.PartType.Block)
		add(Vector3.new(5, 14, 5), CFrame.new(x * 12, -3, -2), B.Color, nil, Enum.PartType.Block)
	end
	add(Vector3.new(12, 2, 1.5), CFrame.new(0, -4, -10.5), Color3.fromRGB(255, 230, 255), Enum.Material.Neon, Enum.PartType.Block)
	for i = 0, 7 do
		local a = i / 8 * math.pi * 2
		add(Vector3.new(1.6, 4, 1.6), CFrame.new(math.cos(a) * 6, 11.5, math.sin(a) * 6), Color3.fromRGB(255, 210, 60), Enum.Material.Neon, Enum.PartType.Block)
	end
	local att = Instance.new("Attachment")
	att.Parent = root
	local align = Instance.new("AlignPosition")
	align.Mode = Enum.PositionAlignmentMode.OneAttachment
	align.Attachment0 = att
	align.MaxForce = 5e6
	align.MaxVelocity = B.Speed
	align.Responsiveness = 20
	align.Position = pos
	align.Parent = root
	local orient = Instance.new("AlignOrientation")
	orient.Mode = Enum.OrientationAlignmentMode.OneAttachment
	orient.Attachment0 = att
	orient.MaxTorque = 5e6
	orient.Responsiveness = 8
	orient.Parent = root
	model.PrimaryPart = root
	model:SetAttribute("Enemy", "Boss")
	model.Parent = workspace:FindFirstChild("Enemies") or workspace
	root:SetNetworkOwner(nil)
	return model, root, align, orient
end

local function nearestPlayer(pos)
	local best, bestDist = nil, 400
	for _, player in ipairs(Players:GetPlayers()) do
		local r = rootOf(player)
		if r and (r.Position - pos).Magnitude < bestDist then
			best, bestDist = player, (r.Position - pos).Magnitude
		end
	end
	return best, bestDist
end

local function telegraph(e, time, label)
	e.Model:SetAttribute("Windup", workspace:GetServerTimeNow())
	Net.fireAll("Fx", { Kind = "Telegraph", Model = e.Model, Time = time, Boss = true, Label = label })
end

local function stomp(e)
	local center = e.Root.Position - Vector3.new(0, B.Size * 0.5, 0)
	local hit = {}
	Net.fireAll("Fx", { Kind = "Shockwave", Pos = center, Radius = B.StompRadius, Time = 1.2, Color = Color3.fromRGB(255, 80, 200) })
	Net.fireAll("Fx", { Kind = "Shake", Pos = center, Amount = 0.8 })
	task.spawn(function()
		local t0 = os.clock()
		while os.clock() - t0 < 1.2 and e.Health > 0 do
			local radius = (os.clock() - t0) / 1.2 * B.StompRadius
			for _, player in ipairs(Players:GetPlayers()) do
				local r = rootOf(player)
				if r and not hit[player] then
					local flat = Vector3.new(r.Position.X - center.X, 0, r.Position.Z - center.Z).Magnitude
					if math.abs(flat - radius) < 4 and onGround(r) then
						hit[player] = true
						Combat.damagePlayer(player, B.StompDamage, center)
					end
				end
			end
			task.wait(0.05)
		end
	end)
end

local function charge(e, target)
	local r = rootOf(target)
	if not r then
		return
	end
	local dir = Vector3.new(r.Position.X - e.Root.Position.X, 0, r.Position.Z - e.Root.Position.Z)
	if dir.Magnitude < 1 then
		return
	end
	dir = dir.Unit
	local hit = {}
	e.Align.MaxVelocity = 70
	e.ChargeUntil = os.clock() + 1.4
	local goal = e.Root.Position + dir * 90
	e.Align.Position = goal
	task.spawn(function()
		local t0 = os.clock()
		while os.clock() - t0 < 1.4 and e.Health > 0 do
			for _, player in ipairs(Players:GetPlayers()) do
				local pr = rootOf(player)
				if pr and not hit[player] and (pr.Position - e.Root.Position).Magnitude < B.Size * 0.5 + 4 then
					hit[player] = true
					Combat.damagePlayer(player, B.ChargeDamage, e.Root.Position)
					pr.AssemblyLinearVelocity = dir * 80 + Vector3.new(0, 40, 0)
				end
			end
			task.wait(0.05)
		end
		e.Align.MaxVelocity = e.Speed
	end)
end

local function volley(e, target)
	local r = rootOf(target)
	if not r then
		return
	end
	local from = e.Root.Position + Vector3.new(0, 3, 0)
	local base = r.Position - from
	for k = -2, 2 do
		local dir = CFrame.Angles(0, math.rad(k * 9), 0):VectorToWorldSpace(base)
		Enemies.fireShot(e, from, from + dir, 65, B.SpitDamage, Color3.fromRGB(255, 80, 200))
	end
end

local function think(e, now, step)
	if e.Stun > 0 then
		e.Stun = e.Stun - step
		if e.Stun <= 0 then
			e.Align.Enabled = true
		end
	end
	local frac = e.Health / e.MaxHealth
	local phase = frac > 0.66 and 1 or (frac > 0.33 and 2 or 3)
	if phase ~= e.PhaseNum then
		e.PhaseNum = phase
		if phase > 1 then
			Net.fireAll("Notify", { Banner = B.Name .. (phase == 3 and " is ENRAGED!" or " - Phase 2"), Color = Color3.fromRGB(255, 80, 200) })
		end
		e.Speed = B.Speed * (phase == 3 and 1.6 or 1)
		e.Align.MaxVelocity = e.Speed
	end
	workspace:SetAttribute("BossHealth", math.max(0, math.floor(e.Health)))
	if e.Staggered > now then
		e.Align.Position = e.Root.Position
		return
	elseif e.Model:GetAttribute("Staggered") then
		e.Model:SetAttribute("Staggered", nil)
	end
	if now - e.PostureAt > 2 and e.Posture > 0 then
		e.Posture = math.max(0, e.Posture - Config.Combat.PostureDecay * 2 * step)
	end
	local target = nearestPlayer(e.Root.Position)
	if e.Pending then
		if now >= e.PendingAt then
			local attack = e.Pending
			e.Pending = nil
			e.Model:SetAttribute("Windup", nil)
			e.State = "Chase"
			if attack == "Stomp" then
				stomp(e)
			elseif attack == "Charge" then
				charge(e, e.Target)
			elseif attack == "Volley" then
				volley(e, e.Target)
			end
			e.NextAttack = now + (phase == 3 and 2.2 or 3.4)
		end
		return
	end
	if not target or now < (e.ChargeUntil or 0) then
		return
	end
	local tr = rootOf(target)
	local ground = Enemies.groundAt(tr.Position)
	local flat = Vector3.new(tr.Position.X - e.Root.Position.X, 0, tr.Position.Z - e.Root.Position.Z)
	if flat.Magnitude > 1 then
		e.Orient.CFrame = CFrame.lookAt(Vector3.zero, flat.Unit)
	end
	if flat.Magnitude > 30 then
		e.Align.Position = Vector3.new(tr.Position.X, ground.Y + B.Size * 0.5 + 1, tr.Position.Z)
	else
		e.Align.Position = Vector3.new(e.Root.Position.X, ground.Y + B.Size * 0.5 + 1, e.Root.Position.Z)
	end
	if phase == 3 and now >= (e.NextSummon or 0) then
		e.NextSummon = now + 15
		Enemies.spawnWave(e.Root.Position, { Gremlin = 3 }, { Mission = "Boss" }, 25)
	end
	if now >= e.NextAttack then
		local options = { "Stomp", "Charge" }
		if phase >= 2 then
			table.insert(options, "Volley")
		end
		local attack = options[math.random(1, #options)]
		local windup = attack == "Stomp" and 1.0 or 0.8
		e.Pending = attack
		e.PendingAt = now + windup
		e.Target = target
		e.State = "BossWindup" -- not cancellable by counters; parry its shots instead
		telegraph(e, windup, attack)
	end
end

function Boss.active()
	return active
end

-- Spawns the Titan at `pos` (ground). onDeath(attacker) runs when it falls.
function Boss.spawn(pos, onDeath)
	if active and active.Health > 0 and active.Model.Parent then
		return active
	end
	local nearby = 0
	for _, player in ipairs(Players:GetPlayers()) do
		local r = rootOf(player)
		if r and (r.Position - pos).Magnitude < 600 then
			nearby = nearby + 1
		end
	end
	local health = B.Health * (1 + B.PerExtraPlayer * math.max(0, nearby - 1))
	local ground = Enemies.groundAt(pos)
	local model, root, align, orient = buildModel(ground + Vector3.new(0, B.Size * 0.5 + 1, 0))
	local e = {
		Model = model, Root = root, Align = align, Orient = orient, IsBoss = true, Type = "Boss",
		Def = { Posture = B.Posture, Xp = B.Xp, Cash = B.Cash, Color = B.Color, Heat = 0 },
		Health = health, MaxHealth = health, Posture = 0, PostureAt = 0, Staggered = 0, Stun = 0,
		State = "Chase", NextAttack = os.clock() + 3, Radius = B.Size * 0.5, Speed = B.Speed, PhaseNum = 1,
		Home = ground, Think = think,
	}
	e.OnDeath = function(attacker)
		workspace:SetAttribute("BossActive", false)
		Net.fireAll("Notify", { Banner = B.Name .. " DEFEATED!", Color = Color3.fromRGB(255, 220, 90) })
		for _, player in ipairs(Players:GetPlayers()) do
			local r = rootOf(player)
			if r and (r.Position - e.Root.Position).Magnitude < 400 then
				Progression.award(player, B.Xp, B.Cash, B.Name)
			end
		end
		active = nil
		if onDeath then
			onDeath(attacker)
		end
	end
	Enemies.register(e)
	active = e
	workspace:SetAttribute("BossName", B.Name)
	workspace:SetAttribute("BossMax", math.floor(health))
	workspace:SetAttribute("BossHealth", math.floor(health))
	workspace:SetAttribute("BossActive", true)
	Net.fireAll("Notify", { Banner = "THE " .. string.upper(B.Name) .. " HAS ARRIVED!", Text = "Jump its shockwaves, dodge its charge, counter its shots", Color = Color3.fromRGB(255, 80, 200) })
	Net.fireAll("Fx", { Kind = "Burst", Pos = root.Position, Color = B.Color, Count = 120, Gravity = true })
	return e
end

function Boss.init(systems)
	Net, Enemies, Combat, Progression, PlayerState = systems.Net, systems.Enemies, systems.Combat, systems.Progression, systems.Players
end

function Boss.start()
	local MapData = require(Shared:WaitForChild("MapData"))
	local anchors = MapData.Anchors or {}
	Enemies.onAllRiftsClosed = function()
		if anchors.Plaza then
			task.delay(4, function()
				Boss.spawn(anchors.Plaza)
			end)
		end
	end
end

return Boss
