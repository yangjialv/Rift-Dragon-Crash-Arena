#pragma once

#include "CoreMinimal.h"
#include "Boss/BossEncounterComponent.h"
#include "Components/ActorComponent.h"
#include "BossAnimationComponent.generated.h"

class UAnimInstance;
class UAnimMontage;
class UBossWeakPointComponent;
class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class EBossAnimationEvent : uint8
{
	IntroTakeoff,
	IntroFinished,
	ProjectileRelease,
	ShockwaveRelease,
	LaserActivate,
	LaserEnd
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnBossAnimationEvent,
	EBossAnimationEvent,
	AnimationEvent);

UCLASS(
	ClassGroup = (Boss),
	meta = (BlueprintSpawnableComponent, DisplayName = "Boss Animation"))
class RDCA_API UBossAnimationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossAnimationComponent();

	UFUNCTION(BlueprintCallable, Category = "Boss|Animation")
	void HandleAnimationEvent(EBossAnimationEvent AnimationEvent);

	UFUNCTION(BlueprintCallable, Category = "Boss|Animation")
	void FinishIntro();

	UPROPERTY(BlueprintAssignable, Category = "Boss|Animation")
	FOnBossAnimationEvent OnAnimationEvent;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Animation|Setup")
	bool bPlayIntroOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Animation|Montages")
	TObjectPtr<UAnimMontage> IntroMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Animation|Montages")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Animation|Montages")
	TObjectPtr<UAnimMontage> RoarMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Animation|Montages")
	TObjectPtr<UAnimMontage> SpellMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Animation|Montages")
	TObjectPtr<UAnimMontage> HitMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Animation|Montages")
	TObjectPtr<UAnimMontage> StunMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Animation|Montages")
	TObjectPtr<UAnimMontage> DeathMontage;

private:
	UFUNCTION()
	void HandleEncounterStateChanged(
		EBossEncounterState PreviousState,
		EBossEncounterState NewState);

	UFUNCTION()
	void HandleWeakPointCrash(
		APawn* CrashingPawn,
		bool bWasEffective,
		const FHitResult& Hit);

	void HandleIntroMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	bool PlayMontage(UAnimMontage* Montage, FName StartSection = NAME_None);
	void JumpSpellToSection(FName SectionName);
	USkeletalMeshComponent* FindBossSkeletalMesh() const;

	TWeakObjectPtr<USkeletalMeshComponent> BossMesh;
	TWeakObjectPtr<UBossEncounterComponent> Encounter;
	TWeakObjectPtr<UBossWeakPointComponent> WeakPoint;
	bool bIntroActive = false;
	bool bIntroFinished = false;
};
