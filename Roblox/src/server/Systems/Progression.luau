--!nonstrict
-- Rewards and growth: XP and cash for kills, missions, courses and shards;
-- level-ups grant skill points; skills (Vitality, Power, Reserves, Aero,
-- Agility, Focus) apply to the hero; the Gear Locker in the Nexus sells
-- health packs, ultimate charge and extra skill points. All server-side;
-- the client only asks.

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))
local MapData = require(Shared:WaitForChild("MapData"))
local Rules = require(Shared:WaitForChild("Progression"))

local Progression = {}
local Net, Profiles, PlayerState

function Progression.effects(player)
	local data = Profiles.get(player)
	return Rules.effects(data and data.Skills)
end

-- Apply skill effects that live on the character (max health).
function Progression.applyToCharacter(player)
	local character = player.Character
	local humanoid = character and character:FindFirstChildOfClass("Humanoid")
	if not humanoid then
		return
	end
	local fx = Progression.effects(player)
	local st = PlayerState.get(player)
	if st then
		st.DamageMul = fx.Damage
		st.MeterMul = fx.Meter
		st.MaxEnergy = Config.Player.MaxEnergy + fx.Energy
	end
	local max = Config.Player.MaxHealth + fx.MaxHealth
	local frac = humanoid.Health / math.max(1, humanoid.MaxHealth)
	humanoid.MaxHealth = max
	humanoid.Health = math.clamp(frac * max, 1, max)
end

function Progression.award(player, xp, cash, reason)
	local data = Profiles.get(player)
	if not data then
		return
	end
	data.Cash = data.Cash + math.max(0, math.floor(cash or 0))
	local levels = Rules.addXp(data, xp or 0, Config)
	Profiles.sync(player)
	if (xp or 0) > 0 or (cash or 0) > 0 then
		Net.fire("Fx", player, { Kind = "Reward", Xp = math.floor(xp or 0), Cash = math.floor(cash or 0), Reason = reason })
	end
	if levels > 0 then
		Net.fire("Notify", player, { Banner = "LEVEL UP! Level " .. data.Level, Text = "+" .. levels .. " skill point" .. (levels > 1 and "s" or "") .. " - press K to spend", Color = Color3.fromRGB(255, 220, 90) })
		Net.fireAll("Fx", { Kind = "Burst", Pos = player.Character and player.Character:GetPivot().Position, Color = Color3.fromRGB(255, 220, 90), Count = 50 })
		local humanoid = player.Character and player.Character:FindFirstChildOfClass("Humanoid")
		if humanoid then
			humanoid.Health = humanoid.MaxHealth
		end
	end
end

local function near(player, pos, range)
	local character = player.Character
	return character and pos and (character:GetPivot().Position - pos).Magnitude < range
end

local function onSkill(player, skill)
	local data = Profiles.get(player)
	if not data or type(skill) ~= "string" then
		return
	end
	if Rules.buySkill(data, skill, Config) then
		Profiles.sync(player)
		Progression.applyToCharacter(player)
		Net.fire("Notify", player, { Text = Config.Skills[skill].Name .. " rank " .. data.Skills[skill], Color = Color3.fromRGB(255, 220, 90), Time = 2 })
	end
end

local function onShop(player, itemId)
	local data = Profiles.get(player)
	local st = PlayerState.get(player)
	if not data or not st or type(itemId) ~= "string" then
		return
	end
	local services = MapData.Services or {}
	if not near(player, services.GearLocker, 30) then
		Net.fire("Notify", player, { Text = "Visit the Gear Locker in the Nexus to shop", Time = 2 })
		return
	end
	local item
	for _, it in ipairs(Config.Shop) do
		if it.Id == itemId then
			item = it
		end
	end
	if not item then
		return
	end
	local price = item.Price
	if item.Id == "SkillPoint" then
		price = Rules.skillPointPrice(data.SkillPointsBought, Config)
	end
	if data.Cash < price then
		Net.fire("Notify", player, { Text = "Not enough cash ($" .. price .. ")", Color = Color3.fromRGB(255, 140, 120), Time = 2 })
		return
	end
	data.Cash = data.Cash - price
	if item.Id == "Medkit" then
		local humanoid = player.Character and player.Character:FindFirstChildOfClass("Humanoid")
		if humanoid then
			humanoid.Health = math.min(humanoid.MaxHealth, humanoid.Health + 60)
		end
	elseif item.Id == "Charge" then
		PlayerState.addMeter(st, 50)
	elseif item.Id == "SkillPoint" then
		data.SkillPoints = data.SkillPoints + 1
		data.SkillPointsBought = data.SkillPointsBought + 1
	end
	Profiles.sync(player)
	Net.fire("Notify", player, { Text = "Bought " .. item.Name, Color = Color3.fromRGB(140, 255, 160), Time = 2 })
	Net.fire("OpenShop", player, { Prices = Progression.prices(player) })
end

function Progression.prices(player)
	local data = Profiles.get(player)
	local prices = {}
	for _, it in ipairs(Config.Shop) do
		prices[it.Id] = it.Id == "SkillPoint" and Rules.skillPointPrice(data and data.SkillPointsBought or 0, Config) or it.Price
	end
	return prices
end

local function servicePrompt(pos, action, object, onTrigger)
	if not pos then
		return
	end
	local anchor = Instance.new("Part")
	anchor.Name = "ServicePrompt_" .. object
	anchor.Anchored = true
	anchor.CanCollide = false
	anchor.CanQuery = false
	anchor.Transparency = 1
	anchor.Size = Vector3.new(4, 4, 4)
	anchor.CFrame = CFrame.new(pos + Vector3.new(0, 3, 0))
	anchor.Parent = workspace:FindFirstChild("Interactables") or workspace
	local prompt = Instance.new("ProximityPrompt")
	prompt.ActionText = action
	prompt.ObjectText = object
	prompt.KeyboardKeyCode = Enum.KeyCode.E
	prompt.HoldDuration = 0
	prompt.MaxActivationDistance = 18
	prompt.RequiresLineOfSight = false
	prompt.Parent = anchor
	prompt.Triggered:Connect(onTrigger)
	local gui = Instance.new("BillboardGui")
	gui.Size = UDim2.fromOffset(200, 40)
	gui.StudsOffset = Vector3.new(0, 7, 0)
	gui.MaxDistance = 200
	gui.Parent = anchor
	local label = Instance.new("TextLabel")
	label.BackgroundTransparency = 1
	label.Size = UDim2.fromScale(1, 1)
	label.Font = Enum.Font.FredokaOne
	label.TextScaled = true
	label.TextColor3 = Color3.fromRGB(255, 230, 120)
	label.TextStrokeTransparency = 0.2
	label.Text = object
	label.Parent = gui
end

function Progression.init(systems)
	Net, Profiles, PlayerState = systems.Net, systems.Profiles, systems.Players
	Net.on("Skill", 5, onSkill)
	Net.on("Shop", 4, onShop)
end

function Progression.start()
	local services = MapData.Services or {}
	servicePrompt(services.GearLocker, "Shop", "Gear Locker", function(player)
		Net.fire("OpenShop", player, { Prices = Progression.prices(player) })
	end)
	servicePrompt(services.UpgradeBench, "Skills", "Upgrade Bench", function(player)
		Net.fire("OpenShop", player, { Skills = true })
	end)
	Profiles.onLoaded(function(player)
		Progression.applyToCharacter(player)
	end)
	-- (Heroes applies the bonus at the end of its own character setup.)
end

return Progression
