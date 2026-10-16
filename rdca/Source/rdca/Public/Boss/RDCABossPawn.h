#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Pawn.h"
#include "RDCABossPawn.generated.h"

/**
 * Reparenting bridge for the existing Actor-based Boss Blueprint.
 * Intentionally supplies no root, mesh, collision, movement or second ASC:
 * the Blueprint retains its authored components and Encounter remains the
 * scheduler until the behavior-tree migration is explicitly enabled.
 */
UCLASS(Blueprintable, meta = (ChildCanTick))
class RDCA_API ARDCABossPawn : public APawn, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ARDCABossPawn();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	DECLARE_MULTICAST_DELEGATE(FOnAIReady);
	FOnAIReady OnAIReady;
	bool IsAIReady() const { return bAIReady; }
protected:
	virtual void BeginPlay() override;
private:
	bool bAIReady = false;
};
