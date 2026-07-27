#include "Arena/AnchorOverloadComponent.h"

#include "Arena/AttachSurfaceComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInterface.h"
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

	if (!AttachedPlayer.IsValid())
	{
		return;
	}

	StateElapsed += DeltaTime;
	if (OverloadState == EAnchorOverloadState::Normal
		&& StateElapsed >= SafeAttachmentDuration)
	{
		SetOverloadState(EAnchorOverloadState::Warning);
	}
	else if (OverloadState == EAnchorOverloadState::Warning
		&& StateElapsed >= WarningDuration)
	{
		TriggerOverload();
	}
}

void UAnchorOverloadComponent::NotifyPlayerAttached(APawn* PlayerPawn)
{
	if (!PlayerPawn || OverloadState == EAnchorOverloadState::Recovering)
	{
		return;
	}

	AttachedPlayer = PlayerPawn;
	StateElapsed = 0.0f;
	SetOverloadState(EAnchorOverloadState::Normal);
}

void UAnchorOverloadComponent::NotifyPlayerDetached(APawn* PlayerPawn)
{
	if (AttachedPlayer.Get() != PlayerPawn)
	{
		return;
	}

	AttachedPlayer.Reset();
	if (OverloadState != EAnchorOverloadState::Recovering)
	{
		StateElapsed = 0.0f;
		SetOverloadState(EAnchorOverloadState::Normal);
	}
}

float UAnchorOverloadComponent::GetWarningRemainingTime() const
{
	return OverloadState == EAnchorOverloadState::Warning
		? FMath::Max(WarningDuration - StateElapsed, 0.0f)
		: 0.0f;
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
		ApplyAnchorMaterial(NormalMaterial);
	}
	else if (NewState == EAnchorOverloadState::Warning)
	{
		ApplyAnchorMaterial(WarningMaterial);
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

void UAnchorOverloadComponent::TriggerOverload()
{
	APawn* PlayerPawn = AttachedPlayer.Get();
	SetOverloadState(EAnchorOverloadState::Recovering);
	SetAnchorAvailable(false);

	if (PlayerPawn)
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
	}

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Anchor overloaded. Anchor=%s Player=%s Damage=%d"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(PlayerPawn),
		OverloadDamage);
}

void UAnchorOverloadComponent::FinishRecovery()
{
	if (SpawnedFractureActor.IsValid())
	{
		SpawnedFractureActor->Destroy();
	}
	SpawnedFractureActor.Reset();
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
