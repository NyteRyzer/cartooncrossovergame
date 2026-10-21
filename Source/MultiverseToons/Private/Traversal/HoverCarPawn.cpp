#include "Traversal/HoverCarPawn.h"
#include "Characters/ToonCharacterBase.h"
#include "MultiverseToons.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

#define LOCTEXT_NAMESPACE "HoverCar"

AHoverCarPawn::AHoverCarPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	Body->SetSimulatePhysics(true);
	Body->SetCollisionProfileName(UCollisionProfile::Vehicle_ProfileName);
	Body->SetLinearDamping(0.3f);
	Body->SetAngularDamping(3.0f);
	Body->BodyInstance.bOverrideMass = true;
	Body->BodyInstance.SetMassOverride(900.0f);

	InteractionVolume = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionVolume"));
	InteractionVolume->SetupAttachment(Body);
	InteractionVolume->InitSphereRadius(300.0f);
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionVolume->SetGenerateOverlapEvents(true);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Body);
	CameraBoom->TargetArmLength = 800.0f;
	CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 200.0f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 8.0f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	HoverPoints = {
		FVector( 180.0f,  90.0f, 0.0f),
		FVector( 180.0f, -90.0f, 0.0f),
		FVector(-180.0f,  90.0f, 0.0f),
		FVector(-180.0f, -90.0f, 0.0f)
	};
}

void AHoverCarPawn::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	Throttle = Steer = Lift = 0.0f;
	bBoosting = false;

	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (VehicleMappingContext)
			{
				Subsystem->AddMappingContext(VehicleMappingContext, 1);
			}
		}
	}
}

void AHoverCarPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		return;
	}

	auto BindAxis = [Input, this](UInputAction* Action, void (AHoverCarPawn::*Func)(const FInputActionValue&))
	{
		if (Action)
		{
			Input->BindAction(Action, ETriggerEvent::Triggered, this, Func);
			Input->BindAction(Action, ETriggerEvent::Completed, this, Func);
		}
	};
	BindAxis(ThrottleAction, &AHoverCarPawn::OnThrottle);
	BindAxis(SteerAction, &AHoverCarPawn::OnSteer);
	BindAxis(LiftAction, &AHoverCarPawn::OnLift);
	BindAxis(BoostAction, &AHoverCarPawn::OnBoost);
	if (LookAction)
	{
		Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHoverCarPawn::OnLook);
	}
	if (ExitAction)
	{
		Input->BindAction(ExitAction, ETriggerEvent::Started, this, &AHoverCarPawn::ExitVehicle);
	}
}

void AHoverCarPawn::OnLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AHoverCarPawn::Interact_Implementation(AToonCharacterBase* Interactor)
{
	EnterVehicle(Interactor);
}

FText AHoverCarPawn::GetInteractionPrompt_Implementation() const
{
	return LOCTEXT("EnterPrompt", "Drive Hover Car");
}

bool AHoverCarPawn::EnterVehicle(AToonCharacterBase* NewDriver)
{
	if (!NewDriver || Driver)
	{
		return false;
	}
	AController* DriverController = NewDriver->GetController();
	if (!DriverController)
	{
		return false;
	}

	Driver = NewDriver;
	Driver->GetCharacterMovement()->DisableMovement();
	Driver->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Driver->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, DriverSeatSocket);

	const FRotator ControlRotation = DriverController->GetControlRotation();
	DriverController->Possess(this);
	DriverController->SetControlRotation(ControlRotation);
	return true;
}

void AHoverCarPawn::ExitVehicle()
{
	if (!Driver)
	{
		return;
	}
	AController* PC = GetController();
	AToonCharacterBase* Exiting = Driver;
	Driver = nullptr;

	Exiting->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	const FVector ExitLocation = GetActorLocation() + GetActorRightVector() * -220.0f + FVector(0.0f, 0.0f, 50.0f);
	Exiting->TeleportTo(ExitLocation, FRotator(0.0f, GetActorRotation().Yaw, 0.0f));
	Exiting->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Exiting->GetCharacterMovement()->SetMovementMode(MOVE_Falling);

	if (PC)
	{
		PC->Possess(Exiting);
	}
}

float AHoverCarPawn::GetForwardSpeed() const
{
	return FVector::DotProduct(Body->GetPhysicsLinearVelocity(), GetActorForwardVector());
}

void AHoverCarPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Body->IsSimulatingPhysics() || DeltaSeconds <= 0.0f)
	{
		return;
	}
	CruiseAltitude = FMath::Clamp(CruiseAltitude + Lift * LiftRate * DeltaSeconds, MinCruiseAltitude, MaxCruiseAltitude);
	ApplyHover(DeltaSeconds);
	ApplyDrive(DeltaSeconds);
}

void AHoverCarPawn::ApplyHover(float DeltaSeconds)
{
	HoverMath::FSpringParams Params;
	Params.TargetHeight = CruiseAltitude;
	Params.MaxRayLength = CruiseAltitude + 300.0f;
	Params.Stiffness = Stiffness;
	Params.Damping = Damping;

	const int32 NumPoints = HoverPoints.Num();
	if (NumPoints == 0)
	{
		return;
	}
	if (PreviousDistances.Num() != NumPoints)
	{
		PreviousDistances.Init(Params.MaxRayLength, NumPoints);
	}

	const FTransform BodyTransform = Body->GetComponentTransform();
	const float Mass = Body->GetMass();
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HoverCarRay), false, this);
	if (Driver)
	{
		QueryParams.AddIgnoredActor(Driver);
	}

	for (int32 i = 0; i < NumPoints; ++i)
	{
		const FVector Start = BodyTransform.TransformPosition(HoverPoints[i]);
		const FVector End = Start - FVector::UpVector * Params.MaxRayLength;

		FHitResult Hit;
		const float Distance = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams)
			? Hit.Distance
			: Params.MaxRayLength;

		const float Accel = HoverMath::SpringAcceleration(Params, Distance, PreviousDistances[i], DeltaSeconds);
		PreviousDistances[i] = Distance;
		if (Accel > 0.0f)
		{
			Body->AddForceAtLocation(FVector::UpVector * (Accel * Mass / NumPoints), Start);
		}
	}

	// Self-righting torque: rotate the body's up vector back toward world up.
	const FVector Up = Body->GetUpVector();
	const FVector Axis = FVector::CrossProduct(Up, FVector::UpVector);
	const float TiltDegrees = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Axis.Size(), 0.0f, 1.0f)));
	if (TiltDegrees > KINDA_SMALL_NUMBER)
	{
		Body->AddTorqueInDegrees(Axis.GetSafeNormal() * TiltDegrees * UprightStrength, NAME_None, true);
	}
}

void AHoverCarPawn::ApplyDrive(float DeltaSeconds)
{
	const FVector Forward = FVector::VectorPlaneProject(GetActorForwardVector(), FVector::UpVector).GetSafeNormal();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	FVector Velocity = Body->GetPhysicsLinearVelocity();
	const float ForwardSpeed = FVector::DotProduct(Velocity, Forward);

	const float Accel = HoverMath::ThrottleAcceleration(Throttle, ForwardSpeed, MaxAcceleration, bBoosting ? BoostTopSpeed : TopSpeed);
	Body->AddForce(Forward * Accel, NAME_None, true);

	// Kill a fraction of sideways drift each frame so turns feel planted.
	const float Lateral = FVector::DotProduct(Velocity, Right);
	Velocity -= Right * Lateral * HoverMath::LateralGripAlpha(LateralGrip, DeltaSeconds);
	Body->SetPhysicsLinearVelocity(Velocity);

	// Arcade yaw: drive angular velocity directly; reverse steering when backing up.
	const float SteerSign = ForwardSpeed < -50.0f ? -1.0f : 1.0f;
	FVector Angular = Body->GetPhysicsAngularVelocityInDegrees();
	const float TargetYawRate = Steer * TurnRateDegrees * SteerSign;
	Angular.Z = FMath::FInterpTo(Angular.Z, TargetYawRate, DeltaSeconds, 6.0f);
	Body->SetPhysicsAngularVelocityInDegrees(Angular);
}

#undef LOCTEXT_NAMESPACE
