#include "Game.h"

#include "Dressing.h"
#include "Traversal/HoverMath.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace
{
	constexpr float Gravity = -1960.0f; // matches Config/DefaultEngine.ini
	constexpr float Deg2Rad = 3.14159265358979f / 180.0f;
	constexpr float DialTimeDilation = 0.25f;

	Vector3 Add(Vector3 A, Vector3 B) { return Vector3{ A.x + B.x, A.y + B.y, A.z + B.z }; }
	Vector3 Sub(Vector3 A, Vector3 B) { return Vector3{ A.x - B.x, A.y - B.y, A.z - B.z }; }
	Vector3 Mul(Vector3 A, float S) { return Vector3{ A.x * S, A.y * S, A.z * S }; }
	float Dot(Vector3 A, Vector3 B) { return A.x * B.x + A.y * B.y + A.z * B.z; }
	float Len(Vector3 A) { return std::sqrt(Dot(A, A)); }
	float Len2D(Vector3 A) { return std::sqrt(A.x * A.x + A.y * A.y); }
	Vector3 Norm(Vector3 A) { const float L = Len(A); return L > 1e-5f ? Mul(A, 1.0f / L) : Vector3{}; }
	Vector3 YawDir(float YawDeg) { return Vector3{ std::cos(YawDeg * Deg2Rad), std::sin(YawDeg * Deg2Rad), 0.0f }; }
	float YawOf(Vector3 D) { return std::atan2(D.y, D.x) / Deg2Rad; }

	float AngleDelta(float From, float To)
	{
		float D = std::fmod(To - From + 540.0f, 360.0f) - 180.0f;
		return D;
	}

	float Approach(float Current, float Target, float MaxStep)
	{
		const float D = Target - Current;
		return std::fabs(D) <= MaxStep ? Target : Current + (D > 0.0f ? MaxStep : -MaxStep);
	}

	float Frand(unsigned& Seed)
	{
		Seed = Seed * 1664525u + 1013904223u;
		return static_cast<float>((Seed >> 8) & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
	}

	unsigned RandSeed = 12345u;
	float Rand01() { return Frand(RandSeed); }
	float RandRange(float A, float B) { return A + (B - A) * Rand01(); }

	// ---------------------------------------------------------------------
	// Roster (docs/GAME_BIBLE.md). Line weights follow docs/RENDERING.md.
	// ---------------------------------------------------------------------
	const HeroDef Heroes[] = {
		{ HeroId::Jimmy, "Jimmy Neutron", "Retroville", "Hover Car + Goddard Jetpack (Q)", "Goddard Jetpack", "Hyper-Cube Anomaly",
		  { 250, 210, 180, 255 }, { 210, 40, 40, 255 }, { 60, 90, 180, 255 }, { 120, 70, 35, 255 }, { 255, 80, 80, 255 },
		  650.0f, 900.0f, false, 0.0f, 0.0f, 0, false, 1.2f,
		  { AttackKind::Projectile, 18.0f, 0.22f, 6500.0f, 0.0f, 18.0f, 250.0f, 1, { 255, 60, 60, 255 }, "Laser Blaster" } },
		{ HeroId::Timmy, "Timmy Turner", "Dimmsdale", "Fairy Cloud-Board Flight (Tab)", "Anvil Drop", "Da Rules Loophole",
		  { 250, 210, 180, 255 }, { 240, 110, 160, 255 }, { 60, 90, 180, 255 }, { 120, 70, 35, 255 }, { 255, 230, 80, 255 },
		  650.0f, 900.0f, true, 1500.0f, 2600.0f, 0, false, 2.8f,
		  { AttackKind::Projectile, 12.0f, 0.25f, 4500.0f, 0.0f, 22.0f, 200.0f, 3, { 255, 230, 80, 255 }, "Magic Stars" } },
		{ HeroId::Danny, "Danny Phantom", "Amity Park", "Ghost Flight (Tab) + Intangibility (Q)", "Intangibility", "Ghost Zone Mastery",
		  { 250, 215, 190, 255 }, { 30, 30, 36, 255 }, { 240, 240, 245, 255 }, { 245, 245, 250, 255 }, { 80, 255, 120, 255 },
		  650.0f, 900.0f, true, 1800.0f, 3500.0f, 0, false, 3.6f,
		  { AttackKind::Projectile, 20.0f, 0.2f, 6500.0f, 0.0f, 22.0f, 300.0f, 1, { 80, 255, 120, 255 }, "Ecto Blast" } },
		{ HeroId::Jake, "Jake Long", "NYC Chinatown", "Dragon Flight (Tab)", "Dragon Fire Breath", "Ancient Dragon Fury",
		  { 225, 185, 150, 255 }, { 200, 40, 40, 255 }, { 40, 40, 50, 255 }, { 20, 20, 20, 255 }, { 80, 220, 120, 255 },
		  700.0f, 950.0f, true, 2000.0f, 3800.0f, 0, false, 2.4f,
		  { AttackKind::Melee, 28.0f, 0.32f, 0.0f, 230.0f, 0.0f, 650.0f, 1, { 255, 140, 40, 255 }, "Dragon Claws" } },
		{ HeroId::XJ9, "XJ9 (Jenny)", "Tremorton", "Rocket-Boot Flight (Tab)", "Missile Barrage", "Global Defense Protocol",
		  { 235, 240, 250, 255 }, { 235, 242, 250, 255 }, { 60, 140, 225, 255 }, { 60, 140, 225, 255 }, { 80, 230, 255, 255 },
		  600.0f, 850.0f, true, 1900.0f, 3600.0f, 0, false, 2.4f,
		  { AttackKind::Projectile, 9.0f, 0.08f, 7500.0f, 0.0f, 14.0f, 120.0f, 1, { 80, 230, 255, 255 }, "Arm Blaster" } },
		{ HeroId::ElTigre, "El Tigre", "Miracle City", "Grappling Chains (hold Q)", "Mighty Chain Grapple", "Spirit of the Ancestors",
		  { 225, 175, 130, 255 }, { 240, 140, 40, 255 }, { 30, 30, 30, 255 }, { 240, 140, 40, 255 }, { 220, 220, 230, 255 },
		  760.0f, 1050.0f, false, 0.0f, 0.0f, 1, false, 3.2f,
		  { AttackKind::Melee, 32.0f, 0.28f, 0.0f, 240.0f, 0.0f, 700.0f, 1, { 255, 200, 80, 255 }, "Steel Claws" } },
		{ HeroId::Ben, "Ben Tennyson", "Bellwood", "Omnitrix: 1/2/3 aliens, Q dial, G glitch", "Omnitrix Dial", "Way Big Overclock",
		  { 250, 210, 180, 255 }, { 240, 240, 240, 255 }, { 60, 140, 70, 255 }, { 120, 75, 40, 255 }, { 90, 255, 90, 255 },
		  650.0f, 900.0f, false, 0.0f, 0.0f, 0, false, 2.0f,
		  { AttackKind::Melee, 10.0f, 0.4f, 0.0f, 170.0f, 0.0f, 250.0f, 1, { 255, 255, 255, 255 }, "Punch" } },
		{ HeroId::Aang, "Aang", "Eastern Air Temple", "Glider (hold Space), Air Scooter (Q), Earth Pillar (R)", "Air Scooter", "The Avatar State",
		  { 245, 205, 165, 255 }, { 240, 190, 60, 255 }, { 238, 185, 75, 255 }, { 245, 205, 165, 255 }, { 80, 180, 255, 255 },
		  700.0f, 950.0f, false, 0.0f, 0.0f, 1, true, 1.0f,
		  { AttackKind::Projectile, 14.0f, 0.28f, 5000.0f, 0.0f, 55.0f, 1500.0f, 1, { 230, 245, 255, 255 }, "Air Blast" } },
	};
	static_assert(sizeof(Heroes) / sizeof(Heroes[0]) == static_cast<int>(HeroId::Count), "roster size");

	// Omnitrix forms: playlist 0..2, then glitch (Cannonbolt) and overclock (Way Big).
	const FormDef Forms[] = {
		{ "Heatblast", { 200, 55, 30, 255 }, { 255, 190, 40, 255 }, 34.0f, 85.0f, 700.0f, 950.0f, 1.0f, true, false,
		  { AttackKind::Projectile, 26.0f, 0.3f, 5000.0f, 0.0f, 40.0f, 300.0f, 1, { 255, 150, 30, 255 }, "Fireball" } },
		{ "Four Arms", { 200, 45, 45, 255 }, { 40, 40, 40, 255 }, 55.0f, 120.0f, 620.0f, 1300.0f, 1.0f, false, false,
		  { AttackKind::Melee, 55.0f, 0.5f, 0.0f, 360.0f, 0.0f, 1300.0f, 1, { 255, 220, 200, 255 }, "Quad Punch" } },
		{ "XLR8", { 40, 60, 170, 255 }, { 30, 30, 35, 255 }, 32.0f, 75.0f, 2400.0f, 900.0f, 1.0f, false, false,
		  { AttackKind::Melee, 18.0f, 0.11f, 0.0f, 220.0f, 0.0f, 400.0f, 1, { 120, 180, 255, 255 }, "Speed Strikes" } },
		{ "Cannonbolt", { 240, 240, 225, 255 }, { 250, 200, 40, 255 }, 90.0f, 90.0f, 1600.0f, 900.0f, 1.0f, false, true,
		  { AttackKind::None, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1, WHITE, "Rolling (contact damage)" } },
		{ "Way Big", { 240, 240, 245, 255 }, { 210, 40, 40, 255 }, 380.0f, 1500.0f, 1100.0f, 1800.0f, 1.0f, false, false,
		  { AttackKind::Melee, 200.0f, 0.9f, 0.0f, 1600.0f, 0.0f, 3000.0f, 1, { 255, 255, 255, 255 }, "Cosmic Stomp" } },
	};
	constexpr int GlitchFormIndex = 3;
	constexpr int OverclockFormIndex = 4;

	// Per-hero flight feel: Timmy surfs a floaty fairy cloud, Danny is an agile
	// ghost that stops on a dime, Jake's wings bank hard and dive fast, XJ9's
	// rockets are the fastest but carve wide turns.
	struct FlightStyle
	{
		float Accel;      // cm/s^2 toward the wanted speed
		float TurnRate;   // deg/s the travel direction can swing at cruise speed
		float Brake;      // cm/s^2 with no input
		float BoostCost;  // energy per second while boosting
		float MaxBank;    // degrees of visual bank into turns
		float DiveGain;   // extra top speed when travelling straight down
	};

	FlightStyle FlightFor(HeroId Id)
	{
		switch (Id)
		{
		case HeroId::Timmy: return { 2200.0f, 150.0f, 1800.0f, 9.0f, 32.0f, 500.0f };
		case HeroId::Danny: return { 6500.0f, 420.0f, 7000.0f, 14.0f, 45.0f, 600.0f };
		case HeroId::Jake:  return { 3200.0f, 170.0f, 2000.0f, 16.0f, 70.0f, 1300.0f };
		case HeroId::XJ9:   return { 3800.0f, 120.0f, 2500.0f, 18.0f, 60.0f, 800.0f };
		default:            return { 4000.0f, 200.0f, 3000.0f, 15.0f, 45.0f, 600.0f };
		}
	}

	Vector3 Cross(Vector3 A, Vector3 B) { return Vector3{ A.y * B.z - A.z * B.y, A.z * B.x - A.x * B.z, A.x * B.y - A.y * B.x }; }

	// Turns unit vector From toward unit vector To by at most MaxRad.
	Vector3 RotateToward(Vector3 From, Vector3 To, float MaxRad)
	{
		const float C = std::clamp(Dot(From, To), -1.0f, 1.0f);
		const float Angle = std::acos(C);
		if (Angle <= MaxRad || Angle < 1e-4f) return To;
		Vector3 Axis = Cross(From, To);
		if (Len(Axis) < 1e-4f) Axis = Cross(From, Vector3{ 0, 0, 1 });
		if (Len(Axis) < 1e-4f) Axis = Vector3{ 1, 0, 0 };
		const Vector3 Perp = Norm(Cross(Norm(Axis), From));
		return Norm(Add(Mul(From, std::cos(MaxRad)), Mul(Perp, std::sin(MaxRad))));
	}

	// A ring of particles around Center, perpendicular to Normal (sonic booms, landings).
	void SpawnRing(std::vector<Particle>& Out, Vector3 Center, Vector3 Normal, Color Tint, int Count, float Speed, float Size)
	{
		Normal = Norm(Normal);
		Vector3 U = Cross(Normal, Vector3{ 0, 0, 1 });
		if (Len(U) < 0.1f) U = Cross(Normal, Vector3{ 1, 0, 0 });
		U = Norm(U);
		const Vector3 W = Cross(Normal, U);
		for (int i = 0; i < Count; ++i)
		{
			const float A = 6.2831853f * static_cast<float>(i) / static_cast<float>(Count);
			const Vector3 Dir = Add(Mul(U, std::cos(A)), Mul(W, std::sin(A)));
			Out.push_back(Particle{ Add(Center, Mul(Dir, 30.0f)), Mul(Dir, Speed), 0.45f, 0.45f, Size, 0.0f, Tint });
		}
	}

	const char* EmoteLine(HeroId Id, const FormDef* Form)
	{
		if (Form) return Form->Name;
		switch (Id)
		{
		case HeroId::Jimmy: return "Brain blast!";
		case HeroId::Timmy: return "Cosmo! Wanda!";
		case HeroId::Danny: return "Going ghost!";
		case HeroId::Jake: return "Dragon up!";
		case HeroId::XJ9: return "Hi! XJ9, ready to save the day!";
		case HeroId::ElTigre: return "TIGRE!";
		case HeroId::Ben: return "It's hero time!";
		case HeroId::Aang: return "Yip yip!";
		default: return "";
		}
	}

	const FormDef* FormFor(int FormId)
	{
		if (FormId >= 0 && FormId < 3) return &Forms[FormId];
		if (FormId == Omnitrix::GlitchForm) return &Forms[GlitchFormIndex];
		if (FormId == Omnitrix::OverclockForm) return &Forms[OverclockFormIndex];
		return nullptr;
	}
}

// ---------------------------------------------------------------------------
// Public accessors used by rendering / HUD (defined in other files)
// ---------------------------------------------------------------------------

const HeroDef& GetHeroDef(HeroId Id) { return Heroes[static_cast<int>(Id)]; }
const FormDef* GetFormDef(int FormId) { return FormFor(FormId); }
const char* OmnitrixStateName(Omnitrix::EState S)
{
	switch (S)
	{
	case Omnitrix::EState::Human: return "READY";
	case Omnitrix::EState::Selecting: return "DIAL OPEN";
	case Omnitrix::EState::Transforming: return "TRANSFORMING";
	case Omnitrix::EState::Alien: return "ALIEN";
	case Omnitrix::EState::Reverting: return "TIMING OUT";
	case Omnitrix::EState::Recharging: return "RECHARGING";
	case Omnitrix::EState::Glitched: return "MASTER CONTROL GLITCH";
	case Omnitrix::EState::Overclocked: return "OVERCLOCK";
	}
	return "?";
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

bool Game::Init(const std::string& DataDir, std::string& Error)
{
	const char* Files[] = { "nexus_observatory.lvl", "retroville_slice.lvl" };
	for (const char* F : Files)
	{
		Level L;
		if (!LoadLevel(DataDir + "/levels/" + F, L, Error))
		{
			return false;
		}
		// Pitched roofs on houses (drawn and walkable).
		for (Block& B : L.Blocks)
		{
			const std::string& N = B.Label;
			if (N.rfind("Suburb", 0) == 0 || N == "Neutron_House" || N.rfind("CandyBar", 0) == 0)
			{
				B.RoofHeight = std::min(B.Size.x, B.Size.y) * 0.45f;
			}
		}
		DressLevel(L, static_cast<int>(Levels.size()));
		Levels.push_back(std::move(L));
	}
	SpawnShards();

	unsigned Seed = 99u;
	for (int i = 0; i < 260; ++i)
	{
		const float Yaw = Frand(Seed) * 360.0f;
		const float Pitch = -20.0f + Frand(Seed) * 100.0f;
		const float D = 150000.0f;
		Stars.push_back(Vector3{ std::cos(Pitch * Deg2Rad) * std::cos(Yaw * Deg2Rad) * D,
		                         std::cos(Pitch * Deg2Rad) * std::sin(Yaw * Deg2Rad) * D,
		                         std::sin(Pitch * Deg2Rad) * D });
	}

	const char* PortraitFiles[] = { "jimmy", "timmy", "danny", "jake", "xj9", "eltigre", "ben", "aang" };
	for (int i = 0; i < static_cast<int>(HeroId::Count); ++i)
	{
		const std::string Path = DataDir + "/portraits/" + PortraitFiles[i] + ".png";
		if (FileExists(Path.c_str()))
		{
			Portraits[i] = LoadTexture(Path.c_str());
			SetTextureFilter(Portraits[i], TEXTURE_FILTER_BILINEAR);
		}
	}

	// name, height in cm for the default-size character (art includes hair/hats)
	const struct { const char* Name; float Height; } SpriteFiles[SprCount] = {
		{ "jimmy", 168.0f }, { "timmy", 162.0f }, { "danny", 172.0f }, { "jake", 152.0f },
		{ "xj9", 162.0f }, { "eltigre", 132.0f }, { "ben", 150.0f }, { "aang", 150.0f },
		{ "heatblast", 182.0f }, { "fourarms", 245.0f }, { "dragon", 300.0f }, { "fairies", 58.0f },
	};
	for (int i = 0; i < SprCount; ++i)
	{
		const std::string Path = DataDir + "/sprites/" + SpriteFiles[i].Name + ".png";
		Sprites[i].HeightCm = SpriteFiles[i].Height;
		if (FileExists(Path.c_str()))
		{
			Sprites[i].Tex = LoadTexture(Path.c_str());
			SetTextureFilter(Sprites[i].Tex, TEXTURE_FILTER_BILINEAR);
		}
		const std::string FacePath = DataDir + "/sprites/faces/" + SpriteFiles[i].Name + ".png";
		if (FileExists(FacePath.c_str()))
		{
			Faces[i] = LoadTexture(FacePath.c_str());
			SetTextureFilter(Faces[i], TEXTURE_FILTER_BILINEAR);
		}
		// Puppet rig: <name>/rig.txt lists the parts and their pivots.
		const std::string RigDir = DataDir + "/sprites/" + SpriteFiles[i].Name + "/";
		std::ifstream Rig(RigDir + "rig.txt");
		std::string Line;
		while (Rig && std::getline(Rig, Line))
		{
			std::istringstream In(Line);
			std::string Kind;
			In >> Kind;
			if (Kind == "sprite")
			{
				In >> Sprites[i].RigW >> Sprites[i].RigH;
			}
			else if (Kind == "part")
			{
				RigPart Part;
				In >> Part.Name >> Part.X >> Part.Y >> Part.W >> Part.H >> Part.PX >> Part.PY >> Part.Z;
				const std::string PartPath = RigDir + Part.Name + ".png";
				if (In && FileExists(PartPath.c_str()))
				{
					Part.Tex = LoadTexture(PartPath.c_str());
					SetTextureFilter(Part.Tex, TEXTURE_FILTER_BILINEAR);
					Sprites[i].Parts.push_back(Part);
				}
			}
		}
		std::stable_sort(Sprites[i].Parts.begin(), Sprites[i].Parts.end(), [](const RigPart& A, const RigPart& B) { return A.Z < B.Z; });
	}

	P.Omni = Omnitrix::FStateMachine(OmnitrixConfig());
	LevelIndex = 0;
	LoadLevelState();
	ResetPlayer();
	Say("Welcome to the Nexus Observatory! Walk up to a terminal and press E.", Color{ 180, 255, 200, 255 }, 6.0f);
	return true;
}

void Game::Shutdown()
{
	for (Texture2D& T : Portraits)
	{
		if (T.id != 0) UnloadTexture(T);
		T = Texture2D{};
	}
	for (Texture2D& F : Faces)
	{
		if (F.id != 0) UnloadTexture(F);
		F = Texture2D{};
	}
	for (SpriteArt& S : Sprites)
	{
		if (S.Tex.id != 0) UnloadTexture(S.Tex);
		S.Tex = Texture2D{};
		for (RigPart& Part : S.Parts)
		{
			if (Part.Tex.id != 0) UnloadTexture(Part.Tex);
		}
		S.Parts.clear();
	}
}

Omnitrix::FConfig Game::OmnitrixConfig() const
{
	Omnitrix::FConfig C; // same defaults as UOmnitrixComponent
	C.AlienDuration = 45.0f;
	C.RechargeTime = 6.0f;
	return C;
}

void Game::LoadLevelState()
{
	const Level& L = Levels[LevelIndex];
	Cars.clear();
	CarIndex = -1;
	Enemies.clear();
	Projectiles.clear();
	Particles.clear();
	Shockwaves.clear();
	Anvils.clear();
	Rifts.clear();
	Courses.clear();
	for (Level& Lv : Levels)
	{
		Lv.Blocks.erase(std::remove_if(Lv.Blocks.begin(), Lv.Blocks.end(),
			[](const Block& B) { return B.Label.rfind("EarthPillar_", 0) == 0; }), Lv.Blocks.end());
	}
	Pillars.clear();
	TerminalMenu = -1;

	for (const Marker& M : L.Markers)
	{
		if (M.Class == "HoverCarPawn")
		{
			HoverCar C;
			C.Pos = M.Location;
			C.Yaw = M.YawDeg;
			Cars.push_back(C);
		}
		else if (M.Class == "TargetPoint" && M.Label.rfind("Rift_", 0) == 0)
		{
			Rift R;
			R.Pos = M.Location;
			R.Name = M.Label.substr(5);
			Rifts.push_back(R);
		}
	}

	auto AddCourse = [&](const char* Name, const std::vector<std::string>& Labels) {
		Course C;
		C.Name = Name;
		for (const std::string& Lbl : Labels)
		{
			const int Idx = L.FindBlockIndex(Lbl);
			if (Idx >= 0)
			{
				C.Gates.push_back(Idx);
			}
		}
		if (C.Gates.size() >= 2)
		{
			Courses.push_back(C);
		}
	};

	std::vector<std::string> Labels;
	char Buf[64];
	for (int i = 0; i < 12; ++i)
	{
		std::snprintf(Buf, sizeof(Buf), "FlightGate_%02d", i);
		Labels.push_back(Buf);
	}
	AddCourse("Flight Ring Challenge", Labels);

	Labels.clear();
	for (int i = 0; i < 8; ++i) { std::snprintf(Buf, sizeof(Buf), "RaceGate_S_%d_0", i); Labels.push_back(Buf); }
	for (int i = 0; i < 8; ++i) { std::snprintf(Buf, sizeof(Buf), "RaceGate_E_0_%d", i); Labels.push_back(Buf); }
	for (int i = 7; i >= 0; --i) { std::snprintf(Buf, sizeof(Buf), "RaceGate_N_%d_0", i); Labels.push_back(Buf); }
	for (int i = 7; i >= 0; --i) { std::snprintf(Buf, sizeof(Buf), "RaceGate_W_0_%d", i); Labels.push_back(Buf); }
	AddCourse("Retroville Hover Race", Labels);
}

void Game::ResetPlayer()
{
	const Level& L = Levels[LevelIndex];
	if (CarIndex >= 0)
	{
		Cars[CarIndex].Occupied = false;
		CarIndex = -1;
	}
	P.Pos = Vector3{ L.Start.x, L.Start.y, L.Start.z };
	P.Vel = Vector3{};
	P.Yaw = L.StartYaw;
	P.Health = P.MaxHealth;
	P.Flying = false;
	P.Intangible = P.WantsIntangible = false;
	P.Grappling = false;
	P.LastSafe = P.Pos;
	P.RespawnTimer = 0.0f;
	P.Pounding = false;
	P.Gliding = false;
	P.DodgeTime = 0.0f;
	P.EmoteTime = 0.0f;
	P.FlipTime = 0.0f;
	P.WallSlide = P.WallTimer = 0.0f;
	P.FlyBank = P.FlyPitch = 0.0f;
	P.Boosting = P.WasBoosting = false;
	CamYaw = L.StartYaw;
	CamPitch = -12.0f;
	GoddardPos = Sub(P.Pos, Vector3{ 0, 0, P.H });
	if (P.Hero == HeroId::Ben)
	{
		P.Omni = Omnitrix::FStateMachine(OmnitrixConfig());
		ApplyForm(Omnitrix::NoForm);
	}
}

void Game::SelectHero(HeroId Id)
{
	if (CarIndex >= 0)
	{
		ExitCar();
	}
	P.Hero = Id;
	P.Flying = false;
	P.Intangible = P.WantsIntangible = false;
	P.Grappling = false;
	P.ScooterTime = 0.0f;
	P.Energy = 100.0f;
	P.Pounding = false;
	P.Gliding = false;
	P.DodgeTime = 0.0f;
	P.EmoteTime = 0.0f;
	P.FlipTime = 0.0f;
	P.WallSlide = P.WallTimer = 0.0f;
	P.FlyBank = P.FlyPitch = 0.0f;
	P.Boosting = P.WasBoosting = false;
	P.Omni = Omnitrix::FStateMachine(OmnitrixConfig());
	P.AppliedForm = Omnitrix::NoForm;
	SetPlayerSize(32.0f, 70.0f);
	const HeroDef& H = Hero();
	SpawnBurst(P.Pos, H.Accent, 40, 600.0f);
	Say(std::string("Now playing: ") + H.Name + " (" + H.Universe + ")", H.Accent);
}

void Game::TravelTo(int Index)
{
	if (Index < 0 || Index >= static_cast<int>(Levels.size()))
	{
		return;
	}
	LevelIndex = Index;
	LoadLevelState();
	ResetPlayer();
	Banner = Levels[LevelIndex].Name;
	BannerTime = 3.0f;
	if (LevelIndex == 1)
	{
		Say("Retroville: close the 3 dimensional rifts, race the hover car loop. P = return to Nexus.", Color{ 255, 220, 120, 255 }, 7.0f);
	}
}

void Game::TeleportPlayer(Vector3 Pos, float Yaw)
{
	P.Pos = Pos;
	P.Vel = Vector3{};
	P.Yaw = Yaw;
	P.LastSafe = Pos;
	CamYaw = Yaw;
}

void Game::SetPlayerSize(float R, float H)
{
	const float OldH = P.H;
	P.R = R;
	P.H = H;
	// Grow upward so feet stay planted.
	P.Pos.z += H - OldH;
}

void Game::Say(const std::string& Text, Color Tint, float Seconds)
{
	Messages.push_back(Message{ Text, Seconds, Tint });
	if (Messages.size() > 5)
	{
		Messages.erase(Messages.begin());
	}
}

bool Game::PlayerIsValid() const
{
	return std::isfinite(P.Pos.x) && std::isfinite(P.Pos.y) && std::isfinite(P.Pos.z)
		&& std::isfinite(P.Vel.x) && std::isfinite(P.Vel.y) && std::isfinite(P.Vel.z);
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

const HeroDef& Game::Hero() const { return Heroes[static_cast<int>(P.Hero)]; }

const FormDef* Game::ActiveForm() const
{
	return P.Hero == HeroId::Ben ? FormFor(P.Omni.GetActiveForm()) : nullptr;
}

float Game::WalkSpeed() const
{
	if (const FormDef* F = ActiveForm())
	{
		return F->WalkSpeed;
	}
	float Speed = Hero().WalkSpeed;
	if (P.ScooterTime > 0.0f)
	{
		Speed *= 2.4f;
	}
	return Speed;
}

float Game::JumpZ() const
{
	if (const FormDef* F = ActiveForm())
	{
		return F->JumpZ;
	}
	return Hero().JumpZ;
}

bool Game::IsImmune() const
{
	if (P.Intangible || CarIndex >= 0 || P.RespawnTimer > 0.0f || P.DodgeTime > 0.0f)
	{
		return true;
	}
	return P.Hero == HeroId::Ben && P.Omni.IsInvulnerable();
}

bool Game::CanAttack() const
{
	if (P.Intangible || CarIndex >= 0 || TerminalMenu >= 0 || P.RespawnTimer > 0.0f)
	{
		return false;
	}
	if (P.Hero == HeroId::Ben && (P.Omni.IsInputLocked() || P.Omni.GetState() == Omnitrix::EState::Selecting))
	{
		return false;
	}
	return true;
}

Vector3 Game::CamForward() const
{
	return Vector3{ std::cos(CamPitch * Deg2Rad) * std::cos(CamYaw * Deg2Rad),
	                std::cos(CamPitch * Deg2Rad) * std::sin(CamYaw * Deg2Rad),
	                std::sin(CamPitch * Deg2Rad) };
}

Vector3 Game::PlayerFocus() const
{
	if (CarIndex >= 0)
	{
		return Add(Cars[CarIndex].Pos, Vector3{ 0.0f, 0.0f, 120.0f });
	}
	return Add(P.Pos, Vector3{ 0.0f, 0.0f, P.H * 0.65f });
}

Vector3 Game::AimPoint() const
{
	const Vector3 Dir = CamForward();
	const RayHit Hit = Raycast(Levels[LevelIndex], CamPos, Dir, 30000.0f);
	Vector3 Point = Hit.Point;
	// Don't aim at things between the camera and the player.
	const float PlayerDist = Dot(Sub(PlayerFocus(), CamPos), Dir);
	if (Hit.Hit && Hit.Distance < PlayerDist + 50.0f)
	{
		Point = Add(CamPos, Mul(Dir, 30000.0f));
	}
	return Point;
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void Game::Update(float Dt, const InputState& RawIn)
{
	const bool bMenuWasOpen = TerminalMenu >= 0;
	const InputState& In = RawIn;
	Dt = std::min(Dt, 1.0f / 20.0f);
	Time += Dt;

	if (In.Help)
	{
		bShowHelp = !bShowHelp;
		HelpTimer = 0.0f;
	}
	if (HelpTimer > 0.0f)
	{
		HelpTimer -= Dt;
		if (HelpTimer <= 0.0f)
		{
			bShowHelp = false;
		}
	}
	for (Message& M : Messages)
	{
		M.Time -= Dt;
	}
	Messages.erase(std::remove_if(Messages.begin(), Messages.end(), [](const Message& M) { return M.Time <= 0.0f; }), Messages.end());
	BannerTime -= Dt;

	if (TerminalMenu >= 0)
	{
		UpdateTerminalMenu(In);
	}
	else
	{
		if (In.Recall && LevelIndex != 0)
		{
			TravelTo(0);
			Say("Recalled to the Nexus Observatory.");
			return;
		}
		if (In.DebugSpawn)
		{
			SpawnEnemiesNearPlayer(6);
			Say("[debug] Spawned rift gremlins", Color{ 220, 150, 255, 255 });
		}
		if (In.DebugMeter)
		{
			P.Meter = 100.0f;
			Say("[debug] Ultimate meter filled", Color{ 220, 150, 255, 255 });
		}
		if (In.ToggleArt)
		{
			bSpriteArt = !bSpriteArt;
			Say(bSpriteArt ? "Art style: concept-art cut-outs" : "Art style: 3D models", Color{ 200, 220, 255, 255 }, 2.0f);
		}
		if (In.DebugNextHero)
		{
			SelectHero(static_cast<HeroId>((static_cast<int>(P.Hero) + 1) % static_cast<int>(HeroId::Count)));
		}
	}

	// Keys that drove the terminal menu this frame don't also act in the world.
	const InputState WorldIn = bMenuWasOpen ? InputState{} : RawIn;
	UpdateWorld(Dt, WorldIn);
}

void Game::UpdateWorld(float Dt, const InputState& In)
{
	// The Omnitrix runs on real time; the world slows down while the dial is open.
	UpdateOmnitrix(Dt, In);
	const bool bDial = P.Hero == HeroId::Ben && P.Omni.GetState() == Omnitrix::EState::Selecting;
	float WorldDt = bDial ? Dt * DialTimeDilation : Dt;
	// Hit-stop: the world freezes for a few frames on big hits.
	if (HitStop > 0.0f)
	{
		HitStop -= Dt;
		WorldDt *= 0.08f;
	}

	UpdateCamera(Dt, In);

	if (P.RespawnTimer > 0.0f)
	{
		P.RespawnTimer -= Dt;
		if (P.RespawnTimer <= 0.0f)
		{
			ResetPlayer();
			Say("Back on your feet!", Color{ 180, 255, 200, 255 });
		}
	}
	else
	{
		for (size_t i = 0; i < Cars.size(); ++i)
		{
			UpdateCar(Cars[i], WorldDt, static_cast<int>(i) == CarIndex && TerminalMenu < 0 ? &In : nullptr);
		}
		if (CarIndex >= 0)
		{
			P.Pos = Add(Cars[CarIndex].Pos, Vector3{ 0.0f, 0.0f, 40.0f });
			P.Vel = Cars[CarIndex].Vel;
			P.Yaw = Cars[CarIndex].Yaw;
		}
		else
		{
			UpdatePlayer(WorldDt, TerminalMenu < 0 ? In : InputState{});
		}
		if (TerminalMenu < 0)
		{
			UpdateCombat(WorldDt, In);
			UpdateInteraction(In);
		}
	}

	UpdateGoddard(WorldDt);
	UpdatePillars(WorldDt);
	UpdateProjectiles(WorldDt);
	UpdateEnemies(WorldDt);
	UpdateRifts(WorldDt);
	UpdateEffects(WorldDt);
	UpdateCourses(Dt);
	UpdateShards(Dt);

	// Out of the world or KO'd.
	const Level& L = Levels[LevelIndex];
	if (P.RespawnTimer <= 0.0f && (P.Pos.z < L.BoundsMin.z + 100.0f || !PlayerIsValid()))
	{
		ResetPlayer();
		Say("Whoa, fell out of the world. Respawned.", Color{ 255, 200, 120, 255 });
	}
	if (P.RespawnTimer <= 0.0f && P.Health <= 0.0f)
	{
		P.RespawnTimer = 2.0f;
		SpawnBurst(P.Pos, Hero().Accent, 60, 900.0f);
		Say("KO! Respawning...", Color{ 255, 120, 120, 255 });
	}
}

void Game::UpdateCamera(float Dt, const InputState& In)
{
	if (TerminalMenu < 0)
	{
		CamYaw += In.LookX;
		CamPitch = std::clamp(CamPitch + In.LookY, -75.0f, 65.0f);
	}
	CamYaw = std::fmod(CamYaw + 360.0f, 360.0f);

	const float Speed = Len(P.Vel);
	float Desired = 420.0f * std::pow(P.H / 70.0f, 0.85f);
	if (CarIndex >= 0) Desired = 950.0f;
	else if (P.Flying || P.Gliding) Desired += 120.0f + 160.0f * std::clamp(Speed / 3000.0f, 0.0f, 1.0f);
	CamDist += (Desired - CamDist) * std::min(1.0f, Dt * 5.0f);
	// Wider field of view at speed sells the rush of flight.
	const bool bFast = P.Flying || P.Gliding || CarIndex >= 0 || (ActiveForm() && std::string(ActiveForm()->Name) == "XLR8");
	const float FovTarget = 70.0f + (bFast ? 18.0f * std::clamp((Speed - 1300.0f) / 2200.0f, 0.0f, 1.0f) : 0.0f) + P.BoostFx * 10.0f;
	CamFov += (FovTarget - CamFov) * std::min(1.0f, Dt * 4.0f);

	const Vector3 Focus = PlayerFocus();
	const Vector3 Fwd = CamForward();
	const Vector3 Right{ -std::sin(CamYaw * Deg2Rad), std::cos(CamYaw * Deg2Rad), 0.0f };
	const float Shoulder = CarIndex >= 0 ? 0.0f : 45.0f * std::pow(P.H / 70.0f, 0.5f);
	Vector3 Want = Add(Sub(Focus, Mul(Fwd, CamDist)), Mul(Right, Shoulder));

	// Keep the camera out of walls.
	const Vector3 ToCam = Sub(Want, Focus);
	const float Dist = Len(ToCam);
	if (Dist > 1.0f && !P.Intangible)
	{
		const RayHit Hit = Raycast(Levels[LevelIndex], Focus, Mul(ToCam, 1.0f / Dist), Dist);
		if (Hit.Hit && Hit.Distance > 1.0f)
		{
			Want = Add(Focus, Mul(ToCam, std::max(30.0f, Hit.Distance - 25.0f) / Dist));
		}
	}
	// Screen shake.
	CamShake = std::max(0.0f, CamShake - Dt * 2.5f);
	if (CamShake > 0.0f)
	{
		const float Amp = CamShake * CamShake * 22.0f;
		Want = Add(Want, Vector3{ RandRange(-Amp, Amp), RandRange(-Amp, Amp), RandRange(-Amp, Amp) });
	}
	CamPos = Want;
}

float Game::PlayerSpeed() const
{
	return Len(P.Vel);
}

void Game::ApplyForm(int FormId)
{
	P.AppliedForm = FormId;
	if (const FormDef* F = FormFor(FormId))
	{
		SetPlayerSize(F->Radius, F->HalfHeight);
		Say(std::string("It's ") + F->Name + "!", Hero().Accent, 2.5f);
	}
	else
	{
		SetPlayerSize(32.0f, 70.0f);
	}
}

void Game::UpdateOmnitrix(float RealDt, const InputState& In)
{
	if (P.Hero != HeroId::Ben)
	{
		return;
	}
	Omnitrix::FStateMachine& M = P.Omni;
	const Omnitrix::EState Before = M.GetState();

	if (TerminalMenu < 0 && CarIndex < 0 && P.RespawnTimer <= 0.0f)
	{
		if (In.Sig)
		{
			if (M.GetState() == Omnitrix::EState::Selecting) M.CancelDial();
			else if (!M.OpenDial()) Say("Omnitrix is recharging...", Color{ 255, 120, 120, 255 }, 1.5f);
		}
		if (In.Number >= 1 && In.Number <= 3)
		{
			const int Slot = In.Number - 1;
			const bool Ok = M.GetState() == Omnitrix::EState::Selecting ? M.Confirm(Slot) : M.QuickSwap(Slot);
			if (!Ok && M.GetState() != Omnitrix::EState::Transforming)
			{
				Say("Can't transform right now", Color{ 255, 160, 120, 255 }, 1.5f);
			}
		}
		if (In.Glitch)
		{
			if (M.TriggerGlitch()) Say("MASTER CONTROL GLITCH - Cannonbolt shield!", Color{ 255, 220, 80, 255 });
			else Say("Glitch on cooldown", Color{ 255, 160, 120, 255 }, 1.5f);
		}
		if (In.Ult && P.Meter >= 100.0f)
		{
			if (M.TriggerOverclock())
			{
				P.Meter = 0.0f;
				Banner = "WAY BIG OVERCLOCK!";
				BannerTime = 2.5f;
			}
		}
	}

	// No streaming in the prototype: assets are always resident.
	if (M.GetState() == Omnitrix::EState::Transforming)
	{
		M.SetPendingAssetsReady(true);
	}
	M.Tick(RealDt);

	if (M.GetState() != Before)
	{
		if (M.GetState() == Omnitrix::EState::Transforming) SpawnBurst(P.Pos, Color{ 90, 255, 90, 255 }, 50, 700.0f, 16.0f);
		if (M.GetState() == Omnitrix::EState::Reverting) SpawnBurst(P.Pos, Color{ 255, 80, 80, 255 }, 30, 500.0f, 14.0f);
		if (M.GetState() == Omnitrix::EState::Human && Before == Omnitrix::EState::Recharging) Say("Omnitrix recharged", Color{ 90, 255, 90, 255 }, 2.0f);
	}
	if (M.GetActiveForm() != P.AppliedForm)
	{
		ApplyForm(M.GetActiveForm());
	}
}

void Game::UpdatePlayer(float Dt, const InputState& In)
{
	const Level& L = Levels[LevelIndex];
	const HeroDef& H = Hero();
	const FormDef* Form = ActiveForm();
	const bool bLocked = P.Hero == HeroId::Ben && P.Omni.IsInputLocked();

	P.AttackCd -= Dt;
	P.SigCd -= Dt;
	P.Sig2Cd -= Dt;
	P.AttackAnim = std::max(0.0f, P.AttackAnim - Dt);
	P.HurtFlash = std::max(0.0f, P.HurtFlash - Dt);
	P.ScooterTime = std::max(0.0f, P.ScooterTime - Dt);
	P.FlipTime = std::max(0.0f, P.FlipTime - Dt);
	P.PoundLand = std::max(0.0f, P.PoundLand - Dt);
	P.TakeoffTime = std::max(0.0f, P.TakeoffTime - Dt);
	P.HitComboTimer -= Dt;
	if (P.HitComboTimer <= 0.0f) P.HitCombo = 0;
	P.RegenDelay -= Dt;
	if (P.RegenDelay <= 0.0f)
	{
		P.Health = std::min(P.MaxHealth, P.Health + 8.0f * Dt);
	}
	if (!P.Intangible && P.BreathTime <= 0.0f && !P.Boosting)
	{
		P.Energy = std::min(100.0f, P.Energy + 14.0f * Dt);
	}

	// Traversal toggle.
	if (In.Traversal && H.CanFly && !Form)
	{
		if (P.Flying && P.Intangible)
		{
			Say("Turn solid first (Q) to land", Color{ 200, 255, 200, 255 }, 1.5f);
		}
		else
		{
			P.Flying = !P.Flying;
			if (P.Flying)
			{
				// Take off with a leap.
				P.Vel.z = std::max(P.Vel.z, P.Grounded ? 950.0f : 300.0f);
				P.TakeoffTime = 0.45f;
				P.Pounding = false;
				P.Gliding = false;
				P.EmoteTime = 0.0f;
				SpawnRing(Particles, Sub(P.Pos, Vector3{ 0, 0, P.H }), Vector3{ 0, 0, 1 }, H.Accent, 20, 700.0f, 14.0f);
			}
		}
	}
	if (Form || !H.CanFly)
	{
		P.Flying = false;
	}
	if (!P.Flying)
	{
		P.Boosting = In.Boost;
		P.WasBoosting = false;
	}

	if (P.Hero == HeroId::Danny)
	{
		UpdateGhost(Dt, In);
	}
	UpdateMoves(Dt, In);

	// Desired move direction from the camera.
	const Vector3 Fwd2D = YawDir(CamYaw);
	const Vector3 Right2D{ -std::sin(CamYaw * Deg2Rad), std::cos(CamYaw * Deg2Rad), 0.0f };
	Vector3 Wish = bLocked || P.EmoteTime > 0.0f ? Vector3{} : Add(Mul(Fwd2D, In.MoveY), Mul(Right2D, In.MoveX));
	if (Len(Wish) > 1.0f) Wish = Norm(Wish);

	if (P.Flying)
	{
		UpdateFlight(Dt, In);
	}
	else if (P.Pounding)
	{
		P.Vel.x = Approach(P.Vel.x, 0.0f, 5000.0f * Dt);
		P.Vel.y = Approach(P.Vel.y, 0.0f, 5000.0f * Dt);
		P.Vel.z = -2800.0f;
		P.Gliding = false;
	}
	else
	{
		const bool CanGlide = (Form ? Form->Glide : H.CanGlide);
		if (!P.Grounded && CanGlide && In.JumpHeld && (P.Vel.z < 0.0f || P.Gliding) && P.DodgeTime <= 0.0f && !P.Grappling)
		{
			UpdateGlide(Dt, In);
		}
		else
		{
			P.Gliding = false;
			float Speed = WalkSpeed() * (In.Boost && !Form ? 1.45f : 1.0f);
			const bool bRolling = Form && Form->Roll;
			const float Accel = (P.Grounded ? (bRolling ? 2500.0f : 5000.0f) : 1800.0f) * Dt;
			// A dodge keeps its dash speed until it ends.
			if (P.DodgeTime <= 0.0f)
			{
				const Vector3 Target = Mul(Wish, Speed);
				P.Vel.x = Approach(P.Vel.x, Target.x, Accel);
				P.Vel.y = Approach(P.Vel.y, Target.y, Accel);
			}

			const float GravScale = Form ? Form->GravityScale : 1.0f;
			float G = Gravity * GravScale;
			if (P.Grappling) G *= 0.35f;
			if (P.DodgeAir && P.DodgeTime > 0.0f) G *= 0.3f; // air dashes hang for a moment
			P.Vel.z += G * Dt;
			// Sliding down a wall: El Tigre digs his claws in, everyone else just slows.
			if (P.WallSlide > 0.0f && P.Vel.z < 0.0f)
			{
				P.Vel.z = std::max(P.Vel.z, P.Hero == HeroId::ElTigre && !Form ? -220.0f : -650.0f);
			}
		}

		// Jumps: coyote time after walking off a ledge, buffered presses before
		// landing, wall jumps, air jumps (with a flip), and variable height.
		P.Coyote = P.Grounded ? 0.12f : P.Coyote - Dt;
		P.JumpBuffer = In.Jump ? 0.14f : P.JumpBuffer - Dt;
		if (P.JumpBuffer > 0.0f && !bLocked)
		{
			const bool bWallJumper = !Form || std::string(Form->Name) == "XLR8";
			if (P.Grounded || P.Coyote > 0.0f)
			{
				P.Vel.z = JumpZ();
				P.Grounded = false;
				P.Coyote = 0.0f;
				P.JumpBuffer = 0.0f;
				P.JumpCut = true;
				P.EmoteTime = 0.0f;
			}
			else if (In.Jump && P.WallTimer > 0.0f && bWallJumper)
			{
				P.Vel.x = P.WallNormal.x * 800.0f;
				P.Vel.y = P.WallNormal.y * 800.0f;
				P.Vel.z = JumpZ() * 0.95f;
				P.Yaw = YawOf(P.WallNormal);
				P.WallTimer = 0.0f;
				P.WallSlide = 0.0f;
				P.JumpBuffer = 0.0f;
				P.JumpCut = true;
				P.AirDashUsed = false;
				P.FlipTime = P.Hero == HeroId::ElTigre || P.Hero == HeroId::Aang ? 0.5f : 0.0f;
				++WallJumpCount;
				SpawnRing(Particles, Sub(P.Pos, Mul(P.WallNormal, P.R)), P.WallNormal, Color{ 230, 230, 230, 255 }, 14, 400.0f, 10.0f);
			}
			else if (In.Jump && P.AirJumpsLeft > 0 && !Form)
			{
				P.Vel.z = JumpZ() * 0.9f;
				--P.AirJumpsLeft;
				P.JumpBuffer = 0.0f;
				P.JumpCut = false;
				P.FlipTime = 0.5f;
				SpawnRing(Particles, Sub(P.Pos, Vector3{ 0, 0, P.H }), Vector3{ 0, 0, 1 }, Color{ 230, 245, 255, 255 }, 16, 500.0f, 12.0f);
			}
		}
		// Let go of jump early for a short hop.
		if (P.JumpCut && !In.JumpHeld && P.Vel.z > 0.0f)
		{
			P.Vel.z *= 0.5f;
			P.JumpCut = false;
		}
		if (P.Vel.z <= 0.0f) P.JumpCut = false;
	}

	// El Tigre's grappling chain.
	if (P.Hero == HeroId::ElTigre)
	{
		if (In.Sig && !P.Grappling)
		{
			const RayHit Hit = Raycast(L, CamPos, CamForward(), 7000.0f);
			if (Hit.Hit && Len(Sub(Hit.Point, P.Pos)) > 200.0f)
			{
				P.Grappling = true;
				P.GrapplePoint = Hit.Point;
				P.Vel.z = std::max(P.Vel.z, 400.0f);
			}
			else
			{
				Say("Nothing to grapple in range", Color{ 255, 200, 120, 255 }, 1.0f);
			}
		}
		if (P.Grappling)
		{
			const Vector3 ToPoint = Sub(P.GrapplePoint, P.Pos);
			const float D = Len(ToPoint);
			if (!In.SigHeld || D < 160.0f)
			{
				P.Grappling = false;
				P.AirJumpsLeft = H.AirJumps;
				// Swing off the end of the chain with a flip.
				if (!P.Grounded) P.FlipTime = 0.5f;
			}
			else
			{
				P.Vel = Add(P.Vel, Mul(ToPoint, 4500.0f * Dt / D));
				const float Speed = Len(P.Vel);
				if (Speed > 3200.0f) P.Vel = Mul(P.Vel, 3200.0f / Speed);
			}
		}
	}

	// Integrate + collide.
	P.Pos = Add(P.Pos, Mul(P.Vel, Dt));
	const CollisionMode Mode = P.Intangible ? CollisionMode::Phasing : CollisionMode::Solid;
	const CollisionResult C = ResolveCylinder(L, P.Pos, P.R, P.H, Mode, std::max(40.0f, P.H * 0.4f));
	const bool WasGrounded = P.Grounded;
	P.Grounded = C.Grounded;
	if (C.Grounded && !WasGrounded && P.Vel.z < -300.0f)
	{
		P.LandTimer = 0.18f;
	}
	P.LandTimer = std::max(0.0f, P.LandTimer - Dt);
	// Sprite facing: flip when moving left on screen.
	const float ScreenRight = P.Vel.x * -std::sin(CamYaw * Deg2Rad) + P.Vel.y * std::cos(CamYaw * Deg2Rad);
	if (ScreenRight < -80.0f) P.SpriteFlip = true;
	else if (ScreenRight > 80.0f) P.SpriteFlip = false;
	if (C.Grounded && !WasGrounded)
	{
		P.AirDashUsed = false;
		P.FlipTime = 0.0f;
		if (P.Pounding)
		{
			// Ground pound impact: a shockwave scaled to the hero.
			P.Pounding = false;
			P.PoundLand = 0.35f;
			++Pounds;
			const Vector3 Feet = Sub(P.Pos, Vector3{ 0, 0, P.H });
			const float Size = std::max(1.0f, P.H / 70.0f);
			Shockwaves.push_back(Shockwave{ Feet, 0.0f, 650.0f * std::sqrt(Size), 5000.0f, 35.0f * Size, H.Accent, {} });
			SpawnRing(Particles, Add(Feet, Vector3{ 0, 0, 15 }), Vector3{ 0, 0, 1 }, Color{ 215, 205, 185, 255 }, 32, 1300.0f, 20.0f * std::sqrt(Size));
			SpawnBurst(Feet, Color{ 200, 190, 170, 255 }, 24, 700.0f, 16.0f, -1500.0f);
			CamShake = std::max(CamShake, 0.8f);
		}
	}
	if (P.Flying && C.Grounded && In.Down && !P.Intangible)
	{
		// Hold descend into the ground to land.
		P.Flying = false;
		P.LandTimer = 0.18f;
	}
	if (C.Grounded && P.Vel.z < 0.0f)
	{
		if (!WasGrounded && P.Vel.z < -1400.0f && P.H > 300.0f)
		{
			// Way Big landing quake.
			Shockwaves.push_back(Shockwave{ Sub(P.Pos, Vector3{ 0, 0, P.H }), 0.0f, 1800.0f, 6000.0f, 60.0f, Color{ 255, 255, 255, 255 }, {} });
			CamShake = std::max(CamShake, 1.0f);
		}
		P.Vel.z = 0.0f;
	}
	if (C.Grounded)
	{
		P.AirJumpsLeft = H.AirJumps;
	}
	if (C.HitCeiling && P.Vel.z > 0.0f)
	{
		P.Vel.z = 0.0f;
	}
	// Walls: remember the last one touched in the air for wall jumps / slides.
	P.WallTimer -= Dt;
	P.WallSlide -= Dt;
	if (C.HitWall)
	{
		const float Into = Dot(P.Vel, C.WallNormal);
		if (!P.Grounded && !P.Flying && !P.Pounding && Len2D(C.WallNormal) > 0.5f)
		{
			P.WallTimer = 0.22f;
			P.WallNormal = Norm(Vector3{ C.WallNormal.x, C.WallNormal.y, 0.0f });
			if (Dot(Wish, P.WallNormal) < -0.3f && P.Vel.z < 0.0f && !P.Gliding)
			{
				P.WallSlide = 0.15f;
			}
		}
		if (Into < 0.0f)
		{
			P.Vel = Sub(P.Vel, Mul(C.WallNormal, Into));
		}
		if (P.Grappling)
		{
			P.Grappling = false;
		}
	}
	if (P.Grounded) P.WallSlide = 0.0f;

	// Facing.
	const float HSpeed = Len2D(P.Vel);
	if (P.WallSlide > 0.0f)
	{
		P.Yaw += AngleDelta(P.Yaw, YawOf(Mul(P.WallNormal, -1.0f))) * std::min(1.0f, Dt * 15.0f);
	}
	else if (P.DodgeTime > 0.0f && !P.Flying)
	{
		// Keep facing the dodge direction.
	}
	else if (P.AttackAnim > 0.0f || P.BreathTime > 0.0f)
	{
		P.Yaw += AngleDelta(P.Yaw, CamYaw) * std::min(1.0f, Dt * 20.0f);
	}
	else if (HSpeed > 30.0f && P.EmoteTime <= 0.0f)
	{
		P.Yaw += AngleDelta(P.Yaw, YawOf(P.Vel)) * std::min(1.0f, Dt * (P.Flying ? 6.0f : 12.0f));
	}
	P.WalkPhase += HSpeed * Dt * 0.025f * (70.0f / P.H);
	if (P.Grounded && HSpeed < 30.0f && P.AttackAnim <= 0.0f && P.DodgeTime <= 0.0f)
	{
		P.IdleTime += Dt;
	}
	else
	{
		P.IdleTime = 0.0f;
	}
}

void Game::UpdateFlight(float Dt, const InputState& In)
{
	const HeroDef& H = Hero();
	const FlightStyle S = FlightFor(P.Hero);
	const Vector3 Right2D{ -std::sin(CamYaw * Deg2Rad), std::cos(CamYaw * Deg2Rad), 0.0f };
	Vector3 Wish = P.EmoteTime > 0.0f ? Vector3{} : Add(Mul(CamForward(), In.MoveY), Mul(Right2D, In.MoveX));
	Wish.z += ((In.JumpHeld ? 1.0f : 0.0f) - (In.Down ? 1.0f : 0.0f)) * 0.75f;
	if (Len(Wish) > 1.0f) Wish = Norm(Wish);
	const float WishLen = Len(Wish);

	// Boost burns energy; an empty tank drops you back to cruise speed.
	const bool bBoost = In.Boost && P.Energy > 1.0f && WishLen > 0.1f;
	if (bBoost) P.Energy = std::max(0.0f, P.Energy - S.BoostCost * Dt);
	P.Boosting = bBoost;

	const Vector3 OldVel = P.Vel;
	float Spd = Len(P.Vel);
	const Vector3 Dir = Spd > 1.0f ? Mul(P.Vel, 1.0f / Spd) : Vector3{ 0, 0, 1 };
	Vector3 V = P.Vel;
	if (WishLen > 0.05f)
	{
		const Vector3 WishDir = Norm(Wish);
		// Diving trades altitude for speed; climbing costs some.
		float Top = (bBoost ? H.FlyBoost : H.FlySpeed) * WishLen;
		Top += -Dir.z * S.DiveGain;
		// Momentum: the travel direction swings toward the input at the hero's
		// turn rate. Slow flight turns freely, cruise speed carves arcs.
		const float Slow = std::clamp(1.0f - Spd / H.FlySpeed, 0.0f, 1.0f);
		const float Turn = S.TurnRate * (1.0f + 4.0f * Slow) * (bBoost ? 0.8f : 1.0f) * Deg2Rad * Dt;
		const Vector3 NewDir = Spd > 150.0f ? RotateToward(Dir, WishDir, Turn) : WishDir;
		// Steering against the motion bleeds speed (air brake).
		const float Against = std::max(0.0f, -Dot(Dir, WishDir));
		Spd = Approach(Spd, Top * (1.0f - 0.5f * Against), (Spd < Top ? S.Accel : S.Accel * 0.7f) * Dt);
		V = Mul(NewDir, Spd);
	}
	else
	{
		Spd = Approach(Spd, 0.0f, S.Brake * Dt);
		V = Mul(Dir, Spd);
	}
	// The take-off leap carries upward for a moment.
	if (P.TakeoffTime > 0.0f)
	{
		V.z = std::max(V.z, 700.0f * (P.TakeoffTime / 0.45f));
	}
	P.Vel = V;

	// Visual bank from the turn rate and strafing, pitch from the climb angle.
	const float HSpd = Len2D(V);
	float YawRate = 0.0f;
	if (HSpd > 100.0f && Len2D(OldVel) > 100.0f && Dt > 0.0f)
	{
		YawRate = AngleDelta(YawOf(OldVel), YawOf(V)) / Dt;
	}
	const float BankTarget = std::clamp(YawRate * 0.22f + In.MoveX * 20.0f, -S.MaxBank, S.MaxBank) * std::clamp(HSpd / 500.0f, 0.0f, 1.0f);
	P.FlyBank += (BankTarget - P.FlyBank) * std::min(1.0f, Dt * 5.0f);
	const float PitchTarget = Spd > 150.0f ? std::atan2(V.z, HSpd) / Deg2Rad : 0.0f;
	P.FlyPitch += (PitchTarget - P.FlyPitch) * std::min(1.0f, Dt * 6.0f);

	// Wing beats speed up when climbing or boosting; diving folds the wings.
	const bool bDive = V.z < -700.0f && !bBoost;
	P.FlapPhase += Dt * (bDive ? 1.0f : 6.0f + (bBoost ? 5.0f : 0.0f) + std::max(0.0f, V.z) / 300.0f);

	// Sonic boom when a boost kicks in at speed.
	if (bBoost && !P.WasBoosting && Spd > 400.0f)
	{
		SpawnRing(Particles, P.Pos, Dir, Color{ 235, 245, 255, 255 }, 26, 900.0f, 14.0f);
		CamShake = std::max(CamShake, 0.35f);
		P.BoostFx = 0.4f;
	}
	P.WasBoosting = bBoost;
	P.BoostFx = std::max(0.0f, P.BoostFx - Dt);

	// Trails.
	const Vector3 Back = Spd > 300.0f ? Mul(Dir, -1.0f) : Vector3{ 0, 0, -1 };
	switch (P.Hero)
	{
	case HeroId::Danny:
		if (Spd > 200.0f)
		{
			const Vector3 At = Add(P.Pos, Add(Mul(Back, P.H * 0.6f), Vector3{ RandRange(-15, 15), RandRange(-15, 15), RandRange(-15, 15) }));
			Particles.push_back(Particle{ At, Mul(Back, 150.0f), 0.6f, 0.6f, RandRange(12, 22), 0.0f, Rand01() < 0.7f ? Color{ 120, 255, 170, 255 } : Color{ 240, 255, 250, 255 } });
		}
		break;
	case HeroId::XJ9:
		for (int k = 0; k < 2; ++k)
		{
			const Vector3 Feet = Add(P.Pos, Mul(Back, P.H * 0.95f));
			const Vector3 Jit{ RandRange(-60, 60), RandRange(-60, 60), RandRange(-60, 60) };
			Particles.push_back(Particle{ Feet, Add(Mul(Back, 900.0f + (bBoost ? 900.0f : 0.0f)), Jit), 0.22f, 0.22f, RandRange(14, 24), 0.0f,
				k == 0 ? Color{ 255, 200, 70, 255 } : Color{ 255, 120, 40, 255 } });
		}
		if (Rand01() < 0.3f)
		{
			Particles.push_back(Particle{ Add(P.Pos, Mul(Back, P.H * 1.3f)), Mul(Back, 200.0f), 0.8f, 0.8f, RandRange(18, 30), 60.0f, Color{ 200, 205, 215, 255 } });
		}
		break;
	case HeroId::Timmy:
		if (Rand01() < 0.7f)
		{
			const Vector3 At = Add(P.Pos, Vector3{ RandRange(-40, 40), RandRange(-40, 40), -P.H - 10.0f });
			const Color Tints[3] = { { 255, 230, 80, 255 }, { 245, 140, 200, 255 }, { 150, 230, 140, 255 } };
			Particles.push_back(Particle{ At, Add(Mul(Back, 200.0f), Vector3{ 0, 0, -60 }), 0.8f, 0.8f, RandRange(8, 14), -150.0f, Tints[static_cast<int>(Rand01() * 2.99f)] });
		}
		break;
	case HeroId::Jake:
		if (bBoost || Spd > 2400.0f)
		{
			const Vector3 Side{ -std::sin(P.Yaw * Deg2Rad), std::cos(P.Yaw * Deg2Rad), 0.0f };
			for (int s = -1; s <= 1; s += 2)
			{
				Particles.push_back(Particle{ Add(P.Pos, Mul(Side, s * P.H * 1.6f)), Mul(Back, 100.0f), 0.3f, 0.3f, 8.0f, 0.0f, Color{ 245, 245, 255, 255 } });
			}
		}
		break;
	default:
		break;
	}
	// Speed lines at high speed.
	if (Spd > 1900.0f)
	{
		const Vector3 At = Add(P.Pos, Vector3{ RandRange(-260, 260), RandRange(-260, 260), RandRange(-200, 200) });
		Particles.push_back(Particle{ At, Mul(Dir, Spd * 0.35f), 0.25f, 0.25f, 6.0f, 0.0f, Color{ 240, 248, 255, 255 } });
	}
}

void Game::UpdateGlide(float Dt, const InputState& In)
{
	// Hang-glider physics: look down to dive and pick up speed, look up to
	// climb and bleed it off; fly too slowly and you stall and sink.
	const float HSpd = Len2D(P.Vel);
	if (!P.Gliding)
	{
		P.Gliding = true;
		P.GlideSpeed = std::max(HSpd, 950.0f);
	}
	const float Pitch = std::clamp(CamPitch + 8.0f, -55.0f, 25.0f) * Deg2Rad;
	P.GlideSpeed += (-std::sin(Pitch) * 1700.0f - (P.GlideSpeed - 1150.0f) * 0.35f) * Dt;
	P.GlideSpeed = std::clamp(P.GlideSpeed, 650.0f, 2900.0f);

	const float TravelYaw = HSpd > 50.0f ? YawOf(P.Vel) : CamYaw;
	const float TargetYaw = CamYaw + In.MoveX * 40.0f;
	const float MaxTurn = (In.MoveY < -0.5f ? 160.0f : 110.0f) * Dt;
	const float Step = std::clamp(AngleDelta(TravelYaw, TargetYaw), -MaxTurn, MaxTurn);
	const float NewYaw = TravelYaw + Step;
	// S air-brakes.
	if (In.MoveY < -0.5f) P.GlideSpeed = std::max(650.0f, P.GlideSpeed - 900.0f * Dt);

	const Vector3 Flat = Mul(YawDir(NewYaw), P.GlideSpeed * std::cos(Pitch));
	P.Vel.x = Flat.x;
	P.Vel.y = Flat.y;
	float Vz = std::sin(Pitch) * P.GlideSpeed * 0.7f - 170.0f;
	if (P.GlideSpeed < 850.0f) Vz -= (850.0f - P.GlideSpeed) * 1.6f; // stall
	P.Vel.z = Approach(P.Vel.z, Vz, 3000.0f * Dt);

	const float YawRate = Dt > 0.0f ? Step / Dt : 0.0f;
	P.FlyBank += (std::clamp(YawRate * 0.35f, -45.0f, 45.0f) - P.FlyBank) * std::min(1.0f, Dt * 5.0f);
	P.FlyPitch += (std::atan2(P.Vel.z, std::max(1.0f, Len2D(P.Vel))) / Deg2Rad - P.FlyPitch) * std::min(1.0f, Dt * 6.0f);
	if (P.GlideSpeed > 1800.0f && Rand01() < 0.5f)
	{
		const Vector3 At = Add(P.Pos, Vector3{ RandRange(-200, 200), RandRange(-200, 200), RandRange(-150, 150) });
		Particles.push_back(Particle{ At, Mul(P.Vel, 0.35f), 0.25f, 0.25f, 6.0f, 0.0f, Color{ 240, 248, 255, 255 } });
	}
}

void Game::UpdateMoves(float Dt, const InputState& In)
{
	const Level& L = Levels[LevelIndex];
	const HeroDef& H = Hero();
	const FormDef* Form = ActiveForm();
	const std::string FormName = Form ? Form->Name : "";
	const bool bLocked = P.Hero == HeroId::Ben && P.Omni.IsInputLocked();

	P.DodgeCd -= Dt;
	if (P.DodgeTime > 0.0f)
	{
		P.DodgeTime = std::max(0.0f, P.DodgeTime - Dt);
	}

	// Dodge: a roll on the ground, a dash in the air, a barrel roll in flight.
	// All of them dodge damage while they last.
	const bool bCanDodge = !bLocked && !P.Grappling && FormName != "Cannonbolt" && FormName != "Way Big" && P.DodgeCd <= 0.0f;
	if (In.Dodge && bCanDodge)
	{
		const Vector3 Fwd2D = YawDir(CamYaw);
		const Vector3 Right2D{ -std::sin(CamYaw * Deg2Rad), std::cos(CamYaw * Deg2Rad), 0.0f };
		Vector3 Wish = Add(Mul(Fwd2D, In.MoveY), Mul(Right2D, In.MoveX));
		if (P.Flying)
		{
			P.DodgeSide = In.MoveX < -0.1f ? -1 : 1;
			P.Vel = Add(P.Vel, Mul(Right2D, P.DodgeSide * 1600.0f));
			P.DodgeTime = 0.45f;
			P.DodgeAir = true;
			P.DodgeCd = 0.55f;
			SpawnRing(Particles, P.Pos, YawDir(P.Yaw), Color{ 235, 245, 255, 255 }, 16, 500.0f, 10.0f);
		}
		else if (P.Grounded || !P.AirDashUsed)
		{
			if (Len(Wish) < 0.1f) Wish = YawDir(P.Yaw);
			P.DodgeDir = Norm(Wish);
			const float Dash = FormName == "XLR8" ? 3400.0f : (FormName == "Four Arms" ? 1500.0f : 1750.0f);
			P.Vel.x = P.DodgeDir.x * Dash;
			P.Vel.y = P.DodgeDir.y * Dash;
			P.DodgeAir = !P.Grounded;
			if (P.DodgeAir)
			{
				P.Vel.z = std::max(P.Vel.z, 150.0f);
				P.AirDashUsed = true;
			}
			P.DodgeTime = P.DodgeAir ? 0.32f : 0.4f;
			P.DodgeCd = 0.6f;
			P.Yaw = YawOf(P.DodgeDir);
			P.EmoteTime = 0.0f;
			P.Pounding = false;
			const Vector3 Mid = Sub(P.Pos, Vector3{ 0, 0, P.H * 0.3f });
			if (P.Hero == HeroId::Danny && !Form) SpawnBurst(P.Pos, Color{ 120, 255, 170, 255 }, 20, 400.0f, 14.0f);
			else if (P.Hero == HeroId::Aang && !Form) SpawnRing(Particles, Mid, Vector3{ 0, 0, 1 }, Color{ 225, 240, 255, 255 }, 24, 600.0f, 14.0f);
			else if (P.Hero == HeroId::XJ9) SpawnBurst(Sub(P.Pos, Vector3{ 0, 0, P.H }), Color{ 255, 190, 60, 255 }, 18, 500.0f, 14.0f);
			else if (FormName == "Heatblast") SpawnBurst(Mid, Color{ 255, 150, 30, 255 }, 24, 600.0f, 16.0f);
			else if (FormName == "XLR8") SpawnBurst(Mid, Color{ 120, 180, 255, 255 }, 18, 300.0f, 12.0f);
			else if (P.Grounded) SpawnBurst(Sub(P.Pos, Vector3{ 0, 0, P.H }), Color{ 210, 200, 180, 255 }, 12, 300.0f, 12.0f, -600.0f);
		}
	}
	const bool bAfterimages = FormName == "XLR8" || (P.Hero == HeroId::Danny && !Form);
	if (P.DodgeTime > 0.0f && !P.Flying && !P.DodgeAir && bAfterimages)
	{
		// Afterimages for the speedsters.
		Particles.push_back(Particle{ P.Pos, Vector3{}, 0.3f, 0.3f, P.R * 0.9f, 0.0f, Form ? Color{ 90, 140, 230, 255 } : Color{ 120, 255, 170, 255 } });
	}
	if (P.DodgeTime <= 0.0f && P.Grounded) P.DodgeAir = false;

	// Ground pound: press descend in the air.
	if (In.DownPressed && !P.Grounded && !P.Flying && !P.Pounding && !bLocked && FormName != "Cannonbolt" && !P.Intangible && !P.Grappling)
	{
		const RayHit Below = Raycast(L, P.Pos, Vector3{ 0, 0, -1 }, P.H + 160.0f);
		if (!Below.Hit)
		{
			P.Pounding = true;
			P.Gliding = false;
			P.FlipTime = 0.0f;
			P.DodgeTime = 0.0f;
			SpawnBurst(P.Pos, H.Accent, 10, 300.0f, 10.0f);
		}
	}

	// Emotes: the hero's signature pose.
	if (In.Emote && P.Grounded && !P.Flying && P.EmoteTime <= 0.0f && FormName != "Cannonbolt")
	{
		P.EmoteTime = 2.4f;
		Say(EmoteLine(P.Hero, Form), Form ? Form->Accent : H.Accent, 2.0f);
	}
	if (P.EmoteTime > 0.0f)
	{
		const float Before = P.EmoteTime;
		P.EmoteTime -= Dt;
		if (std::fabs(In.MoveX) + std::fabs(In.MoveY) > 0.1f || !P.Grounded || In.Attack)
		{
			P.EmoteTime = 0.0f;
		}
		else if (Before > 1.2f && P.EmoteTime <= 1.2f)
		{
			// The pose's big moment.
			const Color Fx = Form ? Form->Accent : H.Accent;
			SpawnRing(Particles, P.Pos, Vector3{ 0, 0, 1 }, Fx, 28, 800.0f, 14.0f);
			SpawnBurst(Add(P.Pos, Vector3{ 0, 0, P.H * 0.5f }), Fx, 24, 500.0f, 12.0f);
		}
		P.EmoteTime = std::max(0.0f, P.EmoteTime);
	}
}

void Game::UpdateGhost(float Dt, const InputState& In)
{
	const Level& L = Levels[LevelIndex];
	if (In.Sig)
	{
		P.WantsIntangible = !P.WantsIntangible;
		if (P.WantsIntangible && !P.Intangible)
		{
			if (P.Energy < 15.0f)
			{
				P.WantsIntangible = false;
				Say("Not enough ecto energy", Color{ 120, 255, 150, 255 }, 1.5f);
			}
			else
			{
				P.Intangible = true;
				P.Flying = true; // phasing always flies: no floor to stand on
				Say("Going ghost... intangible!", Color{ 120, 255, 150, 255 }, 1.5f);
			}
		}
	}

	const float Shrink = 2.0f;
	const bool Inside = OverlapsSolid(L, P.Pos, std::max(1.0f, P.R - Shrink), std::max(1.0f, P.H - Shrink));
	if (!P.Intangible)
	{
		if (!Inside) P.LastSafe = P.Pos;
		return;
	}

	P.Energy = std::max(0.0f, P.Energy - 12.0f * Dt);
	if (!Inside)
	{
		P.LastSafe = P.Pos;
	}
	if (P.Energy <= 0.0f)
	{
		if (Inside)
		{
			P.Pos = P.LastSafe;
			P.Vel = Vector3{};
			Say("Out of energy - pulled back to safety", Color{ 255, 200, 120, 255 }, 2.0f);
		}
		P.Intangible = P.WantsIntangible = false;
	}
	else if (!P.WantsIntangible && !Inside)
	{
		P.Intangible = false;
	}
}

void Game::UpdateCar(HoverCar& Car, float Dt, const InputState* In)
{
	if (Dt <= 0.0f)
	{
		return;
	}
	const Level& L = Levels[LevelIndex];
	const float Throttle = In ? In->MoveY : 0.0f;
	const float Steer = In ? In->MoveX : 0.0f;
	const float Lift = In ? (In->JumpHeld ? 1.0f : 0.0f) - (In->Down ? 1.0f : 0.0f) : 0.0f;
	const bool Boost = In && In->Boost;

	// Tuning mirrors AHoverCarPawn defaults.
	const float MinAlt = 120.0f, MaxAlt = 2500.0f, LiftRate = 600.0f;
	const float TopSpeed = 2500.0f, BoostTop = 4200.0f, MaxAccel = 1600.0f, TurnRate = 110.0f, Grip = 4.0f;

	Car.CruiseAltitude = std::clamp(Car.CruiseAltitude + Lift * LiftRate * Dt, MinAlt, MaxAlt);

	HoverMath::FSpringParams Params;
	Params.TargetHeight = Car.CruiseAltitude;
	Params.MaxRayLength = Car.CruiseAltitude + 300.0f;

	const Vector3 Fwd = YawDir(Car.Yaw);
	const Vector3 Right{ -std::sin(Car.Yaw * Deg2Rad), std::cos(Car.Yaw * Deg2Rad), 0.0f };
	const float Offsets[4][2] = { { 180, 90 }, { 180, -90 }, { -180, 90 }, { -180, -90 } };
	float AccelZ = Gravity;
	float Dist[4];
	for (int i = 0; i < 4; ++i)
	{
		const Vector3 Start = Add(Car.Pos, Add(Mul(Fwd, Offsets[i][0]), Mul(Right, Offsets[i][1])));
		const RayHit Hit = Raycast(L, Start, Vector3{ 0, 0, -1 }, Params.MaxRayLength);
		Dist[i] = Hit.Hit ? Hit.Distance : Params.MaxRayLength;
		AccelZ += HoverMath::SpringAcceleration(Params, Dist[i], Car.PrevDist[i], Dt) / 4.0f;
		Car.PrevDist[i] = Dist[i];
	}

	const float ForwardSpeed = Dot(Car.Vel, Fwd);
	const float Accel = HoverMath::ThrottleAcceleration(Throttle, ForwardSpeed, MaxAccel, Boost ? BoostTop : TopSpeed);
	Car.Vel = Add(Car.Vel, Mul(Fwd, Accel * Dt));
	Car.Vel.z += AccelZ * Dt;

	const float Lateral = Dot(Car.Vel, Right);
	Car.Vel = Sub(Car.Vel, Mul(Right, Lateral * HoverMath::LateralGripAlpha(Grip, Dt)));
	Car.Vel = Mul(Car.Vel, 1.0f - std::min(1.0f, 0.3f * Dt)); // linear damping

	const float SteerSign = ForwardSpeed < -50.0f ? -1.0f : 1.0f;
	const float TargetYawRate = Steer * TurnRate * SteerSign;
	Car.YawRate += (TargetYawRate - Car.YawRate) * std::min(1.0f, Dt * 6.0f);
	Car.Yaw = std::fmod(Car.Yaw + Car.YawRate * Dt + 360.0f, 360.0f);

	Car.Pos = Add(Car.Pos, Mul(Car.Vel, Dt));
	const CollisionResult C = ResolveCylinder(L, Car.Pos, 170.0f, 45.0f, CollisionMode::Solid, 30.0f);
	if (C.HitWall)
	{
		const float Into = Dot(Car.Vel, C.WallNormal);
		if (Into < 0.0f)
		{
			Car.Vel = Sub(Car.Vel, Mul(C.WallNormal, Into * 1.3f));
			if (Into < -900.0f) SpawnBurst(Car.Pos, Color{ 255, 230, 150, 255 }, 12, 500.0f);
		}
	}
	if ((C.Grounded && Car.Vel.z < 0.0f) || (C.HitCeiling && Car.Vel.z > 0.0f))
	{
		Car.Vel.z = 0.0f;
	}

	// Visual tilt from the hover rays and steering.
	const float FrontD = (Dist[0] + Dist[1]) * 0.5f;
	const float BackD = (Dist[2] + Dist[3]) * 0.5f;
	const float TargetPitch = std::clamp(std::atan2(BackD - FrontD, 360.0f) / Deg2Rad, -20.0f, 20.0f) - Accel * 0.002f;
	const float TargetRoll = std::clamp(-Car.YawRate * 0.12f * std::min(1.0f, std::fabs(ForwardSpeed) / 1500.0f), -18.0f, 18.0f);
	Car.VisPitch += (TargetPitch - Car.VisPitch) * std::min(1.0f, Dt * 6.0f);
	Car.VisRoll += (TargetRoll - Car.VisRoll) * std::min(1.0f, Dt * 6.0f);

	// Ram rift gremlins.
	const float Speed = Len(Car.Vel);
	if (Car.Occupied && Speed > 900.0f)
	{
		for (int i = static_cast<int>(Enemies.size()) - 1; i >= 0; --i)
		{
			if (Len(Sub(Enemies[i].Pos, Car.Pos)) < 230.0f)
			{
				DamageEnemy(i, 999.0f, Mul(Norm(Car.Vel), 800.0f));
			}
		}
	}
}

bool Game::EnterNearestCar()
{
	int Best = -1;
	float BestD = 450.0f;
	for (size_t i = 0; i < Cars.size(); ++i)
	{
		const float D = Len(Sub(Cars[i].Pos, P.Pos));
		if (D < BestD)
		{
			BestD = D;
			Best = static_cast<int>(i);
		}
	}
	if (Best < 0 || P.Intangible)
	{
		return false;
	}
	if (P.Hero == HeroId::Ben && P.Omni.IsTransformed())
	{
		Say("Aliens don't fit in the car!", Color{ 255, 200, 120, 255 }, 2.0f);
		return false;
	}
	CarIndex = Best;
	Cars[Best].Occupied = true;
	P.Flying = false;
	P.Grappling = false;
	CamYaw = Cars[Best].Yaw;
	Say("Hover car: W/S throttle, A/D steer, Space/Ctrl altitude, Shift boost, E exit", Color{ 255, 220, 140, 255 }, 5.0f);
	return true;
}

void Game::ExitCar()
{
	if (CarIndex < 0)
	{
		return;
	}
	HoverCar& Car = Cars[CarIndex];
	Car.Occupied = false;
	const Vector3 Right{ -std::sin(Car.Yaw * Deg2Rad), std::cos(Car.Yaw * Deg2Rad), 0.0f };
	P.Pos = Add(Car.Pos, Add(Mul(Right, -260.0f), Vector3{ 0, 0, 80.0f }));
	P.Vel = Mul(Car.Vel, 0.5f);
	P.Yaw = Car.Yaw;
	CarIndex = -1;
	ResolveCylinder(Levels[LevelIndex], P.Pos, P.R, P.H, CollisionMode::Solid);
}

void Game::UpdateInteraction(const InputState& In)
{
	const Level& L = Levels[LevelIndex];
	Prompt.clear();

	if (CarIndex >= 0)
	{
		if (In.Interact) ExitCar();
		return;
	}

	int Terminal = -1;
	for (size_t i = 0; i < L.Markers.size(); ++i)
	{
		const Marker& M = L.Markers[i];
		if (M.Class == "DimensionalTerminal" && Len(Sub(M.Location, Sub(P.Pos, Vector3{ 0, 0, P.H }))) < 420.0f)
		{
			Terminal = static_cast<int>(i);
		}
	}
	bool NearCar = false;
	for (const HoverCar& Car : Cars)
	{
		NearCar = NearCar || Len(Sub(Car.Pos, P.Pos)) < 450.0f;
	}

	if (Terminal >= 0)
	{
		Prompt = "E: Use Dimensional Terminal (" + L.Markers[Terminal].Label.substr(9) + ")";
		if (In.Interact)
		{
			TerminalMenu = Terminal;
		}
	}
	else if (NearCar)
	{
		Prompt = "E: Drive Hover Car";
		if (In.Interact) EnterNearestCar();
	}
}

void Game::UpdateTerminalMenu(const InputState& In)
{
	const Marker& M = Levels[LevelIndex].Markers[TerminalMenu];
	if (In.Back || In.Interact)
	{
		TerminalMenu = -1;
		return;
	}
	if (In.Number >= 1 && In.Number <= static_cast<int>(HeroId::Count))
	{
		SelectHero(static_cast<HeroId>(In.Number - 1));
		TerminalMenu = -1;
		return;
	}
	if (In.Travel)
	{
		if (M.Label == "Terminal_Retroville")
		{
			TravelTo(1);
		}
		else
		{
			Say("That world's portal is still unstable (not in this build). Try Retroville!", Color{ 255, 200, 120, 255 }, 3.0f);
			TerminalMenu = -1;
		}
	}
}

// ---------------------------------------------------------------------------
// Combat
// ---------------------------------------------------------------------------

void Game::UpdateCombat(float Dt, const InputState& In)
{
	if (CarIndex >= 0 || P.RespawnTimer > 0.0f)
	{
		return;
	}
	const HeroDef& H = Hero();
	const FormDef* Form = ActiveForm();
	const AttackDef& A = Form ? Form->Attack : H.Attack;

	const bool Rapid = A.Cooldown < 0.25f;
	if ((In.Attack || (Rapid && In.AttackHeld)) && P.AttackCd <= 0.0f && CanAttack() && A.Kind != AttackKind::None)
	{
		DoAttack(A);
		P.AttackCd = A.Cooldown + (P.AttackStep == 2 ? 0.15f : 0.0f);
		P.EmoteTime = 0.0f;
	}

	// Cannonbolt: rolling contact damage.
	if (Form && Form->Roll && Len2D(P.Vel) > 500.0f)
	{
		for (int i = static_cast<int>(Enemies.size()) - 1; i >= 0; --i)
		{
			if (Len(Sub(Enemies[i].Pos, P.Pos)) < P.R + 70.0f)
			{
				DamageEnemy(i, 80.0f, Mul(Norm(P.Vel), 900.0f));
			}
		}
	}

	// Signature abilities (Danny's intangibility, El Tigre's grapple and Ben's dial live elsewhere).
	if (CanAttack() || P.Hero == HeroId::Danny)
	{
		switch (P.Hero)
		{
		case HeroId::Jimmy:
			if (In.Sig && P.SigCd <= 0.0f && P.Energy >= 25.0f)
			{
				P.Vel.z = 1350.0f;
				P.Energy -= 25.0f;
				P.SigCd = 0.6f;
				SpawnBurst(Sub(P.Pos, Vector3{ 0, 0, P.H }), Color{ 255, 180, 60, 255 }, 20, 500.0f, 12.0f, -400.0f);
			}
			break;
		case HeroId::Timmy:
			if (In.Sig && P.SigCd <= 0.0f)
			{
				const Vector3 Target = AimPoint();
				Anvils.push_back(FallingAnvil{ Add(Target, Vector3{ 0, 0, 1800.0f }), 0.0f, Target.z });
				P.SigCd = 1.5f;
				Say("POOF! Anvil!", Color{ 255, 230, 80, 255 }, 1.0f);
			}
			break;
		case HeroId::Jake:
			if (In.SigHeld && P.Energy > 0.0f)
			{
				P.BreathTime = 0.15f;
				P.Energy = std::max(0.0f, P.Energy - 28.0f * Dt);
				ConeDamage(800.0f, 25.0f, 70.0f * Dt, 30.0f, Color{ 255, 140, 30, 255 });
				const Vector3 Mouth = Add(Add(P.Pos, Vector3{ 0, 0, P.H * 0.5f }), Mul(YawDir(P.Yaw), P.R + 20.0f));
				for (int k = 0; k < 3; ++k)
				{
					Vector3 V = Add(Mul(CamForward(), 1400.0f), Vector3{ RandRange(-200, 200), RandRange(-200, 200), RandRange(-120, 160) });
					Particles.push_back(Particle{ Mouth, V, 0.45f, 0.45f, RandRange(18, 34), 0.0f, Rand01() < 0.5f ? Color{ 255, 120, 20, 255 } : Color{ 255, 220, 60, 255 } });
				}
			}
			else
			{
				P.BreathTime = std::max(0.0f, P.BreathTime - Dt);
			}
			break;
		case HeroId::XJ9:
			if (In.Sig && P.SigCd <= 0.0f)
			{
				for (int k = 0; k < 6; ++k)
				{
					Projectile Pr;
					Pr.Pos = Add(P.Pos, Vector3{ 0, 0, P.H * 0.8f });
					Pr.Vel = Add(Mul(CamForward(), 1500.0f), Vector3{ RandRange(-600, 600), RandRange(-600, 600), 900.0f });
					Pr.Damage = 30.0f;
					Pr.Radius = 26.0f;
					Pr.Knockback = 500.0f;
					Pr.Life = 3.0f;
					Pr.Tint = Color{ 255, 210, 90, 255 };
					Pr.Homing = true;
					Projectiles.push_back(Pr);
				}
				P.SigCd = 3.0f;
			}
			break;
		case HeroId::Aang:
			if (In.Sig && P.SigCd <= 0.0f)
			{
				P.ScooterTime = 5.0f;
				P.SigCd = 1.0f;
				Say("Air Scooter!", Color{ 200, 230, 255, 255 }, 1.0f);
			}
			if (In.Sig2 && P.Sig2Cd <= 0.0f && P.Grounded)
			{
				// Earthbending: raise a rock pillar under Aang and ride it up.
				const RayHit Down = Raycast(Levels[LevelIndex], P.Pos, Vector3{ 0, 0, -1 }, P.H + 200.0f);
				const float Ground = Down.Hit ? Down.Point.z : P.Pos.z - P.H;
				EarthPillar E;
				E.Label = "EarthPillar_" + std::to_string(PillarSerial++);
				E.GroundZ = Ground;
				E.Base = Vector3{ P.Pos.x, P.Pos.y, Ground };
				E.Yaw = P.Yaw;
				Pillars.push_back(E);
				Block B;
				B.Label = E.Label;
				B.Folder = "Abilities";
				B.Center = Vector3{ P.Pos.x, P.Pos.y, Ground - 5.0f };
				B.Size = Vector3{ 170.0f, 170.0f, 10.0f };
				B.YawDeg = P.Yaw;
				B.RoofHeight = 60.0f;
				FinalizeBlock(B);
				Levels[LevelIndex].Blocks.push_back(B);
				P.Vel.z = 1700.0f;
				P.Sig2Cd = 2.0f;
				SpawnBurst(E.Base, Color{ 150, 115, 80, 255 }, 30, 600.0f, 18.0f, -1200.0f);
				Say("Earthbending: rock pillar!", Color{ 200, 170, 120, 255 }, 1.0f);
			}
			break;
		case HeroId::Danny:
			if (In.Sig2 && P.Sig2Cd <= 0.0f && P.Energy >= 30.0f && !P.Intangible)
			{
				P.Energy -= 30.0f;
				P.Sig2Cd = 4.0f;
				ConeDamage(1400.0f, 40.0f, 40.0f, 1800.0f, Color{ 120, 255, 150, 255 });
				Say("GHOST WAIL!", Color{ 120, 255, 150, 255 }, 1.0f);
			}
			break;
		default:
			break;
		}
	}

	if (In.Ult && P.Hero != HeroId::Ben)
	{
		if (P.Meter >= 100.0f && CanAttack())
		{
			TriggerUltimate();
		}
		else if (P.Meter < 100.0f)
		{
			Say("Ultimate meter not full yet (fight to charge it)", Color{ 200, 200, 255, 255 }, 1.5f);
		}
	}
}

void Game::TriggerUltimate()
{
	const HeroDef& H = Hero();
	P.Meter = 0.0f;
	Banner = std::string(H.UltName) + "!";
	BannerTime = 2.5f;
	Shockwaves.push_back(Shockwave{ P.Pos, 0.0f, 3200.0f, 4500.0f, 160.0f, H.Accent, {} });
	SpawnBurst(P.Pos, H.Accent, 120, 1500.0f, 22.0f);
}

void Game::DoAttack(const AttackDef& A)
{
	// Three-hit combos for everything but rapid fire: the third hit is a finisher.
	const bool bCombo = A.Cooldown >= 0.2f;
	P.AttackStep = bCombo && Time - P.LastAttack < A.Cooldown + 0.5f ? (P.AttackStep + 1) % 3 : 0;
	P.LastAttack = Time;
	const bool bFinisher = bCombo && P.AttackStep == 2;
	const float DmgMul = bFinisher ? 1.8f : 1.0f;
	const float KnockMul = bFinisher ? 1.7f : 1.0f;
	P.AttackDur = bFinisher ? 0.38f : 0.25f;
	P.AttackAnim = P.AttackDur;
	const Vector3 Aim = AimPoint();
	const Vector3 Chest = Add(P.Pos, Vector3{ 0, 0, P.H * 0.35f });

	if (A.Kind == AttackKind::Projectile)
	{
		const Vector3 Dir = Norm(Sub(Aim, Chest));
		const Vector3 Right{ -Dir.y, Dir.x, 0.0f };
		for (int k = 0; k < A.Count; ++k)
		{
			const float Spread = A.Count > 1 ? (static_cast<float>(k) - (A.Count - 1) * 0.5f) * 0.08f : 0.0f;
			Projectile Pr;
			Pr.Pos = Add(Chest, Mul(Dir, P.R + 15.0f));
			Pr.Vel = Mul(Norm(Add(Dir, Mul(Right, Spread))), A.Speed);
			Pr.Damage = A.Damage * DmgMul;
			Pr.Radius = A.Radius * (bFinisher ? 1.7f : 1.0f);
			Pr.Knockback = A.Knockback * KnockMul;
			Pr.Tint = A.Tint;
			Pr.Life = 2.5f;
			Projectiles.push_back(Pr);
		}
	}
	else if (A.Kind == AttackKind::Melee)
	{
		const Vector3 Facing = YawDir(CamYaw);
		P.Yaw = CamYaw;
		const float Range = A.Range * (bFinisher ? 1.3f : 1.0f);
		bool bHit = false;
		for (int i = static_cast<int>(Enemies.size()) - 1; i >= 0; --i)
		{
			const Vector3 To = Sub(Enemies[i].Pos, P.Pos);
			const float D = Len2D(To);
			// Finishers hit all the way around (spins, slams).
			const float MinDot = bFinisher ? -1.0f : 0.2f;
			if (D < Range + P.R + 45.0f && std::fabs(To.z) < P.H + 120.0f && Dot(Norm(Vector3{ To.x, To.y, 0 }), Facing) > MinDot)
			{
				Vector3 Knock = Mul(Norm(Vector3{ To.x, To.y, 0 }), A.Knockback * KnockMul);
				Knock.z = A.Knockback * KnockMul * 0.4f;
				DamageEnemy(i, A.Damage * DmgMul, Knock);
				bHit = true;
			}
		}
		if (bHit)
		{
			HitStop = std::max(HitStop, bFinisher ? 0.08f : 0.035f);
			CamShake = std::max(CamShake, bFinisher ? 0.45f : 0.2f);
		}
		// A short lunge carries melee heroes into the hit.
		if (P.Grounded)
		{
			P.Vel.x += Facing.x * (bFinisher ? 500.0f : 300.0f);
			P.Vel.y += Facing.y * (bFinisher ? 500.0f : 300.0f);
		}
		const Vector3 Hand = Add(Add(P.Pos, Vector3{ 0, 0, P.H * 0.3f }), Mul(Facing, P.R + Range * 0.5f));
		SpawnBurst(Hand, A.Tint, bFinisher ? 20 : 8, bFinisher ? 600.0f : 300.0f, 10.0f + P.H * 0.05f);
		if (bFinisher) SpawnRing(Particles, Sub(P.Pos, Vector3{ 0, 0, P.H * 0.5f }), Vector3{ 0, 0, 1 }, A.Tint, 20, 900.0f, 12.0f);
	}
}

void Game::ConeDamage(float Range, float HalfAngleDeg, float Damage, float Knock, Color Tint)
{
	const Vector3 Facing = CamForward();
	const float CosHalf = std::cos(HalfAngleDeg * Deg2Rad);
	for (int i = static_cast<int>(Enemies.size()) - 1; i >= 0; --i)
	{
		const Vector3 To = Sub(Enemies[i].Pos, P.Pos);
		const float D = Len(To);
		if (D < Range && D > 1.0f && Dot(Mul(To, 1.0f / D), Facing) > CosHalf)
		{
			Vector3 K = Mul(Norm(To), Knock);
			K.z += Knock * 0.3f;
			DamageEnemy(i, Damage, K);
		}
	}
	if (Knock > 500.0f)
	{
		for (int k = 0; k < 40; ++k)
		{
			const Vector3 V = Add(Mul(Facing, 2200.0f), Vector3{ RandRange(-500, 500), RandRange(-500, 500), RandRange(-400, 400) });
			Particles.push_back(Particle{ Add(P.Pos, Vector3{ 0, 0, P.H * 0.6f }), V, 0.5f, 0.5f, RandRange(14, 28), 0.0f, Tint });
		}
	}
}

void Game::DamageEnemy(int Index, float Amount, Vector3 Knock)
{
	Enemy& E = Enemies[Index];
	E.Health -= Amount;
	E.HitFlash = 0.12f;
	E.Vel = Add(E.Vel, Knock);
	P.Meter = std::min(100.0f, P.Meter + Amount * 0.06f);
	++P.HitCombo;
	P.HitComboTimer = 2.0f;
	if (E.Health <= 0.0f)
	{
		KillEnemy(Index);
	}
}

void Game::KillEnemy(int Index)
{
	SpawnBurst(Enemies[Index].Pos, Color{ 170, 80, 230, 255 }, 26, 700.0f, 16.0f, -900.0f);
	Enemies.erase(Enemies.begin() + Index);
	P.Meter = std::min(100.0f, P.Meter + 5.0f);
	++Kills;
}

void Game::DamagePlayer(float Amount)
{
	if (IsImmune())
	{
		return;
	}
	P.Health -= Amount;
	P.HurtFlash = 0.25f;
	P.RegenDelay = 4.0f;
	P.Meter = std::min(100.0f, P.Meter + Amount * 0.25f);
}

void Game::SpawnBurst(Vector3 Pos, Color Tint, int Count, float Speed, float Size, float Grav)
{
	for (int i = 0; i < Count; ++i)
	{
		const Vector3 Dir = Norm(Vector3{ RandRange(-1, 1), RandRange(-1, 1), RandRange(-0.6f, 1.0f) });
		const float Life = RandRange(0.3f, 0.8f);
		Particles.push_back(Particle{ Pos, Mul(Dir, Speed * RandRange(0.3f, 1.0f)), Life, Life, Size * RandRange(0.6f, 1.3f), Grav, Tint });
	}
	if (Particles.size() > 1500)
	{
		Particles.erase(Particles.begin(), Particles.begin() + (Particles.size() - 1500));
	}
}

void Game::SpawnEnemiesNearPlayer(int Count)
{
	const Level& L = Levels[LevelIndex];
	for (int i = 0; i < Count; ++i)
	{
		const float A = (static_cast<float>(i) / Count) * 360.0f + RandRange(-15, 15);
		Vector3 Pos = Add(P.Pos, Add(Mul(YawDir(A), RandRange(700.0f, 1100.0f)), Vector3{ 0, 0, 300.0f }));
		const RayHit Down = Raycast(L, Pos, Vector3{ 0, 0, -1 }, 3000.0f);
		if (Down.Hit) Pos.z = Down.Point.z + 60.0f;
		Enemy E;
		E.Pos = Pos;
		E.Serial = NextSerial++;
		E.Phase = RandRange(0, 6.28f);
		Enemies.push_back(E);
		SpawnBurst(Pos, Color{ 170, 80, 230, 255 }, 10, 400.0f);
	}
}

void Game::UpdateProjectiles(float Dt)
{
	const Level& L = Levels[LevelIndex];
	for (int i = static_cast<int>(Projectiles.size()) - 1; i >= 0; --i)
	{
		Projectile& Pr = Projectiles[i];
		Pr.Life -= Dt;
		if (Pr.Homing && !Enemies.empty())
		{
			int Best = -1;
			float BestD = 6000.0f;
			for (size_t e = 0; e < Enemies.size(); ++e)
			{
				const float D = Len(Sub(Enemies[e].Pos, Pr.Pos));
				if (D < BestD) { BestD = D; Best = static_cast<int>(e); }
			}
			if (Best >= 0)
			{
				const Vector3 Want = Mul(Norm(Sub(Enemies[Best].Pos, Pr.Pos)), 3200.0f);
				Pr.Vel = Add(Pr.Vel, Mul(Sub(Want, Pr.Vel), std::min(1.0f, Dt * 3.5f)));
			}
		}
		else if (Pr.Homing)
		{
			Pr.Vel.z -= 900.0f * Dt;
		}

		const Vector3 Step = Mul(Pr.Vel, Dt);
		const float StepLen = Len(Step);
		bool Dead = Pr.Life <= 0.0f;
		if (!Dead && StepLen > 0.0f)
		{
			const RayHit Hit = Raycast(L, Pr.Pos, Mul(Step, 1.0f / StepLen), StepLen);
			if (Hit.Hit)
			{
				SpawnBurst(Hit.Point, Pr.Tint, 8, 350.0f, 10.0f);
				Dead = true;
			}
		}
		Pr.Pos = Add(Pr.Pos, Step);
		if (!Dead)
		{
			for (int e = static_cast<int>(Enemies.size()) - 1; e >= 0; --e)
			{
				if (Len(Sub(Enemies[e].Pos, Pr.Pos)) < Pr.Radius + 55.0f)
				{
					SpawnBurst(Pr.Pos, Pr.Tint, 10, 400.0f, 12.0f);
					DamageEnemy(e, Pr.Damage, Mul(Norm(Pr.Vel), Pr.Knockback));
					Dead = true;
					break;
				}
			}
		}
		if (Dead)
		{
			Projectiles.erase(Projectiles.begin() + i);
		}
	}
}

void Game::UpdateEnemies(float Dt)
{
	const Level& L = Levels[LevelIndex];
	const Vector3 Target = CarIndex >= 0 ? Cars[CarIndex].Pos : P.Pos;
	for (int i = static_cast<int>(Enemies.size()) - 1; i >= 0; --i)
	{
		Enemy& E = Enemies[i];
		E.HitFlash = std::max(0.0f, E.HitFlash - Dt);
		E.AttackCd -= Dt;
		E.Phase += Dt * 8.0f;

		Vector3 To = Sub(Target, E.Pos);
		const float D = Len2D(To);
		Vector3 Wish{};
		if (D < 9000.0f && D > 1.0f)
		{
			Wish = Mul(Vector3{ To.x / D, To.y / D, 0.0f }, 430.0f);
		}
		const float Accel = (E.Grounded ? 1800.0f : 500.0f) * Dt;
		E.Vel.x = Approach(E.Vel.x, Wish.x, Accel);
		E.Vel.y = Approach(E.Vel.y, Wish.y, Accel);
		E.Vel.z += Gravity * Dt;
		E.Pos = Add(E.Pos, Mul(E.Vel, Dt));
		const CollisionResult C = ResolveCylinder(L, E.Pos, 45.0f, 45.0f, CollisionMode::Solid);
		E.Grounded = C.Grounded;
		if (C.Grounded && E.Vel.z < 0.0f) E.Vel.z = 0.0f;
		if (C.HitWall && C.Grounded) E.Vel.z = 750.0f; // hop over obstacles
		if (C.Grounded && Rand01() < Dt * 0.6f) E.Vel.z = 500.0f;

		// Contact attack.
		const float Reach = (CarIndex >= 0 ? 0.0f : P.R) + 70.0f;
		if (CarIndex < 0 && Len(Sub(P.Pos, E.Pos)) < Reach + P.H * 0.5f && E.AttackCd <= 0.0f)
		{
			DamagePlayer(6.0f);
			E.AttackCd = 0.8f;
		}
		if (E.Pos.z < L.BoundsMin.z + 100.0f)
		{
			Enemies.erase(Enemies.begin() + i);
		}
	}
}

void Game::UpdateRifts(float Dt)
{
	for (size_t r = 0; r < Rifts.size(); ++r)
	{
		Rift& R = Rifts[r];
		if (R.Closed)
		{
			continue;
		}
		int Alive = 0;
		for (const Enemy& E : Enemies)
		{
			Alive += E.Rift == static_cast<int>(r) ? 1 : 0;
		}
		const float D = Len(Sub(P.Pos, R.Pos));
		if (R.Wave == 0)
		{
			if (D < 4500.0f)
			{
				R.Wave = 1;
				R.WaveTimer = 0.5f;
				Say("Rift Patrol: dimensional rift at " + R.Name + "! Defeat 3 waves to close it.", Color{ 220, 150, 255, 255 }, 4.0f);
			}
			continue;
		}
		if (Alive == 0)
		{
			R.WaveTimer -= Dt;
			if (R.WaveTimer > 0.0f)
			{
				continue;
			}
			if (R.Wave > 3)
			{
				R.Closed = true;
				++RiftsClosed;
				P.Meter = std::min(100.0f, P.Meter + 25.0f);
				SpawnBurst(R.Pos, Color{ 120, 255, 200, 255 }, 80, 1200.0f, 20.0f);
				Say("Rift closed! (" + std::to_string(RiftsClosed) + " total)", Color{ 120, 255, 200, 255 }, 4.0f);
				continue;
			}
			const int Count = 2 + R.Wave * 2;
			for (int k = 0; k < Count; ++k)
			{
				const Vector3 Pos = Add(R.Pos, Add(Mul(YawDir(k * 360.0f / Count), RandRange(250, 500)), Vector3{ 0, 0, 120.0f }));
				Enemy E;
				E.Pos = Pos;
				E.Rift = static_cast<int>(r);
				E.Serial = NextSerial++;
				E.Health = 30.0f + R.Wave * 15.0f;
				E.Vel = Vector3{ RandRange(-300, 300), RandRange(-300, 300), 600.0f };
				Enemies.push_back(E);
			}
			SpawnBurst(R.Pos, Color{ 170, 80, 230, 255 }, 30, 900.0f);
			Say("Wave " + std::to_string(R.Wave) + "/3", Color{ 220, 150, 255, 255 }, 2.0f);
			++R.Wave;
			R.WaveTimer = 2.5f;
		}
	}
}

void Game::UpdateEffects(float Dt)
{
	for (int i = static_cast<int>(Particles.size()) - 1; i >= 0; --i)
	{
		Particle& Pa = Particles[i];
		Pa.Life -= Dt;
		Pa.Vel.z += Pa.Gravity * Dt;
		Pa.Vel = Mul(Pa.Vel, 1.0f - std::min(1.0f, Dt * 1.5f));
		Pa.Pos = Add(Pa.Pos, Mul(Pa.Vel, Dt));
		if (Pa.Life <= 0.0f) Particles.erase(Particles.begin() + i);
	}
	if (Particles.size() > 2000)
	{
		Particles.erase(Particles.begin(), Particles.begin() + (Particles.size() - 2000));
	}

	for (int s = static_cast<int>(Shockwaves.size()) - 1; s >= 0; --s)
	{
		Shockwave& W = Shockwaves[s];
		W.Radius += W.Speed * Dt;
		for (int i = static_cast<int>(Enemies.size()) - 1; i >= 0; --i)
		{
			Enemy& E = Enemies[i];
			if (std::find(W.AlreadyHit.begin(), W.AlreadyHit.end(), E.Serial) != W.AlreadyHit.end())
			{
				continue;
			}
			const Vector3 To = Sub(E.Pos, W.Center);
			if (Len(To) < W.Radius)
			{
				W.AlreadyHit.push_back(E.Serial);
				Vector3 K = Mul(Norm(To), 1200.0f);
				K.z = 900.0f;
				DamageEnemy(i, W.Damage, K);
			}
		}
		if (W.Radius >= W.MaxRadius) Shockwaves.erase(Shockwaves.begin() + s);
	}

	const Level& L = Levels[LevelIndex];
	for (int a = static_cast<int>(Anvils.size()) - 1; a >= 0; --a)
	{
		FallingAnvil& A = Anvils[a];
		A.VelZ += Gravity * 1.5f * Dt;
		A.Pos.z += A.VelZ * Dt;
		const RayHit Down = Raycast(L, Add(A.Pos, Vector3{ 0, 0, 60.0f }), Vector3{ 0, 0, -1 }, 80.0f);
		if (A.Pos.z <= A.TargetZ || Down.Hit)
		{
			for (int i = static_cast<int>(Enemies.size()) - 1; i >= 0; --i)
			{
				const Vector3 To = Sub(Enemies[i].Pos, A.Pos);
				if (Len2D(To) < 380.0f && std::fabs(To.z) < 300.0f)
				{
					DamageEnemy(i, 70.0f, Vector3{ To.x, To.y, 600.0f });
				}
			}
			SpawnBurst(A.Pos, Color{ 255, 230, 120, 255 }, 30, 700.0f, 16.0f, -800.0f);
			Anvils.erase(Anvils.begin() + a);
		}
	}
}

void Game::UpdateGoddard(float Dt)
{
	if (P.Hero != HeroId::Jimmy || Dt <= 0.0f)
	{
		return;
	}
	const Vector3 Feet = Sub(P.Pos, Vector3{ 0, 0, P.H });
	if (CarIndex >= 0 || Len(Sub(GoddardPos, Feet)) > 2500.0f)
	{
		GoddardPos = Feet; // rides along / catches up
		return;
	}
	// Trot alongside Jimmy, a little ahead on his left (keeps him in view).
	const Vector3 Side = YawDir(P.Yaw - 65.0f);
	Vector3 Target = Add(Feet, Mul(Side, 120.0f));
	Vector3 To = Sub(Target, GoddardPos);
	To.z = 0.0f;
	const float D = Len(To);
	const float Speed = std::min(D * 4.0f, 1400.0f);
	GoddardSpeed = D > 20.0f ? Speed : 0.0f;
	if (D > 1.0f)
	{
		GoddardPos = Add(GoddardPos, Mul(To, std::min(1.0f, Speed * Dt / D)));
		GoddardYaw += AngleDelta(GoddardYaw, YawOf(To)) * std::min(1.0f, Dt * 8.0f);
	}
	else
	{
		GoddardYaw += AngleDelta(GoddardYaw, P.Yaw) * std::min(1.0f, Dt * 4.0f);
	}
	const RayHit Down = Raycast(Levels[LevelIndex], Add(GoddardPos, Vector3{ 0, 0, 300.0f }), Vector3{ 0, 0, -1 }, 900.0f);
	const float GroundZ = Down.Hit ? Down.Point.z : Feet.z;
	GoddardPos.z += (GroundZ - GoddardPos.z) * std::min(1.0f, Dt * 10.0f);
}

void Game::UpdatePillars(float Dt)
{
	Level& L = Levels[LevelIndex];
	for (int i = static_cast<int>(Pillars.size()) - 1; i >= 0; --i)
	{
		EarthPillar& E = Pillars[i];
		E.Age += Dt;
		const float Life = 8.0f, Rise = 0.25f, Sink = 0.6f, MaxH = 650.0f;
		float T = std::min(1.0f, E.Age / Rise);
		if (E.Age > Life - Sink) T = std::max(0.0f, (Life - E.Age) / Sink);
		const float Height = std::max(10.0f, MaxH * T);
		for (Block& B : L.Blocks)
		{
			if (B.Label == E.Label)
			{
				B.Size.z = Height;
				B.Center.z = E.GroundZ + Height * 0.5f - 5.0f;
				FinalizeBlock(B);
			}
		}
		if (E.Age >= Life)
		{
			// Pillars are appended after the level's own blocks, so erasing them
			// never shifts the course gate indices.
			L.Blocks.erase(std::remove_if(L.Blocks.begin(), L.Blocks.end(), [&](const Block& B) { return B.Label == E.Label; }), L.Blocks.end());
			Pillars.erase(Pillars.begin() + i);
		}
	}
}

void Game::UpdateCourses(float Dt)
{
	const Level& L = Levels[LevelIndex];
	const Vector3 Pos = CarIndex >= 0 ? Cars[CarIndex].Pos : P.Pos;
	for (Course& C : Courses)
	{
		if (C.Running)
		{
			C.Time += Dt;
		}
		const Block& Gate = L.Blocks[C.Gates[C.Next]];
		const Vector3 Margin{ std::max(0.0f, 250.0f - Gate.Half.x), std::max(0.0f, 250.0f - Gate.Half.y), 0.0f };
		if (!InsideBlockBox(Gate, Pos, Margin))
		{
			continue;
		}
		if (!C.Running)
		{
			C.Running = true;
			C.Time = 0.0f;
			Say(C.Name + " started! Follow the glowing gates.", Color{ 140, 255, 200, 255 }, 3.0f);
		}
		++C.Next;
		if (C.Next >= static_cast<int>(C.Gates.size()))
		{
			C.Running = false;
			C.Next = 0;
			C.LastResult = C.Time;
			const bool Record = C.Best <= 0.0f || C.Time < C.Best;
			if (Record) C.Best = C.Time;
			char Buf[128];
			std::snprintf(Buf, sizeof(Buf), "%s finished: %.2fs%s", C.Name.c_str(), C.Time, Record ? "  NEW BEST!" : "");
			Say(Buf, Color{ 140, 255, 200, 255 }, 5.0f);
			Banner = Record ? "NEW BEST TIME!" : "FINISH!";
			BannerTime = 2.5f;
		}
	}
}

// ---------------------------------------------------------------------------
// Rift shards: collectibles on rooftops, ledges and up in the sky
// ---------------------------------------------------------------------------

void Game::SpawnShards()
{
	Shards.assign(Levels.size(), {});
	for (size_t Li = 0; Li < Levels.size(); ++Li)
	{
		const Level& L = Levels[Li];
		const int Want = Li == 0 ? 10 : 24;
		unsigned Seed = 4242u + static_cast<unsigned>(Li) * 977u;
		const Vector3 Lo = L.BoundsMin, Hi = L.BoundsMax;
		const float SpanX = Hi.x - Lo.x, SpanY = Hi.y - Lo.y;
		const float MinGap = std::max(SpanX, SpanY) / (Li == 0 ? 8.0f : 9.0f);
		for (int Try = 0; Try < 2000 && static_cast<int>(Shards[Li].size()) < Want; ++Try)
		{
			const Vector3 Top{ Lo.x + SpanX * (0.08f + 0.84f * Frand(Seed)), Lo.y + SpanY * (0.08f + 0.84f * Frand(Seed)), Hi.z - 20.0f };
			const RayHit Down = Raycast(L, Top, Vector3{ 0, 0, -1 }, Hi.z - Lo.z);
			if (!Down.Hit || Down.Point.z < Lo.z + 50.0f)
			{
				continue;
			}
			Vector3 Pos = Add(Down.Point, Vector3{ 0, 0, 110.0f });
			// Every fourth shard floats high up: fly, glide, grapple or pillar to it.
			if (Shards[Li].size() % 4 == 3)
			{
				Pos.z += 500.0f + 400.0f * Frand(Seed);
			}
			bool bTooClose = Len(Sub(Pos, L.Start)) < 400.0f;
			for (const Shard& O : Shards[Li])
			{
				bTooClose = bTooClose || Len2D(Sub(O.Pos, Pos)) < MinGap;
			}
			if (!bTooClose && !OverlapsSolid(L, Pos, 40.0f, 40.0f))
			{
				Shards[Li].push_back(Shard{ Pos, false });
			}
		}
	}
}

bool Game::NearestShard(Vector3& Out) const
{
	float Best = 1e30f;
	for (const Shard& S : Shards[LevelIndex])
	{
		const float D = Len(Sub(S.Pos, P.Pos));
		if (!S.Taken && D < Best)
		{
			Best = D;
			Out = S.Pos;
		}
	}
	return Best < 1e29f;
}

void Game::UpdateShards(float Dt)
{
	(void)Dt;
	std::vector<Shard>& List = Shards[LevelIndex];
	const Vector3 Center = CarIndex >= 0 ? Cars[CarIndex].Pos : P.Pos;
	const float Reach = (CarIndex >= 0 ? 180.0f : P.R + 70.0f) + 30.0f;
	for (Shard& S : List)
	{
		if (S.Taken || P.RespawnTimer > 0.0f)
		{
			continue;
		}
		const Vector3 To = Sub(S.Pos, Center);
		if (Len2D(To) < Reach && To.z < (CarIndex >= 0 ? 200.0f : P.H + 60.0f) && To.z > -(CarIndex >= 0 ? 200.0f : P.H + 60.0f))
		{
			S.Taken = true;
			++ShardsTaken;
			P.Meter = std::min(100.0f, P.Meter + 10.0f);
			P.Health = std::min(P.MaxHealth, P.Health + 15.0f);
			P.Energy = 100.0f;
			SpawnBurst(S.Pos, Color{ 190, 130, 255, 255 }, 30, 700.0f, 14.0f);
			SpawnRing(Particles, S.Pos, Vector3{ 0, 0, 1 }, Color{ 120, 230, 255, 255 }, 20, 600.0f, 10.0f);
			int Left = 0;
			for (const Shard& O : List) Left += O.Taken ? 0 : 1;
			const int Total = static_cast<int>(List.size());
			if (Left == 0)
			{
				Banner = "ALL RIFT SHARDS FOUND!";
				BannerTime = 3.0f;
			}
			Say("Rift shard " + std::to_string(Total - Left) + "/" + std::to_string(Total) + " (+health, energy, ultimate)", Color{ 200, 160, 255, 255 }, 2.0f);
		}
	}
}
