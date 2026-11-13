// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Core/TareTitheTypes.h"
#include "Interfaces/TelemetryPublisherInterface.h"
#include "Subsystems/InspectionShiftSubsystem.h"
#include "InspectorPawn.generated.h"

struct FInputActionValue;
class UAudioComponent;
class UAudioTelemetryBridgeComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UInspectorKinestheticsComponent;
class ULaserCapacitorComponent;
class ULaserPlaneAimComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMonolithContractDataAsset;
class USceneComponent;
class UShiftProgressionDataAsset;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;
class UTareTithePresentationDataAsset;
class UTextRenderComponent;

UCLASS()
class TAREANDTITHE_API AInspectorPawn : public APawn, public ITelemetryPublisherInterface
{
	GENERATED_BODY()

public:
	AInspectorPawn();

	virtual void PawnClientRestart() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ITelemetryPublisherInterface
	virtual FOnTelemetryStreamSignature& GetTelemetryStreamDelegate() override { return OnTelemetryStream; }
	virtual FOnTelemetryImpulseSignature& GetTelemetryImpulseDelegate() override { return OnTelemetryImpulse; }

	void RefreshAimTargetAndFrameFromShift();
	void ApplyPresentationData(const UTareTithePresentationDataAsset* InPresentationData);

	FOnTelemetryStreamSignature OnTelemetryStream;
	FOnTelemetryImpulseSignature OnTelemetryImpulse;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector")
	TObjectPtr<USceneComponent> PawnRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Camera")
	TObjectPtr<USceneComponent> CameraViewPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Camera")
	TObjectPtr<USceneComponent> CameraShakeOffsetComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Camera")
	TObjectPtr<UCameraComponent> InspectorCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Laser")
	TObjectPtr<USceneComponent> LaserPlaneVisualPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Laser")
	TObjectPtr<UStaticMeshComponent> LaserFrameFrontBar;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Laser")
	TObjectPtr<UStaticMeshComponent> LaserFrameBackBar;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Laser")
	TObjectPtr<UStaticMeshComponent> LaserFrameLeftBar;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Laser")
	TObjectPtr<UStaticMeshComponent> LaserFrameRightBar;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Laser")
	TObjectPtr<UStaticMeshComponent> LaserSheetVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Laser")
	TObjectPtr<UStaticMeshComponent> LaserSheetUndersideVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry")
	TObjectPtr<UTextRenderComponent> LaserReticleReadout;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry")
	TObjectPtr<UTextRenderComponent> LaserReticleYieldReadout;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry")
	TObjectPtr<UTextRenderComponent> LaserReticleLoadReadout;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry")
	TObjectPtr<UTextRenderComponent> LaserReticleHazardReadout;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Laser")
	TObjectPtr<ULaserPlaneAimComponent> LaserPlaneAimComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Laser")
	TObjectPtr<ULaserCapacitorComponent> LaserCapacitorComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Kinesthetics")
	TObjectPtr<UInspectorKinestheticsComponent> InspectorKinestheticsComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Audio")
	TObjectPtr<UAudioTelemetryBridgeComponent> AudioTelemetryBridgeComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Audio")
	TObjectPtr<UAudioComponent> ContinuousMetaSoundAudioComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Audio")
	TObjectPtr<UAudioComponent> ImpulseMetaSoundAudioComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Audio")
	TObjectPtr<UAudioComponent> AmbientBedAudioComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Audio")
	TObjectPtr<USoundBase> ContinuousMetaSoundAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Audio")
	TObjectPtr<USoundBase> ImpulseMetaSoundAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspector|Laser", meta = (DisplayName = "Laser Sheet Override Material (M_LaserSheet_Master)"))
	TObjectPtr<UMaterialInterface> LaserSheetOverrideMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser", meta = (ClampMin = "20.0", ClampMax = "120.0"))
	float LaserFrameDepthCm = 72.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser", meta = (ClampMin = "20.0", ClampMax = "120.0"))
	float LaserFrameWidthCm = 68.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser", meta = (ClampMin = "0.2", ClampMax = "3.0"))
	float LaserFrameBarWidthCm = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser", meta = (ClampMin = "0.2", ClampMax = "2.0"))
	float LaserFrameBarThicknessCm = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry", meta = (ClampMin = "20.0", ClampMax = "110.0"))
	float LaserReticleLateralOffsetCm = 52.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry", meta = (ClampMin = "2.0", ClampMax = "24.0"))
	float LaserReticleSheetClearanceGapCm = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry", meta = (ClampMin = "40.0", ClampMax = "200.0"))
	float LaserReticleMaxLateralOffsetCm = 148.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry", meta = (ClampMin = "20.0", ClampMax = "80.0"))
	float LaserReticleFrontOffsetCm = 46.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry", meta = (ClampMin = "2.0", ClampMax = "10.0"))
	float LaserReticlePrimaryWorldSize = 4.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry", meta = (ClampMin = "2.0", ClampMax = "10.0"))
	float LaserReticleYieldWorldSize = 3.9f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry", meta = (ClampMin = "2.0", ClampMax = "10.0"))
	float LaserReticleLoadWorldSize = 3.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry", meta = (ClampMin = "2.0", ClampMax = "10.0"))
	float LaserReticleHazardWorldSize = 3.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry", meta = (ClampMin = "2.0", ClampMax = "10.0"))
	float LaserReticleLineSpacingCm = 4.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry", meta = (ClampMin = "0.001", ClampMax = "1.0"))
	float MinPredictiveContactMassKg = 0.01f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry", meta = (ClampMin = "0.01", ClampMax = "0.5"))
	float LaserReticleVeinWarningThreshold = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry", meta = (ClampMin = "0.1", ClampMax = "0.95"))
	float LaserReticleClampTorqueWarningThreshold = 0.50f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float LaserReticleClampTorqueCriticalThreshold = 0.85f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry")
	FColor ReticleCyanColor = FColor(0, 232, 255, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry")
	FColor ReticleEmeraldColor = FColor(0, 255, 95, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry")
	FColor ReticleCrimsonColor = FColor(255, 45, 45, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry")
	FColor ReticleAmberColor = FColor(255, 175, 20, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry")
	FColor ReticleVeinDropWarningColor = FColor(255, 145, 20, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry")
	FColor ReticlePurgeReadyColor = FColor(0, 250, 165, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser|Telemetry")
	FColor ReticleLoadCellColor = FColor(210, 242, 255, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Camera")
	FVector BaselineCameraRelativeLocation = FVector(-34.0, 0.0, 8.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Camera", meta = (ClampMin = "-25.0", ClampMax = "5.0"))
	float BaselineCameraPitchDegrees = -4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Camera", meta = (ClampMin = "60.0", ClampMax = "115.0"))
	float DefaultCameraFieldOfView = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Camera|Exposure")
	float LockedAutoExposureMinBrightness = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Camera|Exposure")
	float LockedAutoExposureMaxBrightness = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Camera|Exposure", meta = (ClampMin = "-5.0", ClampMax = "5.0"))
	float LockedAutoExposureBias = 0.48f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Camera|Exposure", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float LockedBloomIntensity = 0.42f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Camera|Exposure", meta = (ClampMin = "1.15", ClampMax = "5.0"))
	float LockedBloomThreshold = 1.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Camera", meta = (ClampMin = "0.0", ClampMax = "25.0"))
	float MaxCameraParallaxYawDegrees = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Camera", meta = (ClampMin = "0.0", ClampMax = "25.0"))
	float MaxCameraParallaxPitchDegrees = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Camera", meta = (ClampMin = "-15.0", ClampMax = "5.0"))
	float MaxUpwardCameraPitchCeilingDegrees = -2.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Camera", meta = (ClampMin = "20.0", ClampMax = "250.0"))
	float ParallaxReferenceHorizontalSpanCm = 58.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Camera", meta = (ClampMin = "20.0", ClampMax = "250.0"))
	float ParallaxReferenceVerticalSpanCm = 85.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Controls", meta = (ClampMin = "0.15", ClampMax = "1.5"))
	float ManualResetHoldSeconds = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Input")
	TObjectPtr<UInputMappingContext> AssignedMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Input")
	TObjectPtr<UInputAction> AssignedAimCursorAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Input")
	TObjectPtr<UInputAction> AssignedScrollRollAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Input")
	TObjectPtr<UInputAction> AssignedFastSnapModifierAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Input")
	TObjectPtr<UInputAction> AssignedFireCapacitorAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Input")
	TObjectPtr<UInputAction> AssignedBevelOrCancelAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Input")
	TObjectPtr<UInputAction> AssignedToggleDiverterAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Input")
	TObjectPtr<UInputAction> AssignedStampVerdictAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Input")
	TObjectPtr<UInputAction> AssignedResetContractAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Input")
	TObjectPtr<UInputAction> AssignedScalabilityLowAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Input")
	TObjectPtr<UInputAction> AssignedScalabilityMedAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Input")
	TObjectPtr<UInputAction> AssignedScalabilityHighAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Input")
	TObjectPtr<UInputAction> AssignedScalabilityEpicAction;

private:
	void ApplyLockedCameraExposureSettings();
	void EnsureInputActionsAndMappingInitialized();
	void ApplyMappingContextToLocalPlayer();
	void ConfigureLaserFrameGeometry();
	void InitializeLaserSheetDynamicMaterial();
	void UpdateLaserSheetMaterialVisuals();
	bool HasValidPredictiveSliceContact() const;
	bool DoesCurrentSliceEstimateLandInQuotaWindow() const;
	void UpdateCameraParallaxFromAim();
	void UpdateLaserReticleTransform();
	void UpdateLaserReticleReadoutText();
	void ApplyNativeScalabilityPreset(int32 QualityLevel, float TsrResolutionScalePercent);

	UFUNCTION()
	void HandleContractSnapshotUpdated(const FContractRuntimeSnapshot& Snapshot);

	void HandleFilteredTelemetryStream(ETelemetryStreamId StreamId, float NormalizedValue);

	bool HandleQueryLaserSliceTarget(
		UObject*& OutSliceableTarget,
		FVector& OutPlaneOriginWorld,
		FVector& OutPlaneNormalWorld,
		FLaserSliceAperture& OutSheetAperture);

	void HandlePreviewTelemetryUpdated(const FSlicePreviewTelemetry& PreviewTelemetry);
	void HandleCapacitorChargeStateChanged(bool bIsNowCharging);
	void HandleCapacitorTelemetryStream(ETelemetryStreamId StreamId, float NormalizedValue);
	void HandleCapacitorTelemetryImpulse(ETelemetryImpulseId ImpulseId, float NormalizedMagnitude, const FVector& WorldLocation);
	void HandleShiftPhaseChanged(EShiftPhase NewPhase);
	void HandleActiveContractLoaded(
		const UMonolithContractDataAsset* Contract,
		const UShiftProgressionDataAsset* ShiftConfig,
		int32 ContractIndex);
	void HandleManualResetHoldElapsed();

	// EnhancedInput Callbacks
	void Input_AimCursor(const FInputActionValue& Value);
	void Input_ScrollRoll(const FInputActionValue& Value);
	void Input_FastSnapStarted(const FInputActionValue& Value);
	void Input_FastSnapEnded(const FInputActionValue& Value);
	void Input_FireStarted(const FInputActionValue& Value);
	void Input_FireReleased(const FInputActionValue& Value);
	void Input_BevelOrCancelStarted(const FInputActionValue& Value);
	void Input_BevelOrCancelReleased(const FInputActionValue& Value);
	void Input_ToggleDiverter(const FInputActionValue& Value);
	void Input_StampVerdict(const FInputActionValue& Value);
	void Input_ResetHoldStarted(const FInputActionValue& Value);
	void Input_ResetHoldReleased(const FInputActionValue& Value);
	void Input_ScalabilityLow(const FInputActionValue& Value);
	void Input_ScalabilityMed(const FInputActionValue& Value);
	void Input_ScalabilityHigh(const FInputActionValue& Value);
	void Input_ScalabilityEpic(const FInputActionValue& Value);

	UPROPERTY()
	TObjectPtr<UStaticMesh> FallbackCubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> FallbackPlaneMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseGreyboxPlaneMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> UnlitReticleTextMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> AutoResolvedLaserSheetMasterMaterial;

	UPROPERTY(Transient)
	TObjectPtr<const UTareTithePresentationDataAsset> PresentationData;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LaserSheetMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LaserFrameMID;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> ActiveMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ActiveAimCursorAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ActiveScrollRollAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ActiveFastSnapModifierAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ActiveFireCapacitorAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ActiveBevelOrCancelAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ActiveToggleDiverterAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ActiveStampVerdictAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ActiveResetContractAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ActiveScalabilityLowAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ActiveScalabilityMedAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ActiveScalabilityHighAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ActiveScalabilityEpicAction;

	FTimerHandle ManualResetHoldTimerHandle;
	FShiftHudSnapshot CachedHudSnapshot;
	FSlicePreviewTelemetry CachedPreviewTelemetry;
	float CachedVeinProximityNormalized = 0.0f;
	float CachedClampStrainNormalized = 0.0f;
	float CachedLaserChargeNormalized = 0.0f;
	bool bCachedIntersectsMonolith = false;
	bool bIsFastRollSnapHeld = false;
};
