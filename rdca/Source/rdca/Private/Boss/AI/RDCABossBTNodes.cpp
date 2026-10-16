#include "Boss/AI/RDCABossBTNodes.h"
#include "Boss/AI/RDCABossAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "TimerManager.h"

void URDCABTDecorator_BlackboardCondition::Configure(FName Key, bool Expected, EBTFlowAbortMode::Type AbortMode)
{
 BlackboardKey.SelectedKeyName = Key;
 OperationType = static_cast<uint8>(EBasicKeyOperation::Set);
 SetIsInversed(!Expected); FlowAbortMode = AbortMode;
#if WITH_EDITORONLY_DATA
 BasicOperation = EBasicKeyOperation::Set;
#endif
}
void URDCABTDecorator_BlackboardCondition::ConfigureEnum(FName Key, int32 Value)
{
 BlackboardKey.SelectedKeyName = Key; IntValue = Value;
 OperationType = static_cast<uint8>(EArithmeticKeyOperation::Equal); FlowAbortMode = EBTFlowAbortMode::None;
#if WITH_EDITORONLY_DATA
 ArithmeticOperation = EArithmeticKeyOperation::Equal;
#endif
}

URDCABTTask_BossWait::URDCABTTask_BossWait()
{
 NodeName = TEXT("Wait for blackboard condition / game deadline");
 bCreateNodeInstance = true; bNotifyTaskFinished = true;
}
void URDCABTTask_BossWait::InitializeFromAsset(UBehaviorTree& Asset)
{
 Super::InitializeFromAsset(Asset);
 if (Asset.BlackboardAsset) { ObservedKey.ResolveSelectedKey(*Asset.BlackboardAsset); }
}
bool URDCABTTask_BossWait::IsReady() const
{
 const auto* BT = Tree.Get();
 const auto* BB = BT ? BT->GetBlackboardComponent() : nullptr;
 if (!BB) { return false; }
 const FName Key = ObservedKey.SelectedKeyName;
 switch (WaitFor)
 {
 case ERDCABossWait::Resume:
 case ERDCABossWait::Exposure: return !BB->GetValueAsBool(Key);
 case ERDCABossWait::Target: return IsValid(BB->GetValueAsObject(Key));
 case ERDCABossWait::DecisionReady: return BB->GetValueAsFloat(Key) <= BT->GetWorld()->GetTimeSeconds() + KINDA_SMALL_NUMBER;
 default: return false;
 }
}
EBTNodeResult::Type URDCABTTask_BossWait::ExecuteTask(UBehaviorTreeComponent& BT, uint8* Memory)
{
 Cleanup(); Tree = &BT;
 if (IsReady()) { return EBTNodeResult::Succeeded; }
 auto* BB = BT.GetBlackboardComponent();
 if (!BB) { return EBTNodeResult::Failed; }
 BB->RegisterObserver(ObservedKey.GetSelectedKeyID(), this, FOnBlackboardChangeNotification::CreateUObject(this, &ThisClass::OnKey));
 if (WaitFor == ERDCABossWait::DecisionReady)
 {
  if (auto* AI = Cast<ARDCABossAIController>(BT.GetAIOwner()))
  {
   Encounter = AI->GetBossEncounter();
   if (Encounter.IsValid()) { WindowHandle = Encounter->OnDecisionWindowReady.AddUObject(this, &ThisClass::Wake); }
  }
  const float Remaining = BB->GetValueAsFloat(ObservedKey.SelectedKeyName) - BT.GetWorld()->GetTimeSeconds();
  BT.GetWorld()->GetTimerManager().SetTimer(Timer, this, &ThisClass::Wake, FMath::Max(Remaining, 0.001f), false);
 }
 return EBTNodeResult::InProgress;
}
void URDCABTTask_BossWait::Wake()
{
 if (Tree.IsValid() && IsReady()) { FinishLatentTask(*Tree.Get(), EBTNodeResult::Succeeded); }
}
EBlackboardNotificationResult URDCABTTask_BossWait::OnKey(const UBlackboardComponent& BB, FBlackboard::FKey Key)
{
 if (WaitFor == ERDCABossWait::DecisionReady && Tree.IsValid() && !IsReady())
 {
  const float Remaining = BB.GetValueAsFloat(ObservedKey.SelectedKeyName) - Tree->GetWorld()->GetTimeSeconds();
  Tree->GetWorld()->GetTimerManager().SetTimer(Timer, this, &ThisClass::Wake, FMath::Max(Remaining, 0.001f), false);
 }
 Wake(); return EBlackboardNotificationResult::ContinueObserving;
}
void URDCABTTask_BossWait::Cleanup()
{
 if (Tree.IsValid())
 {
  if (auto* BB = Tree->GetBlackboardComponent()) { BB->UnregisterObserversFrom(this); }
  Tree->GetWorld()->GetTimerManager().ClearTimer(Timer);
 }
 if (Encounter.IsValid()) { Encounter->OnDecisionWindowReady.Remove(WindowHandle); }
 WindowHandle.Reset(); Encounter.Reset(); Tree.Reset();
}
EBTNodeResult::Type URDCABTTask_BossWait::AbortTask(UBehaviorTreeComponent& BT, uint8* Memory)
{ Cleanup(); return EBTNodeResult::Aborted; }
void URDCABTTask_BossWait::OnTaskFinished(UBehaviorTreeComponent& BT, uint8* Memory, EBTNodeResult::Type Result)
{ Cleanup(); Super::OnTaskFinished(BT, Memory, Result); }

URDCABTTask_SelectBossAttack::URDCABTTask_SelectBossAttack()
{
 NodeName = TEXT("Select phase attack plan"); TargetKey.SelectedKeyName = RDCABossBlackboard::TargetActor;
 TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, TargetKey), APawn::StaticClass());
}
void URDCABTTask_SelectBossAttack::InitializeFromAsset(UBehaviorTree& Asset)
{
 Super::InitializeFromAsset(Asset);
 if (Asset.BlackboardAsset) { TargetKey.ResolveSelectedKey(*Asset.BlackboardAsset); }
}
EBTNodeResult::Type URDCABTTask_SelectBossAttack::ExecuteTask(UBehaviorTreeComponent& BT, uint8* Memory)
{
 auto* AI = Cast<ARDCABossAIController>(BT.GetAIOwner());
 auto* C = AI ? AI->GetBossEncounter() : nullptr;
 auto* BB = BT.GetBlackboardComponent();
 auto* Target = BB ? Cast<APawn>(BB->GetValueAsObject(TargetKey.SelectedKeyName)) : nullptr;
 if (!C || !IsValid(Target)) { return EBTNodeResult::Failed; }
 C->SetAITarget(AI, Target);
 const bool Selected = C->SelectAttackForAI(AI, Phase);
 AI->RefreshBlackboard();
 return Selected ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}

URDCABTTask_ExecuteBossAttack::URDCABTTask_ExecuteBossAttack()
{
 NodeName = TEXT("Activate GAS attack and await its result");
 bCreateNodeInstance = true; bNotifyTaskFinished = true;
 TargetKey.SelectedKeyName = RDCABossBlackboard::TargetActor;
 TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, TargetKey), APawn::StaticClass());
}
void URDCABTTask_ExecuteBossAttack::InitializeFromAsset(UBehaviorTree& Asset)
{
 Super::InitializeFromAsset(Asset);
 if (Asset.BlackboardAsset) { TargetKey.ResolveSelectedKey(*Asset.BlackboardAsset); }
}
EBTNodeResult::Type URDCABTTask_ExecuteBossAttack::ExecuteTask(UBehaviorTreeComponent& BT, uint8* Memory)
{
 Cleanup(); Tree = &BT;
 auto* AI = Cast<ARDCABossAIController>(BT.GetAIOwner()); Encounter = AI ? AI->GetBossEncounter() : nullptr;
 auto* BB = BT.GetBlackboardComponent();
 auto* Target = BB ? Cast<APawn>(BB->GetValueAsObject(TargetKey.SelectedKeyName)) : nullptr;
 if (!Encounter.IsValid() || !Target || !BB->GetValueAsBool(RDCABossBlackboard::HasAttackPlan)
     || AbilityTag != ARDCABossAIController::AbilityTagFor(Encounter->GetCurrentAttack())) { return EBTNodeResult::Failed; }
 Encounter->SetAITarget(AI, Target);
 ExecutionId = Encounter->GetAttackExecutionId() + 1;
 EndHandle = Encounter->OnAttackFinished.AddUObject(this, &ThisClass::Finished);
 bActivating = true;
 const uint64 Started = Encounter->ExecuteAttackForAI(AI);
 bActivating = false;
 if (bCompleted) { return bCancelledResult ? EBTNodeResult::Failed : EBTNodeResult::Succeeded; }
 if (!Started) { Cleanup(); return EBTNodeResult::Failed; }
 return EBTNodeResult::InProgress;
}
void URDCABTTask_ExecuteBossAttack::Finished(uint64 Id, bool Cancelled)
{
 if (Id != ExecutionId || bCompleted) { return; }
 bCompleted = true; bCancelledResult = Cancelled;
 if (!bActivating && Tree.IsValid()) { FinishLatentTask(*Tree.Get(), Cancelled ? EBTNodeResult::Failed : EBTNodeResult::Succeeded); }
}
void URDCABTTask_ExecuteBossAttack::Cleanup()
{
 if (Encounter.IsValid()) { Encounter->OnAttackFinished.Remove(EndHandle); }
 EndHandle.Reset(); Encounter.Reset(); Tree.Reset(); ExecutionId = 0; bCompleted = false; bActivating = false;
}
EBTNodeResult::Type URDCABTTask_ExecuteBossAttack::AbortTask(UBehaviorTreeComponent& BT, uint8* Memory)
{
 auto* AI = Cast<ARDCABossAIController>(BT.GetAIOwner());
 auto* C = Encounter.Get(); const uint64 Id = ExecutionId;
 Cleanup();
 if (C) { C->CancelAttackForAI(AI, Id); }
 return EBTNodeResult::Aborted;
}
void URDCABTTask_ExecuteBossAttack::OnTaskFinished(UBehaviorTreeComponent& BT, uint8* Memory, EBTNodeResult::Type Result)
{ Cleanup(); Super::OnTaskFinished(BT, Memory, Result); }

URDCABTService_ValidateBossTarget::URDCABTService_ValidateBossTarget()
{
 NodeName = TEXT("Validate target (event fallback)");
 Interval = 0.2f; RandomDeviation = 0.0f; bNotifyTick = true;
}
void URDCABTService_ValidateBossTarget::TickNode(UBehaviorTreeComponent& BT, uint8* Memory, float DeltaSeconds)
{
 Super::TickNode(BT, Memory, DeltaSeconds);
 if (auto* AI = Cast<ARDCABossAIController>(BT.GetAIOwner())) { AI->ValidateCombatTarget(); }
}
