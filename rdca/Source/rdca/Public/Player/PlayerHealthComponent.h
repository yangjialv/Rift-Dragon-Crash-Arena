#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnPlayerHealthChanged,
	int32,
	CurrentHealth,
	int32,
	MaximumHealth);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerDefeated);

UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class RDCA_API UPlayerHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerHealthComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable, Category = "Player|Health")
	bool ReceiveDamage(int32 DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "Player|Health")
	void ResetHealth();

	UFUNCTION(BlueprintPure, Category = "Player|Health")
	int32 GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Player|Health")
	int32 GetMaximumHealth() const { return MaximumHealth; }

	UFUNCTION(BlueprintPure, Category = "Player|Health")
	bool IsDefeated() const { return CurrentHealth <= 0; }

	UFUNCTION(BlueprintPure, Category = "Player|Health")
	float GetHealthPercent() const
	{
		return MaximumHealth > 0
			? static_cast<float>(CurrentHealth) / MaximumHealth
			: 0.0f;
	}

	UFUNCTION(BlueprintPure, Category = "Player|Health")
	bool IsInvulnerable() const { return InvulnerabilityRemaining > 0.0f; }

	UPROPERTY(BlueprintAssignable, Category = "Player|Health")
	FOnPlayerHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Player|Health")
	FOnPlayerDefeated OnDefeated;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Health",
		meta = (ClampMin = "1"))
	int32 MaximumHealth = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Health",
		meta = (ClampMin = "0.0"))
	float InvulnerabilityDuration = 1.0f;

private:
	int32 CurrentHealth = 3;
	float InvulnerabilityRemaining = 0.0f;
};
