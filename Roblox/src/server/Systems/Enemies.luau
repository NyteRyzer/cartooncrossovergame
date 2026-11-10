--!nonstrict
-- Enemies, freeflow-combat style: every attack is telegraphed (a wind-up the
-- target sees as a "!" and a COUNTER prompt), so a well-timed counter cancels
-- it. Hits build posture; a broken posture staggers the enemy and opens it to
-- a finisher. Types: Gremlin (melee), Brute (heavy ground slam), Spitter
-- (ranged; its shots can be parried back), Hunter (wanted-level squads).
-- Also runs Retroville's rift patrols (3 waves to close a rift).

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))
local MapData = require(Shared:WaitForChild("MapData"))

local Enemies = {}
local Net, PlayerState, Combat, Progression, HeatSystem
local list = {}
local rifts = {}
local shots = {}      -- enemy projectiles
local nextShot = -1   -- negative ids: enemy shots (player shots are positive)
local folder
local rng = Random.new(1337)
local PURPLE = Color3.fromRGB(170, 80, 230)

local function addPart(model, root, size, offset, color, material, shape)
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

local function makeModel(typeName, def, pos)
	local model = Instance.new("Model")
	model.Name = typeName
	local s = def.Size
	local root = Instance.new("Part")
	root.Name = "Body"
	root.Shape = Enum.PartType.Ball
	root.Size = Vector3.new(s, s, s)
	root.Color = def.Color
	root.Material = Enum.Material.SmoothPlastic
	root.CFrame = CFrame.new(pos)
	root.CustomPhysicalProperties = PhysicalProperties.new(0.5, 0.3, 0.5)
	root.Parent = model
	local k = s / 3.2
	for _, x in ipairs({ -0.6, 0.6 }) do
		addPart(model, root, Vector3.new(1.1, 1.1, 1.1) * k, CFrame.new(x * k, 0.5 * k, -1.2 * k), Color3.new(1, 1, 1))
		addPart(model, root, Vector3.new(0.5, 0.5, 0.5) * k, CFrame.new(x * k, 0.5 * k, -1.65 * k), Color3.fromRGB(20, 10, 30))
	end
	if typeName == "Brute" then
		for _, x in ipairs({ -1, 1 }) do
			addPart(model, root, Vector3.new(0.6, 2.2, 0.6) * k, CFrame.new(x * 1.1 * k, 1.6 * k, 0) * CFrame.Angles(0, 0, -x * 0.5), Color3.fromRGB(240, 220, 200), nil, Enum.PartType.Block)
			addPart(model, root, Vector3.new(1.2, 2.6, 1.2) * k, CFrame.new(x * 1.7 * k, -0.4 * k, -0.3 * k), def.Color, nil, Enum.PartType.Block)
		end
		addPart(model, root, Vector3.new(2.2, 0.5, 0.4) * k, CFrame.new(0, -0.6 * k, -1.45 * k), Color3.fromRGB(255, 240, 220), Enum.Material.Neon, Enum.PartType.Block)
	elseif typeName == "Spitter" then
		addPart(model, root, Vector3.new(0.9, 0.9, 1.6) * k, CFrame.new(0, -0.3 * k, -1.5 * k), Color3.fromRGB(60, 140, 60), nil, Enum.PartType.Block)
		addPart(model, root, Vector3.new(0.5, 0.5, 0.5) * k, CFrame.new(0, -0.3 * k, -2.3 * k), Color3.fromRGB(200, 255, 120), Enum.Material.Neon)
	elseif typeName == "Hunter" then
		addPart(model, root, Vector3.new(2.6, 0.6, 1.0) * k, CFrame.new(0, 0.5 * k, -1.2 * k), Color3.fromRGB(30, 30, 40), nil, Enum.PartType.Block)
		addPart(model, root, Vector3.new(2.0, 0.15, 0.2) * k, CFrame.new(0, 0.55 * k, -1.72 * k), Color3.fromRGB(255, 60, 60), Enum.Material.Neon, Enum.PartType.Block)
		addPart(model, root, Vector3.new(0.15, 1.6, 0.15) * k, CFrame.new(0.6 * k, 2 * k, 0), Color3.fromRGB(200, 200, 210), nil, Enum.PartType.Block)
	else
		for _, x in ipairs({ -0.6, 0.6 }) do
			addPart(model, root, Vector3.new(0.4, 1.4, 0.4) * k, CFrame.new(x * 1.3 * k, 1.6 * k, 0) * CFrame.Angles(0, 0, -x * 0.6), Color3.fromRGB(90, 30, 140), nil, Enum.PartType.Block)
		end
		addPart(model, root, Vector3.new(1.4, 0.3, 0.3) * k, CFrame.new(0, -0.5 * k, -1.45 * k), Color3.fromRGB(255, 220, 255), Enum.Material.Neon, Enum.PartType.Block)
	end
	local att = Instance.new("Attachment")
	att.Parent = root
	local align = Instance.new("AlignPosition")
	align.Mode = Enum.PositionAlignmentMode.OneAttachment
	align.Attachment0 = att
	align.MaxForce = 6000 * k * k * k
	align.MaxVelocity = def.Speed
	align.Responsiveness = 25
	align.Position = pos
	align.Parent = root
	local orient = Instance.new("AlignOrientation")
	orient.Mode = Enum.OrientationAlignmentMode.OneAttachment
	orient.Attachment0 = att
	orient.MaxTorque = 5000 * k * k * k
	orient.Responsiveness = 15
	orient.Parent = root
	model.PrimaryPart = root
	model:SetAttribute("Enemy", typeName)
	model:SetAttribute("MaxHealth", def.Health)
	model:SetAttribute("Health", def.Health)
	model.Parent = folder
	root:SetNetworkOwner(nil)
	return model, root, align, orient
end

function Enemies.groundAt(pos)
	-- Only the level maps count as ground (never players or enemies).
	local params = RaycastParams.new()
	params.FilterType = Enum.RaycastFilterType.Include
	local maps = {}
	for name in pairs(MapData.Levels) do
		local f = workspace:FindFirstChild(name)
		if f then
			table.insert(maps, f)
		end
	end
	params.FilterDescendantsInstances = maps
	local hit = workspace:Raycast(pos + Vector3.new(0, 30, 0), Vector3.new(0, -120, 0), params)
	return hit and hit.Position or pos
end

-- opts: { Rift = index, Mission = tag, HuntFor = player }
function Enemies.spawn(pos, typeName, opts)
	typeName = typeName or "Gremlin"
	opts = opts or {}
	local def = Config.EnemyTypes[typeName]
	pos = Enemies.groundAt(pos) + Vector3.new(0, def.Size * 0.5 + 1, 0)
	local model, root, align, orient = makeModel(typeName, def, pos)
	local e = {
		Model = model, Root = root, Align = align, Orient = orient, Def = def, Type = typeName,
		Health = def.Health, MaxHealth = def.Health, Posture = 0, PostureAt = 0, Staggered = 0,
		State = "Chase", StateUntil = 0, NextAttack = os.clock() + rng:NextNumber(0.5, 2), Stun = 0,
		Radius = def.Size * 0.5, Rift = opts.Rift, Mission = opts.Mission, HuntFor = opts.HuntFor,
		Phase = rng:NextNumber(0, 6.28), Home = pos,
	}
	table.insert(list, e)
	Net.fireAll("Fx", { Kind = "Burst", Pos = pos, Color = def.Color, Count = 12 })
	return e
end

-- Bosses and other custom enemies join the same list so every attack hits them.
function Enemies.register(e)
	table.insert(list, e)
end

function Enemies.list()
	return list
end

function Enemies.count(predicate)
	local n = 0
	for _, e in ipairs(list) do
		if e.Health > 0 and e.Model.Parent and predicate(e) then
			n = n + 1
		end
	end
	return n
end

local function kill(e, attacker)
	e.Health = 0
	Net.fireAll("Fx", { Kind = "Burst", Pos = e.Root.Position, Color = e.Def.Color or PURPLE, Count = e.IsBoss and 120 or 26, Gravity = true })
	if e.OnDeath then
		e.OnDeath(attacker)
	end
	e.Model:Destroy()
	local st = attacker and PlayerState.get(attacker)
	if st then
		PlayerState.addMeter(st, 5)
		st.Kills = st.Kills + 1
		attacker:SetAttribute("Kills", st.Kills)
		if Progression and not e.IsBoss then -- the boss pays everyone itself
			Progression.award(attacker, e.Def.Xp or 10, e.Def.Cash or 5, e.Type)
		end
		if HeatSystem and e.Def.Heat then
			HeatSystem.addChaos(attacker, e.Def.Heat)
		end
	end
end

-- Damage from a player. opts.Melee lets a hit on a staggered enemy become a finisher.
function Enemies.damage(e, amount, knock, attacker, opts)
	if e.Health <= 0 or not e.Model.Parent then
		return false
	end
	local now = os.clock()
	local finisher = opts and opts.Melee and e.Staggered > now
	if finisher then
		amount = e.IsBoss and amount * 3 + 120 or e.Health
		e.Staggered = 0
		e.Model:SetAttribute("Staggered", nil)
		Net.fireAll("Fx", { Kind = "Finisher", Pos = e.Root.Position, Model = e.Model, Attacker = attacker })
		local humanoid = attacker and attacker.Character and attacker.Character:FindFirstChildOfClass("Humanoid")
		if humanoid then
			humanoid.Health = math.min(humanoid.MaxHealth, humanoid.Health + Config.Combat.FinisherHeal)
		end
	end
	local st = attacker and PlayerState.get(attacker)
	if st and Progression then
		amount = amount * (st.DamageMul or 1)
	end
	e.Health = e.Health - amount
	e.Model:SetAttribute("Health", math.max(0, math.floor(e.Health)))
	if not e.IsBoss or finisher then
		e.Stun = 0.35
		e.Align.Enabled = false
		e.Root.AssemblyLinearVelocity = knock
	end
	if e.State == "Windup" and not e.IsBoss and (opts and opts.Melee) then
		-- A solid hit interrupts small enemies' wind-ups.
		e.State = "Recover"
		e.StateUntil = now + 0.4
		e.Model:SetAttribute("Windup", nil)
	end
	-- Posture.
	e.Posture = e.Posture + amount * (opts and opts.Posture or 1)
	e.PostureAt = now
	if e.Posture >= (e.Def.Posture or 100) and e.Staggered < now and e.Health > 0 then
		e.Posture = 0
		e.Staggered = now + Config.Combat.StaggerTime
		e.State = "Recover"
		e.StateUntil = e.Staggered
		e.Model:SetAttribute("Windup", nil)
		e.Model:SetAttribute("Staggered", true)
		e.Pending = nil -- a broken posture cancels a pending boss attack
		Net.fireAll("Fx", { Kind = "Stagger", Pos = e.Root.Position, Model = e.Model })
	end
	Net.fireAll("Fx", { Kind = "Hit", Pos = e.Root.Position, Color = Color3.new(1, 1, 1), Model = e.Model, Attacker = attacker })
	if st then
		PlayerState.addMeter(st, amount * 0.06 * (st.MeterMul or 1))
		if Combat then
			Combat.registerHit(attacker)
		end
	end
	if e.Health <= 0 then
		kill(e, attacker)
		return true
	end
	return false
end

-- Counter: cancel the wind-up of every enemy about to hit this player.
function Enemies.counter(player, pos)
	local countered = 0
	for _, e in ipairs(list) do
		-- Only the last part of a wind-up can be countered: it takes timing, not mashing.
		local inWindow = e.State == "Windup" and os.clock() >= e.StateUntil - Config.Combat.CounterWindow
		if e.Health > 0 and e.Model.Parent and inWindow and (e.Target == player or (e.Root.Position - pos).Magnitude < Config.Combat.CounterRange) then
			if (e.Root.Position - pos).Magnitude < Config.Combat.CounterRange + e.Radius then
				e.State = "Recover"
				e.StateUntil = os.clock() + 1.0
				e.Model:SetAttribute("Windup", nil)
				local away = e.Root.Position - pos
				away = Vector3.new(away.X, 0, away.Z)
				away = away.Magnitude > 0.1 and away.Unit or Vector3.new(0, 0, 1)
				Enemies.damage(e, Config.Combat.CounterDamage, away * 45 + Vector3.new(0, 20, 0), player, { Posture = 3, Melee = false })
				countered = countered + 1
			end
		end
	end
	-- Parry incoming shots back at whoever fired them.
	for id, s in pairs(shots) do
		if (s.Pos - pos).Magnitude < 12 and s.Vel:Dot(pos - s.Pos) > 0 then
			Net.fireAll("Fx", { Kind = "ShotEnd", Id = id, Pos = s.Pos })
			shots[id] = nil
			if s.Shooter and s.Shooter.Health > 0 then
				Enemies.damage(s.Shooter, s.Damage * 2.5, Vector3.new(0, 25, 0), player, { Posture = 2 })
				Net.fireAll("Fx", { Kind = "Deflect", From = s.Pos, To = s.Shooter.Root.Position })
			end
			countered = countered + 1
		end
	end
	return countered
end

local function nearestPlayer(pos, range)
	local best, bestDist = nil, range or Config.Enemy.AggroRange
	for _, player in ipairs(Players:GetPlayers()) do
		local character = player.Character
		local root = character and character:FindFirstChild("HumanoidRootPart")
		local humanoid = character and character:FindFirstChildOfClass("Humanoid")
		if root and humanoid and humanoid.Health > 0 then
			local d = (root.Position - pos).Magnitude
			if d < bestDist then
				best, bestDist = player, d
			end
		end
	end
	return best, bestDist
end

local function rootOf(player)
	local character = player and player.Character
	local humanoid = character and character:FindFirstChildOfClass("Humanoid")
	if humanoid and humanoid.Health > 0 then
		return character:FindFirstChild("HumanoidRootPart")
	end
	return nil
end

function Enemies.fireShot(shooter, from, target, speed, damage, color)
	local id = nextShot
	nextShot = nextShot - 1
	local vel = (target - from).Unit * speed
	shots[id] = { Pos = from, Vel = vel, Life = 3, Damage = damage, Shooter = shooter }
	Net.fireAll("Fx", { Kind = "Shot", Id = id, Pos = from, Vel = vel, Color = color or Color3.fromRGB(150, 255, 90), Radius = 0.9, Life = 3 })
end

local function strike(e, now)
	local def = e.Def
	local target = e.Target
	local troot = rootOf(target)
	e.Model:SetAttribute("Windup", nil)
	e.State = "Recover"
	e.StateUntil = now + 0.6
	e.NextAttack = now + rng:NextNumber(1.2, 2.2)
	if not troot then
		return
	end
	if def.Ranged then
		Enemies.fireShot(e, e.Root.Position, troot.Position, 75, def.Damage, def.Color)
		return
	end
	-- Lunge, then check the hit.
	e.Align.MaxVelocity = def.Speed * 4
	e.Align.Position = troot.Position
	task.delay(0.15, function()
		e.Align.MaxVelocity = def.Speed
		if e.Health <= 0 or not e.Model.Parent then
			return
		end
		if def.Slam then
			Net.fireAll("Fx", { Kind = "Shockwave", Pos = e.Root.Position - Vector3.new(0, e.Radius, 0), Radius = def.Slam, Time = 0.3, Color = def.Color })
			for _, player in ipairs(Players:GetPlayers()) do
				local r = rootOf(player)
				if r and (r.Position - e.Root.Position).Magnitude < def.Slam + 2 then
					Combat.damagePlayer(player, def.Damage, e.Root.Position)
				end
			end
		else
			local r = rootOf(target)
			if r and (r.Position - e.Root.Position).Magnitude < def.Reach + e.Radius + 2 then
				Combat.damagePlayer(target, def.Damage, e.Root.Position)
			end
		end
	end)
end

local function think(e, now, step)
	local def = e.Def
	if e.Stun > 0 then
		e.Stun = e.Stun - step
		if e.Stun <= 0 then
			e.Align.Enabled = true
		end
	end
	-- Posture recovers when left alone.
	if now - e.PostureAt > 2 and e.Posture > 0 then
		e.Posture = math.max(0, e.Posture - Config.Combat.PostureDecay * step)
	end
	local pos = e.Root.Position
	if e.Staggered > now then
		e.Align.Position = pos
		return
	elseif e.Model:GetAttribute("Staggered") then
		e.Model:SetAttribute("Staggered", nil)
	end
	if e.State == "Windup" then
		if now >= e.StateUntil then
			strike(e, now)
		end
		return
	end
	if e.State == "Recover" and now < e.StateUntil then
		return
	end
	e.State = "Chase"
	local player, dist
	if e.HuntFor then
		-- Hunters give up on targets who left Retroville (the heat system dismisses them too).
		local hst = PlayerState.get(e.HuntFor)
		if not hst or hst.Level ~= Config.Levels.Retroville then
			e.HuntFor = nil
		end
	end
	if e.HuntFor and rootOf(e.HuntFor) then
		player = e.HuntFor
		dist = (rootOf(player).Position - pos).Magnitude
	else
		player, dist = nearestPlayer(pos)
	end
	local bob = math.sin(now * 3 + e.Phase) * 1.2
	if not player then
		e.Align.Position = e.Home + Vector3.new(math.cos(now * 0.5 + e.Phase) * 6, 2 + bob, math.sin(now * 0.5 + e.Phase) * 6)
		return
	end
	local target = rootOf(player).Position
	local flat = Vector3.new(target.X - pos.X, 0, target.Z - pos.Z)
	if flat.Magnitude > 0.1 then
		e.Orient.CFrame = CFrame.lookAt(Vector3.zero, flat.Unit)
	end
	if def.Ranged and dist < def.KeepAway then
		e.Align.Position = pos - flat.Unit * 6 + Vector3.new(0, bob, 0)
	else
		e.Align.Position = target + Vector3.new(0, 1 + bob, 0)
	end
	if dist < def.Reach + e.Radius + 1.5 and now >= e.NextAttack then
		e.State = "Windup"
		e.StateUntil = now + def.Windup
		e.Target = player
		e.Align.Position = pos
		e.Model:SetAttribute("Windup", workspace:GetServerTimeNow())
		Net.fireAll("Fx", { Kind = "Telegraph", Model = e.Model, Target = player, Time = def.Windup, Ranged = def.Ranged == true })
	end
end

local function stepShots(dt)
	for id, s in pairs(shots) do
		s.Life = s.Life - dt
		local move = s.Vel * dt
		local hitPlayer
		for _, player in ipairs(Players:GetPlayers()) do
			local r = rootOf(player)
			if r then
				local rel = r.Position - s.Pos
				local along = math.clamp(rel:Dot(move.Unit), 0, move.Magnitude)
				if (r.Position - (s.Pos + move.Unit * along)).Magnitude < 2.6 then
					hitPlayer = player
				end
			end
		end
		if hitPlayer then
			Combat.damagePlayer(hitPlayer, s.Damage, s.Pos)
			Net.fireAll("Fx", { Kind = "ShotEnd", Id = id, Pos = s.Pos })
			shots[id] = nil
		elseif s.Life <= 0 then
			Net.fireAll("Fx", { Kind = "ShotEnd", Id = id, Pos = s.Pos })
			shots[id] = nil
		else
			s.Pos = s.Pos + move
		end
	end
end

local function buildRiftVisual(pos)
	local model = Instance.new("Model")
	model.Name = "Rift"
	for i = 0, 23 do
		local a = i / 24 * math.pi * 2
		local p = Instance.new("Part")
		p.Anchored = true
		p.CanCollide = false
		p.CanQuery = false
		p.Material = Enum.Material.Neon
		p.Color = i % 2 == 0 and PURPLE or Color3.fromRGB(255, 120, 220)
		p.Size = Vector3.new(1.6, 1.6, 2.4)
		p.CFrame = CFrame.new(pos + Vector3.new(math.cos(a) * 9, 11 + math.sin(a) * 9, 0)) * CFrame.Angles(0, 0, a)
		p.Parent = model
	end
	local core = Instance.new("Part")
	core.Name = "Core"
	core.Anchored = true
	core.CanCollide = false
	core.CanQuery = false
	core.Material = Enum.Material.Neon
	core.Color = Color3.fromRGB(60, 20, 90)
	core.Transparency = 0.3
	core.Shape = Enum.PartType.Cylinder
	core.Size = Vector3.new(0.6, 16, 16)
	core.CFrame = CFrame.new(pos + Vector3.new(0, 11, 0)) * CFrame.Angles(0, math.pi / 2, 0)
	core.Parent = model
	local emitter = Instance.new("ParticleEmitter")
	emitter.Color = ColorSequence.new(PURPLE, Color3.fromRGB(255, 150, 255))
	emitter.Rate = 30
	emitter.Speed = NumberRange.new(4, 10)
	emitter.SpreadAngle = Vector2.new(180, 180)
	emitter.Lifetime = NumberRange.new(0.6, 1.4)
	emitter.LightEmission = 1
	emitter.Parent = core
	model.Parent = folder
	return model
end

local WAVES = {
	{ Gremlin = 4 },
	{ Gremlin = 3, Spitter = 2 },
	{ Gremlin = 3, Spitter = 2, Brute = 1 },
}

function Enemies.spawnWave(center, composition, opts, spread)
	for typeName, n in pairs(composition) do
		for _ = 1, n do
			Enemies.spawn(center + Vector3.new(rng:NextNumber(-spread, spread), 6, rng:NextNumber(-spread, spread)), typeName, opts)
		end
	end
end

function Enemies.allRiftsClosed()
	for _, r in ipairs(rifts) do
		if not r.Closed then
			return false
		end
	end
	return #rifts > 0
end

function Enemies.init(systems)
	Net, PlayerState, Combat = systems.Net, systems.Players, systems.Combat
	Progression, HeatSystem = systems.Progression, systems.Heat
	folder = Instance.new("Folder")
	folder.Name = "Enemies"
	folder.Parent = workspace
end

function Enemies.start()
	for levelName, level in pairs(MapData.Levels) do
		for _, r in ipairs(level.Rifts) do
			table.insert(rifts, { Name = r.Name, Pos = r.Pos, Level = levelName, Wave = 0, Closed = false, Visual = buildRiftVisual(r.Pos) })
		end
	end
	Enemies.onAllRiftsClosed = Enemies.onAllRiftsClosed or function() end

	local accum = 0
	RunService.Heartbeat:Connect(function(dt)
		stepShots(dt)
		accum = accum + dt
		if accum < 0.05 then
			return
		end
		local step = accum
		accum = 0
		local now = os.clock()
		for i = #list, 1, -1 do
			local e = list[i]
			if not e.Model.Parent or e.Health <= 0 then
				table.remove(list, i)
			elseif e.Think then
				e.Think(e, now, step)
			else
				think(e, now, step)
			end
		end
		for index, rift in ipairs(rifts) do
			if not rift.Closed then
				if rift.Wave == 0 then
					local player, dist = nearestPlayer(rift.Pos)
					if player and dist < 80 then
						rift.Wave = 1
						Net.fireAll("Notify", { Text = "A dimensional rift opens! Wave 1/3", Color = PURPLE })
						Enemies.spawnWave(rift.Pos, WAVES[1], { Rift = index }, 25)
					end
				elseif Enemies.count(function(e)
					return e.Rift == index
				end) == 0 then
					if rift.Wave >= 3 then
						rift.Closed = true
						rift.Visual:Destroy()
						Net.fireAll("Notify", { Banner = "RIFT CLOSED!", Text = rift.Name:gsub("_", " ") .. " is safe again" })
						for player, st in pairs(PlayerState.all()) do
							PlayerState.addMeter(st, 25)
							local root = rootOf(player)
							if Progression and root and (root.Position - rift.Pos).Magnitude < 250 then
								Progression.award(player, 120, 60, "Rift closed")
							end
						end
						if Enemies.allRiftsClosed() then
							Enemies.onAllRiftsClosed()
						end
					else
						rift.Wave = rift.Wave + 1
						Net.fireAll("Notify", { Text = "Wave " .. rift.Wave .. "/3", Color = PURPLE })
						Enemies.spawnWave(rift.Pos, WAVES[rift.Wave], { Rift = index }, 30)
					end
				end
			end
		end
	end)

	task.spawn(function()
		while true do
			task.wait(150)
			for _, rift in ipairs(rifts) do
				if rift.Closed then
					rift.Closed = false
					rift.Wave = 0
					rift.Visual = buildRiftVisual(rift.Pos)
				end
			end
		end
	end)
end

function Enemies.rifts()
	return rifts
end

return Enemies
