#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "RDCAAbilitySystemComponent.generated.h"

class URDCAHealthAttributeSet;

/** Single-player GAS host shared by native player and existing Blueprint-only Boss. */
UCLASS(ClassGroup = (Abilities), meta = (BlueprintSpawnableComponent))
class RDCA_API URDCAAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	URDCAAbilitySystemComponent();
	static URDCAAbilitySystemComponent* FindOrCreateForActor(AActor* Actor);
	void InitializeForOwner();
	void ResetHealth(float MaximumHealth);
	bool ApplyDamage(float Amount, AActor* DamageSource = nullptr, float ImmunityDuration = 0.0f);
	float GetHealth() const;
	float GetMaxHealth() const;
	bool IsHealthInitialized() const { return bHealthInitialized; }
	bool IsDamageImmune() const;
	void GrantPlayerAbilities();
	void GrantBossAbilities();
	void ApplyGroundDashCooldown(float Duration);
	void ClearGroundDashCooldown();
	float GetGroundDashCooldownRemaining() const;
	bool IsGroundDashOnCooldown() const;
	void ApplyPhaseCrashCooldown(float Duration);
	void ClearPhaseCrashCooldown();
	float GetPhaseCrashCooldownRemaining() const;
	bool IsPhaseCrashOnCooldown() const;

protected:
	virtual void BeginPlay() override;

private:
	void ApplyTimedCooldown(TSubclassOf<UGameplayEffect> EffectClass, FGameplayTag Tag, float Duration);
	float GetCooldownRemainingForTag(FGameplayTag Tag) const;
	void HandleHealthChanged(const FOnAttributeChangeData& Data);

	UPROPERTY(Transient)
	TObjectPtr<URDCAHealthAttributeSet> HealthAttributes;

	FActiveGameplayEffectHandle HitImmunityHandle;
	bool bHealthInitialized = false;
	bool bApplyingDamage = false;
};
