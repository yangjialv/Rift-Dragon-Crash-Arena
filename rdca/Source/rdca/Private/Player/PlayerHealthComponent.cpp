#include "Player/PlayerHealthComponent.h"

#include "rdca.h"

UPlayerHealthComponent::UPlayerHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPlayerHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	ResetHealth();
}

void UPlayerHealthComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	InvulnerabilityRemaining =
		FMath::Max(InvulnerabilityRemaining - DeltaTime, 0.0f);
}

bool UPlayerHealthComponent::ReceiveDamage(const int32 DamageAmount)
{
	if (DamageAmount <= 0 || IsDefeated() || InvulnerabilityRemaining > 0.0f)
	{
		return false;
	}

	CurrentHealth = FMath::Max(CurrentHealth - DamageAmount, 0);
	InvulnerabilityRemaining = InvulnerabilityDuration;
	OnHealthChanged.Broadcast(CurrentHealth, MaximumHealth);

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Player damaged. Player=%s HP=%d/%d"),
		*GetNameSafe(GetOwner()),
		CurrentHealth,
		MaximumHealth);

	if (IsDefeated())
	{
		OnDefeated.Broadcast();
		UE_LOG(LogRDCAPlayer, Log, TEXT("Player defeated."));
	}
	return true;
}

void UPlayerHealthComponent::ResetHealth()
{
	CurrentHealth = FMath::Max(MaximumHealth, 1);
	InvulnerabilityRemaining = 0.0f;
	OnHealthChanged.Broadcast(CurrentHealth, MaximumHealth);
}
