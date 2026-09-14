#include "UI/RDCACombatHUDWidget.h"

#include "Boss/BossEncounterComponent.h"
#include "Boss/BossWeakPointComponent.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Game/RDCAPlayerController.h"
#include "Rendering/DrawElements.h"
#include "Player/PhaseCrashComponent.h"
#include "Player/PlayerHealthComponent.h"

namespace
{
FText GetBossAttackText(const EBossAttackType Attack)
{
	switch (Attack)
	{
	case EBossAttackType::Shockwave:
		return FText::FromString(TEXT("SHOCKWAVE"));
	case EBossAttackType::AimedVolley:
		return FText::FromString(TEXT("AIMED VOLLEY"));
	case EBossAttackType::FanBarrage:
		return FText::FromString(TEXT("FAN BARRAGE"));
	case EBossAttackType::SweepLaser:
		return FText::FromString(TEXT("LASER SWEEP"));
	default:
		return FText::FromString(TEXT("ATTACK"));
	}
}

FText GetBossStateText(
	const EBossEncounterState State,
	const EBossAttackType Attack)
{
	switch (State)
	{
	case EBossEncounterState::Idle:
		return FText::FromString(TEXT("IDLE"));
	case EBossEncounterState::SelectingAttack:
		return FText::FromString(TEXT("READING PLAYER"));
	case EBossEncounterState::Preparing:
		return FText::Format(
			FText::FromString(TEXT("{0} WARNING")),
			GetBossAttackText(Attack));
	case EBossEncounterState::Attacking:
		return GetBossAttackText(Attack);
	case EBossEncounterState::Recovery:
		return FText::FromString(TEXT("RECOVERY"));
	case EBossEncounterState::WeakPointExposed:
		return FText::FromString(TEXT("WEAK POINT EXPOSED"));
	case EBossEncounterState::Dead:
		return FText::FromString(TEXT("BOSS DEFEATED"));
	default:
		return FText::GetEmpty();
	}
}
}

void URDCACombatHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ResolvePlayerComponents();
	ResolveBossComponents();
	const ESlateVisibility DebugTextVisibility = bShowDebugCombatText
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed;
	if (TXT_PlayerHP)
	{
		TXT_PlayerHP->SetVisibility(DebugTextVisibility);
	}
	if (TXT_BossHP)
	{
		TXT_BossHP->SetVisibility(DebugTextVisibility);
	}
	if (TXT_WeakPointState)
	{
		TXT_WeakPointState->SetVisibility(DebugTextVisibility);
	}
	if (TXT_AttachedState)
	{
		TXT_AttachedState->SetVisibility(DebugTextVisibility);
	}
	if (TXT_BossState)
	{
		TXT_BossState->SetVisibility(DebugTextVisibility);
	}
	if (BTN_Restart)
	{
		BTN_Restart->OnClicked.AddUniqueDynamic(
			this,
			&URDCACombatHUDWidget::HandleRestartClicked);
	}
	UpdateHUD();
}

void URDCACombatHUDWidget::NativeTick(
	const FGeometry& MyGeometry,
	const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!PlayerHealth.IsValid() || !PhaseCrash.IsValid())
	{
		ResolvePlayerComponents();
	}

	BossResolveRetryRemaining -= InDeltaTime;
	if ((!BossWeakPoint.IsValid() || !BossEncounter.IsValid())
		&& BossResolveRetryRemaining <= 0.0f)
	{
		ResolveBossComponents();
		BossResolveRetryRemaining = 0.5f;
	}

	UpdateHUD();
}

int32 URDCACombatHUDWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	const bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		bParentEnabled);
	APlayerController* OwningPlayer = GetOwningPlayer();
	if (!OwningPlayer)
	{
		return BaseLayer;
	}

	// DeprojectMousePositionToWorld(), which owns gameplay aiming, reads the
	// PlayerController's viewport mouse coordinates. Read that exact source here
	// as well instead of Slate's desktop cursor, then convert both mouse and
	// projected world points through the same DPI-aware viewport geometry.
	float MouseViewportX = 0.0f;
	float MouseViewportY = 0.0f;
	if (!UWidgetLayoutLibrary::GetMousePositionScaledByDPI(
		OwningPlayer,
		MouseViewportX,
		MouseViewportY))
	{
		return BaseLayer;
	}
	const FGeometry ViewportGeometry =
		UWidgetLayoutLibrary::GetViewportWidgetGeometry(OwningPlayer);
	const auto ViewportToWidgetLocal = [
		&ViewportGeometry,
		&AllottedGeometry](const FVector2D& ViewportPosition)
	{
		return AllottedGeometry.AbsoluteToLocal(
			ViewportGeometry.LocalToAbsolute(ViewportPosition));
	};
	const FVector2D CursorPosition = ViewportToWidgetLocal(
		FVector2D(MouseViewportX, MouseViewportY));
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	if (CursorPosition.X < 0.0f || CursorPosition.Y < 0.0f
		|| CursorPosition.X > LocalSize.X || CursorPosition.Y > LocalSize.Y)
	{
		return BaseLayer;
	}

	const ARDCAPlayerController* RDCAController =
		Cast<ARDCAPlayerController>(OwningPlayer);
	const bool bBattlePlaying = !RDCAController
		|| RDCAController->GetCombatResult() == ECombatResult::Playing;
	const EPhaseCrashState CrashState = PhaseCrash.IsValid()
		? PhaseCrash->GetCrashState()
		: EPhaseCrashState::Ready;
	const bool bCharging = CrashState == EPhaseCrashState::Charging;
	const bool bBossAim = PhaseCrash.IsValid() && PhaseCrash->IsBossAimActive();

	int32 DrawLayer = BaseLayer + 10;
	if (bCharging && PhaseCrash.IsValid())
	{
		TArray<FVector> WorldPoints;
		FVector LandingPoint = FVector::ZeroVector;
		bool bHasLaunchTrajectory = false;
		if (PhaseCrash->GetAimPreview(
			WorldPoints,
			LandingPoint,
			bHasLaunchTrajectory))
		{
			const FLinearColor PreviewColor =
				PhaseCrash->GetPredictedArcType() == ECrashArcType::HighArc
					? HighArcPreviewColor
					: LowArcPreviewColor;
			TArray<FVector2D> ScreenPoints;
			if (!bBossAim)
			{
				ScreenPoints.Reserve(WorldPoints.Num());
				for (const FVector& WorldPoint : WorldPoints)
				{
					FVector2D ScreenPoint;
					if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
						OwningPlayer,
						WorldPoint,
						ScreenPoint,
						false))
					{
						ScreenPoints.Add(ViewportToWidgetLocal(ScreenPoint));
					}
				}
			}

			if (!bBossAim && bHasLaunchTrajectory && ScreenPoints.Num() >= 2)
			{
				// Broken segments make the ballistic guide readable without
				// looking like a physical beam in the world.
				for (int32 Index = 1; Index < ScreenPoints.Num(); Index += 2)
				{
					TArray<FVector2D> SegmentPoints;
					SegmentPoints.Add(ScreenPoints[Index - 1]);
					SegmentPoints.Add(ScreenPoints[Index]);
					FSlateDrawElement::MakeLines(
						OutDrawElements,
						DrawLayer,
						AllottedGeometry.ToPaintGeometry(),
						SegmentPoints,
						ESlateDrawEffect::None,
						PreviewColor,
						true,
						TrajectoryThickness);
				}
			}

			if (!bBossAim)
			{
				FVector2D LandingScreenPosition;
				if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
					OwningPlayer,
					LandingPoint,
					LandingScreenPosition,
					false))
				{
					LandingScreenPosition = ViewportToWidgetLocal(
						LandingScreenPosition);
					constexpr int32 MarkerSegments = 32;
					TArray<FVector2D> MarkerPoints;
					MarkerPoints.Reserve(MarkerSegments + 1);
					for (int32 Index = 0; Index <= MarkerSegments; ++Index)
					{
						const float Angle = 2.0f * PI * Index / MarkerSegments;
						MarkerPoints.Add(
							LandingScreenPosition
							+ FVector2D(FMath::Cos(Angle), FMath::Sin(Angle))
								* LandingMarkerRadius);
					}
					FSlateDrawElement::MakeLines(
						OutDrawElements,
						DrawLayer + 1,
						AllottedGeometry.ToPaintGeometry(),
						MarkerPoints,
						ESlateDrawEffect::None,
						PreviewColor,
						true,
						LandingMarkerThickness);
				}
			}
			DrawLayer += 2;
		}
	}

	// During committed travel the aim UI disappears. Once the battle result is
	// visible the neutral cursor returns so the Restart button remains usable.
	if (bBattlePlaying && CrashState == EPhaseCrashState::Crashing)
	{
		return DrawLayer;
	}
	// A platform hardware crosshair follows the physical mouse independently of
	// the game render frame. Do not paint a second, one-frame-late software cursor
	// on top of it. Projects that explicitly select None can still use this HUD
	// reticle as a fallback.
	if (!bDrawGameAimCursor
		|| OwningPlayer->DefaultMouseCursor != EMouseCursor::None)
	{
		return DrawLayer;
	}

	FLinearColor CursorColor = bBossAim
		? AimCursorBossTargetColor
		: bCharging
			? AimCursorChargingColor
			: AimCursorDefaultColor;
	float CursorScale = 1.0f;
	if (bCharging && !bBossAim)
	{
		CursorScale = ChargingCursorScale;
		CursorColor.A *= ChargingCursorOpacity;
	}
	const float CursorRadius = AimCursorRadius * CursorScale;
	const float CursorArmLength = AimCursorArmLength * CursorScale;

	constexpr int32 RingSegments = 24;
	TArray<FVector2D> RingPoints;
	RingPoints.Reserve(RingSegments + 1);
	for (int32 Index = 0; Index <= RingSegments; ++Index)
	{
		const float Angle = 2.0f * PI * Index / RingSegments;
		RingPoints.Add(CursorPosition + FVector2D(
			FMath::Cos(Angle), FMath::Sin(Angle)) * CursorRadius);
	}
	const int32 CursorLayer = DrawLayer;
	FSlateDrawElement::MakeLines(
		OutDrawElements,
		CursorLayer,
		AllottedGeometry.ToPaintGeometry(),
		RingPoints,
		ESlateDrawEffect::None,
		CursorColor,
		true,
		AimCursorThickness);

	const float Inner = CursorRadius + 3.0f * CursorScale;
	const float Outer = Inner + CursorArmLength;
	const auto DrawCursorArm = [
		&OutDrawElements,
		&AllottedGeometry,
		CursorLayer,
		CursorColor,
		this](const FVector2D& Start, const FVector2D& End)
	{
		TArray<FVector2D> LinePoints;
		LinePoints.Add(Start);
		LinePoints.Add(End);
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			CursorLayer,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			CursorColor,
			true,
			AimCursorThickness);
	};
	DrawCursorArm(
		FVector2D(CursorPosition.X - Outer, CursorPosition.Y),
		FVector2D(CursorPosition.X - Inner, CursorPosition.Y));
	DrawCursorArm(
		FVector2D(CursorPosition.X + Inner, CursorPosition.Y),
		FVector2D(CursorPosition.X + Outer, CursorPosition.Y));
	DrawCursorArm(
		FVector2D(CursorPosition.X, CursorPosition.Y - Outer),
		FVector2D(CursorPosition.X, CursorPosition.Y - Inner));
	DrawCursorArm(
		FVector2D(CursorPosition.X, CursorPosition.Y + Inner),
		FVector2D(CursorPosition.X, CursorPosition.Y + Outer));
	return CursorLayer;
}

void URDCACombatHUDWidget::ResolvePlayerComponents()
{
	const APlayerController* PlayerController = GetOwningPlayer();
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!PlayerPawn)
	{
		return;
	}

	PlayerHealth = PlayerPawn->FindComponentByClass<UPlayerHealthComponent>();
	PhaseCrash = PlayerPawn->FindComponentByClass<UPhaseCrashComponent>();
}

void URDCACombatHUDWidget::ResolveBossComponents()
{
	if (!GetWorld())
	{
		return;
	}

	for (TActorIterator<AActor> ActorIterator(GetWorld());
		ActorIterator;
		++ActorIterator)
	{
		UBossWeakPointComponent* WeakPoint =
			ActorIterator->FindComponentByClass<UBossWeakPointComponent>();
		UBossEncounterComponent* Encounter =
			ActorIterator->FindComponentByClass<UBossEncounterComponent>();
		if (WeakPoint && Encounter)
		{
			BossWeakPoint = WeakPoint;
			BossEncounter = Encounter;
			return;
		}
	}
}

void URDCACombatHUDWidget::UpdateHUD()
{
	if (PlayerHealth.IsValid())
	{
		if (PB_PlayerHP)
		{
			PB_PlayerHP->SetPercent(PlayerHealth->GetHealthPercent());
		}
		if (TXT_PlayerHP)
		{
			TXT_PlayerHP->SetText(FText::Format(
				FText::FromString(TEXT("PLAYER HP: {0}/{1}")),
				FText::AsNumber(PlayerHealth->GetCurrentHealth()),
				FText::AsNumber(PlayerHealth->GetMaximumHealth())));
		}
	}

	if (PhaseCrash.IsValid())
	{
		if (PB_CrashCharge)
		{
			PB_CrashCharge->SetPercent(
				PhaseCrash->GetCrashState() == EPhaseCrashState::Charging
					? PhaseCrash->GetChargeAlpha()
					: 0.0f);
		}
		if (PB_CrashCooldown)
		{
			PB_CrashCooldown->SetPercent(
				PhaseCrash->GetCooldownReadyPercent());
		}
		if (TXT_AttachedState)
		{
			TXT_AttachedState->SetText(
				PhaseCrash->IsAttached()
					? FText::FromString(TEXT("ATTACHED"))
					: FText::GetEmpty());
		}
	}

	if (BossWeakPoint.IsValid())
	{
		if (PB_BossHP)
		{
			PB_BossHP->SetPercent(BossWeakPoint->GetHitPointsPercent());
		}
		if (TXT_BossHP)
		{
			TXT_BossHP->SetText(FText::Format(
				FText::FromString(TEXT("BOSS HP: {0}/{1}")),
				FText::AsNumber(BossWeakPoint->GetCurrentHitPoints()),
				FText::AsNumber(BossWeakPoint->GetMaximumHitPoints())));
		}
		if (TXT_WeakPointState)
		{
			TXT_WeakPointState->SetText(
				BossWeakPoint->IsExposed()
					? FText::FromString(TEXT("EXPOSED"))
					: FText::FromString(TEXT("PROTECTED")));
		}
	}

	if (BossEncounter.IsValid() && TXT_BossState)
	{
		TXT_BossState->SetText(
			GetBossStateText(
				BossEncounter->GetEncounterState(),
				BossEncounter->GetCurrentAttack()));
	}

	const ARDCAPlayerController* RDCAController =
		Cast<ARDCAPlayerController>(GetOwningPlayer());
	const ECombatResult CombatResult = RDCAController
		? RDCAController->GetCombatResult()
		: ECombatResult::Playing;
	if (PANEL_BattleResult)
	{
		PANEL_BattleResult->SetVisibility(
			CombatResult == ECombatResult::Playing
				? ESlateVisibility::Collapsed
				: ESlateVisibility::Visible);
	}
	if (TXT_BattleResult && CombatResult != ECombatResult::Playing)
	{
		TXT_BattleResult->SetText(
			CombatResult == ECombatResult::Victory
				? FText::FromString(TEXT("VICTORY"))
				: FText::FromString(TEXT("DEFEAT")));
	}
}

void URDCACombatHUDWidget::HandleRestartClicked()
{
	if (ARDCAPlayerController* RDCAController =
			Cast<ARDCAPlayerController>(GetOwningPlayer()))
	{
		RDCAController->RestartCurrentBattle();
	}
}
