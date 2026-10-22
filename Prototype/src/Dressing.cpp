// Procedural set-dressing for the prototype's levels.
//
// The greybox layouts (shared with the Unreal project) only place the big
// blocks. This pass furnishes them so they read like the shows: Retroville's
// '50s-futurist suburb (streets, lawns, picket fences, trees, streetlights),
// Googie downtown towers, and its landmarks; and the Nexus Observatory's
// floor patterns, energy beams and floating asteroids.
//
// Everything here becomes extra Blocks flagged Decor (mostly non-solid; tree
// trunks, the school bus and similar are solid), so it shares collision and
// the static batch with the layout geometry.

#include "Dressing.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>

namespace
{
	constexpr float Deg2Rad = 3.14159265358979f / 180.0f;

	struct Dresser
	{
		Level& L;
		unsigned Seed = 1234u;
		int Serial = 0;

		float Rand()
		{
			Seed = Seed * 1664525u + 1013904223u;
			return static_cast<float>((Seed >> 8) & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
		}
		float Range(float A, float B) { return A + (B - A) * Rand(); }
		template <typename T> const T& Pick(const T* Items, int Count) { return Items[static_cast<int>(Rand() * Count) % Count]; }

		Block& Add(const char* Prefix, Shape Kind, Vector3 Center, Vector3 Size, Color Tint, float Emissive = 0.0f,
		           float Yaw = 0.0f, float Pitch = 0.0f, bool Solid = false)
		{
			Block B;
			B.Label = std::string("Decor_") + Prefix + "_" + std::to_string(Serial++);
			B.Folder = "Decor";
			B.Kind = Kind;
			B.Center = Center;
			B.Size = Size;
			B.YawDeg = Yaw;
			B.PitchDeg = Pitch;
			B.Solid = Solid;
			B.Decor = true;
			B.HasTint = true;
			B.Tint = Tint;
			B.Emissive = Emissive;
			FinalizeBlock(B);
			std::vector<Block>& Into = Solid ? L.Blocks : L.Decor;
			Into.push_back(B);
			return Into.back();
		}

		// Box in a parent's local frame (yaw in degrees around Z).
		Block& Local(const char* Prefix, Shape Kind, Vector3 Origin, float Yaw, Vector3 Offset, Vector3 Size, Color Tint,
		             float Emissive = 0.0f, float Pitch = 0.0f, bool Solid = false)
		{
			const float C = std::cos(Yaw * Deg2Rad), S = std::sin(Yaw * Deg2Rad);
			const Vector3 P{ Origin.x + Offset.x * C - Offset.y * S, Origin.y + Offset.x * S + Offset.y * C, Origin.z + Offset.z };
			return Add(Prefix, Kind, P, Size, Tint, Emissive, Yaw, Pitch, Solid);
		}

		void Tree(Vector3 Ground, float Scale = 1.0f)
		{
			static const Color Leaves[] = { { 70, 165, 70, 255 }, { 90, 180, 75, 255 }, { 60, 150, 85, 255 }, { 105, 190, 80, 255 } };
			const Color Leaf = Pick(Leaves, 4);
			const float H = Range(240.0f, 330.0f) * Scale;
			Add("TreeTrunk", Shape::Cylinder, { Ground.x, Ground.y, Ground.z + H * 0.5f }, { 45 * Scale, 45 * Scale, H }, { 125, 85, 55, 255 }, 0, 0, 0, true);
			const float R = Range(300.0f, 420.0f) * Scale;
			Add("TreeTop", Shape::Sphere, { Ground.x, Ground.y, Ground.z + H + R * 0.35f }, { R, R, R * 0.9f }, Leaf);
			Add("TreeTop", Shape::Sphere, { Ground.x + R * 0.25f, Ground.y - R * 0.15f, Ground.z + H + R * 0.75f }, { R * 0.7f, R * 0.7f, R * 0.65f }, Leaf);
		}

		void StreetLight(Vector3 Ground, float Yaw)
		{
			Add("LampPost", Shape::Cylinder, { Ground.x, Ground.y, Ground.z + 280 }, { 22, 22, 560 }, { 70, 75, 90, 255 });
			Local("LampArm", Shape::Cube, Ground, Yaw, { 70, 0, 555 }, { 150, 14, 14 }, { 70, 75, 90, 255 });
			Local("Lamp", Shape::Sphere, Ground, Yaw, { 140, 0, 540 }, { 60, 60, 45 }, { 255, 240, 190, 255 }, 1.0f);
		}

		// Road along X (bAlongX) or Y, centred on Line, from A to B.
		void Road(bool bAlongX, float Line, float A, float B, float Z)
		{
			const float Len = B - A, Mid = (A + B) * 0.5f;
			const Vector3 C = bAlongX ? Vector3{ Mid, Line, Z } : Vector3{ Line, Mid, Z };
			auto Sz = [&](float Along, float Across, float H) { return bAlongX ? Vector3{ Along, Across, H } : Vector3{ Across, Along, H }; };
			Add("Asphalt", Shape::Cube, C, Sz(Len, 1000, 4), { 72, 74, 84, 255 });
			for (int s = -1; s <= 1; s += 2)
			{
				const Vector3 Walk = bAlongX ? Vector3{ Mid, Line + s * 630.0f, Z + 2 } : Vector3{ Line + s * 630.0f, Mid, Z + 2 };
				Add("Sidewalk", Shape::Cube, Walk, Sz(Len, 260, 8), { 205, 205, 200, 255 });
			}
			for (float T = A + 350.0f; T < B - 300.0f; T += 700.0f)
			{
				const Vector3 D = bAlongX ? Vector3{ T, Line, Z + 2.5f } : Vector3{ Line, T, Z + 2.5f };
				Add("Dash", Shape::Cube, D, Sz(300, 40, 1), { 245, 205, 60, 255 }, 0.2f);
			}
			for (float T = A + 1300.0f; T < B - 600.0f; T += 2750.0f)
			{
				const Vector3 P = bAlongX ? Vector3{ T, Line + 780.0f, Z } : Vector3{ Line + 780.0f, T, Z };
				StreetLight(P, bAlongX ? -90.0f : 180.0f);
			}
		}
	};

	struct Grid
	{
		std::vector<float> Xs, Ys;
	};

	Grid CollectGrid(const Level& L, const char* Prefix)
	{
		std::set<float> Xs, Ys;
		for (const Block& B : L.Blocks)
		{
			if (B.Label.rfind(Prefix, 0) == 0)
			{
				// Cells are spaced on a fixed pitch; snap away the size jitter.
				Xs.insert(std::round(B.Center.x / 50.0f) * 50.0f);
				Ys.insert(std::round(B.Center.y / 50.0f) * 50.0f);
			}
		}
		return Grid{ std::vector<float>(Xs.begin(), Xs.end()), std::vector<float>(Ys.begin(), Ys.end()) };
	}

	void StreetGrid(Dresser& D, const Grid& G, float Pitch, float Z)
	{
		if (G.Xs.empty() || G.Ys.empty())
		{
			return;
		}
		const float X0 = G.Xs.front() - Pitch * 0.5f, X1 = G.Xs.back() + Pitch * 0.5f;
		const float Y0 = G.Ys.front() - Pitch * 0.5f, Y1 = G.Ys.back() + Pitch * 0.5f;
		for (float Y : G.Ys) D.Road(true, Y - Pitch * 0.5f, X0, X1, Z);
		D.Road(true, Y1, X0, X1, Z);
		for (float X : G.Xs) D.Road(false, X + Pitch * 0.5f, Y0, Y1, Z + 0.6f);
		D.Road(false, X0, Y0, Y1, Z + 0.6f);
	}

	void House(Dresser& D, Block& H, bool bNeutron)
	{
		static const Color Doors[] = { { 170, 50, 45, 255 }, { 60, 90, 160, 255 }, { 90, 140, 80, 255 }, { 120, 80, 50, 255 } };
		static const Color Roofs[] = { { 90, 110, 165, 255 }, { 185, 80, 70, 255 }, { 110, 90, 80, 255 }, { 80, 140, 120, 255 } };
		static const Color Lawns[] = { { 120, 200, 95, 255 }, { 110, 192, 90, 255 }, { 128, 206, 100, 255 } };
		const Vector3 C = H.Center;
		const float Hx = H.Half.x, Hy = H.Half.y, Hz = H.Half.z;
		const float Front = C.y - Hy;
		const float Ground = C.z - Hz;
		H.RoofHeight = std::min(H.Size.x, H.Size.y) * 0.45f;
		H.RoofTint = bNeutron ? Color{ 70, 110, 190, 255 } : D.Pick(Roofs, 4);

		// Lot: lawn, driveway, path to the door.
		D.Add("Lawn", Shape::Cube, { C.x, C.y, Ground + 3 }, { 3960, 3960, 6 }, D.Pick(Lawns, 3));
		const float Curb = C.y - 1990.0f;
		const float DriveX = C.x + Hx * 0.45f;
		D.Add("Driveway", Shape::Cube, { DriveX, (Front + Curb) * 0.5f, Ground + 7 }, { 420, Front - Curb, 6 }, { 190, 190, 192, 255 });
		const float DoorX = C.x - Hx * 0.25f;
		D.Add("Path", Shape::Cube, { DoorX, (Front + Curb) * 0.5f, Ground + 7 }, { 150, Front - Curb, 6 }, { 215, 200, 180, 255 });

		// Facade: door, porch step, windows with frames, garage.
		D.Add("Door", Shape::Cube, { DoorX, Front - 4, Ground + 110 }, { 100, 8, 210 }, bNeutron ? Color{ 200, 40, 40, 255 } : D.Pick(Doors, 4));
		D.Add("Step", Shape::Cube, { DoorX, Front - 60, Ground + 12 }, { 200, 120, 24 }, { 200, 195, 190, 255 });
		auto Window = [&](float X, float Y, float Z, bool bSide) {
			const Vector3 Frame = bSide ? Vector3{ 8, 170, 150 } : Vector3{ 170, 8, 150 };
			const Vector3 Glass = bSide ? Vector3{ 10, 140, 120 } : Vector3{ 140, 10, 120 };
			D.Add("WinFrame", Shape::Cube, { X, Y, Z }, Frame, { 250, 250, 245, 255 });
			D.Add("Window", Shape::Cube, { X, Y, Z }, Glass, { 165, 215, 245, 255 }, 0.25f);
		};
		const bool bTwoStory = H.Size.z > 760.0f;
		Window(C.x - Hx * 0.65f, Front - 5, Ground + 200, false);
		Window(C.x + Hx * 0.05f, Front - 5, Ground + 200, false);
		Window(C.x - Hx - 5, C.y, Ground + 200, true);
		Window(C.x + Hx + 5, C.y, Ground + 200, true);
		if (bTwoStory)
		{
			Window(C.x - Hx * 0.5f, Front - 5, Ground + 520, false);
			Window(C.x + Hx * 0.4f, Front - 5, Ground + 520, false);
		}
		if (D.Rand() < 0.55f)
		{
			D.Add("Garage", Shape::Cube, { DriveX, Front - 4, Ground + 120 }, { 320, 8, 240 }, { 235, 232, 225, 255 });
		}
		// Chimney poking through the roof.
		D.Add("Chimney", Shape::Cube, { C.x + Hx * 0.45f, C.y + Hy * 0.2f, Ground + H.Size.z + H.RoofHeight * 0.45f },
		      { 90, 90, H.RoofHeight * 0.9f }, { 165, 75, 60, 255 });

		// Picket fence around the back and sides of the lot.
		const Color White{ 250, 250, 248, 255 };
		const float FB = C.y + 1900.0f, FS = 1900.0f;
		D.Add("Rail", Shape::Cube, { C.x, FB, Ground + 70 }, { FS * 2, 8, 14 }, White);
		D.Add("Rail", Shape::Cube, { C.x, FB, Ground + 30 }, { FS * 2, 8, 14 }, White);
		for (int s = -1; s <= 1; s += 2)
		{
			const float X = C.x + s * FS;
			D.Add("Rail", Shape::Cube, { X, C.y + 300, Ground + 70 }, { 8, 3200, 14 }, White);
			D.Add("Rail", Shape::Cube, { X, C.y + 300, Ground + 30 }, { 8, 3200, 14 }, White);
		}
		for (float X = C.x - FS; X <= C.x + FS + 1; X += 380.0f)
		{
			D.Add("Picket", Shape::Cube, { X, FB, Ground + 50 }, { 14, 14, 100 }, White);
		}
		for (float Y = C.y - 1300.0f; Y <= FB; Y += 380.0f)
		{
			D.Add("Picket", Shape::Cube, { C.x - FS, Y, Ground + 50 }, { 14, 14, 100 }, White);
			D.Add("Picket", Shape::Cube, { C.x + FS, Y, Ground + 50 }, { 14, 14, 100 }, White);
		}

		// Mailbox at the curb, bushes, backyard trees.
		D.Add("MailPost", Shape::Cube, { DriveX - 330, Curb + 120, Ground + 55 }, { 12, 12, 110 }, { 120, 90, 60, 255 });
		D.Add("Mailbox", Shape::Cube, { DriveX - 330, Curb + 120, Ground + 115 }, { 30, 50, 30 }, bNeutron ? Color{ 60, 120, 220, 255 } : Color{ 90, 95, 110, 255 });
		for (int b = 0; b < 3; ++b)
		{
			const float X = C.x - Hx * 0.9f + b * 130.0f;
			D.Add("Bush", Shape::Sphere, { X, Front - 90, Ground + 50 }, { 130, 110, 100 }, { 60, 140, 60, 255 });
		}
		const int Trees = 1 + static_cast<int>(D.Rand() * 2.0f);
		for (int t = 0; t < Trees; ++t)
		{
			D.Tree({ C.x + D.Range(-1500, 1500), C.y + D.Range(1100, 1650), Ground }, D.Range(0.8f, 1.15f));
		}
	}

	void Tower(Dresser& D, const Block& T)
	{
		static const Color Glass[] = { { 170, 220, 255, 255 }, { 200, 235, 255, 255 }, { 150, 205, 240, 255 } };
		const Vector3 C = T.Center;
		const float Ground = C.z - T.Half.z, Top = C.z + T.Half.z;
		const Color G = D.Pick(Glass, 3);
		// Window bands every floor.
		for (float Z = Ground + 250.0f; Z < Top - 150.0f; Z += 400.0f)
		{
			D.Add("WinBand", Shape::Cube, { C.x, C.y, Z }, { T.Size.x + 8, T.Size.y + 8, 150 }, G, 0.3f);
		}
		// Lobby doors on all four sides.
		D.Add("Lobby", Shape::Cube, { C.x, C.y, Ground + 150 }, { T.Size.x * 0.4f, T.Size.y + 12, 300 }, { 255, 235, 170, 255 }, 0.5f);
		// Googie rooftop: dome, spire, saucer ring or water tank.
		const float R = std::min(T.Size.x, T.Size.y);
		const int Kind = static_cast<int>(D.Rand() * 4.0f);
		if (Kind == 0)
		{
			D.Add("Dome", Shape::Sphere, { C.x, C.y, Top }, { R * 0.75f, R * 0.75f, R * 0.55f }, { 200, 215, 235, 255 });
			D.Add("Antenna", Shape::Cylinder, { C.x, C.y, Top + R * 0.35f + 300 }, { 25, 25, 600 }, { 200, 200, 210, 255 });
			D.Add("Beacon", Shape::Sphere, { C.x, C.y, Top + R * 0.35f + 620 }, { 70, 70, 70 }, { 255, 70, 70, 255 }, 1.0f);
		}
		else if (Kind == 1)
		{
			D.Add("Spire", Shape::Cone, { C.x, C.y, Top + 700 }, { R * 0.5f, R * 0.5f, 1400 }, { 230, 120, 90, 255 });
			D.Add("Beacon", Shape::Sphere, { C.x, C.y, Top + 1420 }, { 90, 90, 90 }, { 120, 230, 255, 255 }, 1.0f);
		}
		else if (Kind == 2)
		{
			D.Add("Saucer", Shape::Cylinder, { C.x, C.y, Top + 300 }, { R * 1.5f, R * 1.5f, 120 }, { 240, 200, 90, 255 });
			D.Add("SaucerStem", Shape::Cylinder, { C.x, C.y, Top + 150 }, { R * 0.3f, R * 0.3f, 300 }, { 200, 200, 210, 255 });
			D.Add("SaucerTop", Shape::Sphere, { C.x, C.y, Top + 380 }, { R * 0.6f, R * 0.6f, 260 }, { 170, 225, 255, 255 }, 0.3f);
		}
		else
		{
			for (int l = 0; l < 4; ++l)
			{
				const float A = (45.0f + l * 90.0f) * Deg2Rad;
				D.Add("TankLeg", Shape::Cube, { C.x + std::cos(A) * 160, C.y + std::sin(A) * 160, Top + 200 }, { 20, 20, 400 }, { 120, 110, 100, 255 });
			}
			D.Add("Tank", Shape::Cylinder, { C.x, C.y, Top + 600 }, { 480, 480, 400 }, { 150, 115, 85, 255 });
			D.Add("TankTop", Shape::Cone, { C.x, C.y, Top + 900 }, { 520, 520, 200 }, { 120, 90, 70, 255 });
		}
		// Neon sign on some fronts.
		if (D.Rand() < 0.4f)
		{
			static const Color Neon[] = { { 255, 90, 170, 255 }, { 90, 230, 255, 255 }, { 255, 210, 70, 255 } };
			D.Add("Neon", Shape::Cube, { C.x + T.Half.x * 0.6f, C.y - T.Half.y - 40, Ground + 700 }, { 120, 40, 700 }, D.Pick(Neon, 3), 1.0f);
		}
	}

	void DressRetroville(Dresser& D)
	{
		Level& L = D.L;
		// Street grids for the suburbs and downtown.
		StreetGrid(D, CollectGrid(L, "SuburbSW_House"), 5500.0f, 2.0f);
		StreetGrid(D, CollectGrid(L, "SuburbNW_House"), 5500.0f, 2.0f);
		StreetGrid(D, CollectGrid(L, "Downtown_Tower"), 7000.0f, 2.0f);
		D.Road(true, 0.0f, -50000.0f, 50000.0f, 1.0f);
		D.Road(false, 0.0f, -50000.0f, 50000.0f, 1.4f);

		// Houses and towers (iterate a snapshot: Add() grows the vector).
		const size_t Count = L.Blocks.size();
		for (size_t i = 0; i < Count; ++i)
		{
			Block& B = L.Blocks[i];
			if (B.Label.rfind("Suburb", 0) == 0) House(D, L.Blocks[i], false);
			else if (B.Label == "Neutron_House") House(D, L.Blocks[i], true);
			else if (B.Label.rfind("Downtown_Tower", 0) == 0) Tower(D, L.Blocks[i]);
		}

		// --- Neutron lab clubhouse + Jimmy's rocket ---------------------------
		if (const Block* Lab = L.FindBlock("Neutron_LabClubhouse"))
		{
			const Vector3 C = Lab->Center;
			const float Top = C.z + Lab->Half.z;
			D.Add("LabDoor", Shape::Cube, { C.x, C.y - Lab->Half.y - 4, C.z - 80 }, { 120, 8, 200 }, { 90, 60, 40, 255 });
			D.Add("LabRoof", Shape::Pyramid, { C.x, C.y, Top + 110 }, { 620, 620, 220 }, { 150, 40, 40, 255 });
			D.Add("DishPole", Shape::Cylinder, { C.x + 120, C.y, Top + 300 }, { 20, 20, 300 }, { 200, 200, 210, 255 });
			D.Add("Dish", Shape::Cone, { C.x + 120, C.y - 40, Top + 470 }, { 240, 240, 80 }, { 230, 230, 235, 255 }, 0, 0, 150);
			D.Add("LabSign", Shape::Cube, { C.x, C.y - Lab->Half.y - 6, Top - 60 }, { 260, 6, 60 }, { 255, 230, 80, 255 }, 0.6f);
			D.Tree({ C.x - 500, C.y + 300, 0 }, 1.3f);
		}
		{
			const Vector3 R{ -33300.0f, -33700.0f, 0.0f };
			D.Add("RocketBody", Shape::Cylinder, { R.x, R.y, 450 }, { 220, 220, 700 }, { 235, 235, 240, 255 }, 0, 0, 0, true);
			D.Add("RocketNose", Shape::Cone, { R.x, R.y, 950 }, { 220, 220, 300 }, { 220, 50, 45, 255 });
			D.Add("RocketWindow", Shape::Sphere, { R.x, R.y - 105, 650 }, { 80, 20, 80 }, { 120, 200, 255, 255 }, 0.6f);
			D.Add("RocketStripe", Shape::Cylinder, { R.x, R.y, 380 }, { 226, 226, 50 }, { 220, 50, 45, 255 });
			for (int f = 0; f < 3; ++f)
			{
				D.Local("RocketFin", Shape::Cube, R, f * 120.0f, { 150, 0, 170 }, { 160, 20, 240 }, { 220, 50, 45, 255 });
			}
			D.Add("LaunchPad", Shape::Cylinder, { R.x, R.y, 50 }, { 500, 500, 100 }, { 150, 150, 160, 255 });
		}

		// --- Lindbergh Elementary -------------------------------------------
		if (const Block* S = L.FindBlock("School_Main"))
		{
			const Vector3 C = S->Center;
			const float Front = C.y - S->Half.y;
			for (int Floor = 0; Floor < 2; ++Floor)
			{
				for (float X = C.x - S->Half.x + 400; X < C.x + S->Half.x - 300; X += 600.0f)
				{
					if (std::fabs(X - C.x) < 500.0f) continue;
					D.Add("SchoolWinFrame", Shape::Cube, { X, Front - 5, 300.0f + Floor * 500.0f }, { 340, 8, 240 }, { 250, 250, 245, 255 });
					D.Add("SchoolWin", Shape::Cube, { X, Front - 6, 300.0f + Floor * 500.0f }, { 300, 8, 200 }, { 170, 220, 245, 255 }, 0.25f);
				}
			}
			D.Add("SchoolTrim", Shape::Cube, { C.x, C.y, S->Size.z - 30 }, { S->Size.x + 30, S->Size.y + 30, 60 }, { 245, 240, 225, 255 });
			D.Add("SchoolDoor", Shape::Cube, { C.x, Front - 6, 160 }, { 400, 10, 320 }, { 120, 70, 45, 255 });
			D.Add("SchoolSteps", Shape::Cube, { C.x, Front - 200, 20 }, { 900, 400, 40 }, { 210, 205, 200, 255 });
			// Clock tower.
			D.Add("ClockTower", Shape::Cube, { C.x, Front + 300, S->Size.z + 700 }, { 600, 600, 1400 }, { 190, 90, 70, 255 }, 0, 0, 0, true);
			D.Add("ClockRoof", Shape::Pyramid, { C.x, Front + 300, S->Size.z + 1650 }, { 760, 760, 500 }, { 90, 100, 150, 255 });
			D.Add("ClockFace", Shape::Cylinder, { C.x, Front - 6, S->Size.z + 1000 }, { 380, 380, 10 }, { 252, 250, 235, 255 }, 0.6f, 0, 90);
			D.Add("ClockHand", Shape::Cube, { C.x, Front - 14, S->Size.z + 1060 }, { 16, 6, 140 }, { 30, 30, 35, 255 });
			D.Add("ClockHand", Shape::Cube, { C.x + 50, Front - 14, S->Size.z + 1000 }, { 110, 6, 14 }, { 30, 30, 35, 255 });
			// Flagpole and the school bus.
			D.Add("Flagpole", Shape::Cylinder, { C.x - 2500, Front - 1200, 800 }, { 20, 20, 1600 }, { 210, 210, 220, 255 });
			D.Add("Flag", Shape::Cube, { C.x - 2300, Front - 1200, 1450 }, { 380, 8, 220 }, { 60, 90, 200, 255 }, 0.1f);
			const Vector3 Bus{ C.x + 2600, Front - 1500, 0 };
			D.Add("Bus", Shape::Cube, { Bus.x, Bus.y, 190 }, { 1150, 300, 280 }, { 250, 195, 40, 255 }, 0, 0, 0, true);
			D.Add("BusWindows", Shape::Cube, { Bus.x, Bus.y, 250 }, { 1000, 310, 80 }, { 60, 70, 90, 255 });
			D.Add("BusStripe", Shape::Cube, { Bus.x, Bus.y, 140 }, { 1160, 310, 20 }, { 30, 30, 30, 255 });
			for (int w = -1; w <= 1; w += 2)
			{
				for (int s = -1; s <= 1; s += 2)
				{
					D.Add("BusWheel", Shape::Cylinder, { Bus.x + w * 380, Bus.y + s * 150, 50 }, { 100, 30, 100 }, { 30, 30, 35, 255 }, 0, 90, 90);
				}
			}
			for (int t = 0; t < 6; ++t) D.Tree({ C.x - S->Half.x + 600 + t * 1400, Front - 2200, 0 }, 1.1f);
		}

		// --- Candy Bar diner -------------------------------------------------
		if (const Block* B = L.FindBlock("CandyBar_Diner"))
		{
			const Vector3 C = B->Center;
			const float Front = C.y - B->Half.y;
			D.Add("DinerChrome", Shape::Cube, { C.x, C.y, 520 }, { B->Size.x + 20, B->Size.y + 20, 70 }, { 225, 228, 235, 255 });
			D.Add("DinerWindows", Shape::Cube, { C.x + 200, Front - 4, 280 }, { 1300, 8, 220 }, { 255, 235, 180, 255 }, 0.5f);
			D.Add("DinerDoor", Shape::Cube, { C.x - 650, Front - 5, 150 }, { 160, 8, 260 }, { 230, 60, 120, 255 });
			for (int k = 0; k < 6; ++k)
			{
				D.Add("Awning", Shape::Cube, { C.x - 750 + k * 300, Front - 160, 430 }, { 300, 320, 20 }, k % 2 ? Color{ 255, 255, 255, 255 } : Color{ 230, 60, 70, 255 }, 0, 0, -15);
			}
			D.Add("SignPole", Shape::Cylinder, { C.x + 1300, Front - 900, 700 }, { 50, 50, 1400 }, { 200, 200, 210, 255 });
			D.Add("SignBoard", Shape::Cube, { C.x + 1300, Front - 900, 1500 }, { 1000, 80, 420 }, { 255, 90, 170, 255 }, 0.9f);
			D.Add("SignInner", Shape::Cube, { C.x + 1300, Front - 945, 1500 }, { 800, 20, 240 }, { 120, 235, 255, 255 }, 1.0f);
			D.Add("SignStar", Shape::Sphere, { C.x + 1300, Front - 900, 1820 }, { 160, 160, 160 }, { 255, 230, 80, 255 }, 1.0f);
			D.Add("Parking", Shape::Cube, { C.x, Front - 1700, 4 }, { 3000, 1800, 6 }, { 80, 82, 92, 255 });
			for (int p = 0; p < 6; ++p)
			{
				D.Add("ParkLine", Shape::Cube, { C.x - 1250 + p * 500, Front - 1700, 8 }, { 20, 1200, 2 }, { 245, 245, 245, 255 }, 0.2f);
			}
		}

		// --- Park: pond, paths, trees, benches; silo hatch warning ring ------------
		if (const Block* P = L.FindBlock("Park_Lawn"))
		{
			const Vector3 C = P->Center;
			D.Add("Pond", Shape::Cylinder, { C.x - 3000, C.y - 2000, 12 }, { 3200, 2400, 10 }, { 90, 175, 235, 255 }, 0.2f);
			D.Add("PondEdge", Shape::Cylinder, { C.x - 3000, C.y - 2000, 9 }, { 3500, 2700, 8 }, { 200, 190, 160, 255 });
			D.Add("ParkPath", Shape::Cube, { C.x, C.y, 12 }, { 11000, 300, 6 }, { 215, 200, 160, 255 });
			D.Add("ParkPath", Shape::Cube, { C.x, C.y, 12 }, { 300, 11000, 6 }, { 215, 200, 160, 255 });
			D.Add("SiloRing", Shape::Cylinder, { C.x, C.y, 13 }, { 1600, 1600, 6 }, { 250, 210, 40, 255 }, 0.2f);
			for (int t = 0; t < 28; ++t)
			{
				const float A = D.Range(0, 360) * Deg2Rad, R = D.Range(2500, 5400);
				const Vector3 G{ C.x + std::cos(A) * R, C.y + std::sin(A) * R, 10 };
				if (std::fabs(G.x - C.x) < 400 || std::fabs(G.y - C.y) < 400) continue;
				D.Tree(G, D.Range(0.9f, 1.4f));
			}
			for (int b = 0; b < 4; ++b)
			{
				D.Add("Bench", Shape::Cube, { C.x + 600 + b * 900, C.y + 350, 45 }, { 300, 80, 20 }, { 140, 95, 60, 255 });
			}
		}

		// --- Retroland: entrance arch, coaster track, tents, carousel ----------------
		if (const Block* Pl = L.FindBlock("Retroland_Plaza"))
		{
			const Vector3 C = Pl->Center;
			const float North = C.y + Pl->Half.y;
			for (int s = -1; s <= 1; s += 2)
			{
				D.Add("ArchPillar", Shape::Cylinder, { C.x + s * 900, North, 600 }, { 200, 200, 1200 }, { 230, 60, 80, 255 }, 0, 0, 0, true);
			}
			D.Add("ArchSign", Shape::Cube, { C.x, North, 1350 }, { 2400, 200, 400 }, { 255, 210, 70, 255 }, 0.7f);
			for (int b = 0; b < 14; ++b)
			{
				D.Add("Bulb", Shape::Sphere, { C.x - 1100 + b * 170, North - 110, 1580 }, { 60, 60, 60 }, b % 2 ? Color{ 255, 90, 90, 255 } : Color{ 120, 230, 255, 255 }, 1.0f);
			}
			// Coaster rails between the ring of pillars.
			std::vector<Vector3> Tops;
			for (const Block& B : L.Blocks)
			{
				if (B.Label.rfind("Retroland_CoasterPillar", 0) == 0) Tops.push_back({ B.Center.x, B.Center.y, B.Center.z + B.Half.z });
			}
			std::sort(Tops.begin(), Tops.end(), [&](const Vector3& A, const Vector3& B) {
				return std::atan2(A.y - (C.y), A.x - C.x) < std::atan2(B.y - C.y, B.x - C.x); });
			for (size_t i = 0; i < Tops.size(); ++i)
			{
				const Vector3 A = Tops[i], B = Tops[(i + 1) % Tops.size()];
				const Vector3 M{ (A.x + B.x) * 0.5f, (A.y + B.y) * 0.5f, A.z + 30 };
				const float Len = std::sqrt((B.x - A.x) * (B.x - A.x) + (B.y - A.y) * (B.y - A.y));
				const float Yaw = std::atan2(B.y - A.y, B.x - A.x) / Deg2Rad;
				D.Add("CoasterTrack", Shape::Cube, M, { Len, 220, 60 }, { 230, 50, 50, 255 }, 0, Yaw);
				D.Add("CoasterRail", Shape::Cube, { M.x, M.y, M.z + 50 }, { Len, 260, 15 }, { 250, 250, 250, 255 }, 0, Yaw);
			}
			// Striped tents.
			for (int t = 0; t < 4; ++t)
			{
				const Vector3 P{ C.x - 4500 + t * 2200, C.y + 4200, 0 };
				D.Add("TentBase", Shape::Cylinder, { P.x, P.y, 200 }, { 900, 900, 400 }, t % 2 ? Color{ 255, 255, 255, 255 } : Color{ 230, 60, 70, 255 }, 0, 0, 0, true);
				D.Add("TentTop", Shape::Cone, { P.x, P.y, 650 }, { 1100, 1100, 500 }, t % 2 ? Color{ 230, 60, 70, 255 } : Color{ 255, 255, 255, 255 });
				D.Add("TentFlag", Shape::Cube, { P.x + 60, P.y, 980 }, { 120, 8, 70 }, { 255, 220, 60, 255 }, 0.4f);
			}
			// Carousel.
			const Vector3 K{ C.x + 3800, C.y + 1500, 0 };
			D.Add("CarouselBase", Shape::Cylinder, { K.x, K.y, 40 }, { 1600, 1600, 80 }, { 240, 200, 90, 255 }, 0, 0, 0, true);
			D.Add("CarouselRoof", Shape::Cone, { K.x, K.y, 700 }, { 1800, 1800, 450 }, { 120, 170, 240, 255 });
			for (int h = 0; h < 8; ++h)
			{
				const float A = h * 45.0f * Deg2Rad;
				D.Add("CarouselPole", Shape::Cylinder, { K.x + std::cos(A) * 600, K.y + std::sin(A) * 600, 300 }, { 20, 20, 520 }, { 240, 220, 120, 255 });
				D.Add("CarouselHorse", Shape::Cube, { K.x + std::cos(A) * 600, K.y + std::sin(A) * 600, 200 }, { 160, 50, 90 }, h % 2 ? Color{ 255, 255, 255, 255 } : Color{ 240, 150, 190, 255 }, 0, A / Deg2Rad + 90);
			}
		}

		// --- Horizon: rolling hills beyond the boundary, puffy clouds -------------------
		for (int h = 0; h < 26; ++h)
		{
			const float A = h * (360.0f / 26.0f) * Deg2Rad;
			const float R = D.Range(68000, 82000);
			const float W = D.Range(18000, 30000);
			D.Add("Hill", Shape::Sphere, { std::cos(A) * R, std::sin(A) * R, -2000 }, { W, W, D.Range(9000, 16000) },
			      h % 2 ? Color{ 95, 175, 90, 255 } : Color{ 80, 160, 95, 255 });
		}
		for (int c = 0; c < 36; ++c)
		{
			const Vector3 P{ D.Range(-70000, 70000), D.Range(-70000, 70000), D.Range(9000, 14000) };
			const int Puffs = 3 + static_cast<int>(D.Rand() * 3);
			for (int p = 0; p < Puffs; ++p)
			{
				const float S = D.Range(1600, 3200);
				D.Add("Cloud", Shape::Sphere, { P.x + (p - Puffs * 0.5f) * S * 0.6f, P.y + D.Range(-400, 400), P.z + D.Range(-200, 300) },
				      { S, S * 0.8f, S * 0.55f }, { 255, 255, 255, 255 }, 0.55f);
			}
		}
	}

	void DressNexus(Dresser& D)
	{
		static const Color Universe[] = { { 230, 60, 60, 255 }, { 245, 120, 190, 255 }, { 90, 255, 130, 255 }, { 240, 90, 40, 255 },
		                                  { 80, 220, 255, 255 }, { 255, 160, 40, 255 }, { 140, 255, 60, 255 }, { 120, 190, 255, 255 } };
		// Floor: concentric inlaid rings.
		const float Radii[] = { 5600, 4600, 3300, 2300, 1300 };
		const Color Tones[] = { { 110, 100, 170, 255 }, { 85, 75, 145, 255 }, { 120, 110, 185, 255 }, { 80, 70, 140, 255 }, { 130, 120, 195, 255 } };
		for (int r = 0; r < 5; ++r)
		{
			D.Add("FloorRing", Shape::Cylinder, { 0, 0, 1.0f + r * 0.8f }, { Radii[r] * 2, Radii[r] * 2, 2 }, Tones[r]);
		}
		// Glowing rim and energy beams from each terminal to the Rift Core.
		for (int i = 0; i < 64; ++i)
		{
			const float A = i * (360.0f / 64.0f) * Deg2Rad;
			D.Add("RimLight", Shape::Cube, { std::cos(A) * 5960, std::sin(A) * 5960, 130 }, { 120, 120, 30 }, { 120, 240, 255, 255 }, 1.0f, A / Deg2Rad);
		}
		for (int t = 0; t < 8; ++t)
		{
			const float A = t * 45.0f;
			D.Local("Beam", Shape::Cube, { 0, 0, 0 }, A, { 2350, 0, 10 }, { 2500, 60, 10 }, Universe[t], 1.0f);
		}
		// Floating asteroids around the platform.
		for (int a = 0; a < 26; ++a)
		{
			const float A = D.Range(0, 360) * Deg2Rad, R = D.Range(9500, 17000);
			const float S = D.Range(500, 1600);
			D.Add("Asteroid", Shape::Sphere, { std::cos(A) * R, std::sin(A) * R, D.Range(-3000, 3500) }, { S, S * D.Range(0.7f, 1.1f), S * 0.7f },
			      a % 3 ? Color{ 110, 95, 140, 255 } : Color{ 140, 120, 170, 255 });
			if (a % 4 == 0)
			{
				D.Add("Crystal", Shape::Cone, { std::cos(A) * R, std::sin(A) * R, 0 }, { S * 0.25f, S * 0.25f, S * 0.6f }, { 120, 255, 230, 255 }, 1.0f);
			}
		}
		// Distant planets.
		D.Add("Planet", Shape::Sphere, { 90000, 60000, 30000 }, { 30000, 30000, 30000 }, { 240, 140, 200, 255 }, 0.7f);
		D.Add("PlanetRing", Shape::Cylinder, { 90000, 60000, 30000 }, { 52000, 52000, 600 }, { 255, 220, 160, 255 }, 0.8f, 20, 15);
		D.Add("Planet", Shape::Sphere, { -110000, -40000, 45000 }, { 18000, 18000, 18000 }, { 120, 190, 255, 255 }, 0.7f);
		// Holo screens above the hub services.
		for (const Block& B : std::vector<Block>(D.L.Blocks))
		{
			if (B.Label.rfind("Service_", 0) == 0)
			{
				D.Add("Holo", Shape::Cube, { B.Center.x, B.Center.y, B.Center.z + B.Half.z + 220 }, { 300, 20, 180 }, { 120, 240, 255, 255 }, 1.0f, B.YawDeg + 90);
			}
		}
	}
}

void DressLevel(Level& L, int LevelIndex)
{
	Dresser D{ L };
	D.Seed = 777u + static_cast<unsigned>(LevelIndex) * 101u;
	// Dressing holds references into Blocks while adding to it: never reallocate.
	L.Blocks.reserve(L.Blocks.size() + 4000);
	L.Decor.reserve(40000);
	if (L.FileStem == "retroville_slice")
	{
		DressRetroville(D);
	}
	else if (L.FileStem == "nexus_observatory")
	{
		DressNexus(D);
	}
}
