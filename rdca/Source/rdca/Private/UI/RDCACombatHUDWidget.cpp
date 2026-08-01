#include "UI/RDCACombatHUDWidget.h"

#include "Boss/BossEncounterComponent.h"
#include "Boss/BossWeakPointComponent.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Game/RDCAPlayerController.h"
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
