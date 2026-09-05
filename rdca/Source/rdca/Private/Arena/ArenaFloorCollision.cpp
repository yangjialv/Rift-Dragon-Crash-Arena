#include "Arena/ArenaFloorCollision.h"

#include "Arena/ArenaCombatBounds.h"
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
	FloorDisc->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	FloorDisc->SetCollisionObjectType(ECC_WorldStatic);
	FloorDisc->SetCollisionResponseToAllChannels(ECR_Ignore);
	FloorDisc->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	FloorDisc->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		FloorDisc->SetStaticMesh(CylinderMesh.Object);
	}
}

void AArenaFloorCollision::BeginPlay()
{
	Super::BeginPlay();
	UpdateFloorGeometry();
	if (FloorDisc)
	{
		FloorDisc->SetVisibility(false);
	}
}

void AArenaFloorCollision::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateFloorGeometry();
	if (FloorDisc)
	{
		FloorDisc->SetVisibility(bVisibleInEditor);
	}
}

void AArenaFloorCollision::UpdateFloorGeometry()
{
	if (!FloorDisc || !FloorDisc->GetStaticMesh())
	{
		return;
	}

	if (ArenaBounds)
	{
		SetActorLocation(ArenaBounds->GetActorLocation());
		FloorRadius = ArenaBounds->GetOuterArenaRadius();
	}

	const FVector BaseExtent = FloorDisc->GetStaticMesh()->GetBounds().BoxExtent;
	if (BaseExtent.X <= UE_KINDA_SMALL_NUMBER
		|| BaseExtent.Y <= UE_KINDA_SMALL_NUMBER
		|| BaseExtent.Z <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}
	FloorDisc->SetRelativeScale3D(FVector(
		FloorRadius / BaseExtent.X,
		FloorRadius / BaseExtent.Y,
		FloorHalfHeight / BaseExtent.Z));
	// The top of the proxy exactly matches this Actor's Z / the visible arena floor.
	FloorDisc->SetRelativeLocation(FVector(0.0f, 0.0f, -FloorHalfHeight));
}
