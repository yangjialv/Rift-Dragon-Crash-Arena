#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/RDCAAbilitySystemComponent.h"
#include "Abilities/RDCAGameplayTags.h"
#include "Abilities/RDCAGroundDashAbility.h"
#include "Abilities/RDCAPhaseCrashAbility.h"
#include "Abilities/RDCAHealthAttributeSet.h"
#include "Abilities/RDCAHealthEffects.h"
#include "Boss/BossWeakPointComponent.h"
#include "Arena/AttachSurfaceComponent.h"
#include "Arena/ArenaFloorCollision.h"
#include "Combat/CrashResponseComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "Player/PlayerHealthComponent.h"
#include "Player/PhaseCrashComponent.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace
{
struct FGasTestWorld
{
	UWorld* World = nullptr;

	FGasTestWorld(bool bWithPhysics = false)
	{
		UWorld::InitializationValues Settings;
		Settings.AllowAudioPlayback(false).CreatePhysicsScene(bWithPhysics)
			.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
			true, ERHIFeatureLevel::Num, &Settings);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	}

	~FGasTestWorld()
	{
		if (World)
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
	}

	URDCAAbilitySystemComponent* CreateASC()
	{
		return URDCAAbilitySystemComponent::FindOrCreateForActor(World->SpawnActor<AActor>());
	}

	APawn* CreateDashPawn()
	{
		APawn* Pawn = World->SpawnActorDeferred<APawn>(APawn::StaticClass(), FTransform::Identity);
		Pawn->AutoPossessAI = EAutoPossessAI::Disabled;
		Pawn->FinishSpawning(FTransform::Identity);
		USphereComponent* Root = NewObject<USphereComponent>(Pawn);
		Pawn->AddInstanceComponent(Root);
		Pawn->SetRootComponent(Root);
		Root->SetSphereRadius(10.0f);
		Root->SetCollisionProfileName(TEXT("Pawn"));
		Root->SetCollisionEnabled(World->GetPhysicsScene() ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		Root->RegisterComponent();
		UPhaseCrashComponent* Motion = NewObject<UPhaseCrashComponent>(Pawn);
		Pawn->AddInstanceComponent(Motion);
		Motion->RegisterComponent();
		FindFProperty<FFloatProperty>(Motion->GetClass(), TEXT("GravityAcceleration"))->SetPropertyValue_InContainer(Motion, 0.0f);
		Pawn->DispatchBeginPlay();
		Pawn->FindComponentByClass<URDCAAbilitySystemComponent>()->ResetHealth(5.0f);
		return Pawn;
	}

	AActor* CreateAnchor(const FVector& Location)
	{
		AActor* Anchor = World->SpawnActor<AActor>();
		UAttachSurfaceComponent* Box = NewObject<UAttachSurfaceComponent>(Anchor);
		Anchor->AddInstanceComponent(Box);
		Anchor->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(20.0f, 100.0f, 100.0f));
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Box->SetWorldLocation(Location);
		Box->RegisterComponent();
		UCrashResponseComponent* Response = NewObject<UCrashResponseComponent>(Anchor);
		Anchor->AddInstanceComponent(Response);
		Response->RegisterComponent();
		FEnumProperty* Property = FindFProperty<FEnumProperty>(Response->GetClass(), TEXT("Response"));
		Property->GetUnderlyingProperty()->SetIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Response),
			static_cast<uint64>(ECrashCollisionResponse::Attach));
		return Anchor;
	}
};

/** Advance game time across real engine frames, so the TimerManager also advances. */
class FWaitForHitImmunity final : public IAutomationLatentCommand
{
public:
	FWaitForHitImmunity(FAutomationTestBase* InTest,
		TSharedRef<FGasTestWorld> InFixture, URDCAAbilitySystemComponent* InASC)
		: Test(InTest), Fixture(InFixture), ASC(InASC) { }

	virtual bool Update() override
	{
		Fixture->World->Tick(LEVELTICK_All, 0.1f);
		Elapsed += 0.1f;
		if (Elapsed < 1.3f)
		{
			return false;
		}
		Test->TestFalse(TEXT("Immunity expires using game time"), ASC->IsDamageImmune());
		Test->TestTrue(TEXT("Hit after expiration succeeds"), ASC->ApplyDamage(1.0f, nullptr, 1.0f));
		ASC->ResetHealth(5.0f);
		Test->TestFalse(TEXT("Restart clears active hit immunity"), ASC->IsDamageImmune());
		Test->TestTrue(TEXT("Can take damage after reset"), ASC->ApplyDamage(1.0f));
		return true;
	}

private:
	FAutomationTestBase* Test;
	TSharedRef<FGasTestWorld> Fixture;
	URDCAAbilitySystemComponent* ASC;
	float Elapsed = 0.0f;
};

class FWaitForBufferedDash final : public IAutomationLatentCommand
{
public:
	FWaitForBufferedDash(FAutomationTestBase* InTest, TSharedRef<FGasTestWorld> InFixture, APawn* InPawn)
		: Test(InTest), Fixture(InFixture), Pawn(InPawn) { }

	virtual bool Update() override
	{
		UPhaseCrashComponent* Motion = Pawn->FindComponentByClass<UPhaseCrashComponent>();
		URDCAAbilitySystemComponent* ASC = Pawn->FindComponentByClass<URDCAAbilitySystemComponent>();
		// TimeOnly advances world time without auto-ticking the component a second time.
		Fixture->World->Tick(LEVELTICK_TimeOnly, 0.05f);
		Motion->TickComponent(0.05f, LEVELTICK_All, nullptr);
		Fixture->World->GetTimerManager().Tick(0.05f);
		Elapsed += 0.05f;
		bSawRecovery |= Motion->GetCrashState() == EPhaseCrashState::Recovery;
		bSawCooldown |= ASC->IsGroundDashOnCooldown();
		if (Elapsed < 1.6f) { return false; }
		Test->TestTrue(TEXT("Motion passes through recovery"), bSawRecovery);
		Test->TestTrue(TEXT("Cooldown is owned by a GAS effect"), bSawCooldown);
		Test->TestTrue(TEXT("Buffered second dash executes once"), FMath::IsNearlyEqual(Pawn->GetActorLocation().X, 900.0f, 1.0f));
		Test->TestTrue(TEXT("Both dashes finish in Ready"), Motion->GetCrashState() == EPhaseCrashState::Ready);
		Test->TestFalse(TEXT("Action tag is cleaned up"), ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Action_Dashing));
		Test->TestFalse(TEXT("Cooldown expires"), ASC->IsGroundDashOnCooldown());
		return true;
	}

private:
	FAutomationTestBase* Test;
	TSharedRef<FGasTestWorld> Fixture;
	APawn* Pawn;
	float Elapsed = 0.0f;
	bool bSawRecovery = false;
	bool bSawCooldown = false;
};

class FWaitForBufferedJump final : public IAutomationLatentCommand
{
public:
	FWaitForBufferedJump(FAutomationTestBase* InTest, TSharedRef<FGasTestWorld> InFixture, APawn* InPawn)
		: Test(InTest), Fixture(InFixture), Pawn(InPawn) { }

	virtual bool Update() override
	{
		UPhaseCrashComponent* Motion = Pawn->FindComponentByClass<UPhaseCrashComponent>();
		URDCAAbilitySystemComponent* ASC = Pawn->FindComponentByClass<URDCAAbilitySystemComponent>();
		Fixture->World->Tick(LEVELTICK_TimeOnly, 0.05f);
		Motion->TickComponent(0.05f, LEVELTICK_All, nullptr);
		Fixture->World->GetTimerManager().Tick(0.05f);
		Elapsed += 0.05f;
		if (!bReleased && Motion->GetCrashState() == EPhaseCrashState::Charging)
		{
			Test->TestFalse(TEXT("Dash ownership ended before buffered jump"),
				ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Action_Dashing));
			Motion->ReleaseCrash();
			Test->TestTrue(TEXT("Buffered release starts jump motion"), Motion->GetMotionType() == EPhaseCrashMotionType::Jump);
			bReleased = true;
		}
		if (Elapsed < 1.8f) { return false; }
		Test->TestTrue(TEXT("Held left-click is consumed after dash cooldown"), bReleased);
		Test->TestTrue(TEXT("Buffered jump reaches its intended target"),
			FMath::IsNearlyEqual(Pawn->GetActorLocation().X, 1000.0f, 1.0f));
		Test->TestTrue(TEXT("Released buffer does not start another charge"), Motion->GetCrashState() == EPhaseCrashState::Ready);
		Test->TestFalse(TEXT("Jump action tag ends"), ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Action_PhaseCrash));
		Test->TestFalse(TEXT("Jump cooldown expires"), ASC->IsPhaseCrashOnCooldown());
		return true;
	}

private:
	FAutomationTestBase* Test;
	TSharedRef<FGasTestWorld> Fixture;
	APawn* Pawn;
	float Elapsed = 0.0f;
	bool bReleased = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRDCAHealthPipelineTest,
	"RDCA.GAS.Health.DamageDeathReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRDCAHealthPipelineTest::RunTest(const FString& Parameters)
{
	FGasTestWorld Fixture;
	URDCAAbilitySystemComponent* ASC = Fixture.CreateASC();
	if (!TestNotNull(TEXT("Actor owns an ASC"), ASC)) { return false; }
	TestTrue(TEXT("Creating it again reuses the same ASC"),
		ASC == URDCAAbilitySystemComponent::FindOrCreateForActor(ASC->GetOwner()));
	ASC->ResetHealth(12.0f);
	TestEqual(TEXT("Configured maximum is retained"), ASC->GetMaxHealth(), 12.0f);
	TestFalse(TEXT("Zero damage is rejected"), ASC->ApplyDamage(0.0f));
	TestFalse(TEXT("Negative damage is rejected"), ASC->ApplyDamage(-1.0f));
	TestTrue(TEXT("Damage is applied"), ASC->ApplyDamage(3.0f));
	TestEqual(TEXT("GAS contains the new health"), ASC->GetHealth(), 9.0f);
	TestTrue(TEXT("Lethal hit succeeds"), ASC->ApplyDamage(100.0f));
	TestEqual(TEXT("Overkill clamps to zero"), ASC->GetHealth(), 0.0f);
	TestTrue(TEXT("Death tag is present"), ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Dead));
	TestFalse(TEXT("Dead targets cannot be damaged again"), ASC->ApplyDamage(1.0f));
	ASC->ResetHealth(8.0f);
	TestEqual(TEXT("Reset restores new maximum"), ASC->GetHealth(), 8.0f);
	TestFalse(TEXT("Reset removes death"), ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Dead));
	ASC->SetNumericAttributeBase(URDCAHealthAttributeSet::GetHealthAttribute(), 50.0f);
	TestEqual(TEXT("Healing cannot exceed max"), ASC->GetHealth(), 8.0f);
	ASC->SetNumericAttributeBase(URDCAHealthAttributeSet::GetMaxHealthAttribute(), 4.0f);
	TestEqual(TEXT("Reducing maximum clamps current health"), ASC->GetHealth(), 4.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRDCAImmunityTest,
	"RDCA.GAS.Health.ImmunityAndExpiration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRDCAImmunityTest::RunTest(const FString& Parameters)
{
	TSharedRef<FGasTestWorld> Fixture = MakeShared<FGasTestWorld>();
	URDCAAbilitySystemComponent* ASC = Fixture->CreateASC();
	if (!TestNotNull(TEXT("ASC created"), ASC)) { return false; }
	ASC->ResetHealth(5.0f);
	TestTrue(TEXT("First hit succeeds"), ASC->ApplyDamage(1.0f, nullptr, 1.0f));
	TestTrue(TEXT("Timed effect grants immunity"), ASC->IsDamageImmune());
	TestFalse(TEXT("Immediate second hit is blocked"), ASC->ApplyDamage(1.0f));
	FGameplayEffectSpecHandle DirectDamage = ASC->MakeOutgoingSpec(
		URDCADamageEffect::StaticClass(), 1.0f, ASC->MakeEffectContext());
	DirectDamage.Data->SetSetByCallerMagnitude(RDCAGameplayTags::Data_Damage, -1.0f);
	ASC->ApplyGameplayEffectSpecToSelf(*DirectDamage.Data);
	TestEqual(TEXT("Direct GAS damage also respects immunity"), ASC->GetHealth(), 4.0f);
	FAutomationTestFramework::Get().EnqueueLatentCommand(
		MakeShared<FWaitForHitImmunity>(this, Fixture, ASC));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRDCAReentrantDamageTest,
	"RDCA.GAS.Health.ReentrantDamage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRDCAReentrantDamageTest::RunTest(const FString& Parameters)
{
	FGasTestWorld Fixture;
	URDCAAbilitySystemComponent* ASC = Fixture.CreateASC();
	if (!TestNotNull(TEXT("ASC created"), ASC)) { return false; }
	ASC->ResetHealth(5.0f);
	int32 Notifications = 0;
	bool bNestedHitAccepted = false;
	FDelegateHandle Handle = ASC->GetGameplayAttributeValueChangeDelegate(
		URDCAHealthAttributeSet::GetHealthAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Data)
		{
			if (Data.NewValue < Data.OldValue)
			{
				++Notifications;
				bNestedHitAccepted |= ASC->ApplyDamage(1.0f);
			}
		});
	ASC->ApplyDamage(1.0f, nullptr, 1.0f);
	ASC->GetGameplayAttributeValueChangeDelegate(URDCAHealthAttributeSet::GetHealthAttribute()).Remove(Handle);
	TestEqual(TEXT("Exactly one damage notification"), Notifications, 1);
	TestFalse(TEXT("A listener cannot recursively damage before immunity"), bNestedHitAccepted);
	TestEqual(TEXT("Only original hit reduces health"), ASC->GetHealth(), 4.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRDCALegacyHealthTest,
	"RDCA.GAS.Health.ExistingComponentCompatibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRDCALegacyHealthTest::RunTest(const FString& Parameters)
{
	FGasTestWorld Fixture;
	AActor* Player = Fixture.World->SpawnActor<AActor>();
	UPlayerHealthComponent* Health = NewObject<UPlayerHealthComponent>(Player);
	Player->AddInstanceComponent(Health);
	Health->RegisterComponent();
	FindFProperty<FIntProperty>(Health->GetClass(), TEXT("MaximumHealth"))->SetPropertyValue_InContainer(Health, 17);
	Health->ResetHealth();
	URDCAAbilitySystemComponent* PlayerASC = Player->FindComponentByClass<URDCAAbilitySystemComponent>();
	if (!TestNotNull(TEXT("Legacy health automatically finds or creates GAS"), PlayerASC)) { return false; }
	TestEqual(TEXT("Existing configurable player HP is preserved"), Health->GetCurrentHealth(), 17);
	Health->ReceiveDamage(2);
	TestEqual(TEXT("Old damage entry updates GAS"), PlayerASC->GetHealth(), 15.0f);
	TestEqual(TEXT("Old HUD query matches GAS"), Health->GetCurrentHealth(), 15);
	TestTrue(TEXT("Old immunity query reads GAS"), Health->IsInvulnerable());

	AActor* Boss = Fixture.World->SpawnActor<AActor>();
	UBossWeakPointComponent* WeakPoint = NewObject<UBossWeakPointComponent>(Boss);
	Boss->AddInstanceComponent(WeakPoint);
	Boss->SetRootComponent(WeakPoint);
	WeakPoint->RegisterComponent();
	FindFProperty<FIntProperty>(WeakPoint->GetClass(), TEXT("MaximumHitPoints"))->SetPropertyValue_InContainer(WeakPoint, 9);
	WeakPoint->ResetWeakPoint();
	URDCAAbilitySystemComponent* BossASC = Boss->FindComponentByClass<URDCAAbilitySystemComponent>();
	if (!TestNotNull(TEXT("Blueprint-only Boss gets GAS without reparenting"), BossASC)) { return false; }
	FHitResult Hit;
	WeakPoint->SetExposed(false);
	TestFalse(TEXT("Protected weak point rejects heavy crash"), WeakPoint->ReceiveCrash(nullptr, Hit, true));
	WeakPoint->SetExposed(true);
	TestFalse(TEXT("Exposed weak point rejects unqualified crash"), WeakPoint->ReceiveCrash(nullptr, Hit, false));
	TestTrue(TEXT("Qualified heavy crash deals damage"), WeakPoint->ReceiveCrash(nullptr, Hit, true));
	TestEqual(TEXT("Boss GAS HP has one damage"), BossASC->GetHealth(), 8.0f);
	TestEqual(TEXT("Existing Boss phase query matches GAS"), WeakPoint->GetCurrentHitPoints(), 8);
	TestEqual(TEXT("Existing configurable Boss HP is preserved"), WeakPoint->GetMaximumHitPoints(), 9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRDCADashLifecycleTest,
	"RDCA.GAS.Dash.LifecycleAndBuffer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRDCADashLifecycleTest::RunTest(const FString& Parameters)
{
	TSharedRef<FGasTestWorld> Fixture = MakeShared<FGasTestWorld>();
	APawn* Pawn = Fixture->CreateDashPawn();
	UPhaseCrashComponent* Motion = Pawn->FindComponentByClass<UPhaseCrashComponent>();
	URDCAAbilitySystemComponent* ASC = Pawn->FindComponentByClass<URDCAAbilitySystemComponent>();
	TestTrue(TEXT("Native dash ability is granted automatically"), ASC->FindAbilitySpecFromClass(URDCAGroundDashAbility::StaticClass()) != nullptr);
	TestTrue(TEXT("Press intent starts dash immediately"), Motion->RequestGroundDashAtLocation(FVector(1000.0f, 0.0f, 0.0f)));
	TestTrue(TEXT("3C motion is in Crashing state"), Motion->GetCrashState() == EPhaseCrashState::Crashing);
	TestTrue(TEXT("GAS action tag is active"), ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Action_Dashing));
	TestFalse(TEXT("Cooldown must not start during travel"), ASC->IsGroundDashOnCooldown());
	TestFalse(TEXT("Second press is buffered rather than overlapping"), Motion->RequestGroundDashAtLocation(FVector(900.0f, 0.0f, 0.0f)));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FWaitForBufferedDash>(this, Fixture, Pawn));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRDCADashCancelTest,
	"RDCA.GAS.Dash.InvalidTargetDeathAndRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRDCADashCancelTest::RunTest(const FString& Parameters)
{
	FGasTestWorld Fixture;
	APawn* Pawn = Fixture.CreateDashPawn();
	UPhaseCrashComponent* Motion = Pawn->FindComponentByClass<UPhaseCrashComponent>();
	URDCAAbilitySystemComponent* ASC = Pawn->FindComponentByClass<URDCAAbilitySystemComponent>();
	TestFalse(TEXT("Zero-distance request is rejected"), Motion->RequestGroundDashAtLocation(Pawn->GetActorLocation()));
	TestFalse(TEXT("Rejected intent does not own an action tag"), ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Action_Dashing));
	Pawn->DisableInput(nullptr);
	TestFalse(TEXT("Intro/result input lock blocks dash"), Motion->RequestGroundDashAtLocation(FVector(450.0f, 0.0f, 0.0f)));
	Pawn->EnableInput(nullptr);
	TestTrue(TEXT("Enabled player can dash"), Motion->RequestGroundDashAtLocation(FVector(450.0f, 0.0f, 0.0f)));
	ASC->ApplyDamage(100.0f);
	TestFalse(TEXT("Death cancels active dash"), ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Action_Dashing));
	TestTrue(TEXT("Cancelled dash no longer drives motion"), Motion->GetCrashState() == EPhaseCrashState::Ready);
	TestFalse(TEXT("Dead player cannot restart dash"), Motion->RequestGroundDashAtLocation(FVector(450.0f, 0.0f, 0.0f)));
	ASC->ResetHealth(5.0f);
	TestTrue(TEXT("Reset permits another dash"), Motion->RequestGroundDashAtLocation(FVector(450.0f, 0.0f, 0.0f)));
	Motion->ForceArenaRecovery(FVector(100.0f, 100.0f, 0.0f), 100.0f, 0.2f);
	TestFalse(TEXT("Arena recovery cancels dash ownership"), ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Action_Dashing));
	Motion->TickComponent(0.25f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Cancellation does not cancel the replacement rescue trajectory"),
		Pawn->GetActorLocation().Equals(FVector(100.0f, 100.0f, 0.0f), 1.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRDCADashZeroRecoveryTest,
	"RDCA.GAS.Dash.ConfiguredCooldownAndZeroRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRDCADashZeroRecoveryTest::RunTest(const FString& Parameters)
{
	FGasTestWorld Fixture;
	APawn* Pawn = Fixture.CreateDashPawn();
	UPhaseCrashComponent* Motion = Pawn->FindComponentByClass<UPhaseCrashComponent>();
	URDCAAbilitySystemComponent* ASC = Pawn->FindComponentByClass<URDCAAbilitySystemComponent>();
	FindFProperty<FFloatProperty>(Motion->GetClass(), TEXT("RecoveryDuration"))->SetPropertyValue_InContainer(Motion, 0.0f);
	FindFProperty<FFloatProperty>(Motion->GetClass(), TEXT("GroundDashCooldown"))->SetPropertyValue_InContainer(Motion, 0.7f);
	Motion->RequestGroundDashAtLocation(FVector(450.0f, 0.0f, 0.0f));
	Motion->TickComponent(0.2f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Zero recovery goes directly to cooldown"), Motion->GetCrashState() == EPhaseCrashState::Cooldown);
	TestTrue(TEXT("Saved dash cooldown is used instead of jump cooldown"), FMath::IsNearlyEqual(Motion->GetCooldownRemaining(), 0.7f));
	TestFalse(TEXT("Skill finishes while cooldown effect remains"), ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Action_Dashing));
	ASC->ResetHealth(5.0f);
	Motion->TickComponent(0.01f, LEVELTICK_All, nullptr);
	FindFProperty<FFloatProperty>(Motion->GetClass(), TEXT("GroundDashCooldown"))->SetPropertyValue_InContainer(Motion, 0.0f);
	Motion->RequestGroundDashAtLocation(FVector(900.0f, 0.0f, 0.0f));
	Motion->TickComponent(0.2f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Zero cooldown and recovery cannot get stuck"), Motion->GetCrashState() == EPhaseCrashState::Ready);
	TestFalse(TEXT("Zero duration creates no lingering cooldown"), ASC->IsGroundDashOnCooldown());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRDCADashCollisionTest,
	"RDCA.GAS.Dash.CollisionAndAttachment", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRDCADashCollisionTest::RunTest(const FString& Parameters)
{
	for (const bool bAttach : {false, true})
	{
		FGasTestWorld Fixture(true);
		APawn* Pawn = Fixture.CreateDashPawn();
		UPhaseCrashComponent* Motion = Pawn->FindComponentByClass<UPhaseCrashComponent>();
		URDCAAbilitySystemComponent* ASC = Pawn->FindComponentByClass<URDCAAbilitySystemComponent>();
		AActor* Obstacle = Fixture.World->SpawnActor<AActor>();
		UBoxComponent* Box = bAttach ? NewObject<UAttachSurfaceComponent>(Obstacle) : NewObject<UBoxComponent>(Obstacle);
		Obstacle->AddInstanceComponent(Box);
		Obstacle->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(20.0f, 100.0f, 100.0f));
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Box->SetWorldLocation(FVector(100.0f, 0.0f, 0.0f));
		Box->RegisterComponent();
		if (bAttach)
		{
			UCrashResponseComponent* Response = NewObject<UCrashResponseComponent>(Obstacle);
			Obstacle->AddInstanceComponent(Response);
			Response->RegisterComponent();
			FEnumProperty* Property = FindFProperty<FEnumProperty>(Response->GetClass(), TEXT("Response"));
			Property->GetUnderlyingProperty()->SetIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Response),
				static_cast<uint64>(ECrashCollisionResponse::Attach));
		}
		Motion->RequestGroundDashAtLocation(FVector(450.0f, 0.0f, 0.0f));
		Motion->TickComponent(0.2f, LEVELTICK_All, nullptr);
		TestTrue(TEXT("Swept dash cannot tunnel through obstacle"), Pawn->GetActorLocation().X < 100.0f);
		if (bAttach)
		{
			TestTrue(TEXT("Anchor impact preserves attachment"), Motion->IsAttached());
			TestFalse(TEXT("Attachment completes GAS dash"), ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Action_Dashing));
			TestFalse(TEXT("Attachment does not impose a dash cooldown"), ASC->IsGroundDashOnCooldown());
		}
		else
		{
			TestTrue(TEXT("Blocking impact enters recovery"), Motion->GetCrashState() == EPhaseCrashState::Recovery);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRDCACrashChargeTest,
	"RDCA.GAS.Crash.ShortClickAndChargedArc", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRDCACrashChargeTest::RunTest(const FString& Parameters)
{
	FGasTestWorld Fixture;
	APawn* Pawn = Fixture.CreateDashPawn();
	UPhaseCrashComponent* Motion = Pawn->FindComponentByClass<UPhaseCrashComponent>();
	URDCAAbilitySystemComponent* ASC = Pawn->FindComponentByClass<URDCAAbilitySystemComponent>();
	TestTrue(TEXT("PhaseCrash ability is auto-granted"), ASC->FindAbilitySpecFromClass(URDCAPhaseCrashAbility::StaticClass()) != nullptr);
	TestTrue(TEXT("Press immediately starts aiming"), Motion->RequestCrashChargeAtLocation(FVector(1000.0f, 0.0f, 0.0f)));
	TestTrue(TEXT("Charging tag mirrors character state"), ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Action_Charging));
	TArray<FVector> Points;
	FVector Landing;
	bool bHasTrajectory = false;
	TestTrue(TEXT("Preview exists without mouse movement"), Motion->GetAimPreview(Points, Landing, bHasTrajectory));
	TestTrue(TEXT("A short click already has a valid arc"), bHasTrajectory);
	if (!TestTrue(TEXT("Preview supplies an apex sample"), Points.Num() > 10)) { return false; }
	TestTrue(TEXT("Preview lands at input target"), Landing.Equals(FVector(1000.0f, 0.0f, 0.0f)));
	Motion->ReleaseCrash();
	TestTrue(TEXT("Same-frame release reaches the GAS wait task"), Motion->GetCrashState() == EPhaseCrashState::Crashing);
	TestTrue(TEXT("Ground launch is a jump"), Motion->GetMotionType() == EPhaseCrashMotionType::Jump);
	TestFalse(TEXT("Release clears charging tag"), ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Action_Charging));
	Motion->TickComponent(0.25f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Actual short-click apex matches preview"), Pawn->GetActorLocation().Equals(Points[10], 1.0f));
	const float ShortApex = Pawn->GetActorLocation().Z;
	Motion->TickComponent(0.25f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Cooldown does not overlap recovery"), ASC->IsPhaseCrashOnCooldown());
	Motion->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Jump cooldown is a GAS effect"), ASC->IsPhaseCrashOnCooldown());
	TestTrue(TEXT("Existing jump cooldown value is preserved"), FMath::IsNearlyEqual(Motion->GetCooldownRemaining(), 0.45f));
	ASC->ResetHealth(5.0f);
	Motion->TickComponent(0.01f, LEVELTICK_All, nullptr);
	Pawn->SetActorLocation(FVector::ZeroVector);
	Motion->RequestCrashChargeAtLocation(FVector(1000.0f, 0.0f, 0.0f));
	Motion->TickComponent(0.8f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Charge has its original cap"), FMath::IsNearlyEqual(Motion->GetChargeAlpha(), 1.0f));
	Motion->GetAimPreview(Points, Landing, bHasTrajectory);
	if (!TestTrue(TEXT("Charged preview supplies an apex sample"), Points.Num() > 10)) { return false; }
	Motion->ReleaseCrash();
	Motion->TickComponent(0.425f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Charged jump keeps target but raises apex"), Pawn->GetActorLocation().Z > ShortApex + 300.0f);
	TestTrue(TEXT("Charged actual apex still matches preview"), Pawn->GetActorLocation().Equals(Points[10], 1.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRDCACrashCancelTest,
	"RDCA.GAS.Crash.CancelDeathAndRescue", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRDCACrashCancelTest::RunTest(const FString& Parameters)
{
	FGasTestWorld Fixture;
	APawn* Pawn = Fixture.CreateDashPawn();
	UPhaseCrashComponent* Motion = Pawn->FindComponentByClass<UPhaseCrashComponent>();
	URDCAAbilitySystemComponent* ASC = Pawn->FindComponentByClass<URDCAAbilitySystemComponent>();
	Motion->RequestCrashChargeAtLocation(FVector(500.0f, 0.0f, 0.0f));
	Motion->CancelCharging();
	TestTrue(TEXT("Cancelled charge returns to Ready"), Motion->GetCrashState() == EPhaseCrashState::Ready);
	TestFalse(TEXT("Cancellation removes the active skill"), ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Action_PhaseCrash));
	TestFalse(TEXT("Cancellation does not consume cooldown"), ASC->IsPhaseCrashOnCooldown());
	Motion->ReleaseCrash();
	TestTrue(TEXT("Release after cancellation cannot launch"), Motion->GetCrashState() == EPhaseCrashState::Ready);
	Motion->RequestCrashChargeAtLocation(Pawn->GetActorLocation());
	Motion->ReleaseCrash();
	TestTrue(TEXT("Invalid trajectory ends cleanly"), Motion->GetCrashState() == EPhaseCrashState::Ready);
	Motion->RequestCrashChargeAtLocation(FVector(500.0f, 0.0f, 0.0f));
	Motion->ReleaseCrash();
	ASC->ApplyDamage(100.0f);
	TestFalse(TEXT("Death cancels flight skill"), ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Action_PhaseCrash));
	TestTrue(TEXT("Death stops the old trajectory"), Motion->GetMotionType() == EPhaseCrashMotionType::None);
	TestFalse(TEXT("Dead players cannot charge"), Motion->RequestCrashChargeAtLocation(FVector(500.0f, 0.0f, 0.0f)));
	ASC->ResetHealth(5.0f);
	Motion->RequestCrashChargeAtLocation(FVector(500.0f, 0.0f, 0.0f));
	Motion->ForceArenaRecovery(FVector(100.0f, 100.0f, 0.0f), 100.0f, 0.2f);
	TestFalse(TEXT("Rescue cancels charging task"), ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Action_Charging));
	TestTrue(TEXT("Rescue is not misidentified as a dash"), Motion->GetMotionType() == EPhaseCrashMotionType::ArenaRecovery && !Motion->IsGroundDashActive());
	Motion->TickComponent(0.25f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Skill cancellation preserves replacement rescue"), Pawn->GetActorLocation().Equals(FVector(100.0f, 100.0f, 0.0f), 1.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRDCACrashBufferTest,
	"RDCA.GAS.Crash.BufferedJumpAfterDash", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRDCACrashBufferTest::RunTest(const FString& Parameters)
{
	TSharedRef<FGasTestWorld> Fixture = MakeShared<FGasTestWorld>();
	APawn* Pawn = Fixture->CreateDashPawn();
	UPhaseCrashComponent* Motion = Pawn->FindComponentByClass<UPhaseCrashComponent>();
	Motion->RequestGroundDashAtLocation(FVector(450.0f, 0.0f, 0.0f));
	TestFalse(TEXT("Held jump request buffers during dash"), Motion->RequestCrashChargeAtLocation(FVector(1000.0f, 0.0f, 0.0f)));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FWaitForBufferedJump>(this, Fixture, Pawn));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRDCACrashBossDirectTest,
	"RDCA.GAS.Crash.AttachmentBossDirectAndWeakPoint", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRDCACrashBossDirectTest::RunTest(const FString& Parameters)
{
	for (const bool bExposed : {false, true})
	{
		FGasTestWorld Fixture(true);
		APawn* Pawn = Fixture.CreateDashPawn();
		UPhaseCrashComponent* Motion = Pawn->FindComponentByClass<UPhaseCrashComponent>();
		URDCAAbilitySystemComponent* ASC = Pawn->FindComponentByClass<URDCAAbilitySystemComponent>();
		AActor* Anchor = Fixture.CreateAnchor(FVector(-100.0f, 0.0f, 0.0f));
		Motion->RequestGroundDashAtLocation(FVector(-450.0f, 0.0f, 0.0f));
		Motion->TickComponent(0.2f, LEVELTICK_All, nullptr);
		TestTrue(TEXT("Player is attached before aiming"), Motion->IsAttached());
		Motion->RequestCrashChargeAtLocation(FVector(300.0f, 100.0f, 0.0f));
		TestTrue(TEXT("Attachment and charging coexist"), ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Movement_Attached)
			&& ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Action_Charging));
		Motion->CancelCharging();
		TestTrue(TEXT("Cancel keeps the original Anchor attachment"), Motion->IsAttached() && IsValid(Anchor));
		TestFalse(TEXT("Cancel removes charging but not attachment"), ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Action_Charging));
		AActor* Boss = Fixture.World->SpawnActor<AActor>();
		UBossWeakPointComponent* WeakPoint = NewObject<UBossWeakPointComponent>(Boss);
		Boss->AddInstanceComponent(WeakPoint);
		Boss->SetRootComponent(WeakPoint);
		WeakPoint->SetWorldLocation(FVector(1800.0f, 0.0f, 300.0f));
		WeakPoint->RegisterComponent();
		WeakPoint->ResetWeakPoint();
		WeakPoint->SetExposed(bExposed);
		Motion->RequestCrashChargeAtLocation(WeakPoint->GetComponentLocation(), Boss);
		TestTrue(TEXT("Explicit Boss target retains Boss aim semantics"), Motion->IsBossAimActive());
		Motion->ReleaseCrash();
		TestTrue(TEXT("Anchor Boss launch is straight-line motion"), Motion->GetMotionType() == EPhaseCrashMotionType::BossDirect);
		TestFalse(TEXT("Departure clears attached tag"), ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Movement_Attached));
		Motion->TickComponent(0.4f, LEVELTICK_All, nullptr);
		TestTrue(TEXT("Boss direct flight reaches beyond normal jump distance"), Pawn->GetActorLocation().X > 1600.0f);
		TestEqual(TEXT("Only exposed qualified crashes deal damage"), WeakPoint->GetCurrentHitPoints(), bExposed ? 4 : 5);
		TestTrue(TEXT("Both protected and effective hits rebound"), Motion->GetMotionType() == EPhaseCrashMotionType::Rebound);
		TestFalse(TEXT("Rebound is not rendered as a ground dash"), Motion->IsGroundDashActive());
		Motion->TickComponent(0.1f, LEVELTICK_All, nullptr);
		TestEqual(TEXT("One flight cannot damage the weak point twice"), WeakPoint->GetCurrentHitPoints(), bExposed ? 4 : 5);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRDCACrashLandingTest,
	"RDCA.GAS.Crash.ArenaLandingRefresh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRDCACrashLandingTest::RunTest(const FString& Parameters)
{
	FGasTestWorld Fixture(true);
	AArenaFloorCollision* Floor = Fixture.World->SpawnActor<AArenaFloorCollision>();
	Floor->DispatchBeginPlay();
	FHitResult FloorHit;
	Fixture.World->LineTraceSingleByChannel(FloorHit, FVector(1000.0f, 0.0f, 100.0f),
		FVector(1000.0f, 0.0f, -100.0f), ECC_Pawn);
	if (!TestTrue(TEXT("Fixture creates a physical arena floor"), FloorHit.GetActor() == Floor)) { return false; }
	APawn* Pawn = Fixture.CreateDashPawn();
	// Match the real player's five-unit floor clearance; exact contact can start a sweep penetrating.
	Pawn->SetActorLocation(FVector(1000.0f, 0.0f, 15.0f));
	UPhaseCrashComponent* Motion = Pawn->FindComponentByClass<UPhaseCrashComponent>();
	URDCAAbilitySystemComponent* ASC = Pawn->FindComponentByClass<URDCAAbilitySystemComponent>();
	FindFProperty<FFloatProperty>(Motion->GetClass(), TEXT("GravityAcceleration"))->SetPropertyValue_InContainer(Motion, 2400.0f);
	Motion->RequestCrashChargeAtLocation(FVector(1300.0f, 0.0f, 15.0f));
	Motion->ReleaseCrash();
	Motion->TickComponent(0.15f, LEVELTICK_All, nullptr);
	AddInfo(FString::Printf(TEXT("First landing: position=%s state=%d floorHeight=%.2f"),
		*Pawn->GetActorLocation().ToString(), static_cast<int32>(Motion->GetCrashState()), FloorHit.ImpactPoint.Z));
	TestTrue(TEXT("Landing during recovery immediately refreshes jump"), Motion->GetCrashState() == EPhaseCrashState::Ready);
	TestFalse(TEXT("Early landing creates no delayed cooldown"), ASC->IsPhaseCrashOnCooldown());
	TestFalse(TEXT("Landing ends the active skill"), ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Action_PhaseCrash));
	Motion->RequestCrashChargeAtLocation(FVector(1500.0f, 0.0f, 500.0f));
	Motion->ReleaseCrash();
	Motion->TickComponent(0.3f, LEVELTICK_All, nullptr);
	Motion->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("A flight ending above ground enters GAS cooldown"), ASC->IsPhaseCrashOnCooldown());
	Pawn->SetActorLocation(FVector(1500.0f, 0.0f, 15.0f));
	Motion->TickComponent(0.01f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Later real floor collision refreshes movement"), Motion->GetCrashState() == EPhaseCrashState::Ready);
	TestFalse(TEXT("Landing removes the GAS cooldown effect"), ASC->IsPhaseCrashOnCooldown());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRDCACrashZeroArcTest,
	"RDCA.GAS.Crash.ZeroArcAndZeroRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRDCACrashZeroArcTest::RunTest(const FString& Parameters)
{
	FGasTestWorld Fixture;
	APawn* Pawn = Fixture.CreateDashPawn();
	UPhaseCrashComponent* Motion = Pawn->FindComponentByClass<UPhaseCrashComponent>();
	URDCAAbilitySystemComponent* ASC = Pawn->FindComponentByClass<URDCAAbilitySystemComponent>();
	for (const TCHAR* Property : {TEXT("MinArcHeight"), TEXT("MaxArcHeight"), TEXT("RecoveryDuration")})
	{
		FindFProperty<FFloatProperty>(Motion->GetClass(), Property)->SetPropertyValue_InContainer(Motion, 0.0f);
	}
	FindFProperty<FFloatProperty>(Motion->GetClass(), TEXT("CooldownDuration"))->SetPropertyValue_InContainer(Motion, 0.65f);
	Motion->RequestCrashChargeAtLocation(FVector(400.0f, 0.0f, 0.0f));
	Motion->ReleaseCrash();
	TestTrue(TEXT("Zero-height jump has explicit Jump motion type"), Motion->GetMotionType() == EPhaseCrashMotionType::Jump);
	TestFalse(TEXT("Zero height does not turn jump into dash"), Motion->IsGroundDashActive());
	Motion->TickComponent(0.25f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Zero recovery uses configured jump cooldown"), FMath::IsNearlyEqual(Motion->GetCooldownRemaining(), 0.65f));
	ASC->ResetHealth(5.0f);
	Motion->TickComponent(0.01f, LEVELTICK_All, nullptr);
	FindFProperty<FFloatProperty>(Motion->GetClass(), TEXT("CooldownDuration"))->SetPropertyValue_InContainer(Motion, 0.0f);
	Motion->RequestCrashChargeAtLocation(FVector(800.0f, 0.0f, 0.0f));
	Motion->ReleaseCrash();
	Motion->TickComponent(0.25f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Zero recovery/cooldown returns to Ready"), Motion->GetCrashState() == EPhaseCrashState::Ready);
	TestFalse(TEXT("No jump cooldown remains"), ASC->IsPhaseCrashOnCooldown());
	return true;
}

#endif
