#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RDCACombatHUDWidget.generated.h"

class UBossEncounterComponent;
class UBossWeakPointComponent;
class UPhaseCrashComponent;
class UPlayerHealthComponent;
class UProgressBar;
class UTextBlock;
class UButton;
class UWidget;

UCLASS(Abstract, Blueprintable)
class RDCA_API URDCACombatHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void HandleRestartClicked();

private:
	void ResolvePlayerComponents();
	void ResolveBossComponents();
	void UpdateHUD();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> PB_PlayerHP;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_PlayerHP;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> PB_BossHP;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_BossHP;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_WeakPointState;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> PB_CrashCharge;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> PB_CrashCooldown;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_AttachedState;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_BossState;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PANEL_BattleResult;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_BattleResult;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BTN_Restart;

	TWeakObjectPtr<UPlayerHealthComponent> PlayerHealth;
	TWeakObjectPtr<UPhaseCrashComponent> PhaseCrash;
	TWeakObjectPtr<UBossWeakPointComponent> BossWeakPoint;
	TWeakObjectPtr<UBossEncounterComponent> BossEncounter;
	float BossResolveRetryRemaining = 0.0f;
};
