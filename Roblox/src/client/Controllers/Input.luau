--!nonstrict
-- Every game action and its bindings (keyboard, mouse, gamepad, and on-screen
-- touch buttons via ContextActionService). Controllers ask Input.held(name)
-- or subscribe with Input.on(name, fn(isDown)).

local ContextActionService = game:GetService("ContextActionService")
local UserInputService = game:GetService("UserInputService")
local Players = game:GetService("Players")

local Input = {}
local held = {}
local listeners = {}
local enabled = true

-- name = { keys..., touch title or nil }
local ACTIONS = {
	Attack = { { Enum.UserInputType.MouseButton1, Enum.KeyCode.ButtonR2 }, "Attack" },
	Sig = { { Enum.KeyCode.Q, Enum.KeyCode.ButtonX }, "Ability" },
	Sig2 = { { Enum.KeyCode.R, Enum.KeyCode.ButtonL1 }, "Ability 2" },
	Ult = { { Enum.KeyCode.F, Enum.KeyCode.ButtonY }, "Ultimate" },
	Fly = { { Enum.KeyCode.Tab, Enum.KeyCode.DPadUp }, "Fly" },
	Dodge = { { Enum.KeyCode.V, Enum.KeyCode.LeftAlt, Enum.KeyCode.ButtonB }, "Dodge" },
	Emote = { { Enum.KeyCode.B, Enum.KeyCode.DPadRight }, nil },
	Down = { { Enum.KeyCode.LeftControl, Enum.KeyCode.C, Enum.KeyCode.ButtonL2 }, "Down" },
	Boost = { { Enum.KeyCode.LeftShift, Enum.KeyCode.ButtonL3 }, nil },
	Omni1 = { { Enum.KeyCode.One }, nil },
	Omni2 = { { Enum.KeyCode.Two }, nil },
	Omni3 = { { Enum.KeyCode.Three }, nil },
	Glitch = { { Enum.KeyCode.G }, nil },
	Recall = { { Enum.KeyCode.P, Enum.KeyCode.DPadDown }, nil },
	Help = { { Enum.KeyCode.F1 }, nil },
}

local function emit(name, down)
	local list = listeners[name]
	if list then
		for _, fn in ipairs(list) do
			task.spawn(fn, down)
		end
	end
end

local function handler(name, state)
	if state == Enum.UserInputState.Begin then
		if not enabled then
			return Enum.ContextActionResult.Pass
		end
		held[name] = true
		emit(name, true)
	elseif state == Enum.UserInputState.End or state == Enum.UserInputState.Cancel then
		if held[name] then
			held[name] = false
			emit(name, false)
		end
	end
	-- Mouse clicks still reach GUI buttons; keys are ours.
	return Enum.ContextActionResult.Pass
end

function Input.init()
	for name, spec in pairs(ACTIONS) do
		local keys = spec[1]
		ContextActionService:BindAction("MT_" .. name, function(_, state)
			return handler(name, state)
		end, spec[2] ~= nil, table.unpack(keys))
		if spec[2] then
			ContextActionService:SetTitle("MT_" .. name, spec[2])
		end
	end
	-- Lay out the touch buttons in an arc above the jump button.
	if UserInputService.TouchEnabled then
		local spots = { Attack = { 0.62, 0.48 }, Sig = { 0.72, 0.36 }, Sig2 = { 0.84, 0.30 }, Ult = { 0.52, 0.62 }, Fly = { 0.52, 0.30 }, Dodge = { 0.62, 0.20 }, Down = { 0.40, 0.55 } }
		for name, p in pairs(spots) do
			ContextActionService:SetPosition("MT_" .. name, UDim2.new(p[1], 0, p[2], 0))
		end
	end
end

function Input.on(name, fn)
	listeners[name] = listeners[name] or {}
	table.insert(listeners[name], fn)
end

function Input.held(name)
	return held[name] == true
end

-- Jump is the humanoid's own control (Space / A / touch jump button).
function Input.jumpHeld()
	if UserInputService:IsKeyDown(Enum.KeyCode.Space) or UserInputService:IsGamepadButtonDown(Enum.UserInputType.Gamepad1, Enum.KeyCode.ButtonA) then
		return true
	end
	local character = Players.LocalPlayer.Character
	local humanoid = character and character:FindFirstChildOfClass("Humanoid")
	return humanoid ~= nil and humanoid.Jump
end

-- Menus switch gameplay actions off so their keys don't also act.
function Input.setEnabled(on)
	enabled = on
	if not on then
		for name in pairs(held) do
			held[name] = false
		end
	end
end

-- Movement stick / WASD in camera space: x = right, y = forward.
local controlModule
function Input.move()
	if not controlModule then
		local ok, module = pcall(function()
			return require(Players.LocalPlayer:WaitForChild("PlayerScripts"):WaitForChild("PlayerModule")):GetControls()
		end)
		if ok then
			controlModule = module
		end
	end
	if controlModule then
		local v = controlModule:GetMoveVector()
		return Vector2.new(v.X, -v.Z)
	end
	return Vector2.zero
end

return Input
