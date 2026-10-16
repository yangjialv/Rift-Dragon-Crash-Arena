#include "Player/RDCACombatCameraComponent.h"

#include "Arena/ArenaCombatBounds.h"
#include "Boss/BossEncounterComponent.h"
#include "Components/MeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/SpringArmComponent.h"
#include "Player/PlayerCorePawn.h"

URDCACombatCameraComponent::URDCACombatCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URDCACombatCameraComponent::InitializeCamera()
{
	Pawn = Cast<APlayerCorePawn>(GetOwner());
	if (Pawn) { FindBossCameraTarget(); }
}

void URDCACombatCameraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestoreCameraOccluders();
	Super::EndPlay(EndPlayReason);
}

void URDCACombatCameraComponent::RestoreCameraOccluders()
{
	for (const TWeakObjectPtr<UMeshComponent>& Component : CameraOccludedComponents)
	{
		if (Component.IsValid()) { Component->SetVisibility(true, true); }
	}
	CameraOccludedComponents.Reset();
}

void URDCACombatCameraComponent::FindBossCameraTarget()
{
	BossCameraTarget.Reset();
	if (!GetWorld())
	{
		return;
	}

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (IsValid(Candidate)
			&& Candidate->FindComponentByClass<UBossEncounterComponent>())
		{
			BossCameraTarget = Candidate;
			return;
		}
	}
}

void URDCACombatCameraComponent::UpdateCombatCamera(const float DeltaTime)
{
	if (!Pawn || !Pawn->SpringArm)
	{
		return;
	}
	if (!BossCameraTarget.IsValid())
	{
		FindBossCameraTarget();
	}

	FVector DesiredTargetOffset(
		0.0f,
		0.0f,
		Pawn->NearCameraPoint.FocusHeight);
	float DesiredArmLength = Pawn->NearCameraPoint.ArmLength;
	if (BossCameraTarget.IsValid())
	{
		const FVector PlayerLocation = Pawn->GetActorLocation();
		const FVector BossLocation = BossCameraTarget->GetActorLocation();
		const FVector PlayerToBoss = BossLocation - PlayerLocation;
		FVector ToBoss = PlayerToBoss;
		ToBoss.Z = 0.0f;
		const float PlayerBossDistance = ToBoss.Size();
		const float DistanceRange = FMath::Max(
			Pawn->FarCameraPointDistance - Pawn->NearCameraPointDistance,
			1.0f);
		const float CameraAlpha = FMath::Clamp(
			(PlayerBossDistance - Pawn->NearCameraPointDistance) / DistanceRange,
			0.0f,
			1.0f);
		const float FocusHeight = FMath::Lerp(
			Pawn->NearCameraPoint.FocusHeight,
			Pawn->FarCameraPoint.FocusHeight,
			CameraAlpha);
		const float BossWeight = FMath::Lerp(
			Pawn->NearCameraPoint.BossFramingWeight,
			Pawn->FarCameraPoint.BossFramingWeight,
			CameraAlpha);
		const float Pitch = FMath::Lerp(
			Pawn->NearCameraPoint.Pitch,
			Pawn->FarCameraPoint.Pitch,
			CameraAlpha);
		DesiredTargetOffset = FVector(0.0f, 0.0f, FocusHeight)
			+ PlayerToBoss * FMath::Clamp(BossWeight, 0.0f, 0.5f);
		if (ToBoss.Normalize())
		{
			const FRotator DesiredRotation(
				Pitch,
				ToBoss.Rotation().Yaw,
				0.0f);
			Pawn->SpringArm->SetWorldRotation(FMath::RInterpTo(
				Pawn->SpringArm->GetComponentRotation(),
				DesiredRotation,
				DeltaTime,
				Pawn->CameraRotationInterpSpeed));
		}

		DesiredArmLength = FMath::Lerp(
			Pawn->NearCameraPoint.ArmLength,
			Pawn->FarCameraPoint.ArmLength,
			CameraAlpha);
	}
	else
	{
		const FRotator FallbackRotation(
			Pawn->NearCameraPoint.Pitch,
			-45.0f,
			0.0f);
		Pawn->SpringArm->SetWorldRotation(FMath::RInterpTo(
			Pawn->SpringArm->GetComponentRotation(),
			FallbackRotation,
			DeltaTime,
			Pawn->CameraRotationInterpSpeed));
	}

	const float FramingInterpSpeed =
		DesiredArmLength > Pawn->SpringArm->TargetArmLength
			? FMath::Max(Pawn->CameraFollowInterpSpeed * 2.0f, 8.0f)
			: Pawn->CameraFollowInterpSpeed;
	Pawn->SpringArm->TargetOffset = FMath::VInterpTo(
		Pawn->SpringArm->TargetOffset,
		DesiredTargetOffset,
		DeltaTime,
		FramingInterpSpeed);
	Pawn->SpringArm->TargetArmLength = FMath::FInterpTo(
		Pawn->SpringArm->TargetArmLength,
		DesiredArmLength,
		DeltaTime,
		FramingInterpSpeed);
	UpdateCameraOccluders();
}

void URDCACombatCameraComponent::UpdateCameraOccluders()
{
	if (!Pawn->bHideCameraOccluderPillars || !GetWorld())
	{
		for (const TWeakObjectPtr<UMeshComponent>& Component : CameraOccludedComponents)
		{
			if (Component.IsValid())
			{
				Component->SetVisibility(true, true);
			}
		}
		CameraOccludedComponents.Reset();
		return;
	}

	const FVector ArenaCenter = GetCameraOcclusionCenter();
	FVector PlayerOutward = Pawn->GetActorLocation() - ArenaCenter;
	PlayerOutward.Z = 0.0f;
	const float PlayerCenterDistance = PlayerOutward.Size();
	if (!PlayerOutward.Normalize())
	{
		PlayerOutward = LastCameraOcclusionOutward;
	}
	else
	{
		LastCameraOcclusionOutward = PlayerOutward;
	}
	const float DistanceRange = FMath::Max(
		Pawn->CameraOcclusionEdgeDistance - Pawn->CameraOcclusionCenterDistance,
		1.0f);
	const float DistanceAlpha = FMath::Clamp(
		(PlayerCenterDistance - Pawn->CameraOcclusionCenterDistance) / DistanceRange,
		0.0f,
		1.0f);
	const float HiddenArcDegrees = FMath::Lerp(
		Pawn->CenterHiddenArcDegrees,
		Pawn->EdgeHiddenArcDegrees,
		DistanceAlpha);
	const float MaximumAngle = HiddenArcDegrees * 0.5f;

	TSet<UMeshComponent*> DesiredOccludedComponents;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (!IsValid(Candidate) || Candidate == Pawn || Candidate->IsHidden()
			|| !IsCameraOccluderActor(*Candidate))
		{
			continue;
		}

		FVector PillarOutward = Candidate->GetActorLocation() - ArenaCenter;
		PillarOutward.Z = 0.0f;
		if (!PillarOutward.Normalize())
		{
			continue;
		}
		const float PillarAngle = FMath::RadiansToDegrees(FMath::Acos(
			FMath::Clamp(
				FVector::DotProduct(PlayerOutward, PillarOutward),
				-1.0f,
				1.0f)));
		if (PillarAngle > MaximumAngle)
		{
			continue;
		}

		TArray<UMeshComponent*> MeshComponents;
		Candidate->GetComponents<UMeshComponent>(MeshComponents);
		for (UMeshComponent* MeshComponent : MeshComponents)
		{
			if (MeshComponent && (MeshComponent->IsVisible()
				|| CameraOccludedComponents.Contains(MeshComponent)))
			{
				DesiredOccludedComponents.Add(MeshComponent);
			}
		}
	}

	for (auto It = CameraOccludedComponents.CreateIterator(); It; ++It)
	{
		UMeshComponent* Component = It->Get();
		if (!Component || !DesiredOccludedComponents.Contains(Component))
		{
			if (Component)
			{
				// Actor-level Phase switching may still keep this component hidden.
				// Restoring component visibility never overrides that actor-level state.
				Component->SetVisibility(true, true);
			}
			It.RemoveCurrent();
		}
	}

	for (UMeshComponent* Component : DesiredOccludedComponents)
	{
		if (Component)
		{
			Component->SetVisibility(false, true);
			CameraOccludedComponents.Add(Component);
		}
	}
}

FVector URDCACombatCameraComponent::GetCameraOcclusionCenter()
{
	if (!CameraOcclusionArenaBounds.IsValid() && GetWorld())
	{
		for (TActorIterator<AArenaCombatBounds> It(GetWorld()); It; ++It)
		{
			CameraOcclusionArenaBounds = *It;
			break;
		}
	}
	if (CameraOcclusionArenaBounds.IsValid())
	{
		return CameraOcclusionArenaBounds->GetArenaCenter();
	}
	return BossCameraTarget.IsValid()
		? BossCameraTarget->GetActorLocation()
		: FVector::ZeroVector;
}

bool URDCACombatCameraComponent::IsCameraOccluderActor(const AActor& Actor) const
{
	const FString Prefix = Pawn->CameraOccluderTagPrefix.ToString();
	if (Prefix.IsEmpty())
	{
		return false;
	}

	const auto MatchesPillarTag = [&Prefix](const FName Tag)
	{
		return Tag.ToString().StartsWith(Prefix, ESearchCase::IgnoreCase);
	};

	for (const FName Tag : Actor.Tags)
	{
		if (MatchesPillarTag(Tag))
		{
			return true;
		}
	}

	// Blueprint users commonly place PhaseMap tags in the Static Mesh component's
	// Component Tags field. Support that placement as well as Actor Tags.
	TArray<UActorComponent*> Components;
	Actor.GetComponents<UActorComponent>(Components);
	for (const UActorComponent* Component : Components)
	{
		if (!IsValid(Component))
		{
			continue;
		}

		for (const FName Tag : Component->ComponentTags)
		{
			if (MatchesPillarTag(Tag))
			{
				return true;
			}
		}
	}

	return false;
}
