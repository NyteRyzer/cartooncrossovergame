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
		float BodyPitch = 0.0f; // whole-body pitch about the pelvis (flight, flips, rolls)
		float BodyRoll = 0.0f;  // whole-body roll about the travel direction (banking, barrel rolls)
		float Spin = 0.0f;      // extra turn about the vertical (spin attacks, surf stance)
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
		// Wing beats follow the flight model; a fast dive folds the wings back.
		const bool bDive = P.Vel.z < -700.0f && !P.Boosting;
		const float Flap = bDive ? -28.0f : std::sin(P.FlapPhase) * 38.0f;
		const float Bob = bDive ? 0.0f : std::sin(P.FlapPhase + 1.0f) * 4.0f;
		const float Roll = P.FlyBank + (P.DodgeTime > 0.0f && P.DodgeAir ? 360.0f * (1.0f - P.DodgeTime / 0.45f) * P.DodgeSide : 0.0f);
		const Matrix Body = Mul(Mul(Rx(8.0f - P.FlyPitch * 0.7f), Rz(Roll)), Mul(T(0, 60 + Bob, 0), Root));
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
			const Matrix Outer = Mul(Rz(s * (bDive ? 25.0f : Flap * 0.6f)), Mul(T(s * 60.0f, 0, 0), Shoulder));
			Pt.Box(Outer, { s * 30.0f, -2, -22 }, { 60, 3, 48 }, Wing);
			Pt.Cyl(Mul(Rx(30.0f), Mul(T(s * 14.0f, -24, 18), Body)), { 0, -12, 0 }, { 9, 24, 9 }, Red);
			Pt.Cyl(Mul(Rx(70.0f), Mul(T(s * 14.0f, -24, -24), Body)), { 0, -14, 0 }, { 11, 28, 11 }, Red);
		}
		Matrix Tail = Mul(T(0, -4, -38), Body);
		for (int k = 0; k < 5; ++k)
		{
			Tail = Mul(Mul(Rx(-8.0f + std::sin(Time * 3.0f + k * 0.7f) * 10.0f), Ry(-P.FlyBank * 0.15f)), Mul(T(0, 0, -16), Tail));
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
	// A shared skeleton, but every hero moves like themselves: gait, idle
	// personality and fidgets, attack style per combo hit, flight pose, dodge,
	// emote. Layers are applied in order and later layers override earlier ones.
	const HeroId Id = P.Hero;
	const bool bHuman = Form == nullptr;
	auto Is = [&](HeroId H) { return bHuman && Id == H; };
	const float Speed = std::sqrt(P.Vel.x * P.Vel.x + P.Vel.y * P.Vel.y);
	const float Speed3 = std::sqrt(Speed * Speed + P.Vel.z * P.Vel.z);
	const float Pi = 3.14159265f;

	// Gait per hero: stride, cadence, arm swing, knee lift, bounce, hip twist, run lean.
	struct Gait { float Stride, Cadence, Arms, Knees, Bounce, Twist, Lean; };
	Gait Gt{ 1, 1, 1, 1, 1, 1, 1 };
	if (FormName == "Four Arms") Gt = { 0.85f, 0.8f, 0.7f, 0.8f, 1.8f, 0.6f, 0.6f };
	else if (FormName == "XLR8") Gt = { 1.3f, 1.3f, 0.5f, 1.4f, 0.5f, 0.6f, 3.0f };
	else if (FormName == "Way Big") Gt = { 0.8f, 0.75f, 0.6f, 0.8f, 1.2f, 0.5f, 0.5f };
	else if (Is(HeroId::Jimmy)) Gt = { 0.85f, 1.15f, 0.8f, 0.9f, 0.9f, 0.8f, 1.4f };
	else if (Is(HeroId::Timmy)) Gt = { 1.0f, 1.05f, 1.5f, 1.15f, 1.5f, 1.2f, 0.8f };
	else if (Is(HeroId::Danny)) Gt = { 1.05f, 0.95f, 0.7f, 1.0f, 0.8f, 1.0f, 1.0f };
	else if (Is(HeroId::Jake)) Gt = { 1.1f, 0.95f, 1.1f, 1.0f, 1.4f, 1.8f, 0.9f };
	else if (Is(HeroId::XJ9)) Gt = { 0.9f, 1.0f, 0.6f, 0.35f, 0.4f, 0.0f, 0.6f };
	else if (Is(HeroId::ElTigre)) Gt = { 1.1f, 1.05f, 0.8f, 1.3f, 0.9f, 1.0f, 1.6f };
	else if (Is(HeroId::Aang)) Gt = { 1.2f, 0.95f, 0.9f, 1.25f, 1.6f, 1.0f, 1.1f };

	const bool bDodge = P.DodgeTime > 0.0f && !P.Flying;
	const float Walk = P.Grounded && !bDodge ? std::min(1.0f, Speed / 650.0f) : 0.0f;
	const float Run = P.Grounded && !bDodge ? std::clamp((Speed - 750.0f) / 900.0f, 0.0f, 1.0f) : 0.0f;
	const float Ph = P.WalkPhase * Gt.Cadence;
	const float Sw = std::sin(Ph), Cw = std::cos(Ph);
	const float Idle = (1.0f - Walk) * (P.Grounded ? 1.0f : 0.0f);
	const float AtkA = P.AttackAnim > 0.0f ? std::clamp(1.0f - P.AttackAnim / P.AttackDur, 0.0f, 1.0f) : 0.0f; // progress
	const float Atk = P.AttackAnim > 0.0f ? std::sin(Pi * AtkA) : 0.0f;                                         // envelope
	const int Step = P.AttackStep;
	const float Hurt = std::min(1.0f, P.HurtFlash / 0.25f);
	const float Land = P.LandTimer / 0.18f;
	const bool bAir = !P.Grounded && !P.Flying && !P.Gliding && !P.Pounding && !bDodge && P.WallSlide <= 0.0f;
	const float A = (1.0f + Run * 0.7f) * Gt.Stride;
	// Idle fidget: after five seconds standing still, every seven seconds.
	float Fid = 0.0f, FidT = 0.0f;
	if (P.IdleTime > 2.0f)
	{
		const float F = std::fmod(P.IdleTime - 2.0f, 7.0f);
		if (F > 3.0f && F < 5.5f) { FidT = (F - 3.0f) / 2.5f; Fid = std::sin(Pi * FidT); Fid = std::min(1.0f, Fid * 1.6f); }
	}
	Fid *= Idle * (P.AttackAnim > 0.0f ? 0.0f : 1.0f);
	// Emote progress and weight.
	const float Em = P.EmoteTime > 0.0f ? 1.0f - P.EmoteTime / 2.4f : 0.0f;
	const float EmW = P.EmoteTime > 0.0f ? std::clamp(std::min(Em, 1.0f - Em) * 6.0f, 0.0f, 1.0f) : 0.0f;
	float StaffSpin = 0.0f;

	Pose3 O;
	auto Blend = [](float& V, float Target, float W) { V += (Target - V) * W; };
	auto Arm = [&](int S, float Fwd, float Abd, float Elb, float W) { Blend(O.ShFwd[S], Fwd, W); Blend(O.ShAbd[S], Abd, W); Blend(O.Elbow[S], Elb, W); };
	auto Leg = [&](int S, float Th, float Kn, float W) { Blend(O.Thigh[S], Th, W); Blend(O.Knee[S], Kn, W); };

	// Base idle: breathing, weight shift, looking around.
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
	// Walk / run cycle, shaped by the gait.
	O.Thigh[0] = 30.0f * Walk * A * Sw;
	O.Thigh[1] = -30.0f * Walk * A * Sw;
	O.Knee[0] += (10.0f + 50.0f * A * std::max(0.0f, Cw)) * Walk * Gt.Knees;
	O.Knee[1] += (10.0f + 50.0f * A * std::max(0.0f, -Cw)) * Walk * Gt.Knees;
	O.Ankle[0] = (O.Knee[0] - O.Thigh[0]) * 0.35f * Walk;
	O.Ankle[1] = (O.Knee[1] - O.Thigh[1]) * 0.35f * Walk;
	O.ShFwd[0] = -32.0f * Walk * A * Sw * Gt.Arms;
	O.ShFwd[1] = 32.0f * Walk * A * Sw * Gt.Arms;
	O.Elbow[0] += (18.0f + 55.0f * Run) * Walk;
	O.Elbow[1] += (18.0f + 55.0f * Run) * Walk;
	O.Bob = (-3.0f * A * std::fabs(Sw) + 1.5f) * Walk * Gt.Bounce;
	O.HipTwist = 8.0f * Walk * Sw * Gt.Twist;
	O.ChestTwist = -10.0f * Walk * Sw * Gt.Twist;
	O.Lean = (4.0f * Walk + 12.0f * Run) * Gt.Lean;
	O.HeadYaw += -O.ChestTwist * 0.6f;

	// --- Personality: idle stance, walk style, fidget ---------------------------
	if (FormName == "Heatblast")
	{
		Arm(0, 10, 28, 35, Idle); Arm(1, 10, 28, 35, Idle);
		O.ShFwd[0] += std::sin(Time * 3.0f) * 6.0f * Idle; O.ShFwd[1] += std::sin(Time * 3.0f + 1.5f) * 6.0f * Idle;
		Arm(0, 100, 15, 95, Fid); Arm(1, 100, 15, 95, Fid);   // fireballs ready
	}
	else if (FormName == "Four Arms")
	{
		O.Breath = 1.0f + 0.06f * std::sin(Time * 1.6f) * Idle;
		Blend(O.ThighAbd[0], 10.0f, Idle); Blend(O.ThighAbd[1], 10.0f, Idle);
		Arm(0, 5, 26, 45, Idle); Arm(1, 5, 26, 45, Idle);
		O.HipRoll += std::sin(Ph) * 6.0f * Walk;                         // heavy stomp
		Arm(0, 0, 95, 125, Fid); Arm(1, 0, 95, 125, Fid);                // flex
	}
	else if (FormName == "XLR8")
	{
		O.Lean += 15.0f * Idle;
		Leg(0, 12, 20.0f + std::max(0.0f, std::sin(Time * 14.0f)) * 22.0f, Idle); // impatient foot tap
		Arm(0, -50, 18, 20, Run); Arm(1, -50, 18, 20, Run);                     // arms back at speed
	}
	else if (FormName == "Way Big")
	{
		O.Breath = 1.0f + 0.04f * std::sin(Time * 1.0f) * Idle;
		Arm(0, 80, -20, 90, Fid); Arm(1, 80, -20, 90, Fid);
	}
	else if (bHuman) switch (Id)
	{
	case HeroId::Jimmy:
		Arm(0, 0, 32, 95, Idle * 0.7f);   // hand on hip, the other free
		O.HeadPitch += 6.0f * Run;        // head-first genius sprint
		if (FidT < 0.6f) { Arm(1, 95, -18, 135, Fid); Blend(O.HeadPitch, -10.0f, Fid); Blend(O.HeadYaw, 12.0f, Fid); } // thinking...
		else { Arm(1, 165, 8, 10, Fid); Blend(O.HeadPitch, -14.0f, Fid); O.Bob += 2.0f * Fid; }                 // brain blast!
		break;
	case HeroId::Timmy:
		O.Bob += std::fabs(std::sin(Time * 4.5f)) * 2.5f * Idle;     // can't stand still
		O.HeadYaw = std::sin(Time * 0.9f) * 26.0f * Idle + O.HeadYaw * Walk;
		O.ShFwd[0] += std::sin(Time * 2.2f) * 10.0f * Idle;
		O.ShFwd[1] += std::sin(Time * 2.2f + Pi) * 10.0f * Idle;
		O.HeadRoll = std::sin(Ph) * 6.0f * Walk;                      // goofy head bob
		Arm(1, 160, 20.0f + std::sin(Time * 10.0f) * 25.0f, 25, Fid);  // wand flourish
		Blend(O.HeadPitch, -10.0f, Fid);
		break;
	case HeroId::Danny:
		O.ShiftX += 3.0f * Idle; O.HipRoll += 5.0f * Idle;            // weight on one leg
		Blend(O.ThighAbd[0], 6.0f, Idle); Blend(O.Knee[1], 14.0f, Idle);
		O.HeadPitch += 4.0f;                                         // chin down, cool
		Arm(0, 65, -38, 115, Fid); Arm(1, 65, -38, 115, Fid);         // arms crossed
		Blend(O.HeadYaw, -15.0f, Fid);
		break;
	case HeroId::Jake:
	{
		const float Beat = Time * 4.0f;                              // hip-hop bounce
		O.Bob += -std::fabs(std::sin(Beat)) * 3.0f * Idle;
		O.HeadPitch += std::sin(Beat * 2.0f) * 5.0f * Idle;
		O.ChestTwist += std::sin(Time * 2.0f) * 8.0f * Idle;
		O.Knee[0] += (8.0f + std::fabs(std::sin(Beat)) * 10.0f) * Idle;
		O.Knee[1] += (8.0f + std::fabs(std::sin(Beat)) * 10.0f) * Idle;
		O.ShAbd[0] += 10.0f * Walk; O.ShAbd[1] += 10.0f * Walk;      // swagger
		O.HeadRoll = std::sin(Ph) * 5.0f * Walk;
		Arm(1, 175, 10, 40.0f + std::sin(Time * 14.0f) * 15.0f, Fid); // fist pump
		Arm(0, 60, -10, 110, Fid);
		break;
	}
	case HeroId::XJ9:
		O.Breath = 1.0f;                                             // robots don't breathe
		O.ShiftX *= 1.0f - Idle; O.HipRoll *= 1.0f - Idle;
		O.HeadYaw = std::round(std::sin(Time * 0.45f) * 2.0f) * 15.0f * Idle; // snappy servo head
		Arm(0, -20, 42, 95, Idle); Arm(1, -20, 42, 95, Idle);         // hands on hips
		O.Elbow[0] = O.Elbow[0] * (1.0f - Walk) + 5.0f * Walk;        // locked elbows on the march
		O.Elbow[1] = O.Elbow[1] * (1.0f - Walk) + 5.0f * Walk;
		O.Bob += std::sin(Ph * 2.0f) * 1.0f * Walk;
		Arm(1, 10, 150, 30.0f + std::sin(Time * 12.0f) * 28.0f, Fid); // friendly wave
		Blend(O.HeadRoll, 15.0f, Fid);
		break;
	case HeroId::ElTigre:
		// Fighting crouch, claws up.
		Leg(0, 22, 38, Idle); Leg(1, 22, 38, Idle);
		Blend(O.ThighAbd[0], 8.0f, Idle); Blend(O.ThighAbd[1], 8.0f, Idle);
		O.Ankle[0] += 14.0f * Idle; O.Ankle[1] += 14.0f * Idle;
		O.Bob -= 6.0f * Idle; O.Lean += 10.0f * Idle;
		Arm(0, 55, 22, 95, Idle); Arm(1, 55, 22, 95, Idle);
		O.ShiftX += std::sin(Time * 2.5f) * 3.0f * Idle;
		O.Knee[0] += 15.0f * Walk; O.Knee[1] += 15.0f * Walk;         // prowl
		Arm(0, -55, 15, 20, Run); Arm(1, -55, 15, 20, Run);           // cat dash
		O.Lean += 22.0f * Run;
		Arm(0, 30, 85, 70.0f + std::sin(Time * 20.0f) * 8.0f, Fid);   // claw flex
		Arm(1, 30, 85, 70.0f + std::sin(Time * 20.0f) * 8.0f, Fid);
		Blend(O.HeadPitch, -12.0f, Fid);
		break;
	case HeroId::Ben:
		Arm(0, 0, 6, 20, Idle * 0.5f); Arm(1, 0, 6, 20, Idle * 0.5f);
		// Checks the Omnitrix, then taps it.
		Arm(0, 75, -25, 105, Fid);
		Blend(O.HeadPitch, 18.0f, Fid); Blend(O.HeadYaw, 20.0f, Fid);
		if (FidT > 0.45f) Arm(1, 60.0f + std::sin(Time * 12.0f) * 6.0f, -30, 95, Fid);
		break;
	case HeroId::Aang:
		O.Bob += (2.0f + std::sin(Time * 2.0f) * 2.0f) * Idle;        // light as air
		Arm(1, 12, 22, 40, Idle);                                     // staff planted
		Arm(0, -40.0f * Run, 15, 10, Run); Arm(1, -55.0f * Run, 15, 10, Run); // airbender run
		O.Lean += 14.0f * Run;
		Arm(1, 80, 10, 30, Fid);                                      // staff twirl
		StaffSpin = Fid > 0.05f ? Time * 720.0f : 0.0f;
		break;
	default:
		break;
	}

	// --- Airborne ------------------------------------------------------------------
	if (bAir)
	{
		const float Up = P.Vel.z > 0.0f ? 1.0f : 0.7f;
		O.Thigh[0] = 50.0f; O.Thigh[1] = 18.0f;
		O.Knee[0] = 80.0f; O.Knee[1] = 35.0f;
		for (int s = 0; s < 2; ++s) { O.ShAbd[s] = 55.0f * Up; O.ShFwd[s] = -15.0f; O.Elbow[s] = 35.0f; }
		O.Lean = 6.0f;
		if (Is(HeroId::Timmy) && P.Vel.z < 0.0f)
		{
			// Flailing on the way down.
			O.ShAbd[0] = 120.0f + std::sin(Time * 16.0f) * 30.0f; O.ShAbd[1] = 120.0f + std::cos(Time * 16.0f) * 30.0f;
			O.Thigh[0] = 30.0f + std::sin(Time * 14.0f) * 25.0f; O.Thigh[1] = 30.0f - std::sin(Time * 14.0f) * 25.0f;
		}
		else if (Is(HeroId::XJ9))
		{
			O.Thigh[0] = O.Thigh[1] = 5.0f; O.Knee[0] = O.Knee[1] = 5.0f;  // stiff and straight
		}
		else if (Is(HeroId::Danny) || Is(HeroId::Aang))
		{
			O.Thigh[1] = 0.0f; O.Knee[1] = 15.0f; O.ShAbd[0] = O.ShAbd[1] = 25.0f;
		}
		else if (Is(HeroId::Jimmy) && P.SigCd > 0.2f)
		{
			// Goddard jetpack: dangling legs, arms out.
			O.Thigh[0] = 6.0f; O.Thigh[1] = -4.0f; O.Knee[0] = O.Knee[1] = 12.0f;
			Arm(0, 10, 38, 20, 1.0f); Arm(1, 10, 38, 20, 1.0f);
			O.HeadPitch = -10.0f;
		}
		if (P.FlipTime > 0.0f)
		{
			// Air-jump flip: tuck and somersault.
			const float F = 1.0f - P.FlipTime / 0.5f;
			O.BodyPitch = 360.0f * (F * F * (3.0f - 2.0f * F));
			O.Thigh[0] = O.Thigh[1] = 95.0f; O.Knee[0] = O.Knee[1] = 115.0f;
			Arm(0, 60, 20, 90, 1.0f); Arm(1, 60, 20, 90, 1.0f);
			O.Bob += std::sin(Pi * F) * 10.0f;
		}
	}
	if (P.LandTimer > 0.0f)
	{
		O.Knee[0] += 50.0f * Land; O.Knee[1] += 50.0f * Land;
		O.Thigh[0] += 30.0f * Land; O.Thigh[1] += 30.0f * Land;
		O.Bob -= 10.0f * Land;
		O.Lean += 12.0f * Land;
	}

	// --- Wall slide: facing the wall, hands (or claws) on it ---------------------
	if (P.WallSlide > 0.0f && !P.Grounded)
	{
		Arm(0, 150, 25, 40, 1.0f); Arm(1, 150, 25, 40, 1.0f);
		Leg(0, 40, 50, 1.0f); Leg(1, 20, 30, 1.0f);
		O.Lean = -5.0f;
		O.HeadPitch = -15.0f;
	}

	// --- Ground pound -----------------------------------------------------------------
	if (P.Pounding)
	{
		Leg(0, 75, 105, 1.0f); Leg(1, 75, 105, 1.0f);
		Arm(0, 170, 10, 30, 1.0f); Arm(1, 170, 10, 30, 1.0f);
		O.Lean = 8.0f;
	}
	if (P.PoundLand > 0.0f)
	{
		const float Pl = P.PoundLand / 0.35f;
		Leg(0, 70, 100, Pl); Leg(1, 40, 90, Pl);
		Arm(0, 55, 10, 10, Pl); Arm(1, 70, 5, 5, Pl);   // fist into the ground
		O.Bob -= 20.0f * Pl;
		O.Lean += 30.0f * Pl;
	}

	// --- Dodge ------------------------------------------------------------------------
	if (bDodge)
	{
		const float Dur = P.DodgeAir ? 0.32f : 0.4f;
		const float D = std::clamp(1.0f - P.DodgeTime / Dur, 0.0f, 1.0f);
		const float S = std::sin(Pi * D);
		const float Tuck = std::min(1.0f, S * 1.8f);
		if (Is(HeroId::Danny))
		{
			// Ghost dash: stretched and streaking.
			O.Lean = 35.0f; Arm(0, -40, 15, 10, 1.0f); Arm(1, -40, 15, 10, 1.0f);
			Leg(0, -20, 40, 1.0f); Leg(1, -10, 30, 1.0f);
		}
		else if (Is(HeroId::XJ9))
		{
			// Thruster side-step: upright, legs together, arms out.
			O.Lean = 15.0f; Arm(0, 0, 40, 0, 1.0f); Arm(1, 0, 40, 0, 1.0f);
			Leg(0, 0, 0, 1.0f); Leg(1, 0, 0, 1.0f);
		}
		else if (Is(HeroId::Aang))
		{
			// Air swirl spin.
			O.Spin = 360.0f * D; Arm(0, 10, 80, 10, 1.0f); Arm(1, 10, 80, 10, 1.0f);
			Leg(0, 50, 80, 1.0f); Leg(1, 0, 10, 1.0f);
			O.Bob += 12.0f * S;
		}
		else if (Is(HeroId::ElTigre))
		{
			// Pounce, claws first.
			O.Lean = 50.0f * S; Arm(0, 150, 15, 20, 1.0f); Arm(1, 150, 15, 20, 1.0f);
			Leg(0, -30, 50, 1.0f); Leg(1, -15, 40, 1.0f);
			O.Bob += 10.0f * S;
		}
		else if (FormName == "Four Arms")
		{
			// Shoulder charge.
			O.Lean = 30.0f; O.ChestTwist = 30.0f; Arm(0, 60, 10, 110, 1.0f); Arm(1, 60, 10, 110, 1.0f);
			O.Thigh[0] = 40.0f * Sw; O.Thigh[1] = -40.0f * Sw;
		}
		else if (FormName == "XLR8")
		{
			O.Lean = 55.0f; Arm(0, -60, 15, 15, 1.0f); Arm(1, -60, 15, 15, 1.0f);
			O.Thigh[0] = 50.0f * std::sin(Time * 40.0f); O.Thigh[1] = -O.Thigh[0];
		}
		else
		{
			// Forward roll (or an air somersault): tuck tight, rotate about the hips.
			O.BodyPitch = 360.0f * D;
			Leg(0, 100, 115, Tuck); Leg(1, 100, 115, Tuck);
			Arm(0, 70, 15, 100, Tuck); Arm(1, 70, 15, 100, Tuck);
			if (!P.DodgeAir) O.Bob -= B.Leg * 0.45f * S;
		}
	}

	// --- Flight -------------------------------------------------------------------------
	if (P.Flying)
	{
		const float Fwd = std::clamp(Speed3 / 1500.0f, 0.0f, 1.0f);
		const float Hover = std::sin(Time * 3.0f);
		const float Travel = std::clamp(80.0f - P.FlyPitch * 0.9f, 0.0f, 170.0f);
		const float Roll = P.FlyBank + (P.DodgeTime > 0.0f && P.DodgeAir ? 360.0f * (1.0f - P.DodgeTime / 0.45f) * P.DodgeSide : 0.0f);
		const float Lift = P.TakeoffTime / 0.45f;
		O.Lean = 0.0f;
		O.BodyRoll = Roll;
		if (Is(HeroId::Timmy))
		{
			// Surfing the cloud-board: sideways stance, knees bent, arms out for balance.
			O.Spin = -65.0f;
			O.HeadYaw = 55.0f;
			O.BodyPitch = 14.0f * Fwd;
			O.BodyRoll = P.FlyBank * 0.8f;
			Leg(0, 25, 40, 1.0f); Leg(1, 25, 40, 1.0f);
			O.ThighAbd[0] = O.ThighAbd[1] = 16.0f;
			Arm(0, 10, 75.0f + std::sin(Time * 1.5f) * 10.0f - P.FlyBank * 0.6f, 20, 1.0f);
			Arm(1, 10, 75.0f - std::sin(Time * 1.5f) * 10.0f + P.FlyBank * 0.6f, 20, 1.0f);
			O.Bob += Hover * 2.0f;
		}
		else if (Is(HeroId::XJ9))
		{
			// Rocket pose: arms at the sides, legs locked together.
			O.BodyPitch = Travel * Fwd;
			O.HeadPitch = -O.BodyPitch * 0.55f;
			Arm(0, -20.0f * Fwd + 10.0f * (1.0f - Fwd), 14.0f + 22.0f * (1.0f - Fwd), 5, 1.0f);
			Arm(1, -20.0f * Fwd + 10.0f * (1.0f - Fwd), 14.0f + 22.0f * (1.0f - Fwd), 5, 1.0f);
			Leg(0, 0, 2, 1.0f); Leg(1, 0, 2, 1.0f);
			O.ThighAbd[0] = O.ThighAbd[1] = -2.0f;
			O.Bob += Hover * 3.0f * (1.0f - Fwd);
			if (Lift > 0.0f) Arm(1, 175, 5, 5, Lift);
		}
		else
		{
			// Danny: superhero flight. One fist leads; both when boosting; relaxed hover when slow.
			O.BodyPitch = Travel * Fwd;
			O.HeadPitch = -O.BodyPitch * 0.6f;
			Leg(0, -8.0f + Hover * 6.0f, 25, 1.0f); Leg(1, -2.0f - Hover * 6.0f, 15, 1.0f);
			Arm(1, 40.0f + 130.0f * Fwd, 8, 5, 1.0f);
			Arm(0, -25, 14, 20, 1.0f);
			if (P.Boosting) Arm(0, 40.0f + 130.0f * Fwd, 8, 5, 1.0f);
			Arm(0, 10, 30, 30, 1.0f - Fwd); Arm(1, 10, 30, 30, 1.0f - Fwd);
			O.Bob += Hover * 4.0f * (1.0f - Fwd);
			if (Lift > 0.0f) Arm(1, 178, 5, 5, Lift);
		}
	}
	if (P.Gliding)
	{
		const float Fwd = std::clamp(Speed3 / 1200.0f, 0.0f, 1.0f);
		if (FormName == "Heatblast")
		{
			// Rides jets of fire from his hands.
			Arm(0, -10, 22, 0, 1.0f); Arm(1, -10, 22, 0, 1.0f);
			Leg(0, 10, 20, 1.0f); Leg(1, -5, 10, 1.0f);
			O.BodyRoll = P.FlyBank * 0.5f;
		}
		else
		{
			// Aang lies prone under the glider.
			O.BodyPitch = (70.0f - P.FlyPitch * 0.5f) * Fwd;
			O.BodyRoll = P.FlyBank;
			O.HeadPitch = -O.BodyPitch * 0.6f;
			Arm(0, 170, 18, 15, 1.0f); Arm(1, 170, 18, 15, 1.0f);
			Leg(0, -6, 10, 1.0f); Leg(1, 4, 15, 1.0f);
		}
	}

	// --- Attacks: each hero, each combo hit ------------------------------------------------
	if (Atk > 0.0f)
	{
		const int S = Step % 2 == 0 ? 1 : 0;          // alternate sides
		const float Sd = S == 1 ? 1.0f : -1.0f;
		const bool bFin = Step == 2;
		const float Sweep = 1.0f - 2.0f * AtkA;       // +1 -> -1 across the swing
		if (FormName == "Four Arms")
		{
			if (bFin) { const float V = 170.0f - 120.0f * AtkA; Arm(0, V, 10, 20, Atk); Arm(1, V, 10, 20, Atk); O.Lean += 20.0f * Atk; }
			else { Arm(S, 95, 0, 8, Atk); O.ChestTwist += Sd * 28.0f * Atk; O.Lean += 8.0f * Atk; }
		}
		else if (FormName == "Heatblast")
		{
			const float Throw = AtkA < 0.3f ? -40.0f * (AtkA / 0.3f) : 140.0f * std::sin(Pi * (AtkA - 0.3f) / 0.7f);
			Arm(S, Throw, 10, 30, 1.0f);
			if (bFin) Arm(1 - S, Throw, 10, 30, 1.0f);
			O.ChestTwist += Sd * 20.0f * Atk;
		}
		else if (FormName == "XLR8")
		{
			const int Q = static_cast<int>(AtkA * 4.0f) % 2;
			Arm(Q, 90.0f * std::sin(Pi * std::fmod(AtkA * 4.0f, 1.0f)), 0, 5, 1.0f);
			O.Lean += 15.0f * Atk;
		}
		else if (FormName == "Way Big")
		{
			Leg(1, 70.0f * Atk, 60.0f * Atk, 1.0f);
			Arm(0, 30, 40, 20, Atk); Arm(1, 30, 40, 20, Atk);
		}
		else if (bHuman) switch (Id)
		{
		case HeroId::Jimmy:
		{
			const float Aim = std::min(1.0f, Atk * 2.0f);
			const float Recoil = AtkA < 0.3f ? std::sin(Pi * AtkA / 0.3f) : 0.0f;
			Arm(1, 88.0f + 15.0f * Recoil, 2, 4.0f + 20.0f * Recoil, Aim);
			O.ChestTwist += 18.0f * Aim;
			if (bFin) { Arm(0, 80, -30, 45, Aim); O.Lean -= 10.0f * Recoil; }
			break;
		}
		case HeroId::Timmy:
			if (bFin) { Arm(1, 170, std::sin(AtkA * 2.0f * Pi) * 40.0f, 20, Atk); O.Bob += 6.0f * Atk; }
			else { Arm(1, 120, (Step == 0 ? 70.0f - 100.0f * AtkA : -30.0f + 100.0f * AtkA), 25, Atk); }
			break;
		case HeroId::Danny:
			if (bFin) { Arm(0, 92, 0, 5, Atk); Arm(1, 92, 0, 5, Atk); O.Lean += 10.0f * Atk; }
			else { Arm(S, 92, 0, 5, Atk); O.ChestTwist += Sd * 22.0f * Atk; }
			break;
		case HeroId::Jake:
			if (bFin) { const float V = (170.0f - 130.0f * AtkA); Arm(0, V, 15, 15, std::min(1.0f, Atk * 2.0f)); Arm(1, V, 15, 15, std::min(1.0f, Atk * 2.0f)); O.Lean += 18.0f * Atk; }
			else { Arm(S, 80, 70.0f * Sweep, 15, Atk); O.ChestTwist += Sd * 30.0f * Sweep * Atk; O.Lean += 8.0f * Atk; }
			break;
		case HeroId::ElTigre:
			if (bFin) { O.Spin = 360.0f * AtkA; Arm(0, 20, 90, 10, Atk); Arm(1, 20, 90, 10, Atk); }
			else { Arm(S, 70, 60.0f * Sweep, 20, Atk); O.ChestTwist += Sd * 30.0f * Sweep * Atk; O.Lean += 12.0f * Atk; }
			break;
		case HeroId::XJ9:
			Arm(1, 90, 2.0f + std::sin(Time * 70.0f) * 2.0f, 0, std::min(1.0f, Atk * 3.0f));
			O.ChestTwist += 20.0f * Atk;
			break;
		case HeroId::Ben:
			if (bFin) { Leg(1, 95, 15, Atk); O.Lean -= 12.0f * Atk; Arm(0, 10, 40, 30, Atk); Arm(1, 10, 40, 30, Atk); }
			else { Arm(S, 95, 0, 5, Atk); O.ChestTwist += Sd * 25.0f * Atk; O.Lean += 8.0f * Atk; }
			break;
		case HeroId::Aang:
			if (bFin) { O.Spin = 360.0f * AtkA; Arm(0, 30, 70, 10, Atk); Arm(1, 30, 70, 10, Atk); StaffSpin = 720.0f * AtkA; }
			else { Arm(S, 90, -5, 5, Atk); O.ChestTwist += Sd * 20.0f * Atk; O.Lean += 6.0f * Atk; }
			break;
		default:
			break;
		}
	}
	if (P.BreathTime > 0.0f) O.HeadPitch -= 12.0f;

	// --- Emote: the hero's signature pose ----------------------------------------------------
	if (EmW > 0.0f)
	{
		const float W = EmW;
		if (FormName == "Four Arms") { Arm(0, 0, 95, 130, W); Arm(1, 0, 95, 130, W); Blend(O.HeadPitch, -10.0f, W); }
		else if (FormName == "Heatblast") { Arm(0, 10, 140, 30, W); Arm(1, 10, 140, 30, W); }
		else if (FormName == "XLR8") { Arm(1, 70, 10, 10, W); Blend(O.Lean, 20.0f, W); }
		else if (FormName == "Way Big") { Arm(0, 80, -20, 90, W); Arm(1, 80, -20, 90, W); }
		else if (bHuman) switch (Id)
		{
		case HeroId::Jimmy:
			Arm(1, 170, -5, 10, W); Arm(0, 0, 35, 100, W);
			Blend(O.HeadPitch, -15.0f, W); Blend(O.HeadRoll, 8.0f, W); O.Bob += 2.0f * W;
			break;
		case HeroId::Timmy:
			Arm(0, 10, 150.0f + std::sin(Time * 14.0f) * 12.0f, 20, W); Arm(1, 10, 150.0f + std::cos(Time * 14.0f) * 12.0f, 20, W);
			O.Bob += std::fabs(std::sin(Time * 7.0f)) * 9.0f * W;
			break;
		case HeroId::Danny:
			if (Em < 0.45f) { Arm(0, 70, -40, 60, W); Arm(1, 70, -40, 60, W); Blend(O.HeadPitch, 10.0f, W); }
			else { Arm(0, 10, 110, 5, W); Arm(1, 10, 110, 5, W); Blend(O.HeadPitch, -10.0f, W); O.Bob += 3.0f * W; }
			break;
		case HeroId::Jake:
			Arm(1, 175, 10, 25.0f + std::sin(Time * 12.0f) * 15.0f, W); Arm(0, 0, 35, 100, W);
			Leg(0, 15, 20, W); Leg(1, 15, 20, W); Blend(O.HeadPitch, -10.0f, W);
			break;
		case HeroId::XJ9:
			Arm(1, 10, 150, 35.0f + std::sin(Time * 12.0f) * 28.0f, W); Arm(0, -20, 42, 95, W);
			Blend(O.HeadRoll, 15.0f, W);
			break;
		case HeroId::ElTigre:
			Arm(0, 30, 120, 70, W); Arm(1, 30, 120, 70, W);
			Leg(0, 30, 40, W); Leg(1, 30, 40, W); Blend(O.ThighAbd[0], 15.0f, W); Blend(O.ThighAbd[1], 15.0f, W);
			Blend(O.HeadPitch, -18.0f, W);
			break;
		case HeroId::Ben:
			Arm(0, 80, -30, 100, W);
			if (Em < 0.5f) Arm(1, 110, -10, 100, W); else Arm(1, 70, -35, 95, W);
			Blend(O.HeadPitch, 15.0f, W);
			break;
		case HeroId::Aang:
		{
			const float Bow = std::sin(Pi * std::clamp((Em - 0.1f) / 0.6f, 0.0f, 1.0f));
			Blend(O.Lean, 45.0f * Bow, W); Arm(0, 75, -32, 110, W); Arm(1, 75, -32, 110, W);
			Blend(O.HeadPitch, 15.0f, W);
			if (Em > 0.7f) StaffSpin = Time * 900.0f;
			break;
		}
		default:
			break;
		}
	}
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
	const Matrix Pelvis = Mul(Mul(Mul(Mul(Ry(O.HipTwist + O.Spin), Rz(O.HipRoll)), Rx(O.Lean)),
	                      Mul(Mul(Rx(O.BodyPitch), Rz(O.BodyRoll)), T(O.ShiftX, HipY, 0))), Root);
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
		if (Atk > 0.0f) Pt.Ball(Hand[Step % 2 == 0 ? 1 : 0], { 0, -10, 0 }, { 30, 30, 30 }, Color{ 255, 170, 40, 255 }, 1.0f);
		if (P.Gliding)
		{
			for (int s = 0; s < 2; ++s)
			{
				Pt.Cone(Hand[s], { 0, -30 - std::sin(Time * 30.0f + s) * 4.0f, 0 }, { 18, 50, 18 }, Color{ 255, 150, 30, 255 }, 1.0f, { 180, 0, 0 });
			}
		}
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
			if (P.Flying || P.DodgeTime > 0.0f) Pt.Cone(Foot[s], { 0, -14, 0 }, { 12, 26 + std::sin(Time * 40.0f) * 6.0f, 12 }, Color{ 255, 190, 60, 255 }, 1.0f, { 180, 0, 0 });
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
			Pt.Cyl(Hand[1], { 0, -6, 0 }, { 5, 160, 5 }, Brown, 0, { StaffSpin + (Atk > 0.0f && Step != 2 ? 180.0f * AtkA : 0.0f), 0, 0 });
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
