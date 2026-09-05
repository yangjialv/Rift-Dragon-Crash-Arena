#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "Boss/BossAnimationComponent.h"
#include "BossAnimationEventNotify.generated.h"

UCLASS(meta = (DisplayName = "Boss Animation Event"))
class RDCA_API UBossAnimationEventNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Animation")
	EBossAnimationEvent AnimationEvent = EBossAnimationEvent::IntroTakeoff;
};
