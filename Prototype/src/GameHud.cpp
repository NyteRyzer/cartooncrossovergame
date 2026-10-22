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

	// Portrait card with a coloured frame; falls back to a flat swatch if the
	// image wasn't found.
	void PortraitCard(const Texture2D& T, int X, int Y, int Size, Color Frame, bool bSelected)
	{
		DrawRectangle(X - 3, Y - 3, Size + 6, Size + 6, bSelected ? Frame : Color{ 0, 0, 0, 200 });
		if (T.id != 0)
		{
			DrawTexturePro(T, Rectangle{ 0, 0, static_cast<float>(T.width), static_cast<float>(T.height) },
			               Rectangle{ static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Size), static_cast<float>(Size) },
			               Vector2{ 0, 0 }, 0.0f, WHITE);
		}
		else
		{
			DrawRectangle(X, Y, Size, Size, Frame);
		}
		DrawRectangleLines(X - 3, Y - 3, Size + 6, Size + 6, Frame);
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

	// Hero panel: portrait, name, bars.
	const int PS = static_cast<int>(92 * UiScale);
	PortraitCard(Portrait(P.Hero), 19, 19, PS, H.Accent, true);
	const int PX = 19 + PS + 16;
	int Y = 16;
	Shadowed(H.Name, PX, Y, static_cast<int>(28 * UiScale), H.Accent);
	Y += static_cast<int>(30 * UiScale);
	std::snprintf(Buf, sizeof(Buf), "%s  |  %s", H.Universe, H.TraversalName);
	Shadowed(Buf, PX, Y, static_cast<int>(16 * UiScale), Color{ 230, 230, 240, 255 });
	Y += static_cast<int>(24 * UiScale);
	const int BW = static_cast<int>(260 * UiScale), BH = static_cast<int>(18 * UiScale);
	std::snprintf(Buf, sizeof(Buf), "HP %d", static_cast<int>(std::ceil(P.Health)));
	Bar(PX, Y, BW, BH, P.Health / P.MaxHealth, Color{ 230, 70, 80, 255 }, Buf);
	Y += BH + 8;
	std::snprintf(Buf, sizeof(Buf), "ULTIMATE %d%%%s", static_cast<int>(P.Meter), P.Meter >= 100.0f ? "  [F]" : "");
	Bar(PX, Y, BW, BH, P.Meter / 100.0f, P.Meter >= 100.0f ? Color{ 255, 220, 60, 255 } : Color{ 120, 140, 255, 255 }, Buf);
	Y += BH + 8;
	if (P.Hero == HeroId::Danny || P.Hero == HeroId::Jimmy || P.Hero == HeroId::Jake)
	{
		const char* Name = P.Hero == HeroId::Danny ? "ECTO ENERGY" : (P.Hero == HeroId::Jimmy ? "JETPACK FUEL" : "FIRE BREATH");
		std::snprintf(Buf, sizeof(Buf), "%s%s", Name, P.Intangible ? (P.WantsIntangible ? "  - INTANGIBLE" : "  - WAITING TO SOLIDIFY") : "");
		Bar(PX, Y, BW, BH, P.Energy / 100.0f, Color{ 90, 230, 130, 255 }, Buf);
		Y += BH + 8;
	}

	// Omnitrix panel.
	if (P.Hero == HeroId::Ben)
	{
		const Omnitrix::FStateMachine& M = P.Omni;
		const FormDef* Form = GetFormDef(M.GetActiveForm());
		std::snprintf(Buf, sizeof(Buf), "OMNITRIX: %s%s%s", OmnitrixStateName(M.GetState()), Form ? " - " : "", Form ? Form->Name : "");
		Shadowed(Buf, PX, Y, static_cast<int>(18 * UiScale), Color{ 110, 255, 110, 255 });
		Y += static_cast<int>(24 * UiScale);
		if (M.GetState() == Omnitrix::EState::Alien || M.GetState() == Omnitrix::EState::Selecting)
		{
			std::snprintf(Buf, sizeof(Buf), "ALIEN TIME %.0fs", M.GetAlienTimeRemaining());
			Bar(PX, Y, BW, BH, M.GetAlienTimeRemaining() / M.GetConfig().AlienDuration, Color{ 90, 230, 90, 255 }, Buf);
			Y += BH + 8;
		}
		if (M.GetState() == Omnitrix::EState::Recharging || M.GetState() == Omnitrix::EState::Glitched || M.GetState() == Omnitrix::EState::Overclocked)
		{
			std::snprintf(Buf, sizeof(Buf), "%s %.1fs", OmnitrixStateName(M.GetState()), M.GetStateTimeRemaining());
			Bar(PX, Y, BW, BH, 1.0f, M.GetState() == Omnitrix::EState::Recharging ? Color{ 220, 60, 60, 255 } : Color{ 255, 210, 60, 255 }, Buf);
			Y += BH + 8;
		}
		std::snprintf(Buf, sizeof(Buf), "1 Heatblast  2 Four Arms  3 XLR8   G Glitch%s", M.GetGlitchCooldownRemaining() > 0.0f ? " (cooldown)" : "");
		Shadowed(Buf, PX, Y, static_cast<int>(15 * UiScale), Color{ 200, 255, 200, 255 });
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

	// Terminal menu: hero cards built from the concept-sheet portraits.
	if (TerminalMenu >= 0)
	{
		const Marker& M = Levels[LevelIndex].Markers[TerminalMenu];
		const int Card = static_cast<int>(124 * UiScale);
		const int Gap = static_cast<int>(26 * UiScale);
		const int Cols = 4;
		const int W = Cols * Card + (Cols + 1) * Gap;
		const int Hh = static_cast<int>(2 * (Card + 46 * UiScale) + 190 * UiScale);
		const int X = (SW - W) / 2, YY = (SH - Hh) / 2;
		DrawRectangle(X, YY, W, Hh, Color{ 15, 10, 40, 235 });
		DrawRectangleLinesEx(Rectangle{ static_cast<float>(X), static_cast<float>(YY), static_cast<float>(W), static_cast<float>(Hh) }, 3, Color{ 150, 120, 255, 255 });
		std::snprintf(Buf, sizeof(Buf), "DIMENSIONAL TERMINAL - %s", M.Label.substr(9).c_str());
		Centered(Buf, SW / 2, YY + 14, static_cast<int>(26 * UiScale), Color{ 200, 180, 255, 255 });
		Centered("Choose your hero: press 1-8", SW / 2, YY + static_cast<int>(46 * UiScale), static_cast<int>(16 * UiScale), Color{ 210, 210, 230, 255 });
		const int GridY = YY + static_cast<int>(78 * UiScale);
		for (int i = 0; i < static_cast<int>(HeroId::Count); ++i)
		{
			const HeroDef& D = GetHeroDef(static_cast<HeroId>(i));
			const int CX = X + Gap + (i % Cols) * (Card + Gap);
			const int CY = GridY + (i / Cols) * (Card + static_cast<int>(46 * UiScale));
			const bool bCurrent = i == static_cast<int>(P.Hero);
			PortraitCard(Portrait(static_cast<HeroId>(i)), CX, CY, Card, D.Accent, bCurrent);
			std::snprintf(Buf, sizeof(Buf), "%d", i + 1);
			DrawRectangle(CX - 3, CY - 3, static_cast<int>(26 * UiScale), static_cast<int>(26 * UiScale), D.Accent);
			DrawText(Buf, CX + 4, CY, static_cast<int>(22 * UiScale), BLACK);
			Centered(D.Name, CX + Card / 2, CY + Card + 6, static_cast<int>(16 * UiScale), bCurrent ? D.Accent : WHITE);
			Centered(D.Universe, CX + Card / 2, CY + Card + 6 + static_cast<int>(18 * UiScale), static_cast<int>(13 * UiScale), Color{ 170, 170, 190, 255 });
		}
		int LY = GridY + 2 * (Card + static_cast<int>(46 * UiScale)) + static_cast<int>(8 * UiScale);
		const bool CanTravel = M.Label == "Terminal_Retroville";
		Centered(CanTravel ? "T  Travel to Retroville (vertical slice)" : "T  Travel (portal unstable - only Retroville is open)", SW / 2, LY, Fs,
		         CanTravel ? Color{ 255, 220, 120, 255 } : Color{ 150, 150, 170, 255 });
		LY += Fs + 10;
		Centered("E / Esc  Close", SW / 2, LY, Fs, Color{ 180, 180, 200, 255 });
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
			"M art style (cut-outs / 3D)   Esc pause/quit   F11 fullscreen   Debug: F5 enemies  F6 meter  F7 next hero",
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
