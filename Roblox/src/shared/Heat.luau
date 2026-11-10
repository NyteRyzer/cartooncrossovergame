--!nonstrict
-- Wanted level ("threat stars"), as a pure state machine. Chaos raises heat;
-- while you have stars, hunter squads are sent after you; stay out of combat
-- and away from hunters for a while and the stars cool off one by one.

local Heat = {}
Heat.__index = Heat

function Heat.new(config)
	return setmetatable({ Config = config.Heat, Value = 0, LastChaos = -1e9, SquadTimer = 0 }, Heat)
end

function Heat:Stars()
	return math.floor(self.Value)
end

function Heat:AddChaos(amount, now)
	self.Value = math.min(self.Config.Max + 0.99, self.Value + amount)
	self.LastChaos = now
end

-- Cooling down: no recent chaos and no hunters near.
function Heat:Cooling(now, huntersNear)
	return self.Value > 0 and not huntersNear and now - self.LastChaos >= self.Config.DecayDelay
end

-- Advances the timers. Returns the number of hunters to spawn this tick (0 most of the time).
function Heat:Tick(dt, now, huntersNear)
	if self:Cooling(now, huntersNear) then
		self.Value = math.max(0, self.Value - self.Config.DecayPerSecond * dt)
	end
	local stars = self:Stars()
	if stars <= 0 then
		self.SquadTimer = 0
		return 0
	end
	self.SquadTimer = self.SquadTimer + dt
	local interval = self.Config.SquadInterval[math.min(stars, #self.Config.SquadInterval)]
	if self.SquadTimer >= interval then
		self.SquadTimer = 0
		return self.Config.SquadSize[math.min(stars, #self.Config.SquadSize)]
	end
	return 0
end

return Heat
