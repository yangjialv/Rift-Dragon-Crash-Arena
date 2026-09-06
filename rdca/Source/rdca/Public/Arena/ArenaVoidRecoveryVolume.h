#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaVoidRecoveryVolume.generated.h"

class AActor;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EArenaVoidRecoveryMode : uint8
{
	/** Preserve the fall: the data updraft launches the player back to safety. */
	LaunchToSafety,
	/** For lethal/story voids, instantly return the player to a placed target. */
	ResetToSafety
};

/**
 * A placeable, circular fall-recovery volume. It deliberately has no dependency
 * on Combat Bounds or Floor Collision: its world placement defines both where a
 * fall becomes dangerous and where the player is recovered.
 */
UCLASS(BlueprintType, Blueprintable)
class RDCA_API AArenaVoidRecoveryVolume : public AActor
{
	GENERATED_BODY()

public:
	AArenaVoidRecoveryVolume();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena Void Recovery|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Hidden query-only cylinder. Its position in the level is the trigger position. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena Void Recovery|Components")
	TObjectPtr<UStaticMeshComponent> RecoveryVolume;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Void Recovery|Shape",
		meta = (ClampMin = "50.0"))
	float Radius = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Void Recovery|Shape",
		meta = (ClampMin = "10.0"))
	float HalfHeight = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Void Recovery|Response")
	EArenaVoidRecoveryMode RecoveryMode = EArenaVoidRecoveryMode::LaunchToSafety;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Void Recovery|Response",
		meta = (ClampMin = "0"))
	int32 Damage = 1;

	/** Optional exact safe landing point. Leave empty to use an outward automatic return. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Arena Void Recovery|Return")
	TObjectPtr<AActor> RecoveryTarget;

	/** Radial distance from this volume used when Recovery Target is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Void Recovery|Return",
		meta = (ClampMin = "0.0", EditCondition = "RecoveryTarget == nullptr"))
	float AutomaticReturnDistance = 1200.0f;

	/** Height above this volume used when Recovery Target is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Void Recovery|Return",
		meta = (EditCondition = "RecoveryTarget == nullptr"))
	float AutomaticReturnHeight = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Void Recovery|Launch",
		meta = (ClampMin = "0.0", EditCondition = "RecoveryMode == EArenaVoidRecoveryMode::LaunchToSafety"))
	float ReturnArcHeight = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Void Recovery|Launch",
		meta = (ClampMin = "0.05", EditCondition = "RecoveryMode == EArenaVoidRecoveryMode::LaunchToSafety"))
	float ReturnDuration = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Void Recovery|Response",
		meta = (ClampMin = "0.0"))
	float TriggerCooldown = 1.0f;

	/** Shows the cylinder only while arranging the level; it is never rendered in play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Void Recovery|Debug")
	bool bVisibleInEditor = true;

private:
	UFUNCTION()
	void HandleRecoveryOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	void UpdateRecoveryGeometry();
	FVector ResolveReturnDestination(const AActor& Player) const;

	TMap<TWeakObjectPtr<AActor>, float> TriggerCooldowns;
};
