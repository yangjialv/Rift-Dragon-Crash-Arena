#include "Abilities/RDCAGroundDashAbility.h"

#include "Abilities/RDCAAbilitySystemComponent.h"
#include "Abilities/RDCAGameplayTags.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "GameFramework/Actor.h"

URDCAGroundDashAbility::URDCAGroundDashAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	AbilityTags.AddTag(RDCAGameplayTags::Ability_Player_GroundDash);
	ActivationOwnedTags.AddTag(RDCAGameplayTags::State_Action_Dashing);
	ActivationBlockedTags.AddTag(RDCAGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(RDCAGameplayTags::State_Action_PhaseCrash);
	DashCooldownTags.AddTag(RDCAGameplayTags::Cooldown_Player_GroundDash);
	FAbilityTriggerData& Trigger = AbilityTriggers.AddDefaulted_GetRef();
	Trigger.TriggerTag = RDCAGameplayTags::Event_Player_GroundDash;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
}

bool URDCAGroundDashAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo || !Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const AActor* Avatar = ActorInfo->AvatarActor.Get();
	const UPhaseCrashComponent* Component = Avatar ? Avatar->FindComponentByClass<UPhaseCrashComponent>() : nullptr;
	return Component && Component->CanStartGroundDash();
}

const FGameplayTagContainer* URDCAGroundDashAbility::GetCooldownTags() const
{
	return &DashCooldownTags;
}

void URDCAGroundDashAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Motion = ActorInfo && ActorInfo->AvatarActor.IsValid()
		? ActorInfo->AvatarActor->FindComponentByClass<UPhaseCrashComponent>() : nullptr;
	bApplyCooldownAtCompletion = false;
	const FGameplayAbilityTargetData* Target = TriggerEventData ? TriggerEventData->TargetData.Get(0) : nullptr;
	if (!Motion || !Target || !Target->HasEndPoint() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}
	Motion->OnCrashStateChanged.AddUniqueDynamic(this, &ThisClass::HandleMotionStateChanged);
	if (!Motion->ExecuteGroundDash(Target->GetEndPoint()))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
	}
}

void URDCAGroundDashAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// Preserve existing timing: the cooldown starts AFTER travel and recovery.
	// CommitAbility at activation still checks cost/cooldown, but does not start this GE.
	if (bApplyCooldownAtCompletion && Motion && ActorInfo)
	{
		if (URDCAAbilitySystemComponent* ASC = Cast<URDCAAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()))
		{
			ASC->ApplyGroundDashCooldown(Motion->GetGroundDashCooldownDuration());
		}
	}
}

void URDCAGroundDashAbility::HandleMotionStateChanged(EPhaseCrashState PreviousState, EPhaseCrashState NewState)
{
	if (!IsActive())
	{
		return;
	}
	if (NewState == EPhaseCrashState::Cooldown)
	{
		bApplyCooldownAtCompletion = true;
		CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
	}
	else if (NewState == EPhaseCrashState::Attached || NewState == EPhaseCrashState::Ready)
	{
		// Attaching ends the action without penalising surface movement with a cooldown.
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
	}
}

void URDCAGroundDashAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility, const bool bWasCancelled)
{
	if (!IsActive())
	{
		return;
	}
	if (Motion)
	{
		Motion->OnCrashStateChanged.RemoveDynamic(this, &ThisClass::HandleMotionStateChanged);
		if (bWasCancelled)
		{
			Motion->CancelGroundDashExecution();
		}
	}
	Motion = nullptr;
	bApplyCooldownAtCompletion = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

URDCAGroundDashCooldownEffect::URDCAGroundDashCooldownEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(1.0f);
}
