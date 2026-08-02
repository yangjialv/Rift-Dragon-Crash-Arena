#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AnchorOverloadComponent.generated.h"

class APawn;
class UAttachSurfaceComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EAnchorOverloadState : uint8
{
	Normal,
	Warning,
	Recovering
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnAnchorOverloadStateChanged,
	EAnchorOverloadState,
	PreviousState,
	EAnchorOverloadState,
	NewState);

UCLASS(
	ClassGroup = (Arena),
	meta = (BlueprintSpawnableComponent, DisplayName = "Anchor Overload"))
class RDCA_API UAnchorOverloadComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAnchorOverloadComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

public:
	void NotifyPlayerAttached(APawn* PlayerPawn);
	void NotifyPlayerDetached(APawn* PlayerPawn);

	UFUNCTION(BlueprintPure, Category = "Anchor|Overload")
	EAnchorOverloadState GetOverloadState() const { return OverloadState; }

	UFUNCTION(BlueprintPure, Category = "Anchor|Overload")
	float GetWarningRemainingTime() const;

	UFUNCTION(BlueprintPure, Category = "Anchor|Overload")
	float GetOverloadAlpha() const { return CurrentOverloadAlpha; }

	UFUNCTION(BlueprintCallable, Category = "Anchor|Overload")
	void AddOverloadAmount(float NormalizedAmount);

	/** Shatters the Anchor after a player deliberately launches from it.
	 *  Unlike overload, this does not damage or force-detach the player. */
	void ShatterAfterPlayerDeparture();

	UPROPERTY(BlueprintAssignable, Category = "Anchor|Overload")
	FOnAnchorOverloadStateChanged OnOverloadStateChanged;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor|Overload",
		meta = (ClampMin = "0.1"))
	float SafeAttachmentDuration = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor|Overload",
		meta = (ClampMin = "0.1"))
	float WarningDuration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor|Overload",
		meta = (ClampMin = "0.1"))
	float RecoveryDuration = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor|Overload",
		meta = (ClampMin = "1"))
	int32 OverloadDamage = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor|Visual")
	TObjectPtr<UMaterialInterface> NormalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor|Visual")
	TObjectPtr<UMaterialInterface> WarningMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor|Fracture")
	TSubclassOf<AActor> FractureActorClass;

private:
	void SetOverloadState(EAnchorOverloadState NewState);
	void TriggerOverload(bool bDamageAttachedPlayer);
	void FinishRecovery();
	void SetAnchorAvailable(bool bAvailable);
	void ApplyAnchorMaterial(UMaterialInterface* Material);
	void UpdateOverloadMaterial(float OverloadAlpha);

	TWeakObjectPtr<APawn> AttachedPlayer;
	TWeakObjectPtr<UAttachSurfaceComponent> AttachSurface;
	TWeakObjectPtr<UStaticMeshComponent> AnchorVisual;
	TWeakObjectPtr<AActor> SpawnedFractureActor;
	TObjectPtr<UMaterialInstanceDynamic> OverloadMaterialInstance;
	EAnchorOverloadState OverloadState = EAnchorOverloadState::Normal;
	float CurrentOverloadAlpha = 0.0f;
	float StateElapsed = 0.0f;
};
