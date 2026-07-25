#include "Arena/AttachSurfaceComponent.h"

#include "GameFramework/Actor.h"

UAttachSurfaceComponent::UAttachSurfaceComponent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BoxExtent = FVector(200.0f, 200.0f, 400.0f);
	BodyInstance.SetCollisionEnabled(ECollisionEnabled::QueryOnly, false);
	BodyInstance.SetObjectType(ECC_WorldDynamic);
	BodyInstance.SetResponseToAllChannels(ECR_Ignore);
	BodyInstance.SetResponseToChannel(ECC_Pawn, ECR_Block);
	BodyInstance.SetResponseToChannel(ECC_Visibility, ECR_Block);
}

EAttachBoxFace UAttachSurfaceComponent::GetFaceFromWorldNormal(
	const FVector& WorldNormal) const
{
	const FVector LocalNormal =
		GetComponentTransform().InverseTransformVectorNoScale(WorldNormal).GetSafeNormal();
	const FVector AbsoluteNormal = LocalNormal.GetAbs();

	if (AbsoluteNormal.X >= AbsoluteNormal.Y && AbsoluteNormal.X >= AbsoluteNormal.Z)
	{
		return LocalNormal.X >= 0.0f
			? EAttachBoxFace::PositiveX
			: EAttachBoxFace::NegativeX;
	}
	if (AbsoluteNormal.Y >= AbsoluteNormal.Z)
	{
		return LocalNormal.Y >= 0.0f
			? EAttachBoxFace::PositiveY
			: EAttachBoxFace::NegativeY;
	}
	return LocalNormal.Z >= 0.0f
		? EAttachBoxFace::PositiveZ
		: EAttachBoxFace::NegativeZ;
}

FVector UAttachSurfaceComponent::GetFaceNormalLocal(
	const EAttachBoxFace Face) const
{
	switch (Face)
	{
	case EAttachBoxFace::PositiveX:
		return FVector::ForwardVector;
	case EAttachBoxFace::NegativeX:
		return -FVector::ForwardVector;
	case EAttachBoxFace::PositiveY:
		return FVector::RightVector;
	case EAttachBoxFace::NegativeY:
		return -FVector::RightVector;
	case EAttachBoxFace::PositiveZ:
		return FVector::UpVector;
	case EAttachBoxFace::NegativeZ:
		return -FVector::UpVector;
	default:
		return FVector::ForwardVector;
	}
}

FVector UAttachSurfaceComponent::GetFaceNormalWorld(
	const EAttachBoxFace Face) const
{
	return GetComponentTransform().TransformVectorNoScale(
		GetFaceNormalLocal(Face)).GetSafeNormal();
}

FVector UAttachSurfaceComponent::ClampPointToFace(
	const FVector& LocalPoint,
	const EAttachBoxFace Face) const
{
	FVector Result = LocalPoint;
	Result.X = FMath::Clamp(Result.X, -BoxExtent.X, BoxExtent.X);
	Result.Y = FMath::Clamp(Result.Y, -BoxExtent.Y, BoxExtent.Y);
	Result.Z = FMath::Clamp(Result.Z, -BoxExtent.Z, BoxExtent.Z);

	switch (Face)
	{
	case EAttachBoxFace::PositiveX:
		Result.X = BoxExtent.X;
		break;
	case EAttachBoxFace::NegativeX:
		Result.X = -BoxExtent.X;
		break;
	case EAttachBoxFace::PositiveY:
		Result.Y = BoxExtent.Y;
		break;
	case EAttachBoxFace::NegativeY:
		Result.Y = -BoxExtent.Y;
		break;
	case EAttachBoxFace::PositiveZ:
		Result.Z = BoxExtent.Z;
		break;
	case EAttachBoxFace::NegativeZ:
		Result.Z = -BoxExtent.Z;
		break;
	default:
		break;
	}
	return Result;
}
