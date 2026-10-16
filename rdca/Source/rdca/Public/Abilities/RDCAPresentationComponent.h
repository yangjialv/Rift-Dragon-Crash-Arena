#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayCueNotify_Static.h"
#include "Audio/RDCAAudio.h"
#include "RDCAPresentationComponent.generated.h"

class UAudioComponent;
class USceneComponent;
class UPlayerHealthComponent;

enum class ERDCAPresentationKind : uint8 { SpatialAudio, Audio2D, SpawnAudio2D, BarrageShot, PlayerHit, LaserFlame, ShockwaveVisual };

/** Per-invocation data, never stored on the shared GameplayCueNotify CDO. */
UCLASS()
class RDCA_API URDCAPresentationRequest : public UObject
{
	GENERATED_BODY()
public:
	ERDCAPresentationKind Kind = ERDCAPresentationKind::SpatialAudio;
	ERDCAAudioCue AudioCue = ERDCAAudioCue::PlayerDash;
	FGameplayTag Tag;
	FVector Location = FVector::ZeroVector;
	float Volume = 0.65f;
	float Pitch = 1.0f;
	float FadeOut = 0.08f;
	int32 Damage = 0;
	bool bLoop = false;
	bool bVisible = false;
	bool bImmediate = false;
	bool bActive = false;
	bool bRemovedHandled = false;
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> SceneComponent;
	UPROPERTY(Transient)
	TObjectPtr<UPlayerHealthComponent> Health;
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Audio;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnRDCACueHandled, FGameplayTag, EGameplayCueEvent::Type);

/** Owns persistent presentation per target, including cleanup when that target leaves the world. */
UCLASS()
class RDCA_API URDCAPresentationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URDCAPresentationComponent();
	static URDCAPresentationComponent* FindOrCreate(AActor* Target);
	void Execute(URDCAPresentationRequest* Request);
	UAudioComponent* Start(URDCAPresentationRequest* Request);
	void Stop(FGameplayTag Tag, float FadeOut = 0.08f);
	void HandleCue(EGameplayCueEvent::Type Event, const FGameplayCueParameters& Parameters);
	int32 GetActiveCueCount() const { return ActiveRequests.Num(); }
	bool HasActiveCue(FGameplayTag Tag) const { return ActiveRequests.Contains(Tag); }
	FOnRDCACueHandled OnCueHandled;
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<URDCAPresentationRequest>> ActiveRequests;
};

/** Native registered Gameplay Cue; all mutable state lives on the target component. */
UCLASS()
class RDCA_API URDCACombatCueNotify : public UGameplayCueNotify_Static
{
	GENERATED_BODY()
public:
	virtual void HandleGameplayCue(AActor* Target, EGameplayCueEvent::Type Event, const FGameplayCueParameters& Parameters) override;
};
