#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/HitResult.h"
#include "CrashResponseComponent.generated.h"

class APawn;

UENUM(BlueprintType)
enum class ECrashCollisionResponse : uint8
{
	Block,
	Attach,
	Rebound,
	Ignore
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnCrashImpactReceived,
	APawn*,
	CrashingPawn,
	ECrashCollisionResponse,
	Response,
	const FHitResult&,
	Hit);

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class RDCA_API UCrashResponseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCrashResponseComponent();

	UFUNCTION(BlueprintPure, Category = "Crash Response")
	ECrashCollisionResponse GetResponse() const { return Response; }

	UFUNCTION(BlueprintPure, Category = "Crash Response")
	float GetReboundDistance() const { return ReboundDistance; }

	UFUNCTION(BlueprintPure, Category = "Crash Response")
	float GetReboundSpeed() const { return ReboundSpeed; }

	UFUNCTION(BlueprintPure, Category = "Crash Response")
	float GetMaximumReboundZRatio() const { return MaximumReboundZRatio; }

	UFUNCTION(BlueprintPure, Category = "Crash Response")
	float GetAttachMoveSpeed() const { return AttachMoveSpeed; }

	void NotifyCrashImpact(APawn* CrashingPawn, const FHitResult& Hit);

	UPROPERTY(BlueprintAssignable, Category = "Crash Response")
	FOnCrashImpactReceived OnCrashImpactReceived;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crash Response")
	ECrashCollisionResponse Response = ECrashCollisionResponse::Block;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crash Response|Rebound",
		meta = (ClampMin = "0.0", EditCondition = "Response == ECrashCollisionResponse::Rebound",
			EditConditionHides))
	float ReboundDistance = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crash Response|Rebound",
		meta = (ClampMin = "1.0", EditCondition = "Response == ECrashCollisionResponse::Rebound",
			EditConditionHides))
	float ReboundSpeed = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crash Response|Rebound",
		meta = (ClampMin = "0.0", ClampMax = "1.0",
			EditCondition = "Response == ECrashCollisionResponse::Rebound",
			EditConditionHides))
	float MaximumReboundZRatio = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crash Response|Attach",
		meta = (ClampMin = "0.0", EditCondition = "Response == ECrashCollisionResponse::Attach",
			EditConditionHides))
	float AttachMoveSpeed = 400.0f;
};
