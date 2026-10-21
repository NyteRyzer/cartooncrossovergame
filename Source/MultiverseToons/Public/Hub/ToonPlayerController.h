#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ToonPlayerController.generated.h"

class UToonHeroData;
class AToonCharacterBase;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHeroSwapped, AToonCharacterBase*, NewHero);

UCLASS()
class MULTIVERSETOONS_API AToonPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// Streams the hero's character class, spawns it where the current pawn
	// stands, possesses it and destroys the old pawn.
	UFUNCTION(BlueprintCallable, Category = "Toon|Roster")
	void SwapToHero(UToonHeroData* Hero);

	UFUNCTION(BlueprintPure, Category = "Toon|Roster")
	UToonHeroData* GetCurrentHero() const { return CurrentHero; }

	UPROPERTY(BlueprintAssignable, Category = "Toon|Roster")
	FOnHeroSwapped OnHeroSwapped;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void FinishSwap();

	UPROPERTY(Transient)
	TObjectPtr<UToonHeroData> CurrentHero;

	UPROPERTY(Transient)
	TObjectPtr<UToonHeroData> PendingHero;

	TSharedPtr<FStreamableHandle> SwapHandle;
};
