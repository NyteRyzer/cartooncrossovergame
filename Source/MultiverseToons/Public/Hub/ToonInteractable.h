#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ToonInteractable.generated.h"

class AToonCharacterBase;

UINTERFACE(BlueprintType)
class UToonInteractable : public UInterface
{
	GENERATED_BODY()
};

class MULTIVERSETOONS_API IToonInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(AToonCharacterBase* Interactor);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionPrompt() const;
};
