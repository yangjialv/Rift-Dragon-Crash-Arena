#include "Player/PhaseCrashComponent.h"

#include "Audio/RDCAAudio.h"
#include "Arena/ArenaCombatBounds.h"
#include "Arena/ArenaFloorCollision.h"
#include "Arena/AttachSurfaceComponent.h"
#include "Arena/AnchorOverloadComponent.h"
#include "Boss/BossEncounterComponent.h"
#include "Boss/BossWeakPointComponent.h"
#include "Combat/CrashResponseComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
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
	case EPhaseCrashState::Attached:
		return TEXT("Attached");
	default:
		return TEXT("Unknown");
	}
}

bool IsDirectCrashAimTarget(const FHitResult& Hit)
{
	AActor* HitActor = Hit.GetActor();
	return IsValid(HitActor)
		&& (HitActor->FindComponentByClass<UCrashResponseComponent>()
			|| HitActor->FindComponentByClass<UBossWeakPointComponent>());
}

bool IsBossCrashAimTarget(const FHitResult& Hit)
{
	AActor* HitActor = Hit.GetActor();
	return (IsValid(HitActor)
			&& (HitActor->FindComponentByClass<UBossEncounterComponent>()
				|| HitActor->FindComponentByClass<UBossWeakPointComponent>()))
		|| Cast<UBossWeakPointComponent>(Hit.GetComponent()) != nullptr;
}

void SetBossAimCursor(APlayerController& PlayerController, const bool bBossAim)
{
	if (bBossAim)
	{
		PlayerController.CurrentMouseCursor = EMouseCursor::Crosshairs;
	}
	else
	{
		PlayerController.CurrentMouseCursor = PlayerController.DefaultMouseCursor;
	}
}

float GetArenaAimPlaneZ(const UWorld& World, const APawn& OwnerPawn)
{
	for (TActorIterator<AArenaCombatBounds> It(&World); It; ++It)
	{
		return It->GetArenaCenter().Z;
	}
	// Keeps older maps functional when they have not added Arena Combat Bounds.
	return OwnerPawn.GetActorLocation().Z;
}

AArenaFloorCollision* FindArenaFloorCollision(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AArenaFloorCollision> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
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
	ReboundPresentationRemaining = FMath::Max(
		ReboundPresentationRemaining - DeltaTime,
		0.0f);

	if (bAttachCornerTransitionActive)
	{
		TickAttachCornerTransition(DeltaTime);
	}

	switch (CrashState)
	{
	case EPhaseCrashState::Charging:
		if (bChargingFromAttachment)
		{
			MoveAttached(FVector2D::ZeroVector);
		}
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

	case EPhaseCrashState::Attached:
		if (!AttachedActor.IsValid()
			|| !AttachedSurfaceComponent.IsValid()
			|| !AttachedResponseComponent.IsValid())
		{
			DetachFromCrashTarget();
			ClearTemporaryMoveIgnores();
			SetCrashState(EPhaseCrashState::Ready);
		}
		else
		{
			MoveAttached(FVector2D::ZeroVector);
		}
		break;

	default:
		break;
	}

	ConsumeBufferedInputs(DeltaTime);

	if (CrashState != EPhaseCrashState::Crashing
		&& CrashState != EPhaseCrashState::Attached
		&& !bChargingFromAttachment)
	{
		ApplyGravity(DeltaTime);
	}
}

void UPhaseCrashComponent::StartCharging()
{
	if (!OwnerPawn)
	{
		return;
	}
	bCrashInputHeld = true;
	bCrashInputBuffered = true;
	if (CrashState != EPhaseCrashState::Ready
		&& CrashState != EPhaseCrashState::Attached)
	{
		UE_LOG(
			LogRDCAPlayer,
			Verbose,
			TEXT("Crash aim buffered. State=%d Cooldown=%.2f"),
			static_cast<int32>(CrashState),
			CooldownRemaining);
		return;
	}

	BeginCrashAim();
}

void UPhaseCrashComponent::BeginCrashAim()
{
	if (!OwnerPawn
		|| (CrashState != EPhaseCrashState::Ready
			&& CrashState != EPhaseCrashState::Attached))
	{
		return;
	}

	bChargingFromAttachment = CrashState == EPhaseCrashState::Attached;
	if (!UpdateAimTarget())
	{
		return;
	}
	bCrashInputBuffered = false;
	if (UPawnMovementComponent* Movement = OwnerPawn->GetMovementComponent())
	{
		Movement->StopMovementImmediately();
	}
	DragStartAimTarget = AimTarget;
	SetCrashState(EPhaseCrashState::Charging);
}

void UPhaseCrashComponent::ReleaseCrash()
{
	bCrashInputHeld = false;
	if (CrashState != EPhaseCrashState::Charging || !OwnerPawn)
	{
		bCrashInputBuffered = false;
		return;
	}

	UpdateAimTarget();

	if (!CalculateTrajectory(CrashStart, CrashEnd, ActiveArcHeight, CrashDuration))
	{
		CancelCharging();
		return;
	}

	bActiveCrashFromAttachment = bChargingFromAttachment;
	bActiveBossCrash = bChargingFromAttachment && bAimingAtBoss;
	bWeakPointDamageAppliedThisCrash = false;
	TWeakObjectPtr<AActor> DepartureAnchor;
	if (bChargingFromAttachment)
	{
		DepartureAnchor = AttachedActor;
	}
	DetachFromCrashTarget();
	bChargingFromAttachment = false;
	bAimingAtBoss = false;
	if (APlayerController* PlayerController =
			Cast<APlayerController>(OwnerPawn->GetController()))
	{
		SetBossAimCursor(*PlayerController, false);
	}
	ClearTemporaryMoveIgnores();
	CrashElapsed = 0.0f;
	VerticalVelocity = 0.0f;
	ActiveCooldownDuration = CooldownDuration;
	bCurrentCrashRefreshesOnLanding = true;
	SetCrashState(EPhaseCrashState::Crashing);
	RDCAAudio::PlayAtLocation(
		this,
		ERDCAAudioCue::PlayerJumpRelease,
		OwnerPawn->GetActorLocation(),
		0.6f);
	if (DepartureAnchor.IsValid())
	{
		if (UAnchorOverloadComponent* Overload =
				DepartureAnchor->FindComponentByClass<UAnchorOverloadComponent>())
		{
			Overload->ShatterAfterPlayerDeparture();
		}
	}
	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Phase crash released. Mode=%s Distance=%.1f Height=%.1f Duration=%.2f"),
		bActiveBossCrash ? TEXT("BossDirect")
			: (GetPredictedArcType() == ECrashArcType::HighArc ? TEXT("High") : TEXT("Low")),
		FVector::Dist2D(CrashStart, CrashEnd),
		ActiveArcHeight,
		CrashDuration);
}

void UPhaseCrashComponent::CancelCharging()
{
	bCrashInputHeld = false;
	bCrashInputBuffered = false;
	bAimingAtBoss = false;
	if (APlayerController* PlayerController =
			Cast<APlayerController>(OwnerPawn ? OwnerPawn->GetController() : nullptr))
	{
		SetBossAimCursor(*PlayerController, false);
	}
	if (CrashState == EPhaseCrashState::Charging)
	{
		const bool bReturnToAttachment =
			bChargingFromAttachment && AttachedActor.IsValid();
		bChargingFromAttachment = false;
		SetCrashState(
			bReturnToAttachment
				? EPhaseCrashState::Attached
				: EPhaseCrashState::Ready);
	}
}

void UPhaseCrashComponent::StartGroundDash()
{
	if ((CrashState != EPhaseCrashState::Ready
			&& CrashState != EPhaseCrashState::Attached)
		|| !OwnerPawn)
	{
		if (OwnerPawn)
		{
			DashInputBufferRemaining = DashInputBufferDuration;
		}
		return;
	}
	DashInputBufferRemaining = 0.0f;
	const bool bDashingFromAttachment =
		CrashState == EPhaseCrashState::Attached && AttachedActor.IsValid();
	TWeakObjectPtr<AActor> DepartureAnchor;
	if (bDashingFromAttachment)
	{
		DepartureAnchor = AttachedActor;
	}

	if (!UpdateAimTarget())
	{
		return;
	}
	bAimingAtBoss = false;
	if (APlayerController* PlayerController =
			Cast<APlayerController>(OwnerPawn->GetController()))
	{
		SetBossAimCursor(*PlayerController, false);
	}
	if (UPawnMovementComponent* Movement = OwnerPawn->GetMovementComponent())
	{
		Movement->StopMovementImmediately();
	}

	DetachFromCrashTarget();
	bActiveCrashFromAttachment = false;
	bActiveBossCrash = false;
	bWeakPointDamageAppliedThisCrash = false;
	ClearTemporaryMoveIgnores();
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
	bCurrentCrashRefreshesOnLanding = false;
	VerticalVelocity = 0.0f;

	SetCrashState(EPhaseCrashState::Crashing);
	RDCAAudio::PlayAtLocation(
		this,
		ERDCAAudioCue::PlayerDash,
		OwnerPawn->GetActorLocation(),
		0.58f);
	if (DepartureAnchor.IsValid())
	{
		if (UAnchorOverloadComponent* Overload =
				DepartureAnchor->FindComponentByClass<UAnchorOverloadComponent>())
		{
			Overload->ShatterAfterPlayerDeparture();
		}
	}
	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Ground dash started. Distance=%.1f Duration=%.2f"),
		TravelDistance,
		CrashDuration);
}

void UPhaseCrashComponent::ForceArenaRecovery(
	const FVector& Destination,
	const float ArcHeight,
	const float Duration)
{
	if (!OwnerPawn || !OwnerPawn->GetRootComponent())
	{
		return;
	}

	if (UPawnMovementComponent* Movement = OwnerPawn->GetMovementComponent())
	{
		Movement->StopMovementImmediately();
	}
	DetachFromCrashTarget();
	ClearTemporaryMoveIgnores();
	bChargingFromAttachment = false;
	bActiveCrashFromAttachment = false;
	bActiveBossCrash = false;
	bAimingAtBoss = false;
	bWeakPointDamageAppliedThisCrash = false;
	bCrashInputBuffered = false;
	DashInputBufferRemaining = 0.0f;
	CrashStart = OwnerPawn->GetActorLocation();
	CrashEnd = Destination;
	ActiveArcHeight = FMath::Max(ArcHeight, 0.0f);
	CrashElapsed = 0.0f;
	CrashDuration = FMath::Max(Duration, MinimumFlightDuration);
	ActiveCooldownDuration = GroundDashCooldown;
	bCurrentCrashRefreshesOnLanding = false;
	VerticalVelocity = 0.0f;
	SetCrashState(EPhaseCrashState::Crashing);

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Arena recovery launched. Start=%s End=%s Height=%.1f Duration=%.2f"),
		*CrashStart.ToCompactString(),
		*CrashEnd.ToCompactString(),
		ActiveArcHeight,
		CrashDuration);
}

void UPhaseCrashComponent::ConsumeBufferedInputs(const float DeltaTime)
{
	DashInputBufferRemaining = FMath::Max(
		DashInputBufferRemaining - DeltaTime,
		0.0f);
	const bool bActionAvailable =
		CrashState == EPhaseCrashState::Ready
		|| CrashState == EPhaseCrashState::Attached;
	if (!bActionAvailable)
	{
		return;
	}

	if (bCrashInputBuffered && bCrashInputHeld)
	{
		BeginCrashAim();
		return;
	}
	if (DashInputBufferRemaining > 0.0f)
	{
		StartGroundDash();
	}
}

void UPhaseCrashComponent::MoveAttached(const FVector2D& MovementInput)
{
	if ((CrashState != EPhaseCrashState::Attached && !bChargingFromAttachment)
		|| !OwnerPawn
		|| !AttachedActor.IsValid()
		|| !AttachedSurfaceComponent.IsValid()
		|| !AttachedResponseComponent.IsValid()
		|| !OwnerPawn->GetRootComponent())
	{
		return;
	}

	if (bAttachCornerTransitionActive)
	{
		return;
	}

	const UCrashResponseComponent* ResponseComponent =
		AttachedResponseComponent.Get();
	UPrimitiveComponent* SurfaceComponent = AttachedSurfaceComponent.Get();
	const float MovementStep =
		ResponseComponent->GetAttachMoveSpeed() * GetWorld()->GetDeltaSeconds();

	if (UAttachSurfaceComponent* AttachBox =
			Cast<UAttachSurfaceComponent>(SurfaceComponent))
	{
		MoveOnAttachBox(*AttachBox, MovementInput, MovementStep);
		return;
	}

	const FTransform SurfaceTransform = SurfaceComponent->GetComponentTransform();
	const FVector SurfaceOrigin =
		SurfaceTransform.TransformPosition(AttachedSurfaceOriginLocal);
	const FVector SurfaceOut =
		SurfaceTransform.TransformVectorNoScale(AttachedSurfaceOutLocal).GetSafeNormal();
	const FVector SurfaceUp =
		SurfaceTransform.TransformVectorNoScale(AttachedSurfaceUpLocal).GetSafeNormal();
	const FVector SurfaceRight =
		SurfaceTransform.TransformVectorNoScale(AttachedSurfaceRightLocal).GetSafeNormal();

	const float RequestedSurfaceX =
		AttachedSurfaceX + MovementInput.X * MovementStep;
	const float RequestedSurfaceY =
		AttachedSurfaceY + MovementInput.Y * MovementStep;

	const float CandidateSurfaceX = FMath::Clamp(
		RequestedSurfaceX,
		AttachedSurfaceMinX,
		AttachedSurfaceMaxX);
	const float CandidateSurfaceY = FMath::Clamp(
		RequestedSurfaceY,
		AttachedSurfaceMinY,
		AttachedSurfaceMaxY);
	const FVector CandidateWorldPosition =
		SurfaceOrigin
		+ SurfaceRight * CandidateSurfaceX
		+ SurfaceUp * CandidateSurfaceY
		+ SurfaceOut * AttachedSurfaceOffset;

	FHitResult Hit;
	OwnerPawn->GetRootComponent()->MoveComponent(
		CandidateWorldPosition - OwnerPawn->GetActorLocation(),
		OwnerPawn->GetActorRotation(),
		true,
		&Hit);

	if (!Hit.IsValidBlockingHit())
	{
		AttachedSurfaceX = CandidateSurfaceX;
		AttachedSurfaceY = CandidateSurfaceY;
	}
}

void UPhaseCrashComponent::ForceDetachFromAttachment()
{
	if (!IsAttached())
	{
		return;
	}

	bChargingFromAttachment = false;
	bAttachCornerTransitionActive = false;
	DetachFromCrashTarget();
	ClearTemporaryMoveIgnores();
	VerticalVelocity = 0.0f;
	SetCrashState(EPhaseCrashState::Ready);
}

float UPhaseCrashComponent::GetChargeAlpha() const
{
	FVector DragOffset = AimTarget - DragStartAimTarget;
	DragOffset.Z = 0.0f;
	return MaxDragDistance > 0.0f
		? FMath::Clamp(DragOffset.Size() / MaxDragDistance, 0.0f, 1.0f)
		: 1.0f;
}

FVector UPhaseCrashComponent::GetAttachedSurfaceNormal() const
{
	if (bAttachCornerTransitionActive)
	{
		if (const UAttachSurfaceComponent* AttachBox =
				Cast<UAttachSurfaceComponent>(AttachedSurfaceComponent.Get()))
		{
			const FVector StartNormal =
				AttachBox->GetFaceNormalWorld(AttachCornerStartFace);
			const FVector EndNormal =
				AttachBox->GetFaceNormalWorld(AttachCornerEndFace);
			const float LinearAlpha = FMath::Clamp(
				AttachCornerTransitionElapsed
					/ FMath::Max(AttachCornerTransitionDuration, 0.01f),
				0.0f,
				1.0f);
			const float SmoothAlpha = FMath::SmoothStep(
				0.0f,
				1.0f,
				LinearAlpha);
			const FQuat QuarterTurn = FQuat::FindBetweenNormals(
				StartNormal,
				EndNormal);
			return FQuat::Slerp(
				FQuat::Identity,
				QuarterTurn,
				SmoothAlpha).RotateVector(StartNormal).GetSafeNormal();
		}
	}

	return AttachedSurfaceComponent.IsValid()
		? AttachedSurfaceComponent->GetComponentTransform()
			.TransformVectorNoScale(AttachedSurfaceOutLocal).GetSafeNormal()
		: FVector::UpVector;
}

float UPhaseCrashComponent::GetCooldownReadyPercent() const
{
	if (CrashState != EPhaseCrashState::Cooldown)
	{
		return CrashState == EPhaseCrashState::Ready
			|| CrashState == EPhaseCrashState::Charging
			|| CrashState == EPhaseCrashState::Attached
			? 1.0f
			: 0.0f;
	}

	const float Duration = FMath::Max(ActiveCooldownDuration, CooldownDuration);
	return Duration > 0.0f
		? 1.0f - FMath::Clamp(CooldownRemaining / Duration, 0.0f, 1.0f)
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
	case EPhaseCrashState::Attached:
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

	bAimingAtBoss = false;
	SetBossAimCursor(*PlayerController, false);

	FVector RayOrigin;
	FVector RayDirection;
	if (!PlayerController->DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		return false;
	}

	const FVector RayEnd = RayOrigin + RayDirection * 100000.0f;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PhaseCrashAim), false, OwnerPawn);

	TArray<FHitResult> Hits;
	if (GetWorld()->LineTraceMultiByChannel(
		Hits,
		RayOrigin,
		RayEnd,
		ECC_Visibility,
		QueryParams))
	{
		for (const FHitResult& CandidateHit : Hits)
		{
			// Decorative environment collision must not pull the landing reticle
			// off the cursor's ground projection. Combat and attachment targets
			// remain directly aimable at their real hit point.
			if (IsDirectCrashAimTarget(CandidateHit))
			{
				AimTarget = CandidateHit.ImpactPoint;
				bAimingAtBoss = bChargingFromAttachment
					&& IsBossCrashAimTarget(CandidateHit);
				SetBossAimCursor(*PlayerController, bAimingAtBoss);
				return true;
			}
		}
	}

	if (FMath::Abs(RayDirection.Z) <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float PlaneZ = GetArenaAimPlaneZ(*GetWorld(), *OwnerPawn);
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
	if (bChargingFromAttachment && bAimingAtBoss)
	{
		const float DirectDistance = FVector::Distance(OutStart, AimTarget);
		if (DirectDistance <= UE_KINDA_SMALL_NUMBER)
		{
			return false;
		}

		OutEnd = AimTarget;
		OutArcHeight = 0.0f;
		OutDuration = FMath::Max(
			DirectDistance / FMath::Max(BossCrashSpeed, 1.0f),
			MinimumFlightDuration);
		return true;
	}

	FVector HorizontalOffset = AimTarget - OutStart;
	HorizontalOffset.Z = 0.0f;

	FVector DragOffset = AimTarget - DragStartAimTarget;
	DragOffset.Z = 0.0f;
	if (DragOffset.Size() < MinimumDragDistance)
	{
		return false;
	}

	const float TargetHorizontalDistance = HorizontalOffset.Size();
	if (TargetHorizontalDistance <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector Direction = HorizontalOffset / TargetHorizontalDistance;
	const float ChargeAlpha = GetChargeAlpha();
	const float TravelDistance = FMath::Min(
		TargetHorizontalDistance,
		MaxCrashDistance);
	const float TargetHeightAlpha = FMath::Clamp(
		TravelDistance / TargetHorizontalDistance,
		0.0f,
		1.0f);
	OutEnd = OutStart + Direction * TravelDistance;
	OutEnd.Z = FMath::Lerp(OutStart.Z, AimTarget.Z, TargetHeightAlpha);
	OutArcHeight = FMath::Lerp(MinArcHeight, MaxArcHeight, ChargeAlpha);
	OutDuration = FMath::Max(
		FVector::Distance(OutStart, OutEnd) / FMath::Max(CrashSpeed, 1.0f),
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
		// A press has already produced a valid cursor projection, even before
		// enough drag distance exists to authorise a launch. Show that point
		// immediately; only the trajectory remains absent until the player drags.
		DrawDebugSphere(
			GetWorld(),
			AimTarget,
			24.0f,
			12,
			FColor::Cyan,
			false,
			0.0f,
			0,
			2.0f);
		return;
	}

	const FColor PreviewColor =
		IsBossAimActive()
			? FColor::Red
			: GetPredictedArcType() == ECrashArcType::HighArc
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
	const FVector MovementDelta = TargetLocation - OwnerPawn->GetActorLocation();
	OwnerPawn->GetRootComponent()->MoveComponent(
		MovementDelta,
		OwnerPawn->GetActorRotation(),
		true,
		&Hit);

	if (Hit.IsValidBlockingHit())
	{
		HandleCrashImpact(Hit, MovementDelta.GetSafeNormal());
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
		AActor* HitActor = Hit.GetActor();
		UCrashResponseComponent* ResponseComponent = HitActor
			? HitActor->FindComponentByClass<UCrashResponseComponent>()
			: nullptr;
		if (ResponseComponent
			&& ResponseComponent->GetResponse()
				== ECrashCollisionResponse::Rebound)
		{
			ResponseComponent->NotifyCrashImpact(OwnerPawn, Hit);
			ClearTemporaryMoveIgnores();
			bActiveCrashFromAttachment = false;
			bWeakPointDamageAppliedThisCrash = false;
			VerticalVelocity = 0.0f;
			SetCrashState(EPhaseCrashState::Crashing);
			// Passive contact has no useful horizontal incoming vector. Treat the
			// surface normal as the stored energy direction so irregular Boss
			// hulls always eject the player outward instead of becoming ground.
			HandleReboundImpact(
				HitActor,
				Hit,
				-Hit.ImpactNormal.GetSafeNormal(),
				*ResponseComponent);
			UE_LOG(
				LogRDCAPlayer,
				Log,
				TEXT("Passive rebound surface rejected player. Target=%s Normal=%s"),
				*GetNameSafe(HitActor),
				*Hit.ImpactNormal.ToCompactString());
			return;
		}
		VerticalVelocity = 0.0f;
		if (bRefreshJumpCooldownOnArenaLanding
			&& bCurrentCrashRefreshesOnLanding
			&& Cast<AArenaFloorCollision>(HitActor)
			&& Hit.ImpactNormal.Z >= 0.5f
			&& (CrashState == EPhaseCrashState::Recovery
				|| CrashState == EPhaseCrashState::Cooldown))
		{
			RecoveryRemaining = 0.0f;
			CooldownRemaining = 0.0f;
			ActiveCooldownDuration = 0.0f;
			bCurrentCrashRefreshesOnLanding = false;
			SetCrashState(EPhaseCrashState::Ready);
		}
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
	ClearTemporaryMoveIgnores();
	bActiveBossCrash = false;
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

void UPhaseCrashComponent::HandleCrashImpact(
	const FHitResult& Hit,
	const FVector& IncomingDirection)
{
	AActor* TargetActor = Hit.GetActor();
	UCrashResponseComponent* ResponseComponent =
		TargetActor
			? TargetActor->FindComponentByClass<UCrashResponseComponent>()
			: nullptr;
	const ECrashCollisionResponse Response =
		Cast<UBossWeakPointComponent>(Hit.GetComponent())
			? ECrashCollisionResponse::Rebound
			: Cast<UAttachSurfaceComponent>(Hit.GetComponent())
			? ECrashCollisionResponse::Attach
			: ResponseComponent
			? ResponseComponent->GetResponse()
			: ECrashCollisionResponse::Block;

	if (ResponseComponent)
	{
		ResponseComponent->NotifyCrashImpact(OwnerPawn, Hit);
	}

	if (UBossWeakPointComponent* WeakPoint =
			Cast<UBossWeakPointComponent>(Hit.GetComponent()))
	{
		const bool bCanApplyDamage =
			bActiveCrashFromAttachment
			&& !bWeakPointDamageAppliedThisCrash;
		if (WeakPoint->ReceiveCrash(OwnerPawn, Hit, bCanApplyDamage))
		{
			bWeakPointDamageAppliedThisCrash = true;
		}

		// A weak point always ejects the player. Some layouts use it as a child
		// Actor and therefore do not inherit the Boss body's Crash Response; use
		// the component defaults as a safe fallback instead of silently blocking.
		if (TargetActor)
		{
			const UCrashResponseComponent* ReboundConfig = ResponseComponent
				? ResponseComponent
				: GetDefault<UCrashResponseComponent>();
			HandleReboundImpact(
				TargetActor,
				Hit,
				IncomingDirection,
				*ReboundConfig);
		}
		else
		{
			FinishCrash();
		}
		return;
	}

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Crash impact. Target=%s Response=%d Point=%s Incoming=%s Normal=%s"),
		*GetNameSafe(TargetActor),
		static_cast<int32>(Response),
		*Hit.ImpactPoint.ToCompactString(),
		*IncomingDirection.ToCompactString(),
		*Hit.ImpactNormal.ToCompactString());

	switch (Response)
	{
	case ECrashCollisionResponse::Attach:
		if (TargetActor && ResponseComponent)
		{
			HandleAttachImpact(TargetActor, Hit, *ResponseComponent);
		}
		else
		{
			FinishCrash();
		}
		break;

	case ECrashCollisionResponse::Rebound:
		if (TargetActor && ResponseComponent)
		{
			HandleReboundImpact(
				TargetActor,
				Hit,
				IncomingDirection,
				*ResponseComponent);
		}
		else
		{
			FinishCrash();
		}
		break;

	case ECrashCollisionResponse::Ignore:
		if (TargetActor)
		{
			AddTemporaryMoveIgnore(TargetActor);
		}
		else
		{
			FinishCrash();
		}
		break;

	case ECrashCollisionResponse::Block:
	default:
		FinishCrash();
		break;
	}
}

void UPhaseCrashComponent::HandleAttachImpact(
	AActor* TargetActor,
	const FHitResult& Hit,
	UCrashResponseComponent& ResponseComponent)
{
	ClearTemporaryMoveIgnores();
	CrashElapsed = 0.0f;
	CrashDuration = 0.0f;
	VerticalVelocity = 0.0f;
	AttachedActor = TargetActor;
	AttachedSurfaceComponent = Hit.GetComponent();
	AttachedResponseComponent = &ResponseComponent;
	AttachedSurfaceX = 0.0f;
	AttachedSurfaceY = 0.0f;
	if (UAnchorOverloadComponent* Overload =
			TargetActor->FindComponentByClass<UAnchorOverloadComponent>())
	{
		Overload->NotifyPlayerAttached(OwnerPawn);
	}

	UPrimitiveComponent* SurfaceComponent = AttachedSurfaceComponent.Get();
	if (TargetActor && SurfaceComponent)
	{
		if (UAttachSurfaceComponent* ExplicitSurface =
				Cast<UAttachSurfaceComponent>(SurfaceComponent))
		{
			ConfigureAttachBoxSurface(
				*ExplicitSurface,
				Hit.ImpactPoint,
				Hit.ImpactNormal);
			AddTemporaryMoveIgnore(TargetActor);
			SetCrashState(EPhaseCrashState::Attached);
			return;
		}

		const FTransform SurfaceTransform = SurfaceComponent->GetComponentTransform();
		const FVector SurfaceNormal = Hit.ImpactNormal.GetSafeNormal();
		FVector SurfaceUp = FVector::VectorPlaneProject(
			FVector::UpVector,
			SurfaceNormal).GetSafeNormal();
		if (SurfaceUp.IsNearlyZero())
		{
			SurfaceUp = FVector::VectorPlaneProject(
				SurfaceComponent->GetUpVector(),
				SurfaceNormal).GetSafeNormal();
		}
		if (SurfaceUp.IsNearlyZero())
		{
			SurfaceUp = FVector::VectorPlaneProject(
				SurfaceComponent->GetForwardVector(),
				SurfaceNormal).GetSafeNormal();
		}
		if (SurfaceUp.IsNearlyZero())
		{
			FinishCrash();
			return;
		}

		const FVector SurfaceRight =
			FVector::CrossProduct(SurfaceNormal, SurfaceUp).GetSafeNormal();
		AttachedSurfaceOriginLocal =
			SurfaceTransform.InverseTransformPosition(Hit.ImpactPoint);
		AttachedSurfaceOutLocal =
			SurfaceTransform.InverseTransformVectorNoScale(SurfaceNormal).GetSafeNormal();
		AttachedSurfaceUpLocal =
			SurfaceTransform.InverseTransformVectorNoScale(SurfaceUp).GetSafeNormal();
		AttachedSurfaceRightLocal =
			SurfaceTransform.InverseTransformVectorNoScale(SurfaceRight).GetSafeNormal();
		AttachedSurfaceOffset = FMath::Max(
			FVector::DotProduct(
				OwnerPawn->GetActorLocation() - Hit.ImpactPoint,
				SurfaceNormal),
			1.0f);

		const FBoxSphereBounds LocalBounds = SurfaceComponent->GetLocalBounds();
		AttachedSurfaceMinX = TNumericLimits<float>::Max();
		AttachedSurfaceMaxX = TNumericLimits<float>::Lowest();
		AttachedSurfaceMinY = TNumericLimits<float>::Max();
		AttachedSurfaceMaxY = TNumericLimits<float>::Lowest();

		for (int32 XSign = -1; XSign <= 1; XSign += 2)
		{
			for (int32 YSign = -1; YSign <= 1; YSign += 2)
			{
				for (int32 ZSign = -1; ZSign <= 1; ZSign += 2)
				{
					const FVector LocalCorner =
						LocalBounds.Origin
						+ FVector(
							LocalBounds.BoxExtent.X * XSign,
							LocalBounds.BoxExtent.Y * YSign,
							LocalBounds.BoxExtent.Z * ZSign);
					const FVector WorldCorner =
						SurfaceTransform.TransformPosition(LocalCorner);
					const FVector CornerFromSurfaceOrigin =
						WorldCorner - Hit.ImpactPoint;
					const float CornerX = FVector::DotProduct(
						CornerFromSurfaceOrigin,
						SurfaceRight);
					const float CornerY = FVector::DotProduct(
						CornerFromSurfaceOrigin,
						SurfaceUp);

					AttachedSurfaceMinX = FMath::Min(AttachedSurfaceMinX, CornerX);
					AttachedSurfaceMaxX = FMath::Max(AttachedSurfaceMaxX, CornerX);
					AttachedSurfaceMinY = FMath::Min(AttachedSurfaceMinY, CornerY);
					AttachedSurfaceMaxY = FMath::Max(AttachedSurfaceMaxY, CornerY);
				}
			}
		}

		AddTemporaryMoveIgnore(TargetActor);
	}
	else
	{
		FinishCrash();
		return;
	}

	SetCrashState(EPhaseCrashState::Attached);
	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Attached to crash target. Target=%s Point=%s"),
		*GetNameSafe(TargetActor),
		*Hit.ImpactPoint.ToCompactString());
}

void UPhaseCrashComponent::HandleReboundImpact(
	AActor* TargetActor,
	const FHitResult& Hit,
	const FVector& IncomingDirection,
	const UCrashResponseComponent& ResponseComponent)
{
	bActiveBossCrash = false;
	ReboundPresentationRemaining = ReboundPresentationDuration;
	if (TargetActor
		&& TargetActor->FindComponentByClass<UBossEncounterComponent>()
		&& !Cast<UBossWeakPointComponent>(Hit.GetComponent()))
	{
		RDCAAudio::PlayAtLocation(
			this,
			ERDCAAudioCue::BossBodyRebound,
			Hit.ImpactPoint,
			0.58f);
	}
	FVector ReboundDirection = FMath::GetReflectionVector(
		IncomingDirection,
		Hit.ImpactNormal.GetSafeNormal());
	const float MaximumZ = ResponseComponent.GetMaximumReboundZRatio();
	ReboundDirection.Z = FMath::Clamp(ReboundDirection.Z, -MaximumZ, MaximumZ);

	if (!ReboundDirection.Normalize())
	{
		FinishCrash();
		return;
	}

	float ReboundDistance = ResponseComponent.GetReboundDistance();
	AArenaFloorCollision* FloorCollision = FindArenaFloorCollision(GetWorld());
	if (FloorCollision)
	{
		const FVector FloorCenter = FloorCollision->GetFloorCenter();
		FVector StartFromCenter = OwnerPawn->GetActorLocation() - FloorCenter;
		StartFromCenter.Z = 0.0f;
		const float StartRadius = StartFromCenter.Size();
		const FVector IntendedEnd = OwnerPawn->GetActorLocation()
			+ ReboundDirection * ReboundDistance;
		FVector IntendedEndFromCenter = IntendedEnd - FloorCenter;
		IntendedEndFromCenter.Z = 0.0f;

		// A Boss-body rebound must never leave the player inside the central hole.
		// Use the arena floor as the source of truth, rather than an arbitrary
		// per-Boss distance that stops working when the arena is resized.
		if (IntendedEndFromCenter.Size()
			< FloorCollision->GetInnerHoleRadius() + 300.0f)
		{
			FVector OutwardDirection = StartFromCenter;
			if (!OutwardDirection.Normalize())
			{
				OutwardDirection = Hit.ImpactPoint - FloorCenter;
				OutwardDirection.Z = 0.0f;
				if (!OutwardDirection.Normalize())
				{
					OutwardDirection = FVector::ForwardVector;
				}
			}

			const float SafeRadius = FMath::Min(
				FloorCollision->GetInnerHoleRadius() + 300.0f,
				FloorCollision->GetOuterFloorRadius() - 150.0f);
			ReboundDirection = OutwardDirection;
			ReboundDistance = FMath::Max(
				ReboundDistance,
				FMath::Max(SafeRadius - StartRadius, 0.0f));
		}
	}

	AddTemporaryMoveIgnore(TargetActor);
	CrashStart = OwnerPawn->GetActorLocation();
	CrashEnd =
		CrashStart
		+ ReboundDirection * ReboundDistance;

	// Boss rebound is a single diagonal ejection to the ring floor, not a
	// horizontal movement at mouth height followed by a separate gravity fall.
	// Other Rebound actors retain their existing generic reflected trajectory.
	if (FloorCollision && TargetActor
		&& TargetActor->FindComponentByClass<UBossEncounterComponent>())
	{
		constexpr float PlayerFloorClearance = 55.0f;
		CrashEnd.Z = FloorCollision->GetFloorCenter().Z + PlayerFloorClearance;
	}
	ActiveArcHeight = 0.0f;
	CrashElapsed = 0.0f;
	CrashDuration = FMath::Max(
		FVector::Dist(CrashStart, CrashEnd)
			/ FMath::Max(ResponseComponent.GetReboundSpeed(), 1.0f),
		MinimumFlightDuration);

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Crash rebound. Direction=%s Distance=%.1f"),
		*ReboundDirection.ToCompactString(),
		ReboundDistance);
}

void UPhaseCrashComponent::AddTemporaryMoveIgnore(AActor* TargetActor)
{
	UPrimitiveComponent* RootPrimitive =
		Cast<UPrimitiveComponent>(OwnerPawn ? OwnerPawn->GetRootComponent() : nullptr);
	if (!RootPrimitive || !TargetActor
		|| TemporarilyIgnoredActors.Contains(TargetActor))
	{
		return;
	}

	RootPrimitive->IgnoreActorWhenMoving(TargetActor, true);
	TemporarilyIgnoredActors.Add(TargetActor);
}

void UPhaseCrashComponent::ClearTemporaryMoveIgnores()
{
	UPrimitiveComponent* RootPrimitive =
		Cast<UPrimitiveComponent>(OwnerPawn ? OwnerPawn->GetRootComponent() : nullptr);
	if (RootPrimitive)
	{
		for (const TWeakObjectPtr<AActor>& IgnoredActor : TemporarilyIgnoredActors)
		{
			if (IgnoredActor.IsValid())
			{
				RootPrimitive->IgnoreActorWhenMoving(IgnoredActor.Get(), false);
			}
		}
	}

	TemporarilyIgnoredActors.Reset();
}

void UPhaseCrashComponent::DetachFromCrashTarget()
{
	if (AttachedActor.IsValid())
	{
		if (UAnchorOverloadComponent* Overload =
				AttachedActor->FindComponentByClass<UAnchorOverloadComponent>())
		{
			Overload->NotifyPlayerDetached(OwnerPawn);
		}
	}

	AttachedActor.Reset();
	AttachedSurfaceComponent.Reset();
	AttachedResponseComponent.Reset();
	AttachedSurfaceOffset = 50.0f;
	AttachedSurfaceX = 0.0f;
	AttachedSurfaceY = 0.0f;
	AttachedSurfaceMinX = 0.0f;
	AttachedSurfaceMaxX = 0.0f;
	AttachedSurfaceMinY = 0.0f;
	AttachedSurfaceMaxY = 0.0f;
	AttachedBoxFace = EAttachBoxFace::PositiveX;
	AttachedBoxContactLocal = FVector::ZeroVector;
	bAttachCornerTransitionActive = false;
	AttachCornerTransitionElapsed = 0.0f;
}

void UPhaseCrashComponent::TickAttachCornerTransition(const float DeltaTime)
{
	UAttachSurfaceComponent* AttachBox =
		Cast<UAttachSurfaceComponent>(AttachedSurfaceComponent.Get());
	if (!AttachBox || !OwnerPawn || !OwnerPawn->GetRootComponent())
	{
		bAttachCornerTransitionActive = false;
		return;
	}

	AttachCornerTransitionElapsed += DeltaTime;
	const float Duration = FMath::Max(AttachCornerTransitionDuration, 0.01f);
	const float LinearAlpha = FMath::Clamp(
		AttachCornerTransitionElapsed / Duration,
		0.0f,
		1.0f);
	const float SmoothAlpha = FMath::SmoothStep(0.0f, 1.0f, LinearAlpha);

	const FTransform BoxTransform = AttachBox->GetComponentTransform();
	const FVector StartNormal =
		AttachBox->GetFaceNormalWorld(AttachCornerStartFace);
	const FVector EndNormal =
		AttachBox->GetFaceNormalWorld(AttachCornerEndFace);
	const FQuat QuarterTurn = FQuat::FindBetweenNormals(StartNormal, EndNormal);
	const FVector ArcNormal = FQuat::Slerp(
		FQuat::Identity,
		QuarterTurn,
		SmoothAlpha).RotateVector(StartNormal).GetSafeNormal();
	const FVector EdgeWorld =
		BoxTransform.TransformPosition(AttachCornerContactLocal);
	const FVector TargetPosition =
		EdgeWorld + ArcNormal * AttachedSurfaceOffset;

	FHitResult Hit;
	OwnerPawn->GetRootComponent()->MoveComponent(
		TargetPosition - OwnerPawn->GetActorLocation(),
		OwnerPawn->GetActorRotation(),
		true,
		&Hit);
	if (Hit.IsValidBlockingHit())
	{
		const FVector StartPosition =
			EdgeWorld + StartNormal * AttachedSurfaceOffset;
		OwnerPawn->GetRootComponent()->MoveComponent(
			StartPosition - OwnerPawn->GetActorLocation(),
			OwnerPawn->GetActorRotation(),
			false);
		bAttachCornerTransitionActive = false;
		AttachCornerTransitionElapsed = 0.0f;

		UE_LOG(
			LogRDCAPlayer,
			Warning,
			TEXT("Attach corner transition blocked. Box=%s Blocker=%s Component=%s"),
			*GetNameSafe(AttachBox),
			*GetNameSafe(Hit.GetActor()),
			*GetNameSafe(Hit.GetComponent()));
		return;
	}

	if (LinearAlpha < 1.0f)
	{
		return;
	}

	const FVector PreviousUp = BoxTransform.TransformVectorNoScale(
		AttachedSurfaceUpLocal).GetSafeNormal();
	FVector NextUp = QuarterTurn.RotateVector(PreviousUp);
	NextUp = FVector::VectorPlaneProject(NextUp, EndNormal).GetSafeNormal();
	if (NextUp.IsNearlyZero())
	{
		NextUp = FVector::VectorPlaneProject(
			FVector::UpVector,
			EndNormal).GetSafeNormal();
	}
	const FVector NextRight =
		FVector::CrossProduct(EndNormal, NextUp).GetSafeNormal();
	AttachedSurfaceOutLocal =
		BoxTransform.InverseTransformVectorNoScale(EndNormal).GetSafeNormal();
	AttachedSurfaceUpLocal =
		BoxTransform.InverseTransformVectorNoScale(NextUp).GetSafeNormal();
	AttachedSurfaceRightLocal =
		BoxTransform.InverseTransformVectorNoScale(NextRight).GetSafeNormal();
	AttachedBoxFace = AttachCornerEndFace;
	AttachedBoxContactLocal = AttachCornerContactLocal;
	bAttachCornerTransitionActive = false;
	AttachCornerTransitionElapsed = 0.0f;

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Attach box face transition completed. Box=%s From=%d To=%d"),
		*GetNameSafe(AttachBox),
		static_cast<int32>(AttachCornerStartFace),
		static_cast<int32>(AttachCornerEndFace));
}

bool UPhaseCrashComponent::MoveOnAttachBox(
	UAttachSurfaceComponent& AttachBox,
	const FVector2D& MovementInput,
	const float MovementStep)
{
	if (!OwnerPawn || !OwnerPawn->GetRootComponent())
	{
		return false;
	}

	const FTransform BoxTransform = AttachBox.GetComponentTransform();
	const FVector SurfaceUp = BoxTransform.TransformVectorNoScale(
		AttachedSurfaceUpLocal).GetSafeNormal();
	const FVector SurfaceRight = BoxTransform.TransformVectorNoScale(
		AttachedSurfaceRightLocal).GetSafeNormal();
	const FVector RequestedDirection =
		SurfaceRight * MovementInput.X + SurfaceUp * MovementInput.Y;
	if (RequestedDirection.IsNearlyZero() || MovementStep <= 0.0f)
	{
		return true;
	}

	const FVector CurrentContactWorld =
		BoxTransform.TransformPosition(AttachedBoxContactLocal);
	const FVector CandidateContactWorld =
		CurrentContactWorld + RequestedDirection.GetSafeNormal() * MovementStep;
	const FVector CandidateLocal =
		BoxTransform.InverseTransformPosition(CandidateContactWorld);
	const FVector Extent = AttachBox.GetUnscaledBoxExtent();
	const FVector CurrentNormalLocal =
		AttachBox.GetFaceNormalLocal(AttachedBoxFace);

	EAttachBoxFace NextFace = AttachedBoxFace;
	float LargestOverflow = 0.0f;
	auto ConsiderAxis = [&LargestOverflow, &NextFace](
		const float Coordinate,
		const float HalfExtent,
		const EAttachBoxFace PositiveFace,
		const EAttachBoxFace NegativeFace)
	{
		const float SafeExtent = FMath::Max(HalfExtent, 1.0f);
		const float PositiveOverflow = (Coordinate - HalfExtent) / SafeExtent;
		const float NegativeOverflow = (-HalfExtent - Coordinate) / SafeExtent;
		if (PositiveOverflow > LargestOverflow)
		{
			LargestOverflow = PositiveOverflow;
			NextFace = PositiveFace;
		}
		if (NegativeOverflow > LargestOverflow)
		{
			LargestOverflow = NegativeOverflow;
			NextFace = NegativeFace;
		}
	};

	if (FMath::Abs(CurrentNormalLocal.X) < 0.5f)
	{
		ConsiderAxis(
			CandidateLocal.X,
			Extent.X,
			EAttachBoxFace::PositiveX,
			EAttachBoxFace::NegativeX);
	}
	if (FMath::Abs(CurrentNormalLocal.Y) < 0.5f)
	{
		ConsiderAxis(
			CandidateLocal.Y,
			Extent.Y,
			EAttachBoxFace::PositiveY,
			EAttachBoxFace::NegativeY);
	}
	if (FMath::Abs(CurrentNormalLocal.Z) < 0.5f)
	{
		ConsiderAxis(
			CandidateLocal.Z,
			Extent.Z,
			EAttachBoxFace::PositiveZ,
			EAttachBoxFace::NegativeZ);
	}

	const bool bChangingFace = NextFace != AttachedBoxFace;
	const FVector NextContactLocal =
		AttachBox.ClampPointToFace(CandidateLocal, NextFace);
	const FVector NextNormalWorld = AttachBox.GetFaceNormalWorld(NextFace);
	const FVector NextContactWorld =
		BoxTransform.TransformPosition(NextContactLocal);
	const FVector NextPawnPosition =
		NextContactWorld + NextNormalWorld * AttachedSurfaceOffset;
	const FVector OriginalPawnPosition = OwnerPawn->GetActorLocation();

	if (bChangingFace)
	{
		AttachCornerStartFace = AttachedBoxFace;
		AttachCornerEndFace = NextFace;
		AttachCornerContactLocal = NextContactLocal;
		AttachCornerTransitionElapsed = 0.0f;
		bAttachCornerTransitionActive = true;
		return true;
	}

	FHitResult Hit;
	OwnerPawn->GetRootComponent()->MoveComponent(
		NextPawnPosition - OwnerPawn->GetActorLocation(),
		OwnerPawn->GetActorRotation(),
		true,
		&Hit);

	if (Hit.IsValidBlockingHit())
	{
		UE_LOG(
			LogRDCAPlayer,
			Warning,
			TEXT("Attach box movement blocked. Box=%s Face=%d NextFace=%d Blocker=%s Component=%s"),
			*GetNameSafe(&AttachBox),
			static_cast<int32>(AttachedBoxFace),
			static_cast<int32>(NextFace),
			*GetNameSafe(Hit.GetActor()),
			*GetNameSafe(Hit.GetComponent()));

		OwnerPawn->GetRootComponent()->MoveComponent(
			OriginalPawnPosition - OwnerPawn->GetActorLocation(),
			OwnerPawn->GetActorRotation(),
			false);
		return false;
	}

	AttachedBoxFace = NextFace;
	AttachedBoxContactLocal = NextContactLocal;
	return true;
}

void UPhaseCrashComponent::ConfigureAttachBoxSurface(
	UAttachSurfaceComponent& AttachBox,
	const FVector& WorldContactPoint,
	const FVector& WorldImpactNormal)
{
	AttachedSurfaceComponent = &AttachBox;
	AttachedBoxFace = AttachBox.GetFaceFromWorldNormal(WorldImpactNormal);
	const FTransform BoxTransform = AttachBox.GetComponentTransform();
	AttachedBoxContactLocal = AttachBox.ClampPointToFace(
		BoxTransform.InverseTransformPosition(WorldContactPoint),
		AttachedBoxFace);

	const FVector SurfaceNormal =
		AttachBox.GetFaceNormalWorld(AttachedBoxFace);
	FVector SurfaceUp = FVector::VectorPlaneProject(
		FVector::UpVector,
		SurfaceNormal).GetSafeNormal();
	if (SurfaceUp.IsNearlyZero())
	{
		SurfaceUp = FVector::VectorPlaneProject(
			AttachBox.GetForwardVector(),
			SurfaceNormal).GetSafeNormal();
	}
	if (SurfaceUp.IsNearlyZero())
	{
		SurfaceUp = FVector::VectorPlaneProject(
			AttachBox.GetRightVector(),
			SurfaceNormal).GetSafeNormal();
	}
	const FVector SurfaceRight =
		FVector::CrossProduct(SurfaceNormal, SurfaceUp).GetSafeNormal();

	AttachedSurfaceOutLocal =
		BoxTransform.InverseTransformVectorNoScale(SurfaceNormal).GetSafeNormal();
	AttachedSurfaceUpLocal =
		BoxTransform.InverseTransformVectorNoScale(SurfaceUp).GetSafeNormal();
	AttachedSurfaceRightLocal =
		BoxTransform.InverseTransformVectorNoScale(SurfaceRight).GetSafeNormal();
	AttachedSurfaceOffset = FMath::Max(
		FVector::DotProduct(
			OwnerPawn->GetActorLocation() - WorldContactPoint,
			SurfaceNormal),
		1.0f);
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
