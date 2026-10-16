#include "Abilities/RDCAAbilitySystemComponent.h"

#include "Abilities/RDCAGameplayTags.h"
#include "Abilities/RDCABossAttackAbility.h"
#include "Abilities/RDCAGroundDashAbility.h"
#include "Abilities/RDCAPhaseCrashAbility.h"
#include "Abilities/RDCAHealthAttributeSet.h"
#include "Abilities/RDCAHealthEffects.h"
#include "GameFramework/Actor.h"
#include "Player/RDCAPlayerControlComponent.h"

URDCAAbilitySystemComponent::URDCAAbilitySystemComponent()
{
	// Network prediction/replication is deliberately outside this migration stage.
	SetIsReplicatedByDefault(false);
}

URDCAAbilitySystemComponent* URDCAAbilitySystemComponent::FindOrCreateForActor(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return nullptr;
	}
	URDCAAbilitySystemComponent* ASC = Actor->FindComponentByClass<URDCAAbilitySystemComponent>();
	if (!ASC)
	{
		// Runtime creation keeps the existing generic Boss Blueprint parent intact.
		// This function must never be called from a UObject constructor.
		ASC = NewObject<URDCAAbilitySystemComponent>(Actor, TEXT("RDCAAbilitySystem"));
		Actor->AddInstanceComponent(ASC);
		ASC->RegisterComponent();
	}
	ASC->InitializeForOwner();
	return ASC;
}

void URDCAAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeForOwner();
}

void URDCAAbilitySystemComponent::InitializeForOwner()
{
	if (!GetOwner())
	{
		return;
	}
	if (GetOwnerActor() != GetOwner() || GetAvatarActor() != GetOwner())
	{
		InitAbilityActorInfo(GetOwner(), GetOwner());
	}
	if (!HealthAttributes)
	{
		HealthAttributes = NewObject<URDCAHealthAttributeSet>(GetOwner());
		AddAttributeSetSubobject(HealthAttributes.Get());
		GetGameplayAttributeValueChangeDelegate(URDCAHealthAttributeSet::GetHealthAttribute())
			.AddUObject(this, &ThisClass::HandleHealthChanged);
	}
}

void URDCAAbilitySystemComponent::ResetHealth(const float MaximumHealth)
{
	InitializeForOwner();
	if (!HealthAttributes)
	{
		return;
	}
	if (HitImmunityHandle.IsValid())
	{
		RemoveActiveGameplayEffect(HitImmunityHandle);
		HitImmunityHandle.Invalidate();
	}
	bHealthInitialized = true;
	CancelAllAbilities();
	if (URDCAPlayerControlComponent* Control = GetOwner()->FindComponentByClass<URDCAPlayerControlComponent>())
	{
		Control->ClearActionRequests();
	}
	ClearGroundDashCooldown();
	ClearPhaseCrashCooldown();
	SetNumericAttributeBase(URDCAHealthAttributeSet::GetMaxHealthAttribute(), FMath::Max(MaximumHealth, 1.0f));
	SetNumericAttributeBase(URDCAHealthAttributeSet::GetHealthAttribute(), GetMaxHealth());
	SetLooseGameplayTagCount(RDCAGameplayTags::State_Dead, 0);
}

bool URDCAAbilitySystemComponent::ApplyDamage(
	const float Amount, AActor* DamageSource, const float ImmunityDuration)
{
	if (!bHealthInitialized || Amount <= 0.0f || !FMath::IsFinite(Amount)
		|| GetHealth() <= 0.0f || IsDamageImmune() || bApplyingDamage)
	{
		return false;
	}
	// Attribute delegates run synchronously; prevent nested hits before immunity is installed.
	TGuardValue<bool> DamageGuard(bApplyingDamage, true);
	const float PreviousHealth = GetHealth();
	FGameplayEffectContextHandle Context = MakeEffectContext();
	Context.AddSourceObject(DamageSource ? DamageSource : GetOwner());
	if (DamageSource)
	{
		Context.AddInstigator(DamageSource, DamageSource);
	}
	FGameplayEffectSpecHandle Damage = MakeOutgoingSpec(URDCADamageEffect::StaticClass(), 1.0f, Context);
	if (!Damage.IsValid())
	{
		return false;
	}
	Damage.Data->SetSetByCallerMagnitude(RDCAGameplayTags::Data_Damage, -Amount);
	ApplyGameplayEffectSpecToSelf(*Damage.Data);
	if (GetHealth() >= PreviousHealth)
	{
		return false;
	}
	if (ImmunityDuration > 0.0f && GetHealth() > 0.0f)
	{
		FGameplayEffectSpecHandle Immunity = MakeOutgoingSpec(
			URDCAInvulnerabilityEffect::StaticClass(), 1.0f, MakeEffectContext());
		if (Immunity.IsValid())
		{
			Immunity.Data->SetDuration(ImmunityDuration, true);
			Immunity.Data->DynamicGrantedTags.AddTag(RDCAGameplayTags::State_Invulnerable);
			HitImmunityHandle = ApplyGameplayEffectSpecToSelf(*Immunity.Data);
		}
	}
	return true;
}

float URDCAAbilitySystemComponent::GetHealth() const
{
	return HealthAttributes ? HealthAttributes->GetHealth() : 0.0f;
}

float URDCAAbilitySystemComponent::GetMaxHealth() const
{
	return HealthAttributes ? HealthAttributes->GetMaxHealth() : 0.0f;
}

bool URDCAAbilitySystemComponent::IsDamageImmune() const
{
	return HasMatchingGameplayTag(RDCAGameplayTags::State_Invulnerable);
}

void URDCAAbilitySystemComponent::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	SetLooseGameplayTagCount(RDCAGameplayTags::State_Dead, Data.NewValue <= 0.0f ? 1 : 0);
	if (Data.NewValue <= 0.0f && Data.OldValue > 0.0f)
	{
		CancelAllAbilities();
		if (URDCAPlayerControlComponent* Control = GetOwner()->FindComponentByClass<URDCAPlayerControlComponent>())
		{
			Control->ClearActionRequests();
		}
	}
}

void URDCAAbilitySystemComponent::GrantPlayerAbilities()
{
	InitializeForOwner();
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	if (!FindAbilitySpecFromClass(URDCAGroundDashAbility::StaticClass()))
	{
		GiveAbility(FGameplayAbilitySpec(URDCAGroundDashAbility::StaticClass(), 1));
	}
	if (!FindAbilitySpecFromClass(URDCAPhaseCrashAbility::StaticClass()))
	{
		GiveAbility(FGameplayAbilitySpec(URDCAPhaseCrashAbility::StaticClass(), 1));
	}
}

void URDCAAbilitySystemComponent::GrantBossAbilities()
{
	InitializeForOwner();
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	for (const TSubclassOf<UGameplayAbility> Class : {URDCABossBarrageAbility::StaticClass(),
		URDCABossShockwaveAbility::StaticClass(), URDCABossLaserAbility::StaticClass()})
	{
		if (!FindAbilitySpecFromClass(Class)) { GiveAbility(FGameplayAbilitySpec(Class, 1)); }
	}
}

void URDCAAbilitySystemComponent::ApplyGroundDashCooldown(const float Duration)
{
	ApplyTimedCooldown(URDCAGroundDashCooldownEffect::StaticClass(), RDCAGameplayTags::Cooldown_Player_GroundDash, Duration);
}

void URDCAAbilitySystemComponent::ApplyPhaseCrashCooldown(const float Duration)
{
	ApplyTimedCooldown(URDCAPhaseCrashCooldownEffect::StaticClass(), RDCAGameplayTags::Cooldown_Player_PhaseCrash, Duration);
}

void URDCAAbilitySystemComponent::ApplyTimedCooldown(
	TSubclassOf<UGameplayEffect> EffectClass, FGameplayTag Tag, const float Duration)
{
	RemoveActiveGameplayEffectBySourceEffect(EffectClass, this);
	if (Duration <= 0.0f)
	{
		return;
	}
	FGameplayEffectSpecHandle Spec = MakeOutgoingSpec(
		EffectClass, 1.0f, MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->SetDuration(Duration, true);
		Spec.Data->DynamicGrantedTags.AddTag(Tag);
		ApplyGameplayEffectSpecToSelf(*Spec.Data);
	}
}

void URDCAAbilitySystemComponent::ClearGroundDashCooldown()
{
	RemoveActiveGameplayEffectBySourceEffect(URDCAGroundDashCooldownEffect::StaticClass(), this);
}

float URDCAAbilitySystemComponent::GetGroundDashCooldownRemaining() const
{
	return GetCooldownRemainingForTag(RDCAGameplayTags::Cooldown_Player_GroundDash);
}

float URDCAAbilitySystemComponent::GetPhaseCrashCooldownRemaining() const
{
	return GetCooldownRemainingForTag(RDCAGameplayTags::Cooldown_Player_PhaseCrash);
}

float URDCAAbilitySystemComponent::GetCooldownRemainingForTag(FGameplayTag Tag) const
{
	FGameplayTagContainer Tags;
	Tags.AddTag(Tag);
	const TArray<float> Times = GetActiveEffectsTimeRemaining(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(Tags));
	float Remaining = 0.0f;
	for (const float Time : Times)
	{
		Remaining = FMath::Max(Remaining, Time);
	}
	return Remaining;
}

bool URDCAAbilitySystemComponent::IsGroundDashOnCooldown() const
{
	return HasMatchingGameplayTag(RDCAGameplayTags::Cooldown_Player_GroundDash);
}

void URDCAAbilitySystemComponent::ClearPhaseCrashCooldown()
{
	RemoveActiveGameplayEffectBySourceEffect(URDCAPhaseCrashCooldownEffect::StaticClass(), this);
}

bool URDCAAbilitySystemComponent::IsPhaseCrashOnCooldown() const
{
	return HasMatchingGameplayTag(RDCAGameplayTags::Cooldown_Player_PhaseCrash);
}
