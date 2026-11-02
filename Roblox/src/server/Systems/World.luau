--!nonstrict
-- The two worlds baked into Workspace by tools/build_place.py (Nexus
-- Observatory hub and Retroville), described by Shared/MapData: spawning,
-- dimensional terminals (swap hero / travel), recall, and Jimmy's hover cars.

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))
local MapData = require(Shared:WaitForChild("MapData"))

local World = {}
local Net, PlayerState
local terminals = {} -- { Name, Pos, Level, Universe, Hero, Travel }

function World.level(name)
	return MapData.Levels[name]
end

function World.startCFrame(levelName)
	local level = MapData.Levels[levelName] or MapData.Levels[Config.Levels.Nexus]
	return CFrame.new(level.Start + Vector3.new(0, 4, 0)) * CFrame.Angles(0, math.rad(level.StartYaw), 0)
end

function World.levelOf(player)
	local st = PlayerState.get(player)
	return st and st.Level or Config.Levels.Nexus
end

function World.nearTerminal(player, wantTravel)
	local character = player.Character
	if not character then
		return nil
	end
	local pos = character:GetPivot().Position
	for _, t in ipairs(terminals) do
		if (t.Pos - pos).Magnitude < 26 and (not wantTravel or t.Travel == wantTravel) then
			return t
		end
	end
	return nil
end

function World.travel(player, levelName)
	local st = PlayerState.get(player)
	local character = player.Character
	if not st or not character or not MapData.Levels[levelName] then
		return
	end
	-- Leave any vehicle first.
	local humanoid = character:FindFirstChildOfClass("Humanoid")
	if humanoid and humanoid.SeatPart then
		humanoid.Sit = false
		task.wait()
	end
	st.Level = levelName
	player:SetAttribute("Level", levelName)
	character:PivotTo(World.startCFrame(levelName))
	local root = character:FindFirstChild("HumanoidRootPart")
	if root then
		root.AssemblyLinearVelocity = Vector3.zero
	end
	Net.fire("Notify", player, { Banner = MapData.Levels[levelName].Name, Text = MapData.Levels[levelName].Blurb })
end

local function buildTerminal(t)
	local anchor = Instance.new("Part")
	anchor.Name = "TerminalPrompt_" .. t.Name
	anchor.Anchored = true
	anchor.CanCollide = false
	anchor.CanQuery = false
	anchor.Transparency = 1
	anchor.Size = Vector3.new(4, 4, 4)
	anchor.CFrame = CFrame.new(t.Pos + Vector3.new(0, 4, 0))
	anchor.Parent = workspace:FindFirstChild("Interactables") or workspace
	local prompt = Instance.new("ProximityPrompt")
	prompt.ActionText = t.Travel and "Swap hero / Travel" or "Swap hero"
	prompt.ObjectText = t.Universe .. " terminal"
	prompt.KeyboardKeyCode = Enum.KeyCode.E
	prompt.HoldDuration = 0
	prompt.MaxActivationDistance = 16
	prompt.RequiresLineOfSight = false
	prompt.Parent = anchor
	prompt.Triggered:Connect(function(player)
		Net.fire("OpenTerminal", player, { Name = t.Name, Universe = t.Universe, Hero = t.Hero, Travel = t.Travel })
	end)
	-- Floating label.
	local gui = Instance.new("BillboardGui")
	gui.Size = UDim2.fromOffset(220, 50)
	gui.StudsOffset = Vector3.new(0, 6, 0)
	gui.MaxDistance = 220
	gui.AlwaysOnTop = false
	gui.Parent = anchor
	local label = Instance.new("TextLabel")
	label.BackgroundTransparency = 1
	label.Size = UDim2.fromScale(1, 1)
	label.Font = Enum.Font.FredokaOne
	label.TextScaled = true
	label.TextColor3 = Color3.new(1, 1, 1)
	label.TextStrokeTransparency = 0.2
	label.Text = t.Universe
	label.Parent = gui
end

-- Jimmy's retro hover convertible: a seat on a hover body the driver's client flies.
local function buildHoverCar(pos, yaw)
	local model = Instance.new("Model")
	model.Name = "HoverCar"
	local base = CFrame.new(pos + Vector3.new(0, 3, 0)) * CFrame.Angles(0, math.rad(yaw), 0)
	local function part(name, size, offset, color, material, shape)
		local p = Instance.new("Part")
		p.Name = name
		p.Size = size
		p.CFrame = base * offset
		p.Color = color
		p.Material = material or Enum.Material.SmoothPlastic
		p.TopSurface = Enum.SurfaceType.Smooth
		p.BottomSurface = Enum.SurfaceType.Smooth
		if shape then
			p.Shape = shape
		end
		p.Parent = model
		return p
	end
	local body = part("Body", Vector3.new(6.5, 1.6, 12), CFrame.new(), Color3.fromRGB(210, 40, 40))
	part("Fin", Vector3.new(6.7, 0.5, 3), CFrame.new(0, 0.9, 4.5), Color3.fromRGB(240, 240, 245))
	part("Windshield", Vector3.new(5.6, 1.4, 0.2), CFrame.new(0, 1.5, -2) * CFrame.Angles(math.rad(-20), 0, 0), Color3.fromRGB(150, 220, 255), Enum.Material.Glass).Transparency = 0.4
	part("Grille", Vector3.new(5, 0.8, 0.3), CFrame.new(0, -0.1, -6.1), Color3.fromRGB(220, 220, 230), Enum.Material.Metal)
	for _, x in ipairs({ -2.2, 2.2 }) do
		part("Headlight", Vector3.new(1, 1, 0.3), CFrame.new(x, 0.2, -6.15), Color3.fromRGB(255, 250, 210), Enum.Material.Neon)
		for _, z in ipairs({ -4, 4 }) do
			part("HoverPad", Vector3.new(0.5, 2.4, 2.4), CFrame.new(x, -1.1, z) * CFrame.Angles(0, 0, math.pi / 2), Color3.fromRGB(110, 230, 255), Enum.Material.Neon, Enum.PartType.Cylinder)
		end
	end
	local seat = Instance.new("Seat")
	seat.Name = "DriverSeat"
	seat.Size = Vector3.new(2.4, 0.6, 2.4)
	seat.CFrame = base * CFrame.new(0, 1.1, 0.5)
	seat.Color = Color3.fromRGB(240, 240, 245)
	seat.Parent = model
	local prompt = Instance.new("ProximityPrompt")
	prompt.ActionText = "Drive"
	prompt.ObjectText = "Hover car"
	prompt.KeyboardKeyCode = Enum.KeyCode.E
	prompt.HoldDuration = 0
	prompt.MaxActivationDistance = 14
	prompt.RequiresLineOfSight = false
	prompt.Parent = seat
	prompt.Triggered:Connect(function(player)
		local character = player.Character
		local humanoid = character and character:FindFirstChildOfClass("Humanoid")
		if humanoid and not seat.Occupant and humanoid.Health > 0 then
			seat:Sit(humanoid)
		end
	end)
	for _, p in ipairs(model:GetChildren()) do
		if p ~= body then
			local w = Instance.new("WeldConstraint")
			w.Part0 = body
			w.Part1 = p
			w.Parent = p
		end
		p.CustomPhysicalProperties = PhysicalProperties.new(0.3, 0.3, 0.2)
	end
	model.PrimaryPart = body
	-- The driver's client controls these (Controllers/Vehicle).
	local att = Instance.new("Attachment")
	att.Name = "Drive"
	att.Parent = body
	local lv = Instance.new("LinearVelocity")
	lv.Name = "DriveVelocity"
	lv.Attachment0 = att
	lv.MaxForce = 0
	lv.VectorVelocity = Vector3.zero
	lv.Parent = body
	local ao = Instance.new("AlignOrientation")
	ao.Name = "DriveOrientation"
	ao.Mode = Enum.OrientationAlignmentMode.OneAttachment
	ao.Attachment0 = att
	ao.MaxTorque = 1e7
	ao.Responsiveness = 18
	ao.CFrame = base.Rotation
	ao.Parent = body
	model.Parent = workspace:FindFirstChild("Vehicles") or workspace
	seat:GetPropertyChangedSignal("Occupant"):Connect(function()
		local occupant = seat.Occupant
		local player = occupant and Players:GetPlayerFromCharacter(occupant.Parent)
		if player then
			body:SetNetworkOwner(player)
			lv.MaxForce = 2e5
		else
			lv.MaxForce = 0
			lv.VectorVelocity = Vector3.zero
			pcall(function()
				body:SetNetworkOwnershipAuto()
			end)
		end
	end)
end

function World.init(systems)
	Net, PlayerState = systems.Net, systems.Players
	for _, name in ipairs({ "Interactables", "Vehicles" }) do
		if not workspace:FindFirstChild(name) then
			local f = Instance.new("Folder")
			f.Name = name
			f.Parent = workspace
		end
	end
	for levelName, level in pairs(MapData.Levels) do
		for _, t in ipairs(level.Terminals) do
			table.insert(terminals, { Name = t.Name, Pos = t.Pos, Level = levelName, Universe = t.Universe, Hero = t.Hero, Travel = t.Travel })
		end
	end
	Net.on("Travel", 2, function(player, target)
		if type(target) ~= "string" or not MapData.Levels[target] then
			return
		end
		-- Recall to the hub works anywhere; other worlds need their terminal.
		if target ~= Config.Levels.Nexus and not World.nearTerminal(player, target) then
			return
		end
		World.travel(player, target)
	end)
end

function World.start()
	for _, t in ipairs(terminals) do
		buildTerminal(t)
	end
	for _, level in pairs(MapData.Levels) do
		for _, car in ipairs(level.HoverCars) do
			buildHoverCar(car.Pos, car.Yaw)
		end
	end
	local function hook(player)
		player:SetAttribute("Level", Config.Levels.Nexus)
		player.CharacterAdded:Connect(function(character)
			character:WaitForChild("HumanoidRootPart", 10)
			task.wait(0.1)
			character:PivotTo(World.startCFrame(World.levelOf(player)))
		end)
	end
	Players.PlayerAdded:Connect(hook)
	for _, player in ipairs(Players:GetPlayers()) do
		hook(player)
	end
end

return World
