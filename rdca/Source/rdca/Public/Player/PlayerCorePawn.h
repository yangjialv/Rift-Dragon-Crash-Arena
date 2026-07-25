#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "PlayerCorePawn.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UPhaseCrashComponent;
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

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	virtual void BeginPlay() override;

private:
	void Move(const FInputActionValue& Value);
	void StartCrashCharge();
	void ReleaseCrash();
	void CancelCrashCharge();

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputMappingContext> PlayerMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> CrashAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Movement", meta = (ClampMin = "0.0"))
	float MoveSpeed = 600.0f;
};
