#pragma once

#include "CoreMinimal.h"
#include "Traversal/ToonTraversalComponent.h"
#include "GhostFlightComponent.generated.h"

class UCapsuleComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnIntangibilityChanged, bool, bIntangible);

// Danny Phantom: free 3D ghost flight plus intangibility.
//
// Intangible = capsule ignores world geometry and pawns (only PhaseBlocker
// volumes still block), damage immunity, no attacks, drains Ecto energy.
// Leaving intangibility while inside geometry is deferred until the capsule
// is clear; if energy runs out while stuck, Danny is returned to the last
// safe location.
UCLASS(ClassGroup = (Toon), meta = (BlueprintSpawnableComponent))
class MULTIVERSETOONS_API UGhostFlightComponent : public UToonTraversalComponent
{
	GENERATED_BODY()

public:
	UGhostFlightComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Ghost")
	void SetIntangible(bool bWantsIntangible);

	UFUNCTION(BlueprintPure, Category = "Ghost")
	bool IsIntangible() const { return bIntangible; }

	// True while the player has released intangibility but is still inside geometry.
	UFUNCTION(BlueprintPure, Category = "Ghost")
	bool IsWaitingToSolidify() const { return bIntangible && !bWantsIntangible; }

	UFUNCTION(BlueprintCallable, Category = "Ghost")
	void SetBoosting(bool bInBoosting) { bBoosting = bInBoosting; }

	UFUNCTION(BlueprintPure, Category = "Ghost")
	float GetEctoEnergy() const { return EctoEnergy; }

	UPROPERTY(BlueprintAssignable, Category = "Ghost")
	FOnIntangibilityChanged OnIntangibilityChanged;

	// --- Tuning (cm, cm/s) -------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Flight")
	float CruiseSpeed = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Flight")
	float BoostSpeed = 3500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Flight")
	float Acceleration = 4000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Flight")
	float Braking = 3000.0f;

	// Fraction of max speed available for pure vertical ascend/descend input.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Flight", meta = (ClampMin = "0", ClampMax = "1"))
	float VerticalSpeedScale = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Intangibility")
	float MaxEctoEnergy = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Intangibility")
	float IntangibleDrainPerSecond = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Intangibility")
	float RegenPerSecond = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Intangibility")
	float MinEnergyToPhase = 15.0f;

protected:
	virtual void OnTraversalStarted() override;
	virtual void OnTraversalStopped() override;

private:
	void ApplyCollisionForIntangibility(bool bPhase);
	bool IsOverlappingSolidGeometry() const;
	void FinishSolidify();
	void UpdateFlightSpeed();

	UPROPERTY(Transient)
	TObjectPtr<UCapsuleComponent> Capsule;

	FCollisionResponseContainer SavedResponses;
	FVector LastSafeLocation = FVector::ZeroVector;
	float EctoEnergy = 100.0f;
	float DefaultMaxFlySpeed = 600.0f;
	bool bIntangible = false;
	bool bWantsIntangible = false;
	bool bBoosting = false;
};
