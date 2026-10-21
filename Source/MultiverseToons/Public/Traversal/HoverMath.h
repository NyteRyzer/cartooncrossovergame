#pragma once

// Engine-independent hover physics helpers shared by hover vehicles.
// Units follow Unreal: centimetres, seconds, kilograms. Unit-tested in
// Tools/tests/hover_math_test.cpp.

namespace HoverMath
{
	struct FSpringParams
	{
		float TargetHeight = 120.0f;  // Rest height of the hover point above ground
		float MaxRayLength = 300.0f;  // Beyond this the hover point produces no lift
		// Whole-vehicle values: the vehicle divides the result across its hover points.
		// Equilibrium sag under gravity g is g / Stiffness (1960 / 60 ~= 33 cm).
		float Stiffness = 60.0f;      // Acceleration (cm/s^2) per cm of compression
		float Damping = 10.0f;        // Acceleration per cm/s of compression velocity (~0.65 damping ratio)
	};

	// Upward acceleration the vehicle needs from one hover point's reading.
	// Distance: current ray hit distance (cm). PreviousDistance: last frame's distance.
	// Returns 0 when the ground is out of range; never pulls the vehicle down.
	inline float SpringAcceleration(const FSpringParams& P, float Distance, float PreviousDistance, float DeltaSeconds)
	{
		if (Distance >= P.MaxRayLength || DeltaSeconds <= 0.0f)
		{
			return 0.0f;
		}
		const float Compression = P.TargetHeight - Distance;
		const float CompressionVelocity = (PreviousDistance - Distance) / DeltaSeconds;
		const float Accel = P.Stiffness * Compression + P.Damping * CompressionVelocity;
		return Accel > 0.0f ? Accel : 0.0f;
	}

	// Fraction of sideways velocity to remove this frame so the car "grips" the air.
	// Grip is per-second (0 = ice, large = rails); frame-rate independent.
	inline float LateralGripAlpha(float GripPerSecond, float DeltaSeconds)
	{
		if (GripPerSecond <= 0.0f || DeltaSeconds <= 0.0f)
		{
			return 0.0f;
		}
		const float Alpha = GripPerSecond * DeltaSeconds;
		return Alpha < 1.0f ? Alpha : 1.0f;
	}

	// Thrust tapers to zero as forward speed approaches the top speed.
	inline float ThrottleAcceleration(float Throttle, float ForwardSpeed, float MaxAccel, float TopSpeed)
	{
		if (TopSpeed <= 0.0f)
		{
			return 0.0f;
		}
		float Headroom = 1.0f - (ForwardSpeed * (Throttle >= 0.0f ? 1.0f : -1.0f)) / TopSpeed;
		Headroom = Headroom < 0.0f ? 0.0f : (Headroom > 1.0f ? 1.0f : Headroom);
		return Throttle * MaxAccel * Headroom;
	}
}
