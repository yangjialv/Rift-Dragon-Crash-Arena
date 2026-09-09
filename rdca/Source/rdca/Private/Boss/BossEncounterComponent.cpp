#include "Boss/BossEncounterComponent.h"

#include "Boss/BossWeakPointComponent.h"
#include "Boss/BossFanProjectile.h"
#include "Boss/BossSweepLaser.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
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
	FanBarrageProjectileClass = ABossFanProjectile::StaticClass();
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

		// Phase two begins at the damage threshold itself, rather than waiting
		// for the next attack-selection pass. This lets the arena transition
		// react on the decisive weak-point hit.
		if (WeakPoint->GetCurrentHitPoints() <= 1)
		{
			return EBossCombatPhase::Phase2;
		}
	}
	return CombatPhase;
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
		return bPhase2ComboActive && Phase2ComboStep == 1
			? Phase2InterAttackDelay
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
	if (ShockwaveOrigin.IsValid() && ShockwaveVisual.IsValid())
	{
		ShockwaveVisual->SetWorldLocation(
			ShockwaveOrigin->GetComponentLocation());
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
			UpdateShockwave(FMath::Clamp(
				StateElapsed / GetShockwaveExpansionDuration(),
				0.0f,
				1.0f));
		}
		else if (CurrentAttack == EBossAttackType::AimedVolley)
		{
			TickAimedVolley(DeltaTime);
		}
		else if (CurrentAttack == EBossAttackType::FanBarrage)
		{
			TickFanBarrage(DeltaTime);
		}
		if (StateElapsed >= GetCurrentAttackActiveDuration())
		{
			SetEncounterState(EBossEncounterState::Recovery);
		}
		break;
	case EBossEncounterState::Recovery:
		if (StateElapsed >= GetCurrentStateDuration())
		{
			if (bPhase2ComboActive && Phase2ComboStep == 1)
			{
				BeginPhase2SecondAttack();
			}
			else if (bPhase2ComboActive && Phase2ComboStep == 2)
			{
				bPhase2ComboActive = false;
				Phase2ComboStep = 0;
				SetEncounterState(EBossEncounterState::WeakPointExposed);
			}
			else
			{
				++CompletedAttacksSinceExposure;
				if (CompletedAttacksSinceExposure
				>= FMath::Max(AttacksBeforeWeakPointExposure, 1))
				{
					SetEncounterState(EBossEncounterState::WeakPointExposed);
				}
				else
				{
					SetEncounterState(EBossEncounterState::SelectingAttack);
				}
			}
		}
		break;
	case EBossEncounterState::WeakPointExposed:
		if (StateElapsed >= GetCurrentStateDuration())
		{
			CompletedAttacksSinceExposure = 0;
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
	else if (NewState == EBossEncounterState::Dead)
	{
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

void UBossEncounterComponent::SelectNextAttack()
{
	if (WeakPoint.IsValid())
	{
		CombatPhase = WeakPoint->GetCurrentHitPoints() <= 1
			? EBossCombatPhase::Phase2
			: EBossCombatPhase::Phase1;
	}
	LastObservedPlayerState = ObservePlayerSpatialState();
	if (const APawn* PlayerPawn =
			UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		LockedTargetLocation = PlayerPawn->GetActorLocation();
	}
	if (GetCombatPhase() == EBossCombatPhase::Phase2)
	{
		SelectPhase2Combo();
		return;
	}

	const FBossAttackWeights& Weights =
		GetWeightsForPlayerState(LastObservedPlayerState);
	CurrentAttack = ChooseWeightedAttack(Weights);
	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss attack selected. Boss=%s Phase=%d PlayerState=%d Weights=(Shockwave=%.1f AimedVolley=%.1f Laser=%.1f) Previous=%d Selected=%d Seed=%d"),
		*GetNameSafe(GetOwner()),
		static_cast<int32>(GetCombatPhase()),
		static_cast<int32>(LastObservedPlayerState),
		Weights.Shockwave,
		Weights.AimedVolley,
		Weights.SweepLaser,
		static_cast<int32>(PreviousAttack),
		static_cast<int32>(CurrentAttack),
		ActiveAttackRandomSeed);
	SetEncounterState(EBossEncounterState::Preparing);
}

EBossAttackType UBossEncounterComponent::ChooseWeightedAttack(
	const FBossAttackWeights& Weights)
{
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
		return LaserAimWarningDuration;
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
		return FMath::Max(
			AimedVolleyAttackDuration,
			(PrecisionVolleyProjectileCount - 1)
				* PrecisionVolleyShotInterval + 0.05f);
	case EBossAttackType::FanBarrage:
		return FMath::Max(
			FanBarrageAttackDuration,
			(DenseFanProjectileCount - 1) * DenseFanShotInterval + 0.05f);
	case EBossAttackType::SweepLaser:
		return LaserActiveSweepDuration;
	case EBossAttackType::Shockwave:
	default:
		return GetShockwaveExpansionDuration();
	}
}

void UBossEncounterComponent::BeginCurrentAttackWarning()
{
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
			// circle from the displayed ring.
			if (ShockwaveOrigin.IsValid())
			{
				ShockwaveVisual->SetWorldLocation(
					ShockwaveOrigin->GetComponentLocation());
			}
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
		if (const APawn* PlayerPawn =
				UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
		{
			LaserWarningInitialPlayerLocation =
				PlayerPawn->GetActorLocation();
		}
		bLaserAimLocked = false;
		SpawnLaserWarning();
	}
}

void UBossEncounterComponent::BeginCurrentAttack()
{
	switch (CurrentAttack)
	{
	case EBossAttackType::Shockwave:
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
		AimedVolleyShotsFired = 0;
		AimedVolleyShotElapsed = 0.0f;
		SpawnAimedVolleyProjectile(AimedVolleyShotsFired++);
		break;
	case EBossAttackType::FanBarrage:
		FanBarrageShotsFired = 0;
		FanBarrageShotElapsed = 0.0f;
		SpawnFanBarrageProjectile(FanBarrageShotsFired++);
		break;
	case EBossAttackType::SweepLaser:
		if (ActiveSweepLaser.IsValid())
		{
			if (!bLaserAimLocked)
			{
				LockLaserSweep();
			}
			ActiveSweepLaser->ActivateLaser();
		}
		break;
	default:
		break;
	}
}

void UBossEncounterComponent::FinishCurrentAttack()
{
	SetShockwaveCollisionEnabled(false);
	SetShockwaveProceduralVisualVisible(false);
	if (ShockwaveVisual.IsValid())
	{
		ShockwaveVisual->SetVisibility(false);
	}
	if (ActiveSweepLaser.IsValid())
	{
		ActiveSweepLaser->Destroy();
		ActiveSweepLaser.Reset();
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

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!PlayerPawn)
	{
		return;
	}

	const float LockStartTime = FMath::Max(
		LaserAimWarningDuration
			- FMath::Clamp(
				LaserAimLockDuration,
				0.05f,
				LaserAimWarningDuration),
		0.0f);
	if (StateElapsed >= LockStartTime)
	{
		LockLaserSweep();
		return;
	}

	const FVector OriginLocation = GetLaserOriginLocation();
	FVector ToPlayer = PlayerPawn->GetActorLocation() - OriginLocation;
	ToPlayer.Z = 0.0f;
	if (ToPlayer.Normalize())
	{
		LaserWarningCurrentYaw = ToPlayer.Rotation().Yaw;
		ActiveSweepLaser->UpdateWarningPose(
			OriginLocation,
			LaserWarningCurrentYaw);
	}
}

void UBossEncounterComponent::LockLaserSweep()
{
	if (!ActiveSweepLaser.IsValid() || bLaserAimLocked)
	{
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	const FVector OriginLocation = GetLaserOriginLocation();
	const FVector CurrentPlayerLocation = PlayerPawn
		? PlayerPawn->GetActorLocation()
		: LockedTargetLocation;
	FVector LockedDirection = CurrentPlayerLocation - OriginLocation;
	LockedDirection.Z = 0.0f;
	if (LockedDirection.Normalize())
	{
		LaserWarningCurrentYaw = LockedDirection.Rotation().Yaw;
	}

	FVector InitialDirection =
		LaserWarningInitialPlayerLocation - OriginLocation;
	InitialDirection.Z = 0.0f;
	InitialDirection.Normalize();
	FVector PlayerMovement =
		CurrentPlayerLocation - LaserWarningInitialPlayerLocation;
	PlayerMovement.Z = 0.0f;
	float MovementSide = FVector::CrossProduct(
		InitialDirection,
		PlayerMovement).Z;
	if (FMath::Abs(MovementSide) <= 5.0f)
	{
		MovementSide = AttackRandomStream.FRand() < 0.5f ? -1.0f : 1.0f;
	}
	const float SweepSign = MovementSide >= 0.0f ? 1.0f : -1.0f;
	const float StartYaw = LaserWarningCurrentYaw - SweepSign * 5.0f;
	const float EndYaw =
		LaserWarningCurrentYaw + SweepSign * LaserActiveSweepDegrees;

	LockedTargetLocation = CurrentPlayerLocation;
	bLaserAimLocked = true;
	ActiveSweepLaser->UpdateWarningPose(OriginLocation, StartYaw);
	ActiveSweepLaser->ConfigureSweep(StartYaw, EndYaw);

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss laser aim locked. Boss=%s StartYaw=%.1f EndYaw=%.1f PlayerMovement=%s LockLead=%.2f"),
		*GetNameSafe(GetOwner()),
		StartYaw,
		EndYaw,
		*PlayerMovement.ToCompactString(),
		LaserAimLockDuration);
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
	FVector ToPlayer = LockedTargetLocation - SpawnLocation;
	ToPlayer.Z = 0.0f;
	const float CenterYaw = ToPlayer.Rotation().Yaw;
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

void UBossEncounterComponent::TickAimedVolley(const float DeltaTime)
{
	if (AimedVolleyShotsFired >= FMath::Max(PrecisionVolleyProjectileCount, 1))
	{
		return;
	}

	AimedVolleyShotElapsed += DeltaTime;
	const float Interval = FMath::Max(PrecisionVolleyShotInterval, 0.01f);
	while (AimedVolleyShotElapsed >= Interval
		&& AimedVolleyShotsFired < FMath::Max(PrecisionVolleyProjectileCount, 1))
	{
		AimedVolleyShotElapsed -= Interval;
		SpawnAimedVolleyProjectile(AimedVolleyShotsFired++);
	}
}

void UBossEncounterComponent::SpawnAimedVolleyProjectile(
	const int32 ShotIndex)
{
	if (!GetWorld() || !FanProjectileClass)
	{
		return;
	}

	const FVector SpawnLocation = GetProjectileOriginLocation();
	FVector ForwardToTarget = LockedTargetLocation - SpawnLocation;
	if (!ForwardToTarget.Normalize())
	{
		return;
	}

	FVector LateralDirection = FVector::CrossProduct(
		FVector::UpVector,
		ForwardToTarget).GetSafeNormal();
	if (LateralDirection.IsNearlyZero())
	{
		LateralDirection = FVector::RightVector;
	}
	const int32 OffsetStep = (ShotIndex + 1) / 2;
	const float CenteredShotIndex = ShotIndex == 0
		? 0.0f
		: static_cast<float>(OffsetStep)
			* (ShotIndex % 2 == 1 ? -1.0f : 1.0f);
	const FVector ShotTarget =
		LockedTargetLocation
		+ LateralDirection * CenteredShotIndex * PrecisionVolleyLateralSpacing;
	const FVector Direction = (ShotTarget - SpawnLocation).GetSafeNormal();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABossFanProjectile* Projectile =
		GetWorld()->SpawnActor<ABossFanProjectile>(
			FanProjectileClass,
			SpawnLocation,
			Direction.Rotation(),
			SpawnParameters);
	if (Projectile)
	{
		Projectile->InitializeGroundSkimmingProjectile(
			Direction,
			AimedVolleyProjectileSpeed,
			AimedVolleyProjectileDamage,
			LockedTargetLocation.Z);
	}

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss aimed volley shot. Boss=%s Shot=%d/%d Target=%s Speed=%.1f"),
		*GetNameSafe(GetOwner()),
		ShotIndex + 1,
		FMath::Max(PrecisionVolleyProjectileCount, 1),
		*ShotTarget.ToCompactString(),
		AimedVolleyProjectileSpeed);
}

void UBossEncounterComponent::SelectPhase2Combo()
{
	bPhase2ComboActive = true;
	Phase2ComboStep = 1;
	CompletedAttacksSinceExposure = 0;

	// Spatial state chooses the first Phase 2 lesson. Later rounds alternate so
	// one common player state cannot permanently hide half of the phase.
	if (bHasSelectedPhase2Combo)
	{
		bPhase2AnchorPressureCombo =
			!bPreviousPhase2AnchorPressureCombo;
	}
	else if (LastObservedPlayerState == EPlayerSpatialState::Attached)
	{
		bPhase2AnchorPressureCombo = true;
	}
	else if (LastObservedPlayerState == EPlayerSpatialState::Airborne)
	{
		bPhase2AnchorPressureCombo = AttackRandomStream.FRand() >= 0.5f;
	}
	else
	{
		bPhase2AnchorPressureCombo = false;
	}
	bHasSelectedPhase2Combo = true;
	bPreviousPhase2AnchorPressureCombo = bPhase2AnchorPressureCombo;

	CurrentAttack = bPhase2AnchorPressureCombo
		? EBossAttackType::FanBarrage
		: EBossAttackType::Shockwave;

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss Phase 2 combo selected. Boss=%s Combo=%s PlayerState=%d First=%d Second=%d Target=%s Seed=%d"),
		*GetNameSafe(GetOwner()),
		bPhase2AnchorPressureCombo
			? TEXT("AnchorPressure")
			: TEXT("GroundPressure"),
		static_cast<int32>(LastObservedPlayerState),
		static_cast<int32>(CurrentAttack),
		static_cast<int32>(
			bPhase2AnchorPressureCombo
				? EBossAttackType::SweepLaser
				: EBossAttackType::AimedVolley),
		*LockedTargetLocation.ToCompactString(),
		ActiveAttackRandomSeed);
	SetEncounterState(EBossEncounterState::Preparing);
}

void UBossEncounterComponent::BeginPhase2SecondAttack()
{
	Phase2ComboStep = 2;
	CurrentAttack = bPhase2AnchorPressureCombo
		? EBossAttackType::SweepLaser
		: EBossAttackType::AimedVolley;

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss Phase 2 combo advancing. Boss=%s Combo=%s Attack=%d LockedTarget=%s"),
		*GetNameSafe(GetOwner()),
		bPhase2AnchorPressureCombo
			? TEXT("AnchorPressure")
			: TEXT("GroundPressure"),
		static_cast<int32>(CurrentAttack),
		*LockedTargetLocation.ToCompactString());
	SetEncounterState(EBossEncounterState::Preparing);
}

void UBossEncounterComponent::TickFanBarrage(const float DeltaTime)
{
	if (FanBarrageShotsFired >= FMath::Max(DenseFanProjectileCount, 3))
	{
		return;
	}

	FanBarrageShotElapsed += DeltaTime;
	const float Interval = FMath::Max(DenseFanShotInterval, 0.01f);
	while (FanBarrageShotElapsed >= Interval
		&& FanBarrageShotsFired < FMath::Max(DenseFanProjectileCount, 3))
	{
		FanBarrageShotElapsed -= Interval;
		SpawnFanBarrageProjectile(FanBarrageShotsFired++);
	}
}

void UBossEncounterComponent::SpawnFanBarrageProjectile(
	const int32 ShotIndex)
{
	const TSubclassOf<ABossFanProjectile> ProjectileClass =
		FanBarrageProjectileClass
			? FanBarrageProjectileClass
			: FanProjectileClass;
	if (!GetWorld() || !ProjectileClass)
	{
		return;
	}

	const FVector SpawnLocation = GetProjectileOriginLocation();
	const FVector CenterDirection =
		(LockedTargetLocation - SpawnLocation).GetSafeNormal();
	if (CenterDirection.IsNearlyZero())
	{
		return;
	}

	const int32 ProjectileCount = FMath::Max(DenseFanProjectileCount, 3);
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
			? static_cast<float>(WaveIndex) / static_cast<float>(WaveCount - 1)
			: 0.5f)
		: (static_cast<float>(WaveIndex) + 0.5f)
			/ static_cast<float>(FMath::Max(WaveCount, 1));
	const float YawOffset = FMath::Lerp(
		-DenseFanArcDegrees * 0.5f,
		DenseFanArcDegrees * 0.5f,
		Alpha);
	const FVector Direction = CenterDirection.RotateAngleAxis(
		YawOffset,
		FVector::UpVector).GetSafeNormal();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABossFanProjectile* Projectile =
		GetWorld()->SpawnActor<ABossFanProjectile>(
			ProjectileClass,
			SpawnLocation,
			Direction.Rotation(),
			SpawnParameters);
	if (Projectile)
	{
		Projectile->InitializeGroundSkimmingProjectile(
			Direction,
			DenseFanProjectileSpeed,
			FanBarrageProjectileDamage,
			LockedTargetLocation.Z);
	}

	UE_LOG(
		LogRDCAPlayer,
		Verbose,
		TEXT("Boss fan barrage shot. Boss=%s Shot=%d/%d YawOffset=%.1f Speed=%.1f"),
		*GetNameSafe(GetOwner()),
		ShotIndex + 1,
		ProjectileCount,
		YawOffset,
		DenseFanProjectileSpeed);
}

void UBossEncounterComponent::UpdateShockwave(const float NormalizedTime)
{
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
}

void UBossEncounterComponent::UpdateShockwaveProceduralVisual(
	const float ConfiguredRadius)
{
	if (!ShockwaveProceduralVisual)
	{
		return;
	}

	const int32 RadialSegments = FMath::Clamp(
		ShockwaveVisualRadialSegments, 12, 64);
	const int32 TubeSegments = FMath::Clamp(
		ShockwaveVisualTubeSegments, 4, 16);
	if (bShockwaveProceduralMeshBuilt
		&& (BuiltShockwaveVisualRadialSegments != RadialSegments
			|| BuiltShockwaveVisualTubeSegments != TubeSegments))
	{
		ShockwaveProceduralVisual->ClearAllMeshSections();
		bShockwaveProceduralMeshBuilt = false;
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
	ShockwaveProceduralVisual->SetWorldLocation(RingCenter);

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

	if (!bShockwaveProceduralMeshBuilt)
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
		ShockwaveProceduralVisual->CreateMeshSection(
			0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, false);
		bShockwaveProceduralMeshBuilt = true;
		BuiltShockwaveVisualRadialSegments = RadialSegments;
		BuiltShockwaveVisualTubeSegments = TubeSegments;
	}
	else
	{
		ShockwaveProceduralVisual->UpdateMeshSection(
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
	if (!GetOwner() || !GetOwner()->GetRootComponent()
		|| !ShockwaveCollisionSegments.IsEmpty())
	{
		return;
	}

	const int32 SegmentCount = FMath::Clamp(ShockwaveCollisionSegmentCount, 8, 48);
	ShockwaveCollisionSegments.Reserve(SegmentCount);
	for (int32 Index = 0; Index < SegmentCount; ++Index)
	{
		const FName SegmentName(*FString::Printf(
			TEXT("ShockwaveCollisionSegment_%02d"), Index));
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
		ShockwaveCollisionSegments.Add(Segment);
	}
}

void UBossEncounterComponent::UpdateShockwaveCollisionSegments(
	const float RingCenterRadius)
{
	if (!ShockwaveVisual.IsValid())
	{
		return;
	}
	if (ShockwaveCollisionSegments.IsEmpty())
	{
		CreateShockwaveCollisionSegments();
	}
	if (ShockwaveCollisionSegments.IsEmpty())
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
	const int32 SegmentCount = ShockwaveCollisionSegments.Num();
	const float ArcLength = (2.0f * PI * RingRadius) / SegmentCount;
	const FVector BoxExtent(
		GameplayWidth * 0.5f + ShockwaveCollisionSegmentOverlap,
		ArcLength * 0.5f + ShockwaveCollisionSegmentOverlap,
		CollisionHeight * 0.5f);
	const FVector RingCenter = ShockwaveVisual->GetComponentLocation();

	for (int32 Index = 0; Index < SegmentCount; ++Index)
	{
		UBoxComponent* Segment = ShockwaveCollisionSegments[Index];
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
	for (UBoxComponent* Segment : ShockwaveCollisionSegments)
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
	ApplyShockwaveOverlapDamage(OtherActor);
}

void UBossEncounterComponent::ApplyShockwaveOverlapDamage(AActor* OtherActor)
{
	if (bPlayerDamagedThisAttack || CurrentAttack != EBossAttackType::Shockwave
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
		bPlayerDamagedThisAttack = Health->ReceiveDamage(ShockwaveDamage);
		if (bPlayerDamagedThisAttack)
		{
			SetShockwaveCollisionEnabled(false);
			UE_LOG(
				LogRDCAPlayer,
				Log,
				TEXT("Boss shockwave overlap hit. Player=%s"),
				*GetNameSafe(PlayerPawn));
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
