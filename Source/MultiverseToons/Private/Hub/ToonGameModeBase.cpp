#include "Hub/ToonGameModeBase.h"
#include "Hub/ToonPlayerController.h"

AToonGameModeBase::AToonGameModeBase()
{
	PlayerControllerClass = AToonPlayerController::StaticClass();
	// DefaultPawnClass is set per map / in a Blueprint subclass to a hero BP.
}
