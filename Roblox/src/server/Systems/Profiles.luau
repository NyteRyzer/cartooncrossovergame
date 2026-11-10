--!nonstrict
-- Saved player progress: level, XP, skill points and ranks, cash, collected
-- rift shards, course best times, missions completed.
--
-- One DataStore key per player, read-modify-write with UpdateAsync only, a
-- session lock so two servers never own the same profile (no dupes or
-- rollbacks when someone hops servers), retries with backoff, staggered
-- autosaves, and a final save on leave and on shutdown (BindToClose).
-- If a live server can't load someone's data they are kicked rather than
-- given a blank profile that would overwrite their real save. In Studio
-- without API access the game still runs with an unsaved profile.

local Players = game:GetService("Players")
local DataStoreService = game:GetService("DataStoreService")
local RunService = game:GetService("RunService")
local HttpService = game:GetService("HttpService")

local Profiles = {}
local STORE_NAME = "MultiverseToonsProfiles_v1"
local VERSION = 1
local AUTOSAVE = 60
local LOCK_STALE = AUTOSAVE * 2.5

local store
local profiles = {}   -- [player] = { Data = table, Saving = bool, NoSave = bool }
local loadedCallbacks = {}
local jobId = game.JobId ~= "" and game.JobId or ("studio-" .. HttpService:GenerateGUID(false))

local function defaultProfile()
	return {
		version = VERSION,
		Level = 1, Xp = 0, SkillPoints = 0, Skills = {}, Cash = 0, SkillPointsBought = 0,
		Shards = {},       -- array of shard ids
		Best = {},         -- course name -> seconds
		Missions = {},     -- mission id -> times completed
	}
end

local MIGRATIONS = {
	-- [1] = function(data) ... return data end,
}

local function migrate(data)
	data.version = data.version or 1
	while data.version < VERSION do
		local step = MIGRATIONS[data.version]
		if not step then
			error("missing migration from v" .. tostring(data.version))
		end
		data = step(data)
		data.version = data.version + 1
	end
	-- Fill fields added since (defensive; keeps old saves valid).
	for k, v in pairs(defaultProfile()) do
		if data[k] == nil then
			data[k] = v
		end
	end
	return data
end

local function withRetry(attempts, fn)
	local wait = 1
	for attempt = 1, attempts do
		local ok, a, b = pcall(fn)
		if ok then
			return true, a, b
		end
		warn(("[Profiles] attempt %d/%d failed: %s"):format(attempt, attempts, tostring(a)))
		if attempt < attempts then
			task.wait(wait)
			wait = math.min(wait * 2, 16)
		end
	end
	return false
end

local function key(player)
	return "u" .. player.UserId
end

-- Claim the lock and load. Returns data, or nil + reason ("locked" / "error").
local function claim(player)
	local result, reason
	local ok = withRetry(4, function()
		result, reason = nil, nil
		store:UpdateAsync(key(player), function(stored)
			local now = os.time()
			if stored and stored.lock and stored.lock.job ~= jobId and now - (stored.lock.time or 0) < LOCK_STALE then
				reason = "locked"
				return nil -- abort: another server owns this profile
			end
			local data = migrate(stored and stored.data or defaultProfile())
			result = data
			return { data = data, lock = { job = jobId, time = now } }
		end)
	end)
	if not ok then
		return nil, "error"
	end
	return result, reason
end

local function save(player, release)
	local entry = profiles[player]
	if not entry or entry.NoSave or not store then
		return true
	end
	local snapshot = HttpService:JSONDecode(HttpService:JSONEncode(entry.Data)) -- detach + validate JSON-safe
	return withRetry(release and 5 or 3, function()
		store:UpdateAsync(key(player), function(stored)
			if stored and stored.lock and stored.lock.job ~= jobId then
				return nil -- we lost the lock; never overwrite the new owner's data
			end
			return { data = snapshot, lock = (not release) and { job = jobId, time = os.time() } or nil }
		end)
	end)
end

local function setAttributes(player, data)
	player:SetAttribute("Level", data.Level)
	player:SetAttribute("Xp", data.Xp)
	player:SetAttribute("Cash", data.Cash)
	player:SetAttribute("SkillPoints", data.SkillPoints)
	for name, rank in pairs(data.Skills) do
		player:SetAttribute("Skill_" .. name, rank)
	end
end

local function onPlayerAdded(player)
	local data, reason
	if store then
		for _ = 1, 5 do
			data, reason = claim(player)
			if data or reason ~= "locked" then
				break
			end
			task.wait(4) -- the previous server may still be releasing it
		end
	end
	if not player.Parent then
		return
	end
	local entry
	if data then
		entry = { Data = data }
	elseif not store or RunService:IsStudio() then
		entry = { Data = defaultProfile(), NoSave = true }
		if store then
			warn("[Profiles] couldn't load data in Studio; using an unsaved profile")
		end
	else
		player:Kick(reason == "locked" and "Your progress is still saving from another server. Please rejoin in a moment."
			or "Your progress couldn't load right now. Please rejoin.")
		return
	end
	profiles[player] = entry
	setAttributes(player, entry.Data)
	player:SetAttribute("ProfileLoaded", true)
	for _, fn in ipairs(loadedCallbacks) do
		task.spawn(fn, player, entry.Data)
	end
	-- Staggered autosave.
	task.spawn(function()
		task.wait((player.UserId % AUTOSAVE) + 5)
		while profiles[player] == entry do
			save(player, false)
			task.wait(AUTOSAVE)
		end
	end)
end

function Profiles.get(player)
	local entry = profiles[player]
	return entry and entry.Data
end

-- fn(player, data) runs when a player's profile is ready (also for players already loaded).
function Profiles.onLoaded(fn)
	table.insert(loadedCallbacks, fn)
	for player, entry in pairs(profiles) do
		task.spawn(fn, player, entry.Data)
	end
end

function Profiles.sync(player)
	local data = Profiles.get(player)
	if data then
		setAttributes(player, data)
	end
end

function Profiles.init()
	local ok, result = pcall(function()
		return DataStoreService:GetDataStore(STORE_NAME)
	end)
	store = ok and result or nil
	if not store then
		warn("[Profiles] DataStores unavailable (unpublished place or API access off); progress won't save")
	end
end

function Profiles.start()
	Players.PlayerAdded:Connect(onPlayerAdded)
	for _, player in ipairs(Players:GetPlayers()) do
		task.spawn(onPlayerAdded, player)
	end
	Players.PlayerRemoving:Connect(function(player)
		if profiles[player] then
			save(player, true)
			profiles[player] = nil
		end
	end)
	game:BindToClose(function()
		if RunService:IsStudio() and not store then
			return
		end
		local pending = 0
		for player in pairs(profiles) do
			pending = pending + 1
			task.spawn(function()
				save(player, true)
				pending = pending - 1
			end)
		end
		local deadline = os.clock() + 25
		while pending > 0 and os.clock() < deadline do
			task.wait(0.2)
		end
	end)
end

return Profiles
