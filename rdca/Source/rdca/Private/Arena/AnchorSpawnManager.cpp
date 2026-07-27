#include "Arena/AnchorSpawnManager.h"

#include "Kismet/GameplayStatics.h"
#include "rdca.h"

AAnchorSpawnManager::AAnchorSpawnManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AAnchorSpawnManager::BeginPlay()
{
	Super::BeginPlay();

	if (bSpawnOnBeginPlay)
	{
		SpawnAnchors();
	}
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

	TArray<AActor*> CandidatePoints;
	UGameplayStatics::GetAllActorsWithTag(
		GetWorld(),
		SpawnPointTag,
		CandidatePoints);
	CandidatePoints.RemoveAll(
		[this](const AActor* Candidate)
		{
			return !Candidate || Candidate == this;
		});

	if (CandidatePoints.IsEmpty())
	{
		UE_LOG(
			LogRDCAPlayer,
			Warning,
			TEXT("Anchor spawn skipped. Manager=%s No actors found with tag '%s'."),
			*GetNameSafe(this),
			*SpawnPointTag.ToString());
		return;
	}

	FRandomStream RandomStream(
		RandomSeed >= 0
			? RandomSeed
			: FMath::Rand());
	for (int32 Index = CandidatePoints.Num() - 1; Index > 0; --Index)
	{
		const int32 SwapIndex = RandomStream.RandRange(0, Index);
		CandidatePoints.Swap(Index, SwapIndex);
	}

	const int32 SpawnCount = FMath::Min(
		FMath::Max(NumberOfAnchors, 1),
		CandidatePoints.Num());
	for (int32 Index = 0; Index < SpawnCount; ++Index)
	{
		const AActor* SpawnPoint = CandidatePoints[Index];
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* SpawnedAnchor = GetWorld()->SpawnActor<AActor>(
			AnchorClass,
			SpawnPoint->GetActorTransform(),
			SpawnParameters);
		if (SpawnedAnchor)
		{
			SpawnedAnchors.Add(SpawnedAnchor);
			UE_LOG(
				LogRDCAPlayer,
				Log,
				TEXT("Anchor spawned. Anchor=%s Point=%s Location=%s"),
				*GetNameSafe(SpawnedAnchor),
				*GetNameSafe(SpawnPoint),
				*SpawnPoint->GetActorLocation().ToCompactString());
		}
	}

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Anchor layout generated. Manager=%s Spawned=%d Candidates=%d Seed=%d"),
		*GetNameSafe(this),
		GetSpawnedAnchorCount(),
		CandidatePoints.Num(),
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
