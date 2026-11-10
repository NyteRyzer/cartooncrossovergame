--!nonstrict
-- Per-player authoritative state: chosen hero, Omnitrix, ultimate meter,
-- cooldowns, energy for server-checked abilities, and damage immunity windows.

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))
local Omnitrix = require(Shared:WaitForChild("Omnitrix"))

local PlayerState = {}
local states = {}

function PlayerState.new(player)
	return {
		Player = player,
		Hero = "Jimmy",
		Omni = Omnitrix.new(Config.Omnitrix),
		Meter = 0,
		Energy = Config.Player.MaxEnergy,
		Cooldowns = {},       -- [name] = time when ready
		AttackStep = 0,
		LastAttack = -10,
		Intangible = false,
		DodgeUntil = 0,
		LastDodge = -10,
		Level = Config.Levels.Nexus,
		Kills = 0,
		Shards = 0,
		RegenAt = 0,
		Combo = 0,
		ComboAt = 0,
		DamageMul = 1,
		MeterMul = 1,
	}
end

function PlayerState.init()
	Players.PlayerAdded:Connect(function(player)
		states[player] = PlayerState.new(player)
	end)
	for _, player in ipairs(Players:GetPlayers()) do
		if not states[player] then
			states[player] = PlayerState.new(player)
		end
	end
	Players.PlayerRemoving:Connect(function(player)
		states[player] = nil
	end)
end

function PlayerState.get(player)
	return states[player]
end

function PlayerState.all()
	return states
end

-- Cooldown helper: true (and arms it) when `name` is ready.
function PlayerState.ready(st, name, seconds)
	local now = os.clock()
	if (st.Cooldowns[name] or 0) > now then
		return false
	end
	st.Cooldowns[name] = now + seconds
	return true
end

function PlayerState.addMeter(st, amount)
	st.Meter = math.clamp(st.Meter + amount, 0, 100)
	local character = st.Player.Character
	if character then
		character:SetAttribute("Meter", math.floor(st.Meter))
	end
end

-- Active Omnitrix form definition (nil when human or not Ben).
function PlayerState.form(st)
	if st.Hero ~= "Ben" then
		return nil
	end
	local index = Omnitrix.FormIndex(st.Omni.ActiveForm, Config)
	if index then
		return Config.Forms[index]
	end
	return nil
end

return PlayerState
