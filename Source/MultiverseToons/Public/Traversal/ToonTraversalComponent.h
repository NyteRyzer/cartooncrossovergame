#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ToonTraversalComponent.generated.h"

class AToonCharacterBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTraversalToggled, bool, bActive);

// Base for each hero's signature traversal (ghost flight, dragon flight,
// air scooter, grappling chains...). The owning character forwards the
// Traversal and Vertical input actions here.
UCLASS(Abstract, Blueprintable, ClassGroup = (Toon))
class MULTIVERSETOONS_API UToonTraversalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UToonTraversalComponent();

	UFUNCTION(BlueprintCallable, Category = "Traversal")
	void ToggleTraversal();

	UFUNCTION(BlueprintCallable, Category = "Traversal")
	void StartTraversal();

	UFUNCTION(BlueprintCallable, Category = "Traversal")
	void StopTraversal();

	UFUNCTION(BlueprintPure, Category = "Traversal")
	bool IsTraversalActive() const { return bTraversalActive; }

	// -1..1, from the Vertical input action.
	virtual void SetVerticalInput(float Value) { VerticalInput = FMath::Clamp(Value, -1.0f, 1.0f); }

	UPROPERTY(BlueprintAssignable, Category = "Traversal")
	FOnTraversalToggled OnTraversalToggled;

protected:
	virtual bool CanStartTraversal() const { return true; }
	virtual void OnTraversalStarted() {}
	virtual void OnTraversalStopped() {}

	AToonCharacterBase* GetToonOwner() const;

	float VerticalInput = 0.0f;

private:
	bool bTraversalActive = false;
};
