--!nonstrict
-- Client entry point: two-phase startup of every controller (init wires
-- them together, start begins loops and connections).

local controllers = {}
local order = { "Input", "Hud", "CameraRig", "Movement", "Animator", "Effects", "Companions", "Vehicle" }
for _, name in ipairs(order) do
	local module = script.Controllers:FindFirstChild(name)
	if module then
		controllers[name] = require(module)
	else
		warn("[MultiverseToons] missing controller " .. name)
	end
end
for _, name in ipairs(order) do
	local c = controllers[name]
	if c and type(c.init) == "function" then
		c.init(controllers)
	end
end
for _, name in ipairs(order) do
	local c = controllers[name]
	if c and type(c.start) == "function" then
		task.spawn(c.start)
	end
end
