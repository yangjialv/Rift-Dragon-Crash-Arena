#include "Abilities/RDCAHealthEffects.h"

#include "Abilities/RDCAGameplayTags.h"
#include "Abilities/RDCAHealthAttributeSet.h"

URDCADamageEffect::URDCADamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = URDCAHealthAttributeSet::GetHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat Damage;
	Damage.DataTag = RDCAGameplayTags::Data_Damage;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Damage);
}

URDCAInvulnerabilityEffect::URDCAInvulnerabilityEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(1.0f);
}
