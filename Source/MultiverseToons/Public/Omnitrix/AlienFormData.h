#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/ToonTypes.h"
#include "AlienFormData.generated.h"

class USkeletalMesh;
class UAnimInstance;
class UToonAbility;

// One Omnitrix alien (Heatblast, Four Arms, Diamondhead, XLR8, Cannonbolt, Way Big...).
// Visual assets are soft references so the Omnitrix component can stream them
// in ahead of time and swap without hitches.
UCLASS(BlueprintType)
class MULTIVERSETOONS_API UAlienFormData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien|UI")
	TSoftObjectPtr<UTexture2D> DialIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien|Visuals")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien|Visuals")
	TSoftClassPtr<UAnimInstance> AnimClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien|Body")
	float CapsuleRadius = 34.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien|Body")
	float CapsuleHalfHeight = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien|Movement")
	float MaxWalkSpeed = 650.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien|Movement")
	float JumpZVelocity = 900.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien|Movement")
	float GravityScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien|Movement")
	float AirControl = 0.6f;

	// Slot overrides while in this form; empty slots keep Ben's defaults.
	UPROPERTY(EditDefaultsOnly, Category = "Alien|Combat")
	TMap<EToonAbilitySlot, TSubclassOf<UToonAbility>> Abilities;

	TArray<FSoftObjectPath> GetAssetsToLoad() const
	{
		TArray<FSoftObjectPath> Paths;
		if (!Mesh.IsNull())
		{
			Paths.Add(Mesh.ToSoftObjectPath());
		}
		if (!AnimClass.IsNull())
		{
			Paths.Add(AnimClass.ToSoftObjectPath());
		}
		return Paths;
	}

	bool AreAssetsLoaded() const
	{
		return (Mesh.IsNull() || Mesh.Get()) && (AnimClass.IsNull() || AnimClass.Get());
	}

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("AlienForm"), GetFName());
	}
};
