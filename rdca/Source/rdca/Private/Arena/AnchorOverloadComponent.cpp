#include "Arena/AnchorOverloadComponent.h"

#include "Arena/AttachSurfaceComponent.h"
#include "Arena/AnchorSpawnManager.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/PhaseCrashComponent.h"
#include "Player/PlayerHealthComponent.h"
#include "rdca.h"

UAnchorOverloadComponent::UAnchorOverloadComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UAnchorOverloadComponent::BeginPlay()
{
	Super::BeginPlay();

	AttachSurface = GetOwner()->FindComponentByClass<UAttachSurfaceComponent>();
	if (AttachSurface.IsValid())
	{
		AttachSurface->SetCollisionObjectType(ECC_WorldDynamic);
		AttachSurface->SetCollisionResponseToChannel(
			ECC_WorldDynamic,
			ECR_Overlap);
		AttachSurface->SetGenerateOverlapEvents(true);
	}
	TArray<UStaticMeshComponent*> MeshComponents;
	GetOwner()->GetComponents<UStaticMeshComponent>(MeshComponents);
	for (UStaticMeshComponent* Mesh : MeshComponents)
	{
		if (Mesh->GetName().Equals(TEXT("AnchorVisual"), ESearchCase::IgnoreCase))
		{
			AnchorVisual = Mesh;
			break;
		}
	}

	if (!AttachSurface.IsValid() || !AnchorVisual.IsValid())
	{
		UE_LOG(
			LogRDCAPlayer,
			Warning,
			TEXT("Anchor Overload setup incomplete. Anchor=%s Surface=%s Visual=%s"),
			*GetNameSafe(GetOwner()),
			AttachSurface.IsValid() ? TEXT("found") : TEXT("missing"),
			AnchorVisual.IsValid() ? TEXT("found") : TEXT("missing"));
	}

	ApplyAnchorMaterial(NormalMaterial);
	if (AnchorVisual.IsValid())
	{
		OverloadMaterialInstance =
			AnchorVisual->CreateDynamicMaterialInstance(0);
		UpdateOverloadMaterial(0.0f);
	}
}

void UAnchorOverloadComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (OverloadState == EAnchorOverloadState::Recovering)
	{
		StateElapsed += DeltaTime;
		if (StateElapsed >= RecoveryDuration)
		{
			FinishRecovery();
		}
		return;
	}

	if (AttachedPlayer.IsValid())
	{
		const float FullAttachmentDuration =
			FMath::Max(SafeAttachmentDuration + WarningDuration, 0.1f);
		AddOverloadAmount(DeltaTime / FullAttachmentDuration);
	}
}

void UAnchorOverloadComponent::NotifyPlayerAttached(APawn* PlayerPawn)
{
	if (!PlayerPawn || OverloadState == EAnchorOverloadState::Recovering)
	{
		return;
	}

	AttachedPlayer = PlayerPawn;
}

void UAnchorOverloadComponent::NotifyPlayerDetached(APawn* PlayerPawn)
{
	if (AttachedPlayer.Get() != PlayerPawn)
	{
		return;
	}

	AttachedPlayer.Reset();
}

float UAnchorOverloadComponent::GetWarningRemainingTime() const
{
	return OverloadState == EAnchorOverloadState::Warning
		? (1.0f - CurrentOverloadAlpha)
			* FMath::Max(SafeAttachmentDuration + WarningDuration, 0.1f)
		: 0.0f;
}

void UAnchorOverloadComponent::AddOverloadAmount(
	const float NormalizedAmount)
{
	if (NormalizedAmount <= 0.0f
		|| OverloadState == EAnchorOverloadState::Recovering)
	{
		return;
	}

	CurrentOverloadAlpha = FMath::Clamp(
		CurrentOverloadAlpha + NormalizedAmount,
		0.0f,
		1.0f);
	UpdateOverloadMaterial(CurrentOverloadAlpha);

	if (CurrentOverloadAlpha >= 1.0f)
	{
		TriggerOverload(true);
	}
	else
	{
		const float FullAttachmentDuration =
			FMath::Max(SafeAttachmentDuration + WarningDuration, 0.1f);
		const float WarningThreshold = FMath::Clamp(
			SafeAttachmentDuration / FullAttachmentDuration,
			0.0f,
			1.0f);
		SetOverloadState(
			CurrentOverloadAlpha >= WarningThreshold
				? EAnchorOverloadState::Warning
				: EAnchorOverloadState::Normal);
	}
}

void UAnchorOverloadComponent::SetOverloadState(
	const EAnchorOverloadState NewState)
{
	if (OverloadState == NewState)
	{
		return;
	}

	const EAnchorOverloadState PreviousState = OverloadState;
	OverloadState = NewState;
	StateElapsed = 0.0f;

	if (NewState == EAnchorOverloadState::Normal)
	{
		UpdateOverloadMaterial(CurrentOverloadAlpha);
	}

	OnOverloadStateChanged.Broadcast(PreviousState, NewState);
	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Anchor overload state. Anchor=%s From=%d To=%d"),
		*GetNameSafe(GetOwner()),
		static_cast<int32>(PreviousState),
		static_cast<int32>(NewState));
}

void UAnchorOverloadComponent::ShatterAfterPlayerDeparture()
{
	if (OverloadState == EAnchorOverloadState::Recovering)
	{
		return;
	}

	AttachedPlayer.Reset();
	TriggerOverload(false);
}

void UAnchorOverloadComponent::TriggerOverload(
	const bool bDamageAttachedPlayer)
{
	APawn* PlayerPawn = AttachedPlayer.Get();
	SetOverloadState(EAnchorOverloadState::Recovering);
	SetAnchorAvailable(false);

	if (bDamageAttachedPlayer && PlayerPawn)
	{
		if (UPlayerHealthComponent* Health =
				PlayerPawn->FindComponentByClass<UPlayerHealthComponent>())
		{
			Health->ReceiveDamage(OverloadDamage);
		}
		if (UPhaseCrashComponent* PhaseCrash =
				PlayerPawn->FindComponentByClass<UPhaseCrashComponent>())
		{
			PhaseCrash->ForceDetachFromAttachment();
		}
	}
	AttachedPlayer.Reset();

	if (FractureActorClass && GetWorld())
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = GetOwner();
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnedFractureActor = GetWorld()->SpawnActor<AActor>(
			FractureActorClass,
			GetOwner()->GetActorTransform(),
			SpawnParameters);
		if (SpawnedFractureActor.IsValid())
		{
			SpawnedFractureActor->SetLifeSpan(RecoveryDuration);
		}
	}

	if (AAnchorSpawnManager* SpawnManager =
			Cast<AAnchorSpawnManager>(GetOwner()->GetOwner()))
	{
		SpawnManager->HandleManagedAnchorOverloaded(GetOwner());
	}

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Anchor shattered. Anchor=%s Reason=%s Player=%s Damage=%d"),
		*GetNameSafe(GetOwner()),
		bDamageAttachedPlayer ? TEXT("Overload") : TEXT("PlayerDeparture"),
		*GetNameSafe(PlayerPawn),
		bDamageAttachedPlayer ? OverloadDamage : 0);
}

void UAnchorOverloadComponent::FinishRecovery()
{
	if (SpawnedFractureActor.IsValid())
	{
		SpawnedFractureActor->Destroy();
	}
	SpawnedFractureActor.Reset();
	CurrentOverloadAlpha = 0.0f;
	SetAnchorAvailable(true);
	SetOverloadState(EAnchorOverloadState::Normal);
}

void UAnchorOverloadComponent::SetAnchorAvailable(const bool bAvailable)
{
	if (AnchorVisual.IsValid())
	{
		AnchorVisual->SetVisibility(bAvailable);
		AnchorVisual->SetHiddenInGame(!bAvailable);
	}
	if (AttachSurface.IsValid())
	{
		AttachSurface->SetCollisionEnabled(
			bAvailable
				? ECollisionEnabled::QueryOnly
				: ECollisionEnabled::NoCollision);
	}
}

void UAnchorOverloadComponent::ApplyAnchorMaterial(
	UMaterialInterface* Material)
{
	if (AnchorVisual.IsValid() && Material)
	{
		AnchorVisual->SetMaterial(0, Material);
	}
}

void UAnchorOverloadComponent::UpdateOverloadMaterial(
	const float OverloadAlpha)
{
	if (OverloadMaterialInstance)
	{
		OverloadMaterialInstance->SetScalarParameterValue(
			TEXT("OverloadAlpha"),
			FMath::Clamp(OverloadAlpha, 0.0f, 1.0f));
	}
}
