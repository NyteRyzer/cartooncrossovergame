--!nonstrict
-- Gameplay tunables shared by server and client (docs/GAME_BIBLE.md roster).
-- Distances are studs (1 stud = 30 cm of the Unreal/prototype layouts), speeds
-- studs/s, times seconds. The server is authoritative for damage, cooldowns
-- and the ultimate meter; the client copy drives movement and prediction.

local Config = {}

Config.StudCm = 30

-- Hero ids (order matches the prototype's HeroId and the terminal menu).
Config.HeroOrder = { "Jimmy", "Timmy", "Danny", "Jake", "XJ9", "ElTigre", "Ben", "Aang" }

local function rgb(r, g, b)
	return Color3.fromRGB(r, g, b)
end

-- Look: Skin/Shirt/Sleeve/Forearm/Hand/Pants/Shin/Shoe colours for the R15
-- body parts, head scale for the cartoon proportions, and face style.
-- Attack: Kind "Projectile" | "Melee"; Damage, Cooldown, Speed, Range, Radius,
-- Knockback, Count (spread shots), Color.
Config.Heroes = {
	Jimmy = {
		Name = "Jimmy Neutron", Universe = "Retroville", Accent = rgb(255, 80, 80),
		Walk = 21, JumpHeight = 7.5, AirJumps = 0, CanFly = false, CanGlide = false,
		Traversal = "Q Goddard jetpack, E drive the hover car",
		Sig = "Goddard Jetpack", Ult = "Hyper-Cube Anomaly",
		Look = {
			Skin = rgb(250, 210, 180), Shirt = rgb(210, 40, 40), Sleeve = rgb(210, 40, 40), Forearm = rgb(250, 210, 180),
			Hand = rgb(250, 210, 180), Pants = rgb(60, 90, 180), Shin = rgb(60, 90, 180), Shoe = rgb(245, 245, 248),
			HeadScale = 1.45, Height = 0.85, Width = 0.95,
			Eyes = rgb(70, 120, 220), Brows = rgb(120, 70, 35),
		},
		Attack = { Kind = "Projectile", Damage = 18, Cooldown = 0.22, Speed = 220, Radius = 0.7, Knockback = 25, Count = 1, Color = rgb(255, 60, 60), Name = "Laser Blaster" },
	},
	Timmy = {
		Name = "Timmy Turner", Universe = "Dimmsdale", Accent = rgb(255, 230, 80),
		Walk = 21, JumpHeight = 7.5, AirJumps = 0, CanFly = true, FlySpeed = 50, FlyBoost = 87, CanGlide = false,
		Traversal = "Tab fairy cloud-board flight",
		Sig = "Anvil Drop", Ult = "Da Rules Loophole",
		Look = {
			Skin = rgb(250, 210, 180), Shirt = rgb(240, 110, 160), Sleeve = rgb(240, 110, 160), Forearm = rgb(250, 210, 180),
			Hand = rgb(250, 210, 180), Pants = rgb(60, 90, 180), Shin = rgb(60, 90, 180), Shoe = rgb(245, 245, 248),
			HeadScale = 1.45, Height = 0.82, Width = 0.95,
			Eyes = rgb(80, 120, 200), Brows = rgb(120, 70, 35), BigTeeth = true,
		},
		Attack = { Kind = "Projectile", Damage = 12, Cooldown = 0.25, Speed = 150, Radius = 0.8, Knockback = 20, Count = 3, Color = rgb(255, 230, 80), Name = "Magic Stars" },
	},
	Danny = {
		Name = "Danny Phantom", Universe = "Amity Park", Accent = rgb(80, 255, 120),
		Walk = 21, JumpHeight = 7.5, AirJumps = 0, CanFly = true, FlySpeed = 60, FlyBoost = 117, CanGlide = false,
		Traversal = "Tab ghost flight, Q intangible",
		Sig = "Intangibility", Ult = "Ghost Zone Mastery",
		Look = {
			Skin = rgb(250, 215, 190), Shirt = rgb(30, 30, 36), Sleeve = rgb(30, 30, 36), Forearm = rgb(30, 30, 36),
			Hand = rgb(245, 245, 250), Pants = rgb(30, 30, 36), Shin = rgb(30, 30, 36), Shoe = rgb(245, 245, 250),
			HeadScale = 1.15, Height = 1.05, Width = 1.0, Belt = rgb(245, 245, 250),
			Eyes = rgb(80, 255, 120), Brows = rgb(240, 240, 245), GlowEyes = true,
		},
		Attack = { Kind = "Projectile", Damage = 20, Cooldown = 0.2, Speed = 220, Radius = 0.8, Knockback = 30, Count = 1, Color = rgb(80, 255, 120), Name = "Ecto Blast" },
	},
	Jake = {
		Name = "Jake Long", Universe = "NYC Chinatown", Accent = rgb(80, 220, 120),
		Walk = 23, JumpHeight = 8, AirJumps = 0, CanFly = true, FlySpeed = 67, FlyBoost = 127, CanGlide = false,
		Traversal = "Tab dragon form flight",
		Sig = "Dragon Fire Breath (hold Q)", Ult = "Ancient Dragon Fury",
		Look = {
			Skin = rgb(225, 185, 150), Shirt = rgb(200, 40, 40), Sleeve = rgb(200, 40, 40), Forearm = rgb(200, 40, 40),
			Hand = rgb(225, 185, 150), Pants = rgb(60, 85, 150), Shin = rgb(60, 85, 150), Shoe = rgb(230, 230, 230),
			HeadScale = 1.15, Height = 1.0, Width = 1.0,
			Eyes = rgb(60, 40, 30), Brows = rgb(20, 20, 22),
		},
		Attack = { Kind = "Melee", Damage = 28, Cooldown = 0.32, Range = 8, Knockback = 60, Color = rgb(255, 140, 40), Name = "Dragon Claws" },
	},
	XJ9 = {
		Name = "XJ9 (Jenny)", Universe = "Tremorton", Accent = rgb(80, 230, 255),
		Walk = 20, JumpHeight = 7, AirJumps = 0, CanFly = true, FlySpeed = 63, FlyBoost = 120, CanGlide = false,
		Traversal = "Tab rocket-boot flight",
		Sig = "Missile Barrage", Ult = "Global Defense Protocol",
		Look = {
			Skin = rgb(240, 244, 250), Shirt = rgb(240, 244, 250), Sleeve = rgb(240, 244, 250), Forearm = rgb(240, 244, 250),
			Hand = rgb(240, 244, 250), Pants = rgb(240, 244, 250), Shin = rgb(70, 190, 205), Shoe = rgb(70, 190, 205),
			HeadScale = 1.25, Height = 1.0, Width = 0.85, Robot = true,
			Eyes = rgb(60, 140, 225), Brows = rgb(60, 140, 225),
		},
		Attack = { Kind = "Projectile", Damage = 9, Cooldown = 0.08, Speed = 250, Radius = 0.5, Knockback = 12, Count = 1, Color = rgb(80, 230, 255), Name = "Arm Blaster" },
	},
	ElTigre = {
		Name = "El Tigre", Universe = "Miracle City", Accent = rgb(240, 140, 40),
		Walk = 25, JumpHeight = 9, AirJumps = 1, CanFly = false, CanGlide = false,
		Traversal = "Hold Q grappling chain, double jump, wall cling",
		Sig = "Mighty Chain Grapple", Ult = "Spirit of the Ancestors",
		Look = {
			Skin = rgb(125, 78, 42), Shirt = rgb(55, 40, 38), Sleeve = rgb(55, 40, 38), Forearm = rgb(70, 50, 40),
			Hand = rgb(70, 50, 40), Pants = rgb(85, 52, 32), Shin = rgb(85, 52, 32), Shoe = rgb(30, 25, 25),
			HeadScale = 1.45, Height = 0.78, Width = 1.05, Belt = rgb(200, 40, 40),
			Eyes = rgb(80, 160, 70), Brows = rgb(30, 20, 15), Mask = rgb(70, 45, 25),
		},
		Attack = { Kind = "Melee", Damage = 32, Cooldown = 0.28, Range = 8, Knockback = 70, Color = rgb(255, 200, 80), Name = "Steel Claws" },
	},
	Ben = {
		Name = "Ben Tennyson", Universe = "Bellwood", Accent = rgb(90, 255, 90),
		Walk = 21, JumpHeight = 7.5, AirJumps = 0, CanFly = false, CanGlide = false,
		Traversal = "Omnitrix: 1/2/3 aliens, Q dial, G glitch",
		Sig = "Omnitrix Dial", Ult = "Way Big Overclock",
		Look = {
			Skin = rgb(250, 210, 180), Shirt = rgb(240, 240, 240), Sleeve = rgb(240, 240, 240), Forearm = rgb(250, 210, 180),
			Hand = rgb(250, 210, 180), Pants = rgb(95, 120, 60), Shin = rgb(95, 120, 60), Shoe = rgb(240, 240, 240),
			HeadScale = 1.2, Height = 0.95, Width = 1.0, Stripe = rgb(30, 30, 30),
			Eyes = rgb(70, 190, 90), Brows = rgb(120, 75, 40),
		},
		Attack = { Kind = "Melee", Damage = 10, Cooldown = 0.4, Range = 6, Knockback = 25, Color = rgb(255, 255, 255), Name = "Punch" },
	},
	Aang = {
		Name = "Aang", Universe = "Eastern Air Temple", Accent = rgb(80, 180, 255),
		Walk = 23, JumpHeight = 8, AirJumps = 1, CanFly = false, CanGlide = true,
		Traversal = "Hold Space falling to glide, Q air scooter, R earth pillar",
		Sig = "Air Scooter", Ult = "The Avatar State",
		Look = {
			Skin = rgb(245, 205, 165), Shirt = rgb(240, 190, 60), Sleeve = rgb(240, 190, 60), Forearm = rgb(240, 190, 60),
			Hand = rgb(245, 205, 165), Pants = rgb(238, 185, 75), Shin = rgb(238, 185, 75), Shoe = rgb(125, 80, 50),
			HeadScale = 1.2, Height = 0.95, Width = 1.0, Belt = rgb(230, 110, 40),
			Eyes = rgb(90, 70, 50), Brows = rgb(90, 60, 40), Bald = true,
		},
		Attack = { Kind = "Projectile", Damage = 14, Cooldown = 0.28, Speed = 170, Radius = 1.8, Knockback = 120, Count = 1, Color = rgb(230, 245, 255), Name = "Air Blast" },
	},
}

-- Ben's Omnitrix forms. Index 1..3 = playlist, then glitch and overclock.
Config.Forms = {
	{ Id = "Heatblast", Body = rgb(200, 55, 30), Accent = rgb(255, 190, 40), Scale = 1.15, Walk = 23, JumpHeight = 8, Glide = true,
		Attack = { Kind = "Projectile", Damage = 26, Cooldown = 0.3, Speed = 170, Radius = 1.3, Knockback = 30, Count = 1, Color = rgb(255, 150, 30), Name = "Fireball" } },
	{ Id = "FourArms", Body = rgb(200, 45, 45), Accent = rgb(40, 40, 40), Scale = 1.7, Walk = 21, JumpHeight = 12,
		Attack = { Kind = "Melee", Damage = 55, Cooldown = 0.5, Range = 12, Knockback = 110, Color = rgb(255, 220, 200), Name = "Quad Punch" } },
	{ Id = "XLR8", Body = rgb(40, 60, 170), Accent = rgb(30, 30, 35), Scale = 1.0, Walk = 80, JumpHeight = 7.5,
		Attack = { Kind = "Melee", Damage = 18, Cooldown = 0.11, Range = 7, Knockback = 35, Color = rgb(120, 180, 255), Name = "Speed Strikes" } },
	{ Id = "Cannonbolt", Body = rgb(240, 240, 225), Accent = rgb(250, 200, 40), Scale = 1.3, Walk = 55, JumpHeight = 7.5, Roll = true,
		Attack = nil },
	{ Id = "WayBig", Body = rgb(240, 240, 245), Accent = rgb(210, 40, 40), Scale = 4.0, Walk = 36, JumpHeight = 20,
		Attack = { Kind = "Melee", Damage = 200, Cooldown = 0.9, Range = 45, Knockback = 200, Color = rgb(255, 255, 255), Name = "Cosmic Stomp" } },
}
Config.GlitchForm = 4
Config.OverclockForm = 5

-- Omnitrix timings (Source/MultiverseToons/Public/Omnitrix/OmnitrixStateMachine.h).
-- Same values as the prototype (Game::OmnitrixConfig) and UOmnitrixComponent.
Config.Omnitrix = {
	NumSlots = 3, TransformTime = 0.45, RevertTime = 0.6, AlienDuration = 45, RechargeTime = 6,
	SwapTimePenalty = 4, GlitchDuration = 4, GlitchCooldown = 30, OverclockDuration = 20,
}

-- Per-hero flight handling (see Prototype/src/Game.cpp FlightFor).
Config.Flight = {
	Timmy = { Accel = 73, TurnRate = 150, Brake = 60, BoostCost = 9, MaxBank = 32, DiveGain = 17 },
	Danny = { Accel = 217, TurnRate = 420, Brake = 233, BoostCost = 14, MaxBank = 45, DiveGain = 20 },
	Jake = { Accel = 107, TurnRate = 170, Brake = 67, BoostCost = 16, MaxBank = 70, DiveGain = 43 },
	XJ9 = { Accel = 127, TurnRate = 120, Brake = 83, BoostCost = 18, MaxBank = 60, DiveGain = 27 },
}

Config.Glide = { MinSpeed = 22, MaxSpeed = 97, Cruise = 38, StallSpeed = 28, DiveAccel = 57 }

Config.Moves = {
	DodgeSpeed = 58, DodgeTime = 0.4, AirDodgeTime = 0.32, DodgeCooldown = 0.6, BarrelRollTime = 0.45,
	PoundSpeed = 95, PoundRadius = 22, PoundDamage = 35,
	WallJumpOut = 27, CoyoteTime = 0.12, JumpBuffer = 0.14, EmoteTime = 2.4,
	ComboWindow = 0.5, FinisherDamage = 1.8, FinisherKnock = 1.7,
}

Config.Player = { MaxHealth = 100, Regen = 8, RegenDelay = 4, MaxEnergy = 100, EnergyRegen = 14 }

Config.Enemy = { Health = 40, Speed = 14, ContactDamage = 6, ContactRange = 5, AggroRange = 160, AttackCooldown = 1.0 }

Config.Shards = { Retroville = 24, Nexus = 10, Reach = 6, Health = 15, Meter = 10 }

-- Names of the map folders built into Workspace by Roblox/tools/build_place.py.
Config.Levels = { Nexus = "NexusObservatory", Retroville = "Retroville" }

-- ---------------------------------------------------------------------------
-- Combat 2.0: freeflow counters, posture/stagger and finishers, takedowns,
-- lock-on and lunges (in the spirit of the Arkham, Spider-Man, Sekiro and
-- Doom-style finisher games), enemy types and the Rift Titan boss.
-- ---------------------------------------------------------------------------
Config.Combat = {
	CounterRange = 16,      -- enemies winding up this close can be countered
	CounterWindow = 0.45,   -- ...during the last this-many seconds of their wind-up
	WhiffLockout = 0.9,     -- a counter with nothing to counter locks you out this long
	CounterDamage = 15,
	CounterImmunity = 0.45,
	BlockTime = 0.4,        -- a counter with nothing to counter becomes a short block
	BlockReduce = 0.7,      -- damage taken while blocking is reduced by this much
	CounterCooldown = 0.45,
	ComboTimeout = 2.5,     -- seconds without landing a hit before the combo drops
	TakedownCombo = 8,      -- combo needed for a special takedown
	TakedownRange = 26,
	StaggerTime = 2.5,      -- posture broken: open to a finisher
	FinisherHeal = 20,
	PostureDecay = 12,      -- per second, after 2 s without being hit
	LockRange = 75,
	LungeRange = 30,
	LungeSpeed = 75,
}

Config.EnemyTypes = {
	Gremlin = { Health = 40, Speed = 14, Damage = 8, Posture = 60, Windup = 0.6, Reach = 6, Size = 3.2,
		Color = rgb(170, 80, 230), Xp = 10, Cash = 5, Heat = 0.12 },
	Brute = { Health = 170, Speed = 9, Damage = 18, Posture = 150, Windup = 0.95, Reach = 10, Size = 6.5,
		Color = rgb(150, 40, 60), Xp = 35, Cash = 20, Heat = 0.35, Slam = 13 },
	Spitter = { Health = 30, Speed = 12, Damage = 10, Posture = 40, Windup = 0.8, Reach = 70, Size = 2.6,
		Color = rgb(90, 200, 90), Xp = 15, Cash = 8, Heat = 0.15, Ranged = true, KeepAway = 35 },
	Hunter = { Health = 70, Speed = 22, Damage = 10, Posture = 80, Windup = 0.5, Reach = 6, Size = 3.4,
		Color = rgb(70, 110, 170), Xp = 20, Cash = 12, Heat = 0.04 },
}

Config.Boss = {
	Name = "Rift Titan", Health = 3000, PerExtraPlayer = 0.5, Size = 22, Speed = 10, Posture = 700,
	StompDamage = 22, StompRadius = 70, ChargeDamage = 25, SpitDamage = 14, Xp = 600, Cash = 400,
	Color = rgb(110, 40, 160),
}

-- ---------------------------------------------------------------------------
-- Progression (levels, skills, cash), wanted level, missions, the shop.
-- ---------------------------------------------------------------------------
Config.Progression = { XpBase = 100, XpGrowth = 1.22, MaxLevel = 50, SkillPrice = 250, SkillPriceGrowth = 1.5 }

Config.SkillOrder = { "Vitality", "Power", "Reserves", "Aero", "Agility", "Focus" }
Config.Skills = {
	Vitality = { Max = 5, Name = "Vitality", Desc = "+12 max health per rank" },
	Power = { Max = 5, Name = "Power", Desc = "+8% damage per rank" },
	Reserves = { Max = 5, Name = "Reserves", Desc = "+15 max energy per rank" },
	Aero = { Max = 5, Name = "Aerodynamics", Desc = "+8% flight and glide speed per rank" },
	Agility = { Max = 5, Name = "Agility", Desc = "+6% run speed per rank" },
	Focus = { Max = 5, Name = "Focus", Desc = "+12% ultimate charge per rank" },
}

Config.Shop = {
	{ Id = "Medkit", Name = "Health Pack", Desc = "Heal 60 health now", Price = 30 },
	{ Id = "Charge", Name = "Ultimate Charge", Desc = "+50 ultimate meter", Price = 70 },
	{ Id = "SkillPoint", Name = "Skill Point", Desc = "One extra skill point (price rises)", Price = 250 },
}

-- Wanted level ("threat"): chaos raises it, hunter squads come after you,
-- break line of sight and lie low to shake them.
Config.Heat = {
	Max = 5, DecayDelay = 10, DecayPerSecond = 0.12, NearRange = 110,
	SquadInterval = { 30, 22, 17, 13, 10 }, SquadSize = { 2, 3, 4, 5, 6 },
}

Config.DayLengthSeconds = 720

return Config
