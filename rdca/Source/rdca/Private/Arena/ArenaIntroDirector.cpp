#include "Arena/ArenaIntroDirector.h"

#include "Arena/AnchorSpawnManager.h"
#include "Boss/BossAnimationComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "rdca.h"

AArenaIntroDirector::AArenaIntroDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AArenaIntroDirector::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	TickBossDescent(DeltaTime);
}

void AArenaIntroDirector::BeginPlay()
{
	Super::BeginPlay();

	ResolveReferences();
	CacheAnchorManagers();
	LockPlayer();

	if (bSpawnAnchorsAfterIntro)
	{
		for (const TWeakObjectPtr<AAnchorSpawnManager>& Manager : AnchorManagers)
		{
			if (Manager.IsValid())
			{
				Manager->ClearSpawnedAnchors();
			}
		}
	}

	if (!BossAnimation.IsValid())
	{
		UE_LOG(LogRDCAPlayer, Warning,
			TEXT("Arena Intro Director could not find Boss Animation; releasing player immediately."));
		CompleteIntro();
		return;
	}

	BossAnimation->OnAnimationEvent.AddDynamic(
		this,
		&AArenaIntroDirector::HandleBossAnimationEvent);
}

void AArenaIntroDirector::HandleBossAnimationEvent(
	const EBossAnimationEvent AnimationEvent)
{
	switch (AnimationEvent)
	{
	case EBossAnimationEvent::IntroTakeoff:
		ShatterCyberCenter();
		break;
	case EBossAnimationEvent::IntroFinished:
		BeginBossDescent();
		break;
	default:
		break;
	}
}

void AArenaIntroDirector::ResolveReferences()
{
	if (BossActor)
	{
		BossAnimation = BossActor->FindComponentByClass<UBossAnimationComponent>();
	}
	if (!BossAnimation.IsValid())
	{
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (UBossAnimationComponent* Candidate =
					It->FindComponentByClass<UBossAnimationComponent>())
			{
				BossAnimation = Candidate;
				BossActor = *It;
				break;
			}
		}
	}
}

void AArenaIntroDirector::BeginBossDescent()
{
	if (bIntroCompleted || bBossDescentActive)
	{
		return;
	}
	if (!bDescendBossAfterTakeoff || !BossActor || !BossCombatHoverTarget)
	{
		if (bDescendBossAfterTakeoff && (!BossActor || !BossCombatHoverTarget))
		{
			UE_LOG(
				LogRDCAPlayer,
				Warning,
				TEXT("Arena intro Boss descent skipped. Boss=%s HoverTarget=%s"),
				*GetNameSafe(BossActor),
				*GetNameSafe(BossCombatHoverTarget));
		}
		CompleteIntro();
		return;
	}

	BossDescentStart = BossActor->GetActorLocation();
	BossDescentEnd = BossCombatHoverTarget->GetActorLocation();
	BossDescentElapsed = 0.0f;
	if (BossDescentDuration <= UE_KINDA_SMALL_NUMBER)
	{
		BossActor->SetActorLocation(BossDescentEnd, false);
		CompleteIntro();
		return;
	}

	bBossDescentActive = true;
	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Arena intro Boss descent started. Start=%s End=%s Duration=%.2f"),
		*BossDescentStart.ToCompactString(),
		*BossDescentEnd.ToCompactString(),
		BossDescentDuration);
}

void AArenaIntroDirector::TickBossDescent(const float DeltaTime)
{
	if (!bBossDescentActive || !BossActor)
	{
		return;
	}

	BossDescentElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(
		BossDescentElapsed / FMath::Max(BossDescentDuration, 0.01f),
		0.0f,
		1.0f);
	const float SmoothedAlpha = FMath::SmoothStep(0.0f, 1.0f, Alpha);
	BossActor->SetActorLocation(FMath::Lerp(
		BossDescentStart,
		BossDescentEnd,
		SmoothedAlpha), false);

	if (Alpha >= 1.0f)
	{
		bBossDescentActive = false;
		CompleteIntro();
	}
}

void AArenaIntroDirector::CacheAnchorManagers()
{
	AnchorManagers.Reset();
	for (TActorIterator<AAnchorSpawnManager> It(GetWorld()); It; ++It)
	{
		AnchorManagers.Add(*It);
	}
}

void AArenaIntroDirector::LockPlayer()
{
	if (!bLockPlayerDuringIntro)
	{
		return;
	}

	if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (APawn* PlayerPawn = Controller->GetPawn())
		{
			PlayerPawn->DisableInput(Controller);
			if (UPawnMovementComponent* Movement = PlayerPawn->GetMovementComponent())
			{
				Movement->StopMovementImmediately();
			}
		}
	}
}

void AArenaIntroDirector::UnlockPlayer()
{
	if (!bLockPlayerDuringIntro)
	{
		return;
	}

	if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (APawn* PlayerPawn = Controller->GetPawn())
		{
			PlayerPawn->EnableInput(Controller);
		}
	}
}

void AArenaIntroDirector::ShatterCyberCenter()
{
	if (bCyberCenterShattered)
	{
		return;
	}
	bCyberCenterShattered = true;

	if (CyberCenterActor)
	{
		CyberCenterActor->SetActorHiddenInGame(true);
		TArray<UPrimitiveComponent*> Components;
		CyberCenterActor->GetComponents<UPrimitiveComponent>(Components);
		for (UPrimitiveComponent* Component : Components)
		{
			if (Component)
			{
				Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
	}

	if (CenterFractureActorClass && GetWorld())
	{
		const FTransform FractureTransform = CyberCenterActor
			? CyberCenterActor->GetActorTransform()
			: GetActorTransform();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AActor* Fracture = GetWorld()->SpawnActor<AActor>(
			CenterFractureActorClass,
			FractureTransform,
			SpawnParameters))
		{
			Fracture->SetLifeSpan(CenterFractureLifeSpan);
		}
	}

	UE_LOG(LogRDCAPlayer, Log, TEXT("Arena intro Cyber centre shattered. Centre=%s"),
		*GetNameSafe(CyberCenterActor));
}

void AArenaIntroDirector::CompleteIntro()
{
	if (bIntroCompleted)
	{
		return;
	}
	bIntroCompleted = true;

	if (bSpawnAnchorsAfterIntro)
	{
		for (const TWeakObjectPtr<AAnchorSpawnManager>& Manager : AnchorManagers)
		{
			if (Manager.IsValid())
			{
				Manager->SpawnAnchors();
			}
		}
	}
	UnlockPlayer();
	UE_LOG(LogRDCAPlayer, Log, TEXT("Arena intro completed; player control released."));
}
