--!nonstrict
-- Dresses a player's R15 character as a hero (or one of Ben's aliens), built
-- from parts so no uploaded assets are needed: body colours, cartoon
-- proportions (big round toon head), a face drawn with GUI frames, and each
-- hero's gear (hair, hats, props, Jake's dragon, Danny's ghost tail, Aang's
-- glider...). Gear that only shows in some states carries a "ShowWhen"
-- attribute that Client/Controllers/Animator toggles.

local ReplicatedStorage = game:GetService("ReplicatedStorage")
local Config = require(ReplicatedStorage:WaitForChild("Shared"):WaitForChild("Config"))

local Looks = {}

local GEAR = "HeroGear"
local WHITE = Color3.fromRGB(245, 245, 250)
local BLACK = Color3.fromRGB(30, 30, 36)

local R15_PARTS = {
	"Head", "UpperTorso", "LowerTorso", "LeftUpperArm", "LeftLowerArm", "LeftHand", "RightUpperArm",
	"RightLowerArm", "RightHand", "LeftUpperLeg", "LeftLowerLeg", "LeftFoot", "RightUpperLeg", "RightLowerLeg", "RightFoot",
}

-- Part factory ---------------------------------------------------------------

local function newPart(folder, name, size, color, material)
	local p = Instance.new("Part")
	p.Name = name
	p.Size = size
	p.Color = color
	p.Material = material or Enum.Material.SmoothPlastic
	p.TopSurface = Enum.SurfaceType.Smooth
	p.BottomSurface = Enum.SurfaceType.Smooth
	p.CanCollide = false
	p.CanQuery = false
	p.CanTouch = false
	p.Massless = true
	p.CastShadow = true
	p.Parent = folder
	return p
end

local function weld(p, to, c0)
	local w = Instance.new("Weld")
	w.Name = "GearWeld"
	w.Part0 = to
	w.Part1 = p
	w.C0 = c0
	w.Parent = p
	return w
end

-- A joint the client animates (wings, extra arms, staff).
local function motor(p, to, c0, name)
	local m = Instance.new("Motor6D")
	m.Name = name
	m.Part0 = to
	m.Part1 = p
	m.C0 = c0
	m.Parent = to
	return m
end

local function ball(folder, to, c0, size, color, neon)
	local p = newPart(folder, "Ball", size, color, neon and Enum.Material.Neon or nil)
	local mesh = Instance.new("SpecialMesh")
	mesh.MeshType = Enum.MeshType.Sphere
	mesh.Parent = p
	weld(p, to, c0)
	return p
end

local function box(folder, to, c0, size, color, neon)
	local p = newPart(folder, "Box", size, color, neon and Enum.Material.Neon or nil)
	weld(p, to, c0)
	return p
end

-- Vertical cylinder centred on c0 (Roblox cylinders run along X).
local function cyl(folder, to, c0, height, diameter, color, neon)
	local p = newPart(folder, "Cyl", Vector3.new(height, diameter, diameter), color, neon and Enum.Material.Neon or nil)
	p.Shape = Enum.PartType.Cylinder
	weld(p, to, c0 * CFrame.Angles(0, 0, math.pi / 2))
	return p
end

-- Cone pointing along c0's +Y from its base: stacked, tapering cylinders.
local function cone(folder, to, c0, height, radius, color, neon)
	local tiers = 3
	for i = 0, tiers - 1 do
		local r = radius * (1 - i / tiers)
		local h = height / tiers
		cyl(folder, to, c0 * CFrame.new(0, h * (i + 0.5), 0), h, r * 2, color, neon)
	end
	ball(folder, to, c0 * CFrame.new(0, height, 0), Vector3.new(radius * 0.5, radius * 0.5, radius * 0.5), color, neon)
end

local function showWhen(folder, condition)
	for _, d in ipairs(folder:GetChildren()) do
		if d:IsA("BasePart") and d:GetAttribute("ShowWhen") == nil then
			d:SetAttribute("ShowWhen", condition)
		end
	end
end

-- Face -----------------------------------------------------------------------

local function frame(parent, name, pos, size, color, round, z)
	local f = Instance.new("Frame")
	f.Name = name
	f.AnchorPoint = Vector2.new(0.5, 0.5)
	f.Position = UDim2.fromScale(pos.X, pos.Y)
	f.Size = UDim2.fromScale(size.X, size.Y)
	f.BackgroundColor3 = color
	f.BorderSizePixel = 0
	f.ZIndex = z or 2
	if round then
		local c = Instance.new("UICorner")
		c.CornerRadius = UDim.new(round, 0)
		c.Parent = f
	end
	f.Parent = parent
	return f
end

local function buildFace(head, look, faceStyle)
	local gui = Instance.new("SurfaceGui")
	gui.Name = "Face"
	gui.Face = Enum.NormalId.Front
	gui.SizingMode = Enum.SurfaceGuiSizingMode.FixedSize
	gui.CanvasSize = Vector2.new(200, 200)
	gui.LightInfluence = 0.6
	gui.Parent = head
	if look.FaceImage then
		local img = Instance.new("ImageLabel")
		img.BackgroundTransparency = 1
		img.Size = UDim2.fromScale(1, 1)
		img.Image = look.FaceImage
		img.Parent = gui
		return
	end
	local eyeY = 0.44
	local eyeW, eyeH = 0.22, 0.28
	if look.Robot then
		eyeW, eyeH = 0.27, 0.32
	end
	if look.Mask then
		frame(gui, "Mask", Vector2.new(0.5, eyeY), Vector2.new(0.95, 0.36), look.Mask, 0.4, 1)
	end
	local count = faceStyle == "FourEyes" and 2 or 1
	for row = 1, count do
		local y = eyeY + (row - 1) * 0.16 - (count - 1) * 0.08
		for side = 1, 2 do
			local x = side == 1 and 0.32 or 0.68
			local name = (side == 1 and "EyeL" or "EyeR") .. (row > 1 and tostring(row) or "")
			if look.GlowEyes or faceStyle == "Glow" then
				frame(gui, name, Vector2.new(x, y), Vector2.new(eyeW, eyeH * 0.8), look.Eyes, 0.5, 3)
			else
				local eye = frame(gui, name, Vector2.new(x, y), Vector2.new(eyeW, eyeH * (count > 1 and 0.6 or 1)), WHITE, 0.5, 3)
				local iris = frame(eye, "Iris", Vector2.new(0.5 + (side == 1 and 0.08 or -0.08), 0.55), Vector2.new(0.62, 0.62), look.Eyes, 0.5, 4)
				iris.SizeConstraint = Enum.SizeConstraint.RelativeXX
				local pupil = frame(iris, "Pupil", Vector2.new(0.5, 0.5), Vector2.new(0.5, 0.5), BLACK, 0.5, 5)
				frame(pupil, "Shine", Vector2.new(0.3, 0.3), Vector2.new(0.4, 0.4), WHITE, 0.5, 6)
			end
		end
	end
	if not look.Robot then
		for side = 1, 2 do
			local x = side == 1 and 0.32 or 0.68
			local brow = frame(gui, side == 1 and "BrowL" or "BrowR", Vector2.new(x, eyeY - 0.2), Vector2.new(0.2, 0.045), look.Brows, 0.5, 3)
			brow.Rotation = side == 1 and -8 or 8
		end
	end
	local mouth = frame(gui, "Mouth", Vector2.new(0.5, 0.74), Vector2.new(0.3, 0.1), Color3.fromRGB(150, 40, 50), 0.5, 3)
	if look.BigTeeth then
		frame(mouth, "ToothL", Vector2.new(0.42, 0.3), Vector2.new(0.14, 0.7), WHITE, 0.2, 4)
		frame(mouth, "ToothR", Vector2.new(0.58, 0.3), Vector2.new(0.14, 0.7), WHITE, 0.2, 4)
	else
		frame(mouth, "Teeth", Vector2.new(0.5, 0.25), Vector2.new(0.8, 0.35), WHITE, 0.3, 4)
	end
end

-- Body -----------------------------------------------------------------------

local function colorBody(character, look)
	local map = {
		Head = look.Skin, UpperTorso = look.Shirt, LowerTorso = look.Pants,
		LeftUpperArm = look.Sleeve, RightUpperArm = look.Sleeve, LeftLowerArm = look.Forearm, RightLowerArm = look.Forearm,
		LeftHand = look.Hand, RightHand = look.Hand, LeftUpperLeg = look.Pants, RightUpperLeg = look.Pants,
		LeftLowerLeg = look.Shin, RightLowerLeg = look.Shin, LeftFoot = look.Shoe, RightFoot = look.Shoe,
		-- R6 fallback
		Torso = look.Shirt, ["Left Arm"] = look.Sleeve, ["Right Arm"] = look.Sleeve, ["Left Leg"] = look.Pants, ["Right Leg"] = look.Pants,
	}
	local bodyColors = character:FindFirstChildOfClass("BodyColors")
	if bodyColors then
		bodyColors:Destroy()
	end
	for name, color in pairs(map) do
		local p = character:FindFirstChild(name)
		if p and p:IsA("BasePart") then
			p.Color = color
			p.Material = Enum.Material.SmoothPlastic
		end
	end
end

local function setScale(humanoid, height, width, head)
	local function set(name, value)
		local v = humanoid:FindFirstChild(name)
		if not v then
			v = Instance.new("NumberValue")
			v.Name = name
			v.Parent = humanoid
		end
		v.Value = value
	end
	set("BodyHeightScale", height)
	set("BodyWidthScale", width)
	set("BodyDepthScale", width)
	set("HeadScale", head)
end

-- Look for the active form (Ben's aliens recolour the whole body).
local function formLook(form)
	local b, a = form.Body, form.Accent
	local look = {
		Skin = b, Shirt = b, Sleeve = b, Forearm = b, Hand = b, Pants = b, Shin = b, Shoe = b,
		HeadScale = 1.1, Height = form.Scale, Width = form.Scale, Eyes = Color3.fromRGB(90, 255, 90), Brows = a,
	}
	if form.Id == "Heatblast" then
		look.Hand = Color3.fromRGB(255, 190, 60)
		look.Shoe = Color3.fromRGB(120, 35, 20)
		look.Eyes = Color3.fromRGB(255, 240, 120)
		look.GlowEyes = true
	elseif form.Id == "FourArms" then
		look.Shirt = WHITE
		look.Pants, look.Shin, look.Shoe = a, a, a
		look.Eyes = Color3.fromRGB(255, 220, 80)
		look.Width = form.Scale * 1.15
	elseif form.Id == "XLR8" then
		look.Pants, look.Shin, look.Shoe = a, a, a
		look.Robot = true
	elseif form.Id == "WayBig" then
		look.Pants, look.Shin = a, a
		look.Eyes = Color3.fromRGB(255, 240, 120)
		look.GlowEyes = true
	end
	return look
end

-- Gear -----------------------------------------------------------------------

local function heroGear(folder, parts, heroId, look, d)
	local head, torso, lower = parts.ToonHead, parts.UpperTorso, parts.LowerTorso
	local rh, lh = parts.RightHand, parts.LeftHand
	local r = d * 0.5
	local tz = torso and torso.Size.Z * 0.5 or 0.5
	if heroId == "Jimmy" then
		local hair = Color3.fromRGB(125, 72, 35)
		ball(folder, head, CFrame.new(0, r * 0.45, r * 0.08), Vector3.new(d * 1.05, d * 0.7, d * 1.0), hair)
		cone(folder, head, CFrame.new(0, r * 0.7, 0) * CFrame.Angles(math.rad(-12), 0, 0), r * 1.2, r * 0.75, hair)
		cone(folder, head, CFrame.new(0, r * 1.7, -r * 0.25) * CFrame.Angles(math.rad(-60), 0, 0), r * 0.6, r * 0.35, hair)
		box(folder, torso, CFrame.new(0, 0.15, -tz - 0.02), Vector3.new(0.35, 0.35, 0.05), Color3.fromRGB(255, 220, 60), true)
		-- Shrink ray.
		box(folder, rh, CFrame.new(0, -0.35, -0.15), Vector3.new(0.3, 0.45, 0.55), Color3.fromRGB(120, 175, 140))
		ball(folder, rh, CFrame.new(0, -0.35, -0.55), Vector3.new(0.3, 0.3, 0.3), Color3.fromRGB(255, 70, 70), true)
		-- Jetpack flames (Q).
		for _, side in ipairs({ -0.3, 0.3 }) do
			cyl(folder, torso, CFrame.new(side, 0, tz + 0.25), 1.0, 0.4, Color3.fromRGB(210, 215, 225))
			local flame = newPart(folder, "JetFlame", Vector3.new(0.3, 0.9, 0.3), Color3.fromRGB(255, 170, 40), Enum.Material.Neon)
			weld(flame, torso, CFrame.new(side, -0.9, tz + 0.25))
			flame:SetAttribute("ShowWhen", "Jetpack")
		end
	elseif heroId == "Timmy" then
		local pink = Color3.fromRGB(240, 110, 160)
		cyl(folder, head, CFrame.new(0, r * 0.62, 0), r * 0.35, d * 1.06, pink)
		ball(folder, head, CFrame.new(0, r * 0.85, r * 0.05), Vector3.new(d * 0.98, d * 0.5, d * 0.98), pink)
		box(folder, head, CFrame.new(0, r * 0.55, -r * 0.95) * CFrame.Angles(math.rad(10), 0, 0), Vector3.new(d * 0.8, 0.08, d * 0.45), pink)
		for _, side in ipairs({ -1, 1 }) do
			ball(folder, head, CFrame.new(side * r * 0.95, r * 0.2, r * 0.1), Vector3.new(r * 0.45, r * 0.45, r * 0.5), Color3.fromRGB(120, 70, 35))
		end
		-- Wand.
		cyl(folder, rh, CFrame.new(0, -0.6, 0), 1.1, 0.1, BLACK)
		local star = ball(folder, rh, CFrame.new(0, -1.2, 0), Vector3.new(0.4, 0.4, 0.4), Color3.fromRGB(255, 230, 70), true)
		local light = Instance.new("PointLight")
		light.Color = star.Color
		light.Range = 6
		light.Parent = star
		-- Cloud-board while flying.
		local cloud = newPart(folder, "Cloud", Vector3.new(4.5, 1.2, 3), WHITE, Enum.Material.SmoothPlastic)
		local mesh = Instance.new("SpecialMesh")
		mesh.MeshType = Enum.MeshType.Sphere
		mesh.Parent = cloud
		weld(cloud, lower, CFrame.new(0, -lower.Size.Y * 0.5 - parts.LegLength - 0.3, 0))
		cloud:SetAttribute("ShowWhen", "Flying")
	elseif heroId == "Danny" then
		ball(folder, head, CFrame.new(0, r * 0.45, r * 0.06), Vector3.new(d * 1.07, d * 0.65, d * 1.05), WHITE)
		for i = -1, 1 do
			cone(folder, head, CFrame.new(i * r * 0.35, r * 0.4, r * 0.85) * CFrame.Angles(math.rad(110), 0, math.rad(i * 20)), r * 0.7, r * 0.25, WHITE)
		end
		box(folder, torso, CFrame.new(0, 0.1, -tz - 0.02), Vector3.new(0.55, 0.5, 0.05), WHITE)
		box(folder, torso, CFrame.new(0.06, 0.1, -tz - 0.04), Vector3.new(0.18, 0.32, 0.05), BLACK)
		for _, hand in ipairs({ lh, rh }) do
			local glow = ball(folder, hand, CFrame.new(0, -0.1, 0), Vector3.new(0.75, 0.75, 0.75), Color3.fromRGB(110, 255, 140), true)
			glow.Transparency = 0.35
			local light = Instance.new("PointLight")
			light.Color = glow.Color
			light.Range = 7
			light.Parent = glow
		end
		-- Ghost tail replaces the legs in flight.
		local tail = Instance.new("Folder")
		tail.Name = "GhostTail"
		tail.Parent = folder
		cone(tail, lower, CFrame.new(0, -lower.Size.Y * 0.4, 0) * CFrame.Angles(math.pi, 0, 0), parts.LegLength * 1.1, lower.Size.X * 0.6, BLACK)
		cone(tail, lower, CFrame.new(0, -lower.Size.Y * 0.4 - parts.LegLength * 0.9, 0.3) * CFrame.Angles(math.pi + 0.3, 0, 0), parts.LegLength * 0.5, lower.Size.X * 0.3, Color3.fromRGB(120, 255, 200), true)
		showWhen(tail, "Flying")
		for _, d2 in ipairs(tail:GetChildren()) do
			d2.Parent = folder
		end
		tail:Destroy()
	elseif heroId == "Jake" then
		local hair, green = Color3.fromRGB(20, 20, 22), Color3.fromRGB(80, 220, 120)
		ball(folder, head, CFrame.new(0, r * 0.5, r * 0.06), Vector3.new(d * 1.06, d * 0.6, d * 1.03), hair)
		for k = 0, 5 do
			local x = (-2.5 + k) * r * 0.22
			cone(folder, head, CFrame.new(x, r * 0.9, r * (k % 2) * 0.15) * CFrame.Angles(math.rad(-15 - k * 3), 0, math.rad(-x * 30)), r * 0.7, r * 0.18, k % 2 == 1 and green or hair)
		end
		box(folder, torso, CFrame.new(0, 0, -tz - 0.02), Vector3.new(0.15, torso.Size.Y * 0.8, 0.05), Color3.fromRGB(130, 130, 140))
	elseif heroId == "XJ9" then
		local teal, joint = Color3.fromRGB(70, 190, 205), Color3.fromRGB(60, 70, 95)
		box(folder, head, CFrame.new(0, r * 0.95, 0), Vector3.new(d * 0.85, 0.15, d * 0.9), teal)
		for _, side in ipairs({ -1, 1 }) do
			box(folder, head, CFrame.new(side * r * 1.15, r * 0.95, 0.1) * CFrame.Angles(0, 0, math.rad(side * 30)), Vector3.new(d * 0.7, 0.12, d * 0.45), teal)
			cyl(folder, head, CFrame.new(side * r * 0.98, 0, 0) * CFrame.Angles(0, 0, math.pi / 2), 0.15, d * 0.35, joint)
		end
		cone(folder, lower, CFrame.new(0, lower.Size.Y * 0.4, 0) * CFrame.Angles(math.pi, 0, 0), lower.Size.Y * 1.4, lower.Size.X * 0.85, teal)
		box(folder, torso, CFrame.new(0, torso.Size.Y * 0.1, 0), Vector3.new(torso.Size.X * 1.05, 0.2, torso.Size.Z * 1.05), teal)
		for _, foot in ipairs({ parts.LeftFoot, parts.RightFoot }) do
			local flame = newPart(folder, "RocketFlame", Vector3.new(0.45, 1.3, 0.45), Color3.fromRGB(255, 190, 60), Enum.Material.Neon)
			weld(flame, foot, CFrame.new(0, -0.8, 0))
			flame:SetAttribute("ShowWhen", "FlyingOrDodge")
		end
	elseif heroId == "ElTigre" then
		local fur = look.Skin
		for _, side in ipairs({ -1, 1 }) do
			cone(folder, head, CFrame.new(side * r * 0.55, r * 0.75, 0) * CFrame.Angles(0, 0, math.rad(-side * 15)), r * 0.6, r * 0.3, fur)
			cone(folder, head, CFrame.new(side * r * 0.95, -r * 0.1, -r * 0.1) * CFrame.Angles(0, 0, math.rad(-side * 75)), r * 0.55, r * 0.22, WHITE)
		end
		for _, hand in ipairs({ lh, rh }) do
			for c = -1, 1 do
				cone(folder, hand, CFrame.new(c * 0.12, -0.2, -0.1) * CFrame.Angles(math.pi, 0, math.rad(c * 8)), 0.75, 0.06, Color3.fromRGB(215, 220, 230))
			end
		end
	elseif heroId == "Ben" then
		local hair = Color3.fromRGB(120, 75, 40)
		ball(folder, head, CFrame.new(0, r * 0.5, r * 0.05), Vector3.new(d * 1.06, d * 0.62, d * 1.03), hair)
		cone(folder, head, CFrame.new(r * 0.25, r * 0.55, -r * 0.7) * CFrame.Angles(math.rad(-70), 0, math.rad(-20)), r * 0.5, r * 0.35, hair)
		box(folder, torso, CFrame.new(0, 0, -tz - 0.02), Vector3.new(0.3, torso.Size.Y * 0.95, 0.05), BLACK)
		box(folder, torso, CFrame.new(0, 0, tz + 0.02), Vector3.new(0.3, torso.Size.Y * 0.95, 0.05), BLACK)
		local wrist = parts.LeftLowerArm
		if wrist then
			cyl(folder, wrist, CFrame.new(0, -wrist.Size.Y * 0.35, 0), 0.25, wrist.Size.X * 1.25, BLACK)
			ball(folder, wrist, CFrame.new(0, -wrist.Size.Y * 0.35, -wrist.Size.Z * 0.6), Vector3.new(0.3, 0.3, 0.12), Color3.fromRGB(90, 255, 90), true)
		end
	elseif heroId == "Aang" then
		local blue, orange, brown = Color3.fromRGB(70, 160, 240), Color3.fromRGB(230, 110, 40), Color3.fromRGB(140, 95, 55)
		box(folder, head, CFrame.new(0, r * 0.97, 0), Vector3.new(0.18, 0.05, d * 0.85), blue, true)
		box(folder, head, CFrame.new(0, r * 0.45, -r * 0.88) * CFrame.Angles(math.rad(25), 0, 0), Vector3.new(0.18, d * 0.4, 0.05), blue, true)
		ball(folder, torso, CFrame.new(0, torso.Size.Y * 0.3, 0.05), Vector3.new(torso.Size.X * 1.2, torso.Size.Y * 0.5, torso.Size.Z * 1.3), orange)
		-- Staff on a joint the client spins.
		local staff = newPart(folder, "Staff", Vector3.new(6, 0.22, 0.22), brown)
		staff.Shape = Enum.PartType.Cylinder
		motor(staff, rh, CFrame.new(0, -0.2, 0) * CFrame.Angles(0, 0, math.pi / 2), "StaffJoint")
		staff:SetAttribute("ShowWhen", "NotGliding")
		-- Glider wings while gliding.
		local wing = newPart(folder, "Glider", Vector3.new(7, 0.1, 2.6), orange)
		weld(wing, rh, CFrame.new(0, -0.4, 0))
		wing:SetAttribute("ShowWhen", "Gliding")
		-- Air scooter.
		local scooter = newPart(folder, "Scooter", Vector3.new(3.4, 2.4, 3.4), Color3.fromRGB(190, 220, 255), Enum.Material.Neon)
		local mesh = Instance.new("SpecialMesh")
		mesh.MeshType = Enum.MeshType.Sphere
		mesh.Parent = scooter
		scooter.Transparency = 0.45
		weld(scooter, lower, CFrame.new(0, -lower.Size.Y * 0.5 - parts.LegLength, 0))
		scooter:SetAttribute("ShowWhen", "Scooter")
	end
	if look.Belt and lower then
		box(folder, lower, CFrame.new(0, lower.Size.Y * 0.3, 0), Vector3.new(lower.Size.X * 1.06, 0.22, lower.Size.Z * 1.06), look.Belt)
	end
end

local function formGear(folder, parts, form, d)
	local head, torso, lower = parts.ToonHead, parts.UpperTorso, parts.LowerTorso
	local r = d * 0.5
	if form.Id == "Heatblast" then
		local fire = Instance.new("Fire")
		fire.Heat = 12
		fire.Size = d * 2.2
		fire.Color = Color3.fromRGB(255, 150, 30)
		fire.SecondaryColor = Color3.fromRGB(255, 230, 90)
		fire.Parent = head
		cone(folder, head, CFrame.new(0, r * 0.6, 0.1), d * 1.0, r * 0.95, Color3.fromRGB(255, 160, 30), true)
		for _, hand in ipairs({ parts.LeftHand, parts.RightHand }) do
			local f = Instance.new("Fire")
			f.Size = 2
			f.Heat = 6
			f.Parent = hand
		end
	elseif form.Id == "FourArms" then
		local tz = torso.Size.Z * 0.5
		box(folder, torso, CFrame.new(0, 0, -tz - 0.02), Vector3.new(0.6, torso.Size.Y * 0.8, 0.05), BLACK)
		box(folder, lower, CFrame.new(0, lower.Size.Y * 0.3, 0), Vector3.new(lower.Size.X * 1.06, 0.4, lower.Size.Z * 1.06), Color3.fromRGB(230, 190, 60))
		-- Second pair of arms on joints the client animates.
		for _, side in ipairs({ -1, 1 }) do
			local up = parts.LeftUpperArm or torso
			local armPart = newPart(folder, side < 0 and "ExtraArmL" or "ExtraArmR", Vector3.new(up.Size.X * 0.9, up.Size.Y * 1.9, up.Size.Z * 0.9), form.Body)
			motor(armPart, torso, CFrame.new(side * (torso.Size.X * 0.5 + up.Size.X * 0.4), -torso.Size.Y * 0.15, 0), side < 0 and "ExtraArmL" or "ExtraArmR")
				.C1 = CFrame.new(0, armPart.Size.Y * 0.4, 0)
		end
	elseif form.Id == "XLR8" then
		box(folder, head, CFrame.new(0, -r * 0.05, -r * 0.55), Vector3.new(d * 0.75, d * 0.38, d * 0.5), Color3.fromRGB(20, 20, 25))
		box(folder, head, CFrame.new(0, r * 0.08, -r * 0.82), Vector3.new(d * 0.55, 0.1, 0.05), Color3.fromRGB(80, 200, 255), true)
		cone(folder, lower, CFrame.new(0, 0, lower.Size.Z * 0.5) * CFrame.Angles(math.rad(115), 0, 0), lower.Size.Y * 3.5, 0.35, form.Body)
	elseif form.Id == "Cannonbolt" then
		local shell = newPart(folder, "CannonShell", Vector3.new(1, 1, 1) * parts.Height * 0.95, form.Body)
		local mesh = Instance.new("SpecialMesh")
		mesh.MeshType = Enum.MeshType.Sphere
		mesh.Parent = shell
		weld(shell, parts.Root, CFrame.new(0, -parts.Height * 0.12, 0))
		shell:SetAttribute("ShowWhen", "Always")
		shell:SetAttribute("Spin", true)
		for k = 0, 3 do
			local band = newPart(folder, "CannonBand", Vector3.new(parts.Height * 0.97, parts.Height * 0.18, parts.Height * 0.97), form.Accent)
			local m2 = Instance.new("SpecialMesh")
			m2.MeshType = Enum.MeshType.Sphere
			m2.Parent = band
			weld(band, parts.Root, CFrame.new(0, -parts.Height * 0.12, 0) * CFrame.Angles(math.rad(k * 45), 0, 0))
			band:SetAttribute("ShowWhen", "Always")
		end
	elseif form.Id == "WayBig" then
		local tz = torso.Size.Z * 0.5
		box(folder, torso, CFrame.new(0, 0, -tz - 0.02), Vector3.new(torso.Size.X * 0.25, torso.Size.Y * 0.8, 0.05), form.Accent)
		box(folder, head, CFrame.new(0, r * 1.0, 0.1), Vector3.new(0.2, d * 0.8, d * 0.9), form.Accent)
	end
end

-- Jake's dragon: built hidden, shown while flying (the human body hides).
local function dragonGear(folder, parts, scale)
	local root = parts.Root
	local red, belly, wingC = Color3.fromRGB(205, 35, 30), Color3.fromRGB(245, 200, 70), Color3.fromRGB(150, 25, 25)
	local k = scale * 1.6
	local group = Instance.new("Folder")
	group.Name = "Dragon"
	group.Parent = folder
	local body = ball(group, root, CFrame.new(0, 0.3 * k, 0), Vector3.new(2.4, 2.2, 4.8) * k, red)
	ball(group, root, CFrame.new(0, -0.3 * k, -0.2 * k), Vector3.new(1.8, 1.5, 4.0) * k, belly)
	cyl(group, root, CFrame.new(0, 1.3 * k, -2.2 * k) * CFrame.Angles(math.rad(-40), 0, 0), 2.2 * k, 1.0 * k, red)
	local headC = CFrame.new(0, 2.3 * k, -3.2 * k)
	ball(group, root, headC, Vector3.new(1.6, 1.3, 2.0) * k, red)
	box(group, root, headC * CFrame.new(0, -0.2 * k, -1.1 * k), Vector3.new(1.0, 0.7, 1.3) * k, red)
	for _, side in ipairs({ -1, 1 }) do
		ball(group, root, headC * CFrame.new(side * 0.45 * k, 0.3 * k, -0.7 * k), Vector3.new(0.35, 0.35, 0.35) * k, Color3.fromRGB(255, 230, 80), true)
		cone(group, root, headC * CFrame.new(side * 0.45 * k, 0.6 * k, 0.4 * k) * CFrame.Angles(math.rad(-50), 0, 0), 1.2 * k, 0.35 * k, Color3.fromRGB(250, 225, 150))
		-- Wing on a joint the client flaps.
		local wing = newPart(group, side < 0 and "DragonWingL" or "DragonWingR", Vector3.new(4.5, 0.15, 3.0) * k, wingC)
		motor(wing, body, CFrame.new(side * 1.0 * k, 0.8 * k, -0.4 * k), side < 0 and "DragonWingL" or "DragonWingR").C1 = CFrame.new(-side * 2.2 * k, 0, 0)
		cyl(group, root, CFrame.new(side * 0.8 * k, -1.2 * k, -1.4 * k), 1.4 * k, 0.55 * k, red)
		cyl(group, root, CFrame.new(side * 0.8 * k, -1.3 * k, 1.4 * k), 1.6 * k, 0.65 * k, red)
	end
	for i = 1, 5 do
		local s = 1.2 - i * 0.17
		ball(group, root, CFrame.new(0, 0.3 * k - i * 0.1 * k, 2.2 * k + i * 0.9 * k), Vector3.new(s, s, 1.6) * k, red)
	end
	showWhen(group, "Flying")
	for _, p in ipairs(group:GetChildren()) do
		p.Parent = folder
	end
	group:Destroy()
end

-- Public -----------------------------------------------------------------------

function Looks.apply(character, heroId, form)
	local humanoid = character:FindFirstChildOfClass("Humanoid")
	if not humanoid then
		return
	end
	local old = character:FindFirstChild(GEAR)
	if old then
		old:Destroy()
	end
	for _, d in ipairs(character:GetDescendants()) do
		if (d:IsA("Fire") or d:IsA("Motor6D") and (d.Name == "StaffJoint" or d.Name:sub(1, 8) == "ExtraArm" or d.Name:sub(1, 10) == "DragonWing")) then
			d:Destroy()
		end
	end
	local hero = Config.Heroes[heroId] or Config.Heroes.Jimmy
	local look = form and formLook(form) or hero.Look
	local r15 = humanoid.RigType == Enum.HumanoidRigType.R15
	if r15 then
		setScale(humanoid, look.Height or 1, look.Width or 1, look.HeadScale or 1)
		-- Let the rig rescale before measuring parts for the gear.
		task.wait(0.1)
		if not character.Parent then
			return
		end
	end
	colorBody(character, look)
	-- Strip anything the avatar brought along.
	for _, d in ipairs(character:GetChildren()) do
		if d:IsA("Accessory") or d:IsA("Shirt") or d:IsA("Pants") or d:IsA("ShirtGraphic") then
			d:Destroy()
		end
	end

	local folder = Instance.new("Folder")
	folder.Name = GEAR
	folder.Parent = character
	local head = character:FindFirstChild("Head")
	if not head then
		return
	end
	-- Round toon head over the original (which stays as the physics part).
	local face = head:FindFirstChild("face") or head:FindFirstChildOfClass("Decal")
	if face then
		face:Destroy()
	end
	head.Transparency = 1
	local d = head.Size.Y * 1.25
	local toon = newPart(folder, "ToonHead", Vector3.new(d, d, d), look.Skin)
	local mesh = Instance.new("SpecialMesh")
	mesh.MeshType = Enum.MeshType.Sphere
	mesh.Parent = toon
	weld(toon, head, CFrame.new(0, d * 0.12, 0))
	buildFace(toon, look, form and form.Id == "FourArms" and "FourEyes" or nil)

	local parts = { ToonHead = toon, Root = character:FindFirstChild("HumanoidRootPart") }
	for _, name in ipairs(R15_PARTS) do
		parts[name] = character:FindFirstChild(name)
	end
	parts.UpperTorso = parts.UpperTorso or character:FindFirstChild("Torso")
	parts.LowerTorso = parts.LowerTorso or parts.UpperTorso
	parts.LeftHand = parts.LeftHand or character:FindFirstChild("Left Arm")
	parts.RightHand = parts.RightHand or character:FindFirstChild("Right Arm")
	local ul, ll = parts.LeftUpperLeg, parts.LeftLowerLeg
	parts.LegLength = (ul and ul.Size.Y or 1) + (ll and ll.Size.Y or 1) + 0.4
	parts.Height = humanoid.HipHeight + (parts.Root and parts.Root.Size.Y or 2) * 0.5 + 3
	if form then
		formGear(folder, parts, form, d)
		if form.Id == "Cannonbolt" then
			character:SetAttribute("HideBody", true)
		else
			character:SetAttribute("HideBody", nil)
		end
	else
		character:SetAttribute("HideBody", nil)
		heroGear(folder, parts, heroId, look, d)
		if heroId == "Jake" then
			dragonGear(folder, parts, look.Height or 1)
		end
	end
end

return Looks
