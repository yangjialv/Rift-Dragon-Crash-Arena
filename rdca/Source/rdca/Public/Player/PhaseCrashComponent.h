#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PhaseCrashComponent.generated.h"

class APawn;
class AActor;
class UCrashResponseComponent;

UENUM(BlueprintType)
enum class EPhaseCrashState : uint8
{
	Ready,
	Charging,
	Crashing,
	Recovery,
	Cooldown,
	Attached
};

UENUM(BlueprintType)
enum class ECrashArcType : uint8
{
	LowArc,
	HighArc
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

	UFUNCTION(BlueprintCallable, Category = "Phase Crash")
	void StartGroundDash();

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	EPhaseCrashState GetCrashState() const { return CrashState; }

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	bool IsCrashing() const { return CrashState == EPhaseCrashState::Crashing; }

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	float GetChargeAlpha() const;

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	float GetCooldownRemaining() const { return CooldownRemaining; }

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	ECrashArcType GetPredictedArcType() const;

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	float GetMovementInputScale() const;

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	bool IsAttached() const { return CrashState == EPhaseCrashState::Attached; }

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Ground Dash",
		meta = (ClampMin = "0.0"))
	float GroundDashDistance = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Ground Dash",
		meta = (ClampMin = "1.0"))
	float GroundDashSpeed = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Ground Dash",
		meta = (ClampMin = "0.0"))
	float GroundDashCooldown = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Trajectory",
		meta = (ClampMin = "0.0"))
	float MinArcHeight = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Trajectory",
		meta = (ClampMin = "0.0"))
	float MaxArcHeight = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Trajectory",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HighArcThreshold = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Trajectory",
		meta = (ClampMin = "0.01"))
	float MinimumFlightDuration = 0.12f;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Gravity",
		meta = (ClampMin = "0.0"))
	float GravityAcceleration = 2400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Gravity",
		meta = (ClampMin = "0.0"))
	float MaximumFallSpeed = 1800.0f;

private:
	bool UpdateAimTarget();
	bool CalculateTrajectory(
		FVector& OutStart,
		FVector& OutEnd,
		float& OutArcHeight,
		float& OutDuration) const;
	FVector EvaluateTrajectory(float NormalizedTime) const;
	void DrawTrajectoryPreview() const;
	void TickCrash(float DeltaTime);
	void ApplyGravity(float DeltaTime);
	void TickRecovery(float DeltaTime);
	void TickCooldown(float DeltaTime);
	void FinishCrash();
	void HandleCrashImpact(const FHitResult& Hit, const FVector& IncomingDirection);
	void HandleAttachImpact(AActor* TargetActor, const FHitResult& Hit);
	void HandleReboundImpact(
		AActor* TargetActor,
		const FHitResult& Hit,
		const FVector& IncomingDirection,
		const UCrashResponseComponent& ResponseComponent);
	void AddTemporaryMoveIgnore(AActor* TargetActor);
	void ClearTemporaryMoveIgnores();
	void DetachFromCrashTarget();
	void SetCrashState(EPhaseCrashState NewState);

	TObjectPtr<APawn> OwnerPawn;
	TWeakObjectPtr<AActor> AttachedActor;
	TArray<TWeakObjectPtr<AActor>> TemporarilyIgnoredActors;

	EPhaseCrashState CrashState = EPhaseCrashState::Ready;
	bool bChargingFromAttachment = false;
	FVector AimTarget = FVector::ZeroVector;
	FVector CrashStart = FVector::ZeroVector;
	FVector CrashEnd = FVector::ZeroVector;
	float ChargeElapsed = 0.0f;
	float ActiveArcHeight = 0.0f;
	float CrashElapsed = 0.0f;
	float CrashDuration = 0.0f;
	float ActiveCooldownDuration = 0.0f;
	float VerticalVelocity = 0.0f;
	float RecoveryRemaining = 0.0f;
	float CooldownRemaining = 0.0f;
};
