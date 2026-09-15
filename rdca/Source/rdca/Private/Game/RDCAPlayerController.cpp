#include "Game/RDCAPlayerController.h"

#include "Audio/RDCAAudio.h"
#include "Arena/ArenaFloorCollision.h"
#include "Arena/ArenaPhaseController.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Boss/BossEncounterComponent.h"
#include "Boss/BossWeakPointComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Components/AudioComponent.h"
#include "Components/InputComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/GameUserSettings.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Player/PhaseCrashComponent.h"
#include "Player/PlayerHealthComponent.h"
#include "rdca.h"

ARDCAPlayerController::ARDCAPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	// Use the platform hardware cursor for the lowest possible aiming latency.
	// The HUD still owns the trajectory and landing-point visualization.
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	CurrentMouseCursor = EMouseCursor::Default;
}

void ARDCAPlayerController::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CombatResult != ECombatResult::Playing)
	{
		return;
	}
	if (bVictoryLandingInProgress)
	{
		UpdateBossMusic(DeltaTime);
		TickVictoryLanding(DeltaTime);
		return;
	}
	if (!PlayerHealth.IsValid() || !BossWeakPoint.IsValid())
	{
		ResolveCombatActors();
	}
	UpdateBossMusic(DeltaTime);

	if (PlayerHealth.IsValid() && PlayerHealth->IsDefeated())
	{
		FinishCombat(ECombatResult::Defeat);
	}
	else if (BossWeakPoint.IsValid() && BossWeakPoint->IsBossDefeated())
	{
		BeginVictoryLanding();
	}
}

void ARDCAPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}
	ApplyPlaytestPerformanceProfile();
	ConfigureHardwareAimCursors();

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
	StartBossMusic();
}

void ARDCAPlayerController::ApplyPlaytestPerformanceProfile()
{
#if !WITH_EDITOR
	constexpr int32 CurrentProfileVersion = 2;
	constexpr TCHAR ProfileSection[] = TEXT("RDCA.PlaytestPerformance");
	constexpr TCHAR ProfileVersionKey[] = TEXT("AppliedProfileVersion");

	int32 AppliedProfileVersion = 0;
	if (GConfig)
	{
		GConfig->GetInt(
			ProfileSection,
			ProfileVersionKey,
			AppliedProfileVersion,
			GGameUserSettingsIni);
	}
	if (AppliedProfileVersion >= CurrentProfileVersion)
	{
		return;
	}

	UGameUserSettings* Settings = GEngine
		? GEngine->GetGameUserSettings()
		: nullptr;
	if (!Settings)
	{
		return;
	}

	// This package targets low-spec playtest machines. Keep the actual gameplay
	// unchanged while selecting the cheapest renderer settings that preserve
	// readable warnings, projectiles and the Boss silhouette.
	Settings->SetOverallScalabilityLevel(0);
	Settings->SetScreenResolution(FIntPoint(1280, 720));
	Settings->SetFullscreenMode(EWindowMode::Fullscreen);
	Settings->SetResolutionScaleValueEx(100.0f);
	Settings->SetViewDistanceQuality(0);
	Settings->SetAntiAliasingQuality(1);
	Settings->SetShadowQuality(0);
	Settings->SetGlobalIlluminationQuality(0);
	Settings->SetReflectionQuality(0);
	Settings->SetPostProcessingQuality(0);
	Settings->SetTextureQuality(1);
	Settings->SetVisualEffectQuality(0);
	Settings->SetFoliageQuality(0);
	Settings->SetShadingQuality(0);
	Settings->SetVSyncEnabled(false);
	Settings->SetFrameRateLimit(60.0f);
	Settings->ApplySettings(false);

	if (GConfig)
	{
		GConfig->SetInt(
			ProfileSection,
			ProfileVersionKey,
			CurrentProfileVersion,
			GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Applied playtest performance profile version %d."),
		CurrentProfileVersion);
#endif
}

void ARDCAPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!InputComponent)
	{
		return;
	}

	FInputKeyBinding& PauseBinding = InputComponent->BindKey(
		EKeys::P,
		IE_Pressed,
		this,
		&ARDCAPlayerController::ToggleGamePause);
	PauseBinding.bExecuteWhenPaused = true;

	FInputKeyBinding& ExitBinding = InputComponent->BindKey(
		EKeys::Escape,
		IE_Pressed,
		this,
		&ARDCAPlayerController::ExitGame);
	ExitBinding.bExecuteWhenPaused = true;
}

void ARDCAPlayerController::ToggleGamePause()
{
	const bool bPauseRequested = !IsPaused();
	SetPause(bPauseRequested);
	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Game pause toggled. Paused=%s"),
		bPauseRequested ? TEXT("true") : TEXT("false"));
}

void ARDCAPlayerController::ExitGame()
{
	UKismetSystemLibrary::QuitGame(
		this,
		this,
		EQuitPreference::Quit,
		false);
}

void ARDCAPlayerController::ConfigureHardwareAimCursors()
{
	const FVector2D CenterHotSpot(0.5f, 0.5f);
	const bool bNormalLoaded = UWidgetBlueprintLibrary::SetHardwareCursor(
		this,
		EMouseCursor::Default,
		TEXT("Slate/Cursors/RDCA_Aim_Default"),
		CenterHotSpot);
	const bool bChargingLoaded = UWidgetBlueprintLibrary::SetHardwareCursor(
		this,
		EMouseCursor::Crosshairs,
		TEXT("Slate/Cursors/RDCA_Aim_Charging"),
		CenterHotSpot);
	const bool bBossLoaded = UWidgetBlueprintLibrary::SetHardwareCursor(
		this,
		EMouseCursor::CardinalCross,
		TEXT("Slate/Cursors/RDCA_Aim_Boss"),
		CenterHotSpot);

	DefaultMouseCursor = EMouseCursor::Default;
	CurrentMouseCursor = EMouseCursor::Default;
	AimCursorState = ERDCAAimCursorState::Normal;
	if (!bNormalLoaded || !bChargingLoaded || !bBossLoaded)
	{
		UE_LOG(
			LogRDCAPlayer,
			Warning,
			TEXT("One or more RDCA hardware aim cursors failed to load. Normal=%s Charging=%s Boss=%s"),
			bNormalLoaded ? TEXT("true") : TEXT("false"),
			bChargingLoaded ? TEXT("true") : TEXT("false"),
			bBossLoaded ? TEXT("true") : TEXT("false"));
	}
}

void ARDCAPlayerController::SetAimCursorState(
	const ERDCAAimCursorState NewState)
{
	if (AimCursorState == NewState)
	{
		return;
	}

	AimCursorState = NewState;
	switch (AimCursorState)
	{
	case ERDCAAimCursorState::Charging:
		CurrentMouseCursor = EMouseCursor::Crosshairs;
		break;
	case ERDCAAimCursorState::BossTarget:
		CurrentMouseCursor = EMouseCursor::CardinalCross;
		break;
	case ERDCAAimCursorState::Normal:
	default:
		CurrentMouseCursor = EMouseCursor::Default;
		break;
	}
}

void ARDCAPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(BossMusicAudio))
	{
		BossMusicAudio->Stop();
		BossMusicAudio->DestroyComponent();
		BossMusicAudio = nullptr;
	}
	Super::EndPlay(EndPlayReason);
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
		for (TActorIterator<AArenaPhaseController> Iterator(GetWorld()); Iterator; ++Iterator)
		{
			ArenaPhaseController = *Iterator;
			break;
		}
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

void ARDCAPlayerController::StartBossMusic()
{
	if (!IsLocalController())
	{
		return;
	}
	if (IsValid(BossMusicAudio))
	{
		if (BossMusicAudio->IsPlaying())
		{
			return;
		}
		BossMusicAudio->Stop();
		BossMusicAudio->DestroyComponent();
		BossMusicAudio = nullptr;
	}

	// Starting a newly loaded long SoundWave at exactly zero can cause the audio
	// device to discard the silent voice before the first volume interpolation.
	// Start just above silence, then let UpdateBossMusic perform the normal fade.
	CurrentBossMusicVolume = FMath::Max(BossMusicStartVolume, 0.001f);
	BossMusicAudio = RDCAAudio::Spawn2D(
		this,
		ERDCAAudioCue::BossMusicMain,
		CurrentBossMusicVolume,
		1.0f,
		true);
	BossMusicRetryRemaining = FMath::Max(BossMusicRetryInterval, 0.1f);
	if (BossMusicAudio)
	{
		BossMusicAudio->bAutoDestroy = false;
		UE_LOG(LogRDCAPlayer, Log, TEXT("Boss BGM playback requested."));
	}
	else
	{
		UE_LOG(LogRDCAPlayer, Warning, TEXT("Boss BGM playback request failed; retry scheduled."));
	}
}

void ARDCAPlayerController::UpdateBossMusic(const float DeltaTime)
{
	if (!IsValid(BossMusicAudio) || !BossMusicAudio->IsPlaying())
	{
		BossMusicRetryRemaining = FMath::Max(
			BossMusicRetryRemaining - DeltaTime,
			0.0f);
		if (BossMusicRetryRemaining <= 0.0f)
		{
			StartBossMusic();
		}
		return;
	}
	CurrentBossMusicVolume = FMath::FInterpTo(
		CurrentBossMusicVolume,
		GetBossMusicTargetVolume(),
		DeltaTime,
		ArenaPhaseController.IsValid()
			? FMath::Max(ArenaPhaseController->GetBGMVolumeInterpSpeed(), 0.1f)
			: 2.0f);
	BossMusicAudio->SetVolumeMultiplier(CurrentBossMusicVolume);
}

float ARDCAPlayerController::GetBossMusicTargetVolume() const
{
	if (!BossEncounter.IsValid() || !BossWeakPoint.IsValid())
	{
		return ArenaPhaseController.IsValid()
			? ArenaPhaseController->GetBGMIntroVolume()
			: 0.45f;
	}
	if (BossEncounter->IsIntroHeld())
	{
		return ArenaPhaseController.IsValid()
			? ArenaPhaseController->GetBGMIntroVolume()
			: 0.45f;
	}
	if (ArenaPhaseController.IsValid()
		&& ArenaPhaseController->IsTransitionActive())
	{
		return ArenaPhaseController->GetBGMTransitionVolume();
	}

	float TargetVolume = ArenaPhaseController.IsValid()
		? ArenaPhaseController->GetBGMPhase1Volume()
		: 0.72f;
	const int32 HitPoints = BossWeakPoint->GetCurrentHitPoints();
	if (BossEncounter->GetCombatPhase() == EBossCombatPhase::Phase2
		|| (ArenaPhaseController.IsValid()
			&& ArenaPhaseController->IsTransitionComplete()))
	{
		TargetVolume = HitPoints <= 1
			? (ArenaPhaseController.IsValid()
				? ArenaPhaseController->GetBGMCriticalVolume()
				: 0.9f)
			: (ArenaPhaseController.IsValid()
				? ArenaPhaseController->GetBGMPhase2Volume()
				: 0.82f);
	}
	else if (HitPoints < BossWeakPoint->GetMaximumHitPoints())
	{
		TargetVolume = ArenaPhaseController.IsValid()
			? ArenaPhaseController->GetBGMPhase1PressureVolume()
			: 0.78f;
	}

	if (BossEncounter->GetEncounterState() == EBossEncounterState::Preparing)
	{
		TargetVolume *= 0.71f;
	}
	return TargetVolume;
}

void ARDCAPlayerController::BeginVictoryLanding()
{
	if (bVictoryLandingInProgress
		|| CombatResult != ECombatResult::Playing)
	{
		return;
	}

	bVictoryLandingInProgress = true;
	VictoryLandingElapsed = 0.0f;
	VictoryLandingDestination = ResolveVictoryLandingDestination();
	if (BossEncounter.IsValid())
	{
		BossEncounter->StopEncounter();
	}

	APawn* PlayerPawn = GetPawn();
	if (!PlayerPawn)
	{
		FinishCombat(ECombatResult::Victory);
		return;
	}
	PlayerPawn->DisableInput(this);
	if (UFloatingPawnMovement* Movement =
			PlayerPawn->FindComponentByClass<UFloatingPawnMovement>())
	{
		Movement->StopMovementImmediately();
	}
	if (UPhaseCrashComponent* PhaseCrash =
			PlayerPawn->FindComponentByClass<UPhaseCrashComponent>())
	{
		PhaseCrash->ForceArenaRecovery(
			VictoryLandingDestination,
			VictoryLandingArcHeight,
			VictoryLandingDuration);
		// The return begins next to the defeated Boss. Ignore its body during this
		// short victory-only flight so it cannot rebound the player a second time.
		if (BossEncounter.IsValid())
		{
			if (UPrimitiveComponent* RootPrimitive =
					Cast<UPrimitiveComponent>(PlayerPawn->GetRootComponent()))
			{
				RootPrimitive->IgnoreActorWhenMoving(
					BossEncounter->GetOwner(), true);
			}
		}
	}
	else
	{
		PlayerPawn->SetActorLocation(
			VictoryLandingDestination,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Victory landing started. Player=%s Destination=%s Duration=%.2f"),
		*GetNameSafe(PlayerPawn),
		*VictoryLandingDestination.ToCompactString(),
		VictoryLandingDuration);
}

void ARDCAPlayerController::TickVictoryLanding(const float DeltaTime)
{
	VictoryLandingElapsed += DeltaTime;
	const float FinishTime = FMath::Max(VictoryLandingDuration, 0.1f)
		+ FMath::Max(VictoryResultSettleDelay, 0.0f);
	if (VictoryLandingElapsed < FinishTime)
	{
		return;
	}

	// End exactly on the floor even if a stray level collision shortened the
	// recovery path. In the normal path this correction is only a few units.
	if (APawn* PlayerPawn = GetPawn())
	{
		PlayerPawn->SetActorLocation(
			VictoryLandingDestination,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}
	bVictoryLandingInProgress = false;
	FinishCombat(ECombatResult::Victory);
}

FVector ARDCAPlayerController::ResolveVictoryLandingDestination() const
{
	const APawn* PlayerPawn = GetPawn();
	if (!PlayerPawn || !GetWorld())
	{
		return FVector::ZeroVector;
	}

	const AArenaFloorCollision* ArenaFloor = nullptr;
	for (TActorIterator<AArenaFloorCollision> It(GetWorld()); It; ++It)
	{
		ArenaFloor = *It;
		break;
	}
	if (!ArenaFloor)
	{
		return PlayerPawn->GetActorLocation();
	}

	const FVector FloorCenter = ArenaFloor->GetFloorCenter();
	FVector Outward = PlayerPawn->GetActorLocation() - FloorCenter;
	Outward.Z = 0.0f;
	float CurrentRadius = Outward.Size();
	if (!Outward.Normalize())
	{
		Outward = PlayerPawn->GetActorForwardVector();
		Outward.Z = 0.0f;
		if (!Outward.Normalize())
		{
			Outward = FVector::ForwardVector;
		}
	}

	const float MinimumRadius = ArenaFloor->GetInnerHoleRadius()
		+ FMath::Max(VictoryLandingInnerClearance, 0.0f);
	const float MaximumRadius = FMath::Max(
		ArenaFloor->GetOuterFloorRadius()
			- FMath::Max(VictoryLandingOuterClearance, 0.0f),
		MinimumRadius);
	CurrentRadius = FMath::Clamp(CurrentRadius, MinimumRadius, MaximumRadius);
	float PlayerHalfHeight = 50.0f;
	if (const UPrimitiveComponent* RootPrimitive =
			Cast<UPrimitiveComponent>(PlayerPawn->GetRootComponent()))
	{
		PlayerHalfHeight = FMath::Max(
			RootPrimitive->Bounds.BoxExtent.Z,
			1.0f);
	}
	return FloorCenter
		+ Outward * CurrentRadius
		+ FVector::UpVector * (PlayerHalfHeight + 2.0f);
}

void ARDCAPlayerController::FinishCombat(const ECombatResult NewResult)
{
	if (CombatResult != ECombatResult::Playing
		|| NewResult == ECombatResult::Playing)
	{
		return;
	}
	CombatResult = NewResult;
	if (IsValid(BossMusicAudio))
	{
		BossMusicAudio->FadeOut(
			ArenaPhaseController.IsValid()
				? FMath::Max(
					ArenaPhaseController->GetBGMResultFadeOutDuration(), 0.0f)
				: 2.5f,
			0.0f);
	}
	RDCAAudio::Play2D(
		this,
		NewResult == ECombatResult::Victory
			? ERDCAAudioCue::Victory
			: ERDCAAudioCue::Defeat,
		0.72f);

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
