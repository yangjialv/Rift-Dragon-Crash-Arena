#include "Boss/RDCABossPawn.h"

#include "Abilities/RDCAAbilitySystemComponent.h"
#include "Boss/AI/RDCABossAIController.h"

ARDCABossPawn::ARDCABossPawn()
{
	// This stage changes eligibility for possession, not who drives the fight.
	// Do not let APawn's default auto-possession install an unrelated AI brain.
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = ARDCABossAIController::StaticClass();
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetReplicateMovement(false);
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
}

UAbilitySystemComponent* ARDCABossPawn::GetAbilitySystemComponent() const
{
	// WeakPoint/Encounter already create and initialize the one shared ASC.
	// A query must neither create a component nor reset health/grant abilities.
	return FindComponentByClass<URDCAAbilitySystemComponent>();
}

void ARDCABossPawn::BeginPlay()
{
	Super::BeginPlay();
	bAIReady = true;
	OnAIReady.Broadcast();
	// Auto-possession usually happened before component initialization.
	if (auto* AI = Cast<ARDCABossAIController>(GetController())) { AI->TryStartCombatBrain(); }
}
