#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AnchorSpawnManager.generated.h"

class AArenaFloorCollision;

UCLASS(Blueprintable)
class RDCA_API AAnchorSpawnManager : public AActor
{
	GENERATED_BODY()

public:
	AAnchorSpawnManager();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable, Category = "Anchor Spawning")
	void SpawnAnchors();

	UFUNCTION(BlueprintCallable, Category = "Anchor Spawning")
	void ClearSpawnedAnchors();

	UFUNCTION(BlueprintPure, Category = "Anchor Spawning")
	int32 GetSpawnedAnchorCount() const;

	void HandleManagedAnchorOverloaded(AActor* OverloadedAnchor);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor Spawning")
	TSubclassOf<AActor> AnchorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor Spawning",
		meta = (ClampMin = "1"))
	int32 NumberOfAnchors = 9;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor Spawning")
	FName SpawnPointTag = TEXT("AnchorSpawnPoint");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor Spawning")
	bool bSpawnOnBeginPlay = true;

	/**
	 * Generates anchors on the walkable annulus supplied by Arena Floor Collision
	 * instead of choosing manually placed AnchorSpawnPoint actors. The manual mode
	 * remains the default so existing layouts are unaffected until this is enabled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor Spawning|Arena Random")
	bool bUseArenaRandomSpawns = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Anchor Spawning|Arena Random",
		meta = (EditCondition = "bUseArenaRandomSpawns"))
	TObjectPtr<AArenaFloorCollision> ArenaFloorCollision;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor Spawning|Arena Random",
		meta = (EditCondition = "bUseArenaRandomSpawns", ClampMin = "0.0"))
	float ArenaInnerClearance = 280.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor Spawning|Arena Random",
		meta = (EditCondition = "bUseArenaRandomSpawns", ClampMin = "0.0"))
	float ArenaOuterClearance = 550.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor Spawning|Arena Random",
		meta = (EditCondition = "bUseArenaRandomSpawns", ClampMin = "0.0"))
	float MinimumAnchorSpacing = 600.0f;

	/**
	 * Ensures that at least one active Anchor is within GuaranteedAnchorDistance
	 * of the player. This avoids a random layout that makes the player cross the
	 * entire arena before they can use the core mechanic.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor Spawning|Arena Random|Reachability",
		meta = (EditCondition = "bUseArenaRandomSpawns"))
	bool bGuaranteeReachableAnchor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor Spawning|Arena Random|Reachability",
		meta = (EditCondition = "bUseArenaRandomSpawns && bGuaranteeReachableAnchor", ClampMin = "100.0"))
	float GuaranteedAnchorDistance = 1800.0f;

	/**
	 * Candidates within this fraction of the best coverage score may be chosen.
	 * A small value keeps coverage even while preventing a fixed radial pattern.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor Spawning|Arena Random|Coverage",
		meta = (EditCondition = "bUseArenaRandomSpawns", ClampMin = "0.0", ClampMax = "0.5"))
	float CoverageRandomness = 0.15f;

	/** Compensates for an imported Anchor mesh whose pivot is not at its base. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor Spawning|Arena Random",
		meta = (EditCondition = "bUseArenaRandomSpawns"))
	float ArenaSpawnHeightOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor Spawning|Arena Random",
		meta = (EditCondition = "bUseArenaRandomSpawns", ClampMin = "1"))
	int32 ArenaRandomAttempts = 48;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor Spawning",
		meta = (ToolTip = "Use -1 for a different layout each run. Use any non-negative value for a repeatable layout."))
	int32 RandomSeed = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor Spawning|Emergence",
		meta = (ClampMin = "0.0"))
	float EmergenceDepth = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor Spawning|Emergence",
		meta = (ClampMin = "0.01"))
	float EmergenceDuration = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor Spawning|Respawn",
		meta = (ClampMin = "0.0"))
	float ReplacementDelay = 2.0f;

private:
	struct FManagedAnchor
	{
		TWeakObjectPtr<AActor> Anchor;
		TWeakObjectPtr<AActor> SpawnPoint;
		FTransform FinalTransform;
		FVector StartLocation = FVector::ZeroVector;
		float EmergenceElapsed = 0.0f;
		bool bEmerging = true;
	};

	struct FPendingReplacement
	{
		TWeakObjectPtr<AActor> PreviousSpawnPoint;
		float RemainingDelay = 0.0f;
	};

	void CacheCandidatePoints();
	bool SpawnAnchorAtPoint(AActor& SpawnPoint);
	bool SpawnAnchorAtTransform(const FTransform& FinalTransform, AActor* SpawnPoint);
	bool SpawnAnchorAtRandomLocation();
	bool ChooseBalancedArenaAnchorLocation(
		bool bRequirePlayerReachability,
		FVector& OutLocation);
	AArenaFloorCollision* ResolveArenaFloorCollision();
	bool IsAnchorLocationClear(const FVector& CandidateLocation) const;
	float GetClosestAnchorDistanceSquared(const FVector& CandidateLocation) const;
	bool HasReachableActiveAnchor() const;
	AActor* ChooseAvailableSpawnPoint(const AActor* ExcludedPoint);
	void SetAnchorInteractionEnabled(AActor& Anchor, bool bEnabled) const;

	TArray<TWeakObjectPtr<AActor>> SpawnedAnchors;
	TArray<TWeakObjectPtr<AActor>> CandidateSpawnPoints;
	TArray<FManagedAnchor> ManagedAnchors;
	TArray<FPendingReplacement> PendingReplacements;
	FRandomStream RuntimeRandomStream;
};
