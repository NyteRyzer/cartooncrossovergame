#include "Game.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

const HeroDef& GetHeroDef(HeroId Id);
const FormDef* GetFormDef(int FormId);
const char* OmnitrixStateName(Omnitrix::EState S);

namespace
{
	void Bar(int X, int Y, int W, int H, float T, Color Fill, const char* Label)
	{
		DrawRectangle(X - 2, Y - 2, W + 4, H + 4, Color{ 0, 0, 0, 170 });
		DrawRectangle(X, Y, static_cast<int>(W * std::clamp(T, 0.0f, 1.0f)), H, Fill);
		DrawRectangleLines(X - 2, Y - 2, W + 4, H + 4, Color{ 255, 255, 255, 120 });
		DrawText(Label, X + 6, Y + (H - 14) / 2, 14, WHITE);
	}

	void Shadowed(const char* Text, int X, int Y, int Size, Color C)
	{
		DrawText(Text, X + 2, Y + 2, Size, Color{ 0, 0, 0, 200 });
		DrawText(Text, X, Y, Size, C);
	}

	void Centered(const char* Text, int CX, int Y, int Size, Color C)
	{
		Shadowed(Text, CX - MeasureText(Text, Size) / 2, Y, Size, C);
	}
}

void Game::DrawHud(int SW, int SH)
{
	const HeroDef& H = GetHeroDef(P.Hero);
	const float UiScale = std::clamp(SH / 900.0f, 0.75f, 2.0f);
	const int Fs = static_cast<int>(20 * UiScale);
	char Buf[256];

	// Crosshair.
	if (CarIndex < 0 && TerminalMenu < 0)
	{
		DrawCircleLines(SW / 2, SH / 2, 6, Color{ 255, 255, 255, 200 });
		DrawPixel(SW / 2, SH / 2, WHITE);
	}

	// Hero panel.
	int Y = 16;
	Shadowed(H.Name, 16, Y, static_cast<int>(28 * UiScale), H.Accent);
	Y += static_cast<int>(30 * UiScale);
	std::snprintf(Buf, sizeof(Buf), "%s  |  %s", H.Universe, H.TraversalName);
	Shadowed(Buf, 16, Y, static_cast<int>(16 * UiScale), Color{ 230, 230, 240, 255 });
	Y += static_cast<int>(24 * UiScale);
	const int BW = static_cast<int>(260 * UiScale), BH = static_cast<int>(18 * UiScale);
	std::snprintf(Buf, sizeof(Buf), "HP %d", static_cast<int>(std::ceil(P.Health)));
	Bar(16, Y, BW, BH, P.Health / P.MaxHealth, Color{ 230, 70, 80, 255 }, Buf);
	Y += BH + 8;
	std::snprintf(Buf, sizeof(Buf), "ULTIMATE %d%%%s", static_cast<int>(P.Meter), P.Meter >= 100.0f ? "  [F]" : "");
	Bar(16, Y, BW, BH, P.Meter / 100.0f, P.Meter >= 100.0f ? Color{ 255, 220, 60, 255 } : Color{ 120, 140, 255, 255 }, Buf);
	Y += BH + 8;
	if (P.Hero == HeroId::Danny || P.Hero == HeroId::Jimmy || P.Hero == HeroId::Jake)
	{
		const char* Name = P.Hero == HeroId::Danny ? "ECTO ENERGY" : (P.Hero == HeroId::Jimmy ? "JETPACK FUEL" : "FIRE BREATH");
		std::snprintf(Buf, sizeof(Buf), "%s%s", Name, P.Intangible ? (P.WantsIntangible ? "  - INTANGIBLE" : "  - WAITING TO SOLIDIFY") : "");
		Bar(16, Y, BW, BH, P.Energy / 100.0f, Color{ 90, 230, 130, 255 }, Buf);
		Y += BH + 8;
	}

	// Omnitrix panel.
	if (P.Hero == HeroId::Ben)
	{
		const Omnitrix::FStateMachine& M = P.Omni;
		const FormDef* Form = GetFormDef(M.GetActiveForm());
		std::snprintf(Buf, sizeof(Buf), "OMNITRIX: %s%s%s", OmnitrixStateName(M.GetState()), Form ? " - " : "", Form ? Form->Name : "");
		Shadowed(Buf, 16, Y, static_cast<int>(18 * UiScale), Color{ 110, 255, 110, 255 });
		Y += static_cast<int>(24 * UiScale);
		if (M.GetState() == Omnitrix::EState::Alien || M.GetState() == Omnitrix::EState::Selecting)
		{
			std::snprintf(Buf, sizeof(Buf), "ALIEN TIME %.0fs", M.GetAlienTimeRemaining());
			Bar(16, Y, BW, BH, M.GetAlienTimeRemaining() / M.GetConfig().AlienDuration, Color{ 90, 230, 90, 255 }, Buf);
			Y += BH + 8;
		}
		if (M.GetState() == Omnitrix::EState::Recharging || M.GetState() == Omnitrix::EState::Glitched || M.GetState() == Omnitrix::EState::Overclocked)
		{
			std::snprintf(Buf, sizeof(Buf), "%s %.1fs", OmnitrixStateName(M.GetState()), M.GetStateTimeRemaining());
			Bar(16, Y, BW, BH, 1.0f, M.GetState() == Omnitrix::EState::Recharging ? Color{ 220, 60, 60, 255 } : Color{ 255, 210, 60, 255 }, Buf);
			Y += BH + 8;
		}
		std::snprintf(Buf, sizeof(Buf), "1 Heatblast  2 Four Arms  3 XLR8   G Glitch%s", M.GetGlitchCooldownRemaining() > 0.0f ? " (cooldown)" : "");
		Shadowed(Buf, 16, Y, static_cast<int>(15 * UiScale), Color{ 200, 255, 200, 255 });
		Y += static_cast<int>(20 * UiScale);

		if (M.GetState() == Omnitrix::EState::Selecting)
		{
			// Dial wheel.
			const int CX = SW / 2, CY = SH / 2;
			const int Rad = static_cast<int>(150 * UiScale);
			DrawCircle(CX, CY, static_cast<float>(Rad + 40), Color{ 0, 0, 0, 140 });
			DrawCircleLines(CX, CY, static_cast<float>(Rad + 40), Color{ 90, 255, 90, 255 });
			DrawCircle(CX, CY, 40 * UiScale, Color{ 90, 255, 90, 255 });
			for (int k = 0; k < 3; ++k)
			{
				const float A = (-90.0f + k * 120.0f) * 3.14159f / 180.0f;
				const int X = CX + static_cast<int>(std::cos(A) * Rad), YY = CY + static_cast<int>(std::sin(A) * Rad);
				const FormDef* F = GetFormDef(k);
				DrawCircle(X, YY, 46 * UiScale, F->Body);
				DrawCircleLines(X, YY, 46 * UiScale, WHITE);
				std::snprintf(Buf, sizeof(Buf), "%d", k + 1);
				Centered(Buf, X, YY - static_cast<int>(22 * UiScale), static_cast<int>(26 * UiScale), WHITE);
				Centered(F->Name, X, YY + static_cast<int>(52 * UiScale), Fs, WHITE);
			}
			Centered("OMNITRIX - press 1/2/3 to transform, Q to cancel (time slowed)", CX, CY + Rad + static_cast<int>(70 * UiScale), Fs, Color{ 200, 255, 200, 255 });
		}
	}

	// Level / speed panel.
	const float Speed = std::sqrt(P.Vel.x * P.Vel.x + P.Vel.y * P.Vel.y + P.Vel.z * P.Vel.z);
	const RayHit Down = Raycast(Levels[LevelIndex], P.Pos, Vector3{ 0, 0, -1 }, 50000.0f);
	std::snprintf(Buf, sizeof(Buf), "%s", Levels[LevelIndex].Name.c_str());
	Shadowed(Buf, SW - MeasureText(Buf, Fs) - 16, 16, Fs, WHITE);
	std::snprintf(Buf, sizeof(Buf), "%3.0f km/h   alt %4.1f m   %d FPS", Speed * 0.036f, Down.Hit ? (Down.Distance - P.H) * 0.01f : 0.0f, GetFPS());
	Shadowed(Buf, SW - MeasureText(Buf, static_cast<int>(16 * UiScale)) - 16, 16 + Fs + 6, static_cast<int>(16 * UiScale), Color{ 220, 220, 230, 255 });
	int RY = 16 + Fs + 32;
	if (!Rifts.empty())
	{
		std::snprintf(Buf, sizeof(Buf), "Rifts closed: %d/%d   KOs: %d", RiftsClosed, static_cast<int>(Rifts.size()), Kills);
		Shadowed(Buf, SW - MeasureText(Buf, static_cast<int>(16 * UiScale)) - 16, RY, static_cast<int>(16 * UiScale), Color{ 220, 170, 255, 255 });
		RY += static_cast<int>(22 * UiScale);
	}
	for (const Course& C : Courses)
	{
		if (C.Running)
		{
			std::snprintf(Buf, sizeof(Buf), "%s  %.2fs  gate %d/%d", C.Name.c_str(), C.Time, C.Next + 1, static_cast<int>(C.Gates.size()));
		}
		else
		{
			std::snprintf(Buf, sizeof(Buf), "%s: fly/drive through the glowing gate%s", C.Name.c_str(), C.Best > 0.0f ? "" : "");
			if (C.Best > 0.0f) std::snprintf(Buf, sizeof(Buf), "%s  best %.2fs", C.Name.c_str(), C.Best);
		}
		Shadowed(Buf, SW - MeasureText(Buf, static_cast<int>(16 * UiScale)) - 16, RY, static_cast<int>(16 * UiScale), Color{ 140, 255, 200, 255 });
		RY += static_cast<int>(22 * UiScale);
	}

	// Terminal labels in the world.
	const Camera3D Cam = MakeCamera();
	for (const Marker& M : Levels[LevelIndex].Markers)
	{
		if (M.Class != "DimensionalTerminal") continue;
		const Vector3 To{ M.Location.x - CamPos.x, M.Location.y - CamPos.y, M.Location.z - CamPos.z };
		const float D = std::sqrt(To.x * To.x + To.y * To.y + To.z * To.z);
		const Vector3 F = { std::cos(CamPitch * 0.0174533f) * std::cos(CamYaw * 0.0174533f), std::cos(CamPitch * 0.0174533f) * std::sin(CamYaw * 0.0174533f), std::sin(CamPitch * 0.0174533f) };
		if (D > 7000.0f || To.x * F.x + To.y * F.y + To.z * F.z < 0.0f) continue;
		const Vector2 S = GetWorldToScreenEx(ToRender(Vector3{ M.Location.x, M.Location.y, M.Location.z + 360.0f }), Cam, SW, SH);
		Centered(M.Label.substr(9).c_str(), static_cast<int>(S.x), static_cast<int>(S.y), static_cast<int>(18 * UiScale), WHITE);
	}

	// Prompt + messages.
	if (!Prompt.empty() && TerminalMenu < 0)
	{
		Centered(Prompt.c_str(), SW / 2, SH / 2 + static_cast<int>(60 * UiScale), Fs, Color{ 255, 240, 160, 255 });
	}
	int MY = SH - static_cast<int>(40 * UiScale);
	for (int i = static_cast<int>(Messages.size()) - 1; i >= 0; --i)
	{
		const Message& M = Messages[i];
		const unsigned char A = static_cast<unsigned char>(255 * std::clamp(M.Time, 0.0f, 1.0f));
		Centered(M.Text.c_str(), SW / 2, MY, Fs, Color{ M.Tint.r, M.Tint.g, M.Tint.b, A });
		MY -= Fs + 8;
	}
	if (BannerTime > 0.0f && !Banner.empty())
	{
		const int Size = static_cast<int>(54 * UiScale);
		const unsigned char A = static_cast<unsigned char>(255 * std::clamp(BannerTime, 0.0f, 1.0f));
		Centered(Banner.c_str(), SW / 2, SH / 4, Size, Color{ 255, 240, 120, A });
	}
	if (P.RespawnTimer > 0.0f)
	{
		Centered("K.O.!", SW / 2, SH / 2 - 40, static_cast<int>(80 * UiScale), Color{ 255, 90, 90, 255 });
	}
	if (P.HurtFlash > 0.0f)
	{
		DrawRectangleLinesEx(Rectangle{ 0, 0, static_cast<float>(SW), static_cast<float>(SH) }, 12, Color{ 255, 40, 40, static_cast<unsigned char>(P.HurtFlash * 500) });
	}

	// Terminal menu.
	if (TerminalMenu >= 0)
	{
		const Marker& M = Levels[LevelIndex].Markers[TerminalMenu];
		const int W = static_cast<int>(620 * UiScale), Hh = static_cast<int>(470 * UiScale);
		const int X = (SW - W) / 2, YY = (SH - Hh) / 2;
		DrawRectangle(X, YY, W, Hh, Color{ 15, 10, 40, 230 });
		DrawRectangleLinesEx(Rectangle{ static_cast<float>(X), static_cast<float>(YY), static_cast<float>(W), static_cast<float>(Hh) }, 3, Color{ 150, 120, 255, 255 });
		std::snprintf(Buf, sizeof(Buf), "DIMENSIONAL TERMINAL - %s", M.Label.substr(9).c_str());
		Centered(Buf, SW / 2, YY + 16, static_cast<int>(26 * UiScale), Color{ 200, 180, 255, 255 });
		int LY = YY + static_cast<int>(60 * UiScale);
		for (int i = 0; i < static_cast<int>(HeroId::Count); ++i)
		{
			const HeroDef& D = GetHeroDef(static_cast<HeroId>(i));
			std::snprintf(Buf, sizeof(Buf), "%d  %-16s %s", i + 1, D.Name, D.Universe);
			DrawRectangle(X + 24, LY + 3, 14, 14, D.Accent);
			Shadowed(Buf, X + 48, LY, Fs, i == static_cast<int>(P.Hero) ? D.Accent : WHITE);
			LY += Fs + static_cast<int>(10 * UiScale);
		}
		LY += 10;
		const bool CanTravel = M.Label == "Terminal_Retroville";
		Shadowed(CanTravel ? "T  Travel to Retroville (vertical slice)" : "T  Travel (portal unstable - only Retroville is open)", X + 24, LY, Fs,
		         CanTravel ? Color{ 255, 220, 120, 255 } : Color{ 150, 150, 170, 255 });
		LY += Fs + 10;
		Shadowed("E / Esc  Close", X + 24, LY, Fs, Color{ 180, 180, 200, 255 });
	}

	// Help overlay.
	if (bShowHelp && TerminalMenu < 0)
	{
		const char* Lines[] = {
			"CONTROLS  (F1 to toggle)",
			"WASD move   Mouse look   Space jump / ascend   Ctrl or C descend   Shift sprint/boost",
			"LMB attack   Q signature   R Ghost Wail (Danny)   F ultimate   Tab fly (flyers)",
			"E interact: terminals (swap hero / travel), hover car   P return to Nexus",
			"Ben: 1/2/3 transform, Q dial, G Master Control Glitch, F Way Big (full meter)",
			"Esc pause/quit   F11 fullscreen   Debug: F5 spawn enemies  F6 fill meter  F7 next hero",
		};
		const int LH = static_cast<int>(18 * UiScale);
		int W = 0;
		for (const char* L : Lines) W = std::max(W, MeasureText(L, LH));
		const int BoxH = (LH + 6) * 6 + 16;
		const int X = (SW - W) / 2 - 16, YY = SH - BoxH - static_cast<int>(150 * UiScale);
		DrawRectangle(X, YY, W + 32, BoxH, Color{ 0, 0, 0, 160 });
		for (int i = 0; i < 6; ++i)
		{
			DrawText(Lines[i], X + 16, YY + 8 + i * (LH + 6), LH, i == 0 ? Color{ 255, 230, 120, 255 } : WHITE);
		}
	}

	if (bPaused)
	{
		DrawRectangle(0, 0, SW, SH, Color{ 0, 0, 0, 150 });
		Centered("PAUSED", SW / 2, SH / 2 - 80, static_cast<int>(60 * UiScale), WHITE);
		Centered("Esc resume     X quit", SW / 2, SH / 2, Fs, Color{ 220, 220, 230, 255 });
	}
}
