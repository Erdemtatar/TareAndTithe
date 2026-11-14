// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"

DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnInspectorReticleProjectionUpdatedSignature,
	const FVector2D& /*NormalizedViewportUV*/,
	bool /*bReticleVisible*/,
	const FVector& /*SteadyWorldAnchor*/);

enum class EHudCenterAlertState : uint8
{
	None = 0,
	P0_FaultStamp,
	P0_PassStamp,
	P0_ShiftSummary,
	P0_BondCleared,
	P1_IsotopeHazard,
	P1_VeinInDrop,
	P1_ClampShearCritical,
	P2_DirectorsCutReady,
	P2_QuotaMetReady,
	P3_TutorialHint
};

struct FTopLeftIndentureViewModel
{
	FText HeaderText;
	FText DirectiveLabelText;
	FText DirectiveBodyText;
	FLinearColor DirectiveTextColor = FLinearColor(1.00f, 0.82f, 0.18f, 1.0f);
	FLinearColor AccentBarColor = FLinearColor(0.05f, 0.86f, 1.00f, 1.0f);
	FText ScripBalanceText;
	FText UpgradePillTexts[3];
	FLinearColor UpgradePillColors[3] = {
		FLinearColor(0.68f, 0.75f, 0.82f, 1.0f),
		FLinearColor(0.68f, 0.75f, 0.82f, 1.0f),
		FLinearColor(0.68f, 0.75f, 0.82f, 1.0f)
	};
	FText TransientFeedbackText;
	FLinearColor TransientFeedbackColor = FLinearColor(0.12f, 0.98f, 0.42f, 1.0f);
	bool bHasTransientFeedback = false;
};

struct FTopRightDirectorViewModel
{
	FText HeaderText;
	FText StatusBadgeText;
	FText IntercomBodyText;
	FLinearColor IntercomTextColor = FLinearColor(0.45f, 0.84f, 1.00f, 1.0f);
	FLinearColor AccentBarColor = FLinearColor(0.05f, 0.86f, 1.00f, 1.0f);
};

struct FCenterAlertViewModel
{
	EHudCenterAlertState AlertState = EHudCenterAlertState::None;
	uint64 TriggerGeneration = 0;
	bool bIsTerminalStamp = false;
	bool bIsTutorialHint = false;
	FText PrimaryBannerText;
	FText SecondarySubText;
	FLinearColor AccentColor = FLinearColor(1.00f, 0.22f, 0.16f, 1.0f);
};

struct FReticleReadoutViewModel
{
	bool bIsVisible = false;
	FVector2D NormalizedViewportUV = FVector2D(0.68f, 0.42f);
	FText PrimaryNeedOrStatusText;
	FLinearColor PrimaryColor = FLinearColor(0.05f, 0.90f, 1.00f, 1.0f);
	FText YieldEstimateText;
	FLinearColor YieldColor = FLinearColor(0.82f, 0.94f, 1.00f, 1.0f);
	FText ClampLoadText;
	FLinearColor ClampLoadColor = FLinearColor(0.82f, 0.94f, 1.00f, 1.0f);
	FLinearColor AccentColor = FLinearColor(0.05f, 0.90f, 1.00f, 1.0f);
};

struct FBottomCrtConsoleViewModel
{
	FText ConsoleHeaderText;
	FText WindowsSummaryText;
	FText MassReadoutText;
	FLinearColor MassReadoutColor = FLinearColor(1.00f, 0.78f, 0.12f, 1.0f);
	FText TareBadgeText;
	FLinearColor TareBadgeColor = FLinearColor(1.00f, 0.78f, 0.12f, 1.0f);
	FText RouteBadgeText;
	FLinearColor RouteBadgeColor = FLinearColor(0.05f, 0.90f, 1.00f, 1.0f);
	FText OperationalStatusText;
	FLinearColor OperationalStatusColor = FLinearColor(0.45f, 0.84f, 1.00f, 1.0f);
	float NormalizedMassFill = 0.0f;
	float NormalizedValidMin = 0.72f;
	float NormalizedValidMax = 0.88f;
	float NormalizedDirCutMin = 0.77f;
	float NormalizedDirCutMax = 0.83f;
};

struct FHudResolvedLayoutTelemetry
{
	FVector2D LocalViewportSize = FVector2D::ZeroVector;
	float EffectiveCornerPanelWidth = 0.0f;
	float EffectiveBottomConsoleWidth = 0.0f;
	float EffectiveControlsCardWidth = 0.0f;
	FVector2D TopLeftMin = FVector2D::ZeroVector;
	FVector2D TopLeftMax = FVector2D::ZeroVector;
	FVector2D ControlsCardMin = FVector2D::ZeroVector;
	FVector2D ControlsCardMax = FVector2D::ZeroVector;
	FVector2D TopRightMin = FVector2D::ZeroVector;
	FVector2D TopRightMax = FVector2D::ZeroVector;
	FVector2D CenterBannerMin = FVector2D::ZeroVector;
	FVector2D CenterBannerMax = FVector2D::ZeroVector;
	FVector2D ReticleCardMin = FVector2D::ZeroVector;
	FVector2D ReticleCardMax = FVector2D::ZeroVector;
	FVector2D BottomConsoleMin = FVector2D::ZeroVector;
	FVector2D BottomConsoleMax = FVector2D::ZeroVector;
	float BottomConsoleMassBottomLocalY = 0.0f;
	float BottomConsoleAnalogBarTopLocalY = 0.0f;
	float TopLeftDesiredWidth = 0.0f;
	float TopRightDesiredWidth = 0.0f;
	float TopLeftHeaderBottomLocalY = 0.0f;
	float TopLeftDividerLocalY = 0.0f;
	float TopRightHeaderBottomLocalY = 0.0f;
	float TopRightDividerLocalY = 0.0f;
	bool bControlsCardExpanded = true;
	bool bControlsCardArranged = false;
	bool bCenterBannerArranged = false;
	bool bReticleCardArranged = false;
};
