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
class APawn;
class ABossFanProjectile;
class ABossSweepLaser;
enum class EBossProjectileMotionMode : uint8;

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

UENUM(BlueprintType)
enum class EBossBarragePattern : uint8
{
	PredictiveVolley UMETA(DisplayName = "Predictive Triple Volley"),
	GapWall UMETA(DisplayName = "Gap Barrage Wall"),
	CurvedVolley UMETA(DisplayName = "Curved Spin Volley"),
	HomingVolley UMETA(DisplayName = "Limited Homing Volley"),
	RotatingGapWall UMETA(DisplayName = "Rotating Gap Barrage"),
	DoubleSpiral UMETA(DisplayName = "Double Spiral Barrage"),
	LegacyAimedVolley UMETA(DisplayName = "Legacy Aimed Volley"),
	LegacyDenseFan UMETA(DisplayName = "Legacy Dense Fan")
};

UENUM(BlueprintType)
enum class EBossPhase2Sequence : uint8
{
	DoubleShockwave UMETA(DisplayName = "Double Shockwave"),
	BarrageLaser UMETA(DisplayName = "Barrage Into Laser"),
	SpecialBarrage UMETA(DisplayName = "Special Barrage Finale")
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

	/** Temporary combat-debug override. Disable this to restore the configured weighted attack selection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Debug")
	bool bDebugForceSweepLaser = false;

	/** Repeats only the selected barrage pattern for isolated tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Debug")
	bool bDebugForceBarrage = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Debug",
		meta = (EditCondition = "bDebugForceBarrage"))
	EBossBarragePattern DebugBarragePattern =
		EBossBarragePattern::PredictiveVolley;

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

	/** The one projectile Blueprint used by all eight barrage patterns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Common",
		meta = (DisplayName = "Barrage Projectile Class"))
	TSubclassOf<ABossFanProjectile> FanProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Fan Barrage",
		meta = (ClampMin = "0.1"))
	float FanBarrageWarningDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Fan Barrage",
		meta = (ClampMin = "0.1"))
	float FanBarrageAttackDuration = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Common",
		meta = (ClampMin = "1.0"))
	float BarrageProjectileSpeed = 1600.0f;

	/** Runtime multiplier applied to every barrage pattern, including saved Blueprint speed values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Common",
		meta = (ClampMin = "0.1", DisplayName = "Barrage Speed Multiplier"))
	float BarrageSpeedMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Common",
		meta = (ClampMin = "1"))
	int32 BarrageProjectileDamage = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Predictive")
	int32 PredictiveWaveCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Predictive")
	int32 PredictiveProjectilesPerWave = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Predictive")
	float PredictiveWaveInterval = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Predictive")
	float PredictiveLeadTime = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Predictive")
	float PredictiveLateralSpacing = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Gap Wall")
	int32 GapWallWaveCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Gap Wall")
	int32 GapWallProjectileCount = 19;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Gap Wall")
	int32 GapWallSkippedProjectileCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Gap Wall")
	float GapWallArcDegrees = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Gap Wall")
	float GapWallWaveInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Curved")
	int32 CurvedWaveCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Curved")
	int32 CurvedProjectilesPerWave = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Curved")
	float CurvedArcDegrees = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Curved")
	float CurvedWaveInterval = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Curved")
	float CurveDegreesPerSecond = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Curved")
	float MaximumCurveDegrees = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Homing")
	int32 HomingProjectileCount = 3;

	/** Multiplies the authored count so existing Blueprint overrides are also upgraded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Homing",
		meta = (ClampMin = "1", DisplayName = "Homing Count Multiplier"))
	int32 HomingCountMultiplier = 2;

	/** Scales both the visual and collision of Limited Homing projectiles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Homing",
		meta = (ClampMin = "1.0", DisplayName = "Homing Size Multiplier"))
	float HomingSizeMultiplier = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Homing")
	float HomingSpreadDegrees = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Homing")
	float HomingProjectileSpeed = 1100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Homing")
	float HomingTurnDegreesPerSecond = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Homing")
	float HomingDuration = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Homing")
	float HomingStopDistance = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Homing")
	float MaximumHomingAngle = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Rotating Gap")
	int32 RotatingGapWaveCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Rotating Gap")
	float RotatingGapDegreesPerWave = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Double Spiral")
	int32 DoubleSpiralPairCount = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Double Spiral")
	float DoubleSpiralPairInterval = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Double Spiral")
	float DoubleSpiralDegreesPerPair = 9.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Legacy Aimed",
		meta = (ClampMin = "1"))
	int32 LegacyAimedProjectileCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Legacy Aimed",
		meta = (ClampMin = "0.01"))
	float LegacyAimedShotInterval = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Legacy Aimed",
		meta = (ClampMin = "0.0"))
	float LegacyAimedLateralSpacing = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Legacy Aimed",
		meta = (ClampMin = "1.0"))
	float LegacyAimedProjectileSpeed = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Legacy Dense Fan",
		meta = (ClampMin = "3"))
	int32 LegacyDenseFanProjectileCount = 21;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Legacy Dense Fan",
		meta = (ClampMin = "1.0", ClampMax = "179.0"))
	float LegacyDenseFanArcDegrees = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Legacy Dense Fan",
		meta = (ClampMin = "0.01"))
	float LegacyDenseFanShotInterval = 0.035f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Barrage|Legacy Dense Fan",
		meta = (ClampMin = "1.0"))
	float LegacyDenseFanProjectileSpeed = 1250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Phase 2",
		meta = (ClampMin = "0.1"))
	float Phase2InterAttackDelay = 0.65f;

	/** First-stage category weight. The three category weights are normalized at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Phase 2|Selection",
		meta = (ClampMin = "0.0", DisplayName = "Phase 2 Barrage Weight"))
	float Phase2BarrageWeight = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Phase 2|Selection",
		meta = (ClampMin = "0.0", DisplayName = "Phase 2 Shockwave Weight"))
	float Phase2ShockwaveWeight = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Phase 2|Selection",
		meta = (ClampMin = "0.0", DisplayName = "Phase 2 Laser Weight"))
	float Phase2LaserWeight = 0.2f;

	/** Number of unique attacks in one Phase 2 round before immediate stun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Phase 2",
		meta = (ClampMin = "1", DisplayName = "Phase 2 Attacks Before Stun"))
	int32 Phase2AttacksBeforeStun = 3;

	/** Boss enters Phase 2 as soon as its HP reaches this value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Phase 2",
		meta = (ClampMin = "1", DisplayName = "Phase 2 Start Hit Points"))
	int32 Phase2StartHitPoints = 3;

	/** Release delay between the two overlapping pulses of the enhanced Phase 2 shockwave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Phase 2",
		meta = (ClampMin = "0.0", DisplayName = "Phase 2 Shockwave Pulse Delay"))
	float Phase2ShockwavePulseDelay = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Phase 2",
		meta = (ClampMin = "0.1"))
	float Phase2WeakPointExposedDuration = 2.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Laser Attack",
		meta = (ClampMin = "0.1"))
	float LaserAimWarningDuration = 1.4f;

	/** Time after the warning direction locks and before the player side is sampled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Laser Attack",
		meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float LaserPostWarningPauseDuration = 0.40f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Laser Attack",
		meta = (ClampMin = "0.1"))
	float LaserActiveSweepDuration = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Laser Attack",
		meta = (ClampMin = "1.0", ClampMax = "300.0"))
	float LaserActiveSweepDegrees = 80.0f;

	/** How far the entire Boss descends when laser warning begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Laser Attack",
		meta = (ClampMin = "0.0", DisplayName = "Laser Warning Descent Distance"))
	float LaserWarningDescentDistance = 300.0f;

	/** Time used to reach the lowered laser firing height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Laser Attack",
		meta = (ClampMin = "0.01", DisplayName = "Laser Warning Descent Duration"))
	float LaserWarningDescentDuration = 0.6f;

	/** Time used to return to the normal flying height during recovery. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Laser Attack",
		meta = (ClampMin = "0.01", DisplayName = "Laser Recovery Ascent Duration"))
	float LaserRecoveryAscentDuration = 0.6f;

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

	/**
	 * Optional level-space center for the shockwave. When assigned, this takes
	 * priority over the ShockwaveOrigin component on the Boss, so a flying or
	 * descending Boss never moves the ground attack's visible ring or damage.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Boss|Encounter|Shockwave")
	TObjectPtr<AActor> ShockwaveWorldAnchor;

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
	bool IsEnhancedDoubleShockwaveAttack() const;
	void BeginCurrentAttackWarning();
	void BeginCurrentAttack();
	void FinishCurrentAttack();
	void TickShockwaveAttack();
	void BeginSecondShockwavePulse();
	void UpdateShockwave(float NormalizedTime);
	void UpdateSecondaryShockwave(float NormalizedTime);
	float GetShockwaveExpansionDuration() const;
	void SetShockwaveVisualRadius(float Radius);
	void ApplyShockwaveVisualWidth(UStaticMeshComponent* Mesh, float Radius);
	void UpdateShockwaveEditorPreviewScales();
	void CreateShockwaveProceduralVisual();
	void UpdateShockwaveProceduralVisual(float ConfiguredRadius);
	void UpdateShockwaveProceduralVisualComponent(
		UProceduralMeshComponent* Visual,
		float ConfiguredRadius,
		bool& bMeshBuilt,
		int32& BuiltRadialSegments,
		int32& BuiltTubeSegments);
	void SetShockwaveProceduralVisualVisible(bool bVisible);
	void SetShockwaveProceduralVisualMaterial(UMaterialInterface* Material);
	void ResolveBossVisual();
	void CreateSphereMaskedBossPhaseVisual();
	void UpdateBossPhaseMaterial();
	void CreateShockwaveCollisionSegments();
	void UpdateShockwaveCollisionSegments(float RingCenterRadius);
	void SetShockwaveCollisionEnabled(bool bEnabled);
	void CreateShockwaveCollisionSegmentsFor(
		TArray<TObjectPtr<UBoxComponent>>& Segments,
		const TCHAR* NamePrefix);
	void UpdateShockwaveCollisionSegmentsFor(
		TArray<TObjectPtr<UBoxComponent>>& Segments,
		float RingCenterRadius);
	void SetShockwaveCollisionEnabledFor(
		TArray<TObjectPtr<UBoxComponent>>& Segments,
		bool bEnabled);
	void ApplyShockwaveOverlapDamage(AActor* OtherActor, bool bSecondaryPulse);

	UFUNCTION()
	void HandleShockwaveSegmentOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
	void UpdateWeakPointVisual(bool bExposed);

	UFUNCTION()
	void HandleWeakPointCrash(
		APawn* CrashingPawn,
		bool bWasEffective,
		const FHitResult& Hit);

	float GetCurrentStateDuration() const;
	int32 GetEffectivePhase2StartHitPoints() const;
	float GetPhase2InterAttackDelay() const;
	void SelectPhase1ScheduledAttack();
	void SetBarragePattern(EBossBarragePattern Pattern);
	void BeginBarrageAttack();
	void TickBarrage(float DeltaTime);
	void SpawnBarrageStep(int32 StepIndex);
	void SpawnPredictiveWave(int32 WaveIndex);
	void SpawnGapWallWave(int32 WaveIndex, bool bRotateGap);
	void SpawnCurvedWave(int32 WaveIndex);
	void SpawnHomingWave();
	void SpawnDoubleSpiralPair(int32 PairIndex);
	void SpawnLegacyAimedProjectile(int32 ShotIndex);
	void SpawnLegacyDenseFanProjectile(int32 ShotIndex);
	void SpawnSharedBarrageProjectile(
		const FVector& Direction,
		float Speed,
		EBossProjectileMotionMode MotionMode,
		float CurveRate = 0.0f,
		float MaxCurve = 0.0f,
		AActor* HomingTarget = nullptr,
		float SizeMultiplier = 1.0f);
	int32 GetBarrageStepCount() const;
	float GetBarrageStepInterval() const;
	float GetBarrageAttackDuration() const;
	bool IsWideBarragePattern(EBossBarragePattern Pattern) const;
	void SelectPhase2RoundAttack();
	void ResetPhase2Round();
	void SpawnLaserWarning();
	void UpdateBossFacing(float DeltaTime);
	void UpdateLaserWarning();
	void LockLaserSweep();
	void ConfigureLaserSweepAtPauseEnd();
	void BeginLaserVerticalMovement();
	void UpdateLaserVerticalMovement();
	void RestoreBossLaserHeight();
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
	TObjectPtr<UProceduralMeshComponent> SecondaryShockwaveProceduralVisual;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBoxComponent>> ShockwaveCollisionSegments;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBoxComponent>> SecondaryShockwaveCollisionSegments;
	FVector ShockwaveBaseScale = FVector::OneVector;
	float ShockwaveBaseWorldRadius = 50.0f;
	float ShockwaveWorldUnitsPerConfiguredUnit = 1.0f;
	bool bShockwaveProceduralMeshBuilt = false;
	int32 BuiltShockwaveVisualRadialSegments = 0;
	int32 BuiltShockwaveVisualTubeSegments = 0;
	bool bSecondaryShockwaveProceduralMeshBuilt = false;
	int32 BuiltSecondaryShockwaveVisualRadialSegments = 0;
	int32 BuiltSecondaryShockwaveVisualTubeSegments = 0;
	EBossEncounterState EncounterState = EBossEncounterState::Idle;
	EBossCombatPhase CombatPhase = EBossCombatPhase::Phase1;
	EBossAttackType CurrentAttack = EBossAttackType::None;
	EBossAttackType PreviousAttack = EBossAttackType::None;
	EPlayerSpatialState LastObservedPlayerState =
		EPlayerSpatialState::Grounded;
	FRandomStream AttackRandomStream;
	int32 ActiveAttackRandomSeed = 0;
	FVector LockedTargetLocation = FVector::ZeroVector;
	float LaserWarningCurrentYaw = 0.0f;
	bool bLaserAimLocked = false;
	float LaserBaseActorZ = 0.0f;
	bool bLaserHeightAdjusted = false;
	float StateElapsed = 0.0f;
	float PreviousShockwaveRadius = 0.0f;
	bool bPlayerDamagedThisAttack = false;
	bool bPlayerDamagedBySecondaryShockwave = false;
	int32 ActiveShockwavePulse = 0;
	bool bBossPhaseMaterialApplied = false;
	EBossCombatPhase LastAppliedBossMaterialPhase = EBossCombatPhase::Dead;
	bool bEncounterStopped = false;
	bool bIntroHold = false;
	int32 CompletedAttacksSinceExposure = 0;
	EBossBarragePattern ActiveBarragePattern =
		EBossBarragePattern::PredictiveVolley;
	int32 BarrageStepsFired = 0;
	float BarrageStepElapsed = 0.0f;
	bool bCurrentPhase2DoubleShockwave = false;
	TSet<int32> Phase2UsedAttackKeys;
	EBossBarragePattern PreviousBarragePattern =
		EBossBarragePattern::PredictiveVolley;
	bool bHasPreviousBarragePattern = false;
	TWeakObjectPtr<ABossSweepLaser> ActiveSweepLaser;
};
