#include "Player/RDCAPlayerControlComponent.h"

#include "Abilities/RDCAAbilitySystemComponent.h"
#include "Abilities/RDCAGameplayTags.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Player/PhaseCrashComponent.h"
#include "Player/PlayerCorePawn.h"
#include "Player/PlayerHealthComponent.h"
#include "rdca.h"

URDCAPlayerControlComponent::URDCAPlayerControlComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool URDCAPlayerControlComponent::ResolvePawn()
{
	if (!OwnerPawn) { OwnerPawn = Cast<APawn>(GetOwner()); }
	if (!Pawn) { Pawn = Cast<APlayerCorePawn>(OwnerPawn); }
	if (!Motion && OwnerPawn) { Motion = OwnerPawn->FindComponentByClass<UPhaseCrashComponent>(); }
	return IsValid(OwnerPawn) && IsValid(Motion);
}

bool URDCAPlayerControlComponent::CanSubmitInput() const
{
	return OwnerPawn && Motion && Motion->CanAcceptActionInput();
}

void URDCAPlayerControlComponent::RefreshMappingContext()
{
	if (!ResolvePawn() || !Pawn) { return; }
	const APlayerController* Controller = Cast<APlayerController>(Pawn->GetController());
	UEnhancedInputLocalPlayerSubsystem* Subsystem = Controller && Controller->GetLocalPlayer()
		? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer()) : nullptr;
	if (AppliedSubsystem.Get() == Subsystem && AppliedMapping.Get() == Pawn->PlayerMappingContext) { return; }
	RemoveMappingContext();
	if (Subsystem && Pawn->PlayerMappingContext && !Subsystem->HasMappingContext(Pawn->PlayerMappingContext))
	{
		Subsystem->AddMappingContext(Pawn->PlayerMappingContext, 0);
		AppliedSubsystem = Subsystem;
		AppliedMapping = Pawn->PlayerMappingContext;
	}
}

void URDCAPlayerControlComponent::RemoveMappingContext()
{
	// Only remove a mapping this component actually installed, not another system's context.
	if (AppliedSubsystem.IsValid() && AppliedMapping.IsValid())
	{
		AppliedSubsystem->RemoveMappingContext(AppliedMapping.Get());
	}
	AppliedSubsystem.Reset();
	AppliedMapping.Reset();
}

void URDCAPlayerControlComponent::RemoveBindings()
{
	if (BoundInput.IsValid())
	{
		for (const uint32 Handle : BindingHandles) { BoundInput->RemoveBindingByHandle(Handle); }
	}
	BindingHandles.Reset();
	BoundInput.Reset();
}

void URDCAPlayerControlComponent::BindInputs(UInputComponent* Input)
{
	RemoveBindings();
	if (!ResolvePawn() || !Pawn) { return; }
	UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(Input);
	if (!Enhanced)
	{
		UE_LOG(LogRDCAPlayer, Error, TEXT("Player Control requires Enhanced Input."));
		return;
	}
	BoundInput = Enhanced;
	if (Pawn->MoveAction)
	{
		BindingHandles.Add(Enhanced->BindAction(Pawn->MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Move).GetHandle());
	}
	else { UE_LOG(LogRDCAPlayer, Warning, TEXT("Player Control: MoveAction is not assigned.")); }
	if (Pawn->CrashAction)
	{
		BindingHandles.Add(Enhanced->BindAction(Pawn->CrashAction, ETriggerEvent::Started, this, &ThisClass::StartCrashCharge).GetHandle());
		BindingHandles.Add(Enhanced->BindAction(Pawn->CrashAction, ETriggerEvent::Completed, this, &ThisClass::ReleaseCrash).GetHandle());
		BindingHandles.Add(Enhanced->BindAction(Pawn->CrashAction, ETriggerEvent::Canceled, this, &ThisClass::CancelCrashCharge).GetHandle());
	}
	else { UE_LOG(LogRDCAPlayer, Warning, TEXT("Player Control: CrashAction is not assigned.")); }
	if (Pawn->DashAction)
	{
		BindingHandles.Add(Enhanced->BindAction(Pawn->DashAction, ETriggerEvent::Started, this, &ThisClass::StartGroundDash).GetHandle());
	}
	else { UE_LOG(LogRDCAPlayer, Warning, TEXT("Player Control: DashAction is not assigned.")); }
	RefreshMappingContext();
}

void URDCAPlayerControlComponent::Move(const FInputActionValue& Value)
{
	SubmitMove(Value.Get<FVector2D>());
}

void URDCAPlayerControlComponent::SubmitMove(const FVector2D& MovementInput)
{
	if (!ResolvePawn() || !Pawn || !CanSubmitInput()) { return; }
	if (Pawn->PhaseCrashComponent->IsAttached())
	{
		Pawn->PhaseCrashComponent->MoveAttached(MovementInput);
		return;
	}
	const float InputScale = Pawn->PhaseCrashComponent->GetMovementInputScale();
	FVector Forward = Pawn->Camera ? Pawn->Camera->GetForwardVector() : FVector::ForwardVector;
	FVector Right = Pawn->Camera ? Pawn->Camera->GetRightVector() : FVector::RightVector;
	Forward.Z = Right.Z = 0.0f;
	Forward.Normalize();
	Right.Normalize();
	Pawn->AddMovementInput(Forward, MovementInput.Y * InputScale);
	Pawn->AddMovementInput(Right, MovementInput.X * InputScale);
}

void URDCAPlayerControlComponent::ResetControl()
{
	RemoveBindings();
	RemoveMappingContext();
	if (!ResolvePawn()) { return; }
	if (Motion)
	{
		ClearActionRequests();
		CancelCrashCharge();
	}
	if (URDCAAbilitySystemComponent* ASC = OwnerPawn->FindComponentByClass<URDCAAbilitySystemComponent>())
	{
		FGameplayTagContainer PlayerSkills;
		PlayerSkills.AddTag(RDCAGameplayTags::Ability_Player_GroundDash);
		PlayerSkills.AddTag(RDCAGameplayTags::Ability_Player_PhaseCrash);
		ASC->CancelAbilities(&PlayerSkills);
	}
	OwnerPawn->ConsumeMovementInputVector();
	if (UPawnMovementComponent* Movement = OwnerPawn->GetMovementComponent()) { Movement->StopMovementImmediately(); }
}

void URDCAPlayerControlComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetControl();
	if (Motion)
	{
		Motion->OnMotionUpdated.RemoveAll(this);
		Motion->OnCrashStateChanged.RemoveDynamic(this, &ThisClass::HandleMotionStateChanged);
	}
	Super::EndPlay(EndPlayReason);
}
