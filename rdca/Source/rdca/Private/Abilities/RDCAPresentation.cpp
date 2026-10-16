#include "Abilities/RDCAPresentation.h"

#include "Abilities/RDCAPresentationComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "GameplayCueManager.h"
#include "GameplayCueSet.h"
#include "NativeGameplayTags.h"
#include "NiagaraComponent.h"
#include "Player/PlayerHealthComponent.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_PlayerDash, "GameplayCue.RDCA.Player.Dash");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_PlayerJumpRelease, "GameplayCue.RDCA.Player.JumpRelease");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_PlayerDamaged, "GameplayCue.RDCA.Player.Damaged");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_BossBodyRebound, "GameplayCue.RDCA.Player.BossRebound");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_WeakPointHit, "GameplayCue.RDCA.Boss.WeakPointHit");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_BossRoar, "GameplayCue.RDCA.Boss.Roar");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_ShockwaveWarning, "GameplayCue.RDCA.Boss.Shockwave.Warning");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_ShockwaveRelease, "GameplayCue.RDCA.Boss.Shockwave.Release");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_LaserWarning, "GameplayCue.RDCA.Boss.Laser.Warning");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_LaserIgnite, "GameplayCue.RDCA.Boss.Laser.Ignite");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_LaserLoop, "GameplayCue.RDCA.Boss.Laser.Loop");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_LaserEnd, "GameplayCue.RDCA.Boss.Laser.End");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_BarrageCharge, "GameplayCue.RDCA.Boss.Barrage.Charge");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_BarrageShot01, "GameplayCue.RDCA.Boss.Barrage.Shot01");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_BarrageShot02, "GameplayCue.RDCA.Boss.Barrage.Shot02");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_BarrageShot03, "GameplayCue.RDCA.Boss.Barrage.Shot03");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_BarragePlayerHit, "GameplayCue.RDCA.Boss.Barrage.PlayerHit");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_BossTakeoff, "GameplayCue.RDCA.Boss.Takeoff");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_CyberCenterFracture, "GameplayCue.RDCA.Arena.CenterFracture");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_AnchorEmerge, "GameplayCue.RDCA.Anchor.Emerge");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_AnchorAttach, "GameplayCue.RDCA.Anchor.Attach");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_AnchorOverloadLoop, "GameplayCue.RDCA.Anchor.OverloadLoop");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_AnchorLaunch, "GameplayCue.RDCA.Anchor.Launch");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_AnchorShatter, "GameplayCue.RDCA.Anchor.Shatter");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_PhaseTransitionStart, "GameplayCue.RDCA.Arena.Transition.Start");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_PhaseTransitionLoop, "GameplayCue.RDCA.Arena.Transition.Loop");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_PhaseTransitionEnd, "GameplayCue.RDCA.Arena.Transition.End");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_BossDeath, "GameplayCue.RDCA.Boss.Death");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_Victory, "GameplayCue.RDCA.Result.Victory");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_Defeat, "GameplayCue.RDCA.Result.Defeat");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_BossMusicMain, "GameplayCue.RDCA.Music.Boss");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_BarrageShot, "GameplayCue.RDCA.Boss.Barrage.Shot");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_LaserFlame, "GameplayCue.RDCA.Boss.Laser.Flame");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_ShockwaveVisual, "GameplayCue.RDCA.Boss.Shockwave.Visual");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_ShockwaveSecondary, "GameplayCue.RDCA.Boss.Shockwave.SecondaryVisual");

AActor* ResolveTarget(const UObject* Context)
{
	if (const AActor* Actor = Cast<AActor>(Context)) { return const_cast<AActor*>(Actor); }
	if (const UActorComponent* Component = Cast<UActorComponent>(Context)) { return Component->GetOwner(); }
	return nullptr;
}

TStrongObjectPtr<URDCAPresentationRequest> MakeRequest(ERDCAAudioCue Cue)
{
	TStrongObjectPtr<URDCAPresentationRequest> Request(NewObject<URDCAPresentationRequest>());
	Request->AudioCue = Cue;
	Request->Tag = RDCAPresentation::GetAudioTag(Cue);
	return Request;
}

URDCAPresentationComponent* GetPresentation(const UObject* Context)
{
	return URDCAPresentationComponent::FindOrCreate(ResolveTarget(Context));
}
}

namespace RDCAPresentationInternal
{
UGameplayCueManager* GetCueManager(FGameplayTag RequiredTag)
{
	UGameplayCueManager* Manager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	if (!Manager) { return nullptr; }
	UGameplayCueSet* Set = Manager->GetRuntimeCueSet();
	// Register native classes explicitly: no Blueprint cue assets or broad content scans required.
	if (Set && !Set->GameplayCueDataMap.Contains(RequiredTag))
	{
		TArray<FGameplayCueReferencePair> Cues;
		const FSoftObjectPath ClassPath(URDCACombatCueNotify::StaticClass());
		Cues.Emplace(Tag_PlayerDash, ClassPath);
		Cues.Emplace(Tag_PlayerJumpRelease, ClassPath);
		Cues.Emplace(Tag_PlayerDamaged, ClassPath);
		Cues.Emplace(Tag_BossBodyRebound, ClassPath);
		Cues.Emplace(Tag_WeakPointHit, ClassPath);
		Cues.Emplace(Tag_BossRoar, ClassPath);
		Cues.Emplace(Tag_ShockwaveWarning, ClassPath);
		Cues.Emplace(Tag_ShockwaveRelease, ClassPath);
		Cues.Emplace(Tag_LaserWarning, ClassPath);
		Cues.Emplace(Tag_LaserIgnite, ClassPath);
		Cues.Emplace(Tag_LaserLoop, ClassPath);
		Cues.Emplace(Tag_LaserEnd, ClassPath);
		Cues.Emplace(Tag_BarrageCharge, ClassPath);
		Cues.Emplace(Tag_BarrageShot01, ClassPath);
		Cues.Emplace(Tag_BarrageShot02, ClassPath);
		Cues.Emplace(Tag_BarrageShot03, ClassPath);
		Cues.Emplace(Tag_BarragePlayerHit, ClassPath);
		Cues.Emplace(Tag_BossTakeoff, ClassPath);
		Cues.Emplace(Tag_CyberCenterFracture, ClassPath);
		Cues.Emplace(Tag_AnchorEmerge, ClassPath);
		Cues.Emplace(Tag_AnchorAttach, ClassPath);
		Cues.Emplace(Tag_AnchorOverloadLoop, ClassPath);
		Cues.Emplace(Tag_AnchorLaunch, ClassPath);
		Cues.Emplace(Tag_AnchorShatter, ClassPath);
		Cues.Emplace(Tag_PhaseTransitionStart, ClassPath);
		Cues.Emplace(Tag_PhaseTransitionLoop, ClassPath);
		Cues.Emplace(Tag_PhaseTransitionEnd, ClassPath);
		Cues.Emplace(Tag_BossDeath, ClassPath);
		Cues.Emplace(Tag_Victory, ClassPath);
		Cues.Emplace(Tag_Defeat, ClassPath);
		Cues.Emplace(Tag_BossMusicMain, ClassPath);
		for (const FGameplayTag Tag : {Tag_BarrageShot.GetTag(), Tag_LaserFlame.GetTag(), Tag_ShockwaveVisual.GetTag(), Tag_ShockwaveSecondary.GetTag()})
		{
			Cues.Emplace(Tag, ClassPath);
		}
		Set->AddCues(Cues);
		for (FGameplayCueNotifyData& Data : Set->GameplayCueData)
		{
			if (Data.GameplayCueNotifyObj == ClassPath) { Data.LoadedGameplayCueClass = URDCACombatCueNotify::StaticClass(); }
		}
	}
	return Manager;
}
}

FGameplayTag RDCAPresentation::GetAudioTag(const ERDCAAudioCue Cue)
{
	switch (Cue)
	{
	case ERDCAAudioCue::PlayerDash: return Tag_PlayerDash;
	case ERDCAAudioCue::PlayerJumpRelease: return Tag_PlayerJumpRelease;
	case ERDCAAudioCue::PlayerDamaged: return Tag_PlayerDamaged;
	case ERDCAAudioCue::BossBodyRebound: return Tag_BossBodyRebound;
	case ERDCAAudioCue::WeakPointHit: return Tag_WeakPointHit;
	case ERDCAAudioCue::BossRoar: return Tag_BossRoar;
	case ERDCAAudioCue::ShockwaveWarning: return Tag_ShockwaveWarning;
	case ERDCAAudioCue::ShockwaveRelease: return Tag_ShockwaveRelease;
	case ERDCAAudioCue::LaserWarning: return Tag_LaserWarning;
	case ERDCAAudioCue::LaserIgnite: return Tag_LaserIgnite;
	case ERDCAAudioCue::LaserLoop: return Tag_LaserLoop;
	case ERDCAAudioCue::LaserEnd: return Tag_LaserEnd;
	case ERDCAAudioCue::BarrageCharge: return Tag_BarrageCharge;
	case ERDCAAudioCue::BarrageShot01: return Tag_BarrageShot01;
	case ERDCAAudioCue::BarrageShot02: return Tag_BarrageShot02;
	case ERDCAAudioCue::BarrageShot03: return Tag_BarrageShot03;
	case ERDCAAudioCue::BarragePlayerHit: return Tag_BarragePlayerHit;
	case ERDCAAudioCue::BossTakeoff: return Tag_BossTakeoff;
	case ERDCAAudioCue::CyberCenterFracture: return Tag_CyberCenterFracture;
	case ERDCAAudioCue::AnchorEmerge: return Tag_AnchorEmerge;
	case ERDCAAudioCue::AnchorAttach: return Tag_AnchorAttach;
	case ERDCAAudioCue::AnchorOverloadLoop: return Tag_AnchorOverloadLoop;
	case ERDCAAudioCue::AnchorLaunch: return Tag_AnchorLaunch;
	case ERDCAAudioCue::AnchorShatter: return Tag_AnchorShatter;
	case ERDCAAudioCue::PhaseTransitionStart: return Tag_PhaseTransitionStart;
	case ERDCAAudioCue::PhaseTransitionLoop: return Tag_PhaseTransitionLoop;
	case ERDCAAudioCue::PhaseTransitionEnd: return Tag_PhaseTransitionEnd;
	case ERDCAAudioCue::BossDeath: return Tag_BossDeath;
	case ERDCAAudioCue::Victory: return Tag_Victory;
	case ERDCAAudioCue::Defeat: return Tag_Defeat;
	case ERDCAAudioCue::BossMusicMain: return Tag_BossMusicMain;
	default: return FGameplayTag();
	}
}

void RDCAPresentation::PlayAtLocation(const UObject* Context, ERDCAAudioCue Cue, const FVector& Location, float Volume, float Pitch)
{
	if (URDCAPresentationComponent* Target = GetPresentation(Context))
	{
		auto Request = MakeRequest(Cue);
		Request->Location = Location; Request->Volume = Volume; Request->Pitch = Pitch;
		Target->Execute(Request.Get());
	}
}
void RDCAPresentation::Play2D(const UObject* Context, ERDCAAudioCue Cue, float Volume, float Pitch)
{
	if (URDCAPresentationComponent* Target = GetPresentation(Context))
	{
		auto Request = MakeRequest(Cue);
		Request->Kind = ERDCAPresentationKind::Audio2D;
		Request->Volume = Volume; Request->Pitch = Pitch;
		Target->Execute(Request.Get());
	}
}
UAudioComponent* RDCAPresentation::Spawn2D(const UObject* Context, ERDCAAudioCue Cue, float Volume, float Pitch, bool bLoop)
{
	if (URDCAPresentationComponent* Target = GetPresentation(Context))
	{
		auto Request = MakeRequest(Cue);
		Request->Kind = ERDCAPresentationKind::SpawnAudio2D;
		Request->Volume = Volume; Request->Pitch = Pitch; Request->bLoop = bLoop;
		if (bLoop) { return Target->Start(Request.Get()); }
		Target->Execute(Request.Get());
		return Request->Audio;
	}
	return nullptr;
}
UAudioComponent* RDCAPresentation::SpawnLoopAttached(ERDCAAudioCue Cue, USceneComponent* Attach, float Volume, float Pitch)
{
	if (URDCAPresentationComponent* Target = GetPresentation(Attach))
	{
		auto Request = MakeRequest(Cue);
		Request->SceneComponent = Attach; Request->Volume = Volume; Request->Pitch = Pitch; Request->bLoop = true;
		return Target->Start(Request.Get());
	}
	return nullptr;
}
UAudioComponent* RDCAPresentation::SpawnLoopAtLocation(const UObject* Context, ERDCAAudioCue Cue, const FVector& Location, float Volume, float Pitch)
{
	if (URDCAPresentationComponent* Target = GetPresentation(Context))
	{
		auto Request = MakeRequest(Cue);
		Request->Location = Location; Request->Volume = Volume; Request->Pitch = Pitch; Request->bLoop = true;
		return Target->Start(Request.Get());
	}
	return nullptr;
}
void RDCAPresentation::StopLoop(const UObject* Context, ERDCAAudioCue Cue, UAudioComponent*& Audio, float FadeOutTime)
{
	if (AActor* Actor = ResolveTarget(Context))
	{
		if (URDCAPresentationComponent* Target = Actor->FindComponentByClass<URDCAPresentationComponent>())
		{
			if (Target->HasActiveCue(GetAudioTag(Cue)))
			{
				Target->Stop(GetAudioTag(Cue), FadeOutTime);
				Audio = nullptr;
				return;
			}
		}
	}
	// Also tolerate an old/untracked component during migration.
	RDCAAudio::StopLoop(Audio, FadeOutTime);
}
void RDCAPresentation::PlayNextBarrageShot(const UObject* Context, const FVector& Location, float Volume)
{
	if (URDCAPresentationComponent* Target = GetPresentation(Context))
	{
		auto Request = MakeRequest(ERDCAAudioCue::BarrageShot01);
		Request->Tag = Tag_BarrageShot; Request->Kind = ERDCAPresentationKind::BarrageShot;
		Request->Location = Location; Request->Volume = Volume;
		Target->Execute(Request.Get());
	}
}
void RDCAPresentation::PlayerDamaged(UPlayerHealthComponent* Health, int32 Damage)
{
	if (URDCAPresentationComponent* Target = GetPresentation(Health))
	{
		auto Request = MakeRequest(ERDCAAudioCue::PlayerDamaged);
		Request->Kind = ERDCAPresentationKind::PlayerHit;
		Request->Health = Health; Request->Damage = Damage;
		Request->Location = Health->GetOwner()->GetActorLocation(); Request->Volume = 0.55f;
		Target->Execute(Request.Get());
	}
}
void RDCAPresentation::SetLaserFlame(UNiagaraComponent* Effect, bool bActive, bool bImmediate)
{
	if (URDCAPresentationComponent* Target = GetPresentation(Effect))
	{
		if (!bActive)
		{
			if (Target->HasActiveCue(Tag_LaserFlame))
			{
				Target->Stop(Tag_LaserFlame, bImmediate ? 0.0f : 0.08f);
			}
			else
			{
				// Also stop an authored auto-active Niagara before its first warning.
				auto Request = MakeRequest(ERDCAAudioCue::LaserLoop);
				Request->Tag = Tag_LaserFlame; Request->Kind = ERDCAPresentationKind::LaserFlame;
				Request->SceneComponent = Effect; Request->bImmediate = bImmediate;
				Target->Execute(Request.Get());
			}
			return;
		}
		auto Request = MakeRequest(ERDCAAudioCue::LaserLoop);
		Request->Tag = Tag_LaserFlame; Request->Kind = ERDCAPresentationKind::LaserFlame;
		Request->SceneComponent = Effect; Request->bVisible = true; Request->bLoop = true;
		Target->Start(Request.Get());
	}
}
void RDCAPresentation::SetShockwaveVisible(USceneComponent* Visual, bool bVisible)
{
	if (URDCAPresentationComponent* Target = GetPresentation(Visual))
	{
		auto Request = MakeRequest(ERDCAAudioCue::ShockwaveRelease);
		Request->Tag = Visual->GetName().StartsWith(TEXT("Secondary")) ? Tag_ShockwaveSecondary.GetTag() : Tag_ShockwaveVisual.GetTag();
		Request->Kind = ERDCAPresentationKind::ShockwaveVisual; Request->SceneComponent = Visual; Request->bVisible = bVisible;
		Target->Execute(Request.Get());
	}
}
