// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/TareTitheTypes.h"
#include "Data/TareTithePresentationDataAsset.h"
#include "Subsystems/InspectionShiftSubsystem.h"
#include "UI/HudViewModels.h"
#include "InspectorHudBridgeComponent.generated.h"

class FViewport;
class STareTitheHudOverlay;
class UAudioTelemetryBridgeComponent;
class UGameViewportClient;
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

	void BindPawnTelemetrySources(
		ULaserPlaneAimComponent* InLaserAimComp,
		UAudioTelemetryBridgeComponent* InAudioBridgeComp,
		FOnInspectorReticleProjectionUpdatedSignature& InReticleProjectionDelegate);

	void ApplyPresentationData(const UTareTithePresentationDataAsset* InPresentationData);

	// ============================================================================
	// Diagnostics & Automated Composited UI Verification (Phase 12A)
	// ============================================================================

	UFUNCTION(BlueprintCallable, Category = "InspectorHud|Diagnostics")
	bool RequestCompositedViewportScreenshot(const FString& AbsoluteFilePath);

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

private:
	void MountOverlayIfNeeded();
	void UnmountOverlayDeterministic();
	FVector2D ComputeCurrentViewportLocalSize() const;
	void HandleViewportResized(FViewport* Viewport, uint32 UnusedResizeCode);

	void ResetTransientTelemetryAndHysteresis();
	void PushAllPanels();
	void PushTopLeftPanel();
	void PushTopRightPanel();
	void PushCenterAlertBanner();
	void PushReticleCard();
	void PushBottomCrtConsole();

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
	double LastVeinInDropAutoClearTimeSec = -10.0;

	bool bIsMountedToViewport = false;
	bool bCachedReticleVisible = false;
	bool bHasLatchedTerminalVerdict = false;

	// Schmitt-Trigger P1 / P2 Hysteresis Latches
	bool bLatchedIsotopeHazard = false;
	bool bLatchedVeinInDrop = false;
	bool bLatchedClampCritical = false;
	bool bLatchedDirectorsCutReady = false;
	bool bLatchedQuotaMetReady = false;

	static int32 GActiveMountedOverlayCount;
};
