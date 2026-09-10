#include "Arena/ArenaPhaseController.h"

#include "Boss/BossEncounterComponent.h"
#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "rdca.h"

namespace
{
bool HasPhaseTag(const AActor& Actor, const FName Tag)
{
	if (Actor.ActorHasTag(Tag))
	{
		return true;
	}

	TInlineComponentArray<UActorComponent*> Components(&Actor);
	for (const UActorComponent* Component : Components)
	{
		if (Component && Component->ComponentHasTag(Tag))
		{
			return true;
		}
	}
	return false;
}

FName FindPhaseMappingTag(const AActor& Actor, const FName Prefix)
{
	for (const FName Tag : Actor.Tags)
	{
		if (Tag.ToString().StartsWith(Prefix.ToString()))
		{
			return Tag;
		}
	}

	TInlineComponentArray<UActorComponent*> Components(&Actor);
	for (const UActorComponent* Component : Components)
	{
		if (!Component)
		{
			continue;
		}
		for (const FName Tag : Component->ComponentTags)
		{
			if (Tag.ToString().StartsWith(Prefix.ToString()))
			{
				return Tag;
			}
		}
	}
	return NAME_None;
}
}

AArenaPhaseController::AArenaPhaseController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	PhaseExpansionWaveVisual = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("PhaseExpansionWaveVisual"));
	PhaseExpansionWaveVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PhaseExpansionWaveVisual->SetGenerateOverlapEvents(false);
	PhaseExpansionWaveVisual->SetCastShadow(false);
	PhaseExpansionWaveVisual->SetReceivesDecals(false);
	PhaseExpansionWaveVisual->SetMobility(EComponentMobility::Movable);
	PhaseExpansionWaveVisual->SetHiddenInGame(true);
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
	UpdateExpansionWaveVisual(false);

	if (bDebugAutoStartTransitionOnBeginPlay)
	{
		bDebugTransitionPending = true;
		DebugTransitionDelayRemaining = FMath::Max(DebugTransitionStartDelay, 0.0f);
	}
}

void AArenaPhaseController::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bDebugTransitionPending)
	{
		DebugTransitionDelayRemaining = FMath::Max(
			DebugTransitionDelayRemaining - DeltaTime,
			0.0f);
		if (DebugTransitionDelayRemaining <= 0.0f)
		{
			bDebugTransitionPending = false;
			ActiveExpansionDuration = FMath::Max(DebugExpansionDuration, 0.1f);
			StartPhaseTransition();
		}
	}

	if (!bTransitionActive && !bTransitionComplete
		&& bStartWhenBossEntersPhase2 && BossEncounter.IsValid()
		&& BossEncounter->GetCombatPhase() == EBossCombatPhase::Phase2)
	{
		ActiveExpansionDuration = FMath::Max(ExpansionDuration, 0.1f);
		StartPhaseTransition();
	}

	if (!bTransitionActive)
	{
		return;
	}

	TransitionElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(
		TransitionElapsed / FMath::Max(ActiveExpansionDuration, 0.1f), 0.0f, 1.0f);
	CurrentRadius = FMath::Lerp(0.0f, MaximumRadius, Alpha);
	UpdateMaterialSphere();
	UpdateExpansionWaveVisual(true);

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
	// Blueprint calls use the normal Phase 2 duration unless the debug path
	// explicitly supplied its temporary slow duration above.
	if (ActiveExpansionDuration <= 0.0f)
	{
		ActiveExpansionDuration = FMath::Max(ExpansionDuration, 0.1f);
	}
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
	UpdateExpansionWaveVisual(true);
	UE_LOG(LogRDCAPlayer, Log, TEXT("Arena phase transition started. Origin=%s Radius=%.0f Duration=%.2f"),
		*PhaseOrigin.ToCompactString(), MaximumRadius, ActiveExpansionDuration);
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

		const FName MappingTag = FindPhaseMappingTag(*Actor, PhaseMapTagPrefix);
		const bool bIsCyber = HasPhaseTag(*Actor, CyberVisualTag);
		const bool bIsSource = HasPhaseTag(*Actor, SourceVisualTag);
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
	UpdateExpansionWaveVisual(false);
	for (FPhaseActorPair& Pair : MappedPairs)
	{
		Pair.bRevealed = false;
		if (Pair.CyberActor.IsValid())
		{
			Pair.CyberActor->SetActorHiddenInGame(false);
			// Arena art is visual only. The dedicated Floor Disc and Combat Bounds
			// own gameplay collision, so a phase swap must never revive collision
			// on the high-triangle visual rings.
			SetActorCollision(Pair.CyberActor.Get(), false);
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
			SetActorCollision(Actor.Get(), false);
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

void AArenaPhaseController::UpdateExpansionWaveVisual(const bool bShouldBeVisible)
{
	if (!PhaseExpansionWaveVisual)
	{
		return;
	}

	PhaseExpansionWaveVisual->SetWorldLocation(PhaseOrigin);
	if (bShouldBeVisible)
	{
		const float VisualRadius = FMath::Max(
			CurrentRadius + ExpansionWaveRadiusOffset,
			0.1f);
		const float UniformScale = VisualRadius
			/ FMath::Max(ExpansionWaveMeshBaseRadius, 0.01f);
		PhaseExpansionWaveVisual->SetWorldScale3D(FVector(UniformScale));
	}
	PhaseExpansionWaveVisual->SetHiddenInGame(!bShouldBeVisible);
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
		SetActorCollision(Pair.SourceActor.Get(), false);
	}
	if (Pair.CyberActor.IsValid())
	{
		Pair.CyberActor->SetActorHiddenInGame(true);
		SetActorCollision(Pair.CyberActor.Get(), false);
	}
	Pair.bRevealed = true;
}

void AArenaPhaseController::FinalizeSourceCodeVoid()
{
	UpdateExpansionWaveVisual(false);
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
