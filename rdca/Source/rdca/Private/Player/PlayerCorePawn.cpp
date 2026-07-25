#include "Player/PlayerCorePawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Player/PhaseCrashComponent.h"
#include "rdca.h"
#include "UObject/ConstructorHelpers.h"

APlayerCorePawn::APlayerCorePawn()
{
	PrimaryActorTick.bCanEverTick = false;

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
}

void APlayerCorePawn::BeginPlay()
{
	Super::BeginPlay();

	MovementComponent->MaxSpeed = MoveSpeed;

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
}

void APlayerCorePawn::Move(const FInputActionValue& Value)
{
	const FVector2D MovementInput = Value.Get<FVector2D>();
	const float InputScale = PhaseCrashComponent
		? PhaseCrashComponent->GetMovementInputScale()
		: 1.0f;
	AddMovementInput(FVector::ForwardVector, MovementInput.Y * InputScale);
	AddMovementInput(FVector::RightVector, MovementInput.X * InputScale);
}

void APlayerCorePawn::StartCrashCharge()
{
	if (PhaseCrashComponent)
	{
		MovementComponent->StopMovementImmediately();
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
