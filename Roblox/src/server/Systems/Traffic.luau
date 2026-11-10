--!nonstrict
-- Ambient traffic: hover cars cruise Retroville's main roads, stop for
-- people in the way, and turn around at the end of the road. Any of them can
-- be hijacked: walk up and press E to drive it.

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local MapData = require(ReplicatedStorage:WaitForChild("Shared"):WaitForChild("MapData"))

local Traffic = {}
local World, Enemies
local cars = {}
local COUNT = 12
local SPEED = 38
local COLORS = {
	Color3.fromRGB(60, 140, 230), Color3.fromRGB(250, 200, 40), Color3.fromRGB(80, 200, 120), Color3.fromRGB(240, 240, 245),
	Color3.fromRGB(240, 120, 40), Color3.fromRGB(160, 90, 220), Color3.fromRGB(40, 40, 50),
}

local function laneFor(road, dirSign)
	-- Drive on the right: offset from the centre line by a quarter of the road width.
	local axis = road.Axis == "X" and Vector3.new(1, 0, 0) or Vector3.new(0, 0, 1)
	local right = Vector3.new(-axis.Z, 0, axis.X) * dirSign
	return axis * dirSign, road.C + right * road.W * 0.25
end

local function spawnCar(road, index)
	local dirSign = index % 2 == 0 and 1 or -1
	local dir, laneCenter = laneFor(road, dirSign)
	local along = (math.random() - 0.5) * road.L * 0.8
	local pos = laneCenter + dir * along
	local yaw = math.deg(math.atan2(-dir.X, -dir.Z))
	local model = World.buildHoverCar(pos, yaw, COLORS[(index % #COLORS) + 1])
	model:SetAttribute("Traffic", true)
	local body = model.PrimaryPart
	local car = {
		Model = model, Body = body, Seat = model:FindFirstChild("DriverSeat"), Road = road, Sign = dirSign,
		Velocity = body:FindFirstChild("DriveVelocity"), Orientation = body:FindFirstChild("DriveOrientation"), AI = true,
	}
	table.insert(cars, car)
	return car
end

local function blocked(car, dir)
	local pos = car.Body.Position
	for _, player in ipairs(Players:GetPlayers()) do
		local character = player.Character
		local root = character and character:FindFirstChild("HumanoidRootPart")
		if root then
			local rel = root.Position - pos
			local ahead = rel:Dot(dir)
			if ahead > 0 and ahead < 16 and (rel - dir * ahead).Magnitude < 5 then
				return true
			end
		end
	end
	return false
end

local function drive(car, dt)
	if car.Seat and car.Seat.Occupant then
		if car.AI and car.Velocity then
			car.Velocity.VectorVelocity = Vector3.zero -- hand it over at rest
		end
		car.AI = false -- hijacked: it's the driver's car now
		return
	end
	if not car.AI or not car.Velocity then
		return
	end
	local road = car.Road
	local dir, laneCenter = laneFor(road, car.Sign)
	local pos = car.Body.Position
	-- Turn around at the end of the road.
	local fromCenter = (pos - road.C):Dot(road.Axis == "X" and Vector3.new(1, 0, 0) or Vector3.new(0, 0, 1)) * car.Sign
	if fromCenter > road.L * 0.47 then
		car.Sign = -car.Sign
		dir, laneCenter = laneFor(road, car.Sign)
	end
	-- Hold the lane and a hover height.
	local toLane = laneCenter - pos
	local lateral = toLane - dir * toLane:Dot(dir)
	local params = RaycastParams.new()
	-- Only the map counts as ground (not people standing on the roof, not other cars).
	params.FilterType = Enum.RaycastFilterType.Include
	params.FilterDescendantsInstances = { workspace:FindFirstChild("Retroville") }
	local hit = workspace:Raycast(pos + Vector3.new(0, 4, 0), Vector3.new(0, -60, 0), params)
	local groundY = math.min(hit and hit.Position.Y or road.C.Y, road.C.Y + 2)
	local vy = math.clamp((groundY + 4 - pos.Y) * 3, -30, 30)
	local speed = blocked(car, dir) and 0 or SPEED
	pcall(function()
		car.Body:SetNetworkOwner(nil)
	end)
	car.Velocity.MaxForce = 2e5
	car.Velocity.VectorVelocity = dir * speed + Vector3.new(lateral.X, 0, lateral.Z) * 2 + Vector3.new(0, vy, 0)
	car.Orientation.CFrame = CFrame.lookAt(Vector3.zero, dir)
end

function Traffic.init(systems)
	World, Enemies = systems.World, systems.Enemies
end

function Traffic.start()
	local roads = MapData.Roads or {}
	if #roads == 0 then
		return
	end
	table.sort(roads, function(a, b)
		return a.L > b.L
	end)
	task.wait(3)
	for i = 1, COUNT do
		spawnCar(roads[((i - 1) % math.min(#roads, 8)) + 1], i)
	end
	local accum = 0
	RunService.Heartbeat:Connect(function(dt)
		accum = accum + dt
		if accum < 0.1 then
			return
		end
		accum = 0
		for i = #cars, 1, -1 do
			local car = cars[i]
			if not car.Model.Parent or not car.Body.Parent then
				table.remove(cars, i)
			else
				drive(car, 0.1)
			end
		end
	end)
	-- Keep the streets busy: replace cars that were taken and abandoned far away.
	task.spawn(function()
		while true do
			task.wait(60)
			local ai = 0
			for _, car in ipairs(cars) do
				if car.AI then
					ai = ai + 1
				end
			end
			for i = ai + 1, COUNT do
				spawnCar(roads[math.random(1, math.min(#roads, 8))], i)
			end
			-- Clean up abandoned cars nobody is near.
			for i = #cars, 1, -1 do
				local car = cars[i]
				if not car.AI and not (car.Seat and car.Seat.Occupant) then
					local near = false
					for _, player in ipairs(Players:GetPlayers()) do
						local character = player.Character
						if character and (character:GetPivot().Position - car.Body.Position).Magnitude < 250 then
							near = true
						end
					end
					if not near then
						car.Model:Destroy()
						table.remove(cars, i)
					end
				end
			end
		end
	end)
end

return Traffic
