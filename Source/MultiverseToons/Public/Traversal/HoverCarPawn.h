#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "Hub/ToonInteractable.h"
#include "Traversal/HoverMath.h"
#include "HoverCarPawn.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class AToonCharacterBase;

// Jimmy Neutron's hover car. A physics-simulated body held up by spring-damper
// hover points (HoverMath), with arcade steering and lateral grip. Lift input
// raises the cruise altitude so the car can climb over Retroville rooftops.
UCLASS()
class MULTIVERSETOONS_API AHoverCarPawn : public APawn, public IToonInteractable
{
	GENERATED_BODY()

public:
	AHoverCarPawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;

	virtual void Interact_Implementation(AToonCharacterBase* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	UFUNCTION(BlueprintCallable, Category = "Hover Car")
	bool EnterVehicle(AToonCharacterBase* NewDriver);

	UFUNCTION(BlueprintCallable, Category = "Hover Car")
	void ExitVehicle();

	UFUNCTION(BlueprintPure, Category = "Hover Car")
	AToonCharacterBase* GetDriver() const { return Driver; }

	UFUNCTION(BlueprintPure, Category = "Hover Car")
	float GetForwardSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Hover Car")
	float GetCruiseAltitude() const { return CruiseAltitude; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> InteractionVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	// Hover points in body-local space (cm). Defaults to the four corners.
	UPROPERTY(EditAnywhere, Category = "Hover Car|Hover")
	TArray<FVector> HoverPoints;

	UPROPERTY(EditAnywhere, Category = "Hover Car|Hover")
	float MinCruiseAltitude = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Hover Car|Hover")
	float MaxCruiseAltitude = 2500.0f;

	// cm/s the cruise altitude changes at full Lift input.
	UPROPERTY(EditAnywhere, Category = "Hover Car|Hover")
	float LiftRate = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Hover Car|Hover")
	float Stiffness = 60.0f;

	UPROPERTY(EditAnywhere, Category = "Hover Car|Hover")
	float Damping = 10.0f;

	// Torque (deg/s^2 per degree of tilt) pulling the car level.
	UPROPERTY(EditAnywhere, Category = "Hover Car|Hover")
	float UprightStrength = 8.0f;

	UPROPERTY(EditAnywhere, Category = "Hover Car|Drive")
	float TopSpeed = 2500.0f;

	UPROPERTY(EditAnywhere, Category = "Hover Car|Drive")
	float BoostTopSpeed = 4200.0f;

	UPROPERTY(EditAnywhere, Category = "Hover Car|Drive")
	float MaxAcceleration = 1600.0f;

	UPROPERTY(EditAnywhere, Category = "Hover Car|Drive")
	float TurnRateDegrees = 110.0f;

	UPROPERTY(EditAnywhere, Category = "Hover Car|Drive")
	float LateralGrip = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Hover Car|Drive")
	FName DriverSeatSocket = TEXT("DriverSeat");

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> VehicleMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ThrottleAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> SteerAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LiftAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> BoostAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ExitAction;

private:
	void OnThrottle(const FInputActionValue& Value) { Throttle = Value.Get<float>(); }
	void OnSteer(const FInputActionValue& Value) { Steer = Value.Get<float>(); }
	void OnLift(const FInputActionValue& Value) { Lift = Value.Get<float>(); }
	void OnBoost(const FInputActionValue& Value) { bBoosting = Value.Get<bool>(); }
	void OnLook(const FInputActionValue& Value);

	void ApplyHover(float DeltaSeconds);
	void ApplyDrive(float DeltaSeconds);

	UPROPERTY(Transient)
	TObjectPtr<AToonCharacterBase> Driver;

	TArray<float> PreviousDistances;
	float CruiseAltitude = 120.0f;
	float Throttle = 0.0f;
	float Steer = 0.0f;
	float Lift = 0.0f;
	bool bBoosting = false;
};
