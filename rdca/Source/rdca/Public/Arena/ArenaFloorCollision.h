#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaFloorCollision.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;

/**
 * A low-cost physical annulus for the visual arena rings. Its central hole is
 * intentional: falling through it is handled separately by a placed recovery
 * volume, not by Combat Bounds.
 */
UCLASS(BlueprintType, Blueprintable)
class RDCA_API AArenaFloorCollision : public AActor
{
	GENERATED_BODY()

public:
	AArenaFloorCollision();

	UFUNCTION(BlueprintPure, Category = "Arena Floor Collision")
	FVector GetFloorCenter() const { return GetActorLocation(); }

	UFUNCTION(BlueprintPure, Category = "Arena Floor Collision")
	float GetInnerHoleRadius() const { return InnerHoleRadius; }

	UFUNCTION(BlueprintPure, Category = "Arena Floor Collision")
	float GetOuterFloorRadius() const { return FloorRadius; }

	/** Random point on the actual walkable annulus, uniformly distributed by area. */
	bool GetRandomAnchorLocation(
		FRandomStream& RandomStream,
		float InnerClearance,
		float OuterClearance,
		FVector& OutLocation) const;

	UFUNCTION(BlueprintPure, Category = "Arena Floor Collision")
	bool IsWalkableFloorLocation(const FVector& Location, float Clearance = 0.0f) const;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena Floor Collision|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Retained for compatibility with existing Blueprints. It is hidden and no longer owns collision. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena Floor Collision|Components")
	TObjectPtr<UStaticMeshComponent> FloorDisc;

	/** Place this actor at the centre and floor height of the walkable outer ring. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Floor Collision|Shape",
		meta = (ClampMin = "100.0"))
	float FloorRadius = 3000.0f;

	/**
	 * Extra collision-only floor beyond FloorRadius. It prevents a visual ring
	 * whose rim extends slightly farther than its configured gameplay radius
	 * from becoming a fall-through seam. Anchor placement still uses FloorRadius.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Floor Collision|Shape",
		meta = (ClampMin = "0.0"))
	float OuterCollisionSafetyMargin = 200.0f;

	/** No floor collision is created inside this radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Floor Collision|Shape",
		meta = (ClampMin = "0.0"))
	float InnerHoleRadius = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Floor Collision|Shape",
		meta = (ClampMin = "12", ClampMax = "48"))
	int32 FloorRingSegmentCount = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Floor Collision|Shape",
		meta = (ClampMin = "5.0"))
	float FloorHalfHeight = 25.0f;

private:
	static constexpr int32 MaxFloorRingSegments = 48;

	void UpdateFloorRingGeometry();
	void ConfigureFloorCollision(class UPrimitiveComponent& Component) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBoxComponent>> FloorRingSegments;
};
