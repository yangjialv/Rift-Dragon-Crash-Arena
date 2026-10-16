#include "Boss/AI/RDCABossAIController.h"
#include "Boss/AI/RDCABossBTNodes.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"

namespace
{
template<class T> T* Key(UBlackboardData* BB, FName Name)
{
 FBlackboardEntry Entry; Entry.EntryName = Name;
 auto* Type = NewObject<T>(BB); Entry.KeyType = Type; BB->Keys.Add(Entry); return Type;
}
void Child(UBTCompositeNode* Parent, UBTNode* Node, UBTDecorator* Condition = nullptr)
{
 FBTCompositeChild Entry; Entry.ChildComposite = Cast<UBTCompositeNode>(Node); Entry.ChildTask = Cast<UBTTaskNode>(Node);
 if (Condition) { Entry.Decorators.Add(Condition); }
 Parent->Children.Add(Entry);
}
URDCABTDecorator_BlackboardCondition* Bool(UObject* Outer, FName Name, bool Expected, EBTFlowAbortMode::Type Abort)
{
 auto* D = NewObject<URDCABTDecorator_BlackboardCondition>(Outer); D->Configure(Name, Expected, Abort); return D;
}
URDCABTDecorator_BlackboardCondition* Enum(UObject* Outer, FName Name, int32 Value)
{
 auto* D = NewObject<URDCABTDecorator_BlackboardCondition>(Outer); D->ConfigureEnum(Name, Value); return D;
}
}
UBehaviorTree* ARDCABossAIController::CreateDefaultCombatTree(UObject* Outer)
{
 auto* Tree = NewObject<UBehaviorTree>(Outer, TEXT("BT_RiftDragonCombat"));
 auto* BB = NewObject<UBlackboardData>(Tree, TEXT("BB_RiftDragonCombat"));
 using namespace RDCABossBlackboard;
 Key<UBlackboardKeyType_Object>(BB, TargetActor)->BaseClass = APawn::StaticClass();
 Key<UBlackboardKeyType_Enum>(BB, CombatPhase)->EnumType = StaticEnum<EBossCombatPhase>();
 Key<UBlackboardKeyType_Enum>(BB, SelectedAttack)->EnumType = StaticEnum<ERDCABossAttackFamily>();
 Key<UBlackboardKeyType_Enum>(BB, SelectedBarragePattern)->EnumType = StaticEnum<EBossBarragePattern>();
 Key<UBlackboardKeyType_Float>(BB, NextActionTime);
 for (FName Name : {IsHeld, IsStopped, IsWeakPointExposed, HasAttackPlan}) { Key<UBlackboardKeyType_Bool>(BB, Name); }
 Tree->BlackboardAsset = BB;
 auto* Root = NewObject<UBTComposite_Selector>(Tree); Root->NodeName = TEXT("Boss combat priorities"); Tree->RootNode = Root;
 Root->Services.Add(NewObject<URDCABTService_ValidateBossTarget>(Root));
 const auto WaitBranch = [Root](FName Name, bool Expected, ERDCABossWait Mode, EBTFlowAbortMode::Type Abort, const TCHAR* Label)
 {
  auto* Task = NewObject<URDCABTTask_BossWait>(Root); Task->WaitFor = Mode;
  Task->ObservedKey.SelectedKeyName = Name; Task->NodeName = Label;
  Child(Root, Task, Bool(Root, Name, Expected, Abort));
 };
 WaitBranch(IsStopped, true, ERDCABossWait::Shutdown, EBTFlowAbortMode::Both, TEXT("Battle ended / wait for shutdown"));
 WaitBranch(IsHeld, true, ERDCABossWait::Resume, EBTFlowAbortMode::Both, TEXT("Intro or transition / wait for resume"));
 WaitBranch(TargetActor, false, ERDCABossWait::Target, EBTFlowAbortMode::Both, TEXT("No target / await target"));
 WaitBranch(IsWeakPointExposed, true, ERDCABossWait::Exposure, EBTFlowAbortMode::None, TEXT("Stunned / await hit or timeout"));
 auto* Normal = NewObject<UBTComposite_Sequence>(Root); Normal->NodeName = TEXT("Choose and execute one attack"); Child(Root, Normal);
 auto* Ready = NewObject<URDCABTTask_BossWait>(Normal); Ready->WaitFor = ERDCABossWait::DecisionReady;
 Ready->ObservedKey.SelectedKeyName = NextActionTime; Ready->NodeName = TEXT("Wait until next action time"); Child(Normal, Ready);
 auto* Strategy = NewObject<UBTComposite_Selector>(Normal); Strategy->NodeName = TEXT("Select phase strategy"); Child(Normal, Strategy);
 for (auto Phase : {EBossCombatPhase::Phase2, EBossCombatPhase::Phase1})
 {
  auto* Task = NewObject<URDCABTTask_SelectBossAttack>(Strategy); Task->Phase = Phase;
  Task->NodeName = Phase == EBossCombatPhase::Phase2 ? TEXT("Phase 2: weighted unique plan") : TEXT("Phase 1: teaching plan");
  Child(Strategy, Task, Enum(Strategy, CombatPhase, static_cast<int32>(Phase)));
 }
 auto* Attacks = NewObject<UBTComposite_Selector>(Normal); Attacks->NodeName = TEXT("Execute selected GAS ability"); Child(Normal, Attacks);
 for (auto Type : {EBossAttackType::AimedVolley, EBossAttackType::Shockwave, EBossAttackType::SweepLaser})
 {
  auto* Task = NewObject<URDCABTTask_ExecuteBossAttack>(Attacks); Task->AbilityTag = AbilityTagFor(Type);
  Task->NodeName = Task->AbilityTag.ToString();
  Child(Attacks, Task, Enum(Attacks, SelectedAttack, static_cast<int32>(FamilyFor(Type))));
 }
 return Tree;
}
