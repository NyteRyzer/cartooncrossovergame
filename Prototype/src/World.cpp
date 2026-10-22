#include "World.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace
{
	constexpr float Deg2Rad = 3.14159265358979f / 180.0f;

	bool ParseFloat(const std::string& S, float& Out)
	{
		char* End = nullptr;
		const double V = std::strtod(S.c_str(), &End);
		if (End == S.c_str() || *End != '\0' || !std::isfinite(V))
		{
			return false;
		}
		Out = static_cast<float>(V);
		return true;
	}

	Shape ParseShape(const std::string& S)
	{
		if (S == "cylinder") return Shape::Cylinder;
		if (S == "sphere") return Shape::Sphere;
		if (S == "cone") return Shape::Cone;
		if (S == "plane") return Shape::Plane;
		return Shape::Cube;
	}

	void FinalizeBlock(Block& B)
	{
		B.Half = Vector3{ B.Size.x * 0.5f, B.Size.y * 0.5f, B.Size.z * 0.5f };
		B.CosYaw = std::cos(B.YawDeg * Deg2Rad);
		B.SinYaw = std::sin(B.YawDeg * Deg2Rad);
		B.BoundRadiusXY = std::sqrt(B.Half.x * B.Half.x + B.Half.y * B.Half.y);
		const bool Upright = std::fmod(std::fabs(B.PitchDeg), 360.0f) < 0.01f;
		B.VerticalCylinder = B.Kind == Shape::Cylinder && Upright;
		// Pitched shapes and cones are decorative (ferris wheel, platform underside).
		if (!Upright || B.Kind == Shape::Cone || B.Kind == Shape::Plane)
		{
			B.Solid = false;
		}
	}

	// World -> block-local (yaw only).
	Vector3 ToLocal(const Block& B, Vector3 P)
	{
		const float Dx = P.x - B.Center.x;
		const float Dy = P.y - B.Center.y;
		return Vector3{ Dx * B.CosYaw + Dy * B.SinYaw, -Dx * B.SinYaw + Dy * B.CosYaw, P.z - B.Center.z };
	}

	Vector3 DirToWorld(const Block& B, Vector3 D)
	{
		return Vector3{ D.x * B.CosYaw - D.y * B.SinYaw, D.x * B.SinYaw + D.y * B.CosYaw, D.z };
	}

	Vector3 DirToLocal(const Block& B, Vector3 D)
	{
		return Vector3{ D.x * B.CosYaw + D.y * B.SinYaw, -D.x * B.SinYaw + D.y * B.CosYaw, D.z };
	}

	bool Blocks(const Block& B, CollisionMode Mode)
	{
		if (!B.Solid)
		{
			return false;
		}
		return Mode == CollisionMode::Solid || B.PhaseBlocker;
	}

	// Penetration of a vertical cylinder into one block. Returns false if none.
	bool Penetration(const Block& B, Vector3 P, float R, float H, float StepUp, Vector3& Push)
	{
		const float Dz = P.z - B.Center.z;
		const float OverlapZ = (H + B.Half.z) - std::fabs(Dz);
		if (OverlapZ <= 0.0f)
		{
			return false;
		}
		const float Dx = P.x - B.Center.x;
		const float Dy = P.y - B.Center.y;
		if (Dx * Dx + Dy * Dy > (B.BoundRadiusXY + R) * (B.BoundRadiusXY + R))
		{
			return false;
		}

		float PenH = 0.0f;
		Vector3 DirH{};
		if (B.VerticalCylinder)
		{
			const float BR = std::max(B.Half.x, B.Half.y);
			const float D = std::sqrt(Dx * Dx + Dy * Dy);
			if (D >= BR + R)
			{
				return false;
			}
			PenH = BR + R - D;
			DirH = D > 1e-3f ? Vector3{ Dx / D, Dy / D, 0.0f } : Vector3{ 1.0f, 0.0f, 0.0f };
		}
		else
		{
			const Vector3 Local = ToLocal(B, P);
			const float Qx = std::clamp(Local.x, -B.Half.x, B.Half.x);
			const float Qy = std::clamp(Local.y, -B.Half.y, B.Half.y);
			const float Ex = Local.x - Qx;
			const float Ey = Local.y - Qy;
			const float D2 = Ex * Ex + Ey * Ey;
			if (D2 >= R * R)
			{
				return false;
			}
			Vector3 LocalDir{};
			if (D2 > 1e-6f)
			{
				const float D = std::sqrt(D2);
				PenH = R - D;
				LocalDir = Vector3{ Ex / D, Ey / D, 0.0f };
			}
			else
			{
				const float PenX = B.Half.x - std::fabs(Local.x) + R;
				const float PenY = B.Half.y - std::fabs(Local.y) + R;
				if (PenX < PenY)
				{
					PenH = PenX;
					LocalDir = Vector3{ Local.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f };
				}
				else
				{
					PenH = PenY;
					LocalDir = Vector3{ 0.0f, Local.y >= 0.0f ? 1.0f : -1.0f, 0.0f };
				}
			}
			DirH = DirToWorld(B, LocalDir);
		}

		const bool Above = Dz > 0.0f;
		// Land on / step onto tops; otherwise take the shallowest way out.
		if ((Above && OverlapZ <= StepUp) || OverlapZ < PenH)
		{
			Push = Vector3{ 0.0f, 0.0f, Above ? OverlapZ : -OverlapZ };
		}
		else
		{
			Push = Vector3{ DirH.x * PenH, DirH.y * PenH, 0.0f };
		}
		return true;
	}

	bool RaySlab(float O, float D, float Min, float Max, float& TNear, float& TFar, int Axis, int& NearAxis, float& NearSign)
	{
		if (std::fabs(D) < 1e-8f)
		{
			return O >= Min && O <= Max;
		}
		float T1 = (Min - O) / D;
		float T2 = (Max - O) / D;
		float Sign = -1.0f;
		if (T1 > T2)
		{
			std::swap(T1, T2);
			Sign = 1.0f;
		}
		if (T1 > TNear)
		{
			TNear = T1;
			NearAxis = Axis;
			NearSign = Sign;
		}
		TFar = std::min(TFar, T2);
		return TNear <= TFar;
	}

	bool RayBox(const Block& B, Vector3 O, Vector3 D, float MaxT, float& OutT, Vector3& OutN)
	{
		const Vector3 Lo = ToLocal(B, O);
		const Vector3 Ld = DirToLocal(B, D);
		float TNear = -1e30f, TFar = 1e30f;
		int Axis = -1;
		float Sign = 0.0f;
		if (!RaySlab(Lo.x, Ld.x, -B.Half.x, B.Half.x, TNear, TFar, 0, Axis, Sign)) return false;
		if (!RaySlab(Lo.y, Ld.y, -B.Half.y, B.Half.y, TNear, TFar, 1, Axis, Sign)) return false;
		if (!RaySlab(Lo.z, Ld.z, -B.Half.z, B.Half.z, TNear, TFar, 2, Axis, Sign)) return false;
		if (TFar < 0.0f || TNear > MaxT)
		{
			return false;
		}
		if (TNear < 0.0f)
		{
			// Origin inside the box: report an immediate hit.
			OutT = 0.0f;
			OutN = Vector3{ -D.x, -D.y, -D.z };
			return true;
		}
		Vector3 LocalN{};
		if (Axis == 0) LocalN.x = Sign;
		if (Axis == 1) LocalN.y = Sign;
		if (Axis == 2) LocalN.z = Sign;
		OutT = TNear;
		OutN = DirToWorld(B, LocalN);
		return true;
	}

	bool RayCylinder(const Block& B, Vector3 O, Vector3 D, float MaxT, float& OutT, Vector3& OutN)
	{
		const float R = std::max(B.Half.x, B.Half.y);
		const float ZMin = B.Center.z - B.Half.z;
		const float ZMax = B.Center.z + B.Half.z;
		float Best = 1e30f;
		Vector3 BestN{};

		const float Ox = O.x - B.Center.x;
		const float Oy = O.y - B.Center.y;
		if (Ox * Ox + Oy * Oy <= R * R && O.z >= ZMin && O.z <= ZMax)
		{
			OutT = 0.0f;
			OutN = Vector3{ -D.x, -D.y, -D.z };
			return true;
		}

		// Caps.
		if (std::fabs(D.z) > 1e-8f)
		{
			for (int i = 0; i < 2; ++i)
			{
				const float Z = i == 0 ? ZMax : ZMin;
				const float T = (Z - O.z) / D.z;
				if (T >= 0.0f && T < Best)
				{
					const float Px = Ox + D.x * T;
					const float Py = Oy + D.y * T;
					if (Px * Px + Py * Py <= R * R)
					{
						Best = T;
						BestN = Vector3{ 0.0f, 0.0f, i == 0 ? 1.0f : -1.0f };
					}
				}
			}
		}
		// Side.
		const float A = D.x * D.x + D.y * D.y;
		if (A > 1e-8f)
		{
			const float Bq = 2.0f * (Ox * D.x + Oy * D.y);
			const float C = Ox * Ox + Oy * Oy - R * R;
			const float Disc = Bq * Bq - 4.0f * A * C;
			if (Disc >= 0.0f)
			{
				const float T = (-Bq - std::sqrt(Disc)) / (2.0f * A);
				const float Z = O.z + D.z * T;
				if (T >= 0.0f && T < Best && Z >= ZMin && Z <= ZMax)
				{
					Best = T;
					const float Px = Ox + D.x * T;
					const float Py = Oy + D.y * T;
					const float Len = std::sqrt(Px * Px + Py * Py);
					BestN = Vector3{ Px / Len, Py / Len, 0.0f };
				}
			}
		}
		if (Best > MaxT)
		{
			return false;
		}
		OutT = Best;
		OutN = BestN;
		return true;
	}
}

const Block* Level::FindBlock(const std::string& Label) const
{
	const int Index = FindBlockIndex(Label);
	return Index >= 0 ? &Blocks[Index] : nullptr;
}

int Level::FindBlockIndex(const std::string& Label) const
{
	for (size_t i = 0; i < Blocks.size(); ++i)
	{
		if (Blocks[i].Label == Label)
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}

bool LoadLevel(const std::string& Path, Level& Out, std::string& Error)
{
	std::ifstream File(Path);
	if (!File)
	{
		Error = "cannot open " + Path;
		return false;
	}
	Out = Level{};
	const size_t Slash = Path.find_last_of("/\\");
	Out.FileStem = Path.substr(Slash == std::string::npos ? 0 : Slash + 1);
	Out.FileStem = Out.FileStem.substr(0, Out.FileStem.find('.'));

	std::string Line;
	int LineNo = 0;
	while (std::getline(File, Line))
	{
		++LineNo;
		std::istringstream In(Line);
		std::vector<std::string> T;
		for (std::string Tok; In >> Tok;)
		{
			T.push_back(Tok);
		}
		if (T.empty())
		{
			continue;
		}
		std::vector<float> F(T.size(), 0.0f);
		auto Nums = [&](size_t From, size_t Count) {
			if (T.size() < From + Count) return false;
			for (size_t i = From; i < From + Count; ++i)
			{
				if (!ParseFloat(T[i], F[i])) return false;
			}
			return true;
		};
		bool Ok = true;
		if (T[0] == "level" && T.size() >= 2)
		{
			Out.Name = T[1];
			std::replace(Out.Name.begin(), Out.Name.end(), '_', ' ');
		}
		else if (T[0] == "bounds" && (Ok = Nums(1, 6)))
		{
			Out.BoundsMin = Vector3{ F[1], F[2], F[3] };
			Out.BoundsMax = Vector3{ F[4], F[5], F[6] };
		}
		else if (T[0] == "start" && (Ok = Nums(1, 4)))
		{
			Out.Start = Vector3{ F[1], F[2], F[3] };
			Out.StartYaw = F[4];
		}
		else if (T[0] == "block" && (Ok = (T.size() >= 14 && Nums(3, 10))))
		{
			Block B;
			B.Label = T[1];
			B.Kind = ParseShape(T[2]);
			B.Center = Vector3{ F[3], F[4], F[5] };
			B.Size = Vector3{ F[6], F[7], F[8] };
			B.YawDeg = F[9];
			B.PitchDeg = F[10];
			B.Solid = F[11] != 0.0f;
			B.PhaseBlocker = F[12] != 0.0f;
			B.Folder = T[13];
			FinalizeBlock(B);
			Out.Blocks.push_back(B);
		}
		else if (T[0] == "marker" && (Ok = (T.size() >= 7 && Nums(3, 4))))
		{
			Marker M;
			M.Label = T[1];
			M.Class = T[2];
			M.Location = Vector3{ F[3], F[4], F[5] };
			M.YawDeg = F[6];
			Out.Markers.push_back(M);
		}
		if (!Ok)
		{
			Error = Path + ":" + std::to_string(LineNo) + ": malformed line";
			return false;
		}
	}
	if (Out.Blocks.empty())
	{
		Error = Path + ": no blocks";
		return false;
	}
	return true;
}

CollisionResult ResolveCylinder(const Level& L, Vector3& P, float R, float H, CollisionMode Mode, float StepUp)
{
	CollisionResult Result;
	for (int Iter = 0; Iter < 4; ++Iter)
	{
		bool Any = false;
		for (const Block& B : L.Blocks)
		{
			if (!Blocks(B, Mode))
			{
				continue;
			}
			Vector3 Push{};
			if (!Penetration(B, P, R, H, StepUp, Push))
			{
				continue;
			}
			Any = true;
			P.x += Push.x;
			P.y += Push.y;
			P.z += Push.z;
			if (Push.z > 0.0f)
			{
				Result.Grounded = true;
			}
			else if (Push.z < 0.0f)
			{
				Result.HitCeiling = true;
			}
			else
			{
				Result.HitWall = true;
				const float Len = std::sqrt(Push.x * Push.x + Push.y * Push.y);
				if (Len > 1e-4f)
				{
					Result.WallNormal = Vector3{ Push.x / Len, Push.y / Len, 0.0f };
				}
			}
		}
		if (!Any)
		{
			break;
		}
	}
	return Result;
}

bool OverlapsSolid(const Level& L, Vector3 P, float R, float H)
{
	for (const Block& B : L.Blocks)
	{
		Vector3 Push{};
		if (B.Solid && Penetration(B, P, R, H, 0.0f, Push))
		{
			return true;
		}
	}
	return false;
}

RayHit Raycast(const Level& L, Vector3 Origin, Vector3 Dir, float MaxDistance, bool IncludePhaseBlockersOnly)
{
	RayHit Best;
	Best.Distance = MaxDistance;
	for (size_t i = 0; i < L.Blocks.size(); ++i)
	{
		const Block& B = L.Blocks[i];
		if (!B.Solid || (IncludePhaseBlockersOnly && !B.PhaseBlocker))
		{
			continue;
		}
		float T = 0.0f;
		Vector3 N{};
		const bool Hit = B.VerticalCylinder ? RayCylinder(B, Origin, Dir, Best.Distance, T, N)
		                                    : RayBox(B, Origin, Dir, Best.Distance, T, N);
		if (Hit && T <= Best.Distance)
		{
			Best.Hit = true;
			Best.Distance = T;
			Best.Normal = N;
			Best.BlockIndex = static_cast<int>(i);
		}
	}
	Best.Point = Vector3{ Origin.x + Dir.x * Best.Distance, Origin.y + Dir.y * Best.Distance, Origin.z + Dir.z * Best.Distance };
	return Best;
}

bool InsideBlockBox(const Block& B, Vector3 P, Vector3 Margin)
{
	const Vector3 Local = ToLocal(B, P);
	return std::fabs(Local.x) <= B.Half.x + Margin.x && std::fabs(Local.y) <= B.Half.y + Margin.y
		&& std::fabs(Local.z) <= B.Half.z + Margin.z;
}
