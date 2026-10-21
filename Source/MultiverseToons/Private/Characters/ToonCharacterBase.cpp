#include "Characters/ToonCharacterBase.h"
#include "Abilities/ToonAbility.h"
#include "Traversal/ToonTraversalComponent.h"
#include "Hub/ToonInteractable.h"
#include "ToonRenderLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

AToonCharacterBase::AToonCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	// Most heroes are kids: ~140 cm tall. See docs/TECH_DESIGN.md "World scale".
	GetCapsuleComponent()->InitCapsuleSize(32.0f, 70.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement = true;
	MoveComp->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	MoveComp->MaxWalkSpeed = 650.0f;
	// 900 cm/s against 1960 cm/s^2 gravity peaks at ~2 m (see the Nexus scale gallery).
	MoveComp->JumpZVelocity = 900.0f;
	MoveComp->AirControl = 0.6f;
	MoveComp->GravityScale = 1.0f;
	MoveComp->BrakingDecelerationWalking = 2400.0f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 450.0f;
	CameraBoom->SocketOffset = FVector(0.0f, 60.0f, 80.0f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.0f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void AToonCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;
	Traversal = FindComponentByClass<UToonTraversalComponent>();
	RestoreDefaultAbilities();
	ApplyUniverseStencil();
}

void AToonCharacterBase::ApplyUniverseStencil()
{
	UToonRenderLibrary::TagComponentWithUniverse(GetMesh(), static_cast<int32>(HomeUniverse));
}

void AToonCharacterBase::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void AToonCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		return;
	}

	if (MoveAction)
	{
		Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AToonCharacterBase::Move);
	}
	if (LookAction)
	{
		Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AToonCharacterBase::Look);
	}
	if (JumpAction)
	{
		Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
	if (VerticalAction)
	{
		Input->BindAction(VerticalAction, ETriggerEvent::Triggered, this, &AToonCharacterBase::VerticalInput);
		Input->BindAction(VerticalAction, ETriggerEvent::Completed, this, &AToonCharacterBase::VerticalInput);
	}
	if (TraversalAction)
	{
		Input->BindAction(TraversalAction, ETriggerEvent::Started, this, &AToonCharacterBase::ToggleTraversal);
	}
	if (InteractAction)
	{
		Input->BindAction(InteractAction, ETriggerEvent::Started, this, &AToonCharacterBase::Interact);
	}
	for (const TPair<EToonAbilitySlot, TObjectPtr<UInputAction>>& Pair : AbilityActions)
	{
		if (Pair.Value)
		{
			Input->BindAction(Pair.Value, ETriggerEvent::Started, this, &AToonCharacterBase::HandleAbilityInput, Pair.Key);
		}
	}
}

void AToonCharacterBase::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller)
	{
		return;
	}
	const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
	const FRotationMatrix YawMatrix(YawRotation);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), Axis.X);
}

void AToonCharacterBase::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AToonCharacterBase::VerticalInput(const FInputActionValue& Value)
{
	if (Traversal)
	{
		Traversal->SetVerticalInput(Value.Get<float>());
	}
}

void AToonCharacterBase::ToggleTraversal()
{
	if (Traversal)
	{
		Traversal->ToggleTraversal();
	}
}

void AToonCharacterBase::Interact()
{
	TArray<AActor*> Overlapping;
	GetOverlappingActors(Overlapping);

	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (AActor* Actor : Overlapping)
	{
		if (Actor && Actor->Implements<UToonInteractable>())
		{
			const float DistSq = FVector::DistSquared(Actor->GetActorLocation(), GetActorLocation());
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Actor;
			}
		}
	}
	if (Best)
	{
		IToonInteractable::Execute_Interact(Best, this);
	}
}

void AToonCharacterBase::HandleAbilityInput(EToonAbilitySlot Slot)
{
	ActivateAbility(Slot);
}

bool AToonCharacterBase::ActivateAbility(EToonAbilitySlot Slot)
{
	UToonAbility* Ability = GetAbility(Slot);
	return Ability && Ability->TryActivate();
}

UToonAbility* AToonCharacterBase::GetAbility(EToonAbilitySlot Slot) const
{
	const TObjectPtr<UToonAbility>* Found = Abilities.Find(Slot);
	return Found ? Found->Get() : nullptr;
}

void AToonCharacterBase::SetAbility(EToonAbilitySlot Slot, TSubclassOf<UToonAbility> AbilityClass)
{
	if (!AbilityClass)
	{
		Abilities.Remove(Slot);
		return;
	}
	UToonAbility* Ability = NewObject<UToonAbility>(this, AbilityClass);
	Ability->Initialize(this);
	Abilities.Add(Slot, Ability);
}

void AToonCharacterBase::RestoreDefaultAbilities()
{
	Abilities.Reset();
	for (const TPair<EToonAbilitySlot, TSubclassOf<UToonAbility>>& Pair : DefaultAbilities)
	{
		SetAbility(Pair.Key, Pair.Value);
	}
}

bool AToonCharacterBase::CanUseAbilities() const
{
	return !bDefeated && AbilityBlockSources.Num() == 0;
}

void AToonCharacterBase::AddUltimateMeter(float Amount)
{
	const float NewValue = FMath::Clamp(UltimateMeter + Amount, 0.0f, 100.0f);
	if (NewValue != UltimateMeter)
	{
		UltimateMeter = NewValue;
		OnUltimateMeterChanged.Broadcast(UltimateMeter);
	}
}

void AToonCharacterBase::SpendUltimateMeter(float Amount)
{
	AddUltimateMeter(-Amount);
}

float AToonCharacterBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDefeated || IsDamageImmune())
	{
		return 0.0f;
	}
	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (Applied <= 0.0f)
	{
		return 0.0f;
	}

	Health = FMath::Max(0.0f, Health - Applied);
	AddUltimateMeter(Applied * MeterPerDamageTaken);
	OnDamaged(Applied, DamageCauser);

	if (Health <= 0.0f)
	{
		bDefeated = true;
		OnDefeated();
	}
	return Applied;
}
