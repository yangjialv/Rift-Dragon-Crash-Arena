#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "Engine/HitResult.h"
#include "BossWeakPointComponent.generated.h"

class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnBossHitPointsChanged,
	int32,
	CurrentHitPoints,
	int32,
	MaximumHitPoints);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnBossWeakPointExposureChanged,
	bool,
	bIsExposed);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnBossWeakPointCrash,
	APawn*,
	CrashingPawn,
	bool,
	bWasEffective,
	const FHitResult&,
	Hit);

UCLASS(
	ClassGroup = (Boss),
	meta = (BlueprintSpawnableComponent, DisplayName = "Boss Weak Point"))
class RDCA_API UBossWeakPointComponent : public USphereComponent
{
	GENERATED_BODY()

public:
	UBossWeakPointComponent(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable, Category = "Boss|Weak Point")
	void SetExposed(bool bNewExposed);

	UFUNCTION(BlueprintCallable, Category = "Boss|Weak Point")
	void ResetWeakPoint();

	UFUNCTION(BlueprintPure, Category = "Boss|Weak Point")
	bool IsExposed() const { return bExposed; }

	UFUNCTION(BlueprintPure, Category = "Boss|Weak Point")
	bool IsBossDefeated() const { return CurrentHitPoints <= 0; }

	UFUNCTION(BlueprintPure, Category = "Boss|Weak Point")
	int32 GetCurrentHitPoints() const { return CurrentHitPoints; }

	UFUNCTION(BlueprintPure, Category = "Boss|Weak Point")
	int32 GetMaximumHitPoints() const { return MaximumHitPoints; }

	UFUNCTION(BlueprintPure, Category = "Boss|Weak Point")
	float GetHitPointsPercent() const
	{
		return MaximumHitPoints > 0
			? static_cast<float>(CurrentHitPoints) / MaximumHitPoints
			: 0.0f;
	}

	bool ReceiveCrash(
		APawn* CrashingPawn,
		const FHitResult& Hit,
		bool bIsQualifiedHeavyCrash);

	UPROPERTY(BlueprintAssignable, Category = "Boss|Weak Point")
	FOnBossHitPointsChanged OnHitPointsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Boss|Weak Point")
	FOnBossWeakPointExposureChanged OnExposureChanged;

	UPROPERTY(BlueprintAssignable, Category = "Boss|Weak Point")
	FOnBossWeakPointCrash OnWeakPointCrash;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Weak Point",
		meta = (ClampMin = "1"))
	int32 MaximumHitPoints = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Weak Point")
	bool bExposed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Weak Point",
		meta = (ClampMin = "1"))
	int32 DamagePerQualifiedCrash = 1;

	/** Multiplies the three layered weak-point impact effects, but not the Boss pain roar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Weak Point|Audio",
		meta = (ClampMin = "0.0", DisplayName = "Weak Point Impact Volume Multiplier"))
	float WeakPointImpactVolumeMultiplier = 4.0f;

private:
	int32 CurrentHitPoints = 5;
};
