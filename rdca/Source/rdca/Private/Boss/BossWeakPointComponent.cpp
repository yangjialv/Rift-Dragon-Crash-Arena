#include "Boss/BossWeakPointComponent.h"

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
		CurrentHitPoints = FMath::Max(
			CurrentHitPoints - FMath::Max(DamagePerQualifiedCrash, 1),
			0);
		OnHitPointsChanged.Broadcast(CurrentHitPoints, MaximumHitPoints);
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
