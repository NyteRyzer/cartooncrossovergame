#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Core/ToonTypes.h"
#include "ToonCharacterBase.generated.h"

class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
class UCameraComponent;
class UToonAbility;
class UToonTraversalComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUltimateMeterChanged, float, NewValue);

// Shared base for all eight heroes: third-person camera, Enhanced Input,
// the combat loadout and ultimate meter, a hook for the hero's signature
// traversal component, and per-universe stencil tagging for the outline pass.
UCLASS(Abstract)
class MULTIVERSETOONS_API AToonCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	AToonCharacterBase(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "Toon")
	EToonUniverse GetHomeUniverse() const { return HomeUniverse; }

	// Re-applies the stencil value. Call after swapping the skeletal mesh asset.
	UFUNCTION(BlueprintCallable, Category = "Toon|Rendering")
	void ApplyUniverseStencil();

	// --- Combat -----------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Toon|Combat")
	bool ActivateAbility(EToonAbilitySlot Slot);

	UFUNCTION(BlueprintPure, Category = "Toon|Combat")
	UToonAbility* GetAbility(EToonAbilitySlot Slot) const;

	// Replace a slot at runtime (used by Omnitrix forms, Aang's stance switch, etc.).
	UFUNCTION(BlueprintCallable, Category = "Toon|Combat")
	void SetAbility(EToonAbilitySlot Slot, TSubclassOf<UToonAbility> AbilityClass);

	UFUNCTION(BlueprintCallable, Category = "Toon|Combat")
	void RestoreDefaultAbilities();

	UFUNCTION(BlueprintPure, Category = "Toon|Combat")
	virtual bool CanUseAbilities() const;

	UFUNCTION(BlueprintPure, Category = "Toon|Combat")
	float GetUltimateMeter() const { return UltimateMeter; }

	UFUNCTION(BlueprintCallable, Category = "Toon|Combat")
	void AddUltimateMeter(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Toon|Combat")
	void SpendUltimateMeter(float Amount);

	UFUNCTION(BlueprintPure, Category = "Toon|Combat")
	float GetHealth() const { return Health; }

	// Named immunity sources (intangibility, Omnitrix i-frames, Elemental Sphere...).
	UFUNCTION(BlueprintCallable, Category = "Toon|Combat")
	void AddDamageImmunity(FName Source) { DamageImmunitySources.Add(Source); }

	UFUNCTION(BlueprintCallable, Category = "Toon|Combat")
	void RemoveDamageImmunity(FName Source) { DamageImmunitySources.Remove(Source); }

	UFUNCTION(BlueprintPure, Category = "Toon|Combat")
	bool IsDamageImmune() const { return DamageImmunitySources.Num() > 0; }

	// Blocks ability use without granting immunity (e.g. while phasing).
	UFUNCTION(BlueprintCallable, Category = "Toon|Combat")
	void AddAbilityBlock(FName Source) { AbilityBlockSources.Add(Source); }

	UFUNCTION(BlueprintCallable, Category = "Toon|Combat")
	void RemoveAbilityBlock(FName Source) { AbilityBlockSources.Remove(Source); }

	UPROPERTY(BlueprintAssignable, Category = "Toon|Combat")
	FOnUltimateMeterChanged OnUltimateMeterChanged;

	UFUNCTION(BlueprintPure, Category = "Toon|Traversal")
	UToonTraversalComponent* GetTraversal() const { return Traversal; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Toon|Combat")
	void OnDamaged(float Amount, AActor* DamageCauser);

	UFUNCTION(BlueprintImplementableEvent, Category = "Toon|Combat")
	void OnDefeated();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Toon")
	EToonUniverse HomeUniverse = EToonUniverse::Nexus;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Toon|Combat")
	float MaxHealth = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Toon|Combat")
	TMap<EToonAbilitySlot, TSubclassOf<UToonAbility>> DefaultAbilities;

	// Meter gained per point of damage taken (dealing damage is credited by abilities).
	UPROPERTY(EditDefaultsOnly, Category = "Toon|Combat")
	float MeterPerDamageTaken = 0.25f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	// --- Input ------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	// Float axis: +1 ascend / -1 descend while flying.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> VerticalAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> TraversalAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TMap<EToonAbilitySlot, TObjectPtr<UInputAction>> AbilityActions;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void VerticalInput(const FInputActionValue& Value);
	void ToggleTraversal();
	void Interact();
	void HandleAbilityInput(EToonAbilitySlot Slot);

private:
	UPROPERTY(Transient)
	TMap<EToonAbilitySlot, TObjectPtr<UToonAbility>> Abilities;

	UPROPERTY(Transient)
	TObjectPtr<UToonTraversalComponent> Traversal;

	TSet<FName> DamageImmunitySources;
	TSet<FName> AbilityBlockSources;
	float Health = 100.0f;
	float UltimateMeter = 0.0f;
	bool bDefeated = false;
};
