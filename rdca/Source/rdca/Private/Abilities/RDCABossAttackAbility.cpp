#include "Abilities/RDCABossAttackAbility.h"

#include "Abilities/RDCAGameplayTags.h"
#include "Boss/BossEncounterComponent.h"
#include "GameFramework/Actor.h"

URDCABossAttackAbility::URDCABossAttackAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationOwnedTags.AddTag(RDCAGameplayTags::State_Boss_Attacking);
	ActivationBlockedTags.AddTag(RDCAGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(RDCAGameplayTags::State_Boss_Attacking);
}

bool URDCABossAttackAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo || !Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const AActor* Avatar = ActorInfo->AvatarActor.Get();
	const UBossEncounterComponent* Component = Avatar ? Avatar->FindComponentByClass<UBossEncounterComponent>() : nullptr;
	return Component && !Component->bEncounterStopped && !Component->bIntroHold
		&& !Component->ActiveAttackAbility && Component->EncounterState == EBossEncounterState::Preparing
		&& MatchesSelectedAttack(*Component);
}

void URDCABossAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Encounter = ActorInfo && ActorInfo->AvatarActor.IsValid()
		? ActorInfo->AvatarActor->FindComponentByClass<UBossEncounterComponent>() : nullptr;
	if (!Encounter || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}
	Encounter->ActiveAttackAbility = this;
	PhaseElapsed = 0.0f;
	BeginWarning();
}

void URDCABossAttackAbility::AdvanceAttack(const float DeltaTime)
{
	if (!IsActive() || !Encounter) { return; }
	PhaseElapsed += FMath::Max(DeltaTime, 0.0f);
	Encounter->StateElapsed = PhaseElapsed;
	Encounter->UpdateLaserVerticalMovement();
	Encounter->UpdateBossFacing(DeltaTime);
	if (Encounter->EncounterState == EBossEncounterState::Preparing)
	{
		TickWarning(DeltaTime);
		if (IsActive() && Encounter && PhaseElapsed >= Encounter->GetCurrentAttackWarningDuration())
		{
			Encounter->SetEncounterState(EBossEncounterState::Attacking);
		}
	}
	else if (Encounter->EncounterState == EBossEncounterState::Attacking)
	{
		TickAttack(DeltaTime);
		// Damage/overlap callbacks can stop the fight and cancel this ability synchronously.
		if (IsActive() && Encounter && PhaseElapsed >= Encounter->GetCurrentAttackActiveDuration())
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
		}
	}
}

void URDCABossAttackAbility::EnterActivePhase()
{
	if (!IsActive() || !Encounter) { return; }
	PhaseElapsed = 0.0f;
	Encounter->StateElapsed = 0.0f;
	BeginAttack();
}

void URDCABossAttackAbility::CancelAttack()
{
	if (IsActive()) { CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false); }
}

void URDCABossAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility, const bool bWasCancelled)
{
	if (!IsActive()) { return; }
	UBossEncounterComponent* Component = Encounter;
	const bool bOwnsAttack = Component && Component->ActiveAttackAbility == this;
	if (bOwnsAttack)
	{
		Component->ActiveAttackAbility = nullptr;
		Component->FinishAttackExecution(!bWasCancelled);
		if (bWasCancelled) { Component->RestoreBossLaserHeight(); }
	}
	Encounter = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	if (bOwnsAttack) { Component->HandleAttackAbilityEnded(bWasCancelled); }
}

URDCABossBarrageAbility::URDCABossBarrageAbility()
{
	AbilityTags.AddTag(RDCAGameplayTags::Ability_Boss_Barrage);
}
bool URDCABossBarrageAbility::MatchesSelectedAttack(const UBossEncounterComponent& C) const
{
	return C.GetCurrentAttack() == EBossAttackType::AimedVolley || C.GetCurrentAttack() == EBossAttackType::FanBarrage;
}
void URDCABossBarrageAbility::BeginWarning() { Encounter->BeginBarrageWarning(); }
void URDCABossBarrageAbility::BeginAttack() { Encounter->BeginBarrageAttack(); }
void URDCABossBarrageAbility::TickAttack(float DeltaTime) { Encounter->TickBarrage(DeltaTime); }

URDCABossShockwaveAbility::URDCABossShockwaveAbility()
{
	AbilityTags.AddTag(RDCAGameplayTags::Ability_Boss_Shockwave);
}
bool URDCABossShockwaveAbility::MatchesSelectedAttack(const UBossEncounterComponent& C) const
{
	return C.GetCurrentAttack() == EBossAttackType::Shockwave;
}
void URDCABossShockwaveAbility::BeginWarning() { Encounter->BeginShockwaveWarning(); }
void URDCABossShockwaveAbility::BeginAttack() { Encounter->BeginShockwaveAttack(); }
void URDCABossShockwaveAbility::TickAttack(float DeltaTime) { Encounter->TickShockwaveAttack(); }

URDCABossLaserAbility::URDCABossLaserAbility()
{
	AbilityTags.AddTag(RDCAGameplayTags::Ability_Boss_Laser);
}
bool URDCABossLaserAbility::MatchesSelectedAttack(const UBossEncounterComponent& C) const
{
	return C.GetCurrentAttack() == EBossAttackType::SweepLaser;
}
void URDCABossLaserAbility::BeginWarning() { Encounter->BeginLaserWarning(); }
void URDCABossLaserAbility::BeginAttack() { Encounter->BeginLaserAttack(); }
void URDCABossLaserAbility::TickWarning(float DeltaTime) { Encounter->UpdateLaserWarning(); }
