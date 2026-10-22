// 3D character models for the prototype, designed from the production-bible
// concept sheet (Prototype/assets/concept). Each hero is an articulated
// skeleton (pelvis, spine, chest, neck, head, two-segment arms and legs) built
// from rounded primitives, with the character's face from the sheet applied
// to the head as a decal, and procedurally animated: idle breathing and
// weight shift, walk/run cycles with knee bend and hip twist, jumps, landings,
// flight, gliding, attacks and hit reactions.
//
// Local model space: centimetres, origin between the feet, +Y up, +Z forward,
// +X = the character's left.

#include "Game.h"

#include <algorithm>
#include <cmath>

const HeroDef& GetHeroDef(HeroId Id);
const FormDef* GetFormDef(int FormId);

namespace
{
	constexpr float D2R = 3.14159265358979f / 180.0f;

	Matrix Mul(const Matrix& A, const Matrix& B) { return MatrixMultiply(A, B); }
	Matrix T(float X, float Y, float Z) { return MatrixTranslate(X, Y, Z); }
	Matrix Rx(float Deg) { return MatrixRotateX(Deg * D2R); }
	Matrix Ry(float Deg) { return MatrixRotateY(Deg * D2R); }
	Matrix Rz(float Deg) { return MatrixRotateZ(Deg * D2R); }
	Vector3 Point(const Matrix& M, Vector3 P) { return Vector3Transform(P, M); }

	struct Painter
	{
		ToonRenderer* R;
		float Line;
		unsigned char Alpha;
		bool bFlash;

		Color Tint(Color C) const { return bFlash ? Color{ 255, 255, 255, Alpha } : Color{ C.r, C.g, C.b, Alpha }; }

		void Piece(MeshKind K, const Matrix& Bone, Vector3 Off, Vector3 Size, Color C, float Em = 0.0f, Vector3 RotDeg = { 0, 0, 0 }) const
		{
			Matrix L = MatrixScale(Size.x, Size.y, Size.z);
			if (RotDeg.x != 0.0f || RotDeg.y != 0.0f || RotDeg.z != 0.0f)
			{
				L = Mul(L, MatrixRotateXYZ(Vector3{ RotDeg.x * D2R, RotDeg.y * D2R, RotDeg.z * D2R }));
			}
			L = Mul(L, T(Off.x, Off.y, Off.z));
			R->Draw(K, Mul(L, Bone), Tint(C), Line, Em);
		}
		void Box(const Matrix& B, Vector3 O, Vector3 S, Color C, float Em = 0, Vector3 Rot = { 0, 0, 0 }) const { Piece(MeshKind::Cube, B, O, S, C, Em, Rot); }
		void Ball(const Matrix& B, Vector3 O, Vector3 S, Color C, float Em = 0, Vector3 Rot = { 0, 0, 0 }) const { Piece(MeshKind::Sphere, B, O, S, C, Em, Rot); }
		void Cyl(const Matrix& B, Vector3 O, Vector3 S, Color C, float Em = 0, Vector3 Rot = { 0, 0, 0 }) const { Piece(MeshKind::Cylinder, B, O, S, C, Em, Rot); }
		void Cone(const Matrix& B, Vector3 O, Vector3 S, Color C, float Em = 0, Vector3 Rot = { 0, 0, 0 }) const { Piece(MeshKind::Cone, B, O, S, C, Em, Rot); }

		// Limb segment hanging down -Y from the bone origin, with a rounded end.
		void Limb(const Matrix& Bone, float Len, float W, Color C, bool bEndCap = true) const
		{
			Cyl(Bone, { 0, -Len * 0.5f, 0 }, { W, Len, W }, C);
			if (bEndCap) Ball(Bone, { 0, -Len, 0 }, { W * 1.02f, W * 1.02f, W * 1.02f }, C);
		}
	};

	// Proportions and palette (sampled from the concept sheet).
	struct Body
	{
		float HeadR = 18.0f;
		Vector3 HeadScale{ 1.0f, 1.0f, 1.0f };
		float Neck = 6.0f, Torso = 40.0f, Leg = 60.0f, Arm = 50.0f;
		float ShoulderW = 32.0f, HipW = 24.0f, ArmW = 9.0f, LegW = 12.0f;
		float Chest = 1.0f;              // chest bulk multiplier
		Color Skin{ 250, 210, 180, 255 };
		Color Top, Sleeve, Forearm, Hand, Pants, Shin, Shoe, Belt;
		bool bBelt = false;
		float FaceW = 1.25f, FaceH = 1.0f, FaceY = -0.1f; // decal size/offset in head radii
		float RestAbduct = 9.0f;         // arms slightly away from the body
	};

	Body BodyFor(HeroId Id, const FormDef* Form)
	{
		Body B;
		const Color White{ 245, 245, 248, 255 }, Black{ 32, 32, 38, 255 };
		if (Form)
		{
			const std::string N = Form->Name;
			if (N == "Heatblast")
			{
				B.HeadR = 17; B.Torso = 46; B.Leg = 68; B.Arm = 56; B.ShoulderW = 36; B.LegW = 13; B.ArmW = 10;
				B.Skin = Form->Body; B.Top = B.Sleeve = B.Forearm = B.Pants = B.Shin = Form->Body;
				B.Hand = Color{ 255, 190, 60, 255 }; B.Shoe = Color{ 120, 35, 20, 255 };
				B.FaceW = 1.3f; B.FaceH = 1.3f; B.FaceY = 0.0f;
			}
			else if (N == "Four Arms")
			{
				B.HeadR = 19; B.HeadScale = { 1.05f, 1.0f, 1.0f }; B.Neck = 6; B.Torso = 72; B.Leg = 72; B.Arm = 78;
				B.ShoulderW = 96; B.HipW = 46; B.ArmW = 22; B.LegW = 24; B.Chest = 1.35f;
				B.Skin = Form->Body; B.Top = White; B.Sleeve = B.Forearm = B.Hand = Form->Body;
				B.Pants = B.Shin = Black; B.Shoe = Black; B.Belt = Color{ 230, 190, 60, 255 }; B.bBelt = true;
				B.FaceW = 1.2f; B.FaceH = 1.1f; B.FaceY = 0.05f; B.RestAbduct = 16;
			}
			else if (N == "XLR8")
			{
				B.HeadR = 16; B.HeadScale = { 0.9f, 1.0f, 1.35f }; B.Torso = 40; B.Leg = 64; B.Arm = 48; B.ShoulderW = 30;
				B.Skin = Form->Body; B.Top = B.Sleeve = B.Forearm = B.Hand = Form->Body;
				B.Pants = B.Shin = Form->Accent; B.Shoe = Form->Accent;
			}
			else // Way Big
			{
				B.HeadR = 19; B.HeadScale = { 0.9f, 1.15f, 1.0f }; B.Torso = 48; B.Leg = 66; B.Arm = 58; B.ShoulderW = 40; B.Chest = 1.2f;
				B.Skin = Form->Body; B.Top = Form->Body; B.Sleeve = B.Forearm = B.Hand = Form->Body;
				B.Pants = B.Shin = Form->Accent; B.Shoe = Form->Body;
			}
			return B;
		}
		const HeroDef& H = GetHeroDef(Id);
		B.Skin = H.Skin;
		B.Top = B.Sleeve = H.Primary;
		B.Forearm = H.Skin;
		B.Hand = H.Skin;
		B.Pants = B.Shin = H.Secondary;
		B.Shoe = White;
		switch (Id)
		{
		case HeroId::Jimmy:
			B.HeadR = 25; B.HeadScale = { 1.0f, 1.08f, 0.95f }; B.Neck = 5; B.Torso = 33; B.Leg = 50; B.Arm = 42;
			B.ShoulderW = 28; B.HipW = 22; B.ArmW = 8; B.LegW = 11; B.Sleeve = H.Primary;
			B.FaceW = 1.45f; B.FaceH = 1.1f; B.FaceY = -0.18f;
			break;
		case HeroId::Timmy:
			B.HeadR = 25; B.HeadScale = { 1.08f, 1.0f, 1.0f }; B.Neck = 4; B.Torso = 32; B.Leg = 48; B.Arm = 40;
			B.ShoulderW = 27; B.HipW = 22; B.ArmW = 8; B.LegW = 10.5f;
			B.FaceW = 1.75f; B.FaceH = 1.45f; B.FaceY = -0.05f;
			break;
		case HeroId::Danny:
			B.HeadR = 17; B.Neck = 8; B.Torso = 44; B.Leg = 66; B.Arm = 54; B.ShoulderW = 33; B.HipW = 24;
			B.Top = B.Sleeve = B.Forearm = B.Pants = B.Shin = Color{ 30, 30, 36, 255 };
			B.Hand = White; B.Shoe = White; B.Belt = White; B.bBelt = true;
			B.FaceW = 1.3f; B.FaceH = 1.15f; B.FaceY = -0.05f;
			break;
		case HeroId::Jake:
			B.HeadR = 17; B.Neck = 7; B.Torso = 42; B.Leg = 64; B.Arm = 52; B.ShoulderW = 34; B.HipW = 24;
			B.Forearm = H.Primary; B.Pants = B.Shin = Color{ 60, 85, 150, 255 }; B.Shoe = Color{ 230, 230, 230, 255 };
			B.FaceW = 1.15f; B.FaceH = 1.25f; B.FaceY = -0.1f;
			break;
		case HeroId::XJ9:
			B.HeadR = 19; B.HeadScale = { 1.05f, 1.0f, 1.0f }; B.Neck = 6; B.Torso = 36; B.Leg = 68; B.Arm = 52;
			B.ShoulderW = 30; B.HipW = 22; B.ArmW = 7; B.LegW = 9;
			B.Skin = Color{ 240, 244, 250, 255 }; B.Top = B.Sleeve = B.Forearm = B.Hand = B.Skin;
			B.Pants = B.Skin; B.Shin = Color{ 70, 190, 205, 255 }; B.Shoe = Color{ 70, 190, 205, 255 };
			B.FaceW = 1.45f; B.FaceH = 1.1f; B.FaceY = -0.1f;
			break;
		case HeroId::ElTigre:
			B.HeadR = 25; B.HeadScale = { 1.1f, 1.0f, 1.0f }; B.Neck = 4; B.Torso = 32; B.Leg = 40; B.Arm = 40;
			B.ShoulderW = 34; B.HipW = 26; B.ArmW = 10; B.LegW = 12;
			B.Skin = Color{ 125, 78, 42, 255 }; B.Top = B.Sleeve = Color{ 55, 40, 38, 255 }; B.Forearm = B.Hand = Color{ 70, 50, 40, 255 };
			B.Pants = B.Shin = Color{ 85, 52, 32, 255 }; B.Shoe = Color{ 30, 25, 25, 255 };
			B.Belt = Color{ 200, 40, 40, 255 }; B.bBelt = true;
			B.FaceW = 1.8f; B.FaceH = 1.15f; B.FaceY = 0.05f;
			break;
		case HeroId::Ben:
			B.HeadR = 18; B.Neck = 6; B.Torso = 40; B.Leg = 60; B.Arm = 50; B.ShoulderW = 32;
			B.Sleeve = White; B.Pants = B.Shin = Color{ 95, 120, 60, 255 }; B.Shoe = Color{ 240, 240, 240, 255 };
			B.FaceW = 1.2f; B.FaceH = 1.25f; B.FaceY = -0.12f;
			break;
		case HeroId::Aang:
			B.HeadR = 18; B.Neck = 6; B.Torso = 40; B.Leg = 58; B.Arm = 50; B.ShoulderW = 31;
			B.Top = Color{ 240, 190, 60, 255 }; B.Sleeve = B.Forearm = Color{ 240, 190, 60, 255 };
			B.Pants = B.Shin = Color{ 238, 185, 75, 255 }; B.Shoe = Color{ 125, 80, 50, 255 };
			B.Belt = Color{ 230, 110, 40, 255 }; B.bBelt = true;
			B.FaceW = 1.15f; B.FaceH = 1.3f; B.FaceY = -0.12f;
			break;
		default:
			break;
		}
		return B;
	}

	// Joint angles for one frame (degrees). Index 0 = left, 1 = right.
	struct Pose3
	{
		float Lean = 0, HipTwist = 0, ChestTwist = 0, Bob = 0, ShiftX = 0, HipRoll = 0;
		float HeadYaw = 0, HeadPitch = 0, HeadRoll = 0;
		float ShFwd[2]{}, ShAbd[2]{}, Elbow[2]{};
		float Thigh[2]{}, ThighAbd[2]{}, Knee[2]{}, Ankle[2]{};
		float Breath = 1.0f;
		float BodyPitch = 0.0f; // whole-body pitch about the pelvis (flight)
	};
}

void Game::RenderHero3D(ToonRenderer& R, unsigned char Alpha, bool bHurt)
{
	const HeroDef& H = GetHeroDef(P.Hero);
	const FormDef* Form = P.Hero == HeroId::Ben ? GetFormDef(P.Omni.GetActiveForm()) : nullptr;
	const std::string FormName = Form ? Form->Name : "";
	// Characters get a lighter line than the per-show weight used for cut-outs.
	Painter Pt{ &R, std::clamp((Form ? 2.0f : H.LineWeight) * 0.5f, 0.9f, 1.8f), Alpha, bHurt };

	// Cannonbolt: armoured ball, rolling.
	if (FormName == "Cannonbolt")
	{
		const Vector3 C = ToRender(P.Pos);
		const float D = P.H * 2.0f * 0.01f;
		const Matrix Ball = Mul(Mul(Rx(P.WalkPhase * 40.0f), Ry(90.0f - P.Yaw)), T(C.x, C.y, C.z));
		Pt.Ball(Ball, { 0, 0, 0 }, { D, D, D }, Form->Body);
		for (int k = 0; k < 4; ++k)
		{
			Pt.Box(Mul(Rx(k * 45.0f), Ball), { 0, 0, 0 }, { D * 1.03f, D * 0.2f, D * 1.03f }, Form->Accent);
		}
		return;
	}

	// Jake's dragon form while flying.
	if (P.Hero == HeroId::Jake && P.Flying)
	{
		const Vector3 F = ToRender(Vector3{ P.Pos.x, P.Pos.y, P.Pos.z - P.H });
		const float K = 0.01f * 1.6f * P.H / 70.0f;
		const Matrix Root = Mul(Mul(MatrixScale(K, K, K), Ry(90.0f - P.Yaw)), T(F.x, F.y, F.z));
		const Color Red = Pt.Tint(Color{ 205, 35, 30, 255 }), Belly{ 245, 200, 70, 255 }, Wing{ 150, 25, 25, 255 }, Horn{ 250, 225, 150, 255 };
		const float Flap = std::sin(Time * 7.0f) * 38.0f;
		const float Bob = std::sin(Time * 7.0f + 1.0f) * 4.0f;
		const Matrix Body = Mul(Rx(8.0f), Mul(T(0, 60 + Bob, 0), Root));
		Pt.Ball(Body, { 0, 0, 0 }, { 40, 38, 80 }, Red);
		Pt.Ball(Body, { 0, -10, 4 }, { 30, 26, 66 }, Belly);
		const Matrix Neck = Mul(Rx(-35.0f + std::sin(Time * 2.0f) * 5.0f - (P.BreathTime > 0.0f ? 15.0f : 0.0f)), Mul(T(0, 12, 34), Body));
		Pt.Cyl(Neck, { 0, 16, 0 }, { 18, 34, 18 }, Red);
		const Matrix Head = Mul(Rx(50.0f), Mul(T(0, 34, 0), Neck));
		Pt.Ball(Head, { 0, 0, 6 }, { 26, 22, 34 }, Red);
		Pt.Box(Head, { 0, -4, 24 }, { 18, 12, 22 }, Red);
		Pt.Ball(Head, { 8, 5, 14 }, { 6, 6, 6 }, Color{ 255, 230, 80, 255 }, 1.0f);
		Pt.Ball(Head, { -8, 5, 14 }, { 6, 6, 6 }, Color{ 255, 230, 80, 255 }, 1.0f);
		Pt.Cone(Head, { 8, 14, -8 }, { 7, 20, 7 }, Horn, 0, { -40, 0, 0 });
		Pt.Cone(Head, { -8, 14, -8 }, { 7, 20, 7 }, Horn, 0, { -40, 0, 0 });
		for (int s = -1; s <= 1; s += 2)
		{
			const Matrix Shoulder = Mul(Rz(s * Flap), Mul(T(s * 18.0f, 14, 8), Body));
			Pt.Cyl(Shoulder, { s * 30.0f, 0, 0 }, { 6, 60, 6 }, Red, 0, { 0, 0, 90 });
			Pt.Box(Shoulder, { s * 34.0f, -2, -18 }, { 64, 3, 44 }, Wing);
			const Matrix Outer = Mul(Rz(s * Flap * 0.6f), Mul(T(s * 60.0f, 0, 0), Shoulder));
			Pt.Box(Outer, { s * 30.0f, -2, -22 }, { 60, 3, 48 }, Wing);
			Pt.Cyl(Mul(Rx(30.0f), Mul(T(s * 14.0f, -24, 18), Body)), { 0, -12, 0 }, { 9, 24, 9 }, Red);
			Pt.Cyl(Mul(Rx(70.0f), Mul(T(s * 14.0f, -24, -24), Body)), { 0, -14, 0 }, { 11, 28, 11 }, Red);
		}
		Matrix Tail = Mul(T(0, -4, -38), Body);
		for (int k = 0; k < 5; ++k)
		{
			Tail = Mul(Rx(-8.0f + std::sin(Time * 3.0f + k * 0.7f) * 10.0f), Mul(T(0, 0, -16), Tail));
			Pt.Ball(Tail, { 0, 0, 0 }, { 20.0f - k * 3.0f, 18.0f - k * 3.0f, 26 }, Red);
		}
		Pt.Cone(Tail, { 0, 0, -14 }, { 10, 20, 10 }, Red, 0, { -90, 0, 0 });
		return;
	}

	Body B = BodyFor(P.Hero, Form);
	// Chunky cartoon proportions: thick limbs, big hands and shoes.
	B.ArmW *= 1.45f;
	B.LegW *= 1.4f;
	B.ShoulderW += 6.0f;
	B.HipW += 4.0f;
	const float BaseH = Form ? Form->HalfHeight : 70.0f;
	const float ModelH = B.Leg + B.Torso + B.Neck + 2.0f * B.HeadR * B.HeadScale.y;
	const float Scale = (2.0f * P.H) / ModelH * (BaseH / BaseH);

	// --- Animation state -> joint angles ---------------------------------------
	const float Speed = std::sqrt(P.Vel.x * P.Vel.x + P.Vel.y * P.Vel.y);
	const float Walk = P.Grounded ? std::min(1.0f, Speed / 650.0f) : 0.0f;
	const float Run = P.Grounded ? std::clamp((Speed - 750.0f) / 900.0f, 0.0f, 1.0f) : 0.0f;
	const float Ph = P.WalkPhase;
	const float Sw = std::sin(Ph), Cw = std::cos(Ph);
	const float Idle = 1.0f - Walk;
	const float Atk = P.AttackAnim > 0.0f ? std::sin(3.14159f * (1.0f - P.AttackAnim / 0.25f)) : 0.0f;
	const float Hurt = std::min(1.0f, P.HurtFlash / 0.25f);
	const float Land = P.LandTimer / 0.18f;
	const bool bAir = !P.Grounded && !P.Flying && !P.Gliding;
	const float A = 1.0f + Run * 0.7f;

	Pose3 O;
	O.Breath = 1.0f + 0.025f * std::sin(Time * 2.2f) * Idle;
	O.ShiftX = std::sin(Time * 0.8f) * 1.5f * Idle;
	O.HipRoll = std::sin(Time * 0.8f) * 2.0f * Idle;
	O.HeadYaw = std::sin(Time * 0.45f) * 14.0f * Idle;
	O.HeadPitch = std::sin(Time * 0.7f) * 3.0f * Idle;
	for (int s = 0; s < 2; ++s)
	{
		O.ShAbd[s] = B.RestAbduct + std::sin(Time * 2.2f) * 2.0f * Idle;
		O.Elbow[s] = 12.0f + 6.0f * Idle;
		O.Knee[s] = 4.0f;
	}
	// Walk / run cycle.
	O.Thigh[0] = 30.0f * Walk * A * Sw;
	O.Thigh[1] = -30.0f * Walk * A * Sw;
	O.Knee[0] += (10.0f + 50.0f * A * std::max(0.0f, Cw)) * Walk;
	O.Knee[1] += (10.0f + 50.0f * A * std::max(0.0f, -Cw)) * Walk;
	O.Ankle[0] = (O.Knee[0] - O.Thigh[0]) * 0.35f * Walk;
	O.Ankle[1] = (O.Knee[1] - O.Thigh[1]) * 0.35f * Walk;
	O.ShFwd[0] = -32.0f * Walk * A * Sw;
	O.ShFwd[1] = 32.0f * Walk * A * Sw;
	O.Elbow[0] += (18.0f + 55.0f * Run) * Walk;
	O.Elbow[1] += (18.0f + 55.0f * Run) * Walk;
	O.Bob = -3.0f * Walk * A * std::fabs(Sw) + 1.5f * Walk;
	O.HipTwist = 8.0f * Walk * Sw;
	O.ChestTwist = -10.0f * Walk * Sw;
	O.Lean = 4.0f * Walk + 12.0f * Run;
	O.HeadYaw += -O.ChestTwist * 0.6f;

	if (bAir)
	{
		const float Up = P.Vel.z > 0.0f ? 1.0f : 0.7f;
		O.Thigh[0] = 50.0f; O.Thigh[1] = 18.0f;
		O.Knee[0] = 80.0f; O.Knee[1] = 35.0f;
		for (int s = 0; s < 2; ++s) { O.ShAbd[s] = 55.0f * Up; O.ShFwd[s] = -15.0f; O.Elbow[s] = 35.0f; }
		O.Lean = 6.0f;
	}
	if (P.LandTimer > 0.0f)
	{
		O.Knee[0] += 50.0f * Land; O.Knee[1] += 50.0f * Land;
		O.Thigh[0] += 30.0f * Land; O.Thigh[1] += 30.0f * Land;
		O.Bob -= 10.0f * Land;
		O.Lean += 12.0f * Land;
	}
	if (P.Flying)
	{
		// Superhero flight: body pitches into the direction of travel.
		const float Fwd = std::clamp(Speed / 1500.0f, 0.0f, 1.0f);
		const float Hover = std::sin(Time * 3.0f);
		O.BodyPitch = 65.0f * Fwd;
		O.Thigh[0] = -8.0f + Hover * 6.0f; O.Thigh[1] = -2.0f - Hover * 6.0f;
		O.Knee[0] = 25.0f; O.Knee[1] = 15.0f;
		O.ShFwd[1] = 40.0f + 120.0f * Fwd; O.Elbow[1] = 5.0f; O.ShAbd[1] = 8.0f; // lead arm
		O.ShFwd[0] = -25.0f; O.ShAbd[0] = 14.0f; O.Elbow[0] = 20.0f;
		O.Lean = 0.0f;
		O.HeadPitch = -O.BodyPitch * 0.6f;
		if (P.Hero == HeroId::Timmy)
		{
			O.BodyPitch = 0.0f; O.HeadPitch = 0.0f;
			O.ShFwd[1] = 30.0f; O.Thigh[0] = O.Thigh[1] = 10.0f; O.Knee[0] = O.Knee[1] = 25.0f;
		}
	}
	if (P.Gliding)
	{
		for (int s = 0; s < 2; ++s) { O.ShFwd[s] = 170.0f; O.ShAbd[s] = 18.0f; O.Elbow[s] = 15.0f; O.Knee[s] = 25.0f; }
		O.Thigh[0] = -10.0f; O.Thigh[1] = 10.0f;
	}
	// Attacks.
	const bool bBrawler = P.Hero == HeroId::Jake || P.Hero == HeroId::ElTigre || FormName == "Four Arms" || FormName == "Way Big"
		|| (P.Hero == HeroId::Ben && !Form) || FormName == "XLR8";
	if (Atk > 0.0f)
	{
		if (bBrawler)
		{
			const int S = std::fmod(Time * 4.0f, 2.0f) < 1.0f ? 1 : 0;
			O.ShFwd[S] = 90.0f * Atk; O.Elbow[S] = 10.0f; O.ShAbd[S] = 0.0f;
			O.ChestTwist += (S == 1 ? 25.0f : -25.0f) * Atk;
			O.Lean += 8.0f * Atk;
			if (P.Hero == HeroId::ElTigre || P.Hero == HeroId::Jake) O.ShAbd[S] = 35.0f * Atk; // claw swipe
		}
		else if (P.Hero == HeroId::Timmy)
		{
			O.ShFwd[1] = 150.0f * Atk; O.Elbow[1] = 30.0f;
		}
		else
		{
			O.ShFwd[1] = 88.0f * Atk; O.Elbow[1] = 4.0f; O.ShAbd[1] = 2.0f;
			O.ChestTwist += 18.0f * Atk;
		}
	}
	if (P.BreathTime > 0.0f) O.HeadPitch -= 12.0f;
	if (Hurt > 0.0f)
	{
		O.Lean -= 15.0f * Hurt;
		O.HeadPitch -= 14.0f * Hurt;
		O.ShAbd[0] += 30.0f * Hurt; O.ShAbd[1] += 30.0f * Hurt;
	}

	// --- Skeleton ----------------------------------------------------------------
	const Vector3 Feet = ToRender(Vector3{ P.Pos.x, P.Pos.y, P.Pos.z - P.H });
	const float K = 0.01f * Scale;
	const Matrix Root = Mul(Mul(MatrixScale(K, K, K), Ry(90.0f - P.Yaw)), T(Feet.x, Feet.y, Feet.z));
	const float HipY = B.Leg + O.Bob;
	const bool bTail = P.Hero == HeroId::Danny && P.Flying; // ghost tail replaces the legs
	const Matrix Pelvis = Mul(Mul(Mul(Mul(Ry(O.HipTwist), Rz(O.HipRoll)), Rx(O.Lean)),
	                      Mul(Rx(O.BodyPitch), T(O.ShiftX, HipY, 0))), Root);
	const Matrix Chest = Mul(Mul(Ry(O.ChestTwist - O.HipTwist), T(0, B.Torso * 0.5f, 0)), Pelvis);
	const Matrix Head = Mul(Mul(MatrixRotateXYZ(Vector3{ O.HeadPitch * D2R, O.HeadYaw * D2R, O.HeadRoll * D2R }),
	                        T(0, B.Torso * 0.5f + B.Neck + B.HeadR * B.HeadScale.y * 0.9f, 0)), Chest);
	Matrix Shoulder[2], Elbow[2], Hand[2], Hip[2], Knee[2], Foot[2];
	const float Upper = B.Arm * 0.48f, Fore = B.Arm * 0.40f;
	const float Thigh = B.Leg * 0.47f, Shin = B.Leg * 0.43f;
	for (int s = 0; s < 2; ++s)
	{
		const float Side = s == 0 ? 1.0f : -1.0f;
		Shoulder[s] = Mul(Mul(Mul(Rz(Side * O.ShAbd[s]), Rx(-O.ShFwd[s])), T(Side * B.ShoulderW * 0.5f, B.Torso * 0.38f, 0)), Chest);
		Elbow[s] = Mul(Mul(Rx(-O.Elbow[s]), T(0, -Upper, 0)), Shoulder[s]);
		Hand[s] = Mul(T(0, -Fore, 0), Elbow[s]);
		Hip[s] = Mul(Mul(Mul(Rz(Side * (2.0f + O.ThighAbd[s])), Rx(-O.Thigh[s])), T(Side * B.HipW * 0.28f, -2.0f, 0)), Pelvis);
		Knee[s] = Mul(Mul(Rx(O.Knee[s]), T(0, -Thigh, 0)), Hip[s]);
		Foot[s] = Mul(Mul(Rx(-O.Ankle[s]), T(0, -Shin, 0)), Knee[s]);
	}

	// --- Body ---------------------------------------------------------------------
	Pt.Ball(Pelvis, { 0, 1, 0 }, { B.HipW * 1.05f, 18, B.HipW * 0.75f }, B.Pants);
	Pt.Cyl(Pelvis, { 0, B.Torso * 0.26f, 0 }, { B.HipW * 0.95f, B.Torso * 0.42f, B.HipW * 0.68f }, B.Top);
	Pt.Ball(Chest, { 0, B.Torso * 0.12f, 0 }, { B.ShoulderW * 0.95f * B.Chest, B.Torso * 0.82f * O.Breath, std::min(B.ShoulderW * 0.6f * B.Chest, B.Torso * 0.8f) }, B.Top);
	if (B.bBelt) Pt.Cyl(Pelvis, { 0, 9, 0 }, { B.HipW * 1.0f, 6, B.HipW * 0.74f }, B.Belt);
	Pt.Cyl(Chest, { 0, B.Torso * 0.5f + B.Neck * 0.4f, 0 }, { B.ArmW * 1.1f, B.Neck * 1.4f, B.ArmW * 1.1f }, B.Skin);
	for (int s = 0; s < 2; ++s)
	{
		Pt.Ball(Shoulder[s], { 0, 0, 0 }, { B.ArmW * 1.35f, B.ArmW * 1.35f, B.ArmW * 1.35f }, B.Sleeve);
		Pt.Limb(Shoulder[s], Upper, B.ArmW * 1.08f, B.Sleeve);
		Pt.Limb(Elbow[s], Fore, B.ArmW, B.Forearm, false);
		Pt.Ball(Hand[s], { 0, -B.ArmW * 0.55f, 0 }, { B.ArmW * 1.5f, B.ArmW * 1.65f, B.ArmW * 1.25f }, B.Hand);
		if (!bTail)
		{
			Pt.Limb(Hip[s], Thigh, B.LegW, B.Pants);
			Pt.Limb(Knee[s], Shin, B.LegW * 0.92f, B.Shin, false);
			Pt.Ball(Foot[s], { 0, -B.LegW * 0.3f, B.LegW * 0.5f }, { B.LegW * 1.25f, B.LegW * 0.8f, B.LegW * 2.2f }, B.Shoe);
		}
	}

	// --- Head + face decal from the sheet -------------------------------------------
	const float HR = B.HeadR;
	Pt.Ball(Head, { 0, 0, 0 }, { HR * 2.0f * B.HeadScale.x, HR * 2.0f * B.HeadScale.y, HR * 2.0f * B.HeadScale.z }, B.Skin);
	const int FaceIndex = FormName == "Heatblast" ? SprHeatblast : (FormName == "Four Arms" ? SprFourArms : (Form ? -1 : static_cast<int>(P.Hero)));
	if (FaceIndex >= 0 && Faces[FaceIndex].id != 0 && R.CurrentPass() == RenderPass::Color)
	{
		const float Fw = HR * B.FaceW * B.HeadScale.x, Fh = HR * B.FaceH * B.HeadScale.y;
		const float Fz = HR * B.HeadScale.z * 1.01f + 0.4f, Fy = HR * B.FaceY * B.HeadScale.y;
		const Vector3 BL = Point(Head, { Fw * 0.5f, Fy - Fh * 0.5f, Fz });
		const Vector3 BR = Point(Head, { -Fw * 0.5f, Fy - Fh * 0.5f, Fz });
		const Vector3 TL = Point(Head, { Fw * 0.5f, Fy + Fh * 0.5f, Fz });
		const Vector3 N = Vector3Normalize(Vector3Subtract(Point(Head, { 0, 0, 1 }), Point(Head, { 0, 0, 0 })));
		// Shade the decal like the cel-lit head around it.
		const SceneLighting Light = Lighting();
		const float Ndl = std::max(0.0f, Vector3DotProduct(N, Vector3Negate(Vector3Normalize(Light.SunDirection))));
		const float Band = std::floor(Ndl * Light.Bands + 0.5f) / Light.Bands;
		const unsigned char Lum = static_cast<unsigned char>(std::clamp(185.0f + 70.0f * Band, 0.0f, 255.0f));
		R.DrawSpriteQuad(Faces[FaceIndex], BL, BR, TL, N, Color{ Lum, Lum, Lum, Alpha }, bHurt ? 0.75f : 0.0f, Pt.Line);
	}

	// --- Character details --------------------------------------------------------------
	const Color White{ 245, 245, 250, 255 };
	if (FormName == "Heatblast")
	{
		for (int k = 0; k < 3; ++k)
		{
			Pt.Box(Chest, { -6.0f + k * 6, -4.0f + k * 6, B.ShoulderW * 0.29f }, { 2, 14, 2 }, Color{ 255, 210, 60, 255 }, 1.0f, { 0, 0, 30.0f - k * 25 });
		}
		const float Flick = std::sin(Time * 20.0f) * 3.0f;
		Pt.Cone(Head, { 0, HR * 1.3f + Flick, -3 }, { HR * 2.0f, HR * 2.2f, HR * 2.0f }, Color{ 255, 160, 30, 255 }, 1.0f);
		Pt.Cone(Head, { 0, HR * 2.2f + Flick, -5 }, { HR * 1.1f, HR * 1.6f, HR * 1.1f }, Color{ 255, 240, 120, 255 }, 1.0f);
		for (int s = 0; s < 2; ++s) Pt.Ball(Hand[s], { 0, -4, 0 }, { 16, 16, 16 }, Color{ 255, 200, 60, 255 }, 1.0f);
		if (Atk > 0.0f) Pt.Ball(Hand[1], { 0, -10, 0 }, { 30, 30, 30 }, Color{ 255, 170, 40, 255 }, 1.0f);
	}
	else if (FormName == "Four Arms")
	{
		Pt.Box(Chest, { 0, B.Torso * 0.1f, B.ShoulderW * 0.27f }, { 16, B.Torso * 0.7f, 4 }, Color{ 30, 30, 35, 255 });
		// Second (lower) pair of arms.
		for (int s = 0; s < 2; ++s)
		{
			const float Side = s == 0 ? 1.0f : -1.0f;
			const int Opp = 1 - s;
			const Matrix Low = Mul(Mul(Mul(Rz(Side * (B.RestAbduct + 10.0f)), Rx(-O.ShFwd[Opp] * 0.8f)), T(Side * B.ShoulderW * 0.42f, B.Torso * 0.05f, 0)), Chest);
			const Matrix LowE = Mul(Mul(Rx(-O.Elbow[Opp]), T(0, -Upper * 0.9f, 0)), Low);
			Pt.Ball(Low, { 0, 0, 0 }, { B.ArmW * 1.2f, B.ArmW * 1.2f, B.ArmW * 1.2f }, B.Skin);
			Pt.Limb(Low, Upper * 0.9f, B.ArmW * 0.95f, B.Skin);
			Pt.Limb(LowE, Fore * 0.9f, B.ArmW * 0.9f, B.Skin, false);
			Pt.Ball(LowE, { 0, -Fore * 0.9f - 4, 0 }, { B.ArmW * 1.2f, B.ArmW * 1.3f, B.ArmW }, B.Skin);
		}
	}
	else if (FormName == "XLR8")
	{
		Pt.Box(Head, { 0, -2, HR * 0.8f }, { HR * 1.3f, HR * 0.7f, HR * 0.9f }, Color{ 20, 20, 25, 255 });
		Pt.Box(Head, { 0, 2, HR * 1.2f }, { HR * 1.0f, HR * 0.25f, 3 }, Color{ 80, 200, 255, 255 }, 1.0f);
		Pt.Cone(Pelvis, { 0, -4, -26 }, { 12, 60, 12 }, Form->Body, 0, { -115, 0, 0 });
	}
	else if (FormName == "Way Big")
	{
		Pt.Box(Chest, { 0, 0, B.ShoulderW * 0.29f }, { B.ShoulderW * 0.25f, B.Torso * 0.8f, 3 }, Form->Accent);
		Pt.Cone(Head, { 0, HR * 1.4f, -4 }, { 6, HR * 1.6f, HR * 1.6f }, Form->Accent);
		Pt.Ball(Head, { 7, 3, HR * 0.9f }, { 6, 7, 3 }, Color{ 255, 240, 120, 255 }, 1.0f);
		Pt.Ball(Head, { -7, 3, HR * 0.9f }, { 6, 7, 3 }, Color{ 255, 240, 120, 255 }, 1.0f);
	}
	else switch (P.Hero)
	{
	case HeroId::Jimmy:
	{
		// The famous swirl.
		const Color Hair{ 125, 72, 35, 255 };
		Pt.Ball(Head, { 0, HR * 0.55f, -2 }, { HR * 2.1f, HR * 1.4f, HR * 2.0f }, Hair);
		Pt.Cone(Head, { 0, HR * 1.4f, -2 }, { HR * 1.6f, HR * 1.9f, HR * 1.6f }, Hair, 0, { -10, 0, 0 });
		Pt.Cone(Head, { 0, HR * 2.35f, 4 }, { HR * 0.8f, HR * 0.9f, HR * 0.8f }, Hair, 0, { -50, 0, 0 });
		Pt.Ball(Head, { HR * 0.95f, -HR * 0.05f, 0 }, { 8, 12, 6 }, B.Skin); // ears
		Pt.Ball(Head, { -HR * 0.95f, -HR * 0.05f, 0 }, { 8, 12, 6 }, B.Skin);
		Pt.Box(Chest, { 0, 2, B.ShoulderW * 0.29f }, { 8, 8, 2 }, Color{ 255, 220, 60, 255 }, 0.5f);
		Pt.Box(Chest, { 0, 2, B.ShoulderW * 0.29f }, { 16, 2.5f, 2 }, Color{ 255, 220, 60, 255 }, 0.4f, { 0, 0, 50 });
		Pt.Box(Chest, { 0, 2, B.ShoulderW * 0.29f }, { 16, 2.5f, 2 }, Color{ 255, 220, 60, 255 }, 0.4f, { 0, 0, -50 });
		for (int s = -1; s <= 1; s += 2)
		{
			Pt.Cyl(Chest, { s * 7.0f, 2, -B.ShoulderW * 0.3f - 4 }, { 9, 26, 9 }, Color{ 210, 215, 225, 255 });
			Pt.Cone(Chest, { s * 7.0f, 19, -B.ShoulderW * 0.3f - 4 }, { 9, 10, 9 }, Color{ 225, 50, 40, 255 });
			if (P.SigCd > 0.2f) Pt.Cone(Chest, { s * 7.0f, -20, -B.ShoulderW * 0.3f - 4 }, { 9, 22 + std::sin(Time * 40.0f) * 5, 9 }, Color{ 255, 170, 40, 255 }, 1.0f, { 180, 0, 0 });
		}
		// Shrink ray in the right hand, pointing along the forearm.
		Pt.Box(Hand[1], { 0, -8, 4 }, { 7, 12, 14 }, Color{ 120, 175, 140, 255 });
		Pt.Cyl(Hand[1], { 0, -14, 0 }, { 5, 14, 5 }, Color{ 200, 200, 210, 255 });
		Pt.Ball(Hand[1], { 0, -22, 0 }, { 8, 8, 8 }, Color{ 255, 70, 70, 255 }, 0.9f);
		break;
	}
	case HeroId::Timmy:
	{
		const Color Pink = H.Primary;
		Pt.Cyl(Head, { 0, HR * 0.75f, -2 }, { HR * 2.05f, HR * 0.55f, HR * 2.0f }, Pink);
		Pt.Ball(Head, { 0, HR * 1.0f, -3 }, { HR * 1.9f, HR * 0.9f, HR * 1.85f }, Pink);
		Pt.Box(Head, { 0, HR * 0.55f, HR * 1.0f }, { HR * 1.7f, 3, HR * 0.9f }, Pink, 0, { -8, 0, 0 });
		Pt.Ball(Head, { HR * 0.95f, HR * 0.2f, -4 }, { 12, 12, 14 }, H.Hair);
		Pt.Ball(Head, { -HR * 0.95f, HR * 0.2f, -4 }, { 12, 12, 14 }, H.Hair);
		Pt.Cyl(Hand[1], { 0, -14, 0 }, { 2.5f, 26, 2.5f }, Color{ 30, 30, 35, 255 });
		Pt.Ball(Hand[1], { 0, -28, 0 }, { 12, 12, 12 }, Color{ 255, 230, 70, 255 }, Atk > 0.0f ? 1.0f : 0.7f);
		Pt.Box(Hand[0], { 0, -8, 6 }, { 6, 26, 20 }, Color{ 135, 60, 165, 255 });
		Pt.Box(Hand[0], { 0, -8, 9.5f }, { 6.5f, 8, 1 }, Color{ 240, 200, 80, 255 }, 0.3f);
		// Cosmo & Wanda.
		const Vector3 Center = ToRender(P.Pos);
		for (int k = 0; k < 2; ++k)
		{
			const float An = Time * 2.0f + k * 3.14159f;
			Vector3 F = Vector3Add(Center, Vector3{ std::cos(An) * 0.62f, 0.62f + std::sin(Time * 4.0f + k) * 0.08f, std::sin(An) * 0.62f });
			if (P.Flying) F = Vector3Add(ToRender(Vector3{ P.Pos.x, P.Pos.y, P.Pos.z - P.H }), Vector3{ k ? 0.25f : -0.25f, -0.12f, 0.0f });
			const Matrix Fm = Mul(MatrixScale(0.01f, 0.01f, 0.01f), T(F.x, F.y, F.z));
			const Color HairC = k == 0 ? Color{ 90, 210, 100, 255 } : Color{ 245, 120, 185, 255 };
			if (P.Flying)
			{
				Pt.Ball(Fm, { 0, -6, 0 }, { 70, 30, 50 }, Color{ 250, 250, 255, 255 }, 0.3f); // cloud-board
			}
			Pt.Cyl(Fm, { 0, -12, 0 }, { 10, 16, 10 }, k == 0 ? White : Color{ 250, 220, 80, 255 });
			Pt.Ball(Fm, { 0, 4, 0 }, { 18, 18, 18 }, Color{ 250, 215, 185, 255 });
			Pt.Ball(Fm, { 0, 16, -2 }, { k ? 24.0f : 16.0f, k ? 18.0f : 22.0f, k ? 24.0f : 16.0f }, HairC);
			Pt.Cone(Fm, { 0, 30, 0 }, { 8, 10, 8 }, Color{ 255, 220, 60, 255 }, 0.6f);
			const float Fl = std::sin(Time * 30.0f) * 25.0f;
			Pt.Box(Fm, { 8, 0, -8 }, { 14, 10, 2 }, Color{ 220, 240, 255, 255 }, 0.4f, { 0, 20 + Fl, 0 });
			Pt.Box(Fm, { -8, 0, -8 }, { 14, 10, 2 }, Color{ 220, 240, 255, 255 }, 0.4f, { 0, -20 - Fl, 0 });
		}
		break;
	}
	case HeroId::Danny:
	{
		Pt.Ball(Head, { 0, HR * 0.5f, -2 }, { HR * 2.15f, HR * 1.3f, HR * 2.1f }, White);
		for (int k = -1; k <= 1; ++k) Pt.Cone(Head, { k * 8.0f, HR * 0.55f, -HR * 0.9f }, { 10, 22, 10 }, White, 0, { -110, 0, k * 20.0f });
		Pt.Cone(Head, { 0, HR * 0.65f, HR * 0.75f }, { 12, 14, 10 }, White, 0, { 70, 0, 0 });
		Pt.Cyl(Chest, { 0, B.Torso * 0.46f, 0 }, { B.ArmW * 1.5f, 4, B.ArmW * 1.5f }, White); // collar
		Pt.Box(Chest, { 0, 2, B.ShoulderW * 0.29f }, { 14, 12, 2 }, White);                  // DP emblem
		Pt.Box(Chest, { 2, 2, B.ShoulderW * 0.29f + 1 }, { 4, 8, 2 }, B.Top);
		for (int s = 0; s < 2; ++s) Pt.Ball(Hand[s], { 0, -5, 0 }, { 20, 20, 20 }, Color{ 110, 255, 140, 255 }, 1.0f);
		if (bTail)
		{
			const float Sway = std::sin(Time * 5.0f) * 12.0f;
			Pt.Cone(Pelvis, { 0, -26, -4 }, { 30, 54, 26 }, B.Top, 0, { 180 + Sway, 0, 0 });
			Pt.Cone(Pelvis, { 0, -56, -14 - Sway * 0.5f }, { 18, 32, 16 }, Color{ 120, 255, 200, 255 }, 0.7f, { 200 + Sway * 1.5f, 0, 0 });
		}
		break;
	}
	case HeroId::Jake:
	{
		const Color Hair{ 20, 20, 22, 255 }, Green{ 80, 220, 120, 255 };
		Pt.Ball(Head, { 0, HR * 0.55f, -2 }, { HR * 2.1f, HR * 1.25f, HR * 2.05f }, Hair);
		for (int k = 0; k < 6; ++k)
		{
			const float X = -12.0f + k * 5.0f;
			Pt.Cone(Head, { X, HR * 1.05f, -2.0f + (k % 2) * 4 }, { 9, 22, 9 }, k % 2 ? Green : Hair, 0, { -15.0f - k * 3.0f, 0, -X * 1.5f });
		}
		Pt.Box(Chest, { 0, 0, B.ShoulderW * 0.28f }, { 9, B.Torso * 0.65f, 2 }, Color{ 130, 130, 140, 255 });
		break;
	}
	case HeroId::XJ9:
	{
		const Color Teal{ 70, 190, 205, 255 }, Joint{ 60, 70, 95, 255 };
		Pt.Box(Head, { 0, HR * 0.95f, -2 }, { HR * 1.7f, 6, HR * 1.8f }, Teal);
		for (int s = -1; s <= 1; s += 2)
		{
			Pt.Box(Head, { s * HR * 1.15f, HR * 1.0f, -3 }, { HR * 1.4f, 5, HR * 0.9f }, Teal, 0, { 0, 0, s * 30.0f });
			Pt.Cyl(Head, { s * HR * 0.98f, 0, 0 }, { 10, 6, 10 }, Joint, 0, { 0, 0, 90 });
		}
		Pt.Cone(Pelvis, { 0, -4, 0 }, { B.HipW * 2.0f, 22, B.HipW * 1.6f }, Teal, 0, { 180, 0, 0 });
		Pt.Cyl(Chest, { 0, B.Torso * 0.15f, 0 }, { B.ShoulderW * 0.9f, 6, B.ShoulderW * 0.58f }, Teal);
		Pt.Ball(Chest, { 0, 0, B.ShoulderW * 0.29f }, { 6, 6, 3 }, Teal, 0.5f);
		for (int s = 0; s < 2; ++s)
		{
			Pt.Ball(Elbow[s], { 0, 0, 0 }, { 9, 9, 9 }, Joint);
			Pt.Ball(Knee[s], { 0, 0, 0 }, { 11, 11, 11 }, Joint);
			Pt.Cyl(Elbow[s], { 0, -Fore * 0.5f, 4 }, { 8, 14, 8 }, Color{ 225, 60, 50, 255 }); // forearm launcher
			if (P.Flying) Pt.Cone(Foot[s], { 0, -14, 0 }, { 12, 26 + std::sin(Time * 40.0f) * 6.0f, 12 }, Color{ 255, 190, 60, 255 }, 1.0f, { 180, 0, 0 });
		}
		break;
	}
	case HeroId::ElTigre:
	{
		const Color Fur = B.Skin;
		Pt.Cone(Head, { HR * 0.55f, HR * 1.0f, -2 }, { 13, 18, 13 }, Fur, 0, { 0, 0, -15 });
		Pt.Cone(Head, { -HR * 0.55f, HR * 1.0f, -2 }, { 13, 18, 13 }, Fur, 0, { 0, 0, 15 });
		Pt.Cone(Head, { HR * 1.0f, -HR * 0.1f, 2 }, { 10, 18, 10 }, White, 0, { 0, 0, -75 });
		Pt.Cone(Head, { -HR * 1.0f, -HR * 0.1f, 2 }, { 10, 18, 10 }, White, 0, { 0, 0, 75 });
		Pt.Cyl(Chest, { 0, B.Torso * 0.45f, 0 }, { B.ShoulderW * 0.8f, 6, B.ShoulderW * 0.55f }, Color{ 200, 40, 40, 255 });
		for (int s = 0; s < 2; ++s)
		{
			for (int c = -1; c <= 1; ++c)
			{
				Pt.Cone(Hand[s], { c * 4.0f, -12, 3 }, { 4, 22, 4 }, Color{ 215, 220, 230, 255 }, 0, { 180, 0, c * 8.0f });
			}
		}
		break;
	}
	case HeroId::Ben:
	{
		Pt.Ball(Head, { 0, HR * 0.55f, -2 }, { HR * 2.12f, HR * 1.3f, HR * 2.05f }, H.Hair);
		Pt.Cone(Head, { HR * 0.3f, HR * 0.6f, HR * 0.75f }, { 16, 14, 10 }, H.Hair, 0, { 70, 0, -20 });
		Pt.Box(Chest, { 0, 0, B.ShoulderW * 0.28f }, { 8, B.Torso * 0.7f, 2 }, Color{ 30, 30, 30, 255 });
		Pt.Box(Chest, { 0, 0, -B.ShoulderW * 0.28f }, { 8, B.Torso * 0.7f, 2 }, Color{ 30, 30, 30, 255 });
		for (int s = 0; s < 2; ++s) Pt.Box(Hip[s], { (s ? -1.0f : 1.0f) * B.LegW * 0.5f, -Thigh * 0.55f, 0 }, { 3, 10, 9 }, Color{ 80, 105, 50, 255 });
		Pt.Cyl(Elbow[0], { 0, -Fore + 4, 0 }, { 13, 7, 13 }, Color{ 35, 35, 40, 255 });
		Pt.Ball(Elbow[0], { 0, -Fore + 4, 6 }, { 8, 8, 4 }, Color{ 90, 255, 90, 255 }, 1.0f);
		break;
	}
	case HeroId::Aang:
	{
		const Color Orange{ 230, 110, 40, 255 }, Blue{ 70, 160, 240, 255 }, Brown{ 140, 95, 55, 255 };
		Pt.Box(Head, { 0, HR * 0.97f, 0 }, { 5, 3, HR * 1.7f }, Blue, 0, { 0, 0, 0 });
		Pt.Box(Head, { 0, HR * 0.5f, HR * 0.86f }, { 5, HR * 0.8f, 3 }, Blue, 0, { -25, 0, 0 });
		Pt.Ball(Chest, { 0, B.Torso * 0.3f, -2 }, { B.ShoulderW * 1.15f, B.Torso * 0.45f, B.ShoulderW * 0.75f }, Orange);
		Pt.Box(Pelvis, { 0, 2, B.HipW * 0.36f }, { B.HipW * 0.7f, 20, 2 }, Orange);
		if (P.Gliding)
		{
			const Matrix Grip = Mul(T(0, -6, 0), Hand[1]);
			Pt.Cyl(Grip, { -15, 0, 0 }, { 5, 150, 5 }, Brown, 0, { 0, 0, 90 });
			Pt.Box(Grip, { -15, 4, 0 }, { 130, 2, 55 }, Orange);
		}
		else
		{
			Pt.Cyl(Hand[1], { 0, -6, 0 }, { 5, 160, 5 }, Brown, 0, { Atk > 0.0f ? 360.0f * (1.0f - P.AttackAnim / 0.25f) : 0.0f, 0, 0 });
		}
		if (P.ScooterTime > 0.0f)
		{
			Pt.Ball(Root, { 0, -6, 0 }, { 70, 50, 70 }, Color{ 170, 210, 255, 255 }, 0.5f, { 0, Time * 500.0f, 0 });
		}
		break;
	}
	default:
		break;
	}
}
