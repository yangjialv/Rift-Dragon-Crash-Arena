#include "Player/PlayerCorePawn.h"

#include "Abilities/RDCAAbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"
#include "Player/PhaseCrashComponent.h"
#include "Player/PlayerHealthComponent.h"
#include "Player/RDCACombatCameraComponent.h"
#include "Player/RDCAPlayerControlComponent.h"
#include "Player/RDCASlimePresentationComponent.h"
#include "UObject/ConstructorHelpers.h"

APlayerCorePawn::APlayerCorePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	AbilitySystemComponent = CreateDefaultSubobject<URDCAAbilitySystemComponent>(TEXT("AbilitySystem"));
	FarCameraPoint.ArmLength = 3000.0f;
	FarCameraPoint.Pitch = -36.0f;
	FarCameraPoint.FocusHeight = 170.0f;
	FarCameraPoint.BossFramingWeight = 0.36f;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(CollisionComponent);
	CollisionComponent->InitSphereRadius(50.0f);
	CollisionComponent->SetCollisionProfileName(TEXT("Pawn"));
	// Chaos debris settles against WorldStatic but must never block this Pawn.
	CollisionComponent->SetCollisionResponseToChannel(ECC_Destructible, ECR_Ignore);

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
	MovementComponent->MaxSpeed = MoveSpeed * MovementSpeedMultiplier;
	MovementComponent->Acceleration = 4000.0f;
	MovementComponent->Deceleration = 5000.0f;

	PhaseCrashComponent = CreateDefaultSubobject<UPhaseCrashComponent>(TEXT("PhaseCrash"));
	HealthComponent = CreateDefaultSubobject<UPlayerHealthComponent>(TEXT("Health"));
	PlayerControl = CreateDefaultSubobject<URDCAPlayerControlComponent>(TEXT("PlayerControl"));
	SlimePresentation = CreateDefaultSubobject<URDCASlimePresentationComponent>(TEXT("SlimePresentation"));
	CombatCamera = CreateDefaultSubobject<URDCACombatCameraComponent>(TEXT("CombatCamera"));
}

UAbilitySystemComponent* APlayerCorePawn::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void APlayerCorePawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	AbilitySystemComponent->InitializeForOwner();
	AbilitySystemComponent->RefreshAbilityActorInfo();
	PlayerControl->RefreshMappingContext();
}

void APlayerCorePawn::UnPossessed()
{
	PlayerControl->ResetControl();
	Super::UnPossessed();
	AbilitySystemComponent->RefreshAbilityActorInfo();
}

void APlayerCorePawn::BeginPlay()
{
	Super::BeginPlay();
	// Preserve serialized Blueprint collision/movement tuning.
	CollisionComponent->SetCollisionResponseToChannel(ECC_Destructible, ECR_Ignore);
	MovementComponent->MaxSpeed = MoveSpeed * FMath::Max(MovementSpeedMultiplier, 0.1f);
	SlimePresentation->InitializePresentation();
	CombatCamera->InitializeCamera();
	PlayerControl->RefreshMappingContext();
}

void APlayerCorePawn::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	// A single clock preserves the original visual/camera update order.
	SlimePresentation->UpdateSlimePresentation(DeltaTime);
	CombatCamera->UpdateCombatCamera(DeltaTime);
}

EPlayerSlimeState APlayerCorePawn::GetSlimeState() const
{
	return SlimePresentation ? SlimePresentation->GetSlimeState() : EPlayerSlimeState::Idle;
}

void APlayerCorePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PlayerControl->BindInputs(PlayerInputComponent);
}
