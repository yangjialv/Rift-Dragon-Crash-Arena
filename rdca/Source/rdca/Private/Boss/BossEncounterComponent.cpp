#include "Boss/BossEncounterComponent.h"

#include "Audio/RDCAAudio.h"
#include "Boss/BossWeakPointComponent.h"
#include "Boss/BossFanProjectile.h"
#include "Boss/BossSweepLaser.h"
#include "Components/BoxComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/PhaseCrashComponent.h"
#include "Player/PlayerHealthComponent.h"
#include "ProceduralMeshComponent.h"
#include "rdca.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

namespace
{
float GetVisualWorldRadius(UStaticMeshComponent* Visual)
{
	if (!Visual)
	{
		return 0.0f;
	}

	Visual->UpdateBounds();
	return FMath::Max(
		Visual->Bounds.BoxExtent.X,
		Visual->Bounds.BoxExtent.Y);
}
}

UBossEncounterComponent::UBossEncounterComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	FanProjectileClass = ABossFanProjectile::StaticClass();
	SweepLaserClass = ABossSweepLaser::StaticClass();
	AirborneAttackWeights.Shockwave = 10.0f;
	AirborneAttackWeights.AimedVolley = 30.0f;
	AirborneAttackWeights.SweepLaser = 35.0f;
	AttachedAttackWeights.Shockwave = 5.0f;
	AttachedAttackWeights.AimedVolley = 10.0f;
	AttachedAttackWeights.SweepLaser = 40.0f;
}

EBossCombatPhase UBossEncounterComponent::GetCombatPhase() const
{
	if (WeakPoint.IsValid())
	{
		if (WeakPoint->IsBossDefeated())
		{
			return EBossCombatPhase::Dead;
		}

		// Phase two begins at the configured damage threshold itself, rather
		// than waiting for the next attack-selection pass. This lets the arena
		// transition react on the decisive weak-point hit.
		if (WeakPoint->GetCurrentHitPoints()
			<= GetEffectivePhase2StartHitPoints())
		{
			return EBossCombatPhase::Phase2;
		}
	}
	return CombatPhase;
}

int32 UBossEncounterComponent::GetEffectivePhase2StartHitPoints() const
{
	if (!WeakPoint.IsValid())
	{
		return FMath::Max(Phase2StartHitPoints, 1);
	}

	const int32 MaximumHitPoints = WeakPoint->GetMaximumHitPoints();
	if (MaximumHitPoints <= 1)
	{
		return 0;
	}
	return FMath::Clamp(Phase2StartHitPoints, 1, MaximumHitPoints - 1);
}

float UBossEncounterComponent::GetPhase2InterAttackDelay() const
{
	return FMath::Max(Phase2InterAttackDelay, 0.0f);
}

float UBossEncounterComponent::GetStateProgress() const
{
	const float Duration = GetCurrentStateDuration();
	return Duration > 0.0f
		? FMath::Clamp(StateElapsed / Duration, 0.0f, 1.0f)
		: 1.0f;
}

float UBossEncounterComponent::GetStateRemainingTime() const
{
	return FMath::Max(GetCurrentStateDuration() - StateElapsed, 0.0f);
}

float UBossEncounterComponent::GetCurrentStateDuration() const
{
	switch (EncounterState)
	{
	case EBossEncounterState::Idle:
		return InitialIdleDuration;
	case EBossEncounterState::SelectingAttack:
		return 0.0f;
	case EBossEncounterState::Preparing:
		return GetCurrentAttackWarningDuration();
	case EBossEncounterState::Attacking:
		return GetCurrentAttackActiveDuration();
	case EBossEncounterState::Recovery:
		return GetCombatPhase() == EBossCombatPhase::Phase2
			? GetPhase2InterAttackDelay()
			: RecoveryDuration;
	case EBossEncounterState::WeakPointExposed:
		return (GetCombatPhase() == EBossCombatPhase::Phase2
			? Phase2WeakPointExposedDuration
			: WeakPointExposedDuration)
			* FMath::Max(WeakPointStunDurationMultiplier, 0.1f);
	case EBossEncounterState::Dead:
	default:
		return 0.0f;
	}
}

void UBossEncounterComponent::BeginPlay()
{
	Super::BeginPlay();

	WeakPoint = GetOwner()->FindComponentByClass<UBossWeakPointComponent>();
	if (WeakPoint.IsValid())
	{
		WeakPoint->OnWeakPointCrash.AddUniqueDynamic(
			this,
			&UBossEncounterComponent::HandleWeakPointCrash);
	}
	ResolveBossVisual();
	CreateSphereMaskedBossPhaseVisual();
	ProjectileOrigin = FindNamedSceneComponent(TEXT("ProjectileOrigin"));
	LaserOrigin = FindNamedSceneComponent(TEXT("LaserOrigin"));
	ShockwaveOrigin = FindNamedSceneComponent(TEXT("ShockwaveOrigin"));
	WeakPointOrigin = FindNamedSceneComponent(TEXT("WeakPointOrigin"));
	if (ShockwaveOrigin.IsValid())
	{
		ShockwaveOrigin->SetMobility(EComponentMobility::Movable);
	}
	TArray<UStaticMeshComponent*> MeshComponents;
	GetOwner()->GetComponents<UStaticMeshComponent>(MeshComponents);
	for (UStaticMeshComponent* Mesh : MeshComponents)
	{
		if (Mesh->GetName().Equals(TEXT("ShockwaveVisual"), ESearchCase::IgnoreCase))
		{
			ShockwaveVisual = Mesh;
			Mesh->SetMobility(EComponentMobility::Movable);
			ShockwaveBaseScale = Mesh->GetRelativeScale3D();
			// The Blueprint Construction Script already calibrates this component
			// to the configured initial radius. Preserve that authored scale as the
			// runtime reference instead of deriving a new reference from mesh bounds.
			ShockwaveBaseWorldRadius = FMath::Max(
				FMath::Min(ShockwaveInitialRadius, ShockwaveExpandedMaximumRadius),
				1.0f);
			Mesh->SetVisibility(false);
			UE_LOG(
				LogRDCAPlayer,
				Warning,
				TEXT("Shockwave visual resolved. Component=%s Mesh=%s InitialScale=%s ReferenceRadius=%.1f Mobility=%d"),
				*GetNameSafe(Mesh),
				*GetNameSafe(Mesh->GetStaticMesh()),
				*ShockwaveBaseScale.ToString(),
				ShockwaveBaseWorldRadius,
				static_cast<int32>(Mesh->Mobility));
		}
		else if (Mesh->GetName().Equals(TEXT("WeakPoint"), ESearchCase::IgnoreCase))
		{
			WeakPointVisual = Mesh;
		}
	}
	if (ShockwaveVisual.IsValid())
	{
		ShockwaveVisual->SetWorldLocation(
			GetShockwaveOriginLocation());
	}
	if (WeakPointOrigin.IsValid())
	{
		const FVector OriginLocation =
			WeakPointOrigin->GetComponentLocation();
		if (WeakPoint.IsValid())
		{
			WeakPoint->SetWorldLocation(OriginLocation);
		}
		if (WeakPointVisual.IsValid())
		{
			WeakPointVisual->SetWorldLocation(OriginLocation);
		}
	}

	if (!WeakPoint.IsValid() || !ShockwaveVisual.IsValid())
	{
		UE_LOG(
			LogRDCAPlayer,
			Warning,
			TEXT("Boss Encounter setup incomplete. Boss=%s WeakPoint=%s ShockwaveVisual=%s"),
			*GetNameSafe(GetOwner()),
			WeakPoint.IsValid() ? TEXT("found") : TEXT("missing"),
			ShockwaveVisual.IsValid() ? TEXT("found") : TEXT("missing"));
	}
	CreateShockwaveProceduralVisual();
	CreateShockwaveCollisionSegments();
	UpdateBossPhaseMaterial();

	ActiveAttackRandomSeed =
		AttackSelectionRandomSeed >= 0
			? AttackSelectionRandomSeed
			: FMath::Rand();
	AttackRandomStream.Initialize(ActiveAttackRandomSeed);
	SetEncounterState(EBossEncounterState::Idle);
}

#if WITH_EDITOR
void UBossEncounterComponent::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateShockwaveEditorPreviewScales();
}
#endif

void UBossEncounterComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateBossPhaseMaterial();

	if (WeakPoint.IsValid() && WeakPoint->IsBossDefeated())
	{
		if (EncounterState != EBossEncounterState::Dead)
		{
			SetEncounterState(EBossEncounterState::Dead);
		}
		StopEncounter();
		return;
	}
	if (EncounterState == EBossEncounterState::Dead || bEncounterStopped)
	{
		return;
	}
	if (bIntroHold)
	{
		return;
	}

	StateElapsed += DeltaTime;
	UpdateLaserVerticalMovement();
	UpdateBossFacing(DeltaTime);
	switch (EncounterState)
	{
	case EBossEncounterState::Idle:
		if (StateElapsed >= InitialIdleDuration)
		{
			SetEncounterState(EBossEncounterState::SelectingAttack);
		}
		break;
	case EBossEncounterState::SelectingAttack:
		SelectNextAttack();
		break;
	case EBossEncounterState::Preparing:
		if (CurrentAttack == EBossAttackType::SweepLaser)
		{
			UpdateLaserWarning();
		}
		if (StateElapsed >= GetCurrentAttackWarningDuration())
		{
			SetEncounterState(EBossEncounterState::Attacking);
		}
		break;
	case EBossEncounterState::Attacking:
		if (CurrentAttack == EBossAttackType::Shockwave)
		{
			TickShockwaveAttack();
		}
		else if (CurrentAttack == EBossAttackType::AimedVolley
			|| CurrentAttack == EBossAttackType::FanBarrage)
		{
			TickBarrage(DeltaTime);
		}
		if (StateElapsed >= GetCurrentAttackActiveDuration())
		{
			const bool bPhase2 =
				GetCombatPhase() == EBossCombatPhase::Phase2;
			const int32 AttacksBeforeStun = bPhase2
				? FMath::Max(Phase2AttacksBeforeStun, 1)
				: FMath::Max(AttacksBeforeWeakPointExposure, 1);
			++CompletedAttacksSinceExposure;
			const bool bStunImmediately = !bDebugForceBarrage
				&& CompletedAttacksSinceExposure >= AttacksBeforeStun;
			if (bStunImmediately)
			{
				// The final attack of a round goes straight into Stun. Never insert
				// Recovery or the Phase 2 inter-attack delay at this boundary.
				SetEncounterState(EBossEncounterState::WeakPointExposed);
			}
			else
			{
				SetEncounterState(EBossEncounterState::Recovery);
			}
		}
		break;
	case EBossEncounterState::Recovery:
		if (StateElapsed >= GetCurrentStateDuration())
		{
			if (bDebugForceBarrage)
			{
				CompletedAttacksSinceExposure = 0;
				ResetPhase2Round();
				SetEncounterState(EBossEncounterState::SelectingAttack);
			}
			else
			{
				SetEncounterState(EBossEncounterState::SelectingAttack);
			}
		}
		break;
	case EBossEncounterState::WeakPointExposed:
		if (StateElapsed >= GetCurrentStateDuration())
		{
			CompletedAttacksSinceExposure = 0;
			ResetPhase2Round();
			SetEncounterState(EBossEncounterState::SelectingAttack);
		}
		break;
	default:
		break;
	}

}

void UBossEncounterComponent::SetIntroHold(const bool bHold)
{
	SetEncounterHold(bHold);
}

void UBossEncounterComponent::SetEncounterHold(const bool bHold)
{
	if (EncounterState == EBossEncounterState::Dead || bEncounterStopped)
	{
		return;
	}
	bIntroHold = bHold;
	if (bIntroHold)
	{
		RestoreBossLaserHeight();
		CurrentAttack = EBossAttackType::None;
		StateElapsed = 0.0f;
		if (EncounterState != EBossEncounterState::Idle)
		{
			SetEncounterState(EBossEncounterState::Idle);
		}
	}
	else
	{
		StateElapsed = 0.0f;
	}

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss encounter hold changed. Boss=%s Held=%s"),
		*GetNameSafe(GetOwner()),
		bIntroHold ? TEXT("true") : TEXT("false"));
}

void UBossEncounterComponent::StopEncounter()
{
	if (bEncounterStopped)
	{
		return;
	}
	bEncounterStopped = true;
	RestoreBossLaserHeight();

	if (ShockwaveVisual.IsValid())
	{
		ShockwaveVisual->SetVisibility(false);
	}
	if (ActiveSweepLaser.IsValid())
	{
		ActiveSweepLaser->Destroy();
		ActiveSweepLaser.Reset();
	}

	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* AttackActor = *It;
			if (IsValid(AttackActor)
				&& AttackActor->GetOwner() == GetOwner()
				&& (AttackActor->IsA<ABossFanProjectile>()
					|| AttackActor->IsA<ABossSweepLaser>()))
			{
				AttackActor->Destroy();
			}
		}
	}

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss encounter stopped. Boss=%s"),
		*GetNameSafe(GetOwner()));
}

void UBossEncounterComponent::SetEncounterState(
	const EBossEncounterState NewState)
{
	if (EncounterState == NewState && StateElapsed > 0.0f)
	{
		return;
	}

	const EBossEncounterState PreviousState = EncounterState;
	if (bLaserHeightAdjusted
		&& ((PreviousState == EBossEncounterState::Recovery
				&& NewState != EBossEncounterState::Recovery)
			|| (PreviousState == EBossEncounterState::WeakPointExposed
				&& NewState != EBossEncounterState::WeakPointExposed)
			|| NewState == EBossEncounterState::Dead))
	{
		RestoreBossLaserHeight();
	}
	EncounterState = NewState;
	StateElapsed = 0.0f;

	const bool bExposed = NewState == EBossEncounterState::WeakPointExposed;
	if (WeakPoint.IsValid())
	{
		WeakPoint->SetExposed(bExposed);
	}
	UpdateWeakPointVisual(bExposed);

	if (NewState == EBossEncounterState::Preparing)
	{
		BeginCurrentAttackWarning();
	}
	else if (NewState == EBossEncounterState::Attacking)
	{
		BeginCurrentAttack();
	}
	else if (NewState == EBossEncounterState::Recovery)
	{
		FinishCurrentAttack();
	}
	else if (NewState == EBossEncounterState::WeakPointExposed
		&& PreviousState == EBossEncounterState::Attacking)
	{
		FinishCurrentAttack();
	}
	else if (NewState == EBossEncounterState::Dead)
	{
		RDCAAudio::PlayAtLocation(
			this,
			ERDCAAudioCue::BossDeath,
			GetOwner()->GetActorLocation(),
			0.92f);
		RDCAAudio::PlayAtLocation(
			this,
			ERDCAAudioCue::BossRoar,
			GetOwner()->GetActorLocation(),
			0.78f,
			0.72f);
		FinishCurrentAttack();
	}

	OnEncounterStateChanged.Broadcast(PreviousState, NewState);
	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss encounter state. Boss=%s From=%d To=%d"),
		*GetNameSafe(GetOwner()),
		static_cast<int32>(PreviousState),
		static_cast<int32>(NewState));
}

void UBossEncounterComponent::HandleWeakPointCrash(
	APawn* CrashingPawn,
	const bool bWasEffective,
	const FHitResult& Hit)
{
	if (!bWasEffective
		|| EncounterState != EBossEncounterState::WeakPointExposed)
	{
		return;
	}

	// One exposure represents one Boss HP opportunity. Closing it immediately
	// prevents multiple Anchors from removing several HP during the same stun.
	CompletedAttacksSinceExposure = 0;
	ResetPhase2Round();
	if (WeakPoint.IsValid() && WeakPoint->IsBossDefeated())
	{
		SetEncounterState(EBossEncounterState::Dead);
	}
	else if (WeakPoint.IsValid())
	{
		SetEncounterState(EBossEncounterState::Idle);
	}

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss weak-point window consumed. Boss=%s Player=%s RemainingHP=%d Hit=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(CrashingPawn),
		WeakPoint.IsValid() ? WeakPoint->GetCurrentHitPoints() : -1,
		*Hit.ImpactPoint.ToCompactString());
}

void UBossEncounterComponent::SelectNextAttack()
{
	if (WeakPoint.IsValid())
	{
		CombatPhase = WeakPoint->GetCurrentHitPoints()
			<= GetEffectivePhase2StartHitPoints()
			? EBossCombatPhase::Phase2
			: EBossCombatPhase::Phase1;
	}
	LastObservedPlayerState = ObservePlayerSpatialState();
	if (const APawn* PlayerPawn =
			UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		LockedTargetLocation = PlayerPawn->GetActorLocation();
	}
	if (bDebugForceBarrage && FanProjectileClass)
	{
		ActiveBarragePattern = DebugBarragePattern;
		CurrentAttack = IsWideBarragePattern(ActiveBarragePattern)
			? EBossAttackType::FanBarrage
			: EBossAttackType::AimedVolley;
		UE_LOG(
			LogRDCAPlayer,
			Log,
			TEXT("Boss barrage debug selected. Boss=%s Pattern=%d"),
			*GetNameSafe(GetOwner()),
			static_cast<int32>(ActiveBarragePattern));
		SetEncounterState(EBossEncounterState::Preparing);
		return;
	}
	if (bDebugForceSweepLaser && SweepLaserClass)
	{
		CurrentAttack = EBossAttackType::SweepLaser;
		SetEncounterState(EBossEncounterState::Preparing);
		return;
	}
	if (GetCombatPhase() == EBossCombatPhase::Phase2)
	{
		SelectPhase2RoundAttack();
		return;
	}

	SelectPhase1ScheduledAttack();
	const FBossAttackWeights& Weights =
		GetWeightsForPlayerState(LastObservedPlayerState);
	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss Phase 1 scheduled attack. Boss=%s Round=%d Step=%d PlayerState=%d Previous=%d Selected=%d Pattern=%d FallbackWeights=(Shockwave=%.1f AimedVolley=%.1f Laser=%.1f) Seed=%d"),
		*GetNameSafe(GetOwner()),
		WeakPoint.IsValid()
			? FMath::Max(
				WeakPoint->GetMaximumHitPoints()
					- WeakPoint->GetCurrentHitPoints(),
				0)
			: 0,
		CompletedAttacksSinceExposure,
		static_cast<int32>(LastObservedPlayerState),
		static_cast<int32>(PreviousAttack),
		static_cast<int32>(CurrentAttack),
		static_cast<int32>(ActiveBarragePattern),
		Weights.Shockwave,
		Weights.AimedVolley,
		Weights.SweepLaser,
		ActiveAttackRandomSeed);
	SetEncounterState(EBossEncounterState::Preparing);
}

EBossAttackType UBossEncounterComponent::ChooseWeightedAttack(
	const FBossAttackWeights& Weights)
{
	if (bDebugForceSweepLaser && SweepLaserClass)
	{
		return EBossAttackType::SweepLaser;
	}

	struct FWeightedCandidate
	{
		EBossAttackType Attack = EBossAttackType::None;
		float Weight = 0.0f;
	};

	TArray<FWeightedCandidate> Candidates;
	Candidates.Add({EBossAttackType::Shockwave, Weights.Shockwave});
	if (FanProjectileClass)
	{
		Candidates.Add({EBossAttackType::AimedVolley, Weights.AimedVolley});
	}
	if (SweepLaserClass)
	{
		Candidates.Add({EBossAttackType::SweepLaser, Weights.SweepLaser});
	}

	float TotalWithoutRepeat = 0.0f;
	for (const FWeightedCandidate& Candidate : Candidates)
	{
		if (Candidate.Attack != PreviousAttack)
		{
			TotalWithoutRepeat += FMath::Max(Candidate.Weight, 0.0f);
		}
	}
	const bool bCanAvoidRepeat = TotalWithoutRepeat > 0.0f;
	float TotalWeight = 0.0f;
	for (const FWeightedCandidate& Candidate : Candidates)
	{
		if (!bCanAvoidRepeat || Candidate.Attack != PreviousAttack)
		{
			TotalWeight += FMath::Max(Candidate.Weight, 0.0f);
		}
	}
	if (TotalWeight <= UE_KINDA_SMALL_NUMBER)
	{
		return EBossAttackType::Shockwave;
	}

	float Roll = AttackRandomStream.FRandRange(0.0f, TotalWeight);
	for (const FWeightedCandidate& Candidate : Candidates)
	{
		if (bCanAvoidRepeat && Candidate.Attack == PreviousAttack)
		{
			continue;
		}
		Roll -= FMath::Max(Candidate.Weight, 0.0f);
		if (Roll <= 0.0f)
		{
			return Candidate.Attack;
		}
	}
	return Candidates.Last().Attack;
}

EPlayerSpatialState UBossEncounterComponent::ObservePlayerSpatialState() const
{
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!PlayerPawn)
	{
		return EPlayerSpatialState::Grounded;
	}
	if (const UPhaseCrashComponent* PhaseCrash =
			PlayerPawn->FindComponentByClass<UPhaseCrashComponent>())
	{
		if (PhaseCrash->IsAttached())
		{
			return EPlayerSpatialState::Attached;
		}
		if (PhaseCrash->IsCrashing())
		{
			return EPlayerSpatialState::Airborne;
		}
	}

	return EPlayerSpatialState::Grounded;
}

const FBossAttackWeights& UBossEncounterComponent::GetWeightsForPlayerState(
	const EPlayerSpatialState PlayerState) const
{
	switch (PlayerState)
	{
	case EPlayerSpatialState::Airborne:
		return AirborneAttackWeights;
	case EPlayerSpatialState::Attached:
		return AttachedAttackWeights;
	case EPlayerSpatialState::Grounded:
	default:
		return GroundedAttackWeights;
	}
}

float UBossEncounterComponent::GetCurrentAttackWarningDuration() const
{
	switch (CurrentAttack)
	{
	case EBossAttackType::AimedVolley:
		return AimedVolleyWarningDuration;
	case EBossAttackType::FanBarrage:
		return FanBarrageWarningDuration;
	case EBossAttackType::SweepLaser:
		return LaserAimWarningDuration
			+ FMath::Max(LaserPostWarningPauseDuration, 0.0f);
	case EBossAttackType::Shockwave:
	default:
		return WarningDuration;
	}
}

float UBossEncounterComponent::GetCurrentAttackActiveDuration() const
{
	switch (CurrentAttack)
	{
	case EBossAttackType::AimedVolley:
	case EBossAttackType::FanBarrage:
		return GetBarrageAttackDuration();
	case EBossAttackType::SweepLaser:
		return LaserActiveSweepDuration;
	case EBossAttackType::Shockwave:
		if (IsEnhancedDoubleShockwaveAttack())
		{
			return GetShockwaveExpansionDuration()
				+ FMath::Max(Phase2ShockwavePulseDelay, 0.0f);
		}
		return GetShockwaveExpansionDuration();
	default:
		return GetShockwaveExpansionDuration();
	}
}

bool UBossEncounterComponent::IsEnhancedDoubleShockwaveAttack() const
{
	return bCurrentPhase2DoubleShockwave
		&& CurrentAttack == EBossAttackType::Shockwave;
}

void UBossEncounterComponent::BeginCurrentAttackWarning()
{
	if (CurrentAttack == EBossAttackType::Shockwave)
	{
		const FVector Origin = GetShockwaveOriginLocation();
		RDCAAudio::PlayAtLocation(
			this, ERDCAAudioCue::BossRoar, GetOwner()->GetActorLocation(), 0.68f);
		RDCAAudio::PlayAtLocation(
			this, ERDCAAudioCue::ShockwaveWarning, Origin, 0.55f);
	}
	else if (CurrentAttack == EBossAttackType::AimedVolley
		|| CurrentAttack == EBossAttackType::FanBarrage)
	{
		RDCAAudio::PlayAtLocation(
			this,
			ERDCAAudioCue::BarrageCharge,
			GetProjectileOriginLocation(),
			0.5f);
	}

	if (ShockwaveVisual.IsValid())
	{
		const bool bShockwave = CurrentAttack == EBossAttackType::Shockwave;
		// The legacy Plane remains the authored radius reference and editor
		// preview. During play the generated Torus is the actual visible Wave.
		ShockwaveVisual->SetVisibility(
			bShockwave && !ShockwaveProceduralVisual);
		SetShockwaveProceduralVisualVisible(bShockwave);
		if (bShockwave)
		{
			// The visual is the authoritative representation of this attack.  Lock its
			// center to the emitter at warning start; later damage queries read this
			// same component location, so animation movement cannot split the damage
			// circle from the displayed ring. A level-space World Anchor, when
			// configured, deliberately takes priority over the Boss-local emitter.
			ShockwaveVisual->SetWorldLocation(GetShockwaveOriginLocation());
			SetShockwaveVisualRadius(FMath::Min(
				ShockwaveInitialRadius,
				ShockwaveExpandedMaximumRadius));
			const float InitialConfiguredRadius = FMath::Max(
				FMath::Min(ShockwaveInitialRadius, ShockwaveExpandedMaximumRadius),
				1.0f);
			ShockwaveWorldUnitsPerConfiguredUnit = FMath::Max(
				GetVisualWorldRadius(ShockwaveVisual.Get()) / InitialConfiguredRadius,
				0.001f);
			UpdateShockwaveProceduralVisual(InitialConfiguredRadius);
			if (ShockwaveProceduralVisual)
			{
				SetShockwaveProceduralVisualMaterial(ShockwaveFireWarningMaterial);
			}
		}
	}
	if (CurrentAttack == EBossAttackType::SweepLaser)
	{
		bLaserAimLocked = false;
		BeginLaserVerticalMovement();
		SpawnLaserWarning();
	}
}

void UBossEncounterComponent::BeginCurrentAttack()
{
	switch (CurrentAttack)
	{
	case EBossAttackType::Shockwave:
		ActiveShockwavePulse = 1;
		bPlayerDamagedBySecondaryShockwave = false;
		if (SecondaryShockwaveProceduralVisual)
		{
			SecondaryShockwaveProceduralVisual->SetVisibility(false);
			SecondaryShockwaveProceduralVisual->SetHiddenInGame(true);
		}
		SetShockwaveCollisionEnabledFor(
			SecondaryShockwaveCollisionSegments, false);
		RDCAAudio::PlayAtLocation(
			this,
			ERDCAAudioCue::ShockwaveRelease,
			GetShockwaveOriginLocation(),
			0.65f);
		PreviousShockwaveRadius = FMath::Min(
			ShockwaveInitialRadius,
			ShockwaveExpandedMaximumRadius);
		bPlayerDamagedThisAttack = false;
		if (ShockwaveProceduralVisual)
		{
			SetShockwaveProceduralVisualMaterial(ShockwaveFireActiveMaterial);
		}
		UE_LOG(
			LogRDCAPlayer,
			Warning,
			TEXT("Shockwave started. Initial=%.1f Maximum=%.1f Speed=%.1f Duration=%.2f VisualRadius=%.1f VisualScale=%s PlayerDistance=%.1f"),
			ShockwaveInitialRadius,
			ShockwaveExpandedMaximumRadius,
			ShockwaveExpansionSpeed,
			GetShockwaveExpansionDuration(),
			GetVisualWorldRadius(ShockwaveVisual.Get()),
			ShockwaveVisual.IsValid()
				? *ShockwaveVisual->GetRelativeScale3D().ToString()
				: TEXT("Missing"),
			UGameplayStatics::GetPlayerPawn(GetWorld(), 0)
				? FVector::Dist2D(
					UGameplayStatics::GetPlayerPawn(GetWorld(), 0)->GetActorLocation(),
					ShockwaveVisual.IsValid()
						? ShockwaveVisual->GetComponentLocation()
						: GetShockwaveOriginLocation())
				: -1.0f);
		UpdateShockwaveCollisionSegments(
			FMath::Min(ShockwaveInitialRadius, ShockwaveExpandedMaximumRadius)
				* ShockwaveWorldUnitsPerConfiguredUnit);
		SetShockwaveCollisionEnabled(true);
		break;
	case EBossAttackType::AimedVolley:
	case EBossAttackType::FanBarrage:
		BeginBarrageAttack();
		break;
	case EBossAttackType::SweepLaser:
		if (ActiveSweepLaser.IsValid())
		{
			if (!bLaserAimLocked)
			{
				LockLaserSweep();
			}
			ConfigureLaserSweepAtPauseEnd();
			ActiveSweepLaser->ActivateLaser();
		}
		break;
	default:
		break;
	}
}

void UBossEncounterComponent::FinishCurrentAttack()
{
	ActiveShockwavePulse = 0;
	SetShockwaveCollisionEnabled(false);
	SetShockwaveProceduralVisualVisible(false);
	SetShockwaveCollisionEnabledFor(
		SecondaryShockwaveCollisionSegments, false);
	if (SecondaryShockwaveProceduralVisual)
	{
		SecondaryShockwaveProceduralVisual->SetVisibility(false);
		SecondaryShockwaveProceduralVisual->SetHiddenInGame(true);
	}
	if (ShockwaveVisual.IsValid())
	{
		ShockwaveVisual->SetVisibility(false);
	}
	if (ActiveSweepLaser.IsValid())
	{
		ActiveSweepLaser->Destroy();
		ActiveSweepLaser.Reset();
	}
	if (CurrentAttack == EBossAttackType::AimedVolley
		|| CurrentAttack == EBossAttackType::FanBarrage)
	{
		PreviousBarragePattern = ActiveBarragePattern;
		bHasPreviousBarragePattern = true;
	}
	PreviousAttack = CurrentAttack;
}

void UBossEncounterComponent::UpdateBossFacing(const float DeltaTime)
{
	if (!GetOwner()
		|| EncounterState == EBossEncounterState::Recovery
		|| EncounterState == EBossEncounterState::WeakPointExposed
		|| EncounterState == EBossEncounterState::Dead)
	{
		return;
	}

	float DesiredYaw = GetOwner()->GetActorRotation().Yaw;
	bool bHasDesiredYaw = false;
	if (CurrentAttack == EBossAttackType::SweepLaser
		&& EncounterState == EBossEncounterState::Attacking
		&& ActiveSweepLaser.IsValid())
	{
		DesiredYaw = ActiveSweepLaser->GetCurrentLaserYaw();
		ActiveSweepLaser->SetActorLocation(GetLaserOriginLocation());
		bHasDesiredYaw = true;
	}
	else if (CurrentAttack == EBossAttackType::SweepLaser
		&& EncounterState == EBossEncounterState::Preparing
		&& bLaserAimLocked)
	{
		DesiredYaw = LaserWarningCurrentYaw;
		bHasDesiredYaw = true;
	}
	else
	{
		FVector TargetLocation = LockedTargetLocation;
		if (EncounterState == EBossEncounterState::Idle
			|| EncounterState == EBossEncounterState::SelectingAttack
			|| (CurrentAttack == EBossAttackType::SweepLaser
				&& EncounterState == EBossEncounterState::Preparing))
		{
			if (const APawn* PlayerPawn =
					UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
			{
				TargetLocation = PlayerPawn->GetActorLocation();
			}
		}

		FVector ToTarget = TargetLocation - GetOwner()->GetActorLocation();
		ToTarget.Z = 0.0f;
		if (ToTarget.Normalize())
		{
			DesiredYaw = ToTarget.Rotation().Yaw;
			bHasDesiredYaw = true;
		}
	}

	if (bHasDesiredYaw)
	{
		FRotator NewRotation = GetOwner()->GetActorRotation();
		NewRotation.Yaw = FMath::FixedTurn(
			NewRotation.Yaw,
			DesiredYaw,
			FMath::Max(BossFacingRotationSpeed, 1.0f) * DeltaTime);
		NewRotation.Pitch = 0.0f;
		NewRotation.Roll = 0.0f;
		GetOwner()->SetActorRotation(NewRotation);
	}
}

void UBossEncounterComponent::UpdateLaserWarning()
{
	if (!ActiveSweepLaser.IsValid() || bLaserAimLocked)
	{
		return;
	}

	if (StateElapsed >= FMath::Max(LaserAimWarningDuration, 0.1f))
	{
		LockLaserSweep();
		return;
	}

	const FVector OriginLocation = GetLaserOriginLocation();
	LaserWarningCurrentYaw = GetOwner()
		? GetOwner()->GetActorRotation().Yaw
		: ActiveSweepLaser->GetActorRotation().Yaw;
	ActiveSweepLaser->UpdateWarningPose(
		OriginLocation,
		LaserWarningCurrentYaw);
}

void UBossEncounterComponent::LockLaserSweep()
{
	if (!ActiveSweepLaser.IsValid() || bLaserAimLocked)
	{
		return;
	}

	const FVector OriginLocation = GetLaserOriginLocation();
	LaserWarningCurrentYaw = GetOwner()
		? GetOwner()->GetActorRotation().Yaw
		: ActiveSweepLaser->GetActorRotation().Yaw;
	bLaserAimLocked = true;
	ActiveSweepLaser->UpdateWarningPose(OriginLocation, LaserWarningCurrentYaw);
	ActiveSweepLaser->ConfigureSweep(
		LaserWarningCurrentYaw,
		LaserWarningCurrentYaw);

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss laser warning locked. Boss=%s Yaw=%.1f Pause=%.2f"),
		*GetNameSafe(GetOwner()),
		LaserWarningCurrentYaw,
		LaserPostWarningPauseDuration);
}

void UBossEncounterComponent::ConfigureLaserSweepAtPauseEnd()
{
	if (!ActiveSweepLaser.IsValid())
	{
		return;
	}
	// The warning uses the stable Boss frame, but the flame must still begin at
	// the mouth's latest animated position after the pause has elapsed.
	ActiveSweepLaser->UpdateWarningPose(
		GetLaserOriginLocation(),
		LaserWarningCurrentYaw);

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	const bool bPlayerInsideWarning = PlayerPawn
		&& ActiveSweepLaser->IsActorInsideWarningArea(PlayerPawn);
	float EndYaw = LaserWarningCurrentYaw;
	float SideSign = 0.0f;
	if (!bPlayerInsideWarning && PlayerPawn && GetOwner())
	{
		FVector ToPlayer = PlayerPawn->GetActorLocation()
			- GetOwner()->GetActorLocation();
		ToPlayer.Z = 0.0f;
		if (ToPlayer.Normalize())
		{
			const FVector LockedForward = FRotator(
				0.0f,
				LaserWarningCurrentYaw,
				0.0f).Vector();
			const float Side = FVector::CrossProduct(
				LockedForward,
				ToPlayer).Z;
			if (!FMath::IsNearlyZero(Side, 0.001f))
			{
				SideSign = Side > 0.0f ? 1.0f : -1.0f;
				EndYaw = LaserWarningCurrentYaw
					+ SideSign * LaserActiveSweepDegrees;
			}
		}
	}

	ActiveSweepLaser->ConfigureSweep(LaserWarningCurrentYaw, EndYaw);
	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss laser pause ended. PlayerInside=%s Side=%s StartYaw=%.1f EndYaw=%.1f"),
		bPlayerInsideWarning ? TEXT("true") : TEXT("false"),
		SideSign > 0.0f
			? TEXT("left")
			: (SideSign < 0.0f ? TEXT("right") : TEXT("straight")),
		LaserWarningCurrentYaw,
		EndYaw);
}

void UBossEncounterComponent::BeginLaserVerticalMovement()
{
	if (!GetOwner())
	{
		return;
	}

	// Capture the flying height once per laser attack. Repeated warning updates
	// must never subtract the descent distance again.
	if (!bLaserHeightAdjusted)
	{
		LaserBaseActorZ = GetOwner()->GetActorLocation().Z;
		bLaserHeightAdjusted = true;
	}
	UpdateLaserVerticalMovement();
}

void UBossEncounterComponent::UpdateLaserVerticalMovement()
{
	if (!bLaserHeightAdjusted || !GetOwner())
	{
		return;
	}

	const float LoweredZ = LaserBaseActorZ
		- FMath::Max(LaserWarningDescentDistance, 0.0f);
	float DesiredZ = GetOwner()->GetActorLocation().Z;
	if (EncounterState == EBossEncounterState::Preparing
		&& CurrentAttack == EBossAttackType::SweepLaser)
	{
		const float Alpha = FMath::Clamp(
			StateElapsed / FMath::Max(LaserWarningDescentDuration, 0.01f),
			0.0f,
			1.0f);
		DesiredZ = FMath::InterpEaseInOut(
			LaserBaseActorZ,
			LoweredZ,
			Alpha,
			2.0f);
	}
	else if (EncounterState == EBossEncounterState::Attacking
		&& CurrentAttack == EBossAttackType::SweepLaser)
	{
		DesiredZ = LoweredZ;
	}
	else if (EncounterState == EBossEncounterState::Recovery
		&& CurrentAttack == EBossAttackType::SweepLaser)
	{
		const float Alpha = FMath::Clamp(
			StateElapsed / FMath::Max(LaserRecoveryAscentDuration, 0.01f),
			0.0f,
			1.0f);
		DesiredZ = FMath::InterpEaseInOut(
			LoweredZ,
			LaserBaseActorZ,
			Alpha,
			2.0f);
	}
	else if (EncounterState == EBossEncounterState::WeakPointExposed
		&& CurrentAttack == EBossAttackType::SweepLaser)
	{
		// The stun starts immediately after the laser. Height recovery continues
		// during the stun instead of inserting a separate Recovery-state delay.
		const float Alpha = FMath::Clamp(
			StateElapsed / FMath::Max(LaserRecoveryAscentDuration, 0.01f),
			0.0f,
			1.0f);
		DesiredZ = FMath::InterpEaseInOut(
			LoweredZ,
			LaserBaseActorZ,
			Alpha,
			2.0f);
	}
	else
	{
		RestoreBossLaserHeight();
		return;
	}

	FVector NewLocation = GetOwner()->GetActorLocation();
	NewLocation.Z = DesiredZ;
	GetOwner()->SetActorLocation(NewLocation);
}

void UBossEncounterComponent::RestoreBossLaserHeight()
{
	if (!bLaserHeightAdjusted)
	{
		return;
	}

	if (GetOwner())
	{
		FVector NewLocation = GetOwner()->GetActorLocation();
		NewLocation.Z = LaserBaseActorZ;
		GetOwner()->SetActorLocation(NewLocation);
	}
	bLaserHeightAdjusted = false;
}

void UBossEncounterComponent::SpawnLaserWarning()
{
	if (!GetWorld() || !SweepLaserClass)
	{
		return;
	}

	if (ActiveSweepLaser.IsValid())
	{
		ActiveSweepLaser->Destroy();
	}

	const FVector SpawnLocation = GetLaserOriginLocation();
	const float CenterYaw = GetOwner()
		? GetOwner()->GetActorRotation().Yaw
		: 0.0f;
	LaserWarningCurrentYaw = CenterYaw;

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABossSweepLaser* Laser = GetWorld()->SpawnActor<ABossSweepLaser>(
		SweepLaserClass,
		SpawnLocation,
		FRotator::ZeroRotator,
		SpawnParameters);
	if (Laser)
	{
		Laser->InitializeLaser(
			CenterYaw,
			CenterYaw,
			LaserActiveSweepDuration,
			LaserDamage);
		ActiveSweepLaser = Laser;
	}
}

void UBossEncounterComponent::SetBarragePattern(
	const EBossBarragePattern Pattern)
{
	ActiveBarragePattern = Pattern;
	CurrentAttack = IsWideBarragePattern(Pattern)
		? EBossAttackType::FanBarrage
		: EBossAttackType::AimedVolley;
}

void UBossEncounterComponent::SelectPhase1ScheduledAttack()
{
	const int32 DamageTaken = WeakPoint.IsValid()
		? FMath::Max(
			WeakPoint->GetMaximumHitPoints()
				- WeakPoint->GetCurrentHitPoints(),
			0)
		: 0;
	const bool bFirstTeachingRound = DamageTaken == 0;
	const int32 Step = FMath::Clamp(CompletedAttacksSinceExposure, 0, 1);

	if (Step == 0 && FanProjectileClass)
	{
		if (bFirstTeachingRound)
		{
			SetBarragePattern(EBossBarragePattern::LegacyAimedVolley);
		}
		else
		{
			// Repeating a missed weak-point round swaps between the two
			// remaining Phase 1 lessons instead of replaying the same pattern.
			EBossBarragePattern Pattern;
			if (bHasPreviousBarragePattern
				&& PreviousBarragePattern == EBossBarragePattern::PredictiveVolley)
			{
				Pattern = EBossBarragePattern::GapWall;
			}
			else if (bHasPreviousBarragePattern
				&& PreviousBarragePattern == EBossBarragePattern::GapWall)
			{
				Pattern = EBossBarragePattern::PredictiveVolley;
			}
			else
			{
				Pattern = AttackRandomStream.FRand() < 0.5f
					? EBossBarragePattern::PredictiveVolley
					: EBossBarragePattern::GapWall;
			}
			SetBarragePattern(Pattern);
		}
		return;
	}

	if (Step == 1)
	{
		if (bFirstTeachingRound || !SweepLaserClass)
		{
			CurrentAttack = EBossAttackType::Shockwave;
		}
		else
		{
			CurrentAttack = EBossAttackType::SweepLaser;
		}
		return;
	}

	const FBossAttackWeights& Weights =
		GetWeightsForPlayerState(LastObservedPlayerState);
	CurrentAttack = ChooseWeightedAttack(Weights);
	if (CurrentAttack == EBossAttackType::AimedVolley)
	{
		SetBarragePattern(EBossBarragePattern::PredictiveVolley);
	}
}

bool UBossEncounterComponent::IsWideBarragePattern(
	const EBossBarragePattern Pattern) const
{
	return Pattern == EBossBarragePattern::GapWall
		|| Pattern == EBossBarragePattern::RotatingGapWall
		|| Pattern == EBossBarragePattern::DoubleSpiral
		|| Pattern == EBossBarragePattern::LegacyDenseFan;
}

int32 UBossEncounterComponent::GetBarrageStepCount() const
{
	switch (ActiveBarragePattern)
	{
	case EBossBarragePattern::PredictiveVolley:
		return FMath::Max(PredictiveWaveCount, 1);
	case EBossBarragePattern::GapWall:
		return FMath::Max(GapWallWaveCount, 1);
	case EBossBarragePattern::CurvedVolley:
		return FMath::Max(CurvedWaveCount, 1);
	case EBossBarragePattern::HomingVolley:
		return 1;
	case EBossBarragePattern::RotatingGapWall:
		return FMath::Max(RotatingGapWaveCount, 1);
	case EBossBarragePattern::DoubleSpiral:
		return FMath::Max(DoubleSpiralPairCount, 1);
	case EBossBarragePattern::LegacyAimedVolley:
		return FMath::Max(LegacyAimedProjectileCount, 1);
	case EBossBarragePattern::LegacyDenseFan:
		return FMath::Max(LegacyDenseFanProjectileCount, 3);
	default:
		return 1;
	}
}

float UBossEncounterComponent::GetBarrageStepInterval() const
{
	switch (ActiveBarragePattern)
	{
	case EBossBarragePattern::PredictiveVolley:
		return FMath::Max(PredictiveWaveInterval, 0.01f);
	case EBossBarragePattern::GapWall:
	case EBossBarragePattern::RotatingGapWall:
		return FMath::Max(GapWallWaveInterval, 0.01f);
	case EBossBarragePattern::CurvedVolley:
		return FMath::Max(CurvedWaveInterval, 0.01f);
	case EBossBarragePattern::DoubleSpiral:
		return FMath::Max(DoubleSpiralPairInterval, 0.01f);
	case EBossBarragePattern::LegacyAimedVolley:
		return FMath::Max(LegacyAimedShotInterval, 0.01f);
	case EBossBarragePattern::LegacyDenseFan:
		return FMath::Max(LegacyDenseFanShotInterval, 0.01f);
	case EBossBarragePattern::HomingVolley:
	default:
		return 0.1f;
	}
}

float UBossEncounterComponent::GetBarrageAttackDuration() const
{
	const float ScheduledDuration =
		(GetBarrageStepCount() - 1) * GetBarrageStepInterval() + 0.15f;
	const float AuthoredMinimum = IsWideBarragePattern(ActiveBarragePattern)
		? FanBarrageAttackDuration
		: AimedVolleyAttackDuration;
	return FMath::Max(AuthoredMinimum, ScheduledDuration);
}

void UBossEncounterComponent::BeginBarrageAttack()
{
	BarrageStepsFired = 0;
	BarrageStepElapsed = 0.0f;
	SpawnBarrageStep(BarrageStepsFired++);
	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss barrage started. Boss=%s Pattern=%d Steps=%d Interval=%.2f"),
		*GetNameSafe(GetOwner()),
		static_cast<int32>(ActiveBarragePattern),
		GetBarrageStepCount(),
		GetBarrageStepInterval());
}

void UBossEncounterComponent::TickBarrage(const float DeltaTime)
{
	if (BarrageStepsFired >= GetBarrageStepCount())
	{
		return;
	}

	BarrageStepElapsed += DeltaTime;
	const float Interval = GetBarrageStepInterval();
	while (BarrageStepElapsed >= Interval
		&& BarrageStepsFired < GetBarrageStepCount())
	{
		BarrageStepElapsed -= Interval;
		SpawnBarrageStep(BarrageStepsFired++);
	}
}

void UBossEncounterComponent::SpawnBarrageStep(const int32 StepIndex)
{
	switch (ActiveBarragePattern)
	{
	case EBossBarragePattern::PredictiveVolley:
		SpawnPredictiveWave(StepIndex);
		break;
	case EBossBarragePattern::GapWall:
		SpawnGapWallWave(StepIndex, false);
		break;
	case EBossBarragePattern::CurvedVolley:
		SpawnCurvedWave(StepIndex);
		break;
	case EBossBarragePattern::HomingVolley:
		SpawnHomingWave();
		break;
	case EBossBarragePattern::RotatingGapWall:
		SpawnGapWallWave(StepIndex, true);
		break;
	case EBossBarragePattern::DoubleSpiral:
		SpawnDoubleSpiralPair(StepIndex);
		break;
	case EBossBarragePattern::LegacyAimedVolley:
		SpawnLegacyAimedProjectile(StepIndex);
		break;
	case EBossBarragePattern::LegacyDenseFan:
		SpawnLegacyDenseFanProjectile(StepIndex);
		break;
	default:
		break;
	}
}

void UBossEncounterComponent::SpawnSharedBarrageProjectile(
	const FVector& Direction,
	const float Speed,
	const EBossProjectileMotionMode MotionMode,
	const float CurveRate,
	const float MaxCurve,
	AActor* HomingTarget,
	const float SizeMultiplier)
{
	const TSubclassOf<ABossFanProjectile> ProjectileClass = FanProjectileClass;
	if (!GetWorld() || !ProjectileClass || Direction.IsNearlyZero())
	{
		return;
	}

	const FVector SpawnLocation = GetProjectileOriginLocation();
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	const float CruiseHeight = PlayerPawn
		? PlayerPawn->GetActorLocation().Z
		: LockedTargetLocation.Z;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABossFanProjectile* Projectile = GetWorld()->SpawnActor<ABossFanProjectile>(
		ProjectileClass,
		SpawnLocation,
		Direction.Rotation(),
		SpawnParameters);
	if (Projectile)
	{
		Projectile->SetActorScale3D(
			Projectile->GetActorScale3D()
				* FMath::Max(SizeMultiplier, 0.1f));
		Projectile->InitializePatternProjectile(
			Direction,
			Speed * FMath::Max(BarrageSpeedMultiplier, 0.1f),
			BarrageProjectileDamage,
			CruiseHeight,
			MotionMode,
			CurveRate,
			MaxCurve,
			HomingTarget,
			HomingTurnDegreesPerSecond,
			HomingDuration,
			HomingStopDistance,
			MaximumHomingAngle);
		Projectile->SetCodePhaseVisual(
			GetCombatPhase() == EBossCombatPhase::Phase2);
		RDCAAudio::PlayNextBarrageShot(this, SpawnLocation);
	}
}

void UBossEncounterComponent::SpawnPredictiveWave(const int32 WaveIndex)
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!PlayerPawn)
	{
		return;
	}

	const FVector SpawnLocation = GetProjectileOriginLocation();
	const float LeadMultiplier = WaveIndex == 0
		? 0.0f
		: (WaveIndex % 2 == 1 ? 1.0f : -0.5f);
	const FVector Target = PlayerPawn->GetActorLocation()
		+ PlayerPawn->GetVelocity()
			* FMath::Max(PredictiveLeadTime, 0.0f)
			* LeadMultiplier;
	FVector CenterDirection = (Target - SpawnLocation).GetSafeNormal();
	if (CenterDirection.IsNearlyZero())
	{
		return;
	}
	FVector Right = FVector::CrossProduct(
		FVector::UpVector,
		CenterDirection).GetSafeNormal();
	const int32 Count = FMath::Max(PredictiveProjectilesPerWave, 1);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float CenteredIndex = Index - (Count - 1) * 0.5f;
		const FVector ShotTarget = Target
			+ Right * CenteredIndex * PredictiveLateralSpacing;
		SpawnSharedBarrageProjectile(
			(ShotTarget - SpawnLocation).GetSafeNormal(),
			BarrageProjectileSpeed,
			EBossProjectileMotionMode::Straight);
	}
}

void UBossEncounterComponent::SpawnGapWallWave(
	const int32 WaveIndex,
	const bool bRotateGap)
{
	const FVector SpawnLocation = GetProjectileOriginLocation();
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	const FVector Target = PlayerPawn
		? PlayerPawn->GetActorLocation()
		: LockedTargetLocation;
	const FVector CenterDirection = (Target - SpawnLocation).GetSafeNormal();
	if (CenterDirection.IsNearlyZero())
	{
		return;
	}

	const int32 Count = FMath::Max(GapWallProjectileCount, 3);
	const float Arc = FMath::Clamp(GapWallArcDegrees, 1.0f, 179.0f);
	const float AngleStep = Arc / static_cast<float>(Count - 1);
	const float RawGapCenter = bRotateGap
		? (WaveIndex - (FMath::Max(RotatingGapWaveCount, 1) - 1) * 0.5f)
			* RotatingGapDegreesPerWave
		: (WaveIndex % 2 == 0 ? -Arc * 0.18f : Arc * 0.18f);
	const int32 SkippedCount = FMath::Clamp(
		GapWallSkippedProjectileCount,
		1,
		Count - 1);
	const float GapHalfAngle = AngleStep * SkippedCount * 0.5f;
	const float GapCenterLimit = FMath::Max(
		Arc * 0.5f - GapHalfAngle,
		0.0f);
	const float GapCenter = FMath::Clamp(
		RawGapCenter,
		-GapCenterLimit,
		GapCenterLimit);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float YawOffset = -Arc * 0.5f + AngleStep * Index;
		if (FMath::Abs(YawOffset - GapCenter) <= GapHalfAngle)
		{
			continue;
		}
		SpawnSharedBarrageProjectile(
			CenterDirection.RotateAngleAxis(
				YawOffset,
				FVector::UpVector).GetSafeNormal(),
			BarrageProjectileSpeed,
			EBossProjectileMotionMode::Straight);
	}
}

void UBossEncounterComponent::SpawnCurvedWave(const int32 WaveIndex)
{
	const FVector SpawnLocation = GetProjectileOriginLocation();
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	const FVector Target = PlayerPawn
		? PlayerPawn->GetActorLocation()
		: LockedTargetLocation;
	const FVector CenterDirection = (Target - SpawnLocation).GetSafeNormal();
	if (CenterDirection.IsNearlyZero())
	{
		return;
	}

	const int32 Count = FMath::Max(CurvedProjectilesPerWave, 1);
	const float SignedCurveRate = FMath::Abs(CurveDegreesPerSecond)
		* (WaveIndex % 2 == 0 ? 1.0f : -1.0f);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Alpha = Count > 1
			? static_cast<float>(Index) / static_cast<float>(Count - 1)
			: 0.5f;
		const float YawOffset = FMath::Lerp(
			-CurvedArcDegrees * 0.5f,
			CurvedArcDegrees * 0.5f,
			Alpha);
		SpawnSharedBarrageProjectile(
			CenterDirection.RotateAngleAxis(
				YawOffset,
				FVector::UpVector).GetSafeNormal(),
			BarrageProjectileSpeed,
			EBossProjectileMotionMode::Curved,
			SignedCurveRate,
			MaximumCurveDegrees);
	}
}

void UBossEncounterComponent::SpawnHomingWave()
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!PlayerPawn)
	{
		return;
	}

	const FVector SpawnLocation = GetProjectileOriginLocation();
	const FVector CenterDirection =
		(PlayerPawn->GetActorLocation() - SpawnLocation).GetSafeNormal();
	const int32 Count = FMath::Max(HomingProjectileCount, 1)
		* FMath::Max(HomingCountMultiplier, 1);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Alpha = Count > 1
			? static_cast<float>(Index) / static_cast<float>(Count - 1)
			: 0.5f;
		const float YawOffset = FMath::Lerp(
			-HomingSpreadDegrees * 0.5f,
			HomingSpreadDegrees * 0.5f,
			Alpha);
		SpawnSharedBarrageProjectile(
			CenterDirection.RotateAngleAxis(
				YawOffset,
				FVector::UpVector).GetSafeNormal(),
			HomingProjectileSpeed,
			EBossProjectileMotionMode::Homing,
			0.0f,
			0.0f,
			PlayerPawn,
			HomingSizeMultiplier);
	}
}

void UBossEncounterComponent::SpawnDoubleSpiralPair(const int32 PairIndex)
{
	const FVector SpawnLocation = GetProjectileOriginLocation();
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	const FVector Target = PlayerPawn
		? PlayerPawn->GetActorLocation()
		: LockedTargetLocation;
	const FVector CenterDirection = (Target - SpawnLocation).GetSafeNormal();
	if (CenterDirection.IsNearlyZero())
	{
		return;
	}

	const float CenteredPair = PairIndex
		- (FMath::Max(DoubleSpiralPairCount, 1) - 1) * 0.5f;
	const float EmissionOffset = CenteredPair * DoubleSpiralDegreesPerPair;
	const float CurveRate = FMath::Abs(CurveDegreesPerSecond);
	for (const float Side : {-1.0f, 1.0f})
	{
		SpawnSharedBarrageProjectile(
			CenterDirection.RotateAngleAxis(
				EmissionOffset * Side,
				FVector::UpVector).GetSafeNormal(),
			BarrageProjectileSpeed,
			EBossProjectileMotionMode::Curved,
			CurveRate * Side,
			FMath::Max(MaximumCurveDegrees, 120.0f));
	}
}

void UBossEncounterComponent::SpawnLegacyAimedProjectile(
	const int32 ShotIndex)
{
	const FVector SpawnLocation = GetProjectileOriginLocation();
	FVector CenterDirection = LockedTargetLocation - SpawnLocation;
	if (!CenterDirection.Normalize())
	{
		return;
	}

	FVector Right = FVector::CrossProduct(
		FVector::UpVector,
		CenterDirection).GetSafeNormal();
	if (Right.IsNearlyZero())
	{
		Right = FVector::RightVector;
	}
	const int32 OffsetStep = (ShotIndex + 1) / 2;
	const float CenteredShotIndex = ShotIndex == 0
		? 0.0f
		: static_cast<float>(OffsetStep)
			* (ShotIndex % 2 == 1 ? -1.0f : 1.0f);
	const FVector ShotTarget = LockedTargetLocation
		+ Right * CenteredShotIndex * LegacyAimedLateralSpacing;
	SpawnSharedBarrageProjectile(
		(ShotTarget - SpawnLocation).GetSafeNormal(),
		LegacyAimedProjectileSpeed,
		EBossProjectileMotionMode::Straight);
}

void UBossEncounterComponent::SpawnLegacyDenseFanProjectile(
	const int32 ShotIndex)
{
	const FVector SpawnLocation = GetProjectileOriginLocation();
	const FVector CenterDirection =
		(LockedTargetLocation - SpawnLocation).GetSafeNormal();
	if (CenterDirection.IsNearlyZero())
	{
		return;
	}

	const int32 ProjectileCount = FMath::Max(
		LegacyDenseFanProjectileCount,
		3);
	const int32 FirstWaveCount = (ProjectileCount + 1) / 2;
	const bool bFirstWave = ShotIndex < FirstWaveCount;
	const int32 WaveIndex = bFirstWave
		? ShotIndex
		: ShotIndex - FirstWaveCount;
	const int32 WaveCount = bFirstWave
		? FirstWaveCount
		: ProjectileCount - FirstWaveCount;
	const float Alpha = bFirstWave
		? (WaveCount > 1
			? static_cast<float>(WaveIndex)
				/ static_cast<float>(WaveCount - 1)
			: 0.5f)
		: (static_cast<float>(WaveIndex) + 0.5f)
			/ static_cast<float>(FMath::Max(WaveCount, 1));
	const float YawOffset = FMath::Lerp(
		-LegacyDenseFanArcDegrees * 0.5f,
		LegacyDenseFanArcDegrees * 0.5f,
		Alpha);
	SpawnSharedBarrageProjectile(
		CenterDirection.RotateAngleAxis(
			YawOffset,
			FVector::UpVector).GetSafeNormal(),
		LegacyDenseFanProjectileSpeed,
		EBossProjectileMotionMode::Straight);
}

void UBossEncounterComponent::SelectPhase2RoundAttack()
{
	if (CompletedAttacksSinceExposure == 0)
	{
		ResetPhase2Round();
	}

	// Selection is deliberately hierarchical. The whole barrage family owns one
	// category weight; its five patterns do not each compete with Shockwave and
	// Laser. Exact attacks are then removed until the current three-attack round
	// ends, so Shockwave/Laser cannot repeat and barrage patterns cannot repeat.
	const EBossBarragePattern Phase2Patterns[] = {
		EBossBarragePattern::CurvedVolley,
		EBossBarragePattern::HomingVolley,
		EBossBarragePattern::RotatingGapWall,
		EBossBarragePattern::DoubleSpiral,
		EBossBarragePattern::LegacyDenseFan};
	TArray<EBossBarragePattern> AvailableBarragePatterns;
	if (FanProjectileClass)
	{
		for (const EBossBarragePattern Pattern : Phase2Patterns)
		{
			const int32 PatternKey = 100 + static_cast<int32>(Pattern);
			if (!Phase2UsedAttackKeys.Contains(PatternKey))
			{
				AvailableBarragePatterns.Add(Pattern);
			}
		}
	}

	const bool bBarrageAvailable = !AvailableBarragePatterns.IsEmpty();
	const bool bShockwaveAvailable = !Phase2UsedAttackKeys.Contains(0);
	const bool bLaserAvailable = SweepLaserClass
		&& !Phase2UsedAttackKeys.Contains(1);
	if (!bBarrageAvailable && !bShockwaveAvailable && !bLaserAvailable)
	{
		// A designer may request a round longer than the currently configured
		// pool. Start a fresh pool instead of producing an invalid selection.
		ResetPhase2Round();
		SelectPhase2RoundAttack();
		return;
	}
	float BarrageWeight = bBarrageAvailable
		? FMath::Max(Phase2BarrageWeight, 0.0f)
		: 0.0f;
	float ShockwaveWeight = bShockwaveAvailable
		? FMath::Max(Phase2ShockwaveWeight, 0.0f)
		: 0.0f;
	float LaserWeight = bLaserAvailable
		? FMath::Max(Phase2LaserWeight, 0.0f)
		: 0.0f;
	float TotalWeight = BarrageWeight + ShockwaveWeight + LaserWeight;
	if (TotalWeight <= UE_KINDA_SMALL_NUMBER)
	{
		// All-zero editor settings remain playable: choose uniformly between the
		// categories that still contain an unused attack.
		BarrageWeight = bBarrageAvailable ? 1.0f : 0.0f;
		ShockwaveWeight = bShockwaveAvailable ? 1.0f : 0.0f;
		LaserWeight = bLaserAvailable ? 1.0f : 0.0f;
		TotalWeight = BarrageWeight + ShockwaveWeight + LaserWeight;
	}

	int32 SelectedKey = 0;
	FString SelectedCategory = TEXT("Shockwave");
	float Roll = AttackRandomStream.FRandRange(0.0f, TotalWeight);
	if (bBarrageAvailable
		&& (Roll < BarrageWeight
			|| (!bShockwaveAvailable && !bLaserAvailable)))
	{
		const EBossBarragePattern SelectedPattern = AvailableBarragePatterns[
			AttackRandomStream.RandRange(0, AvailableBarragePatterns.Num() - 1)];
		SelectedKey = 100 + static_cast<int32>(SelectedPattern);
		SelectedCategory = TEXT("Barrage");
		SetBarragePattern(SelectedPattern);
	}
	else
	{
		Roll -= BarrageWeight;
		if (bShockwaveAvailable
			&& (Roll < ShockwaveWeight || !bLaserAvailable))
		{
			SelectedKey = 0;
			SelectedCategory = TEXT("Shockwave");
			CurrentAttack = EBossAttackType::Shockwave;
		}
		else if (bLaserAvailable)
		{
			SelectedKey = 1;
			SelectedCategory = TEXT("Laser");
			CurrentAttack = EBossAttackType::SweepLaser;
		}
		else
		{
			// Numerical endpoint fallback; the barrage category is the only one
			// that can reach this branch.
			const EBossBarragePattern SelectedPattern =
				AvailableBarragePatterns.Last();
			SelectedKey = 100 + static_cast<int32>(SelectedPattern);
			SelectedCategory = TEXT("Barrage");
			SetBarragePattern(SelectedPattern);
		}
	}
	Phase2UsedAttackKeys.Add(SelectedKey);
	bCurrentPhase2DoubleShockwave = SelectedKey == 0;

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss Phase 2 round attack selected. Boss=%s Step=%d/%d Category=%s Key=%d Attack=%d Pattern=%d AvailableWeights=(Barrage=%.2f Shockwave=%.2f Laser=%.2f) Used=%d Target=%s Seed=%d"),
		*GetNameSafe(GetOwner()),
		CompletedAttacksSinceExposure + 1,
		FMath::Max(Phase2AttacksBeforeStun, 1),
		*SelectedCategory,
		SelectedKey,
		static_cast<int32>(CurrentAttack),
		static_cast<int32>(ActiveBarragePattern),
		BarrageWeight,
		ShockwaveWeight,
		LaserWeight,
		Phase2UsedAttackKeys.Num(),
		*LockedTargetLocation.ToCompactString(),
		ActiveAttackRandomSeed);
	SetEncounterState(EBossEncounterState::Preparing);
}

void UBossEncounterComponent::ResetPhase2Round()
{
	Phase2UsedAttackKeys.Reset();
	bCurrentPhase2DoubleShockwave = false;
}

void UBossEncounterComponent::TickShockwaveAttack()
{
	const float PulseDuration = GetShockwaveExpansionDuration();
	if (!IsEnhancedDoubleShockwaveAttack())
	{
		UpdateShockwave(FMath::Clamp(StateElapsed / PulseDuration, 0.0f, 1.0f));
		return;
	}

	const float PulseDelay = FMath::Max(Phase2ShockwavePulseDelay, 0.0f);
	if (StateElapsed <= PulseDuration)
	{
		UpdateShockwave(FMath::Clamp(StateElapsed / PulseDuration, 0.0f, 1.0f));
	}
	else
	{
		SetShockwaveCollisionEnabled(false);
		SetShockwaveProceduralVisualVisible(false);
		if (ShockwaveVisual.IsValid())
		{
			ShockwaveVisual->SetVisibility(false);
		}
	}

	if (StateElapsed >= PulseDelay)
	{
		if (ActiveShockwavePulse < 2)
		{
			BeginSecondShockwavePulse();
		}
		const float SecondPulseElapsed = StateElapsed - PulseDelay;
		UpdateSecondaryShockwave(FMath::Clamp(
			SecondPulseElapsed / PulseDuration, 0.0f, 1.0f));
	}
}

void UBossEncounterComponent::BeginSecondShockwavePulse()
{
	ActiveShockwavePulse = 2;
	bPlayerDamagedBySecondaryShockwave = false;
	const float InitialRadius = FMath::Min(
		ShockwaveInitialRadius,
		ShockwaveExpandedMaximumRadius);

	if (SecondaryShockwaveProceduralVisual)
	{
		SecondaryShockwaveProceduralVisual->SetWorldLocation(
			GetShockwaveOriginLocation());
		SecondaryShockwaveProceduralVisual->SetVisibility(true);
		SecondaryShockwaveProceduralVisual->SetHiddenInGame(false);
		if (ShockwaveFireActiveMaterial)
		{
			SecondaryShockwaveProceduralVisual->SetMaterial(
				0, ShockwaveFireActiveMaterial);
		}
	}
	UpdateSecondaryShockwave(0.0f);
	SetShockwaveCollisionEnabledFor(
		SecondaryShockwaveCollisionSegments, true);
	RDCAAudio::PlayAtLocation(
		this,
		ERDCAAudioCue::ShockwaveRelease,
		GetShockwaveOriginLocation(),
		0.72f,
		1.06f);

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss enhanced shockwave second pulse started. Boss=%s Delay=%.2f"),
		*GetNameSafe(GetOwner()),
		FMath::Max(Phase2ShockwavePulseDelay, 0.0f));
}

void UBossEncounterComponent::UpdateSecondaryShockwave(
	const float NormalizedTime)
{
	const float InitialRadius = FMath::Min(
		ShockwaveInitialRadius,
		ShockwaveExpandedMaximumRadius);
	const float CurrentRadius = FMath::Lerp(
		InitialRadius,
		ShockwaveExpandedMaximumRadius,
		FMath::Clamp(NormalizedTime, 0.0f, 1.0f));
	UpdateShockwaveProceduralVisualComponent(
		SecondaryShockwaveProceduralVisual,
		CurrentRadius,
		bSecondaryShockwaveProceduralMeshBuilt,
		BuiltSecondaryShockwaveVisualRadialSegments,
		BuiltSecondaryShockwaveVisualTubeSegments);
	UpdateShockwaveCollisionSegmentsFor(
		SecondaryShockwaveCollisionSegments,
		CurrentRadius * ShockwaveWorldUnitsPerConfiguredUnit);
}

void UBossEncounterComponent::UpdateShockwave(const float NormalizedTime)
{
	// Reapply the configured level-space anchor every tick. The visual is a
	// component of the Boss actor for lifecycle convenience, but its center must
	// not inherit any airborne movement or animation-driven Boss translation.
	if (ShockwaveVisual.IsValid())
	{
		ShockwaveVisual->SetWorldLocation(GetShockwaveOriginLocation());
	}

	const float InitialRadius = FMath::Min(
		ShockwaveInitialRadius,
		ShockwaveExpandedMaximumRadius);
	const float CurrentRadius = FMath::Lerp(
		InitialRadius,
		ShockwaveExpandedMaximumRadius,
		FMath::Clamp(NormalizedTime, 0.0f, 1.0f));
	SetShockwaveVisualRadius(CurrentRadius);
	UpdateShockwaveProceduralVisual(CurrentRadius);
	const float CurrentGameplayRadius = FMath::Max(
		CurrentRadius * ShockwaveWorldUnitsPerConfiguredUnit,
		1.0f);
	const float HalfwayRadius = (InitialRadius + ShockwaveExpandedMaximumRadius) * 0.5f;
	if (PreviousShockwaveRadius < HalfwayRadius && CurrentRadius >= HalfwayRadius)
	{
		UE_LOG(
			LogRDCAPlayer,
			Warning,
			TEXT("Shockwave halfway. Radius=%.1f Scale=%s"),
			CurrentRadius,
			ShockwaveVisual.IsValid()
				? *ShockwaveVisual->GetRelativeScale3D().ToString()
				: TEXT("Missing"));
	}
	UpdateShockwaveCollisionSegments(CurrentGameplayRadius);
	PreviousShockwaveRadius = CurrentRadius;
}

FVector UBossEncounterComponent::GetShockwaveVisualScaleForRadius(
	const float TargetRadius) const
{
	if (!GetOwner())
	{
		return FVector::OneVector;
	}

	TArray<UStaticMeshComponent*> MeshComponents;
	GetOwner()->GetComponents<UStaticMeshComponent>(MeshComponents);
	for (const UStaticMeshComponent* MeshComponent : MeshComponents)
	{
		if (!MeshComponent
			|| !MeshComponent->GetName().Equals(
				TEXT("ShockwaveVisual"), ESearchCase::IgnoreCase)
			|| !MeshComponent->GetStaticMesh())
		{
			continue;
		}

		const FVector BaseScale = MeshComponent->GetRelativeScale3D();
		const FVector MeshExtent = MeshComponent->GetStaticMesh()->GetBounds().BoxExtent;
		const float BaseRadius = FMath::Max(
			MeshExtent.X * FMath::Abs(BaseScale.X),
			MeshExtent.Y * FMath::Abs(BaseScale.Y));
		const float RadiusScale = FMath::Max(
			FMath::Max(TargetRadius, 1.0f) / FMath::Max(BaseRadius, 1.0f),
			0.01f);
		return FVector(
			BaseScale.X * RadiusScale,
			BaseScale.Y * RadiusScale,
			BaseScale.Z);
	}

	return FVector::OneVector;
}

void UBossEncounterComponent::UpdateShockwaveEditorPreviewScales()
{
	if (!GetOwner())
	{
		return;
	}

	TArray<UStaticMeshComponent*> MeshComponents;
	GetOwner()->GetComponents<UStaticMeshComponent>(MeshComponents);
	const FVector InitialScale =
		GetShockwaveVisualScaleForRadius(ShockwaveInitialRadius);
	for (UStaticMeshComponent* MeshComponent : MeshComponents)
	{
		if (!MeshComponent)
		{
			continue;
		}

		if (MeshComponent->GetName().Equals(
			TEXT("ShockwaveVisual"), ESearchCase::IgnoreCase))
		{
			// Keep the real visual representative of the configured start state
			// in the Blueprint viewport; BeginPlay uses that same scale as its base.
			MeshComponent->SetRelativeScale3D(InitialScale);
			ApplyShockwaveVisualWidth(MeshComponent, ShockwaveInitialRadius);
		}
		else if (MeshComponent->GetName().Equals(
			TEXT("SW_Preview_Initial"), ESearchCase::IgnoreCase))
		{
			MeshComponent->SetRelativeScale3D(InitialScale);
			ApplyShockwaveVisualWidth(MeshComponent, ShockwaveInitialRadius);
		}
		else if (MeshComponent->GetName().Equals(
			TEXT("SW_Preview_Max"), ESearchCase::IgnoreCase))
		{
			MeshComponent->SetRelativeScale3D(
				GetShockwaveVisualScaleForRadius(ShockwaveExpandedMaximumRadius));
			ApplyShockwaveVisualWidth(
				MeshComponent,
				ShockwaveExpandedMaximumRadius);
		}
	}
}

float UBossEncounterComponent::GetShockwaveExpansionDuration() const
{
	return FMath::Max(
		(ShockwaveExpandedMaximumRadius - FMath::Min(
			ShockwaveInitialRadius,
			ShockwaveExpandedMaximumRadius))
			/ FMath::Max(ShockwaveExpansionSpeed, 1.0f),
		0.01f);
}

void UBossEncounterComponent::SetShockwaveVisualRadius(const float Radius)
{
	if (!ShockwaveVisual.IsValid())
	{
		return;
	}

	const float RadiusScale = FMath::Max(
		Radius / FMath::Max(ShockwaveBaseWorldRadius, 1.0f),
		0.01f);
	ShockwaveVisual->SetRelativeScale3D(
		FVector(
			ShockwaveBaseScale.X * RadiusScale,
			ShockwaveBaseScale.Y * RadiusScale,
			ShockwaveBaseScale.Z));
	ApplyShockwaveVisualWidth(ShockwaveVisual.Get(), Radius);
}

void UBossEncounterComponent::ApplyShockwaveVisualWidth(
	UStaticMeshComponent* Mesh,
	const float Radius)
{
	if (!Mesh || ShockwaveVisualWidthParameter.IsNone())
	{
		return;
	}

	UMaterialInstanceDynamic* DynamicMaterial =
		Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
	if (!DynamicMaterial)
	{
		DynamicMaterial = Mesh->CreateDynamicMaterialInstance(0);
	}
	if (!DynamicMaterial)
	{
		return;
	}

	// The material draws a unit-radius disc, so convert the editable world-unit
	// width to its normalized radial width at the current ring radius.
	DynamicMaterial->SetScalarParameterValue(
		ShockwaveVisualWidthParameter,
		FMath::Clamp(
			ShockwaveVisualWidth / FMath::Max(Radius, 1.0f),
			0.001f,
			0.95f));
}

void UBossEncounterComponent::CreateShockwaveProceduralVisual()
{
	if (ShockwaveProceduralVisual || !GetOwner()
		|| !GetOwner()->GetRootComponent())
	{
		return;
	}

	ShockwaveProceduralVisual = NewObject<UProceduralMeshComponent>(
		GetOwner(), TEXT("ShockwaveProceduralVisual"));
	if (!ShockwaveProceduralVisual)
	{
		return;
	}

	ShockwaveProceduralVisual->SetupAttachment(GetOwner()->GetRootComponent());
	ShockwaveProceduralVisual->SetMobility(EComponentMobility::Movable);
	ShockwaveProceduralVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShockwaveProceduralVisual->SetGenerateOverlapEvents(false);
	ShockwaveProceduralVisual->SetCastShadow(false);
	ShockwaveProceduralVisual->SetVisibility(false);
	ShockwaveProceduralVisual->SetHiddenInGame(true);
	ShockwaveProceduralVisual->RegisterComponent();
	// The Boss actor is deliberately scaled. Keep this generated world-space
	// ring at scale 1 so its radius/width/height match the collision values.
	ShockwaveProceduralVisual->SetWorldScale3D(FVector::OneVector);

	SecondaryShockwaveProceduralVisual = NewObject<UProceduralMeshComponent>(
		GetOwner(), TEXT("SecondaryShockwaveProceduralVisual"));
	if (SecondaryShockwaveProceduralVisual)
	{
		SecondaryShockwaveProceduralVisual->SetupAttachment(
			GetOwner()->GetRootComponent());
		SecondaryShockwaveProceduralVisual->SetMobility(
			EComponentMobility::Movable);
		SecondaryShockwaveProceduralVisual->SetCollisionEnabled(
			ECollisionEnabled::NoCollision);
		SecondaryShockwaveProceduralVisual->SetGenerateOverlapEvents(false);
		SecondaryShockwaveProceduralVisual->SetCastShadow(false);
		SecondaryShockwaveProceduralVisual->SetVisibility(false);
		SecondaryShockwaveProceduralVisual->SetHiddenInGame(true);
		SecondaryShockwaveProceduralVisual->RegisterComponent();
		SecondaryShockwaveProceduralVisual->SetWorldScale3D(FVector::OneVector);
	}
}

void UBossEncounterComponent::UpdateShockwaveProceduralVisual(
	const float ConfiguredRadius)
{
	UpdateShockwaveProceduralVisualComponent(
		ShockwaveProceduralVisual,
		ConfiguredRadius,
		bShockwaveProceduralMeshBuilt,
		BuiltShockwaveVisualRadialSegments,
		BuiltShockwaveVisualTubeSegments);
}

void UBossEncounterComponent::UpdateShockwaveProceduralVisualComponent(
	UProceduralMeshComponent* Visual,
	const float ConfiguredRadius,
	bool& bMeshBuilt,
	int32& BuiltRadialSegments,
	int32& BuiltTubeSegments)
{
	if (!Visual)
	{
		return;
	}

	const int32 RadialSegments = FMath::Clamp(
		ShockwaveVisualRadialSegments, 12, 64);
	const int32 TubeSegments = FMath::Clamp(
		ShockwaveVisualTubeSegments, 4, 16);
	if (bMeshBuilt
		&& (BuiltRadialSegments != RadialSegments
			|| BuiltTubeSegments != TubeSegments))
	{
		Visual->ClearAllMeshSections();
		bMeshBuilt = false;
	}

	const float WorldUnitScale = FMath::Max(
		ShockwaveWorldUnitsPerConfiguredUnit,
		0.001f);
	const float RingRadius = FMath::Max(
		ConfiguredRadius * WorldUnitScale,
		1.0f);
	const float HorizontalTubeRadius = FMath::Max(
		ShockwaveVisualWidth * WorldUnitScale * 0.5f,
		1.0f);
	const float VerticalTubeRadius = FMath::Max(
		GroundDamageMaximumHeight * WorldUnitScale * 0.5f,
		1.0f);
	const FVector RingCenter = ShockwaveVisual.IsValid()
		? ShockwaveVisual->GetComponentLocation()
		: GetShockwaveOriginLocation();
	Visual->SetWorldLocation(RingCenter);

	const int32 VertexCount = RadialSegments * TubeSegments;
	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;
	Vertices.Reserve(VertexCount);
	Normals.Reserve(VertexCount);
	UVs.Reserve(VertexCount);
	VertexColors.Reserve(VertexCount);
	Tangents.Reserve(VertexCount);

	for (int32 RadialIndex = 0; RadialIndex < RadialSegments; ++RadialIndex)
	{
		const float Theta = 2.0f * PI * RadialIndex / RadialSegments;
		const FVector RadialDirection(
			FMath::Cos(Theta), FMath::Sin(Theta), 0.0f);
		const FVector AroundRingTangent(
			-FMath::Sin(Theta), FMath::Cos(Theta), 0.0f);

		for (int32 TubeIndex = 0; TubeIndex < TubeSegments; ++TubeIndex)
		{
			const float Phi = 2.0f * PI * TubeIndex / TubeSegments;
			const float CosPhi = FMath::Cos(Phi);
			const float SinPhi = FMath::Sin(Phi);
			Vertices.Add(
				RadialDirection * (RingRadius + HorizontalTubeRadius * CosPhi)
				+ FVector::UpVector * (VerticalTubeRadius * SinPhi));
			Normals.Add((
				RadialDirection * (CosPhi / HorizontalTubeRadius)
				+ FVector::UpVector * (SinPhi / VerticalTubeRadius)).GetSafeNormal());
			UVs.Add(FVector2D(
				static_cast<float>(RadialIndex) / RadialSegments,
				static_cast<float>(TubeIndex) / TubeSegments));
			VertexColors.Add(FColor::White);
			Tangents.Add(FProcMeshTangent(AroundRingTangent, false));
		}
	}

	if (!bMeshBuilt)
	{
		TArray<int32> Triangles;
		Triangles.Reserve(RadialSegments * TubeSegments * 6);
		for (int32 RadialIndex = 0; RadialIndex < RadialSegments; ++RadialIndex)
		{
			const int32 NextRadialIndex = (RadialIndex + 1) % RadialSegments;
			for (int32 TubeIndex = 0; TubeIndex < TubeSegments; ++TubeIndex)
			{
				const int32 NextTubeIndex = (TubeIndex + 1) % TubeSegments;
				const int32 A = RadialIndex * TubeSegments + TubeIndex;
				const int32 B = NextRadialIndex * TubeSegments + TubeIndex;
				const int32 C = NextRadialIndex * TubeSegments + NextTubeIndex;
				const int32 D = RadialIndex * TubeSegments + NextTubeIndex;
				// ProceduralMeshComponent expects UE's clockwise front-face winding.
				// Keeping the generated surface outward-facing is important because
				// the fire material deliberately culls the inward half of the ring.
				Triangles.Add(A);
				Triangles.Add(C);
				Triangles.Add(B);
				Triangles.Add(A);
				Triangles.Add(D);
				Triangles.Add(C);
			}
		}
		Visual->CreateMeshSection(
			0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, false);
		bMeshBuilt = true;
		BuiltRadialSegments = RadialSegments;
		BuiltTubeSegments = TubeSegments;
	}
	else
	{
		Visual->UpdateMeshSection(
			0, Vertices, Normals, UVs, VertexColors, Tangents);
	}
}

void UBossEncounterComponent::SetShockwaveProceduralVisualVisible(
	const bool bVisible)
{
	if (ShockwaveProceduralVisual)
	{
		ShockwaveProceduralVisual->SetVisibility(bVisible);
		ShockwaveProceduralVisual->SetHiddenInGame(!bVisible);
	}
}

void UBossEncounterComponent::SetShockwaveProceduralVisualMaterial(
	UMaterialInterface* Material)
{
	if (ShockwaveProceduralVisual && Material)
	{
		ShockwaveProceduralVisual->SetMaterial(0, Material);
	}
}

void UBossEncounterComponent::CreateShockwaveCollisionSegments()
{
	CreateShockwaveCollisionSegmentsFor(
		ShockwaveCollisionSegments, TEXT("ShockwaveCollisionSegment"));
	CreateShockwaveCollisionSegmentsFor(
		SecondaryShockwaveCollisionSegments,
		TEXT("SecondaryShockwaveCollisionSegment"));
}

void UBossEncounterComponent::CreateShockwaveCollisionSegmentsFor(
	TArray<TObjectPtr<UBoxComponent>>& Segments,
	const TCHAR* NamePrefix)
{
	if (!GetOwner() || !GetOwner()->GetRootComponent()
		|| !Segments.IsEmpty())
	{
		return;
	}

	const int32 SegmentCount = FMath::Clamp(ShockwaveCollisionSegmentCount, 8, 48);
	Segments.Reserve(SegmentCount);
	for (int32 Index = 0; Index < SegmentCount; ++Index)
	{
		const FName SegmentName(*FString::Printf(
			TEXT("%s_%02d"), NamePrefix, Index));
		UBoxComponent* Segment = NewObject<UBoxComponent>(GetOwner(), SegmentName);
		if (!Segment)
		{
			continue;
		}

		Segment->SetupAttachment(GetOwner()->GetRootComponent());
		Segment->SetMobility(EComponentMobility::Movable);
		Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Segment->SetCollisionObjectType(ECC_WorldDynamic);
		Segment->SetCollisionResponseToAllChannels(ECR_Ignore);
		Segment->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Segment->SetGenerateOverlapEvents(true);
		Segment->SetHiddenInGame(true);
		Segment->OnComponentBeginOverlap.AddDynamic(
			this, &UBossEncounterComponent::HandleShockwaveSegmentOverlap);
		Segment->RegisterComponent();
		Segment->SetWorldScale3D(FVector::OneVector);
		Segments.Add(Segment);
	}
}

void UBossEncounterComponent::UpdateShockwaveCollisionSegments(
	const float RingCenterRadius)
{
	UpdateShockwaveCollisionSegmentsFor(
		ShockwaveCollisionSegments, RingCenterRadius);
}

void UBossEncounterComponent::UpdateShockwaveCollisionSegmentsFor(
	TArray<TObjectPtr<UBoxComponent>>& Segments,
	const float RingCenterRadius)
{
	if (!ShockwaveVisual.IsValid())
	{
		return;
	}
	if (Segments.IsEmpty())
	{
		return;
	}

	// RingCenterRadius is generated from the same configured radius as the
	// procedural Torus. Width and height deliberately stay independent of it.
	const float WorldUnitScale = FMath::Max(
		ShockwaveWorldUnitsPerConfiguredUnit,
		0.001f);
	const float GameplayWidth = FMath::Max(
		ShockwaveGameplayWidth * WorldUnitScale,
		1.0f);
	const float CollisionHeight = FMath::Max(
		GroundDamageMaximumHeight * WorldUnitScale,
		1.0f);
	const float RingRadius = FMath::Max(
		RingCenterRadius,
		GameplayWidth * 0.5f);
	const int32 SegmentCount = Segments.Num();
	const float ArcLength = (2.0f * PI * RingRadius) / SegmentCount;
	const FVector BoxExtent(
		GameplayWidth * 0.5f + ShockwaveCollisionSegmentOverlap,
		ArcLength * 0.5f + ShockwaveCollisionSegmentOverlap,
		CollisionHeight * 0.5f);
	const FVector RingCenter = ShockwaveVisual->GetComponentLocation();

	for (int32 Index = 0; Index < SegmentCount; ++Index)
	{
		UBoxComponent* Segment = Segments[Index];
		if (!Segment)
		{
			continue;
		}

		const float AngleDegrees = 360.0f * Index / SegmentCount;
		const FVector RadialDirection = FVector(
			FMath::Cos(FMath::DegreesToRadians(AngleDegrees)),
			FMath::Sin(FMath::DegreesToRadians(AngleDegrees)),
			0.0f);
		Segment->SetBoxExtent(BoxExtent, false);
		Segment->SetWorldLocationAndRotation(
			RingCenter + RadialDirection * RingRadius,
			FRotator(0.0f, AngleDegrees, 0.0f),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		Segment->UpdateOverlaps();
	}
}

void UBossEncounterComponent::SetShockwaveCollisionEnabled(const bool bEnabled)
{
	SetShockwaveCollisionEnabledFor(ShockwaveCollisionSegments, bEnabled);
}

void UBossEncounterComponent::SetShockwaveCollisionEnabledFor(
	TArray<TObjectPtr<UBoxComponent>>& Segments,
	const bool bEnabled)
{
	for (UBoxComponent* Segment : Segments)
	{
		if (Segment)
		{
			Segment->SetCollisionEnabled(
				bEnabled
					? ECollisionEnabled::QueryOnly
					: ECollisionEnabled::NoCollision);
			if (bEnabled)
			{
				// Enabling QueryOnly does not always emit an initial overlap for an
				// already-overlapping Pawn, so explicitly refresh it once here.
				Segment->UpdateOverlaps();
			}
		}
	}
}

void UBossEncounterComponent::HandleShockwaveSegmentOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const int32 OtherBodyIndex,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	const bool bSecondaryPulse = SecondaryShockwaveCollisionSegments.Contains(
		Cast<UBoxComponent>(OverlappedComponent));
	ApplyShockwaveOverlapDamage(OtherActor, bSecondaryPulse);
}

void UBossEncounterComponent::ApplyShockwaveOverlapDamage(
	AActor* OtherActor,
	const bool bSecondaryPulse)
{
	bool& bPulseDamagedPlayer = bSecondaryPulse
		? bPlayerDamagedBySecondaryShockwave
		: bPlayerDamagedThisAttack;
	if (bPulseDamagedPlayer || CurrentAttack != EBossAttackType::Shockwave
		|| EncounterState != EBossEncounterState::Attacking || !GetWorld())
	{
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!PlayerPawn || OtherActor != PlayerPawn)
	{
		return;
	}

	if (const UPhaseCrashComponent* PhaseCrash =
			PlayerPawn->FindComponentByClass<UPhaseCrashComponent>())
	{
		if (PhaseCrash->IsCrashing() || PhaseCrash->IsAttached())
		{
			return;
		}
	}

	if (UPlayerHealthComponent* Health =
		PlayerPawn->FindComponentByClass<UPlayerHealthComponent>())
	{
		bPulseDamagedPlayer = Health->ReceiveDamage(ShockwaveDamage);
		if (bPulseDamagedPlayer)
		{
			if (bSecondaryPulse)
			{
				SetShockwaveCollisionEnabledFor(
					SecondaryShockwaveCollisionSegments, false);
			}
			else
			{
				SetShockwaveCollisionEnabled(false);
			}
			UE_LOG(
				LogRDCAPlayer,
				Log,
				TEXT("Boss shockwave overlap hit. Player=%s Pulse=%d"),
				*GetNameSafe(PlayerPawn),
				bSecondaryPulse ? 2 : 1);
		}
	}
}

void UBossEncounterComponent::UpdateWeakPointVisual(const bool bExposed)
{
	if (!WeakPointVisual.IsValid())
	{
		return;
	}

	UMaterialInterface* Material =
		bExposed ? WeakPointExposedMaterial : WeakPointProtectedMaterial;
	if (Material)
	{
		WeakPointVisual->SetMaterial(0, Material);
	}
}

void UBossEncounterComponent::ResolveBossVisual()
{
	BossVisual.Reset();
	if (!GetOwner())
	{
		return;
	}

	TArray<UMeshComponent*> MeshComponents;
	GetOwner()->GetComponents<UMeshComponent>(MeshComponents);
	for (UMeshComponent* Mesh : MeshComponents)
	{
		if (Mesh && Mesh->GetName().Equals(
			BossVisualComponentName.ToString(), ESearchCase::IgnoreCase))
		{
			BossVisual = Mesh;
			return;
		}
	}

	// Most Boss Blueprints have one skeletal render mesh plus static helper
	// meshes for the weak point and shockwave. Prefer the skeletal mesh when
	// its authored component name differs from the default BossMesh.
	for (UMeshComponent* Mesh : MeshComponents)
	{
		if (Cast<USkeletalMeshComponent>(Mesh))
		{
			BossVisual = Mesh;
			return;
		}
	}
}

void UBossEncounterComponent::UpdateBossPhaseMaterial()
{
	if (bUseSphereMaskedBossPhaseTransition)
	{
		return;
	}

	if (!BossVisual.IsValid())
	{
		ResolveBossVisual();
	}
	if (!BossVisual.IsValid())
	{
		return;
	}

	const EBossCombatPhase DesiredPhase = GetCombatPhase();
	if (bBossPhaseMaterialApplied && DesiredPhase == LastAppliedBossMaterialPhase)
	{
		return;
	}

	UMaterialInterface* Material = DesiredPhase == EBossCombatPhase::Phase2
		? BossPhase2Material.Get()
		: BossPhase1Material.Get();
	if (Material)
	{
		BossVisual->SetMaterial(BossBodyMaterialSlot, Material);
	}

	bBossPhaseMaterialApplied = true;
	LastAppliedBossMaterialPhase = DesiredPhase;
}

void UBossEncounterComponent::CreateSphereMaskedBossPhaseVisual()
{
	if (!bUseSphereMaskedBossPhaseTransition
		|| BossPhase2SphereVisual
		|| !GetOwner())
	{
		return;
	}

	USkeletalMeshComponent* SourceMesh = Cast<USkeletalMeshComponent>(
		BossVisual.Get());
	if (!SourceMesh)
	{
		UE_LOG(
			LogRDCAPlayer,
			Warning,
			TEXT("Sphere-masked Boss phase transition requires a Skeletal Mesh. Resolved=%s"),
			*GetNameSafe(BossVisual.Get()));
		return;
	}
	if (!BossPhase2Material)
	{
		UE_LOG(
			LogRDCAPlayer,
			Warning,
			TEXT("Sphere-masked Boss phase transition enabled without BossPhase2Material."));
		return;
	}

	if (BossPhase1Material)
	{
		SourceMesh->SetMaterial(BossBodyMaterialSlot, BossPhase1Material);
	}

	BossPhase2SphereVisual = NewObject<USkeletalMeshComponent>(
		GetOwner(), TEXT("BossPhase2SphereVisual_Runtime"));
	BossPhase2SphereVisual->SetSkeletalMeshAsset(
		SourceMesh->GetSkeletalMeshAsset());
	for (int32 MaterialIndex = 0;
		MaterialIndex < SourceMesh->GetNumMaterials();
		++MaterialIndex)
	{
		BossPhase2SphereVisual->SetMaterial(
			MaterialIndex, SourceMesh->GetMaterial(MaterialIndex));
	}
	BossPhase2SphereVisual->SetMaterial(
		BossBodyMaterialSlot, BossPhase2Material);
	BossPhase2SphereVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BossPhase2SphereVisual->SetGenerateOverlapEvents(false);
	BossPhase2SphereVisual->SetCastShadow(true);
	GetOwner()->AddInstanceComponent(BossPhase2SphereVisual);
	BossPhase2SphereVisual->RegisterComponent();
	BossPhase2SphereVisual->AttachToComponent(
		SourceMesh, FAttachmentTransformRules::SnapToTargetIncludingScale);
	BossPhase2SphereVisual->SetLeaderPoseComponent(SourceMesh, true);
	BossPhase2SphereVisual->SetVisibility(true, true);
	BossPhase2SphereVisual->SetHiddenInGame(false, true);
}

USceneComponent* UBossEncounterComponent::FindNamedSceneComponent(
	const FName ComponentName) const
{
	if (!GetOwner())
	{
		return nullptr;
	}

	TArray<USceneComponent*> SceneComponents;
	GetOwner()->GetComponents<USceneComponent>(SceneComponents);
	for (USceneComponent* Component : SceneComponents)
	{
		if (Component
			&& Component->GetFName().IsEqual(
				ComponentName,
				ENameCase::IgnoreCase))
		{
			return Component;
		}
	}
	return nullptr;
}

FVector UBossEncounterComponent::GetProjectileOriginLocation() const
{
	return ProjectileOrigin.IsValid()
		? ProjectileOrigin->GetComponentLocation()
		: GetOwner()->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
}

FVector UBossEncounterComponent::GetLaserOriginLocation() const
{
	return LaserOrigin.IsValid()
		? LaserOrigin->GetComponentLocation()
		: GetOwner()->GetActorLocation();
}

FVector UBossEncounterComponent::GetShockwaveOriginLocation() const
{
	if (IsValid(ShockwaveWorldAnchor))
	{
		return ShockwaveWorldAnchor->GetActorLocation();
	}
	if (ShockwaveOrigin.IsValid())
	{
		return ShockwaveOrigin->GetComponentLocation();
	}
	if (ShockwaveVisual.IsValid())
	{
		return ShockwaveVisual->GetComponentLocation();
	}
	return GetOwner()->GetActorLocation();
}
