#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RDCAPlayerController.generated.h"

UCLASS()
class RDCA_API ARDCAPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARDCAPlayerController();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<class UUserWidget> RuntimeCombatHUDWidget;
};
