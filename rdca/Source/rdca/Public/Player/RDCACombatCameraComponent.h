#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RDCACombatCameraComponent.generated.h"

class APlayerCorePawn;
class AArenaCombatBounds;
class UMeshComponent;

/** Camera-only framing and pillar visibility. Does not decide movement or damage. */
UCLASS(ClassGroup = (Player))
class RDCA_API URDCACombatCameraComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URDCACombatCameraComponent();
	void InitializeCamera();
	void UpdateCombatCamera(float DeltaTime);
	void RestoreCameraOccluders();
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	void FindBossCameraTarget();
	void UpdateCameraOccluders();
	FVector GetCameraOcclusionCenter();
	bool IsCameraOccluderActor(const AActor& Actor) const;
	UPROPERTY(Transient)
	TObjectPtr<APlayerCorePawn> Pawn;
	TWeakObjectPtr<AActor> BossCameraTarget;
	FVector LastCameraOcclusionOutward = FVector::ForwardVector;
	TWeakObjectPtr<AArenaCombatBounds> CameraOcclusionArenaBounds;
	TSet<TWeakObjectPtr<UMeshComponent>> CameraOccludedComponents;
};
