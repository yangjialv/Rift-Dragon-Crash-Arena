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
	PreparingAttack,
	Attacking,
	Recovery,
	WeakPointExposed,
	PreparingFanAttack,
	FanAttacking,
	PreparingLaserAttack,
	LaserAttacking,
	Dead
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
	float AttackDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Timing",
		meta = (ClampMin = "0.0"))
	float RecoveryDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Timing",
		meta = (ClampMin = "0.1"))
	float WeakPointExposedDuration = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Fan Attack",
		meta = (ClampMin = "0.1"))
	float FanWarningDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Fan Attack",
		meta = (ClampMin = "0.1"))
	float FanAttackDuration = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Fan Attack",
		meta = (ClampMin = "1", ClampMax = "21"))
	int32 FanProjectileCount = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Fan Attack",
		meta = (ClampMin = "0.0", ClampMax = "170.0"))
	float FanSpreadDegrees = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Fan Attack",
		meta = (ClampMin = "1.0"))
	float FanProjectileSpeed = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Fan Attack",
		meta = (ClampMin = "1"))
	int32 FanProjectileDamage = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Encounter|Fan Attack")
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
	void UpdateShockwave(float NormalizedTime);
	void TryDamagePlayer(float PreviousRadius, float CurrentRadius);
	void UpdateWeakPointVisual(bool bExposed);
	float GetCurrentStateDuration() const;
	void SpawnFanProjectiles();
	void SpawnLaserWarning();

	TWeakObjectPtr<UStaticMeshComponent> ShockwaveVisual;
	TWeakObjectPtr<UStaticMeshComponent> WeakPointVisual;
	TWeakObjectPtr<UBossWeakPointComponent> WeakPoint;
	FVector ShockwaveBaseScale = FVector::OneVector;
	EBossEncounterState EncounterState = EBossEncounterState::Idle;
	float StateElapsed = 0.0f;
	float PreviousShockwaveRadius = 0.0f;
	bool bPlayerDamagedThisAttack = false;
	bool bEncounterStopped = false;
	int32 NextAttackPattern = 1;
	TWeakObjectPtr<ABossSweepLaser> ActiveSweepLaser;
};
