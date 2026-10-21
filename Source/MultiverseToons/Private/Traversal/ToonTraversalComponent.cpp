#include "Traversal/ToonTraversalComponent.h"
#include "Characters/ToonCharacterBase.h"

UToonTraversalComponent::UToonTraversalComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

AToonCharacterBase* UToonTraversalComponent::GetToonOwner() const
{
	return Cast<AToonCharacterBase>(GetOwner());
}

void UToonTraversalComponent::ToggleTraversal()
{
	if (bTraversalActive)
	{
		StopTraversal();
	}
	else
	{
		StartTraversal();
	}
}

void UToonTraversalComponent::StartTraversal()
{
	if (bTraversalActive || !CanStartTraversal())
	{
		return;
	}
	bTraversalActive = true;
	OnTraversalStarted();
	OnTraversalToggled.Broadcast(true);
}

void UToonTraversalComponent::StopTraversal()
{
	if (!bTraversalActive)
	{
		return;
	}
	bTraversalActive = false;
	OnTraversalStopped();
	OnTraversalToggled.Broadcast(false);
}
