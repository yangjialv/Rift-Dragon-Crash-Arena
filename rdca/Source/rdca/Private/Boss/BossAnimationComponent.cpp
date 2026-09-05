#include "Boss/BossAnimationComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Boss/BossWeakPointComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "rdca.h"

UBossAnimationComponent::UBossAnimationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UBossAnimationComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TickStunLoop();
}

void UBossAnimationComponent::BeginPlay()
{
	Super::BeginPlay();

	BossMesh = FindBossSkeletalMesh();
	Encounter = GetOwner()->FindComponentByClass<UBossEncounterComponent>();
	WeakPoint = GetOwner()->FindComponentByClass<UBossWeakPointComponent>();

	if (Encounter.IsValid())
	{
		Encounter->OnEncounterStateChanged.AddDynamic(
			this,
			&UBossAnimationComponent::HandleEncounterStateChanged);
	}
	if (WeakPoint.IsValid())
	{
		WeakPoint->OnWeakPointCrash.AddDynamic(
			this,
			&UBossAnimationComponent::HandleWeakPointCrash);
	}

	if (!BossMesh.IsValid() || !BossMesh->GetAnimInstance())
	{
		UE_LOG(
			LogRDCAPlayer,
			Warning,
			TEXT("Boss Animation requires a skeletal mesh with an Anim Instance. Boss=%s"),
			*GetNameSafe(GetOwner()));
		return;
	}

	if (bPlayIntroOnBeginPlay && IntroMontage)
	{
		if (Encounter.IsValid())
		{
			Encounter->SetIntroHold(true);
		}
		if (GroundIdleMontage && GroundIdleDuration > 0.0f
			&& PlayMontage(GroundIdleMontage))
		{
			bIntroActive = true;
			GetWorld()->GetTimerManager().SetTimer(
				GroundIdleTimer,
				this,
				&UBossAnimationComponent::StartIntroMontage,
				GroundIdleDuration,
				false);
		}
		else
		{
			StartIntroMontage();
		}
	}
	else
	{
		FinishIntro();
	}
}

void UBossAnimationComponent::HandleEncounterStateChanged(
	const EBossEncounterState PreviousState,
	const EBossEncounterState NewState)
{
	if (bIntroActive || !Encounter.IsValid())
	{
		return;
	}
	if (PreviousState == EBossEncounterState::WeakPointExposed
		&& NewState != EBossEncounterState::WeakPointExposed)
	{
		StopStunLoop();
	}

	const EBossAttackType Attack = Encounter->GetCurrentAttack();
	switch (NewState)
	{
	case EBossEncounterState::Preparing:
		switch (Attack)
		{
		case EBossAttackType::Shockwave:
			PlayMontage(RoarMontage);
			break;
		case EBossAttackType::AimedVolley:
		case EBossAttackType::FanBarrage:
			PlayMontage(AttackMontage);
			break;
		case EBossAttackType::SweepLaser:
			PlayMontage(SpellMontage, TEXT("Start"));
			break;
		default:
			break;
		}
		break;

	case EBossEncounterState::Attacking:
		if (Attack == EBossAttackType::SweepLaser)
		{
			JumpSpellToSection(TEXT("Loop"));
		}
		break;

	case EBossEncounterState::Recovery:
		if (Attack == EBossAttackType::SweepLaser)
		{
			JumpSpellToSection(TEXT("End"));
		}
		break;

	case EBossEncounterState::WeakPointExposed:
		PlayMontage(StunMontage);
		break;

	case EBossEncounterState::Dead:
		if (UAnimInstance* AnimInstance = BossMesh.IsValid()
			? BossMesh->GetAnimInstance()
			: nullptr)
		{
			AnimInstance->Montage_Stop(0.05f);
		}
		PlayMontage(DeathMontage);
		break;

	default:
		break;
	}
}

void UBossAnimationComponent::HandleWeakPointCrash(
	APawn* CrashingPawn,
	const bool bWasEffective,
	const FHitResult& Hit)
{
	if (!bWasEffective || !Encounter.IsValid()
		|| Encounter->GetEncounterState() == EBossEncounterState::Dead)
	{
		return;
	}
	PlayMontage(HitMontage);
}

void UBossAnimationComponent::HandleAnimationEvent(
	const EBossAnimationEvent AnimationEvent)
{
	if (AnimationEvent == EBossAnimationEvent::IntroFinished)
	{
		FinishIntro();
	}
	else
	{
		if (AnimationEvent == EBossAnimationEvent::IntroTakeoff)
		{
			// StartIntroMontage already emits this event for the default opening.
			// Keeping the Notify compatible avoids double-shattering the centre.
			if (bIntroTakeoffBroadcast)
			{
				return;
			}
			bIntroTakeoffBroadcast = true;
		}
		OnAnimationEvent.Broadcast(AnimationEvent);
	}

	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss animation event. Boss=%s Event=%d"),
		*GetNameSafe(GetOwner()),
		static_cast<int32>(AnimationEvent));
}

void UBossAnimationComponent::FinishIntro()
{
	if (bIntroFinished)
	{
		return;
	}
	bIntroFinished = true;
	bIntroActive = false;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(GroundIdleTimer);
		GetWorld()->GetTimerManager().ClearTimer(IntroGroundHoldTimer);
	}
	if (Encounter.IsValid())
	{
		Encounter->SetIntroHold(false);
	}
	OnAnimationEvent.Broadcast(EBossAnimationEvent::IntroFinished);
}

void UBossAnimationComponent::StartIntroMontage()
{
	if (bIntroFinished || !BossMesh.IsValid() || !IntroMontage)
	{
		FinishIntro();
		return;
	}

	UAnimInstance* AnimInstance = BossMesh->GetAnimInstance();
	if (!AnimInstance)
	{
		FinishIntro();
		return;
	}

	// Do not stop Ground Idle manually here. Montage_Play takes ownership of
	// the shared slot and blends from its current pose into Intro. Explicitly
	// stopping first briefly exposed the AnimBP's FlyIdle base pose, producing
	// a visible pop between the grounded and takeoff animations.
	bIntroActive = PlayMontage(IntroMontage);
	if (!bIntroActive)
	{
		FinishIntro();
		return;
	}

	if (IntroGroundHoldDuration > 0.0f)
	{
		AnimInstance->Montage_SetPlayRate(IntroMontage, 0.0f);
		GetWorld()->GetTimerManager().SetTimer(
			IntroGroundHoldTimer,
			this,
			&UBossAnimationComponent::ReleaseIntroGroundHold,
			IntroGroundHoldDuration,
			false);
	}

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &UBossAnimationComponent::HandleIntroMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, IntroMontage);

	// Ground Idle has completed: the following Montage is the actual takeoff.
	// Emit this once so the arena can fracture the centre without a hand-placed
	// Notify; an existing IntroTakeoff Notify remains harmless and is ignored.
	bIntroTakeoffBroadcast = true;
	OnAnimationEvent.Broadcast(EBossAnimationEvent::IntroTakeoff);
	UE_LOG(LogRDCAPlayer, Log, TEXT("Boss intro takeoff began. Boss=%s"),
		*GetNameSafe(GetOwner()));
}

void UBossAnimationComponent::ReleaseIntroGroundHold()
{
	if (!bIntroActive || !BossMesh.IsValid() || !IntroMontage)
	{
		return;
	}

	if (UAnimInstance* AnimInstance = BossMesh->GetAnimInstance())
	{
		AnimInstance->Montage_SetPlayRate(IntroMontage, 1.0f);
	}
}

void UBossAnimationComponent::HandleIntroMontageEnded(
	UAnimMontage* Montage,
	const bool bInterrupted)
{
	if (Montage == IntroMontage)
	{
		FinishIntro();
	}
}

bool UBossAnimationComponent::PlayMontage(
	UAnimMontage* Montage,
	const FName StartSection)
{
	UAnimInstance* AnimInstance = BossMesh.IsValid()
		? BossMesh->GetAnimInstance()
		: nullptr;
	if (!AnimInstance || !Montage)
	{
		return false;
	}

	const float Duration = AnimInstance->Montage_Play(Montage, 1.0f);
	if (Duration <= 0.0f)
	{
		UE_LOG(
			LogRDCAPlayer,
			Warning,
			TEXT("Boss montage failed to play. Boss=%s Montage=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Montage));
		return false;
	}
	if (!StartSection.IsNone())
	{
		AnimInstance->Montage_JumpToSection(StartSection, Montage);
	}
	return true;
}

void UBossAnimationComponent::JumpSpellToSection(const FName SectionName)
{
	UAnimInstance* AnimInstance = BossMesh.IsValid()
		? BossMesh->GetAnimInstance()
		: nullptr;
	if (AnimInstance && SpellMontage
		&& AnimInstance->Montage_IsPlaying(SpellMontage))
	{
		AnimInstance->Montage_JumpToSection(SectionName, SpellMontage);
	}
}

void UBossAnimationComponent::TickStunLoop()
{
	if (!Encounter.IsValid()
		|| Encounter->GetEncounterState() != EBossEncounterState::WeakPointExposed
		|| !StunMontage)
	{
		return;
	}

	UAnimInstance* AnimInstance = BossMesh.IsValid()
		? BossMesh->GetAnimInstance()
		: nullptr;
	if (!AnimInstance)
	{
		return;
	}

	if (!AnimInstance->Montage_IsActive(StunMontage))
	{
		// A successful weak-point hit may briefly interrupt Stun with Hit. Once
		// that reaction has completed, restore the stunned pose for the rest of
		// the exposure window rather than falling through to FlyIdle.
		if (!AnimInstance->Montage_IsActive(nullptr))
		{
			PlayMontage(StunMontage);
		}
	}
}

void UBossAnimationComponent::StopStunLoop()
{
	if (UAnimInstance* AnimInstance = BossMesh.IsValid()
		? BossMesh->GetAnimInstance()
		: nullptr)
	{
		if (StunMontage && AnimInstance->Montage_IsActive(StunMontage))
		{
			AnimInstance->Montage_Stop(StunLoopReleaseBlendOut, StunMontage);
		}
	}
}

USkeletalMeshComponent* UBossAnimationComponent::FindBossSkeletalMesh() const
{
	TArray<USkeletalMeshComponent*> Meshes;
	GetOwner()->GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh->GetName().Equals(
			TEXT("BossSkeletalMesh"),
			ESearchCase::IgnoreCase))
		{
			return Mesh;
		}
	}
	return Meshes.IsEmpty() ? nullptr : Meshes[0];
}
