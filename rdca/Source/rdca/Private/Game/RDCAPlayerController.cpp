#include "Game/RDCAPlayerController.h"

#include "Blueprint/UserWidget.h"

ARDCAPlayerController::ARDCAPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void ARDCAPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	const TSubclassOf<UUserWidget> CombatHUDClass = LoadClass<UUserWidget>(
		nullptr,
		TEXT("/Game/Blueprints/UI/WBP_CombatHUD.WBP_CombatHUD_C"));
	if (CombatHUDClass)
	{
		RuntimeCombatHUDWidget =
			CreateWidget<UUserWidget>(this, CombatHUDClass);
		if (RuntimeCombatHUDWidget)
		{
			RuntimeCombatHUDWidget->AddToViewport();
		}
	}
}
