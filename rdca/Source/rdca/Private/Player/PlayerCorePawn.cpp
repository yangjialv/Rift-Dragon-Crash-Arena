#include "Player/PlayerCorePawn.h"

#include "Boss/BossEncounterComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Player/PhaseCrashComponent.h"
#include "Player/PlayerHealthComponent.h"
#include "rdca.h"
#include "UObject/ConstructorHelpers.h"

APlayerCorePawn::APlayerCorePawn()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(CollisionComponent);
	CollisionComponent->InitSphereRadius(50.0f);
	CollisionComponent->SetCollisionProfileName(TEXT("Pawn"));

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(CollisionComponent);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		VisualMesh->SetStaticMesh(SphereMesh.Object);
		VisualMesh->SetRelativeScale3D(FVector(1.0f));
	}

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(CollisionComponent);
	SpringArm->TargetArmLength = 1200.0f;
	SpringArm->SetRelativeRotation(FRotator(-55.0f, -45.0f, 0.0f));
	SpringArm->bDoCollisionTest = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);

	MovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	MovementComponent->UpdatedComponent = CollisionComponent;
	MovementComponent->MaxSpeed = MoveSpeed;
	MovementComponent->Acceleration = 4000.0f;
	MovementComponent->Deceleration = 5000.0f;

	PhaseCrashComponent = CreateDefaultSubobject<UPhaseCrashComponent>(TEXT("PhaseCrash"));
	HealthComponent = CreateDefaultSubobject<UPlayerHealthComponent>(TEXT("Health"));
}

void APlayerCorePawn::BeginPlay()
{
	Super::BeginPlay();

	MovementComponent->MaxSpeed = MoveSpeed;
	BaseVisualScale = VisualMesh
		? VisualMesh->GetRelativeScale3D()
		: FVector::OneVector;
	BaseVisualLocation = VisualMesh
		? VisualMesh->GetRelativeLocation()
		: FVector::ZeroVector;
	BaseVisualRotation = VisualMesh
		? VisualMesh->GetRelativeRotation()
		: FRotator::ZeroRotator;
	if (VisualMesh)
	{
		FVector LocalBoundsMin;
		FVector LocalBoundsMax;
		VisualMesh->GetLocalBounds(LocalBoundsMin, LocalBoundsMax);
		const FVector BoundsOrigin =
			(LocalBoundsMin + LocalBoundsMax) * 0.5f * BaseVisualScale;
		const FVector BoundsExtent =
			(LocalBoundsMax - LocalBoundsMin) * 0.5f * BaseVisualScale.GetAbs();
		const FVector BottomAxis = ModelBottomLocalAxis.GetSafeNormal();
		BaseVisualBottomDistance = FMath::Max(
			FVector::DotProduct(BoundsOrigin, BottomAxis)
				+ FVector::DotProduct(BoundsExtent, BottomAxis.GetAbs()),
			1.0f);
	}
	PreviousPresentationLocation = GetActorLocation();
	LastFacingDirection = GetActorForwardVector();
	FindBossCameraTarget();

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);

	if (InputSubsystem && PlayerMappingContext)
	{
		InputSubsystem->AddMappingContext(PlayerMappingContext, 0);
	}
	else if (!PlayerMappingContext)
	{
		UE_LOG(LogRDCAPlayer, Warning, TEXT("PlayerCorePawn: PlayerMappingContext is not assigned."));
	}
}

void APlayerCorePawn::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateSlimePresentation(DeltaTime);
	UpdateCombatCamera(DeltaTime);
}

void APlayerCorePawn::SetSlimeState(const EPlayerSlimeState NewState)
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
		DashReboundRemaining = DashReboundDuration;
	}
	OnSlimeStateChanged.Broadcast(PreviousState, SlimeState);
}

void APlayerCorePawn::UpdateSlimePresentation(const float DeltaTime)
{
	if (!VisualMesh || !PhaseCrashComponent)
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
	const FVector CurrentLocation = GetActorLocation();
	FVector FrameVelocity = DeltaTime > UE_KINDA_SMALL_NUMBER
		? (CurrentLocation - PreviousPresentationLocation) / DeltaTime
		: FVector::ZeroVector;
	PreviousPresentationLocation = CurrentLocation;

	const EPhaseCrashState CrashState = PhaseCrashComponent->GetCrashState();
	EPlayerSlimeState DesiredState = EPlayerSlimeState::Idle;
	if (CrashState == EPhaseCrashState::Charging)
	{
		DesiredState = EPlayerSlimeState::Charging;
	}
	else if (PhaseCrashComponent->IsAttached())
	{
		DesiredState = EPlayerSlimeState::Attached;
	}
	else if (CrashState == EPhaseCrashState::Crashing)
	{
		DesiredState = PhaseCrashComponent->IsGroundDashActive()
			? EPlayerSlimeState::Dashing
			: EPlayerSlimeState::Airborne;
	}
	else if (FVector(FrameVelocity.X, FrameVelocity.Y, 0.0f).Size()
		> MovementStateSpeedThreshold)
	{
		DesiredState = EPlayerSlimeState::Moving;
	}
	SetSlimeState(DesiredState);

	FVector ShapeMultiplier = FVector::OneVector;
	float PitchTilt = 0.0f;
	float BackwardVisualOffset = 0.0f;
	FVector DesiredUp = FVector::UpVector;
	FVector DesiredFacing = LastFacingDirection;
	FVector HorizontalVelocity(FrameVelocity.X, FrameVelocity.Y, 0.0f);
	if (HorizontalVelocity.Normalize())
	{
		DesiredFacing = HorizontalVelocity;
	}

	switch (SlimeState)
	{
	case EPlayerSlimeState::Idle:
	{
		const float Pulse = FMath::Sin(PresentationTime * 3.0f) * 0.018f;
		ShapeMultiplier = FVector(
			1.16f + Pulse,
			1.16f + Pulse,
			0.72f - Pulse);
		break;
	}
	case EPlayerSlimeState::Moving:
		// The leading side is compressed while the liquid mass visually lags
		// behind the collision body. Dash is the state that stretches forward.
		ShapeMultiplier = FVector(0.84f, 1.10f, 0.78f);
		BackwardVisualOffset = MovementTrailOffset;
		PitchTilt = 6.0f;
		break;
	case EPlayerSlimeState::Charging:
	{
		const float Charge = PhaseCrashComponent->GetChargeAlpha();
		DesiredFacing = PhaseCrashComponent->GetAimDirection();
		ShapeMultiplier = FVector(
			FMath::Lerp(0.90f, 0.68f, Charge),
			FMath::Lerp(1.10f, 1.24f, Charge),
			FMath::Lerp(0.76f, 0.58f, Charge));
		BackwardVisualOffset = MaximumChargeRecoilOffset * Charge;
		PitchTilt = FMath::Lerp(3.0f, 10.0f, Charge);
		break;
	}
	case EPlayerSlimeState::Dashing:
	{
		DesiredFacing = PhaseCrashComponent->GetActiveTravelDirection();
		const float Progress = PhaseCrashComponent->GetActiveCrashProgress();
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
				MaximumChargeRecoilOffset * 0.65f,
				MovementTrailOffset * 1.25f,
				ReleaseAlpha);
		}
		else if (Progress < 0.86f)
		{
			ShapeMultiplier = ReleasedShape;
			BackwardVisualOffset = MovementTrailOffset * 1.25f;
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
				MovementTrailOffset * 1.25f,
				0.0f,
				ReboundAlpha);
		}
		break;
	}
	case EPlayerSlimeState::Airborne:
	{
		DesiredFacing = PhaseCrashComponent->GetActiveTravelDirection();
		const float Progress = PhaseCrashComponent->GetActiveCrashProgress();
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
		DesiredUp = PhaseCrashComponent->GetAttachedSurfaceNormal();
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
			SurfaceSpeed / FMath::Max(MoveSpeed, 1.0f),
			0.0f,
			1.0f);
		const float SuctionPulse =
			FMath::Sin(SlimeStateElapsed * 5.0f) * 0.015f;
		ShapeMultiplier = FVector(
			FMath::Lerp(1.18f, 0.88f, MoveAlpha)
				+ SurfaceImpactEnergy * 0.22f + SuctionPulse,
			1.18f + SurfaceImpactEnergy * 0.22f + SuctionPulse,
			0.70f - SurfaceImpactEnergy * 0.18f - SuctionPulse);
		BackwardVisualOffset = MovementTrailOffset * MoveAlpha;
		break;
	}
	}

	if (SlimeState != EPlayerSlimeState::Dashing
		&& DashReboundRemaining > 0.0f)
	{
		const float ReboundAlpha = DashReboundDuration > UE_KINDA_SMALL_NUMBER
			? DashReboundRemaining / DashReboundDuration
			: 0.0f;
		const float ReboundWave =
			FMath::Sin((1.0f - ReboundAlpha) * PI) * ReboundAlpha;
		ShapeMultiplier.X *= 1.0f - ReboundWave * 0.18f;
		ShapeMultiplier.Y *= 1.0f + ReboundWave * 0.16f;
		ShapeMultiplier.Z *= 1.0f - ReboundWave * 0.12f;
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
	if (!DesiredFacing.IsNearlyZero())
	{
		LastFacingDirection = DesiredFacing;
	}

	const FVector DesiredScale = BaseVisualScale * ShapeMultiplier;
	const float ActiveTransformInterpSpeed =
		SlimeState == EPlayerSlimeState::Dashing
			? DashTransformInterpSpeed
			: SlimeTransformInterpSpeed;
	VisualMesh->SetRelativeScale3D(FMath::VInterpTo(
		VisualMesh->GetRelativeScale3D(),
		DesiredScale,
		DeltaTime,
		ActiveTransformInterpSpeed));

	const FVector DesiredWorldLocation =
		CollisionComponent->GetComponentTransform().TransformPosition(BaseVisualLocation)
		- LastFacingDirection * BackwardVisualOffset
		- DesiredUp * (SlimeState == EPlayerSlimeState::Attached
			? BaseVisualBottomDistance * (1.0f - ShapeMultiplier.Z)
			: 0.0f);
	VisualMesh->SetWorldLocation(FMath::VInterpTo(
		VisualMesh->GetComponentLocation(),
		DesiredWorldLocation,
		DeltaTime,
		ActiveTransformInterpSpeed));

	FRotator PresentationRotation = FRotationMatrix::MakeFromXZ(
		LastFacingDirection,
		DesiredUp).Rotator();
	PresentationRotation.Yaw += VisualForwardYawOffset;
	PresentationRotation.Pitch += PitchTilt;
	if (SlimeState == EPlayerSlimeState::Idle)
	{
		PresentationRotation.Roll +=
			FMath::Sin(PresentationTime * 2.2f) * IdleWobbleAmount;
	}
	FVector EffectiveModelUp = BaseVisualRotation.RotateVector(
		-ModelBottomLocalAxis.GetSafeNormal());
	FVector EffectiveModelForward = BaseVisualRotation.RotateVector(
		ModelForwardLocalAxis.GetSafeNormal());
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
	VisualMesh->SetWorldRotation(FMath::RInterpTo(
		VisualMesh->GetComponentRotation(),
		DesiredRotation,
		DeltaTime,
		SlimeFacingInterpSpeed));
}

void APlayerCorePawn::FindBossCameraTarget()
{
	BossCameraTarget.Reset();
	if (!GetWorld())
	{
		return;
	}

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (IsValid(Candidate)
			&& Candidate->FindComponentByClass<UBossEncounterComponent>())
		{
			BossCameraTarget = Candidate;
			return;
		}
	}
}

void APlayerCorePawn::UpdateCombatCamera(const float DeltaTime)
{
	if (!SpringArm)
	{
		return;
	}
	if (!BossCameraTarget.IsValid())
	{
		FindBossCameraTarget();
	}

	FVector DesiredTargetOffset(0.0f, 0.0f, CameraFocusHeight);
	float DesiredArmLength = MinimumCameraArmLength;
	if (BossCameraTarget.IsValid())
	{
		const FVector PlayerLocation = GetActorLocation();
		const FVector BossLocation = BossCameraTarget->GetActorLocation();
		const FVector PlayerToBoss = BossLocation - PlayerLocation;
		FVector ToBoss = PlayerToBoss;
		ToBoss.Z = 0.0f;
		const float PlayerBossDistance = ToBoss.Size();
		const float ZoomAlpha = FMath::Clamp(
			PlayerBossDistance / FMath::Max(ArenaRadiusForMaximumZoom, 100.0f),
			0.0f,
			1.0f);
		DesiredTargetOffset += PlayerToBoss * FMath::Clamp(
			BossFramingWeight,
			0.0f,
			0.5f);
		if (ToBoss.Normalize())
		{
			const FRotator DesiredRotation(
				FMath::Lerp(
					CombatCameraPitch,
					MaximumDistanceCameraPitch,
					ZoomAlpha),
				ToBoss.Rotation().Yaw,
				0.0f);
			SpringArm->SetWorldRotation(FMath::RInterpTo(
				SpringArm->GetComponentRotation(),
				DesiredRotation,
				DeltaTime,
				CameraRotationInterpSpeed));
		}

		DesiredArmLength = FMath::Clamp(
			MinimumCameraArmLength
				+ PlayerBossDistance * CameraArmLengthPerBossDistance,
			MinimumCameraArmLength,
			FMath::Max(MaximumCameraArmLength, MinimumCameraArmLength));
	}
	else
	{
		const FRotator FallbackRotation(
			CombatCameraPitch,
			-45.0f,
			0.0f);
		SpringArm->SetWorldRotation(FMath::RInterpTo(
			SpringArm->GetComponentRotation(),
			FallbackRotation,
			DeltaTime,
			CameraRotationInterpSpeed));
	}

	const float FramingInterpSpeed =
		DesiredArmLength > SpringArm->TargetArmLength
			? FMath::Max(CameraFollowInterpSpeed * 2.0f, 8.0f)
			: CameraFollowInterpSpeed;
	SpringArm->TargetOffset = FMath::VInterpTo(
		SpringArm->TargetOffset,
		DesiredTargetOffset,
		DeltaTime,
		FramingInterpSpeed);
	SpringArm->TargetArmLength = FMath::FInterpTo(
		SpringArm->TargetArmLength,
		DesiredArmLength,
		DeltaTime,
		FramingInterpSpeed);
}

void APlayerCorePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent =
		CastChecked<UEnhancedInputComponent>(PlayerInputComponent);

	if (MoveAction)
	{
		EnhancedInputComponent->BindAction(
			MoveAction,
			ETriggerEvent::Triggered,
			this,
			&APlayerCorePawn::Move);
	}
	else
	{
		UE_LOG(LogRDCAPlayer, Warning, TEXT("PlayerCorePawn: MoveAction is not assigned."));
	}

	if (CrashAction)
	{
		EnhancedInputComponent->BindAction(
			CrashAction,
			ETriggerEvent::Started,
			this,
			&APlayerCorePawn::StartCrashCharge);
		EnhancedInputComponent->BindAction(
			CrashAction,
			ETriggerEvent::Completed,
			this,
			&APlayerCorePawn::ReleaseCrash);
		EnhancedInputComponent->BindAction(
			CrashAction,
			ETriggerEvent::Canceled,
			this,
			&APlayerCorePawn::CancelCrashCharge);
	}
	else
	{
		UE_LOG(LogRDCAPlayer, Warning, TEXT("PlayerCorePawn: CrashAction is not assigned."));
	}

	if (DashAction)
	{
		EnhancedInputComponent->BindAction(
			DashAction,
			ETriggerEvent::Started,
			this,
			&APlayerCorePawn::StartGroundDash);
	}
	else
	{
		UE_LOG(LogRDCAPlayer, Warning, TEXT("PlayerCorePawn: DashAction is not assigned."));
	}
}

void APlayerCorePawn::Move(const FInputActionValue& Value)
{
	const FVector2D MovementInput = Value.Get<FVector2D>();
	if (PhaseCrashComponent && PhaseCrashComponent->IsAttached())
	{
		PhaseCrashComponent->MoveAttached(MovementInput);
		return;
	}

	const float InputScale = PhaseCrashComponent
		? PhaseCrashComponent->GetMovementInputScale()
		: 1.0f;
	FVector CameraForward = Camera
		? Camera->GetForwardVector()
		: FVector::ForwardVector;
	FVector CameraRight = Camera
		? Camera->GetRightVector()
		: FVector::RightVector;
	CameraForward.Z = 0.0f;
	CameraRight.Z = 0.0f;
	CameraForward.Normalize();
	CameraRight.Normalize();
	AddMovementInput(CameraForward, MovementInput.Y * InputScale);
	AddMovementInput(CameraRight, MovementInput.X * InputScale);
}

void APlayerCorePawn::StartCrashCharge()
{
	if (PhaseCrashComponent)
	{
		PhaseCrashComponent->StartCharging();
	}
}

void APlayerCorePawn::ReleaseCrash()
{
	if (PhaseCrashComponent)
	{
		MovementComponent->StopMovementImmediately();
		PhaseCrashComponent->ReleaseCrash();
	}
}

void APlayerCorePawn::CancelCrashCharge()
{
	if (PhaseCrashComponent)
	{
		PhaseCrashComponent->CancelCharging();
	}
}

void APlayerCorePawn::StartGroundDash()
{
	if (PhaseCrashComponent)
	{
		PhaseCrashComponent->StartGroundDash();
	}
}
