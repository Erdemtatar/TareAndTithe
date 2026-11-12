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

	FOnTelemetryStreamSignature OnTelemetryStream;
	FOnTelemetryImpulseSignature OnTelemetryImpulse;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector")
	TObjectPtr<USceneComponent> PawnRoot;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Laser")
	TObjectPtr<ULaserPlaneAimComponent> LaserPlaneAimComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Laser")
	TObjectPtr<ULaserCapacitorComponent> LaserCapacitorComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Audio")
	TObjectPtr<UAudioTelemetryBridgeComponent> AudioTelemetryBridgeComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Audio")
	TObjectPtr<UAudioComponent> ContinuousMetaSoundAudioComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspector|Audio")
	TObjectPtr<UAudioComponent> ImpulseMetaSoundAudioComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Audio")
	TObjectPtr<USoundBase> ContinuousMetaSoundAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Audio")
	TObjectPtr<USoundBase> ImpulseMetaSoundAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspector|Laser", meta = (DisplayName = "Laser Sheet Override Material (M_LaserSheet_Master)"))
	TObjectPtr<UMaterialInterface> LaserSheetOverrideMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser", meta = (ClampMin = "20.0", ClampMax = "120.0"))
	float LaserFrameDepthCm = 54.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser", meta = (ClampMin = "20.0", ClampMax = "120.0"))
	float LaserFrameWidthCm = 58.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser", meta = (ClampMin = "0.2", ClampMax = "3.0"))
	float LaserFrameBarWidthCm = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Laser", meta = (ClampMin = "0.2", ClampMax = "2.0"))
	float LaserFrameBarThicknessCm = 0.5f;

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
	float LockedAutoExposureBias = -0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Camera|Exposure", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float LockedBloomIntensity = 0.22f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Camera", meta = (ClampMin = "0.0", ClampMax = "25.0"))
	float MaxCameraParallaxYawDegrees = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Camera", meta = (ClampMin = "0.0", ClampMax = "25.0"))
	float MaxCameraParallaxPitchDegrees = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Camera", meta = (ClampMin = "-15.0", ClampMax = "5.0"))
	float MaxUpwardCameraPitchCeilingDegrees = -2.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspector|Camera", meta = (ClampMin = "20.0", ClampMax = "250.0"))
	float ParallaxReferenceHorizontalSpanCm = 75.0f;

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
	void UpdateCameraParallaxFromAim();
	void ApplyNativeScalabilityPreset(int32 QualityLevel, float TsrResolutionScalePercent);

	bool HandleQueryLaserSliceTarget(
		UObject*& OutSliceableTarget,
		FVector& OutPlaneOriginWorld,
		FVector& OutPlaneNormalWorld);

	void HandlePreviewTelemetryUpdated(const FSlicePreviewTelemetry& PreviewTelemetry);
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

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> AutoResolvedLaserSheetMasterMaterial;

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
	float CachedVeinProximityNormalized = 0.0f;
	float CachedLaserChargeNormalized = 0.0f;
	bool bCachedIntersectsMonolith = false;
	bool bIsFastRollSnapHeld = false;
};
