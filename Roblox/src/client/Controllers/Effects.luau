--!nonstrict
-- Client-side visuals for server events (Remotes "Fx"): particle bursts,
-- projectiles, melee slashes, shockwaves, hit flashes, Jake's fire breath,
-- Danny's wail, collected shards, the next course gate. Also flight trails,
-- speed lines, sonic booms, ground-pound rings, and the marker that points to
-- the nearest rift shard.

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local TweenService = game:GetService("TweenService")
local Debris = game:GetService("Debris")

local Effects = {}
local player = Players.LocalPlayer
local Hud, CameraRig, Movement
local fxFolder
local shots = {}        -- [id] = { Part, Vel, Life }
local takenShards = {}  -- [id] = true
local breath = {}       -- [player] = emitter
local trails = {}       -- [character] = emitter

local function part(props)
	local p = Instance.new("Part")
	p.Anchored = true
	p.CanCollide = false
	p.CanQuery = false
	p.CanTouch = false
	p.CastShadow = false
	p.Material = Enum.Material.Neon
	for k, v in pairs(props) do
		p[k] = v
	end
	p.Parent = fxFolder
	return p
end

function Effects.burst(pos, color, count, gravity)
	if not pos then
		return
	end
	local holder = part({ Transparency = 1, Size = Vector3.new(0.2, 0.2, 0.2), CFrame = CFrame.new(pos) })
	local e = Instance.new("ParticleEmitter")
	e.Color = ColorSequence.new(color or Color3.new(1, 1, 1))
	e.LightEmission = 0.8
	e.Size = NumberSequence.new({ NumberSequenceKeypoint.new(0, 0.6), NumberSequenceKeypoint.new(1, 0) })
	e.Speed = NumberRange.new(8, 26)
	e.SpreadAngle = Vector2.new(180, 180)
	e.Lifetime = NumberRange.new(0.3, 0.8)
	e.Acceleration = gravity and Vector3.new(0, -60, 0) or Vector3.zero
	e.Rate = 0
	e.Parent = holder
	e:Emit(count or 20)
	Debris:AddItem(holder, 1.5)
end

-- An expanding flat ring (shockwaves, landings, sonic booms).
function Effects.ring(pos, normal, radius, time, color)
	local disc = part({ Shape = Enum.PartType.Cylinder, Size = Vector3.new(0.3, 1, 1), Color = color or Color3.new(1, 1, 1), Transparency = 0.2 })
	local up = (normal or Vector3.new(0, 1, 0)).Unit
	disc.CFrame = CFrame.lookAt(pos, pos + up) * CFrame.Angles(0, math.pi / 2, 0)
	TweenService:Create(disc, TweenInfo.new(time or 0.5, Enum.EasingStyle.Quad), { Size = Vector3.new(0.3, radius * 2, radius * 2), Transparency = 1 }):Play()
	Debris:AddItem(disc, (time or 0.5) + 0.1)
end

local function slash(pos, dir, color, finisher)
	local count = finisher and 10 or 6
	local base = CFrame.lookAt(pos, pos + dir)
	for i = 1, count do
		local a = math.rad(-70 + 140 * (i - 1) / (count - 1))
		local p = part({ Size = Vector3.new(0.4, 0.4, finisher and 2.4 or 1.6), Color = color, Transparency = 0.1 })
		p.CFrame = base * CFrame.Angles(0, a, 0) * CFrame.new(0, 0, -2)
		TweenService:Create(p, TweenInfo.new(0.25), { Transparency = 1, Size = p.Size * Vector3.new(0.2, 0.2, 1.5) }):Play()
		Debris:AddItem(p, 0.3)
	end
end

local function flash(model)
	if not model or not model.Parent then
		return
	end
	local h = Instance.new("Highlight")
	h.FillColor = Color3.new(1, 1, 1)
	h.FillTransparency = 0.1
	h.OutlineTransparency = 1
	h.Parent = model
	Debris:AddItem(h, 0.12)
end

local function setBreath(who, on)
	local character = who and who.Character
	local existing = breath[who]
	if existing then
		existing:Destroy()
		breath[who] = nil
	end
	if not on or not character then
		return
	end
	local head = character:FindFirstChild("Head")
	if not head then
		return
	end
	local att = Instance.new("Attachment")
	att.Name = "BreathFx"
	att.Position = Vector3.new(0, 0, -1)
	att.Parent = head
	local e = Instance.new("ParticleEmitter")
	e.Color = ColorSequence.new(Color3.fromRGB(255, 220, 60), Color3.fromRGB(255, 90, 20))
	e.Size = NumberSequence.new({ NumberSequenceKeypoint.new(0, 0.8), NumberSequenceKeypoint.new(1, 3) })
	e.Transparency = NumberSequence.new(0, 1)
	e.LightEmission = 1
	e.Speed = NumberRange.new(40, 55)
	e.SpreadAngle = Vector2.new(14, 14)
	e.Lifetime = NumberRange.new(0.4, 0.6)
	e.Rate = 120
	e.EmissionDirection = Enum.NormalId.Front
	e.Parent = att
	breath[who] = att
end

local function onFx(fx)
	if type(fx) ~= "table" then
		return
	end
	local kind = fx.Kind
	if kind == "Burst" then
		Effects.burst(fx.Pos, fx.Color, fx.Count, fx.Gravity)
	elseif kind == "Shot" then
		local p = part({ Shape = Enum.PartType.Ball, Size = Vector3.new(1, 1, 1) * math.max(0.6, fx.Radius * 1.6), Color = fx.Color, CFrame = CFrame.new(fx.Pos) })
		local light = Instance.new("PointLight")
		light.Color = fx.Color
		light.Range = 8
		light.Parent = p
		shots[fx.Id] = { Part = p, Vel = fx.Vel, Life = fx.Life }
	elseif kind == "ShotEnd" then
		local s = shots[fx.Id]
		if s then
			s.Part:Destroy()
			shots[fx.Id] = nil
		end
		Effects.burst(fx.Pos, Color3.new(1, 1, 1), 6)
	elseif kind == "Slash" then
		slash(fx.Pos, fx.Dir, fx.Color, fx.Finisher)
		if fx.Finisher then
			Effects.ring(fx.Pos - Vector3.new(0, 2, 0), Vector3.new(0, 1, 0), 10, 0.35, fx.Color)
		end
		if fx.Player == player and (fx.Hits or 0) > 0 then
			CameraRig.shake(fx.Finisher and 0.45 or 0.2)
		end
	elseif kind == "Hit" then
		flash(fx.Model)
		Effects.burst(fx.Pos, Color3.new(1, 1, 1), 6)
		if fx.Attacker == player then
			Hud.addHit()
		end
	elseif kind == "Shockwave" then
		Effects.ring(fx.Pos, Vector3.new(0, 1, 0), fx.Radius, fx.Time, fx.Color)
		local root = player.Character and player.Character:FindFirstChild("HumanoidRootPart")
		if root and (root.Position - fx.Pos).Magnitude < fx.Radius then
			CameraRig.shake(0.5)
		end
	elseif kind == "Shake" then
		local root = player.Character and player.Character:FindFirstChild("HumanoidRootPart")
		if root and (root.Position - fx.Pos).Magnitude < 80 then
			CameraRig.shake(fx.Amount or 0.4)
		end
	elseif kind == "Breath" then
		setBreath(fx.Player, fx.On)
	elseif kind == "Wail" then
		local character = fx.Player and fx.Player.Character
		local head = character and character:FindFirstChild("Head")
		if head then
			for i = 1, 4 do
				task.delay(i * 0.06, function()
					Effects.ring(head.Position + fx.Dir * i * 4, fx.Dir, 3 + i * 3, 0.4, Color3.fromRGB(120, 255, 150))
				end)
			end
		end
	elseif kind == "ShardTaken" then
		takenShards[fx.Id] = true
		local shard = workspace:FindFirstChild("Shards") and workspace.Shards:FindFirstChild("RiftShard_" .. fx.Id)
		if shard then
			shard.Parent = nil -- hidden for this player only
		end
		Effects.burst(fx.Pos, Color3.fromRGB(190, 130, 255), 30)
		Effects.ring(fx.Pos, Vector3.new(0, 1, 0), 8, 0.4, Color3.fromRGB(120, 230, 255))
	elseif kind == "Gate" then
		if Effects.lastGate and Effects.lastGate.Parent then
			for _, d in ipairs(Effects.lastGate:GetDescendants()) do
				if d:IsA("BasePart") then
					d.Color = Color3.fromRGB(120, 255, 210)
				end
			end
		end
		Effects.lastGate = fx.Gate
		if fx.Gate then
			for _, d in ipairs(fx.Gate:GetDescendants()) do
				if d:IsA("BasePart") then
					d.Color = Color3.fromRGB(255, 220, 60)
				end
			end
		end
		local text = fx.Next == 1 and (fx.Name .. (fx.Best and ("  best %.2fs"):format(fx.Best) or "")) or ("%s  gate %d/%d"):format(fx.Name, fx.Next, fx.Count)
		Hud.setCourse(text)
	end
end

-- Flight trails for everyone who's flying.
local function updateTrails()
	for _, p in ipairs(Players:GetPlayers()) do
		local character = p.Character
		local root = character and character:FindFirstChild("HumanoidRootPart")
		local flying = character and (character:GetAttribute("MvFlying") or (p == player and Movement.state().Flying))
		local hero = character and character:GetAttribute("Hero")
		local existing = character and trails[character]
		if root and flying and not existing and (hero == "Danny" or hero == "XJ9" or hero == "Timmy" or hero == "Jake") then
			local att = Instance.new("Attachment")
			att.Name = "FlightTrail"
			att.Parent = root
			local e = Instance.new("ParticleEmitter")
			local colors = {
				Danny = ColorSequence.new(Color3.fromRGB(120, 255, 170), Color3.fromRGB(240, 255, 250)),
				XJ9 = ColorSequence.new(Color3.fromRGB(255, 200, 70), Color3.fromRGB(200, 205, 215)),
				Timmy = ColorSequence.new(Color3.fromRGB(255, 230, 80), Color3.fromRGB(245, 140, 200)),
				Jake = ColorSequence.new(Color3.fromRGB(255, 150, 60), Color3.fromRGB(255, 240, 200)),
			}
			e.Color = colors[hero]
			e.LightEmission = 0.8
			e.Size = NumberSequence.new({ NumberSequenceKeypoint.new(0, hero == "XJ9" and 1.2 or 0.7), NumberSequenceKeypoint.new(1, 0) })
			e.Transparency = NumberSequence.new(0.1, 1)
			e.Lifetime = NumberRange.new(0.4, 0.7)
			e.Speed = NumberRange.new(0, 2)
			e.Rate = 60
			e.LockedToPart = false
			e.Parent = att
			trails[character] = att
		elseif existing and (not flying or not root) then
			existing:Destroy()
			trails[character] = nil
		end
	end
	for character, att in pairs(trails) do
		if not character.Parent then
			att:Destroy()
			trails[character] = nil
		end
	end
end

-- Marker on the nearest rift shard this player hasn't collected.
local marker
local function updateMarker()
	local root = player.Character and player.Character:FindFirstChild("HumanoidRootPart")
	local folder = workspace:FindFirstChild("Shards")
	if not root or not folder then
		return
	end
	local best, bestDist = nil, 500
	for _, shard in ipairs(folder:GetChildren()) do
		local id = shard:GetAttribute("ShardId")
		if id and not takenShards[id] and shard.PrimaryPart then
			local d = (shard.PrimaryPart.Position - root.Position).Magnitude
			if d < bestDist then
				best, bestDist = shard, d
			end
		end
	end
	if not marker then
		marker = Instance.new("BillboardGui")
		marker.Size = UDim2.fromOffset(90, 40)
		marker.AlwaysOnTop = true
		marker.StudsOffset = Vector3.new(0, 3, 0)
		local l = Instance.new("TextLabel")
		l.Name = "Text"
		l.BackgroundTransparency = 1
		l.Size = UDim2.fromScale(1, 1)
		l.Font = Enum.Font.FredokaOne
		l.TextScaled = true
		l.TextColor3 = Color3.fromRGB(220, 200, 255)
		l.TextStrokeTransparency = 0.3
		l.Parent = marker
		marker.Parent = player:WaitForChild("PlayerGui")
	end
	marker.Adornee = best and best.PrimaryPart or nil
	marker.Enabled = best ~= nil
	if best then
		marker.Text.Text = ("◆ %d m"):format(bestDist * 0.3)
	end
end

function Effects.init(controllers)
	Hud, CameraRig, Movement = controllers.Hud, controllers.CameraRig, controllers.Movement
	Movement.onBoom = function(pos, dir)
		Effects.ring(pos, dir, 10, 0.45, Color3.fromRGB(235, 245, 255))
	end
	Movement.onPound = function(pos)
		Effects.ring(pos + Vector3.new(0, 0.5, 0), Vector3.new(0, 1, 0), 22, 0.4, Color3.fromRGB(215, 205, 185))
		Effects.burst(pos, Color3.fromRGB(200, 190, 170), 24, true)
	end
	Movement.onEmotePeak = function(pos, color)
		Effects.ring(pos, Vector3.new(0, 1, 0), 12, 0.5, color)
		Effects.burst(pos + Vector3.new(0, 2, 0), color, 24)
	end
end

function Effects.start()
	fxFolder = Instance.new("Folder")
	fxFolder.Name = "LocalFx"
	fxFolder.Parent = workspace
	ReplicatedStorage:WaitForChild("Remotes").Fx.OnClientEvent:Connect(onFx)
	local accum = 0
	RunService.RenderStepped:Connect(function(dt)
		for id, s in pairs(shots) do
			s.Life = s.Life - dt
			if s.Life <= 0 then
				s.Part:Destroy()
				shots[id] = nil
			else
				s.Part.CFrame = s.Part.CFrame + s.Vel * dt
			end
		end
		-- Spin the shards locally.
		local folder = workspace:FindFirstChild("Shards")
		if folder then
			local spin = CFrame.Angles(0, dt * 2.2, 0)
			for _, shard in ipairs(folder:GetChildren()) do
				if shard:IsA("Model") and shard.PrimaryPart then
					shard:PivotTo(shard:GetPivot() * spin)
				end
			end
		end
		-- Speed lines for the local hero at high speed.
		local S = Movement.state()
		local root = player.Character and player.Character:FindFirstChild("HumanoidRootPart")
		if root and (S.Flying or S.Gliding) then
			local v = root.AssemblyLinearVelocity
			if v.Magnitude > 63 and math.random() < 0.6 then
				local p = part({ Size = Vector3.new(0.08, 0.08, 4), Color = Color3.fromRGB(240, 248, 255), Transparency = 0.3 })
				local offset = Vector3.new(math.random(-9, 9), math.random(-7, 7), math.random(-9, 9))
				p.CFrame = CFrame.lookAt(root.Position + offset, root.Position + offset + v)
				TweenService:Create(p, TweenInfo.new(0.25), { Transparency = 1 }):Play()
				Debris:AddItem(p, 0.3)
			end
		end
		accum = accum + dt
		if accum > 0.25 then
			accum = 0
			updateTrails()
			updateMarker()
		end
	end)
end

return Effects
