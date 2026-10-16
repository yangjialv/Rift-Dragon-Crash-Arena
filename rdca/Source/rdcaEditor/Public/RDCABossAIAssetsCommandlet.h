#pragma once
#include "Commandlets/Commandlet.h"
#include "RDCABossAIAssetsCommandlet.generated.h"

/** Targeted, non-overwriting visual BT asset authoring utility. */
UCLASS()
class URDCABossAIAssetsCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 URDCABossAIAssetsCommandlet();
 virtual int32 Main(const FString& Params) override;
};
