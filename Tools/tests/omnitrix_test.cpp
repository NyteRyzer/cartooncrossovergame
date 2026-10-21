// Standalone tests for Omnitrix::FStateMachine. Build: Tools/run_logic_tests.sh
#include "Omnitrix/OmnitrixStateMachine.h"

#include <cmath>
#include <cstdio>

using namespace Omnitrix;

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #Cond); ++Failures; } } while (0)

static void Advance(FStateMachine& SM, float Seconds, float Step = 1.0f / 60.0f)
{
	for (float T = 0.0f; T < Seconds; T += Step)
	{
		SM.Tick(Step);
	}
}

static void TransformFromHuman(FStateMachine& SM, int Slot)
{
	SM.OpenDial();
	SM.Confirm(Slot);
	SM.SetPendingAssetsReady(true);
	Advance(SM, SM.GetConfig().TransformTime + 0.05f);
}

static void TestDialToAlien()
{
	FStateMachine SM;
	CHECK(SM.GetState() == EState::Human);
	CHECK(SM.OpenDial());
	CHECK(SM.GetState() == EState::Selecting);
	CHECK(!SM.Confirm(3)); // out of range
	CHECK(SM.Confirm(1));
	CHECK(SM.GetState() == EState::Transforming);
	CHECK(SM.IsInvulnerable());
	CHECK(SM.IsInputLocked());
	CHECK(SM.GetPendingForm() == 1);
	SM.SetPendingAssetsReady(true);
	Advance(SM, 1.0f);
	CHECK(SM.GetState() == EState::Alien);
	CHECK(SM.GetActiveForm() == 1);
}

static void TestTransformWaitsForAssets()
{
	FStateMachine SM;
	SM.OpenDial();
	SM.Confirm(0);
	Advance(SM, 2.0f);
	CHECK(SM.GetState() == EState::Transforming);
	SM.SetPendingAssetsReady(true);
	SM.Tick(0.016f);
	CHECK(SM.GetState() == EState::Alien);
}

static void TestCancelDial()
{
	FStateMachine SM;
	SM.OpenDial();
	CHECK(SM.CancelDial());
	CHECK(SM.GetState() == EState::Human);

	TransformFromHuman(SM, 0);
	CHECK(SM.OpenDial());
	CHECK(SM.CancelDial());
	CHECK(SM.GetState() == EState::Alien);
	CHECK(SM.GetActiveForm() == 0);
}

static void TestTimeoutAndRecharge()
{
	FConfig C;
	C.AlienDuration = 5.0f;
	C.RechargeTime = 3.0f;
	FStateMachine SM(C);
	TransformFromHuman(SM, 2);
	Advance(SM, 5.0f);
	CHECK(SM.GetState() == EState::Reverting);
	Advance(SM, C.RevertTime + 0.05f);
	CHECK(SM.GetState() == EState::Recharging);
	CHECK(SM.GetActiveForm() == NoForm);
	CHECK(!SM.OpenDial());
	CHECK(!SM.QuickSwap(0));
	Advance(SM, 3.1f);
	CHECK(SM.GetState() == EState::Human);
	CHECK(SM.OpenDial());
}

static void TestQuickSwapCostsTime()
{
	FStateMachine SM;
	TransformFromHuman(SM, 0);
	const float Before = SM.GetAlienTimeRemaining();
	CHECK(!SM.QuickSwap(0)); // already active
	CHECK(SM.QuickSwap(1));
	CHECK(SM.GetState() == EState::Transforming);
	CHECK(std::fabs(SM.GetAlienTimeRemaining() - (Before - SM.GetConfig().SwapTimePenalty)) < 0.001f);
	SM.SetPendingAssetsReady(true);
	Advance(SM, 1.0f);
	CHECK(SM.GetState() == EState::Alien);
	CHECK(SM.GetActiveForm() == 1);
}

static void TestQuickSwapDeniedWhenLowOnTime()
{
	FConfig C;
	C.AlienDuration = 6.0f;
	C.SwapTimePenalty = 4.0f;
	FStateMachine SM(C);
	TransformFromHuman(SM, 0);
	Advance(SM, 2.5f);
	CHECK(!SM.QuickSwap(1));
	CHECK(SM.GetState() == EState::Alien);
}

static void TestSwapViaDialFromAlien()
{
	FStateMachine SM;
	TransformFromHuman(SM, 0);
	CHECK(SM.OpenDial());
	Advance(SM, 1.0f); // alien clock keeps running while the dial is open
	CHECK(SM.GetAlienTimeRemaining() < SM.GetConfig().AlienDuration - 1.0f);
	CHECK(SM.Confirm(2));
	CHECK(SM.GetState() == EState::Transforming);
	CHECK(SM.GetPendingForm() == 2);
}

static void TestGlitch()
{
	FStateMachine SM;
	TransformFromHuman(SM, 0);
	CHECK(SM.TriggerGlitch());
	CHECK(SM.GetState() == EState::Glitched);
	CHECK(SM.GetActiveForm() == GlitchForm);
	CHECK(SM.IsInvulnerable());
	CHECK(!SM.TriggerGlitch());
	Advance(SM, SM.GetConfig().GlitchDuration + 0.05f);
	CHECK(SM.GetState() == EState::Reverting);
	Advance(SM, SM.GetConfig().RevertTime + SM.GetConfig().RechargeTime + 0.1f);
	CHECK(SM.GetState() == EState::Human);
	CHECK(!SM.TriggerGlitch()); // still on cooldown
	Advance(SM, SM.GetConfig().GlitchCooldown);
	CHECK(SM.TriggerGlitch());
}

static void TestOverclock()
{
	FStateMachine SM;
	TransformFromHuman(SM, 0);
	CHECK(SM.TriggerOverclock());
	CHECK(SM.GetPendingForm() == OverclockForm);
	SM.SetPendingAssetsReady(true);
	Advance(SM, 1.0f);
	CHECK(SM.GetState() == EState::Overclocked);
	CHECK(SM.GetActiveForm() == OverclockForm);
	CHECK(!SM.OpenDial());
	Advance(SM, SM.GetConfig().OverclockDuration);
	CHECK(SM.GetState() == EState::Reverting);

	// Ultimate ignores the watch's recharge.
	Advance(SM, SM.GetConfig().RevertTime + 0.05f);
	CHECK(SM.GetState() == EState::Recharging);
	CHECK(SM.TriggerOverclock());
}

static void TestForceRevert()
{
	FStateMachine SM;
	CHECK(!SM.ForceRevert());
	SM.OpenDial();
	CHECK(SM.ForceRevert());
	CHECK(SM.GetState() == EState::Human);
	TransformFromHuman(SM, 1);
	CHECK(SM.ForceRevert());
	CHECK(SM.GetState() == EState::Reverting);
	CHECK(SM.GetAlienTimeRemaining() == 0.0f);
}

int main()
{
	TestDialToAlien();
	TestTransformWaitsForAssets();
	TestCancelDial();
	TestTimeoutAndRecharge();
	TestQuickSwapCostsTime();
	TestQuickSwapDeniedWhenLowOnTime();
	TestSwapViaDialFromAlien();
	TestGlitch();
	TestOverclock();
	TestForceRevert();
	if (Failures == 0)
	{
		std::printf("omnitrix_test: all tests passed\n");
	}
	return Failures == 0 ? 0 : 1;
}
