--!nonstrict
-- Third-person camera like the prototype's: orbit with mouse (cursor locked),
-- touch drag or right stick; over-the-shoulder offset; pulls back and widens
-- the field of view at speed; screen shake; never clips into walls.

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local UserInputService = game:GetService("UserInputService")

local CameraRig = {}
local player = Players.LocalPlayer
local camera
local yaw, pitch = 0, -12
local dist = 14
local fov = 70
local shake = 0
local cursorFree = false
local touchId = nil
local Movement

function CameraRig.init(controllers)
	Movement = controllers.Movement
end

function CameraRig.shake(amount)
	shake = math.max(shake, amount)
end

-- Menus free the cursor; gameplay locks it to the centre for mouse-look.
function CameraRig.setCursorFree(free)
	cursorFree = free
end

function CameraRig.yaw()
	return yaw
end

function CameraRig.setYaw(deg)
	yaw = deg
end

function CameraRig.lookVector()
	return camera and camera.CFrame.LookVector or Vector3.new(0, 0, -1)
end

-- Point under the crosshair (for aiming), ignoring the local character.
function CameraRig.aimPoint()
	local cf = camera.CFrame
	local params = RaycastParams.new()
	params.FilterType = Enum.RaycastFilterType.Exclude
	params.FilterDescendantsInstances = { player.Character }
	local hit = workspace:Raycast(cf.Position, cf.LookVector * 1000, params)
	local point = hit and hit.Position or cf.Position + cf.LookVector * 1000
	-- Ignore things between the camera and the hero.
	local character = player.Character
	local root = character and character:FindFirstChild("HumanoidRootPart")
	if root and hit and (hit.Position - cf.Position).Magnitude < (root.Position - cf.Position).Magnitude + 2 then
		point = cf.Position + cf.LookVector * 1000
	end
	return point
end

function CameraRig.start()
	camera = workspace.CurrentCamera
	camera.CameraType = Enum.CameraType.Scriptable

	UserInputService.InputChanged:Connect(function(input, processed)
		if input.UserInputType == Enum.UserInputType.MouseMovement and not cursorFree then
			yaw = yaw - input.Delta.X * 0.25
			pitch = math.clamp(pitch - input.Delta.Y * 0.25, -75, 65)
		elseif input.UserInputType == Enum.UserInputType.Touch and input == touchId then
			yaw = yaw - input.Delta.X * 0.35
			pitch = math.clamp(pitch - input.Delta.Y * 0.35, -75, 65)
		elseif input.KeyCode == Enum.KeyCode.Thumbstick2 then
			-- handled per frame below
		end
	end)
	UserInputService.TouchStarted:Connect(function(input, processed)
		-- Drags on the right half of the screen (not on buttons) orbit the camera.
		if not processed and not touchId and input.Position.X > camera.ViewportSize.X * 0.4 then
			touchId = input
		end
	end)
	UserInputService.TouchEnded:Connect(function(input)
		if input == touchId then
			touchId = nil
		end
	end)
	UserInputService.InputBegan:Connect(function(input)
		if input.KeyCode == Enum.KeyCode.Escape then
			cursorFree = not cursorFree
		end
	end)

	RunService:BindToRenderStep("MultiverseCamera", Enum.RenderPriority.Camera.Value, function(dt)
		camera.CameraType = Enum.CameraType.Scriptable
		UserInputService.MouseBehavior = cursorFree and Enum.MouseBehavior.Default or Enum.MouseBehavior.LockCenter
		UserInputService.MouseIconEnabled = cursorFree
		for _, pad in ipairs(UserInputService:GetConnectedGamepads()) do
			for _, s in ipairs(UserInputService:GetGamepadState(pad)) do
				if s.KeyCode == Enum.KeyCode.Thumbstick2 and s.Position.Magnitude > 0.15 then
					yaw = yaw - s.Position.X * 180 * dt
					pitch = math.clamp(pitch + s.Position.Y * 120 * dt, -75, 65)
				end
			end
		end

		local character = player.Character
		local root = character and character:FindFirstChild("HumanoidRootPart")
		local humanoid = character and character:FindFirstChildOfClass("Humanoid")
		if not root or not humanoid then
			return
		end
		local seat = humanoid.SeatPart
		local scale = math.max(0.6, (humanoid.HipHeight + root.Size.Y * 0.5) / 3)
		local focusPart = seat and seat.Parent and seat.Parent.PrimaryPart or root
		local focus = focusPart.Position + Vector3.new(0, 1.6 * scale + (seat and 3 or 0), 0)
		local velocity = focusPart.AssemblyLinearVelocity
		local speed = velocity.Magnitude
		local state = Movement and Movement.state() or {}
		local fast = state.Flying or state.Gliding or seat ~= nil or character:GetAttribute("Form") == "XLR8"
		local want = 14 * math.pow(scale, 0.85)
		if seat then
			want = 30
		elseif fast then
			want = want + 4 + 5 * math.clamp(speed / 100, 0, 1)
		end
		dist = dist + (want - dist) * math.min(1, dt * 5)
		local fovWant = 70 + (fast and 18 * math.clamp((speed - 43) / 73, 0, 1) or 0) + (state.BoostFx or 0) * 25
		fov = fov + (fovWant - fov) * math.min(1, dt * 4)
		camera.FieldOfView = fov

		local rot = CFrame.Angles(0, math.rad(yaw), 0) * CFrame.Angles(math.rad(pitch), 0, 0)
		local shoulder = seat and 0 or 1.5 * math.sqrt(scale)
		local wantPos = focus + rot:VectorToWorldSpace(Vector3.new(shoulder, 0, dist))
		-- Keep out of walls.
		local params = RaycastParams.new()
		params.FilterType = Enum.RaycastFilterType.Exclude
		local ignore = { character, workspace:FindFirstChild("Enemies"), workspace:FindFirstChild("Shards") }
		if seat and seat.Parent then
			table.insert(ignore, seat.Parent)
		end
		params.FilterDescendantsInstances = ignore
		local offset = wantPos - focus
		if not state.Intangible then
			local hit = workspace:Raycast(focus, offset, params)
			if hit then
				wantPos = focus + offset.Unit * math.max(1, hit.Distance - 0.8)
			end
		end
		shake = math.max(0, shake - dt * 2.5)
		if shake > 0 then
			local a = shake * shake * 0.7
			wantPos = wantPos + Vector3.new((math.random() - 0.5) * a, (math.random() - 0.5) * a, (math.random() - 0.5) * a)
		end
		camera.CFrame = CFrame.new(wantPos) * rot
		camera.Focus = CFrame.new(focus)
	end)
end

return CameraRig
