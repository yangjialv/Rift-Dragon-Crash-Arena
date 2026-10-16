#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/RDCAAbilitySystemComponent.h"
#include "Abilities/RDCAGameplayTags.h"
#include "Abilities/RDCAPresentation.h"
#include "Abilities/RDCAPresentationComponent.h"
#include "Arena/ArenaCombatBounds.h"
#include "Arena/AttachSurfaceComponent.h"
#include "Boss/BossEncounterComponent.h"
#include "Camera/CameraComponent.h"
#include "Combat/CrashResponseComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "Misc/AutomationTest.h"
#include "NiagaraComponent.h"
#include "Player/PlayerHealthComponent.h"
#include "Player/PhaseCrashComponent.h"
#include "Player/PlayerCorePawn.h"
#include "Player/RDCACombatCameraComponent.h"
#include "Player/RDCAPlayerControlComponent.h"
#include "Player/RDCASlimePresentationComponent.h"
#include "UObject/UnrealType.h"

namespace
{
struct FThreeCWorld
{
	UWorld* World;
	APlayerCorePawn* Pawn;
	UPhaseCrashComponent* Motion;
	URDCAAbilitySystemComponent* ASC;
	URDCAPlayerControlComponent* Control;
	URDCASlimePresentationComponent* Presentation;
	URDCACombatCameraComponent* Camera;

	FThreeCWorld(bool bPhysics = false)
	{
		UWorld::InitializationValues Settings;
		Settings.AllowAudioPlayback(false).CreatePhysicsScene(bPhysics).CreateNavigation(false)
			.CreateAISystem(false).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
			true, ERHIFeatureLevel::Num, &Settings);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		Pawn = World->SpawnActorDeferred<APlayerCorePawn>(APlayerCorePawn::StaticClass(), FTransform::Identity);
		Pawn->AutoPossessAI = EAutoPossessAI::Disabled;
		Pawn->FinishSpawning(FTransform::Identity);
		Motion = Pawn->FindComponentByClass<UPhaseCrashComponent>();
		FindFProperty<FFloatProperty>(Motion->GetClass(), TEXT("GravityAcceleration"))->SetPropertyValue_InContainer(Motion, 0.0f);
		Pawn->DispatchBeginPlay();
		ASC = Pawn->FindComponentByClass<URDCAAbilitySystemComponent>();
		Control = Pawn->FindComponentByClass<URDCAPlayerControlComponent>();
		Presentation = Pawn->FindComponentByClass<URDCASlimePresentationComponent>();
		Camera = Pawn->FindComponentByClass<URDCACombatCameraComponent>();
	}
	~FThreeCWorld()
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	AActor* CreateBoss()
	{
		AActor* Boss = World->SpawnActor<AActor>();
		USceneComponent* Root = NewObject<USceneComponent>(Boss);
		Boss->AddInstanceComponent(Root);
		Boss->SetRootComponent(Root);
		Root->RegisterComponent();
		Boss->SetActorLocation(FVector(0.0f, 0.0f, 300.0f));
		UBossEncounterComponent* Encounter = NewObject<UBossEncounterComponent>(Boss);
		Boss->AddInstanceComponent(Encounter);
		Encounter->RegisterComponent();
		// Camera target only; do not begin an unconfigured encounter.
		return Boss;
	}
	UStaticMeshComponent* CreatePillar(bool bComponentTag)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Actor);
		Actor->AddInstanceComponent(Mesh);
		Actor->SetRootComponent(Mesh);
		Mesh->RegisterComponent();
		Actor->SetActorLocation(FVector(1500.0f, 0.0f, 100.0f));
		if (bComponentTag) { Mesh->ComponentTags.Add(TEXT("PhaseMap_Pillar_01")); }
		else { Actor->Tags.Add(TEXT("PhaseMap_Pillar_02")); }
		return Mesh;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThreeCComposition, "RDCA.ThreeC.CompositionAndPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThreeCComposition::RunTest(const FString& Parameters)
{
	FThreeCWorld F;
	TestNotNull(TEXT("Native control component"), F.Control);
	TestNotNull(TEXT("Native presentation component"), F.Presentation);
	TestNotNull(TEXT("Native camera component"), F.Camera);
	for (const FName Name : {FName(TEXT("NearCameraPoint")), FName(TEXT("FarCameraPoint")), FName(TEXT("MoveSpeed")), FName(TEXT("ModelBottomLocalAxis"))})
	{
		TestNotNull(TEXT("Saved tuning remains on original Pawn class"), FindFProperty<FProperty>(F.Pawn->GetClass(), Name));
	}
	const FVector BodyScale = F.Pawn->GetActorScale3D();
	const float Health = F.ASC->GetHealth();
	F.Presentation->UpdateSlimePresentation(0.1f);
	TestTrue(TEXT("Idle remains settled"), F.Pawn->GetSlimeState() == EPlayerSlimeState::Idle);
	F.Pawn->SetActorLocation(FVector(20.0f, 0.0f, 0.0f));
	F.Presentation->UpdateSlimePresentation(0.1f);
	TestTrue(TEXT("Displacement produces Moving state"), F.Pawn->GetSlimeState() == EPlayerSlimeState::Moving);
	TestTrue(TEXT("Old getter forwards component state"), F.Pawn->GetSlimeState() == F.Presentation->GetSlimeState());
	TestTrue(TEXT("Visual update cannot resize collision"), F.Pawn->GetActorScale3D().Equals(BodyScale));
	TestTrue(TEXT("Visual update cannot move Pawn"), F.Pawn->GetActorLocation().Equals(FVector(20.0f, 0.0f, 0.0f)));
	TestEqual(TEXT("Visual update cannot damage player"), F.ASC->GetHealth(), Health);
	F.Motion->RequestCrashChargeAtLocation(FVector(600.0f, 0.0f, 0.0f));
	F.Presentation->UpdateSlimePresentation(0.1f);
	TestTrue(TEXT("Presentation reads actual charging motion"), F.Pawn->GetSlimeState() == EPlayerSlimeState::Charging);
	F.Motion->ReleaseCrash();
	F.Presentation->UpdateSlimePresentation(0.1f);
	TestTrue(TEXT("Jump reads Airborne"), F.Pawn->GetSlimeState() == EPlayerSlimeState::Airborne);
	F.Control->ResetControl();
	F.Motion->RequestGroundDashAtLocation(FVector(1000.0f, 0.0f, 0.0f));
	F.Presentation->UpdateSlimePresentation(0.1f);
	TestTrue(TEXT("Dash reads Dashing"), F.Pawn->GetSlimeState() == EPlayerSlimeState::Dashing);
	F.Motion->ForceArenaRecovery(FVector(200.0f, 0.0f, 0.0f), 0.0f, 0.5f);
	F.Presentation->UpdateSlimePresentation(0.1f);
	TestTrue(TEXT("Straight rescue is not misidentified as dash"), F.Pawn->GetSlimeState() == EPlayerSlimeState::Airborne);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThreeCControl, "RDCA.ThreeC.InputBindingsAndCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThreeCControl::RunTest(const FString& Parameters)
{
	FThreeCWorld F;
	for (const FName Name : {FName(TEXT("MoveAction")), FName(TEXT("CrashAction")), FName(TEXT("DashAction"))})
	{
		FindFProperty<FObjectPropertyBase>(F.Pawn->GetClass(), Name)->SetObjectPropertyValue_InContainer(F.Pawn, NewObject<UInputAction>(F.Pawn));
	}
	UEnhancedInputComponent* Input = NewObject<UEnhancedInputComponent>(F.Pawn);
	Input->RegisterComponent();
	UInputAction* ExternalAction = NewObject<UInputAction>(F.Pawn);
	Input->BindAction(ExternalAction, ETriggerEvent::Started, F.Motion, &UPhaseCrashComponent::CancelCharging);
	F.Control->BindInputs(Input);
	TestEqual(TEXT("Five gameplay bindings plus unrelated binding"), Input->GetActionEventBindings().Num(), 6);
	F.Control->BindInputs(Input);
	TestEqual(TEXT("Rebinding never doubles input"), Input->GetActionEventBindings().Num(), 6);
	F.Motion->RequestCrashChargeAtLocation(FVector(500.0f, 0.0f, 0.0f));
	F.Motion->RequestGroundDashAtLocation(FVector(800.0f, 0.0f, 0.0f)); // buffered during charge
	F.Control->ResetControl();
	TestEqual(TEXT("Reset preserves unrelated bindings"), Input->GetActionEventBindings().Num(), 1);
	TestFalse(TEXT("Reset cancels held skill"), F.ASC->HasMatchingGameplayTag(RDCAGameplayTags::State_Action_PhaseCrash));
	F.Motion->TickComponent(2.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Reset clears buffered dash"), F.Motion->GetCrashState() == EPhaseCrashState::Ready);
	TestTrue(TEXT("No ghost movement after reset"), F.Pawn->GetActorLocation().IsNearlyZero());
	APlayerController* Controller = F.World->SpawnActor<APlayerController>();
	F.World->AddController(Controller);
	Controller->Possess(F.Pawn);
	F.Pawn->FindComponentByClass<UCameraComponent>()->SetWorldRotation(FRotator(0.0f, 90.0f, 0.0f));
	F.Control->SubmitMove(FVector2D(0.0f, 1.0f));
	TestTrue(TEXT("Forward input follows camera, not world X"), F.Pawn->ConsumeMovementInputVector().Y > 0.9f);
	F.Pawn->DisableInput(Controller);
	F.Control->SubmitMove(FVector2D(1.0f, 1.0f));
	TestTrue(TEXT("Disabled input cannot submit movement"), F.Pawn->ConsumeMovementInputVector().IsNearlyZero());
	F.Pawn->EnableInput(Controller);
	F.ASC->ApplyDamage(100.0f);
	F.Control->SubmitMove(FVector2D(1.0f, 1.0f));
	TestTrue(TEXT("Dead player cannot submit movement"), F.Pawn->ConsumeMovementInputVector().IsNearlyZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThreeCCamera, "RDCA.ThreeC.CameraEndpointsAndIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThreeCCamera::RunTest(const FString& Parameters)
{
	FThreeCWorld F;
	AActor* Boss = F.CreateBoss();
	USpringArmComponent* Arm = F.Pawn->FindComponentByClass<USpringArmComponent>();
	FCombatCameraPoint* Near = FindFProperty<FStructProperty>(F.Pawn->GetClass(), TEXT("NearCameraPoint"))->ContainerPtrToValuePtr<FCombatCameraPoint>(F.Pawn);
	FCombatCameraPoint* Far = FindFProperty<FStructProperty>(F.Pawn->GetClass(), TEXT("FarCameraPoint"))->ContainerPtrToValuePtr<FCombatCameraPoint>(F.Pawn);
	Near->ArmLength = 900.0f;
	Far->ArmLength = 4200.0f;
	F.Pawn->SetActorLocation(FVector(450.0f, 0.0f, 0.0f));
	F.Camera->UpdateCombatCamera(1.0f);
	TestTrue(TEXT("Reads original near tuning"), FMath::IsNearlyEqual(Arm->TargetArmLength, 900.0f));
	TestTrue(TEXT("Near pitch preserved"), FMath::IsNearlyEqual(Arm->GetComponentRotation().Pitch, Near->Pitch));
	F.Pawn->SetActorLocation(FVector(3000.0f, 0.0f, 0.0f));
	F.Camera->UpdateCombatCamera(0.01f);
	TestTrue(TEXT("Distance change is interpolated, not instant"), Arm->TargetArmLength > 900.0f && Arm->TargetArmLength < 4200.0f);
	F.Camera->UpdateCombatCamera(1.0f);
	TestTrue(TEXT("Reads original far tuning"), FMath::IsNearlyEqual(Arm->TargetArmLength, 4200.0f));
	TestTrue(TEXT("Camera cannot move player"), F.Pawn->GetActorLocation().Equals(FVector(3000.0f, 0.0f, 0.0f)));
	TestTrue(TEXT("Camera cannot activate player skills"), F.Motion->GetCrashState() == EPhaseCrashState::Ready);
	Boss->Destroy();
	F.Camera->UpdateCombatCamera(1.0f);
	TestTrue(TEXT("Lost Boss target uses stable near fallback"), FMath::IsNearlyEqual(Arm->TargetArmLength, 900.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThreeCOcclusion, "RDCA.ThreeC.OcclusionAndCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThreeCOcclusion::RunTest(const FString& Parameters)
{
	FThreeCWorld F;
	F.CreateBoss();
	F.Pawn->SetActorLocation(FVector(3000.0f, 0.0f, 0.0f));
	for (const FName Name : {FName(TEXT("CenterHiddenArcDegrees")), FName(TEXT("EdgeHiddenArcDegrees"))})
	{
		FindFProperty<FFloatProperty>(F.Pawn->GetClass(), Name)->SetPropertyValue_InContainer(F.Pawn, 360.0f);
	}
	UStaticMeshComponent* ComponentTagged = F.CreatePillar(true);
	UStaticMeshComponent* ActorTagged = F.CreatePillar(false);
	UStaticMeshComponent* AlreadyInvisible = F.CreatePillar(true);
	AlreadyInvisible->SetVisibility(false);
	UStaticMeshComponent* PhaseHidden = F.CreatePillar(false);
	PhaseHidden->GetOwner()->SetActorHiddenInGame(true);
	F.Camera->UpdateCombatCamera(1.0f);
	TestFalse(TEXT("Component-tagged pillar hidden"), ComponentTagged->IsVisible());
	TestFalse(TEXT("Actor-tagged pillar hidden"), ActorTagged->IsVisible());
	TestTrue(TEXT("Phase actor hidden flag not overwritten"), PhaseHidden->GetOwner()->IsHidden());
	FindFProperty<FBoolProperty>(F.Pawn->GetClass(), TEXT("bHideCameraOccluderPillars"))->SetPropertyValue_InContainer(F.Pawn, false);
	F.Camera->UpdateCombatCamera(1.0f);
	TestTrue(TEXT("Disabling restores camera-hidden meshes"), ComponentTagged->IsVisible() && ActorTagged->IsVisible());
	TestFalse(TEXT("Originally invisible mesh stays invisible"), AlreadyInvisible->IsVisible());
	FindFProperty<FBoolProperty>(F.Pawn->GetClass(), TEXT("bHideCameraOccluderPillars"))->SetPropertyValue_InContainer(F.Pawn, true);
	F.Camera->UpdateCombatCamera(1.0f);
	F.Camera->DestroyComponent();
	TestTrue(TEXT("EndPlay restores owned visibility changes"), ComponentTagged->IsVisible() && ActorTagged->IsVisible());
	TestTrue(TEXT("EndPlay never shows phase-hidden actor"), PhaseHidden->GetOwner()->IsHidden());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThreeCAttachment, "RDCA.ThreeC.AttachedBottomAndIntent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThreeCAttachment::RunTest(const FString& Parameters)
{
	FThreeCWorld F(true);
	AActor* Anchor = F.World->SpawnActor<AActor>();
	UAttachSurfaceComponent* Box = NewObject<UAttachSurfaceComponent>(Anchor);
	Anchor->AddInstanceComponent(Box);
	Anchor->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(20.0f, 200.0f, 200.0f));
	Box->SetCollisionProfileName(TEXT("BlockAll"));
	Box->SetWorldLocation(FVector(250.0f, 0.0f, 0.0f));
	Box->RegisterComponent();
	UCrashResponseComponent* Response = NewObject<UCrashResponseComponent>(Anchor);
	Anchor->AddInstanceComponent(Response);
	Response->RegisterComponent();
	FEnumProperty* Property = FindFProperty<FEnumProperty>(Response->GetClass(), TEXT("Response"));
	Property->GetUnderlyingProperty()->SetIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Response), static_cast<uint64>(ECrashCollisionResponse::Attach));
	F.Motion->RequestGroundDashAtLocation(FVector(1000.0f, 0.0f, 0.0f));
	F.Motion->TickComponent(0.1f, LEVELTICK_All, nullptr);
	if (!TestTrue(TEXT("Real collision attaches player"), F.Motion->IsAttached())) { return false; }
	F.Presentation->UpdateSlimePresentation(1.0f);
	TestTrue(TEXT("Attached display state"), F.Pawn->GetSlimeState() == EPlayerSlimeState::Attached);
	UStaticMeshComponent* Mesh = F.Pawn->FindComponentByClass<UStaticMeshComponent>();
	TestTrue(TEXT("Mesh bottom matches current support face"), FVector::DotProduct(Mesh->GetUpVector(), F.Motion->GetAttachedSurfaceNormal()) > 0.99f);
	const FVector Before = F.Pawn->GetActorLocation();
	F.World->Tick(LEVELTICK_TimeOnly, 0.016f); // Attached motion consumes the world's frame delta.
	F.Control->SubmitMove(FVector2D(0.0f, 1.0f));
	TestTrue(TEXT("Attached intent moves along current face"), !F.Pawn->GetActorLocation().Equals(Before));
	TestTrue(TEXT("Attached movement stays attached"), F.Motion->IsAttached());
	F.Motion->RequestCrashChargeAtLocation(FVector(0.0f, 500.0f, 0.0f));
	F.Presentation->UpdateSlimePresentation(1.0f);
	TestTrue(TEXT("Charging and attached support coexist"), F.Pawn->GetSlimeState() == EPlayerSlimeState::Charging && F.Motion->IsAttached());
	TestTrue(TEXT("Charging keeps bottom on current support"), FVector::DotProduct(Mesh->GetUpVector(), F.Motion->GetAttachedSurfaceNormal()) > 0.99f);
	FindFProperty<FFloatProperty>(F.Pawn->GetClass(), TEXT("VisualForwardYawOffset"))->SetPropertyValue_InContainer(F.Pawn, 180.0f);
	F.Presentation->UpdateSlimePresentation(1.0f);
	TestTrue(TEXT("Imported forward correction cannot flip wall support"), FVector::DotProduct(Mesh->GetUpVector(), F.Motion->GetAttachedSurfaceNormal()) > 0.99f);
	FindFProperty<FStructProperty>(F.Pawn->GetClass(), TEXT("ModelBottomLocalAxis"))->ContainerPtrToValuePtr<FVector>(F.Pawn)->Set(0.0f, 1.0f, 0.0f);
	F.Presentation->UpdateSlimePresentation(1.0f);
	const FVector ImportedUp = Mesh->GetComponentTransform().TransformVectorNoScale(FVector(0.0f, -1.0f, 0.0f)).GetSafeNormal();
	TestTrue(TEXT("Imported local Y bottom still aligns to support"), FVector::DotProduct(ImportedUp, F.Motion->GetAttachedSurfaceNormal()) > 0.99f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControlAimProjection, "RDCA.Control.AimRayProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FControlAimProjection::RunTest(const FString& Parameters)
{
	FThreeCWorld F(true);
	AArenaCombatBounds* Bounds = F.World->SpawnActor<AArenaCombatBounds>();
	Bounds->SetActorLocation(FVector(0.0f, 0.0f, 80.0f));
	FVector Target;
	const AActor* Actor = nullptr;
	TestTrue(TEXT("Ray projects without a mouse viewport"), F.Control->ResolveAimRay(FVector(50.0f, 60.0f, 1000.0f), FVector::DownVector, Target, Actor));
	TestTrue(TEXT("Preserves cursor XY and old arena reference height"), Target.Equals(FVector(50.0f, 60.0f, Bounds->GetArenaCenter().Z), 0.01f));
	AActor* Decoration = F.World->SpawnActor<AActor>();
	UBoxComponent* DecorBox = NewObject<UBoxComponent>(Decoration);
	Decoration->AddInstanceComponent(DecorBox); Decoration->SetRootComponent(DecorBox);
	DecorBox->SetBoxExtent(FVector(100.0f)); DecorBox->SetCollisionProfileName(TEXT("BlockAll"));
	DecorBox->SetWorldLocation(FVector(50.0f, 60.0f, 500.0f)); DecorBox->RegisterComponent();
	F.Control->ResolveAimRay(FVector(50.0f, 60.0f, 1000.0f), FVector::DownVector, Target, Actor);
	TestTrue(TEXT("Decoration cannot pull landing off the ground plane"), !Actor && FMath::IsNearlyEqual(Target.Z, Bounds->GetArenaCenter().Z));
	UCrashResponseComponent* Response = NewObject<UCrashResponseComponent>(Decoration);
	Decoration->AddInstanceComponent(Response); Response->RegisterComponent();
	F.Control->ResolveAimRay(FVector(50.0f, 60.0f, 1000.0f), FVector::DownVector, Target, Actor);
	TestTrue(TEXT("Combat target retains its real surface hit"), Actor == Decoration && Target.Z > 500.0f);
	TestFalse(TEXT("Horizontal ray cannot create a phantom landing"), F.Control->ResolveAimRay(FVector(1000.0f), FVector::ForwardVector, Target, Actor));
	TestFalse(TEXT("Backward ground intersection rejected"), F.Control->ResolveAimRay(FVector(1000.0f), FVector::UpVector, Target, Actor));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControlBufferedTargets, "RDCA.Control.BufferedWorldTargetsAndRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FControlBufferedTargets::RunTest(const FString& Parameters)
{
	FThreeCWorld F;
	for (const FName Name : {FName(TEXT("GroundDashCooldown")), FName(TEXT("RecoveryDuration"))})
	{
		FindFProperty<FFloatProperty>(F.Motion->GetClass(), Name)->SetPropertyValue_InContainer(F.Motion, 0.0f);
	}
	FindFProperty<FFloatProperty>(F.Motion->GetClass(), TEXT("DashInputBufferDuration"))->SetPropertyValue_InContainer(F.Motion, 1.0f);
	TestTrue(TEXT("Control starts explicit dash"), F.Control->RequestGroundDashAtLocation(FVector(400.0f, 0.0f, 0.0f)));
	TestFalse(TEXT("Charge queues while dash is busy"), F.Control->RequestCrashChargeAtLocation(FVector(800.0f, 0.0f, 0.0f)));
	F.Control->ReleaseCrash();
	F.Motion->TickComponent(0.25f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Early release removes queued charge"), F.Motion->GetCrashState() == EPhaseCrashState::Ready);
	TestTrue(TEXT("Only first dash moved player"), FMath::IsNearlyEqual(F.Pawn->GetActorLocation().X, 400.0f));
	F.Control->RequestGroundDashAtLocation(FVector(600.0f, 0.0f, 0.0f));
	F.Control->RequestGroundDashAtLocation(FVector(800.0f, 0.0f, 0.0f));
	F.Motion->TickComponent(0.15f, LEVELTICK_All, nullptr); // Must exceed the authored 0.12-second minimum flight.
	TestTrue(TEXT("Control retries queued dash after motion completes"), F.Motion->IsGroundDashActive());
	F.Motion->TickComponent(0.25f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Queued world target is retained, not reprojected"), FMath::IsNearlyEqual(F.Pawn->GetActorLocation().X, 800.0f));
	TestTrue(TEXT("One queue produces exactly one follow-up"), F.Motion->GetCrashState() == EPhaseCrashState::Ready);
	F.ASC->ApplyGroundDashCooldown(1.0f);
	F.Control->RequestGroundDashAtLocation(FVector(1000.0f, 0.0f, 0.0f));
	F.ASC->ResetHealth(5.0f); // No active ability exists during cooldown, so reset must clear Control explicitly.
	F.Motion->TickComponent(0.2f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Health reset clears even a cooldown-only pending input"), FMath::IsNearlyEqual(F.Pawn->GetActorLocation().X, 800.0f) && !F.Motion->IsGroundDashActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCueNativeDispatch, "RDCA.Cues.NativeDispatchAndTags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCueNativeDispatch::RunTest(const FString& Parameters)
{
	FThreeCWorld F;
	URDCAPresentationComponent* Presentation = URDCAPresentationComponent::FindOrCreate(F.Pawn);
	int32 Executions = 0;
	FGameplayTag LastTag;
	Presentation->OnCueHandled.AddLambda([&](FGameplayTag Tag, EGameplayCueEvent::Type Event)
	{
		if (Event == EGameplayCueEvent::Executed) { ++Executions; LastTag = Tag; }
	});
	for (int32 Index = 0; Index <= static_cast<int32>(ERDCAAudioCue::BossMusicMain); ++Index)
	{
		TestTrue(TEXT("Every approved audio event has native GameplayCue tag"), RDCAPresentation::GetAudioTag(static_cast<ERDCAAudioCue>(Index)).IsValid());
	}
	RDCAPresentation::PlayAtLocation(F.Motion, ERDCAAudioCue::PlayerDash, F.Pawn->GetActorLocation());
	TestEqual(TEXT("Native cue routes once even with audio disabled"), Executions, 1);
	TestTrue(TEXT("Correct event reaches notify"), LastTag == RDCAPresentation::GetAudioTag(ERDCAAudioCue::PlayerDash));
	RDCAPresentation::Play2D(F.Pawn, ERDCAAudioCue::Victory);
	TestEqual(TEXT("2D result also passes through cue manager"), Executions, 2);
	TestEqual(TEXT("Executed cues do not leave persistent records"), Presentation->GetActiveCueCount(), 0);
	TestEqual(TEXT("Presentation does not change HP"), F.ASC->GetHealth(), 5.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCueLifetime, "RDCA.Cues.PersistentIsolationAndCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCueLifetime::RunTest(const FString& Parameters)
{
	FThreeCWorld F;
	URDCAPresentationComponent* Presentation = URDCAPresentationComponent::FindOrCreate(F.Pawn);
	int32 Starts = 0, Stops = 0;
	Presentation->OnCueHandled.AddLambda([&](FGameplayTag Tag, EGameplayCueEvent::Type Event)
	{
		Starts += Event == EGameplayCueEvent::OnActive ? 1 : 0;
		Stops += Event == EGameplayCueEvent::Removed ? 1 : 0;
	});
	UAudioComponent* Loop = RDCAPresentation::SpawnLoopAttached(ERDCAAudioCue::AnchorOverloadLoop, F.Pawn->GetRootComponent());
	RDCAPresentation::SpawnLoopAttached(ERDCAAudioCue::AnchorOverloadLoop, F.Pawn->GetRootComponent());
	const FGameplayTag LoopTag = RDCAPresentation::GetAudioTag(ERDCAAudioCue::AnchorOverloadLoop);
	TestEqual(TEXT("Repeated start is idempotent even without audio device"), Starts, 1);
	TestEqual(TEXT("One persistent loop record"), Presentation->GetActiveCueCount(), 1);
	TestEqual(TEXT("One GAS local cue tag"), F.ASC->GetTagCount(LoopTag), 1);
	AActor* Other = F.World->SpawnActor<AActor>();
	Other->DispatchBeginPlay();
	RDCAPresentation::SpawnLoopAtLocation(Other, ERDCAAudioCue::AnchorOverloadLoop, FVector::ZeroVector);
	URDCAPresentationComponent* OtherPresentation = Other->FindComponentByClass<URDCAPresentationComponent>();
	RDCAPresentation::StopLoop(F.Pawn, ERDCAAudioCue::AnchorOverloadLoop, Loop, 0.0f);
	RDCAPresentation::StopLoop(F.Pawn, ERDCAAudioCue::AnchorOverloadLoop, Loop, 0.0f);
	TestEqual(TEXT("Stop runs exactly once"), Stops, 1);
	TestEqual(TEXT("Tag removed"), F.ASC->GetTagCount(LoopTag), 0);
	TestEqual(TEXT("Shared notify never stores another target's loop"), OtherPresentation->GetActiveCueCount(), 1);
	OtherPresentation->DestroyComponent();
	TestEqual(TEXT("EndPlay clears persistent presentation"), OtherPresentation->GetActiveCueCount(), 0);
	UNiagaraComponent* Flame = NewObject<UNiagaraComponent>(F.Pawn);
	F.Pawn->AddInstanceComponent(Flame); Flame->SetAutoActivate(false); Flame->RegisterComponent();
	RDCAPresentation::SetLaserFlame(Flame, true);
	RDCAPresentation::SetLaserFlame(Flame, true);
	TestEqual(TEXT("Flame has one lifecycle"), Presentation->GetActiveCueCount(), 1);
	Presentation->DestroyComponent();
	TestEqual(TEXT("Destroy cleans flame cue"), Presentation->GetActiveCueCount(), 0);
	TestEqual(TEXT("Every lifecycle start has one removal"), Starts, Stops);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCueFeedback, "RDCA.Cues.DamageFeedbackAndWaveVisual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCueFeedback::RunTest(const FString& Parameters)
{
	FThreeCWorld F;
	URDCAPresentationComponent* Presentation = URDCAPresentationComponent::FindOrCreate(F.Pawn);
	int32 DamageCues = 0;
	Presentation->OnCueHandled.AddLambda([&](FGameplayTag Tag, EGameplayCueEvent::Type Event)
	{
		if (Tag == RDCAPresentation::GetAudioTag(ERDCAAudioCue::PlayerDamaged) && Event == EGameplayCueEvent::Executed) { ++DamageCues; }
	});
	UPlayerHealthComponent* Health = F.Pawn->FindComponentByClass<UPlayerHealthComponent>();
	TestTrue(TEXT("Real damage succeeds"), Health->ReceiveDamage(1));
	TestEqual(TEXT("One accepted hit presents once"), DamageCues, 1);
	TestFalse(TEXT("Immunity still rejects repeated hit"), Health->ReceiveDamage(1));
	TestEqual(TEXT("Rejected damage emits no feedback"), DamageCues, 1);
	USceneComponent* Visual = NewObject<USceneComponent>(F.Pawn, TEXT("SecondaryShockwaveVisual"));
	F.Pawn->AddInstanceComponent(Visual); Visual->RegisterComponent();
	RDCAPresentation::SetShockwaveVisible(Visual, false);
	TestFalse(TEXT("Wave cue hides real visual"), Visual->IsVisible());
	RDCAPresentation::SetShockwaveVisible(Visual, true);
	TestTrue(TEXT("Wave cue shows real visual"), Visual->IsVisible());
	TestEqual(TEXT("Cosmetic wave changes never apply extra damage"), F.ASC->GetHealth(), 4.0f);
	return true;
}

#endif
