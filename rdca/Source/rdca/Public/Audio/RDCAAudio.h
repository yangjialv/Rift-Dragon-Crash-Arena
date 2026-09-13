#pragma once

#include "CoreMinimal.h"

class UAudioComponent;
class USceneComponent;
class USoundBase;

/** Stable identifiers for the approved v0.4 combat-audio pass. */
enum class ERDCAAudioCue : uint8
{
	PlayerDash,
	PlayerJumpRelease,
	PlayerDamaged,
	BossBodyRebound,
	WeakPointHit,
	BossRoar,
	ShockwaveWarning,
	ShockwaveRelease,
	LaserWarning,
	LaserIgnite,
	LaserLoop,
	LaserEnd,
	BarrageCharge,
	BarrageShot01,
	BarrageShot02,
	BarrageShot03,
	BarragePlayerHit,
	BossTakeoff,
	CyberCenterFracture,
	AnchorEmerge,
	AnchorAttach,
	AnchorOverloadLoop,
	AnchorLaunch,
	AnchorShatter,
	PhaseTransitionStart,
	PhaseTransitionLoop,
	PhaseTransitionEnd,
	BossDeath,
	Victory,
	Defeat,
	BossMusicMain
};

/**
 * One small audio gateway keeps asset paths and playback rules out of gameplay
 * classes. It also makes replacing this review pass later a single-file job.
 */
namespace RDCAAudio
{
	RDCA_API USoundBase* Load(ERDCAAudioCue Cue);

	RDCA_API void PlayAtLocation(
		const UObject* WorldContextObject,
		ERDCAAudioCue Cue,
		const FVector& Location,
		float Volume = 0.65f,
		float Pitch = 1.0f);

	RDCA_API void Play2D(
		const UObject* WorldContextObject,
		ERDCAAudioCue Cue,
		float Volume = 0.65f,
		float Pitch = 1.0f);

	RDCA_API UAudioComponent* Spawn2D(
		const UObject* WorldContextObject,
		ERDCAAudioCue Cue,
		float Volume = 0.65f,
		float Pitch = 1.0f,
		bool bLoop = false);

	RDCA_API UAudioComponent* SpawnLoopAttached(
		ERDCAAudioCue Cue,
		USceneComponent* AttachToComponent,
		float Volume = 0.5f,
		float Pitch = 1.0f);

	RDCA_API UAudioComponent* SpawnLoopAtLocation(
		const UObject* WorldContextObject,
		ERDCAAudioCue Cue,
		const FVector& Location,
		float Volume = 0.5f,
		float Pitch = 1.0f);

	RDCA_API void StopLoop(UAudioComponent*& AudioComponent, float FadeOutTime = 0.08f);

	/** Cycles through the three variants so dense barrages remain rhythmic. */
	RDCA_API void PlayNextBarrageShot(
		const UObject* WorldContextObject,
		const FVector& Location,
		float Volume = 0.28f);
}
