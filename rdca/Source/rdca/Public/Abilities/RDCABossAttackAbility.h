#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "RDCABossAttackAbility.generated.h"

class UBossEncounterComponent;

/** One warning + one attack. The encounter chooses a skill; GAS owns its lifetime. */
UCLASS(Abstract)
class RDCA_API URDCABossAttackAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	URDCABossAttackAbility();
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// Driven once by the encounter's tick, not by a second independent actor/timer.
	void AdvanceAttack(float DeltaTime);
	void EnterActivePhase();
	void CancelAttack();

protected:
	virtual bool MatchesSelectedAttack(const UBossEncounterComponent& Component) const PURE_VIRTUAL(URDCABossAttackAbility::MatchesSelectedAttack, return false;);
	virtual void BeginWarning() PURE_VIRTUAL(URDCABossAttackAbility::BeginWarning, );
	virtual void BeginAttack() PURE_VIRTUAL(URDCABossAttackAbility::BeginAttack, );
	virtual void TickWarning(float DeltaTime) { }
	virtual void TickAttack(float DeltaTime) { }
	UPROPERTY(Transient)
	TObjectPtr<UBossEncounterComponent> Encounter;

private:
	float PhaseElapsed = 0.0f;
};

UCLASS()
class RDCA_API URDCABossBarrageAbility : public URDCABossAttackAbility
{
	GENERATED_BODY()
public:
	URDCABossBarrageAbility();
protected:
	virtual bool MatchesSelectedAttack(const UBossEncounterComponent& Component) const override;
	virtual void BeginWarning() override;
	virtual void BeginAttack() override;
	virtual void TickAttack(float DeltaTime) override;
};

UCLASS()
class RDCA_API URDCABossShockwaveAbility : public URDCABossAttackAbility
{
	GENERATED_BODY()
public:
	URDCABossShockwaveAbility();
protected:
	virtual bool MatchesSelectedAttack(const UBossEncounterComponent& Component) const override;
	virtual void BeginWarning() override;
	virtual void BeginAttack() override;
	virtual void TickAttack(float DeltaTime) override;
};

UCLASS()
class RDCA_API URDCABossLaserAbility : public URDCABossAttackAbility
{
	GENERATED_BODY()
public:
	URDCABossLaserAbility();
protected:
	virtual bool MatchesSelectedAttack(const UBossEncounterComponent& Component) const override;
	virtual void BeginWarning() override;
	virtual void BeginAttack() override;
	virtual void TickWarning(float DeltaTime) override;
};
