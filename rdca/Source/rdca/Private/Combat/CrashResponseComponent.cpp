#include "Combat/CrashResponseComponent.h"

#include "GameFramework/Pawn.h"

UCrashResponseComponent::UCrashResponseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCrashResponseComponent::NotifyCrashImpact(
	APawn* CrashingPawn,
	const FHitResult& Hit)
{
	OnCrashImpactReceived.Broadcast(CrashingPawn, Response, Hit);
}
