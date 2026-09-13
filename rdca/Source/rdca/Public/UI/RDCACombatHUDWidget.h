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
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	/** Drawn by Slate while the platform cursor remains hidden. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat HUD|Aim Cursor")
	bool bDrawGameAimCursor = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat HUD|Aim Cursor")
	FLinearColor AimCursorDefaultColor = FLinearColor(0.15f, 0.9f, 1.0f, 0.95f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat HUD|Aim Cursor")
	FLinearColor AimCursorChargingColor = FLinearColor(1.0f, 0.75f, 0.12f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat HUD|Aim Cursor")
	FLinearColor AimCursorBossTargetColor = FLinearColor(1.0f, 0.12f, 0.08f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat HUD|Aim Cursor",
		meta = (ClampMin = "4.0"))
	float AimCursorRadius = 13.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat HUD|Aim Cursor",
		meta = (ClampMin = "0.0"))
	float AimCursorArmLength = 9.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat HUD|Aim Cursor",
		meta = (ClampMin = "0.5"))
	float AimCursorThickness = 2.0f;

	/** The cursor yields visual priority to the landing marker while charging. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat HUD|Aim Cursor",
		meta = (ClampMin = "0.2", ClampMax = "1.0"))
	float ChargingCursorScale = 0.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat HUD|Aim Cursor",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ChargingCursorOpacity = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat HUD|Aim Preview")
	FLinearColor LowArcPreviewColor = FLinearColor(0.15f, 0.9f, 1.0f, 0.9f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat HUD|Aim Preview")
	FLinearColor HighArcPreviewColor = FLinearColor(1.0f, 0.75f, 0.12f, 0.95f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat HUD|Aim Preview",
		meta = (ClampMin = "0.5"))
	float TrajectoryThickness = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat HUD|Aim Preview",
		meta = (ClampMin = "4.0"))
	float LandingMarkerRadius = 22.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat HUD|Aim Preview",
		meta = (ClampMin = "0.5"))
	float LandingMarkerThickness = 3.0f;

	/** Temporary diagnostic labels; disabled for the player-facing demo HUD. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat HUD|Debug")
	bool bShowDebugCombatText = false;

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
