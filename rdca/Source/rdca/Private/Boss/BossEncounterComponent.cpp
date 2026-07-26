#include "Boss/BossEncounterComponent.h"

#include "Boss/BossWeakPointComponent.h"
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
			SetEncounterState(EBossEncounterState::PreparingAttack);
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

	OnEncounterStateChanged.Broadcast(PreviousState, NewState);
	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss encounter state. Boss=%s From=%d To=%d"),
		*GetNameSafe(GetOwner()),
		static_cast<int32>(PreviousState),
		static_cast<int32>(NewState));
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
