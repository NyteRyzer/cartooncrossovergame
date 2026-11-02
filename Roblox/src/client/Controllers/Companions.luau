--!nonstrict
-- Sidekicks drawn on every client: Cosmo & Wanda orbit any Timmy (and ride
-- below him as his cloud-board in flight), and Goddard trots after any Jimmy.

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")

local Companions = {}
local pals = {} -- [character] = { Models, Kind, Pos, Yaw }
local folder

local function ballPart(model, size, color, neon)
	local p = Instance.new("Part")
	p.Anchored = true
	p.CanCollide = false
	p.CanQuery = false
	p.CastShadow = false
	p.Size = size
	p.Color = color
	p.Material = neon and Enum.Material.Neon or Enum.Material.SmoothPlastic
	local mesh = Instance.new("SpecialMesh")
	mesh.MeshType = Enum.MeshType.Sphere
	mesh.Parent = p
	p.Parent = model
	return p
end

local function boxPart(model, size, color)
	local p = Instance.new("Part")
	p.Anchored = true
	p.CanCollide = false
	p.CanQuery = false
	p.Size = size
	p.Color = color
	p.Material = Enum.Material.SmoothPlastic
	p.TopSurface = Enum.SurfaceType.Smooth
	p.Parent = model
	return p
end

-- A fairy: { part, offset } pairs posed relative to a centre each frame.
local function fairy(hair, shirt)
	local model = Instance.new("Model")
	model.Name = "Fairy"
	local pieces = {
		{ ballPart(model, Vector3.new(0.6, 0.6, 0.6), Color3.fromRGB(250, 215, 185)), CFrame.new(0, 0.2, 0) },
		{ ballPart(model, Vector3.new(0.8, 0.7, 0.8), hair), CFrame.new(0, 0.6, 0.05) },
		{ boxPart(model, Vector3.new(0.4, 0.55, 0.3), shirt), CFrame.new(0, -0.3, 0) },
		{ ballPart(model, Vector3.new(0.25, 0.25, 0.25), Color3.fromRGB(255, 220, 60), true), CFrame.new(0, 1.15, 0) },
		{ boxPart(model, Vector3.new(0.5, 0.35, 0.05), Color3.fromRGB(220, 240, 255)), CFrame.new(0.3, -0.05, 0.25), true },
		{ boxPart(model, Vector3.new(0.5, 0.35, 0.05), Color3.fromRGB(220, 240, 255)), CFrame.new(-0.3, -0.05, 0.25), true },
	}
	model.Parent = folder
	return { Model = model, Pieces = pieces }
end

local function goddard()
	local model = Instance.new("Model")
	model.Name = "Goddard"
	local grey, dark, red = Color3.fromRGB(210, 215, 225), Color3.fromRGB(70, 75, 90), Color3.fromRGB(255, 70, 70)
	local pieces = {
		{ boxPart(model, Vector3.new(1.4, 1.1, 2.2), grey), CFrame.new(0, 1.4, 0) },
		{ boxPart(model, Vector3.new(1.1, 1.0, 1.1), grey), CFrame.new(0, 2.3, -1.2) },
		{ boxPart(model, Vector3.new(0.8, 0.5, 0.6), dark), CFrame.new(0, 2.1, -1.9) },
		{ ballPart(model, Vector3.new(0.3, 0.3, 0.3), red, true), CFrame.new(0.3, 2.5, -1.75) },
		{ ballPart(model, Vector3.new(0.3, 0.3, 0.3), red, true), CFrame.new(-0.3, 2.5, -1.75) },
		{ boxPart(model, Vector3.new(0.2, 0.9, 0.5), dark), CFrame.new(0.5, 3.0, -1.1) },
		{ boxPart(model, Vector3.new(0.2, 0.9, 0.5), dark), CFrame.new(-0.5, 3.0, -1.1) },
		{ boxPart(model, Vector3.new(0.12, 0.8, 0.12), dark), CFrame.new(0, 2.0, 1.2) * CFrame.Angles(-0.6, 0, 0) },
	}
	for _, x in ipairs({ -0.45, 0.45 }) do
		for _, z in ipairs({ -0.7, 0.7 }) do
			table.insert(pieces, { boxPart(model, Vector3.new(0.3, 0.9, 0.3), dark), CFrame.new(x, 0.45, z), "leg" })
		end
	end
	model.Parent = folder
	return { Model = model, Pieces = pieces }
end

local function pose(rig, center, extra)
	for _, piece in ipairs(rig.Pieces) do
		local cf = center * piece[2]
		if extra then
			cf = extra(piece, cf) or cf
		end
		piece[1].CFrame = cf
	end
end

function Companions.start()
	folder = Instance.new("Folder")
	folder.Name = "Companions"
	folder.Parent = workspace
	RunService.RenderStepped:Connect(function(dt)
		local t = os.clock()
		local seen = {}
		for _, p in ipairs(Players:GetPlayers()) do
			local character = p.Character
			local root = character and character:FindFirstChild("HumanoidRootPart")
			local hero = character and character:GetAttribute("Hero")
			local form = character and character:GetAttribute("Form")
			local kind = (hero == "Timmy" and "Fairies") or (hero == "Jimmy" and "Goddard") or nil
			if form then
				kind = nil
			end
			if root and kind then
				seen[character] = true
				local entry = pals[character]
				if entry and entry.Kind ~= kind then
					for _, m in ipairs(entry.Rigs) do
						m.Model:Destroy()
					end
					entry = nil
				end
				if not entry then
					if kind == "Fairies" then
						entry = { Kind = kind, Rigs = { fairy(Color3.fromRGB(90, 210, 100), Color3.fromRGB(245, 245, 250)), fairy(Color3.fromRGB(245, 120, 185), Color3.fromRGB(250, 220, 80)) } }
					else
						entry = { Kind = kind, Rigs = { goddard() }, Pos = root.Position, Yaw = 0 }
					end
					pals[character] = entry
				end
				if kind == "Fairies" then
					local flying = character:GetAttribute("MvFlying")
					for i, rig in ipairs(entry.Rigs) do
						local center
						if flying then
							center = root.CFrame * CFrame.new(i == 1 and -0.8 or 0.8, -3.4, 0)
						else
							local a = t * 2 + (i - 1) * math.pi
							center = CFrame.new(root.Position + Vector3.new(math.cos(a) * 3, 2.2 + math.sin(t * 4 + i) * 0.3, math.sin(a) * 3)) * CFrame.Angles(0, -a, 0)
						end
						local flap = math.sin(t * 30) * 0.5
						pose(rig, center, function(piece, cf)
							if piece[3] then
								return cf * CFrame.Angles(0, piece[2].X > 0 and flap or -flap, 0)
							end
						end)
					end
				else
					-- Goddard follows a few studs behind and to the side.
					local want = root.Position + root.CFrame:VectorToWorldSpace(Vector3.new(4, 0, 4))
					local params = RaycastParams.new()
					params.FilterType = Enum.RaycastFilterType.Exclude
					params.FilterDescendantsInstances = { character, folder }
					local ground = workspace:Raycast(want + Vector3.new(0, 6, 0), Vector3.new(0, -40, 0), params)
					if ground then
						want = ground.Position
					else
						want = want - Vector3.new(0, 3, 0)
					end
					local to = want - entry.Pos
					local speed = math.min(to.Magnitude * 4, 60)
					if to.Magnitude > 0.2 then
						entry.Pos = entry.Pos + to.Unit * math.min(to.Magnitude, speed * dt)
						entry.Yaw = math.atan2(-to.X, -to.Z)
					end
					if to.Magnitude > 120 then
						entry.Pos = want
					end
					local trot = math.sin(t * 14) * math.clamp(speed / 20, 0, 1)
					pose(entry.Rigs[1], CFrame.new(entry.Pos) * CFrame.Angles(0, entry.Yaw, 0), function(piece, cf)
						if piece[3] == "leg" then
							local s = (piece[2].X > 0) == (piece[2].Z > 0) and 1 or -1
							return cf * CFrame.Angles(trot * 0.6 * s, 0, 0)
						end
					end)
				end
			end
		end
		for character, entry in pairs(pals) do
			if not seen[character] then
				for _, m in ipairs(entry.Rigs) do
					m.Model:Destroy()
				end
				pals[character] = nil
			end
		end
	end)
end

return Companions
