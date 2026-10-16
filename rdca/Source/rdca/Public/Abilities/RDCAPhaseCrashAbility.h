#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "Player/PhaseCrashComponent.h"
#include "RDCAPhaseCrashAbility.generated.h"

/** One charged action; its motion may be an arc jump or an attached Boss-direct crash. */
UCLASS()
class RDCA_API URDCAPhaseCrashAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	URDCAPhaseCrashAbility();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UFUNCTION()
	void HandleRelease(FGameplayEventData Payload);
	UFUNCTION()
	void HandleMotionStateChanged(EPhaseCrashState PreviousState, EPhaseCrashState NewState);

	UPROPERTY(Transient)
	TObjectPtr<UPhaseCrashComponent> Motion;
	FGameplayTagContainer CrashCooldownTags;
	bool bApplyCooldownAtCompletion = false;
};

UCLASS()
class RDCA_API URDCAPhaseCrashCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	URDCAPhaseCrashCooldownEffect();
};
