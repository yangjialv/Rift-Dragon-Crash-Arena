#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "RDCAHealthEffects.generated.h"

/** Instant health subtraction, with magnitude supplied by the damage caller. */
UCLASS()
class RDCA_API URDCADamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URDCADamageEffect();
};

/** Timed post-hit immunity. Each application supplies its duration and tag. */
UCLASS()
class RDCA_API URDCAInvulnerabilityEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URDCAInvulnerabilityEffect();
};
