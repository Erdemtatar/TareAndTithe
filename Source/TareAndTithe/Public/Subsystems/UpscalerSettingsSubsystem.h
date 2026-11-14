// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/TareTithePresentationDataAsset.h"
#include "Interfaces/UpscalerControlInterface.h"
#include "UpscalerSettingsSubsystem.generated.h"

class FViewport;

/**
 * Tier-1 process-wide graphics scalability and temporal reconstruction subsystem (zero Tick, 0.00 ms idle CPU cost).
 * - Sole module boundary permitted to query hardware reconstruction capabilities when WITH_TARETITHE_DLSS == 1.
 * - Guarantees AMD, Intel, non-RTX GPUs, commandlets, and outdated drivers run 100% safely on native UE5 TSR.
 * - Preserves temporal upscaler CVars across [F6-F9] scalability preset changes and restores TSR cleanly on [F10] cycle.
 */
UCLASS()
class TAREANDTITHE_API UUpscalerSettingsSubsystem : public UGameInstanceSubsystem, public IUpscalerControlInterface
{
	GENERATED_BODY()

public:
	UUpscalerSettingsSubsystem();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// IUpscalerControlInterface
	virtual void ApplyScalabilityQualityLevel(int32 InQualityLevel) override;
	virtual bool RequestUpscalerMode(ETareUpscalerMode InMode) override;
	virtual ETareUpscalerMode CycleUpscalerMode() override;
	virtual void ApplyPresentationUpscalerConfig(const FUpscalerPresentationConfig& InConfig) override;
	virtual FTareGraphicsStateSnapshot GetGraphicsStateSnapshot() const override { return CurrentSnapshot; }
	virtual FOnTareGraphicsStateChangedSignature& OnGraphicsStateChanged() override { return OnGraphicsStateChangedNative; }

	UPROPERTY(BlueprintAssignable, Category = "Graphics")
	FOnTareGraphicsStateChangedDynamicSignature OnGraphicsStateChangedDynamic;

private:
	ETareSuperResAvailability EvaluateHardwareSupport() const;
	bool QueryHardwareModeScreenPercentage(
		ETareUpscalerMode CandidateMode,
		const FVector2D& RenderResolution,
		float& OutOptimalScreenPercentage) const;
	bool ResolveModeAndScreenPercentage(
		ETareUpscalerMode RequestedMode,
		ETareSuperResAvailability Availability,
		const FVector2D& RenderResolution,
		ETareUpscalerMode& OutActiveMode,
		float& OutEffectiveScreenPct,
		bool& OutDidFallbackToTsr) const;
	float GetNativeTsrScaleForQualityLevel(int32 QualityLevel) const;
	FVector2D ResolveCurrentViewportPixelResolution() const;
	int32 ResolveInitialQualityLevel() const;
	bool ApplyInternalState(int32 InQualityLevel, ETareUpscalerMode InRequestedMode, bool bPersistUserPreference);
	void PersistPreferencesToIni(int32 InQualityLevel, ETareUpscalerMode InRequestedMode) const;
	void BroadcastSnapshotUpdate();
	void HandleViewportResized(FViewport* Viewport, uint32 UnusedResizeCode);

	FUpscalerPresentationConfig ActiveConfig;
	FTareGraphicsStateSnapshot CurrentSnapshot;
	FOnTareGraphicsStateChangedSignature OnGraphicsStateChangedNative;
	FDelegateHandle ViewportResizedDelegateHandle;
	bool bHasExplicitUserModePreference = false;
	bool bIsApplyingInternalState = false;
};
