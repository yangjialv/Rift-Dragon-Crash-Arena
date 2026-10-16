#include "Player/PlayerHealthComponent.h"

#include "Abilities/RDCAAbilitySystemComponent.h"
#include "Abilities/RDCAHealthAttributeSet.h"
#include "Abilities/RDCAPresentation.h"
#include "Camera/CameraShakeBase.h"
#include "Components/MeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "rdca.h"

UPlayerHealthComponent::UPlayerHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPlayerHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeFeedbackMaterials();
	ResetHealth();
}

void UPlayerHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AbilitySystem)
	{
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(URDCAHealthAttributeSet::GetHealthAttribute()).RemoveAll(this);
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(URDCAHealthAttributeSet::GetMaxHealthAttribute()).RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

void UPlayerHealthComponent::InitializeAbilitySystem()
{
	if (AbilitySystem)
	{
		return;
	}
	AbilitySystem = URDCAAbilitySystemComponent::FindOrCreateForActor(GetOwner());
	if (AbilitySystem)
	{
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(URDCAHealthAttributeSet::GetHealthAttribute())
			.AddUObject(this, &ThisClass::HandleHealthChanged);
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(URDCAHealthAttributeSet::GetMaxHealthAttribute())
			.AddUObject(this, &ThisClass::HandleMaxHealthChanged);
	}
}

int32 UPlayerHealthComponent::GetCurrentHealth() const
{
	return AbilitySystem && AbilitySystem->IsHealthInitialized()
		? FMath::RoundToInt(AbilitySystem->GetHealth()) : FMath::Max(MaximumHealth, 1);
}

int32 UPlayerHealthComponent::GetMaximumHealth() const
{
	return AbilitySystem && AbilitySystem->IsHealthInitialized()
		? FMath::RoundToInt(AbilitySystem->GetMaxHealth()) : FMath::Max(MaximumHealth, 1);
}

bool UPlayerHealthComponent::IsInvulnerable() const
{
	return AbilitySystem && AbilitySystem->IsDamageImmune();
}

void UPlayerHealthComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (HitFlashRemaining > 0.0f)
	{
		HitFlashRemaining = FMath::Max(HitFlashRemaining - DeltaTime, 0.0f);
		const float FlashAlpha = HitFlashDuration > 0.0f
			? HitFlashRemaining / HitFlashDuration
			: 0.0f;
		SetHitFlashValue(FlashAlpha * HitFlashMaximumValue);
	}
}

bool UPlayerHealthComponent::ReceiveDamage(const int32 DamageAmount)
{
	InitializeAbilitySystem();
	if (AbilitySystem && !AbilitySystem->IsHealthInitialized())
	{
		ResetHealth();
	}
	return AbilitySystem && AbilitySystem->ApplyDamage(DamageAmount, nullptr, InvulnerabilityDuration);
}

void UPlayerHealthComponent::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	if (bResettingHealth)
	{
		return;
	}
	OnHealthChanged.Broadcast(GetCurrentHealth(), GetMaximumHealth());
	if (Data.NewValue >= Data.OldValue)
	{
		return;
	}
	const int32 DamageAmount = FMath::RoundToInt(Data.OldValue - Data.NewValue);
	OnDamaged.Broadcast(DamageAmount);
	RDCAPresentation::PlayerDamaged(this, DamageAmount);

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Player damaged. Player=%s HP=%d/%d"),
		*GetNameSafe(GetOwner()),
		GetCurrentHealth(),
		GetMaximumHealth());

	if (IsDefeated())
	{
		OnDefeated.Broadcast();
		UE_LOG(LogRDCAPlayer, Log, TEXT("Player defeated."));
	}
}

void UPlayerHealthComponent::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	if (!bResettingHealth)
	{
		OnHealthChanged.Broadcast(GetCurrentHealth(), GetMaximumHealth());
	}
}

void UPlayerHealthComponent::ResetHealth()
{
	InitializeAbilitySystem();
	if (!AbilitySystem)
	{
		return;
	}
	TGuardValue<bool> ResetGuard(bResettingHealth, true);
	AbilitySystem->ResetHealth(FMath::Max(MaximumHealth, 1));
	HitFlashRemaining = 0.0f;
	SetHitFlashValue(0.0f);
	OnHealthChanged.Broadcast(GetCurrentHealth(), GetMaximumHealth());
}

void UPlayerHealthComponent::InitializeFeedbackMaterials()
{
	FeedbackMaterials.Reset();
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<UMeshComponent*> MeshComponents;
	Owner->GetComponents<UMeshComponent>(MeshComponents);
	for (UMeshComponent* Mesh : MeshComponents)
	{
		if (!Mesh)
		{
			continue;
		}
		for (int32 MaterialIndex = 0;
			MaterialIndex < Mesh->GetNumMaterials();
			++MaterialIndex)
		{
			if (UMaterialInstanceDynamic* DynamicMaterial =
					Mesh->CreateAndSetMaterialInstanceDynamic(MaterialIndex))
			{
				FeedbackMaterials.Add(DynamicMaterial);
			}
		}
	}
	SetHitFlashValue(0.0f);
}

void UPlayerHealthComponent::PlayDamageFeedback(const int32 DamageAmount)
{
	HitFlashRemaining = FMath::Max(HitFlashDuration, 0.01f);
	SetHitFlashValue(HitFlashMaximumValue);

	AActor* Owner = GetOwner();
	if (HitSound && Owner)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			HitSound,
			Owner->GetActorLocation(),
			HitSoundVolume);
	}

	if (HitCameraShakeClass && Owner)
	{
		const APawn* OwnerPawn = Cast<APawn>(Owner);
		if (APlayerController* PlayerController = OwnerPawn
			? Cast<APlayerController>(OwnerPawn->GetController())
			: nullptr)
		{
			PlayerController->ClientStartCameraShake(HitCameraShakeClass);
		}
	}

	UE_LOG(
		LogRDCAPlayer,
		Verbose,
		TEXT("Player damage feedback. Player=%s Damage=%d Materials=%d Shake=%s Sound=%s"),
		*GetNameSafe(Owner),
		DamageAmount,
		FeedbackMaterials.Num(),
		HitCameraShakeClass ? TEXT("true") : TEXT("false"),
		HitSound ? TEXT("true") : TEXT("false"));
}

void UPlayerHealthComponent::SetHitFlashValue(const float Value)
{
	for (UMaterialInstanceDynamic* Material : FeedbackMaterials)
	{
		if (Material)
		{
			Material->SetScalarParameterValue(HitFlashParameterName, Value);
		}
	}
}
