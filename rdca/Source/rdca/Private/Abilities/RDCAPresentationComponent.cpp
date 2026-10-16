#include "Abilities/RDCAPresentationComponent.h"

#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "GameplayCueManager.h"
#include "NiagaraComponent.h"
#include "Player/PlayerHealthComponent.h"
#include "UObject/StrongObjectPtr.h"

namespace RDCAPresentationInternal { UGameplayCueManager* GetCueManager(FGameplayTag RequiredTag); }

namespace
{
FGameplayCueParameters MakeParameters(URDCAPresentationRequest* Request)
{
	FGameplayCueParameters Parameters;
	Parameters.SourceObject = Request;
	Parameters.Location = Request->Location;
	Parameters.RawMagnitude = Request->Kind == ERDCAPresentationKind::PlayerHit ? Request->Damage : Request->Volume;
	return Parameters;
}
}

URDCAPresentationComponent::URDCAPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

URDCAPresentationComponent* URDCAPresentationComponent::FindOrCreate(AActor* Target)
{
	if (!IsValid(Target)) { return nullptr; }
	URDCAPresentationComponent* Component = Target->FindComponentByClass<URDCAPresentationComponent>();
	if (!Component)
	{
		Component = NewObject<URDCAPresentationComponent>(Target, TEXT("GameplayCuePresentation"));
		Target->AddInstanceComponent(Component);
		Component->RegisterComponent();
	}
	return Component;
}

void URDCAPresentationComponent::Execute(URDCAPresentationRequest* Request)
{
	if (!Request || !Request->Tag.IsValid()) { return; }
	if (UGameplayCueManager* Manager = RDCAPresentationInternal::GetCueManager(Request->Tag))
	{
		Manager->HandleGameplayCue(GetOwner(), Request->Tag, EGameplayCueEvent::Executed, MakeParameters(Request));
	}
}

UAudioComponent* URDCAPresentationComponent::Start(URDCAPresentationRequest* Request)
{
	if (!Request || !Request->Tag.IsValid()) { return nullptr; }
	if (const TObjectPtr<URDCAPresentationRequest>* Existing = ActiveRequests.Find(Request->Tag))
	{
		if ((*Existing)->Kind != ERDCAPresentationKind::SpawnAudio2D
			|| (IsValid((*Existing)->Audio) && (*Existing)->Audio->IsPlaying()))
		{
			return (*Existing)->Audio;
		}
		Stop(Request->Tag, 0.0f); // Allows a failed/no-audio BGM request to retry without accumulating tags.
	}
	Request->bActive = true;
	ActiveRequests.Add(Request->Tag, Request);
	if (RDCAPresentationInternal::GetCueManager(Request->Tag))
	{
		UGameplayCueManager::AddGameplayCue_NonReplicated(GetOwner(), Request->Tag, MakeParameters(Request));
	}
	return Request->Audio;
}

void URDCAPresentationComponent::Stop(FGameplayTag Tag, float FadeOut)
{
	TObjectPtr<URDCAPresentationRequest> Request;
	if (!ActiveRequests.RemoveAndCopyValue(Tag, Request)) { return; }
	TStrongObjectPtr<URDCAPresentationRequest> KeepAlive(Request.Get());
	Request->bActive = false;
	Request->FadeOut = FadeOut;
	const FGameplayCueParameters Parameters = MakeParameters(Request);
	UGameplayCueManager::RemoveGameplayCue_NonReplicated(GetOwner(), Tag, Parameters);
	// Cue Manager may suppress a target already being destroyed. Cleanup must still run.
	if (!Request->bRemovedHandled) { HandleCue(EGameplayCueEvent::Removed, Parameters); }
}

void URDCAPresentationComponent::HandleCue(EGameplayCueEvent::Type Event, const FGameplayCueParameters& Parameters)
{
	URDCAPresentationRequest* Request = Cast<URDCAPresentationRequest>(const_cast<UObject*>(Parameters.SourceObject.Get()));
	if (!Request) { return; }
	if (Event == EGameplayCueEvent::Removed)
	{
		if (Request->bRemovedHandled) { return; }
		Request->bRemovedHandled = true;
		OnCueHandled.Broadcast(Request->Tag, Event);
		UAudioComponent* Audio = Request->Audio;
		RDCAAudio::StopLoop(Audio, Request->FadeOut);
		Request->Audio = nullptr;
		if (UNiagaraComponent* Effect = Cast<UNiagaraComponent>(Request->SceneComponent))
		{
			if (Request->FadeOut <= 0.0f) { Effect->DeactivateImmediate(); }
			else { Effect->Deactivate(); }
		}
		return;
	}
	if (Event == EGameplayCueEvent::WhileActive) { return; } // OnActive is the single start.
	OnCueHandled.Broadcast(Request->Tag, Event);
	if (Event == EGameplayCueEvent::OnActive && !Request->bActive) { return; }
	if (Request->Kind == ERDCAPresentationKind::LaserFlame)
	{
		if (UNiagaraComponent* Effect = Cast<UNiagaraComponent>(Request->SceneComponent))
		{
			if (!Request->bVisible)
			{
				if (Request->bImmediate) { Effect->DeactivateImmediate(); }
				else { Effect->Deactivate(); }
			}
			else if (Effect->GetAsset()) { Effect->Activate(true); }
		}
		return;
	}
	if (Request->Kind == ERDCAPresentationKind::ShockwaveVisual)
	{
		if (Request->SceneComponent)
		{
			Request->SceneComponent->SetVisibility(Request->bVisible);
			Request->SceneComponent->SetHiddenInGame(!Request->bVisible);
		}
		return;
	}
	if (Request->bLoop)
	{
		if (Request->Kind == ERDCAPresentationKind::SpawnAudio2D)
		{
			Request->Audio = RDCAAudio::Spawn2D(this, Request->AudioCue, Request->Volume, Request->Pitch, true);
		}
		else if (Request->SceneComponent)
		{
			Request->Audio = RDCAAudio::SpawnLoopAttached(Request->AudioCue, Request->SceneComponent, Request->Volume, Request->Pitch);
		}
		else { Request->Audio = RDCAAudio::SpawnLoopAtLocation(this, Request->AudioCue, Request->Location, Request->Volume, Request->Pitch); }
	}
	else if (Request->Kind == ERDCAPresentationKind::Audio2D)
	{
		RDCAAudio::Play2D(this, Request->AudioCue, Request->Volume, Request->Pitch);
	}
	else if (Request->Kind == ERDCAPresentationKind::SpawnAudio2D)
	{
		Request->Audio = RDCAAudio::Spawn2D(this, Request->AudioCue, Request->Volume, Request->Pitch, false);
	}
	else if (Request->Kind == ERDCAPresentationKind::BarrageShot)
	{
		RDCAAudio::PlayNextBarrageShot(this, Request->Location, Request->Volume);
	}
	else
	{
		RDCAAudio::PlayAtLocation(this, Request->AudioCue, Request->Location, Request->Volume, Request->Pitch);
		if (Request->Kind == ERDCAPresentationKind::PlayerHit && Request->Health)
		{
			Request->Health->PlayDamageFeedback(Request->Damage);
		}
	}
}

void URDCAPresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TArray<FGameplayTag> Tags;
	ActiveRequests.GetKeys(Tags);
	for (const FGameplayTag Tag : Tags) { Stop(Tag, 0.0f); }
	Super::EndPlay(EndPlayReason);
}

void URDCACombatCueNotify::HandleGameplayCue(AActor* Target, EGameplayCueEvent::Type Event, const FGameplayCueParameters& Parameters)
{
	if (Target)
	{
		if (URDCAPresentationComponent* Component = Target->FindComponentByClass<URDCAPresentationComponent>())
		{
			Component->HandleCue(Event, Parameters);
		}
	}
}
