--!nonstrict
-- Server-authoritative combat: attacks with 3-hit combos (third hit is a
-- finisher), projectiles, signature abilities, ultimates, ground pounds,
-- Cannonbolt's rolling damage, and damage/healing for players.
-- Clients only send intent (aim direction, ability name); every cooldown,
-- range and damage number is decided here.

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local TweenService = game:GetService("TweenService")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))
local Omnitrix = require(Shared:WaitForChild("Omnitrix"))

local Combat = {}
local Net, PlayerState, Enemies, World
local projectiles = {}
local nextShot = 1
local shockwaves = {}
local breathing = {} -- [player] = aim
local regenAt = {}   -- [player] = time

local function charOf(player)
	local character = player.Character
	local humanoid = character and character:FindFirstChildOfClass("Humanoid")
	local root = character and character:FindFirstChild("HumanoidRootPart")
	if humanoid and root and humanoid.Health > 0 then
		return character, humanoid, root
	end
	return nil
end

local function canAttack(st, character)
	if st.Intangible or character:GetAttribute("MvIntangible") then
		return false
	end
	local humanoid = character:FindFirstChildOfClass("Humanoid")
	if humanoid and humanoid.SeatPart then
		return false
	end
	if st.Hero == "Ben" and (st.Omni:IsInputLocked() or st.Omni.State == Omnitrix.State.Selecting) then
		return false
	end
	return true
end

local function attackDef(st)
	local form = PlayerState.form(st)
	if form then
		return form.Attack
	end
	return (Config.Heroes[st.Hero] or Config.Heroes.Jimmy).Attack
end

local function bodyScale(character)
	local humanoid = character:FindFirstChildOfClass("Humanoid")
	local v = humanoid and humanoid:FindFirstChild("BodyHeightScale")
	return v and v.Value or 1
end

-- Damage -------------------------------------------------------------------------

function Combat.isImmune(player)
	local st = PlayerState.get(player)
	local character = player.Character
	if not st or not character then
		return true
	end
	if st.Intangible or os.clock() < st.DodgeUntil then
		return true
	end
	local humanoid = character:FindFirstChildOfClass("Humanoid")
	if humanoid and humanoid.SeatPart then
		return true
	end
	return st.Hero == "Ben" and st.Omni:IsInvulnerable()
end

function Combat.damagePlayer(player, amount)
	local character, humanoid = charOf(player)
	if not character or Combat.isImmune(player) then
		return
	end
	humanoid:TakeDamage(amount)
	regenAt[player] = os.clock() + Config.Player.RegenDelay
	local st = PlayerState.get(player)
	PlayerState.addMeter(st, amount * 0.25)
	character:SetAttribute("HurtAt", workspace:GetServerTimeNow())
end

local function hitEnemiesInSphere(center, radius, damage, knockScale, attacker, alreadyHit)
	local hits = 0
	for _, e in ipairs(Enemies.list()) do
		if e.Health > 0 and e.Root.Parent and not (alreadyHit and alreadyHit[e]) then
			local to = e.Root.Position - center
			if to.Magnitude < radius + 1.6 then
				if alreadyHit then
					alreadyHit[e] = true
				end
				local flat = Vector3.new(to.X, 0, to.Z)
				local dir = flat.Magnitude > 0.01 and flat.Unit or Vector3.new(0, 0, 1)
				Enemies.damage(e, damage, dir * 40 * knockScale + Vector3.new(0, 30 * knockScale, 0), attacker)
				hits = hits + 1
			end
		end
	end
	return hits
end

local function shockwave(center, maxRadius, speed, damage, color, attacker)
	table.insert(shockwaves, { Center = center, Radius = 0, Max = maxRadius, Speed = speed, Damage = damage, Hit = {}, Attacker = attacker })
	Net.fireAll("Fx", { Kind = "Shockwave", Pos = center, Radius = maxRadius, Time = maxRadius / speed, Color = color })
end

-- Projectiles -----------------------------------------------------------------------

local function fire(player, origin, velocity, a, damageMul, radiusMul, homing)
	local id = nextShot
	nextShot = nextShot + 1
	local shot = {
		Id = id, Pos = origin, Vel = velocity, Life = homing and 3 or 2.5, Damage = a.Damage * damageMul,
		Radius = (a.Radius or 0.6) * radiusMul, Knock = (a.Knockback or 20) / 40, Owner = player, Homing = homing,
	}
	projectiles[id] = shot
	if not homing then
		Net.fireAll("Fx", { Kind = "Shot", Id = id, Pos = origin, Vel = velocity, Color = a.Color, Radius = shot.Radius, Life = shot.Life })
	else
		local p = Instance.new("Part")
		p.Name = "Missile"
		p.Anchored = true
		p.CanCollide = false
		p.CanQuery = false
		p.Material = Enum.Material.Neon
		p.Color = Color3.fromRGB(255, 210, 90)
		p.Size = Vector3.new(0.6, 0.6, 1.6)
		p.CFrame = CFrame.lookAt(origin, origin + velocity)
		local trail = Instance.new("Fire")
		trail.Size = 2
		trail.Heat = 0
		trail.Parent = p
		p.Parent = workspace
		shot.Part = p
	end
	return shot
end

local function endShot(shot, pos)
	projectiles[shot.Id] = nil
	if shot.Part then
		shot.Part:Destroy()
		Net.fireAll("Fx", { Kind = "Burst", Pos = pos, Color = Color3.fromRGB(255, 200, 90), Count = 10 })
	else
		Net.fireAll("Fx", { Kind = "ShotEnd", Id = shot.Id, Pos = pos })
	end
end

local function stepProjectiles(dt)
	for _, shot in pairs(projectiles) do
		shot.Life = shot.Life - dt
		if shot.Homing then
			local best, bestDist = nil, 200
			for _, e in ipairs(Enemies.list()) do
				if e.Health > 0 and e.Root.Parent then
					local d = (e.Root.Position - shot.Pos).Magnitude
					if d < bestDist then
						best, bestDist = e, d
					end
				end
			end
			if best then
				local want = (best.Root.Position - shot.Pos).Unit * 120
				shot.Vel = shot.Vel:Lerp(want, math.min(1, dt * 4))
			else
				shot.Vel = shot.Vel + Vector3.new(0, -60 * dt, 0)
			end
		end
		local move = shot.Vel * dt
		local params = RaycastParams.new()
		params.FilterType = Enum.RaycastFilterType.Exclude
		local exclude = { workspace:FindFirstChild("Enemies") }
		if shot.Owner.Character then
			table.insert(exclude, shot.Owner.Character)
		end
		params.FilterDescendantsInstances = exclude
		local hit = workspace:Raycast(shot.Pos, move, params)
		local done = false
		-- Enemies along the segment.
		for _, e in ipairs(Enemies.list()) do
			if not done and e.Health > 0 and e.Root.Parent then
				local rel = e.Root.Position - shot.Pos
				local along = math.clamp(rel:Dot(move.Unit), 0, move.Magnitude)
				local closest = shot.Pos + move.Unit * along
				if (e.Root.Position - closest).Magnitude < shot.Radius + 1.8 then
					local dir = shot.Vel.Unit
					Enemies.damage(e, shot.Damage, dir * 40 * shot.Knock + Vector3.new(0, 15 * shot.Knock, 0), shot.Owner)
					endShot(shot, closest)
					done = true
				end
			end
		end
		if not done then
			if hit or shot.Life <= 0 then
				endShot(shot, hit and hit.Position or shot.Pos)
			else
				shot.Pos = shot.Pos + move
				if shot.Part then
					shot.Part.CFrame = CFrame.lookAt(shot.Pos, shot.Pos + shot.Vel)
				end
			end
		end
	end
end

-- Attacks --------------------------------------------------------------------------

local function onAttack(player, aim)
	aim = Net.unit(aim)
	local st = PlayerState.get(player)
	local character, _, root = charOf(player)
	if not aim or not st or not character or not canAttack(st, character) then
		return
	end
	local a = attackDef(st)
	if not a then
		return
	end
	local now = os.clock()
	if (st.Cooldowns.Attack or 0) > now + 0.03 then
		return
	end
	local combo = a.Cooldown >= 0.2
	if combo and now - st.LastAttack < a.Cooldown + Config.Moves.ComboWindow + 0.1 then
		st.AttackStep = (st.AttackStep + 1) % 3
	else
		st.AttackStep = 0
	end
	st.LastAttack = now
	local finisher = combo and st.AttackStep == 2
	st.Cooldowns.Attack = now + a.Cooldown + (finisher and 0.15 or 0)
	local dmg = finisher and Config.Moves.FinisherDamage or 1
	local knock = finisher and Config.Moves.FinisherKnock or 1
	local dur = finisher and 0.38 or 0.25
	character:SetAttribute("AttackStep", st.AttackStep)
	character:SetAttribute("AttackDur", dur)
	character:SetAttribute("AttackAt", workspace:GetServerTimeNow())
	local scale = bodyScale(character)
	local chest = root.Position + Vector3.new(0, 0.8 * scale, 0)

	if a.Kind == "Projectile" then
		local right = aim:Cross(Vector3.new(0, 1, 0))
		right = right.Magnitude > 0.01 and right.Unit or Vector3.new(1, 0, 0)
		for k = 1, a.Count or 1 do
			local spread = (a.Count or 1) > 1 and (k - ((a.Count or 1) + 1) / 2) * 0.08 or 0
			local dir = (aim + right * spread).Unit
			fire(player, chest + dir * 2.5, dir * a.Speed, a, dmg, finisher and 1.7 or 1, false)
		end
	elseif a.Kind == "Melee" then
		local flatAim = Vector3.new(aim.X, 0, aim.Z)
		flatAim = flatAim.Magnitude > 0.01 and flatAim.Unit or Vector3.new(0, 0, -1)
		local range = a.Range * (finisher and 1.3 or 1)
		local hits = 0
		for _, e in ipairs(Enemies.list()) do
			if e.Health > 0 and e.Root.Parent then
				local to = e.Root.Position - root.Position
				local flat = Vector3.new(to.X, 0, to.Z)
				local inFront = flat.Magnitude < 0.01 or flat.Unit:Dot(flatAim) > (finisher and -1.1 or 0.2)
				if flat.Magnitude < range + 2.5 * scale and math.abs(to.Y) < 6 * scale and inFront then
					local dir = flat.Magnitude > 0.01 and flat.Unit or flatAim
					Enemies.damage(e, a.Damage * dmg, dir * (a.Knockback or 40) * knock * 0.6 + Vector3.new(0, (a.Knockback or 40) * knock * 0.3, 0), player)
					hits = hits + 1
				end
			end
		end
		Net.fireAll("Fx", { Kind = "Slash", Pos = root.Position + flatAim * range * 0.5, Dir = flatAim, Color = a.Color, Finisher = finisher, Hits = hits, Player = player })
	end
end

-- Signature abilities, ultimates ---------------------------------------------------------

local function aimPoint(character, aim, maxDist)
	local params = RaycastParams.new()
	params.FilterType = Enum.RaycastFilterType.Exclude
	params.FilterDescendantsInstances = { character, workspace:FindFirstChild("Enemies") }
	local origin = character:GetPivot().Position + Vector3.new(0, 2, 0)
	local hit = workspace:Raycast(origin, aim * maxDist, params)
	return hit and hit.Position or origin + aim * maxDist
end

local function coneDamage(player, root, aim, range, halfAngle, damage, knock)
	local cosHalf = math.cos(math.rad(halfAngle))
	local hits = 0
	for _, e in ipairs(Enemies.list()) do
		if e.Health > 0 and e.Root.Parent then
			local to = e.Root.Position - root.Position
			local d = to.Magnitude
			if d < range and d > 0.1 and to.Unit:Dot(aim) > cosHalf then
				Enemies.damage(e, damage, to.Unit * knock + Vector3.new(0, knock * 0.3, 0), player)
				hits = hits + 1
			end
		end
	end
	return hits
end

local function dropAnvil(player, target)
	local anvil = Instance.new("Part")
	anvil.Name = "Anvil"
	anvil.Anchored = true
	anvil.CanCollide = false
	anvil.Size = Vector3.new(4, 3, 6)
	anvil.Color = Color3.fromRGB(70, 70, 80)
	anvil.Material = Enum.Material.Metal
	anvil.CFrame = CFrame.new(target + Vector3.new(0, 60, 0))
	anvil.Parent = workspace
	local tween = TweenService:Create(anvil, TweenInfo.new(0.7, Enum.EasingStyle.Quad, Enum.EasingDirection.In), { CFrame = CFrame.new(target + Vector3.new(0, 1.5, 0)) })
	tween:Play()
	tween.Completed:Connect(function()
		hitEnemiesInSphere(target, 13, 70, 1.4, player)
		Net.fireAll("Fx", { Kind = "Burst", Pos = target, Color = Color3.fromRGB(255, 230, 120), Count = 30, Gravity = true })
		Net.fireAll("Fx", { Kind = "Shake", Pos = target, Amount = 0.5 })
		task.delay(1.5, function()
			anvil:Destroy()
		end)
	end)
end

local function earthPillar(player, root)
	local down = workspace:Raycast(root.Position, Vector3.new(0, -12, 0))
	local ground = down and down.Position.Y or root.Position.Y - 3
	local rock = Instance.new("Part")
	rock.Name = "EarthPillar"
	rock.Anchored = true
	rock.Size = Vector3.new(6, 1, 6)
	rock.Color = Color3.fromRGB(115, 92, 72)
	rock.Material = Enum.Material.Slate
	rock.CFrame = CFrame.new(root.Position.X, ground - 0.5, root.Position.Z)
	rock.Parent = workspace
	TweenService:Create(rock, TweenInfo.new(0.35, Enum.EasingStyle.Back), { Size = Vector3.new(6, 14, 6), CFrame = CFrame.new(root.Position.X, ground + 6.5, root.Position.Z) }):Play()
	Net.fireAll("Fx", { Kind = "Burst", Pos = Vector3.new(root.Position.X, ground, root.Position.Z), Color = Color3.fromRGB(150, 115, 80), Count = 30, Gravity = true })
	task.delay(8, function()
		local sink = TweenService:Create(rock, TweenInfo.new(1), { Size = Vector3.new(6, 1, 6), CFrame = CFrame.new(rock.Position.X, ground - 0.5, rock.Position.Z) })
		sink:Play()
		sink.Completed:Wait()
		rock:Destroy()
	end)
end

local function onAbility(player, name, aim, held)
	aim = Net.unit(aim) or Vector3.new(0, 0, -1)
	local st = PlayerState.get(player)
	local character, _, root = charOf(player)
	if not st or not character or type(name) ~= "string" then
		return
	end
	local hero = st.Hero
	if name == "Ult" then
		if hero == "Ben" then
			return
		end
		if st.Meter < 100 or not canAttack(st, character) then
			return
		end
		PlayerState.addMeter(st, -100)
		local def = Config.Heroes[hero]
		shockwave(root.Position, 107, 150, 160, def.Accent, player)
		Net.fire("Notify", player, { Banner = def.Ult .. "!" })
		return
	end
	if name == "Breath" then
		if hero == "Jake" and held and st.Energy > 5 and canAttack(st, character) then
			breathing[player] = aim
		else
			breathing[player] = nil
		end
		Net.fireAll("Fx", { Kind = "Breath", Player = player, On = breathing[player] ~= nil })
		return
	end
	if not canAttack(st, character) then
		return
	end
	if name == "Sig" then
		if hero == "Timmy" and PlayerState.ready(st, "Sig", 1.5) then
			dropAnvil(player, aimPoint(character, aim, 200))
			Net.fire("Notify", player, { Text = "POOF! Anvil!", Color = Color3.fromRGB(255, 230, 80), Time = 1 })
		elseif hero == "XJ9" and PlayerState.ready(st, "Sig", 3) then
			for _ = 1, 6 do
				local v = aim * 50 + Vector3.new(math.random(-20, 20), 30, math.random(-20, 20))
				fire(player, root.Position + Vector3.new(0, 3, 0), v, { Damage = 30, Radius = 0.9, Knockback = 50, Color = Color3.fromRGB(255, 210, 90) }, 1, 1, true)
			end
		end
	elseif name == "Sig2" then
		if hero == "Danny" and st.Energy >= 30 and PlayerState.ready(st, "Sig2", 4) then
			st.Energy = st.Energy - 30
			coneDamage(player, root, aim, 47, 40, 40, 60)
			Net.fireAll("Fx", { Kind = "Wail", Player = player, Dir = aim })
			Net.fire("Notify", player, { Text = "GHOST WAIL!", Color = Color3.fromRGB(120, 255, 150), Time = 1 })
		elseif hero == "Aang" and PlayerState.ready(st, "Sig2", 2) then
			earthPillar(player, root)
		end
	end
end

local function onPound(player)
	local st = PlayerState.get(player)
	local character, _, root = charOf(player)
	if not st or not character or not PlayerState.ready(st, "Pound", 0.7) then
		return
	end
	local scale = math.max(1, bodyScale(character))
	local def = Config.Heroes[st.Hero]
	shockwave(root.Position - Vector3.new(0, 2.5 * scale, 0), Config.Moves.PoundRadius * math.sqrt(scale), 170, Config.Moves.PoundDamage * scale, def and def.Accent or Color3.new(1, 1, 1), player)
end

function Combat.init(systems)
	Net, PlayerState, Enemies, World = systems.Net, systems.Players, systems.Enemies, systems.World
	Net.on("Attack", 20, onAttack)
	Net.on("Ability", 10, onAbility)
	Net.on("Pound", 3, onPound)
end

function Combat.start()
	Players.PlayerRemoving:Connect(function(player)
		breathing[player] = nil
		regenAt[player] = nil
	end)
	RunService.Heartbeat:Connect(function(dt)
		stepProjectiles(dt)
		for i = #shockwaves, 1, -1 do
			local w = shockwaves[i]
			w.Radius = w.Radius + w.Speed * dt
			hitEnemiesInSphere(w.Center, w.Radius, w.Damage, 1.5, w.Attacker, w.Hit)
			if w.Radius >= w.Max then
				table.remove(shockwaves, i)
			end
		end
		for player, aim in pairs(breathing) do
			local st = PlayerState.get(player)
			local character, _, root = charOf(player)
			if not st or not character or st.Energy <= 0 then
				breathing[player] = nil
				Net.fireAll("Fx", { Kind = "Breath", Player = player, On = false })
			else
				st.Energy = math.max(0, st.Energy - 28 * dt)
				coneDamage(player, root, aim, 27, 25, 70 * dt, 3)
			end
		end
		local now = os.clock()
		for player, st in pairs(PlayerState.all()) do
			local character, humanoid, root = charOf(player)
			if character then
				-- Regeneration after a few seconds without damage.
				if now > (regenAt[player] or 0) and humanoid.Health < humanoid.MaxHealth then
					humanoid.Health = math.min(humanoid.MaxHealth, humanoid.Health + Config.Player.Regen * dt)
				end
				-- Cannonbolt rolls through enemies.
				local form = PlayerState.form(st)
				if form and form.Roll then
					local v = root.AssemblyLinearVelocity
					if Vector3.new(v.X, 0, v.Z).Magnitude > 17 then
						for _, e in ipairs(Enemies.list()) do
							if e.Health > 0 and e.Root.Parent and (e.Root.Position - root.Position).Magnitude < 6 and (e.RolledAt or 0) < now then
								e.RolledAt = now + 0.4
								Enemies.damage(e, 80, v.Unit * 50 + Vector3.new(0, 20, 0), player)
							end
						end
					end
				end
			end
		end
	end)
end

return Combat
