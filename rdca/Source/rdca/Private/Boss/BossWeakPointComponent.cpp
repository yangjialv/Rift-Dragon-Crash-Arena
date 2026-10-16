#include "Boss/BossWeakPointComponent.h"

#include "Abilities/RDCAAbilitySystemComponent.h"
#include "Abilities/RDCAHealthAttributeSet.h"
#include "Abilities/RDCAPresentation.h"
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
	ResetWeakPoint();
}

void UBossWeakPointComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AbilitySystem)
	{
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(URDCAHealthAttributeSet::GetHealthAttribute()).RemoveAll(this);
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(URDCAHealthAttributeSet::GetMaxHealthAttribute()).RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

void UBossWeakPointComponent::InitializeAbilitySystem()
{
	if (AbilitySystem)
	{
		return;
	}
	AbilitySystem = URDCAAbilitySystemComponent::FindOrCreateForActor(GetOwner());
	if (AbilitySystem)
	{
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(URDCAHealthAttributeSet::GetHealthAttribute())
			.AddUObject(this, &ThisClass::HandleHealthChanged);
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(URDCAHealthAttributeSet::GetMaxHealthAttribute())
			.AddUObject(this, &ThisClass::HandleHealthChanged);
	}
}

int32 UBossWeakPointComponent::GetCurrentHitPoints() const
{
	return AbilitySystem && AbilitySystem->IsHealthInitialized()
		? FMath::RoundToInt(AbilitySystem->GetHealth()) : FMath::Max(MaximumHitPoints, 1);
}

int32 UBossWeakPointComponent::GetMaximumHitPoints() const
{
	return AbilitySystem && AbilitySystem->IsHealthInitialized()
		? FMath::RoundToInt(AbilitySystem->GetMaxHealth()) : FMath::Max(MaximumHitPoints, 1);
}

void UBossWeakPointComponent::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	if (!bResettingHealth)
	{
		OnHitPointsChanged.Broadcast(GetCurrentHitPoints(), GetMaximumHitPoints());
	}
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
	InitializeAbilitySystem();
	if (!AbilitySystem)
	{
		return;
	}
	TGuardValue<bool> ResetGuard(bResettingHealth, true);
	AbilitySystem->ResetHealth(FMath::Max(MaximumHitPoints, 1));
	OnHitPointsChanged.Broadcast(GetCurrentHitPoints(), GetMaximumHitPoints());
}

bool UBossWeakPointComponent::ReceiveCrash(
	APawn* CrashingPawn,
	const FHitResult& Hit,
	const bool bIsQualifiedHeavyCrash)
{
	InitializeAbilitySystem();
	if (AbilitySystem && !AbilitySystem->IsHealthInitialized())
	{
		ResetWeakPoint();
	}
	const bool bWasEffective =
		bExposed
		&& bIsQualifiedHeavyCrash
		&& !IsBossDefeated()
		&& AbilitySystem
		&& AbilitySystem->ApplyDamage(FMath::Max(DamagePerQualifiedCrash, 1), CrashingPawn);

	if (bWasEffective)
	{
		const float ImpactVolume = FMath::Max(
			WeakPointImpactVolumeMultiplier, 0.0f);
		RDCAPresentation::PlayAtLocation(
			this,
			ERDCAAudioCue::WeakPointHit,
			Hit.ImpactPoint,
			0.95f * ImpactVolume);
		// A qualified weak-point crash needs to sit above an ordinary Anchor
		// fracture in the mix: glass/transient, low body impact, then Boss pain.
		RDCAPresentation::PlayAtLocation(
			this,
			ERDCAAudioCue::AnchorShatter,
			Hit.ImpactPoint,
			0.58f * ImpactVolume,
			0.88f);
		RDCAPresentation::PlayAtLocation(
			this,
			ERDCAAudioCue::BossBodyRebound,
			Hit.ImpactPoint,
			0.38f * ImpactVolume,
			0.72f);
		if (!IsBossDefeated())
		{
			RDCAPresentation::PlayAtLocation(
				this,
				ERDCAAudioCue::BossRoar,
				GetOwner()->GetActorLocation(),
				0.62f,
				0.9f);
		}
	}
	else
	{
		RDCAPresentation::PlayAtLocation(
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
		GetCurrentHitPoints(),
		GetMaximumHitPoints());
	return bWasEffective;
}
