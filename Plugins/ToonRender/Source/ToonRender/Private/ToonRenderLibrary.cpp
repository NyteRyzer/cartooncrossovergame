#include "ToonRenderLibrary.h"
#include "Components/PrimitiveComponent.h"

void UToonRenderLibrary::TagComponentWithUniverse(UPrimitiveComponent* Component, int32 UniverseIndex)
{
	if (!Component)
	{
		return;
	}
	Component->SetRenderCustomDepth(true);
	Component->SetCustomDepthStencilValue(FMath::Clamp(UniverseIndex + StencilOffset, 0, 255));
}
