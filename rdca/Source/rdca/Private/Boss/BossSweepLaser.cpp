#include "Boss/BossSweepLaser.h"

#include "Arena/AnchorOverloadComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "Player/PlayerHealthComponent.h"
#include "rdca.h"
#include "UObject/ConstructorHelpers.h"

ABossSweepLaser::ABossSweepLaser()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	DamageVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageVolume"));
	DamageVolume->SetupAttachment(SceneRoot);
	DamageVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DamageVolume->SetCollisionObjectType(ECC_WorldDynamic);
	DamageVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	DamageVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	DamageVolume->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	DamageVolume->OnComponentBeginOverlap.AddDynamic(
		this,
		&ABossSweepLaser::HandleLaserOverlap);

	BeamRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BeamRoot"));
	BeamRoot->SetupAttachment(SceneRoot);

	LaserVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaserVisual"));
	LaserVisual->SetupAttachment(BeamRoot);
	LaserVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LaserVisual->SetCastShadow(false);

	LaserEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("LaserEffect"));
	LaserEffect->SetupAttachment(BeamRoot);
	LaserEffect->SetAutoActivate(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		LaserVisual->SetStaticMesh(CylinderMesh.Object);
	}

	UpdateComponentDimensions();
}

void ABossSweepLaser::InitializeLaser(
	const float NewStartYaw,
	const float NewEndYaw,
	const float NewSweepDuration,
	const int32 NewDamage)
{
	StartYaw = NewStartYaw;
	EndYaw = NewEndYaw;
	SweepDuration = FMath::Max(NewSweepDuration, 0.1f);
	Damage = FMath::Max(NewDamage, 1);
	DamageVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DamageVolume->SetGenerateOverlapEvents(true);
	DamageVolume->SetCollisionResponseToChannel(
		ECC_WorldDynamic,
		ECR_Overlap);
	SetActorRotation(FRotator(0.0f, StartYaw, 0.0f));
	UpdateComponentDimensions();
	LaserEffect->DeactivateImmediate();

	if (WarningMaterial)
	{
		LaserVisual->SetMaterial(0, WarningMaterial);
	}
}

void ABossSweepLaser::ActivateLaser()
{
	bLaserActive = true;
	SweepElapsed = 0.0f;
	DamagedActors.Reset();
	if (ActiveMaterial)
	{
		LaserVisual->SetMaterial(0, ActiveMaterial);
	}
	if (LaserEffect->GetAsset())
	{
		LaserEffect->Activate(true);
	}

	TArray<AActor*> InitiallyOverlappingActors;
	DamageVolume->GetOverlappingActors(InitiallyOverlappingActors);
	for (AActor* Actor : InitiallyOverlappingActors)
	{
		ApplyDamageToActor(Actor);
	}
}

void ABossSweepLaser::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bLaserActive)
	{
		return;
	}

	SweepElapsed = FMath::Min(SweepElapsed + DeltaTime, SweepDuration);
	const float Alpha = FMath::Clamp(SweepElapsed / SweepDuration, 0.0f, 1.0f);
	SetActorRotation(FRotator(
		0.0f,
		FMath::Lerp(StartYaw, EndYaw, Alpha),
		0.0f));
	ApplyAnchorOverload(DeltaTime);

	if (Alpha >= 1.0f)
	{
		bLaserActive = false;
	}
}

void ABossSweepLaser::HandleLaserOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (bLaserActive)
	{
		ApplyDamageToActor(OtherActor);
	}
}

void ABossSweepLaser::ApplyAnchorOverload(const float DeltaTime)
{
	if (AnchorOverloadPerSecond <= 0.0f)
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	DamageVolume->GetOverlappingActors(OverlappingActors);
	for (AActor* Actor : OverlappingActors)
	{
		if (UAnchorOverloadComponent* AnchorOverload =
				Actor
					? Actor->FindComponentByClass<UAnchorOverloadComponent>()
					: nullptr)
		{
			AnchorOverload->AddOverloadAmount(
				AnchorOverloadPerSecond * DeltaTime);
		}
	}
}

void ABossSweepLaser::ApplyDamageToActor(AActor* OtherActor)
{
	if (!OtherActor || DamagedActors.Contains(OtherActor))
	{
		return;
	}

	UPlayerHealthComponent* Health =
		OtherActor->FindComponentByClass<UPlayerHealthComponent>();
	if (!Health)
	{
		return;
	}

	DamagedActors.Add(OtherActor);
	const bool bDamageApplied = Health->ReceiveDamage(Damage);
	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss sweep laser hit. Player=%s DamageApplied=%s"),
		*GetNameSafe(OtherActor),
		bDamageApplied ? TEXT("true") : TEXT("false"));
}

void ABossSweepLaser::UpdateComponentDimensions()
{
	const FVector LocalCenter(LaserLength * 0.5f, 0.0f, LaserHalfHeight);
	DamageVolume->SetRelativeLocation(LocalCenter);
	DamageVolume->SetBoxExtent(
		FVector(LaserLength * 0.5f, LaserHalfWidth, LaserHalfHeight));

	BeamRoot->SetRelativeLocation(VisualLocationOffset);
	BeamRoot->SetRelativeRotation(VisualRotationOffset);
	LaserVisual->SetRelativeLocation(FVector(LaserLength * 0.5f, 0.0f, LaserHalfHeight));
	LaserVisual->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	LaserVisual->SetRelativeScale3D(FVector(
		LaserHalfWidth / 50.0f,
		LaserHalfWidth / 50.0f,
		LaserLength / 100.0f));
}
