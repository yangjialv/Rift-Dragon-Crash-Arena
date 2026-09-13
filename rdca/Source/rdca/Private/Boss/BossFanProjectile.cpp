#include "Boss/BossFanProjectile.h"

#include "Audio/RDCAAudio.h"
#include "Arena/AnchorOverloadComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Player/PlayerHealthComponent.h"
#include "rdca.h"
#include "UObject/ConstructorHelpers.h"

ABossFanProjectile::ABossFanProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComponent =
		CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(CollisionComponent);
	CollisionComponent->InitSphereRadius(30.0f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	CollisionComponent->OnComponentBeginOverlap.AddDynamic(
		this,
		&ABossFanProjectile::HandleProjectileOverlap);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(CollisionComponent);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetRelativeScale3D(FVector(0.6f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		VisualMesh->SetStaticMesh(SphereMesh.Object);
	}
}

void ABossFanProjectile::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateSteering(DeltaTime);
	if (VisualMesh && VisualSpinDegreesPerSecond > 0.0f)
	{
		VisualMesh->AddLocalRotation(FRotator(
			VisualSpinDegreesPerSecond * 0.37f * DeltaTime,
			VisualSpinDegreesPerSecond * DeltaTime,
			VisualSpinDegreesPerSecond * 0.61f * DeltaTime));
	}

	FVector Movement = TravelDirection * TravelSpeed * DeltaTime;
	if (bGroundSkimming)
	{
		const float CurrentHeight = GetActorLocation().Z;
		if (CurrentHeight <= GroundSkimHeight
			|| CurrentHeight + Movement.Z <= GroundSkimHeight)
		{
			Movement = GroundTravelDirection * TravelSpeed * DeltaTime;
			Movement.Z = GroundSkimHeight - CurrentHeight;
			TravelDirection = GroundTravelDirection;
			SetActorRotation(TravelDirection.Rotation());
		}
	}

	FHitResult Hit;
	RootComponent->MoveComponent(
		Movement,
		GetActorRotation(),
		true,
		&Hit);
	if (Hit.IsValidBlockingHit())
	{
		Destroy();
	}
}

void ABossFanProjectile::UpdateSteering(const float DeltaTime)
{
	if (MotionMode == EBossProjectileMotionMode::Curved)
	{
		const float RemainingCurve = FMath::Max(
			MaximumCurveDegrees - FMath::Abs(AccumulatedCurveDegrees),
			0.0f);
		const float SignedStep = FMath::Sign(CurveDegreesPerSecond)
			* FMath::Min(
				FMath::Abs(CurveDegreesPerSecond) * DeltaTime,
				RemainingCurve);
		if (!FMath::IsNearlyZero(SignedStep))
		{
			TravelDirection = TravelDirection.RotateAngleAxis(
				SignedStep,
				FVector::UpVector).GetSafeNormal();
			AccumulatedCurveDegrees += SignedStep;
			UpdateGroundDirectionFromTravel();
			SetActorRotation(TravelDirection.Rotation());
		}
		return;
	}

	if (MotionMode != EBossProjectileMotionMode::Homing)
	{
		return;
	}

	HomingElapsed += DeltaTime;
	AActor* Target = HomingTarget.Get();
	if (!Target
		|| HomingElapsed >= HomingDuration
		|| FVector::Dist2D(GetActorLocation(), Target->GetActorLocation())
			<= HomingStopDistance)
	{
		MotionMode = EBossProjectileMotionMode::Straight;
		return;
	}

	FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	if (bGroundSkimming && GetActorLocation().Z <= GroundSkimHeight + 1.0f)
	{
		ToTarget.Z = 0.0f;
	}
	if (!ToTarget.Normalize())
	{
		return;
	}

	const FRotator CurrentRotation = TravelDirection.Rotation();
	const FRotator TargetRotation = ToTarget.Rotation();
	const float MaximumTurnThisFrame = FMath::Max(
		HomingTurnDegreesPerSecond,
		0.0f) * DeltaTime;
	const float TurnedYaw = FMath::FixedTurn(
		CurrentRotation.Yaw,
		TargetRotation.Yaw,
		MaximumTurnThisFrame);
	const float YawFromInitial = FMath::Clamp(
		FMath::FindDeltaAngleDegrees(InitialTravelYaw, TurnedYaw),
		-MaximumHomingAngle,
		MaximumHomingAngle);
	const float TurnedPitch = FMath::FixedTurn(
		CurrentRotation.Pitch,
		TargetRotation.Pitch,
		MaximumTurnThisFrame);
	TravelDirection = FRotator(
		TurnedPitch,
		InitialTravelYaw + YawFromInitial,
		0.0f).Vector().GetSafeNormal();
	UpdateGroundDirectionFromTravel();
	SetActorRotation(TravelDirection.Rotation());
}

void ABossFanProjectile::UpdateGroundDirectionFromTravel()
{
	GroundTravelDirection = FVector(
		TravelDirection.X,
		TravelDirection.Y,
		0.0f).GetSafeNormal();
	if (GroundTravelDirection.IsNearlyZero())
	{
		GroundTravelDirection = FVector::ForwardVector;
	}
}

void ABossFanProjectile::InitializeGroundSkimmingProjectile(
	const FVector& WorldDirection,
	const float NewSpeed,
	const int32 NewDamage,
	const float WorldCruiseHeight)
{
	InitializeProjectile(WorldDirection, NewSpeed, NewDamage);
	UpdateGroundDirectionFromTravel();
	GroundSkimHeight = WorldCruiseHeight;
	bGroundSkimming = TravelDirection.Z < 0.0f;
}

void ABossFanProjectile::InitializePatternProjectile(
	const FVector& WorldDirection,
	const float NewSpeed,
	const int32 NewDamage,
	const float WorldCruiseHeight,
	const EBossProjectileMotionMode NewMotionMode,
	const float NewCurveDegreesPerSecond,
	const float NewMaximumCurveDegrees,
	AActor* NewHomingTarget,
	const float NewHomingTurnDegreesPerSecond,
	const float NewHomingDuration,
	const float NewHomingStopDistance,
	const float NewMaximumHomingAngle)
{
	InitializeGroundSkimmingProjectile(
		WorldDirection,
		NewSpeed,
		NewDamage,
		WorldCruiseHeight);
	MotionMode = NewMotionMode;
	CurveDegreesPerSecond = NewCurveDegreesPerSecond;
	MaximumCurveDegrees = FMath::Max(NewMaximumCurveDegrees, 0.0f);
	AccumulatedCurveDegrees = 0.0f;
	HomingTarget = NewHomingTarget;
	HomingTurnDegreesPerSecond = FMath::Max(
		NewHomingTurnDegreesPerSecond,
		0.0f);
	HomingDuration = FMath::Max(NewHomingDuration, 0.0f);
	HomingElapsed = 0.0f;
	HomingStopDistance = FMath::Max(NewHomingStopDistance, 0.0f);
	MaximumHomingAngle = FMath::Clamp(
		NewMaximumHomingAngle,
		0.0f,
		180.0f);
	InitialTravelYaw = TravelDirection.Rotation().Yaw;
}

void ABossFanProjectile::InitializeProjectile(
	const FVector& WorldDirection,
	const float NewSpeed,
	const int32 NewDamage)
{
	TravelDirection = WorldDirection.GetSafeNormal();
	InitialTravelYaw = TravelDirection.Rotation().Yaw;
	MotionMode = EBossProjectileMotionMode::Straight;
	TravelSpeed = FMath::Max(NewSpeed, 1.0f);
	Damage = FMath::Max(NewDamage, 1);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetGenerateOverlapEvents(true);
	CollisionComponent->SetCollisionResponseToChannel(
		ECC_WorldDynamic,
		ECR_Overlap);
	SetLifeSpan(FMath::Max(LifeSeconds, 0.1f));
	SetActorRotation(TravelDirection.Rotation());
}

void ABossFanProjectile::SetCodePhaseVisual(const bool bCodePhase)
{
	if (!VisualMesh)
	{
		return;
	}
	UMaterialInterface* DesiredMaterial = bCodePhase
		? CodePhaseMaterial.Get()
		: CyberPhaseMaterial.Get();
	if (DesiredMaterial)
	{
		VisualMesh->SetMaterial(0, DesiredMaterial);
	}
}

void ABossFanProjectile::HandleProjectileOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (bHasAppliedDamage || !OtherActor)
	{
		return;
	}

	UPlayerHealthComponent* Health =
		OtherActor->FindComponentByClass<UPlayerHealthComponent>();
	if (UAnchorOverloadComponent* AnchorOverload =
			OtherActor->FindComponentByClass<UAnchorOverloadComponent>())
	{
		bHasAppliedDamage = true;
		AnchorOverload->AddOverloadAmount(AnchorOverloadAmount);
		UE_LOG(
			LogRDCAPlayer,
			Log,
			TEXT("Boss fan projectile overloaded anchor. Anchor=%s Amount=%.2f"),
			*GetNameSafe(OtherActor),
			AnchorOverloadAmount);
		Destroy();
		return;
	}

	if (!Health)
	{
		return;
	}

	bHasAppliedDamage = true;
	const bool bDamageApplied = Health->ReceiveDamage(Damage);
	if (bDamageApplied)
	{
		RDCAAudio::PlayAtLocation(
			this,
			ERDCAAudioCue::BarragePlayerHit,
			OtherActor->GetActorLocation(),
			0.48f);
	}
	UE_LOG(
		LogRDCAPlayer,
		Log,
		TEXT("Boss fan projectile hit. Projectile=%s Player=%s DamageApplied=%s"),
		*GetNameSafe(this),
		*GetNameSafe(OtherActor),
		bDamageApplied ? TEXT("true") : TEXT("false"));
	Destroy();
}
