#include "Arena/ArenaCombatBounds.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AArenaCombatBounds::AArenaCombatBounds()
{
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ArenaBoundsRoot"));
	SetRootComponent(SceneRoot);

	InnerBoundaryVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InnerBoundaryVisual"));
	InnerBoundaryVisual->SetupAttachment(SceneRoot);
	InnerBoundaryVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		InnerBoundaryVisual->SetStaticMesh(CylinderMesh.Object);
	}

	OuterWallSegments.Reserve(MaxOuterWallSegments);
	for (int32 Index = 0; Index < MaxOuterWallSegments; ++Index)
	{
		UBoxComponent* Segment = CreateDefaultSubobject<UBoxComponent>(
			*FString::Printf(TEXT("OuterWall_%02d"), Index + 1));
		Segment->SetupAttachment(SceneRoot);
		ConfigurePawnOnlyCollision(*Segment, false);
		OuterWallSegments.Add(Segment);
	}

	InnerWallSegments.Reserve(MaxInnerWallSegments);
	for (int32 Index = 0; Index < MaxInnerWallSegments; ++Index)
	{
		UBoxComponent* Segment = CreateDefaultSubobject<UBoxComponent>(
			*FString::Printf(TEXT("InnerWall_%02d"), Index + 1));
		Segment->SetupAttachment(SceneRoot);
		ConfigurePawnOnlyCollision(*Segment, false);
		InnerWallSegments.Add(Segment);
	}
}

void AArenaCombatBounds::BeginPlay()
{
	Super::BeginPlay();
	UpdateBoundaryGeometry();
}

void AArenaCombatBounds::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateBoundaryGeometry();
}

bool AArenaCombatBounds::GetRandomAnchorLocation(
	FRandomStream& RandomStream,
	const float InnerClearance,
	const float OuterClearance,
	FVector& OutLocation) const
{
	const float MinimumRadius = FMath::Max(InnerVoidRadius + InnerClearance, 0.0f);
	const float MaximumRadius = OuterArenaRadius - FMath::Max(OuterClearance, 0.0f);
	if (MaximumRadius <= MinimumRadius)
	{
		return false;
	}

	const float RadiusSquared = RandomStream.FRandRange(
		MinimumRadius * MinimumRadius,
		MaximumRadius * MaximumRadius);
	const float AngleRadians = RandomStream.FRandRange(0.0f, 2.0f * PI);
	const FVector Direction(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f);
	OutLocation = GetActorLocation() + Direction * FMath::Sqrt(RadiusSquared);
	return true;
}

bool AArenaCombatBounds::IsInsideOuterArena(
	const FVector& Location,
	const float Clearance) const
{
	return FVector::Dist2D(Location, GetActorLocation())
		<= FMath::Max(OuterArenaRadius - Clearance, 0.0f);
}

void AArenaCombatBounds::UpdateBoundaryGeometry()
{
	const int32 ActiveSegments = FMath::Clamp(
		OuterWallSegmentCount,
		8,
		MaxOuterWallSegments);
	const float ArcLength = 2.0f * PI * OuterArenaRadius / ActiveSegments;
	const FVector SegmentExtent(
		ArcLength * 0.5f + 12.0f,
		OuterWallThickness * 0.5f,
		OuterWallHeight * 0.5f);

	for (int32 Index = 0; Index < OuterWallSegments.Num(); ++Index)
	{
		UBoxComponent* Segment = OuterWallSegments[Index];
		if (!Segment)
		{
			continue;
		}
		const bool bActive = Index < ActiveSegments;
		Segment->SetCollisionEnabled(
			bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		Segment->SetGenerateOverlapEvents(false);
		if (!bActive)
		{
			continue;
		}

		const float AngleRadians = 2.0f * PI * Index / ActiveSegments;
		const FVector RadialDirection(
			FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f);
		Segment->SetBoxExtent(SegmentExtent, false);
		Segment->SetRelativeLocation(
			RadialDirection * OuterArenaRadius
			+ FVector::UpVector * (OuterWallHeight * 0.5f - OuterWallBurialDepth));
		Segment->SetRelativeRotation(FRotator(0.0f,
			FMath::RadiansToDegrees(AngleRadians) + 90.0f, 0.0f));
	}

	const int32 ActiveInnerSegments = FMath::Clamp(
		InnerWallSegmentCount,
		8,
		MaxInnerWallSegments);
	const float InnerArcLength = 2.0f * PI * InnerVoidRadius / ActiveInnerSegments;
	const FVector InnerSegmentExtent(
		InnerArcLength * 0.5f + 10.0f,
		InnerWallThickness * 0.5f,
		InnerWallHeight * 0.5f);
	for (int32 Index = 0; Index < InnerWallSegments.Num(); ++Index)
	{
		UBoxComponent* Segment = InnerWallSegments[Index];
		if (!Segment)
		{
			continue;
		}
		const bool bActive = Index < ActiveInnerSegments;
		Segment->SetCollisionEnabled(
			bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		if (!bActive)
		{
			continue;
		}
		const float AngleRadians = 2.0f * PI * Index / ActiveInnerSegments;
		const FVector RadialDirection(
			FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f);
		Segment->SetBoxExtent(InnerSegmentExtent, false);
		Segment->SetRelativeLocation(
			RadialDirection * InnerVoidRadius
			+ FVector::UpVector * (InnerWallHeight * 0.5f));
		Segment->SetRelativeRotation(FRotator(0.0f,
			FMath::RadiansToDegrees(AngleRadians) + 90.0f, 0.0f));
	}

	if (InnerBoundaryVisual)
	{
		UpdateDiscGeometry(
			*InnerBoundaryVisual,
			InnerVoidRadius,
			InnerBoundaryVisualHeight * 0.5f,
			InnerBoundaryVisualHeight);
	}
}

void AArenaCombatBounds::UpdateDiscGeometry(
	UStaticMeshComponent& Disc,
	const float Radius,
	const float HalfHeight,
	const float TopHeight) const
{
	const UStaticMesh* Mesh = Disc.GetStaticMesh();
	if (!Mesh)
	{
		return;
	}
	const FVector BaseExtent = Mesh->GetBounds().BoxExtent;
	if (BaseExtent.X <= UE_KINDA_SMALL_NUMBER
		|| BaseExtent.Y <= UE_KINDA_SMALL_NUMBER
		|| BaseExtent.Z <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	Disc.SetRelativeScale3D(FVector(
		Radius / BaseExtent.X,
		Radius / BaseExtent.Y,
		HalfHeight / BaseExtent.Z));
	Disc.SetRelativeLocation(FVector(0.0f, 0.0f, TopHeight - HalfHeight));
}

void AArenaCombatBounds::ConfigurePawnOnlyCollision(
	UPrimitiveComponent& Component,
	const bool bOverlap) const
{
	Component.SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Component.SetCollisionObjectType(ECC_WorldStatic);
	Component.SetCollisionResponseToAllChannels(ECR_Ignore);
	Component.SetCollisionResponseToChannel(
		ECC_Pawn,
		bOverlap ? ECR_Overlap : ECR_Block);
}
