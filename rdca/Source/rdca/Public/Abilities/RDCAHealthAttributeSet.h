#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "RDCAHealthAttributeSet.generated.h"

/** The single runtime source of health for both the player and the Boss. */
UCLASS()
class RDCA_API URDCAHealthAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	URDCAHealthAttributeSet();

	UPROPERTY(BlueprintReadOnly, Category = "RDCA|Attributes")
	FGameplayAttributeData Health;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(URDCAHealthAttributeSet, Health)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(Health)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(Health)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(Health)

	UPROPERTY(BlueprintReadOnly, Category = "RDCA|Attributes")
	FGameplayAttributeData MaxHealth;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(URDCAHealthAttributeSet, MaxHealth)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(MaxHealth)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(MaxHealth)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(MaxHealth)

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

private:
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
};
