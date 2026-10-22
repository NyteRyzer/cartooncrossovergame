#pragma once

// Cel-shaded renderer: a colour pass with banded lighting, a normal/line-weight
// pass, and a Sobel outline post-process that mirrors the UE ToonCommon.ush
// approach (depth + normal edges, per-object line weight).

#include "raylib.h"
#include "raymath.h"

#include <cstdint>

enum class MeshKind : uint8_t { Cube, Cylinder, Sphere, Cone, Pyramid, Count };

enum class RenderPass : uint8_t { Color, Normals };

struct SceneLighting
{
	Vector3 SunDirection{ -0.4f, -0.8f, -0.3f }; // raylib space, pointing from the sun
	Color Sky{ 120, 180, 235, 255 };
	Color SkyHorizon{ 210, 230, 250, 255 };
	Color Ambient{ 120, 125, 150, 255 };
	float FogDensity = 0.0012f;
	float Bands = 3.0f;
};

class ToonRenderer
{
public:
	bool Init(int Width, int Height);
	void Shutdown();
	void Resize(int Width, int Height);

	// Begin rendering one pass. Camera in raylib space (metres).
	void BeginPass(RenderPass Pass, const Camera3D& Cam, const SceneLighting& Light);
	void EndPass();
	RenderPass CurrentPass() const { return Pass; }

	// Draws a unit mesh transformed by Transform (raylib space). Unit meshes
	// are centred on the origin with extent 1 on every axis.
	void Draw(MeshKind Kind, const Matrix& Transform, Color Tint, float LineWeight = 1.0f, float Emissive = 0.0f);

	// Camera-facing cut-out sprite ("paper doll"). Feet = bottom-centre in
	// raylib space; Right = horizontal camera-right; ToCamera = horizontal
	// direction toward the camera. Size in metres; Roll leans the sprite.
	void DrawSprite(const Texture2D& Tex, Vector3 Feet, Vector3 Right, Vector3 ToCamera, float Width, float Height,
	                float Roll, bool bFlip, Color Tint, float Flash, float LineWeight);

	// Composites colour + outlines to the backbuffer (call between BeginDrawing/EndDrawing).
	void Composite(const SceneLighting& Light);

	int Width() const { return W; }
	int Height() const { return H; }

	static constexpr float NearPlane = 0.1f;
	static constexpr float FarPlane = 3000.0f;

private:
	RenderTexture2D CreateTarget(int Width, int Height);
	void DestroyTarget(RenderTexture2D& Target);

	Mesh Meshes[static_cast<int>(MeshKind::Count)]{};
	Material CelMaterial{};
	Material NormalMaterial{};
	Shader CelShader{};
	Shader NormalShader{};
	Shader OutlineShader{};
	Shader SpriteShader{};
	Shader SpriteNormalShader{};
	Material SpriteMaterial{};
	Material SpriteNormalMaterial{};
	Mesh SpriteQuad{};
	int LocSpriteFlash = -1, LocSpriteFogColor = -1, LocSpriteFogDensity = -1, LocSpriteViewPos = -1, LocSpriteLineWeight = -1;
	RenderTexture2D ColorTarget{};
	RenderTexture2D NormalTarget{};

	int LocLightDir = -1, LocAmbient = -1, LocFogColor = -1, LocFogDensity = -1, LocBands = -1;
	int LocViewPos = -1, LocEmissive = -1, LocLineWeight = -1;
	int LocNormalTex = -1, LocDepthTex = -1, LocTexel = -1, LocNearFar = -1, LocSkyTop = -1;

	RenderPass Pass = RenderPass::Color;
	int W = 0, H = 0;
	bool bReady = false;
};

// Helpers to build transforms in raylib space.
Matrix MakeTransform(Vector3 Position, Vector3 Scale, float YawRad, float PitchRad = 0.0f, float RollRad = 0.0f);
