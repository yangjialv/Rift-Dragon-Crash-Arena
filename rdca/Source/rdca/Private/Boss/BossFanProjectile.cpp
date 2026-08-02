#include "Boss/BossFanProjectile.h"

#include "Arena/AnchorOverloadComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Player/PlayerHealthComponent.h"
#include "rdca.h"
#include "UObject/ConstructorHelpers.h"

ABossFanProjectile::ABossFanProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComponent =
		CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(CollisionComponent);
	CollisionComponent->InitSphereRadius(30.0f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	CollisionComponent->OnComponentBeginOverlap.AddDynamic(
		this,
		&ABossFanProjectile::HandleProjectileOverlap);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(CollisionComponent);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetRelativeScale3D(FVector(0.6f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		VisualMesh->SetStaticMesh(SphereMesh.Object);
	}
}

void ABossFanProjectile::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector Movement = TravelDirection * TravelSpeed * DeltaTime;
	if (bGroundSkimming)
	{
		const float CurrentHeight = GetActorLocation().Z;
		if (CurrentHeight <= GroundSkimHeight
			|| CurrentHeight + Movement.Z <= GroundSkimHeight)
		{
			Movement = GroundTravelDirection * TravelSpeed * DeltaTime;
			Movement.Z = GroundSkimHeight - CurrentHeight;
			TravelDirection = GroundTravelDirection;
			SetActorRotation(TravelDirection.Rotation());
		}
	}

	FHitResult Hit;
	RootComponent->MoveComponent(
		Movement,
		GetActorRotation(),
		true,
		&Hit);
	if (Hit.IsValidBlockingHit())
	{
		Destroy();
	}
}

void ABossFanProjectile::InitializeGroundSkimmingProjectile(
	const FVector& WorldDirection,
	const float NewSpeed,
	const int32 NewDamage,
	const float WorldCruiseHeight)
{
	InitializeProjectile(WorldDirection, NewSpeed, NewDamage);
	GroundTravelDirection = FVector(TravelDirection.X, TravelDirection.Y, 0.0f)
		.GetSafeNormal();
	if (GroundTravelDirection.IsNearlyZero())
	{
		GroundTravelDirection = FVector::ForwardVector;
	}
	GroundSkimHeight = WorldCruiseHeight;
	bGroundSkimming = TravelDirection.Z < 0.0f;
}

void ABossFanProjectile::InitializeProjectile(
	const FVector& WorldDirection,
	const float NewSpeed,
	const int32 NewDamage)
{
	TravelDirection = WorldDirection.GetSafeNormal();
	TravelSpeed = FMath::Max(NewSpeed, 1.0f);
	Damage = FMath::Max(NewDamage, 1);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetGenerateOverlapEvents(true);
	CollisionComponent->SetCollisionResponseToChannel(
		ECC_WorldDynamic,
		ECR_Overlap);
	SetLifeSpan(FMath::Max(LifeSeconds, 0.1f));
	SetActorRotation(TravelDirection.Rotation());
}

void ABossFanProjectile::HandleProjectileOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (bHasAppliedDamage || !OtherActor)
	{
		return;
	}

	UPlayerHealthComponent* Health =
		OtherActor->FindComponentByClass<UPlayerHealthComponent>();
	if (UAnchorOverloadComponent* AnchorOverload =
			OtherActor->FindComponentByClass<UAnchorOverloadComponent>())
	{
		bHasAppliedDamage = true;
		AnchorOverload->AddOverloadAmount(AnchorOverloadAmount);
		UE_LOG(
			LogRDCAPlayer,
			Log,
			TEXT("Boss fan projectile overloaded anchor. Anchor=%s Amount=%.2f"),
			*GetNameSafe(OtherActor),
			AnchorOverloadAmount);
		Destroy();
		return;
	}

	if (!Health)
	{
		return;
	}

	bHasAppliedDamage = true;
	const bool bDamageApplied = Health->ReceiveDamage(Damage);
	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss fan projectile hit. Projectile=%s Player=%s DamageApplied=%s"),
		*GetNameSafe(this),
		*GetNameSafe(OtherActor),
		bDamageApplied ? TEXT("true") : TEXT("false"));
	Destroy();
}
