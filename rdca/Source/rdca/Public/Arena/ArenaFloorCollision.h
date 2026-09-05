#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaFloorCollision.generated.h"

class AArenaCombatBounds;
class USceneComponent;
class UStaticMeshComponent;

/** A separate, low-cost circular floor collision proxy for the visual arena rings. */
UCLASS(BlueprintType, Blueprintable)
class RDCA_API AArenaFloorCollision : public AActor
{
	GENERATED_BODY()

public:
	AArenaFloorCollision();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena Floor Collision|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena Floor Collision|Components")
	TObjectPtr<UStaticMeshComponent> FloorDisc;

	/** Optional source of centre and radius; avoids duplicating arena measurements. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Arena Floor Collision|Setup")
	TObjectPtr<AArenaCombatBounds> ArenaBounds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Floor Collision|Shape",
		meta = (ClampMin = "100.0"))
	float FloorRadius = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Floor Collision|Shape",
		meta = (ClampMin = "5.0"))
	float FloorHalfHeight = 25.0f;

	/** Shows the low-poly proxy in editor only; it is always hidden during play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Floor Collision|Debug")
	bool bVisibleInEditor = true;

private:
	void UpdateFloorGeometry();
};
