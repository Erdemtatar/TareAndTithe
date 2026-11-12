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
class UMonolithContractDataAsset;
class UMonolithSliceComponent;
class USceneComponent;
class UShiftProgressionDataAsset;
class UStaticMesh;
class UStaticMeshComponent;
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
		const FVector& PlaneNormalWorld) override;

	virtual FSliceResultSummary CommitSlicePlane_Implementation(
		const FVector& PlaneOriginWorld,
		const FVector& PlaneNormalWorld) override;

	virtual FBox GetTargetWorldBounds_Implementation() const override;

	// ITelemetryPublisherInterface
	virtual FOnTelemetryStreamSignature& GetTelemetryStreamDelegate() override { return OnTelemetryStream; }
	virtual FOnTelemetryImpulseSignature& GetTelemetryImpulseDelegate() override { return OnTelemetryImpulse; }

	bool LoadContractOnRig(
		const UMonolithContractDataAsset* Contract,
		const UShiftProgressionDataAsset* ShiftConfig,
		float& OutInitialMassKg);

	FOnTelemetryStreamSignature OnTelemetryStream;
	FOnTelemetryImpulseSignature OnTelemetryImpulse;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rig")
	TObjectPtr<USceneComponent> ClampPivotRoot;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rig|Materials", meta = (DisplayName = "Outer Shell Override Material (M_SlagOuter_Master)"))
	TObjectPtr<UMaterialInterface> OuterShellOverrideMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rig|Materials", meta = (DisplayName = "Molten Cut Cap Override Material (M_MoltenCutCap_Master)"))
	TObjectPtr<UMaterialInterface> MoltenCutCapOverrideMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig|Greybox")
	FLinearColor GreyboxOuterShellColor = FLinearColor(0.018f, 0.020f, 0.024f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig|Greybox")
	FLinearColor GreyboxMoltenCapColor = FLinearColor(1.00f, 0.30f, 0.015f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig|Greybox")
	FLinearColor GreyboxMoltenCapEmissiveColor = FLinearColor(2.35f, 0.52f, 0.02f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig|Greybox")
	FLinearColor GreyboxClampJawColor = FLinearColor(0.075f, 0.080f, 0.090f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig|Telemetry", meta = (ClampMin = "0.05", ClampMax = "5.0"))
	float VaporizedImpulseReferenceMassKg = 0.5f;

private:
	void EnsureGreyboxMaterialsInitialized();
	UMaterialInterface* ResolveEffectiveOuterMaterial() const;
	UMaterialInterface* ResolveEffectiveCutCapMaterial() const;
	FVector TransformRemainderCoMToMountLocal(const FVector& RemainderLocalCoMCm) const;
	float QueryCurrentRemainderMassKg() const;
	FVector QueryRigWorldLocation() const;

	void HandleChildTelemetryStream(ETelemetryStreamId StreamId, float NormalizedValue);
	void HandleChildTelemetryImpulse(ETelemetryImpulseId ImpulseId, float NormalizedMagnitude, const FVector& WorldLocation);
	void HandleChunkSpawnedFromSlice(ASeveredChunkActor* SpawnedChunk);
	void HandleSliverVaporized(float VaporizedMassKg, const FVector& WorldLocation);
	void HandleClampStructuralSnap();

	UPROPERTY()
	TObjectPtr<UStaticMesh> FallbackCubeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseGreyboxShapeMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> AutoResolvedOuterMasterMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> AutoResolvedCutCapMasterMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FallbackOuterMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FallbackCutCapMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ClampJawMID;
};
