// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/TareTithePresentationDataAsset.h"
#include "UI/HudViewModels.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SBox;
class SOverlay;
class STopLeftIndenturePanel;
class STopRightDirectorPanel;
class SCenterAlertBanner;
class SReticleReadoutCard;
class SBottomCrtWeighingConsole;

/**
 * Root pure-C++ post-tonemap Slate HUD overlay (SCompoundWidget + SOverlay hierarchy, zero .uasset WBPs).
 * - Root and all child nodes strictly enforce EVisibility::HitTestInvisible (or Collapsed) to preserve FInputModeGameOnly mouse capture.
 * - Dynamically clamps Top-Left & Top-Right widths to <= 34% of local viewport width (525 px target) for 100%..150% DPI safety.
 * - Converts unshaken normalized [0..1] reticle UV into local Slate coordinates clamped clear of all sibling panels.
 */
class TAREANDTITHE_API STareTitheHudOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STareTitheHudOverlay)
		: _HudConfig(FHudPresentationConfig())
		, _InitialLocalViewportSize(FVector2D(1536.0f, 864.0f))
	{
		_Visibility = EVisibility::HitTestInvisible;
	}
		SLATE_ARGUMENT(FHudPresentationConfig, HudConfig)
		SLATE_ARGUMENT(FVector2D, InitialLocalViewportSize)
	SLATE_END_ARGS()

	STareTitheHudOverlay();
	virtual ~STareTitheHudOverlay() override;

	void Construct(const FArguments& InArgs);

	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	// One-way push updates from UInspectorHudBridgeComponent (zero UObject pointers inside Slate)
	void ApplyHudConfig(const FHudPresentationConfig& InConfig);
	void SetLocalViewportSize(const FVector2D& InLocalViewportSize);
	void UpdateTopLeftPanel(const FTopLeftIndentureViewModel& ViewModel);
	void UpdateTopRightPanel(const FTopRightDirectorViewModel& ViewModel);
	void UpdateCenterAlertBanner(const FCenterAlertViewModel& ViewModel);
	void UpdateReticleCard(const FReticleReadoutViewModel& ViewModel);
	void UpdateBottomCrtConsole(const FBottomCrtConsoleViewModel& ViewModel);

	// Read-only diagnostics for automated verification
	FHudResolvedLayoutTelemetry GetResolvedLayoutTelemetry() const { return LastResolvedLayout; }
	bool IsCenterAlertPulseTimerActive() const;
	float GetCenterAlertCurrentBgOpacity() const;
	float GetCenterAlertCurrentTextOpacity() const;

private:
	void RefreshResponsiveMetrics();
	void RefreshReticleClampedOffset();
	void RefreshCenterBannerOffset();
	EActiveTimerReturnType HandleDeferredViewportSync(double InCurrentTime, float InDeltaTime);

	TSharedPtr<SOverlay> RootOverlay;
	TSharedPtr<SBox> TopLeftWidthBox;
	TSharedPtr<SBox> TopRightWidthBox;
	TSharedPtr<SBox> BottomConsoleWidthBox;
	TSharedPtr<SBox> CenterBannerWidthBox;

	SOverlay::FOverlaySlot* TopLeftOverlaySlot = nullptr;
	SOverlay::FOverlaySlot* TopRightOverlaySlot = nullptr;
	SOverlay::FOverlaySlot* BottomConsoleOverlaySlot = nullptr;
	SOverlay::FOverlaySlot* ReticleOverlaySlot = nullptr;
	SOverlay::FOverlaySlot* CenterBannerOverlaySlot = nullptr;

	TSharedPtr<STopLeftIndenturePanel> TopLeftPanel;
	TSharedPtr<STopRightDirectorPanel> TopRightPanel;
	TSharedPtr<SCenterAlertBanner> CenterAlertBanner;
	TSharedPtr<SReticleReadoutCard> ReticleReadoutCard;
	TSharedPtr<SBottomCrtWeighingConsole> BottomCrtConsole;

	FHudPresentationConfig ActiveConfig;
	FVector2D EffectiveLocalViewportSize = FVector2D(1536.0f, 864.0f);
	FVector2D CachedReticleNormalizedUV = FVector2D(0.68f, 0.42f);
	float CachedEffectiveCornerWidth = 522.2f;
	float CachedEffectiveBottomWidth = 720.0f;
	uint64 LastObservedBannerGeneration = 0;
	bool bIsCenterBannerTerminalStamp = false;

	mutable FVector2D PendingObservedLocalSize = FVector2D::ZeroVector;
	mutable bool bViewportSyncScheduled = false;
	mutable FHudResolvedLayoutTelemetry LastResolvedLayout;
};
