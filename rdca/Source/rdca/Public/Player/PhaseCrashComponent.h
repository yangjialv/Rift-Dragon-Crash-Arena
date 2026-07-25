#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PhaseCrashComponent.generated.h"

class APawn;

UENUM(BlueprintType)
enum class EPhaseCrashState : uint8
{
	Ready,
	Charging,
	Crashing,
	Recovery,
	Cooldown
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnPhaseCrashStateChanged,
	EPhaseCrashState,
	PreviousState,
	EPhaseCrashState,
	NewState);

UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class RDCA_API UPhaseCrashComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPhaseCrashComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Phase Crash")
	void StartCharging();

	UFUNCTION(BlueprintCallable, Category = "Phase Crash")
	void ReleaseCrash();

	UFUNCTION(BlueprintCallable, Category = "Phase Crash")
	void CancelCharging();

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	EPhaseCrashState GetCrashState() const { return CrashState; }

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	bool IsCrashing() const { return CrashState == EPhaseCrashState::Crashing; }

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	float GetChargeAlpha() const;

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	float GetCooldownRemaining() const { return CooldownRemaining; }

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	float GetMovementInputScale() const;

	UPROPERTY(BlueprintAssignable, Category = "Phase Crash")
	FOnPhaseCrashStateChanged OnCrashStateChanged;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Charge",
		meta = (ClampMin = "0.05"))
	float MaxChargeTime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Movement",
		meta = (ClampMin = "0.0"))
	float MinCrashDistance = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Movement",
		meta = (ClampMin = "0.0"))
	float MaxCrashDistance = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Movement",
		meta = (ClampMin = "1.0"))
	float CrashSpeed = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Timing",
		meta = (ClampMin = "0.0"))
	float RecoveryDuration = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Timing",
		meta = (ClampMin = "0.0"))
	float CooldownDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Charge",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ChargingMovementScale = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Debug")
	bool bDrawDebugAim = true;

private:
	bool UpdateAimTarget();
	void TickCrash(float DeltaTime);
	void TickRecovery(float DeltaTime);
	void TickCooldown(float DeltaTime);
	void FinishCrash();
	void SetCrashState(EPhaseCrashState NewState);

	TObjectPtr<APawn> OwnerPawn;

	EPhaseCrashState CrashState = EPhaseCrashState::Ready;
	FVector AimTarget = FVector::ZeroVector;
	FVector CrashDirection = FVector::ZeroVector;
	float ChargeElapsed = 0.0f;
	float CrashDistanceRemaining = 0.0f;
	float RecoveryRemaining = 0.0f;
	float CooldownRemaining = 0.0f;
};
