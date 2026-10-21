#include "Abilities/ToonAbility.h"
#include "Characters/ToonCharacterBase.h"
#include "Engine/World.h"

UWorld* UToonAbility::GetWorld() const
{
	return OwnerCharacter ? OwnerCharacter->GetWorld() : nullptr;
}

float UToonAbility::GetCooldownRemaining() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}
	return FMath::Max(0.0f, static_cast<float>(LastActivationTime + Cooldown - World->GetTimeSeconds()));
}

bool UToonAbility::CanActivate() const
{
	if (!OwnerCharacter || GetCooldownRemaining() > 0.0f || !OwnerCharacter->CanUseAbilities())
	{
		return false;
	}
	if (MeterCost > 0.0f && OwnerCharacter->GetUltimateMeter() < MeterCost)
	{
		return false;
	}
	return CanActivateExtra();
}

bool UToonAbility::TryActivate()
{
	if (!CanActivate())
	{
		return false;
	}
	if (MeterCost > 0.0f)
	{
		OwnerCharacter->SpendUltimateMeter(MeterCost);
	}
	LastActivationTime = GetWorld()->GetTimeSeconds();
	OnActivated();
	return true;
}

bool UToonAbility::CanActivateExtra_Implementation() const
{
	return true;
}

void UToonAbility::OnActivated_Implementation()
{
}
