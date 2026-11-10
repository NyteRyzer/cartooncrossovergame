--!nonstrict
-- Timed courses through glowing gates: the Nexus Flight Ring Challenge and
-- Retroville's Hover Race. Each player runs their own attempt; passing gate 1
-- starts the clock, the last gate stops it.

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local MapData = require(ReplicatedStorage:WaitForChild("Shared"):WaitForChild("MapData"))

local Courses = {}
local Net, Profiles, Progression
local courses = {}  -- { Name, Gates = { { Part, Pos, Radius } } }
local runs = {}     -- [player][courseIndex] = { Next, Start, Best }

function Courses.init(systems)
	Net, Profiles, Progression = systems.Net, systems.Profiles, systems.Progression
end

function Courses.start()
	for levelName, level in pairs(MapData.Levels) do
		local folder = workspace:FindFirstChild(levelName)
		if folder and level.Course then
			local course = { Name = level.Course.Name, Gates = {} }
			for _, gateName in ipairs(level.Course.Gates) do
				local part = folder:FindFirstChild(gateName, true)
				if part then
					local r = math.max(part.Size.X, part.Size.Y, part.Size.Z) * 0.5
					table.insert(course.Gates, { Part = part, Pos = part.Position, Radius = r })
				end
			end
			if #course.Gates > 0 then
				table.insert(courses, course)
			end
		end
	end
	Players.PlayerRemoving:Connect(function(player)
		runs[player] = nil
	end)
	RunService.Heartbeat:Connect(function()
		local now = os.clock()
		for _, player in ipairs(Players:GetPlayers()) do
			local character = player.Character
			local root = character and character:FindFirstChild("HumanoidRootPart")
			if root then
				-- Hover-car drivers count with the car's position.
				local humanoid = character:FindFirstChildOfClass("Humanoid")
				local pos = humanoid and humanoid.SeatPart and humanoid.SeatPart.Position or root.Position
				local mine = runs[player]
				if not mine then
					mine = {}
					runs[player] = mine
				end
				for ci, course in ipairs(courses) do
					local run = mine[ci]
					if not run then
						run = { Next = 1, Start = 0, Best = nil }
						mine[ci] = run
					end
					local gate = course.Gates[run.Next]
					if gate and (gate.Pos - pos).Magnitude < gate.Radius then
						if run.Next == 1 then
							run.Start = now
							Net.fire("Notify", player, { Text = course.Name .. ": GO!", Color = Color3.fromRGB(140, 255, 200), Time = 1.5 })
						end
						run.Next = run.Next + 1
						if run.Next > #course.Gates then
							local time = now - run.Start
							-- Faster than anything in the game can fly the course = not a real run.
							local minTime = 0
							for gi = 2, #course.Gates do
								minTime = minTime + (course.Gates[gi].Pos - course.Gates[gi - 1].Pos).Magnitude / 160
							end
							local legit = time >= minTime and now >= (run.RewardAt or 0)
							local data = Profiles.get(player)
							local saved = data and data.Best[course.Name]
							run.Best = run.Best or saved
							local best = not run.Best or time < run.Best
							if best and legit then
								run.Best = time
								if data then
									data.Best[course.Name] = math.floor(time * 100) / 100
								end
							end
							if legit then
								run.RewardAt = now + 20
								Progression.award(player, best and 100 or 50, best and 60 or 25, course.Name)
							end
							Net.fire("Notify", player, { Banner = ("%s: %.2fs%s"):format(course.Name, time, best and "  NEW BEST!" or ""), Color = Color3.fromRGB(140, 255, 200) })
							run.Next = 1
						end
						Net.fire("Fx", player, { Kind = "Gate", Course = ci, Next = run.Next, Gate = course.Gates[run.Next] and course.Gates[run.Next].Part, Count = #course.Gates, Name = course.Name, Best = run.Best })
					end
				end
			end
		end
	end)
end

return Courses
