--!nonstrict
-- Server entry point: requires every system, then runs init (wiring, remotes)
-- for all of them before any start (loops, connections that need others).

local systems = {}
local order = {
	"Net", "Players", "Profiles", "Heroes", "World", "Progression", "Heat", "Enemies", "Combat", "Boss",
	"Collectibles", "Courses", "Missions", "Traffic",
}
for _, name in ipairs(order) do
	local module = script:WaitForChild("Systems"):FindFirstChild(name)
	if module then
		systems[name] = require(module)
	else
		warn("[MultiverseToons] missing system " .. name)
	end
end
for _, name in ipairs(order) do
	local system = systems[name]
	if system and type(system.init) == "function" then
		system.init(systems)
	end
end
for _, name in ipairs(order) do
	local system = systems[name]
	if system and type(system.start) == "function" then
		task.spawn(system.start)
	end
end
print("[MultiverseToons] server ready")
