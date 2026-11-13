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
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMoltenThermalLightComponent;
class UMonolithContractDataAsset;
class UPointLightComponent;
class UPrimitiveComponent;
class UScaleWeighingComponent;
class USceneComponent;
class UShiftProgressionDataAsset;
class UStaticMesh;
class UStaticMeshComponent;
class UTareTithePresentationDataAsset;
class UTextRenderComponent;
enum class EShiftPhase : uint8;

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
	virtual bool TryRecoverPayloadOnReceiver_Implementation(AActor* PayloadActor) override;
	virtual bool IsScalePanSupportComponent_Implementation(const UPrimitiveComponent* CandidateComponent) const override;
	virtual void NotifyPayloadScaleSupportImpact_Implementation(
		AActor* PayloadActor,
		UPrimitiveComponent* PayloadPrimitive,
		const FVector& WorldImpactPoint,
		const FVector& NormalImpulse) override;

	// ITelemetryPublisherInterface
	virtual FOnTelemetryStreamSignature& GetTelemetryStreamDelegate() override { return OnTelemetryStream; }
	virtual FOnTelemetryImpulseSignature& GetTelemetryImpulseDelegate() override { return OnTelemetryImpulse; }

	void ConfigureForContract(const UMonolithContractDataAsset* Contract, EDiverterRoute InitialRoute);
	bool RequestToggleDiverterRoute();
	void ReconcileScaleOverlaps();
	void ApplyPresentationData(const UTareTithePresentationDataAsset* InPresentationData);

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
	TObjectPtr<UBoxComponent> LeftHopperGuideCollider;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly")
	TObjectPtr<USceneComponent> PanMotionRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly")
	TObjectPtr<UStaticMeshComponent> PanPlatformVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly")
	TObjectPtr<UBoxComponent> PanDeckCollider;

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
	TObjectPtr<UBoxComponent> DiverterPlateCollider;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly")
	TObjectPtr<UStaticMeshComponent> TareChuteVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|TareHopper")
	TObjectPtr<UInstancedStaticMeshComponent> TareHopperCollarISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|TareHopper")
	TObjectPtr<UInstancedStaticMeshComponent> TareHopperThroatISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|TareHopper")
	TObjectPtr<UStaticMeshComponent> TareHopperFurnaceGlowCard;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|TareHopper")
	TObjectPtr<UBoxComponent> TareGuideWallOuterRightBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|TareHopper")
	TObjectPtr<UBoxComponent> TareGuideWallFrontBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|TareHopper")
	TObjectPtr<UBoxComponent> TareGuideWallBackBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|TareHopper")
	TObjectPtr<UBoxComponent> TareGuideWallInnerLeftLowerBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|TareHopper")
	TObjectPtr<UBoxComponent> TareGuideFeedApronSlopeBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|TareHopper")
	TObjectPtr<UBoxComponent> TareGuideChamferOuterRightBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|TareHopper")
	TObjectPtr<UBoxComponent> TareGuideChamferFrontBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|TareHopper")
	TObjectPtr<UBoxComponent> TareGuideChamferBackBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly")
	TObjectPtr<UBoxComponent> TareChuteTriggerBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|TareHopper")
	TObjectPtr<UBoxComponent> TareAbyssDespawnTriggerBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly")
	TObjectPtr<UScaleWeighingComponent> ScaleWeighingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly")
	TObjectPtr<UDiverterGateComponent> DiverterGateComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|Lighting")
	TObjectPtr<UPointLightComponent> ScaleWellEmberLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|Lighting")
	TObjectPtr<UMoltenThermalLightComponent> ScaleWellThermalLightComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|Readout")
	TObjectPtr<USceneComponent> ReadoutConsolePivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|Readout")
	TObjectPtr<USceneComponent> ScaleConsoleRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|Readout")
	TObjectPtr<UStaticMeshComponent> ReadoutBackingPlate;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|Readout")
	TObjectPtr<UTextRenderComponent> StatusBannerText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScaleAssembly|Readout")
	TObjectPtr<UTextRenderComponent> ControlStencilText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Trigger", meta = (ClampMin = "1.0", ClampMax = "10.0"))
	float PanDeckTopLocalZCm = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Trigger", meta = (ClampMin = "8.0", ClampMax = "30.0"))
	float PanRimCurbHeightAboveDeckCm = 16.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Trigger", meta = (ClampMin = "10.0", ClampMax = "80.0"))
	float PanTriggerCenterAboveDeckZCm = 43.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Trigger")
	FVector PanTriggerBoxExtentCm = FVector(35.05, 28.05, 49.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Colliders")
	FVector PanDeckBoxExtentCm = FVector(38.0, 31.0, 3.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Colliders")
	FVector DiverterPlateColliderOffsetCm = FVector(0.0, 0.0, 45.5);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Colliders")
	FVector DiverterPlateBoxExtentCm = FVector(42.0, 1.5, 45.5);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Colliders")
	FVector LeftHopperGuideOffsetCm = FVector(0.0, -32.5, 28.5);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Colliders")
	FVector LeftHopperGuideBoxExtentCm = FVector(38.0, 1.5, 16.5);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|TareHopper")
	FVector TareBasinVisualOffsetCm = FVector(0.0, 90.0, -82.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|TareHopper", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float TareBasinVisualXYScaleMultiplier = 0.86f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|TareHopper")
	FVector TareMouthTriggerOffsetCm = FVector(0.0, 87.0, 20.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|TareHopper")
	FVector TareMouthTriggerExtentCm = FVector(45.0, 45.0, 26.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|TareHopper")
	FVector TareAbyssDespawnTriggerOffsetCm = FVector(0.0, 90.0, -92.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|TareHopper")
	FVector TareAbyssDespawnTriggerExtentCm = FVector(46.0, 42.0, 8.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|TareHopper")
	FLinearColor TareFurnaceBaffleEmissiveColor = FLinearColor(1.10f, 0.25f, 0.025f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Trigger", meta = (ClampMin = "2.0", ClampMax = "40.0"))
	float FlapperFilterEngagementAngleDegrees = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Trigger", meta = (ClampMin = "-6.0", ClampMax = "6.0"))
	float FlapperUpperSideSignedMarginCm = -1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Colors")
	FLinearColor ConsoleBezelLinearColor = FLinearColor(0.005f, 0.006f, 0.008f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Colors")
	FLinearColor DarkGunmetalLinearColor = FLinearColor(0.17f, 0.19f, 0.22f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Colors")
	FColor ReadoutHeaderCyanColor = FColor(0, 230, 255, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Colors")
	FColor ReadoutGoldColor = FColor(255, 195, 0, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Colors")
	FColor ReadoutEmeraldColor = FColor(0, 255, 90, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Colors")
	FColor ReadoutCrimsonColor = FColor(255, 45, 45, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Colors")
	FColor ReadoutFaultBannerColor = FColor(255, 55, 35, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Colors")
	FColor ReadoutNormalSkyBlueColor = FColor(90, 210, 255, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Colors")
	FColor ReadoutTareOrangeColor = FColor(255, 135, 0, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Colors")
	FColor ReadoutBriefAmberColor = FColor(255, 185, 25, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Colors")
	FColor ReadoutControlStencilColor = FColor(240, 230, 180, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Readout", meta = (ClampMin = "160.0", ClampMax = "320.0"))
	float ReadoutBackingPlateWidthCm = 220.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Readout", meta = (ClampMin = "3.5", ClampMax = "10.0"))
	float StatusBannerDefaultWorldSize = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Readout", meta = (ClampMin = "2.5", ClampMax = "8.0"))
	float StatusBannerMinWorldSize = 3.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Readout", meta = (ClampMin = "120.0", ClampMax = "310.0"))
	float StatusBannerMaxSafeWidthCm = 202.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Readout", meta = (ClampMin = "2.5", ClampMax = "6.0"))
	float ControlStencilDefaultWorldSize = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Readout", meta = (ClampMin = "2.0", ClampMax = "5.0"))
	float ControlStencilMinWorldSize = 2.9f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScaleAssembly|Readout", meta = (ClampMin = "120.0", ClampMax = "310.0"))
	float ControlStencilMaxSafeWidthCm = 202.0f;

private:
	UFUNCTION()
	void HandleContractSnapshotUpdated(const FContractRuntimeSnapshot& Snapshot);

	UFUNCTION()
	void HandleShiftVerdict(EShiftVerdictType Verdict, EContractFaultReason FaultReason);

	void HandleShiftPhaseChanged(EShiftPhase NewPhase);

	void HandleActiveContractLoaded(
		const UMonolithContractDataAsset* Contract,
		const UShiftProgressionDataAsset* ShiftConfig,
		int32 ContractIndex);

	void HandleDiverterRouteChanged(EDiverterRoute NewRoute);
	void HandleDiverterRouteSlewCompleted(EDiverterRoute ConvergedRoute);
	bool ShouldIgnorePayloadInPanForTareRamp(const AActor* CandidateActor) const;
	void EnforcePanTriggerGeometryAndCollision();
	bool IsLegacyWorldSpaceReadoutEnabled() const;
	void UpdateLegacyConsoleVisibility();
	void RefreshContractHeaderReadout();
	void RefreshStatusBannerReadout();
	void SetStatusBannerTextFitted(const FString& BannerString, const FColor& BannerColor);
	static void FitReadoutLineWidth(UTextRenderComponent* TextComp, float DefaultWorldSize, float MinWorldSize, float MaxSafeWidthCm);
	void RefreshRouteReadout();
	static FString FormatFaultReasonBanner(EContractFaultReason FaultReason);
	void InitializeGreyboxVisualMaterials();
	void RebuildTareHopperVisualInstances();
	void ApplyTareHopperPresentationMaterials();
	void ApplyDarkGunmetalMaterial(UStaticMeshComponent* TargetMesh);
	void ApplyDarkGunmetalMaterialToISM(UInstancedStaticMeshComponent* TargetISM);
	UMaterialInterface* ResolveDarkGunmetalMaterial();
	UMaterialInterface* ResolveRodinHopperCollarMaterial() const;
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

	UPROPERTY()
	TObjectPtr<UMaterialInterface> UnlitReadoutTextMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> AutoResolvedDarkGunmetalMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> AutoResolvedHopperCollarMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TareFurnaceBaffleMID;

	UPROPERTY(Transient)
	TObjectPtr<const UTareTithePresentationDataAsset> PresentationData;

	FContractRuntimeSnapshot CachedSnapshot;
	int32 CachedContractIndex = 0;
	int32 CachedTotalContracts = 6;
	EShiftPhase CachedShiftPhase;
	EShiftVerdictType LatchedVerdict = EShiftVerdictType::ContractPassed;
	EContractFaultReason LatchedFaultReason = EContractFaultReason::None;
	bool bHasLatchedVerdictBanner = false;
};
