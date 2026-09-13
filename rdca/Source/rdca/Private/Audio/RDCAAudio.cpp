#include "Audio/RDCAAudio.h"

#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "rdca.h"

namespace
{
const TCHAR* GetAssetPath(const ERDCAAudioCue Cue)
{
	switch (Cue)
	{
	case ERDCAAudioCue::PlayerDash: return TEXT("/Game/Audio/v0_4/SFX_Player_Dash.SFX_Player_Dash");
	case ERDCAAudioCue::PlayerJumpRelease: return TEXT("/Game/Audio/v0_4/SFX_Player_JumpRelease.SFX_Player_JumpRelease");
	case ERDCAAudioCue::PlayerDamaged: return TEXT("/Game/Audio/v0_4/SFX_Player_Damaged.SFX_Player_Damaged");
	case ERDCAAudioCue::BossBodyRebound: return TEXT("/Game/Audio/v0_4/SFX_BossBody_Rebound.SFX_BossBody_Rebound");
	case ERDCAAudioCue::WeakPointHit: return TEXT("/Game/Audio/v0_4/SFX_WeakPoint_Hit.SFX_WeakPoint_Hit");
	case ERDCAAudioCue::BossRoar: return TEXT("/Game/Audio/v0_4/SFX_Boss_Roar.SFX_Boss_Roar");
	case ERDCAAudioCue::ShockwaveWarning: return TEXT("/Game/Audio/v0_4/SFX_Shockwave_Warning.SFX_Shockwave_Warning");
	case ERDCAAudioCue::ShockwaveRelease: return TEXT("/Game/Audio/v0_4/SFX_Shockwave_Release.SFX_Shockwave_Release");
	case ERDCAAudioCue::LaserWarning: return TEXT("/Game/Audio/v0_4/SFX_Laser_Warning.SFX_Laser_Warning");
	case ERDCAAudioCue::LaserIgnite: return TEXT("/Game/Audio/v0_4/SFX_Laser_Ignite.SFX_Laser_Ignite");
	case ERDCAAudioCue::LaserLoop: return TEXT("/Game/Audio/v0_4/SFX_Laser_Loop.SFX_Laser_Loop");
	case ERDCAAudioCue::LaserEnd: return TEXT("/Game/Audio/v0_4/SFX_Laser_End.SFX_Laser_End");
	case ERDCAAudioCue::BarrageCharge: return TEXT("/Game/Audio/v0_4/SFX_Barrage_Charge.SFX_Barrage_Charge");
	case ERDCAAudioCue::BarrageShot01: return TEXT("/Game/Audio/v0_4/SFX_Barrage_Shot_01.SFX_Barrage_Shot_01");
	case ERDCAAudioCue::BarrageShot02: return TEXT("/Game/Audio/v0_4/SFX_Barrage_Shot_02.SFX_Barrage_Shot_02");
	case ERDCAAudioCue::BarrageShot03: return TEXT("/Game/Audio/v0_4/SFX_Barrage_Shot_03.SFX_Barrage_Shot_03");
	case ERDCAAudioCue::BarragePlayerHit: return TEXT("/Game/Audio/v0_4/SFX_Barrage_PlayerHit.SFX_Barrage_PlayerHit");
	case ERDCAAudioCue::BossTakeoff: return TEXT("/Game/Audio/v0_4/SFX_Boss_Takeoff.SFX_Boss_Takeoff");
	case ERDCAAudioCue::CyberCenterFracture: return TEXT("/Game/Audio/v0_4/SFX_CyberCenter_Fracture.SFX_CyberCenter_Fracture");
	case ERDCAAudioCue::AnchorEmerge: return TEXT("/Game/Audio/v0_4/SFX_Anchor_Emerge.SFX_Anchor_Emerge");
	case ERDCAAudioCue::AnchorAttach: return TEXT("/Game/Audio/v0_4/SFX_Anchor_Attach.SFX_Anchor_Attach");
	case ERDCAAudioCue::AnchorOverloadLoop: return TEXT("/Game/Audio/v0_4/SFX_Anchor_OverloadLoop.SFX_Anchor_OverloadLoop");
	case ERDCAAudioCue::AnchorLaunch: return TEXT("/Game/Audio/v0_4/SFX_Anchor_Launch.SFX_Anchor_Launch");
	case ERDCAAudioCue::AnchorShatter: return TEXT("/Game/Audio/v0_4/SFX_Anchor_Shatter.SFX_Anchor_Shatter");
	case ERDCAAudioCue::PhaseTransitionStart: return TEXT("/Game/Audio/v0_4/SFX_PhaseTransition_Start.SFX_PhaseTransition_Start");
	case ERDCAAudioCue::PhaseTransitionLoop: return TEXT("/Game/Audio/v0_4/SFX_PhaseTransition_Loop.SFX_PhaseTransition_Loop");
	case ERDCAAudioCue::PhaseTransitionEnd: return TEXT("/Game/Audio/v0_4/SFX_PhaseTransition_End.SFX_PhaseTransition_End");
	case ERDCAAudioCue::BossDeath: return TEXT("/Game/Audio/v0_4/SFX_Boss_Death.SFX_Boss_Death");
	case ERDCAAudioCue::Victory: return TEXT("/Game/Audio/v0_4/SFX_Victory.SFX_Victory");
	case ERDCAAudioCue::Defeat: return TEXT("/Game/Audio/v0_4/SFX_Defeat.SFX_Defeat");
	case ERDCAAudioCue::BossMusicMain: return TEXT("/Game/Audio/BGM/BGM_Boss_Main.BGM_Boss_Main");
	default: return nullptr;
	}
}

TMap<ERDCAAudioCue, TWeakObjectPtr<USoundBase>> LoadedSounds;
int32 NextBarrageShotIndex = 0;
TArray<TWeakObjectPtr<UAudioComponent>> ActiveBarrageShotVoices;
}

USoundBase* RDCAAudio::Load(const ERDCAAudioCue Cue)
{
	if (const TWeakObjectPtr<USoundBase>* Existing = LoadedSounds.Find(Cue))
	{
		if (Existing->IsValid())
		{
			return Existing->Get();
		}
	}

	const TCHAR* AssetPath = GetAssetPath(Cue);
	USoundBase* Sound = AssetPath
		? LoadObject<USoundBase>(nullptr, AssetPath)
		: nullptr;
	LoadedSounds.Add(Cue, Sound);
	if (!Sound)
	{
		UE_LOG(LogRDCAPlayer, Warning, TEXT("RDCA audio asset missing: %s"), AssetPath ? AssetPath : TEXT("None"));
	}
	return Sound;
}

void RDCAAudio::PlayAtLocation(
	const UObject* WorldContextObject,
	const ERDCAAudioCue Cue,
	const FVector& Location,
	const float Volume,
	const float Pitch)
{
	if (WorldContextObject)
	{
		if (USoundBase* Sound = Load(Cue))
		{
			UGameplayStatics::PlaySoundAtLocation(
				WorldContextObject, Sound, Location, Volume, Pitch);
		}
	}
}

void RDCAAudio::Play2D(
	const UObject* WorldContextObject,
	const ERDCAAudioCue Cue,
	const float Volume,
	const float Pitch)
{
	if (WorldContextObject)
	{
		if (USoundBase* Sound = Load(Cue))
		{
			UGameplayStatics::PlaySound2D(
				WorldContextObject, Sound, Volume, Pitch);
		}
	}
}

UAudioComponent* RDCAAudio::Spawn2D(
	const UObject* WorldContextObject,
	const ERDCAAudioCue Cue,
	const float Volume,
	const float Pitch,
	const bool bLoop)
{
	USoundBase* Sound = WorldContextObject ? Load(Cue) : nullptr;
	if (!Sound)
	{
		return nullptr;
	}
	if (USoundWave* Wave = Cast<USoundWave>(Sound))
	{
		Wave->bLooping = bLoop;
	}
	return UGameplayStatics::SpawnSound2D(
		WorldContextObject,
		Sound,
		Volume,
		Pitch,
		0.0f,
		nullptr,
		false,
		false);
}

UAudioComponent* RDCAAudio::SpawnLoopAttached(
	const ERDCAAudioCue Cue,
	USceneComponent* AttachToComponent,
	const float Volume,
	const float Pitch)
{
	USoundBase* Sound = AttachToComponent ? Load(Cue) : nullptr;
	if (!Sound)
	{
		return nullptr;
	}
	if (USoundWave* Wave = Cast<USoundWave>(Sound))
	{
		Wave->bLooping = true;
	}
	return UGameplayStatics::SpawnSoundAttached(
		Sound,
		AttachToComponent,
		NAME_None,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset,
		true,
		Volume,
		Pitch,
		0.0f,
		nullptr,
		nullptr,
		false);
}

UAudioComponent* RDCAAudio::SpawnLoopAtLocation(
	const UObject* WorldContextObject,
	const ERDCAAudioCue Cue,
	const FVector& Location,
	const float Volume,
	const float Pitch)
{
	USoundBase* Sound = WorldContextObject ? Load(Cue) : nullptr;
	if (!Sound)
	{
		return nullptr;
	}
	if (USoundWave* Wave = Cast<USoundWave>(Sound))
	{
		Wave->bLooping = true;
	}
	return UGameplayStatics::SpawnSoundAtLocation(
		WorldContextObject,
		Sound,
		Location,
		FRotator::ZeroRotator,
		Volume,
		Pitch,
		0.0f,
		nullptr,
		nullptr,
		false);
}

void RDCAAudio::StopLoop(UAudioComponent*& AudioComponent, const float FadeOutTime)
{
	if (!IsValid(AudioComponent))
	{
		AudioComponent = nullptr;
		return;
	}
	if (FadeOutTime > 0.0f)
	{
		AudioComponent->bAutoDestroy = true;
		AudioComponent->FadeOut(FadeOutTime, 0.0f);
	}
	else
	{
		AudioComponent->Stop();
		AudioComponent->DestroyComponent();
	}
	AudioComponent = nullptr;
}

void RDCAAudio::PlayNextBarrageShot(
	const UObject* WorldContextObject,
	const FVector& Location,
	const float Volume)
{
	constexpr ERDCAAudioCue Variants[] = {
		ERDCAAudioCue::BarrageShot01,
		ERDCAAudioCue::BarrageShot02,
		ERDCAAudioCue::BarrageShot03
	};
	const ERDCAAudioCue Cue = Variants[NextBarrageShotIndex % UE_ARRAY_COUNT(Variants)];
	constexpr float PitchVariants[] = {0.98f, 1.01f, 1.0f};
	const float Pitch = PitchVariants[
		NextBarrageShotIndex % UE_ARRAY_COUNT(PitchVariants)];
	++NextBarrageShotIndex;

	ActiveBarrageShotVoices.RemoveAll(
		[](const TWeakObjectPtr<UAudioComponent>& Voice)
		{
			return !Voice.IsValid() || !Voice->IsPlaying();
		});
	while (ActiveBarrageShotVoices.Num() >= 4)
	{
		if (ActiveBarrageShotVoices[0].IsValid())
		{
			ActiveBarrageShotVoices[0]->Stop();
		}
		ActiveBarrageShotVoices.RemoveAt(0);
	}

	if (WorldContextObject)
	{
		if (USoundBase* Sound = Load(Cue))
		{
			if (UAudioComponent* Voice = UGameplayStatics::SpawnSoundAtLocation(
				WorldContextObject,
				Sound,
				Location,
				FRotator::ZeroRotator,
				Volume,
				Pitch))
			{
				ActiveBarrageShotVoices.Add(Voice);
			}
		}
	}
}
