#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Boss/BossEncounterComponent.h"
#include "GameplayTagContainer.h"
#include "RDCABossBTNodes.generated.h"

UENUM()
enum class ERDCABossWait : uint8 { Shutdown, Resume, Target, Exposure, DecisionReady };

UCLASS(meta = (DisplayName = "RDCA Blackboard Condition"))
class RDCA_API URDCABTDecorator_BlackboardCondition : public UBTDecorator_Blackboard
{
 GENERATED_BODY()
public:
 void Configure(FName Key, bool bExpected, EBTFlowAbortMode::Type AbortMode);
 void ConfigureEnum(FName Key, int32 Value);
};

UCLASS(meta = (DisplayName = "RDCA Wait For Blackboard / Deadline"))
class RDCA_API URDCABTTask_BossWait : public UBTTaskNode
{
 GENERATED_BODY()
public:
 URDCABTTask_BossWait();
 UPROPERTY(EditAnywhere, Category = "Boss") ERDCABossWait WaitFor = ERDCABossWait::DecisionReady;
 UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector ObservedKey;
 virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
protected:
 virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
 virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
 virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type Result) override;
private:
 bool IsReady() const;
 void Wake();
 void Cleanup();
 EBlackboardNotificationResult OnKey(const UBlackboardComponent& BB, FBlackboard::FKey Key);
 TWeakObjectPtr<UBehaviorTreeComponent> Tree;
 TWeakObjectPtr<UBossEncounterComponent> Encounter;
 FDelegateHandle WindowHandle;
 FTimerHandle Timer;
};

UCLASS(meta = (DisplayName = "RDCA Select Boss Attack"))
class RDCA_API URDCABTTask_SelectBossAttack : public UBTTaskNode
{
 GENERATED_BODY()
public:
 URDCABTTask_SelectBossAttack();
 UPROPERTY(EditAnywhere, Category = "Boss") EBossCombatPhase Phase = EBossCombatPhase::Phase1;
 UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector TargetKey;
 virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
protected:
 virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};

UCLASS(meta = (DisplayName = "RDCA Activate And Await GAS Attack"))
class RDCA_API URDCABTTask_ExecuteBossAttack : public UBTTaskNode
{
 GENERATED_BODY()
public:
 URDCABTTask_ExecuteBossAttack();
 UPROPERTY(EditAnywhere, Category = "Boss") FGameplayTag AbilityTag;
 UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector TargetKey;
 virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
protected:
 virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
 virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
 virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type Result) override;
private:
 void Finished(uint64 Id, bool bCancelled);
 void Cleanup();
 TWeakObjectPtr<UBehaviorTreeComponent> Tree;
 TWeakObjectPtr<UBossEncounterComponent> Encounter;
 FDelegateHandle EndHandle;
 uint64 ExecutionId = 0;
 bool bActivating = false;
 bool bCompleted = false;
 bool bCancelledResult = false;
};

UCLASS(meta = (DisplayName = "RDCA Validate Combat Target"))
class RDCA_API URDCABTService_ValidateBossTarget : public UBTService
{
 GENERATED_BODY()
public:
 URDCABTService_ValidateBossTarget();
protected:
 virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
