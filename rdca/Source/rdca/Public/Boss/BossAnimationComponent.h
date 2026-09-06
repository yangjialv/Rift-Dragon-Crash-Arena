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
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

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

	/** Fallback hold used only when no Ground Idle Montage is configured. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Animation|Intro",
		meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float IntroGroundHoldDuration = 0.0f;

	/** A looping grounded pose played before the Dragon's takeoff Intro. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Animation|Intro")
	TObjectPtr<UAnimMontage> GroundIdleMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Animation|Intro",
		meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float GroundIdleDuration = 3.0f;

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

	/** Blend-out time when the stun loop ends with the weak-point window. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Animation|Stun",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StunLoopReleaseBlendOut = 0.12f;

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
	void StartIntroMontage();
	void ReleaseIntroGroundHold();
	bool PlayMontage(UAnimMontage* Montage, FName StartSection = NAME_None);
	void JumpSpellToSection(FName SectionName);
	void TickStunLoop();
	void StopStunLoop();
	USkeletalMeshComponent* FindBossSkeletalMesh() const;

	TWeakObjectPtr<USkeletalMeshComponent> BossMesh;
	TWeakObjectPtr<UBossEncounterComponent> Encounter;
	TWeakObjectPtr<UBossWeakPointComponent> WeakPoint;
	bool bIntroActive = false;
	bool bIntroFinished = false;
	bool bIntroTakeoffBroadcast = false;
	/** A successful weak-point hit ends the visual stun for this exposure window. */
	bool bWeakPointHitDuringExposure = false;
	FTimerHandle GroundIdleTimer;
	FTimerHandle IntroGroundHoldTimer;
};
