--!nonstrict
-- Jimmy's hover car (anyone can drive it): sit in the seat (E), then
-- W/S throttle, A/D steer, Space/Ctrl altitude, Shift boost, E to hop out.
-- Same feel as the prototype's hover car: hovers at a cruise height over the
-- ground, banks into turns, noses down when accelerating.

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local UserInputService = game:GetService("UserInputService")
local ContextActionService = game:GetService("ContextActionService")

local Vehicle = {}
local player = Players.LocalPlayer
local Input
local car -- { Model, Body, Velocity, Orientation, Yaw, Speed, YawRate, Altitude }

local function sitting()
	local character = player.Character
	local humanoid = character and character:FindFirstChildOfClass("Humanoid")
	local seat = humanoid and humanoid.SeatPart
	if seat and seat.Name == "DriverSeat" and seat.Parent and seat.Parent.PrimaryPart and seat.Parent.PrimaryPart:FindFirstChild("DriveVelocity") then
		return seat.Parent, humanoid
	end
	return nil
end

local function bindDriving(on)
	if on then
		-- Space and E belong to the car while driving (not jump / prompts).
		ContextActionService:BindActionAtPriority("MT_DriveSink", function(_, state, input)
			if input.KeyCode == Enum.KeyCode.E and state == Enum.UserInputState.Begin then
				local _, humanoid = sitting()
				if humanoid then
					humanoid.Sit = false
					humanoid.Jump = true
				end
			end
			return Enum.ContextActionResult.Sink
		end, false, 3000, Enum.KeyCode.Space, Enum.KeyCode.E)
	else
		ContextActionService:UnbindAction("MT_DriveSink")
	end
end

function Vehicle.init(controllers)
	Input = controllers.Input
end

function Vehicle.start()
	RunService.Heartbeat:Connect(function(dt)
		local model, humanoid = sitting()
		if not model then
			if car then
				bindDriving(false)
				car = nil
			end
			return
		end
		local body = model.PrimaryPart
		if not car or car.Model ~= model then
			local _, yaw = body.CFrame:ToEulerAnglesYXZ()
			car = {
				Model = model, Body = body, Velocity = body:FindFirstChild("DriveVelocity"), Orientation = body:FindFirstChild("DriveOrientation"),
				Yaw = math.deg(yaw), Speed = 0, YawRate = 0, Altitude = 8,
			}
			bindDriving(true)
		end
		local throttle = (UserInputService:IsKeyDown(Enum.KeyCode.W) and 1 or 0) - (UserInputService:IsKeyDown(Enum.KeyCode.S) and 1 or 0)
		local steer = (UserInputService:IsKeyDown(Enum.KeyCode.D) and 1 or 0) - (UserInputService:IsKeyDown(Enum.KeyCode.A) and 1 or 0)
		local mv = Input.move()
		if throttle == 0 then
			throttle = mv.Y
		end
		if steer == 0 then
			steer = mv.X
		end
		local boost = Input.held("Boost")
		local up = (UserInputService:IsKeyDown(Enum.KeyCode.Space) and 1 or 0) - ((Input.held("Down")) and 1 or 0)
		car.Altitude = math.clamp(car.Altitude + up * 14 * dt, 4, 60)
		local top = boost and 140 or 85
		local target = throttle * top
		local accel = throttle ~= 0 and 45 or 25
		car.Speed = car.Speed + math.clamp(target - car.Speed, -accel * dt, accel * dt)
		local turnRate = 110 * math.clamp(1 - math.abs(car.Speed) / 220, 0.35, 1)
		car.YawRate = car.YawRate + (-steer * turnRate - car.YawRate) * math.min(1, dt * 6)
		car.Yaw = car.Yaw + car.YawRate * dt
		-- Hover: hold the cruise height above whatever is below.
		local params = RaycastParams.new()
		params.FilterType = Enum.RaycastFilterType.Exclude
		params.FilterDescendantsInstances = { model, player.Character }
		local hit = workspace:Raycast(body.Position, Vector3.new(0, -200, 0), params)
		local groundY = hit and hit.Position.Y or body.Position.Y - car.Altitude
		local vy = math.clamp((groundY + car.Altitude - body.Position.Y) * 3, -40, 40)
		local forward = CFrame.Angles(0, math.rad(car.Yaw), 0).LookVector
		if car.Velocity then
			car.Velocity.VectorVelocity = forward * car.Speed + Vector3.new(0, vy, 0)
		end
		if car.Orientation then
			local bank = math.clamp(car.YawRate * 0.25, -30, 30)
			local pitch = math.clamp(-(target - car.Speed) * 0.08, -10, 10)
			car.Orientation.CFrame = CFrame.Angles(0, math.rad(car.Yaw), 0) * CFrame.Angles(math.rad(pitch), 0, math.rad(bank))
		end
	end)
end

return Vehicle
