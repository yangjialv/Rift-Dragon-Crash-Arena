#include "Game/RDCAGameMode.h"

#include "Game/RDCAPlayerController.h"
#include "Player/PlayerCorePawn.h"

ARDCAGameMode::ARDCAGameMode()
{
	DefaultPawnClass = APlayerCorePawn::StaticClass();
	PlayerControllerClass = ARDCAPlayerController::StaticClass();
}
