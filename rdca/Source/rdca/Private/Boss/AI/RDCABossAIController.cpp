#include "Boss/AI/RDCABossAIController.h"
#include "Boss/RDCABossPawn.h"
#include "Abilities/RDCAGameplayTags.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Player/PlayerHealthComponent.h"
#include "TimerManager.h"
#include "rdca.h"

namespace
{
bool ValidSchema(UBlackboardData* Data)
{
 if (!Data) { return false; }
 using namespace RDCABossBlackboard;
 for (FName Key : {IsHeld, IsStopped, IsWeakPointExposed, HasAttackPlan})
 {
  auto Id = Data->GetKeyID(Key);
  if (Data->GetKeyType(Id) != UBlackboardKeyType_Bool::StaticClass() || Data->IsKeyInstanceSynced(Id)) { return false; }
 }
 const TPair<FName, UEnum*> Enums[] = {
  {CombatPhase, StaticEnum<EBossCombatPhase>()},
  {SelectedAttack, StaticEnum<ERDCABossAttackFamily>()},
  {SelectedBarragePattern, StaticEnum<EBossBarragePattern>()}
 };
 for (const auto& Pair : Enums)
 {
  auto Id = Data->GetKeyID(Pair.Key);
  if (Data->GetKeyType(Id) != UBlackboardKeyType_Enum::StaticClass() || Data->IsKeyInstanceSynced(Id)) { return false; }
  const auto* Type = Cast<UBlackboardKeyType_Enum>(Data->GetKey(Id)->KeyType);
  if (!Type || Type->EnumType != Pair.Value) { return false; }
 }
 const auto TargetId = Data->GetKeyID(TargetActor);
 const auto TimeId = Data->GetKeyID(NextActionTime);
 if (Data->GetKeyType(TargetId) != UBlackboardKeyType_Object::StaticClass()
     || Data->GetKeyType(TimeId) != UBlackboardKeyType_Float::StaticClass()
     || Data->IsKeyInstanceSynced(TargetId) || Data->IsKeyInstanceSynced(TimeId)) { return false; }
 const auto* TargetType = Cast<UBlackboardKeyType_Object>(Data->GetKey(TargetId)->KeyType);
 return TargetType && TargetType->BaseClass == APawn::StaticClass();
}
bool LiveTarget(APawn* Target)
{
 if (!IsValid(Target)) { return false; }
 const auto* Health = Target->FindComponentByClass<UPlayerHealthComponent>();
 return !Health || !Health->IsDefeated();
}
}
ARDCABossAIController::ARDCABossAIController()
{
 CombatTreeComponent = CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BossBehaviorTree"));
 Blackboard = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BossBlackboard"));
 BrainComponent = CombatTreeComponent;
 bStartAILogicOnPossess = false; bStopAILogicOnUnposses = true;
}
void ARDCABossAIController::OnPossess(APawn* InPawn)
{
 Super::OnPossess(InPawn);
 Encounter = InPawn ? InPawn->FindComponentByClass<UBossEncounterComponent>() : nullptr;
 bStartFailed = false; bManualTarget = false;
 if (auto* Boss = Cast<ARDCABossPawn>(InPawn)) { ReadyHandle = Boss->OnAIReady.AddUObject(this, &ThisClass::Ready); }
 TryStartCombatBrain();
}
void ARDCABossAIController::Ready() { TryStartCombatBrain(); }
void ARDCABossAIController::OnUnPossess()
{
 StopCombatBrain(); RemoveSubscriptions(); Encounter.Reset(); Super::OnUnPossess();
}
void ARDCABossAIController::EndPlay(const EEndPlayReason::Type Reason)
{
 StopCombatBrain(); RemoveSubscriptions(); Super::EndPlay(Reason);
}
void ARDCABossAIController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
 // Dedicated facing/laser logic owns rotation, not AIController focus.
}
bool ARDCABossAIController::TryStartCombatBrain()
{
 if (bBrainStarted) { return true; }
 auto* Boss = Cast<ARDCABossPawn>(GetPawn()); auto* C = Encounter.Get();
 if (bStartFailed || !HasAuthority() || !Boss || !Boss->IsAIReady() || !C || !C->HasBegunPlay() || C->IsEncounterStopped()) { return false; }
 if (!CombatBehaviorTree) { CombatBehaviorTree = LoadObject<UBehaviorTree>(nullptr, TEXT("/Game/AI/BT_RiftDragonCombat.BT_RiftDragonCombat")); }
 // Claim before validating: malformed formal AI configuration must fail closed.
 if (!C->ClaimDecisionControl(this)) { bStartFailed = true; return false; }
 if (!CombatBehaviorTree || !CombatBehaviorTree->RootNode || !ValidSchema(CombatBehaviorTree->BlackboardAsset))
 {
  C->ReleaseDecisionControl(this); bStartFailed = true;
  UE_LOG(LogRDCAPlayer, Error, TEXT("Boss AI tree/blackboard is invalid; combat suspended. Boss=%s"), *GetNameSafe(Boss));
  return false;
 }
 UBlackboardComponent* Used = Blackboard;
 if (!UseBlackboard(CombatBehaviorTree->BlackboardAsset, Used))
 {
  C->ReleaseDecisionControl(this); bStartFailed = true; return false;
 }
 CombatTreeComponent->CacheBlackboardComponent(Used);
 Used->CacheBrainComponent(*CombatTreeComponent);
 ContextChangedHandle = C->OnDecisionContextChanged.AddUObject(this, &ThisClass::RefreshBlackboard);
 ValidateCombatTarget(); RefreshBlackboard();
 if (!RunBehaviorTree(CombatBehaviorTree))
 {
  StopCombatBrain(); UE_LOG(LogRDCAPlayer, Error, TEXT("Boss AI failed to start; combat suspended.")); return false;
 }
 bBrainStarted = true;
 UE_LOG(LogRDCAPlayer, Log, TEXT("Boss AI behavior tree started. Boss=%s Tree=%s"), *GetNameSafe(Boss), *GetNameSafe(CombatBehaviorTree));
 return true;
}
void ARDCABossAIController::StopCombatBrain()
{
 bBrainStarted = false; bStartFailed = true;
 GetWorld()->GetTimerManager().ClearTimer(StopTimer);
 if (auto* C = Encounter.Get())
 {
  C->OnDecisionContextChanged.Remove(ContextChangedHandle);
  C->ReleaseDecisionControl(this);
 }
 ContextChangedHandle.Reset();
 CombatTreeComponent->StopTree(EBTStopMode::Forced);
 RemoveSubscriptions();
}
void ARDCABossAIController::RemoveSubscriptions()
{
 if (auto* Boss = Cast<ARDCABossPawn>(GetPawn())) { Boss->OnAIReady.Remove(ReadyHandle); }
 if (PlayerProvider.IsValid()) { PlayerProvider->GetOnNewPawnNotifier().Remove(PawnChangedHandle); }
 if (TargetHealth.IsValid()) { TargetHealth->OnDefeated.RemoveDynamic(this, &ThisClass::TargetDefeated); }
 if (CombatTarget.IsValid()) { CombatTarget->OnDestroyed.RemoveDynamic(this, &ThisClass::TargetDestroyed); }
 ReadyHandle.Reset(); PawnChangedHandle.Reset(); PlayerProvider.Reset(); TargetHealth.Reset(); CombatTarget.Reset();
}
void ARDCABossAIController::ValidateCombatTarget()
{
 if (bManualTarget) { if (!LiveTarget(CombatTarget.Get())) { AcceptTarget(nullptr); } return; }
 auto* PC = UGameplayStatics::GetPlayerController(this, 0);
 if (PC != PlayerProvider.Get())
 {
  if (PlayerProvider.IsValid()) { PlayerProvider->GetOnNewPawnNotifier().Remove(PawnChangedHandle); }
  PlayerProvider = PC;
  if (PC) { PawnChangedHandle = PC->GetOnNewPawnNotifier().AddUObject(this, &ThisClass::PlayerPawnChanged); }
 }
 APawn* Target = PC ? PC->GetPawn() : nullptr;
 if (!LiveTarget(Target) || Target == GetPawn()) { Target = nullptr; }
 if (Target != CombatTarget.Get()) { AcceptTarget(Target); }
}
void ARDCABossAIController::PlayerPawnChanged(APawn* NewPawn) { ValidateCombatTarget(); }
void ARDCABossAIController::TargetDefeated() { AcceptTarget(nullptr); }
void ARDCABossAIController::TargetDestroyed(AActor* Actor) { AcceptTarget(nullptr); }
void ARDCABossAIController::SetCombatTarget(APawn* Target)
{
 bManualTarget = true; AcceptTarget(LiveTarget(Target) ? Target : nullptr);
}
void ARDCABossAIController::AcceptTarget(APawn* Target)
{
 if (TargetHealth.IsValid()) { TargetHealth->OnDefeated.RemoveDynamic(this, &ThisClass::TargetDefeated); }
 if (CombatTarget.IsValid()) { CombatTarget->OnDestroyed.RemoveDynamic(this, &ThisClass::TargetDestroyed); }
 CombatTarget = Target; TargetHealth = Target ? Target->FindComponentByClass<UPlayerHealthComponent>() : nullptr;
 if (TargetHealth.IsValid()) { TargetHealth->OnDefeated.AddUniqueDynamic(this, &ThisClass::TargetDefeated); }
 if (Target) { Target->OnDestroyed.AddUniqueDynamic(this, &ThisClass::TargetDestroyed); }
 if (Encounter.IsValid()) { Encounter->SetAITarget(this, Target); }
 RefreshBlackboard();
}
APawn* ARDCABossAIController::GetCombatTarget() const { return LiveTarget(CombatTarget.Get()) ? CombatTarget.Get() : nullptr; }
bool ARDCABossAIController::IsCombatBlocked() const
{
 return !Blackboard || Blackboard->GetValueAsBool(RDCABossBlackboard::IsStopped)
  || Blackboard->GetValueAsBool(RDCABossBlackboard::IsHeld) || !Blackboard->GetValueAsObject(RDCABossBlackboard::TargetActor);
}
bool ARDCABossAIController::IsCombatBrainRunning() const
{
 return bBrainStarted && CombatTreeComponent->IsRunning() && !CombatTreeComponent->IsInstanceStackEmpty()
  && Encounter.IsValid() && Encounter->IsDecisionControlledBy(this);
}
ERDCABossAttackFamily ARDCABossAIController::FamilyFor(EBossAttackType Type)
{
 switch (Type)
 {
 case EBossAttackType::AimedVolley: case EBossAttackType::FanBarrage: return ERDCABossAttackFamily::Barrage;
 case EBossAttackType::Shockwave: return ERDCABossAttackFamily::Shockwave;
 case EBossAttackType::SweepLaser: return ERDCABossAttackFamily::Laser;
 default: return ERDCABossAttackFamily::None;
 }
}
FGameplayTag ARDCABossAIController::AbilityTagFor(EBossAttackType Type)
{
 switch (FamilyFor(Type))
 {
 case ERDCABossAttackFamily::Barrage: return RDCAGameplayTags::Ability_Boss_Barrage;
 case ERDCABossAttackFamily::Shockwave: return RDCAGameplayTags::Ability_Boss_Shockwave;
 case ERDCABossAttackFamily::Laser: return RDCAGameplayTags::Ability_Boss_Laser;
 default: return FGameplayTag();
 }
}
void ARDCABossAIController::RefreshBlackboard()
{
 if (!Blackboard || !Blackboard->GetBlackboardAsset()) { return; }
 const auto* C = Encounter.Get(); using namespace RDCABossBlackboard;
 Blackboard->PauseObserverNotifications();
 Blackboard->SetValueAsObject(TargetActor, GetCombatTarget());
 Blackboard->SetValueAsBool(IsStopped, !C || C->IsEncounterStopped());
 Blackboard->SetValueAsBool(IsHeld, C && C->IsEncounterHeld());
 Blackboard->SetValueAsBool(IsWeakPointExposed, C && C->GetEncounterState() == EBossEncounterState::WeakPointExposed);
 Blackboard->SetValueAsEnum(CombatPhase, C ? static_cast<uint8>(C->GetCombatPhase()) : 0);
 Blackboard->SetValueAsFloat(NextActionTime, C ? C->GetNextActionTime() : 0);
 const bool HasPlan = C && C->HasAttackPlan();
 Blackboard->SetValueAsBool(HasAttackPlan, HasPlan);
 Blackboard->SetValueAsEnum(SelectedAttack, HasPlan ? static_cast<uint8>(FamilyFor(C->GetCurrentAttack())) : 0);
 Blackboard->SetValueAsEnum(SelectedBarragePattern, C ? static_cast<uint8>(C->GetCurrentBarragePattern()) : 0);
 Blackboard->ResumeObserverNotifications(true);
 if (C && C->IsEncounterStopped() && bBrainStarted && !StopTimer.IsValid())
 { StopTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::StopCombatBrain); }
}
