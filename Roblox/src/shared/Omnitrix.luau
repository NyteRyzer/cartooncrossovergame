--!nonstrict
-- Ben's Omnitrix as a pure state machine: a line-for-line port of
-- Source/MultiverseToons/Public/Omnitrix/OmnitrixStateMachine.h so the Unreal
-- project, the prototype and the Roblox game share the same rules. The server
-- owns the authoritative instance; Roblox/tools/test_logic.py tests it.
--
-- Slots are 0-based like the C++ (0..NumSlots-1). Forms: >= 0 playlist slot,
-- NoForm, GlitchForm, OverclockForm. Assets are always resident on Roblox, so
-- transforms complete as soon as their timer runs out.

local Omnitrix = {}
Omnitrix.__index = Omnitrix

Omnitrix.NoForm = -1
Omnitrix.GlitchForm = -2
Omnitrix.OverclockForm = -3

Omnitrix.State = {
	Human = "Human",
	Selecting = "Selecting",
	Transforming = "Transforming",
	Alien = "Alien",
	Reverting = "Reverting",
	Recharging = "Recharging",
	Glitched = "Glitched",
	Overclocked = "Overclocked",
}
local S = Omnitrix.State

local DEFAULTS = {
	NumSlots = 3, TransformTime = 0.45, RevertTime = 0.6, AlienDuration = 60, RechargeTime = 8,
	SwapTimePenalty = 4, GlitchDuration = 4, GlitchCooldown = 30, OverclockDuration = 20,
}

local function max0(v)
	if v > 0 then
		return v
	end
	return 0
end

function Omnitrix.new(config)
	local c = {}
	for k, v in pairs(DEFAULTS) do
		c[k] = v
	end
	if config then
		for k, v in pairs(config) do
			c[k] = v
		end
	end
	local self = setmetatable({}, Omnitrix)
	self.Config = c
	self.State = S.Human
	self.ActiveForm = Omnitrix.NoForm
	self.PendingForm = Omnitrix.NoForm
	self.StateTimer = 0
	self.AlienTimeRemaining = 0
	self.GlitchCooldownRemaining = 0
	self.DialFromAlien = false
	return self
end

function Omnitrix:IsInvulnerable()
	return self.State == S.Transforming or self.State == S.Glitched
end

function Omnitrix:IsInputLocked()
	return self.State == S.Transforming or self.State == S.Reverting
end

function Omnitrix:IsTransformed()
	return self.State == S.Alien or self.State == S.Glitched or self.State == S.Overclocked
end

function Omnitrix:_validSlot(slot)
	return type(slot) == "number" and slot >= 0 and slot < self.Config.NumSlots and slot == math.floor(slot)
end

function Omnitrix:_beginTransform(form)
	self.DialFromAlien = false
	self.PendingForm = form
	self.State = S.Transforming
	self.StateTimer = self.Config.TransformTime
end

function Omnitrix:_beginRevert()
	self.DialFromAlien = false
	self.PendingForm = Omnitrix.NoForm
	self.AlienTimeRemaining = 0
	self.State = S.Reverting
	self.StateTimer = self.Config.RevertTime
end

function Omnitrix:OpenDial()
	if self.State ~= S.Human and self.State ~= S.Alien then
		return false
	end
	self.DialFromAlien = self.State == S.Alien
	self.State = S.Selecting
	return true
end

function Omnitrix:CancelDial()
	if self.State ~= S.Selecting then
		return false
	end
	if self.DialFromAlien then
		self.State = S.Alien
	else
		self.State = S.Human
	end
	return true
end

function Omnitrix:Confirm(slot)
	if self.State ~= S.Selecting or not self:_validSlot(slot) then
		return false
	end
	if self.DialFromAlien then
		self.State = S.Alien
		return self:QuickSwap(slot)
	end
	self.AlienTimeRemaining = self.Config.AlienDuration
	self:_beginTransform(slot)
	return true
end

function Omnitrix:QuickSwap(slot)
	if not self:_validSlot(slot) then
		return false
	end
	if self.State == S.Human then
		self.AlienTimeRemaining = self.Config.AlienDuration
		self:_beginTransform(slot)
		return true
	end
	if self.State ~= S.Alien or slot == self.ActiveForm or self.AlienTimeRemaining <= self.Config.SwapTimePenalty then
		return false
	end
	self.AlienTimeRemaining = self.AlienTimeRemaining - self.Config.SwapTimePenalty
	self:_beginTransform(slot)
	return true
end

function Omnitrix:TriggerGlitch()
	if self.GlitchCooldownRemaining > 0 then
		return false
	end
	if self.State == S.Glitched or self.State == S.Overclocked or self.State == S.Reverting then
		return false
	end
	self.DialFromAlien = false
	self.PendingForm = Omnitrix.NoForm
	self.ActiveForm = Omnitrix.GlitchForm
	self.State = S.Glitched
	self.StateTimer = self.Config.GlitchDuration
	self.GlitchCooldownRemaining = self.Config.GlitchCooldown
	return true
end

function Omnitrix:TriggerOverclock()
	if self.State ~= S.Human and self.State ~= S.Alien and self.State ~= S.Recharging then
		return false
	end
	self:_beginTransform(Omnitrix.OverclockForm)
	return true
end

function Omnitrix:ForceRevert()
	if self.State == S.Human or self.State == S.Reverting or self.State == S.Recharging then
		return false
	end
	if self.State == S.Selecting and not self.DialFromAlien then
		self.State = S.Human
		return true
	end
	self:_beginRevert()
	return true
end

function Omnitrix:Tick(dt)
	if dt <= 0 then
		return
	end
	if self.GlitchCooldownRemaining > 0 then
		self.GlitchCooldownRemaining = max0(self.GlitchCooldownRemaining - dt)
	end
	local alienClock = self.State == S.Alien
		or (self.State == S.Selecting and self.DialFromAlien)
		or (self.State == S.Transforming and self.PendingForm >= 0 and self.ActiveForm >= 0)
	if alienClock then
		self.AlienTimeRemaining = max0(self.AlienTimeRemaining - dt)
		if self.AlienTimeRemaining <= 0 and self.State ~= S.Transforming then
			self:_beginRevert()
			return
		end
	end
	local st = self.State
	if st == S.Transforming then
		self.StateTimer = max0(self.StateTimer - dt)
		if self.StateTimer <= 0 then
			self.ActiveForm = self.PendingForm
			self.PendingForm = Omnitrix.NoForm
			if self.ActiveForm == Omnitrix.OverclockForm then
				self.State = S.Overclocked
				self.StateTimer = self.Config.OverclockDuration
			elseif self.AlienTimeRemaining <= 0 then
				self:_beginRevert()
			else
				self.State = S.Alien
			end
		end
	elseif st == S.Glitched or st == S.Overclocked then
		self.StateTimer = max0(self.StateTimer - dt)
		if self.StateTimer <= 0 then
			self:_beginRevert()
		end
	elseif st == S.Reverting then
		self.StateTimer = max0(self.StateTimer - dt)
		if self.StateTimer <= 0 then
			self.ActiveForm = Omnitrix.NoForm
			self.State = S.Recharging
			self.StateTimer = self.Config.RechargeTime
		end
	elseif st == S.Recharging then
		self.StateTimer = max0(self.StateTimer - dt)
		if self.StateTimer <= 0 then
			self.State = S.Human
		end
	end
end

-- Index into Config.Forms for the active form (nil when human).
function Omnitrix.FormIndex(form, config)
	if form >= 0 then
		return form + 1
	elseif form == Omnitrix.GlitchForm then
		return config.GlitchForm
	elseif form == Omnitrix.OverclockForm then
		return config.OverclockForm
	end
	return nil
end

return Omnitrix
