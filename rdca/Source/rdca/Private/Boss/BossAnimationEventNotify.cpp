#include "Boss/BossAnimationEventNotify.h"

#include "Animation/AnimSequenceBase.h"
#include "Boss/BossAnimationComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UBossAnimationEventNotify::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}
	if (UBossAnimationComponent* AnimationComponent =
			MeshComp->GetOwner()->FindComponentByClass<UBossAnimationComponent>())
	{
		AnimationComponent->HandleAnimationEvent(AnimationEvent);
	}
}

FString UBossAnimationEventNotify::GetNotifyName_Implementation() const
{
	const UEnum* EventEnum = StaticEnum<EBossAnimationEvent>();
	return EventEnum
		? FString::Printf(
			TEXT("Boss: %s"),
			*EventEnum->GetNameStringByValue(static_cast<int64>(AnimationEvent)))
		: TEXT("Boss Animation Event");
}
