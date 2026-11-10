--!nonstrict
-- Freeflow combat helpers on the client:
--  * soft targeting: the enemy nearest the crosshair is the attack target, and
--    melee attacks lunge to it (Movement reads CombatAssist.target());
--  * hard lock-on (Z / middle mouse / R3): the camera tracks the target;
--  * counter prompts: when an enemy winds up an attack on you, a "!" appears
--    over it and a COUNTER prompt flashes; press Counter (right click / X / R1)
--    in time to reverse it, or just to block;
--  * takedowns: at a high enough combo, T performs a special takedown.

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local Debris = game:GetService("Debris")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))

local CombatAssist = {}
local player = Players.LocalPlayer
local Input, CameraRig, Movement, Hud
local current = nil   -- soft or hard target model
local locked = nil    -- hard-locked model
local reticle
local threats = {}    -- [model] = expiry time (enemies winding up on me)

local function alive(model)
	if not model or not model.Parent or not model.PrimaryPart then
		return false
	end
	local h = model:GetAttribute("Health")
	return h == nil or h > 0
end

local function enemies()
	local out = {}
	local folder = workspace:FindFirstChild("Enemies")
	if folder then
		for _, m in ipairs(folder:GetChildren()) do
			if m:IsA("Model") and m:GetAttribute("Enemy") and alive(m) then
				table.insert(out, m)
			end
		end
	end
	return out
end

local function pick()
	local character = player.Character
	local root = character and character:FindFirstChild("HumanoidRootPart")
	if not root then
		return nil
	end
	local look = CameraRig.lookVector()
	local best, bestScore = nil, math.huge
	for _, m in ipairs(enemies()) do
		local to = m.PrimaryPart.Position - root.Position
		local d = to.Magnitude
		if d < Config.Combat.LockRange then
			local flat = Vector3.new(to.X, 0, to.Z)
			local lookFlat = Vector3.new(look.X, 0, look.Z)
			local angle = (flat.Magnitude > 0.1 and lookFlat.Magnitude > 0.1) and math.acos(math.clamp(flat.Unit:Dot(lookFlat.Unit), -1, 1)) or 0
			-- Prefer what you're looking at, then what's close.
			local score = angle * 40 + d
			if angle < math.rad(70) and score < bestScore then
				best, bestScore = m, score
			end
		end
	end
	return best
end

function CombatAssist.target()
	if alive(locked) then
		return locked
	end
	if alive(current) then
		return current
	end
	return nil
end

function CombatAssist.isLocked()
	return alive(locked)
end

local function ensureReticle()
	if reticle then
		return reticle
	end
	reticle = Instance.new("BillboardGui")
	reticle.Name = "TargetReticle"
	reticle.Size = UDim2.fromOffset(46, 46)
	reticle.AlwaysOnTop = true
	local ring = Instance.new("Frame")
	ring.Size = UDim2.fromScale(1, 1)
	ring.BackgroundTransparency = 1
	local stroke = Instance.new("UIStroke")
	stroke.Thickness = 3
	stroke.Color = Color3.fromRGB(255, 230, 120)
	stroke.Parent = ring
	local corner = Instance.new("UICorner")
	corner.CornerRadius = UDim.new(0.5, 0)
	corner.Parent = ring
	ring.Parent = reticle
	reticle.Parent = player:WaitForChild("PlayerGui")
	return reticle
end

-- "!" over an enemy that's about to strike.
local function showThreat(model, time, mine, label)
	if not model or not model.PrimaryPart then
		return
	end
	local gui = Instance.new("BillboardGui")
	gui.Size = UDim2.fromOffset(60, 60)
	gui.StudsOffset = Vector3.new(0, model.PrimaryPart.Size.Y * 0.7 + 2, 0)
	gui.AlwaysOnTop = true
	gui.Adornee = model.PrimaryPart
	local text = Instance.new("TextLabel")
	text.BackgroundTransparency = 1
	text.Size = UDim2.fromScale(1, 1)
	text.Font = Enum.Font.FredokaOne
	text.TextScaled = true
	text.Text = label and ("! " .. label) or "!"
	text.TextColor3 = mine and Color3.fromRGB(255, 70, 70) or Color3.fromRGB(255, 200, 80)
	text.TextStrokeTransparency = 0
	text.Parent = gui
	gui.Parent = player:WaitForChild("PlayerGui")
	Debris:AddItem(gui, time + 0.1)
	local h = Instance.new("Highlight")
	h.FillColor = Color3.fromRGB(255, 60, 60)
	h.FillTransparency = 0.55
	h.OutlineColor = Color3.fromRGB(255, 220, 220)
	h.Parent = model
	Debris:AddItem(h, time)
	if mine then
		threats[model] = os.clock() + time
	end
end

function CombatAssist.onTelegraph(fx)
	local character = player.Character
	local root = character and character:FindFirstChild("HumanoidRootPart")
	local model = fx.Model
	local mine = fx.Target == player
	if not mine and root and model and model.PrimaryPart and (model.PrimaryPart.Position - root.Position).Magnitude < Config.Combat.CounterRange then
		mine = true
	end
	showThreat(model, fx.Time or 0.6, mine, fx.Label)
end

function CombatAssist.threatened()
	local now = os.clock()
	for m, t in pairs(threats) do
		if t < now or not m.Parent then
			threats[m] = nil
		end
	end
	return next(threats) ~= nil
end

function CombatAssist.takedownReady()
	local combo = player:GetAttribute("Combo") or 0
	local at = player:GetAttribute("ComboAt") or 0
	return combo >= Config.Combat.TakedownCombo and workspace:GetServerTimeNow() - at < Config.Combat.ComboTimeout
end

function CombatAssist.init(controllers)
	Input, CameraRig, Movement, Hud = controllers.Input, controllers.CameraRig, controllers.Movement, controllers.Hud
	Input.on("LockOn", function(down)
		if not down then
			return
		end
		if alive(locked) then
			locked = nil
		else
			locked = pick()
		end
	end)
	Input.on("Counter", function(down)
		if not down then
			return
		end
		local S = Movement.state()
		if CombatAssist.threatened() then
			S.CounterAnim = 0.3
			threats = {}
		else
			S.BlockAnim = Config.Combat.BlockTime
		end
		ReplicatedStorage.Remotes.Counter:FireServer()
	end)
	Input.on("Takedown", function(down)
		if down and CombatAssist.takedownReady() then
			Movement.state().TakedownAnim = 0.6
			ReplicatedStorage.Remotes.Takedown:FireServer()
		end
	end)
end

function CombatAssist.start()
	RunService.RenderStepped:Connect(function(dt)
		if locked and not alive(locked) then
			locked = nil
		end
		current = alive(locked) and locked or pick()
		local r = ensureReticle()
		r.Adornee = current and current.PrimaryPart or nil
		r.Enabled = current ~= nil
		local stroke = r:FindFirstChildOfClass("Frame"):FindFirstChildOfClass("UIStroke")
		stroke.Color = alive(locked) and Color3.fromRGB(255, 80, 80) or Color3.fromRGB(255, 230, 120)
		stroke.Transparency = alive(locked) and 0 or 0.4
		-- Hard lock: steer the camera toward the target.
		if alive(locked) then
			local character = player.Character
			local root = character and character:FindFirstChild("HumanoidRootPart")
			if root then
				local to = locked.PrimaryPart.Position - root.Position
				local want = math.deg(math.atan2(-to.X, -to.Z))
				local yaw = CameraRig.yaw()
				local d = (want - yaw + 540) % 360 - 180
				CameraRig.setYaw(yaw + d * math.min(1, dt * 8))
			end
		end
		if Hud then
			Hud.setPrompts(CombatAssist.threatened(), CombatAssist.takedownReady())
		end
	end)
end

return CombatAssist
