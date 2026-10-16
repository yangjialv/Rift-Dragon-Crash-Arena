#include "Abilities/RDCAPhaseCrashAbility.h"

#include "Abilities/RDCAAbilitySystemComponent.h"
#include "Abilities/RDCAGameplayTags.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameFramework/Actor.h"

URDCAPhaseCrashAbility::URDCAPhaseCrashAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	AbilityTags.AddTag(RDCAGameplayTags::Ability_Player_PhaseCrash);
	ActivationOwnedTags.AddTag(RDCAGameplayTags::State_Action_PhaseCrash);
	ActivationBlockedTags.AddTag(RDCAGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(RDCAGameplayTags::State_Action_Dashing);
	CrashCooldownTags.AddTag(RDCAGameplayTags::Cooldown_Player_PhaseCrash);
	FAbilityTriggerData& Trigger = AbilityTriggers.AddDefaulted_GetRef();
	Trigger.TriggerTag = RDCAGameplayTags::Event_Player_PhaseCrash_Begin;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
}

bool URDCAPhaseCrashAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo || !Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const AActor* Avatar = ActorInfo->AvatarActor.Get();
	const UPhaseCrashComponent* Component = Avatar ? Avatar->FindComponentByClass<UPhaseCrashComponent>() : nullptr;
	return Component && Component->CanStartCrashCharge();
}

const FGameplayTagContainer* URDCAPhaseCrashAbility::GetCooldownTags() const
{
	return &CrashCooldownTags;
}

void URDCAPhaseCrashAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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
	const bool bWorldSpaceAim = TriggerEventData->EventTag == RDCAGameplayTags::Event_Player_PhaseCrash_Begin_WorldSpace;
	if (!Motion->ExecuteCrashAim(Target->GetEndPoint(), TriggerEventData->Target.Get(), bWorldSpaceAim))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}
	UAbilityTask_WaitGameplayEvent* ReleaseTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, RDCAGameplayTags::Event_Player_PhaseCrash_Release, nullptr, true, true);
	ReleaseTask->EventReceived.AddDynamic(this, &ThisClass::HandleRelease);
	ReleaseTask->ReadyForActivation();
}

void URDCAPhaseCrashAbility::HandleRelease(FGameplayEventData Payload)
{
	if (IsActive() && (!Motion || !Motion->ExecuteCrashRelease()))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
	}
}

void URDCAPhaseCrashAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// As with dash, recovery completes before the cooldown begins.
	if (bApplyCooldownAtCompletion && Motion && ActorInfo)
	{
		if (URDCAAbilitySystemComponent* ASC = Cast<URDCAAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()))
		{
			ASC->ApplyPhaseCrashCooldown(Motion->GetCrashCooldownDuration());
		}
	}
}

void URDCAPhaseCrashAbility::HandleMotionStateChanged(EPhaseCrashState PreviousState, EPhaseCrashState NewState)
{
	if (!IsActive()) { return; }
	if (NewState == EPhaseCrashState::Cooldown)
	{
		bApplyCooldownAtCompletion = true;
		CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
	}
	else if (NewState == EPhaseCrashState::Attached || NewState == EPhaseCrashState::Ready)
	{
		// Landing refresh, reattachment or loss of the attachment can end the action.
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
	}
}

void URDCAPhaseCrashAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility, const bool bWasCancelled)
{
	if (!IsActive()) { return; }
	if (Motion)
	{
		Motion->OnCrashStateChanged.RemoveDynamic(this, &ThisClass::HandleMotionStateChanged);
		if (bWasCancelled) { Motion->CancelCrashExecution(); }
	}
	Motion = nullptr;
	bApplyCooldownAtCompletion = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

URDCAPhaseCrashCooldownEffect::URDCAPhaseCrashCooldownEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(1.0f);
}
