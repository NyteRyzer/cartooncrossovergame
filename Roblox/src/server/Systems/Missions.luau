--!nonstrict
-- Open-world missions: glowing markers around Retroville. Walk up and press E
-- to start; each has an objective, a waypoint and a timer, and pays XP and
-- cash. Completions are saved.
--   Rift Rush        clear three waves at Downtown before time runs out
--   Express Delivery hop in the delivery hover car, rush it to the Neutron house
--   Rooftop Run      reach the tower-top checkpoints by any means
--   Titan Hunt       summon and defeat the Rift Titan

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))
local MapData = require(Shared:WaitForChild("MapData"))

local Missions = {}
local Net, PlayerState, Enemies, World, Boss, Progression, Profiles
local active = {} -- [player] = mission run

local DEFS = {
	{ Id = "RiftRush", Name = "Rift Rush", Time = 150, Xp = 250, Cash = 150, Anchor = "Downtown", Blurb = "Clear three waves of rift monsters" },
	{ Id = "Delivery", Name = "Express Delivery", Time = 75, Xp = 200, Cash = 180, Anchor = "School", Blurb = "Rush the delivery hover car to the Neutron house" },
	{ Id = "Rooftop", Name = "Rooftop Run", Time = 110, Xp = 220, Cash = 140, Anchor = "TowerBase", Blurb = "Hit every tower-top checkpoint" },
	{ Id = "Titan", Name = "Titan Hunt", Time = 300, Xp = 300, Cash = 200, Anchor = "Plaza", Blurb = "Bring down the Rift Titan" },
}

local function rootOf(player)
	local character = player and player.Character
	local humanoid = character and character:FindFirstChildOfClass("Humanoid")
	if humanoid and humanoid.Health > 0 then
		return character:FindFirstChild("HumanoidRootPart"), humanoid
	end
	return nil
end

local function objective(player, run, text, pos)
	run.Text = text
	run.Waypoint = pos
	Net.fire("Objective", player, { Id = run.Def.Id, Name = run.Def.Name, Text = text, Pos = pos, Ends = run.EndsServer })
end

local function finish(player, success, reason)
	local run = active[player]
	if not run then
		return
	end
	active[player] = nil
	if run.Cleanup then
		run.Cleanup()
	end
	Net.fire("Objective", player, { Clear = true })
	if success then
		Net.fire("Notify", player, { Banner = "MISSION PASSED", Text = run.Def.Name, Color = Color3.fromRGB(255, 220, 90) })
		Progression.award(player, run.Def.Xp, run.Def.Cash, run.Def.Name)
		local data = Profiles.get(player)
		if data then
			data.Missions[run.Def.Id] = (data.Missions[run.Def.Id] or 0) + 1
		end
	else
		Net.fire("Notify", player, { Banner = "MISSION FAILED", Text = reason or run.Def.Name, Color = Color3.fromRGB(255, 110, 100) })
	end
end

local STARTERS = {}

STARTERS.RiftRush = function(player, run, anchor)
	run.Tag = "Mission_" .. player.UserId
	run.Wave = 1
	local waves = { { Gremlin = 4, Spitter = 1 }, { Gremlin = 4, Spitter = 2 }, { Gremlin = 3, Spitter = 2, Brute = 1 } }
	Enemies.spawnWave(anchor, waves[1], { Mission = run.Tag }, 25)
	objective(player, run, "Clear the rift monsters: wave 1/3", anchor)
	run.Tick = function()
		local alive = Enemies.count(function(e)
			return e.Mission == run.Tag
		end)
		if alive == 0 then
			if run.Wave >= 3 then
				finish(player, true)
			else
				run.Wave = run.Wave + 1
				Enemies.spawnWave(anchor, waves[run.Wave], { Mission = run.Tag }, 30)
				objective(player, run, "Clear the rift monsters: wave " .. run.Wave .. "/3", anchor)
			end
		end
	end
	run.Cleanup = function()
		for _, e in ipairs(Enemies.list()) do
			if e.Mission == run.Tag and e.Model.Parent then
				e.Model:Destroy()
			end
		end
	end
end

STARTERS.Delivery = function(player, run, anchor)
	local dest = MapData.Anchors.NeutronHouse
	local car = World.buildHoverCar(anchor + Vector3.new(14, 0, 0), 0, Color3.fromRGB(250, 200, 40))
	run.Car = car
	objective(player, run, "Get in the delivery hover car", car.PrimaryPart.Position)
	run.Tick = function()
		if not car.Parent then
			finish(player, false, "The delivery car was lost")
			return
		end
		local _, humanoid = rootOf(player)
		local seat = humanoid and humanoid.SeatPart
		local inCar = seat and seat:IsDescendantOf(car)
		if inCar and run.Text ~= "Deliver the package!" then
			objective(player, run, "Deliver the package!", dest)
		elseif not inCar and run.Text == "Deliver the package!" then
			objective(player, run, "Get back in the delivery hover car", car.PrimaryPart.Position)
		end
		if inCar and (car.PrimaryPart.Position - dest).Magnitude < 30 then
			finish(player, true)
		end
	end
	run.Cleanup = function()
		task.delay(8, function()
			if car.Parent then
				car:Destroy()
			end
		end)
	end
end

STARTERS.Rooftop = function(player, run)
	local points = MapData.Anchors.Towers
	run.Index = 1
	local rings = {}
	for i, p in ipairs(points) do
		local ring = Instance.new("Part")
		ring.Name = "Checkpoint"
		ring.Anchored = true
		ring.CanCollide = false
		ring.CanQuery = false
		ring.Shape = Enum.PartType.Cylinder
		ring.Material = Enum.Material.Neon
		ring.Color = i == 1 and Color3.fromRGB(255, 220, 60) or Color3.fromRGB(120, 255, 210)
		ring.Transparency = 0.45
		ring.Size = Vector3.new(0.6, 16, 16)
		ring.CFrame = CFrame.new(p + Vector3.new(0, 4, 0)) * CFrame.Angles(0, 0, math.pi / 2)
		ring.Parent = workspace
		rings[i] = ring
	end
	objective(player, run, "Checkpoint 1/" .. #points, points[1])
	run.Tick = function()
		local root = rootOf(player)
		local p = points[run.Index]
		if root and p and (root.Position - (p + Vector3.new(0, 4, 0))).Magnitude < 12 then
			rings[run.Index]:Destroy()
			Net.fire("Fx", player, { Kind = "Burst", Pos = p, Color = Color3.fromRGB(255, 220, 60), Count = 30 })
			run.Index = run.Index + 1
			if run.Index > #points then
				finish(player, true)
			else
				rings[run.Index].Color = Color3.fromRGB(255, 220, 60)
				objective(player, run, ("Checkpoint %d/%d"):format(run.Index, #points), points[run.Index])
			end
		end
	end
	run.Cleanup = function()
		for _, ring in pairs(rings) do
			if ring.Parent then
				ring:Destroy()
			end
		end
	end
end

STARTERS.Titan = function(player, run, anchor)
	local done = false
	local boss = Boss.spawn(anchor, function()
		done = true
	end)
	objective(player, run, "Defeat the " .. Config.Boss.Name, boss and boss.Root.Position or anchor)
	run.Tick = function()
		local current = Boss.active()
		if done or not current then
			finish(player, true)
		elseif current.Root.Parent then
			run.Waypoint = current.Root.Position
		end
	end
	run.AllowDeath = true
end

local function start(player, def)
	if active[player] then
		Net.fire("Notify", player, { Text = "Finish your current mission first", Time = 2 })
		return
	end
	local st = PlayerState.get(player)
	if not st or st.Level ~= Config.Levels.Retroville then
		return
	end
	local anchor = MapData.Anchors[def.Anchor]
	local run = { Def = def, Ends = os.clock() + def.Time, EndsServer = workspace:GetServerTimeNow() + def.Time }
	active[player] = run
	Net.fire("Notify", player, { Banner = string.upper(def.Name), Text = def.Blurb, Color = Color3.fromRGB(255, 220, 90) })
	STARTERS[def.Id](player, run, anchor)
end

local function buildMarker(def)
	local pos = MapData.Anchors[def.Anchor]
	if not pos then
		return
	end
	local ground = Enemies.groundAt(pos)
	local marker = Instance.new("Part")
	marker.Name = "MissionMarker_" .. def.Id
	marker.Anchored = true
	marker.CanCollide = false
	marker.CanQuery = false
	marker.Shape = Enum.PartType.Cylinder
	marker.Material = Enum.Material.Neon
	marker.Color = Color3.fromRGB(255, 210, 60)
	marker.Transparency = 0.4
	marker.Size = Vector3.new(0.4, 8, 8)
	marker.CFrame = CFrame.new(ground + Vector3.new(0, 0.3, 0)) * CFrame.Angles(0, 0, math.pi / 2)
	marker:SetAttribute("Mission", def.Name)
	marker.Parent = workspace:FindFirstChild("Interactables") or workspace
	local beam = Instance.new("Part")
	beam.Anchored = true
	beam.CanCollide = false
	beam.CanQuery = false
	beam.Material = Enum.Material.Neon
	beam.Color = Color3.fromRGB(255, 210, 60)
	beam.Transparency = 0.7
	beam.Size = Vector3.new(1.5, 80, 1.5)
	beam.CFrame = CFrame.new(ground + Vector3.new(0, 40, 0))
	beam.Parent = marker
	local prompt = Instance.new("ProximityPrompt")
	prompt.ActionText = "Start mission"
	prompt.ObjectText = def.Name
	prompt.KeyboardKeyCode = Enum.KeyCode.E
	prompt.HoldDuration = 0.4
	prompt.MaxActivationDistance = 12
	prompt.RequiresLineOfSight = false
	prompt.Parent = marker
	prompt.Triggered:Connect(function(player)
		start(player, def)
	end)
	local gui = Instance.new("BillboardGui")
	gui.Size = UDim2.fromOffset(240, 50)
	gui.StudsOffset = Vector3.new(0, 9, 0)
	gui.MaxDistance = 300
	gui.Parent = marker
	local label = Instance.new("TextLabel")
	label.BackgroundTransparency = 1
	label.Size = UDim2.fromScale(1, 1)
	label.Font = Enum.Font.FredokaOne
	label.TextScaled = true
	label.TextColor3 = Color3.fromRGB(255, 220, 90)
	label.TextStrokeTransparency = 0.2
	label.Text = "★ " .. def.Name
	label.Parent = gui
end

function Missions.init(systems)
	Net, PlayerState, Enemies, World, Boss = systems.Net, systems.Players, systems.Enemies, systems.World, systems.Boss
	Progression, Profiles = systems.Progression, systems.Profiles
end

function Missions.start()
	if not MapData.Anchors then
		return
	end
	for _, def in ipairs(DEFS) do
		buildMarker(def)
	end
	Players.PlayerRemoving:Connect(function(player)
		local run = active[player]
		if run and run.Cleanup then
			run.Cleanup()
		end
		active[player] = nil
	end)
	local accum = 0
	RunService.Heartbeat:Connect(function(dt)
		accum = accum + dt
		if accum < 0.2 then
			return
		end
		accum = 0
		local now = os.clock()
		for player, run in pairs(active) do
			local root = rootOf(player)
			local st = PlayerState.get(player)
			if not run.AllowDeath and not root then
				finish(player, false, "You were knocked out")
			elseif st and st.Level ~= Config.Levels.Retroville then
				finish(player, false, "You left Retroville")
			elseif now > run.Ends then
				finish(player, false, "Out of time")
			elseif run.Tick then
				local ok, err = pcall(run.Tick)
				if not ok then
					warn("[Missions] " .. tostring(err))
					finish(player, false)
				end
			end
		end
	end)
end

return Missions
