#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Player/PlayerCorePawn.h"
#include "RDCASlimePresentationComponent.generated.h"

class UMaterialInstanceDynamic;

/** Visual-only Character layer. Never changes the collision body or activates skills. */
UCLASS(ClassGroup = (Player))
class RDCA_API URDCASlimePresentationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URDCASlimePresentationComponent();
	void InitializePresentation();
	void UpdateSlimePresentation(float DeltaTime);
	EPlayerSlimeState GetSlimeState() const { return SlimeState; }
private:
	void SetSlimeState(EPlayerSlimeState NewState);
	void UpdateSlimeMaterialParameters(float MovementAlpha, const FVector& FlowDirection,
		const FVector& SupportNormal, float OriginalBottomDepth);
	UPROPERTY(Transient)
	TObjectPtr<APlayerCorePawn> Pawn;
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
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SlimeMaterialInstance;

};
