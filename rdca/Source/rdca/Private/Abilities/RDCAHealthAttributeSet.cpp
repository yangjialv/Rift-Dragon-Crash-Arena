#include "Abilities/RDCAHealthAttributeSet.h"

#include "Abilities/RDCAGameplayTags.h"
#include "GameplayEffectExtension.h"

URDCAHealthAttributeSet::URDCAHealthAttributeSet()
{
	InitMaxHealth(5.0f);
	InitHealth(5.0f);
}

void URDCAHealthAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
}

void URDCAHealthAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void URDCAHealthAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void URDCAHealthAttributeSet::PostAttributeChange(
	const FGameplayAttribute& Attribute, const float OldValue, const float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
	if (Attribute == GetMaxHealthAttribute() && GetHealth() > NewValue)
	{
		SetHealth(NewValue);
	}
}

bool URDCAHealthAttributeSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (!Super::PreGameplayEffectExecute(Data))
	{
		return false;
	}
	// Protect the attribute even if a future ability applies the damage GE directly.
	if (Data.EvaluatedData.Attribute == GetHealthAttribute() && Data.EvaluatedData.Magnitude < 0.0f)
	{
		return !Data.Target.HasMatchingGameplayTag(RDCAGameplayTags::State_Invulnerable)
			&& !Data.Target.HasMatchingGameplayTag(RDCAGameplayTags::State_Dead);
	}
	return true;
}

void URDCAHealthAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		const float ClampedHealth = FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth());
		if (!FMath::IsNearlyEqual(ClampedHealth, GetHealth()))
		{
			SetHealth(ClampedHealth);
		}
	}
}
