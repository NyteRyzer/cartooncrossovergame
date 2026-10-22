#include "Game.h"

#include <algorithm>
#include <cmath>
#include <cstring>

const HeroDef& GetHeroDef(HeroId Id);
const FormDef* GetFormDef(int FormId);

namespace
{
	constexpr float Deg2Rad = 3.14159265358979f / 180.0f;
	constexpr float EnvLineWeight = 1.2f;

	Vector3 Add(Vector3 A, Vector3 B) { return Vector3{ A.x + B.x, A.y + B.y, A.z + B.z }; }
	Vector3 Sub(Vector3 A, Vector3 B) { return Vector3{ A.x - B.x, A.y - B.y, A.z - B.z }; }
	Vector3 Mul(Vector3 A, float S) { return Vector3{ A.x * S, A.y * S, A.z * S }; }
	float Len(Vector3 A) { return std::sqrt(A.x * A.x + A.y * A.y + A.z * A.z); }

	Color WithAlpha(Color C, unsigned char A) { return Color{ C.r, C.g, C.b, A }; }
	Color Shade(Color C, float F)
	{
		auto Ch = [F](unsigned char V) { return static_cast<unsigned char>(std::clamp(V * F, 0.0f, 255.0f)); };
		return Color{ Ch(C.r), Ch(C.g), Ch(C.b), C.a };
	}
	Color Lerp(Color A, Color B, float T)
	{
		auto Ch = [T](unsigned char X, unsigned char Y) { return static_cast<unsigned char>(X + (Y - X) * T); };
		return Color{ Ch(A.r, B.r), Ch(A.g, B.g), Ch(A.b, B.b), Ch(A.a, B.a) };
	}

	unsigned Hash(const std::string& S)
	{
		unsigned H = 2166136261u;
		for (char C : S) H = (H ^ static_cast<unsigned char>(C)) * 16777619u;
		return H;
	}

	bool StartsWith(const std::string& S, const char* Prefix) { return S.rfind(Prefix, 0) == 0; }
	bool Contains(const std::string& S, const char* Part) { return S.find(Part) != std::string::npos; }

	const Color UniverseColors[] = {
		{ 230, 60, 60, 255 },   // Retroville
		{ 245, 120, 190, 255 }, // Dimmsdale
		{ 90, 255, 130, 255 },  // Amity Park
		{ 240, 90, 40, 255 },   // Chinatown
		{ 80, 220, 255, 255 },  // Tremorton
		{ 255, 160, 40, 255 },  // Miracle City
		{ 140, 255, 60, 255 },  // Bellwood
		{ 120, 190, 255, 255 }, // Air Temple
	};
	const char* UniverseOrder[] = { "Retroville", "Dimmsdale", "AmityPark", "Chinatown", "Tremorton", "MiracleCity", "Bellwood", "AirTemple" };

	Color UniverseColorFor(const std::string& Label)
	{
		for (int i = 0; i < 8; ++i)
		{
			if (Contains(Label, UniverseOrder[i])) return UniverseColors[i];
		}
		return Color{ 200, 200, 255, 255 };
	}

	// Oriented box in UE space (yaw/pitch in degrees, size = full extents in block-local axes).
	Matrix BoxTransform(Vector3 CenterUe, Vector3 SizeUe, float YawDeg, float PitchDeg = 0.0f, float RollDeg = 0.0f)
	{
		return MakeTransform(ToRender(CenterUe), Vector3{ SizeUe.x * 0.01f, SizeUe.z * 0.01f, SizeUe.y * 0.01f },
		                     -YawDeg * Deg2Rad, PitchDeg * Deg2Rad, RollDeg * Deg2Rad);
	}

	struct BlockStyle
	{
		Color Tint;
		bool Translucent = false;
		float Emissive = 0.0f;
		bool Hidden = false;
		bool Gate = false;
		bool Roof = false;
	};

	BlockStyle StyleFor(const Block& B, int LevelIndex)
	{
		BlockStyle S;
		const std::string& L = B.Label;
		const unsigned H = Hash(L);
		if (Contains(L, "_Volume")) { S.Hidden = true; return S; }
		if (StartsWith(L, "FlightGate") || StartsWith(L, "RaceGate")) { S.Gate = true; S.Tint = Color{ 120, 255, 210, 255 }; return S; }
		if (StartsWith(L, "EarthPillar")) { S.Tint = Color{ 115, 92, 72, 255 }; return S; }

		if (LevelIndex == 0)
		{
			if (StartsWith(L, "Platform_Main")) S.Tint = Color{ 95, 85, 150, 255 };
			else if (StartsWith(L, "Platform_Underside")) S.Tint = Color{ 60, 50, 105, 255 };
			else if (StartsWith(L, "Railing")) S.Tint = Color{ 205, 195, 255, 255 };
			else if (StartsWith(L, "RiftCore_Orb")) { S.Tint = Color{ 90, 255, 210, 255 }; S.Emissive = 0.9f; }
			else if (StartsWith(L, "RiftCore")) S.Tint = Color{ 150, 140, 210, 255 };
			else if (StartsWith(L, "TerminalPad")) { S.Tint = UniverseColorFor(L); S.Emissive = 0.35f; }
			else if (StartsWith(L, "PortalSurface")) { S.Tint = WithAlpha(UniverseColorFor(L), 150); S.Translucent = true; S.Emissive = 0.75f; }
			else if (StartsWith(L, "Portal")) S.Tint = Color{ 170, 160, 215, 255 };
			else if (StartsWith(L, "Service")) S.Tint = Color{ 245, 205, 95, 255 };
			else if (StartsWith(L, "Scale")) S.Tint = Color{ 245, 130, 95, 255 };
			else S.Tint = Color{ 180, 180, 200, 255 };
			return S;
		}

		static const Color HouseColors[] = { { 250, 240, 210, 255 }, { 190, 220, 250, 255 }, { 250, 200, 210, 255 }, { 250, 235, 150, 255 }, { 200, 245, 210, 255 }, { 230, 210, 250, 255 } };
		static const Color TowerColors[] = { { 150, 170, 200, 255 }, { 120, 145, 185, 255 }, { 175, 185, 205, 255 }, { 110, 160, 190, 255 } };
		if (L == "Ground") S.Tint = Color{ 115, 195, 95, 255 };
		else if (StartsWith(L, "Road")) S.Tint = Color{ 75, 75, 85, 255 };
		else if (StartsWith(L, "Suburb")) S.Tint = HouseColors[H % 6];
		else if (StartsWith(L, "Downtown")) S.Tint = TowerColors[H % 4];
		else if (L == "Neutron_House") S.Tint = Color{ 245, 245, 250, 255 };
		else if (StartsWith(L, "Neutron_Lab")) S.Tint = Color{ 205, 60, 60, 255 };
		else if (StartsWith(L, "School")) S.Tint = Color{ 200, 95, 70, 255 };
		else if (StartsWith(L, "CandyBar")) S.Tint = Color{ 250, 165, 200, 255 };
		else if (L == "Park_Lawn") S.Tint = Color{ 90, 170, 75, 255 };
		else if (StartsWith(L, "Park_Silo")) S.Tint = Color{ 155, 155, 170, 255 };
		else if (StartsWith(L, "Retroland_Ferris")) S.Tint = Color{ 255, 90, 100, 255 };
		else if (StartsWith(L, "Retroland_Plaza")) S.Tint = Color{ 240, 205, 125, 255 };
		else if (StartsWith(L, "Retroland_Coaster")) S.Tint = Color{ 255, 220, 80, 255 };
		else if (StartsWith(L, "Boundary")) { S.Tint = Color{ 110, 200, 255, 40 }; S.Translucent = true; S.Emissive = 1.0f; }
		else S.Tint = Color{ 200, 200, 210, 255 };
		return S;
	}

	MeshKind MeshFor(Shape K)
	{
		switch (K)
		{
		case Shape::Cylinder: return MeshKind::Cylinder;
		case Shape::Sphere: return MeshKind::Sphere;
		case Shape::Cone: return MeshKind::Cone;
		default: return MeshKind::Cube;
		}
	}

	// Body-part drawing in a character's local frame (UE cm, forward/right/up from the feet).
	struct Rig
	{
		ToonRenderer* R;
		Vector3 Feet;
		float Yaw;
		float Scale;
		float LineWeight;
		unsigned char Alpha;

		Vector3 World(float F, float Rt, float U) const
		{
			const Vector3 Fw{ std::cos(Yaw * Deg2Rad), std::sin(Yaw * Deg2Rad), 0.0f };
			const Vector3 Ri{ -std::sin(Yaw * Deg2Rad), std::cos(Yaw * Deg2Rad), 0.0f };
			return Add(Feet, Add(Add(Mul(Fw, F * Scale), Mul(Ri, Rt * Scale)), Vector3{ 0, 0, U * Scale }));
		}

		void Part(MeshKind K, float F, float Rt, float U, float SF, float SR, float SU, Color C,
		          float Emissive = 0.0f, float PitchDeg = 0.0f, float YawOff = 0.0f, float RollDeg = 0.0f) const
		{
			const Vector3 P = World(F, Rt, U);
			const Matrix M = MakeTransform(ToRender(P), Vector3{ SF * Scale * 0.01f, SU * Scale * 0.01f, SR * Scale * 0.01f },
			                               -(Yaw + YawOff) * Deg2Rad, PitchDeg * Deg2Rad, RollDeg * Deg2Rad);
			R->Draw(K, M, WithAlpha(C, Alpha), LineWeight, Emissive);
		}

		// Limb hanging from a pivot, swung forward by Angle degrees.
		void Limb(float F, float Rt, float PivotU, float Thick, float Length, float Angle, Color C, float Emissive = 0.0f) const
		{
			const float A = Angle * Deg2Rad;
			const float CF = F + std::sin(A) * Length * 0.5f;
			const float CU = PivotU - std::cos(A) * Length * 0.5f;
			Part(MeshKind::Cube, CF, Rt, CU, Thick, Thick, Length, C, Emissive, Angle);
		}
	};

	// Shared kid-sized humanoid (140 cm). Arm angles swing forward from the
	// shoulder (90 = pointing straight ahead); returns nothing, props are added
	// by the caller at HandF/HandU.
	void Humanoid(const Rig& G, Color Skin, Color Shirt, Color Pants, Color Shoes, float LegSwing,
	              float RightArm, float LeftArm, bool bHead = true, float HeadSize = 36.0f, bool bLegs = true)
	{
		if (bLegs)
		{
			G.Limb(0, 8, 46, 12, 44, LegSwing, Pants);
			G.Limb(0, -8, 46, 12, 44, -LegSwing, Pants);
			G.Part(MeshKind::Cube, 3 + std::sin(LegSwing * Deg2Rad) * 40, 8, 3, 18, 12, 7, Shoes);
			G.Part(MeshKind::Cube, 3 - std::sin(LegSwing * Deg2Rad) * 40, -8, 3, 18, 12, 7, Shoes);
		}
		G.Part(MeshKind::Cube, 0, 0, 64, 20, 30, 38, Shirt);
		G.Limb(0, 20, 80, 9, 36, RightArm, Shirt);
		G.Limb(0, -20, 80, 9, 36, LeftArm, Shirt);
		G.Part(MeshKind::Sphere, std::sin(RightArm * Deg2Rad) * 38, 20, 80 - std::cos(RightArm * Deg2Rad) * 38, 11, 11, 11, Skin);
		G.Part(MeshKind::Sphere, std::sin(LeftArm * Deg2Rad) * 38, -20, 80 - std::cos(LeftArm * Deg2Rad) * 38, 11, 11, 11, Skin);
		if (bHead)
		{
			G.Part(MeshKind::Sphere, 0, 0, 100, HeadSize, HeadSize, HeadSize, Skin);
			G.Part(MeshKind::Sphere, HeadSize * 0.42f, 6, 103, 7, 6, 9, Color{ 250, 250, 255, 255 });
			G.Part(MeshKind::Sphere, HeadSize * 0.42f, -6, 103, 7, 6, 9, Color{ 250, 250, 255, 255 });
			G.Part(MeshKind::Sphere, HeadSize * 0.47f, 6, 103, 4, 4, 5, Color{ 25, 25, 30, 255 });
			G.Part(MeshKind::Sphere, HeadSize * 0.47f, -6, 103, 4, 4, 5, Color{ 25, 25, 30, 255 });
		}
	}

	float HandF(float Angle) { return std::sin(Angle * Deg2Rad) * 38.0f; }
	float HandU(float Angle) { return 80.0f - std::cos(Angle * Deg2Rad) * 38.0f; }
}

SceneLighting Game::Lighting() const
{
	SceneLighting L;
	if (LevelIndex == 0)
	{
		L.Sky = Color{ 25, 15, 55, 255 };
		L.SkyHorizon = Color{ 95, 65, 150, 255 };
		L.Ambient = Color{ 110, 95, 150, 255 };
		L.SunDirection = Vector3{ 0.5f, -0.7f, -0.4f };
		L.FogDensity = 0.0004f;
	}
	else
	{
		L.Sky = Color{ 95, 160, 240, 255 };
		L.SkyHorizon = Color{ 205, 228, 250, 255 };
		L.Ambient = Color{ 120, 130, 155, 255 };
		L.SunDirection = Vector3{ -0.45f, -0.75f, -0.3f };
		L.FogDensity = 0.0011f;
	}
	return L;
}

Camera3D Game::MakeCamera() const
{
	Camera3D Cam{};
	Cam.position = ToRender(CamPos);
	Cam.target = ToRender(Add(CamPos, Mul(CamForward(), 100.0f)));
	Cam.up = Vector3{ 0.0f, 1.0f, 0.0f };
	Cam.fovy = 70.0f;
	Cam.projection = CAMERA_PERSPECTIVE;
	return Cam;
}

void Game::Render(ToonRenderer& R)
{
	const Camera3D Cam = MakeCamera();
	const SceneLighting Light = Lighting();
	R.BeginPass(RenderPass::Color, Cam, Light);
	RenderScene(R, false);
	RenderScene(R, true);
	R.EndPass();
	R.BeginPass(RenderPass::Normals, Cam, Light);
	RenderScene(R, false);
	R.EndPass();
}

void Game::RenderScene(ToonRenderer& R, bool bTranslucent)
{
	const Level& L = Levels[LevelIndex];
	const bool bColor = R.CurrentPass() == RenderPass::Color;

	if (bColor && !bTranslucent && LevelIndex == 0)
	{
		for (size_t i = 0; i < Stars.size(); ++i)
		{
			const float Twinkle = 0.6f + 0.4f * std::sin(Time * 2.0f + static_cast<float>(i));
			R.Draw(MeshKind::Sphere, MakeTransform(ToRender(Add(CamPos, Stars[i])), Vector3{ 3.0f, 3.0f, 3.0f }, 0.0f),
			       Shade(Color{ 255, 250, 230, 255 }, Twinkle), 0.0f, 1.0f);
		}
	}

	RenderBlocks(R, bTranslucent);

	if (!bTranslucent)
	{
		for (size_t i = 0; i < L.Markers.size(); ++i)
		{
			if (L.Markers[i].Class == "DimensionalTerminal")
			{
				RenderTerminal(R, L.Markers[i], static_cast<int>(i));
			}
		}
		for (const HoverCar& Car : Cars)
		{
			RenderCar(R, Car);
		}

		// Rift gremlins.
		for (const Enemy& E : Enemies)
		{
			Rig G{ &R, Sub(E.Pos, Vector3{ 0, 0, 45 }), std::atan2(P.Pos.y - E.Pos.y, P.Pos.x - E.Pos.x) / Deg2Rad, 1.0f, 2.2f, 255 };
			const float Bob = std::sin(E.Phase) * 6.0f;
			const Color Body = E.HitFlash > 0.0f ? Color{ 255, 255, 255, 255 } : Color{ 140, 60, 205, 255 };
			G.Part(MeshKind::Sphere, 0, 0, 48 + Bob, 90, 90, 84, Body);
			for (int k = 0; k < 5; ++k)
			{
				const float A = k * 72.0f + E.Phase * 10.0f;
				G.Part(MeshKind::Cone, std::cos(A * Deg2Rad) * 30, std::sin(A * Deg2Rad) * 30, 90 + Bob, 18, 18, 34, Color{ 70, 30, 110, 255 }, 0.0f, 0.0f, 0.0f, 0.0f);
			}
			G.Part(MeshKind::Sphere, 38, 14, 58 + Bob, 14, 14, 18, Color{ 255, 240, 80, 255 }, 1.0f);
			G.Part(MeshKind::Sphere, 38, -14, 58 + Bob, 14, 14, 18, Color{ 255, 240, 80, 255 }, 1.0f);
		}

		// Rifts.
		for (const Rift& Rf : Rifts)
		{
			if (Rf.Closed) continue;
			const float Spin = Time * (Rf.Wave > 0 ? 90.0f : 30.0f);
			for (int k = 0; k < 12; ++k)
			{
				const float A = (k * 30.0f + Spin) * Deg2Rad;
				const Vector3 Pos = Add(Rf.Pos, Vector3{ std::cos(A) * 260.0f, std::sin(A) * 260.0f, 220.0f + std::sin(A * 2 + Time) * 40.0f });
				R.Draw(MeshKind::Cube, BoxTransform(Pos, Vector3{ 60, 60, 60 }, Spin * 2 + k * 20), k % 2 ? Color{ 160, 70, 230, 255 } : Color{ 90, 255, 180, 255 }, 1.5f, 0.8f);
			}
		}

		// Projectiles, anvils.
		for (const Projectile& Pr : Projectiles)
		{
			const float S = Pr.Radius * 2.0f;
			R.Draw(MeshKind::Sphere, BoxTransform(Pr.Pos, Vector3{ S, S, S }, 0), Pr.Tint, 0.8f, 1.0f);
		}
		for (const FallingAnvil& A : Anvils)
		{
			R.Draw(MeshKind::Cube, BoxTransform(A.Pos, Vector3{ 160, 90, 70 }, 0), Color{ 70, 70, 80, 255 }, 2.8f);
			R.Draw(MeshKind::Cube, BoxTransform(Add(A.Pos, Vector3{ 0, 0, -60 }), Vector3{ 90, 60, 60 }, 0), Color{ 60, 60, 70, 255 }, 2.8f);
		}

		if (bColor)
		{
			for (const Particle& Pa : Particles)
			{
				const float T = std::max(0.0f, Pa.Life / Pa.MaxLife);
				const float S = Pa.Size * (0.4f + 0.6f * T);
				R.Draw(MeshKind::Cube, BoxTransform(Pa.Pos, Vector3{ S, S, S }, Pa.Life * 400.0f, Pa.Life * 300.0f), Pa.Tint, 0.0f, 1.0f);
			}
		}

		// El Tigre's chain.
		if (P.Grappling)
		{
			const Vector3 From = Add(P.Pos, Vector3{ 0, 0, P.H * 0.3f });
			const Vector3 D = Sub(P.GrapplePoint, From);
			const float Length = Len(D);
			const int Links = std::max(2, static_cast<int>(Length / 60.0f));
			for (int k = 0; k <= Links; ++k)
			{
				const Vector3 Pos = Add(From, Mul(D, static_cast<float>(k) / Links));
				R.Draw(MeshKind::Cube, BoxTransform(Pos, Vector3{ 24, 24, 24 }, k * 45.0f, k * 30.0f), Color{ 200, 200, 215, 255 }, 1.0f);
			}
		}
	}
	else if (bColor)
	{
		// Translucent: blob shadows, shockwaves.
		if (P.RespawnTimer <= 0.0f && CarIndex < 0)
		{
			RenderShadow(R, P.Pos, P.R * 1.3f);
		}
		for (const Enemy& E : Enemies)
		{
			RenderShadow(R, E.Pos, 55.0f);
		}
		for (const Shockwave& W : Shockwaves)
		{
			const float T = W.Radius / W.MaxRadius;
			const float D = W.Radius * 2.0f;
			R.Draw(MeshKind::Sphere, BoxTransform(W.Center, Vector3{ D, D, D * 0.5f }, 0), WithAlpha(W.Tint, static_cast<unsigned char>(110 * (1.0f - T))), 0.0f, 1.0f);
		}
		for (const Rift& Rf : Rifts)
		{
			if (Rf.Closed) continue;
			const float Pulse = 1.0f + 0.1f * std::sin(Time * 4.0f);
			R.Draw(MeshKind::Sphere, BoxTransform(Add(Rf.Pos, Vector3{ 0, 0, 220 }), Vector3{ 380 * Pulse, 80, 380 * Pulse }, Time * 20.0f), Color{ 120, 40, 200, 150 }, 0.0f, 0.9f);
		}
	}

	RenderHero(R, bTranslucent);
}

void Game::RenderShadow(ToonRenderer& R, Vector3 Pos, float Radius)
{
	const RayHit Down = Raycast(Levels[LevelIndex], Pos, Vector3{ 0, 0, -1 }, 5000.0f);
	if (!Down.Hit)
	{
		return;
	}
	const float Height = Down.Distance;
	const float Fade = std::clamp(1.0f - Height / 2500.0f, 0.15f, 1.0f);
	const float D = Radius * 2.0f * (0.7f + 0.3f * Fade);
	R.Draw(MeshKind::Cylinder, BoxTransform(Add(Down.Point, Vector3{ 0, 0, 2.0f }), Vector3{ D, D, 2.0f }, 0),
	       Color{ 0, 0, 20, static_cast<unsigned char>(90 * Fade) }, 0.0f, 0.0f);
}

void Game::RenderGate(ToonRenderer& R, const Block& B, Color Tint, float Emissive)
{
	const bool WideX = B.Size.x >= B.Size.y;
	const float Wide = WideX ? B.Size.x : B.Size.y;
	const float Thin = 40.0f;
	const float CosY = std::cos(B.YawDeg * Deg2Rad), SinY = std::sin(B.YawDeg * Deg2Rad);
	auto Local = [&](float Lx, float Ly, float Lz) {
		return Add(B.Center, Vector3{ Lx * CosY - Ly * SinY, Lx * SinY + Ly * CosY, Lz });
	};
	auto Bar = [&](float Along, float Up, float SizeAlong, float SizeUp) {
		const Vector3 C = WideX ? Local(Along, 0, Up) : Local(0, Along, Up);
		const Vector3 S = WideX ? Vector3{ SizeAlong, Thin, SizeUp } : Vector3{ Thin, SizeAlong, SizeUp };
		R.Draw(MeshKind::Cube, BoxTransform(C, S, B.YawDeg), Tint, 1.6f, Emissive);
	};
	Bar(0, B.Half.z - Thin * 0.5f, Wide, Thin);
	Bar(0, -B.Half.z + Thin * 0.5f, Wide, Thin);
	Bar(Wide * 0.5f - Thin * 0.5f, 0, Thin, B.Size.z);
	Bar(-Wide * 0.5f + Thin * 0.5f, 0, Thin, B.Size.z);
}

void Game::RenderBlocks(ToonRenderer& R, bool bTranslucent)
{
	const Level& L = Levels[LevelIndex];
	const Vector3 Fwd = CamForward();

	// Gates of the active course: the next one glows.
	std::vector<int> NextGates, CourseGates;
	for (const Course& C : Courses)
	{
		NextGates.push_back(C.Gates[C.Next]);
		for (int G : C.Gates) CourseGates.push_back(G);
	}

	for (size_t i = 0; i < L.Blocks.size(); ++i)
	{
		const Block& B = L.Blocks[i];
		const BlockStyle S = StyleFor(B, LevelIndex);
		if (S.Hidden || S.Translucent != bTranslucent)
		{
			continue;
		}
		// Cheap culling: behind the camera.
		const Vector3 To = Sub(B.Center, CamPos);
		const float Radius = std::max(B.BoundRadiusXY, B.Half.z);
		if (To.x * Fwd.x + To.y * Fwd.y + To.z * Fwd.z < -Radius - 500.0f)
		{
			continue;
		}

		if (S.Gate)
		{
			const bool bNext = std::find(NextGates.begin(), NextGates.end(), static_cast<int>(i)) != NextGates.end();
			const float Pulse = 0.5f + 0.5f * std::sin(Time * 6.0f);
			RenderGate(R, B, bNext ? Lerp(Color{ 80, 255, 160, 255 }, Color{ 255, 255, 120, 255 }, Pulse) : Color{ 110, 150, 190, 255 }, bNext ? 0.9f : 0.1f);
			continue;
		}

		Vector3 Size = B.Size;
		if (B.Kind == Shape::Plane) Size.z = 4.0f;
		R.Draw(MeshFor(B.Kind), BoxTransform(B.Center, Size, B.YawDeg, B.PitchDeg), S.Tint, EnvLineWeight, S.Emissive);

		if (B.RoofHeight > 0.0f)
		{
			const float RoofH = B.RoofHeight;
			const Vector3 Top{ B.Center.x, B.Center.y, B.Center.z + B.Half.z + RoofH * 0.5f };
			const Color RoofColor = (Hash(B.Label) % 3 == 0) ? Color{ 90, 110, 160, 255 } : (Hash(B.Label) % 3 == 1 ? Color{ 185, 80, 70, 255 } : Color{ 110, 90, 80, 255 });
			R.Draw(MeshKind::Pyramid, BoxTransform(Top, Vector3{ B.Size.x * Block::RoofOverhang, B.Size.y * Block::RoofOverhang, RoofH }, B.YawDeg), RoofColor, EnvLineWeight);
		}
	}
}

void Game::RenderTerminal(ToonRenderer& R, const Marker& M, int Index)
{
	const Color U = UniverseColorFor(M.Label);
	const float Glow = TerminalMenu == Index ? 1.0f : 0.6f + 0.2f * std::sin(Time * 3.0f + Index);
	const Vector3 Base = M.Location;
	R.Draw(MeshKind::Cube, BoxTransform(Add(Base, Vector3{ 0, 0, 55 }), Vector3{ 70, 110, 110 }, M.YawDeg), Color{ 70, 70, 95, 255 }, EnvLineWeight);
	const Vector3 Fw{ std::cos(M.YawDeg * Deg2Rad), std::sin(M.YawDeg * Deg2Rad), 0 };
	R.Draw(MeshKind::Cube, BoxTransform(Add(Add(Base, Mul(Fw, 10.0f)), Vector3{ 0, 0, 150 }), Vector3{ 20, 120, 80 }, M.YawDeg, -20.0f), U, EnvLineWeight, Glow);
	// Hologram of the universe above the pad.
	R.Draw(MeshKind::Sphere, BoxTransform(Add(Base, Vector3{ 0, 0, 260 + std::sin(Time * 2 + Index) * 15 }), Vector3{ 70, 70, 70 }, Time * 40.0f), U, 1.0f, 1.0f);
}


void Game::RenderCar(ToonRenderer& R, const HoverCar& Car)
{
	// Jimmy's retro hover convertible (see Prototype/assets/concept).
	Rig G{ &R, Sub(Car.Pos, Vector3{ 0, 0, 30 }), Car.Yaw, 1.0f, 1.6f, 255 };
	const float Pr = Car.VisPitch * Deg2Rad, Rr = Car.VisRoll * Deg2Rad;
	auto Part = [&](MeshKind K, float F, float Rt, float U, float SF, float SR, float SU, Color C, float Em = 0.0f, float ExtraPitch = 0.0f) {
		// Rotate the offset by the visual pitch/roll so the body tilts as one piece.
		const float U1 = U * std::cos(Pr) + F * std::sin(Pr);
		const float F1 = F * std::cos(Pr) - U * std::sin(Pr);
		const float U2 = U1 * std::cos(Rr) - Rt * std::sin(Rr);
		const float R2 = Rt * std::cos(Rr) + U1 * std::sin(Rr);
		G.Part(K, F1, R2, U2, SF, SR, SU, C, Em, Car.VisPitch + ExtraPitch, 0.0f, -Car.VisRoll);
	};
	const Color Red{ 225, 50, 40, 255 }, Chrome{ 225, 228, 235, 255 }, Seat{ 60, 95, 205, 255 };
	Part(MeshKind::Cube, 0, 0, 25, 430, 190, 44, Red);                       // lower body
	Part(MeshKind::Cube, 120, 0, 52, 170, 176, 14, Red);                     // hood
	Part(MeshKind::Cube, -150, 0, 54, 120, 176, 18, Red);                    // trunk
	Part(MeshKind::Cube, 40, 0, 50, 6, 190, 4, Color{ 250, 250, 250, 255 }); // white stripe
	Part(MeshKind::Cube, 30, 0, 78, 6, 160, 40, Color{ 160, 225, 255, 255 }, 0.3f, 25.0f); // windshield
	Part(MeshKind::Cube, -10, 42, 58, 50, 60, 22, Seat);
	Part(MeshKind::Cube, -10, -42, 58, 50, 60, 22, Seat);
	Part(MeshKind::Cube, -36, 42, 82, 12, 60, 40, Seat);
	Part(MeshKind::Cube, -36, -42, 82, 12, 60, 40, Seat);
	Part(MeshKind::Cube, 218, 0, 22, 12, 200, 16, Chrome);                   // bumpers
	Part(MeshKind::Cube, -218, 0, 22, 12, 200, 16, Chrome);
	Part(MeshKind::Sphere, 212, 62, 40, 18, 34, 34, Color{ 255, 250, 210, 255 }, 0.9f); // headlights
	Part(MeshKind::Sphere, 212, -62, 40, 18, 34, 34, Color{ 255, 250, 210, 255 }, 0.9f);
	Part(MeshKind::Cone, -190, 80, 80, 50, 14, 60, Red, 0.0f, 35.0f);        // tail fins
	Part(MeshKind::Cone, -190, -80, 80, 50, 14, 60, Red, 0.0f, 35.0f);
	const float Thrust = 0.5f + 0.5f * std::sin(Time * 30.0f);
	Part(MeshKind::Cylinder, -228, 50, 30, 40, 40, 30, Color{ 90, 90, 110, 255 }, 0.0f, 90.0f);
	Part(MeshKind::Cylinder, -228, -50, 30, 40, 40, 30, Color{ 90, 90, 110, 255 }, 0.0f, 90.0f);
	Part(MeshKind::Sphere, -248, 50, 30, 26, 30, 30, Color{ 120, 230, 255, 255 }, 0.7f + 0.3f * Thrust);
	Part(MeshKind::Sphere, -248, -50, 30, 26, 30, 30, Color{ 120, 230, 255, 255 }, 0.7f + 0.3f * Thrust);
	const float Pads[4][2] = { { 180, 90 }, { 180, -90 }, { -180, 90 }, { -180, -90 } };
	for (const auto& Pd : Pads)
	{
		Part(MeshKind::Cylinder, Pd[0], Pd[1], -2, 60, 60, 10, Color{ 120, 230, 255, 255 }, 0.9f);
	}
	if (Car.Occupied)
	{
		const HeroDef& H = GetHeroDef(P.Hero);
		Part(MeshKind::Cube, -10, 42, 92, 20, 28, 30, H.Primary);
		Part(MeshKind::Sphere, -10, 42, 122, 34, 34, 34, H.Skin);
		if (P.Hero == HeroId::Jimmy)
		{
			Part(MeshKind::Sphere, -12, 42, 142, 34, 34, 22, H.Hair);
			Part(MeshKind::Cone, -14, 42, 160, 24, 24, 30, H.Hair);
		}
		// Goddard rides shotgun.
		Part(MeshKind::Cube, -10, -42, 88, 34, 22, 24, Color{ 200, 205, 215, 255 });
		Part(MeshKind::Cube, 4, -42, 108, 22, 20, 18, Color{ 200, 205, 215, 255 });
	}
}

void Game::RenderGoddard(ToonRenderer& R)
{
	if (P.Hero != HeroId::Jimmy || CarIndex >= 0 || P.RespawnTimer > 0.0f)
	{
		return;
	}
	Rig G{ &R, GoddardPos, GoddardYaw, 1.0f, 1.2f, 255 };
	const Color Silver{ 200, 205, 218, 255 }, Ears{ 55, 75, 150, 255 }, Dark{ 70, 75, 90, 255 };
	const float Trot = std::sin(Time * 14.0f) * (GoddardSpeed > 60.0f ? 22.0f : 0.0f);
	const float Wag = std::sin(Time * 9.0f) * 25.0f;
	G.Limb(15, 10, 24, 8, 22, Trot, Dark);
	G.Limb(15, -10, 24, 8, 22, -Trot, Dark);
	G.Limb(-15, 10, 24, 8, 22, -Trot, Dark);
	G.Limb(-15, -10, 24, 8, 22, Trot, Dark);
	G.Part(MeshKind::Cube, 0, 0, 32, 48, 28, 24, Silver);
	G.Part(MeshKind::Cube, 30, 0, 52, 26, 24, 22, Silver);
	G.Part(MeshKind::Cube, 43.5f, 0, 52, 2, 18, 13, Color{ 30, 45, 70, 255 }, 0.4f);   // screen face
	G.Part(MeshKind::Sphere, 44.5f, 5, 54, 3, 4, 4, Color{ 120, 230, 255, 255 }, 1.0f);
	G.Part(MeshKind::Sphere, 44.5f, -5, 54, 3, 4, 4, Color{ 120, 230, 255, 255 }, 1.0f);
	G.Part(MeshKind::Cube, 28, 15, 56, 14, 4, 24, Ears, 0.0f, 0.0f, 0.0f, 25.0f + Wag * 0.2f);
	G.Part(MeshKind::Cube, 28, -15, 56, 14, 4, 24, Ears, 0.0f, 0.0f, 0.0f, -25.0f - Wag * 0.2f);
	G.Part(MeshKind::Cylinder, -28, 0, 44, 4, 4, 22, Dark, 0.0f, 55.0f, Wag);           // antenna tail
	G.Part(MeshKind::Sphere, -36, 0, 52, 6, 6, 6, Color{ 255, 80, 80, 255 }, 0.8f);
}

bool Game::RenderHeroSprite(ToonRenderer& R, unsigned char Alpha, bool bHurt)
{
	const FormDef* Form = P.Hero == HeroId::Ben ? GetFormDef(P.Omni.GetActiveForm()) : nullptr;
	int Id = static_cast<int>(P.Hero);
	float BaseH = 70.0f;
	if (Form)
	{
		const std::string Name = Form->Name;
		if (Name == "Heatblast") Id = SprHeatblast;
		else if (Name == "Four Arms") Id = SprFourArms;
		else return false; // XLR8, Cannonbolt and Way Big aren't on the sheet: 3D models
		BaseH = Form->HalfHeight;
	}
	else if (P.Hero == HeroId::Jake && P.Flying)
	{
		Id = SprDragon;
	}
	const SpriteArt& S = Sprites[Id];
	if (S.Tex.id == 0)
	{
		return false;
	}

	const HeroDef& H = GetHeroDef(P.Hero);
	const float Speed = std::sqrt(P.Vel.x * P.Vel.x + P.Vel.y * P.Vel.y);
	const float Scale = P.H / BaseH;
	float SX = 1.0f, SY = 1.0f + 0.015f * std::sin(Time * 3.0f); // idle breathing
	float Bob = 0.0f, Roll = 0.0f;
	if (P.Grounded && Speed > 50.0f)
	{
		const float Amt = std::min(1.0f, Speed / 650.0f);
		Bob = std::fabs(std::sin(P.WalkPhase)) * 8.0f * Amt * Scale;
		Roll = std::sin(P.WalkPhase) * 0.07f * Amt;
	}
	const float ScreenRight = P.Vel.x * -std::sin(CamYaw * Deg2Rad) + P.Vel.y * std::cos(CamYaw * Deg2Rad);
	if (!P.Grounded)
	{
		Roll = std::clamp(-ScreenRight / 2500.0f, -0.3f, 0.3f); // lean into the turn
		if (!P.Flying) { SY *= P.Vel.z > 200.0f ? 1.06f : 1.0f; SX *= P.Vel.z > 200.0f ? 0.96f : 1.0f; }
		if (P.Flying || P.Gliding) Bob = std::sin(Time * 2.5f) * 5.0f;
	}
	if (P.LandTimer > 0.0f)
	{
		const float T = P.LandTimer / 0.18f;
		SY *= 1.0f - 0.14f * T;
		SX *= 1.0f + 0.12f * T;
	}
	if (P.AttackAnim > 0.0f)
	{
		SX *= 1.06f;
		SY *= 1.04f;
		Roll += (P.SpriteFlip ? 0.08f : -0.08f);
	}

	const float Height = S.HeightCm * Scale * SY;
	const float Width = S.HeightCm * Scale * SX * static_cast<float>(S.Tex.width) / static_cast<float>(S.Tex.height);
	const Vector3 Right{ -std::sin(CamYaw * Deg2Rad), 0.0f, std::cos(CamYaw * Deg2Rad) };
	const Vector3 ToCam{ -std::cos(CamYaw * Deg2Rad), 0.0f, -std::sin(CamYaw * Deg2Rad) };
	const Vector3 Feet = Sub(P.Pos, Vector3{ 0, 0, P.H - Bob });
	R.DrawSprite(S.Tex, ToRender(Feet), Right, ToCam, Width * 0.01f, Height * 0.01f, Roll, P.SpriteFlip,
	             Color{ 255, 255, 255, Alpha }, bHurt ? 0.75f : 0.0f, H.LineWeight);

	// Cosmo & Wanda: hovering at Timmy's shoulder, or his cloud-board in flight.
	const SpriteArt& F = Sprites[SprFairies];
	if (P.Hero == HeroId::Timmy && F.Tex.id != 0)
	{
		const float FH = F.HeightCm * (P.Flying ? 1.4f : 1.0f);
		const float FW = FH * static_cast<float>(F.Tex.width) / static_cast<float>(F.Tex.height);
		const Vector3 RightUe{ -std::sin(CamYaw * Deg2Rad), std::cos(CamYaw * Deg2Rad), 0.0f };
		const Vector3 BackUe{ std::cos(CamYaw * Deg2Rad), std::sin(CamYaw * Deg2Rad), 0.0f };
		Vector3 FPos = P.Flying ? Sub(Feet, Vector3{ 0, 0, FH * 0.55f })
		                        : Add(Add(Feet, Mul(RightUe, Width * 0.5f + 25.0f)), Vector3{ 0, 0, Height * 0.62f + std::sin(Time * 3.0f) * 8.0f });
		FPos = Add(FPos, Mul(BackUe, P.Flying ? 0.0f : 10.0f));
		R.DrawSprite(F.Tex, ToRender(FPos), Right, ToCam, FW * 0.01f, FH * 0.01f, std::sin(Time * 2.0f) * 0.06f, false,
		             Color{ 255, 255, 255, Alpha }, 0.0f, H.LineWeight);
	}
	return true;
}

void Game::RenderHero(ToonRenderer& R, bool bTranslucent)
{
	if (!bTranslucent)
	{
		RenderGoddard(R);
	}
	if (P.RespawnTimer > 0.0f || CarIndex >= 0)
	{
		return;
	}
	const bool bGhost = P.Intangible;
	if (bGhost != bTranslucent || (bTranslucent && R.CurrentPass() != RenderPass::Color))
	{
		return;
	}
	const HeroDef& H = GetHeroDef(P.Hero);
	const FormDef* Form = P.Hero == HeroId::Ben ? GetFormDef(P.Omni.GetActiveForm()) : nullptr;
	const Vector3 Feet = Sub(P.Pos, Vector3{ 0, 0, P.H });
	const float Speed = std::sqrt(P.Vel.x * P.Vel.x + P.Vel.y * P.Vel.y);
	const bool bAttack = P.AttackAnim > 0.0f || P.BreathTime > 0.0f;
	float Leg = (P.Grounded ? std::min(1.0f, Speed / 600.0f) : 0.25f) * std::sin(P.WalkPhase) * 35.0f;
	float RA = bAttack ? 85.0f : -Leg * 0.8f;
	float LA = Leg * 0.8f;
	if (P.Flying)
	{
		Leg = -12.0f;
		RA = bAttack ? 85.0f : -25.0f;
		LA = -25.0f;
	}
	const unsigned char Alpha = bGhost ? 110 : 255;
	const bool bHurt = P.HurtFlash > 0.0f && std::fmod(Time, 0.1f) < 0.05f;
	auto Tint = [&](Color C) { return bHurt ? Color{ 255, 255, 255, C.a } : C; };
	// Point along an arm, Dist cm past the hand.
	auto AlongF = [](float A, float Dist) { return HandF(A) + std::sin(A * Deg2Rad) * Dist; };
	auto AlongU = [](float A, float Dist) { return HandU(A) - std::cos(A * Deg2Rad) * Dist; };

	if (P.Hero == HeroId::Ben && P.Omni.GetState() == Omnitrix::EState::Transforming)
	{
		const float D = P.H * 2.4f;
		R.Draw(MeshKind::Sphere, MakeTransform(ToRender(P.Pos), Vector3{ D * 0.01f, D * 0.012f, D * 0.01f }, 0.0f), Color{ 90, 255, 90, 255 }, 1.0f, 1.0f);
		return;
	}

	if (bSpriteArt && RenderHeroSprite(R, Alpha, bHurt))
	{
		return;
	}

	if (Form)
	{
		Rig G{ &R, Feet, P.Yaw, P.H / Form->HalfHeight, 2.0f, Alpha };
		const std::string Name = Form->Name;
		if (Name == "Heatblast")
		{
			G.Scale = P.H / 70.0f * 0.95f;
			const Color Magma = Tint(Form->Body), Rock = Shade(Form->Body, 0.6f);
			Humanoid(G, Magma, Magma, Rock, Shade(Form->Body, 0.45f), Leg, RA, LA, false);
			for (int k = 0; k < 3; ++k)
			{
				G.Part(MeshKind::Cube, 10.5f, -6 + k * 6, 58 + k * 8, 2, 3, 14, Color{ 255, 210, 60, 255 }, 1.0f, 0.0f, 0.0f, 30.0f - k * 25.0f);
			}
			const float Flick = std::sin(Time * 20.0f) * 4.0f;
			G.Part(MeshKind::Sphere, 0, 0, 100, 34, 34, 34, Magma, 0.3f);
			G.Part(MeshKind::Cone, -2, 0, 128 + Flick, 36, 36, 52, Color{ 255, 160, 30, 255 }, 1.0f);
			G.Part(MeshKind::Cone, -6, 0, 152 + Flick, 20, 20, 36, Color{ 255, 240, 120, 255 }, 1.0f);
			G.Part(MeshKind::Sphere, 15, 6, 102, 7, 7, 8, Color{ 255, 255, 180, 255 }, 1.0f);
			G.Part(MeshKind::Sphere, 15, -6, 102, 7, 7, 8, Color{ 255, 255, 180, 255 }, 1.0f);
			if (bAttack)
			{
				G.Part(MeshKind::Sphere, HandF(RA), 20, HandU(RA), 18, 18, 18, Color{ 255, 190, 40, 255 }, 1.0f);
			}
		}
		else if (Name == "Four Arms")
		{
			// 240 cm red brute: black top with a white stripe, gold belt, two pairs of arms.
			G.Scale = P.H / 120.0f;
			const Color Skin = Tint(Form->Body), Black{ 35, 35, 40, 255 };
			G.Limb(0, 16, 80, 22, 80, Leg, Black);
			G.Limb(0, -16, 80, 22, 80, -Leg, Black);
			G.Part(MeshKind::Cube, 0, 0, 125, 44, 70, 70, Black);
			G.Part(MeshKind::Cube, 0, 0, 125, 45, 18, 70, Color{ 240, 240, 240, 255 });
			G.Part(MeshKind::Cube, 0, 0, 92, 46, 72, 8, Color{ 230, 190, 60, 255 });
			const float Punch = bAttack ? 80.0f : 0.0f;
			G.Limb(0, 44, 155, 20, 70, -Leg + Punch, Skin);
			G.Limb(0, -44, 155, 20, 70, Leg + Punch, Skin);
			G.Limb(0, 40, 128, 18, 62, Leg * 0.5f + Punch * 0.7f, Skin);
			G.Limb(0, -40, 128, 18, 62, -Leg * 0.5f + Punch * 0.7f, Skin);
			G.Part(MeshKind::Sphere, 4, 0, 180, 40, 40, 38, Skin);
			for (int e = 0; e < 4; ++e)
			{
				G.Part(MeshKind::Sphere, 21, e < 2 ? 8 : -8, e % 2 ? 176 : 186, 6, 6, 6, Color{ 255, 220, 60, 255 }, 0.8f);
			}
		}
		else if (Name == "XLR8")
		{
			G.Scale = P.H / 75.0f;
			const float Lean = std::min(1.0f, Speed / 1500.0f) * 25.0f;
			G.Limb(0, 9, 50, 12, 48, Leg * 1.4f, Form->Accent);
			G.Limb(0, -9, 50, 12, 48, -Leg * 1.4f, Form->Accent);
			G.Part(MeshKind::Cube, 4, 0, 70, 22, 28, 40, Tint(Form->Body), 0.0f, -Lean);
			G.Limb(4, 20, 86, 9, 36, RA, Form->Body);
			G.Limb(4, -20, 86, 9, 36, LA, Form->Body);
			G.Part(MeshKind::Sphere, 14, 0, 108, 36, 30, 32, Form->Body);
			G.Part(MeshKind::Cube, 30, 0, 110, 22, 24, 14, Color{ 20, 20, 25, 255 }, 0.0f, 10.0f);
			G.Part(MeshKind::Cone, -32, 0, 56, 14, 14, 60, Form->Body, 0.0f, 120.0f);
			G.Part(MeshKind::Sphere, 24, 0, 112, 8, 20, 6, Color{ 80, 200, 255, 255 }, 1.0f);
		}
		else if (Name == "Cannonbolt")
		{
			const float Roll = P.WalkPhase * 40.0f;
			G.Scale = 1.0f;
			G.Part(MeshKind::Sphere, 0, 0, P.H, P.H * 2.0f, P.H * 2.0f, P.H * 2.0f, Form->Body, 0.0f, Roll);
			for (int k = 0; k < 4; ++k)
			{
				G.Part(MeshKind::Cube, 0, 0, P.H, P.H * 2.04f, 14, P.H * 0.5f, Form->Accent, 0.0f, Roll + k * 45.0f);
			}
		}
		else // Way Big
		{
			G.Scale = P.H / 70.0f;
			Humanoid(G, Form->Body, Form->Body, Form->Accent, Form->Accent, Leg * 0.6f, RA, LA, false);
			G.Part(MeshKind::Cube, 0.5f, 0, 64, 21, 10, 40, Form->Accent);
			G.Part(MeshKind::Sphere, 0, 0, 100, 34, 30, 36, Form->Body);
			G.Part(MeshKind::Cone, -2, 0, 124, 8, 30, 30, Form->Accent);
			G.Part(MeshKind::Sphere, 15, 6, 102, 6, 7, 5, Color{ 255, 240, 120, 255 }, 1.0f);
			G.Part(MeshKind::Sphere, 15, -6, 102, 6, 7, 5, Color{ 255, 240, 120, 255 }, 1.0f);
		}
		return;
	}

	Rig G{ &R, Feet, P.Yaw, P.H / 70.0f, H.LineWeight, Alpha };
	const Color Skin = Tint(H.Skin);
	const Color Shirt = Tint(H.Primary);
	const Color Pants = Tint(H.Secondary);
	const Color White{ 245, 245, 250, 255 };

	switch (P.Hero)
	{
	case HeroId::Jimmy:
	{
		Humanoid(G, Skin, Shirt, Pants, White, Leg, RA, LA, true, 46.0f);
		// The famous swirl.
		G.Part(MeshKind::Sphere, -2, 0, 124, 42, 42, 28, H.Hair);
		G.Part(MeshKind::Cone, -1, 0, 148, 32, 32, 42, H.Hair, 0.0f, -10.0f);
		G.Part(MeshKind::Cone, 6, 0, 170, 15, 15, 20, H.Hair, 0.0f, -45.0f);
		// Atom logo.
		G.Part(MeshKind::Sphere, 10.5f, 0, 70, 3, 6, 6, Color{ 255, 220, 60, 255 }, 0.6f);
		G.Part(MeshKind::Cube, 10.5f, 0, 70, 2, 18, 3, Color{ 255, 220, 60, 255 }, 0.4f, 0.0f, 0.0f, 50.0f);
		G.Part(MeshKind::Cube, 10.5f, 0, 70, 2, 18, 3, Color{ 255, 220, 60, 255 }, 0.4f, 0.0f, 0.0f, -50.0f);
		// Goddard jetpack: twin rockets.
		for (int s = -1; s <= 1; s += 2)
		{
			G.Part(MeshKind::Cylinder, -16, s * 8.0f, 66, 10, 10, 30, Color{ 210, 215, 225, 255 });
			G.Part(MeshKind::Cone, -16, s * 8.0f, 86, 10, 10, 12, Color{ 225, 50, 40, 255 });
			if (P.SigCd > 0.2f)
			{
				G.Part(MeshKind::Cone, -16, s * 8.0f, 40, 10, 10, 26 + std::sin(Time * 40.0f) * 6.0f, Color{ 255, 170, 40, 255 }, 1.0f, 180.0f);
			}
		}
		// Shrink ray.
		G.Part(MeshKind::Cube, AlongF(RA, 4), 20, AlongU(RA, 4), 12, 7, 16, Color{ 120, 175, 140, 255 }, 0.0f, RA - 180.0f);
		G.Part(MeshKind::Cylinder, AlongF(RA, 14), 20, AlongU(RA, 14), 6, 6, 14, Color{ 200, 200, 210, 255 }, 0.0f, RA - 180.0f);
		G.Part(MeshKind::Sphere, AlongF(RA, 22), 20, AlongU(RA, 22), 8, 8, 8, Color{ 255, 70, 70, 255 }, 0.9f);
		break;
	}
	case HeroId::Timmy:
	{
		Humanoid(G, Skin, Shirt, Pants, White, Leg, RA, LA, true, 40.0f);
		G.Part(MeshKind::Cylinder, 0, 0, 122, 40, 40, 14, Shirt);
		G.Part(MeshKind::Sphere, -2, 0, 128, 36, 36, 18, Shirt);
		G.Part(MeshKind::Cube, 17, 0, 117, 20, 32, 3, Shirt);
		G.Part(MeshKind::Sphere, -6, 17, 110, 14, 10, 12, H.Hair);
		G.Part(MeshKind::Sphere, -6, -17, 110, 14, 10, 12, H.Hair);
		G.Part(MeshKind::Cube, 19, 2.5f, 94, 3, 4, 6, White);
		G.Part(MeshKind::Cube, 19, -2.5f, 94, 3, 4, 6, White);
		// Magic wand + Da Rules book.
		G.Part(MeshKind::Cylinder, AlongF(RA, 12), 20, AlongU(RA, 12), 3, 3, 26, Color{ 30, 30, 35, 255 }, 0.0f, RA - 180.0f);
		G.Part(MeshKind::Sphere, AlongF(RA, 26), 20, AlongU(RA, 26), 12, 12, 12, Color{ 255, 230, 70, 255 }, bAttack ? 1.0f : 0.7f);
		G.Part(MeshKind::Cube, HandF(LA) + 4, -24, HandU(LA) + 4, 8, 22, 28, Color{ 135, 60, 165, 255 });
		G.Part(MeshKind::Cube, HandF(LA) + 8.5f, -24, HandU(LA) + 8, 1, 14, 6, Color{ 240, 200, 80, 255 }, 0.3f);
		// Cosmo & Wanda: orbiting, or the cloud-board while flying.
		if (P.Flying)
		{
			G.Part(MeshKind::Sphere, 0, 0, -10, 120, 76, 30, Color{ 250, 250, 255, 255 }, 0.2f);
			G.Part(MeshKind::Sphere, 34, 22, -6, 56, 50, 30, Color{ 200, 255, 210, 255 }, 0.2f);
			G.Part(MeshKind::Sphere, 34, -22, -6, 56, 50, 30, Color{ 255, 205, 230, 255 }, 0.2f);
		}
		else
		{
			for (int k = 0; k < 2; ++k)
			{
				const float A = Time * 2.0f + k * 3.14159f;
				const float Fx = std::cos(A) * 60.0f, Fy = std::sin(A) * 60.0f, Fz = 128.0f + std::sin(Time * 4.0f + k) * 8.0f;
				const Color HairC = k == 0 ? Color{ 90, 210, 100, 255 } : Color{ 245, 120, 185, 255 };
				const Color Body = k == 0 ? White : Color{ 250, 220, 80, 255 };
				const float Flap = std::sin(Time * 30.0f) * 30.0f;
				G.Part(MeshKind::Cylinder, Fx, Fy, Fz - 16, 10, 10, 16, Body);
				G.Part(MeshKind::Sphere, Fx, Fy, Fz, 18, 18, 18, Color{ 250, 215, 185, 255 });
				G.Part(MeshKind::Sphere, Fx - 2, Fy, Fz + 12, k == 0 ? 16.0f : 22.0f, k == 0 ? 16.0f : 22.0f, k == 0 ? 18.0f : 16.0f, HairC);
				G.Part(MeshKind::Cone, Fx, Fy, Fz + 24, 8, 8, 10, Color{ 255, 220, 60, 255 }, 0.6f);
				G.Part(MeshKind::Cube, Fx - 8, Fy + 8, Fz - 6, 2, 14, 10, Color{ 220, 240, 255, 255 }, 0.4f, 0.0f, 0.0f, 20.0f + Flap);
				G.Part(MeshKind::Cube, Fx - 8, Fy - 8, Fz - 6, 2, 14, 10, Color{ 220, 240, 255, 255 }, 0.4f, 0.0f, 0.0f, -20.0f - Flap);
			}
		}
		break;
	}
	case HeroId::Danny:
	{
		const Color Suit = Shirt;
		Humanoid(G, Skin, Suit, Suit, White, Leg, RA, LA, true, 38.0f, !P.Flying);
		G.Part(MeshKind::Cube, 0, 0, 83, 21, 24, 4, White);   // collar
		G.Part(MeshKind::Cube, 0, 0, 48, 21, 31, 6, White);   // belt
		G.Part(MeshKind::Cube, 10.5f, 0, 70, 2, 14, 12, White); // DP emblem
		G.Part(MeshKind::Cube, 11.0f, 2, 70, 2, 4, 8, Suit);
		G.Part(MeshKind::Sphere, -3, 0, 112, 40, 40, 24, H.Hair);
		G.Part(MeshKind::Cone, -14, 0, 112, 14, 30, 24, H.Hair, 0.0f, 70.0f);
		G.Part(MeshKind::Sphere, 16.5f, 6, 103, 7, 6, 8, Color{ 90, 255, 130, 255 }, 1.0f);
		G.Part(MeshKind::Sphere, 16.5f, -6, 103, 7, 6, 8, Color{ 90, 255, 130, 255 }, 1.0f);
		// Glowing ecto-energy hands.
		G.Part(MeshKind::Sphere, HandF(RA), 20, HandU(RA), 14, 14, 14, Color{ 110, 255, 140, 255 }, 1.0f);
		G.Part(MeshKind::Sphere, HandF(LA), -20, HandU(LA), 14, 14, 14, Color{ 110, 255, 140, 255 }, 1.0f);
		if (P.Flying)
		{
			// Ghost tail replaces the legs in flight.
			const float Sway = std::sin(Time * 5.0f) * 12.0f;
			G.Part(MeshKind::Cone, -4, 0, 24, 30, 26, 54, Color{ 30, 30, 36, 255 }, 0.0f, 180.0f + Sway);
			G.Part(MeshKind::Cone, -14 - Sway * 0.5f, 0, -8, 18, 16, 32, Color{ 120, 255, 200, 255 }, 0.7f, 200.0f + Sway * 1.5f);
		}
		break;
	}
	case HeroId::Jake:
		if (P.Flying)
		{
			// American Dragon form: red with yellow belly and big wings.
			Rig D = G;
			D.Scale = P.H / 70.0f * 1.5f;
			const Color Red = Tint(Color{ 205, 35, 30, 255 }), Belly{ 245, 200, 70, 255 }, Wing{ 150, 25, 25, 255 };
			const float Flap = std::sin(Time * 8.0f) * 35.0f;
			D.Part(MeshKind::Cube, 0, 0, 50, 70, 34, 34, Red);
			D.Part(MeshKind::Cube, 2, 0, 42, 60, 26, 14, Belly);
			D.Part(MeshKind::Cube, 42, 0, 64, 30, 22, 30, Red, 0.0f, -20.0f);
			D.Part(MeshKind::Cube, 60, 0, 74, 30, 24, 22, Red);
			D.Part(MeshKind::Cube, 76, 0, 70, 18, 18, 12, Red);
			D.Part(MeshKind::Cone, 54, 9, 92, 7, 7, 20, Color{ 250, 225, 150, 255 }, 0.0f, 30.0f);
			D.Part(MeshKind::Cone, 54, -9, 92, 7, 7, 20, Color{ 250, 225, 150, 255 }, 0.0f, 30.0f);
			D.Part(MeshKind::Sphere, 70, 9, 80, 6, 6, 6, Color{ 255, 230, 80, 255 }, 1.0f);
			D.Part(MeshKind::Sphere, 70, -9, 80, 6, 6, 6, Color{ 255, 230, 80, 255 }, 1.0f);
			for (int s = -1; s <= 1; s += 2)
			{
				const float F = s * Flap;
				D.Part(MeshKind::Cube, 4, s * 42.0f, 62 + std::sin(F * Deg2Rad) * 20, 50, 60, 4, Wing, 0.0f, 0.0f, 0.0f, -F);
				D.Part(MeshKind::Cube, -6, s * 92.0f, 62 + std::sin(F * Deg2Rad) * 60, 40, 50, 3, Wing, 0.0f, 0.0f, 0.0f, -F * 1.3f);
				D.Limb(10, s * 12.0f, 38, 9, 22, -40, Red);
			}
			for (int k = 0; k < 3; ++k)
			{
				D.Part(MeshKind::Cone, -48 - k * 26, 0, 46 - k * 4, 22 - k * 6, 22 - k * 6, 34, Red, 0.0f, 100.0f + std::sin(Time * 4 + k) * 10.0f);
			}
		}
		else
		{
			Humanoid(G, Skin, Shirt, Pants, White, Leg, RA, LA);
			G.Part(MeshKind::Cube, 10.5f, 0, 64, 2, 10, 36, Color{ 120, 120, 130, 255 }); // shirt under the jacket
			G.Part(MeshKind::Sphere, -3, 0, 114, 38, 38, 22, H.Hair);
			for (int k = 0; k < 5; ++k)
			{
				G.Part(MeshKind::Cone, -8 + k * 2, -12 + k * 6, 124, 12, 12, 22, k % 2 ? Color{ 80, 220, 120, 255 } : H.Hair, 0.0f, -10.0f - k * 4.0f);
			}
		}
		break;
	case HeroId::XJ9:
	{
		const Color Body = Shirt, Teal{ 70, 190, 205, 255 }, Joint{ 60, 70, 95, 255 };
		G.Limb(0, 8, 42, 10, 40, Leg, Body);
		G.Limb(0, -8, 42, 10, 40, -Leg, Body);
		G.Part(MeshKind::Sphere, std::sin(Leg * Deg2Rad) * 20, 8, 22, 11, 11, 11, Joint);
		G.Part(MeshKind::Sphere, -std::sin(Leg * Deg2Rad) * 20, -8, 22, 11, 11, 11, Joint);
		G.Part(MeshKind::Cone, 0, 0, 42, 44, 44, 26, Teal, 0.0f, 180.0f);
		G.Part(MeshKind::Cube, 0, 0, 64, 20, 26, 34, Body);
		G.Part(MeshKind::Cube, 0, 0, 74, 21, 27, 6, Teal);
		G.Part(MeshKind::Sphere, 10.5f, 0, 64, 3, 7, 7, Teal, 0.5f);
		G.Limb(0, 19, 78, 9, 36, RA, Body);
		G.Limb(0, -19, 78, 9, 36, LA, Body);
		// Forearm missile launchers.
		G.Part(MeshKind::Cylinder, AlongF(RA, -10), 25, AlongU(RA, -10), 8, 8, 18, Color{ 225, 60, 50, 255 }, 0.0f, RA - 180.0f);
		G.Part(MeshKind::Cylinder, AlongF(LA, -10), -25, AlongU(LA, -10), 8, 8, 18, Color{ 225, 60, 50, 255 }, 0.0f, LA - 180.0f);
		G.Part(MeshKind::Sphere, 0, 0, 100, 38, 36, 38, Body);
		G.Part(MeshKind::Cube, -2, 0, 119, 32, 36, 8, Teal);
		G.Part(MeshKind::Cube, -2, 22, 120, 12, 34, 7, Teal, 0.0f, 0.0f, 0.0f, 35.0f);
		G.Part(MeshKind::Cube, -2, -22, 120, 12, 34, 7, Teal, 0.0f, 0.0f, 0.0f, -35.0f);
		G.Part(MeshKind::Sphere, 15, 7, 102, 9, 9, 11, Color{ 245, 248, 255, 255 });
		G.Part(MeshKind::Sphere, 15, -7, 102, 9, 9, 11, Color{ 245, 248, 255, 255 });
		G.Part(MeshKind::Sphere, 18, 7, 102, 5, 5, 6, Color{ 50, 130, 220, 255 }, 0.6f);
		G.Part(MeshKind::Sphere, 18, -7, 102, 5, 5, 6, Color{ 50, 130, 220, 255 }, 0.6f);
		if (P.Flying)
		{
			const float Fl = 26.0f + std::sin(Time * 40.0f) * 6.0f;
			G.Part(MeshKind::Cone, 0, 8, -12, 16, 16, Fl, Color{ 255, 190, 60, 255 }, 1.0f, 180.0f);
			G.Part(MeshKind::Cone, 0, -8, -12, 16, 16, Fl, Color{ 255, 190, 60, 255 }, 1.0f, 180.0f);
		}
		break;
	}
	case HeroId::ElTigre:
	{
		const Color Suit{ 55, 40, 38, 255 }, Fur = Tint(Color{ 125, 78, 42, 255 }), Muzzle{ 235, 205, 155, 255 };
		Humanoid(G, Fur, Tint(Suit), Tint(Color{ 85, 52, 32, 255 }), Color{ 30, 25, 25, 255 }, Leg, RA, LA, true, 38.0f);
		G.Part(MeshKind::Cube, 0, 0, 84, 22, 32, 6, Color{ 200, 40, 40, 255 });   // red scarf
		G.Part(MeshKind::Cube, 0, 0, 50, 21, 31, 6, Color{ 200, 40, 40, 255 });   // belt sash
		G.Part(MeshKind::Sphere, 15, 0, 95, 16, 22, 12, Muzzle);
		G.Part(MeshKind::Cube, 16, 0, 104, 6, 32, 9, Color{ 25, 20, 20, 255 });  // mask band
		G.Part(MeshKind::Sphere, 18, 7, 104, 9, 9, 11, White);
		G.Part(MeshKind::Sphere, 18, -7, 104, 9, 9, 11, White);
		G.Part(MeshKind::Sphere, 21, 7, 104, 5, 5, 6, Color{ 70, 200, 90, 255 }, 0.4f);
		G.Part(MeshKind::Sphere, 21, -7, 104, 5, 5, 6, Color{ 70, 200, 90, 255 }, 0.4f);
		G.Part(MeshKind::Cone, -2, 13, 123, 13, 13, 18, Fur);
		G.Part(MeshKind::Cone, -2, -13, 123, 13, 13, 18, Fur);
		G.Part(MeshKind::Cone, 4, 20, 98, 10, 10, 18, White, 0.0f, 0.0f, 0.0f, 70.0f);
		G.Part(MeshKind::Cone, 4, -20, 98, 10, 10, 18, White, 0.0f, 0.0f, 0.0f, -70.0f);
		// Steel claws.
		for (int k = -1; k <= 1; ++k)
		{
			G.Part(MeshKind::Cone, AlongF(RA, 12), 20 + k * 4.0f, AlongU(RA, 12), 5, 5, 22, Color{ 215, 220, 230, 255 }, 0.0f, RA - 180.0f);
			G.Part(MeshKind::Cone, AlongF(LA, 12), -20 + k * 4.0f, AlongU(LA, 12), 5, 5, 22, Color{ 215, 220, 230, 255 }, 0.0f, LA - 180.0f);
		}
		break;
	}
	case HeroId::Ben:
	{
		Humanoid(G, Skin, Shirt, Pants, Color{ 40, 40, 40, 255 }, Leg, RA, LA);
		G.Part(MeshKind::Cube, 10.5f, 0, 64, 2, 9, 38, Color{ 30, 30, 30, 255 });
		G.Part(MeshKind::Sphere, -3, 0, 113, 38, 38, 24, H.Hair);
		G.Part(MeshKind::Cone, 12, 6, 114, 14, 22, 18, H.Hair, 0.0f, -70.0f);
		// Omnitrix on the left wrist.
		G.Part(MeshKind::Cylinder, AlongF(LA, -8), -20, AlongU(LA, -8), 14, 14, 10, Color{ 35, 35, 40, 255 }, 0.0f, LA - 180.0f);
		G.Part(MeshKind::Sphere, AlongF(LA, -8) + 4, -20, AlongU(LA, -8) + 3, 9, 9, 9, Color{ 90, 255, 90, 255 }, 1.0f);
		break;
	}
	case HeroId::Aang:
	{
		const Color Orange{ 230, 110, 40, 255 }, Yellow = Shirt, Brown{ 140, 95, 55, 255 };
		Humanoid(G, Skin, Yellow, Pants, Color{ 120, 80, 50, 255 }, Leg, RA, LA);
		G.Part(MeshKind::Cube, -2, 0, 70, 24, 34, 26, Tint(Orange));             // orange top / cape
		G.Part(MeshKind::Cube, 0, 0, 50, 22, 32, 8, Tint(Orange));
		G.Part(MeshKind::Cube, 4, 0, 119, 28, 6, 4, Color{ 70, 160, 240, 255 }); // arrow
		G.Part(MeshKind::Cube, 18, 0, 110, 6, 6, 12, Color{ 70, 160, 240, 255 });
		if (P.Gliding)
		{
			G.Part(MeshKind::Cube, 0, 0, 150, 60, 130, 4, Orange);
			G.Part(MeshKind::Cylinder, 0, 0, 150, 6, 6, 150, Brown, 0.0f, 90.0f);
		}
		else
		{
			// Glider staff held across the body.
			G.Part(MeshKind::Cylinder, HandF(RA) + 4, 4, HandU(RA) + 6, 6, 6, 160, Brown, 0.0f, 0.0f, 0.0f, 75.0f);
		}
		if (P.ScooterTime > 0.0f)
		{
			G.Part(MeshKind::Sphere, 0, 0, -12, 74, 74, 54, Color{ 170, 210, 255, 255 }, 0.5f, Time * 500.0f);
		}
		break;
	}
	default:
		Humanoid(G, Skin, Shirt, Pants, Color{ 40, 40, 40, 255 }, Leg, RA, LA);
		break;
	}
}
