#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "AttachSurfaceComponent.generated.h"

UENUM(BlueprintType)
enum class EAttachBoxFace : uint8
{
	PositiveX,
	NegativeX,
	PositiveY,
	NegativeY,
	PositiveZ,
	NegativeZ
};

UCLASS(
	ClassGroup = (Arena),
	meta = (BlueprintSpawnableComponent, DisplayName = "Attachable Box"))
class RDCA_API UAttachSurfaceComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UAttachSurfaceComponent(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Attach Surface")
	EAttachBoxFace GetFaceFromWorldNormal(const FVector& WorldNormal) const;

	UFUNCTION(BlueprintPure, Category = "Attach Surface")
	FVector GetFaceNormalLocal(EAttachBoxFace Face) const;

	UFUNCTION(BlueprintPure, Category = "Attach Surface")
	FVector GetFaceNormalWorld(EAttachBoxFace Face) const;

	UFUNCTION(BlueprintPure, Category = "Attach Surface")
	FVector ClampPointToFace(
		const FVector& LocalPoint,
		EAttachBoxFace Face) const;
};
