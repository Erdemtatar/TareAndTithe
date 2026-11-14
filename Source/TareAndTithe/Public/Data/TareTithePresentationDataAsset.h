// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/SoftObjectPtr.h"
#include "Core/TareTitheTypes.h"
#include "Interfaces/UpscalerControlInterface.h"
#include "TareTithePresentationDataAsset.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class UShiftProgressionDataAsset;
class USoundAttenuation;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;
class UTextureCube;

USTRUCT(BlueprintType)
struct TAREANDTITHE_API FMeshPresentationSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Mesh")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Mesh")
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Mesh")
	FTransform VisualLocalTransform = FTransform::Identity;

	/**
	 * Safe dual-mode visual applier:
	 * - Returns false (no-op) when Mesh is null so calibrated greybox Cube & RelativeTransform remain 100% untouched.
	 * - When Mesh is valid, assigns Mesh, applies VisualLocalTransform ((1,1,1) default scale), applies MaterialOverrides,
	 *   and guarantees NoCollision on the visual component.
	 */
	bool ApplyToVisualComponent(UStaticMeshComponent* TargetVisualComp) const;
};

USTRUCT(BlueprintType)
struct TAREANDTITHE_API FCellDirectionalSpotConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Lighting")
	FVector Location = FVector(165.0, -22.0, 330.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Lighting")
	FVector TargetLocation = FVector(260.0, 0.0, 105.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Lighting", meta = (ClampMin = "0.0", ClampMax = "100000.0"))
	float Intensity = 8800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Lighting", meta = (ClampMin = "100.0", ClampMax = "2500.0"))
	float AttenuationRadius = 580.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Lighting", meta = (ClampMin = "1.0", ClampMax = "80.0"))
	float InnerConeAngle = 22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Lighting", meta = (ClampMin = "5.0", ClampMax = "89.0"))
	float OuterConeAngle = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Lighting", meta = (ClampMin = "0.0", ClampMax = "250.0"))
	float SourceRadius = 26.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Lighting", meta = (ClampMin = "0.0", ClampMax = "500.0"))
	float SourceLength = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Lighting")
	FLinearColor LightColor = FLinearColor(0.96f, 0.97f, 1.00f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Lighting")
	bool bCastShadows = false;
};

USTRUCT(BlueprintType)
struct TAREANDTITHE_API FCellLightingRigConfig
{
	GENERATED_BODY()

	FCellLightingRigConfig();

	/** Sole VSM shadow-casting key spotlight aimed down the monolith and scale drop column. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Lighting")
	FCellDirectionalSpotConfig KeySpot;

	/** Unshadowed front-side fill point light tightly bounded off the outer side walls (< 360 cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Lighting")
	FVector FillPointLocation = FVector(95.0, 14.0, 142.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Lighting", meta = (ClampMin = "0.0", ClampMax = "50000.0"))
	float FillPointIntensity = 2650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Lighting", meta = (ClampMin = "100.0", ClampMax = "2500.0"))
	float FillPointRadius = 355.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Lighting", meta = (ClampMin = "0.0", ClampMax = "250.0"))
	float FillPointSourceRadius = 36.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Lighting", meta = (ClampMin = "0.0", ClampMax = "500.0"))
	float FillPointSourceLength = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Lighting")
	FLinearColor FillPointColor = FLinearColor(0.84f, 0.90f, 1.00f, 1.0f);

	/** Cool ~7400K left grazing rim spotlight raking across hero spine chamfers (bCastShadows = false). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Lighting")
	FCellDirectionalSpotConfig RimSpotLeft;

	/** Cool ~7400K right grazing rim spotlight raking across hero spine chamfers (bCastShadows = false). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Lighting")
	FCellDirectionalSpotConfig RimSpotRight;

	/** Unshadowed ~2150K frontal warm foundry shaft light illuminating the rear grate shaft & flanking columns (+X away from hero monolith). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Lighting")
	FCellDirectionalSpotConfig MidgroundWashSpot;
};

USTRUCT(BlueprintType)
struct TAREANDTITHE_API FCellAtmosphereConfig
{
	GENERATED_BODY()

	/** Cook-safe cubemap inside /Game/Presentation/ for SLS_SpecifiedCubemap inside the inspection cell. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Atmosphere")
	TObjectPtr<UTextureCube> AmbientSkyCubemap = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Atmosphere", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float SkyLightIntensity = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Atmosphere")
	FLinearColor SkyTintColor = FLinearColor(0.64f, 0.78f, 0.96f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Atmosphere")
	FLinearColor LowerHemisphereColor = FLinearColor(0.035f, 0.042f, 0.055f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Atmosphere")
	bool bEnableDepthFog = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Atmosphere", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FogDensity = 0.015f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Atmosphere", meta = (ClampMin = "0.01", ClampMax = "2.0"))
	float FogHeightFalloff = 0.28f;

	/** Starts behind the hero station and tare chute (~390 cm from pulled-back camera) so foreground has zero fog veil. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Atmosphere", meta = (ClampMin = "0.0", ClampMax = "1500.0"))
	float FogStartDistanceCm = 390.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Atmosphere", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FogMaxOpacity = 0.32f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Atmosphere")
	FLinearColor FogInscatteringColor = FLinearColor(0.008f, 0.007f, 0.006f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Atmosphere", meta = (ClampMin = "-200.0", ClampMax = "400.0"))
	float FogActorZCm = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Atmosphere")
	bool bSpawnMidgroundArchitectureRelief = true;
};

USTRUCT(BlueprintType)
struct TAREANDTITHE_API FMoltenBounceLightConfig
{
	GENERATED_BODY()

	/** Warm ~2200K molten slag bounce color shared by cut-cap and scale-well non-shadow lights. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|MoltenBounce")
	FLinearColor BounceColor = FLinearColor(1.00f, 0.36f, 0.07f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|MoltenBounce", meta = (ClampMin = "0.0", ClampMax = "25000.0"))
	float CutFlashPeakIntensity = 5600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|MoltenBounce", meta = (ClampMin = "0.0", ClampMax = "10000.0"))
	float CutSteadyRemainderIntensity = 1350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|MoltenBounce", meta = (ClampMin = "40.0", ClampMax = "800.0"))
	float CutAttenuationRadiusCm = 265.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|MoltenBounce")
	FVector CutLightLocalOffset = FVector(-22.0, 0.0, -24.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|MoltenBounce", meta = (ClampMin = "0.0", ClampMax = "15000.0"))
	float ScaleWellFlashIntensity = 2800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|MoltenBounce", meta = (ClampMin = "0.0", ClampMax = "8000.0"))
	float ScaleWellMaxSteadyIntensity = 1150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|MoltenBounce", meta = (ClampMin = "40.0", ClampMax = "500.0"))
	float ScaleWellAttenuationRadiusCm = 165.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|MoltenBounce")
	FVector ScaleWellLocalOffset = FVector(0.0, 0.0, 24.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|MoltenBounce", meta = (ClampMin = "0.10", ClampMax = "4.0"))
	float FlashDecayDurationSeconds = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|MoltenBounce", meta = (ClampMin = "0.5", ClampMax = "30.0"))
	float SteadyInterpSpeed = 6.0f;
};

USTRUCT(BlueprintType)
struct TAREANDTITHE_API FCameraPostProcessPolishConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|PostProcess", meta = (ClampMin = "0.05", ClampMax = "10.0"))
	float LockedEV100MinMax = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|PostProcess", meta = (ClampMin = "-5.0", ClampMax = "5.0"))
	float ExposureCompensationBias = 0.85f;

	/** Neutral 1.0f bypasses bilateral local-exposure contrast compression to prevent halation around UnlitText HUD glyphs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|PostProcess", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float LocalExposureHighlightContrast = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|PostProcess", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float LocalExposureShadowContrast = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|PostProcess", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float BloomIntensity = 0.42f;

	/** Mathematical HUD firewall: >= 1.65f excludes all UnlitText FColor <= 1.0 while blooming HDR molten cuts & sparks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|PostProcess", meta = (ClampMin = "1.15", ClampMax = "5.0"))
	float BloomThreshold = 1.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|PostProcess", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float FilmSlope = 0.90f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|PostProcess", meta = (ClampMin = "0.3", ClampMax = "0.8"))
	float FilmToe = 0.53f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|PostProcess", meta = (ClampMin = "0.1", ClampMax = "0.8"))
	float FilmShoulder = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|PostProcess", meta = (ClampMin = "0.8", ClampMax = "1.4"))
	float ColorContrast = 1.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|PostProcess", meta = (ClampMin = "0.5", ClampMax = "1.4"))
	float ColorSaturation = 0.96f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|PostProcess", meta = (ClampMin = "0.0", ClampMax = "0.6"))
	float VignetteIntensity = 0.36f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|PostProcess", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TonemapperSharpen = 0.35f;
};

USTRUCT(BlueprintType)
struct TAREANDTITHE_API FHudPresentationConfig
{
	GENERATED_BODY()

	/** Legacy 3D world-space UTextRenderComponent kill-switch (default false: pure C++ post-tonemap Slate HUD active). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Legacy")
	bool bEnableLegacyWorldSpaceReadouts = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Opacity", meta = (ClampMin = "0.40", ClampMax = "0.98"))
	float CornerPanelOpacity = 0.80f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Opacity", meta = (ClampMin = "0.35", ClampMax = "0.95"))
	float ReticleBackingOpacity = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Opacity", meta = (ClampMin = "0.50", ClampMax = "0.98"))
	float BottomCrtConsoleOpacity = 0.88f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|CRT", meta = (ClampMin = "0.00", ClampMax = "0.60"))
	float CrtScanlineOpacity = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|CRT", meta = (ClampMin = "2.0", ClampMax = "8.0"))
	float CrtScanlinePitchPx = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Layout", meta = (ClampMin = "240.0", ClampMax = "680.0"))
	float CornerPanelWidthPx = 525.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Layout", meta = (ClampMin = "6.0", ClampMax = "40.0"))
	float CornerPanelMarginPx = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Layout", meta = (ClampMin = "0.14", ClampMax = "0.38"))
	float CornerPanelMaxViewportWidthFraction = 0.34f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Typography", meta = (ClampMin = "8", ClampMax = "22"))
	int32 CornerHeaderFontSizePt = 13;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Typography", meta = (ClampMin = "9", ClampMax = "24"))
	int32 CornerBodyFontSizePt = 14;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Typography", meta = (ClampMin = "8", ClampMax = "22"))
	int32 CornerSubFontSizePt = 13;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Typography", meta = (ClampMin = "7", ClampMax = "18"))
	int32 CornerPillFontSizePt = 11;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Layout", meta = (ClampMin = "480.0", ClampMax = "1000.0"))
	float BottomConsoleWidthPx = 720.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Layout", meta = (ClampMin = "0.35", ClampMax = "0.75"))
	float BottomConsoleMaxViewportWidthFraction = 0.54f;

	/** Uncluttered central sightline corridor between lowest laser plane travel (~0.575 Y) and scale pan top lip (~0.790 Y). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Banner", meta = (ClampMin = "0.05", ClampMax = "0.72"))
	float HazardBannerTopAnchorNormalized = 0.595f;

	/** Dead-center authoritative stamp zone for P0 terminal fault/pass/shift/bond plaques. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Banner", meta = (ClampMin = "0.28", ClampMax = "0.55"))
	float VerdictStampTopAnchorNormalized = 0.42f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Banner", meta = (ClampMin = "1", ClampMax = "5"))
	int32 AlertPulseCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Banner", meta = (ClampMin = "0.30", ClampMax = "3.00"))
	float AlertPulseEnvelopeSeconds = 1.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Banner", meta = (ClampMin = "0.30", ClampMax = "0.95"))
	float AlertPostPulseBackgroundOpacity = 0.62f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Banner", meta = (ClampMin = "0.60", ClampMax = "1.00"))
	float AlertPostPulseTextOpacity = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Hysteresis", meta = (ClampMin = "0.10", ClampMax = "0.90"))
	float VeinHazardEnterThreshold = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Hysteresis", meta = (ClampMin = "0.05", ClampMax = "0.80"))
	float VeinHazardExitThreshold = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Hysteresis", meta = (ClampMin = "0.50", ClampMax = "0.98"))
	float ClampCriticalEnterThreshold = 0.78f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Hysteresis", meta = (ClampMin = "0.35", ClampMax = "0.90"))
	float ClampCriticalExitThreshold = 0.68f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Hysteresis", meta = (ClampMin = "0.10", ClampMax = "0.90"))
	float ClampFatigueWarnEnterThreshold = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Hysteresis", meta = (ClampMin = "0.05", ClampMax = "0.80"))
	float ClampFatigueWarnExitThreshold = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Reticle", meta = (ClampMin = "0.0001", ClampMax = "0.02"))
	float ReticleNormalizedUvDeadband = 0.001f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Theme")
	FLinearColor PanelAnthraciteTint = FLinearColor(0.014f, 0.017f, 0.022f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Theme")
	FLinearColor PanelBorderGunmetalTint = FLinearColor(0.19f, 0.23f, 0.28f, 0.92f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Tutorial")
	bool bEnableShift1MicroTutorial = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Tutorial", meta = (ClampMin = "1.0", ClampMax = "15.0"))
	float Shift1Stage1DurationSeconds = 4.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Tutorial", meta = (ClampMin = "1.0", ClampMax = "15.0"))
	float Shift1Stage2DurationSeconds = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Tutorial", meta = (ClampMin = "1.0", ClampMax = "15.0"))
	float Shift1Stage3DurationSeconds = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Tutorial", meta = (ClampMin = "1.0", ClampMax = "25.0"))
	float Shift1Stage2DismissTiltDegrees = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Tutorial")
	FLinearColor TutorialIceCyanTint = FLinearColor(0.18f, 0.90f, 1.00f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Tutorial", meta = (ClampMin = "1", ClampMax = "5"))
	int32 TutorialPulseCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Tutorial", meta = (ClampMin = "0.25", ClampMax = "2.50"))
	float TutorialPulseEnvelopeSeconds = 0.80f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Tutorial", meta = (ClampMin = "0.40", ClampMax = "0.95"))
	float TutorialPostPulseBackgroundOpacity = 0.76f;
};

USTRUCT(BlueprintType)
struct TAREANDTITHE_API FPanopticonSurveillanceConfig
{
	GENERATED_BODY()

	/** Scales serialized idle Panopticon bay point-light intensities down so window luminance drops ~45% below the hero monolith. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|Luminance", meta = (ClampMin = "0.05", ClampMax = "1.00"))
	float IdleBayLightIntensityScale = 0.36f;

	/** Compensated 9 Hz crimson strobe peak multiplier relative to suppressed idle bay intensity during FaultResetDelay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|Luminance", meta = (ClampMin = "1.00", ClampMax = "15.00"))
	float FaultStrobePeakMultiplier = 6.80f;

	/** Pulls bay point lights forward in front of corrugated blast shutters (-13.05 cm) to eliminate the 0.95 cm specular singularity while preserving N.L > 0 on slammed shutters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|Luminance", meta = (ClampMin = "-45.0", ClampMax = "-16.0"))
	float BayLightForwardLocationX = -26.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|Luminance", meta = (ClampMin = "140.0", ClampMax = "230.0"))
	float FlankBayLightLocationY = 204.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|Luminance", meta = (ClampMin = "-12.0", ClampMax = "28.0"))
	float FlankBayLightLocationZ = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|Luminance", meta = (ClampMin = "-5.0", ClampMax = "36.0"))
	float CenterBayLightLocationZ = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|Materials", meta = (ClampMin = "0.20", ClampMax = "1.20"))
	float BoothFrameAlbedoScale = 0.56f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|Materials", meta = (ClampMin = "0.05", ClampMax = "1.00"))
	float BoothFrameSpecularScale = 0.38f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|Materials", meta = (ClampMin = "0.20", ClampMax = "1.20"))
	float BoothShutterAlbedoScale = 0.58f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|Materials", meta = (ClampMin = "0.05", ClampMax = "1.00"))
	float BoothShutterSpecularScale = 0.34f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|FrostedWindow")
	FLinearColor IdleWindowCardBaseColor = FLinearColor(0.28f, 0.40f, 0.54f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|FrostedWindow")
	FLinearColor FaultWindowCardBaseColor = FLinearColor(0.88f, 0.09f, 0.05f, 1.0f);

	/** Strictly bloom-safe (< 1.65 BloomThreshold) cold overlook emissive backing behind louvers and Director silhouette. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|FrostedWindow")
	FLinearColor IdleWindowCardEmissive = FLinearColor(0.044f, 0.070f, 0.102f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|FrostedWindow")
	FLinearColor FaultWindowCardEmissive = FLinearColor(0.55f, 0.045f, 0.025f, 1.0f);

	/** Diffused frosted roughness preventing sharp foreground point-light tube reflections behind the silhouette. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|FrostedWindow", meta = (ClampMin = "0.50", ClampMax = "1.00"))
	float WindowCardRoughness = 0.82f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|FrostedWindow", meta = (ClampMin = "0.00", ClampMax = "0.25"))
	float WindowCardMetallic = 0.04f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|Louvers")
	bool bEnablePanopticonLouvers = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|Louvers", meta = (ClampMin = "4", ClampMax = "12"))
	int32 FlankLouverSlatCount = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|Louvers", meta = (ClampMin = "3", ClampMax = "10"))
	int32 CenterLouverSlatCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|Louvers", meta = (ClampMin = "3.80", ClampMax = "9.00"))
	float LouverVerticalPitchCm = 6.20f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|Louvers", meta = (ClampMin = "1.20", ClampMax = "3.40"))
	float LouverDepthCm = 2.40f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|Louvers", meta = (ClampMin = "0.50", ClampMax = "1.60"))
	float LouverThicknessCm = 1.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|Louvers", meta = (ClampMin = "-25.0", ClampMax = "10.0"))
	float LouverPitchDegrees = -12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|DirectorSilhouette")
	bool bEnableWatchingDirectorSilhouette = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|DirectorSilhouette", meta = (ClampMin = "150.0", ClampMax = "205.0"))
	float DirectorBayLocalY = 176.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|DirectorSilhouette", meta = (ClampMin = "0.75", ClampMax = "1.15"))
	float DirectorScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panopticon|DirectorSilhouette", meta = (ClampMin = "-180.0", ClampMax = "-155.0"))
	float DirectorInwardYawDegrees = -168.0f;

	FPanopticonSurveillanceConfig GetSanitized() const;
};

USTRUCT(BlueprintType)
struct TAREANDTITHE_API FOuterEdgeFramingConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OuterFraming|Layout")
	bool bEnableOuterArchitecturalFraming = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OuterFraming|Layout", meta = (ClampMin = "155.0", ClampMax = "225.0"))
	float ForegroundPipeWorldX = 172.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OuterFraming|Layout", meta = (ClampMin = "185.0", ClampMax = "245.0"))
	float ForegroundPipeAbsY = 205.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OuterFraming|Layout", meta = (ClampMin = "8.0", ClampMax = "20.0"))
	float ForegroundPipeRadiusCm = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OuterFraming|Layout", meta = (ClampMin = "205.0", ClampMax = "335.0"))
	float MidgroundColumnWorldX = 224.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OuterFraming|Layout", meta = (ClampMin = "175.0", ClampMax = "315.0"))
	float MidgroundColumnAbsY = 198.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OuterFraming|Layout", meta = (ClampMin = "0.60", ClampMax = "1.30"))
	float MidgroundColumnXYScale = 0.92f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OuterFraming|Layout", meta = (ClampMin = "175.0", ClampMax = "320.0"))
	float CompanionPipeWorldX = 192.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OuterFraming|Layout", meta = (ClampMin = "170.0", ClampMax = "275.0"))
	float CompanionPipeAbsY = 185.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OuterFraming|Layout", meta = (ClampMin = "5.0", ClampMax = "16.0"))
	float CompanionPipeRadiusCm = 14.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OuterFraming|Material", meta = (ClampMin = "0.15", ClampMax = "2.20"))
	float OuterMetalAlbedoScale = 0.605f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OuterFraming|Material", meta = (ClampMin = "0.004", ClampMax = "0.25"))
	float OuterMetalConductorMinF0Scale = 0.0168f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OuterFraming|Material", meta = (ClampMin = "0.16", ClampMax = "0.70"))
	float OuterMetalRoughnessMin = 0.48f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OuterFraming|Material", meta = (ClampMin = "0.20", ClampMax = "0.85"))
	float OuterMetalRoughnessMax = 0.64f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OuterFraming|Material", meta = (ClampMin = "0.15", ClampMax = "0.95"))
	float OuterMetalMetallic = 0.42f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OuterFraming|Material", meta = (ClampMin = "0.15", ClampMax = "1.00"))
	float OuterMetalSpecularScale = 0.68f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OuterFraming|Material", meta = (ClampMin = "0.10", ClampMax = "1.00"))
	float OuterMetalAOStrength = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OuterFraming|Material", meta = (ClampMin = "0.25", ClampMax = "2.50"))
	float OuterMetalNormalStrength = 1.45f;

	/** Strategy 1 locked shipping default (false): zero extra lights; catches grazing rim from CellFillPoint + cross-axis RimSpotLeft/Right + TC_CellAmbientCubemap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OuterFraming|RimLighting")
	bool bEnableDedicatedOuterRimLights = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OuterFraming|RimLighting", meta = (ClampMin = "0.0", ClampMax = "600.0"))
	float DedicatedOuterRimIntensity = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OuterFraming|RimLighting", meta = (ClampMin = "60.0", ClampMax = "220.0"))
	float DedicatedOuterRimRadius = 135.0f;

	FOuterEdgeFramingConfig GetSanitized() const;
};

USTRUCT(BlueprintType)
struct TAREANDTITHE_API FClampGroanAudioConfig
{
	GENERATED_BODY()

	FClampGroanAudioConfig();

	/** Cook-safe soft reference to /Game/Presentation/Audio/Waves/SFX_ClampSteelGroan_Loop.SFX_ClampSteelGroan_Loop (resolved without DA_Presentation.uasset resave). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan")
	TSoftObjectPtr<USoundBase> ClampSteelGroanLoopSound;

	/** Ducked 2D SFX_CellAmbience_Loop gain (-7.5 dB) so clamp tension, spatial one-shots, and Director PA cues cut cleanly through room tone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Mix", meta = (ClampMin = "0.0", ClampMax = "1.5"))
	float AmbientBedVolumeMultiplier = 0.42f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Mix", meta = (ClampMin = "0.1", ClampMax = "2.5"))
	float SpatialImpulseMinVolumeMultiplier = 0.92f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Mix", meta = (ClampMin = "0.1", ClampMax = "2.5"))
	float SpatialImpulseMaxVolumeMultiplier = 1.25f;

	/** Neutral parameter value (0.15f) sent to MS_TelemetryLoops "ClampStrain" so its unclamped MapRange(0.15..1.0 -> 0.0..0.65) outputs exact 0.000f mono bleed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MetaSoundClampStrainNeutralValue = 0.15f;

	/** Silent balanced gate: ClampStrain < 0.46f and ClampFatigue < 0.05f holds groan at 0.0f. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan", meta = (ClampMin = "0.10", ClampMax = "0.75"))
	float GroanOnsetStrainThreshold = 0.46f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan", meta = (ClampMin = "0.01", ClampMax = "0.50"))
	float GroanOnsetFatigueThreshold = 0.05f;

	/** Zone 2 boundary: ClampStrain >= 0.66f OR ClampFatigue >= 0.15f transitions from subtle resting tension into urgent tip-over danger. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan", meta = (ClampMin = "0.48", ClampMax = "0.90"))
	float GroanZone2StrainThreshold = 0.66f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan", meta = (ClampMin = "0.05", ClampMax = "0.80"))
	float GroanZone2FatigueThreshold = 0.15f;

	/** JAW SHEAR CRITICAL strain threshold (0.78f). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan", meta = (ClampMin = "0.55", ClampMax = "0.99"))
	float GroanCriticalStrainThreshold = 0.78f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan", meta = (ClampMin = "0.10", ClampMax = "1.00"))
	float GroanCriticalFatigueThreshold = 0.35f;

	/** Zone 1 subtle resting overhang volume [0.16f .. 0.38f] across [0.46f .. 0.66f) strain (~0.303f at Ctr 02 59% spawn). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan", meta = (ClampMin = "0.02", ClampMax = "1.00"))
	float GroanZone1MinVolume = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan", meta = (ClampMin = "0.05", ClampMax = "1.20"))
	float GroanZone1MaxVolume = 0.38f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan", meta = (ClampMin = "0.50", ClampMax = "1.50"))
	float GroanZone1MinPitch = 0.94f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan", meta = (ClampMin = "0.50", ClampMax = "1.50"))
	float GroanZone1MaxPitch = 0.99f;

	/** Zone 2 urgent shear danger volume: 0.78f at 0.66 strain -> 1.18f at 0.78 critical -> 1.35f approaching 1.00 snap / >= 0.35 fatigue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan", meta = (ClampMin = "0.20", ClampMax = "2.00"))
	float GroanZone2EntryVolume = 0.78f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan", meta = (ClampMin = "0.30", ClampMax = "2.20"))
	float GroanCriticalVolume = 1.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan", meta = (ClampMin = "0.30", ClampMax = "2.50"))
	float GroanPreSnapVolume = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan", meta = (ClampMin = "0.70", ClampMax = "2.00"))
	float GroanZone2EntryPitch = 1.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan", meta = (ClampMin = "0.70", ClampMax = "2.00"))
	float GroanCriticalPitch = 1.11f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan", meta = (ClampMin = "0.70", ClampMax = "2.00"))
	float GroanMaxPitchMultiplier = 1.15f;

	/** Post-slice / torque-shift creak surge: Clamp(Max(SteadyVolume, 0.52f) + 0.36f, 0.75f, 1.40f), SteadyPitch + 0.05f. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan|Surge", meta = (ClampMin = "0.005", ClampMax = "0.25"))
	float GroanTorqueSpikeMinDelta = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan|Surge", meta = (ClampMin = "0.01", ClampMax = "0.50"))
	float GroanFatigueSpikeMinDelta = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan|Surge", meta = (ClampMin = "0.10", ClampMax = "1.50"))
	float GroanSurgeBaseFloorVolume = 0.52f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan|Surge", meta = (ClampMin = "0.00", ClampMax = "1.20"))
	float GroanSliceSurgeVolumeBoost = 0.36f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan|Surge", meta = (ClampMin = "0.20", ClampMax = "2.00"))
	float GroanSurgeMinClampVolume = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan|Surge", meta = (ClampMin = "0.30", ClampMax = "2.50"))
	float GroanMaxVolumeMultiplier = 1.40f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan|Surge", meta = (ClampMin = "0.00", ClampMax = "0.30"))
	float GroanSliceSurgePitchBoost = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan|Surge", meta = (ClampMin = "0.10", ClampMax = "2.00"))
	float GroanSliceSurgeDurationSeconds = 0.85f;

	/** FadeIn (0.06s to 1.0f internal envelope) and <= 0.08s instant cutoff FadeOut on snap/fault/phase exit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan|Envelope", meta = (ClampMin = "0.01", ClampMax = "0.25"))
	float GroanFadeInSeconds = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan|Envelope", meta = (ClampMin = "0.01", ClampMax = "0.12"))
	float GroanFastCutoffSeconds = 0.08f;

	FClampGroanAudioConfig GetSanitized() const;
};

/**
 * Phase 12D Industrial Material Wear, Rear Foundry Luminance Suppression,
 * Right Panopticon Surveillance Contrast Lift & Molten Cut-Cap Thermal Cooling Configuration.
 * Pure C++ defaults preserve zero binary diff on DA_Presentation.uasset.
 */
USTRUCT(BlueprintType)
struct TAREANDTITHE_API FPhase12DPresentationPolishConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|Toggles")
	bool bEnableProceduralStationWear = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|Toggles")
	bool bEnableRearGridLuminanceSuppression = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|Toggles")
	bool bEnableRearShaftHeavyConduits = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|Toggles")
	bool bEnableRightPanopticonContrastLift = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|Toggles")
	bool bEnableCutCapThermalCooling = true;

	/** Attenuates +X warm foundry shaft spot intensity (5700 * 0.80 = 4560 lm) so hero monolith & scale dominate ROI luminance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|RearFoundry", meta = (ClampMin = "0.40", ClampMax = "1.20"))
	float MidgroundWashIntensityMultiplier = 0.80f;

	/** Pulls excessive orange saturation slightly toward warm soot-ash luminance (0.12f). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|RearFoundry", meta = (ClampMin = "0.00", ClampMax = "0.50"))
	float MidgroundWashChromaDesaturation = 0.12f;

	/** Scales FoundryGrateShaftISM AlbedoTint (0.80 wash * 0.88 albedo = 0.704 -> -29.6% combined rear grid luminance reduction). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|RearFoundry", meta = (ClampMin = "0.45", ClampMax = "1.15"))
	float RearGridAlbedoMultiplier = 0.88f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|RearFoundry", meta = (ClampMin = "0.40", ClampMax = "1.15"))
	float RearGridConductorF0Multiplier = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|RearFoundry", meta = (ClampMin = "0.35", ClampMax = "1.15"))
	float RearGridSpecularMultiplier = 0.78f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|RearFoundry", meta = (ClampMin = "0.00", ClampMax = "1.00"))
	float RearGridProceduralWearWeight = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|RearFoundry", meta = (ClampMin = "0.00", ClampMax = "1.00"))
	float RearGridMacroSootWeight = 1.00f;

	/** Safeguard 4 dark soot-iron parameters on FoundryRearConduitISM framing the suppressed rear grid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|RearConduits")
	FLinearColor ConduitAlbedoTint = FLinearColor(0.52f, 0.55f, 0.60f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|RearConduits")
	FLinearColor ConduitConductorMinF0 = FLinearColor(0.08f, 0.09f, 0.10f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|RearConduits", meta = (ClampMin = "0.20", ClampMax = "0.80"))
	float ConduitRoughnessMin = 0.40f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|RearConduits", meta = (ClampMin = "0.30", ClampMax = "0.85"))
	float ConduitRoughnessMax = 0.68f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|RearConduits", meta = (ClampMin = "0.10", ClampMax = "1.00"))
	float ConduitSpecularScale = 0.42f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|RearConduits", meta = (ClampMin = "0.00", ClampMax = "1.00"))
	float ConduitProceduralWearWeight = 0.65f;

	/** Lifts ONLY RightWindowGlowCardMID during active surveillance so the matte-black Director silhouette cuts sharply against frosted cyan-slate glass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|Panopticon", meta = (ClampMin = "1.00", ClampMax = "4.00"))
	float RightBayWindowEmissiveMultiplier = 2.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|Panopticon", meta = (ClampMin = "1.00", ClampMax = "2.50"))
	float RightBayWindowBaseColorMultiplier = 1.15f;

	/** Safeguard 3 cold cyan-white ocular lens glint emissive (< 1.65f BloomThreshold; zero red REC light). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|Panopticon")
	FLinearColor OpticalLensGlintEmissive = FLinearColor(0.85f, 1.28f, 1.48f, 1.0f);

	/** Safeguard 3 Nyquist-safe twin cold optical lens dimensions (cm): >= 2.05 TSR internal pixels vertically at 85% TSR. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|Panopticon")
	FVector OpticalLensGlintSizeCm = FVector(0.45, 1.65, 1.15);

	/** Booth-local Z (+11.05 cm): +1.11 cm clear above Slat 3 ray shadow, +0.90 cm clear below officer visor brim. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|Panopticon", meta = (ClampMin = "9.80", ClampMax = "12.00"))
	float OpticalLensGlintBoothLocalZ = 11.05f;

	/** Lateral ocular half-spacing (+-1.85 cm) along FigRot -> 2.05 cm dark bridge gap between lenses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|Panopticon", meta = (ClampMin = "1.20", ClampMax = "2.80"))
	float OpticalLensGlintHalfSpacingCm = 1.85f;

	/** Forward offset along -FigRot.AxisX from (MidX, BaseY) placing lenses cleanly ahead of cranium and behind SafeMinX. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|Panopticon", meta = (ClampMin = "1.10", ClampMax = "2.20"))
	float OpticalLensGlintForwardOffsetCm = 1.44f;

	/** GPU Time shader cooling duration (3.85s) from white-gold incandescent plasma to dark cherry-ember crust. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|CutCapCooling", meta = (ClampMin = "0.75", ClampMax = "8.00"))
	float CutCapCoolingDurationSeconds = 3.85f;

	/** Residual steady MoltenCutBounceLight multiplier (0.15f) once the freshly severed cut cap cools into dark slag crust. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|CutCapCooling", meta = (ClampMin = "0.00", ClampMax = "1.00"))
	float CutCapCooledBounceMultiplier = 0.15f;

	/** Synchronized MoltenThermalLightComponent flash decay duration (3.60s) matching GPU cut-cap thermal dissipation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish|CutCapCooling", meta = (ClampMin = "0.50", ClampMax = "8.00"))
	float CutCapThermalDecayDurationSeconds = 3.60f;

	FPhase12DPresentationPolishConfig GetSanitized() const;
};

/**
 * Phase 12C OBB-Masked Incandescent Contact Seam, 5-State Palette Separation & Plasma Scanline Configuration.
 */
USTRUCT(BlueprintType)
struct TAREANDTITHE_API FLaserSheetPresentationConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials|LaserSheet|Seam", meta = (ClampMin = "0.35", ClampMax = "3.00"))
	float CoreSeamWidthCm = 1.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials|LaserSheet|Seam", meta = (ClampMin = "1.00", ClampMax = "6.00"))
	float ShoulderSeamWidthCm = 2.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials|LaserSheet|Seam", meta = (ClampMin = "0.10", ClampMax = "0.60"))
	float GrazingReferenceSin = 0.26f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials|LaserSheet|Seam", meta = (ClampMin = "0.02", ClampMax = "1.00"))
	float MinGrazingScale = 0.055f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials|LaserSheet|Mask", meta = (ClampMin = "0.00", ClampMax = "4.00"))
	float JawSeamExclusionMarginCm = 0.80f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials|LaserSheet|Mask", meta = (ClampMin = "0.10", ClampMax = "2.50"))
	float SceneMaskFeatherCm = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials|LaserSheet|Mask", meta = (ClampMin = "1.00", ClampMax = "32.00"))
	float SheetMaskPadXCm = 22.00f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials|LaserSheet|Mask", meta = (ClampMin = "0.50", ClampMax = "16.00"))
	float SheetMaskPadYCm = 6.00f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials|LaserSheet|Scanlines", meta = (ClampMin = "0.10", ClampMax = "0.35"))
	float InteriorOpacityMultiplier = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials|LaserSheet|Scanlines", meta = (ClampMin = "0.02", ClampMax = "0.60"))
	float InteriorTroughEmissiveGain = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials|LaserSheet|Scanlines", meta = (ClampMin = "0.10", ClampMax = "1.20"))
	float InteriorCrestEmissiveGain = 0.46f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials|LaserSheet|Border", meta = (ClampMin = "1.00", ClampMax = "5.00"))
	float BorderEmissiveGain = 2.60f;

	/** Distinct Yellow-Gold for [VEIN -> SPACE] (Checkpoint 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials|LaserSheet|Palette")
	FLinearColor VeinDropYellowColor = FLinearColor(1.00f, 0.88f, 0.04f, 1.0f);

	/** Distinct Amber-Orange for ISOTOPE PROXIMITY (Checkpoint 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials|LaserSheet|Palette")
	FLinearColor IsotopeProximityAmberColor = FLinearColor(1.00f, 0.36f, 0.02f, 1.0f);

	/** Phase 12-FIX4 Electric Magenta base chromaticity for Tithe Quota Overshoot [OVER!] (0 bytes diff to DA_Presentation.uasset). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials|LaserSheet|Palette")
	FLinearColor OvershootMagentaColor = FLinearColor(0.94f, 0.04f, 0.78f, 1.0f);

	/** Bloom-safe (< 1.65f BloomThreshold) sheet interior emissive gain (0.94 * 1.45 = 1.363) for OvershootMagenta. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials|LaserSheet|Palette", meta = (ClampMin = "1.00", ClampMax = "1.65"))
	float OvershootEmissiveGain = 1.45f;

	/** White-hot incandescent contact core (#FFF6D6 chromaticity, exceeds BloomThreshold=1.65f). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials|LaserSheet|Seam")
	FLinearColor SeamCoreLinearColor = FLinearColor(6.20f, 5.10f, 2.90f, 1.0f);

	/** Tight molten gold-orange thermal shoulder matching freshly severed hot monolith cap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials|LaserSheet|Seam")
	FLinearColor SeamShoulderLinearColor = FLinearColor(3.60f, 1.02f, 0.10f, 1.0f);

	/** White-crimson detonation arc core when bWillDetonate (VEIN INTERSECT) is active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials|LaserSheet|Seam")
	FLinearColor VeinHitCoreLinearColor = FLinearColor(6.80f, 1.80f, 1.60f, 1.0f);

	/** Deep plasma crimson arc shoulder when bWillDetonate (VEIN INTERSECT) is active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials|LaserSheet|Seam")
	FLinearColor VeinHitShoulderLinearColor = FLinearColor(4.60f, 0.12f, 0.06f, 1.0f);

	FLaserSheetPresentationConfig GetSanitized() const;
};

USTRUCT(BlueprintType)
struct TAREANDTITHE_API FPhase12EPresentationPolishConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12EPolish|Transition")
	bool bEnableInitial26DopCollisionCache = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12EPolish|Transition")
	bool bEnableSnappyContractTransition = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12EPolish|Transition", meta = (ClampMin = "0.35", ClampMax = "2.50"))
	float ContractPassTransitionDurationSeconds = 1.10f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12EPolish|Transition")
	bool bEnableContractPassFastSkip = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12EPolish|Transition", meta = (ClampMin = "0.10", ClampMax = "1.00"))
	float ContractPassFastSkipDebounceSeconds = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12EPolish|Transition", meta = (ClampMin = "0.00", ClampMax = "1.00"))
	float ContractPassShutterDipAlpha = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12EPolish|WeighingConsole")
	bool bEnableDampedWeightReadout = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12EPolish|WeighingConsole", meta = (ClampMin = "4.0", ClampMax = "40.0"))
	float WeightReadoutInterpSpeed = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12EPolish|WeighingConsole")
	bool bEnableAsciiMassStatusBadge = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12EPolish|PassStamp")
	bool bEnablePassReceiptSubline = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12EPolish|PassStamp")
	bool bEnableStampFovPunch = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12EPolish|PassStamp", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float StampFovPunchDegrees = 1.50f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12EPolish|PassStamp", meta = (ClampMin = "0.08", ClampMax = "0.40"))
	float StampFovPunchDurationSeconds = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12EPolish|FaultFeedback")
	bool bEnableFaultChromaticPulse = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12EPolish|FaultFeedback", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float FaultChromaticPeakIntensity = 2.80f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12EPolish|FaultFeedback", meta = (ClampMin = "0.08", ClampMax = "0.60"))
	float FaultChromaticDurationSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12EPolish|FaultFeedback")
	bool bEnableFaultAmbienceDuck = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12EPolish|FaultFeedback", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FaultAmbienceDuckVolumeScale = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12EPolish|FaultFeedback", meta = (ClampMin = "0.10", ClampMax = "0.60"))
	float FaultAmbienceDuckDurationSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12EPolish|ControlsHud")
	bool bEnableControlsGraphicsRow = true;

	FPhase12EPresentationPolishConfig GetSanitized() const;
};

USTRUCT(BlueprintType)
struct TAREANDTITHE_API FUpscalerPresentationConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upscaler|Defaults")
	ETareUpscalerMode DefaultModeOnSupportedHardware = ETareUpscalerMode::SuperRes_Auto;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upscaler|Defaults")
	ETareUpscalerMode FallbackModeOnUnsupportedHardware = ETareUpscalerMode::NativeTSR;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upscaler|NativeTSR", meta = (ClampMin = "50.0", ClampMax = "100.0"))
	float NativeTsrScaleLowPercent = 65.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upscaler|NativeTSR", meta = (ClampMin = "50.0", ClampMax = "100.0"))
	float NativeTsrScaleMediumPercent = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upscaler|NativeTSR", meta = (ClampMin = "50.0", ClampMax = "100.0"))
	float NativeTsrScaleHighPercent = 85.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upscaler|NativeTSR", meta = (ClampMin = "50.0", ClampMax = "100.0"))
	float NativeTsrScaleEpicPercent = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upscaler|Sharpening", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TonemapperSharpenNativeTsr = 0.35f;

	/** Zero tonemapper sharpening while hardware Super Resolution / Native AA is active to prevent ringing on 6.20f HDR laser seams. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upscaler|Sharpening", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TonemapperSharpenSuperResActive = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upscaler|Controls")
	bool bIncludeUltraPerformanceInCycle = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upscaler|Controls")
	bool bShowControlsUpscalerRow = true;

	FUpscalerPresentationConfig GetSanitized() const;
};

UCLASS(BlueprintType)
class TAREANDTITHE_API UTareTithePresentationDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ============================================================================
	// Audio (Hard References — Cook-Safe under bCookMapsOnly=True)
	// ============================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (DisplayName = "Continuous Loops MetaSound (MS_TelemetryLoops)"))
	TObjectPtr<USoundBase> ContinuousLoopsMetaSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (DisplayName = "Discrete Impulses MetaSound (MS_TelemetryImpulses)"))
	TObjectPtr<USoundBase> DiscreteImpulsesMetaSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundBase> AmbientCellBedSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundAttenuation> DefaultSpatialAttenuation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TMap<ETelemetryImpulseId, TObjectPtr<USoundBase>> SpatialImpulseSounds;

	/** Cold magnetic relay latch + two-tone metallic PA carrier chime + radio squelch tail (~0.45s) for shift/contract directives. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|DirectorIntercom")
	TObjectPtr<USoundBase> DirectorIntercomDirectiveSound = nullptr;

	/** Heavy resonant pneumatic approval chime + harmonic PA triad (~0.50s) for valid stamps & precision Director's Cut. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|DirectorIntercom")
	TObjectPtr<USoundBase> DirectorIntercomApproveSound = nullptr;

	/** Brutal overdriven PA klaxon horn + harsh speaker-cabinet buzz & mic-cut click (~0.70s) for fault reprimands & reject buzz. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|DirectorIntercom")
	TObjectPtr<USoundBase> DirectorIntercomReprimandSound = nullptr;

	/** Phase 12-FIX3 dedicated stereo SFX_ClampSteelGroan_Loop two-zone danger envelope, post-slice creak surge & spatial mix calibration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|ClampGroan")
	FClampGroanAudioConfig ClampGroanAudio;

	// ============================================================================
	// Core Materials
	// ============================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
	TObjectPtr<UMaterialInterface> LaserSheetMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials|LaserSheet")
	FLaserSheetPresentationConfig LaserSheetConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
	TObjectPtr<UMaterialInterface> DefaultMonolithOuterMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
	TObjectPtr<UMaterialInterface> DefaultMoltenCutCapMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
	TObjectPtr<UMaterialInterface> CellWallMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
	TObjectPtr<UMaterialInterface> CellFloorMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
	TObjectPtr<UMaterialInterface> DarkGunmetalMaterial = nullptr;

	// ============================================================================
	// Phase 10 Industrial Cell Lighting, Atmosphere & Post-Process Polish
	// ============================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightingAndAtmosphere")
	FCellLightingRigConfig LightingRigConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightingAndAtmosphere")
	FCellAtmosphereConfig AtmosphereConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightingAndAtmosphere")
	FMoltenBounceLightConfig MoltenBounceConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightingAndAtmosphere")
	FCameraPostProcessPolishConfig CameraPostProcessConfig;

	// ============================================================================
	// Station Mesh Slots
	// ============================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StationMeshes|Clamp")
	FMeshPresentationSlot ClampJawMeshSlot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StationMeshes|Clamp")
	FMeshPresentationSlot ClampSpringHousingMeshSlot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StationMeshes|Scale")
	FMeshPresentationSlot ScaleBaseMeshSlot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StationMeshes|Scale")
	FMeshPresentationSlot ScalePanMeshSlot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StationMeshes|Scale")
	FMeshPresentationSlot DiverterFlapperMeshSlot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StationMeshes|Scale")
	FMeshPresentationSlot TareChuteMeshSlot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StationMeshes|DirectorBooth")
	FMeshPresentationSlot DirectorBoothFrameMeshSlot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StationMeshes|DirectorBooth")
	FMeshPresentationSlot DirectorShutterMeshSlot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StationMeshes|Enclosure")
	FMeshPresentationSlot CellWallPanelMeshSlot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StationMeshes|Enclosure")
	FMeshPresentationSlot CellFloorGrateMeshSlot;

	// ============================================================================
	// Diegetic Console Alignment Offsets
	// ============================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ConsoleAlignment")
	FTransform ScaleConsoleLocalOffset = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ConsoleAlignment")
	FTransform BoothSignLocalOffset = FTransform::Identity;

	// ============================================================================
	// Pure C++ Screen-Space Slate HUD Styling & Hysteresis Configuration
	// ============================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	FHudPresentationConfig HudConfig;

	// ============================================================================
	// Phase 12B Panopticon Surveillance Suppression & Outer Cell Edge Framing
	// ============================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PanopticonSurveillance")
	FPanopticonSurveillanceConfig PanopticonSurveillance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OuterEdgeFraming")
	FOuterEdgeFramingConfig OuterEdgeFraming;

	// ============================================================================
	// Niagara VFX
	// ============================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TObjectPtr<UNiagaraSystem> LaserSliceSparksVFX = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TObjectPtr<UNiagaraSystem> SliverVaporizedVFX = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TObjectPtr<UNiagaraSystem> VeinDetonationVFX = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TObjectPtr<UNiagaraSystem> ScaleHeavyImpactVFX = nullptr;

	// ============================================================================
	// Optional Progression Override
	// ============================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression")
	TObjectPtr<UShiftProgressionDataAsset> ShiftProgressionOverride = nullptr;

	// ============================================================================
	// Phase 12D Industrial Material Wear, Rear Foundry & Panopticon Contrast Polish
	// ============================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12DPolish")
	FPhase12DPresentationPolishConfig Phase12DPolishConfig;

	// ============================================================================
	// Phase 12E Transition Pacing, Micro-Juice, Weighing Console & Quality HUD
	// ============================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase12EPolish")
	FPhase12EPresentationPolishConfig Phase12EPolishConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upscaler")
	FUpscalerPresentationConfig UpscalerConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|System", meta = (ClampMin = "0.15", ClampMax = "2.0"))
	float EscapeQuitHoldDurationSeconds = 0.35f;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
