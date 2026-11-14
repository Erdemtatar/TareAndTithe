// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/TareTitheTypes.h"
#include "Data/TareTithePresentationDataAsset.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/InspectionShiftSubsystem.h"
#include "UI/HudViewModels.h"
#include "InspectorHudBridgeComponent.generated.h"

class FViewport;
class STareTitheHudOverlay;
class UAudioTelemetryBridgeComponent;
class UGameViewportClient;
class ULaserCapacitorComponent;
class ULaserPlaneAimComponent;
class UMonolithContractDataAsset;
class UShiftProgressionDataAsset;

/**
 * Tier-1 (bCanEverTick = false) HUD orchestration bridge.
 * - Mounts pure C++ STareTitheHudOverlay in BeginPlay and deterministically removes it in EndPlay.
 * - Implements P0 > P1 > P2 Schmitt-trigger hysteresis so 60Hz slice-preview updates never flicker.
 * - Subscribes to both ClampStrain and ClampFatigue streams alongside FSlicePreviewTelemetry and FContractRuntimeSnapshot.
 */
UCLASS(ClassGroup = (TareAndTithe), meta = (BlueprintSpawnableComponent))
class TAREANDTITHE_API UInspectorHudBridgeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInspectorHudBridgeComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void BindPawnTelemetrySources(
		ULaserPlaneAimComponent* InLaserAimComp,
		ULaserCapacitorComponent* InLaserCapacitorComp,
		UAudioTelemetryBridgeComponent* InAudioBridgeComp,
		FOnInspectorReticleProjectionUpdatedSignature& InReticleProjectionDelegate);

	void ApplyPresentationData(const UTareTithePresentationDataAsset* InPresentationData);
	void NotifyScalabilityPresetApplied(int32 QualityLevel, float TsrScalePercent);

	// ============================================================================
	// Diagnostics & Automated Composited UI Verification (Phase 12A)
	// ============================================================================

	UFUNCTION(BlueprintCallable, Category = "InspectorHud|Diagnostics")
	bool RequestCompositedViewportScreenshot(const FString& AbsoluteFilePath);

	UFUNCTION(BlueprintCallable, Category = "InspectorHud|Diagnostics")
	bool RequestCleanLitViewportScreenshot(const FString& AbsoluteFilePath);

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	bool IsOverlayMounted() const { return bIsMountedToViewport && OverlayWidget.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	static int32 GetActiveMountedOverlayCount();

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	FVector2D GetLastLocalViewportSize() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	float GetLastEffectiveCornerPanelWidth() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	float GetLastEffectiveBottomConsoleWidth() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	FVector2D GetTopLeftPanelBoundsMin() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	FVector2D GetTopLeftPanelBoundsMax() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	FVector2D GetTopRightPanelBoundsMin() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	FVector2D GetTopRightPanelBoundsMax() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	FVector2D GetCenterBannerBoundsMin() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	FVector2D GetCenterBannerBoundsMax() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	FVector2D GetReticleCardBoundsMin() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	FVector2D GetReticleCardBoundsMax() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	FVector2D GetBottomConsoleBoundsMin() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	FVector2D GetBottomConsoleBoundsMax() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	FVector2D GetLastReticleNormalizedUV() const { return CachedReticleNormalizedUV; }

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	FString GetActiveCenterAlertStateName() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	bool IsCenterAlertPulseTimerActive() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	float GetCenterAlertBgOpacity() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	float GetCenterAlertTextOpacity() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	FString GetActiveCenterAlertHeadline() const { return LastPushedCenterHeadline; }

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	FString GetActiveCenterAlertSubText() const { return LastPushedCenterSubText; }

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	float GetTopLeftContentDesiredWidth() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	float GetTopRightContentDesiredWidth() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	float GetTopLeftHeaderBottomLocalY() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	float GetTopLeftDividerLocalY() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	float GetTopRightHeaderBottomLocalY() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	float GetTopRightDividerLocalY() const;

	UFUNCTION(BlueprintCallable, Category = "InspectorHud|Diagnostics")
	void InjectRollWheelForTest(float WheelSteps, bool bFastSnap = false);

	UFUNCTION(BlueprintCallable, Category = "InspectorHud|Diagnostics")
	void OverrideCornerWidthForTest(float TestCornerWidthPx, bool bEnableOverride);

	UFUNCTION(BlueprintCallable, Category = "InspectorHud|Controls")
	void ToggleControlsCardVisibility();

	UFUNCTION(BlueprintCallable, Category = "InspectorHud|Controls")
	void SetControlsCardExpanded(bool bExpanded);

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Controls")
	bool IsControlsCardExpanded() const { return bControlsCardExpanded; }

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	FVector2D GetControlsCardBoundsMin() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	FVector2D GetControlsCardBoundsMax() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	float GetLastEffectiveControlsCardWidth() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	float GetBottomConsoleMassBottomLocalY() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	float GetBottomConsoleAnalogBarTopLocalY() const;

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	ELaserSliceVisualState GetLastReticleVisualStateForTest() const { return LastEvaluatedReticleState; }

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	FString GetLastReticleYieldTextForTest() const { return LastEvaluatedReticleYieldText; }

	UFUNCTION(BlueprintPure, Category = "InspectorHud|Diagnostics")
	FLinearColor GetLastReticleYieldColorForTest() const { return LastEvaluatedReticleYieldColor; }

	UFUNCTION(BlueprintCallable, Category = "InspectorHud|Diagnostics")
	void SetLaserChargeForTest(float NormalizedCharge);

	UFUNCTION(BlueprintCallable, Category = "InspectorHud|Diagnostics")
	void OverrideViewportSizeForTest(FVector2D TestLocalViewportSize, bool bEnableOverride);

private:
	void MountOverlayIfNeeded();
	void UnmountOverlayDeterministic();
	FVector2D ComputeCurrentViewportLocalSize() const;
	void HandleViewportResized(FViewport* Viewport, uint32 UnusedResizeCode);

	void ResetTransientTelemetryAndHysteresis();
	void TryArmShift1TutorialForCurrentContract();
	void DismissActiveTutorialHint(bool bRefreshBanner);
	void HandleTutorialAutoDismissTimerElapsed();
	void HandleCareerReset();
	void HandleSliceCommittedResult(const FSliceResultSummary& Summary);

	void PushAllPanels();
	void PushTopLeftPanel();
	void PushTopRightPanel();
	void PushCenterAlertBanner();
	void PushReticleCard();
	void PushBottomCrtConsole();
	void PushGraphicsQualityRow();
	bool IsScaleVisuallySettling() const;
	void SnapRollingMassImmediate(float NewMassKg);
	void SyncTargetPanMassFromSnapshot(bool bForceSnap);
	FPhase12EPresentationPolishConfig ResolvePhase12EConfig() const;
	static FText FormatGraphicsQualityReadout(int32 QualityLevel, float TsrScalePercent);

	UFUNCTION()
	void HandleShiftVerdict(EShiftVerdictType Verdict, EContractFaultReason FaultReason);

	void HandleContractSnapshotUpdated(const FContractRuntimeSnapshot& Snapshot);
	void HandleShiftPhaseChanged(EShiftPhase NewPhase);
	void HandleActiveContractLoaded(
		const UMonolithContractDataAsset* Contract,
		const UShiftProgressionDataAsset* ShiftConfig,
		int32 ContractIndex);
	void HandlePreviewUpdated(const FSlicePreviewTelemetry& PreviewTelemetry);
	void HandleFilteredTelemetryStream(ETelemetryStreamId StreamId, float NormalizedValue);
	void HandleCapacitorTelemetryStream(ETelemetryStreamId StreamId, float NormalizedValue);
	void HandleReticleProjectionUpdated(
		const FVector2D& NormalizedViewportUV,
		bool bReticleVisible,
		const FVector& SteadyWorldAnchor);

	static FText FormatFaultStampHeadline(EContractFaultReason FaultReason);
	static FString AlertStateToDiagnosticString(EHudCenterAlertState State);

	UPROPERTY(Transient)
	TObjectPtr<const UTareTithePresentationDataAsset> PresentationData;

	UPROPERTY(Transient)
	TObjectPtr<ULaserPlaneAimComponent> BoundLaserAimComp;

	UPROPERTY(Transient)
	TObjectPtr<ULaserCapacitorComponent> BoundLaserCapacitorComp;

	UPROPERTY(Transient)
	TObjectPtr<UAudioTelemetryBridgeComponent> BoundAudioBridgeComp;

	TWeakObjectPtr<UGameViewportClient> MountedViewportClient;
	TSharedPtr<STareTitheHudOverlay> OverlayWidget;
	FDelegateHandle ViewportResizeDelegateHandle;
	FDelegateHandle ReticleProjectionDelegateHandle;

	FHudPresentationConfig ActiveHudConfig;
	FContractRuntimeSnapshot CachedSnapshot;
	FSlicePreviewTelemetry CachedPreview;

	EShiftPhase CachedShiftPhase = EShiftPhase::WaitingForStations;
	EShiftVerdictType LatchedVerdict = EShiftVerdictType::ContractPassed;
	EContractFaultReason LatchedFaultReason = EContractFaultReason::None;
	EHudCenterAlertState ActiveCenterAlertState = EHudCenterAlertState::None;

	FVector2D CachedReticleNormalizedUV = FVector2D(0.68f, 0.42f);
	uint64 AlertTriggerGeneration = 0;
	int32 CachedContractIndex = 0;
	int32 CachedTotalContracts = 6;

	float CachedClampStrain = 0.0f;
	float CachedClampFatigue = 0.0f;
	float CachedLaserCharge = 0.0f;
	float TargetPanMassKg = 0.0f;
	float DisplayedPanMassKg = 0.0f;
	float CachedTsrScalePercent = 85.0f;
	int32 CachedGraphicsQualityLevel = 2;
	double LastVeinInDropAutoClearTimeSec = -10.0;

	ELaserSliceVisualState LastEvaluatedReticleState = ELaserSliceVisualState::IdleOrSafeCyan;
	FString LastEvaluatedReticleYieldText;
	FLinearColor LastEvaluatedReticleYieldColor = FLinearColor(0.82f, 0.94f, 1.00f, 1.0f);

	bool bIsMountedToViewport = false;
	bool bControlsCardExpanded = true;
	bool bCachedReticleVisible = false;
	bool bHasLatchedTerminalVerdict = false;

	// Schmitt-Trigger P1 / P2 Hysteresis Latches
	bool bLatchedIsotopeHazard = false;
	bool bLatchedVeinInDrop = false;
	bool bLatchedClampCritical = false;
	bool bLatchedDirectorsCutReady = false;
	bool bLatchedQuotaMetReady = false;

	// Phase 12-FIX2 Shift-01 3-Stage Progressive Micro-Tutorial State (Tier-1 Event + Timer)
	FTimerHandle TutorialAutoDismissTimerHandle;
	FString LastPushedCenterHeadline;
	FString LastPushedCenterSubText;
	int32 ActiveTutorialStageIndex = -1;
	bool bActiveTutorialHint = false;
	bool bShift1TutorialStageSeen[3] = { false, false, false };

	static int32 GActiveMountedOverlayCount;
};
