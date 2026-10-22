#pragma once

#include "Omnitrix/OmnitrixStateMachine.h"
#include "Render.h"
#include "World.h"

#include <string>
#include <vector>

// One frame of player intent. Filled from the keyboard/mouse, or by the
// scripted smoke test.
struct InputState
{
	float MoveX = 0.0f;   // strafe right
	float MoveY = 0.0f;   // forward
	float LookX = 0.0f;   // degrees this frame
	float LookY = 0.0f;
	bool Jump = false, JumpHeld = false, Down = false, Boost = false;
	bool Attack = false, AttackHeld = false;
	bool Sig = false, SigHeld = false, Sig2 = false, Ult = false;
	bool Interact = false, Traversal = false, Glitch = false;
	int Number = 0;       // 1-9 pressed this frame
	bool Back = false, Help = false, Recall = false, Travel = false;
	bool DebugSpawn = false, DebugMeter = false, DebugNextHero = false;
	bool ToggleArt = false;
	bool DownPressed = false;  // Ctrl/C this frame (ground pound in the air)
	bool Dodge = false;        // V / Left Alt: dodge roll, air dash, barrel roll
	bool Emote = false;        // B: hero signature pose
};

enum class HeroId : int { Jimmy, Timmy, Danny, Jake, XJ9, ElTigre, Ben, Aang, Count };

enum class AttackKind { None, Projectile, Melee, Cone };

struct AttackDef
{
	AttackKind Kind = AttackKind::None;
	float Damage = 10.0f;
	float Cooldown = 0.3f;
	float Speed = 5000.0f;     // projectile cm/s
	float Range = 200.0f;      // melee / cone reach
	float Radius = 25.0f;      // projectile radius
	float Knockback = 400.0f;
	int Count = 1;             // projectiles per shot (spread)
	Color Tint = WHITE;
	const char* Name = "";
};

struct HeroDef
{
	HeroId Id;
	const char* Name;
	const char* Universe;
	const char* TraversalName;
	const char* SigName;
	const char* UltName;
	Color Skin, Primary, Secondary, Hair, Accent;
	float WalkSpeed;
	float JumpZ;
	bool CanFly;
	float FlySpeed;
	float FlyBoost;
	int AirJumps;
	bool CanGlide;
	float LineWeight;          // per-show line thickness (docs/RENDERING.md)
	AttackDef Attack;
};

struct FormDef
{
	const char* Name;
	Color Body, Accent;
	float Radius, HalfHeight;
	float WalkSpeed, JumpZ, GravityScale;
	bool Glide, Roll;
	AttackDef Attack;
};

struct HoverCar
{
	Vector3 Pos{};
	Vector3 Vel{};
	float Yaw = 0.0f;           // degrees
	float YawRate = 0.0f;       // deg/s
	float CruiseAltitude = 120.0f;
	float PrevDist[4]{ 300, 300, 300, 300 };
	float VisPitch = 0.0f, VisRoll = 0.0f;
	bool Occupied = false;
};

struct Enemy
{
	Vector3 Pos{};
	Vector3 Vel{};
	float Health = 40.0f;
	float HitFlash = 0.0f;
	float AttackCd = 0.0f;
	float Phase = 0.0f;
	int Rift = -1;
	int Serial = 0;
	bool Grounded = false;
};

struct Projectile
{
	Vector3 Pos{};
	Vector3 Vel{};
	float Life = 2.0f;
	float Damage = 10.0f;
	float Radius = 25.0f;
	float Knockback = 300.0f;
	Color Tint = WHITE;
	bool Homing = false;
};

struct Particle
{
	Vector3 Pos{};
	Vector3 Vel{};
	float Life = 0.5f, MaxLife = 0.5f;
	float Size = 12.0f;
	float Gravity = 0.0f;
	Color Tint = WHITE;
};

struct Shockwave
{
	Vector3 Center{};
	float Radius = 0.0f, MaxRadius = 2500.0f, Speed = 5000.0f;
	float Damage = 150.0f;
	Color Tint = WHITE;
	std::vector<int> AlreadyHit; // enemy serials
};

struct FallingAnvil
{
	Vector3 Pos{};
	float VelZ = 0.0f;
	float TargetZ = 0.0f;
};

struct Rift
{
	Vector3 Pos{};
	std::string Name;
	int Wave = 0;          // 0 = dormant, 1..3 active
	float WaveTimer = 0.0f;
	bool Closed = false;
};

struct Course
{
	std::string Name;
	std::vector<int> Gates;   // block indices in order
	int Next = 0;
	bool Running = false;
	float Time = 0.0f;
	float Best = 0.0f;
	float LastResult = 0.0f;
};

struct Message
{
	std::string Text;
	float Time = 0.0f;
	Color Tint = WHITE;
};

class Game
{
public:
	bool Init(const std::string& DataDir, std::string& Error);
	void Shutdown();
	const Texture2D& Portrait(HeroId Id) const { return Portraits[static_cast<int>(Id)]; }
	void Update(float Dt, const InputState& In);
	void Render(ToonRenderer& R);
	void DrawHud(int ScreenW, int ScreenH);
	SceneLighting Lighting() const;
	Camera3D MakeCamera() const;

	bool IsTerminalOpen() const { return TerminalMenu >= 0; }
	bool SpriteArtEnabled() const { return bSpriteArt; }
	bool HasSprite(HeroId Id) const { return Sprites[static_cast<int>(Id)].Tex.id != 0; }
	bool IsPaused() const { return bPaused; }
	void SetPaused(bool bInPaused) { bPaused = bInPaused; }
	bool WantsQuit() const { return bQuit; }
	void RequestQuit() { bQuit = true; }

	// --- Hooks used by the smoke test ---------------------------------------
	void SelectHero(HeroId Id);
	void TravelTo(int LevelIndex);
	int LevelCount() const { return static_cast<int>(Levels.size()); }
	int CurrentLevel() const { return LevelIndex; }
	void TeleportPlayer(Vector3 Pos, float Yaw);
	bool EnterNearestCar();
	void ExitCar();
	void FillMeter() { P.Meter = 100.0f; }
	void SpawnEnemiesNearPlayer(int Count);
	void ClearEnemies() { Enemies.clear(); P.Health = P.MaxHealth; }
	void SetCameraAngles(float Yaw, float Pitch) { CamYaw = Yaw; CamPitch = Pitch; }
	Vector3 PlayerPos() const { return P.Pos; }
	const Level& CurrentLevelData() const { return Levels[LevelIndex]; }
	bool PlayerIsValid() const;
	bool PlayerIntangible() const { return P.Intangible; }
	Omnitrix::EState OmnitrixState() const { return P.Omni.GetState(); }
	int EnemyCount() const { return static_cast<int>(Enemies.size()); }
	int KillCount() const { return Kills; }
	int PillarCount() const { return static_cast<int>(Pillars.size()); }
	int CourseProgress() const { return Courses.empty() ? -1 : Courses[0].Next; }
	bool IsDodging() const { return P.DodgeTime > 0.0f; }
	bool IsPounding() const { return P.Pounding; }
	int PoundCount() const { return Pounds; }
	bool IsEmoting() const { return P.EmoteTime > 0.0f; }
	bool IsFlying() const { return P.Flying; }
	bool IsGliding() const { return P.Gliding; }
	float PlayerSpeed() const;
	float FlightBank() const { return P.FlyBank; }
	int ComboStep() const { return P.AttackStep; }
	int ShardsCollected() const { return ShardsTaken; }
	bool NearestShard(Vector3& Out) const;
	int WallJumps() const { return WallJumpCount; }
	const HoverCar* PlayerCar() const { return CarIndex >= 0 ? &Cars[CarIndex] : nullptr; }

private:
	struct PlayerState
	{
		HeroId Hero = HeroId::Jimmy;
		Vector3 Pos{};
		Vector3 Vel{};
		float Yaw = 0.0f;
		float R = 32.0f, H = 70.0f;
		bool Grounded = false;
		bool Flying = false;
		bool Boosting = false;
		float Health = 100.0f, MaxHealth = 100.0f;
		float Meter = 0.0f;
		float Energy = 100.0f;
		float AttackCd = 0.0f, SigCd = 0.0f, Sig2Cd = 0.0f;
		float HurtFlash = 0.0f, RegenDelay = 0.0f;
		bool Intangible = false, WantsIntangible = false;
		Vector3 LastSafe{};
		int AirJumpsLeft = 0;
		bool Gliding = false;
		bool Grappling = false;
		Vector3 GrapplePoint{};
		float ScooterTime = 0.0f;
		float WalkPhase = 0.0f;
		float AttackAnim = 0.0f;
		float BreathTime = 0.0f;
		float RespawnTimer = 0.0f;
		Omnitrix::FStateMachine Omni;
		int AppliedForm = Omnitrix::NoForm;
		bool SpriteFlip = false;
		float LandTimer = 0.0f;

		// Flight feel: visual bank/pitch, boost, barrel rolls, wing flaps.
		float FlyBank = 0.0f, FlyPitch = 0.0f, FlapPhase = 0.0f;
		float TakeoffTime = 0.0f, BoostFx = 0.0f;
		bool WasBoosting = false;
		float GlideSpeed = 0.0f;
		// Dodge roll / air dash / barrel roll.
		float DodgeTime = 0.0f, DodgeCd = 0.0f;
		Vector3 DodgeDir{};
		int DodgeSide = 1;
		bool DodgeAir = false, AirDashUsed = false;
		// Attack combos: step 0, 1, 2 (finisher).
		int AttackStep = 0;
		float AttackDur = 0.25f, LastAttack = -10.0f;
		int HitCombo = 0;
		float HitComboTimer = 0.0f;
		// Jumping: coyote time, buffered jumps, variable height, flips, wall jumps.
		float Coyote = 0.0f, JumpBuffer = 0.0f;
		bool JumpCut = false;
		float FlipTime = 0.0f;
		float WallTimer = 0.0f, WallSlide = 0.0f;
		Vector3 WallNormal{};
		// Ground pound.
		bool Pounding = false;
		float PoundLand = 0.0f;
		// Emotes and idle fidgets.
		float EmoteTime = 0.0f;
		float IdleTime = 0.0f;
	};

	// Setup
	void LoadLevelState();
	void ResetPlayer();
	Omnitrix::FConfig OmnitrixConfig() const;

	// Per-frame
	void UpdateWorld(float Dt, const InputState& In);
	void UpdateCamera(float Dt, const InputState& In);
	void UpdateOmnitrix(float RealDt, const InputState& In);
	void UpdatePlayer(float Dt, const InputState& In);
	void UpdateGhost(float Dt, const InputState& In);
	void UpdateFlight(float Dt, const InputState& In);
	void UpdateGlide(float Dt, const InputState& In);
	void UpdateMoves(float Dt, const InputState& In);
	void UpdateShards(float Dt);
	void SpawnShards();
	void RenderShards(ToonRenderer& R);
	void UpdateCar(HoverCar& Car, float Dt, const InputState* In);
	void UpdateCombat(float Dt, const InputState& In);
	void UpdateProjectiles(float Dt);
	void UpdateEnemies(float Dt);
	void UpdateRifts(float Dt);
	void UpdateEffects(float Dt);
	void UpdateCourses(float Dt);
	void UpdateInteraction(const InputState& In);
	void UpdateTerminalMenu(const InputState& In);

	// Combat helpers
	void DoAttack(const AttackDef& A);
	void DamageEnemy(int Index, float Amount, Vector3 Knock);
	void DamagePlayer(float Amount);
	void ConeDamage(float Range, float HalfAngleDeg, float Damage, float Knock, Color Tint);
	void SpawnBurst(Vector3 Pos, Color Tint, int Count, float Speed, float Size = 14.0f, float Gravity = 0.0f);
	void TriggerUltimate();
	void KillEnemy(int Index);

	// Queries
	const HeroDef& Hero() const;
	const FormDef* ActiveForm() const;
	float WalkSpeed() const;
	float JumpZ() const;
	bool CanAttack() const;
	bool IsImmune() const;
	Vector3 CamForward() const;
	Vector3 AimPoint() const;
	Vector3 PlayerFocus() const;
	void ApplyForm(int FormId);
	void SetPlayerSize(float R, float H);
	void Say(const std::string& Text, Color Tint = WHITE, float Seconds = 3.5f);

	// Rendering
	void RenderScene(ToonRenderer& R, bool bTranslucent);
	void RenderBlocks(ToonRenderer& R, bool bTranslucent);
	void BuildStaticBatch(ToonRenderer& R);
	void RenderGate(ToonRenderer& R, const Block& B, Color Tint, float Emissive);
	void RenderHero(ToonRenderer& R, bool bTranslucent);
	void RenderCar(ToonRenderer& R, const HoverCar& Car);
	void RenderTerminal(ToonRenderer& R, const Marker& M, int Index);
	void RenderShadow(ToonRenderer& R, Vector3 Pos, float Radius);
	void RenderGoddard(ToonRenderer& R);
	bool RenderHeroSprite(ToonRenderer& R, unsigned char Alpha, bool bHurt);
	void RenderHero3D(ToonRenderer& R, unsigned char Alpha, bool bHurt);
	void UpdateGoddard(float Dt);
	void UpdatePillars(float Dt);

	std::vector<Level> Levels;
	int LevelIndex = 0;
	int BatchLevel = -1;

	PlayerState P;
	std::vector<HoverCar> Cars;
	int CarIndex = -1;
	std::vector<Enemy> Enemies;
	int NextSerial = 1;
	std::vector<Projectile> Projectiles;
	std::vector<Particle> Particles;
	std::vector<Shockwave> Shockwaves;
	std::vector<FallingAnvil> Anvils;
	std::vector<Rift> Rifts;
	std::vector<Course> Courses;
	std::vector<Message> Messages;
	std::vector<Vector3> Stars;

	float CamYaw = 0.0f, CamPitch = -12.0f, CamDist = 420.0f;
	Vector3 CamPos{};
	float Time = 0.0f;
	float HelpTimer = 12.0f;
	bool bShowHelp = true;
	bool bPaused = false;
	bool bQuit = false;
	int TerminalMenu = -1;        // marker index of the open terminal
	std::string Prompt;
	std::string Banner;
	float BannerTime = 0.0f;
	int Kills = 0;
	int RiftsClosed = 0;
	int Pounds = 0;
	int WallJumpCount = 0;
	float CamShake = 0.0f;
	float HitStop = 0.0f;
	float CamFov = 70.0f;

	// Rift shards: collectibles scattered over every level (rooftops, ledges, sky).
	struct Shard
	{
		Vector3 Pos{};
		bool Taken = false;
	};
	std::vector<std::vector<Shard>> Shards;   // per level
	int ShardsTaken = 0;

	// Goddard follows Jimmy around.
	Vector3 GoddardPos{};
	float GoddardYaw = 0.0f;
	float GoddardSpeed = 0.0f;

	// Aang's earth pillars live in the level as temporary solid blocks.
	struct EarthPillar
	{
		std::string Label;
		float Age = 0.0f;
		float GroundZ = 0.0f;
		Vector3 Base{};
		float Yaw = 0.0f;
	};
	std::vector<EarthPillar> Pillars;
	int PillarSerial = 0;

	// Hero portraits cut from the production-bible concept sheet.
	Texture2D Portraits[static_cast<int>(HeroId::Count)]{};

	// Full-body cut-outs from the same sheet (tools/make_sprites.py), drawn as
	// camera-facing "paper doll" sprites. Indices 0-7 follow HeroId.
	enum SpriteId { SprHeatblast = static_cast<int>(HeroId::Count), SprFourArms, SprDragon, SprFairies, SprCount };
	// A cut-out split into posable parts (tools/make_sprites.py RIGS).
	struct RigPart
	{
		std::string Name;
		Texture2D Tex{};
		float X = 0, Y = 0, W = 0, H = 0;   // sprite pixels
		float PX = 0, PY = 0;               // pivot, sprite pixels
		int Z = 0;
	};
	struct SpriteArt
	{
		Texture2D Tex{};
		float HeightCm = 150.0f;
		float RigW = 0.0f, RigH = 0.0f;
		std::vector<RigPart> Parts;         // sorted by Z; empty = draw the whole sprite
	};
	// Joint angles (degrees, counter-clockwise as seen) and offsets for one frame.
	struct Pose
	{
		float ArmA = 0, ArmB = 0, LegA = 0, LegB = 0, Head = 0, Tail = 0, Wing = 0, Staff = 0;
		float LiftA = 0, LiftB = 0;   // leg lift, fraction of sprite height
		float StaffLift = 0;          // fraction of sprite height
		float HeadScaleY = 1.0f;
	};
	Pose ComputePose(int SpriteIndex) const;
	SpriteArt Sprites[SprCount];
	Texture2D Faces[SprCount]{};   // face decals for the 3D models
	bool bSpriteArt = false;       // M toggles 3D models / cut-outs
};
