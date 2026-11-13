// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/TareTitheTypes.h"
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

	// ============================================================================
	// Core Materials
	// ============================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
	TObjectPtr<UMaterialInterface> LaserSheetMaterial = nullptr;

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

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
