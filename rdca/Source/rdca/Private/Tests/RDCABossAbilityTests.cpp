#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/RDCAAbilitySystemComponent.h"
#include "Abilities/RDCABossAttackAbility.h"
#include "Abilities/RDCAGameplayTags.h"
#include "Boss/BossEncounterComponent.h"
#include "Boss/BossFanProjectile.h"
#include "Boss/BossSweepLaser.h"
#include "Boss/BossWeakPointComponent.h"
#include "Boss/RDCABossPawn.h"
#include "Boss/AI/RDCABossAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "Boss/AI/RDCABossBTNodes.h"
#include "EdGraph/EdGraph.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

namespace
{
void SetBossFloat(UObject* Object, const FName Name, float Value)
{
	FindFProperty<FFloatProperty>(Object->GetClass(), Name)->SetPropertyValue_InContainer(Object, Value);
}
void SetBossInt(UObject* Object, const FName Name, int32 Value)
{
	FindFProperty<FIntProperty>(Object->GetClass(), Name)->SetPropertyValue_InContainer(Object, Value);
}
void SetBossBool(UObject* Object, const FName Name, bool Value)
{
	FindFProperty<FBoolProperty>(Object->GetClass(), Name)->SetPropertyValue_InContainer(Object, Value);
}

struct FBossGasWorld
{
	UWorld* World;
	AActor* Boss;
	UBossEncounterComponent* Encounter;
	UBossWeakPointComponent* WeakPoint;
	URDCAAbilitySystemComponent* ASC;

	explicit FBossGasWorld(bool bUseBossPawn = false, bool bUseAI = false)
	{
		UWorld::InitializationValues Settings;
		Settings.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
			.CreateAISystem(bUseAI).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
			true, ERHIFeatureLevel::Num, &Settings);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		Boss = bUseBossPawn
			? World->SpawnActor<ARDCABossPawn>()
			: World->SpawnActor<AActor>();
		USceneComponent* Root = NewObject<USceneComponent>(Boss);
		Boss->AddInstanceComponent(Root);
		Boss->SetRootComponent(Root);
		Root->RegisterComponent();
		Boss->SetActorLocation(FVector(0.0f, 0.0f, 300.0f));
		UStaticMeshComponent* Wave = NewObject<UStaticMeshComponent>(Boss, TEXT("ShockwaveVisual"));
		Boss->AddInstanceComponent(Wave);
		Wave->SetupAttachment(Root);
		Wave->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Wave->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
		Wave->SetRelativeScale3D(FVector(2.0f));
		Wave->RegisterComponent();
		WeakPoint = NewObject<UBossWeakPointComponent>(Boss);
		Boss->AddInstanceComponent(WeakPoint);
		WeakPoint->SetupAttachment(Root);
		WeakPoint->RegisterComponent();
		SetBossInt(WeakPoint, TEXT("MaximumHitPoints"), 6);
		Encounter = NewObject<UBossEncounterComponent>(Boss);
		Boss->AddInstanceComponent(Encounter);
		Encounter->RegisterComponent();
		SetBossFloat(Encounter, TEXT("InitialIdleDuration"), 0.0f);
		SetBossFloat(Encounter, TEXT("WarningDuration"), 0.1f);
		SetBossFloat(Encounter, TEXT("AimedVolleyWarningDuration"), 0.1f);
		SetBossFloat(Encounter, TEXT("FanBarrageWarningDuration"), 0.1f);
		SetBossFloat(Encounter, TEXT("LaserAimWarningDuration"), 0.1f);
		SetBossFloat(Encounter, TEXT("LaserPostWarningPauseDuration"), 0.2f);
		SetBossFloat(Encounter, TEXT("LaserActiveSweepDuration"), 0.25f);
		SetBossFloat(Encounter, TEXT("LaserWarningDescentDistance"), 100.0f);
		SetBossFloat(Encounter, TEXT("LaserWarningDescentDuration"), 0.05f);
		SetBossFloat(Encounter, TEXT("ShockwaveInitialRadius"), 100.0f);
		SetBossFloat(Encounter, TEXT("ShockwaveExpandedMaximumRadius"), 1100.0f);
		SetBossFloat(Encounter, TEXT("ShockwaveExpansionSpeed"), 1000.0f);
		SetBossFloat(Encounter, TEXT("Phase2ShockwavePulseDelay"), 0.2f);
		SetBossInt(Encounter, TEXT("AttackSelectionRandomSeed"), 123);
	}

	void Begin(bool bPhase2 = false)
	{
		// Predictive and homing patterns deliberately require a real player target.
		APawn* Player = World->SpawnActorDeferred<APawn>(APawn::StaticClass(), FTransform::Identity);
		Player->AutoPossessAI = EAutoPossessAI::Disabled;
		Player->FinishSpawning(FTransform::Identity);
		USceneComponent* PlayerRoot = NewObject<USceneComponent>(Player);
		Player->AddInstanceComponent(PlayerRoot);
		Player->SetRootComponent(PlayerRoot);
		PlayerRoot->RegisterComponent();
		Player->SetActorLocation(FVector(1000.0f, 0.0f, 300.0f));
		APlayerController* Controller = World->SpawnActor<APlayerController>();
		World->AddController(Controller); // Isolated world has not initialized actors for play.
		Controller->Possess(Player);
		Boss->DispatchBeginPlay();
		ASC = Boss->FindComponentByClass<URDCAAbilitySystemComponent>();
		if (bPhase2) { ASC->ApplyDamage(3.0f); }
	}
	~FBossGasWorld()
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	void Step(float Seconds) { World->Tick(LEVELTICK_TimeOnly, Seconds); Encounter->TickComponent(Seconds, LEVELTICK_All, nullptr); }
	void Select() { Step(0.0f); Step(0.0f); }
	void ForceWave()
	{
		SetBossFloat(Encounter, TEXT("Phase2BarrageWeight"), 0.0f);
		SetBossFloat(Encounter, TEXT("Phase2ShockwaveWeight"), 1.0f);
		SetBossFloat(Encounter, TEXT("Phase2LaserWeight"), 0.0f);
	}
	ARDCABossAIController* StartAI(UBehaviorTree* OverrideTree = nullptr)
	{
		// Tests explicitly supply the standard tree; production loads the saved asset.
		auto* AI = World->SpawnActor<ARDCABossAIController>();
		if (!OverrideTree) { OverrideTree = ARDCABossAIController::CreateDefaultCombatTree(AI); }
		if (OverrideTree)
		{
			FindFProperty<FObjectPropertyBase>(AI->GetClass(), TEXT("CombatBehaviorTree"))
				->SetObjectPropertyValue_InContainer(AI, OverrideTree);
		}
		AI->DispatchBeginPlay();
		AI->Possess(CastChecked<APawn>(Boss));
		return AI;
	}
	void StepAI(ARDCABossAIController* AI, float Seconds = 0.05f)
	{
		Step(Seconds);
		AI->Tick(Seconds);
		CastChecked<UBehaviorTreeComponent>(AI->GetBrainComponent())->TickComponent(Seconds, LEVELTICK_All, nullptr);
	}
	int32 LiveAttackActors() const
	{
		int32 Count = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (IsValid(*It) && It->GetOwner() == Boss
				&& (It->IsA<ABossSweepLaser>() || It->IsA<ABossFanProjectile>())) { ++Count; }
		}
		return Count;
	}
	bool AnyWaveCollision(bool bSecondary = false) const
	{
		TArray<UBoxComponent*> Boxes;
		Boss->GetComponents(Boxes);
		const FString Prefix = bSecondary ? TEXT("SecondaryShockwaveCollisionSegment") : TEXT("ShockwaveCollisionSegment");
		for (const UBoxComponent* Box : Boxes)
		{
			if (Box->GetName().StartsWith(Prefix) && Box->GetCollisionEnabled() != ECollisionEnabled::NoCollision) { return true; }
		}
		return false;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossGasLaserLifecycle, "RDCA.GAS.Boss.LaserLifecycleAndExclusivity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBossGasLaserLifecycle::RunTest(const FString& Parameters)
{
	FBossGasWorld F;
	SetBossBool(F.Encounter, TEXT("bDebugForceSweepLaser"), true);
	SetBossInt(F.Encounter, TEXT("AttacksBeforeWeakPointExposure"), 1);
	F.Begin();
	TestNotNull(TEXT("Barrage granted to existing generic Boss actor"), F.ASC->FindAbilitySpecFromClass(URDCABossBarrageAbility::StaticClass()));
	TestNotNull(TEXT("Wave granted"), F.ASC->FindAbilitySpecFromClass(URDCABossShockwaveAbility::StaticClass()));
	F.ASC->GrantBossAbilities();
	TestEqual(TEXT("Repeated granting does not duplicate skills"), F.ASC->GetActivatableAbilities().Num(), 3);
	F.Select();
	TestTrue(TEXT("Preparing activates GAS skill"), F.ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Boss_Attacking));
	TestEqual(TEXT("Warning laser exists"), F.LiveAttackActors(), 1);
	TestFalse(TEXT("Another skill cannot start concurrently"), F.ASC->TryActivateAbilityByClass(URDCABossShockwaveAbility::StaticClass()));
	F.Step(0.11f);
	TestTrue(TEXT("Post-warning pause still preparing"), F.Encounter->GetEncounterState() == EBossEncounterState::Preparing);
	TestTrue(TEXT("Saved descent still applies"), FMath::IsNearlyEqual(F.Boss->GetActorLocation().Z, 200.0f));
	F.Step(0.20f);
	TestTrue(TEXT("Pause ends before attack begins"), F.Encounter->GetEncounterState() == EBossEncounterState::Attacking);
	F.Step(0.26f);
	TestTrue(TEXT("Final attack enters stun directly, no Recovery gap"), F.Encounter->GetEncounterState() == EBossEncounterState::WeakPointExposed);
	TestEqual(TEXT("Exactly one completed attack"), F.Encounter->GetCompletedAttackCount(), 1);
	TestTrue(TEXT("Weak point opens"), F.WeakPoint->IsExposed());
	TestFalse(TEXT("Skill tag removed"), F.ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Boss_Attacking));
	TestEqual(TEXT("Laser cleaned before stun"), F.LiveAttackActors(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossGasCancellation, "RDCA.GAS.Boss.HoldStopDeathAndReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBossGasCancellation::RunTest(const FString& Parameters)
{
	for (int32 Scenario = 0; Scenario < 5; ++Scenario)
	{
		FBossGasWorld F;
		SetBossBool(F.Encounter, TEXT("bDebugForceSweepLaser"), true);
		F.Begin();
		F.Select();
		F.Step(0.11f);
		if (Scenario != 0) { F.Step(0.20f); }
		if (Scenario <= 1) { F.Encounter->SetEncounterHold(true); }
		else if (Scenario == 2) { F.Encounter->StopEncounter(); }
		else if (Scenario == 3) { F.ASC->ApplyDamage(100.0f); F.Step(0.0f); }
		else { F.WeakPoint->ResetWeakPoint(); }
		TestEqual(TEXT("Cancelled attack is not completed"), F.Encounter->GetCompletedAttackCount(), 0);
		TestFalse(TEXT("Cancellation clears owned tag"), F.ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Boss_Attacking));
		TestEqual(TEXT("Warning/active laser destroyed"), F.LiveAttackActors(), 0);
		TestTrue(TEXT("Cancelled descent restores normal altitude"), FMath::IsNearlyEqual(F.Boss->GetActorLocation().Z, 300.0f));
		TestFalse(TEXT("Cancellation never exposes weak point"), F.WeakPoint->IsExposed());
		if (Scenario <= 1)
		{
			F.Step(10.0f);
			TestEqual(TEXT("Hold cannot accidentally emit another laser"), F.LiveAttackActors(), 0);
			F.Encounter->SetEncounterHold(false);
			F.Select();
			TestTrue(TEXT("Resume can start a fresh skill"), F.ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Boss_Attacking));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossGasDoubleWave, "RDCA.GAS.Boss.DoubleWaveCountsOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBossGasDoubleWave::RunTest(const FString& Parameters)
{
	FBossGasWorld F;
	F.ForceWave();
	SetBossInt(F.Encounter, TEXT("Phase2AttacksBeforeStun"), 1);
	F.Begin(true);
	F.Select();
	TestTrue(TEXT("Category weight selects shockwave"), F.Encounter->GetCurrentAttack() == EBossAttackType::Shockwave);
	F.Step(0.11f);
	TestTrue(TEXT("First wave has active collision"), F.AnyWaveCollision());
	TestFalse(TEXT("Second wave not yet released"), F.AnyWaveCollision(true));
	F.Step(0.21f);
	TestTrue(TEXT("Second wave overlaps first in same ability"), F.AnyWaveCollision(true) && F.AnyWaveCollision());
	TestEqual(TEXT("Second pulse does not count another attack"), F.Encounter->GetCompletedAttackCount(), 0);
	F.Step(0.80f);
	TestFalse(TEXT("First wave ends independently"), F.AnyWaveCollision());
	TestTrue(TEXT("Second wave continues"), F.AnyWaveCollision(true));
	TestTrue(TEXT("Ability still active after first pulse"), F.ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Boss_Attacking));
	F.Step(0.20f);
	TestEqual(TEXT("Two pulses complete one attack"), F.Encounter->GetCompletedAttackCount(), 1);
	TestTrue(TEXT("Stun starts when entire double wave completes"), F.Encounter->GetEncounterState() == EBossEncounterState::WeakPointExposed);
	TestFalse(TEXT("All wave collision disabled"), F.AnyWaveCollision() || F.AnyWaveCollision(true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossGasWaveCancel, "RDCA.GAS.Boss.WaveCancellationAndEndPlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBossGasWaveCancel::RunTest(const FString& Parameters)
{
	for (int32 Scenario = 0; Scenario < 3; ++Scenario)
	{
		FBossGasWorld F;
		F.ForceWave();
		F.Begin(true);
		F.Select();
		F.Step(0.11f);
		F.Step(0.21f);
		if (Scenario == 0) { F.Encounter->SetEncounterHold(true); }
		else if (Scenario == 1) { F.Encounter->StopEncounter(); }
		else { F.Encounter->DestroyComponent(); }
		TestFalse(TEXT("Both wave collisions cleaned on interruption"), F.AnyWaveCollision() || F.AnyWaveCollision(true));
		TestFalse(TEXT("EndPlay/hold/stop leaves no active skill"), F.ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Boss_Attacking));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossGasBarrage, "RDCA.GAS.Boss.AllEightBarragePatterns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBossGasBarrage::RunTest(const FString& Parameters)
{
	for (int32 Pattern = 0; Pattern < 8; ++Pattern)
	{
		FBossGasWorld F;
		SetBossBool(F.Encounter, TEXT("bDebugForceBarrage"), true);
		FEnumProperty* Property = FindFProperty<FEnumProperty>(F.Encounter->GetClass(), TEXT("DebugBarragePattern"));
		Property->GetUnderlyingProperty()->SetIntPropertyValue(Property->ContainerPtrToValuePtr<void>(F.Encounter), static_cast<uint64>(Pattern));
		F.Begin();
		F.Select();
		F.Step(0.11f);
		TestTrue(TEXT("Barrage skill active"), F.ASC->FindAbilitySpecFromClass(URDCABossBarrageAbility::StaticClass())->IsActive());
		TestTrue(TEXT("Selected pattern emits projectiles"), F.LiveAttackActors() > 0);
		for (int32 Step = 0; Step < 300 && F.Encounter->GetEncounterState() == EBossEncounterState::Attacking; ++Step) { F.Step(0.05f); }
		TestTrue(TEXT("Pattern finishes into Recovery"), F.Encounter->GetEncounterState() == EBossEncounterState::Recovery);
		TestEqual(TEXT("Pattern counts once"), F.Encounter->GetCompletedAttackCount(), 1);
		TestFalse(TEXT("Emission skill has ended"), F.ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Boss_Attacking));
		F.Encounter->StopEncounter();
		TestEqual(TEXT("Stopping encounter clears fired projectiles"), F.LiveAttackActors(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossGasRound, "RDCA.GAS.Boss.Phase2SelectionAndThreeAttackRound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBossGasRound::RunTest(const FString& Parameters)
{
	FBossGasWorld F;
	F.ForceWave();
	SetBossFloat(F.Encounter, TEXT("Phase2InterAttackDelay"), 0.0f);
	F.Begin(true);
	F.Select();
	TSet<int32> Keys;
	for (int32 Attack = 0; Attack < 3; ++Attack)
	{
		const EBossAttackType Type = F.Encounter->GetCurrentAttack();
		const int32 Key = Type == EBossAttackType::Shockwave ? 0 : Type == EBossAttackType::SweepLaser ? 1
			: 100 + static_cast<int32>(F.Encounter->GetCurrentBarragePattern());
		TestFalse(TEXT("Exact attack cannot repeat within round"), Keys.Contains(Key));
		Keys.Add(Key);
		if (Attack == 0) { TestEqual(TEXT("Wave weight is not diluted by barrage pattern count"), Key, 0); }
		F.Step(0.31f);
		for (int32 Step = 0; Step < 400 && F.Encounter->GetEncounterState() == EBossEncounterState::Attacking; ++Step) { F.Step(0.05f); }
		TestEqual(TEXT("Completion count advances once per skill"), F.Encounter->GetCompletedAttackCount(), Attack + 1);
		TestTrue(TEXT("Only third attack opens weak point"), (F.Encounter->GetEncounterState() == EBossEncounterState::WeakPointExposed) == (Attack == 2));
		if (Attack < 2) { F.Select(); }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossAIDecisionOwnership, "RDCA.AI.DecisionOwnershipAndExecutionIds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBossAIDecisionOwnership::RunTest(const FString& Parameters)
{
	FBossGasWorld F(true);
	F.Begin();
	UObject* Driver = NewObject<USceneComponent>(F.Boss);
	UObject* Other = NewObject<USceneComponent>(F.Boss);
	TestTrue(TEXT("Driver claims decision ownership"), F.Encounter->ClaimDecisionControl(Driver));
	TestFalse(TEXT("Second driver cannot steal ownership"), F.Encounter->ClaimDecisionControl(Other));
	F.Step(10.0f);
	TestTrue(TEXT("Legacy Tick cannot select while controlled"), F.Encounter->GetEncounterState() == EBossEncounterState::Idle);
	TestFalse(TEXT("Other driver cannot advance"), F.Encounter->AdvanceDecisionWindow(Other));
	TestTrue(TEXT("Owner advances existing elapsed window"), F.Encounter->AdvanceDecisionWindow(Driver));
	TestFalse(TEXT("Wrong phase cannot select"), F.Encounter->SelectAttackForAI(Driver, EBossCombatPhase::Phase2));
	TestTrue(TEXT("Phase strategy selects without execution"), F.Encounter->SelectAttackForAI(Driver, EBossCombatPhase::Phase1));
	TestFalse(TEXT("Selection does not activate GAS"), F.Encounter->IsExecutingAttack());
	TestFalse(TEXT("Selected attack cannot be selected twice"), F.Encounter->SelectAttackForAI(Driver, EBossCombatPhase::Phase1));
	const uint64 First = F.Encounter->ExecuteAttackForAI(Driver);
	TestTrue(TEXT("Execution receives a unique nonzero ID"), First > 0);
	TestEqual(TEXT("Duplicate execution rejected"), F.Encounter->ExecuteAttackForAI(Driver), uint64(0));
	F.Encounter->CancelAttackForAI(Other, First);
	TestTrue(TEXT("Foreign driver cannot cancel"), F.Encounter->IsExecutingAttack());
	F.Encounter->SetEncounterHold(true);
	TestEqual(TEXT("Cancellation publishes exact finished ID"), F.Encounter->GetLastFinishedAttackId(), First);
	TestTrue(TEXT("Cancellation result distinguished"), F.Encounter->WasLastAttackCancelled());
	TestEqual(TEXT("Cancelled attack not counted"), F.Encounter->GetCompletedAttackCount(), 0);
	F.Encounter->SetEncounterHold(false);
	F.Step(0.0f);
	F.Encounter->AdvanceDecisionWindow(Driver);
	F.Encounter->SelectAttackForAI(Driver, EBossCombatPhase::Phase1);
	const uint64 Second = F.Encounter->ExecuteAttackForAI(Driver);
	TestTrue(TEXT("Next execution ID differs"), Second > First);
	F.Encounter->CancelAttackForAI(Driver, First);
	TestTrue(TEXT("Stale task cannot cancel new execution"), F.Encounter->IsExecutingAttack());
	F.Encounter->ReleaseDecisionControl(Driver);
	F.Step(10.0f);
	TestFalse(TEXT("Lost driver does not resume legacy attacks"), F.Encounter->IsExecutingAttack());
	TestTrue(TEXT("A new driver may claim suspended Boss"), F.Encounter->ClaimDecisionControl(Other));
	F.Encounter->ReleaseDecisionControl(Other, true);
	F.Select();
	TestTrue(TEXT("Explicit startup-failure fallback restores one legacy scheduler"), F.Encounter->IsExecutingAttack());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossAITreePhase1, "RDCA.AI.Tree.Phase1HoldAndBlackboard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBossAITreePhase1::RunTest(const FString& Parameters)
{
	FBossGasWorld F(true, true);
	F.Begin();
	F.Encounter->SetEncounterHold(true);
	auto* AI = F.StartAI();
	TestTrue(TEXT("Real UE behavior tree starts"), AI->IsCombatBrainRunning());
	TestTrue(TEXT("Encounter has exactly this decision owner"), F.Encounter->IsDecisionControlledBy(AI));
	TestTrue(TEXT("Blackboard sees Hold immediately"), AI->GetBlackboardComponent()->GetValueAsBool(RDCABossBlackboard::IsHeld));
	for (int32 I = 0; I < 10; ++I) { F.StepAI(AI); }
	TestFalse(TEXT("Hold cannot start an attack"), F.Encounter->IsExecutingAttack());
	F.Encounter->SetEncounterHold(false);
	TestFalse(TEXT("Hold release updates blackboard immediately"), AI->GetBlackboardComponent()->GetValueAsBool(RDCABossBlackboard::IsHeld));
	TestNotNull(TEXT("Blackboard uses registered player, not Boss Pawn"), AI->GetBlackboardComponent()->GetValueAsObject(RDCABossBlackboard::TargetActor));
	for (int32 I = 0; I < 800 && !F.WeakPoint->IsExposed(); ++I) { F.StepAI(AI); }
	if (!F.WeakPoint->IsExposed())
	{
		auto* BT = CastChecked<UBehaviorTreeComponent>(AI->GetBrainComponent());
		AddInfo(FString::Printf(TEXT("BT flags: registered=%d paused=%d running=%d stackEmpty=%d"), BT->IsRegistered(), BT->IsPaused(), BT->IsRunning(), BT->IsInstanceStackEmpty()));
		AddInfo(FString::Printf(TEXT("AI diagnostic: active=%s tasks=%s now=%.2f deadline=%.2f target=%s held=%d stopped=%d"),
			BT->GetActiveNode() ? *BT->GetActiveNode()->GetNodeName() : TEXT("none"), *BT->DescribeActiveTasks(), F.World->GetTimeSeconds(),
			AI->GetBlackboardComponent()->GetValueAsFloat(RDCABossBlackboard::NextActionTime),
			*GetNameSafe(AI->GetBlackboardComponent()->GetValueAsObject(RDCABossBlackboard::TargetActor)),
			AI->GetBlackboardComponent()->GetValueAsBool(RDCABossBlackboard::IsHeld), AI->GetBlackboardComponent()->GetValueAsBool(RDCABossBlackboard::IsStopped)));
	}
	TestTrue(TEXT("Phase 1 round opens weak point"), F.WeakPoint->IsExposed());
	TestEqual(TEXT("Phase 1 completes exactly two attacks"), F.Encounter->GetCompletedAttackCount(), 2);
	TestTrue(TEXT("Exposure already reflected without controller polling"), AI->GetBlackboardComponent()->GetValueAsBool(RDCABossBlackboard::IsWeakPointExposed));
	TestFalse(TEXT("No skill remains active in stun"), F.Encounter->IsExecutingAttack());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossAITreePhase2, "RDCA.AI.Tree.Phase2UniqueRoundDoubleWaveAndExpiry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBossAITreePhase2::RunTest(const FString& Parameters)
{
	FBossGasWorld F(true, true);
	F.ForceWave();
	SetBossFloat(F.Encounter, TEXT("Phase2InterAttackDelay"), 0.0f);
	SetBossFloat(F.Encounter, TEXT("Phase2WeakPointExposedDuration"), 0.1f);
	SetBossFloat(F.Encounter, TEXT("WeakPointStunDurationMultiplier"), 1.0f);
	F.Begin(true);
	auto* AI = F.StartAI();
	uint64 SeenId = 0;
	TSet<int32> Keys;
	bool bSawBothPulses = false;
	for (int32 I = 0; I < 1200 && !F.WeakPoint->IsExposed(); ++I)
	{
		F.StepAI(AI);
		bSawBothPulses |= F.AnyWaveCollision() && F.AnyWaveCollision(true);
		if (F.Encounter->IsExecutingAttack() && F.Encounter->GetAttackExecutionId() != SeenId)
		{
			SeenId = F.Encounter->GetAttackExecutionId();
			const auto Type = F.Encounter->GetCurrentAttack();
			const int32 Key = Type == EBossAttackType::Shockwave ? 0 : Type == EBossAttackType::SweepLaser ? 1
				: 100 + static_cast<int32>(F.Encounter->GetCurrentBarragePattern());
			TestFalse(TEXT("Exact attacks do not repeat in this round"), Keys.Contains(Key));
			if (Keys.IsEmpty()) { TestEqual(TEXT("Wave category is not diluted by barrage count"), Key, 0); }
			Keys.Add(Key);
		}
	}
	TestEqual(TEXT("Tree executes exactly three concrete attacks"), Keys.Num(), 3);
	TestEqual(TEXT("Double wave counted as one attack"), F.Encounter->GetCompletedAttackCount(), 3);
	TestTrue(TEXT("Both shockwave pulses existed concurrently"), bSawBothPulses);
	TestTrue(TEXT("Third attack immediately exposes weak point"), F.WeakPoint->IsExposed());
	for (int32 I = 0; I < 30 && F.Encounter->GetAttackExecutionId() == SeenId; ++I) { F.StepAI(AI); }
	TestTrue(TEXT("Missed exposure expires into next round"), F.Encounter->GetAttackExecutionId() > SeenId);
	TestEqual(TEXT("Expiry resets previous completion count"), F.Encounter->GetCompletedAttackCount(), 0);
	TestFalse(TEXT("New round closes weak point"), F.WeakPoint->IsExposed());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossAITreeCancellation, "RDCA.AI.Tree.InterruptionTargetLossAndUnpossess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBossAITreeCancellation::RunTest(const FString& Parameters)
{
	for (int32 Scenario = 0; Scenario < 5; ++Scenario)
	{
		FBossGasWorld F(true, true);
		SetBossBool(F.Encounter, TEXT("bDebugForceSweepLaser"), true);
		F.Begin();
		auto* AI = F.StartAI();
		for (int32 I = 0; I < 20 && !F.Encounter->IsExecutingAttack(); ++I) { F.StepAI(AI, 0.01f); }
		TestTrue(TEXT("Tree has a live warning skill before interruption"), F.Encounter->IsExecutingAttack());
		if (Scenario == 0) { F.Encounter->SetEncounterHold(true); }
		else if (Scenario == 1) { F.Encounter->StopEncounter(); }
		else if (Scenario == 2) { F.ASC->ApplyDamage(100.0f); }
		else if (Scenario == 3)
		{
			if (!TestNotNull(TEXT("Live player before target-loss case"), AI->GetCombatTarget())) { return false; }
			CastChecked<APlayerController>(AI->GetCombatTarget()->GetController())->UnPossess();
		}
		else { AI->UnPossess(); }
		for (int32 I = 0; I < 10; ++I) { F.StepAI(AI); }
		TestFalse(TEXT("Interrupted tree leaves no GAS attack"), F.Encounter->IsExecutingAttack());
		TestEqual(TEXT("Cancellation never counts a completed attack"), F.Encounter->GetCompletedAttackCount(), 0);
		TestEqual(TEXT("No laser or projectile remains"), F.LiveAttackActors(), 0);
		TestFalse(TEXT("Cancellation does not expose weak point"), F.WeakPoint->IsExposed());
		if (Scenario == 0)
		{
			F.Encounter->SetEncounterHold(false);
			for (int32 I = 0; I < 30 && !F.Encounter->IsExecutingAttack(); ++I) { F.StepAI(AI); }
			TestTrue(TEXT("Hold release can start one fresh attack"), F.Encounter->IsExecutingAttack());
		}
		if (Scenario == 4)
		{
			F.Step(10.0f);
			TestFalse(TEXT("Unpossessed Boss does not revert to autonomous fire"), F.Encounter->IsExecutingAttack());
			TestFalse(TEXT("Controller no longer owns decision"), F.Encounter->IsBehaviorTreeDriven());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossAITreeStartup, "RDCA.AI.Tree.DeferredStartupAndRepossess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBossAITreeStartup::RunTest(const FString& Parameters)
{
	FBossGasWorld F(true, true);
	SetBossBool(F.Encounter, TEXT("bDebugForceSweepLaser"), true);
	auto* AI = F.StartAI(); // Matches auto-possession before Pawn/component BeginPlay.
	TestFalse(TEXT("Possession alone cannot start uninitialized combat"), AI->IsCombatBrainRunning());
	TestFalse(TEXT("Uninitialized Encounter remains unclaimed"), F.Encounter->IsBehaviorTreeDriven());
	F.Begin();
	for (int32 I = 0; I < 20 && !F.Encounter->IsExecutingAttack(); ++I) { F.StepAI(AI, 0.01f); }
	TestTrue(TEXT("Controller starts after all components are ready"), AI->IsCombatBrainRunning());
	TestTrue(TEXT("Initialized Boss executes first skill"), F.Encounter->IsExecutingAttack());
	TestTrue(TEXT("Repeated start is idempotent"), AI->TryStartCombatBrain());
	TestEqual(TEXT("AI does not reinitialize or duplicate GAS abilities"), F.ASC->GetActivatableAbilities().Num(), 3);
	const uint64 OldId = F.Encounter->GetAttackExecutionId();
	AI->UnPossess();
	F.Step(10.0f);
	TestFalse(TEXT("Unpossessed Boss remains suspended"), F.Encounter->IsExecutingAttack());
	AI->Possess(CastChecked<APawn>(F.Boss));
	for (int32 I = 0; I < 20 && !F.Encounter->IsExecutingAttack(); ++I) { F.StepAI(AI, 0.01f); }
	TestTrue(TEXT("Re-possession starts a fresh tree"), AI->IsCombatBrainRunning());
	TestTrue(TEXT("Re-possession can execute a new skill"), F.Encounter->IsExecutingAttack());
	TestTrue(TEXT("New tree owns a later execution ID"), F.Encounter->GetAttackExecutionId() > OldId);
	F.Encounter->CancelAttackForAI(AI, OldId);
	TestTrue(TEXT("Old tree ID cannot cancel re-possessed attack"), F.Encounter->IsExecutingAttack());
	AI->StopCombatBrain();
	TestFalse(TEXT("Stopping brain cancels its active skill"), F.Encounter->IsExecutingAttack());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossAITreeInvalidConfig, "RDCA.AI.Tree.InvalidAssetSuspendsCombat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBossAITreeInvalidConfig::RunTest(const FString& Parameters)
{
	FBossGasWorld F(true, true);
	F.Begin();
	AddExpectedError(TEXT("Boss AI tree/blackboard is invalid"), EAutomationExpectedErrorFlags::Contains, 1);
	auto* AI = F.StartAI(NewObject<UBehaviorTree>(F.Boss));
	TestFalse(TEXT("Invalid tree does not run"), AI->IsCombatBrainRunning());
	TestFalse(TEXT("Invalid tree does not claim decision"), F.Encounter->IsBehaviorTreeDriven());
	F.Select();
	TestFalse(TEXT("Invalid formal AI cannot silently fall back to legacy attacks"), F.Encounter->IsExecutingAttack());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossPawnComposition, "RDCA.AI.BossPawn.CompositionAndSharedASC",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBossPawnComposition::RunTest(const FString& Parameters)
{
	const ARDCABossPawn* Defaults = GetDefault<ARDCABossPawn>();
	TestNull(TEXT("Bridge does not replace Blueprint root"), Defaults->GetRootComponent());
	TestNull(TEXT("Bridge does not create a second ASC"), Defaults->GetAbilitySystemComponent());
	TestNull(TEXT("Bridge does not add character movement"), Defaults->GetMovementComponent());
	TestTrue(TEXT("Controller class is available without automatic possession"), Defaults->AIControllerClass == ARDCABossAIController::StaticClass());
	TestTrue(TEXT("Does not take player control"), Defaults->AutoPossessPlayer == EAutoReceiveInput::Disabled);
	TestTrue(TEXT("Does not auto-spawn an AI controller"), Defaults->AutoPossessAI == EAutoPossessAI::Disabled);
	TestFalse(TEXT("Encounter keeps ownership of facing"), Defaults->bUseControllerRotationYaw);
#if WITH_EDITOR
	TestTrue(TEXT("Authored Blueprint Event Tick remains allowed"), Defaults->GetClass()->HasMetaData(TEXT("ChildCanTick")));
#endif

	FBossGasWorld F(true);
	ARDCABossPawn* Pawn = CastChecked<ARDCABossPawn>(F.Boss);
	const FTransform AuthoredTransform(FRotator(0.0f, 90.0f, 0.0f), FVector(0.0f, 0.0f, 300.0f), FVector(2.0f));
	Pawn->SetActorTransform(AuthoredTransform);
	USceneComponent* AuthoredRoot = Pawn->GetRootComponent();
	const ECollisionChannel WeakPointChannel = F.WeakPoint->GetCollisionObjectType();
	F.Begin();
	TestTrue(TEXT("Existing authored transform survives initialization"), Pawn->GetActorTransform().Equals(AuthoredTransform));
	TestTrue(TEXT("Existing root survives initialization"), Pawn->GetRootComponent() == AuthoredRoot);
	TestTrue(TEXT("Existing weak-point collision type unchanged"), F.WeakPoint->GetCollisionObjectType() == WeakPointChannel);
	TestTrue(TEXT("Native GAS interface returns existing runtime ASC"), Pawn->GetAbilitySystemComponent() == F.ASC);
	TestTrue(TEXT("UE GAS lookup uses same ASC"), UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn) == F.ASC);
	TArray<URDCAAbilitySystemComponent*> Systems;
	Pawn->GetComponents(Systems);
	TestEqual(TEXT("Exactly one shared ASC"), Systems.Num(), 1);
	TestEqual(TEXT("Saved weak-point HP remains authoritative"), F.WeakPoint->GetCurrentHitPoints(), 6);
	TestEqual(TEXT("Exactly three Boss skills"), F.ASC->GetActivatableAbilities().Num(), 3);
	TestNull(TEXT("No accidental AI possession"), Pawn->GetController());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossPawnCompatibility, "RDCA.AI.BossPawn.LegacySchedulingCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBossPawnCompatibility::RunTest(const FString& Parameters)
{
	for (bool bPhase2 : { false, true })
	{
		TArray<int32> ActorChoices;
		for (bool bUseBossPawn : { false, true })
		{
			FBossGasWorld F(bUseBossPawn);
			SetBossFloat(F.Encounter, TEXT("RecoveryDuration"), 0.0f);
			SetBossFloat(F.Encounter, TEXT("Phase2InterAttackDelay"), 0.0f);
			F.Begin(bPhase2);
			F.Select();
			const int32 RoundLength = bPhase2 ? 3 : 2;
			for (int32 Attack = 0; Attack < RoundLength; ++Attack)
			{
				TestTrue(TEXT("One GAS attack starts"), F.ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Boss_Attacking));
				const int32 Key = static_cast<int32>(F.Encounter->GetCurrentAttack()) * 100
					+ static_cast<int32>(F.Encounter->GetCurrentBarragePattern());
				if (!bUseBossPawn) { ActorChoices.Add(Key); }
				else { TestEqual(TEXT("Same seed and rules produce same attack as Actor"), Key, ActorChoices[Attack]); }
				F.Step(F.Encounter->GetStateRemainingTime() + 0.01f);
				TestTrue(TEXT("Warning enters active phase"), F.Encounter->GetEncounterState() == EBossEncounterState::Attacking);
				for (int32 Step = 0; Step < 600 && F.Encounter->GetEncounterState() == EBossEncounterState::Attacking; ++Step)
				{
					F.Step(0.05f);
				}
				TestEqual(TEXT("Completion counts once"), F.Encounter->GetCompletedAttackCount(), Attack + 1);
				TestTrue(TEXT("Final attack immediately enters stun"),
					(F.Encounter->GetEncounterState() == EBossEncounterState::WeakPointExposed) == (Attack + 1 == RoundLength));
				if (Attack + 1 < RoundLength) { F.Select(); }
			}
			TestTrue(TEXT("Weak point exposed after the round"), F.WeakPoint->IsExposed());
			F.Encounter->StopEncounter();
			TestEqual(TEXT("Stopping clears owned projectiles and laser"), F.LiveAttackActors(), 0);
			TestFalse(TEXT("Stopping clears both wave collision rings"), F.AnyWaveCollision() || F.AnyWaveCollision(true));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossSavedVisualTree, "RDCA.AI.Standard.SavedVisualTreeAndController",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBossSavedVisualTree::RunTest(const FString& Parameters)
{
	auto* Tree = LoadObject<UBehaviorTree>(nullptr, TEXT("/Game/AI/BT_RiftDragonCombat.BT_RiftDragonCombat"));
	auto* BB = LoadObject<UBlackboardData>(nullptr, TEXT("/Game/AI/BB_RiftDragonCombat.BB_RiftDragonCombat"));
	UClass* ControllerClass = LoadClass<ARDCABossAIController>(nullptr, TEXT("/Game/AI/BP_RiftDragonAIController.BP_RiftDragonAIController_C"));
	if (!TestNotNull(TEXT("Saved visual tree loads"), Tree) || !TestNotNull(TEXT("Saved blackboard loads"), BB)
		|| !TestNotNull(TEXT("Saved controller Blueprint loads"), ControllerClass)) { return false; }
	TestTrue(TEXT("Tree references separate saved blackboard"), Tree->BlackboardAsset == BB);
	TestNotNull(TEXT("Asset contains editable visual graph"), Tree->BTGraph.Get());
	if (Tree->BTGraph) { TestEqual(TEXT("Graph has root and 14 main decision/task nodes"), Tree->BTGraph->Nodes.Num(), 15); }
	const auto* TreeProperty = FindFProperty<FObjectPropertyBase>(ControllerClass, TEXT("CombatBehaviorTree"));
	TestTrue(TEXT("Controller Blueprint persists real tree configuration"), TreeProperty->GetObjectPropertyValue_InContainer(ControllerClass->GetDefaultObject()) == Tree);
	for (const FBlackboardEntry& Entry : BB->Keys) { TestFalse(TEXT("Blackboard keys are not shared between Boss instances"), Entry.bInstanceSynced); }
	FBossGasWorld F(true, true);
	F.Begin();
	auto* AI = F.StartAI(Tree);
	for (int32 I = 0; I < 800 && !F.WeakPoint->IsExposed(); ++I) { F.StepAI(AI); }
	TestTrue(TEXT("Reloaded visual asset executes real round"), F.WeakPoint->IsExposed());
	TestEqual(TEXT("Saved tree counts only completed attacks"), F.Encounter->GetCompletedAttackCount(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossExplicitTarget, "RDCA.AI.Standard.BlackboardTargetActuallyAimsBarrage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBossExplicitTarget::RunTest(const FString& Parameters)
{
	FBossGasWorld F(true, true);
	F.Begin();
	F.Encounter->SetEncounterHold(true);
	auto* AI = F.StartAI();
	APawn* Original = AI->GetCombatTarget();
	APawn* Chosen = F.World->SpawnActor<APawn>();
	auto* Root = NewObject<USceneComponent>(Chosen);
	Chosen->AddInstanceComponent(Root); Chosen->SetRootComponent(Root); Root->RegisterComponent();
	Chosen->SetActorLocation(FVector(-1000.0f, 0.0f, 300.0f));
	AI->SetCombatTarget(Chosen);
	AI->ValidateCombatTarget();
	TestTrue(TEXT("Manual target is not overwritten by PlayerController zero"), AI->GetCombatTarget() == Chosen && Chosen != Original);
	TestTrue(TEXT("Blackboard contains chosen target"), AI->GetBlackboardComponent()->GetValueAsObject(RDCABossBlackboard::TargetActor) == Chosen);
	TestTrue(TEXT("Skill backend receives same target"), F.Encounter->GetAttackTargetPawn() == Chosen);
	F.Encounter->SetEncounterHold(false);
	for (int32 I = 0; I < 30 && F.LiveAttackActors() == 0; ++I) { F.StepAI(AI, 0.02f); }
	ABossFanProjectile* Shot = nullptr;
	for (TActorIterator<ABossFanProjectile> It(F.World); It; ++It) { if (It->GetOwner() == F.Boss) { Shot = *It; break; } }
	if (TestNotNull(TEXT("Real aimed projectile emitted"), Shot))
	{
		const FVector Toward = (Chosen->GetActorLocation() - Shot->GetActorLocation()).GetSafeNormal();
		TestTrue(TEXT("Projectile points at chosen target, opposite original player"), FVector::DotProduct(Shot->GetActorForwardVector(), Toward) > 0.9f);
	}
	Chosen->Destroy();
	TestNull(TEXT("Target destruction immediately clears blackboard, without service tick"), AI->GetBlackboardComponent()->GetValueAsObject(RDCABossBlackboard::TargetActor));
	for (int32 I = 0; I < 4; ++I) { F.StepAI(AI, 0.01f); }
	TestFalse(TEXT("Native blackboard observer aborts running skill"), F.Encounter->IsExecutingAttack());
	TestFalse(TEXT("Aborted task removes completion subscription"), F.Encounter->OnAttackFinished.IsBound());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossSharedTree, "RDCA.AI.Standard.SharedAssetIndependentNodeInstances",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBossSharedTree::RunTest(const FString& Parameters)
{
	auto* Tree = LoadObject<UBehaviorTree>(nullptr, TEXT("/Game/AI/BT_RiftDragonCombat.BT_RiftDragonCombat"));
	if (!TestNotNull(TEXT("Shared saved asset"), Tree)) { return false; }
	FBossGasWorld First(true, true), Second(true, true);
	SetBossBool(First.Encounter, TEXT("bDebugForceSweepLaser"), true);
	SetBossBool(Second.Encounter, TEXT("bDebugForceSweepLaser"), true);
	First.Begin(); Second.Begin();
	auto* A = First.StartAI(Tree); auto* B = Second.StartAI(Tree);
	for (int32 I = 0; I < 10 && (!First.Encounter->IsExecutingAttack() || !Second.Encounter->IsExecutingAttack()); ++I)
	{ First.StepAI(A, 0.01f); Second.StepAI(B, 0.01f); }
	TestTrue(TEXT("Both trees execute"), First.Encounter->IsExecutingAttack() && Second.Encounter->IsExecutingAttack());
	TestTrue(TEXT("Independent targets and blackboards"), A->GetCombatTarget() != B->GetCombatTarget() && A->GetBlackboardComponent() != B->GetBlackboardComponent());
	A->StopCombatBrain();
	TestFalse(TEXT("First skill cancelled"), First.Encounter->IsExecutingAttack());
	TestFalse(TEXT("First task callback removed"), First.Encounter->OnAttackFinished.IsBound());
	TestTrue(TEXT("Other Boss skill remains active"), Second.Encounter->IsExecutingAttack());
	for (int32 I = 0; I < 15; ++I) { Second.StepAI(B); }
	TestEqual(TEXT("Second completion delivered to its own node"), Second.Encounter->GetCompletedAttackCount(), 1);
	TestEqual(TEXT("First cancellation never counted"), First.Encounter->GetCompletedAttackCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossSchemaGuard, "RDCA.AI.Standard.SchemaRejectsSyncedTargetAndWrongEnum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBossSchemaGuard::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("Boss AI tree/blackboard is invalid"), EAutomationExpectedErrorFlags::Contains, 2);
	for (int32 Case = 0; Case < 2; ++Case)
	{
		FBossGasWorld F(true, true); F.Begin();
		auto* Tree = ARDCABossAIController::CreateDefaultCombatTree(F.Boss);
		for (FBlackboardEntry& Entry : Tree->BlackboardAsset->Keys)
		{
			if (Case == 0 && Entry.EntryName == RDCABossBlackboard::TargetActor) { Entry.bInstanceSynced = true; }
			if (Case == 1 && Entry.EntryName == RDCABossBlackboard::CombatPhase)
			{ CastChecked<UBlackboardKeyType_Enum>(Entry.KeyType)->EnumType = StaticEnum<EBossBarragePattern>(); }
		}
		auto* AI = F.StartAI(Tree);
		TestFalse(TEXT("Malformed blackboard cannot start production AI"), AI->IsCombatBrainRunning());
		F.Step(10.0f);
		TestFalse(TEXT("Malformed formal configuration remains suspended"), F.Encounter->IsExecutingAttack());
	}
	return true;
}

#endif
