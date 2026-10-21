// Standalone tests for HoverMath. Build: Tools/run_logic_tests.sh
#include "Traversal/HoverMath.h"

#include <cmath>
#include <cstdio>

using namespace HoverMath;

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #Cond); ++Failures; } } while (0)

static void TestSpring()
{
	FSpringParams P;
	const float Dt = 1.0f / 60.0f;
	CHECK(SpringAcceleration(P, P.MaxRayLength + 1.0f, P.MaxRayLength + 1.0f, Dt) == 0.0f);
	CHECK(SpringAcceleration(P, P.TargetHeight, P.TargetHeight, Dt) == 0.0f);
	CHECK(SpringAcceleration(P, 60.0f, 60.0f, Dt) > 0.0f);
	// Above rest height and not moving: no downward pull.
	CHECK(SpringAcceleration(P, 200.0f, 200.0f, Dt) == 0.0f);
	// Falling toward the ground adds damping force.
	CHECK(SpringAcceleration(P, 100.0f, 110.0f, Dt) > SpringAcceleration(P, 100.0f, 100.0f, Dt));
	CHECK(SpringAcceleration(P, 60.0f, 60.0f, 0.0f) == 0.0f);
}

// Simulate a 1-point hover body under gravity and check it settles near rest height.
static void TestSettles()
{
	FSpringParams P;
	const float Gravity = -1960.0f;
	const float Dt = 1.0f / 120.0f;
	float Height = 250.0f, Velocity = 0.0f, Prev = Height;
	for (int i = 0; i < 120 * 10; ++i)
	{
		const float Accel = Gravity + SpringAcceleration(P, Height, Prev, Dt);
		Prev = Height;
		Velocity += Accel * Dt;
		Height += Velocity * Dt;
	}
	const float Expected = P.TargetHeight + Gravity / P.Stiffness; // static equilibrium
	CHECK(std::fabs(Height - Expected) < 2.0f);
	CHECK(std::fabs(Velocity) < 5.0f);
	CHECK(Height > 0.0f);
}

static void TestGripAndThrottle()
{
	CHECK(LateralGripAlpha(0.0f, 0.1f) == 0.0f);
	CHECK(std::fabs(LateralGripAlpha(5.0f, 0.1f) - 0.5f) < 1e-6f);
	CHECK(LateralGripAlpha(50.0f, 0.1f) == 1.0f);

	CHECK(ThrottleAcceleration(1.0f, 0.0f, 1000.0f, 2500.0f) == 1000.0f);
	CHECK(ThrottleAcceleration(1.0f, 2500.0f, 1000.0f, 2500.0f) == 0.0f);
	CHECK(ThrottleAcceleration(1.0f, 1250.0f, 1000.0f, 2500.0f) == 500.0f);
	// Braking while moving forward gets full authority.
	CHECK(ThrottleAcceleration(-1.0f, 2500.0f, 1000.0f, 2500.0f) == -1000.0f);
}

int main()
{
	TestSpring();
	TestSettles();
	TestGripAndThrottle();
	if (Failures == 0)
	{
		std::printf("hover_math_test: all tests passed\n");
	}
	return Failures == 0 ? 0 : 1;
}
