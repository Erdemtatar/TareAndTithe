// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/TareTitheTypes.h"
#include "Interfaces/SliceableTargetInterface.h"
#include "Interfaces/TelemetryPublisherInterface.h"
#include "MonolithRigActor.generated.h"

class ASeveredChunkActor;
class UClampTorqueSolverComponent;
class UDynamicMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMoltenThermalLightComponent;
class UMonolithContractDataAsset;
class UMonolithSliceComponent;
class UPointLightComponent;
class USceneComponent;
class UShiftProgressionDataAsset;
class UStaticMesh;
class UStaticMeshComponent;
class UTareTithePresentationDataAsset;
class UVeinScannerComponent;

UCLASS()
class TAREANDTITHE_API AMonolithRigActor
	: public AActor
	, public ISliceableTargetInterface
	, public ITelemetryPublisherInterface
{
	GENERATED_BODY()

public:
	AMonolithRigActor();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ISliceableTargetInterface
	virtual FSlicePreviewTelemetry PreviewSlicePlane_Implementation(
		const FVector& PlaneOriginWorld,
		const FVector& PlaneNormalWorld,
		const FLaserSliceAperture& SheetAperture) override;

	virtual FSliceResultSummary CommitSlicePlane_Implementation(
		const FVector& PlaneOriginWorld,
		const FVector& PlaneNormalWorld,
		const FLaserSliceAperture& SheetAperture) override;

	virtual FBox GetTargetWorldBounds_Implementation() const override;
	virtual FMonolithCutMaskVolume GetCuttableWorldMask_Implementation() const override;

	// ITelemetryPublisherInterface
	virtual FOnTelemetryStreamSignature& GetTelemetryStreamDelegate() override { return OnTelemetryStream; }
	virtual FOnTelemetryImpulseSignature& GetTelemetryImpulseDelegate() override { return OnTelemetryImpulse; }

	bool LoadContractOnRig(
		const UMonolithContractDataAsset* Contract,
		const UShiftProgressionDataAsset* ShiftConfig,
		float& OutInitialMassKg);

	void ApplyPresentationData(const UTareTithePresentationDataAsset* InPresentationData);

	UFUNCTION(BlueprintPure, Category = "Rig")
	UDynamicMeshComponent* GetRemainderMeshComponent() const { return RemainderMeshComponent; }

	UFUNCTION(BlueprintPure, Category = "Rig")
	UClampTorqueSolverComponent* GetClampTorqueSolverComponent() const { return ClampTorqueSolverComponent; }

	UFUNCTION(BlueprintPure, Category = "Rig|Test")
	int32 GetActiveHydUpgradeTierForTest() const { return static_cast<int32>(CachedHydUpgradeLevel); }

	UFUNCTION(BlueprintPure, Category = "Rig|Test")
	int32 GetActiveScanUpgradeTierForTest() const { return static_cast<int32>(CachedScanUpgradeLevel); }

	FOnTelemetryStreamSignature OnTelemetryStream;
	FOnTelemetryImpulseSignature OnTelemetryImpulse;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rig")
	TObjectPtr<USceneComponent> ClampPivotRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rig")
	TObjectPtr<UStaticMeshComponent> ClampSpringHousingVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rig")
	TObjectPtr<USceneComponent> TiltingMountComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rig")
	TObjectPtr<UStaticMeshComponent> ClampJawVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rig")
	TObjectPtr<UDynamicMeshComponent> RemainderMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rig")
	TObjectPtr<UMonolithSliceComponent> MonolithSliceComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rig")
	TObjectPtr<UVeinScannerComponent> VeinScannerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rig")
	TObjectPtr<UClampTorqueSolverComponent> ClampTorqueSolverComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rig|Lighting")
	TObjectPtr<UPointLightComponent> MoltenCutBounceLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rig|Lighting")
	TObjectPtr<UMoltenThermalLightComponent> MoltenThermalLightComponent;

	// Phase 12-FIX4 [1] HYD & [3] SCAN Pre-Created Stationary Upgrade Hardware Visuals on ClampPivotRoot (NoCollision, CastShadow=false)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rig|Upgrades")
	TObjectPtr<UStaticMeshComponent> HydUpgradeCylinderLeftL1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rig|Upgrades")
	TObjectPtr<UStaticMeshComponent> HydUpgradeCylinderRightL1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rig|Upgrades")
	TObjectPtr<UStaticMeshComponent> HydUpgradeAccumulatorLeftL2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rig|Upgrades")
	TObjectPtr<UStaticMeshComponent> HydUpgradeAccumulatorRightL2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rig|Upgrades")
	TObjectPtr<UStaticMeshComponent> ScanUpgradePodLeftL1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rig|Upgrades")
	TObjectPtr<UStaticMeshComponent> ScanUpgradePodRightL1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rig|Upgrades")
	TObjectPtr<UStaticMeshComponent> ScanUpgradeLensLeftL1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rig|Upgrades")
	TObjectPtr<UStaticMeshComponent> ScanUpgradeLensRightL1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rig|Upgrades")
	TObjectPtr<UStaticMeshComponent> ScanUpgradeArrayLeftL2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rig|Upgrades")
	TObjectPtr<UStaticMeshComponent> ScanUpgradeArrayRightL2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rig|Upgrades")
	TObjectPtr<UStaticMeshComponent> ScanUpgradeEmitterLeftL2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rig|Upgrades")
	TObjectPtr<UStaticMeshComponent> ScanUpgradeEmitterRightL2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rig|Materials", meta = (DisplayName = "Outer Shell Override Material (M_SlagOuter_Master)"))
	TObjectPtr<UMaterialInterface> OuterShellOverrideMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rig|Materials", meta = (DisplayName = "Molten Cut Cap Override Material (M_MoltenCutCap_Master)"))
	TObjectPtr<UMaterialInterface> MoltenCutCapOverrideMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig|Greybox")
	FLinearColor GreyboxOuterShellColor = FLinearColor(0.065f, 0.072f, 0.085f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig|Greybox")
	FLinearColor GreyboxMoltenCapColor = FLinearColor(1.00f, 0.30f, 0.015f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig|Greybox")
	FLinearColor GreyboxMoltenCapEmissiveColor = FLinearColor(1.65f, 0.36f, 0.015f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig|Greybox")
	FLinearColor GreyboxClampJawColor = FLinearColor(0.13f, 0.14f, 0.16f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig|Telemetry", meta = (ClampMin = "0.05", ClampMax = "5.0"))
	float VaporizedImpulseReferenceMassKg = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig|Telemetry", meta = (ClampMin = "0.001", ClampMax = "1.0"))
	float MinPredictiveContactMassKg = 0.01f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig|Telemetry", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float VeinRemainderBoundsToleranceCm = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig|ClampFit", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float JawPocketSeatEmbedZCm = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig|ClampFit", meta = (ClampMin = "0.5", ClampMax = "8.0"))
	float JawProngOuterProtrusionCm = 2.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig|ClampFit", meta = (ClampMin = "0.25", ClampMax = "0.95"))
	float CustomJawProngInnerRatioX = 0.52f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig|ClampFit", meta = (ClampMin = "0.35", ClampMax = "1.0"))
	float CustomJawProngOuterRatioX = 0.83f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig|ClampFit", meta = (ClampMin = "0.25", ClampMax = "0.95"))
	float CustomJawProngInnerRatioY = 0.40f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig|ClampFit", meta = (ClampMin = "0.35", ClampMax = "1.0"))
	float CustomJawProngOuterRatioY = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig|ClampFit", meta = (ClampMin = "14.0", ClampMax = "40.0"))
	float MinAdaptiveJawOuterWidthCm = 22.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig|ClampFit", meta = (ClampMin = "30.0", ClampMax = "80.0"))
	float MaxAdaptiveJawOuterWidthCm = 72.0f;

private:
	void EnsureGreyboxMaterialsInitialized();
	void ApplyUpgradeHardwareMaterials();
	void UpdateRigUpgradeHardwareVisuals(uint8 HydLevel, uint8 ScanLevel);
	void FitClampJawToActiveMonolith();
	UMaterialInterface* ResolveEffectiveOuterMaterial(const UMonolithContractDataAsset* Contract = nullptr) const;
	UMaterialInterface* ResolveEffectiveCutCapMaterial(const UMonolithContractDataAsset* Contract = nullptr) const;
	FVector TransformRemainderCoMToMountLocal(const FVector& RemainderLocalCoMCm) const;
	float QueryCurrentRemainderMassKg() const;
	FVector QueryRigWorldLocation() const;

	void HandleChildTelemetryStream(ETelemetryStreamId StreamId, float NormalizedValue);
	void HandleChildTelemetryImpulse(ETelemetryImpulseId ImpulseId, float NormalizedMagnitude, const FVector& WorldLocation);
	void HandleContractSnapshotUpdated(const FContractRuntimeSnapshot& Snapshot);
	void HandleChunkSpawnedFromSlice(ASeveredChunkActor* SpawnedChunk);
	void HandleSliverVaporized(float VaporizedMassKg, const FVector& WorldLocation);
	void HandleClampStructuralSnap();

	UPROPERTY()
	TObjectPtr<UStaticMesh> FallbackCubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> FallbackCylinderMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseGreyboxShapeMaterial;

	UPROPERTY(Transient)
	TObjectPtr<const UTareTithePresentationDataAsset> PresentationData;

	UPROPERTY(Transient)
	TObjectPtr<const UMonolithContractDataAsset> CachedLoadedContract;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> AutoResolvedOuterMasterMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> AutoResolvedCutCapMasterMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> AutoResolvedUpgradeSteelMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FallbackOuterMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FallbackCutCapMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ClampJawMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FallbackUpgradeSteelMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ScanOpticalSensorMID;

	uint8 CachedHydUpgradeLevel = 0;
	uint8 CachedScanUpgradeLevel = 0;
	bool bHasCustomClampJawMesh = false;
};
