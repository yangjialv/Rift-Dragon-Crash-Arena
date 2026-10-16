#pragma once

#include "Arena/AttachSurfaceComponent.h"
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PhaseCrashComponent.generated.h"

class APawn;
class AActor;
class UCrashResponseComponent;
class UPrimitiveComponent;
class URDCAAbilitySystemComponent;
class URDCAGroundDashAbility;
class URDCAPhaseCrashAbility;
class URDCAPlayerControlComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPhaseCrashMotionUpdated, float);

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

/** Actual travel type, independent of skill lifetime or the height of its trajectory. */
UENUM(BlueprintType)
enum class EPhaseCrashMotionType : uint8
{
	None,
	Jump,
	GroundDash,
	BossDirect,
	Rebound,
	ArenaRecovery
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

	/** Forget pending control intent without changing the current physical motion. */
	void ClearBufferedActionInput();

	/** Explicit aiming input; release still uses ReleaseCrash and the same GAS skill. */
	bool RequestCrashChargeAtLocation(const FVector& WorldTarget, const AActor* TargetActor = nullptr);
	bool CanStartCrashCharge() const;
	float GetCrashCooldownDuration() const { return FMath::Max(CooldownDuration, 0.0f); }

	UFUNCTION(BlueprintCallable, Category = "Phase Crash")
	void StartGroundDash();

	/** World-space control intent, also usable by non-mouse callers. No movement before validation. */
	bool RequestGroundDashAtLocation(const FVector& WorldTarget);
	bool CanStartGroundDash() const;
	float GetGroundDashCooldownDuration() const { return FMath::Max(GroundDashCooldown, 0.0f); }

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	EPhaseCrashState GetCrashState() const { return CrashState; }

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	bool IsCrashing() const { return CrashState == EPhaseCrashState::Crashing; }

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	float GetChargeAlpha() const;

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	float GetCooldownRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	float GetCooldownReadyPercent() const;

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	ECrashArcType GetPredictedArcType() const;

	/**
	 * Supplies the HUD with the same trajectory used by the real launch.
	 * Returns true while an aim point should be presented. A short click can
	 * therefore show its landing marker before it has produced a valid launch.
	 */
	bool GetAimPreview(
		TArray<FVector>& OutWorldPoints,
		FVector& OutLandingPoint,
		bool& bOutHasLaunchTrajectory) const;

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	float GetMovementInputScale() const;

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	bool IsAttached() const
	{
		return CrashState == EPhaseCrashState::Attached || bChargingFromAttachment;
	}

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	bool IsGroundDashActive() const
	{
		return CrashState == EPhaseCrashState::Crashing
			&& MotionType == EPhaseCrashMotionType::GroundDash;
	}

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	EPhaseCrashMotionType GetMotionType() const { return MotionType; }

	/** True while an attached player is pointing the left-click aim ray at the Boss. */
	UFUNCTION(BlueprintPure, Category = "Phase Crash|Boss Aim")
	bool IsBossAimActive() const
	{
		return CrashState == EPhaseCrashState::Charging
			&& bChargingFromAttachment
			&& bAimingAtBoss;
	}

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	FVector GetActiveTravelDirection() const
	{
		return (CrashEnd - CrashStart).GetSafeNormal();
	}

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	float GetActiveCrashProgress() const
	{
		return CrashDuration > UE_KINDA_SMALL_NUMBER
			? FMath::Clamp(CrashElapsed / CrashDuration, 0.0f, 1.0f)
			: 0.0f;
	}

	/** One-shot visual signal fired by a solid rebound; 1 at impact and 0 at recovery. */
	UFUNCTION(BlueprintPure, Category = "Phase Crash|Presentation")
	float GetReboundPresentationAlpha() const
	{
		return ReboundPresentationDuration > UE_KINDA_SMALL_NUMBER
			? FMath::Clamp(
				ReboundPresentationRemaining / ReboundPresentationDuration,
				0.0f,
				1.0f)
			: 0.0f;
	}

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	FVector GetAimDirection() const
	{
		return OwnerPawn
			? (AimTarget - OwnerPawn->GetActorLocation()).GetSafeNormal()
			: FVector::ForwardVector;
	}

	UFUNCTION(BlueprintPure, Category = "Phase Crash")
	FVector GetAttachedSurfaceNormal() const;

	UFUNCTION(BlueprintCallable, Category = "Phase Crash")
	void MoveAttached(const FVector2D& MovementInput);

	UFUNCTION(BlueprintCallable, Category = "Phase Crash")
	void ForceDetachFromAttachment();

	/** Used by the arena recovery volume after the player falls through the centre void. */
	UFUNCTION(BlueprintCallable, Category = "Phase Crash|Arena")
	void ForceArenaRecovery(
		const FVector& Destination,
		float ArcHeight,
		float Duration);

	UPROPERTY(BlueprintAssignable, Category = "Phase Crash")
	FOnPhaseCrashStateChanged OnCrashStateChanged;
	/** End-of-motion step: Control can update aim and retry input without a second ticking cache. */
	FOnPhaseCrashMotionUpdated OnMotionUpdated;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Time required to reach maximum jump height. Holding longer stays capped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Charge",
		meta = (ClampMin = "0.01", DisplayName = "Maximum Charge Duration"))
	float MaximumChargeDuration = 0.8f;

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
	float GroundDashCooldown = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Input Buffer",
		meta = (ClampMin = "0.0",
			ToolTip = "How long a ground-dash press is remembered while the action is unavailable."))
	float DashInputBufferDuration = 0.6f;

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

	/** Extra air time added at full charge so a high arc does not move unnaturally fast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Trajectory",
		meta = (ClampMin = "0.0", DisplayName = "Maximum Charge Extra Air Time"))
	float MaximumChargeExtraAirTime = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Timing",
		meta = (ClampMin = "0.0"))
	float RecoveryDuration = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Timing",
		meta = (ClampMin = "0.0"))
	float CooldownDuration = 0.45f;

	/** A left-click launch becomes ready as soon as it lands on Arena Floor Collision. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Timing")
	bool bRefreshJumpCooldownOnArenaLanding = true;

	/** Visual-only impact-to-ejection time after any Rebound collision. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Presentation",
		meta = (ClampMin = "0.01"))
	float ReboundPresentationDuration = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Charge",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ChargingMovementScale = 0.35f;

	/** Enables the in-game trajectory and landing presentation drawn by the combat HUD. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Presentation",
		meta = (DisplayName = "Show Aim Trajectory Preview"))
	bool bDrawDebugAim = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Gravity",
		meta = (ClampMin = "0.0"))
	float GravityAcceleration = 2400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Gravity",
		meta = (ClampMin = "0.0"))
	float MaximumFallSpeed = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Attachment",
		meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float AttachCornerTransitionDuration = 0.15f;

	/** Straight-line speed used only when an attached player aims at the Boss. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Crash|Boss Aim",
		meta = (ClampMin = "1.0"))
	float BossCrashSpeed = 5200.0f;

private:
	friend class URDCAGroundDashAbility;
	friend class URDCAPhaseCrashAbility;
	friend class URDCAPlayerControlComponent;
	bool RequestCrashAim(const FVector& WorldTarget, const AActor* TargetActor, bool bWorldSpaceAim);
	bool ExecuteCrashAim(const FVector& WorldTarget, const AActor* TargetActor, bool bWorldSpaceAim);
	bool ExecuteCrashRelease();
	void CancelCrashExecution();
	bool IsCurrentGASActionOnCooldown() const;
	bool ExecuteGroundDash(const FVector& WorldTarget);
	void CancelGroundDashExecution();
	bool CanAcceptActionInput() const;
	bool CalculateTrajectory(
		FVector& OutStart,
		FVector& OutEnd,
		float& OutArcHeight,
		float& OutDuration) const;
	FVector EvaluateTrajectory(float NormalizedTime) const;
	void TickCrash(float DeltaTime);
	void ApplyGravity(float DeltaTime);
	void TickRecovery(float DeltaTime);
	void TickCooldown(float DeltaTime);
	void FinishCrash();
	void HandleCrashImpact(const FHitResult& Hit, const FVector& IncomingDirection);
	void HandleAttachImpact(
		AActor* TargetActor,
		const FHitResult& Hit,
		UCrashResponseComponent& ResponseComponent);
	void HandleReboundImpact(
		AActor* TargetActor,
		const FHitResult& Hit,
		const FVector& IncomingDirection,
		const UCrashResponseComponent& ResponseComponent);
	void AddTemporaryMoveIgnore(AActor* TargetActor);
	void ClearTemporaryMoveIgnores();
	void DetachFromCrashTarget();
	void TickAttachCornerTransition(float DeltaTime);
	bool MoveOnAttachBox(
		UAttachSurfaceComponent& AttachBox,
		const FVector2D& MovementInput,
		float MovementStep);
	void ConfigureAttachBoxSurface(
		UAttachSurfaceComponent& AttachBox,
		const FVector& WorldContactPoint,
		const FVector& WorldImpactNormal);
	void SetCrashState(EPhaseCrashState NewState);

	TObjectPtr<APawn> OwnerPawn;
	UPROPERTY(Transient)
	TObjectPtr<URDCAAbilitySystemComponent> AbilitySystem;
	UPROPERTY(Transient)
	TObjectPtr<URDCAPlayerControlComponent> PlayerControl;
	bool bUsesGASDashCooldown = false;
	bool bUsesGASCrashCooldown = false;
	TWeakObjectPtr<const AActor> CurrentAimActor;
	EPhaseCrashMotionType MotionType = EPhaseCrashMotionType::None;
	TWeakObjectPtr<AActor> AttachedActor;
	TWeakObjectPtr<UPrimitiveComponent> AttachedSurfaceComponent;
	TWeakObjectPtr<UCrashResponseComponent> AttachedResponseComponent;
	TArray<TWeakObjectPtr<AActor>> TemporarilyIgnoredActors;

	EPhaseCrashState CrashState = EPhaseCrashState::Ready;
	bool bChargingFromAttachment = false;
	bool bActiveCrashFromAttachment = false;
	bool bAimingAtBoss = false;
	bool bActiveBossCrash = false;
	bool bWeakPointDamageAppliedThisCrash = false;
	bool bCurrentCrashRefreshesOnLanding = false;
	FVector AttachedSurfaceOriginLocal = FVector::ZeroVector;
	FVector AttachedSurfaceOutLocal = FVector::ForwardVector;
	FVector AttachedSurfaceUpLocal = FVector::UpVector;
	FVector AttachedSurfaceRightLocal = FVector::RightVector;
	float AttachedSurfaceOffset = 50.0f;
	float AttachedSurfaceX = 0.0f;
	float AttachedSurfaceY = 0.0f;
	float AttachedSurfaceMinX = 0.0f;
	float AttachedSurfaceMaxX = 0.0f;
	float AttachedSurfaceMinY = 0.0f;
	float AttachedSurfaceMaxY = 0.0f;
	EAttachBoxFace AttachedBoxFace = EAttachBoxFace::PositiveX;
	FVector AttachedBoxContactLocal = FVector::ZeroVector;
	bool bAttachCornerTransitionActive = false;
	float AttachCornerTransitionElapsed = 0.0f;
	EAttachBoxFace AttachCornerStartFace = EAttachBoxFace::PositiveX;
	EAttachBoxFace AttachCornerEndFace = EAttachBoxFace::PositiveX;
	FVector AttachCornerContactLocal = FVector::ZeroVector;
	FVector AimTarget = FVector::ZeroVector;
	FVector CrashStart = FVector::ZeroVector;
	FVector CrashEnd = FVector::ZeroVector;
	float ActiveArcHeight = 0.0f;
	float ChargeElapsed = 0.0f;
	float CrashElapsed = 0.0f;
	float CrashDuration = 0.0f;
	float ActiveCooldownDuration = 0.0f;
	float VerticalVelocity = 0.0f;
	float RecoveryRemaining = 0.0f;
	float CooldownRemaining = 0.0f;
	float ReboundPresentationRemaining = 0.0f;
};
