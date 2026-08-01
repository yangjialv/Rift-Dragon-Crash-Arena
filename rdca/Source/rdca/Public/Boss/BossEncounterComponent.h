#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BossEncounterComponent.generated.h"

class UBossWeakPointComponent;
class UMaterialInterface;
class UStaticMeshComponent;
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

	UFUNCTION(BlueprintCallable, Category = "Boss|Encounter")
	void StopEncounter();

	UPROPERTY(BlueprintAssignable, Category = "Boss|Encounter")
	FOnBossEncounterStateChanged OnEncounterStateChanged;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Timing",
		meta = (ClampMin = "0.0"))
	float InitialIdleDuration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Timing",
		meta = (ClampMin = "0.1"))
	float WarningDuration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Timing",
		meta = (ClampMin = "0.1"))
	float ShockwaveAttackDuration = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Timing",
		meta = (ClampMin = "0.0"))
	float RecoveryDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Timing",
		meta = (ClampMin = "0.1"))
	float WeakPointExposedDuration = 3.0f;

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
	int32 AimedVolleyProjectileCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Aimed Volley",
		meta = (ClampMin = "0.01"))
	float AimedVolleyShotInterval = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Aimed Volley",
		meta = (ClampMin = "0.0"))
	float AimedVolleyLateralSpacing = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Aimed Volley",
		meta = (ClampMin = "1.0"))
	float AimedVolleyProjectileSpeed = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Aimed Volley",
		meta = (ClampMin = "1"))
	int32 AimedVolleyProjectileDamage = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Aimed Volley")
	TSubclassOf<ABossFanProjectile> FanProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Laser Attack",
		meta = (ClampMin = "0.1"))
	float LaserWarningDuration = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Laser Attack",
		meta = (ClampMin = "0.1"))
	float LaserSweepDuration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Laser Attack",
		meta = (ClampMin = "1.0", ClampMax = "300.0"))
	float LaserSweepDegrees = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Laser Attack",
		meta = (ClampMin = "1"))
	int32 LaserDamage = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Laser Attack")
	TSubclassOf<ABossSweepLaser> SweepLaserClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Shockwave",
		meta = (ClampMin = "1.0"))
	float ShockwaveMaximumRadius = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Shockwave",
		meta = (ClampMin = "0.0"))
	float GroundDamageMaximumHeight = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Shockwave",
		meta = (ClampMin = "0.0",
			ToolTip = "Gameplay thickness added around the expanding shockwave radius to avoid frame-step misses."))
	float ShockwaveHitTolerance = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Shockwave",
		meta = (ClampMin = "1"))
	int32 ShockwaveDamage = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Visual")
	TObjectPtr<UMaterialInterface> ShockwaveWarningMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Visual")
	TObjectPtr<UMaterialInterface> ShockwaveActiveMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Visual")
	TObjectPtr<UMaterialInterface> WeakPointProtectedMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Visual")
	TObjectPtr<UMaterialInterface> WeakPointExposedMaterial;

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
	void TryDamagePlayer(float PreviousRadius, float CurrentRadius);
	void UpdateWeakPointVisual(bool bExposed);
	float GetCurrentStateDuration() const;
	void TickAimedVolley(float DeltaTime);
	void SpawnAimedVolleyProjectile(int32 ShotIndex);
	void SpawnLaserWarning();

	TWeakObjectPtr<UStaticMeshComponent> ShockwaveVisual;
	TWeakObjectPtr<UStaticMeshComponent> WeakPointVisual;
	TWeakObjectPtr<UBossWeakPointComponent> WeakPoint;
	FVector ShockwaveBaseScale = FVector::OneVector;
	EBossEncounterState EncounterState = EBossEncounterState::Idle;
	EBossAttackType CurrentAttack = EBossAttackType::None;
	EBossAttackType PreviousAttack = EBossAttackType::None;
	EPlayerSpatialState LastObservedPlayerState =
		EPlayerSpatialState::Grounded;
	FRandomStream AttackRandomStream;
	int32 ActiveAttackRandomSeed = 0;
	FVector LockedTargetLocation = FVector::ZeroVector;
	float StateElapsed = 0.0f;
	float PreviousShockwaveRadius = 0.0f;
	bool bPlayerDamagedThisAttack = false;
	bool bEncounterStopped = false;
	int32 CompletedAttacksSinceExposure = 0;
	int32 AimedVolleyShotsFired = 0;
	float AimedVolleyShotElapsed = 0.0f;
	TWeakObjectPtr<ABossSweepLaser> ActiveSweepLaser;
};
