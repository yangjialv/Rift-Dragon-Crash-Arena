#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AnchorSpawnManager.generated.h"

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
	int32 NumberOfAnchors = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor Spawning")
	FName SpawnPointTag = TEXT("AnchorSpawnPoint");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor Spawning")
	bool bSpawnOnBeginPlay = true;

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
	AActor* ChooseAvailableSpawnPoint(const AActor* ExcludedPoint);
	void SetAnchorInteractionEnabled(AActor& Anchor, bool bEnabled) const;

	TArray<TWeakObjectPtr<AActor>> SpawnedAnchors;
	TArray<TWeakObjectPtr<AActor>> CandidateSpawnPoints;
	TArray<FManagedAnchor> ManagedAnchors;
	TArray<FPendingReplacement> PendingReplacements;
	FRandomStream RuntimeRandomStream;
};
