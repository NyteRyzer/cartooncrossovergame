#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ToonRenderLibrary.generated.h"

class UPrimitiveComponent;

UCLASS()
class TOONRENDER_API UToonRenderLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Stencil value 0 means "no toon line style" (environment, sky).
	static constexpr int32 StencilOffset = 1;

	// Enables custom depth on the component and writes UniverseIndex + 1 to the
	// stencil buffer. PP_ToonOutline looks this up in T_UniverseLineStyles to
	// draw each show's native line weight and colour.
	UFUNCTION(BlueprintCallable, Category = "Toon|Rendering")
	static void TagComponentWithUniverse(UPrimitiveComponent* Component, int32 UniverseIndex);
};
