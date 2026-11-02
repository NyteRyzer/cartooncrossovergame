--!nonstrict
-- Every remote in the game, created by the server (Systems/Net) at startup.
-- Client -> server requests are validated and rate limited there.
return {
	Events = {
		"State",        -- client: own movement state (flying, dodge, emote...) for animation replication
		"Attack",       -- client: aim direction
		"Ability",      -- client: ability name, aim direction, held flag
		"Omnitrix",     -- client: "dial" | "cancel" | "slot" n | "glitch" | "overclock"
		"SelectHero",   -- client: hero id (at a dimensional terminal)
		"Travel",       -- client: level name (at a terminal) or "Nexus" (recall anywhere)
		"Pound",        -- client: ground pound landed
		"Fx",           -- server -> clients: visual effects
		"Notify",       -- server -> client: messages, banners
		"OpenTerminal", -- server -> client: show the hero/travel menu
	},
	Functions = {},
}
