// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/TareTitheTypes.h"
#include "Interfaces/QuotaReceiverInterface.h"
#include "Interfaces/TelemetryPublisherInterface.h"
#include "ScaleAssemblyActor.generated.h"

class UBoxComponent;
class UDiverterGateComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMonolithContractDataAsset;
class UScaleWeighingComponent;
class USceneComponent;
class UShiftProgressionDataAsset;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class TAREANDTITHE_API AScaleAssemblyActor
	: public AActor
	, public IQuotaReceiverInterface
	, public ITelemetryPublisherInterface
{
	GENERATED_BODY()

public:
	AScaleAssemblyActor();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// IQuotaReceiverInterface
	virtual float GetSettledMassKg_Implementation() const override;
	virtual bool IsReceiverSettled_Implementation() const override;
	virtual bool HasContaminationFault_Implementation() const override;
	virtual EDiverterRoute GetActiveDiverterRoute_Implementation() const override;
	virtual void ResetReceiverState_Implementation() override;

	// ITelemetryPublisherInterface
	virtual FOnTelemetryStreamSignature& GetTelemetryStreamDelegate() override { return OnTelemetryStream; }
	virtual FOnTelemetryImpulseSignature& GetTelemetryImpulseDelegate() override { return OnTelemetryImpulse; }

	void ConfigureForContract(const UMonolithContractDataAsset* Contract, EDiverterRoute InitialRoute);
	bool RequestToggleDiverterRoute();

	FOnTelemetryStreamSignature OnTelemetryStream;
	FOnTelemetryImpulseSignature OnTelemetryImpulse;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly")
	TObjectPtr<USceneComponent> ScaleRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly")
	TObjectPtr<UStaticMeshComponent> ScaleBaseVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly")
	TObjectPtr<UStaticMeshComponent> LeftHopperGuideVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly")
	TObjectPtr<UStaticMeshComponent> PanPlatform;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly")
	TObjectPtr<UBoxComponent> PanRimFrontBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly")
	TObjectPtr<UBoxComponent> PanRimBackBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly")
	TObjectPtr<UBoxComponent> PanRimLeftBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly")
	TObjectPtr<UBoxComponent> PanRimRightBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly")
	TObjectPtr<UBoxComponent> PanTriggerBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly")
	TObjectPtr<USceneComponent> DiverterFlapperPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly")
	TObjectPtr<UStaticMeshComponent> DiverterFlapperVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly")
	TObjectPtr<UStaticMeshComponent> TareChuteVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly")
	TObjectPtr<UBoxComponent> TareChuteTriggerBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly")
	TObjectPtr<UScaleWeighingComponent> ScaleWeighingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly")
	TObjectPtr<UDiverterGateComponent> DiverterGateComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|Readout")
	TObjectPtr<UTextRenderComponent> HeaderStencilText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|Readout")
	TObjectPtr<UTextRenderComponent> ContractCodeText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|Readout")
	TObjectPtr<UTextRenderComponent> MassReadoutText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|Readout")
	TObjectPtr<UTextRenderComponent> TareReadoutText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|Readout")
	TObjectPtr<UTextRenderComponent> RouteReadoutText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Colors")
	FColor ReadoutAmberColor = FColor(255, 184, 46, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Colors")
	FColor ReadoutGreenColor = FColor(64, 242, 118, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Colors")
	FColor ReadoutRedColor = FColor(255, 58, 52, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Colors")
	FColor ReadoutCyanColor = FColor(82, 218, 255, 255);

private:
	UFUNCTION()
	void HandleContractSnapshotUpdated(const FContractRuntimeSnapshot& Snapshot);

	void HandleActiveContractLoaded(
		const UMonolithContractDataAsset* Contract,
		const UShiftProgressionDataAsset* ShiftConfig,
		int32 ContractIndex);

	void HandleDiverterRouteChanged(EDiverterRoute NewRoute);
	void RefreshRouteReadout();
	void InitializeGreyboxVisualMaterials();
	FVector QueryScaleWorldLocation() const;

	void HandleChildTelemetryStream(ETelemetryStreamId StreamId, float NormalizedValue);
	void HandleChildTelemetryImpulse(ETelemetryImpulseId ImpulseId, float NormalizedMagnitude, const FVector& WorldLocation);

	void HandleChunkLandedOnScale(AActor* PayloadActor, float ChunkMassKg, float UpdatedPanMassKg);
	void HandleChunkBouncedOffScale(AActor* PayloadActor, float ChunkMassKg, float UpdatedPanMassKg);
	void HandleScaleContaminationDetected(AActor* ContaminatedActor);
	void HandleScaleSettledChanged(bool bIsSettled, float SettledPanMassKg);
	void HandleChunkRoutedToTare(AActor* PayloadActor, float MassKg, bool bWasContaminated);

	UPROPERTY()
	TObjectPtr<UStaticMesh> FallbackCubeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseGreyboxShapeMaterial;
};
