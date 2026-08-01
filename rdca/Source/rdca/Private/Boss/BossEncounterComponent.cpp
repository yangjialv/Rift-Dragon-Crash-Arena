#include "Boss/BossEncounterComponent.h"

#include "Boss/BossWeakPointComponent.h"
#include "Boss/BossFanProjectile.h"
#include "Boss/BossSweepLaser.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Player/PhaseCrashComponent.h"
#include "Player/PlayerHealthComponent.h"
#include "rdca.h"

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
	if (!WeakPoint.IsValid() || WeakPoint->IsBossDefeated())
	{
		return WeakPoint.IsValid()
			? EBossCombatPhase::Dead
			: EBossCombatPhase::Phase1;
	}
	return WeakPoint->GetCurrentHitPoints() <= 1
		? EBossCombatPhase::Phase2
		: EBossCombatPhase::Phase1;
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
		return RecoveryDuration;
	case EBossEncounterState::WeakPointExposed:
		return WeakPointExposedDuration;
	case EBossEncounterState::Dead:
	default:
		return 0.0f;
	}
}

void UBossEncounterComponent::BeginPlay()
{
	Super::BeginPlay();

	WeakPoint = GetOwner()->FindComponentByClass<UBossWeakPointComponent>();
	TArray<UStaticMeshComponent*> MeshComponents;
	GetOwner()->GetComponents<UStaticMeshComponent>(MeshComponents);
	for (UStaticMeshComponent* Mesh : MeshComponents)
	{
		if (Mesh->GetName().Equals(TEXT("ShockwaveVisual"), ESearchCase::IgnoreCase))
		{
			ShockwaveVisual = Mesh;
			ShockwaveBaseScale = Mesh->GetRelativeScale3D();
			Mesh->SetVisibility(false);
		}
		else if (Mesh->GetName().Equals(TEXT("WeakPoint"), ESearchCase::IgnoreCase))
		{
			WeakPointVisual = Mesh;
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

	ActiveAttackRandomSeed =
		AttackSelectionRandomSeed >= 0
			? AttackSelectionRandomSeed
			: FMath::Rand();
	AttackRandomStream.Initialize(ActiveAttackRandomSeed);
	SetEncounterState(EBossEncounterState::Idle);
}

void UBossEncounterComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (WeakPoint.IsValid() && WeakPoint->IsBossDefeated())
	{
		SetEncounterState(EBossEncounterState::Dead);
		StopEncounter();
		return;
	}
	if (EncounterState == EBossEncounterState::Dead || bEncounterStopped)
	{
		return;
	}

	StateElapsed += DeltaTime;
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
		if (StateElapsed >= GetCurrentAttackWarningDuration())
		{
			SetEncounterState(EBossEncounterState::Attacking);
		}
		break;
	case EBossEncounterState::Attacking:
		if (CurrentAttack == EBossAttackType::Shockwave)
		{
			UpdateShockwave(FMath::Clamp(
				StateElapsed / FMath::Max(ShockwaveAttackDuration, 0.1f),
				0.0f,
				1.0f));
		}
		else if (CurrentAttack == EBossAttackType::AimedVolley)
		{
			TickAimedVolley(DeltaTime);
		}
		if (StateElapsed >= GetCurrentAttackActiveDuration())
		{
			SetEncounterState(EBossEncounterState::Recovery);
		}
		break;
	case EBossEncounterState::Recovery:
		if (StateElapsed >= RecoveryDuration)
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
		break;
	case EBossEncounterState::WeakPointExposed:
		if (StateElapsed >= WeakPointExposedDuration)
		{
			CompletedAttacksSinceExposure = 0;
			SetEncounterState(EBossEncounterState::SelectingAttack);
		}
		break;
	default:
		break;
	}
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
	LastObservedPlayerState = ObservePlayerSpatialState();
	if (const APawn* PlayerPawn =
			UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		LockedTargetLocation = PlayerPawn->GetActorLocation();
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

	const float FloorZ = ShockwaveVisual.IsValid()
		? ShockwaveVisual->GetComponentLocation().Z
		: GetOwner()->GetActorLocation().Z;
	return FMath::Abs(PlayerPawn->GetActorLocation().Z - FloorZ)
			> GroundDamageMaximumHeight
		? EPlayerSpatialState::Airborne
		: EPlayerSpatialState::Grounded;
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
	case EBossAttackType::SweepLaser:
		return LaserWarningDuration;
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
			(AimedVolleyProjectileCount - 1) * AimedVolleyShotInterval + 0.05f);
	case EBossAttackType::SweepLaser:
		return LaserSweepDuration;
	case EBossAttackType::Shockwave:
	default:
		return ShockwaveAttackDuration;
	}
}

void UBossEncounterComponent::BeginCurrentAttackWarning()
{
	if (ShockwaveVisual.IsValid())
	{
		const bool bShockwave = CurrentAttack == EBossAttackType::Shockwave;
		ShockwaveVisual->SetVisibility(bShockwave);
		if (bShockwave)
		{
			ShockwaveVisual->SetRelativeScale3D(ShockwaveBaseScale);
			if (ShockwaveWarningMaterial)
			{
				ShockwaveVisual->SetMaterial(0, ShockwaveWarningMaterial);
			}
		}
	}
	if (CurrentAttack == EBossAttackType::SweepLaser)
	{
		SpawnLaserWarning();
	}
}

void UBossEncounterComponent::BeginCurrentAttack()
{
	switch (CurrentAttack)
	{
	case EBossAttackType::Shockwave:
		PreviousShockwaveRadius = 0.0f;
		bPlayerDamagedThisAttack = false;
		if (ShockwaveVisual.IsValid() && ShockwaveActiveMaterial)
		{
			ShockwaveVisual->SetMaterial(0, ShockwaveActiveMaterial);
		}
		break;
	case EBossAttackType::AimedVolley:
		AimedVolleyShotsFired = 0;
		AimedVolleyShotElapsed = 0.0f;
		SpawnAimedVolleyProjectile(AimedVolleyShotsFired++);
		break;
	case EBossAttackType::SweepLaser:
		if (ActiveSweepLaser.IsValid())
		{
			ActiveSweepLaser->ActivateLaser();
		}
		break;
	default:
		break;
	}
}

void UBossEncounterComponent::FinishCurrentAttack()
{
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

	const FVector SpawnLocation = GetOwner()->GetActorLocation();
	FVector ToPlayer = LockedTargetLocation - SpawnLocation;
	ToPlayer.Z = 0.0f;
	const float CenterYaw = ToPlayer.Rotation().Yaw;
	const float StartYaw = CenterYaw - LaserSweepDegrees * 0.5f;
	const float EndYaw = CenterYaw + LaserSweepDegrees * 0.5f;

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
			StartYaw,
			EndYaw,
			LaserSweepDuration,
			LaserDamage);
		ActiveSweepLaser = Laser;
	}
}

void UBossEncounterComponent::TickAimedVolley(const float DeltaTime)
{
	if (AimedVolleyShotsFired >= FMath::Max(AimedVolleyProjectileCount, 1))
	{
		return;
	}

	AimedVolleyShotElapsed += DeltaTime;
	const float Interval = FMath::Max(AimedVolleyShotInterval, 0.01f);
	while (AimedVolleyShotElapsed >= Interval
		&& AimedVolleyShotsFired < FMath::Max(AimedVolleyProjectileCount, 1))
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

	const FVector SpawnLocation =
		GetOwner()->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
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
		+ LateralDirection * CenteredShotIndex * AimedVolleyLateralSpacing;
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
		Projectile->InitializeProjectile(
			Direction,
			AimedVolleyProjectileSpeed,
			AimedVolleyProjectileDamage);
	}

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss aimed volley shot. Boss=%s Shot=%d/%d Target=%s Speed=%.1f"),
		*GetNameSafe(GetOwner()),
		ShotIndex + 1,
		FMath::Max(AimedVolleyProjectileCount, 1),
		*ShotTarget.ToCompactString(),
		AimedVolleyProjectileSpeed);
}

void UBossEncounterComponent::UpdateShockwave(const float NormalizedTime)
{
	const float CurrentRadius = ShockwaveMaximumRadius * NormalizedTime;
	if (ShockwaveVisual.IsValid())
	{
		const float DiameterScale = FMath::Max(CurrentRadius * 2.0f / 100.0f, 0.01f);
		ShockwaveVisual->SetRelativeScale3D(
			FVector(DiameterScale, DiameterScale, ShockwaveBaseScale.Z));
	}
	TryDamagePlayer(PreviousShockwaveRadius, CurrentRadius);
	PreviousShockwaveRadius = CurrentRadius;
}

void UBossEncounterComponent::TryDamagePlayer(
	const float PreviousRadius,
	const float CurrentRadius)
{
	if (bPlayerDamagedThisAttack || !GetWorld())
	{
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!PlayerPawn)
	{
		return;
	}

	const FVector FromBoss = PlayerPawn->GetActorLocation() - GetOwner()->GetActorLocation();
	const float HorizontalDistance = FVector(FromBoss.X, FromBoss.Y, 0.0f).Size();
	const float ShockwaveFloorZ = ShockwaveVisual.IsValid()
		? ShockwaveVisual->GetComponentLocation().Z
		: GetOwner()->GetActorLocation().Z;
	const float HeightAboveShockwave =
		PlayerPawn->GetActorLocation().Z - ShockwaveFloorZ;
	const bool bWaveReachedPlayer =
		HorizontalDistance + ShockwaveHitTolerance >= PreviousRadius
		&& HorizontalDistance - ShockwaveHitTolerance <= CurrentRadius;
	const bool bPlayerIsGroundedEnough =
		FMath::Abs(HeightAboveShockwave) <= GroundDamageMaximumHeight;
	if (bWaveReachedPlayer && bPlayerIsGroundedEnough)
	{
		if (UPlayerHealthComponent* Health =
				PlayerPawn->FindComponentByClass<UPlayerHealthComponent>())
		{
			bPlayerDamagedThisAttack = Health->ReceiveDamage(ShockwaveDamage);
			if (bPlayerDamagedThisAttack)
			{
				UE_LOG(
					LogRDCAPlayer,
					Log,
					TEXT("Boss shockwave hit. Player=%s Radius=%.1f Distance=%.1f Height=%.1f"),
					*GetNameSafe(PlayerPawn),
					CurrentRadius,
					HorizontalDistance,
					HeightAboveShockwave);
			}
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
