#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "ToonTypes.generated.h"

// Home universe of a hero. The numeric value (+1) is written to the custom
// stencil buffer so the outline pass can apply each show's native line work.
UENUM(BlueprintType)
enum class EToonUniverse : uint8
{
	Nexus          UMETA(DisplayName = "Nexus (Neutral)"),
	Retroville     UMETA(DisplayName = "Retroville"),
	Dimmsdale      UMETA(DisplayName = "Dimmsdale"),
	AmityPark      UMETA(DisplayName = "Amity Park"),
	Chinatown      UMETA(DisplayName = "NYC Chinatown"),
	Tremorton      UMETA(DisplayName = "Tremorton"),
	MiracleCity    UMETA(DisplayName = "Miracle City"),
	Bellwood       UMETA(DisplayName = "Bellwood"),
	AirTemple      UMETA(DisplayName = "Eastern Air Temple")
};

UENUM(BlueprintType)
enum class EToonAbilitySlot : uint8
{
	LightAttack,
	HeavyAttack,
	Signature1,
	Signature2,
	Signature3,
	Ultimate
};

namespace ToonCollision
{
	// Matches Config/DefaultEngine.ini [/Script/Engine.CollisionProfile].
	constexpr ECollisionChannel PhaseBlocker = ECC_GameTraceChannel1;
}
