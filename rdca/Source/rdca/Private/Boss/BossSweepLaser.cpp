#include "Boss/BossSweepLaser.h"

#include "Arena/AnchorOverloadComponent.h"
#include "Arena/ArenaFloorCollision.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "Player/PlayerHealthComponent.h"
#include "rdca.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
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

/**
 * Finds the first continuous part of a forward warning ray that lies on the
 * walkable annulus. Width is used as clearance so the warning plane does not
 * visibly hang over either circular edge.
 */
bool ResolveFloorWarningInterval(
	const AArenaFloorCollision& Floor,
	const FVector& RayStart,
	const FVector& Forward,
	const float MaximumLength,
	const float WarningHalfWidth,
	float& OutStartDistance,
	float& OutLength)
{
	if (MaximumLength <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector2D Start2D(RayStart.X, RayStart.Y);
	const FVector FloorCenter = Floor.GetFloorCenter();
	const FVector2D Center2D(FloorCenter.X, FloorCenter.Y);
	FVector2D Direction2D(Forward.X, Forward.Y);
	if (!Direction2D.Normalize())
	{
		return false;
	}

	TArray<float, TInlineAllocator<8>> Breakpoints;
	Breakpoints.Add(0.0f);
	Breakpoints.Add(MaximumLength);

	const FVector2D StartFromCenter = Start2D - Center2D;
	const float RayProjection = FVector2D::DotProduct(
		StartFromCenter,
		Direction2D);
	const float StartRadiusSquared = StartFromCenter.SizeSquared();

	auto AddCircleIntersections = [&](const float Radius)
	{
		if (Radius <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const float Discriminant = FMath::Square(RayProjection)
			- (StartRadiusSquared - FMath::Square(Radius));
		if (Discriminant < 0.0f)
		{
			return;
		}

		const float Root = FMath::Sqrt(Discriminant);
		const float NearDistance = -RayProjection - Root;
		const float FarDistance = -RayProjection + Root;
		if (NearDistance > 0.0f && NearDistance < MaximumLength)
		{
			Breakpoints.Add(NearDistance);
		}
		if (FarDistance > 0.0f && FarDistance < MaximumLength)
		{
			Breakpoints.Add(FarDistance);
		}
	};

	const float Clearance = FMath::Max(WarningHalfWidth, 0.0f);
	const float SafeInnerRadius = Floor.GetInnerHoleRadius() + Clearance;
	const float SafeOuterRadius = Floor.GetOuterFloorRadius() - Clearance;
	if (SafeOuterRadius <= SafeInnerRadius)
	{
		return false;
	}

	AddCircleIntersections(SafeInnerRadius);
	AddCircleIntersections(SafeOuterRadius);
	Breakpoints.Sort();

	for (int32 Index = 0; Index + 1 < Breakpoints.Num(); ++Index)
	{
		const float SegmentStart = Breakpoints[Index];
		const float SegmentEnd = Breakpoints[Index + 1];
		if (SegmentEnd - SegmentStart <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float Midpoint = (SegmentStart + SegmentEnd) * 0.5f;
		const FVector2D TestPoint = Start2D + Direction2D * Midpoint;
		const float TestRadius = FVector2D::Distance(TestPoint, Center2D);
		if (TestRadius >= SafeInnerRadius && TestRadius <= SafeOuterRadius)
		{
			OutStartDistance = SegmentStart;
			OutLength = SegmentEnd - SegmentStart;
			return true;
		}
	}

	return false;
}
}

ABossSweepLaser::ABossSweepLaser()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BeamRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BeamRoot"));
	BeamRoot->SetupAttachment(SceneRoot);

	DamageVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageVolume"));
	// Reattached to LaserEffect below, after that component is constructed.
	// This placeholder keeps the subobject hierarchy valid during construction.
	DamageVolume->SetupAttachment(BeamRoot);
	DamageVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DamageVolume->SetCollisionObjectType(ECC_WorldDynamic);
	DamageVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	DamageVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	DamageVolume->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	DamageVolume->OnComponentBeginOverlap.AddDynamic(
		this,
		&ABossSweepLaser::HandleLaserOverlap);

	LaserVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaserVisual"));
	LaserVisual->SetupAttachment(BeamRoot);
	LaserVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LaserVisual->SetCastShadow(false);

	GroundWarningVisual = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("GroundWarningVisual"));
	GroundWarningVisual->SetupAttachment(SceneRoot);
	GroundWarningVisual->SetMobility(EComponentMobility::Movable);
	GroundWarningVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GroundWarningVisual->SetCastShadow(false);
	GroundWarningVisual->SetVisibility(false);
	GroundWarningVisual->SetHiddenInGame(true);

	LaserEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("LaserEffect"));
	LaserEffect->SetupAttachment(BeamRoot);
	LaserEffect->SetAutoActivate(false);
	// The Niagara component remains the authoritative active-flame transform.
	DamageVolume->SetupAttachment(LaserEffect);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		LaserVisual->SetStaticMesh(CylinderMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		GroundWarningVisual->SetStaticMesh(CubeMesh.Object);
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
	DamageVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LaserVisual->SetVisibility(false, true);
	LaserVisual->SetHiddenInGame(true, true);
	GroundWarningVisual->SetVisibility(true, true);
	GroundWarningVisual->SetHiddenInGame(false, true);
	SetActorRotation(FRotator(0.0f, StartYaw, 0.0f));
	UpdateComponentDimensions();
	LaserEffect->DeactivateImmediate();

	if (UMaterialInterface* Warning = GroundWarningMaterial
		? GroundWarningMaterial.Get()
		: WarningMaterial.Get())
	{
		GroundWarningVisual->SetMaterial(0, Warning);
	}
	UpdateGroundWarningVisual();
}

void ABossSweepLaser::ActivateLaser()
{
	bLaserActive = true;
	SweepElapsed = 0.0f;
	DamagedActors.Reset();
	GroundWarningVisual->SetVisibility(false, true);
	GroundWarningVisual->SetHiddenInGame(true, true);
	LaserVisual->SetVisibility(bShowLaserVisualDuringActive, true);
	LaserVisual->SetHiddenInGame(!bShowLaserVisualDuringActive, true);
	DamageVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DamageVolume->SetGenerateOverlapEvents(true);
	DamageVolume->SetCollisionResponseToChannel(
		ECC_WorldDynamic,
		ECR_Overlap);
	if (bShowLaserVisualDuringActive && ActiveMaterial)
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

void ABossSweepLaser::UpdateWarningPose(
	const FVector& WorldLocation,
	const float WorldYaw)
{
	if (bLaserActive)
	{
		return;
	}
	SetActorLocation(WorldLocation);
	SetActorRotation(FRotator(0.0f, WorldYaw, 0.0f));
	UpdateGroundWarningVisual();
}

void ABossSweepLaser::ConfigureSweep(
	const float NewStartYaw,
	const float NewEndYaw)
{
	if (bLaserActive)
	{
		return;
	}
	StartYaw = NewStartYaw;
	EndYaw = NewEndYaw;
	SetActorRotation(FRotator(0.0f, StartYaw, 0.0f));
	UpdateGroundWarningVisual();
}

bool ABossSweepLaser::IsActorInsideWarningArea(
	const AActor* Candidate) const
{
	if (!Candidate)
	{
		return false;
	}

	const AActor* Boss = GetOwner();
	const FVector BossLocation = Boss
		? Boss->GetActorLocation()
		: GetActorLocation();
	FVector Forward = GetActorForwardVector();
	Forward.Z = 0.0f;
	if (!Forward.Normalize())
	{
		return false;
	}

	FVector WarningStart = BossLocation
		+ Forward * GroundWarningStartOffset;
	const float ConfiguredWarningLength = FMath::Max(
		GroundWarningLength,
		1.0f);
	float EffectiveLength = ConfiguredWarningLength;
	if (const AArenaFloorCollision* Floor = FindArenaFloorCollision(GetWorld()))
	{
		const float WarningHalfWidth = FMath::Max(
			GroundWarningWidth * 0.5f,
			0.5f);
		float FloorStartDistance = 0.0f;
		float AvailableFloorLength = 0.0f;
		const float FloorSearchLength = FVector::Dist2D(
			WarningStart,
			Floor->GetFloorCenter())
			+ Floor->GetOuterFloorRadius() * 2.0f
			+ ConfiguredWarningLength;
		if (!ResolveFloorWarningInterval(
				*Floor,
				WarningStart,
				Forward,
				FloorSearchLength,
				WarningHalfWidth,
				FloorStartDistance,
				AvailableFloorLength))
		{
			return false;
		}
		WarningStart += Forward * FloorStartDistance;
		EffectiveLength = FMath::Min(
			ConfiguredWarningLength,
			AvailableFloorLength);
	}
	FVector ToCandidate = Candidate->GetActorLocation() - WarningStart;
	ToCandidate.Z = 0.0f;
	const float ForwardDistance = FVector::DotProduct(ToCandidate, Forward);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	const float SideDistance = FMath::Abs(
		FVector::DotProduct(ToCandidate, Right));
	const float WarningHalfWidth = FMath::Max(
		GroundWarningWidth * 0.5f,
		0.5f);
	return ForwardDistance >= 0.0f
		&& ForwardDistance <= EffectiveLength
		&& SideDistance <= WarningHalfWidth;
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
	// LaserVisual, LaserEffect and DamageVolume are authored as one calibrated
	// beam at these default dimensions. Scale their common root so all three
	// remain aligned instead of trying to reshape the Niagara system itself.
	constexpr float ReferenceLength = 1400.0f;
	constexpr float ReferenceWidth = 40.0f;
	constexpr float ReferenceHeight = 90.0f;
	const FVector LocalCenter(ReferenceLength * 0.5f, 0.0f, 0.0f);
	const float ReferenceHalfWidth = ReferenceWidth * 0.5f;
	const float ReferenceHalfHeight = ReferenceHeight * 0.5f;
	DamageVolume->SetRelativeLocation(LocalCenter);
	DamageVolume->SetBoxExtent(
		FVector(
			ReferenceLength * 0.5f,
			ReferenceHalfWidth,
			ReferenceHalfHeight));

	BeamRoot->SetRelativeLocation(VisualLocationOffset);
	BeamRoot->SetRelativeRotation(VisualRotationOffset);
	BeamRoot->SetRelativeScale3D(FVector(
		FMath::Max(LaserLength, 1.0f) / ReferenceLength,
		FMath::Max(LaserWidth, 1.0f) / ReferenceWidth,
		FMath::Max(LaserHeight, 1.0f) / ReferenceHeight));
	LaserVisual->SetRelativeLocation(FVector(
		ReferenceLength * 0.5f,
		0.0f,
		0.0f));
	LaserVisual->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	LaserVisual->SetRelativeScale3D(FVector(
		ReferenceHalfHeight / 50.0f,
		ReferenceHalfWidth / 50.0f,
		ReferenceLength / 100.0f));
	UpdateGroundWarningVisual();
}

void ABossSweepLaser::UpdateGroundWarningVisual()
{
	if (!GroundWarningVisual)
	{
		return;
	}

	const AActor* Boss = GetOwner();
	const FVector BossLocation = Boss
		? Boss->GetActorLocation()
		: GetActorLocation();
	FVector Forward = GetActorForwardVector();
	Forward.Z = 0.0f;
	if (!Forward.Normalize())
	{
		return;
	}

	FVector WarningStart(
		BossLocation.X + Forward.X * GroundWarningStartOffset,
		BossLocation.Y + Forward.Y * GroundWarningStartOffset,
		BossLocation.Z);
	const float ConfiguredWarningLength = FMath::Max(
		GroundWarningLength,
		1.0f);
	float EffectiveLength = ConfiguredWarningLength;
	float GroundZ = 0.0f;
	const float WarningHalfWidth = FMath::Max(
		GroundWarningWidth * 0.5f,
		0.5f);
	if (const AArenaFloorCollision* Floor = FindArenaFloorCollision(GetWorld()))
	{
		float FloorStartDistance = 0.0f;
		float AvailableFloorLength = 0.0f;
		const float FloorSearchLength = FVector::Dist2D(
			WarningStart,
			Floor->GetFloorCenter())
			+ Floor->GetOuterFloorRadius() * 2.0f
			+ ConfiguredWarningLength;
		if (!ResolveFloorWarningInterval(
				*Floor,
				WarningStart,
				Forward,
				FloorSearchLength,
				WarningHalfWidth,
				FloorStartDistance,
				AvailableFloorLength))
		{
			GroundWarningVisual->SetVisibility(false, true);
			GroundWarningVisual->SetHiddenInGame(true, true);
			return;
		}

		WarningStart += Forward * FloorStartDistance;
		EffectiveLength = FMath::Min(
			ConfiguredWarningLength,
			AvailableFloorLength);
		GroundZ = Floor->GetFloorCenter().Z;
	}
	else if (!ResolveTaggedGroundHeight(GroundZ))
	{
		GroundWarningVisual->SetVisibility(false, true);
		GroundWarningVisual->SetHiddenInGame(true, true);
		return;
	}
	const float WarningThickness = FMath::Max(GroundWarningThickness, 0.1f);
	WarningStart.Z = GroundZ
		+ GroundWarningSurfaceOffset
		+ WarningThickness * 0.5f;
	const FVector WarningCenter = WarningStart
		+ Forward * (EffectiveLength * 0.5f);
	const float WarningWidth = FMath::Max(GroundWarningWidth, 1.0f);

	GroundWarningVisual->SetWorldLocationAndRotation(
		WarningCenter,
		Forward.Rotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	GroundWarningVisual->SetWorldScale3D(FVector(
		EffectiveLength / 100.0f,
		WarningWidth / 100.0f,
		WarningThickness / 100.0f));
	GroundWarningVisual->SetVisibility(!bLaserActive, true);
	GroundWarningVisual->SetHiddenInGame(bLaserActive, true);
}

bool ABossSweepLaser::ResolveTaggedGroundHeight(float& OutGroundZ) const
{
	if (!GetWorld())
	{
		return false;
	}

	static const FName LaserGroundTag(TEXT("LaserGround"));
	for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(*ActorIt);
		for (const UPrimitiveComponent* Primitive : PrimitiveComponents)
		{
			if (IsValid(Primitive)
				&& Primitive->ComponentHasTag(LaserGroundTag))
			{
				OutGroundZ = Primitive->Bounds.GetBox().Max.Z;
				return true;
			}
		}
	}

	return false;
}
