#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Player/PhaseCrashComponent.h"
#include "RDCAPlayerControlComponent.generated.h"

class APlayerCorePawn;
class APawn;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputMappingContext;
struct FInputActionValue;

/** Control layer: bindings and player intent. GAS/motion remain authoritative. */
UCLASS(ClassGroup = (Player))
class RDCA_API URDCAPlayerControlComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URDCAPlayerControlComponent();
	static URDCAPlayerControlComponent* FindOrCreateForPawn(APawn* InPawn);
	void InitializeControl();
	bool RequestCrashChargeAtLocation(const FVector& WorldTarget, const AActor* TargetActor = nullptr);
	bool RequestGroundDashAtLocation(const FVector& WorldTarget);
	void ClearActionRequests();
	void NotifyChargeStarted(bool bWorldSpaceAim);
	bool ResolveAimRay(const FVector& RayOrigin, const FVector& RayDirection,
		FVector& OutTarget, const AActor*& OutActor) const;
	void RefreshMappingContext();
	void BindInputs(UInputComponent* Input);
	void ResetControl();
	void SubmitMove(const FVector2D& MovementInput);
	void StartCrashCharge();
	void ReleaseCrash();
	void CancelCrashCharge();
	void StartGroundDash();
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	void HandleMotionUpdated(float DeltaTime);
	void ConsumeBufferedInputs(float DeltaTime);
	bool UpdateCursorAim();
	void SetAimCursor();
	bool TryBeginCharge();
	bool TryStartDash();
	UFUNCTION()
	void HandleMotionStateChanged(EPhaseCrashState Previous, EPhaseCrashState Next);
	bool ResolvePawn();
	bool CanSubmitInput() const;
	void Move(const FInputActionValue& Value);
	void RemoveMappingContext();
	void RemoveBindings();
	UPROPERTY(Transient)
	TObjectPtr<APlayerCorePawn> Pawn;
	UPROPERTY(Transient)
	TObjectPtr<APawn> OwnerPawn;
	UPROPERTY(Transient)
	TObjectPtr<UPhaseCrashComponent> Motion;
	bool bCrashInputHeld = false;
	bool bCrashInputBuffered = false;
	bool bAimUsesWorldTarget = false;
	bool bBufferedCrashHasWorldTarget = false;
	FVector BufferedCrashWorldTarget = FVector::ZeroVector;
	TWeakObjectPtr<const AActor> BufferedCrashTargetActor;
	float DashInputBufferRemaining = 0.0f;
	bool bBufferedDashHasWorldTarget = false;
	FVector BufferedDashWorldTarget = FVector::ZeroVector;
	TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> AppliedSubsystem;
	TWeakObjectPtr<UInputMappingContext> AppliedMapping;
	TArray<uint32> BindingHandles;
};
