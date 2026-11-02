--!nonstrict
-- Rift gremlins and Retroville's rift patrols: walk up to a purple rift and it
-- opens; beat three waves to close it. Gremlins hover toward the nearest
-- player, deal contact damage, and get knocked around by hits.

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))
local MapData = require(Shared:WaitForChild("MapData"))

local Enemies = {}
local Net, PlayerState, Combat
local list = {}   -- { Model, Root, Align, Health, Rift, Stun, AttackAt, Phase }
local rifts = {}  -- { Name, Pos, Level, Wave, Closed, Visual }
local folder
local rng = Random.new(1337)

local PURPLE = Color3.fromRGB(170, 80, 230)

local function makeGremlin(pos)
	local model = Instance.new("Model")
	model.Name = "RiftGremlin"
	local root = Instance.new("Part")
	root.Name = "Body"
	root.Shape = Enum.PartType.Ball
	root.Size = Vector3.new(3.2, 3.2, 3.2)
	root.Color = PURPLE
	root.Material = Enum.Material.SmoothPlastic
	root.CFrame = CFrame.new(pos)
	root.CustomPhysicalProperties = PhysicalProperties.new(0.5, 0.3, 0.5)
	root.Parent = model
	local function add(size, offset, color, material, shape)
		local p = Instance.new("Part")
		p.Size = size
		p.Color = color
		p.Material = material or Enum.Material.SmoothPlastic
		p.Shape = shape or Enum.PartType.Ball
		p.CanCollide = false
		p.Massless = true
		p.CFrame = root.CFrame * offset
		p.Parent = model
		local w = Instance.new("WeldConstraint")
		w.Part0 = root
		w.Part1 = p
		w.Parent = p
		return p
	end
	for _, x in ipairs({ -0.6, 0.6 }) do
		add(Vector3.new(1.1, 1.1, 1.1), CFrame.new(x, 0.5, -1.2), Color3.new(1, 1, 1))
		add(Vector3.new(0.5, 0.5, 0.5), CFrame.new(x, 0.5, -1.65), Color3.fromRGB(20, 10, 30))
		add(Vector3.new(0.4, 1.4, 0.4), CFrame.new(x * 1.3, 1.6, 0) * CFrame.Angles(0, 0, -x * 0.6), Color3.fromRGB(90, 30, 140), nil, Enum.PartType.Block)
	end
	add(Vector3.new(1.4, 0.3, 0.3), CFrame.new(0, -0.5, -1.45), Color3.fromRGB(255, 220, 255), Enum.Material.Neon, Enum.PartType.Block)
	local att = Instance.new("Attachment")
	att.Parent = root
	local align = Instance.new("AlignPosition")
	align.Mode = Enum.PositionAlignmentMode.OneAttachment
	align.Attachment0 = att
	align.MaxForce = 6000
	align.MaxVelocity = Config.Enemy.Speed
	align.Responsiveness = 25
	align.Position = pos
	align.Parent = root
	local orient = Instance.new("AlignOrientation")
	orient.Mode = Enum.OrientationAlignmentMode.OneAttachment
	orient.Attachment0 = att
	orient.MaxTorque = 5000
	orient.Responsiveness = 15
	orient.Parent = root
	model.PrimaryPart = root
	model:SetAttribute("Enemy", true)
	model.Parent = folder
	root:SetNetworkOwner(nil)
	return model, root, align, orient
end

function Enemies.spawn(pos, riftIndex)
	local params = RaycastParams.new()
	params.FilterType = Enum.RaycastFilterType.Exclude
	params.FilterDescendantsInstances = { folder }
	local down = workspace:Raycast(pos + Vector3.new(0, 20, 0), Vector3.new(0, -80, 0), params)
	if down then
		pos = down.Position + Vector3.new(0, 4, 0)
	end
	local model, root, align, orient = makeGremlin(pos)
	table.insert(list, {
		Model = model, Root = root, Align = align, Orient = orient, Health = Config.Enemy.Health,
		Rift = riftIndex, Stun = 0, AttackAt = 0, Phase = rng:NextNumber(0, 6.28), Home = pos,
	})
	Net.fireAll("Fx", { Kind = "Burst", Pos = pos, Color = PURPLE, Count = 12 })
end

function Enemies.list()
	return list
end

-- Damage from a player's attack. Returns true when it killed.
function Enemies.damage(enemy, amount, knock, attacker)
	if enemy.Health <= 0 then
		return false
	end
	enemy.Health = enemy.Health - amount
	enemy.Stun = 0.35
	enemy.Align.Enabled = false
	enemy.Root.AssemblyLinearVelocity = knock
	Net.fireAll("Fx", { Kind = "Hit", Pos = enemy.Root.Position, Color = Color3.new(1, 1, 1), Model = enemy.Model, Attacker = attacker })
	local st = attacker and PlayerState.get(attacker)
	if st then
		PlayerState.addMeter(st, amount * 0.06)
	end
	if enemy.Health <= 0 then
		Net.fireAll("Fx", { Kind = "Burst", Pos = enemy.Root.Position, Color = PURPLE, Count = 26, Gravity = true })
		enemy.Model:Destroy()
		if st then
			PlayerState.addMeter(st, 5)
			st.Kills = st.Kills + 1
			attacker:SetAttribute("Kills", st.Kills)
		end
		return true
	end
	return false
end

local function nearestPlayer(pos)
	local best, bestDist = nil, Config.Enemy.AggroRange
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

local function buildRiftVisual(pos)
	local model = Instance.new("Model")
	model.Name = "Rift"
	for i = 0, 23 do
		local a = i / 24 * math.pi * 2
		local p = Instance.new("Part")
		p.Anchored = true
		p.CanCollide = false
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

function Enemies.init(systems)
	Net, PlayerState, Combat = systems.Net, systems.Players, systems.Combat
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

	local accum = 0
	RunService.Heartbeat:Connect(function(dt)
		accum = accum + dt
		local t = os.clock()
		-- AI at 20 Hz.
		if accum < 0.05 then
			return
		end
		local step = accum
		accum = 0
		for i = #list, 1, -1 do
			local e = list[i]
			if not e.Model.Parent or e.Health <= 0 then
				table.remove(list, i)
			else
				if e.Stun > 0 then
					e.Stun = e.Stun - step
					if e.Stun <= 0 then
						e.Align.Enabled = true
					end
				end
				local pos = e.Root.Position
				local player, dist = nearestPlayer(pos)
				local bob = math.sin(t * 3 + e.Phase) * 1.2
				if player then
					local target = player.Character.HumanoidRootPart.Position
					local flat = Vector3.new(target.X - pos.X, 0, target.Z - pos.Z)
					local goal = target + Vector3.new(0, 1 + bob, 0)
					if flat.Magnitude > 0.1 then
						e.Orient.CFrame = CFrame.lookAt(Vector3.zero, flat.Unit)
					end
					e.Align.Position = goal
					if dist < Config.Enemy.ContactRange + 2 and t > e.AttackAt then
						e.AttackAt = t + Config.Enemy.AttackCooldown
						if Combat then
							Combat.damagePlayer(player, Config.Enemy.ContactDamage)
						end
					end
				else
					e.Align.Position = e.Home + Vector3.new(math.cos(t * 0.5 + e.Phase) * 6, 2 + bob, math.sin(t * 0.5 + e.Phase) * 6)
				end
			end
		end
		-- Rift patrols.
		for index, rift in ipairs(rifts) do
			if not rift.Closed then
				if rift.Wave == 0 then
					local player, dist = nearestPlayer(rift.Pos)
					if player and dist < 80 then
						rift.Wave = 1
						Net.fireAll("Notify", { Text = "A dimensional rift opens! Wave 1/3", Color = PURPLE })
						for _ = 1, 4 do
							Enemies.spawn(rift.Pos + Vector3.new(rng:NextNumber(-25, 25), 6, rng:NextNumber(-25, 25)), index)
						end
					end
				else
					local alive = 0
					for _, e in ipairs(list) do
						if e.Rift == index and e.Health > 0 then
							alive = alive + 1
						end
					end
					if alive == 0 then
						if rift.Wave >= 3 then
							rift.Closed = true
							rift.Visual:Destroy()
							Net.fireAll("Notify", { Banner = "RIFT CLOSED!", Text = rift.Name:gsub("_", " ") .. " is safe again" })
							for _, st in pairs(PlayerState.all()) do
								PlayerState.addMeter(st, 25)
							end
						else
							rift.Wave = rift.Wave + 1
							Net.fireAll("Notify", { Text = "Wave " .. rift.Wave .. "/3", Color = PURPLE })
							for _ = 1, 3 + rift.Wave * 2 do
								Enemies.spawn(rift.Pos + Vector3.new(rng:NextNumber(-30, 30), 6, rng:NextNumber(-30, 30)), index)
							end
						end
					end
				end
			end
		end
	end)

	-- Reopen closed rifts after a while so the world stays lively.
	task.spawn(function()
		while true do
			task.wait(120)
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

return Enemies
