#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossSweepLaser.generated.h"

class UBoxComponent;
class UAudioComponent;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class USceneComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class RDCA_API ABossSweepLaser : public AActor
{
	GENERATED_BODY()

public:
	ABossSweepLaser();

	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void InitializeLaser(
		float NewStartYaw,
		float NewEndYaw,
		float NewSweepDuration,
		int32 NewDamage,
		bool bUsePhase2Effect);

	void ActivateLaser();
	void UpdateWarningPose(const FVector& WorldLocation, float WorldYaw);
	void ConfigureSweep(float NewStartYaw, float NewEndYaw);
	float GetCurrentLaserYaw() const { return GetActorRotation().Yaw; }
	bool IsActorInsideWarningArea(const AActor* Candidate) const;

protected:
	UFUNCTION()
	void HandleLaserOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Laser")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Laser")
	TObjectPtr<UBoxComponent> DamageVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Laser")
	TObjectPtr<USceneComponent> BeamRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Laser")
	TObjectPtr<UStaticMeshComponent> LaserVisual;

	/** Stable ground warning driven by Boss facing, independent of head animation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Laser|Warning")
	TObjectPtr<UStaticMeshComponent> GroundWarningVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Laser")
	TObjectPtr<UNiagaraComponent> LaserEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser",
		meta = (ClampMin = "100.0",
			DisplayName = "Laser Length",
			ToolTip = "Full gameplay laser length in world units. Also drives the damage volume and native laser visual."))
	float LaserLength = 1400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser",
		meta = (ClampMin = "1.0",
			DisplayName = "Laser Width",
			ToolTip = "Full gameplay laser width in world units. The damage volume and native laser visual use this exact width."))
	float LaserWidth = 40.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser",
		meta = (ClampMin = "1.0",
			DisplayName = "Laser Height",
			ToolTip = "Full vertical height of the laser damage volume and native laser visual."))
	float LaserHeight = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser|Visual")
	TObjectPtr<UMaterialInterface> WarningMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser|Visual")
	TObjectPtr<UMaterialInterface> ActiveMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser|Visual")
	FVector VisualLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser|Visual")
	FRotator VisualRotationOffset = FRotator::ZeroRotator;

	/** Keep false when Niagara is the complete active flame presentation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser|Visual")
	bool bShowLaserVisualDuringActive = false;

	/**
	 * Optional explicit Phase 1 Niagara system. When left empty, the system
	 * already assigned to LaserEffect in the Blueprint remains in use.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser|Visual|Phase",
		meta = (DisplayName = "Phase 1 Laser Niagara"))
	TObjectPtr<UNiagaraSystem> Phase1LaserNiagara;

	/**
	 * Niagara system used for the Phase 2 laser. It shares the same component,
	 * transform, dimensions and damage volume as the Phase 1 effect.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser|Visual|Phase",
		meta = (DisplayName = "Phase 2 Laser Niagara"))
	TObjectPtr<UNiagaraSystem> Phase2LaserNiagara;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser|Warning")
	TObjectPtr<UMaterialInterface> GroundWarningMaterial;

	/** Distance from the Boss centre to the beginning of the warning corridor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser|Warning",
		meta = (ClampMin = "0.0"))
	float GroundWarningStartOffset = 300.0f;

	/** Full horizontal width of the warning area in world units. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser|Warning",
		meta = (ClampMin = "1.0", DisplayName = "Ground Warning Width"))
	float GroundWarningWidth = 50.0f;

	/** Maximum forward length of the ground warning before arena-floor clipping. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser|Warning",
		meta = (ClampMin = "1.0", DisplayName = "Ground Warning Length"))
	float GroundWarningLength = 1400.0f;

	/** Vertical thickness of the warning slab. Its bottom remains on the arena floor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser|Warning",
		meta = (ClampMin = "0.1", DisplayName = "Ground Warning Thickness"))
	float GroundWarningThickness = 2.0f;

	/** Small separation from the floor used only to prevent depth flicker. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, AdvancedDisplay,
		Category = "Laser|Warning",
		meta = (ClampMin = "0.0", DisplayName = "Ground Warning Surface Offset"))
	float GroundWarningSurfaceOffset = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser|Anchor",
		meta = (ClampMin = "0.0"))
	float AnchorOverloadPerSecond = 0.35f;

private:
	void ApplyDamageToActor(AActor* OtherActor);
	void ApplyAnchorOverload(float DeltaTime);
	void UpdateComponentDimensions();
	void UpdateGroundWarningVisual();
	void ApplyPhaseNiagaraSystem(bool bUsePhase2Effect);
	bool ResolveTaggedGroundHeight(float& OutGroundZ) const;
	void FinishLaserAudio(bool bPlayEndCue);

	TSet<TWeakObjectPtr<AActor>> DamagedActors;
	float StartYaw = 0.0f;
	float EndYaw = 0.0f;
	float SweepDuration = 1.5f;
	float SweepElapsed = 0.0f;
	int32 Damage = 1;
	bool bLaserActive = false;
	bool bLaserEndAudioPlayed = false;
	TObjectPtr<UAudioComponent> LaserLoopAudio;
};
