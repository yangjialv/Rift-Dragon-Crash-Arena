#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BossEncounterComponent.generated.h"

class UBossWeakPointComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;
class UMeshComponent;
class USkeletalMeshComponent;
class UBoxComponent;
class UPrimitiveComponent;
class UProceduralMeshComponent;
class AActor;
class ABossFanProjectile;
class ABossSweepLaser;

UENUM(BlueprintType)
enum class EBossEncounterState : uint8
{
	Idle,
	SelectingAttack,
	Preparing,
	Attacking,
	Recovery,
	WeakPointExposed,
	Dead
};

UENUM(BlueprintType)
enum class EBossAttackType : uint8
{
	None,
	Shockwave,
	AimedVolley,
	FanBarrage,
	SweepLaser
};

UENUM(BlueprintType)
enum class EBossCombatPhase : uint8
{
	Phase1,
	Phase2,
	Dead
};

UENUM(BlueprintType)
enum class EPlayerSpatialState : uint8
{
	Grounded,
	Airborne,
	Attached
};

USTRUCT(BlueprintType)
struct FBossAttackWeights
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Selection",
		meta = (ClampMin = "0.0"))
	float Shockwave = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Selection",
		meta = (ClampMin = "0.0"))
	float AimedVolley = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Selection",
		meta = (ClampMin = "0.0"))
	float SweepLaser = 20.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnBossEncounterStateChanged,
	EBossEncounterState,
	PreviousState,
	EBossEncounterState,
	NewState);

UCLASS(
	ClassGroup = (Boss),
	meta = (BlueprintSpawnableComponent, DisplayName = "Boss Encounter"))
class RDCA_API UBossEncounterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossEncounterComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(
		FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
	UFUNCTION(BlueprintPure, Category = "Boss|Encounter")
	EBossEncounterState GetEncounterState() const { return EncounterState; }

	UFUNCTION(BlueprintPure, Category = "Boss|Encounter")
	EBossAttackType GetCurrentAttack() const { return CurrentAttack; }

	UFUNCTION(BlueprintPure, Category = "Boss|Encounter")
	EBossCombatPhase GetCombatPhase() const;

	UFUNCTION(BlueprintPure, Category = "Boss|Encounter")
	EPlayerSpatialState GetLastObservedPlayerState() const
	{
		return LastObservedPlayerState;
	}

	UFUNCTION(BlueprintPure, Category = "Boss|Encounter")
	float GetStateProgress() const;

	UFUNCTION(BlueprintPure, Category = "Boss|Encounter")
	float GetStateRemainingTime() const;

	/** Returns the ShockwaveVisual scale required to render a ring at TargetRadius. */
	UFUNCTION(BlueprintPure, Category = "Boss|Encounter|Shockwave")
	FVector GetShockwaveVisualScaleForRadius(float TargetRadius) const;

	UFUNCTION(BlueprintCallable, Category = "Boss|Encounter")
	void StopEncounter();

	UFUNCTION(BlueprintCallable, Category = "Boss|Encounter|Intro")
	void SetIntroHold(bool bHold);

	UFUNCTION(BlueprintPure, Category = "Boss|Encounter|Intro")
	bool IsIntroHeld() const { return bIntroHold; }

	// Pauses the state machine for a short in-world transition.  SetIntroHold is
	// kept for the existing intro Blueprint and calls through to this function.
	UFUNCTION(BlueprintCallable, Category = "Boss|Encounter")
	void SetEncounterHold(bool bHold);

	UFUNCTION(BlueprintPure, Category = "Boss|Encounter")
	bool IsEncounterHeld() const { return bIntroHold; }

	UPROPERTY(BlueprintAssignable, Category = "Boss|Encounter")
	FOnBossEncounterStateChanged OnEncounterStateChanged;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Timing",
		meta = (ClampMin = "0.0"))
	float InitialIdleDuration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Timing",
		meta = (ClampMin = "0.1"))
	float WarningDuration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Facing",
		meta = (ClampMin = "1.0", ClampMax = "720.0"))
	float BossFacingRotationSpeed = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Timing",
		meta = (ClampMin = "0.0"))
	float RecoveryDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Timing",
		meta = (ClampMin = "0.1"))
	float WeakPointExposedDuration = 3.0f;

	/** Applied to both Phase 1 and Phase 2 weak-point exposure (Boss stun) windows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Timing",
		meta = (ClampMin = "0.1"))
	float WeakPointStunDurationMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Selection",
		meta = (ClampMin = "1"))
	int32 AttacksBeforeWeakPointExposure = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Selection")
	int32 AttackSelectionRandomSeed = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Selection")
	FBossAttackWeights GroundedAttackWeights;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Selection")
	FBossAttackWeights AirborneAttackWeights;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Selection")
	FBossAttackWeights AttachedAttackWeights;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Aimed Volley",
		meta = (ClampMin = "0.1"))
	float AimedVolleyWarningDuration = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Aimed Volley",
		meta = (ClampMin = "0.1"))
	float AimedVolleyAttackDuration = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Aimed Volley",
		meta = (ClampMin = "1", ClampMax = "9"))
	int32 PrecisionVolleyProjectileCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Aimed Volley",
		meta = (ClampMin = "0.01"))
	float PrecisionVolleyShotInterval = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Aimed Volley",
		meta = (ClampMin = "0.0"))
	float PrecisionVolleyLateralSpacing = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Aimed Volley",
		meta = (ClampMin = "1.0"))
	float AimedVolleyProjectileSpeed = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Aimed Volley",
		meta = (ClampMin = "1"))
	int32 AimedVolleyProjectileDamage = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Aimed Volley")
	TSubclassOf<ABossFanProjectile> FanProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Fan Barrage",
		meta = (ClampMin = "0.1"))
	float FanBarrageWarningDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Fan Barrage",
		meta = (ClampMin = "0.1"))
	float FanBarrageAttackDuration = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Fan Barrage",
		meta = (ClampMin = "3", ClampMax = "31"))
	int32 DenseFanProjectileCount = 21;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Fan Barrage",
		meta = (ClampMin = "1.0", ClampMax = "170.0"))
	float DenseFanArcDegrees = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Fan Barrage",
		meta = (ClampMin = "0.01"))
	float DenseFanShotInterval = 0.035f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Fan Barrage",
		meta = (ClampMin = "1.0"))
	float DenseFanProjectileSpeed = 1250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Fan Barrage",
		meta = (ClampMin = "1"))
	int32 FanBarrageProjectileDamage = 1;

	/** Optional visual variant. Falls back to FanProjectileClass when unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Fan Barrage")
	TSubclassOf<ABossFanProjectile> FanBarrageProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Phase 2",
		meta = (ClampMin = "0.1"))
	float Phase2InterAttackDelay = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Phase 2",
		meta = (ClampMin = "0.1"))
	float Phase2WeakPointExposedDuration = 2.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Laser Attack",
		meta = (ClampMin = "0.1"))
	float LaserAimWarningDuration = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Laser Attack",
		meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float LaserAimLockDuration = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Laser Attack",
		meta = (ClampMin = "0.1"))
	float LaserActiveSweepDuration = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Laser Attack",
		meta = (ClampMin = "1.0", ClampMax = "300.0"))
	float LaserActiveSweepDegrees = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Laser Attack",
		meta = (ClampMin = "1"))
	int32 LaserDamage = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Laser Attack")
	TSubclassOf<ABossSweepLaser> SweepLaserClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Shockwave",
		meta = (ClampMin = "0.0", ToolTip = "Radius of the shockwave ring when its active expansion begins."))
	float ShockwaveInitialRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Shockwave",
		meta = (ClampMin = "1.0"))
	float ShockwaveExpandedMaximumRadius = 3600.0f;

	/** World units per second. The active duration is derived from the initial and maximum radii. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Shockwave",
		meta = (ClampMin = "1.0"))
	float ShockwaveExpansionSpeed = 1500.0f;

	/** Total radial gameplay width of the damaging band, rather than a per-side tolerance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Shockwave",
		meta = (ClampMin = "0.0", ToolTip = "Total width of the damage band in world units."))
	float ShockwaveGameplayWidth = 120.0f;

	/** Internal scalar name used by the legacy editor-only Plane previews. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Shockwave|Visual",
		meta = (ClampMin = "0.0", ToolTip = "Visual ring width. Requires this scalar parameter in both shockwave materials."))
	float ShockwaveVisualWidth = 120.0f;

	FName ShockwaveVisualWidthParameter = TEXT("RingWidth");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Shockwave",
		meta = (ClampMin = "1.0", DisplayName = "Shockwave Height",
			ToolTip = "Total height of the damaging shockwave volume. The Torus visual should use this same height."))
	float GroundDamageMaximumHeight = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Boss|Encounter|Shockwave|Collision",
		meta = (ClampMin = "8", ClampMax = "48",
			ToolTip = "Number of low-cost overlap segments used to form the circular shockwave collision band."))
	int32 ShockwaveCollisionSegmentCount = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Boss|Encounter|Shockwave|Collision",
		meta = (ClampMin = "0.0",
			ToolTip = "Small world-space overlap between collision segments, preventing gaps as the ring expands."))
	float ShockwaveCollisionSegmentOverlap = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Shockwave",
		meta = (ClampMin = "1"))
	int32 ShockwaveDamage = 1;

	/** Material used by the runtime-generated fire ring during its warning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Shockwave|Fire Visual")
	TObjectPtr<UMaterialInterface> ShockwaveFireWarningMaterial;

	/** Material used by the runtime-generated fire ring during its damaging pass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Shockwave|Fire Visual")
	TObjectPtr<UMaterialInterface> ShockwaveFireActiveMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Boss|Encounter|Shockwave|Fire Visual",
		meta = (ClampMin = "12", ClampMax = "64", ToolTip = "Roundness of the generated fire ring."))
	int32 ShockwaveVisualRadialSegments = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Boss|Encounter|Shockwave|Fire Visual",
		meta = (ClampMin = "4", ClampMax = "16", ToolTip = "Roundness of the fire ring's tube cross-section."))
	int32 ShockwaveVisualTubeSegments = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Visual")
	TObjectPtr<UMaterialInterface> WeakPointProtectedMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Visual")
	TObjectPtr<UMaterialInterface> WeakPointExposedMaterial;

	/** Optional explicit Boss render component. Leave BossMesh as the default, or use the exact component name in BP. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Phase Material")
	FName BossVisualComponentName = TEXT("BossMesh");

	/** Material slot on the Boss Skeletal Mesh that represents its main body. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Phase Material",
		meta = (ClampMin = "0"))
	int32 BossBodyMaterialSlot = 0;

	/** Optional. Empty preserves the Skeletal Mesh's authored initial material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Phase Material")
	TObjectPtr<UMaterialInterface> BossPhase1Material;

	/** Applied to BossBodyMaterialSlot as soon as the encounter enters Phase 2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Phase Material")
	TObjectPtr<UMaterialInterface> BossPhase2Material;

	/**
	 * Renders an animation-synchronised Phase 2 copy of the Boss. Use only
	 * after the Phase 1 and Phase 2 Boss materials both implement the arena
	 * sphere mask; this makes the expanding sphere reveal the Boss per pixel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Phase Material")
	bool bUseSphereMaskedBossPhaseTransition = false;

private:
	void SetEncounterState(EBossEncounterState NewState);
	void SelectNextAttack();
	EBossAttackType ChooseWeightedAttack(
		const FBossAttackWeights& Weights);
	EPlayerSpatialState ObservePlayerSpatialState() const;
	const FBossAttackWeights& GetWeightsForPlayerState(
		EPlayerSpatialState PlayerState) const;
	float GetCurrentAttackWarningDuration() const;
	float GetCurrentAttackActiveDuration() const;
	void BeginCurrentAttackWarning();
	void BeginCurrentAttack();
	void FinishCurrentAttack();
	void UpdateShockwave(float NormalizedTime);
	float GetShockwaveExpansionDuration() const;
	void SetShockwaveVisualRadius(float Radius);
	void ApplyShockwaveVisualWidth(UStaticMeshComponent* Mesh, float Radius);
	void UpdateShockwaveEditorPreviewScales();
	void CreateShockwaveProceduralVisual();
	void UpdateShockwaveProceduralVisual(float ConfiguredRadius);
	void SetShockwaveProceduralVisualVisible(bool bVisible);
	void SetShockwaveProceduralVisualMaterial(UMaterialInterface* Material);
	void ResolveBossVisual();
	void CreateSphereMaskedBossPhaseVisual();
	void UpdateBossPhaseMaterial();
	void CreateShockwaveCollisionSegments();
	void UpdateShockwaveCollisionSegments(float RingCenterRadius);
	void SetShockwaveCollisionEnabled(bool bEnabled);
	void ApplyShockwaveOverlapDamage(AActor* OtherActor);

	UFUNCTION()
	void HandleShockwaveSegmentOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
	void UpdateWeakPointVisual(bool bExposed);
	float GetCurrentStateDuration() const;
	void TickAimedVolley(float DeltaTime);
	void SpawnAimedVolleyProjectile(int32 ShotIndex);
	void TickFanBarrage(float DeltaTime);
	void SpawnFanBarrageProjectile(int32 ShotIndex);
	void SelectPhase2Combo();
	void BeginPhase2SecondAttack();
	void SpawnLaserWarning();
	void UpdateBossFacing(float DeltaTime);
	void UpdateLaserWarning();
	void LockLaserSweep();
	USceneComponent* FindNamedSceneComponent(FName ComponentName) const;
	FVector GetProjectileOriginLocation() const;
	FVector GetLaserOriginLocation() const;
	FVector GetShockwaveOriginLocation() const;

	TWeakObjectPtr<UStaticMeshComponent> ShockwaveVisual;
	TWeakObjectPtr<UStaticMeshComponent> WeakPointVisual;
	TWeakObjectPtr<UBossWeakPointComponent> WeakPoint;
	TWeakObjectPtr<UMeshComponent> BossVisual;
	TObjectPtr<USkeletalMeshComponent> BossPhase2SphereVisual;
	TWeakObjectPtr<USceneComponent> ProjectileOrigin;
	TWeakObjectPtr<USceneComponent> LaserOrigin;
	TWeakObjectPtr<USceneComponent> ShockwaveOrigin;
	TWeakObjectPtr<USceneComponent> WeakPointOrigin;
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> ShockwaveProceduralVisual;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBoxComponent>> ShockwaveCollisionSegments;
	FVector ShockwaveBaseScale = FVector::OneVector;
	float ShockwaveBaseWorldRadius = 50.0f;
	float ShockwaveWorldUnitsPerConfiguredUnit = 1.0f;
	bool bShockwaveProceduralMeshBuilt = false;
	int32 BuiltShockwaveVisualRadialSegments = 0;
	int32 BuiltShockwaveVisualTubeSegments = 0;
	EBossEncounterState EncounterState = EBossEncounterState::Idle;
	EBossCombatPhase CombatPhase = EBossCombatPhase::Phase1;
	EBossAttackType CurrentAttack = EBossAttackType::None;
	EBossAttackType PreviousAttack = EBossAttackType::None;
	EPlayerSpatialState LastObservedPlayerState =
		EPlayerSpatialState::Grounded;
	FRandomStream AttackRandomStream;
	int32 ActiveAttackRandomSeed = 0;
	FVector LockedTargetLocation = FVector::ZeroVector;
	FVector LaserWarningInitialPlayerLocation = FVector::ZeroVector;
	float LaserWarningCurrentYaw = 0.0f;
	bool bLaserAimLocked = false;
	float StateElapsed = 0.0f;
	float PreviousShockwaveRadius = 0.0f;
	bool bPlayerDamagedThisAttack = false;
	bool bBossPhaseMaterialApplied = false;
	EBossCombatPhase LastAppliedBossMaterialPhase = EBossCombatPhase::Dead;
	bool bEncounterStopped = false;
	bool bIntroHold = false;
	int32 CompletedAttacksSinceExposure = 0;
	int32 AimedVolleyShotsFired = 0;
	float AimedVolleyShotElapsed = 0.0f;
	int32 FanBarrageShotsFired = 0;
	float FanBarrageShotElapsed = 0.0f;
	bool bPhase2ComboActive = false;
	bool bPhase2AnchorPressureCombo = false;
	bool bHasSelectedPhase2Combo = false;
	bool bPreviousPhase2AnchorPressureCombo = false;
	int32 Phase2ComboStep = 0;
	TWeakObjectPtr<ABossSweepLaser> ActiveSweepLaser;
};
