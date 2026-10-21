#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ToonAbility.generated.h"

class AToonCharacterBase;

// Lightweight ability object for the v0 combat loadout (Light, Heavy, three
// Signatures, Ultimate). Gameplay lives in Blueprint subclasses via OnActivated.
// Intended to be replaced by / wrapped in GAS once combat design settles.
UCLASS(Abstract, Blueprintable, BlueprintType)
class MULTIVERSETOONS_API UToonAbility : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (ClampMin = "0"))
	float Cooldown = 0.0f;

	// Ultimate meter consumed on activation (0-100).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (ClampMin = "0", ClampMax = "100"))
	float MeterCost = 0.0f;

	void Initialize(AToonCharacterBase* InOwner) { OwnerCharacter = InOwner; }

	UFUNCTION(BlueprintCallable, Category = "Ability")
	bool TryActivate();

	UFUNCTION(BlueprintPure, Category = "Ability")
	bool CanActivate() const;

	UFUNCTION(BlueprintPure, Category = "Ability")
	float GetCooldownRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Ability")
	AToonCharacterBase* GetOwnerCharacter() const { return OwnerCharacter; }

	virtual UWorld* GetWorld() const override;

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Ability")
	bool CanActivateExtra() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Ability")
	void OnActivated();

private:
	UPROPERTY(Transient)
	TObjectPtr<AToonCharacterBase> OwnerCharacter;

	double LastActivationTime = -1.0e9;
};
