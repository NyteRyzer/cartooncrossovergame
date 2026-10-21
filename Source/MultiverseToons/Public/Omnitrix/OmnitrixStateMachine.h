#pragma once

// Engine-independent Omnitrix transformation state machine.
//
// Kept free of Unreal types so the rules can be unit-tested outside the editor
// (see Tools/tests/omnitrix_test.cpp). UOmnitrixComponent owns one of these,
// feeds it input + delta time, and reacts to state changes (asset swaps, VFX).

namespace Omnitrix
{
	enum class EState : unsigned char
	{
		Human,        // Ben, watch ready
		Selecting,    // Dial wheel open (game slows time while here)
		Transforming, // Green flash; i-frames, input locked, waits on assets
		Alien,        // Active alien form, timeout counting down
		Reverting,    // Timed out / forced out; input locked
		Recharging,   // Watch red; cannot transform
		Glitched,     // Master Control Glitch: invulnerable defensive form
		Overclocked   // Ultimate: high-tier form on its own timer
	};

	// Form identifiers. Values >= 0 are playlist slot indices.
	constexpr int NoForm = -1;
	constexpr int GlitchForm = -2;
	constexpr int OverclockForm = -3;

	struct FConfig
	{
		int NumSlots = 3;
		float TransformTime = 0.45f;
		float RevertTime = 0.6f;
		float AlienDuration = 60.0f;
		float RechargeTime = 8.0f;
		float SwapTimePenalty = 4.0f;    // Alien seconds spent per in-combat quick swap
		float GlitchDuration = 4.0f;
		float GlitchCooldown = 30.0f;
		float OverclockDuration = 20.0f;
	};

	class FStateMachine
	{
	public:
		explicit FStateMachine(const FConfig& InConfig = FConfig())
			: Config(InConfig)
		{
		}

		EState GetState() const { return State; }
		int GetActiveForm() const { return ActiveForm; }
		int GetPendingForm() const { return PendingForm; }
		float GetAlienTimeRemaining() const { return AlienTimeRemaining; }
		float GetStateTimeRemaining() const { return StateTimer; }
		float GetGlitchCooldownRemaining() const { return GlitchCooldownRemaining; }
		const FConfig& GetConfig() const { return Config; }

		bool IsInvulnerable() const { return State == EState::Transforming || State == EState::Glitched; }
		bool IsInputLocked() const { return State == EState::Transforming || State == EState::Reverting; }
		bool IsTransformed() const { return State == EState::Alien || State == EState::Glitched || State == EState::Overclocked; }

		// The owner calls this when the pending form's mesh/anim assets finish streaming.
		// Transforming only completes once both the timer and the assets are ready.
		void SetPendingAssetsReady(bool bReady) { bPendingAssetsReady = bReady; }

		bool OpenDial()
		{
			if (State != EState::Human && State != EState::Alien)
			{
				return false;
			}
			bDialFromAlien = (State == EState::Alien);
			State = EState::Selecting;
			return true;
		}

		bool CancelDial()
		{
			if (State != EState::Selecting)
			{
				return false;
			}
			State = bDialFromAlien ? EState::Alien : EState::Human;
			return true;
		}

		// Confirm a playlist slot from the dial.
		bool Confirm(int Slot)
		{
			if (State != EState::Selecting || !IsValidSlot(Slot))
			{
				return false;
			}
			if (bDialFromAlien)
			{
				State = EState::Alien;
				if (!QuickSwap(Slot))
				{
					return false;
				}
				return true;
			}
			AlienTimeRemaining = Config.AlienDuration;
			BeginTransform(Slot);
			return true;
		}

		// Direct swap between playlist aliens mid-combat (no dial).
		bool QuickSwap(int Slot)
		{
			if (!IsValidSlot(Slot))
			{
				return false;
			}
			if (State == EState::Human)
			{
				AlienTimeRemaining = Config.AlienDuration;
				BeginTransform(Slot);
				return true;
			}
			if (State != EState::Alien || Slot == ActiveForm || AlienTimeRemaining <= Config.SwapTimePenalty)
			{
				return false;
			}
			AlienTimeRemaining -= Config.SwapTimePenalty;
			BeginTransform(Slot);
			return true;
		}

		// Defensive escape: instant invulnerable transform into the glitch form.
		bool TriggerGlitch()
		{
			if (GlitchCooldownRemaining > 0.0f)
			{
				return false;
			}
			if (State == EState::Glitched || State == EState::Overclocked || State == EState::Reverting)
			{
				return false;
			}
			bDialFromAlien = false;
			PendingForm = NoForm;
			ActiveForm = GlitchForm;
			State = EState::Glitched;
			StateTimer = Config.GlitchDuration;
			GlitchCooldownRemaining = Config.GlitchCooldown;
			return true;
		}

		// Ultimate. The caller is responsible for checking/spending the ultimate meter.
		bool TriggerOverclock()
		{
			if (State != EState::Human && State != EState::Alien && State != EState::Recharging)
			{
				return false;
			}
			BeginTransform(OverclockForm);
			return true;
		}

		// e.g. lethal damage while transformed.
		bool ForceRevert()
		{
			if (State == EState::Human || State == EState::Reverting || State == EState::Recharging)
			{
				return false;
			}
			if (State == EState::Selecting && !bDialFromAlien)
			{
				State = EState::Human;
				return true;
			}
			BeginRevert();
			return true;
		}

		void Tick(float DeltaSeconds)
		{
			if (DeltaSeconds <= 0.0f)
			{
				return;
			}
			if (GlitchCooldownRemaining > 0.0f)
			{
				GlitchCooldownRemaining = Max0(GlitchCooldownRemaining - DeltaSeconds);
			}

			const bool bAlienClockRunning = State == EState::Alien
				|| (State == EState::Selecting && bDialFromAlien)
				|| (State == EState::Transforming && PendingForm >= 0 && ActiveForm >= 0);
			if (bAlienClockRunning)
			{
				AlienTimeRemaining = Max0(AlienTimeRemaining - DeltaSeconds);
				if (AlienTimeRemaining <= 0.0f && State != EState::Transforming)
				{
					BeginRevert();
					return;
				}
			}

			switch (State)
			{
			case EState::Transforming:
				StateTimer = Max0(StateTimer - DeltaSeconds);
				if (StateTimer <= 0.0f && bPendingAssetsReady)
				{
					ActiveForm = PendingForm;
					PendingForm = NoForm;
					if (ActiveForm == OverclockForm)
					{
						State = EState::Overclocked;
						StateTimer = Config.OverclockDuration;
					}
					else if (AlienTimeRemaining <= 0.0f)
					{
						BeginRevert();
					}
					else
					{
						State = EState::Alien;
					}
				}
				break;
			case EState::Glitched:
			case EState::Overclocked:
				StateTimer = Max0(StateTimer - DeltaSeconds);
				if (StateTimer <= 0.0f)
				{
					BeginRevert();
				}
				break;
			case EState::Reverting:
				StateTimer = Max0(StateTimer - DeltaSeconds);
				if (StateTimer <= 0.0f)
				{
					ActiveForm = NoForm;
					State = EState::Recharging;
					StateTimer = Config.RechargeTime;
				}
				break;
			case EState::Recharging:
				StateTimer = Max0(StateTimer - DeltaSeconds);
				if (StateTimer <= 0.0f)
				{
					State = EState::Human;
				}
				break;
			default:
				break;
			}
		}

	private:
		static float Max0(float V) { return V > 0.0f ? V : 0.0f; }

		bool IsValidSlot(int Slot) const { return Slot >= 0 && Slot < Config.NumSlots; }

		void BeginTransform(int Form)
		{
			bDialFromAlien = false;
			PendingForm = Form;
			bPendingAssetsReady = false;
			State = EState::Transforming;
			StateTimer = Config.TransformTime;
		}

		void BeginRevert()
		{
			bDialFromAlien = false;
			PendingForm = NoForm;
			AlienTimeRemaining = 0.0f;
			State = EState::Reverting;
			StateTimer = Config.RevertTime;
		}

		FConfig Config;
		EState State = EState::Human;
		int ActiveForm = NoForm;
		int PendingForm = NoForm;
		float StateTimer = 0.0f;
		float AlienTimeRemaining = 0.0f;
		float GlitchCooldownRemaining = 0.0f;
		bool bDialFromAlien = false;
		bool bPendingAssetsReady = false;
	};
}
