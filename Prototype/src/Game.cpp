#include "Game.h"

#include "Traversal/HoverMath.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

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
		{ HeroId::Aang, "Aang", "Eastern Air Temple", "Double Jump + Glider (hold Space) + Air Scooter (Q)", "Air Scooter", "The Avatar State",
		  { 245, 205, 165, 255 }, { 240, 190, 60, 255 }, { 230, 110, 40, 255 }, { 245, 205, 165, 255 }, { 80, 180, 255, 255 },
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
		Levels.push_back(std::move(L));
	}

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

	P.Omni = Omnitrix::FStateMachine(OmnitrixConfig());
	LevelIndex = 0;
	LoadLevelState();
	ResetPlayer();
	Say("Welcome to the Nexus Observatory! Walk up to a terminal and press E.", Color{ 180, 255, 200, 255 }, 6.0f);
	return true;
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
	CamYaw = L.StartYaw;
	CamPitch = -12.0f;
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
	if (P.Intangible || CarIndex >= 0 || P.RespawnTimer > 0.0f)
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
	const float WorldDt = bDial ? Dt * DialTimeDilation : Dt;

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

	UpdateProjectiles(WorldDt);
	UpdateEnemies(WorldDt);
	UpdateRifts(WorldDt);
	UpdateEffects(WorldDt);
	UpdateCourses(Dt);

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

	float Desired = 420.0f * std::pow(P.H / 70.0f, 0.85f);
	if (CarIndex >= 0) Desired = 950.0f;
	else if (P.Flying) Desired += 120.0f;
	CamDist += (Desired - CamDist) * std::min(1.0f, Dt * 5.0f);

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
	CamPos = Want;
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
	P.RegenDelay -= Dt;
	if (P.RegenDelay <= 0.0f)
	{
		P.Health = std::min(P.MaxHealth, P.Health + 8.0f * Dt);
	}
	if (!P.Intangible && P.BreathTime <= 0.0f)
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
			if (P.Flying) P.Vel.z = std::max(P.Vel.z, 300.0f);
		}
	}
	if (Form || !H.CanFly)
	{
		P.Flying = false;
	}
	P.Boosting = In.Boost;

	if (P.Hero == HeroId::Danny)
	{
		UpdateGhost(Dt, In);
	}

	// Desired move direction from the camera.
	const Vector3 Fwd2D = YawDir(CamYaw);
	const Vector3 Right2D{ -std::sin(CamYaw * Deg2Rad), std::cos(CamYaw * Deg2Rad), 0.0f };
	Vector3 Wish = bLocked ? Vector3{} : Add(Mul(Fwd2D, In.MoveY), Mul(Right2D, In.MoveX));
	if (Len(Wish) > 1.0f) Wish = Norm(Wish);

	if (P.Flying)
	{
		Vector3 Wish3 = Add(Mul(CamForward(), In.MoveY), Mul(Right2D, In.MoveX));
		if (Len(Wish3) > 1.0f) Wish3 = Norm(Wish3);
		const float Speed = P.Boosting ? H.FlyBoost : H.FlySpeed;
		const float Vertical = (In.JumpHeld ? 1.0f : 0.0f) - (In.Down ? 1.0f : 0.0f);
		Vector3 Target = Mul(Wish3, Speed);
		Target.z += Vertical * Speed * 0.7f;
		const float Accel = 4000.0f * Dt;
		P.Vel.x = Approach(P.Vel.x, Target.x, Accel);
		P.Vel.y = Approach(P.Vel.y, Target.y, Accel);
		P.Vel.z = Approach(P.Vel.z, Target.z, Accel);
		P.Gliding = false;
	}
	else
	{
		float Speed = WalkSpeed() * (In.Boost && !Form ? 1.45f : 1.0f);
		const bool bRolling = Form && Form->Roll;
		const float Accel = (P.Grounded ? (bRolling ? 2500.0f : 5000.0f) : 1800.0f) * Dt;

		P.Gliding = false;
		const bool CanGlide = (Form ? Form->Glide : H.CanGlide);
		if (!P.Grounded && CanGlide && In.JumpHeld && P.Vel.z < 0.0f)
		{
			P.Gliding = true;
			Speed *= 1.6f;
		}
		const Vector3 Target = Mul(Wish, Speed);
		P.Vel.x = Approach(P.Vel.x, Target.x, Accel);
		P.Vel.y = Approach(P.Vel.y, Target.y, Accel);

		const float GravScale = Form ? Form->GravityScale : 1.0f;
		float G = Gravity * GravScale;
		if (P.Grappling) G *= 0.35f;
		P.Vel.z += G * Dt;
		if (P.Gliding) P.Vel.z = std::max(P.Vel.z, -220.0f);

		if (In.Jump && !bLocked)
		{
			if (P.Grounded)
			{
				P.Vel.z = JumpZ();
				P.Grounded = false;
			}
			else if (P.AirJumpsLeft > 0 && !Form)
			{
				P.Vel.z = JumpZ() * 0.9f;
				--P.AirJumpsLeft;
				SpawnBurst(Sub(P.Pos, Vector3{ 0, 0, P.H }), Color{ 230, 245, 255, 255 }, 14, 400.0f);
			}
		}
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
	if (C.Grounded && P.Vel.z < 0.0f)
	{
		if (!WasGrounded && P.Vel.z < -1400.0f && P.H > 300.0f)
		{
			// Way Big landing quake.
			Shockwaves.push_back(Shockwave{ Sub(P.Pos, Vector3{ 0, 0, P.H }), 0.0f, 1800.0f, 6000.0f, 60.0f, Color{ 255, 255, 255, 255 }, {} });
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
	if (C.HitWall)
	{
		const float Into = Dot(P.Vel, C.WallNormal);
		if (Into < 0.0f)
		{
			P.Vel = Sub(P.Vel, Mul(C.WallNormal, Into));
		}
		if (P.Grappling)
		{
			P.Grappling = false;
		}
	}

	// Facing.
	const float HSpeed = Len2D(P.Vel);
	if (P.AttackAnim > 0.0f || P.BreathTime > 0.0f)
	{
		P.Yaw += AngleDelta(P.Yaw, CamYaw) * std::min(1.0f, Dt * 20.0f);
	}
	else if (HSpeed > 30.0f)
	{
		P.Yaw += AngleDelta(P.Yaw, YawOf(P.Vel)) * std::min(1.0f, Dt * 12.0f);
	}
	P.WalkPhase += HSpeed * Dt * 0.025f * (70.0f / P.H);
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
		P.AttackCd = A.Cooldown;
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
	P.AttackAnim = 0.25f;
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
			Pr.Damage = A.Damage;
			Pr.Radius = A.Radius;
			Pr.Knockback = A.Knockback;
			Pr.Tint = A.Tint;
			Pr.Life = 2.5f;
			Projectiles.push_back(Pr);
		}
	}
	else if (A.Kind == AttackKind::Melee)
	{
		const Vector3 Facing = YawDir(CamYaw);
		P.Yaw = CamYaw;
		for (int i = static_cast<int>(Enemies.size()) - 1; i >= 0; --i)
		{
			const Vector3 To = Sub(Enemies[i].Pos, P.Pos);
			const float D = Len2D(To);
			if (D < A.Range + P.R + 45.0f && std::fabs(To.z) < P.H + 120.0f && Dot(Norm(Vector3{ To.x, To.y, 0 }), Facing) > 0.2f)
			{
				Vector3 Knock = Mul(Norm(Vector3{ To.x, To.y, 0 }), A.Knockback);
				Knock.z = A.Knockback * 0.4f;
				DamageEnemy(i, A.Damage, Knock);
			}
		}
		const Vector3 Hand = Add(Add(P.Pos, Vector3{ 0, 0, P.H * 0.3f }), Mul(Facing, P.R + A.Range * 0.5f));
		SpawnBurst(Hand, A.Tint, 8, 300.0f, 10.0f + P.H * 0.05f);
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
			DamagePlayer(9.0f);
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
