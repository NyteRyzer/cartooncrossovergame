#pragma once

// Level data and collision for the prototype.
//
// All gameplay runs in Unreal space (centimetres, Z up, X forward, Y right) so
// tuning numbers and the shared headers (HoverMath.h, OmnitrixStateMachine.h)
// mean exactly what they mean in the UE project. ToRender() converts to
// raylib space (metres, Y up) only when drawing.

#include "raylib.h"

#include <string>
#include <vector>

enum class Shape { Cube, Cylinder, Sphere, Cone, Plane };

struct Block
{
	std::string Label;
	std::string Folder;
	Shape Kind = Shape::Cube;
	Vector3 Center{};   // UE cm
	Vector3 Size{};     // full extents, UE cm
	float YawDeg = 0.0f;
	float PitchDeg = 0.0f;
	bool Solid = true;
	bool PhaseBlocker = false;

	// Derived collision data.
	Vector3 Half{};
	float CosYaw = 1.0f;
	float SinYaw = 0.0f;
	float BoundRadiusXY = 0.0f;
	bool VerticalCylinder = false;
};

struct Marker
{
	std::string Label;
	std::string Class;
	Vector3 Location{};
	float YawDeg = 0.0f;
};

struct Level
{
	std::string Name;
	std::string FileStem;
	Vector3 BoundsMin{};
	Vector3 BoundsMax{};
	Vector3 Start{};
	float StartYaw = 0.0f;
	std::vector<Block> Blocks;
	std::vector<Marker> Markers;

	const Block* FindBlock(const std::string& Label) const;
	int FindBlockIndex(const std::string& Label) const;
};

bool LoadLevel(const std::string& Path, Level& Out, std::string& Error);

// Unreal cm (Z up) -> raylib metres (Y up).
inline Vector3 ToRender(Vector3 Ue) { return Vector3{ Ue.x * 0.01f, Ue.z * 0.01f, Ue.y * 0.01f }; }

struct CollisionResult
{
	bool Grounded = false;
	bool HitCeiling = false;
	bool HitWall = false;
	Vector3 WallNormal{};
};

enum class CollisionMode
{
	Solid,      // everything solid blocks
	Phasing     // only PhaseBlocker blocks (Danny intangible)
};

// Pushes a vertical cylinder (centre P, radius R, half height H) out of the
// level's solids. StepUp lets small ledges be climbed instead of blocking.
CollisionResult ResolveCylinder(const Level& L, Vector3& P, float R, float H, CollisionMode Mode, float StepUp = 40.0f);

bool OverlapsSolid(const Level& L, Vector3 P, float R, float H);

struct RayHit
{
	bool Hit = false;
	float Distance = 0.0f;
	Vector3 Point{};
	Vector3 Normal{};
	int BlockIndex = -1;
};

RayHit Raycast(const Level& L, Vector3 Origin, Vector3 Dir, float MaxDistance, bool IncludePhaseBlockersOnly = false);

// True if P lies inside the block's oriented box (yaw only), expanded by Margin.
bool InsideBlockBox(const Block& B, Vector3 P, Vector3 Margin);
