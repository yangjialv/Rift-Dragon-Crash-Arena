#pragma once

#include "CoreMinimal.h"
#include "Audio/RDCAAudio.h"
#include "GameplayTagContainer.h"

class UAudioComponent;
class UNiagaraComponent;
class USceneComponent;
class UPlayerHealthComponent;

/** Local single-player Gameplay Cue gateway. Playback/render backends never change gameplay. */
namespace RDCAPresentation
{
	RDCA_API FGameplayTag GetAudioTag(ERDCAAudioCue Cue);
	RDCA_API void PlayAtLocation(const UObject* Context, ERDCAAudioCue Cue, const FVector& Location, float Volume = 0.65f, float Pitch = 1.0f);
	RDCA_API void Play2D(const UObject* Context, ERDCAAudioCue Cue, float Volume = 0.65f, float Pitch = 1.0f);
	RDCA_API UAudioComponent* Spawn2D(const UObject* Context, ERDCAAudioCue Cue, float Volume = 0.65f, float Pitch = 1.0f, bool bLoop = false);
	RDCA_API UAudioComponent* SpawnLoopAttached(ERDCAAudioCue Cue, USceneComponent* Attach, float Volume = 0.5f, float Pitch = 1.0f);
	RDCA_API UAudioComponent* SpawnLoopAtLocation(const UObject* Context, ERDCAAudioCue Cue, const FVector& Location, float Volume = 0.5f, float Pitch = 1.0f);
	RDCA_API void StopLoop(const UObject* Context, ERDCAAudioCue Cue, UAudioComponent*& Audio, float FadeOutTime = 0.08f);
	RDCA_API void PlayNextBarrageShot(const UObject* Context, const FVector& Location, float Volume = 0.28f);
	RDCA_API void PlayerDamaged(UPlayerHealthComponent* Health, int32 Damage);
	RDCA_API void SetLaserFlame(UNiagaraComponent* Effect, bool bActive, bool bImmediate = false);
	RDCA_API void SetShockwaveVisible(USceneComponent* Visual, bool bVisible);
}
