#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "Boss/BossEncounterComponent.h"
#include "GameplayTagContainer.h"
#include "RDCABossAIController.generated.h"
class UBehaviorTree;
class UBehaviorTreeComponent;
class UPlayerHealthComponent;
UENUM(BlueprintType)
enum class ERDCABossAttackFamily : uint8 { None, Barrage, Shockwave, Laser };
namespace RDCABossBlackboard
{
 inline const FName TargetActor(TEXT("TargetActor"));
 inline const FName CombatPhase(TEXT("CombatPhase"));
 inline const FName IsHeld(TEXT("IsEncounterHeld"));
 inline const FName IsStopped(TEXT("IsBattleOver"));
 inline const FName IsWeakPointExposed(TEXT("IsStunned"));
 inline const FName NextActionTime(TEXT("NextActionTime"));
 inline const FName SelectedAttack(TEXT("SelectedAttack"));
 inline const FName SelectedBarragePattern(TEXT("SelectedBarragePattern"));
 inline const FName HasAttackPlan(TEXT("HasAttackPlan"));
}
UCLASS(Blueprintable)
class RDCA_API ARDCABossAIController : public AAIController
{
 GENERATED_BODY()
public:
 ARDCABossAIController();
 virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true) override;
 bool TryStartCombatBrain();
 void StopCombatBrain();
 void RefreshBlackboard();
 void ValidateCombatTarget();
 void SetCombatTarget(APawn* Target);
 UBossEncounterComponent* GetBossEncounter() const { return Encounter.Get(); }
 APawn* GetCombatTarget() const;
 bool IsCombatBlocked() const;
 UFUNCTION(BlueprintPure, Category = "Boss|AI") bool IsCombatBrainRunning() const;
 // Authoring/tests only. Production requires the saved visual asset.
 static UBehaviorTree* CreateDefaultCombatTree(UObject* Outer);
 static ERDCABossAttackFamily FamilyFor(EBossAttackType Type);
 static FGameplayTag AbilityTagFor(EBossAttackType Type);
protected:
 virtual void OnPossess(APawn* InPawn) override;
 virtual void OnUnPossess() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 UPROPERTY(EditDefaultsOnly, Category = "Boss|AI") TObjectPtr<UBehaviorTree> CombatBehaviorTree;
private:
 void AcceptTarget(APawn* Target);
 void RemoveSubscriptions();
 void Ready();
 UFUNCTION() void TargetDefeated();
 UFUNCTION() void TargetDestroyed(AActor* Actor);
 void PlayerPawnChanged(APawn* Pawn);
 UPROPERTY(VisibleAnywhere, Category = "Boss|AI") TObjectPtr<UBehaviorTreeComponent> CombatTreeComponent;
 TWeakObjectPtr<UBossEncounterComponent> Encounter;
 TWeakObjectPtr<APlayerController> PlayerProvider;
 TWeakObjectPtr<APawn> CombatTarget;
 TWeakObjectPtr<UPlayerHealthComponent> TargetHealth;
 FDelegateHandle ContextChangedHandle, ReadyHandle, PawnChangedHandle;
 FTimerHandle StopTimer;
 bool bBrainStarted = false, bStartFailed = false, bManualTarget = false;
};
