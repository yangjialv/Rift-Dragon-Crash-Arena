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

UCLASS()
class RDCA_API APlayerCorePawn : public APawn
{
	GENERATED_BODY()

public:
	APlayerCorePawn();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	virtual void BeginPlay() override;

private:
	void FindBossCameraTarget();
	void UpdateCombatCamera(float DeltaTime);
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

private:
	TWeakObjectPtr<AActor> BossCameraTarget;
};
