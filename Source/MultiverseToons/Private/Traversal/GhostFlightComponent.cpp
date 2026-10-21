#include "Traversal/GhostFlightComponent.h"
#include "Characters/ToonCharacterBase.h"
#include "Core/ToonTypes.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	const FName IntangibleSource(TEXT("GhostIntangibility"));
}

UGhostFlightComponent::UGhostFlightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UGhostFlightComponent::BeginPlay()
{
	Super::BeginPlay();

	EctoEnergy = MaxEctoEnergy;
	if (const AToonCharacterBase* Owner = GetToonOwner())
	{
		Capsule = Owner->GetCapsuleComponent();
		DefaultMaxFlySpeed = Owner->GetCharacterMovement()->MaxFlySpeed;
		LastSafeLocation = Owner->GetActorLocation();
	}
}

void UGhostFlightComponent::OnTraversalStarted()
{
	AToonCharacterBase* Owner = GetToonOwner();
	if (!Owner)
	{
		return;
	}
	UCharacterMovementComponent* Move = Owner->GetCharacterMovement();
	Move->SetMovementMode(MOVE_Flying);
	Move->BrakingDecelerationFlying = Braking;
	UpdateFlightSpeed();
}

void UGhostFlightComponent::OnTraversalStopped()
{
	if (bIntangible)
	{
		// Landing while phased would drop Danny through the floor: solidify
		// first, FinishSolidify() switches to falling once clear.
		SetIntangible(false);
		return;
	}
	if (AToonCharacterBase* Owner = GetToonOwner())
	{
		UCharacterMovementComponent* Move = Owner->GetCharacterMovement();
		Move->MaxFlySpeed = DefaultMaxFlySpeed;
		Move->SetMovementMode(MOVE_Falling);
	}
}

void UGhostFlightComponent::UpdateFlightSpeed()
{
	if (AToonCharacterBase* Owner = GetToonOwner())
	{
		UCharacterMovementComponent* Move = Owner->GetCharacterMovement();
		Move->MaxFlySpeed = bBoosting ? BoostSpeed : CruiseSpeed;
		Move->MaxAcceleration = Acceleration;
	}
}

void UGhostFlightComponent::SetIntangible(bool bInWantsIntangible)
{
	AToonCharacterBase* Owner = GetToonOwner();
	if (!Owner || !Capsule)
	{
		return;
	}

	bWantsIntangible = bInWantsIntangible;
	if (bWantsIntangible && !bIntangible)
	{
		if (EctoEnergy < MinEnergyToPhase)
		{
			bWantsIntangible = false;
			return;
		}
		// Phasing always flies: there's no floor to stand on.
		StartTraversal();
		bIntangible = true;
		ApplyCollisionForIntangibility(true);
		Owner->AddDamageImmunity(IntangibleSource);
		Owner->AddAbilityBlock(IntangibleSource);
		OnIntangibilityChanged.Broadcast(true);
	}
	else if (!bWantsIntangible && bIntangible && !IsOverlappingSolidGeometry())
	{
		FinishSolidify();
	}
	// Otherwise TickComponent keeps checking until the capsule is clear.
}

void UGhostFlightComponent::FinishSolidify()
{
	AToonCharacterBase* Owner = GetToonOwner();
	bIntangible = false;
	bWantsIntangible = false;
	ApplyCollisionForIntangibility(false);
	if (Owner)
	{
		Owner->RemoveDamageImmunity(IntangibleSource);
		Owner->RemoveAbilityBlock(IntangibleSource);
		if (!IsTraversalActive())
		{
			OnTraversalStopped();
		}
	}
	OnIntangibilityChanged.Broadcast(false);
}

void UGhostFlightComponent::ApplyCollisionForIntangibility(bool bPhase)
{
	if (bPhase)
	{
		SavedResponses = Capsule->GetCollisionResponseToChannels();
		Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
		Capsule->SetCollisionResponseToChannel(ToonCollision::PhaseBlocker, ECR_Block);
	}
	else
	{
		Capsule->SetCollisionResponseToChannels(SavedResponses);
	}
}

bool UGhostFlightComponent::IsOverlappingSolidGeometry() const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Capsule)
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GhostSolidifyTest), false, Owner);

	// Slightly shrunk so resting against a wall doesn't count as "inside" it.
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(
		FMath::Max(1.0f, Capsule->GetScaledCapsuleRadius() - 2.0f),
		FMath::Max(1.0f, Capsule->GetScaledCapsuleHalfHeight() - 2.0f));

	return GetWorld()->OverlapAnyTestByObjectType(Owner->GetActorLocation(), Owner->GetActorQuat(), ObjectParams, Shape, QueryParams);
}

void UGhostFlightComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AToonCharacterBase* Owner = GetToonOwner();
	if (!Owner)
	{
		return;
	}

	if (IsTraversalActive())
	{
		UpdateFlightSpeed();
		if (!FMath::IsNearlyZero(VerticalInput))
		{
			Owner->AddMovementInput(FVector::UpVector, VerticalInput * VerticalSpeedScale);
		}
	}

	if (!bIntangible)
	{
		EctoEnergy = FMath::Min(MaxEctoEnergy, EctoEnergy + RegenPerSecond * DeltaTime);
		LastSafeLocation = Owner->GetActorLocation();
		return;
	}

	EctoEnergy = FMath::Max(0.0f, EctoEnergy - IntangibleDrainPerSecond * DeltaTime);
	const bool bInsideGeometry = IsOverlappingSolidGeometry();
	if (!bInsideGeometry)
	{
		LastSafeLocation = Owner->GetActorLocation();
	}

	if (EctoEnergy <= 0.0f)
	{
		if (bInsideGeometry)
		{
			Owner->TeleportTo(LastSafeLocation, Owner->GetActorRotation(), false, true);
		}
		FinishSolidify();
	}
	else if (!bWantsIntangible && !bInsideGeometry)
	{
		FinishSolidify();
	}
}
