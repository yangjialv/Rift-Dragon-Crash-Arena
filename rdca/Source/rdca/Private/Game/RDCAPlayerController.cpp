#include "Game/RDCAPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Boss/BossEncounterComponent.h"
#include "Boss/BossWeakPointComponent.h"
#include "EngineUtils.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Kismet/GameplayStatics.h"
#include "Player/PhaseCrashComponent.h"
#include "Player/PlayerHealthComponent.h"
#include "rdca.h"

ARDCAPlayerController::ARDCAPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void ARDCAPlayerController::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CombatResult != ECombatResult::Playing)
	{
		return;
	}
	if (!PlayerHealth.IsValid() || !BossWeakPoint.IsValid())
	{
		ResolveCombatActors();
	}

	if (PlayerHealth.IsValid() && PlayerHealth->IsDefeated())
	{
		FinishCombat(ECombatResult::Defeat);
	}
	else if (BossWeakPoint.IsValid() && BossWeakPoint->IsBossDefeated())
	{
		FinishCombat(ECombatResult::Victory);
	}
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

	ResolveCombatActors();
}

void ARDCAPlayerController::ResolveCombatActors()
{
	APawn* PlayerPawn = GetPawn();
	if (PlayerPawn)
	{
		PlayerHealth =
			PlayerPawn->FindComponentByClass<UPlayerHealthComponent>();
	}

	if (GetWorld())
	{
		for (TActorIterator<AActor> Iterator(GetWorld()); Iterator; ++Iterator)
		{
			UBossWeakPointComponent* WeakPoint =
				Iterator->FindComponentByClass<UBossWeakPointComponent>();
			UBossEncounterComponent* Encounter =
				Iterator->FindComponentByClass<UBossEncounterComponent>();
			if (WeakPoint && Encounter)
			{
				BossWeakPoint = WeakPoint;
				BossEncounter = Encounter;
				break;
			}
		}
	}
}

void ARDCAPlayerController::FinishCombat(const ECombatResult NewResult)
{
	if (CombatResult != ECombatResult::Playing
		|| NewResult == ECombatResult::Playing)
	{
		return;
	}
	CombatResult = NewResult;

	if (BossEncounter.IsValid())
	{
		BossEncounter->StopEncounter();
	}

	if (APawn* PlayerPawn = GetPawn())
	{
		PlayerPawn->DisableInput(this);
		if (UFloatingPawnMovement* Movement =
				PlayerPawn->FindComponentByClass<UFloatingPawnMovement>())
		{
			Movement->StopMovementImmediately();
		}
		if (UPhaseCrashComponent* PhaseCrash =
				PlayerPawn->FindComponentByClass<UPhaseCrashComponent>())
		{
			PhaseCrash->ForceDetachFromAttachment();
			PhaseCrash->SetComponentTickEnabled(false);
		}
	}

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Combat finished. Result=%s"),
		CombatResult == ECombatResult::Victory
			? TEXT("Victory")
			: TEXT("Defeat"));
}

void ARDCAPlayerController::RestartCurrentBattle()
{
	if (!GetWorld())
	{
		return;
	}

	const FName CurrentLevelName(
		*UGameplayStatics::GetCurrentLevelName(GetWorld(), true));
	UGameplayStatics::OpenLevel(this, CurrentLevelName);
}
