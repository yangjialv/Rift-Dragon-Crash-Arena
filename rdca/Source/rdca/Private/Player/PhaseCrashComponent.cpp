#include "Player/PhaseCrashComponent.h"

#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "rdca.h"

namespace
{
const TCHAR* LexToString(const EPhaseCrashState State)
{
	switch (State)
	{
	case EPhaseCrashState::Ready:
		return TEXT("Ready");
	case EPhaseCrashState::Charging:
		return TEXT("Charging");
	case EPhaseCrashState::Crashing:
		return TEXT("Crashing");
	case EPhaseCrashState::Recovery:
		return TEXT("Recovery");
	case EPhaseCrashState::Cooldown:
		return TEXT("Cooldown");
	default:
		return TEXT("Unknown");
	}
}
}

UPhaseCrashComponent::UPhaseCrashComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPhaseCrashComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		UE_LOG(
			LogRDCAPlayer,
			Error,
			TEXT("PhaseCrashComponent must be attached to a Pawn. Owner=%s"),
			*GetNameSafe(GetOwner()));
		SetComponentTickEnabled(false);
	}
}

void UPhaseCrashComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	switch (CrashState)
	{
	case EPhaseCrashState::Charging:
		ChargeElapsed = FMath::Min(ChargeElapsed + DeltaTime, MaxChargeTime);
		UpdateAimTarget();
		if (bDrawDebugAim && GetWorld())
		{
			const FVector Start = OwnerPawn->GetActorLocation();
			DrawDebugLine(GetWorld(), Start, AimTarget, FColor::Cyan, false, 0.0f, 0, 3.0f);
			DrawDebugSphere(GetWorld(), AimTarget, 30.0f, 12, FColor::Cyan, false, 0.0f);
		}
		break;

	case EPhaseCrashState::Crashing:
		TickCrash(DeltaTime);
		break;

	case EPhaseCrashState::Recovery:
		TickRecovery(DeltaTime);
		break;

	case EPhaseCrashState::Cooldown:
		TickCooldown(DeltaTime);
		break;

	default:
		break;
	}
}

void UPhaseCrashComponent::StartCharging()
{
	if (CrashState != EPhaseCrashState::Ready || !OwnerPawn)
	{
		return;
	}

	ChargeElapsed = 0.0f;
	UpdateAimTarget();
	SetCrashState(EPhaseCrashState::Charging);
}

void UPhaseCrashComponent::ReleaseCrash()
{
	if (CrashState != EPhaseCrashState::Charging || !OwnerPawn)
	{
		return;
	}

	UpdateAimTarget();

	CrashDirection = AimTarget - OwnerPawn->GetActorLocation();
	CrashDirection.Z = 0.0f;

	if (!CrashDirection.Normalize())
	{
		CancelCharging();
		return;
	}

	CrashDistanceRemaining = FMath::Lerp(
		MinCrashDistance,
		MaxCrashDistance,
		GetChargeAlpha());

	SetCrashState(EPhaseCrashState::Crashing);
	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Phase crash released. Distance=%.1f Direction=%s"),
		CrashDistanceRemaining,
		*CrashDirection.ToCompactString());
}

void UPhaseCrashComponent::CancelCharging()
{
	if (CrashState == EPhaseCrashState::Charging)
	{
		SetCrashState(EPhaseCrashState::Ready);
	}
}

float UPhaseCrashComponent::GetChargeAlpha() const
{
	return MaxChargeTime > 0.0f
		? FMath::Clamp(ChargeElapsed / MaxChargeTime, 0.0f, 1.0f)
		: 1.0f;
}

float UPhaseCrashComponent::GetMovementInputScale() const
{
	switch (CrashState)
	{
	case EPhaseCrashState::Charging:
		return ChargingMovementScale;
	case EPhaseCrashState::Crashing:
	case EPhaseCrashState::Recovery:
		return 0.0f;
	default:
		return 1.0f;
	}
}

bool UPhaseCrashComponent::UpdateAimTarget()
{
	if (!OwnerPawn)
	{
		return false;
	}

	APlayerController* PlayerController =
		Cast<APlayerController>(OwnerPawn->GetController());
	if (!PlayerController)
	{
		return false;
	}

	FVector RayOrigin;
	FVector RayDirection;
	if (!PlayerController->DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		return false;
	}

	const FVector RayEnd = RayOrigin + RayDirection * 100000.0f;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PhaseCrashAim), false, OwnerPawn);
	FHitResult Hit;

	if (GetWorld()->LineTraceSingleByChannel(
		Hit,
		RayOrigin,
		RayEnd,
		ECC_Visibility,
		QueryParams))
	{
		AimTarget = Hit.ImpactPoint;
		AimTarget.Z = OwnerPawn->GetActorLocation().Z;
		return true;
	}

	if (FMath::Abs(RayDirection.Z) <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float PlaneZ = OwnerPawn->GetActorLocation().Z;
	const float DistanceAlongRay = (PlaneZ - RayOrigin.Z) / RayDirection.Z;
	if (DistanceAlongRay <= 0.0f)
	{
		return false;
	}

	AimTarget = RayOrigin + RayDirection * DistanceAlongRay;
	AimTarget.Z = PlaneZ;
	return true;
}

void UPhaseCrashComponent::TickCrash(const float DeltaTime)
{
	const float StepDistance = FMath::Min(CrashSpeed * DeltaTime, CrashDistanceRemaining);
	if (StepDistance <= UE_KINDA_SMALL_NUMBER)
	{
		FinishCrash();
		return;
	}

	FHitResult Hit;
	OwnerPawn->GetRootComponent()->MoveComponent(
		CrashDirection * StepDistance,
		OwnerPawn->GetActorRotation(),
		true,
		&Hit);

	CrashDistanceRemaining -= StepDistance;
	if (Hit.IsValidBlockingHit() || CrashDistanceRemaining <= UE_KINDA_SMALL_NUMBER)
	{
		FinishCrash();
	}
}

void UPhaseCrashComponent::TickRecovery(const float DeltaTime)
{
	RecoveryRemaining = FMath::Max(RecoveryRemaining - DeltaTime, 0.0f);
	if (RecoveryRemaining <= 0.0f)
	{
		CooldownRemaining = CooldownDuration;
		SetCrashState(
			CooldownRemaining > 0.0f
				? EPhaseCrashState::Cooldown
				: EPhaseCrashState::Ready);
	}
}

void UPhaseCrashComponent::TickCooldown(const float DeltaTime)
{
	CooldownRemaining = FMath::Max(CooldownRemaining - DeltaTime, 0.0f);
	if (CooldownRemaining <= 0.0f)
	{
		SetCrashState(EPhaseCrashState::Ready);
	}
}

void UPhaseCrashComponent::FinishCrash()
{
	CrashDistanceRemaining = 0.0f;
	RecoveryRemaining = RecoveryDuration;
	SetCrashState(
		RecoveryRemaining > 0.0f
			? EPhaseCrashState::Recovery
			: EPhaseCrashState::Cooldown);

	if (CrashState == EPhaseCrashState::Cooldown)
	{
		CooldownRemaining = CooldownDuration;
		if (CooldownRemaining <= 0.0f)
		{
			SetCrashState(EPhaseCrashState::Ready);
		}
	}
}

void UPhaseCrashComponent::SetCrashState(const EPhaseCrashState NewState)
{
	if (CrashState == NewState)
	{
		return;
	}

	const EPhaseCrashState PreviousState = CrashState;
	CrashState = NewState;
	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Phase crash state: %s -> %s"),
		LexToString(PreviousState),
		LexToString(NewState));
	OnCrashStateChanged.Broadcast(PreviousState, NewState);
}
