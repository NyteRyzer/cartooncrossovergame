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
		if (B.HasTint)
		{
			S.Tint = B.Tint;
			S.Translucent = B.Tint.a < 255;
			S.Emissive = B.Emissive;
			return S;
		}
		if (L == "Retroland_FerrisWheel") { S.Hidden = true; return S; } // drawn animated
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
		case Shape::Pyramid: return MeshKind::Pyramid;
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
	if (BatchLevel != LevelIndex)
	{
		BuildStaticBatch(R);
	}
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

	if (!bTranslucent)
	{
		R.DrawStatic(ToRender(CamPos), Vector3{ CamForward().x, CamForward().z, CamForward().y }, 1600.0f);
	}
	RenderBlocks(R, bTranslucent);

	if (!bTranslucent)
	{
		// Retroland ferris wheel, turning.
		if (const Block* W = L.FindBlock("Retroland_FerrisWheel"))
		{
			const float Spin = Time * 8.0f;
			const float Rad = W->Half.x;
			const Vector3 C = W->Center;
			auto OnWheel = [&](float A, float Rr) { return Vector3{ C.x, C.y + std::cos(A * Deg2Rad) * Rr, C.z + std::sin(A * Deg2Rad) * Rr }; };
			for (int k = 0; k < 24; ++k)
			{
				const float A = Spin + k * 15.0f;
				R.Draw(MeshKind::Cube, BoxTransform(OnWheel(A, Rad), Vector3{ 60, 2 * 3.14159f * Rad / 24.0f + 30, 60 }, 0.0f, 0.0f, A), Color{ 255, 90, 100, 255 }, 1.6f, 0.2f);
			}
			for (int k = 0; k < 12; ++k)
			{
				const float A = Spin + k * 30.0f;
				R.Draw(MeshKind::Cube, BoxTransform(OnWheel(A, Rad * 0.5f), Vector3{ 30, Rad, 30 }, 0.0f, 0.0f, A), Color{ 250, 250, 250, 255 }, 1.2f);
				const Vector3 G = Add(OnWheel(A, Rad), Vector3{ 120, 0, -200 });
				const Color Gc = k % 3 == 0 ? Color{ 120, 200, 255, 255 } : (k % 3 == 1 ? Color{ 255, 220, 80, 255 } : Color{ 140, 230, 140, 255 });
				R.Draw(MeshKind::Cube, BoxTransform(G, Vector3{ 220, 200, 180 }, 0.0f), Gc, 1.6f);
			}
			R.Draw(MeshKind::Cylinder, BoxTransform(C, Vector3{ 260, 260, 500 }, 0.0f, 90.0f), Color{ 220, 220, 230, 255 }, 1.6f);
			for (int s = -1; s <= 1; s += 2)
			{
				R.Draw(MeshKind::Cube, BoxTransform(Vector3{ C.x + 150, C.y + s * Rad * 0.45f, C.z * 0.5f }, Vector3{ 60, 60, C.z + 100 }, 0.0f, 0.0f, s * 20.0f), Color{ 200, 200, 210, 255 }, 1.6f);
			}
		}
		// Rift Core energy rings.
		if (const Block* Orb = L.FindBlock("RiftCore_Orb"))
		{
			for (int ring = 0; ring < 3; ++ring)
			{
				const float Tilt = 25.0f + ring * 55.0f, Spin = Time * (30.0f + ring * 20.0f) * (ring % 2 ? -1.0f : 1.0f);
				const float Rr = Orb->Half.x * (1.5f + ring * 0.35f);
				for (int k = 0; k < 18; ++k)
				{
					const float A = (Spin + k * 20.0f) * Deg2Rad;
					const float Ct = std::cos(Tilt * Deg2Rad), St = std::sin(Tilt * Deg2Rad);
					const Vector3 P{ Orb->Center.x + std::cos(A) * Rr, Orb->Center.y + std::sin(A) * Rr * Ct, Orb->Center.z + std::sin(A) * Rr * St };
					R.Draw(MeshKind::Cube, BoxTransform(P, Vector3{ 70, 70, 70 }, A * 50.0f), ring == 1 ? Color{ 255, 120, 220, 255 } : Color{ 120, 255, 230, 255 }, 1.0f, 1.0f);
				}
			}
		}

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

namespace
{
	bool IsDynamicBlock(const Block& B, const BlockStyle& S)
	{
		return S.Gate || S.Translucent || B.Label.rfind("EarthPillar", 0) == 0;
	}

	Color RoofColorFor(const Block& B)
	{
		if (B.RoofTint.a > 0) return B.RoofTint;
		const unsigned H = Hash(B.Label) % 3;
		return H == 0 ? Color{ 90, 110, 160, 255 } : (H == 1 ? Color{ 185, 80, 70, 255 } : Color{ 110, 90, 80, 255 });
	}
}

void Game::BuildStaticBatch(ToonRenderer& R)
{
	const Level& L = Levels[LevelIndex];
	R.ClearStatic();
	R.BeginStaticBatch();
	auto AddBlock = [&](const Block& B) {
		const BlockStyle S = StyleFor(B, LevelIndex);
		if (S.Hidden || IsDynamicBlock(B, S))
		{
			return;
		}
		Vector3 Size = B.Size;
		if (B.Kind == Shape::Plane) Size.z = 4.0f;
		R.AddStatic(MeshFor(B.Kind), BoxTransform(B.Center, Size, B.YawDeg, B.PitchDeg), S.Tint, S.Emissive, EnvLineWeight);
		if (B.RoofHeight > 0.0f)
		{
			const Vector3 Top{ B.Center.x, B.Center.y, B.Center.z + B.Half.z + B.RoofHeight * 0.5f };
			R.AddStatic(MeshKind::Pyramid, BoxTransform(Top, Vector3{ B.Size.x * Block::RoofOverhang, B.Size.y * Block::RoofOverhang, B.RoofHeight }, B.YawDeg),
			            RoofColorFor(B), 0.0f, EnvLineWeight);
		}
	};
	for (const Block& B : L.Blocks) AddBlock(B);
	for (const Block& B : L.Decor) AddBlock(B);
	R.EndStaticBatch();
	BatchLevel = LevelIndex;
	TraceLog(LOG_INFO, "Static batch: %d vertices", R.StaticVertexCount());
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

	auto DrawDynamic = [&](const Block& B, int Index) {
		const BlockStyle S = StyleFor(B, LevelIndex);
		if (S.Hidden || S.Translucent != bTranslucent || !IsDynamicBlock(B, S))
		{
			return;
		}
		if (S.Gate)
		{
			const bool bNext = std::find(NextGates.begin(), NextGates.end(), Index) != NextGates.end();
			const float Pulse = 0.5f + 0.5f * std::sin(Time * 6.0f);
			RenderGate(R, B, bNext ? Lerp(Color{ 80, 255, 160, 255 }, Color{ 255, 255, 120, 255 }, Pulse) : Color{ 110, 150, 190, 255 }, bNext ? 0.9f : 0.1f);
			return;
		}
		R.Draw(MeshFor(B.Kind), BoxTransform(B.Center, B.Size, B.YawDeg, B.PitchDeg), S.Tint, EnvLineWeight, S.Emissive);
		if (B.RoofHeight > 0.0f)
		{
			const Vector3 Top{ B.Center.x, B.Center.y, B.Center.z + B.Half.z + B.RoofHeight * 0.5f };
			R.Draw(MeshKind::Pyramid, BoxTransform(Top, Vector3{ B.Size.x * Block::RoofOverhang, B.Size.y * Block::RoofOverhang, B.RoofHeight }, B.YawDeg), RoofColorFor(B), EnvLineWeight);
		}
	};
	for (size_t i = 0; i < L.Blocks.size(); ++i) DrawDynamic(L.Blocks[i], static_cast<int>(i));
	for (const Block& B : L.Decor) DrawDynamic(B, -1);
	(void)Fwd;
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

Game::Pose Game::ComputePose(int Id) const
{
	Pose O;
	const float Speed = std::sqrt(P.Vel.x * P.Vel.x + P.Vel.y * P.Vel.y);
	const float S = P.Grounded ? std::min(1.0f, Speed / 650.0f) : 0.0f;
	const float Run = P.Grounded ? std::clamp((Speed - 700.0f) / 800.0f, 0.0f, 1.0f) : 0.0f;
	const float Ph = P.WalkPhase;
	const float Swing = std::sin(Ph);
	const float Atk = P.AttackAnim > 0.0f ? std::sin(3.14159f * (1.0f - P.AttackAnim / 0.25f)) : 0.0f;
	const float Hurt = std::min(1.0f, P.HurtFlash / 0.25f);
	const bool bAir = !P.Grounded && !P.Flying && !P.Gliding;

	// Arms at rest (the sheet has T- and A-poses; bring them down naturally).
	float RestA = 0.0f, RestB = 0.0f;
	if (Id == static_cast<int>(HeroId::Jimmy)) { RestA = 68.0f; RestB = -68.0f; }
	else if (Id == SprHeatblast) { RestA = 12.0f; RestB = -12.0f; }
	else if (Id == static_cast<int>(HeroId::Jake) || Id == static_cast<int>(HeroId::Ben) || Id == static_cast<int>(HeroId::XJ9)) { RestA = 6.0f; RestB = -6.0f; }

	// Idle: breathing sway and a slow head bob.
	const float Breath = std::sin(Time * 2.2f);
	O.ArmA = RestA + Breath * 3.0f;
	O.ArmB = RestB - Breath * 3.0f;
	O.Head = std::sin(Time * 1.6f) * 3.0f;

	// Walk / run: alternating leg lifts, pendulum arms, bobbing head.
	const float Amp = 1.0f + Run * 0.8f;
	O.LegA = Swing * 16.0f * S * Amp;
	O.LegB = Swing * 16.0f * S * Amp;
	O.LiftA = std::max(0.0f, Swing) * 0.07f * S * Amp;
	O.LiftB = std::max(0.0f, -Swing) * 0.07f * S * Amp;
	O.ArmA += -Swing * 28.0f * S * Amp;
	O.ArmB += -Swing * 28.0f * S * Amp;
	O.Head += Swing * 5.0f * S;

	if (bAir)
	{
		// Jump: arms thrown up, legs apart.
		const float Up = P.Vel.z > 0.0f ? 1.0f : 0.6f;
		O.ArmA = RestA - 55.0f * Up;
		O.ArmB = RestB + 55.0f * Up;
		O.LegA = -14.0f;
		O.LegB = 14.0f;
		O.LiftA = O.LiftB = 0.03f;
	}
	if (P.Flying)
	{
		// Flight: arms swept back, legs trailing and drifting.
		const float F = std::sin(Time * 3.5f);
		O.ArmA = RestA - 30.0f + F * 6.0f;
		O.ArmB = RestB + 30.0f - F * 6.0f;
		O.LegA = -6.0f + F * 7.0f;
		O.LegB = 6.0f + F * 7.0f;
	}

	// Attacks: blasters raise the casting arm, brawlers punch with both.
	const bool bBrawler = Id == static_cast<int>(HeroId::Jake) || Id == static_cast<int>(HeroId::ElTigre)
		|| Id == static_cast<int>(HeroId::Ben) || Id == SprFourArms;
	if (Atk > 0.0f)
	{
		if (Id == static_cast<int>(HeroId::Timmy))
		{
			O.ArmA = RestA - 70.0f * Atk; // wand hand
		}
		else if (bBrawler)
		{
			const bool bRight = std::fmod(Time * 4.0f, 2.0f) < 1.0f; // alternate jabs
			if (bRight) O.ArmB = RestB + 80.0f * Atk; else O.ArmA = RestA - 80.0f * Atk;
		}
		else
		{
			O.ArmB = RestB + 85.0f * Atk;
		}
		O.Head -= 6.0f * Atk;
	}
	if (P.BreathTime > 0.0f)
	{
		O.Head += 10.0f; // rear back to breathe fire
	}
	if (Hurt > 0.0f)
	{
		O.Head += 12.0f * Hurt;
		O.ArmA += std::sin(Time * 40.0f) * 18.0f * Hurt;
		O.ArmB -= std::sin(Time * 40.0f) * 18.0f * Hurt;
	}

	// Character specifics.
	if (Id == static_cast<int>(HeroId::Danny))
	{
		O.Tail = std::sin(Time * 3.0f) * 10.0f + Swing * 8.0f * S;
	}
	else if (Id == static_cast<int>(HeroId::Aang))
	{
		O.Staff = std::sin(Time * 1.4f) * 4.0f - Swing * 6.0f * S;
		if (Atk > 0.0f) O.Staff = 360.0f * (1.0f - P.AttackAnim / 0.25f); // staff twirl
		if (P.Gliding) { O.Staff = 0.0f; O.StaffLift = 0.22f; }
		if (P.ScooterTime > 0.0f) { O.LegA = -10.0f; O.LegB = 10.0f; }
	}
	else if (Id == SprHeatblast)
	{
		O.HeadScaleY = 1.0f + 0.06f * std::sin(Time * 13.0f);
	}
	else if (Id == SprDragon)
	{
		O.Wing = std::sin(Time * 7.0f) * 22.0f;
		O.Head = std::sin(Time * 2.0f) * 4.0f - (P.BreathTime > 0.0f ? 8.0f : 0.0f);
		O.Tail = std::sin(Time * 2.5f) * 9.0f;
	}
	return O;
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
	const Color Tint{ 255, 255, 255, Alpha };
	const float Flash = bHurt ? 0.75f : 0.0f;
	if (S.Parts.empty() || S.RigH <= 0.0f)
	{
		R.DrawSprite(S.Tex, ToRender(Feet), Right, ToCam, Width * 0.01f, Height * 0.01f, Roll, P.SpriteFlip, Tint, Flash, H.LineWeight);
	}
	else
	{
		// Posed cut-out puppet: rotate each part about its joint (sprite pixel
		// space, y down), then squash/lean the whole figure about the feet.
		const Pose Po = ComputePose(Id);
		const float K = S.HeightCm * Scale / S.RigH;   // cm per sprite pixel (height)
		const float FX = S.RigW * 0.5f, FY = S.RigH;
		const Vector3 RightUe{ -std::sin(CamYaw * Deg2Rad), std::cos(CamYaw * Deg2Rad), 0.0f };
		const Vector3 ToCamUe{ -std::cos(CamYaw * Deg2Rad), -std::sin(CamYaw * Deg2Rad), 0.0f };
		const float Cr = std::cos(Roll), Sr = std::sin(Roll);
		for (const RigPart& Part : S.Parts)
		{
			float Angle = 0.0f, LiftY = 0.0f, ScaleY = 1.0f;
			if (Part.Name == "arm_a") Angle = Po.ArmA;
			else if (Part.Name == "arm_b") Angle = Po.ArmB;
			else if (Part.Name == "leg_a") { Angle = Po.LegA; LiftY = -Po.LiftA * S.RigH; }
			else if (Part.Name == "leg_b") { Angle = Po.LegB; LiftY = -Po.LiftB * S.RigH; }
			else if (Part.Name == "head") { Angle = Po.Head; ScaleY = Po.HeadScaleY; }
			else if (Part.Name == "tail") Angle = Po.Tail;
			else if (Part.Name == "wing") Angle = Po.Wing;
			else if (Part.Name == "staff") { Angle = Po.Staff; LiftY = -Po.StaffLift * S.RigH; }
			const float A = Angle * Deg2Rad, Ca = std::cos(A), Sa = std::sin(A);
			auto Map = [&](float X, float Y) {
				// Joint rotation (counter-clockwise as seen; y points down).
				float Dx = X - Part.PX, Dy = (Y - Part.PY) * ScaleY;
				float Px = Part.PX + Dx * Ca + Dy * Sa;
				float Py = Part.PY - Dx * Sa + Dy * Ca + LiftY;
				// Whole-figure squash and lean about the feet.
				float Lx = (Px - FX) * SX, Ly = (Py - FY) * SY;
				float Rx = Lx * Cr + Ly * Sr, Ry = -Lx * Sr + Ly * Cr;
				if (P.SpriteFlip) Rx = -Rx;
				const Vector3 W = Add(Add(Feet, Mul(RightUe, Rx * K)), Vector3{ 0.0f, 0.0f, -Ry * K });
				return ToRender(Add(W, Mul(ToCamUe, 0.6f * static_cast<float>(Part.Z))));
			};
			R.DrawSpriteQuad(Part.Tex, Map(Part.X, Part.Y + Part.H), Map(Part.X + Part.W, Part.Y + Part.H), Map(Part.X, Part.Y),
			                 ToCam, Tint, Flash, H.LineWeight);
		}
	}

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
	const unsigned char Alpha = bGhost ? 110 : 255;
	const bool bHurt = P.HurtFlash > 0.0f && std::fmod(Time, 0.1f) < 0.05f;

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

	RenderHero3D(R, Alpha, bHurt);
}
