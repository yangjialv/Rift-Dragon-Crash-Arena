#include "Boss/AI/BossAttackPolicyComponent.h"
#include "Boss/BossWeakPointComponent.h"
#include "Boss/BossFanProjectile.h"
#include "Boss/BossSweepLaser.h"
#include "rdca.h"

UBossAttackPolicyComponent::UBossAttackPolicyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

EBossAttackType UBossAttackPolicyComponent::ChooseWeightedAttack(UBossEncounterComponent& C, const FBossAttackWeights& Weights)
{
	if (C.bDebugForceSweepLaser && C.SweepLaserClass)
	{
		return EBossAttackType::SweepLaser;
	}

	struct FWeightedCandidate
	{
		EBossAttackType Attack = EBossAttackType::None;
		float Weight = 0.0f;
	};

	TArray<FWeightedCandidate> Candidates;
	Candidates.Add({EBossAttackType::Shockwave, Weights.Shockwave});
	if (C.FanProjectileClass)
	{
		Candidates.Add({EBossAttackType::AimedVolley, Weights.AimedVolley});
	}
	if (C.SweepLaserClass)
	{
		Candidates.Add({EBossAttackType::SweepLaser, Weights.SweepLaser});
	}

	float TotalWithoutRepeat = 0.0f;
	for (const FWeightedCandidate& Candidate : Candidates)
	{
		if (Candidate.Attack != C.PreviousAttack)
		{
			TotalWithoutRepeat += FMath::Max(Candidate.Weight, 0.0f);
		}
	}
	const bool bCanAvoidRepeat = TotalWithoutRepeat > 0.0f;
	float TotalWeight = 0.0f;
	for (const FWeightedCandidate& Candidate : Candidates)
	{
		if (!bCanAvoidRepeat || Candidate.Attack != C.PreviousAttack)
		{
			TotalWeight += FMath::Max(Candidate.Weight, 0.0f);
		}
	}
	if (TotalWeight <= UE_KINDA_SMALL_NUMBER)
	{
		return EBossAttackType::Shockwave;
	}

	float Roll = C.AttackRandomStream.FRandRange(0.0f, TotalWeight);
	for (const FWeightedCandidate& Candidate : Candidates)
	{
		if (bCanAvoidRepeat && Candidate.Attack == C.PreviousAttack)
		{
			continue;
		}
		Roll -= FMath::Max(Candidate.Weight, 0.0f);
		if (Roll <= 0.0f)
		{
			return Candidate.Attack;
		}
	}
	return Candidates.Last().Attack;
}

void UBossAttackPolicyComponent::SelectPhase1ScheduledAttack(UBossEncounterComponent& C)
{
	const int32 DamageTaken = C.WeakPoint.IsValid()
		? FMath::Max(
			C.WeakPoint->GetMaximumHitPoints()
				- C.WeakPoint->GetCurrentHitPoints(),
			0)
		: 0;
	const bool bFirstTeachingRound = DamageTaken == 0;
	const int32 Step = FMath::Clamp(C.CompletedAttacksSinceExposure, 0, 1);

	if (Step == 0 && C.FanProjectileClass)
	{
		if (bFirstTeachingRound)
		{
			C.SetBarragePattern(EBossBarragePattern::LegacyAimedVolley);
		}
		else
		{
			// Repeating a missed weak-point round swaps between the two
			// remaining Phase 1 lessons instead of replaying the same pattern.
			EBossBarragePattern Pattern;
			if (C.bHasPreviousBarragePattern
				&& C.PreviousBarragePattern == EBossBarragePattern::PredictiveVolley)
			{
				Pattern = EBossBarragePattern::GapWall;
			}
			else if (C.bHasPreviousBarragePattern
				&& C.PreviousBarragePattern == EBossBarragePattern::GapWall)
			{
				Pattern = EBossBarragePattern::PredictiveVolley;
			}
			else
			{
				Pattern = C.AttackRandomStream.FRand() < 0.5f
					? EBossBarragePattern::PredictiveVolley
					: EBossBarragePattern::GapWall;
			}
			C.SetBarragePattern(Pattern);
		}
		return;
	}

	if (Step == 1)
	{
		if (bFirstTeachingRound || !C.SweepLaserClass)
		{
			C.CurrentAttack = EBossAttackType::Shockwave;
		}
		else
		{
			C.CurrentAttack = EBossAttackType::SweepLaser;
		}
		return;
	}

	const FBossAttackWeights& Weights =
		C.GetWeightsForPlayerState(C.LastObservedPlayerState);
	C.CurrentAttack = C.ChooseWeightedAttack(Weights);
	if (C.CurrentAttack == EBossAttackType::AimedVolley)
	{
		C.SetBarragePattern(EBossBarragePattern::PredictiveVolley);
	}
}

void UBossAttackPolicyComponent::SelectPhase2RoundAttack(UBossEncounterComponent& C)
{
	if (C.CompletedAttacksSinceExposure == 0)
	{
		C.ResetPhase2Round();
	}

	// Selection is deliberately hierarchical. The whole barrage family owns one
	// category weight; its five patterns do not each compete with Shockwave and
	// Laser. Exact attacks are then removed until the current three-attack round
	// ends, so Shockwave/Laser cannot repeat and barrage patterns cannot repeat.
	const EBossBarragePattern Phase2Patterns[] = {
		EBossBarragePattern::CurvedVolley,
		EBossBarragePattern::HomingVolley,
		EBossBarragePattern::RotatingGapWall,
		EBossBarragePattern::DoubleSpiral,
		EBossBarragePattern::LegacyDenseFan};
	TArray<EBossBarragePattern> AvailableBarragePatterns;
	if (C.FanProjectileClass)
	{
		for (const EBossBarragePattern Pattern : Phase2Patterns)
		{
			const int32 PatternKey = 100 + static_cast<int32>(Pattern);
			if (!C.Phase2UsedAttackKeys.Contains(PatternKey))
			{
				AvailableBarragePatterns.Add(Pattern);
			}
		}
	}

	const bool bBarrageAvailable = !AvailableBarragePatterns.IsEmpty();
	const bool bShockwaveAvailable = !C.Phase2UsedAttackKeys.Contains(0);
	const bool bLaserAvailable = C.SweepLaserClass
		&& !C.Phase2UsedAttackKeys.Contains(1);
	if (!bBarrageAvailable && !bShockwaveAvailable && !bLaserAvailable)
	{
		// A designer may request a round longer than the currently configured
		// pool. Start a fresh pool instead of producing an invalid selection.
		C.ResetPhase2Round();
		SelectPhase2RoundAttack(C);
		return;
	}
	float BarrageWeight = bBarrageAvailable
		? FMath::Max(C.Phase2BarrageWeight, 0.0f)
		: 0.0f;
	float ShockwaveWeight = bShockwaveAvailable
		? FMath::Max(C.Phase2ShockwaveWeight, 0.0f)
		: 0.0f;
	float LaserWeight = bLaserAvailable
		? FMath::Max(C.Phase2LaserWeight, 0.0f)
		: 0.0f;
	float TotalWeight = BarrageWeight + ShockwaveWeight + LaserWeight;
	if (TotalWeight <= UE_KINDA_SMALL_NUMBER)
	{
		// All-zero editor settings remain playable: choose uniformly between the
		// categories that still contain an unused attack.
		BarrageWeight = bBarrageAvailable ? 1.0f : 0.0f;
		ShockwaveWeight = bShockwaveAvailable ? 1.0f : 0.0f;
		LaserWeight = bLaserAvailable ? 1.0f : 0.0f;
		TotalWeight = BarrageWeight + ShockwaveWeight + LaserWeight;
	}

	int32 SelectedKey = 0;
	FString SelectedCategory = TEXT("Shockwave");
	float Roll = C.AttackRandomStream.FRandRange(0.0f, TotalWeight);
	if (bBarrageAvailable
		&& (Roll < BarrageWeight
			|| (!bShockwaveAvailable && !bLaserAvailable)))
	{
		const EBossBarragePattern SelectedPattern = AvailableBarragePatterns[
			C.AttackRandomStream.RandRange(0, AvailableBarragePatterns.Num() - 1)];
		SelectedKey = 100 + static_cast<int32>(SelectedPattern);
		SelectedCategory = TEXT("Barrage");
		C.SetBarragePattern(SelectedPattern);
	}
	else
	{
		Roll -= BarrageWeight;
		if (bShockwaveAvailable
			&& (Roll < ShockwaveWeight || !bLaserAvailable))
		{
			SelectedKey = 0;
			SelectedCategory = TEXT("Shockwave");
			C.CurrentAttack = EBossAttackType::Shockwave;
		}
		else if (bLaserAvailable)
		{
			SelectedKey = 1;
			SelectedCategory = TEXT("Laser");
			C.CurrentAttack = EBossAttackType::SweepLaser;
		}
		else
		{
			// Numerical endpoint fallback; the barrage category is the only one
			// that can reach this branch.
			const EBossBarragePattern SelectedPattern =
				AvailableBarragePatterns.Last();
			SelectedKey = 100 + static_cast<int32>(SelectedPattern);
			SelectedCategory = TEXT("Barrage");
			C.SetBarragePattern(SelectedPattern);
		}
	}
	C.Phase2UsedAttackKeys.Add(SelectedKey);
	C.bCurrentPhase2DoubleShockwave = SelectedKey == 0;

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss Phase 2 round attack selected. Boss=%s Step=%d/%d Category=%s Key=%d Attack=%d Pattern=%d AvailableWeights=(Barrage=%.2f Shockwave=%.2f Laser=%.2f) Used=%d Target=%s Seed=%d"),
		*GetNameSafe(C.GetOwner()),
		C.CompletedAttacksSinceExposure + 1,
		FMath::Max(C.Phase2AttacksBeforeStun, 1),
		*SelectedCategory,
		SelectedKey,
		static_cast<int32>(C.CurrentAttack),
		static_cast<int32>(C.ActiveBarragePattern),
		BarrageWeight,
		ShockwaveWeight,
		LaserWeight,
		C.Phase2UsedAttackKeys.Num(),
		*C.LockedTargetLocation.ToCompactString(),
		C.ActiveAttackRandomSeed);
}
