#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "PlayerCorePawn.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UMaterialInstanceDynamic;
class UMeshComponent;
class UPhaseCrashComponent;
class UPlayerHealthComponent;
class AArenaCombatBounds;
class USphereComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UFloatingPawnMovement;
struct FInputActionValue;

UENUM(BlueprintType)
enum class EPlayerSlimeState : uint8
{
	Idle,
	Moving,
	Charging,
	Dashing,
	Airborne,
	Attached
};

/** One editable endpoint of the radial Boss-combat camera. */
USTRUCT(BlueprintType)
struct FCombatCameraPoint
{
	GENERATED_BODY()

	/** Spring-arm length at this endpoint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Point",
		meta = (ClampMin = "100.0"))
	float ArmLength = 1350.0f;

	/** A shallower pitch presents the Boss instead of looking straight down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Point",
		meta = (ClampMin = "-85.0", ClampMax = "-5.0"))
	float Pitch = -28.0f;

	/** Height above the player used as the base look-at focus. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Point",
		meta = (ClampMin = "-500.0", ClampMax = "2000.0"))
	float FocusHeight = 100.0f;

	/** Blend toward the Boss from the player. Keeps both characters in frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Point",
		meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float BossFramingWeight = 0.28f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnPlayerSlimeStateChanged,
	EPlayerSlimeState,
	PreviousState,
	EPlayerSlimeState,
	NewState);

UCLASS()
class RDCA_API APlayerCorePawn : public APawn
{
	GENERATED_BODY()

public:
	APlayerCorePawn();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintPure, Category = "Player|Presentation")
	EPlayerSlimeState GetSlimeState() const { return SlimeState; }

	UPROPERTY(BlueprintAssignable, Category = "Player|Presentation")
	FOnPlayerSlimeStateChanged OnSlimeStateChanged;

protected:
	virtual void BeginPlay() override;

private:
	void FindBossCameraTarget();
	void UpdateCombatCamera(float DeltaTime);
	void UpdateCameraOccluders();
	bool IsCameraOccluderActor(const AActor& Actor) const;
	FVector GetCameraOcclusionCenter();
	void UpdateSlimePresentation(float DeltaTime);
	void UpdateSlimeMaterialParameters(
		float MovementAlpha,
		const FVector& FlowDirection,
		const FVector& SupportNormal,
		float OriginalBottomDepth);
	void SetSlimeState(EPlayerSlimeState NewState);
	void Move(const FInputActionValue& Value);
	void StartCrashCharge();
	void ReleaseCrash();
	void CancelCrashCharge();
	void StartGroundDash();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Components")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Components")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Components")
	TObjectPtr<UFloatingPawnMovement> MovementComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Components")
	TObjectPtr<UPhaseCrashComponent> PhaseCrashComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Components")
	TObjectPtr<UPlayerHealthComponent> HealthComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputMappingContext> PlayerMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> CrashAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> DashAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Movement", meta = (ClampMin = "0.0"))
	float MoveSpeed = 600.0f;

	/** Begins blending away from Near Camera Point at this player-to-Boss distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Camera|Two Point",
		meta = (ClampMin = "0.0"))
	float NearCameraPointDistance = 450.0f;

	/** Reaches Far Camera Point at this player-to-Boss distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Camera|Two Point",
		meta = (ClampMin = "1.0"))
	float FarCameraPointDistance = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Camera|Two Point")
	FCombatCameraPoint NearCameraPoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Camera|Two Point")
	FCombatCameraPoint FarCameraPoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Camera",
		meta = (ClampMin = "0.1"))
	float CameraFollowInterpSpeed = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Camera",
		meta = (ClampMin = "0.1"))
	float CameraRotationInterpSpeed = 8.0f;

	/** Hide the rear radial pillar sector so the camera remains clean at the arena edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Camera|Occlusion")
	bool bHideCameraOccluderPillars = true;

	/** Existing Cyber/Code pillar mappings already use this prefix. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Camera|Occlusion")
	FName CameraOccluderTagPrefix = TEXT("PhaseMap_Pillar");

	/** At or inside this distance, use the larger Center Hidden Arc Degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Camera|Occlusion",
		meta = (ClampMin = "0.0"))
	float CameraOcclusionCenterDistance = 400.0f;

	/** At or beyond this distance, use the smaller Edge Hidden Arc Degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Camera|Occlusion",
		meta = (ClampMin = "1.0"))
	float CameraOcclusionEdgeDistance = 3000.0f;

	/** Wide rear sector near the Boss/arena centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Camera|Occlusion",
		meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float CenterHiddenArcDegrees = 240.0f;

	/** Narrow rear sector while the player is at the outer edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Camera|Occlusion",
		meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float EdgeHiddenArcDegrees = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Presentation",
		meta = (ClampMin = "0.1"))
	float SlimeTransformInterpSpeed = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Presentation",
		meta = (ClampMin = "0.1"))
	float SlimeFacingInterpSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Presentation")
	float VisualForwardYawOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Presentation",
		meta = (ToolTip = "Direction from the slime center toward its physical bottom in the imported mesh local space."))
	FVector ModelBottomLocalAxis = FVector(0.0f, 0.0f, -1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Presentation",
		meta = (ToolTip = "Visual forward direction in the imported mesh local space."))
	FVector ModelForwardLocalAxis = FVector(1.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Presentation",
		meta = (ClampMin = "0.0"))
	float IdleWobbleAmount = 2.0f;

	/** Volume-conserving idle pulse. Keeps the resting slime settled rather than rigid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Presentation|Slime Motion",
		meta = (ClampMin = "0.0"))
	float IdlePulseAmount = 0.024f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Presentation|Slime Motion",
		meta = (ClampMin = "0.0"))
	float IdlePulseFrequency = 0.8f;

	/** Maximum scale change applied during one natural crawl/compression cycle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Presentation|Slime Motion",
		meta = (ClampMin = "0.0", ClampMax = "0.25"))
	float LocomotionSquirmAmount = 0.052f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Presentation|Slime Motion",
		meta = (ClampMin = "0.0"))
	float LocomotionMinFrequency = 1.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Presentation|Slime Motion",
		meta = (ClampMin = "0.0"))
	float LocomotionMaxFrequency = 2.4f;

	/** Visual-only downward press at the dense part of each crawl cycle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Presentation|Slime Motion",
		meta = (ClampMin = "0.0"))
	float LocomotionPressOffset = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Presentation",
		meta = (ClampMin = "0.0"))
	float MovementStateSpeedThreshold = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Presentation",
		meta = (ClampMin = "0.0"))
	float MovementTrailOffset = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Presentation",
		meta = (ClampMin = "0.0"))
	float MaximumChargeRecoilOffset = 32.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Presentation",
		meta = (ClampMin = "0.1"))
	float DashTransformInterpSpeed = 42.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Presentation",
		meta = (ClampMin = "0.0"))
	float DashReboundDuration = 0.22f;

private:
	TWeakObjectPtr<AActor> BossCameraTarget;
	EPlayerSlimeState SlimeState = EPlayerSlimeState::Idle;
	FVector BaseVisualScale = FVector::OneVector;
	FVector BaseVisualLocation = FVector::ZeroVector;
	FRotator BaseVisualRotation = FRotator::ZeroRotator;
	float BaseVisualBottomDistance = 50.0f;
	FVector PreviousPresentationLocation = FVector::ZeroVector;
	FVector LastFacingDirection = FVector::ForwardVector;
	float PresentationTime = 0.0f;
	float LocomotionPhase = 0.0f;
	float SlimeStateElapsed = 0.0f;
	float SurfaceImpactEnergy = 0.0f;
	float DashReboundRemaining = 0.0f;
	FVector PreviousAttachedNormal = FVector::ZeroVector;
	/** Runtime instance of VisualMesh material slot 0. It supplies state data to the liquid WPO material. */
	TObjectPtr<UMaterialInstanceDynamic> SlimeMaterialInstance;
	FVector LastCameraOcclusionOutward = FVector::ForwardVector;
	TWeakObjectPtr<AArenaCombatBounds> CameraOcclusionArenaBounds;
	TSet<TWeakObjectPtr<UMeshComponent>> CameraOccludedComponents;
};
