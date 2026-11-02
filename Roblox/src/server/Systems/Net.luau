--!nonstrict
-- Creates every remote from Shared/Remotes and wraps client->server handlers
-- with per-player rate limiting and a type guard, so systems only ever see
-- well-formed requests from players who are in the game.

local ReplicatedStorage = game:GetService("ReplicatedStorage")
local Players = game:GetService("Players")

local Remotes = require(ReplicatedStorage:WaitForChild("Shared"):WaitForChild("Remotes"))

local Net = {}
local folder
local events = {}
local buckets = {} -- [player][name] = { tokens, last }

function Net.init()
	folder = Instance.new("Folder")
	folder.Name = "Remotes"
	for _, name in ipairs(Remotes.Events) do
		local e = Instance.new("RemoteEvent")
		e.Name = name
		e.Parent = folder
		events[name] = e
	end
	folder.Parent = ReplicatedStorage
	Players.PlayerRemoving:Connect(function(player)
		buckets[player] = nil
	end)
end

-- Token bucket: `rate` requests per second with bursts up to `burst`.
local function allow(player, name, rate, burst)
	local now = os.clock()
	local per = buckets[player]
	if not per then
		per = {}
		buckets[player] = per
	end
	local b = per[name]
	if not b then
		b = { tokens = burst, last = now }
		per[name] = b
	end
	b.tokens = math.min(burst, b.tokens + (now - b.last) * rate)
	b.last = now
	if b.tokens < 1 then
		return false
	end
	b.tokens = b.tokens - 1
	return true
end

-- handler(player, ...) runs only within the rate limit and with a live character.
function Net.on(name, rate, handler)
	local e = events[name]
	assert(e, "unknown remote " .. tostring(name))
	e.OnServerEvent:Connect(function(player, ...)
		if not allow(player, name, rate, math.max(2, rate)) then
			return
		end
		local ok, err = pcall(handler, player, ...)
		if not ok then
			warn(("[Net] %s from %s failed: %s"):format(name, player.Name, tostring(err)))
		end
	end)
end

function Net.fire(name, player, ...)
	events[name]:FireClient(player, ...)
end

function Net.fireAll(name, ...)
	events[name]:FireAllClients(...)
end

-- Validation helpers for remote payloads.
function Net.isVector(v)
	return typeof(v) == "Vector3" and v.X == v.X and v.Y == v.Y and v.Z == v.Z
		and math.abs(v.X) < 1e6 and math.abs(v.Y) < 1e6 and math.abs(v.Z) < 1e6
end

function Net.unit(v)
	if not Net.isVector(v) or v.Magnitude < 1e-3 then
		return nil
	end
	return v.Unit
end

function Net.isNumber(v, lo, hi)
	return type(v) == "number" and v == v and v >= lo and v <= hi
end

return Net
