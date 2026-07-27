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

protected:
	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable, Category = "Anchor Spawning")
	void SpawnAnchors();

	UFUNCTION(BlueprintCallable, Category = "Anchor Spawning")
	void ClearSpawnedAnchors();

	UFUNCTION(BlueprintPure, Category = "Anchor Spawning")
	int32 GetSpawnedAnchorCount() const;

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

private:
	TArray<TWeakObjectPtr<AActor>> SpawnedAnchors;
};
