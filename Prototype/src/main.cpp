// Project Multiverse Toons - playable prototype build.
//
//   ./MultiverseToons                 play
//   ./MultiverseToons --smoke-test    scripted run through every system, saves
//                                     screenshots, exits non-zero on failure

#include "Game.h"
#include "Render.h"

#include "raylib.h"
#include "rlgl.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cstdarg>
#include <sys/stat.h>
#if defined(_WIN32)
#include <direct.h>
// Avoid <windows.h>: its names clash with raylib's (CloseWindow, DrawText...).
extern "C" __declspec(dllimport) int __stdcall AttachConsole(unsigned long ProcessId);
extern "C" __declspec(dllimport) int __stdcall MessageBoxA(void* Wnd, const char* Text, const char* Caption, unsigned int Type);
#endif

namespace
{
	FILE* LogFile = nullptr;

	// Smoke-test output goes to stdout and to <shots>/smoke-test.log (Windows
	// GUI builds and double-clicked apps have no visible console).
	void Log(const char* Fmt, ...)
	{
		va_list Args;
		va_start(Args, Fmt);
		std::vprintf(Fmt, Args);
		va_end(Args);
		std::fflush(stdout);
		if (LogFile)
		{
			va_start(Args, Fmt);
			std::vfprintf(LogFile, Fmt, Args);
			va_end(Args);
			std::fflush(LogFile);
		}
	}

	void Fatal(const std::string& Message)
	{
		std::fprintf(stderr, "%s\n", Message.c_str());
#if defined(_WIN32)
		MessageBoxA(nullptr, Message.c_str(), "Multiverse Toons", 0x10 /* MB_ICONERROR */);
#endif
	}

	void MakeDir(const std::string& Path)
	{
#if defined(_WIN32)
		_mkdir(Path.c_str());
#else
		mkdir(Path.c_str(), 0755);
#endif
	}

	struct Options
	{
		bool SmokeTest = false;
		bool Fullscreen = false;
		std::string ShotDir = "smoke-test-shots";
		std::string DataDir;
		int Width = 0, Height = 0;
	};

	bool FileExists(const std::string& Path)
	{
		struct stat St;
		return stat(Path.c_str(), &St) == 0;
	}

	std::string FindDataDir(const std::string& Override)
	{
		if (!Override.empty()) return Override;
		const std::string App = GetApplicationDirectory();
		// ../Resources/data is the macOS .app bundle layout.
		const std::string Candidates[] = { App + "data", App + "../Resources/data", App + "../data", "data", "../data" };
		for (const std::string& C : Candidates)
		{
			if (FileExists(C + "/levels/nexus_observatory.lvl")) return C;
		}
		return App + "data";
	}

	InputState ReadInput(const Game& G, bool bCursorLocked)
	{
		InputState In;
		const bool Menu = G.IsTerminalOpen();
		In.MoveX = (IsKeyDown(KEY_D) ? 1.0f : 0.0f) - (IsKeyDown(KEY_A) ? 1.0f : 0.0f);
		In.MoveY = (IsKeyDown(KEY_W) ? 1.0f : 0.0f) - (IsKeyDown(KEY_S) ? 1.0f : 0.0f);
		if (bCursorLocked)
		{
			const Vector2 Delta = GetMouseDelta();
			In.LookX = Delta.x * 0.12f;
			In.LookY = -Delta.y * 0.12f;
		}
		// Arrow keys also look around (handy on laptops).
		In.LookX += ((IsKeyDown(KEY_RIGHT) ? 1.0f : 0.0f) - (IsKeyDown(KEY_LEFT) ? 1.0f : 0.0f)) * 2.0f;
		In.LookY += ((IsKeyDown(KEY_UP) ? 1.0f : 0.0f) - (IsKeyDown(KEY_DOWN) ? 1.0f : 0.0f)) * 1.5f;

		In.Jump = IsKeyPressed(KEY_SPACE);
		In.JumpHeld = IsKeyDown(KEY_SPACE);
		In.Down = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_C);
		In.Boost = IsKeyDown(KEY_LEFT_SHIFT);
		In.Attack = bCursorLocked && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
		In.AttackHeld = bCursorLocked && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
		In.Sig = IsKeyPressed(KEY_Q) || (bCursorLocked && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT));
		In.SigHeld = IsKeyDown(KEY_Q) || (bCursorLocked && IsMouseButtonDown(MOUSE_BUTTON_RIGHT));
		In.Sig2 = IsKeyPressed(KEY_R);
		In.Ult = IsKeyPressed(KEY_F);
		In.Interact = IsKeyPressed(KEY_E);
		In.Traversal = IsKeyPressed(KEY_TAB);
		In.Glitch = IsKeyPressed(KEY_G);
		for (int K = 1; K <= 9; ++K)
		{
			if (IsKeyPressed(KEY_ZERO + K) || IsKeyPressed(KEY_KP_0 + K)) In.Number = K;
		}
		In.Back = Menu && IsKeyPressed(KEY_ESCAPE);
		In.Help = IsKeyPressed(KEY_F1);
		In.Recall = IsKeyPressed(KEY_P);
		In.Travel = IsKeyPressed(KEY_T);
		In.DebugSpawn = IsKeyPressed(KEY_F5);
		In.DebugMeter = IsKeyPressed(KEY_F6);
		In.DebugNextHero = IsKeyPressed(KEY_F7);
		return In;
	}

	// ------------------------------------------------------------------
	// Scripted smoke test
	// ------------------------------------------------------------------
	class SmokeTest
	{
	public:
		explicit SmokeTest(std::string Dir) : ShotDir(std::move(Dir)) {}

		// Returns false when finished. Fills In and may request a screenshot.
		bool Step(Game& G, float T, float PrevT, InputState& In)
		{
			auto Once = [&](float At) { return PrevT < At && T >= At; };
			auto During = [&](float A, float B) { return T >= A && T < B; };
			In = InputState{};
			Shot.clear();

			// --- Nexus: Jimmy on foot ---------------------------------------
			if (Once(0.0f)) { G.SelectHero(HeroId::Jimmy); G.SetCameraAngles(0.0f, -10.0f); }
			if (During(0.2f, 2.0f)) In.MoveY = 1.0f;
			if (Once(1.0f)) In.Jump = true;
			if (Once(1.9f)) Shot = "01_nexus_jimmy";

			// --- Hover car -------------------------------------------------
			if (Once(2.0f)) G.TeleportPlayer(Vector3{ -1500.0f, -2150.0f, 120.0f }, -90.0f);
			if (Once(2.2f)) Check(G.EnterNearestCar(), "enter hover car");
			if (During(2.3f, 5.0f)) { In.MoveY = 1.0f; In.MoveX = 0.35f; }
			if (During(3.0f, 4.0f)) In.JumpHeld = true;
			if (Once(4.9f))
			{
				const HoverCar* Car = G.PlayerCar();
				Check(Car != nullptr, "player is in a car");
				if (Car)
				{
					const float Speed = std::sqrt(Car->Vel.x * Car->Vel.x + Car->Vel.y * Car->Vel.y);
					Log("  hover car: speed %.0f cm/s, cruise altitude %.0f cm\n", Speed, Car->CruiseAltitude);
					Check(Speed > 800.0f, "hover car accelerates");
					Check(Car->CruiseAltitude > 300.0f, "hover car lift raises altitude");
				}
				Shot = "02_hover_car";
			}
			if (Once(5.1f)) G.ExitCar();

			// --- Danny: flight + intangibility ------------------------------
			if (Once(5.3f)) G.SelectHero(HeroId::Danny);
			if (Once(5.5f)) In.Traversal = true;
			if (During(5.6f, 6.6f)) { In.JumpHeld = true; In.MoveY = 1.0f; }
			if (Once(6.8f))
			{
				Check(G.PlayerPos().z > 250.0f, "Danny flies upward");
				In.Sig = true; // go intangible
			}
			if (Once(7.0f)) G.TeleportPlayer(Vector3{ -150.0f, 0.0f, 900.0f }, 0.0f); // inside the Rift Core column
			if (Once(7.4f)) { Check(G.PlayerIntangible(), "intangible inside geometry"); Shot = "03_danny_phasing"; G.SetCameraAngles(0.0f, -15.0f); }
			if (Once(7.6f)) In.Sig = true; // ask to solidify while still inside
			if (Once(8.0f)) Check(G.PlayerIntangible(), "stays intangible until clear of geometry");
			if (Once(8.2f)) G.TeleportPlayer(Vector3{ -2200.0f, 0.0f, 600.0f }, 0.0f);
			if (Once(8.5f)) Check(!G.PlayerIntangible(), "solidifies once clear");
			auto FlyThroughGate = [&](const char* Label) {
				if (const Block* B = G.CurrentLevelData().FindBlock(Label)) G.TeleportPlayer(B->Center, 0.0f);
			};
			if (Once(8.6f)) FlyThroughGate("FlightGate_00");
			if (Once(8.7f)) FlyThroughGate("FlightGate_01");
			if (Once(8.8f)) Check(G.CourseProgress() == 2, "flight ring course counts gates in order");

			// --- Retroville: Ben and the Omnitrix -----------------------------
			if (Once(9.0f)) { G.TravelTo(1); G.SelectHero(HeroId::Ben); G.SetCameraAngles(60.0f, -12.0f); }
			if (Once(9.5f)) In.Number = 1;
			if (Once(10.5f)) { Check(G.OmnitrixState() == Omnitrix::EState::Alien, "Heatblast transform"); Shot = "04_ben_heatblast"; }
			if (Once(11.0f)) In.Number = 2;
			if (During(11.2f, 12.0f)) In.MoveY = 1.0f;
			if (Once(12.2f)) { Check(G.OmnitrixState() == Omnitrix::EState::Alien, "quick swap to Four Arms"); Shot = "05_ben_four_arms"; }
			if (Once(12.5f)) In.Glitch = true;
			if (Once(12.7f)) Check(G.OmnitrixState() == Omnitrix::EState::Glitched, "Master Control Glitch");
			if (During(12.8f, 13.6f)) In.MoveY = 1.0f;
			if (Once(13.6f)) Shot = "06_ben_cannonbolt";
			if (Once(17.5f)) { G.FillMeter(); In.Ult = true; }
			if (Once(18.6f)) { Check(G.OmnitrixState() == Omnitrix::EState::Overclocked, "Way Big overclock"); G.SetCameraAngles(60.0f, -5.0f); }
			if (Once(19.6f)) Shot = "07_ben_way_big";
			if (Once(20.0f)) G.SpawnEnemiesNearPlayer(6);
			if (During(20.2f, 21.6f)) { In.Attack = static_cast<int>(T * 10) % 10 == 0; In.AttackHeld = true; }

			// --- Combat with XJ9 ------------------------------------------
			if (Once(22.0f)) { G.SelectHero(HeroId::XJ9); G.SetCameraAngles(60.0f, -14.0f); G.SpawnEnemiesNearPlayer(6); }
			if (Once(22.3f)) In.Sig = true; // missile barrage
			if (During(22.3f, 24.0f)) { In.AttackHeld = true; In.LookX = 2.0f; }
			if (Once(23.2f)) Shot = "08_xj9_combat";

			// --- Rest of the roster -----------------------------------------
			const HeroId Others[] = { HeroId::Timmy, HeroId::Jake, HeroId::ElTigre, HeroId::Aang, HeroId::Jimmy };
			const char* Names[] = { "09_timmy", "10_jake_dragon", "11_el_tigre", "12_aang", "13_jimmy_retroville" };
			for (int i = 0; i < 5; ++i)
			{
				const float Base = 24.5f + i * 2.0f;
				if (Once(Base)) { G.SelectHero(Others[i]); G.SpawnEnemiesNearPlayer(3); G.SetCameraAngles(60.0f + i * 50.0f, -12.0f); }
				if (Once(Base + 0.2f)) { In.Traversal = true; In.Sig = true; }
				if (During(Base + 0.3f, Base + 1.5f)) { In.MoveY = 0.6f; In.AttackHeld = true; In.Attack = true; In.SigHeld = true; In.JumpHeld = i == 1; }
				if (Once(Base + 1.4f)) Shot = Names[i];
			}

			// --- Back to the Nexus for an overview ------------------------------
			if (Once(35.0f)) { In.Recall = true; }
			if (Once(35.3f)) { Check(G.CurrentLevel() == 0, "recall to Nexus"); G.SetCameraAngles(20.0f, -25.0f); }
			if (Once(35.8f)) Shot = "14_nexus_overview";

			if (!G.PlayerIsValid()) { Check(false, "player state is finite"); return false; }
			return T < 36.5f;
		}

		void Check(bool bOk, const char* What)
		{
			Log("  [%s] %s\n", bOk ? " ok " : "FAIL", What);
			Failures += bOk ? 0 : 1;
		}

		std::string Shot;
		std::string ShotDir;
		int Failures = 0;
	};
}

int main(int Argc, char** Argv)
{
	Options Opt;
	for (int i = 1; i < Argc; ++i)
	{
		const std::string A = Argv[i];
		if (A == "--smoke-test") Opt.SmokeTest = true;
		else if (A == "--fullscreen") Opt.Fullscreen = true;
		else if (A == "--shots" && i + 1 < Argc) Opt.ShotDir = Argv[++i];
		else if (A == "--data" && i + 1 < Argc) Opt.DataDir = Argv[++i];
		else if (A == "--size" && i + 1 < Argc)
		{
			const std::string V = Argv[++i];
			const size_t X = V.find('x');
			if (X != std::string::npos)
			{
				Opt.Width = static_cast<int>(std::strtod(V.substr(0, X).c_str(), nullptr));
				Opt.Height = static_cast<int>(std::strtod(V.substr(X + 1).c_str(), nullptr));
			}
		}
		else if (A == "--help" || A == "-h")
		{
			std::printf("Usage: %s [--fullscreen] [--size WxH] [--smoke-test [--shots DIR]] [--data DIR]\n", Argv[0]);
			return 0;
		}
	}

#if defined(_WIN32)
	if (Opt.SmokeTest && AttachConsole(static_cast<unsigned long>(-1)))
	{
		std::freopen("CONOUT$", "w", stdout);
		std::freopen("CONOUT$", "w", stderr);
	}
#endif
	SetTraceLogLevel(Opt.SmokeTest ? LOG_WARNING : LOG_INFO);
	SetConfigFlags(FLAG_WINDOW_RESIZABLE | (Opt.SmokeTest ? 0 : FLAG_VSYNC_HINT));
	const int W = Opt.Width > 0 ? Opt.Width : 1600;
	const int H = Opt.Height > 0 ? Opt.Height : 900;
	InitWindow(W, H, "Multiverse Toons - Prototype Build 0.1");
	if (!IsWindowReady())
	{
		Fatal("Could not open a window. An OpenGL 3.3 capable graphics driver is required.");
		return 1;
	}
	SetExitKey(KEY_NULL);
	if (!Opt.SmokeTest)
	{
		const int Mon = GetCurrentMonitor();
		if (Opt.Width == 0 && GetMonitorWidth(Mon) > 0 && GetMonitorWidth(Mon) < W + 100)
		{
			SetWindowSize(GetMonitorWidth(Mon) * 4 / 5, GetMonitorHeight(Mon) * 4 / 5);
		}
		if (Opt.Fullscreen) ToggleFullscreen();
		SetTargetFPS(144);
	}

	const std::string DataDir = FindDataDir(Opt.DataDir);
	Game G;
	std::string Error;
	if (!G.Init(DataDir, Error))
	{
		Fatal("Failed to load game data: " + Error + "\nKeep the 'data' folder next to the game.");
		CloseWindow();
		return 1;
	}

	ToonRenderer R;
	if (!R.Init(GetRenderWidth(), GetRenderHeight()))
	{
		Fatal("Failed to initialise the renderer. An OpenGL 3.3 capable graphics driver is required.");
		CloseWindow();
		return 1;
	}

	SmokeTest Smoke(Opt.ShotDir);
	if (Opt.SmokeTest)
	{
		MakeDir(Opt.ShotDir);
		LogFile = std::fopen((Opt.ShotDir + "/smoke-test.log").c_str(), "w");
		Log("Smoke test: data=%s shots=%s\n", DataDir.c_str(), Opt.ShotDir.c_str());
	}

	bool bCursorLocked = false;
	if (!Opt.SmokeTest)
	{
		DisableCursor();
		bCursorLocked = true;
	}

	float SmokeTime = 0.0f;
	int Frames = 0;
	double RenderMs = 0.0;
	while (!WindowShouldClose() && !G.WantsQuit())
	{
		InputState In;
		float Dt = GetFrameTime();
		bool bRunning = true;

		if (Opt.SmokeTest)
		{
			Dt = 1.0f / 30.0f;
			const float Prev = SmokeTime;
			SmokeTime += Dt;
			bRunning = Smoke.Step(G, SmokeTime, Prev == 0.0f ? -1.0f : Prev, In);
		}
		else
		{
			if (IsKeyPressed(KEY_F11)) ToggleFullscreen();
			if (IsKeyPressed(KEY_ESCAPE) && !G.IsTerminalOpen())
			{
				G.SetPaused(!G.IsPaused());
				if (G.IsPaused()) { EnableCursor(); bCursorLocked = false; }
				else { DisableCursor(); bCursorLocked = true; }
			}
			if (G.IsPaused() && IsKeyPressed(KEY_X)) G.RequestQuit();
			if (!G.IsPaused() && !bCursorLocked && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
			{
				DisableCursor();
				bCursorLocked = true;
			}
			In = ReadInput(G, bCursorLocked);
		}

		if (!G.IsPaused())
		{
			G.Update(Dt, In);
		}

		R.Resize(GetRenderWidth(), GetRenderHeight());
		const double T0 = GetTime();
		G.Render(R);
		BeginDrawing();
		ClearBackground(BLACK);
		R.Composite(G.Lighting());
		G.DrawHud(GetScreenWidth(), GetScreenHeight());
		if (Opt.SmokeTest && !Smoke.Shot.empty())
		{
			rlDrawRenderBatchActive();
			Image Img = LoadImageFromScreen();
			const std::string Path = Opt.ShotDir + "/" + Smoke.Shot + ".png";
			ExportImage(Img, Path.c_str());
			UnloadImage(Img);
			Log("  screenshot %s\n", Path.c_str());
		}
		EndDrawing();
		RenderMs += (GetTime() - T0) * 1000.0;
		++Frames;

		if (!bRunning) break;
	}

	int Exit = 0;
	if (Opt.SmokeTest)
	{
		Log("Smoke test: %d frames, %.1f ms avg render, %d enemies defeated, %d failure(s)\n",
		            Frames, Frames ? RenderMs / Frames : 0.0, G.KillCount(), Smoke.Failures);
		if (G.KillCount() == 0)
		{
			Log("  [FAIL] combat defeated at least one enemy\n");
			++Smoke.Failures;
		}
		Exit = Smoke.Failures == 0 ? 0 : 1;
	}

	if (LogFile) std::fclose(LogFile);
	R.Shutdown();
	CloseWindow();
	return Exit;
}
