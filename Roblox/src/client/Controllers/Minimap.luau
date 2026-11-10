--!nonstrict
-- Rotating radar in the corner, open-world style: roads, and blips for
-- mission markers, rifts, terminals, shops, uncollected shards, enemies,
-- other players, cars, the boss, and your current objective waypoint (with an
-- arrow on the rim when it's off the map).

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))
local MapData = require(Shared:WaitForChild("MapData"))

local Minimap = {}
local player = Players.LocalPlayer
local CameraRig, Effects
local SIZE = 220
local RANGE = 260 -- studs from centre to rim
local canvas, mapLayer, blipLayer, arrow
local roadFrames = {}
local pool = {}
local used = 0
local waypoint = nil

local function frame(parent, props)
	local f = Instance.new("Frame")
	f.BorderSizePixel = 0
	for k, v in pairs(props) do
		f[k] = v
	end
	f.Parent = parent
	return f
end

local function build()
	local gui = Instance.new("ScreenGui")
	gui.Name = "Minimap"
	gui.ResetOnSpawn = false
	gui.IgnoreGuiInset = true
	gui.Parent = player:WaitForChild("PlayerGui")
	canvas = Instance.new("CanvasGroup")
	canvas.Name = "Radar"
	canvas.Size = UDim2.fromOffset(SIZE, SIZE)
	canvas.AnchorPoint = Vector2.new(1, 1)
	canvas.Position = UDim2.new(1, -16, 1, -16)
	canvas.BackgroundColor3 = Color3.fromRGB(30, 60, 40)
	canvas.BackgroundTransparency = 0.15
	canvas.Parent = gui
	local corner = Instance.new("UICorner")
	corner.CornerRadius = UDim.new(0.5, 0)
	corner.Parent = canvas
	mapLayer = frame(canvas, { Size = UDim2.fromScale(1, 1), BackgroundTransparency = 1 })
	blipLayer = frame(canvas, { Size = UDim2.fromScale(1, 1), BackgroundTransparency = 1, ZIndex = 3 })
	for _, road in ipairs(MapData.Roads or {}) do
		local f = frame(mapLayer, { BackgroundColor3 = Color3.fromRGB(70, 70, 82), AnchorPoint = Vector2.new(0.5, 0.5), ZIndex = 2 })
		table.insert(roadFrames, { Frame = f, Road = road })
	end
	-- Player arrow (always centre, pointing up = camera forward).
	local me = frame(canvas, { Size = UDim2.fromOffset(12, 12), AnchorPoint = Vector2.new(0.5, 0.5), Position = UDim2.fromScale(0.5, 0.5), BackgroundColor3 = Color3.new(1, 1, 1), ZIndex = 5, Rotation = 45 })
	local s = Instance.new("UIStroke")
	s.Thickness = 2
	s.Parent = me
	local ring = Instance.new("UIStroke")
	ring.Thickness = 3
	ring.Color = Color3.fromRGB(240, 240, 250)
	ring.Parent = canvas
	-- Rim arrow toward an off-map waypoint.
	arrow = Instance.new("TextLabel")
	arrow.BackgroundTransparency = 1
	arrow.Size = UDim2.fromOffset(22, 22)
	arrow.AnchorPoint = Vector2.new(0.5, 0.5)
	arrow.Font = Enum.Font.FredokaOne
	arrow.Text = "▲"
	arrow.TextScaled = true
	arrow.TextColor3 = Color3.fromRGB(255, 220, 60)
	arrow.TextStrokeTransparency = 0
	arrow.ZIndex = 6
	arrow.Parent = canvas
	local north = Instance.new("TextLabel")
	north.Name = "North"
	north.BackgroundTransparency = 1
	north.Size = UDim2.fromOffset(18, 18)
	north.AnchorPoint = Vector2.new(0.5, 0.5)
	north.Font = Enum.Font.FredokaOne
	north.Text = "N"
	north.TextScaled = true
	north.TextColor3 = Color3.new(1, 1, 1)
	north.TextStrokeTransparency = 0
	north.ZIndex = 6
	north.Parent = canvas
end

local function blip(color, size, round)
	used = used + 1
	local f = pool[used]
	if not f then
		f = frame(blipLayer, { AnchorPoint = Vector2.new(0.5, 0.5), ZIndex = 4 })
		local c = Instance.new("UICorner")
		c.CornerRadius = UDim.new(0.5, 0)
		c.Parent = f
		pool[used] = f
	end
	f.Visible = true
	f.BackgroundColor3 = color
	f.Size = UDim2.fromOffset(size, size)
	f.UICorner.CornerRadius = UDim.new(round and 0.5 or 0.15, 0)
	return f
end

-- World position -> radar offset in pixels (camera forward is up).
local function project(origin, yawRad, pos)
	local rel = pos - origin
	local c, s = math.cos(yawRad), math.sin(yawRad)
	-- Rotate into camera space: x right, y forward.
	local x = rel.X * c - rel.Z * s
	local z = rel.X * s + rel.Z * c
	local scale = (SIZE * 0.5) / RANGE
	return x * scale, z * scale
end

local function place(f, x, y, clampToRim)
	local r = SIZE * 0.5 - 8
	local d = math.sqrt(x * x + y * y)
	if d > r then
		if not clampToRim then
			f.Visible = false
			return false
		end
		x, y = x / d * r, y / d * r
	end
	f.Position = UDim2.new(0.5, x, 0.5, y)
	return true
end

function Minimap.setWaypoint(pos)
	waypoint = pos
end

local function update()
	local character = player.Character
	local root = character and character:FindFirstChild("HumanoidRootPart")
	if not root then
		return
	end
	local origin = root.Position
	local yawRad = math.rad(CameraRig.yaw())
	used = 0
	-- Roads (Retroville only).
	local inRetro = (player:GetAttribute("Level") or "") == Config.Levels.Retroville
	local scale = (SIZE * 0.5) / RANGE
	for _, r in ipairs(roadFrames) do
		if inRetro then
			local x, y = project(origin, yawRad, r.Road.C)
			local len, wid = r.Road.L * scale, math.max(3, r.Road.W * scale)
			r.Frame.Visible = true
			r.Frame.Size = r.Road.Axis == "X" and UDim2.fromOffset(len, wid) or UDim2.fromOffset(wid, len)
			r.Frame.Position = UDim2.new(0.5, x, 0.5, y)
			r.Frame.Rotation = math.deg(yawRad)
		else
			r.Frame.Visible = false
		end
	end
	local function add(pos, color, size, round, rim)
		local f = blip(color, size, round)
		local x, y = project(origin, yawRad, pos)
		place(f, x, y, rim)
	end
	local interact = workspace:FindFirstChild("Interactables")
	if interact then
		for _, p in ipairs(interact:GetChildren()) do
			if p:IsA("BasePart") then
				if p:GetAttribute("Mission") then
					add(p.Position, Color3.fromRGB(255, 210, 60), 12, false, true)
				elseif p.Name:find("Terminal") or p.Name:find("Service") then
					add(p.Position, Color3.fromRGB(120, 220, 255), 8, false, false)
				end
			end
		end
	end
	local enemies = workspace:FindFirstChild("Enemies")
	if enemies then
		for _, m in ipairs(enemies:GetChildren()) do
			if m:IsA("Model") and m.Name == "Rift" then
				local core = m:FindFirstChild("Core")
				if core then
					add(core.Position, Color3.fromRGB(190, 90, 255), 12, true, true)
				end
			elseif m:IsA("Model") and m.PrimaryPart then
				if m:GetAttribute("Enemy") == "Boss" then
					add(m.PrimaryPart.Position, Color3.fromRGB(255, 60, 200), 16, true, true)
				elseif m:GetAttribute("Enemy") then
					add(m.PrimaryPart.Position, Color3.fromRGB(255, 70, 70), 6, true, false)
				elseif m.Name == "Rift" then
					local core = m:FindFirstChild("Core")
					if core then
						add(core.Position, Color3.fromRGB(190, 90, 255), 12, true, true)
					end
				end
			end
		end
	end
	local shards = workspace:FindFirstChild("Shards")
	if shards then
		for _, m in ipairs(shards:GetChildren()) do
			if m:IsA("Model") and m.PrimaryPart then
				add(m.PrimaryPart.Position, Color3.fromRGB(200, 160, 255), 5, true, false)
			end
		end
	end
	local vehicles = workspace:FindFirstChild("Vehicles")
	if vehicles then
		for _, m in ipairs(vehicles:GetChildren()) do
			if m.PrimaryPart then
				add(m.PrimaryPart.Position, Color3.fromRGB(235, 235, 240), 6, false, false)
			end
		end
	end
	for _, other in ipairs(Players:GetPlayers()) do
		local r = other ~= player and other.Character and other.Character:FindFirstChild("HumanoidRootPart")
		if r then
			add(r.Position, Color3.fromRGB(80, 160, 255), 8, true, true)
		end
	end
	-- Objective waypoint: blip, or an arrow on the rim.
	arrow.Visible = false
	if waypoint then
		local x, y = project(origin, yawRad, waypoint)
		local d = math.sqrt(x * x + y * y)
		local r = SIZE * 0.5 - 10
		if d > r then
			arrow.Visible = true
			arrow.Position = UDim2.new(0.5, x / d * r, 0.5, y / d * r)
			arrow.Rotation = math.deg(math.atan2(x, -y))
		else
			add(waypoint, Color3.fromRGB(120, 255, 120), 12, true, true)
		end
	end
	-- North marker on the rim (world -Z is north).
	local nx, ny = project(origin, yawRad, origin + Vector3.new(0, 0, -RANGE * 2))
	local nd = math.sqrt(nx * nx + ny * ny)
	canvas.North.Position = UDim2.new(0.5, nx / nd * (SIZE * 0.5 - 12), 0.5, ny / nd * (SIZE * 0.5 - 12))
	for i = used + 1, #pool do
		pool[i].Visible = false
	end
end

function Minimap.init(controllers)
	CameraRig, Effects = controllers.CameraRig, controllers.Effects
end

function Minimap.start()
	build()
	local accum = 0
	RunService.RenderStepped:Connect(function(dt)
		accum = accum + dt
		if accum < 1 / 30 then
			return
		end
		accum = 0
		update()
	end)
end

return Minimap
