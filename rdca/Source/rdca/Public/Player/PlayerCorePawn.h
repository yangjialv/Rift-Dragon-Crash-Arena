#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "PlayerCorePawn.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UPhaseCrashComponent;
class UPlayerHealthComponent;
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
	void UpdateSlimePresentation(float DeltaTime);
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Camera",
		meta = (ClampMin = "100.0"))
	float MinimumCameraArmLength = 1050.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Camera",
		meta = (ClampMin = "100.0"))
	float MaximumCameraArmLength = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Camera",
		meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float CameraArmLengthPerBossDistance = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Camera",
		meta = (ClampMin = "100.0"))
	float ArenaRadiusForMaximumZoom = 2600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Camera",
		meta = (ClampMin = "-85.0", ClampMax = "-15.0"))
	float CombatCameraPitch = -55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Camera",
		meta = (ClampMin = "-85.0", ClampMax = "-15.0"))
	float MaximumDistanceCameraPitch = -62.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Camera",
		meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float BossFramingWeight = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Camera",
		meta = (ClampMin = "0.0"))
	float CameraFocusHeight = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Camera",
		meta = (ClampMin = "0.1"))
	float CameraFollowInterpSpeed = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Camera",
		meta = (ClampMin = "0.1"))
	float CameraRotationInterpSpeed = 8.0f;

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
	float SlimeStateElapsed = 0.0f;
	float SurfaceImpactEnergy = 0.0f;
	float DashReboundRemaining = 0.0f;
	FVector PreviousAttachedNormal = FVector::ZeroVector;
};
