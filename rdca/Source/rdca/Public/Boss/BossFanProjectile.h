#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossFanProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UMaterialInterface;

UENUM(BlueprintType)
enum class EBossProjectileMotionMode : uint8
{
	Straight UMETA(DisplayName = "Straight"),
	Curved UMETA(DisplayName = "Curved"),
	Homing UMETA(DisplayName = "Limited Homing")
};

UCLASS(Blueprintable)
class RDCA_API ABossFanProjectile : public AActor
{
	GENERATED_BODY()

public:
	ABossFanProjectile();

	virtual void Tick(float DeltaTime) override;

	void InitializeProjectile(
		const FVector& WorldDirection,
		float NewSpeed,
		int32 NewDamage);

	void InitializeGroundSkimmingProjectile(
		const FVector& WorldDirection,
		float NewSpeed,
		int32 NewDamage,
		float WorldCruiseHeight);

	/** Configures straight, fixed-curve or limited-homing movement on one shared projectile class. */
	void InitializePatternProjectile(
		const FVector& WorldDirection,
		float NewSpeed,
		int32 NewDamage,
		float WorldCruiseHeight,
		EBossProjectileMotionMode NewMotionMode,
		float NewCurveDegreesPerSecond = 0.0f,
		float NewMaximumCurveDegrees = 0.0f,
		AActor* NewHomingTarget = nullptr,
		float NewHomingTurnDegreesPerSecond = 0.0f,
		float NewHomingDuration = 0.0f,
		float NewHomingStopDistance = 0.0f,
		float NewMaximumHomingAngle = 180.0f);

	/** Applies the colour belonging to the phase that spawned this projectile. */
	void SetCodePhaseVisual(bool bCodePhase);

protected:
	UFUNCTION()
	void HandleProjectileOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile",
		meta = (ClampMin = "0.1"))
	float LifeSeconds = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile|Anchor",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AnchorOverloadAmount = 0.2f;

	/** Cosmetic spin shared by every barrage pattern. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile|Visual",
		meta = (ClampMin = "0.0"))
	float VisualSpinDegreesPerSecond = 240.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile|Visual|Phase")
	TObjectPtr<UMaterialInterface> CyberPhaseMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile|Visual|Phase")
	TObjectPtr<UMaterialInterface> CodePhaseMaterial;

private:
	void UpdateSteering(float DeltaTime);
	void UpdateGroundDirectionFromTravel();

	FVector TravelDirection = FVector::ForwardVector;
	FVector GroundTravelDirection = FVector::ForwardVector;
	float GroundSkimHeight = 0.0f;
	bool bGroundSkimming = false;
	float TravelSpeed = 900.0f;
	int32 Damage = 1;
	bool bHasAppliedDamage = false;
	EBossProjectileMotionMode MotionMode = EBossProjectileMotionMode::Straight;
	float CurveDegreesPerSecond = 0.0f;
	float MaximumCurveDegrees = 0.0f;
	float AccumulatedCurveDegrees = 0.0f;
	TWeakObjectPtr<AActor> HomingTarget;
	float HomingTurnDegreesPerSecond = 0.0f;
	float HomingDuration = 0.0f;
	float HomingElapsed = 0.0f;
	float HomingStopDistance = 0.0f;
	float MaximumHomingAngle = 180.0f;
	float InitialTravelYaw = 0.0f;
};
