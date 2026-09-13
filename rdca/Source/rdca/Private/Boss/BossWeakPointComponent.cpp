#include "Boss/BossWeakPointComponent.h"

#include "Audio/RDCAAudio.h"
#include "GameFramework/Pawn.h"
#include "rdca.h"

UBossWeakPointComponent::UBossWeakPointComponent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SphereRadius = 60.0f;
	BodyInstance.SetCollisionEnabled(ECollisionEnabled::QueryOnly, false);
	BodyInstance.SetObjectType(ECC_WorldDynamic);
	BodyInstance.SetResponseToAllChannels(ECR_Ignore);
	BodyInstance.SetResponseToChannel(ECC_Pawn, ECR_Block);
	BodyInstance.SetResponseToChannel(ECC_Visibility, ECR_Block);
}

void UBossWeakPointComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentHitPoints = FMath::Max(MaximumHitPoints, 1);
}

void UBossWeakPointComponent::SetExposed(const bool bNewExposed)
{
	if (bExposed == bNewExposed)
	{
		return;
	}

	bExposed = bNewExposed;
	OnExposureChanged.Broadcast(bExposed);
}

void UBossWeakPointComponent::ResetWeakPoint()
{
	CurrentHitPoints = FMath::Max(MaximumHitPoints, 1);
	OnHitPointsChanged.Broadcast(CurrentHitPoints, MaximumHitPoints);
}

bool UBossWeakPointComponent::ReceiveCrash(
	APawn* CrashingPawn,
	const FHitResult& Hit,
	const bool bIsQualifiedHeavyCrash)
{
	const bool bWasEffective =
		bExposed
		&& bIsQualifiedHeavyCrash
		&& !IsBossDefeated();

	if (bWasEffective)
	{
		const float ImpactVolume = FMath::Max(
			WeakPointImpactVolumeMultiplier, 0.0f);
		CurrentHitPoints = FMath::Max(
			CurrentHitPoints - FMath::Max(DamagePerQualifiedCrash, 1),
			0);
		OnHitPointsChanged.Broadcast(CurrentHitPoints, MaximumHitPoints);
		RDCAAudio::PlayAtLocation(
			this,
			ERDCAAudioCue::WeakPointHit,
			Hit.ImpactPoint,
			0.95f * ImpactVolume);
		// A qualified weak-point crash needs to sit above an ordinary Anchor
		// fracture in the mix: glass/transient, low body impact, then Boss pain.
		RDCAAudio::PlayAtLocation(
			this,
			ERDCAAudioCue::AnchorShatter,
			Hit.ImpactPoint,
			0.58f * ImpactVolume,
			0.88f);
		RDCAAudio::PlayAtLocation(
			this,
			ERDCAAudioCue::BossBodyRebound,
			Hit.ImpactPoint,
			0.38f * ImpactVolume,
			0.72f);
		if (!IsBossDefeated())
		{
			RDCAAudio::PlayAtLocation(
				this,
				ERDCAAudioCue::BossRoar,
				GetOwner()->GetActorLocation(),
				0.62f,
				0.9f);
		}
	}
	else
	{
		RDCAAudio::PlayAtLocation(
			this,
			ERDCAAudioCue::BossBodyRebound,
			Hit.ImpactPoint,
			0.55f);
	}

	OnWeakPointCrash.Broadcast(CrashingPawn, bWasEffective, Hit);

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss weak point crash. Boss=%s Effective=%s Exposed=%s Heavy=%s HP=%d/%d"),
		*GetNameSafe(GetOwner()),
		bWasEffective ? TEXT("true") : TEXT("false"),
		bExposed ? TEXT("true") : TEXT("false"),
		bIsQualifiedHeavyCrash ? TEXT("true") : TEXT("false"),
		CurrentHitPoints,
		MaximumHitPoints);
	return bWasEffective;
}
