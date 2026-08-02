#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossSweepLaser.generated.h"

class UBoxComponent;
class UMaterialInterface;
class UNiagaraComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class RDCA_API ABossSweepLaser : public AActor
{
	GENERATED_BODY()

public:
	ABossSweepLaser();

	virtual void Tick(float DeltaTime) override;

	void InitializeLaser(
		float NewStartYaw,
		float NewEndYaw,
		float NewSweepDuration,
		int32 NewDamage);

	void ActivateLaser();
	void UpdateWarningPose(const FVector& WorldLocation, float WorldYaw);
	void ConfigureSweep(float NewStartYaw, float NewEndYaw);
	float GetCurrentLaserYaw() const { return GetActorRotation().Yaw; }

protected:
	UFUNCTION()
	void HandleLaserOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Laser")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Laser")
	TObjectPtr<UBoxComponent> DamageVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Laser")
	TObjectPtr<USceneComponent> BeamRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Laser")
	TObjectPtr<UStaticMeshComponent> LaserVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Laser")
	TObjectPtr<UNiagaraComponent> LaserEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser",
		meta = (ClampMin = "100.0"))
	float LaserLength = 1400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser",
		meta = (ClampMin = "1.0"))
	float LaserHalfWidth = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser",
		meta = (ClampMin = "1.0"))
	float LaserHalfHeight = 45.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser|Visual")
	TObjectPtr<UMaterialInterface> WarningMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser|Visual")
	TObjectPtr<UMaterialInterface> ActiveMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser|Visual")
	FVector VisualLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser|Visual")
	FRotator VisualRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser|Anchor",
		meta = (ClampMin = "0.0"))
	float AnchorOverloadPerSecond = 0.35f;

private:
	void ApplyDamageToActor(AActor* OtherActor);
	void ApplyAnchorOverload(float DeltaTime);
	void UpdateComponentDimensions();

	TSet<TWeakObjectPtr<AActor>> DamagedActors;
	float StartYaw = 0.0f;
	float EndYaw = 0.0f;
	float SweepDuration = 1.5f;
	float SweepElapsed = 0.0f;
	int32 Damage = 1;
	bool bLaserActive = false;
};
