#include "Boss/BossEncounterComponent.h"

#include "Boss/BossWeakPointComponent.h"
#include "Boss/BossFanProjectile.h"
#include "Boss/BossSweepLaser.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Player/PlayerHealthComponent.h"
#include "rdca.h"

UBossEncounterComponent::UBossEncounterComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	FanProjectileClass = ABossFanProjectile::StaticClass();
	SweepLaserClass = ABossSweepLaser::StaticClass();
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
	case EBossEncounterState::PreparingAttack:
		return WarningDuration;
	case EBossEncounterState::Attacking:
		return AttackDuration;
	case EBossEncounterState::Recovery:
		return RecoveryDuration;
	case EBossEncounterState::WeakPointExposed:
		return WeakPointExposedDuration;
	case EBossEncounterState::PreparingFanAttack:
		return FanWarningDuration;
	case EBossEncounterState::FanAttacking:
		return FanAttackDuration;
	case EBossEncounterState::PreparingLaserAttack:
		return LaserWarningDuration;
	case EBossEncounterState::LaserAttacking:
		return LaserSweepDuration;
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
		return;
	}
	if (EncounterState == EBossEncounterState::Dead)
	{
		return;
	}

	StateElapsed += DeltaTime;
	switch (EncounterState)
	{
	case EBossEncounterState::Idle:
		if (StateElapsed >= InitialIdleDuration)
		{
			SetEncounterState(EBossEncounterState::PreparingAttack);
		}
		break;
	case EBossEncounterState::PreparingAttack:
		if (StateElapsed >= WarningDuration)
		{
			SetEncounterState(EBossEncounterState::Attacking);
		}
		break;
	case EBossEncounterState::Attacking:
		UpdateShockwave(FMath::Clamp(StateElapsed / AttackDuration, 0.0f, 1.0f));
		if (StateElapsed >= AttackDuration)
		{
			SetEncounterState(EBossEncounterState::Recovery);
		}
		break;
	case EBossEncounterState::Recovery:
		if (StateElapsed >= RecoveryDuration)
		{
			SetEncounterState(EBossEncounterState::WeakPointExposed);
		}
		break;
	case EBossEncounterState::WeakPointExposed:
		if (StateElapsed >= WeakPointExposedDuration)
		{
			switch (NextAttackPattern)
			{
			case 1:
				SetEncounterState(EBossEncounterState::PreparingFanAttack);
				break;
			case 2:
				SetEncounterState(EBossEncounterState::PreparingLaserAttack);
				break;
			case 0:
			default:
				SetEncounterState(EBossEncounterState::PreparingAttack);
				break;
			}
			NextAttackPattern = (NextAttackPattern + 1) % 3;
		}
		break;
	case EBossEncounterState::PreparingFanAttack:
		if (StateElapsed >= FanWarningDuration)
		{
			SetEncounterState(EBossEncounterState::FanAttacking);
		}
		break;
	case EBossEncounterState::FanAttacking:
		if (StateElapsed >= FanAttackDuration)
		{
			SetEncounterState(EBossEncounterState::Recovery);
		}
		break;
	case EBossEncounterState::PreparingLaserAttack:
		if (StateElapsed >= LaserWarningDuration)
		{
			SetEncounterState(EBossEncounterState::LaserAttacking);
		}
		break;
	case EBossEncounterState::LaserAttacking:
		if (StateElapsed >= LaserSweepDuration)
		{
			SetEncounterState(EBossEncounterState::Recovery);
		}
		break;
	default:
		break;
	}
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

	if (ShockwaveVisual.IsValid())
	{
		const bool bShow =
			NewState == EBossEncounterState::PreparingAttack
			|| NewState == EBossEncounterState::Attacking;
		ShockwaveVisual->SetVisibility(bShow);
		if (NewState == EBossEncounterState::PreparingAttack)
		{
			ShockwaveVisual->SetRelativeScale3D(ShockwaveBaseScale);
			if (ShockwaveWarningMaterial)
			{
				ShockwaveVisual->SetMaterial(0, ShockwaveWarningMaterial);
			}
		}
		else if (NewState == EBossEncounterState::Attacking)
		{
			PreviousShockwaveRadius = 0.0f;
			bPlayerDamagedThisAttack = false;
			if (ShockwaveActiveMaterial)
			{
				ShockwaveVisual->SetMaterial(0, ShockwaveActiveMaterial);
			}
		}
	}

	if (NewState == EBossEncounterState::FanAttacking)
	{
		SpawnFanProjectiles();
	}
	else if (NewState == EBossEncounterState::PreparingLaserAttack)
	{
		SpawnLaserWarning();
	}
	else if (NewState == EBossEncounterState::LaserAttacking
		&& ActiveSweepLaser.IsValid())
	{
		ActiveSweepLaser->ActivateLaser();
	}
	else if (NewState == EBossEncounterState::Recovery
		&& ActiveSweepLaser.IsValid())
	{
		ActiveSweepLaser->Destroy();
		ActiveSweepLaser.Reset();
	}
	else if (NewState == EBossEncounterState::Dead
		&& ActiveSweepLaser.IsValid())
	{
		ActiveSweepLaser->Destroy();
		ActiveSweepLaser.Reset();
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

void UBossEncounterComponent::SpawnLaserWarning()
{
	if (!GetWorld() || !SweepLaserClass)
	{
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!PlayerPawn)
	{
		return;
	}

	if (ActiveSweepLaser.IsValid())
	{
		ActiveSweepLaser->Destroy();
	}

	const FVector SpawnLocation = GetOwner()->GetActorLocation();
	FVector ToPlayer = PlayerPawn->GetActorLocation() - SpawnLocation;
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

void UBossEncounterComponent::SpawnFanProjectiles()
{
	if (!GetWorld() || !FanProjectileClass)
	{
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!PlayerPawn)
	{
		return;
	}

	const FVector SpawnLocation =
		GetOwner()->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
	FVector CenterDirection = PlayerPawn->GetActorLocation() - SpawnLocation;
	CenterDirection.Z = 0.0f;
	if (!CenterDirection.Normalize())
	{
		return;
	}

	const int32 ProjectileCount = FMath::Max(FanProjectileCount, 1);
	for (int32 Index = 0; Index < ProjectileCount; ++Index)
	{
		const float Alpha = ProjectileCount > 1
			? static_cast<float>(Index) / (ProjectileCount - 1)
			: 0.5f;
		const float YawOffset =
			FMath::Lerp(-FanSpreadDegrees * 0.5f, FanSpreadDegrees * 0.5f, Alpha);
		const FVector Direction =
			CenterDirection.RotateAngleAxis(YawOffset, FVector::UpVector);

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
				FanProjectileSpeed,
				FanProjectileDamage);
		}
	}

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss fan attack spawned. Boss=%s Count=%d Spread=%.1f Speed=%.1f"),
		*GetNameSafe(GetOwner()),
		ProjectileCount,
		FanSpreadDegrees,
		FanProjectileSpeed);
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
	const float HeightAboveBossFloor = FromBoss.Z;
	if (HorizontalDistance >= PreviousRadius
		&& HorizontalDistance <= CurrentRadius
		&& HeightAboveBossFloor <= GroundDamageMaximumHeight)
	{
		if (UPlayerHealthComponent* Health =
				PlayerPawn->FindComponentByClass<UPlayerHealthComponent>())
		{
			bPlayerDamagedThisAttack = Health->ReceiveDamage(ShockwaveDamage);
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
