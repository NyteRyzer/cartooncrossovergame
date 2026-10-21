#include "Omnitrix/OmnitrixComponent.h"
#include "Omnitrix/AlienFormData.h"
#include "Characters/ToonCharacterBase.h"
#include "Abilities/ToonAbility.h"
#include "MultiverseToons.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

static_assert(static_cast<uint8>(EOmnitrixState::Overclocked) == static_cast<uint8>(Omnitrix::EState::Overclocked),
	"EOmnitrixState must mirror Omnitrix::EState");

namespace
{
	const FName OmnitrixIFrames(TEXT("OmnitrixIFrames"));
	const FName OmnitrixInputLock(TEXT("OmnitrixInputLock"));
}

UOmnitrixComponent::UOmnitrixComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

AToonCharacterBase* UOmnitrixComponent::GetCharacter() const
{
	return Cast<AToonCharacterBase>(GetOwner());
}

void UOmnitrixComponent::BeginPlay()
{
	Super::BeginPlay();

	Omnitrix::FConfig Config;
	Config.NumSlots = Playlist.Num();
	Config.TransformTime = TransformTime;
	Config.RevertTime = RevertTime;
	Config.AlienDuration = AlienDuration;
	Config.RechargeTime = RechargeTime;
	Config.SwapTimePenalty = SwapTimePenalty;
	Config.GlitchDuration = GlitchDuration;
	Config.GlitchCooldown = GlitchCooldown;
	Config.OverclockDuration = OverclockDuration;
	Machine = Omnitrix::FStateMachine(Config);

	if (AToonCharacterBase* Character = GetCharacter())
	{
		USkeletalMeshComponent* Mesh = Character->GetMesh();
		HumanMesh = Mesh->GetSkeletalMeshAsset();
		HumanAnimClass = Mesh->GetAnimClass();
		Human.CapsuleRadius = Character->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
		Human.CapsuleHalfHeight = Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
		Human.MaxWalkSpeed = Move->MaxWalkSpeed;
		Human.JumpZVelocity = Move->JumpZVelocity;
		Human.GravityScale = Move->GravityScale;
		Human.AirControl = Move->AirControl;
	}

	// Pre-stream the dial forms and the glitch form: those swaps must be instant.
	// The overclock form (potentially huge) streams on demand.
	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	TArray<const UAlienFormData*> Preload;
	for (const UAlienFormData* Form : Playlist)
	{
		Preload.Add(Form);
	}
	Preload.Add(GlitchForm);
	for (const UAlienFormData* Form : Preload)
	{
		if (Form)
		{
			const TArray<FSoftObjectPath> Paths = Form->GetAssetsToLoad();
			if (Paths.Num() > 0)
			{
				PreloadHandles.Add(Streamable.RequestAsyncLoad(Paths, FStreamableDelegate()));
			}
		}
	}
}

void UOmnitrixComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bTimeDilated)
	{
		UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
		bTimeDilated = false;
	}
	for (TSharedPtr<FStreamableHandle>& Handle : PreloadHandles)
	{
		if (Handle.IsValid())
		{
			Handle->CancelHandle();
		}
	}
	PreloadHandles.Reset();
	if (PendingHandle.IsValid())
	{
		PendingHandle->CancelHandle();
		PendingHandle.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

UAlienFormData* UOmnitrixComponent::ResolveForm(int FormId) const
{
	if (FormId == Omnitrix::GlitchForm)
	{
		return GlitchForm;
	}
	if (FormId == Omnitrix::OverclockForm)
	{
		return OverclockForm;
	}
	return Playlist.IsValidIndex(FormId) ? Playlist[FormId].Get() : nullptr;
}

bool UOmnitrixComponent::OpenDial()
{
	const bool bOk = Machine.OpenDial();
	SyncWithMachine();
	return bOk;
}

bool UOmnitrixComponent::CancelDial()
{
	const bool bOk = Machine.CancelDial();
	SyncWithMachine();
	return bOk;
}

bool UOmnitrixComponent::ConfirmSlot(int32 Slot)
{
	if (!ResolveForm(Slot))
	{
		return false;
	}
	const bool bOk = Machine.Confirm(Slot);
	SyncWithMachine();
	return bOk;
}

bool UOmnitrixComponent::QuickSwap(int32 Slot)
{
	if (!ResolveForm(Slot))
	{
		return false;
	}
	const bool bOk = Machine.QuickSwap(Slot);
	SyncWithMachine();
	return bOk;
}

bool UOmnitrixComponent::TriggerMasterControlGlitch()
{
	if (!GlitchForm)
	{
		return false;
	}
	const bool bOk = Machine.TriggerGlitch();
	SyncWithMachine();
	return bOk;
}

bool UOmnitrixComponent::TriggerOverclock()
{
	AToonCharacterBase* Character = GetCharacter();
	if (!OverclockForm || !Character || Character->GetUltimateMeter() < OverclockMeterCost)
	{
		return false;
	}
	if (!Machine.TriggerOverclock())
	{
		return false;
	}
	Character->SpendUltimateMeter(OverclockMeterCost);
	SyncWithMachine();
	return true;
}

bool UOmnitrixComponent::ForceRevert()
{
	const bool bOk = Machine.ForceRevert();
	SyncWithMachine();
	return bOk;
}

void UOmnitrixComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// The dial slows the world, but the Omnitrix timers run in undilated time.
	const float Dilation = bTimeDilated ? DialTimeDilation : 1.0f;
	Machine.Tick(DeltaTime / Dilation);
	SyncWithMachine();
}

void UOmnitrixComponent::SyncWithMachine()
{
	using Omnitrix::EState;

	AToonCharacterBase* Character = GetCharacter();
	if (!Character)
	{
		return;
	}

	const EState State = Machine.GetState();
	const int ActiveForm = Machine.GetActiveForm();

	if (State != LastState)
	{
		const EState OldState = LastState;
		LastState = State;

		if (State == EState::Transforming)
		{
			RequestFormAssets(Machine.GetPendingForm());
		}

		const bool bWantDilation = (State == EState::Selecting);
		if (bWantDilation != bTimeDilated)
		{
			UGameplayStatics::SetGlobalTimeDilation(this, bWantDilation ? DialTimeDilation : 1.0f);
			bTimeDilated = bWantDilation;
		}

		OnStateChanged.Broadcast(static_cast<EOmnitrixState>(State), static_cast<EOmnitrixState>(OldState));
	}

	if (ActiveForm != LastActiveForm)
	{
		LastActiveForm = ActiveForm;
		if (const UAlienFormData* Form = ResolveForm(ActiveForm))
		{
			ApplyForm(Form);
		}
		else
		{
			ApplyHuman();
		}
		OnFormChanged.Broadcast(ResolveForm(ActiveForm));
	}

	if (Machine.IsInvulnerable())
	{
		Character->AddDamageImmunity(OmnitrixIFrames);
	}
	else
	{
		Character->RemoveDamageImmunity(OmnitrixIFrames);
	}

	if (Machine.IsInputLocked() || State == EState::Selecting)
	{
		Character->AddAbilityBlock(OmnitrixInputLock);
	}
	else
	{
		Character->RemoveAbilityBlock(OmnitrixInputLock);
	}
}

void UOmnitrixComponent::RequestFormAssets(int FormId)
{
	const UAlienFormData* Form = ResolveForm(FormId);
	if (!Form || Form->AreAssetsLoaded())
	{
		Machine.SetPendingAssetsReady(true);
		return;
	}

	if (PendingHandle.IsValid())
	{
		PendingHandle->CancelHandle();
	}
	PendingHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Form->GetAssetsToLoad(),
		FStreamableDelegate::CreateUObject(this, &UOmnitrixComponent::OnFormAssetsLoaded, FormId));
}

void UOmnitrixComponent::OnFormAssetsLoaded(int FormId)
{
	// Ignore stale loads if the player swapped again before this one finished.
	if (Machine.GetState() == Omnitrix::EState::Transforming && Machine.GetPendingForm() == FormId)
	{
		Machine.SetPendingAssetsReady(true);
	}
}

void UOmnitrixComponent::ApplyBody(float Radius, float HalfHeight)
{
	AToonCharacterBase* Character = GetCharacter();
	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float OldHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	Capsule->SetCapsuleSize(Radius, HalfHeight, true);

	// Keep the feet planted: grow upward rather than into the floor.
	const float Delta = HalfHeight - OldHalfHeight;
	if (Delta > 0.0f)
	{
		Character->AddActorWorldOffset(FVector(0.0f, 0.0f, Delta * Capsule->GetComponentScale().Z), false, nullptr, ETeleportType::TeleportPhysics);
	}
	// Mesh pivot sits at the capsule bottom.
	Character->GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -HalfHeight));
}

void UOmnitrixComponent::ApplyForm(const UAlienFormData* Form)
{
	AToonCharacterBase* Character = GetCharacter();
	USkeletalMeshComponent* Mesh = Character->GetMesh();

	if (USkeletalMesh* NewMesh = Form->Mesh.Get())
	{
		Mesh->SetSkeletalMeshAsset(NewMesh);
	}
	else if (!Form->Mesh.IsNull())
	{
		UE_LOG(LogToons, Warning, TEXT("Omnitrix: mesh for %s not loaded at swap time"), *Form->GetName());
	}
	if (UClass* NewAnim = Form->AnimClass.Get())
	{
		Mesh->SetAnimInstanceClass(NewAnim);
	}

	ApplyBody(Form->CapsuleRadius, Form->CapsuleHalfHeight);

	UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	Move->MaxWalkSpeed = Form->MaxWalkSpeed;
	Move->JumpZVelocity = Form->JumpZVelocity;
	Move->GravityScale = Form->GravityScale;
	Move->AirControl = Form->AirControl;

	Character->RestoreDefaultAbilities();
	for (const TPair<EToonAbilitySlot, TSubclassOf<UToonAbility>>& Pair : Form->Abilities)
	{
		Character->SetAbility(Pair.Key, Pair.Value);
	}
	Character->ApplyUniverseStencil();
}

void UOmnitrixComponent::ApplyHuman()
{
	AToonCharacterBase* Character = GetCharacter();
	USkeletalMeshComponent* Mesh = Character->GetMesh();
	if (HumanMesh)
	{
		Mesh->SetSkeletalMeshAsset(HumanMesh);
	}
	if (HumanAnimClass)
	{
		Mesh->SetAnimInstanceClass(HumanAnimClass);
	}

	ApplyBody(Human.CapsuleRadius, Human.CapsuleHalfHeight);

	UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	Move->MaxWalkSpeed = Human.MaxWalkSpeed;
	Move->JumpZVelocity = Human.JumpZVelocity;
	Move->GravityScale = Human.GravityScale;
	Move->AirControl = Human.AirControl;

	Character->RestoreDefaultAbilities();
	Character->ApplyUniverseStencil();
}
