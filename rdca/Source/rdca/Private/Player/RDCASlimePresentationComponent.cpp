#include "Player/RDCASlimePresentationComponent.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/PhaseCrashComponent.h"

URDCASlimePresentationComponent::URDCASlimePresentationComponent()
{
	// Pawn drives presentation then camera once, preserving the pre-refactor order.
	PrimaryComponentTick.bCanEverTick = false;
}

void URDCASlimePresentationComponent::InitializePresentation()
{
	Pawn = Cast<APlayerCorePawn>(GetOwner());
	if (!Pawn) { return; }
	BaseVisualScale = Pawn->VisualMesh
		? Pawn->VisualMesh->GetRelativeScale3D()
		: FVector::OneVector;
	BaseVisualLocation = Pawn->VisualMesh
		? Pawn->VisualMesh->GetRelativeLocation()
		: FVector::ZeroVector;
	BaseVisualRotation = Pawn->VisualMesh
		? Pawn->VisualMesh->GetRelativeRotation()
		: FRotator::ZeroRotator;
	if (Pawn->VisualMesh)
	{
		// material_dynamic (or whichever material occupies slot 0 in the
		// Blueprint) becomes a per-player instance. This does not replace the
		// authored material; it only allows the slime state to drive its WPO.
		SlimeMaterialInstance = Pawn->VisualMesh->CreateDynamicMaterialInstance(0);

		FVector LocalBoundsMin;
		FVector LocalBoundsMax;
		Pawn->VisualMesh->GetLocalBounds(LocalBoundsMin, LocalBoundsMax);
		const FVector BoundsOrigin =
			(LocalBoundsMin + LocalBoundsMax) * 0.5f * BaseVisualScale;
		const FVector BoundsExtent =
			(LocalBoundsMax - LocalBoundsMin) * 0.5f * BaseVisualScale.GetAbs();
		const FVector BottomAxis = Pawn->ModelBottomLocalAxis.GetSafeNormal();
		BaseVisualBottomDistance = FMath::Max(
			FVector::DotProduct(BoundsOrigin, BottomAxis)
				+ FVector::DotProduct(BoundsExtent, BottomAxis.GetAbs()),
			1.0f);
	}
	PreviousPresentationLocation = Pawn->GetActorLocation();
	LastFacingDirection = Pawn->GetActorForwardVector();
}

void URDCASlimePresentationComponent::SetSlimeState(const EPlayerSlimeState NewState)
{
	if (SlimeState == NewState)
	{
		return;
	}
	const EPlayerSlimeState PreviousState = SlimeState;
	SlimeState = NewState;
	SlimeStateElapsed = 0.0f;
	if (SlimeState == EPlayerSlimeState::Attached)
	{
		SurfaceImpactEnergy = 1.0f;
		PreviousAttachedNormal = FVector::ZeroVector;
	}
	if (PreviousState == EPlayerSlimeState::Dashing
		&& SlimeState != EPlayerSlimeState::Dashing)
	{
		DashReboundRemaining = Pawn->DashReboundDuration;
	}
	Pawn->OnSlimeStateChanged.Broadcast(PreviousState, SlimeState);
}

void URDCASlimePresentationComponent::UpdateSlimePresentation(const float DeltaTime)
{
	if (!Pawn || !Pawn->VisualMesh || !Pawn->PhaseCrashComponent)
	{
		return;
	}

	PresentationTime += DeltaTime;
	SlimeStateElapsed += DeltaTime;
	SurfaceImpactEnergy = FMath::Max(
		SurfaceImpactEnergy - DeltaTime * 3.5f,
		0.0f);
	DashReboundRemaining = FMath::Max(
		DashReboundRemaining - DeltaTime,
		0.0f);
	const FVector CurrentLocation = Pawn->GetActorLocation();
	FVector FrameVelocity = DeltaTime > UE_KINDA_SMALL_NUMBER
		? (CurrentLocation - PreviousPresentationLocation) / DeltaTime
		: FVector::ZeroVector;
	PreviousPresentationLocation = CurrentLocation;

	const EPhaseCrashState CrashState = Pawn->PhaseCrashComponent->GetCrashState();
	EPlayerSlimeState DesiredState = EPlayerSlimeState::Idle;
	if (CrashState == EPhaseCrashState::Charging)
	{
		DesiredState = EPlayerSlimeState::Charging;
	}
	else if (Pawn->PhaseCrashComponent->IsAttached())
	{
		DesiredState = EPlayerSlimeState::Attached;
	}
	else if (CrashState == EPhaseCrashState::Crashing)
	{
		DesiredState = Pawn->PhaseCrashComponent->IsGroundDashActive()
			? EPlayerSlimeState::Dashing
			: EPlayerSlimeState::Airborne;
	}
	else if (FVector(FrameVelocity.X, FrameVelocity.Y, 0.0f).Size()
		> Pawn->MovementStateSpeedThreshold)
	{
		DesiredState = EPlayerSlimeState::Moving;
	}
	SetSlimeState(DesiredState);

	FVector ShapeMultiplier = FVector::OneVector;
	float PitchTilt = 0.0f;
	float BackwardVisualOffset = 0.0f;
	float SurfacePressOffset = 0.0f;
	// Charging and attachment can coexist; support does not become world-up just because a skill starts.
	FVector DesiredUp = Pawn->PhaseCrashComponent->IsAttached()
		? Pawn->PhaseCrashComponent->GetAttachedSurfaceNormal() : FVector::UpVector;
	FVector DesiredFacing = LastFacingDirection;
	FVector HorizontalVelocity(FrameVelocity.X, FrameVelocity.Y, 0.0f);
	const float HorizontalSpeed = HorizontalVelocity.Size();
	if (HorizontalVelocity.Normalize())
	{
		DesiredFacing = HorizontalVelocity;
	}

	switch (SlimeState)
	{
	case EPlayerSlimeState::Idle:
	{
		const float PrimaryPulse = FMath::Sin(
			PresentationTime * Pawn->IdlePulseFrequency * 2.0f * PI)
			* Pawn->IdlePulseAmount;
		const float SecondaryPulse = FMath::Sin(
			PresentationTime * Pawn->IdlePulseFrequency * 4.3f * PI + 0.7f)
			* Pawn->IdlePulseAmount * 0.32f;
		ShapeMultiplier = FVector(
			1.16f + PrimaryPulse * 0.45f + SecondaryPulse,
			1.16f + PrimaryPulse - SecondaryPulse,
			0.72f - PrimaryPulse * 0.82f);
		break;
	}
	case EPlayerSlimeState::Moving:
	{
		// The leading side is compressed while the liquid mass visually lags
		// behind the collision body. Each compression/recovery cycle makes the
		// low-speed glide read as liquid motion instead of a static pose.
		const float MoveAlpha = FMath::Clamp(
			HorizontalSpeed / FMath::Max(
				Pawn->MoveSpeed * Pawn->MovementSpeedMultiplier, 1.0f),
			0.0f,
			1.0f);
		const float CrawlFrequency = FMath::Lerp(
			Pawn->LocomotionMinFrequency,
			Pawn->LocomotionMaxFrequency,
			MoveAlpha);
		LocomotionPhase += DeltaTime * CrawlFrequency * 2.0f * PI;
		const float CrawlWave = FMath::Sin(LocomotionPhase);
		const float CompressionAlpha = (CrawlWave + 1.0f) * 0.5f;
		const float Squirm = Pawn->LocomotionSquirmAmount * MoveAlpha;
		ShapeMultiplier = FVector(
			FMath::Lerp(0.94f, 0.84f, MoveAlpha)
				- CompressionAlpha * Squirm,
			FMath::Lerp(1.07f, 1.12f, MoveAlpha)
				+ CompressionAlpha * Squirm * 0.52f,
			FMath::Lerp(0.80f, 0.75f, MoveAlpha)
				- CompressionAlpha * Squirm * 0.58f);
		BackwardVisualOffset = Pawn->MovementTrailOffset * MoveAlpha
			* FMath::Lerp(0.65f, 1.15f, CompressionAlpha);
		SurfacePressOffset = Pawn->LocomotionPressOffset * MoveAlpha * CompressionAlpha;
		PitchTilt = 4.0f + CrawlWave * 1.8f;
		break;
	}
	case EPlayerSlimeState::Charging:
	{
		const float Charge = Pawn->PhaseCrashComponent->GetChargeAlpha();
		DesiredFacing = Pawn->PhaseCrashComponent->GetAimDirection();
		ShapeMultiplier = FVector(
			FMath::Lerp(0.90f, 0.68f, Charge),
			FMath::Lerp(1.10f, 1.24f, Charge),
			FMath::Lerp(0.76f, 0.58f, Charge));
		BackwardVisualOffset = Pawn->MaximumChargeRecoilOffset * Charge;
		PitchTilt = FMath::Lerp(3.0f, 10.0f, Charge);
		break;
	}
	case EPlayerSlimeState::Dashing:
	{
		DesiredFacing = Pawn->PhaseCrashComponent->GetActiveTravelDirection();
		const float Progress = Pawn->PhaseCrashComponent->GetActiveCrashProgress();
		const FVector StoredShape(0.68f, 1.26f, 0.68f);
		const FVector ReleasedShape(1.88f, 0.68f, 0.72f);
		const FVector ReboundShape(0.88f, 1.14f, 0.72f);
		if (Progress < 0.10f)
		{
			const float ReleaseAlpha = FMath::SmoothStep(
				0.0f,
				1.0f,
				Progress / 0.10f);
			ShapeMultiplier = FMath::Lerp(
				StoredShape,
				ReleasedShape,
				ReleaseAlpha);
			BackwardVisualOffset = FMath::Lerp(
				Pawn->MaximumChargeRecoilOffset * 0.65f,
				Pawn->MovementTrailOffset * 1.25f,
				ReleaseAlpha);
		}
		else if (Progress < 0.86f)
		{
			ShapeMultiplier = ReleasedShape;
			BackwardVisualOffset = Pawn->MovementTrailOffset * 1.25f;
		}
		else
		{
			const float ReboundAlpha = FMath::SmoothStep(
				0.0f,
				1.0f,
				(Progress - 0.86f) / 0.14f);
			ShapeMultiplier = FMath::Lerp(
				ReleasedShape,
				ReboundShape,
				ReboundAlpha);
			BackwardVisualOffset = FMath::Lerp(
				Pawn->MovementTrailOffset * 1.25f,
				0.0f,
				ReboundAlpha);
		}
		break;
	}
	case EPlayerSlimeState::Airborne:
	{
		DesiredFacing = Pawn->PhaseCrashComponent->GetActiveTravelDirection();
		const float Progress = Pawn->PhaseCrashComponent->GetActiveCrashProgress();
		if (Progress < 0.16f)
		{
			const float ReleaseAlpha = FMath::SmoothStep(
				0.0f,
				1.0f,
				Progress / 0.16f);
			ShapeMultiplier = FMath::Lerp(
				FVector(0.74f, 1.18f, 0.66f),
				FVector(1.34f, 0.86f, 0.94f),
				ReleaseAlpha);
		}
		else
		{
			const float FallStretch = FMath::Clamp(
				(Progress - 0.55f) / 0.45f,
				0.0f,
				1.0f);
			ShapeMultiplier = FMath::Lerp(
				FVector(1.12f, 0.92f, 1.02f),
				FVector(1.30f, 0.84f, 0.90f),
				FallStretch);
		}
		PitchTilt = DesiredFacing.Z * 18.0f;
		break;
	}
	case EPlayerSlimeState::Attached:
	{
		DesiredUp = Pawn->PhaseCrashComponent->GetAttachedSurfaceNormal();
		if (!PreviousAttachedNormal.IsNearlyZero()
			&& FVector::DotProduct(PreviousAttachedNormal, DesiredUp) < 0.96f)
		{
			// A face transition is a new directional contact. Re-spread the
			// liquid against the new plane, but less violently than first impact.
			SurfaceImpactEnergy = FMath::Max(SurfaceImpactEnergy, 0.65f);
		}
		PreviousAttachedNormal = DesiredUp;
		FVector SurfaceVelocity = FVector::VectorPlaneProject(
			FrameVelocity,
			DesiredUp);
		const float SurfaceSpeed = SurfaceVelocity.Size();
		if (SurfaceVelocity.Normalize())
		{
			DesiredFacing = SurfaceVelocity;
		}
		const float MoveAlpha = FMath::Clamp(
			SurfaceSpeed / FMath::Max(
				Pawn->MoveSpeed * Pawn->MovementSpeedMultiplier, 1.0f),
			0.0f,
			1.0f);
		const float SuctionPulse =
			FMath::Sin(SlimeStateElapsed * 5.0f) * 0.015f;
		const float CrawlFrequency = FMath::Lerp(
			Pawn->LocomotionMinFrequency,
			Pawn->LocomotionMaxFrequency,
			MoveAlpha);
		LocomotionPhase += DeltaTime * CrawlFrequency * 2.0f * PI;
		const float CompressionAlpha =
			(FMath::Sin(LocomotionPhase) + 1.0f) * 0.5f;
		const float SurfaceSquirm = Pawn->LocomotionSquirmAmount * MoveAlpha;
		ShapeMultiplier = FVector(
			FMath::Lerp(1.18f, 0.88f, MoveAlpha)
				- CompressionAlpha * SurfaceSquirm
				+ SurfaceImpactEnergy * 0.22f + SuctionPulse,
			1.18f + CompressionAlpha * SurfaceSquirm * 0.50f
				+ SurfaceImpactEnergy * 0.22f + SuctionPulse,
			0.70f - CompressionAlpha * SurfaceSquirm * 0.55f
				- SurfaceImpactEnergy * 0.18f - SuctionPulse);
		BackwardVisualOffset = Pawn->MovementTrailOffset * MoveAlpha
			* FMath::Lerp(0.65f, 1.12f, CompressionAlpha);
		SurfacePressOffset = Pawn->LocomotionPressOffset * MoveAlpha * CompressionAlpha;
		break;
	}
	}

	if (SlimeState != EPlayerSlimeState::Dashing
		&& DashReboundRemaining > 0.0f)
	{
		const float ReboundAlpha = Pawn->DashReboundDuration > UE_KINDA_SMALL_NUMBER
			? DashReboundRemaining / Pawn->DashReboundDuration
			: 0.0f;
		const float ReboundWave =
			FMath::Sin((1.0f - ReboundAlpha) * PI) * ReboundAlpha;
		ShapeMultiplier.X *= 1.0f - ReboundWave * 0.18f;
		ShapeMultiplier.Y *= 1.0f + ReboundWave * 0.16f;
		ShapeMultiplier.Z *= 1.0f - ReboundWave * 0.12f;
	}

	// A rebound is not a second voluntary dash. Briefly squash the liquid
	// against the collision direction, then spring it into the reflected path.
	// The gameplay trajectory remains wholly owned by Pawn->PhaseCrashComponent.
	const float ReboundPresentationAlpha =
		Pawn->PhaseCrashComponent->GetReboundPresentationAlpha();
	if (ReboundPresentationAlpha > 0.0f)
	{
		const float ReboundElapsedAlpha = 1.0f - ReboundPresentationAlpha;
		const float EjectionAlpha = FMath::SmoothStep(
			0.0f,
			1.0f,
			ReboundElapsedAlpha / 0.42f);
		const FVector ImpactSquashShape(0.58f, 1.34f, 0.58f);
		const FVector EjectionShape(1.58f, 0.80f, 0.82f);
		ShapeMultiplier = FMath::Lerp(
			ImpactSquashShape,
			EjectionShape,
			EjectionAlpha);
		BackwardVisualOffset = FMath::Lerp(
			Pawn->MovementTrailOffset * 0.65f,
			Pawn->MovementTrailOffset * 1.45f,
			EjectionAlpha);
	}

	if (DesiredFacing.IsNearlyZero())
	{
		DesiredFacing = LastFacingDirection;
	}
	DesiredFacing = FVector::VectorPlaneProject(DesiredFacing, DesiredUp).GetSafeNormal();
	if (DesiredFacing.IsNearlyZero())
	{
		DesiredFacing = FVector::VectorPlaneProject(LastFacingDirection, DesiredUp)
			.GetSafeNormal();
	}
	if (DesiredFacing.IsNearlyZero())
	{
		// A head-on wall hit has no tangent velocity and the old facing can be parallel to the normal.
		// Pick a stable tangent instead of passing a degenerate pair of axes into the model basis.
		const FVector Reference = FMath::Abs(DesiredUp.Z) < 0.99f ? FVector::UpVector : FVector::ForwardVector;
		DesiredFacing = FVector::VectorPlaneProject(Reference, DesiredUp).GetSafeNormal();
	}
	if (!DesiredFacing.IsNearlyZero())
	{
		LastFacingDirection = DesiredFacing;
	}

	const FVector DesiredScale = BaseVisualScale * ShapeMultiplier;
	const float ActiveTransformInterpSpeed =
		SlimeState == EPlayerSlimeState::Dashing
			? Pawn->DashTransformInterpSpeed
			: Pawn->SlimeTransformInterpSpeed;
	Pawn->VisualMesh->SetRelativeScale3D(FMath::VInterpTo(
		Pawn->VisualMesh->GetRelativeScale3D(),
		DesiredScale,
		DeltaTime,
		ActiveTransformInterpSpeed));

	const FVector DesiredWorldLocation =
		Pawn->CollisionComponent->GetComponentTransform().TransformPosition(BaseVisualLocation)
		- LastFacingDirection * BackwardVisualOffset
		- DesiredUp * (
			(SlimeState == EPlayerSlimeState::Attached
				? BaseVisualBottomDistance * (1.0f - ShapeMultiplier.Z)
				: 0.0f)
			+ SurfacePressOffset);
	Pawn->VisualMesh->SetWorldLocation(FMath::VInterpTo(
		Pawn->VisualMesh->GetComponentLocation(),
		DesiredWorldLocation,
		DeltaTime,
		ActiveTransformInterpSpeed));

	FRotator PresentationRotation = FRotationMatrix::MakeFromXZ(
		LastFacingDirection,
		DesiredUp).Rotator();
	if (Pawn->PhaseCrashComponent->IsAttached())
	{
		// Correct imported forward around the support axis, not world Z: wall attachment must keep its bottom.
		PresentationRotation = (FQuat(DesiredUp, FMath::DegreesToRadians(Pawn->VisualForwardYawOffset))
			* PresentationRotation.Quaternion()).Rotator();
	}
	else
	{
		PresentationRotation.Yaw += Pawn->VisualForwardYawOffset;
		PresentationRotation.Pitch += PitchTilt;
	}
	if (SlimeState == EPlayerSlimeState::Idle)
	{
		PresentationRotation.Roll +=
			FMath::Sin(PresentationTime * 2.2f) * Pawn->IdleWobbleAmount;
	}
	FVector EffectiveModelUp = BaseVisualRotation.RotateVector(
		-Pawn->ModelBottomLocalAxis.GetSafeNormal());
	FVector EffectiveModelForward = BaseVisualRotation.RotateVector(
		Pawn->ModelForwardLocalAxis.GetSafeNormal());
	EffectiveModelForward = FVector::VectorPlaneProject(
		EffectiveModelForward,
		EffectiveModelUp).GetSafeNormal();
	if (EffectiveModelUp.IsNearlyZero())
	{
		EffectiveModelUp = FVector::UpVector;
	}
	if (EffectiveModelForward.IsNearlyZero())
	{
		EffectiveModelForward = FVector::ForwardVector;
	}
	const FQuat ModelBasis = FRotationMatrix::MakeFromXZ(
		EffectiveModelForward,
		EffectiveModelUp).ToQuat();
	const FRotator DesiredRotation =
		(PresentationRotation.Quaternion() * ModelBasis.Inverse()).Rotator();
	Pawn->VisualMesh->SetWorldRotation(FMath::RInterpTo(
		Pawn->VisualMesh->GetComponentRotation(),
		DesiredRotation,
		DeltaTime,
		Pawn->SlimeFacingInterpSpeed));

	// The material measures each vertex from ObjectPositionWS (the mesh bounds
	// centre). Give it the distance from that centre to the original support-side
	// bound, so WPO can flatten onto that plane without pushing vertices through it.
	FVector VisualLocalBoundsMin;
	FVector VisualLocalBoundsMax;
	Pawn->VisualMesh->GetLocalBounds(VisualLocalBoundsMin, VisualLocalBoundsMax);
	const FVector VisualLocalExtent =
		(VisualLocalBoundsMax - VisualLocalBoundsMin) * 0.5f;
	const FVector VisualScale = Pawn->VisualMesh->GetComponentScale().GetAbs();
	const FVector SupportNormalLocal =
		Pawn->VisualMesh->GetComponentTransform()
			.InverseTransformVectorNoScale(DesiredUp)
			.GetSafeNormal();
	const float OriginalBottomDepth = FMath::Max(
		FVector::DotProduct(
			VisualLocalExtent * VisualScale,
			SupportNormalLocal.GetAbs()),
		1.0f);

	const FVector MaterialMotionVelocity = SlimeState == EPlayerSlimeState::Attached
		? FVector::VectorPlaneProject(FrameVelocity, DesiredUp)
		: HorizontalVelocity;
	const float MaterialMovementAlpha = FMath::Clamp(
		MaterialMotionVelocity.Size() / FMath::Max(
			Pawn->MoveSpeed * Pawn->MovementSpeedMultiplier, 1.0f),
		0.0f,
		1.0f);
	UpdateSlimeMaterialParameters(
		MaterialMovementAlpha,
		LastFacingDirection,
		DesiredUp,
		OriginalBottomDepth);
}

void URDCASlimePresentationComponent::UpdateSlimeMaterialParameters(
	const float MovementAlpha,
	const FVector& FlowDirection,
	const FVector& SupportNormal,
	const float OriginalBottomDepth)
{
	if (!SlimeMaterialInstance)
	{
		return;
	}

	// These names are deliberately stable: build matching Scalar/Vector
	// Parameters in material_dynamic, then use them to offset mesh vertices
	// through World Position Offset. Values are visual only: collision remains
	// controlled by Pawn->CollisionComponent.
	SlimeMaterialInstance->SetScalarParameterValue(
		TEXT("SlimeMoveAmount"), MovementAlpha);
	SlimeMaterialInstance->SetScalarParameterValue(
		TEXT("SlimeChargeAmount"),
		SlimeState == EPlayerSlimeState::Charging
			? Pawn->PhaseCrashComponent->GetChargeAlpha()
			: 0.0f);
	SlimeMaterialInstance->SetScalarParameterValue(
		TEXT("SlimeDashAmount"),
		SlimeState == EPlayerSlimeState::Dashing ? 1.0f : 0.0f);
	SlimeMaterialInstance->SetScalarParameterValue(
		TEXT("SlimeAttachAmount"),
		SlimeState == EPlayerSlimeState::Attached ? 1.0f : 0.0f);
	SlimeMaterialInstance->SetScalarParameterValue(
		TEXT("SlimeSurfaceContactAmount"),
		SlimeState == EPlayerSlimeState::Airborne ? 0.0f : 1.0f);
	SlimeMaterialInstance->SetScalarParameterValue(
		TEXT("SlimeRestAmount"),
		SlimeState == EPlayerSlimeState::Idle ? 1.0f : 0.0f);
	SlimeMaterialInstance->SetScalarParameterValue(
		TEXT("SlimeFlowPhase"), LocomotionPhase);
	SlimeMaterialInstance->SetScalarParameterValue(
		TEXT("SlimeImpactAmount"), SurfaceImpactEnergy);
	SlimeMaterialInstance->SetScalarParameterValue(
		TEXT("SlimeBottomLimitDepth"), OriginalBottomDepth);
	SlimeMaterialInstance->SetVectorParameterValue(
		TEXT("SlimeFlowDirection"), FLinearColor(FlowDirection));
	SlimeMaterialInstance->SetVectorParameterValue(
		TEXT("SlimeSupportNormal"), FLinearColor(SupportNormal));
	SlimeMaterialInstance->SetVectorParameterValue(
		TEXT("SlimeDownDirection"), FLinearColor(-SupportNormal));
}
