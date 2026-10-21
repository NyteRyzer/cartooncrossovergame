#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/ToonTypes.h"
#include "ToonHeroData.generated.h"

class AToonCharacterBase;

// One entry in the playable roster. Dimensional terminals in the Nexus list
// these and the player controller swaps to the referenced character class.
UCLASS(BlueprintType)
class MULTIVERSETOONS_API UToonHeroData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hero")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hero")
	FText Archetype;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hero")
	EToonUniverse HomeUniverse = EToonUniverse::Nexus;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hero")
	TSoftClassPtr<AToonCharacterBase> CharacterClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hero|UI")
	TSoftObjectPtr<UTexture2D> Portrait;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("ToonHero"), GetFName());
	}
};
