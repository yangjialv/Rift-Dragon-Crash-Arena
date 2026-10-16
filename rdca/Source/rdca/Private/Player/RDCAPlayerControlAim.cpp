#include "Player/RDCAPlayerControlComponent.h"

#include "Abilities/RDCAAbilitySystemComponent.h"
#include "Abilities/RDCAGameplayTags.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Arena/ArenaCombatBounds.h"
#include "Boss/BossEncounterComponent.h"
#include "Boss/BossWeakPointComponent.h"
#include "Combat/CrashResponseComponent.h"
#include "EngineUtils.h"
#include "Game/RDCAPlayerController.h"
#include "GameFramework/PawnMovementComponent.h"

namespace
{
bool IsDirectCrashAimTarget(const FHitResult& Hit)
{
	AActor* HitActor = Hit.GetActor();
	return IsValid(HitActor)
		&& (HitActor->FindComponentByClass<UCrashResponseComponent>()
			|| HitActor->FindComponentByClass<UBossWeakPointComponent>());
}

bool IsBossCrashAimTarget(const FHitResult& Hit)
{
	AActor* HitActor = Hit.GetActor();
	return (IsValid(HitActor)
			&& (HitActor->FindComponentByClass<UBossEncounterComponent>()
				|| HitActor->FindComponentByClass<UBossWeakPointComponent>()))
		|| Cast<UBossWeakPointComponent>(Hit.GetComponent()) != nullptr;
}

float GetArenaAimPlaneZ(const UWorld& World, const APawn& OwnerPawn)
{
	for (TActorIterator<AArenaCombatBounds> It(&World); It; ++It)
	{
		return It->GetArenaCenter().Z;
	}
	// Keeps older maps functional when they have not added Arena Combat Bounds.
	return OwnerPawn.GetActorLocation().Z;
}

}

URDCAPlayerControlComponent* URDCAPlayerControlComponent::FindOrCreateForPawn(APawn* InPawn)
{
	if (!IsValid(InPawn)) { return nullptr; }
	URDCAPlayerControlComponent* Control = InPawn->FindComponentByClass<URDCAPlayerControlComponent>();
	if (!Control)
	{
		Control = NewObject<URDCAPlayerControlComponent>(InPawn, TEXT("PlayerControl"));
		InPawn->AddInstanceComponent(Control);
		Control->RegisterComponent();
	}
	Control->InitializeControl();
	return Control;
}

void URDCAPlayerControlComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeControl();
}

void URDCAPlayerControlComponent::InitializeControl()
{
	if (!ResolvePawn()) { return; }
	Motion->OnMotionUpdated.RemoveAll(this);
	Motion->OnMotionUpdated.AddUObject(this, &ThisClass::HandleMotionUpdated);
	Motion->OnCrashStateChanged.AddUniqueDynamic(this, &ThisClass::HandleMotionStateChanged);
}

void URDCAPlayerControlComponent::ClearActionRequests()
{
	bCrashInputHeld = bCrashInputBuffered = bBufferedCrashHasWorldTarget = false;
	BufferedCrashTargetActor.Reset();
	DashInputBufferRemaining = 0.0f;
	bBufferedDashHasWorldTarget = false;
}

void URDCAPlayerControlComponent::NotifyChargeStarted(const bool bWorldSpaceAim)
{
	bAimUsesWorldTarget = bWorldSpaceAim;
	bCrashInputBuffered = bBufferedCrashHasWorldTarget = false;
}

void URDCAPlayerControlComponent::StartCrashCharge()
{
	if (!ResolvePawn() || !CanSubmitInput()) { return; }
	bCrashInputHeld = bCrashInputBuffered = true;
	bBufferedCrashHasWorldTarget = false;
	TryBeginCharge();
}

bool URDCAPlayerControlComponent::TryBeginCharge()
{
	if (!Motion->CanStartCrashCharge() || !UpdateCursorAim()) { return false; }
	return Motion->RequestCrashAim(Motion->AimTarget, Motion->CurrentAimActor.Get(), false);
}

bool URDCAPlayerControlComponent::RequestCrashChargeAtLocation(const FVector& Target, const AActor* Actor)
{
	if (!ResolvePawn() || !CanSubmitInput() || Target.ContainsNaN()) { return false; }
	bCrashInputHeld = bCrashInputBuffered = true;
	if (!Motion->CanStartCrashCharge() || (Motion->AbilitySystem && Motion->AbilitySystem->IsPhaseCrashOnCooldown()))
	{
		bBufferedCrashHasWorldTarget = true;
		BufferedCrashWorldTarget = Target;
		BufferedCrashTargetActor = Actor;
		return false;
	}
	return Motion->RequestCrashAim(Target, Actor, true);
}

void URDCAPlayerControlComponent::ReleaseCrash()
{
	bCrashInputHeld = false;
	if (!ResolvePawn() || Motion->CrashState != EPhaseCrashState::Charging)
	{
		bCrashInputBuffered = false;
		return;
	}
	if (!CanSubmitInput()) { CancelCrashCharge(); return; }
	if (!bAimUsesWorldTarget) { UpdateCursorAim(); }
	if (UPawnMovementComponent* Movement = OwnerPawn->GetMovementComponent()) { Movement->StopMovementImmediately(); }
	if (Motion->AbilitySystem)
	{
		FGameplayEventData Event;
		Event.EventTag = RDCAGameplayTags::Event_Player_PhaseCrash_Release;
		Event.Instigator = OwnerPawn;
		Motion->AbilitySystem->HandleGameplayEvent(Event.EventTag, &Event);
	}
}

void URDCAPlayerControlComponent::CancelCrashCharge()
{
	bCrashInputHeld = bCrashInputBuffered = false;
	if (!ResolvePawn()) { return; }
	Motion->bAimingAtBoss = false;
	if (Motion->CrashState == EPhaseCrashState::Charging && Motion->AbilitySystem)
	{
		FGameplayTagContainer Tags;
		Tags.AddTag(RDCAGameplayTags::Ability_Player_PhaseCrash);
		Motion->AbilitySystem->CancelAbilities(&Tags);
	}
	SetAimCursor();
}

void URDCAPlayerControlComponent::StartGroundDash()
{
	if (!ResolvePawn()) { return; }
	bBufferedDashHasWorldTarget = false;
	if (!CanSubmitInput()) { DashInputBufferRemaining = 0.0f; return; }
	if (!Motion->CanStartGroundDash() || (Motion->AbilitySystem && Motion->AbilitySystem->IsGroundDashOnCooldown()))
	{
		DashInputBufferRemaining = Motion->DashInputBufferDuration;
		return;
	}
	DashInputBufferRemaining = 0.0f;
	TryStartDash();
}

bool URDCAPlayerControlComponent::TryStartDash()
{
	return UpdateCursorAim() && RequestGroundDashAtLocation(Motion->AimTarget);
}

bool URDCAPlayerControlComponent::RequestGroundDashAtLocation(const FVector& Target)
{
	if (!ResolvePawn() || !Motion->AbilitySystem || !CanSubmitInput() || Target.ContainsNaN()
		|| FVector::DistSquared2D(Target, OwnerPawn->GetActorLocation()) <= UE_KINDA_SMALL_NUMBER
		|| Motion->GroundDashDistance <= 0.0f) { return false; }
	if (!Motion->CanStartGroundDash() || Motion->AbilitySystem->IsGroundDashOnCooldown())
	{
		DashInputBufferRemaining = Motion->DashInputBufferDuration;
		bBufferedDashHasWorldTarget = true;
		BufferedDashWorldTarget = Target;
		return false;
	}
	DashInputBufferRemaining = 0.0f;
	bBufferedDashHasWorldTarget = false;
	FGameplayEventData Event;
	Event.EventTag = RDCAGameplayTags::Event_Player_GroundDash;
	Event.Instigator = OwnerPawn;
	FGameplayAbilityTargetData_LocationInfo* Data = new FGameplayAbilityTargetData_LocationInfo();
	Data->TargetLocation.LiteralTransform = FTransform(Target);
	Event.TargetData.Add(Data);
	return Motion->AbilitySystem->HandleGameplayEvent(Event.EventTag, &Event) > 0 && Motion->IsGroundDashActive();
}

void URDCAPlayerControlComponent::ConsumeBufferedInputs(const float DeltaTime)
{
	if (!CanSubmitInput()) { ClearActionRequests(); return; }
	DashInputBufferRemaining = FMath::Max(DashInputBufferRemaining - DeltaTime, 0.0f);
	if (Motion->CrashState != EPhaseCrashState::Ready && Motion->CrashState != EPhaseCrashState::Attached) { return; }
	if (bCrashInputBuffered && bCrashInputHeld)
	{
		if (bBufferedCrashHasWorldTarget) { RequestCrashChargeAtLocation(BufferedCrashWorldTarget, BufferedCrashTargetActor.Get()); }
		else { TryBeginCharge(); }
		return;
	}
	if (DashInputBufferRemaining > 0.0f && (!Motion->AbilitySystem || !Motion->AbilitySystem->IsGroundDashOnCooldown()))
	{
		if (bBufferedDashHasWorldTarget) { RequestGroundDashAtLocation(BufferedDashWorldTarget); }
		else { StartGroundDash(); }
	}
}

void URDCAPlayerControlComponent::HandleMotionUpdated(const float DeltaTime)
{
	if (Motion->CrashState == EPhaseCrashState::Charging)
	{
		if (!bAimUsesWorldTarget) { UpdateCursorAim(); }
		SetAimCursor();
	}
	ConsumeBufferedInputs(DeltaTime);
}

void URDCAPlayerControlComponent::HandleMotionStateChanged(EPhaseCrashState Previous, EPhaseCrashState Next)
{
	SetAimCursor();
}

void URDCAPlayerControlComponent::SetAimCursor()
{
	if (ARDCAPlayerController* Controller = Cast<ARDCAPlayerController>(OwnerPawn ? OwnerPawn->GetController() : nullptr))
	{
		Controller->SetAimCursorState(Motion && Motion->CrashState == EPhaseCrashState::Charging
			? (Motion->IsBossAimActive() ? ERDCAAimCursorState::BossTarget : ERDCAAimCursorState::Charging)
			: ERDCAAimCursorState::Normal);
	}
}

bool URDCAPlayerControlComponent::UpdateCursorAim()
{
	const APlayerController* Controller = Cast<APlayerController>(OwnerPawn ? OwnerPawn->GetController() : nullptr);
	if (!Controller) { return false; }
	FVector Origin, Direction, Target;
	const AActor* TargetActor = nullptr;
	if (!Controller->DeprojectMousePositionToWorld(Origin, Direction) || !ResolveAimRay(Origin, Direction, Target, TargetActor)) { return false; }
	Motion->AimTarget = Target;
	Motion->CurrentAimActor = TargetActor;
	Motion->bAimingAtBoss = Motion->IsAttached() && IsValid(TargetActor)
		&& (TargetActor->FindComponentByClass<UBossEncounterComponent>() || TargetActor->FindComponentByClass<UBossWeakPointComponent>());
	return true;
}

bool URDCAPlayerControlComponent::ResolveAimRay(const FVector& Origin, const FVector& Direction,
	FVector& OutTarget, const AActor*& OutActor) const
{
	OutActor = nullptr;
	if (!OwnerPawn || !GetWorld() || Origin.ContainsNaN() || Direction.ContainsNaN() || Direction.IsNearlyZero()) { return false; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PlayerControlAim), false, OwnerPawn);
	TArray<FHitResult> Hits;
	if (GetWorld()->LineTraceMultiByChannel(Hits, Origin, Origin + Direction * 100000.0f, ECC_Visibility, Params))
	{
		for (const FHitResult& Hit : Hits)
		{
			if (IsDirectCrashAimTarget(Hit))
			{
				OutTarget = Hit.ImpactPoint;
				OutActor = Hit.GetActor();
				return true;
			}
		}
	}
	if (FMath::Abs(Direction.Z) <= UE_KINDA_SMALL_NUMBER) { return false; }
	const float Z = GetArenaAimPlaneZ(*GetWorld(), *OwnerPawn);
	const float Distance = (Z - Origin.Z) / Direction.Z;
	if (Distance <= 0.0f) { return false; }
	OutTarget = Origin + Direction * Distance;
	OutTarget.Z = Z;
	return true;
}
