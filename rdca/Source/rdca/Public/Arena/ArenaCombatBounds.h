#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaCombatBounds.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;

/**
 * The gameplay walls of an arena. Ground collision intentionally belongs to
 * AArenaFloorCollision, so this actor only owns the outer and inner air walls.
 */
UCLASS(BlueprintType, Blueprintable)
class RDCA_API AArenaCombatBounds : public AActor
{
	GENERATED_BODY()

public:
	AArenaCombatBounds();

	UFUNCTION(BlueprintPure, Category = "Arena Bounds")
	FVector GetArenaCenter() const { return GetActorLocation(); }

	UFUNCTION(BlueprintPure, Category = "Arena Bounds")
	float GetInnerVoidRadius() const { return InnerVoidRadius; }

	UFUNCTION(BlueprintPure, Category = "Arena Bounds")
	float GetOuterArenaRadius() const { return OuterArenaRadius; }

	/** Returns a uniformly-area-distributed point in the configured anchor band. */
	bool GetRandomAnchorLocation(
		FRandomStream& RandomStream,
		float InnerClearance,
		float OuterClearance,
		FVector& OutLocation) const;

	UFUNCTION(BlueprintPure, Category = "Arena Bounds")
	bool IsInsideOuterArena(const FVector& Location, float Clearance = 0.0f) const;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena Bounds|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena Bounds|Components")
	TObjectPtr<UStaticMeshComponent> InnerBoundaryVisual;

	/** Place this Actor at the arena centre, at the visible ring's floor height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Bounds|Shape",
		meta = (ClampMin = "100.0"))
	float InnerVoidRadius = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Bounds|Shape",
		meta = (ClampMin = "200.0"))
	float OuterArenaRadius = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Bounds|Outer Wall",
		meta = (ClampMin = "8", ClampMax = "24"))
	int32 OuterWallSegmentCount = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Bounds|Outer Wall",
		meta = (ClampMin = "10.0"))
	float OuterWallThickness = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Bounds|Outer Wall",
		meta = (ClampMin = "100.0"))
	float OuterWallHeight = 1400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Bounds|Outer Wall",
		meta = (ClampMin = "0.0"))
	float OuterWallBurialDepth = 50.0f;

	/** A low wall keeps grounded movement out of the Boss centre; high crashes can clear it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Bounds|Inner Wall",
		meta = (ClampMin = "8", ClampMax = "24"))
	int32 InnerWallSegmentCount = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Bounds|Inner Wall",
		meta = (ClampMin = "10.0"))
	float InnerWallThickness = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Bounds|Inner Wall",
		meta = (ClampMin = "30.0"))
	float InnerWallHeight = 140.0f;

	/** Visible circular marker. Replace its mesh/material in Details when final art is ready. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Bounds|Inner Visual",
		meta = (ClampMin = "2.0"))
	float InnerBoundaryVisualHeight = 16.0f;

private:
	static constexpr int32 MaxOuterWallSegments = 24;
	static constexpr int32 MaxInnerWallSegments = 24;

	void UpdateBoundaryGeometry();
	void UpdateDiscGeometry(
		UStaticMeshComponent& Disc,
		float Radius,
		float HalfHeight,
		float TopHeight) const;
	void ConfigurePawnOnlyCollision(UPrimitiveComponent& Component, bool bOverlap) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBoxComponent>> OuterWallSegments;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBoxComponent>> InnerWallSegments;
};
