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
			DrawTrajectoryPreview();
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

	if (CrashState != EPhaseCrashState::Crashing)
	{
		ApplyGravity(DeltaTime);
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

	if (!CalculateTrajectory(CrashStart, CrashEnd, ActiveArcHeight, CrashDuration))
	{
		CancelCharging();
		return;
	}

	CrashElapsed = 0.0f;
	VerticalVelocity = 0.0f;
	ActiveCooldownDuration = CooldownDuration;
	SetCrashState(EPhaseCrashState::Crashing);
	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Phase crash released. Arc=%s Distance=%.1f Height=%.1f Duration=%.2f"),
		GetPredictedArcType() == ECrashArcType::HighArc ? TEXT("High") : TEXT("Low"),
		FVector::Dist2D(CrashStart, CrashEnd),
		ActiveArcHeight,
		CrashDuration);
}

void UPhaseCrashComponent::CancelCharging()
{
	if (CrashState == EPhaseCrashState::Charging)
	{
		SetCrashState(EPhaseCrashState::Ready);
	}
}

void UPhaseCrashComponent::StartGroundDash()
{
	if (CrashState != EPhaseCrashState::Ready || !OwnerPawn)
	{
		return;
	}

	if (!UpdateAimTarget())
	{
		return;
	}

	CrashStart = OwnerPawn->GetActorLocation();
	FVector DashDirection = AimTarget - CrashStart;
	DashDirection.Z = 0.0f;
	const float CursorDistance = DashDirection.Size();
	if (!DashDirection.Normalize() || CursorDistance <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float TravelDistance = FMath::Min(CursorDistance, GroundDashDistance);
	CrashEnd = CrashStart + DashDirection * TravelDistance;
	CrashEnd.Z = CrashStart.Z;
	ActiveArcHeight = 0.0f;
	CrashElapsed = 0.0f;
	CrashDuration = FMath::Max(
		TravelDistance / FMath::Max(GroundDashSpeed, 1.0f),
		MinimumFlightDuration);
	ActiveCooldownDuration = GroundDashCooldown;
	VerticalVelocity = 0.0f;

	SetCrashState(EPhaseCrashState::Crashing);
	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Ground dash started. Distance=%.1f Duration=%.2f"),
		TravelDistance,
		CrashDuration);
}

float UPhaseCrashComponent::GetChargeAlpha() const
{
	return MaxChargeTime > 0.0f
		? FMath::Clamp(ChargeElapsed / MaxChargeTime, 0.0f, 1.0f)
		: 1.0f;
}

ECrashArcType UPhaseCrashComponent::GetPredictedArcType() const
{
	return GetChargeAlpha() >= HighArcThreshold
		? ECrashArcType::HighArc
		: ECrashArcType::LowArc;
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

bool UPhaseCrashComponent::CalculateTrajectory(
	FVector& OutStart,
	FVector& OutEnd,
	float& OutArcHeight,
	float& OutDuration) const
{
	if (!OwnerPawn)
	{
		return false;
	}

	OutStart = OwnerPawn->GetActorLocation();
	FVector HorizontalOffset = AimTarget - OutStart;
	HorizontalOffset.Z = 0.0f;

	const float CursorDistance = HorizontalOffset.Size();
	if (CursorDistance <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector Direction = HorizontalOffset / CursorDistance;
	const float ChargeAlpha = GetChargeAlpha();
	const float MaximumChargedDistance = FMath::Lerp(
		MinCrashDistance,
		MaxCrashDistance,
		ChargeAlpha);
	const float TravelDistance = FMath::Clamp(
		CursorDistance,
		FMath::Min(MinCrashDistance, MaximumChargedDistance),
		MaximumChargedDistance);

	OutEnd = OutStart + Direction * TravelDistance;
	OutEnd.Z = OutStart.Z;
	OutArcHeight = FMath::Lerp(MinArcHeight, MaxArcHeight, ChargeAlpha);
	OutDuration = FMath::Max(
		TravelDistance / FMath::Max(CrashSpeed, 1.0f),
		MinimumFlightDuration);
	return true;
}

FVector UPhaseCrashComponent::EvaluateTrajectory(const float NormalizedTime) const
{
	const float Alpha = FMath::Clamp(NormalizedTime, 0.0f, 1.0f);
	const float ParabolaOffset = 4.0f * ActiveArcHeight * Alpha * (1.0f - Alpha);
	return FMath::Lerp(CrashStart, CrashEnd, Alpha) + FVector::UpVector * ParabolaOffset;
}

void UPhaseCrashComponent::DrawTrajectoryPreview() const
{
	if (!GetWorld())
	{
		return;
	}

	FVector PreviewStart;
	FVector PreviewEnd;
	float PreviewArcHeight = 0.0f;
	float PreviewDuration = 0.0f;
	if (!CalculateTrajectory(
		PreviewStart,
		PreviewEnd,
		PreviewArcHeight,
		PreviewDuration))
	{
		return;
	}

	const FColor PreviewColor =
		GetPredictedArcType() == ECrashArcType::HighArc
			? FColor::Yellow
			: FColor::Green;
	constexpr int32 SegmentCount = 20;
	FVector PreviousPoint = PreviewStart;

	for (int32 SegmentIndex = 1; SegmentIndex <= SegmentCount; ++SegmentIndex)
	{
		const float Alpha = static_cast<float>(SegmentIndex) / SegmentCount;
		const FVector Point =
			FMath::Lerp(PreviewStart, PreviewEnd, Alpha)
			+ FVector::UpVector * (4.0f * PreviewArcHeight * Alpha * (1.0f - Alpha));
		DrawDebugLine(
			GetWorld(),
			PreviousPoint,
			Point,
			PreviewColor,
			false,
			0.0f,
			0,
			3.0f);
		PreviousPoint = Point;
	}

	DrawDebugSphere(
		GetWorld(),
		PreviewEnd,
		30.0f,
		12,
		PreviewColor,
		false,
		0.0f);
}

void UPhaseCrashComponent::TickCrash(const float DeltaTime)
{
	if (!OwnerPawn || CrashDuration <= UE_KINDA_SMALL_NUMBER)
	{
		FinishCrash();
		return;
	}

	CrashElapsed = FMath::Min(CrashElapsed + DeltaTime, CrashDuration);
	const float NormalizedTime = CrashElapsed / CrashDuration;
	const FVector TargetLocation = EvaluateTrajectory(NormalizedTime);

	FHitResult Hit;
	OwnerPawn->GetRootComponent()->MoveComponent(
		TargetLocation - OwnerPawn->GetActorLocation(),
		OwnerPawn->GetActorRotation(),
		true,
		&Hit);

	if (Hit.IsValidBlockingHit())
	{
		UE_LOG(
			LogRDCAPlayer,
			Log,
			TEXT("Phase crash blocked. Point=%s Normal=%s"),
			*Hit.ImpactPoint.ToCompactString(),
			*Hit.ImpactNormal.ToCompactString());
		FinishCrash();
		return;
	}

	if (CrashElapsed >= CrashDuration)
	{
		FinishCrash();
	}
}

void UPhaseCrashComponent::ApplyGravity(const float DeltaTime)
{
	if (!OwnerPawn || !OwnerPawn->GetRootComponent() || GravityAcceleration <= 0.0f)
	{
		return;
	}

	VerticalVelocity = FMath::Max(
		VerticalVelocity - GravityAcceleration * DeltaTime,
		-MaximumFallSpeed);

	FHitResult Hit;
	OwnerPawn->GetRootComponent()->MoveComponent(
		FVector(0.0f, 0.0f, VerticalVelocity * DeltaTime),
		OwnerPawn->GetActorRotation(),
		true,
		&Hit);

	if (Hit.IsValidBlockingHit() && VerticalVelocity < 0.0f)
	{
		VerticalVelocity = 0.0f;
	}
}

void UPhaseCrashComponent::TickRecovery(const float DeltaTime)
{
	RecoveryRemaining = FMath::Max(RecoveryRemaining - DeltaTime, 0.0f);
	if (RecoveryRemaining <= 0.0f)
	{
		CooldownRemaining = ActiveCooldownDuration;
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
	CrashElapsed = 0.0f;
	CrashDuration = 0.0f;
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
