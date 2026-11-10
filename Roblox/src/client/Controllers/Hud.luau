--!nonstrict
-- On-screen UI, built in code: hero card with health / energy / ultimate,
-- world info (shards, KOs, course), messages and banners, hit-combo counter,
-- crosshair, the dimensional-terminal hero menu, Ben's Omnitrix dial, and
-- the controls overlay (F1).

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local TweenService = game:GetService("TweenService")
local UserInputService = game:GetService("UserInputService")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))
local MapData = require(Shared:WaitForChild("MapData"))
local Rules = require(Shared:WaitForChild("Progression"))

local Hud = {}
local player = Players.LocalPlayer
local Input, CameraRig, Movement, Minimap
local gui, card, bars, info, msgFrame, banner, combo, menu, dial, help
local stats, stars, bossBar, missionPanel, prompts, skillsMenu, shopMenu, rewardFeed
local mission = nil
local dialIsOpen = false
local comboCount, comboTimer = 0, 0
local FONT = Enum.Font.FredokaOne

local function new(class, props, parent)
	local i = Instance.new(class)
	for k, v in pairs(props) do
		i[k] = v
	end
	i.Parent = parent
	return i
end

local function corner(parent, r)
	new("UICorner", { CornerRadius = UDim.new(r or 0.2, 0) }, parent)
end

local function label(parent, text, size, pos, color, xAlign)
	return new("TextLabel", {
		BackgroundTransparency = 1, Font = FONT, Text = text, TextColor3 = color or Color3.new(1, 1, 1),
		TextStrokeTransparency = 0.4, TextScaled = true, Size = size, Position = pos,
		TextXAlignment = xAlign or Enum.TextXAlignment.Left,
	}, parent)
end

local function bar(parent, y, color, title)
	local back = new("Frame", { BackgroundColor3 = Color3.fromRGB(20, 20, 30), BackgroundTransparency = 0.3, Size = UDim2.new(1, -90, 0, 14), Position = UDim2.new(0, 84, 0, y), BorderSizePixel = 0 }, parent)
	corner(back, 0.5)
	local fill = new("Frame", { BackgroundColor3 = color, Size = UDim2.fromScale(1, 1), BorderSizePixel = 0 }, back)
	corner(fill, 0.5)
	local text = label(back, title, UDim2.new(1, -8, 1, 0), UDim2.fromOffset(6, 0), Color3.new(1, 1, 1))
	text.ZIndex = 3
	return { Fill = fill, Text = text, Title = title }
end

local function setBar(b, frac, suffix)
	b.Fill.Size = UDim2.fromScale(math.clamp(frac, 0, 1), 1)
	b.Text.Text = b.Title .. (suffix or "")
end

function Hud.message(text, color, seconds)
	if not msgFrame or not text or text == "" then
		return
	end
	local l = label(msgFrame, text, UDim2.new(1, 0, 0, 26), UDim2.new(0, 0, 1, 0), color or Color3.new(1, 1, 1), Enum.TextXAlignment.Center)
	l.LayoutOrder = math.floor(os.clock() * 100)
	task.delay(seconds or 3.5, function()
		TweenService:Create(l, TweenInfo.new(0.4), { TextTransparency = 1, TextStrokeTransparency = 1 }):Play()
		task.wait(0.45)
		l:Destroy()
	end)
	local children = msgFrame:GetChildren()
	if #children > 7 then
		for _, c in ipairs(children) do
			if c:IsA("TextLabel") then
				c:Destroy()
				break
			end
		end
	end
end

function Hud.banner(text, color)
	banner.Text = text
	banner.TextColor3 = color or Color3.fromRGB(255, 230, 120)
	banner.TextTransparency = 0
	banner.TextStrokeTransparency = 0.2
	local stamp = os.clock()
	banner:SetAttribute("Stamp", stamp)
	task.delay(2.5, function()
		if banner:GetAttribute("Stamp") == stamp then
			TweenService:Create(banner, TweenInfo.new(0.5), { TextTransparency = 1, TextStrokeTransparency = 1 }):Play()
		end
	end)
end

function Hud.addHit()
	comboCount = comboCount + 1
	comboTimer = 2
end

function Hud.dialOpen()
	return dialIsOpen
end

function Hud.setDial(open)
	dialIsOpen = open
	dial.Visible = open
end

local function closeMenu()
	menu.Visible = false
	Input.setEnabled(true)
	CameraRig.setCursorFree(false)
end

local function openMenu(t)
	for _, c in ipairs(menu.Grid:GetChildren()) do
		if c:IsA("TextButton") then
			c:Destroy()
		end
	end
	menu.Title.Text = (t.Universe or "Dimensional") .. " Terminal - pick a hero (1-8)"
	for i, id in ipairs(Config.HeroOrder) do
		local h = Config.Heroes[id]
		local b = new("TextButton", {
			Name = id, LayoutOrder = i, BackgroundColor3 = h.Accent, AutoButtonColor = true, Text = "", BorderSizePixel = 0,
		}, menu.Grid)
		corner(b, 0.12)
		local stroke = new("UIStroke", { Thickness = id == t.Hero and 4 or 2, Color = id == t.Hero and Color3.new(1, 1, 1) or Color3.fromRGB(20, 20, 30) }, b)
		stroke.ApplyStrokeMode = Enum.ApplyStrokeMode.Border
		local portrait = new("Frame", { BackgroundColor3 = h.Look.Skin, Size = UDim2.fromScale(0.5, 0.5), Position = UDim2.fromScale(0.25, 0.08), SizeConstraint = Enum.SizeConstraint.RelativeYY, BorderSizePixel = 0 }, b)
		corner(portrait, 0.5)
		new("UIAspectRatioConstraint", {}, portrait)
		if h.Portrait then
			new("ImageLabel", { BackgroundTransparency = 1, Size = UDim2.fromScale(1, 1), Image = h.Portrait }, portrait)
		else
			label(portrait, id:sub(1, 1), UDim2.fromScale(1, 1), UDim2.new(), Color3.fromRGB(30, 30, 40), Enum.TextXAlignment.Center)
		end
		label(b, i .. "  " .. h.Name, UDim2.new(0.9, 0, 0.18, 0), UDim2.fromScale(0.05, 0.62), Color3.new(1, 1, 1), Enum.TextXAlignment.Center)
		label(b, h.Universe, UDim2.new(0.9, 0, 0.12, 0), UDim2.fromScale(0.05, 0.82), Color3.fromRGB(30, 30, 40), Enum.TextXAlignment.Center).TextStrokeTransparency = 1
		b.Activated:Connect(function()
			ReplicatedStorage.Remotes.SelectHero:FireServer(id)
			closeMenu()
		end)
	end
	menu.Travel.Visible = t.Travel ~= nil
	menu.Travel.Text = t.Travel and ("Travel to " .. (MapData.Levels[t.Travel] and MapData.Levels[t.Travel].Name or t.Travel) .. "  (T)") or ""
	menu:SetAttribute("Travel", t.Travel)
	menu.Visible = true
	Input.setEnabled(false)
	CameraRig.setCursorFree(true)
end

local function build()
	gui = new("ScreenGui", { Name = "MultiverseHud", ResetOnSpawn = false, IgnoreGuiInset = true, ZIndexBehavior = Enum.ZIndexBehavior.Sibling }, player:WaitForChild("PlayerGui"))
	local scale = new("UIScale", {}, gui)
	local function rescale()
		local vp = workspace.CurrentCamera.ViewportSize
		scale.Scale = math.clamp(vp.Y / 900, 0.6, 1.4)
	end
	rescale()
	workspace.CurrentCamera:GetPropertyChangedSignal("ViewportSize"):Connect(rescale)

	-- Hero card.
	card = new("Frame", { BackgroundColor3 = Color3.fromRGB(15, 15, 25), BackgroundTransparency = 0.35, Size = UDim2.fromOffset(330, 100), Position = UDim2.fromOffset(16, 16), BorderSizePixel = 0 }, gui)
	corner(card, 0.15)
	local portrait = new("Frame", { Name = "Portrait", Size = UDim2.fromOffset(68, 68), Position = UDim2.fromOffset(8, 16), BorderSizePixel = 0 }, card)
	corner(portrait, 0.5)
	new("UIStroke", { Thickness = 3, Color = Color3.new(1, 1, 1) }, portrait)
	local initial = label(portrait, "J", UDim2.fromScale(1, 1), UDim2.new(), Color3.fromRGB(30, 30, 40), Enum.TextXAlignment.Center)
	initial.Name = "Initial"
	local name = label(card, "Jimmy Neutron", UDim2.new(1, -90, 0, 24), UDim2.fromOffset(84, 6))
	name.Name = "HeroName"
	local trav = label(card, "", UDim2.new(1, -90, 0, 14), UDim2.fromOffset(84, 30), Color3.fromRGB(200, 200, 220))
	trav.Name = "Traversal"
	bars = {
		Health = bar(card, 48, Color3.fromRGB(235, 70, 70), "HP"),
		Energy = bar(card, 64, Color3.fromRGB(90, 220, 140), "ENERGY"),
		Meter = bar(card, 80, Color3.fromRGB(110, 120, 240), "ULTIMATE"),
	}

	-- World info.
	info = new("Frame", { BackgroundTransparency = 1, Size = UDim2.fromOffset(360, 120), Position = UDim2.new(1, -376, 0, 16) }, gui)
	new("UIListLayout", { HorizontalAlignment = Enum.HorizontalAlignment.Right, SortOrder = Enum.SortOrder.LayoutOrder }, info)
	for i, key in ipairs({ "Level", "Speed", "Shards", "Kills", "Course" }) do
		local l = label(info, "", UDim2.new(1, 0, 0, i == 1 and 26 or 18), UDim2.new(), i == 1 and Color3.new(1, 1, 1) or Color3.fromRGB(220, 220, 235), Enum.TextXAlignment.Right)
		l.Name = key
		l.LayoutOrder = i
	end

	-- Messages, banner, combo, crosshair.
	msgFrame = new("Frame", { BackgroundTransparency = 1, Size = UDim2.new(0.6, 0, 0, 200), Position = UDim2.new(0.2, 0, 1, -330) }, gui)
	new("UIListLayout", { VerticalAlignment = Enum.VerticalAlignment.Bottom, SortOrder = Enum.SortOrder.LayoutOrder }, msgFrame)
	banner = label(gui, "", UDim2.new(0.8, 0, 0, 64), UDim2.new(0.1, 0, 0.18, 0), Color3.fromRGB(255, 230, 120), Enum.TextXAlignment.Center)
	banner.TextTransparency = 1
	banner.TextStrokeTransparency = 1
	combo = label(gui, "", UDim2.fromOffset(260, 40), UDim2.new(0.5, 140, 0.5, -120), Color3.fromRGB(255, 220, 90))
	local cross = new("Frame", { Size = UDim2.fromOffset(6, 6), Position = UDim2.new(0.5, -3, 0.5, -3), BackgroundColor3 = Color3.new(1, 1, 1), BorderSizePixel = 0 }, gui)
	corner(cross, 0.5)
	new("UIStroke", { Thickness = 1.5, Color = Color3.new(0, 0, 0) }, cross)

	-- Terminal menu.
	menu = new("Frame", { Name = "TerminalMenu", Visible = false, BackgroundColor3 = Color3.fromRGB(15, 15, 30), BackgroundTransparency = 0.15, Size = UDim2.fromOffset(820, 520), Position = UDim2.new(0.5, -410, 0.5, -260), BorderSizePixel = 0 }, gui)
	corner(menu, 0.05)
	local title = label(menu, "", UDim2.new(1, -40, 0, 36), UDim2.fromOffset(20, 12), Color3.fromRGB(255, 230, 120), Enum.TextXAlignment.Center)
	title.Name = "Title"
	local grid = new("Frame", { Name = "Grid", BackgroundTransparency = 1, Size = UDim2.new(1, -40, 0, 380), Position = UDim2.fromOffset(20, 56) }, menu)
	new("UIGridLayout", { CellSize = UDim2.fromOffset(180, 180), CellPadding = UDim2.fromOffset(12, 12), SortOrder = Enum.SortOrder.LayoutOrder, HorizontalAlignment = Enum.HorizontalAlignment.Center }, grid)
	local travel = new("TextButton", { Name = "Travel", BackgroundColor3 = Color3.fromRGB(255, 200, 80), Font = FONT, TextScaled = true, Size = UDim2.fromOffset(360, 36), Position = UDim2.new(0.5, -270, 1, -50), BorderSizePixel = 0 }, menu)
	corner(travel, 0.3)
	travel.Activated:Connect(function()
		local target = menu:GetAttribute("Travel")
		if target then
			ReplicatedStorage.Remotes.Travel:FireServer(target)
		end
		closeMenu()
	end)
	local close = new("TextButton", { Name = "Close", Text = "Close (Esc)", BackgroundColor3 = Color3.fromRGB(80, 80, 100), TextColor3 = Color3.new(1, 1, 1), Font = FONT, TextScaled = true, Size = UDim2.fromOffset(160, 36), Position = UDim2.new(0.5, 110, 1, -50), BorderSizePixel = 0 }, menu)
	corner(close, 0.3)
	close.Activated:Connect(closeMenu)

	-- Omnitrix dial.
	dial = new("Frame", { Name = "OmnitrixDial", Visible = false, BackgroundColor3 = Color3.fromRGB(10, 30, 10), BackgroundTransparency = 0.25, Size = UDim2.fromOffset(420, 420), Position = UDim2.new(0.5, -210, 0.5, -210), BorderSizePixel = 0 }, gui)
	corner(dial, 0.5)
	new("UIStroke", { Thickness = 4, Color = Color3.fromRGB(90, 255, 90) }, dial)
	for k = 1, 3 do
		local f = Config.Forms[k]
		local a = math.rad(-90 + (k - 1) * 120)
		local b = new("TextButton", { Text = k .. "\n" .. f.Id, Font = FONT, TextScaled = true, TextColor3 = Color3.new(1, 1, 1), BackgroundColor3 = f.Body, Size = UDim2.fromOffset(110, 110), Position = UDim2.new(0.5, math.cos(a) * 130 - 55, 0.5, math.sin(a) * 130 - 55), BorderSizePixel = 0 }, dial)
		corner(b, 0.5)
		b.Activated:Connect(function()
			ReplicatedStorage.Remotes.Omnitrix:FireServer("slot", k - 1)
			Hud.setDial(false)
		end)
	end
	label(dial, "OMNITRIX: 1/2/3 to transform, Q to cancel", UDim2.new(1, -60, 0, 24), UDim2.new(0, 30, 1, -40), Color3.fromRGB(200, 255, 200), Enum.TextXAlignment.Center)

	-- Controls overlay.
	help = new("Frame", { BackgroundColor3 = Color3.new(0, 0, 0), BackgroundTransparency = 0.4, Size = UDim2.fromOffset(840, 228), Position = UDim2.new(0.5, -420, 1, -430), BorderSizePixel = 0 }, gui)
	corner(help, 0.06)
	local lines = {
		"CONTROLS  (F1 to toggle)",
		"WASD move   Mouse look   Space jump / ascend (jump at walls to wall-jump)   Ctrl or C descend   Shift sprint / boost",
		"Click attack (3-hit combos)   Q signature   R Ghost Wail / Earth Pillar   F ultimate   Tab fly (flyers)",
		"V or Alt dodge / air dash / barrel roll   Ctrl in the air: ground pound   B emote   Hold Space falling: glide (Aang)",
		"E terminals (swap hero / travel) and Jimmy's hover car   P return to the Nexus   M (or Esc) free the mouse",
		"Ben: 1/2/3 transform, Q Omnitrix dial, G Master Control Glitch, F Way Big (full meter)",
		"Right click / X counter (on a red !)   T takedown (8+ combo)   Z lock-on   K skills   Shift: wall-run, climb, slide (+Ctrl)",
	}
	for i, text in ipairs(lines) do
		label(help, text, UDim2.new(1, -24, 0, 22), UDim2.fromOffset(12, 8 + (i - 1) * 30), i == 1 and Color3.fromRGB(255, 230, 120) or Color3.new(1, 1, 1))
	end
	task.delay(14, function()
		help.Visible = false
	end)
end

-- Cash, level, XP, wanted stars, boss bar, mission timer.
local function refreshExtras()
	local level = player:GetAttribute("Level") or 1
	local xp = player:GetAttribute("Xp") or 0
	local need = Rules.xpToNext(level, Config)
	stats.Level.Text = ("LV %d"):format(level)
	stats.Cash.Text = ("$%d"):format(player:GetAttribute("Cash") or 0)
	local points = player:GetAttribute("SkillPoints") or 0
	stats.Points.Text = points > 0 and ("%d skill point%s (K)"):format(points, points > 1 and "s" or "") or ""
	stats.XpFill.Size = UDim2.fromScale(math.clamp(xp / need, 0, 1), 1)
	local heat = player:GetAttribute("Heat") or 0
	local cooling = player:GetAttribute("HeatCooling")
	local n = math.floor(heat)
	local blink = cooling and (os.clock() % 0.6 < 0.3)
	for i, star in ipairs(stars) do
		star.TextColor3 = i <= n and Color3.fromRGB(255, 215, 60) or Color3.fromRGB(70, 70, 80)
		star.TextTransparency = (i <= n and blink) and 0.7 or (n > 0 and 0 or 1)
		star.TextStrokeTransparency = n > 0 and 0.3 or 1
	end
	local bossOn = workspace:GetAttribute("BossActive")
	bossBar.Visible = bossOn == true
	if bossOn then
		local hp, max = workspace:GetAttribute("BossHealth") or 0, workspace:GetAttribute("BossMax") or 1
		bossBar.Fill.Size = UDim2.fromScale(math.clamp(hp / max, 0, 1), 1)
		bossBar.Title.Text = (workspace:GetAttribute("BossName") or "Boss") .. ("  %d / %d"):format(hp, max)
	end
	if mission then
		local left = math.max(0, mission.Ends - workspace:GetServerTimeNow())
		missionPanel.Visible = true
		missionPanel.Title.Text = ("%s   %d:%02d"):format(mission.Name, math.floor(left / 60), math.floor(left % 60))
		missionPanel.Body.Text = mission.Text or ""
	else
		missionPanel.Visible = false
	end
end

function Hud.setPrompts(threat, takedown)
	if not prompts then
		return
	end
	prompts.Counter.Visible = threat
	prompts.Counter.TextTransparency = (os.clock() % 0.25 < 0.12) and 0 or 0.25
	prompts.Takedown.Visible = takedown
end

function Hud.reward(xp, cash, reason)
	if not rewardFeed then
		return
	end
	local parts = {}
	if xp and xp > 0 then
		table.insert(parts, "+" .. xp .. " XP")
	end
	if cash and cash > 0 then
		table.insert(parts, "+$" .. cash)
	end
	if #parts == 0 then
		return
	end
	local l = label(rewardFeed, table.concat(parts, "   ") .. (reason and ("  " .. reason) or ""), UDim2.new(1, 0, 0, 20), UDim2.new(), Color3.fromRGB(255, 225, 120), Enum.TextXAlignment.Right)
	l.LayoutOrder = math.floor(os.clock() * 100)
	task.delay(2.5, function()
		TweenService:Create(l, TweenInfo.new(0.4), { TextTransparency = 1, TextStrokeTransparency = 1 }):Play()
		task.wait(0.45)
		l:Destroy()
	end)
end

function Hud.setMission(m)
	mission = m
	if Minimap then
		Minimap.setWaypoint(m and m.Pos or nil)
	end
end

local function menuFrame(name, title, height)
	local f = new("Frame", { Name = name, Visible = false, BackgroundColor3 = Color3.fromRGB(15, 15, 30), BackgroundTransparency = 0.12, Size = UDim2.fromOffset(520, height), Position = UDim2.new(0.5, -260, 0.5, -height / 2), BorderSizePixel = 0 }, gui)
	corner(f, 0.05)
	label(f, title, UDim2.new(1, -40, 0, 34), UDim2.fromOffset(20, 10), Color3.fromRGB(255, 230, 120), Enum.TextXAlignment.Center)
	local list = new("Frame", { Name = "List", BackgroundTransparency = 1, Size = UDim2.new(1, -40, 1, -110), Position = UDim2.fromOffset(20, 52) }, f)
	new("UIListLayout", { Padding = UDim.new(0, 6), SortOrder = Enum.SortOrder.LayoutOrder }, list)
	local close = new("TextButton", { Text = "Close", BackgroundColor3 = Color3.fromRGB(80, 80, 100), TextColor3 = Color3.new(1, 1, 1), Font = FONT, TextScaled = true, Size = UDim2.fromOffset(140, 32), Position = UDim2.new(0.5, -70, 1, -44), BorderSizePixel = 0 }, f)
	corner(close, 0.3)
	close.Activated:Connect(function()
		f.Visible = false
		Input.setEnabled(true)
		CameraRig.setCursorFree(false)
	end)
	return f
end

local function row(list, order, text, button, onClick)
	local r = new("Frame", { BackgroundColor3 = Color3.fromRGB(40, 40, 60), BackgroundTransparency = 0.2, Size = UDim2.new(1, 0, 0, 44), LayoutOrder = order, BorderSizePixel = 0 }, list)
	corner(r, 0.2)
	label(r, text, UDim2.new(1, -150, 1, -8), UDim2.fromOffset(10, 4))
	local b = new("TextButton", { Text = button, Font = FONT, TextScaled = true, BackgroundColor3 = Color3.fromRGB(255, 200, 80), Size = UDim2.fromOffset(120, 32), Position = UDim2.new(1, -128, 0, 6), BorderSizePixel = 0 }, r)
	corner(b, 0.3)
	b.Activated:Connect(onClick)
	return r
end

local function openSkills()
	for _, c in ipairs(skillsMenu.List:GetChildren()) do
		if c:IsA("Frame") then
			c:Destroy()
		end
	end
	local points = player:GetAttribute("SkillPoints") or 0
	skillsMenu:FindFirstChildOfClass("TextLabel").Text = ("SKILLS  -  %d point%s to spend"):format(points, points == 1 and "" or "s")
	for i, id in ipairs(Config.SkillOrder) do
		local def = Config.Skills[id]
		local rank = player:GetAttribute("Skill_" .. id) or 0
		row(skillsMenu.List, i, ("%s  %d/%d\n%s"):format(def.Name, rank, def.Max, def.Desc), rank >= def.Max and "MAX" or "+1", function()
			ReplicatedStorage.Remotes.Skill:FireServer(id)
			task.delay(0.3, openSkills)
		end)
	end
	skillsMenu.Visible = true
	Input.setEnabled(false)
	CameraRig.setCursorFree(true)
end

local function openShop(prices)
	for _, c in ipairs(shopMenu.List:GetChildren()) do
		if c:IsA("Frame") then
			c:Destroy()
		end
	end
	for i, item in ipairs(Config.Shop) do
		local price = prices and prices[item.Id] or item.Price
		row(shopMenu.List, i, ("%s  -  $%d\n%s"):format(item.Name, price, item.Desc), "Buy", function()
			ReplicatedStorage.Remotes.Shop:FireServer(item.Id)
		end)
	end
	shopMenu.Visible = true
	Input.setEnabled(false)
	CameraRig.setCursorFree(true)
end

local function buildExtras()
	-- Level / XP / cash under the hero card.
	stats = new("Frame", { BackgroundColor3 = Color3.fromRGB(15, 15, 25), BackgroundTransparency = 0.35, Size = UDim2.fromOffset(330, 46), Position = UDim2.fromOffset(16, 122), BorderSizePixel = 0 }, gui)
	corner(stats, 0.2)
	local lv = label(stats, "LV 1", UDim2.fromOffset(70, 24), UDim2.fromOffset(10, 4), Color3.fromRGB(255, 230, 120))
	lv.Name = "Level"
	local cash = label(stats, "$0", UDim2.fromOffset(110, 24), UDim2.fromOffset(84, 4), Color3.fromRGB(140, 255, 160))
	cash.Name = "Cash"
	local pts = label(stats, "", UDim2.fromOffset(140, 18), UDim2.fromOffset(184, 7), Color3.fromRGB(255, 200, 90))
	pts.Name = "Points"
	local xpBack = new("Frame", { BackgroundColor3 = Color3.fromRGB(30, 30, 45), Size = UDim2.new(1, -20, 0, 8), Position = UDim2.fromOffset(10, 32), BorderSizePixel = 0 }, stats)
	corner(xpBack, 0.5)
	stats.XpFill = new("Frame", { BackgroundColor3 = Color3.fromRGB(255, 210, 80), Size = UDim2.fromScale(0, 1), BorderSizePixel = 0 }, xpBack)
	corner(stats.XpFill, 0.5)
	-- Wanted stars, top centre.
	local starRow = new("Frame", { BackgroundTransparency = 1, Size = UDim2.fromOffset(200, 34), Position = UDim2.new(0.5, -100, 0, 12) }, gui)
	new("UIListLayout", { FillDirection = Enum.FillDirection.Horizontal, HorizontalAlignment = Enum.HorizontalAlignment.Center }, starRow)
	stars = {}
	for i = 1, Config.Heat.Max do
		local s = label(starRow, "★", UDim2.fromOffset(34, 34), UDim2.new(), Color3.fromRGB(70, 70, 80), Enum.TextXAlignment.Center)
		s.LayoutOrder = i
		stars[i] = s
	end
	-- Boss bar.
	bossBar = new("Frame", { Visible = false, BackgroundColor3 = Color3.fromRGB(20, 10, 30), BackgroundTransparency = 0.2, Size = UDim2.new(0.5, 0, 0, 22), Position = UDim2.new(0.25, 0, 0, 56), BorderSizePixel = 0 }, gui)
	corner(bossBar, 0.4)
	bossBar.Fill = new("Frame", { BackgroundColor3 = Color3.fromRGB(230, 60, 180), Size = UDim2.fromScale(1, 1), BorderSizePixel = 0 }, bossBar)
	corner(bossBar.Fill, 0.4)
	bossBar.Title = label(bossBar, "", UDim2.new(1, -10, 1, 0), UDim2.fromOffset(5, 0), Color3.new(1, 1, 1), Enum.TextXAlignment.Center)
	bossBar.Title.ZIndex = 3
	-- Mission panel.
	missionPanel = new("Frame", { Visible = false, BackgroundColor3 = Color3.fromRGB(15, 15, 25), BackgroundTransparency = 0.3, Size = UDim2.fromOffset(330, 62), Position = UDim2.fromOffset(16, 176), BorderSizePixel = 0 }, gui)
	corner(missionPanel, 0.15)
	missionPanel.Title = label(missionPanel, "", UDim2.new(1, -16, 0, 26), UDim2.fromOffset(8, 4), Color3.fromRGB(255, 220, 90))
	missionPanel.Body = label(missionPanel, "", UDim2.new(1, -16, 0, 22), UDim2.fromOffset(8, 34), Color3.new(1, 1, 1))
	-- Combat prompts.
	prompts = {}
	prompts.Counter = label(gui, "COUNTER! (Right click / X)", UDim2.fromOffset(360, 40), UDim2.new(0.5, -180, 0.5, 60), Color3.fromRGB(255, 80, 80), Enum.TextXAlignment.Center)
	prompts.Counter.Visible = false
	prompts.Takedown = label(gui, "TAKEDOWN READY (T)", UDim2.fromOffset(300, 30), UDim2.new(0.5, -150, 0.5, 104), Color3.fromRGB(255, 220, 90), Enum.TextXAlignment.Center)
	prompts.Takedown.Visible = false
	-- Reward feed (right side, under world info).
	rewardFeed = new("Frame", { BackgroundTransparency = 1, Size = UDim2.fromOffset(320, 140), Position = UDim2.new(1, -336, 0, 140) }, gui)
	new("UIListLayout", { SortOrder = Enum.SortOrder.LayoutOrder, HorizontalAlignment = Enum.HorizontalAlignment.Right }, rewardFeed)
	skillsMenu = menuFrame("SkillsMenu", "SKILLS", 420)
	shopMenu = menuFrame("ShopMenu", "GEAR LOCKER", 300)
end

local function refresh(dt)
	local character = player.Character
	local humanoid = character and character:FindFirstChildOfClass("Humanoid")
	if not humanoid then
		return
	end
	local heroId = character:GetAttribute("Hero") or "Jimmy"
	local h = Config.Heroes[heroId] or Config.Heroes.Jimmy
	local form = character:GetAttribute("Form")
	card.HeroName.Text = form and ("Ben as " .. form) or h.Name
	card.Traversal.Text = h.Universe .. "  |  " .. h.Traversal
	card.Portrait.BackgroundColor3 = h.Accent
	card.Portrait.Initial.Text = heroId:sub(1, 1)
	setBar(bars.Health, humanoid.Health / humanoid.MaxHealth, ("  %d"):format(humanoid.Health))
	local S = Movement.state()
	setBar(bars.Energy, S.Energy / Movement.maxEnergy())
	local meter = character:GetAttribute("Meter") or 0
	setBar(bars.Meter, meter / 100, meter >= 100 and "  READY (F)" or ("  %d%%"):format(meter))
	if heroId == "Ben" then
		local state = character:GetAttribute("Omnitrix") or "Human"
		local t = character:GetAttribute("AlienTime") or 0
		card.Traversal.Text = "Omnitrix: " .. state .. ((state == "Alien" and t > 0) and ("  " .. t .. "s") or "")
	end

	local levelName = player:GetAttribute("Level") or Config.Levels.Nexus
	local level = MapData.Levels[levelName]
	info.Level.Text = level and level.Name or levelName
	local root = character:FindFirstChild("HumanoidRootPart")
	local speed = root and root.AssemblyLinearVelocity.Magnitude or 0
	info.Speed.Text = ("%d km/h"):format(speed * 0.3 * 3.6)
	info.Shards.Text = ("Rift shards: %d/%d"):format(player:GetAttribute("Shards_" .. levelName) or 0, workspace:GetAttribute("ShardTotal_" .. levelName) or 0)
	info.Kills.Text = ("KOs: %d"):format(player:GetAttribute("Kills") or 0)

	local c = player:GetAttribute("Combo") or 0
	local age = workspace:GetServerTimeNow() - (player:GetAttribute("ComboAt") or 0)
	local fade = math.clamp((Config.Combat.ComboTimeout - age) / 0.5, 0, 1)
	combo.Text = (c >= 2 and fade > 0) and (c .. " HIT COMBO!") or ""
	combo.TextTransparency = 1 - fade
	refreshExtras()
end

function Hud.setCourse(text)
	if info then
		info.Course.Text = text or ""
	end
end

function Hud.init(controllers)
	Input, CameraRig, Movement, Minimap = controllers.Input, controllers.CameraRig, controllers.Movement, controllers.Minimap
	build()
	buildExtras()
	Input.on("Skills", function(down)
		if down then
			if skillsMenu.Visible then
				skillsMenu.Visible = false
				Input.setEnabled(true)
				CameraRig.setCursorFree(false)
			else
				openSkills()
			end
		end
	end)
	Input.on("Help", function(down)
		if down then
			help.Visible = not help.Visible
		end
	end)
end

function Hud.start()
	local remotes = ReplicatedStorage:WaitForChild("Remotes")
	remotes.Notify.OnClientEvent:Connect(function(n)
		if type(n) ~= "table" then
			return
		end
		if n.Banner then
			Hud.banner(n.Banner, n.Color)
		end
		if n.Text then
			Hud.message(n.Text, n.Color, n.Time)
		end
	end)
	remotes.OpenTerminal.OnClientEvent:Connect(openMenu)
	remotes.OpenShop.OnClientEvent:Connect(function(info)
		if type(info) ~= "table" then
			return
		end
		if info.Skills then
			openSkills()
		else
			openShop(info.Prices)
		end
	end)
	remotes.Objective.OnClientEvent:Connect(function(o)
		if type(o) ~= "table" or o.Clear then
			Hud.setMission(nil)
		else
			Hud.setMission(o)
		end
	end)
	UserInputService.InputBegan:Connect(function(input)
		if not menu.Visible then
			return
		end
		if input.KeyCode == Enum.KeyCode.Escape then
			closeMenu()
		elseif input.KeyCode == Enum.KeyCode.T and menu:GetAttribute("Travel") then
			remotes.Travel:FireServer(menu:GetAttribute("Travel"))
			closeMenu()
		else
			local numbers = { Enum.KeyCode.One, Enum.KeyCode.Two, Enum.KeyCode.Three, Enum.KeyCode.Four, Enum.KeyCode.Five, Enum.KeyCode.Six, Enum.KeyCode.Seven, Enum.KeyCode.Eight }
			for i, key in ipairs(numbers) do
				if input.KeyCode == key then
					remotes.SelectHero:FireServer(Config.HeroOrder[i])
					closeMenu()
				end
			end
		end
	end)
	RunService.RenderStepped:Connect(refresh)
	Hud.message("Welcome to the Nexus Observatory! Walk up to a terminal and press E to pick a hero.", Color3.fromRGB(255, 230, 120), 8)
end

return Hud
