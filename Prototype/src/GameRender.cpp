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
		else if (StartsWith(L, "Suburb")) { S.Tint = HouseColors[H % 6]; S.Roof = true; }
		else if (StartsWith(L, "Downtown")) S.Tint = TowerColors[H % 4];
		else if (L == "Neutron_House") { S.Tint = Color{ 245, 245, 250, 255 }; S.Roof = true; }
		else if (StartsWith(L, "Neutron_Lab")) S.Tint = Color{ 205, 60, 60, 255 };
		else if (StartsWith(L, "School")) S.Tint = Color{ 200, 95, 70, 255 };
		else if (StartsWith(L, "CandyBar")) { S.Tint = Color{ 250, 165, 200, 255 }; S.Roof = true; }
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

	// Shared kid-sized humanoid (140 cm). Returns nothing; extras are drawn by the caller.
	void Humanoid(const Rig& G, Color Skin, Color Shirt, Color Pants, Color Shoes, float Swing, float ArmSwing, bool bHead = true, float HeadSize = 36.0f)
	{
		G.Limb(0, 8, 46, 12, 44, Swing, Pants);
		G.Limb(0, -8, 46, 12, 44, -Swing, Pants);
		G.Part(MeshKind::Cube, 3, 8, 3, 18, 12, 7, Shoes);
		G.Part(MeshKind::Cube, 3, -8, 3, 18, 12, 7, Shoes);
		G.Part(MeshKind::Cube, 0, 0, 64, 20, 30, 38, Shirt);
		G.Limb(0, 20, 80, 9, 36, -ArmSwing, Shirt);
		G.Limb(0, -20, 80, 9, 36, ArmSwing, Shirt);
		if (bHead)
		{
			G.Part(MeshKind::Sphere, 0, 0, 100, HeadSize, HeadSize, HeadSize, Skin);
			G.Part(MeshKind::Sphere, HeadSize * 0.42f, 6, 103, 6, 6, 8, Color{ 25, 25, 30, 255 });
			G.Part(MeshKind::Sphere, HeadSize * 0.42f, -6, 103, 6, 6, 8, Color{ 25, 25, 30, 255 });
		}
	}
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

		if (S.Roof)
		{
			const float RoofH = std::min(B.Size.x, B.Size.y) * 0.45f;
			const Vector3 Top{ B.Center.x, B.Center.y, B.Center.z + B.Half.z + RoofH * 0.5f };
			const Color RoofColor = (Hash(B.Label) % 3 == 0) ? Color{ 90, 110, 160, 255 } : (Hash(B.Label) % 3 == 1 ? Color{ 185, 80, 70, 255 } : Color{ 110, 90, 80, 255 });
			R.Draw(MeshKind::Pyramid, BoxTransform(Top, Vector3{ B.Size.x * 1.12f, B.Size.y * 1.12f, RoofH }, B.YawDeg), RoofColor, EnvLineWeight);
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
	Rig G{ &R, Sub(Car.Pos, Vector3{ 0, 0, 30 }), Car.Yaw, 1.0f, 1.6f, 255 };
	const float P = Car.VisPitch, Ro = Car.VisRoll;
	auto Part = [&](MeshKind K, float F, float Rt, float U, float SF, float SR, float SU, Color C, float Em = 0.0f, float ExtraPitch = 0.0f) {
		// Rotate the offset by the visual pitch/roll so the body tilts as one piece.
		const float Pr = P * Deg2Rad, Rr = Ro * Deg2Rad;
		const float U1 = U * std::cos(Pr) + F * std::sin(Pr);
		const float F1 = F * std::cos(Pr) - U * std::sin(Pr);
		const float U2 = U1 * std::cos(Rr) - Rt * std::sin(Rr);
		const float R2 = Rt * std::cos(Rr) + U1 * std::sin(Rr);
		G.Part(K, F1, R2, U2, SF, SR, SU, C, Em, P + ExtraPitch, 0.0f, -Ro);
	};
	Part(MeshKind::Cube, 0, 0, 30, 420, 190, 60, Color{ 225, 55, 55, 255 });
	Part(MeshKind::Cube, 40, 0, 63, 300, 120, 8, Color{ 250, 250, 250, 255 });
	Part(MeshKind::Sphere, -10, 0, 85, 170, 130, 90, Color{ 150, 220, 255, 255 }, 0.25f);
	Part(MeshKind::Cube, -200, 0, 80, 40, 220, 14, Color{ 225, 55, 55, 255 });
	Part(MeshKind::Cylinder, -225, 60, 35, 50, 50, 70, Color{ 90, 90, 110, 255 }, 0.0f, 90.0f);
	Part(MeshKind::Cylinder, -225, -60, 35, 50, 50, 70, Color{ 90, 90, 110, 255 }, 0.0f, 90.0f);
	const float Thrust = 0.5f + 0.5f * std::sin(Time * 30.0f);
	Part(MeshKind::Sphere, -262, 60, 35, 30, 34, 34, Color{ 120, 230, 255, 255 }, 0.7f + 0.3f * Thrust);
	Part(MeshKind::Sphere, -262, -60, 35, 30, 34, 34, Color{ 120, 230, 255, 255 }, 0.7f + 0.3f * Thrust);
	const float Pads[4][2] = { { 180, 90 }, { 180, -90 }, { -180, 90 }, { -180, -90 } };
	for (const auto& Pd : Pads)
	{
		Part(MeshKind::Cylinder, Pd[0], Pd[1], -2, 60, 60, 10, Color{ 120, 230, 255, 255 }, 0.9f);
	}
	if (Car.Occupied)
	{
		const HeroDef& H = GetHeroDef(this->P.Hero);
		Part(MeshKind::Sphere, -10, 0, 120, 34, 34, 34, H.Skin);
		Part(MeshKind::Cube, -10, 0, 90, 20, 28, 30, H.Primary);
		if (this->P.Hero == HeroId::Jimmy)
		{
			Part(MeshKind::Sphere, -10, 0, 142, 30, 30, 24, H.Hair);
		}
	}
}

void Game::RenderHero(ToonRenderer& R, bool bTranslucent)
{
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
	const float Swing = (P.Grounded ? std::min(1.0f, Speed / 600.0f) : 0.3f) * std::sin(P.WalkPhase) * 35.0f;
	const float AttackSwing = P.AttackAnim > 0.0f ? 70.0f : 0.0f;
	const unsigned char Alpha = bGhost ? 110 : 255;
	const bool bHurt = P.HurtFlash > 0.0f && std::fmod(Time, 0.1f) < 0.05f;
	auto Tint = [&](Color C) { return bHurt ? Color{ 255, 255, 255, C.a } : C; };

	if (P.Hero == HeroId::Ben && P.Omni.GetState() == Omnitrix::EState::Transforming)
	{
		const float D = P.H * 2.4f;
		R.Draw(MeshKind::Sphere, MakeTransform(ToRender(P.Pos), Vector3{ D * 0.01f, D * 0.012f, D * 0.01f }, 0.0f), Color{ 90, 255, 90, 255 }, 1.0f, 1.0f);
		return;
	}

	if (Form)
	{
		const float S = P.H / Form->HalfHeight;
		Rig G{ &R, Feet, P.Yaw, S, 2.0f, Alpha };
		const std::string Name = Form->Name;
		if (Name == "Heatblast")
		{
			Rig G2 = G;
			G2.Scale = P.H / 70.0f * 0.95f;
			Humanoid(G2, Form->Body, Form->Body, Shade(Form->Body, 0.7f), Shade(Form->Body, 0.5f), Swing, Swing + AttackSwing, false);
			const float Flick = std::sin(Time * 20.0f) * 4.0f;
			G2.Part(MeshKind::Sphere, 0, 0, 100, 34, 34, 34, Form->Body, 0.3f);
			G2.Part(MeshKind::Cone, -2, 0, 128 + Flick, 34, 34, 50, Color{ 255, 170, 40, 255 }, 1.0f);
			G2.Part(MeshKind::Cone, -6, 0, 150 + Flick, 18, 18, 34, Color{ 255, 240, 120, 255 }, 1.0f);
			G2.Part(MeshKind::Sphere, 15, 6, 102, 7, 7, 8, Color{ 255, 255, 180, 255 }, 1.0f);
			G2.Part(MeshKind::Sphere, 15, -6, 102, 7, 7, 8, Color{ 255, 255, 180, 255 }, 1.0f);
		}
		else if (Name == "Four Arms")
		{
			// 240 cm red brute with two pairs of arms.
			G.Scale = P.H / 120.0f;
			G.Limb(0, 16, 80, 22, 80, Swing, Form->Accent);
			G.Limb(0, -16, 80, 22, 80, -Swing, Form->Accent);
			G.Part(MeshKind::Cube, 0, 0, 125, 44, 70, 70, Form->Body);
			G.Limb(0, 44, 155, 20, 70, -Swing - AttackSwing, Form->Body);
			G.Limb(0, -44, 155, 20, 70, Swing + AttackSwing, Form->Body);
			G.Limb(0, 40, 128, 18, 62, Swing * 0.5f + AttackSwing * 0.7f, Form->Body);
			G.Limb(0, -40, 128, 18, 62, -Swing * 0.5f + AttackSwing * 0.7f, Form->Body);
			G.Part(MeshKind::Sphere, 4, 0, 180, 40, 40, 38, Form->Body);
			for (int e = 0; e < 4; ++e)
			{
				G.Part(MeshKind::Sphere, 21, e < 2 ? 8 : -8, e % 2 ? 176 : 186, 6, 6, 6, Color{ 255, 220, 60, 255 }, 0.8f);
			}
		}
		else if (Name == "XLR8")
		{
			G.Scale = P.H / 75.0f;
			const float Lean = std::min(1.0f, Speed / 1500.0f) * 25.0f;
			G.Limb(0, 9, 50, 12, 48, Swing * 1.4f, Form->Accent);
			G.Limb(0, -9, 50, 12, 48, -Swing * 1.4f, Form->Accent);
			G.Part(MeshKind::Cube, 4, 0, 70, 22, 28, 40, Form->Body, 0.0f, -Lean);
			G.Limb(4, 20, 86, 9, 36, -Swing - AttackSwing, Form->Body);
			G.Limb(4, -20, 86, 9, 36, Swing + AttackSwing, Form->Body);
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
			Humanoid(G, Form->Body, Form->Body, Form->Accent, Form->Accent, Swing * 0.6f, Swing * 0.6f + AttackSwing, false);
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
	const float ArmSwing = Swing + AttackSwing;

	switch (P.Hero)
	{
	case HeroId::Jimmy:
		Humanoid(G, Skin, Shirt, Pants, Color{ 250, 250, 250, 255 }, Swing, ArmSwing, true, 42.0f);
		G.Part(MeshKind::Sphere, -2, 0, 126, 34, 34, 26, H.Hair);
		G.Part(MeshKind::Cone, -4, 0, 146, 18, 18, 24, H.Hair);
		G.Part(MeshKind::Sphere, 9, 0, 70, 8, 10, 10, Color{ 255, 230, 120, 255 }, 0.6f); // atom
		G.Part(MeshKind::Cube, -14, 0, 66, 10, 20, 26, Color{ 200, 200, 210, 255 });    // jetpack
		break;
	case HeroId::Timmy:
	{
		Humanoid(G, Skin, Shirt, Pants, Color{ 250, 250, 250, 255 }, Swing, ArmSwing);
		G.Part(MeshKind::Cylinder, 0, 0, 120, 36, 36, 14, Shirt);
		G.Part(MeshKind::Cube, 16, 0, 114, 18, 30, 3, Shirt);
		G.Part(MeshKind::Cube, 17, 0, 98, 4, 10, 4, Color{ 255, 255, 255, 255 }); // buck teeth
		// Cosmo & Wanda orbiting.
		for (int k = 0; k < 2; ++k)
		{
			const float A = Time * 2.0f + k * 3.14159f;
			const Color F = k == 0 ? Color{ 90, 220, 110, 255 } : Color{ 245, 110, 180, 255 };
			const float Fx = std::cos(A) * 55.0f, Fy = std::sin(A) * 55.0f, Fz = 125.0f + std::sin(Time * 4.0f + k) * 8.0f;
			G.Part(MeshKind::Sphere, Fx, Fy, Fz, 18, 18, 18, F, 0.3f);
			G.Part(MeshKind::Cone, Fx, Fy, Fz + 14, 10, 10, 10, Color{ 255, 220, 60, 255 }, 0.6f);
		}
		if (P.Flying)
		{
			G.Part(MeshKind::Sphere, 0, 0, -12, 110, 70, 30, Color{ 250, 250, 255, 255 }, 0.2f);
			G.Part(MeshKind::Sphere, 30, 18, -8, 50, 50, 30, Color{ 250, 250, 255, 255 }, 0.2f);
		}
		break;
	}
	case HeroId::Danny:
	{
		const Color White = Tint(H.Secondary);
		Humanoid(G, Skin, Shirt, Shirt, White, Swing, ArmSwing);
		G.Part(MeshKind::Cube, 0, 0, 48, 21, 31, 6, White);   // belt
		G.Part(MeshKind::Sphere, -3, 0, 110, 38, 38, 22, H.Hair);
		G.Part(MeshKind::Sphere, 16, 6, 103, 6, 6, 8, Color{ 90, 255, 130, 255 }, 1.0f);
		G.Part(MeshKind::Sphere, 16, -6, 103, 6, 6, 8, Color{ 90, 255, 130, 255 }, 1.0f);
		G.Part(MeshKind::Cube, 10.5f, 0, 70, 2, 14, 14, White); // DP logo
		break;
	}
	case HeroId::Jake:
		if (P.Flying)
		{
			// American Dragon form.
			const Color Red = Tint(Color{ 205, 40, 40, 255 });
			const float Flap = std::sin(Time * 9.0f) * 35.0f;
			G.Part(MeshKind::Cube, 0, 0, 60, 70, 34, 34, Red);
			G.Part(MeshKind::Cube, 0, 0, 52, 60, 24, 14, Tint(Color{ 250, 200, 80, 255 }));
			G.Part(MeshKind::Cube, 44, 0, 74, 34, 26, 26, Red);
			G.Part(MeshKind::Cube, 64, 0, 70, 18, 18, 14, Red);
			G.Part(MeshKind::Cone, 40, 8, 92, 8, 8, 18, Color{ 250, 220, 120, 255 });
			G.Part(MeshKind::Cone, 40, -8, 92, 8, 8, 18, Color{ 250, 220, 120, 255 });
			G.Part(MeshKind::Cube, 0, 50, 76, 50, 70, 4, Tint(Color{ 40, 170, 90, 255 }), 0.0f, 0.0f, 0.0f, -Flap);
			G.Part(MeshKind::Cube, 0, -50, 76, 50, 70, 4, Tint(Color{ 40, 170, 90, 255 }), 0.0f, 0.0f, 0.0f, Flap);
			G.Part(MeshKind::Cone, -58, 0, 56, 18, 18, 60, Red, 0.0f, 100.0f);
			G.Part(MeshKind::Sphere, 58, 7, 80, 6, 6, 6, Color{ 255, 230, 80, 255 }, 1.0f);
			G.Part(MeshKind::Sphere, 58, -7, 80, 6, 6, 6, Color{ 255, 230, 80, 255 }, 1.0f);
		}
		else
		{
			Humanoid(G, Skin, Shirt, Pants, Color{ 240, 240, 240, 255 }, Swing, ArmSwing);
			for (int k = 0; k < 4; ++k)
			{
				G.Part(MeshKind::Cone, -6 + k * 2, -9 + k * 6, 122, 12, 12, 22, k % 2 ? Color{ 80, 220, 120, 255 } : H.Hair, 0.0f, -15.0f);
			}
		}
		break;
	case HeroId::XJ9:
	{
		const Color White = Shirt, Blue = Pants;
		G.Limb(0, 8, 42, 11, 40, Swing, White);
		G.Limb(0, -8, 42, 11, 40, -Swing, White);
		G.Part(MeshKind::Cone, 0, 0, 40, 44, 44, 26, Blue, 0.0f, 180.0f);
		G.Part(MeshKind::Cube, 0, 0, 64, 20, 26, 34, White);
		G.Part(MeshKind::Cube, 0, 0, 72, 21, 27, 6, Blue);
		G.Limb(0, 19, 78, 9, 36, -ArmSwing, White);
		G.Limb(0, -19, 78, 9, 36, ArmSwing, White);
		G.Part(MeshKind::Sphere, 0, 0, 100, 36, 34, 36, White);
		G.Part(MeshKind::Cube, -2, 0, 116, 34, 34, 10, Blue);
		G.Part(MeshKind::Cylinder, 0, 22, 104, 14, 12, 22, Blue, 0.0f, 0.0f, 0.0f, 90.0f);
		G.Part(MeshKind::Cylinder, 0, -22, 104, 14, 12, 22, Blue, 0.0f, 0.0f, 0.0f, 90.0f);
		G.Part(MeshKind::Sphere, 15, 6, 101, 7, 7, 9, Color{ 40, 120, 220, 255 }, 0.6f);
		G.Part(MeshKind::Sphere, 15, -6, 101, 7, 7, 9, Color{ 40, 120, 220, 255 }, 0.6f);
		if (P.Flying)
		{
			const float Fl = 20.0f + std::sin(Time * 40.0f) * 6.0f;
			G.Part(MeshKind::Cone, 0, 8, -10, 14, 14, Fl, Color{ 255, 190, 60, 255 }, 1.0f, 180.0f);
			G.Part(MeshKind::Cone, 0, -8, -10, 14, 14, Fl, Color{ 255, 190, 60, 255 }, 1.0f, 180.0f);
		}
		break;
	}
	case HeroId::ElTigre:
	{
		const Color Orange = Shirt, Black = Pants;
		Humanoid(G, Orange, Orange, Black, Black, Swing, ArmSwing, true, 36.0f);
		G.Part(MeshKind::Cube, 0, 0, 52, 21, 31, 6, Black);
		G.Part(MeshKind::Cone, -2, 13, 120, 12, 12, 16, Orange);
		G.Part(MeshKind::Cone, -2, -13, 120, 12, 12, 16, Orange);
		G.Part(MeshKind::Cube, 15, 0, 104, 6, 26, 6, Black);   // mask stripe
		G.Part(MeshKind::Sphere, 17, 0, 94, 14, 18, 10, Skin);  // jaw
		G.Part(MeshKind::Cone, 8, 20, 40, 6, 6, 16, Color{ 220, 220, 235, 255 }, 0.0f, 160.0f);
		G.Part(MeshKind::Cone, 8, -20, 40, 6, 6, 16, Color{ 220, 220, 235, 255 }, 0.0f, 160.0f);
		break;
	}
	case HeroId::Ben:
		Humanoid(G, Skin, Shirt, Pants, Color{ 40, 40, 40, 255 }, Swing, ArmSwing);
		G.Part(MeshKind::Cube, 10.5f, 0, 64, 2, 8, 38, Color{ 30, 30, 30, 255 });
		G.Part(MeshKind::Sphere, -3, 0, 112, 38, 38, 24, H.Hair);
		G.Part(MeshKind::Cube, 0, -20, 48, 14, 14, 10, Color{ 40, 40, 40, 255 });
		G.Part(MeshKind::Sphere, 0, -20, 48, 9, 9, 9, Color{ 90, 255, 90, 255 }, 1.0f); // Omnitrix
		break;
	case HeroId::Aang:
	{
		Humanoid(G, Skin, Shirt, Pants, Color{ 120, 80, 50, 255 }, Swing, ArmSwing);
		G.Part(MeshKind::Cube, -4, 0, 58, 24, 34, 30, Pants);                     // robe
		G.Part(MeshKind::Cube, 4, 0, 119, 26, 6, 4, Color{ 70, 160, 240, 255 });   // arrow
		G.Part(MeshKind::Cube, 17, 0, 110, 6, 6, 10, Color{ 70, 160, 240, 255 });
		if (P.Gliding)
		{
			G.Part(MeshKind::Cube, 0, 0, 150, 60, 130, 4, Pants);
			G.Part(MeshKind::Cylinder, 0, 0, 150, 6, 6, 150, Color{ 140, 95, 55, 255 }, 0.0f, 90.0f);
		}
		else
		{
			G.Part(MeshKind::Cylinder, 8, 28, 70, 6, 6, 150, Color{ 140, 95, 55, 255 });
		}
		if (P.ScooterTime > 0.0f)
		{
			G.Part(MeshKind::Sphere, 0, 0, -10, 70, 70, 50, Color{ 230, 245, 255, 255 }, 0.5f, Time * 500.0f);
		}
		break;
	}
	default:
		Humanoid(G, Skin, Shirt, Pants, Color{ 40, 40, 40, 255 }, Swing, ArmSwing);
		break;
	}
}
