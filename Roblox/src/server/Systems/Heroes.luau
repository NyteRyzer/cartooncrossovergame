--!nonstrict
-- Hero selection, looks and stats, Ben's Omnitrix, and replication of each
-- client's movement state (flying, dodging, emoting...) as character
-- attributes so every client can animate every character.

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))
local Omnitrix = require(Shared:WaitForChild("Omnitrix"))
local Looks = require(script.Parent.Parent:WaitForChild("Lib"):WaitForChild("Looks"))

local Heroes = {}
local Net, PlayerState, World, Progression

-- Movement state keys the client may report, with their type and limits.
local STATE_KEYS = {
	Flying = "boolean", Gliding = "boolean", Dodge = "boolean", DodgeAir = "boolean", Pounding = "boolean",
	Emote = "boolean", WallSlide = "boolean", Boosting = "boolean", Jetpack = "boolean", Scooter = "boolean",
	Intangible = "boolean", Takeoff = "boolean", Grapple = "boolean", Breath = "boolean",
	Slide = "boolean", WallRun = "boolean", Climb = "boolean", Swing = "boolean", WallSide = "number", Lunge = "boolean",
	DodgeSide = "number", FlyBank = "number", FlyPitch = "number",
}

local function heroDef(st)
	return Config.Heroes[st.Hero] or Config.Heroes.Jimmy
end

-- Walk speed / jump from the hero or active form.
function Heroes.applyStats(player)
	local st = PlayerState.get(player)
	local character = player.Character
	local humanoid = character and character:FindFirstChildOfClass("Humanoid")
	if not st or not humanoid then
		return
	end
	local hero = heroDef(st)
	local form = PlayerState.form(st)
	humanoid.UseJumpPower = false
	humanoid.WalkSpeed = form and form.Walk or hero.Walk
	humanoid.JumpHeight = form and form.JumpHeight or hero.JumpHeight
	character:SetAttribute("Hero", st.Hero)
	character:SetAttribute("Form", form and form.Id or nil)
	character:SetAttribute("AirJumps", form and 0 or hero.AirJumps)
	character:SetAttribute("Meter", math.floor(st.Meter))
end

function Heroes.refreshLook(player)
	local st = PlayerState.get(player)
	local character = player.Character
	if not st or not character then
		return
	end
	Heroes.applyStats(player)
	task.spawn(Looks.apply, character, st.Hero, PlayerState.form(st))
end

function Heroes.select(player, heroId)
	local st = PlayerState.get(player)
	if not st or not Config.Heroes[heroId] then
		return false
	end
	st.Hero = heroId
	st.Omni = Omnitrix.new(Config.Omnitrix)
	st.Energy = Config.Player.MaxEnergy
	st.Intangible = false
	Heroes.refreshLook(player)
	local hero = Config.Heroes[heroId]
	Net.fire("Notify", player, { Text = "Now playing: " .. hero.Name .. " (" .. hero.Universe .. ")", Color = hero.Accent })
	Net.fireAll("Fx", { Kind = "Burst", Pos = player.Character and player.Character:GetPivot().Position, Color = hero.Accent, Count = 40 })
	return true
end

local function onCharacter(player, character)
	local humanoid = character:WaitForChild("Humanoid", 10)
	if not humanoid then
		return
	end
	-- Procedural animation replaces the default Animate script on R15 rigs.
	if humanoid.RigType == Enum.HumanoidRigType.R15 then
		local animate = character:WaitForChild("Animate", 2)
		if animate then
			animate:Destroy()
		end
	end
	humanoid.MaxHealth = Config.Player.MaxHealth
	humanoid.Health = Config.Player.MaxHealth
	humanoid.BreakJointsOnDeath = false
	local st = PlayerState.get(player)
	if st and st.Hero == "Ben" then
		st.Omni = Omnitrix.new(Config.Omnitrix)
	end
	Heroes.refreshLook(player)
	-- Skill bonuses (max health) after the base stats are set.
	if Progression then
		Progression.applyToCharacter(player)
	end
end

local function onState(player, state)
	local st = PlayerState.get(player)
	local character = player.Character
	if not st or not character or type(state) ~= "table" then
		return
	end
	local hero = heroDef(st)
	for key, kind in pairs(STATE_KEYS) do
		local v = state[key]
		if type(v) == kind and not (kind == "number" and v ~= v) then
			if kind == "number" then
				v = math.clamp(v, -90, 90)
				if key == "DodgeSide" then
					v = v < 0 and -1 or 1
				end
			end
			-- Only heroes who can fly report flight; only Danny goes intangible.
			if key == "Flying" and v and not (hero.CanFly and not PlayerState.form(st)) then
				v = false
			end
			if key == "Intangible" then
				-- Same 15-energy threshold as the client to switch on.
				v = v and st.Hero == "Danny" and (st.Intangible and st.Energy > 0 or st.Energy >= 15)
				st.Intangible = v
			end
			if key == "Pounding" and v and not character:GetAttribute("MvPounding") then
				local root = character:FindFirstChild("HumanoidRootPart")
				st.PoundStartY = root and root.Position.Y or nil
				st.PoundAt = os.clock()
			end
			if key == "Dodge" and v and not character:GetAttribute("MvDodge") then
				-- Server-side dodge cooldown bounds the immunity window.
				local now = os.clock()
				-- Immunity is much shorter than the cooldown, so toggling can't chain it.
				if now - st.LastDodge >= Config.Moves.DodgeCooldown * 0.9 then
					st.LastDodge = now
					st.DodgeUntil = now + 0.25
				else
					v = false
				end
			end
			character:SetAttribute("Mv" .. key, v)
		end
	end
end

local function onOmnitrix(player, action, slot)
	local st = PlayerState.get(player)
	if not st or st.Hero ~= "Ben" or type(action) ~= "string" then
		return
	end
	local m = st.Omni
	local ok = false
	if action == "dial" then
		ok = m:OpenDial()
	elseif action == "cancel" then
		ok = m:CancelDial()
	elseif action == "slot" and type(slot) == "number" then
		if m.State == Omnitrix.State.Selecting then
			ok = m:Confirm(slot)
		else
			ok = m:QuickSwap(slot)
		end
	elseif action == "glitch" then
		ok = m:TriggerGlitch()
		if ok then
			Net.fire("Notify", player, { Text = "MASTER CONTROL GLITCH - Cannonbolt shield!", Color = Color3.fromRGB(255, 220, 80) })
		end
	elseif action == "overclock" then
		if st.Meter >= 100 then
			ok = m:TriggerOverclock()
			if ok then
				PlayerState.addMeter(st, -100)
				Net.fire("Notify", player, { Banner = "WAY BIG OVERCLOCK!" })
			end
		end
	end
	if not ok and action ~= "cancel" and action ~= "dial" then
		Net.fire("Notify", player, { Text = m.State == Omnitrix.State.Recharging and "Omnitrix is recharging..." or "Can't transform right now", Color = Color3.fromRGB(255, 140, 120), Time = 1.5 })
	end
end

function Heroes.init(systems)
	Net, PlayerState, World, Progression = systems.Net, systems.Players, systems.World, systems.Progression
	Net.on("State", 20, onState)
	Net.on("Omnitrix", 8, onOmnitrix)
	Net.on("SelectHero", 2, function(player, heroId)
		if type(heroId) ~= "string" then
			return
		end
		if World and not World.nearTerminal(player) then
			Net.fire("Notify", player, { Text = "Walk up to a dimensional terminal to swap heroes", Time = 2 })
			return
		end
		Heroes.select(player, heroId)
	end)
end

function Heroes.start()
	local function hook(player)
		player.CharacterAdded:Connect(function(character)
			onCharacter(player, character)
		end)
		if player.Character then
			task.spawn(onCharacter, player, player.Character)
		end
	end
	Players.PlayerAdded:Connect(hook)
	for _, player in ipairs(Players:GetPlayers()) do
		hook(player)
	end

	-- Omnitrix ticks, Danny's intangibility energy, and form changes.
	RunService.Heartbeat:Connect(function(dt)
		for player, st in pairs(PlayerState.all()) do
			local character = player.Character
			if st.Hero == "Ben" and character then
				local before = st.Omni.ActiveForm
				local beforeState = st.Omni.State
				st.Omni:Tick(dt)
				if st.Omni.ActiveForm ~= before then
					Heroes.refreshLook(player)
					local form = PlayerState.form(st)
					if form then
						Net.fire("Notify", player, { Text = "It's " .. form.Id .. "!", Color = Config.Heroes.Ben.Accent, Time = 2.5 })
					end
				end
				if st.Omni.State ~= beforeState then
					character:SetAttribute("Omnitrix", st.Omni.State)
					if beforeState == Omnitrix.State.Recharging and st.Omni.State == Omnitrix.State.Human then
						Net.fire("Notify", player, { Text = "Omnitrix recharged", Color = Color3.fromRGB(90, 255, 90), Time = 2 })
					end
				end
				character:SetAttribute("AlienTime", math.ceil(st.Omni.AlienTimeRemaining))
			end
			if st.Intangible then
				st.Energy = math.max(0, st.Energy - 12 * dt)
				if st.Energy <= 0 and character then
					st.Intangible = false
					character:SetAttribute("MvIntangible", false)
				end
			elseif st.Energy < (st.MaxEnergy or Config.Player.MaxEnergy) then
				st.Energy = math.min(st.MaxEnergy or Config.Player.MaxEnergy, st.Energy + Config.Player.EnergyRegen * dt)
			end
		end
	end)
end

return Heroes
