#include "Arena/ArenaPhaseController.h"

#include "Boss/BossEncounterComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "rdca.h"

AArenaPhaseController::AArenaPhaseController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AArenaPhaseController::BeginPlay()
{
	Super::BeginPlay();

	ResolveReferences();
	CacheEnvironmentActors();
	if (bInitializeCyberRiftOnBeginPlay)
	{
		InitializeCyberRift();
	}
	UpdateMaterialSphere();
}

void AArenaPhaseController::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bTransitionActive && !bTransitionComplete
		&& bStartWhenBossEntersPhase2 && BossEncounter.IsValid()
		&& BossEncounter->GetCombatPhase() == EBossCombatPhase::Phase2)
	{
		StartPhaseTransition();
	}

	if (!bTransitionActive)
	{
		return;
	}

	TransitionElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(
		TransitionElapsed / FMath::Max(ExpansionDuration, 0.1f), 0.0f, 1.0f);
	CurrentRadius = FMath::Lerp(0.0f, MaximumRadius, Alpha);
	UpdateMaterialSphere();

	if (!bUseMaterialSphereMask)
	{
		for (FPhaseActorPair& Pair : MappedPairs)
		{
			if (!Pair.bRevealed && CurrentRadius >= GetRevealRadius(Pair))
			{
				RevealPair(Pair);
			}
		}
	}

	if (Alpha >= 1.0f)
	{
		FinalizeSourceCodeVoid();
	}
}

void AArenaPhaseController::StartPhaseTransition()
{
	if (bTransitionActive || bTransitionComplete)
	{
		return;
	}

	if (MappedPairs.IsEmpty())
	{
		ResolveReferences();
		CacheEnvironmentActors();
	}

	bTransitionActive = true;
	TransitionElapsed = 0.0f;
	CurrentRadius = 0.0f;
	if (BossEncounter.IsValid())
	{
		BossEncounter->SetEncounterHold(true);
	}

	// Per-pixel masking needs both worlds rendered while their materials decide
	// which side of the moving sphere is visible.
	if (bUseMaterialSphereMask && PhaseMaterialParameters)
	{
		for (const FPhaseActorPair& Pair : MappedPairs)
		{
			if (Pair.SourceActor.IsValid())
			{
				Pair.SourceActor->SetActorHiddenInGame(false);
			}
		}
		for (const TWeakObjectPtr<AActor>& Actor : SourceOnlyActors)
		{
			if (Actor.IsValid())
			{
				Actor->SetActorHiddenInGame(false);
			}
		}
	}
	else
	{
		// Source-only actors are intentional Phase 2 set pieces, such as the
		// small centre rift. They appear at the sphere origin immediately.
		for (const TWeakObjectPtr<AActor>& Actor : SourceOnlyActors)
		{
			if (Actor.IsValid())
			{
				Actor->SetActorHiddenInGame(false);
			}
		}
	}

	UpdateMaterialSphere();
	UE_LOG(LogRDCAPlayer, Log, TEXT("Arena phase transition started. Origin=%s Radius=%.0f Duration=%.2f"),
		*PhaseOrigin.ToCompactString(), MaximumRadius, ExpansionDuration);
}

void AArenaPhaseController::ResolveReferences()
{
	if (PhaseOriginActor)
	{
		PhaseOrigin = PhaseOriginActor->GetActorLocation();
	}
	else
	{
		PhaseOrigin = GetActorLocation();
		UE_LOG(LogRDCAPlayer, Warning,
			TEXT("Arena Phase Controller has no Phase Origin Actor. Using controller location: %s"),
			*PhaseOrigin.ToCompactString());
	}

	if (BossActor)
	{
		BossEncounter = BossActor->FindComponentByClass<UBossEncounterComponent>();
	}
	if (!BossEncounter.IsValid())
	{
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (UBossEncounterComponent* Candidate = It->FindComponentByClass<UBossEncounterComponent>())
			{
				BossEncounter = Candidate;
				break;
			}
		}
	}
	if (!BossEncounter.IsValid())
	{
		UE_LOG(LogRDCAPlayer, Warning, TEXT("Arena Phase Controller could not find a Boss Encounter component."));
	}
}

void AArenaPhaseController::CacheEnvironmentActors()
{
	MappedPairs.Reset();
	CyberOnlyActors.Reset();
	SourceOnlyActors.Reset();

	TMap<FName, int32> PairIndices;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor) || Actor == this)
		{
			continue;
		}

		FName MappingTag = NAME_None;
		for (const FName Tag : Actor->Tags)
		{
			if (Tag.ToString().StartsWith(PhaseMapTagPrefix.ToString()))
			{
				MappingTag = Tag;
				break;
			}
		}

		const bool bIsCyber = Actor->ActorHasTag(CyberVisualTag);
		const bool bIsSource = Actor->ActorHasTag(SourceVisualTag);
		if ((!bIsCyber && !bIsSource) || (bIsCyber && bIsSource))
		{
			continue;
		}

		if (MappingTag.IsNone())
		{
			(bIsCyber ? CyberOnlyActors : SourceOnlyActors).Add(Actor);
			continue;
		}

		int32* ExistingIndex = PairIndices.Find(MappingTag);
		if (!ExistingIndex)
		{
			ExistingIndex = &PairIndices.Add(MappingTag, MappedPairs.AddDefaulted());
			MappedPairs[*ExistingIndex].MappingTag = MappingTag;
		}
		FPhaseActorPair& Pair = MappedPairs[*ExistingIndex];
		if (bIsCyber)
		{
			Pair.CyberActor = Actor;
		}
		else
		{
			Pair.SourceActor = Actor;
		}
	}

	for (const FPhaseActorPair& Pair : MappedPairs)
	{
		if (!Pair.CyberActor.IsValid() || !Pair.SourceActor.IsValid())
		{
			UE_LOG(LogRDCAPlayer, Warning,
				TEXT("Arena phase map incomplete for tag %s. Cyber=%s Source=%s"),
				*Pair.MappingTag.ToString(), *GetNameSafe(Pair.CyberActor.Get()), *GetNameSafe(Pair.SourceActor.Get()));
		}
	}

	UE_LOG(LogRDCAPlayer, Log, TEXT("Arena phase controller cached %d mapped pairs, %d Cyber-only actors and %d Source-only actors."),
		MappedPairs.Num(), CyberOnlyActors.Num(), SourceOnlyActors.Num());
}

void AArenaPhaseController::InitializeCyberRift()
{
	for (FPhaseActorPair& Pair : MappedPairs)
	{
		Pair.bRevealed = false;
		if (Pair.CyberActor.IsValid())
		{
			Pair.CyberActor->SetActorHiddenInGame(false);
			if (IsRingPair(Pair))
			{
				SetActorCollision(Pair.CyberActor.Get(), true);
			}
		}
		if (Pair.SourceActor.IsValid())
		{
			Pair.SourceActor->SetActorHiddenInGame(true);
			SetActorCollision(Pair.SourceActor.Get(), false);
		}
	}
	for (const TWeakObjectPtr<AActor>& Actor : CyberOnlyActors)
	{
		if (Actor.IsValid())
		{
			Actor->SetActorHiddenInGame(false);
		}
	}
	for (const TWeakObjectPtr<AActor>& Actor : SourceOnlyActors)
	{
		if (Actor.IsValid())
		{
			Actor->SetActorHiddenInGame(true);
			SetActorCollision(Actor.Get(), false);
		}
	}
}

void AArenaPhaseController::UpdateMaterialSphere() const
{
	if (!bUseMaterialSphereMask || !PhaseMaterialParameters || !GetWorld())
	{
		return;
	}

	UKismetMaterialLibrary::SetVectorParameterValue(
		GetWorld(), PhaseMaterialParameters, SphereCenterParameter,
		FLinearColor(PhaseOrigin.X, PhaseOrigin.Y, PhaseOrigin.Z, 1.0f));
	UKismetMaterialLibrary::SetScalarParameterValue(
		GetWorld(), PhaseMaterialParameters, SphereRadiusParameter, CurrentRadius);
	UKismetMaterialLibrary::SetScalarParameterValue(
		GetWorld(), PhaseMaterialParameters, SphereEdgeWidthParameter, SphereEdgeWidth);
}

void AArenaPhaseController::RevealPair(FPhaseActorPair& Pair)
{
	if (Pair.bRevealed)
	{
		return;
	}

	if (Pair.SourceActor.IsValid())
	{
		Pair.SourceActor->SetActorHiddenInGame(false);
	}
	if (IsRingPair(Pair))
	{
		// Enable the new floor before disabling the old one: there is never a
		// physics frame in which the player can fall through the arena.
		SetActorCollision(Pair.SourceActor.Get(), true);
	}
	if (Pair.CyberActor.IsValid())
	{
		Pair.CyberActor->SetActorHiddenInGame(true);
		if (IsRingPair(Pair))
		{
			SetActorCollision(Pair.CyberActor.Get(), false);
		}
	}
	Pair.bRevealed = true;
}

void AArenaPhaseController::FinalizeSourceCodeVoid()
{
	for (FPhaseActorPair& Pair : MappedPairs)
	{
		if (!Pair.bRevealed)
		{
			RevealPair(Pair);
		}
	}
	for (const TWeakObjectPtr<AActor>& Actor : CyberOnlyActors)
	{
		if (Actor.IsValid())
		{
			Actor->SetActorHiddenInGame(true);
			SetActorCollision(Actor.Get(), false);
		}
	}
	for (const TWeakObjectPtr<AActor>& Actor : SourceOnlyActors)
	{
		if (Actor.IsValid())
		{
			Actor->SetActorHiddenInGame(false);
		}
	}

	bTransitionActive = false;
	bTransitionComplete = true;
	if (BossEncounter.IsValid())
	{
		BossEncounter->SetEncounterHold(false);
	}
	UE_LOG(LogRDCAPlayer, Log, TEXT("Arena phase transition completed."));
}

void AArenaPhaseController::SetActorCollision(AActor* Actor, const bool bEnabled) const
{
	if (!Actor)
	{
		return;
	}
	TArray<UPrimitiveComponent*> Components;
	Actor->GetComponents<UPrimitiveComponent>(Components);
	for (UPrimitiveComponent* Component : Components)
	{
		if (!Component)
		{
			continue;
		}
		Component->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
}

float AArenaPhaseController::GetRevealRadius(const FPhaseActorPair& Pair) const
{
	if (IsRingPair(Pair))
	{
		return RingRevealRadius;
	}
	const AActor* ReferenceActor = Pair.CyberActor.IsValid()
		? Pair.CyberActor.Get()
		: Pair.SourceActor.Get();
	return ReferenceActor
		? FVector::Dist(PhaseOrigin, ReferenceActor->GetActorLocation()) + ObjectRevealRadiusOffset
		: MaximumRadius;
}

bool AArenaPhaseController::IsRingPair(const FPhaseActorPair& Pair) const
{
	return Pair.MappingTag == RingMappingTag;
}
