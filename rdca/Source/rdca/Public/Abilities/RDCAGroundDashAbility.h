#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "Player/PhaseCrashComponent.h"
#include "RDCAGroundDashAbility.generated.h"

/** GAS owns permission/lifetime/cooldown; PhaseCrash owns the actual swept motion. */
UCLASS()
class RDCA_API URDCAGroundDashAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	URDCAGroundDashAbility();
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
	void HandleMotionStateChanged(EPhaseCrashState PreviousState, EPhaseCrashState NewState);

	UPROPERTY(Transient)
	TObjectPtr<UPhaseCrashComponent> Motion;
	FGameplayTagContainer DashCooldownTags;
	bool bApplyCooldownAtCompletion = false;
};

/** Native duration effect: no extra Blueprint setup is needed for existing players. */
UCLASS()
class RDCA_API URDCAGroundDashCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URDCAGroundDashCooldownEffect();
};
