#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/ToonTypes.h"
#include "Hub/ToonInteractable.h"
#include "DimensionalTerminal.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UToonHeroData;

// Nexus Observatory terminal. Interacting opens the hero select / mission UI
// (OnTerminalOpened, implemented in Blueprint), which calls SelectHero.
UCLASS()
class MULTIVERSETOONS_API ADimensionalTerminal : public AActor, public IToonInteractable
{
	GENERATED_BODY()

public:
	ADimensionalTerminal();

	virtual void Interact_Implementation(AToonCharacterBase* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	UFUNCTION(BlueprintCallable, Category = "Terminal")
	void SelectHero(AToonCharacterBase* Interactor, UToonHeroData* Hero);

	// Universe this terminal is tuned to; drives portal VFX and mission lists.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terminal")
	EToonUniverse Universe = EToonUniverse::Nexus;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terminal")
	TArray<TObjectPtr<UToonHeroData>> AvailableHeroes;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Terminal")
	void OnTerminalOpened(AToonCharacterBase* Interactor);

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Console;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USphereComponent> InteractionVolume;
};
