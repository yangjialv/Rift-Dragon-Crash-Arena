#include "Arena/AnchorSpawnManager.h"

#include "Arena/ArenaFloorCollision.h"
#include "Arena/AttachSurfaceComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
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

		const bool bSpawnedReplacement = bUseArenaRandomSpawns
			? SpawnAnchorAtRandomLocation()
			: [&Pending, this]()
			{
				if (AActor* Point = ChooseAvailableSpawnPoint(
						Pending.PreviousSpawnPoint.Get()))
				{
					return SpawnAnchorAtPoint(*Point);
				}
				return false;
			}();
		if (bSpawnedReplacement)
		{
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

	if (bUseArenaRandomSpawns)
	{
		if (!ResolveArenaFloorCollision())
		{
			UE_LOG(
				LogRDCAPlayer,
				Warning,
				TEXT("Anchor arena-random spawn skipped. Manager=%s Arena Floor Collision is not assigned."),
				*GetNameSafe(this));
			return;
		}
		for (int32 Index = 0; Index < NumberOfAnchors; ++Index)
		{
			SpawnAnchorAtRandomLocation();
		}
	}
	else
	{
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
	return SpawnAnchorAtTransform(SpawnPoint.GetActorTransform(), &SpawnPoint);
}

bool AAnchorSpawnManager::SpawnAnchorAtTransform(
	const FTransform& FinalTransform,
	AActor* SpawnPoint)
{
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

	// Arena-random Anchors inherit the floor transform. The floor is therefore
	// both the source of valid spawn coordinates and the spatial parent should a
	// future arena move, rotate, or phase-shift as one piece.
	if (bUseArenaRandomSpawns)
	{
		if (AArenaFloorCollision* FloorCollision = ResolveArenaFloorCollision())
		{
			SpawnedAnchor->AttachToActor(
				FloorCollision,
				FAttachmentTransformRules::KeepWorldTransform);
		}
	}

	SetAnchorInteractionEnabled(*SpawnedAnchor, false);
	SpawnedAnchors.Add(SpawnedAnchor);
	FManagedAnchor& Entry = ManagedAnchors.AddDefaulted_GetRef();
	Entry.Anchor = SpawnedAnchor;
	Entry.SpawnPoint = SpawnPoint;
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
		*GetNameSafe(SpawnPoint),
		EmergenceDepth,
		EmergenceDuration);
	return true;
}

bool AAnchorSpawnManager::SpawnAnchorAtRandomLocation()
{
	AArenaFloorCollision* FloorCollision = ResolveArenaFloorCollision();
	if (!FloorCollision)
	{
		return false;
	}

	const bool bRequirePlayerReachability = bGuaranteeReachableAnchor
		&& !HasReachableActiveAnchor();
	FVector CandidateLocation;
	if (!ChooseBalancedArenaAnchorLocation(
			bRequirePlayerReachability,
			CandidateLocation)
		// A tight reachable radius must never stop the encounter. If no valid
		// position exists within it, retain balanced arena coverage instead.
		&& (!bRequirePlayerReachability
			|| !ChooseBalancedArenaAnchorLocation(false, CandidateLocation)))
	{
		UE_LOG(
			LogRDCAPlayer,
			Warning,
			TEXT("Anchor balanced arena spawn failed after %d candidates. Manager=%s"),
			ArenaRandomAttempts,
			*GetNameSafe(this));
		return false;
	}

	CandidateLocation.Z += ArenaSpawnHeightOffset;
	return SpawnAnchorAtTransform(
		FTransform(GetActorQuat(), CandidateLocation),
		nullptr);
}

bool AAnchorSpawnManager::ChooseBalancedArenaAnchorLocation(
	const bool bRequirePlayerReachability,
	FVector& OutLocation)
{
	AArenaFloorCollision* FloorCollision = ResolveArenaFloorCollision();
	if (!FloorCollision)
	{
		return false;
	}

	const APawn* PlayerPawn = bRequirePlayerReachability
		? UGameplayStatics::GetPlayerPawn(this, 0)
		: nullptr;
	const float ReachableDistanceSquared = FMath::Square(GuaranteedAnchorDistance);
	const float Randomness = FMath::Clamp(CoverageRandomness, 0.0f, 0.5f);
	float BestCoverageScore = -1.0f;
	TArray<FVector> BestCandidates;

	for (int32 Attempt = 0; Attempt < FMath::Max(ArenaRandomAttempts, 1); ++Attempt)
	{
		FVector CandidateLocation;
		if (!FloorCollision->GetRandomAnchorLocation(
				RuntimeRandomStream,
				ArenaInnerClearance,
				ArenaOuterClearance,
				CandidateLocation)
			|| !IsAnchorLocationClear(CandidateLocation))
		{
			continue;
		}

		if (PlayerPawn
			&& FVector::DistSquared2D(
				CandidateLocation,
				PlayerPawn->GetActorLocation()) > ReachableDistanceSquared)
		{
			continue;
		}

		const float CoverageScore = GetClosestAnchorDistanceSquared(CandidateLocation);
		if (CoverageScore > BestCoverageScore)
		{
			BestCoverageScore = CoverageScore;
			BestCandidates.Reset();
			BestCandidates.Add(CandidateLocation);
		}
		else if (CoverageScore >= BestCoverageScore * (1.0f - Randomness))
		{
			BestCandidates.Add(CandidateLocation);
		}
	}

	if (BestCandidates.IsEmpty())
	{
		return false;
	}

	OutLocation = BestCandidates[RuntimeRandomStream.RandRange(0, BestCandidates.Num() - 1)];
	return true;
}

AArenaFloorCollision* AAnchorSpawnManager::ResolveArenaFloorCollision()
{
	if (ArenaFloorCollision)
	{
		return ArenaFloorCollision;
	}

	if (!GetWorld())
	{
		return nullptr;
	}

	for (TActorIterator<AArenaFloorCollision> It(GetWorld()); It; ++It)
	{
		ArenaFloorCollision = *It;
		return ArenaFloorCollision;
	}
	return nullptr;
}

bool AAnchorSpawnManager::IsAnchorLocationClear(
	const FVector& CandidateLocation) const
{
	const float MinimumSpacingSquared = FMath::Square(MinimumAnchorSpacing);
	return GetClosestAnchorDistanceSquared(CandidateLocation)
		>= MinimumSpacingSquared;
}

float AAnchorSpawnManager::GetClosestAnchorDistanceSquared(
	const FVector& CandidateLocation) const
{
	float ClosestDistanceSquared = TNumericLimits<float>::Max();
	for (const FManagedAnchor& Entry : ManagedAnchors)
	{
		if (Entry.Anchor.IsValid())
		{
			ClosestDistanceSquared = FMath::Min(
				ClosestDistanceSquared,
				FVector::DistSquared2D(
					Entry.FinalTransform.GetLocation(),
					CandidateLocation));
		}
	}
	return ClosestDistanceSquared;
}

bool AAnchorSpawnManager::HasReachableActiveAnchor() const
{
	if (!bGuaranteeReachableAnchor)
	{
		return true;
	}

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn)
	{
		return false;
	}

	const float ReachableDistanceSquared = FMath::Square(GuaranteedAnchorDistance);
	return ManagedAnchors.ContainsByPredicate(
		[PlayerPawn, ReachableDistanceSquared](const FManagedAnchor& Entry)
		{
			return Entry.Anchor.IsValid()
				&& FVector::DistSquared2D(
					Entry.FinalTransform.GetLocation(),
					PlayerPawn->GetActorLocation()) <= ReachableDistanceSquared;
		});
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
