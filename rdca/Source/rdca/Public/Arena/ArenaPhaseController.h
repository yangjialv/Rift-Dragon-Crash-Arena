#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaPhaseController.generated.h"

class UBossEncounterComponent;
class UMaterialParameterCollection;

/**
 * Switches the arena from Cyber Rift to Source Code Void when the Boss enters
 * phase two.  Actors are paired by a shared PhaseMap_* Actor Tag, while their
 * side is identified with CyberRiftVisual or SourceCodeVoidVisual.
 */
UCLASS(BlueprintType, Blueprintable)
class RDCA_API AArenaPhaseController : public AActor
{
	GENERATED_BODY()

public:
	AArenaPhaseController();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Arena Phase")
	void StartPhaseTransition();

	UFUNCTION(BlueprintPure, Category = "Arena Phase")
	bool IsTransitionActive() const { return bTransitionActive; }

	UFUNCTION(BlueprintPure, Category = "Arena Phase")
	bool IsTransitionComplete() const { return bTransitionComplete; }

	UFUNCTION(BlueprintPure, Category = "Arena Phase")
	float GetExpansionRadius() const { return CurrentRadius; }

protected:
	virtual void BeginPlay() override;

	/** Assign the Empty Actor named ArenaPhaseOrigin here. It is the sphere centre. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Arena Phase|Setup")
	TObjectPtr<AActor> PhaseOriginActor;

	/** Optional. If left empty, the first Boss Encounter component in the level is used. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Arena Phase|Setup")
	TObjectPtr<AActor> BossActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Phase|Setup")
	bool bStartWhenBossEntersPhase2 = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Phase|Setup")
	bool bInitializeCyberRiftOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Phase|Timing", meta = (ClampMin = "0.1"))
	float ExpansionDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Phase|Timing", meta = (ClampMin = "1.0"))
	float MaximumRadius = 5000.0f;

	/** Whole-ring meshes have their pivot at the centre, so they use this explicit threshold. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Phase|Timing", meta = (ClampMin = "0.0"))
	float RingRevealRadius = 900.0f;

	/** Optional lead/lag applied to pillars and other mapped objects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Phase|Timing")
	float ObjectRevealRadiusOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Phase|Tags")
	FName PhaseMapTagPrefix = TEXT("PhaseMap_");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Phase|Tags")
	FName RingMappingTag = TEXT("PhaseMap_RingFloor");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Phase|Tags")
	FName CyberVisualTag = TEXT("CyberRiftVisual");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Phase|Tags")
	FName SourceVisualTag = TEXT("SourceCodeVoidVisual");

	/**
	 * Optional MPC interface for the final per-pixel sphere effect. Leave
	 * bUseMaterialSphereMask disabled until both environment materials use it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Phase|Material Sphere")
	bool bUseMaterialSphereMask = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Phase|Material Sphere", meta = (EditCondition = "bUseMaterialSphereMask"))
	TObjectPtr<UMaterialParameterCollection> PhaseMaterialParameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Phase|Material Sphere")
	FName SphereCenterParameter = TEXT("PhaseSphereCenter");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Phase|Material Sphere")
	FName SphereRadiusParameter = TEXT("PhaseSphereRadius");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Phase|Material Sphere")
	FName SphereEdgeWidthParameter = TEXT("PhaseSphereEdgeWidth");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Phase|Material Sphere", meta = (ClampMin = "0.0"))
	float SphereEdgeWidth = 150.0f;

private:
	struct FPhaseActorPair
	{
		TWeakObjectPtr<AActor> CyberActor;
		TWeakObjectPtr<AActor> SourceActor;
		FName MappingTag;
		bool bRevealed = false;
	};

	void ResolveReferences();
	void CacheEnvironmentActors();
	void InitializeCyberRift();
	void UpdateMaterialSphere() const;
	void RevealPair(FPhaseActorPair& Pair);
	void FinalizeSourceCodeVoid();
	void SetActorCollision(AActor* Actor, bool bEnabled) const;
	float GetRevealRadius(const FPhaseActorPair& Pair) const;
	bool IsRingPair(const FPhaseActorPair& Pair) const;

	TWeakObjectPtr<UBossEncounterComponent> BossEncounter;
	TArray<FPhaseActorPair> MappedPairs;
	TArray<TWeakObjectPtr<AActor>> CyberOnlyActors;
	TArray<TWeakObjectPtr<AActor>> SourceOnlyActors;
	FVector PhaseOrigin = FVector::ZeroVector;
	float CurrentRadius = 0.0f;
	float TransitionElapsed = 0.0f;
	bool bTransitionActive = false;
	bool bTransitionComplete = false;
};
