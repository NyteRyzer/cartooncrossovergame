--!nonstrict
-- Rift shards: crystals scattered over every world (rooftops, ledges, and
-- every fourth one high in the sky). Each player collects their own set; a
-- shard refills health, energy and some of the ultimate meter.

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))
local MapData = require(Shared:WaitForChild("MapData"))

local Collectibles = {}
local Net, PlayerState
local shards = {}  -- { Id, Pos, Level }
local taken = {}   -- [player] = { [id] = true }

local function place(levelName, level, count, seed)
	local rng = Random.new(seed)
	local lo, hi = level.Bounds.Min, level.Bounds.Max
	local span = hi - lo
	local minGap = math.max(span.X, span.Z) / (count <= 10 and 8 or 9)
	local placed = {}
	local params = RaycastParams.new()
	params.FilterType = Enum.RaycastFilterType.Include
	params.FilterDescendantsInstances = { workspace:FindFirstChild(levelName) }
	for _ = 1, 2000 do
		if #placed >= count then
			break
		end
		local top = Vector3.new(lo.X + span.X * (0.08 + 0.84 * rng:NextNumber()), hi.Y - 1, lo.Z + span.Z * (0.08 + 0.84 * rng:NextNumber()))
		local hit = workspace:Raycast(top, Vector3.new(0, -(hi.Y - lo.Y), 0), params)
		if hit and hit.Position.Y > lo.Y + 2 and hit.Instance.Transparency < 0.5 then
			local pos = hit.Position + Vector3.new(0, 3.7, 0)
			if #placed % 4 == 3 then
				pos = pos + Vector3.new(0, 17 + 13 * rng:NextNumber(), 0)
			end
			local ok = (pos - level.Start).Magnitude > 13
			for _, other in ipairs(placed) do
				local d = Vector3.new(other.X - pos.X, 0, other.Z - pos.Z).Magnitude
				ok = ok and d > minGap
			end
			if ok then
				table.insert(placed, pos)
			end
		end
	end
	return placed
end

local function buildShard(id, pos, folder)
	local model = Instance.new("Model")
	model.Name = "RiftShard_" .. id
	local function crystal(offset, color, flip)
		local p = Instance.new("Part")
		p.Anchored = true
		p.CanCollide = false
		p.CanQuery = false
		p.Material = Enum.Material.Neon
		p.Color = color
		p.Size = Vector3.new(1.3, 1.6, 1.3)
		local mesh = Instance.new("SpecialMesh")
		mesh.MeshType = Enum.MeshType.Sphere
		mesh.Scale = Vector3.new(0.7, 1.3, 0.7)
		mesh.Parent = p
		p.CFrame = CFrame.new(pos + offset) * CFrame.Angles(flip and math.pi or 0, 0, 0)
		p.Parent = model
		return p
	end
	local core = crystal(Vector3.new(0, 0.6, 0), Color3.fromRGB(190, 130, 255))
	crystal(Vector3.new(0, -0.6, 0), Color3.fromRGB(120, 230, 255), true)
	local light = Instance.new("PointLight")
	light.Color = Color3.fromRGB(200, 160, 255)
	light.Range = 12
	light.Brightness = 2
	light.Parent = core
	local sparkle = Instance.new("ParticleEmitter")
	sparkle.Rate = 6
	sparkle.Lifetime = NumberRange.new(0.6, 1.2)
	sparkle.Speed = NumberRange.new(1, 3)
	sparkle.SpreadAngle = Vector2.new(180, 180)
	sparkle.LightEmission = 1
	sparkle.Color = ColorSequence.new(Color3.fromRGB(230, 210, 255))
	sparkle.Size = NumberSequence.new(0.3, 0)
	sparkle.Parent = core
	model.PrimaryPart = core
	model:SetAttribute("ShardId", id)
	model.Parent = folder
end

function Collectibles.init(systems)
	Net, PlayerState = systems.Net, systems.Players
end

function Collectibles.start()
	local folder = Instance.new("Folder")
	folder.Name = "Shards"
	folder.Parent = workspace
	local counts = { [Config.Levels.Nexus] = Config.Shards.Nexus, [Config.Levels.Retroville] = Config.Shards.Retroville }
	local seed = 4242
	for levelName, level in pairs(MapData.Levels) do
		seed = seed + 977
		for _, pos in ipairs(place(levelName, level, counts[levelName] or 12, seed)) do
			local id = #shards + 1
			table.insert(shards, { Id = id, Pos = pos, Level = levelName })
			buildShard(id, pos, folder)
		end
	end
	workspace:SetAttribute("ShardTotal_" .. Config.Levels.Nexus, 0)
	local totals = {}
	for _, s in ipairs(shards) do
		totals[s.Level] = (totals[s.Level] or 0) + 1
	end
	for levelName, n in pairs(totals) do
		workspace:SetAttribute("ShardTotal_" .. levelName, n)
	end

	Players.PlayerAdded:Connect(function(player)
		taken[player] = {}
	end)
	for _, player in ipairs(Players:GetPlayers()) do
		taken[player] = {}
	end
	Players.PlayerRemoving:Connect(function(player)
		taken[player] = nil
	end)

	local accum = 0
	RunService.Heartbeat:Connect(function(dt)
		accum = accum + dt
		if accum < 0.1 then
			return
		end
		accum = 0
		for _, player in ipairs(Players:GetPlayers()) do
			local character = player.Character
			local root = character and character:FindFirstChild("HumanoidRootPart")
			local humanoid = character and character:FindFirstChildOfClass("Humanoid")
			local mine = taken[player]
			if root and humanoid and humanoid.Health > 0 and mine then
				local reach = Config.Shards.Reach * math.max(1, root.Size.Y / 2)
				for _, s in ipairs(shards) do
					if not mine[s.Id] and (s.Pos - root.Position).Magnitude < reach then
						mine[s.Id] = true
						local st = PlayerState.get(player)
						humanoid.Health = math.min(humanoid.MaxHealth, humanoid.Health + Config.Shards.Health)
						if st then
							PlayerState.addMeter(st, Config.Shards.Meter)
							st.Energy = Config.Player.MaxEnergy
							st.Shards = st.Shards + 1
						end
						local have, total = 0, 0
						for _, o in ipairs(shards) do
							if o.Level == s.Level then
								total = total + 1
								if mine[o.Id] then
									have = have + 1
								end
							end
						end
						player:SetAttribute("Shards_" .. s.Level, have)
						Net.fire("Fx", player, { Kind = "ShardTaken", Id = s.Id, Pos = s.Pos })
						Net.fire("Notify", player, {
							Text = ("Rift shard %d/%d (+health, energy, ultimate)"):format(have, total),
							Color = Color3.fromRGB(200, 160, 255), Time = 2,
							Banner = have == total and "ALL RIFT SHARDS FOUND!" or nil,
						})
					end
				end
			end
		end
	end)
end

return Collectibles
