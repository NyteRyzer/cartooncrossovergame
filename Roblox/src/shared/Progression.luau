--!nonstrict
-- Pure progression rules shared by server (authoritative) and client (UI):
-- XP curve and level-ups, skill effects, shop prices. Tested by
-- Roblox/tools/test_logic.py.

local Progression = {}

function Progression.xpToNext(level, config)
	local p = config.Progression
	return math.floor(p.XpBase * p.XpGrowth ^ (level - 1))
end

-- Adds XP to a profile table ({ Level, Xp, SkillPoints }); returns levels gained.
function Progression.addXp(profile, amount, config)
	local gained = 0
	profile.Xp = profile.Xp + math.max(0, math.floor(amount))
	while profile.Level < config.Progression.MaxLevel and profile.Xp >= Progression.xpToNext(profile.Level, config) do
		profile.Xp = profile.Xp - Progression.xpToNext(profile.Level, config)
		profile.Level = profile.Level + 1
		profile.SkillPoints = profile.SkillPoints + 1
		gained = gained + 1
	end
	if profile.Level >= config.Progression.MaxLevel then
		profile.Xp = 0
	end
	return gained
end

-- Spends a point on a skill. Returns true on success.
function Progression.buySkill(profile, skill, config)
	local def = config.Skills[skill]
	if not def or profile.SkillPoints < 1 then
		return false
	end
	local rank = profile.Skills[skill] or 0
	if rank >= def.Max then
		return false
	end
	profile.Skills[skill] = rank + 1
	profile.SkillPoints = profile.SkillPoints - 1
	return true
end

-- Multipliers and bonuses from skill ranks.
function Progression.effects(skills)
	local function r(name)
		return (skills and skills[name]) or 0
	end
	return {
		MaxHealth = 12 * r("Vitality"),
		Damage = 1 + 0.08 * r("Power"),
		Energy = 15 * r("Reserves"),
		Flight = 1 + 0.08 * r("Aero"),
		Walk = 1 + 0.06 * r("Agility"),
		Meter = 1 + 0.12 * r("Focus"),
	}
end

function Progression.skillPointPrice(bought, config)
	return math.floor(config.Progression.SkillPrice * config.Progression.SkillPriceGrowth ^ bought)
end

return Progression
