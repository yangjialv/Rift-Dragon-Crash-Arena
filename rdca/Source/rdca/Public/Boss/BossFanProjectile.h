#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossFanProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;

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

private:
	FVector TravelDirection = FVector::ForwardVector;
	FVector GroundTravelDirection = FVector::ForwardVector;
	float GroundSkimHeight = 0.0f;
	bool bGroundSkimming = false;
	float TravelSpeed = 900.0f;
	int32 Damage = 1;
	bool bHasAppliedDamage = false;
};
