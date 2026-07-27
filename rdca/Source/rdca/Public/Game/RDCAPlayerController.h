#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RDCAPlayerController.generated.h"

UENUM(BlueprintType)
enum class ECombatResult : uint8
{
	Playing,
	Victory,
	Defeat
};

UCLASS()
class RDCA_API ARDCAPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARDCAPlayerController();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, Category = "Combat")
	ECombatResult GetCombatResult() const { return CombatResult; }

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void RestartCurrentBattle();

protected:
	virtual void BeginPlay() override;

private:
	void ResolveCombatActors();
	void FinishCombat(ECombatResult NewResult);

	UPROPERTY(Transient)
	TObjectPtr<class UUserWidget> RuntimeCombatHUDWidget;

	TWeakObjectPtr<class UPlayerHealthComponent> PlayerHealth;
	TWeakObjectPtr<class UBossWeakPointComponent> BossWeakPoint;
	TWeakObjectPtr<class UBossEncounterComponent> BossEncounter;
	ECombatResult CombatResult = ECombatResult::Playing;
};
