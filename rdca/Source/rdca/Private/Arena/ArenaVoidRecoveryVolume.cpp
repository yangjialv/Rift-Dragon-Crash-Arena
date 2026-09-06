#include "Arena/ArenaVoidRecoveryVolume.h"

#include "Arena/ArenaFloorCollision.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Player/PhaseCrashComponent.h"
#include "Player/PlayerHealthComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "rdca.h"

AArenaVoidRecoveryVolume::AArenaVoidRecoveryVolume()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ArenaVoidRecoveryRoot"));
	SetRootComponent(SceneRoot);

	RecoveryVolume = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RecoveryVolume"));
	RecoveryVolume->SetupAttachment(SceneRoot);
	RecoveryVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	RecoveryVolume->SetCollisionObjectType(ECC_WorldDynamic);
	RecoveryVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	RecoveryVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RecoveryVolume->SetGenerateOverlapEvents(true);
	RecoveryVolume->SetHiddenInGame(true);
	RecoveryVolume->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		RecoveryVolume->SetStaticMesh(CylinderMesh.Object);
	}
}

void AArenaVoidRecoveryVolume::BeginPlay()
{
	Super::BeginPlay();
	UpdateRecoveryGeometry();
	if (RecoveryVolume)
	{
		RecoveryVolume->SetVisibility(false);
		RecoveryVolume->OnComponentBeginOverlap.AddDynamic(
			this,
			&AArenaVoidRecoveryVolume::HandleRecoveryOverlap);
	}
}

void AArenaVoidRecoveryVolume::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	for (auto It = TriggerCooldowns.CreateIterator(); It; ++It)
	{
		It.Value() -= DeltaSeconds;
		if (!It.Key().IsValid() || It.Value() <= 0.0f)
		{
			It.RemoveCurrent();
		}
	}
}

void AArenaVoidRecoveryVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateRecoveryGeometry();
	if (RecoveryVolume)
	{
		RecoveryVolume->SetVisibility(bVisibleInEditor);
	}
}

void AArenaVoidRecoveryVolume::HandleRecoveryOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const int32 OtherBodyIndex,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	APawn* PlayerPawn = Cast<APawn>(OtherActor);
	if (!PlayerPawn
		|| PlayerPawn != UGameplayStatics::GetPlayerPawn(this, 0)
		|| TriggerCooldowns.Contains(PlayerPawn))
	{
		return;
	}

	TriggerCooldowns.Add(PlayerPawn, FMath::Max(TriggerCooldown, 0.01f));
	if (UPlayerHealthComponent* Health =
			PlayerPawn->FindComponentByClass<UPlayerHealthComponent>())
	{
		Health->ReceiveDamage(Damage);
	}

	const FVector Destination = ResolveReturnDestination(*PlayerPawn);
	if (UPhaseCrashComponent* PhaseCrash =
			PlayerPawn->FindComponentByClass<UPhaseCrashComponent>())
	{
		if (RecoveryMode == EArenaVoidRecoveryMode::LaunchToSafety)
		{
			PhaseCrash->ForceArenaRecovery(Destination, ReturnArcHeight, ReturnDuration);
		}
		else
		{
			PhaseCrash->ForceDetachFromAttachment();
			PlayerPawn->SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);
			PhaseCrash->ForceArenaRecovery(Destination, 0.0f, 0.01f);
		}
	}
	else
	{
		PlayerPawn->SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);
	}

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Arena void recovery. Player=%s Mode=%s Damage=%d Destination=%s"),
		*GetNameSafe(PlayerPawn),
		RecoveryMode == EArenaVoidRecoveryMode::LaunchToSafety ? TEXT("Launch") : TEXT("Reset"),
		Damage,
		*Destination.ToCompactString());
}

void AArenaVoidRecoveryVolume::UpdateRecoveryGeometry()
{
	if (!RecoveryVolume || !RecoveryVolume->GetStaticMesh())
	{
		return;
	}

	const FVector BaseExtent = RecoveryVolume->GetStaticMesh()->GetBounds().BoxExtent;
	if (BaseExtent.X <= UE_KINDA_SMALL_NUMBER
		|| BaseExtent.Y <= UE_KINDA_SMALL_NUMBER
		|| BaseExtent.Z <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	RecoveryVolume->SetRelativeScale3D(FVector(
		Radius / BaseExtent.X,
		Radius / BaseExtent.Y,
		HalfHeight / BaseExtent.Z));
}

FVector AArenaVoidRecoveryVolume::ResolveReturnDestination(const AActor& Player) const
{
	if (RecoveryTarget)
	{
		return RecoveryTarget->GetActorLocation();
	}

	const AArenaFloorCollision* FloorCollision = nullptr;
	if (GetWorld())
	{
		for (TActorIterator<AArenaFloorCollision> It(GetWorld()); It; ++It)
		{
			FloorCollision = *It;
			break;
		}
	}

	const FVector ReturnCenter = FloorCollision
		? FloorCollision->GetFloorCenter()
		: GetActorLocation();
	FVector OutwardDirection = Player.GetActorLocation() - ReturnCenter;
	OutwardDirection.Z = 0.0f;
	if (!OutwardDirection.Normalize())
	{
		OutwardDirection = Player.GetActorForwardVector();
		OutwardDirection.Z = 0.0f;
		if (!OutwardDirection.Normalize())
		{
			OutwardDirection = FVector::ForwardVector;
		}
	}

	if (!FloorCollision)
	{
		return GetActorLocation()
			+ OutwardDirection * AutomaticReturnDistance
			+ FVector::UpVector * AutomaticReturnHeight;
	}

	const float MinimumSafeRadius = FMath::Min(
		FloorCollision->GetInnerHoleRadius() + 300.0f,
		FloorCollision->GetOuterFloorRadius() - 150.0f);
	const float SafeRadius = FMath::Clamp(
		FMath::Max(AutomaticReturnDistance, MinimumSafeRadius),
		0.0f,
		FloorCollision->GetOuterFloorRadius() - 150.0f);
	const float ReturnZ = FMath::Max(
		GetActorLocation().Z + AutomaticReturnHeight,
		ReturnCenter.Z + 100.0f);
	return ReturnCenter
		+ OutwardDirection * SafeRadius
		+ FVector::UpVector * (ReturnZ - ReturnCenter.Z);
}
