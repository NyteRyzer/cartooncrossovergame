#include "Hub/DimensionalTerminal.h"
#include "Characters/ToonCharacterBase.h"
#include "Hub/ToonPlayerController.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

#define LOCTEXT_NAMESPACE "DimensionalTerminal"

ADimensionalTerminal::ADimensionalTerminal()
{
	PrimaryActorTick.bCanEverTick = false;

	Console = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Console"));
	RootComponent = Console;

	InteractionVolume = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionVolume"));
	InteractionVolume->SetupAttachment(Console);
	InteractionVolume->InitSphereRadius(250.0f);
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionVolume->SetGenerateOverlapEvents(true);
}

void ADimensionalTerminal::Interact_Implementation(AToonCharacterBase* Interactor)
{
	OnTerminalOpened(Interactor);
}

FText ADimensionalTerminal::GetInteractionPrompt_Implementation() const
{
	return LOCTEXT("Prompt", "Use Dimensional Terminal");
}

void ADimensionalTerminal::SelectHero(AToonCharacterBase* Interactor, UToonHeroData* Hero)
{
	if (!Interactor || !AvailableHeroes.Contains(Hero))
	{
		return;
	}
	if (AToonPlayerController* PC = Cast<AToonPlayerController>(Interactor->GetController()))
	{
		PC->SwapToHero(Hero);
	}
}

#undef LOCTEXT_NAMESPACE
