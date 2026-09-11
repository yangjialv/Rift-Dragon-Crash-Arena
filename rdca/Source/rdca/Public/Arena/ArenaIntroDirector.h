#pragma once

#include "Boss/BossAnimationComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaIntroDirector.generated.h"

class AAnchorSpawnManager;

/**
 * Runs the non-interactive opening: Dragon takeoff, Cyber centre fracture,
 * then releases player control and begins Anchor spawning.
 */
UCLASS(Blueprintable)
class RDCA_API AArenaIntroDirector : public AActor
{
	GENERATED_BODY()

public:
	AArenaIntroDirector();
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Arena Intro|Setup")
	TObjectPtr<AActor> BossActor;

	/**
	 * Place an Empty Actor at the desired final world transform of the flying
	 * Boss. It avoids tying combat height to a particular dragon mesh pivot.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Arena Intro|Boss Descent")
	TObjectPtr<AActor> BossCombatHoverTarget;

	/** Smoothly lower the Boss from its takeoff position before combat begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Intro|Boss Descent")
	bool bDescendBossAfterTakeoff = true;

	/** Time from the takeoff's final pose to the configured combat hover point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Intro|Boss Descent",
		meta = (ClampMin = "0.0"))
	float BossDescentDuration = 1.8f;

	/** The Phase-1 Cyber centre disc beneath the Dragon. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Arena Intro|Setup")
	TObjectPtr<AActor> CyberCenterActor;

	/** Optional Geometry Collection Blueprint spawned when the Dragon leaves the centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Intro|Fracture")
	TSubclassOf<AActor> CenterFractureActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Intro|Fracture",
		meta = (ClampMin = "0.1"))
	float CenterFractureLifeSpan = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Intro|Player")
	bool bLockPlayerDuringIntro = true;

	/** Clears early Anchor spawns and creates the full layout when Intro finishes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena Intro|Anchors")
	bool bSpawnAnchorsAfterIntro = true;

	UFUNCTION()
	void HandleBossAnimationEvent(EBossAnimationEvent AnimationEvent);

private:
	void ResolveReferences();
	void CacheAnchorManagers();
	void LockPlayer();
	void UnlockPlayer();
	void ShatterCyberCenter();
	void BeginBossDescent();
	void TickBossDescent(float DeltaTime);
	void CompleteIntro();

	TWeakObjectPtr<UBossAnimationComponent> BossAnimation;
	TArray<TWeakObjectPtr<AAnchorSpawnManager>> AnchorManagers;
	bool bCyberCenterShattered = false;
	bool bIntroCompleted = false;
	bool bBossDescentActive = false;
	FVector BossDescentStart = FVector::ZeroVector;
	FVector BossDescentEnd = FVector::ZeroVector;
	float BossDescentElapsed = 0.0f;
};
