#include "Hub/ToonPlayerController.h"
#include "Characters/ToonCharacterBase.h"
#include "Core/ToonHeroData.h"
#include "MultiverseToons.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"

void AToonPlayerController::SwapToHero(UToonHeroData* Hero)
{
	if (!Hero || Hero->CharacterClass.IsNull())
	{
		return;
	}
	if (SwapHandle.IsValid())
	{
		SwapHandle->CancelHandle();
	}
	PendingHero = Hero;
	SwapHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Hero->CharacterClass.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &AToonPlayerController::FinishSwap));
}

void AToonPlayerController::FinishSwap()
{
	SwapHandle.Reset();
	UToonHeroData* Hero = PendingHero;
	PendingHero = nullptr;

	UClass* HeroClass = Hero ? Hero->CharacterClass.Get() : nullptr;
	if (!HeroClass)
	{
		UE_LOG(LogToons, Warning, TEXT("SwapToHero: character class failed to load"));
		return;
	}

	APawn* OldPawn = GetPawn();
	FTransform SpawnTransform = OldPawn ? OldPawn->GetActorTransform() : FTransform::Identity;
	SpawnTransform.SetScale3D(FVector::OneVector);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Params.Owner = this;

	AToonCharacterBase* NewHero = GetWorld()->SpawnActor<AToonCharacterBase>(HeroClass, SpawnTransform, Params);
	if (!NewHero)
	{
		return;
	}

	const FRotator ControlRot = GetControlRotation();
	Possess(NewHero);
	SetControlRotation(ControlRot);
	if (OldPawn)
	{
		OldPawn->Destroy();
	}

	CurrentHero = Hero;
	OnHeroSwapped.Broadcast(NewHero);
}

void AToonPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SwapHandle.IsValid())
	{
		SwapHandle->CancelHandle();
		SwapHandle.Reset();
	}
	Super::EndPlay(EndPlayReason);
}
