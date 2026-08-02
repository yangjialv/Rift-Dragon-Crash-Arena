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
	UpdateCombatCamera(DeltaTime);
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
