#include "Boss/BossAnimationComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Boss/BossWeakPointComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "rdca.h"

UBossAnimationComponent::UBossAnimationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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
		bIntroActive = PlayMontage(IntroMontage);
		if (bIntroActive)
		{
			FOnMontageEnded EndDelegate;
			EndDelegate.BindUObject(
				this,
				&UBossAnimationComponent::HandleIntroMontageEnded);
			BossMesh->GetAnimInstance()->Montage_SetEndDelegate(
				EndDelegate,
				IntroMontage);
		}
		else
		{
			FinishIntro();
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
	if (Encounter.IsValid())
	{
		Encounter->SetIntroHold(false);
	}
	OnAnimationEvent.Broadcast(EBossAnimationEvent::IntroFinished);
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
