#include "Player/PlayerHealthComponent.h"

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

void UPlayerHealthComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	InvulnerabilityRemaining =
		FMath::Max(InvulnerabilityRemaining - DeltaTime, 0.0f);

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
	if (DamageAmount <= 0 || IsDefeated() || InvulnerabilityRemaining > 0.0f)
	{
		return false;
	}

	CurrentHealth = FMath::Max(CurrentHealth - DamageAmount, 0);
	InvulnerabilityRemaining = InvulnerabilityDuration;
	OnHealthChanged.Broadcast(CurrentHealth, MaximumHealth);
	OnDamaged.Broadcast(DamageAmount);
	PlayDamageFeedback(DamageAmount);

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
	HitFlashRemaining = 0.0f;
	SetHitFlashValue(0.0f);
	OnHealthChanged.Broadcast(CurrentHealth, MaximumHealth);
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
