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

UENUM(BlueprintType)
enum class ERDCAAimCursorState : uint8
{
	Normal,
	Charging,
	BossTarget
};

UCLASS()
class RDCA_API ARDCAPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARDCAPlayerController();

	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Combat")
	ECombatResult GetCombatResult() const { return CombatResult; }

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void RestartCurrentBattle();

	void SetAimCursorState(ERDCAAimCursorState NewState);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void ToggleGamePause();
	void ExitGame();
	void ConfigureHardwareAimCursors();
	void ResolveCombatActors();
	void BeginVictoryLanding();
	void TickVictoryLanding(float DeltaTime);
	FVector ResolveVictoryLandingDestination() const;
	void FinishCombat(ECombatResult NewResult);
	void StartBossMusic();
	void UpdateBossMusic(float DeltaTime);
	float GetBossMusicTargetVolume() const;

	UPROPERTY(Transient)
	TObjectPtr<class UUserWidget> RuntimeCombatHUDWidget;

	TWeakObjectPtr<class UPlayerHealthComponent> PlayerHealth;
	TWeakObjectPtr<class UBossWeakPointComponent> BossWeakPoint;
	TWeakObjectPtr<class UBossEncounterComponent> BossEncounter;
	TWeakObjectPtr<class AArenaPhaseController> ArenaPhaseController;

	UPROPERTY(Transient)
	TObjectPtr<class UAudioComponent> BossMusicAudio;

	float CurrentBossMusicVolume = 0.0f;
	float BossMusicRetryRemaining = 0.0f;
	ERDCAAimCursorState AimCursorState = ERDCAAimCursorState::Normal;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Audio",
		meta = (ClampMin = "0.001", ClampMax = "0.1"))
	float BossMusicStartVolume = 0.01f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Audio",
		meta = (ClampMin = "0.1"))
	float BossMusicRetryInterval = 0.75f;
	bool bVictoryLandingInProgress = false;
	float VictoryLandingElapsed = 0.0f;
	FVector VictoryLandingDestination = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Victory Landing",
		meta = (ClampMin = "0.1"))
	float VictoryLandingDuration = 0.55f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Victory Landing",
		meta = (ClampMin = "0.0"))
	float VictoryLandingArcHeight = 160.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Victory Landing",
		meta = (ClampMin = "0.0"))
	float VictoryLandingInnerClearance = 300.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Victory Landing",
		meta = (ClampMin = "0.0"))
	float VictoryLandingOuterClearance = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Victory Landing",
		meta = (ClampMin = "0.0"))
	float VictoryResultSettleDelay = 0.12f;

	ECombatResult CombatResult = ECombatResult::Playing;
};
