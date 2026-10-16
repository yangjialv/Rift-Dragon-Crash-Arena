#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Boss/BossEncounterComponent.h"
#include "BossAttackPolicyComponent.generated.h"

/** Stateless policy; reads the one set of authored tuning and round facts. */
UCLASS()
class RDCA_API UBossAttackPolicyComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UBossAttackPolicyComponent();
	EBossAttackType ChooseWeightedAttack(UBossEncounterComponent& C, const FBossAttackWeights& Weights);
	void SelectPhase1ScheduledAttack(UBossEncounterComponent& C);
	void SelectPhase2RoundAttack(UBossEncounterComponent& C);
};
