--!nonstrict
-- Wanted level, open-world-crime-game style but for superheroes: the more rift
-- monsters you smash in Retroville, the more "threat stars" you collect.
-- While you have stars, Hunter squads are dispatched after you; keep fighting
-- and it escalates, break away (no combat, no hunters near) and the stars
-- cool off one by one. Shown on the HUD as flashing stars while cooling.

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))
local HeatRules = require(Shared:WaitForChild("Heat"))

local HeatSystem = {}
local Net, PlayerState, Enemies
local heat = {} -- [player] = Heat

local function heatOf(player)
	local h = heat[player]
	if not h then
		h = HeatRules.new(Config)
		heat[player] = h
	end
	return h
end

local function inRetroville(player)
	local st = PlayerState.get(player)
	return st and st.Level == Config.Levels.Retroville
end

function HeatSystem.addChaos(player, amount)
	if not inRetroville(player) then
		return
	end
	local h = heatOf(player)
	local before = h:Stars()
	h:AddChaos(amount, os.clock())
	if h:Stars() > before then
		Net.fire("Notify", player, { Text = ("Threat level %d! Hunters are coming for you"):format(h:Stars()), Color = Color3.fromRGB(255, 120, 90), Time = 2.5 })
	end
end

function HeatSystem.clear(player)
	heat[player] = nil
	for _, e in ipairs(Enemies.list()) do
		if e.HuntFor == player and e.Model.Parent then
			e.Model:Destroy()
		end
	end
	player:SetAttribute("Heat", 0)
	player:SetAttribute("HeatCooling", false)
end

local function huntersNear(player, root)
	for _, e in ipairs(Enemies.list()) do
		if e.HuntFor == player and e.Health > 0 and e.Model.Parent and (e.Root.Position - root.Position).Magnitude < Config.Heat.NearRange then
			return true
		end
	end
	return false
end

local MAX_HUNTERS = 8

local function dispatch(player, root, count)
	local alive = 0
	for _, e in ipairs(Enemies.list()) do
		if e.HuntFor == player and e.Health > 0 and e.Model.Parent then
			alive = alive + 1
		end
	end
	count = math.min(count, MAX_HUNTERS - alive)
	for i = 1, count do
		local a = (i / count) * math.pi * 2 + math.random() * 0.5
		local pos = root.Position + Vector3.new(math.cos(a) * 75, 10, math.sin(a) * 75)
		Enemies.spawn(pos, "Hunter", { HuntFor = player })
	end
end

function HeatSystem.init(systems)
	Net, PlayerState, Enemies = systems.Net, systems.Players, systems.Enemies
end

function HeatSystem.start()
	Players.PlayerRemoving:Connect(function(player)
		heat[player] = nil
	end)
	local accum = 0
	RunService.Heartbeat:Connect(function(dt)
		accum = accum + dt
		if accum < 0.25 then
			return
		end
		local step = accum
		accum = 0
		local now = os.clock()
		for player, h in pairs(heat) do
			local character = player.Character
			local root = character and character:FindFirstChild("HumanoidRootPart")
			local humanoid = character and character:FindFirstChildOfClass("Humanoid")
			if not inRetroville(player) or not root or not humanoid or humanoid.Health <= 0 then
				-- Leaving the world or getting KO'd clears your threat.
				if h.Value > 0 then
					HeatSystem.clear(player)
				end
			else
				local near = huntersNear(player, root)
				local spawnCount = h:Tick(step, now, near)
				if spawnCount > 0 then
					dispatch(player, root, spawnCount)
				end
				player:SetAttribute("Heat", h.Value)
				player:SetAttribute("HeatCooling", h:Cooling(now, near))
			end
		end
	end)
end

return HeatSystem
