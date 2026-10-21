#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Omnitrix/OmnitrixStateMachine.h"
#include "OmnitrixComponent.generated.h"

class AToonCharacterBase;
class UAlienFormData;
class UAnimInstance;
class USkeletalMesh;
struct FStreamableHandle;

// Blueprint mirror of Omnitrix::EState (order must match).
UENUM(BlueprintType)
enum class EOmnitrixState : uint8
{
	Human,
	Selecting,
	Transforming,
	Alien,
	Reverting,
	Recharging,
	Glitched,
	Overclocked
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOmnitrixStateChanged, EOmnitrixState, NewState, EOmnitrixState, OldState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOmnitrixFormChanged, UAlienFormData*, NewForm);

// Ben 10's Omnitrix. Drives Omnitrix::FStateMachine and performs the real-time
// asset swap (mesh, anim class, capsule, movement, ability slots) on the
// owning character. Playlist forms are pre-streamed at BeginPlay so in-combat
// swaps don't hitch; the state machine holds in Transforming until the target
// form's assets are resident.
UCLASS(ClassGroup = (Toon), meta = (BlueprintSpawnableComponent))
class MULTIVERSETOONS_API UOmnitrixComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOmnitrixComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Omnitrix")
	bool OpenDial();

	UFUNCTION(BlueprintCallable, Category = "Omnitrix")
	bool CancelDial();

	UFUNCTION(BlueprintCallable, Category = "Omnitrix")
	bool ConfirmSlot(int32 Slot);

	UFUNCTION(BlueprintCallable, Category = "Omnitrix")
	bool QuickSwap(int32 Slot);

	UFUNCTION(BlueprintCallable, Category = "Omnitrix")
	bool TriggerMasterControlGlitch();

	// Spends OverclockMeterCost from the owner's ultimate meter.
	UFUNCTION(BlueprintCallable, Category = "Omnitrix")
	bool TriggerOverclock();

	UFUNCTION(BlueprintCallable, Category = "Omnitrix")
	bool ForceRevert();

	UFUNCTION(BlueprintPure, Category = "Omnitrix")
	EOmnitrixState GetState() const { return static_cast<EOmnitrixState>(Machine.GetState()); }

	UFUNCTION(BlueprintPure, Category = "Omnitrix")
	UAlienFormData* GetActiveForm() const { return ResolveForm(Machine.GetActiveForm()); }

	UFUNCTION(BlueprintPure, Category = "Omnitrix")
	float GetAlienTimeRemaining() const { return Machine.GetAlienTimeRemaining(); }

	UFUNCTION(BlueprintPure, Category = "Omnitrix")
	float GetStateTimeRemaining() const { return Machine.GetStateTimeRemaining(); }

	UFUNCTION(BlueprintPure, Category = "Omnitrix")
	float GetGlitchCooldownRemaining() const { return Machine.GetGlitchCooldownRemaining(); }

	UPROPERTY(BlueprintAssignable, Category = "Omnitrix")
	FOnOmnitrixStateChanged OnStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Omnitrix")
	FOnOmnitrixFormChanged OnFormChanged;

	// --- Setup --------------------------------------------------------------

	// The three combat-ready aliens on the dial wheel.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Omnitrix|Forms")
	TArray<TObjectPtr<UAlienFormData>> Playlist;

	// Master Control Glitch defensive form (Cannonbolt).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Omnitrix|Forms")
	TObjectPtr<UAlienFormData> GlitchForm;

	// Ultimate form (Way Big / upgraded alien).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Omnitrix|Forms")
	TObjectPtr<UAlienFormData> OverclockForm;

	UPROPERTY(EditAnywhere, Category = "Omnitrix|Tuning")
	float TransformTime = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Omnitrix|Tuning")
	float RevertTime = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Omnitrix|Tuning")
	float AlienDuration = 60.0f;

	UPROPERTY(EditAnywhere, Category = "Omnitrix|Tuning")
	float RechargeTime = 8.0f;

	UPROPERTY(EditAnywhere, Category = "Omnitrix|Tuning")
	float SwapTimePenalty = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Omnitrix|Tuning")
	float GlitchDuration = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Omnitrix|Tuning")
	float GlitchCooldown = 30.0f;

	UPROPERTY(EditAnywhere, Category = "Omnitrix|Tuning")
	float OverclockDuration = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Omnitrix|Tuning", meta = (ClampMin = "0", ClampMax = "100"))
	float OverclockMeterCost = 100.0f;

	// Global time dilation while the dial wheel is open.
	UPROPERTY(EditAnywhere, Category = "Omnitrix|Tuning", meta = (ClampMin = "0.05", ClampMax = "1"))
	float DialTimeDilation = 0.25f;

private:
	struct FHumanSnapshot
	{
		float CapsuleRadius = 0.0f;
		float CapsuleHalfHeight = 0.0f;
		float MaxWalkSpeed = 0.0f;
		float JumpZVelocity = 0.0f;
		float GravityScale = 1.0f;
		float AirControl = 0.0f;
	};

	UAlienFormData* ResolveForm(int FormId) const;
	AToonCharacterBase* GetCharacter() const;

	// Runs after every state-machine call; reacts to state/form transitions.
	void SyncWithMachine();
	void RequestFormAssets(int FormId);
	void OnFormAssetsLoaded(int FormId);
	void ApplyForm(const UAlienFormData* Form);
	void ApplyHuman();
	void ApplyBody(float Radius, float HalfHeight);

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMesh> HumanMesh;

	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> HumanAnimClass;

	Omnitrix::FStateMachine Machine;
	FHumanSnapshot Human;
	TArray<TSharedPtr<FStreamableHandle>> PreloadHandles;
	TSharedPtr<FStreamableHandle> PendingHandle;
	Omnitrix::EState LastState = Omnitrix::EState::Human;
	int LastActiveForm = Omnitrix::NoForm;
	bool bTimeDilated = false;
};
