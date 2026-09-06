#include "Arena/ArenaFloorCollision.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AArenaFloorCollision::AArenaFloorCollision()
{
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ArenaFloorCollisionRoot"));
	SetRootComponent(SceneRoot);

	FloorDisc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FloorDisc"));
	FloorDisc->SetupAttachment(SceneRoot);
	FloorDisc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FloorDisc->SetVisibility(false);
	FloorDisc->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		FloorDisc->SetStaticMesh(CylinderMesh.Object);
	}

	FloorRingSegments.Reserve(MaxFloorRingSegments);
	for (int32 Index = 0; Index < MaxFloorRingSegments; ++Index)
	{
		UBoxComponent* Segment = CreateDefaultSubobject<UBoxComponent>(
			*FString::Printf(TEXT("FloorRing_%02d"), Index + 1));
		Segment->SetupAttachment(SceneRoot);
		ConfigureFloorCollision(*Segment);
		Segment->SetGenerateOverlapEvents(false);
		FloorRingSegments.Add(Segment);
	}
}

void AArenaFloorCollision::BeginPlay()
{
	Super::BeginPlay();
	UpdateFloorRingGeometry();
	if (FloorDisc)
	{
		// Existing Blueprint instances may have saved the previous full-disc
		// collision profile. Force it off so the central hole is always real.
		FloorDisc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FloorDisc->SetVisibility(false);
	}
}

void AArenaFloorCollision::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateFloorRingGeometry();
	if (FloorDisc)
	{
		FloorDisc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FloorDisc->SetVisibility(false);
	}
}

bool AArenaFloorCollision::GetRandomAnchorLocation(
	FRandomStream& RandomStream,
	const float InnerClearance,
	const float OuterClearance,
	FVector& OutLocation) const
{
	const float MinimumRadius = FMath::Max(InnerHoleRadius + InnerClearance, 0.0f);
	const float MaximumRadius = FloorRadius - FMath::Max(OuterClearance, 0.0f);
	if (MaximumRadius <= MinimumRadius)
	{
		return false;
	}

	const float RadiusSquared = RandomStream.FRandRange(
		MinimumRadius * MinimumRadius,
		MaximumRadius * MaximumRadius);
	const float AngleRadians = RandomStream.FRandRange(0.0f, 2.0f * PI);
	OutLocation = GetActorLocation() + FVector(
		FMath::Cos(AngleRadians) * FMath::Sqrt(RadiusSquared),
		FMath::Sin(AngleRadians) * FMath::Sqrt(RadiusSquared),
		0.0f);
	return true;
}

bool AArenaFloorCollision::IsWalkableFloorLocation(
	const FVector& Location,
	const float Clearance) const
{
	const float Radius = FVector::Dist2D(Location, GetActorLocation());
	return Radius >= InnerHoleRadius + FMath::Max(Clearance, 0.0f)
		&& Radius <= FloorRadius - FMath::Max(Clearance, 0.0f);
}

void AArenaFloorCollision::UpdateFloorRingGeometry()
{
	const int32 ActiveSegments = FMath::Clamp(
		FloorRingSegmentCount,
		12,
		MaxFloorRingSegments);
	const float SafeInnerRadius = FMath::Min(
		FMath::Max(InnerHoleRadius, 0.0f),
		FMath::Max(FloorRadius - 1.0f, 0.0f));
	const float RingWidth = FMath::Max(FloorRadius - SafeInnerRadius, 1.0f);
	const float MidRadius = SafeInnerRadius + RingWidth * 0.5f;
	const float TangentialLength = 2.0f * PI * MidRadius / ActiveSegments;
	const FVector SegmentExtent(
		RingWidth * 0.5f + 5.0f,
		TangentialLength * 0.5f + 12.0f,
		FloorHalfHeight);

	for (int32 Index = 0; Index < FloorRingSegments.Num(); ++Index)
	{
		UBoxComponent* Segment = FloorRingSegments[Index];
		if (!Segment)
		{
			continue;
		}

		const bool bActive = Index < ActiveSegments;
		Segment->SetCollisionEnabled(
			bActive ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (!bActive)
		{
			continue;
		}

		const float AngleRadians = 2.0f * PI * Index / ActiveSegments;
		const FVector RadialDirection(
			FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f);
		Segment->SetBoxExtent(SegmentExtent, false);
		Segment->SetRelativeLocation(
			RadialDirection * MidRadius - FVector::UpVector * FloorHalfHeight);
		Segment->SetRelativeRotation(FRotator(
			0.0f,
			FMath::RadiansToDegrees(AngleRadians),
			0.0f));
	}
}

void AArenaFloorCollision::ConfigureFloorCollision(UPrimitiveComponent& Component) const
{
	Component.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Component.SetCollisionObjectType(ECC_WorldStatic);
	Component.SetCollisionResponseToAllChannels(ECR_Ignore);
	Component.SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Component.SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	Component.SetCollisionResponseToChannel(ECC_Destructible, ECR_Block);
}
