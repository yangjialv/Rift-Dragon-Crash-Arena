#include "Arena/AnchorSpawnManager.h"

#include "Arena/AttachSurfaceComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "rdca.h"

AAnchorSpawnManager::AAnchorSpawnManager()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AAnchorSpawnManager::BeginPlay()
{
	Super::BeginPlay();
	RuntimeRandomStream.Initialize(
		RandomSeed >= 0
			? RandomSeed
			: FMath::Rand());

	if (bSpawnOnBeginPlay)
	{
		SpawnAnchors();
	}
}

void AAnchorSpawnManager::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	for (FManagedAnchor& Entry : ManagedAnchors)
	{
		AActor* Anchor = Entry.Anchor.Get();
		if (!Anchor || !Entry.bEmerging)
		{
			continue;
		}

		Entry.EmergenceElapsed += DeltaSeconds;
		const float Alpha = FMath::Clamp(
			Entry.EmergenceElapsed / FMath::Max(EmergenceDuration, 0.01f),
			0.0f,
			1.0f);
		const float SmoothedAlpha = FMath::SmoothStep(0.0f, 1.0f, Alpha);
		Anchor->SetActorLocation(FMath::Lerp(
			Entry.StartLocation,
			Entry.FinalTransform.GetLocation(),
			SmoothedAlpha));

		if (Alpha >= 1.0f)
		{
			Entry.bEmerging = false;
			Anchor->SetActorTransform(Entry.FinalTransform);
			SetAnchorInteractionEnabled(*Anchor, true);
		}
	}

	for (int32 Index = PendingReplacements.Num() - 1; Index >= 0; --Index)
	{
		FPendingReplacement& Pending = PendingReplacements[Index];
		Pending.RemainingDelay -= DeltaSeconds;
		if (Pending.RemainingDelay > 0.0f)
		{
			continue;
		}

		if (AActor* Point = ChooseAvailableSpawnPoint(
				Pending.PreviousSpawnPoint.Get()))
		{
			SpawnAnchorAtPoint(*Point);
			PendingReplacements.RemoveAtSwap(Index);
		}
	}

	ManagedAnchors.RemoveAll(
		[](const FManagedAnchor& Entry)
		{
			return !Entry.Anchor.IsValid();
		});
	SpawnedAnchors.RemoveAll(
		[](const TWeakObjectPtr<AActor>& Anchor)
		{
			return !Anchor.IsValid();
		});
}

void AAnchorSpawnManager::SpawnAnchors()
{
	ClearSpawnedAnchors();

	if (!GetWorld() || !AnchorClass)
	{
		UE_LOG(
			LogRDCAPlayer,
			Warning,
			TEXT("Anchor spawn skipped. Manager=%s AnchorClass is not assigned."),
			*GetNameSafe(this));
		return;
	}

	CacheCandidatePoints();

	if (CandidateSpawnPoints.IsEmpty())
	{
		UE_LOG(
			LogRDCAPlayer,
			Warning,
			TEXT("Anchor spawn skipped. Manager=%s No actors found with tag '%s'."),
			*GetNameSafe(this),
			*SpawnPointTag.ToString());
		return;
	}

	for (int32 Index = CandidateSpawnPoints.Num() - 1; Index > 0; --Index)
	{
		const int32 SwapIndex = RuntimeRandomStream.RandRange(0, Index);
		CandidateSpawnPoints.Swap(Index, SwapIndex);
	}

	const int32 SpawnCount = FMath::Min(
		FMath::Max(NumberOfAnchors, 1),
		CandidateSpawnPoints.Num());
	for (int32 Index = 0; Index < SpawnCount; ++Index)
	{
		if (AActor* SpawnPoint = CandidateSpawnPoints[Index].Get())
		{
			SpawnAnchorAtPoint(*SpawnPoint);
		}
	}

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Anchor layout generated. Manager=%s Spawned=%d Candidates=%d Seed=%d"),
		*GetNameSafe(this),
		GetSpawnedAnchorCount(),
		CandidateSpawnPoints.Num(),
		RandomSeed);
}

void AAnchorSpawnManager::ClearSpawnedAnchors()
{
	for (const TWeakObjectPtr<AActor>& Anchor : SpawnedAnchors)
	{
		if (Anchor.IsValid())
		{
			Anchor->Destroy();
		}
	}
	SpawnedAnchors.Reset();
	ManagedAnchors.Reset();
	PendingReplacements.Reset();
}

int32 AAnchorSpawnManager::GetSpawnedAnchorCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AActor>& Anchor : SpawnedAnchors)
	{
		if (Anchor.IsValid())
		{
			++Count;
		}
	}
	return Count;
}

void AAnchorSpawnManager::HandleManagedAnchorOverloaded(AActor* OverloadedAnchor)
{
	if (!IsValid(OverloadedAnchor))
	{
		return;
	}

	AActor* PreviousPoint = nullptr;
	for (const FManagedAnchor& Entry : ManagedAnchors)
	{
		if (Entry.Anchor.Get() == OverloadedAnchor)
		{
			PreviousPoint = Entry.SpawnPoint.Get();
			break;
		}
	}

	FPendingReplacement& Pending = PendingReplacements.AddDefaulted_GetRef();
	Pending.PreviousSpawnPoint = PreviousPoint;
	Pending.RemainingDelay = ReplacementDelay;
	OverloadedAnchor->SetLifeSpan(0.01f);

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Managed anchor overloaded. Anchor=%s PreviousPoint=%s ReplacementDelay=%.2f"),
		*GetNameSafe(OverloadedAnchor),
		*GetNameSafe(PreviousPoint),
		ReplacementDelay);
}

void AAnchorSpawnManager::CacheCandidatePoints()
{
	TArray<AActor*> FoundPoints;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), SpawnPointTag, FoundPoints);
	FoundPoints.RemoveAll(
		[this](const AActor* Candidate)
		{
			return !Candidate || Candidate == this;
		});

	CandidateSpawnPoints.Reset();
	for (AActor* Point : FoundPoints)
	{
		CandidateSpawnPoints.Add(Point);
	}
}

bool AAnchorSpawnManager::SpawnAnchorAtPoint(AActor& SpawnPoint)
{
	FTransform FinalTransform = SpawnPoint.GetActorTransform();
	FTransform InitialTransform = FinalTransform;
	const FVector StartLocation =
		FinalTransform.GetLocation() - FVector::UpVector * EmergenceDepth;
	InitialTransform.SetLocation(StartLocation);

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* SpawnedAnchor = GetWorld()->SpawnActor<AActor>(
		AnchorClass,
		InitialTransform,
		SpawnParameters);
	if (!SpawnedAnchor)
	{
		return false;
	}

	SetAnchorInteractionEnabled(*SpawnedAnchor, false);
	SpawnedAnchors.Add(SpawnedAnchor);
	FManagedAnchor& Entry = ManagedAnchors.AddDefaulted_GetRef();
	Entry.Anchor = SpawnedAnchor;
	Entry.SpawnPoint = &SpawnPoint;
	Entry.FinalTransform = FinalTransform;
	Entry.StartLocation = StartLocation;
	Entry.EmergenceElapsed = 0.0f;
	Entry.bEmerging = EmergenceDepth > 0.0f && EmergenceDuration > 0.0f;
	if (!Entry.bEmerging)
	{
		SpawnedAnchor->SetActorTransform(FinalTransform);
		SetAnchorInteractionEnabled(*SpawnedAnchor, true);
	}

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Anchor emerging. Anchor=%s Point=%s Depth=%.1f Duration=%.2f"),
		*GetNameSafe(SpawnedAnchor),
		*GetNameSafe(&SpawnPoint),
		EmergenceDepth,
		EmergenceDuration);
	return true;
}

AActor* AAnchorSpawnManager::ChooseAvailableSpawnPoint(
	const AActor* ExcludedPoint)
{
	TArray<AActor*> AvailablePoints;
	for (const TWeakObjectPtr<AActor>& PointPtr : CandidateSpawnPoints)
	{
		AActor* Point = PointPtr.Get();
		if (!Point || Point == ExcludedPoint)
		{
			continue;
		}

		const bool bOccupied = ManagedAnchors.ContainsByPredicate(
			[Point](const FManagedAnchor& Entry)
			{
				return Entry.Anchor.IsValid() && Entry.SpawnPoint.Get() == Point;
			});
		if (!bOccupied)
		{
			AvailablePoints.Add(Point);
		}
	}

	if (AvailablePoints.IsEmpty() && ExcludedPoint)
	{
		const bool bExcludedPointOccupied = ManagedAnchors.ContainsByPredicate(
			[ExcludedPoint](const FManagedAnchor& Entry)
			{
				return Entry.Anchor.IsValid()
					&& Entry.SpawnPoint.Get() == ExcludedPoint;
			});
		if (!bExcludedPointOccupied)
		{
			return const_cast<AActor*>(ExcludedPoint);
		}
	}

	return AvailablePoints.IsEmpty()
		? nullptr
		: AvailablePoints[RuntimeRandomStream.RandRange(
			0,
			AvailablePoints.Num() - 1)];
}

void AAnchorSpawnManager::SetAnchorInteractionEnabled(
	AActor& Anchor,
	const bool bEnabled) const
{
	if (UAttachSurfaceComponent* Surface =
			Anchor.FindComponentByClass<UAttachSurfaceComponent>())
	{
		Surface->SetCollisionEnabled(
			bEnabled
				? ECollisionEnabled::QueryOnly
				: ECollisionEnabled::NoCollision);
	}
}
