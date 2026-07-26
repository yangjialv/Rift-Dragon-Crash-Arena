#include "Boss/BossFanProjectile.h"

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

	FHitResult Hit;
	RootComponent->MoveComponent(
		TravelDirection * TravelSpeed * DeltaTime,
		GetActorRotation(),
		true,
		&Hit);
	if (Hit.IsValidBlockingHit())
	{
		Destroy();
	}
}

void ABossFanProjectile::InitializeProjectile(
	const FVector& WorldDirection,
	const float NewSpeed,
	const int32 NewDamage)
{
	TravelDirection = WorldDirection.GetSafeNormal();
	TravelSpeed = FMath::Max(NewSpeed, 1.0f);
	Damage = FMath::Max(NewDamage, 1);
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
