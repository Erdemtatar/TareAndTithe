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

USTRUCT(BlueprintType)
struct TAREANDTITHE_API FMeshPresentationSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation|Mesh")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation|Mesh")
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation|Mesh")
	FTransform VisualLocalTransform = FTransform::Identity;

	/**
	 * Safe dual-mode visual applier:
	 * - Returns false (no-op) when Mesh is null so calibrated greybox Cube & RelativeTransform remain 100% untouched.
	 * - When Mesh is valid, assigns Mesh, applies VisualLocalTransform ((1,1,1) default scale), applies MaterialOverrides,
	 *   and guarantees NoCollision on the visual component.
	 */
	bool ApplyToVisualComponent(UStaticMeshComponent* TargetVisualComp) const;
};

UCLASS(BlueprintType)
class TAREANDTITHE_API UTareTithePresentationDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ============================================================================
	// Audio (Hard References — Cook-Safe under bCookMapsOnly=True)
	// ============================================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (DisplayName = "Continuous Loops MetaSound (MS_TelemetryLoops)"))
	TObjectPtr<USoundBase> ContinuousLoopsMetaSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (DisplayName = "Discrete Impulses MetaSound (MS_TelemetryImpulses)"))
	TObjectPtr<USoundBase> DiscreteImpulsesMetaSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<USoundBase> AmbientCellBedSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<USoundAttenuation> DefaultSpatialAttenuation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TMap<ETelemetryImpulseId, TObjectPtr<USoundBase>> SpatialImpulseSounds;

	// ============================================================================
	// Core Materials
	// ============================================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
	TObjectPtr<UMaterialInterface> LaserSheetMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
	TObjectPtr<UMaterialInterface> DefaultMonolithOuterMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
	TObjectPtr<UMaterialInterface> DefaultMoltenCutCapMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
	TObjectPtr<UMaterialInterface> CellWallMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
	TObjectPtr<UMaterialInterface> CellFloorMaterial = nullptr;

	// ============================================================================
	// Station Mesh Slots
	// ============================================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StationMeshes|Clamp")
	FMeshPresentationSlot ClampJawMeshSlot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StationMeshes|Clamp")
	FMeshPresentationSlot ClampSpringHousingMeshSlot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StationMeshes|Scale")
	FMeshPresentationSlot ScaleBaseMeshSlot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StationMeshes|Scale")
	FMeshPresentationSlot ScalePanMeshSlot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StationMeshes|Scale")
	FMeshPresentationSlot DiverterFlapperMeshSlot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StationMeshes|Scale")
	FMeshPresentationSlot TareChuteMeshSlot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StationMeshes|DirectorBooth")
	FMeshPresentationSlot DirectorBoothFrameMeshSlot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StationMeshes|DirectorBooth")
	FMeshPresentationSlot DirectorShutterMeshSlot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StationMeshes|Enclosure")
	FMeshPresentationSlot CellWallPanelMeshSlot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StationMeshes|Enclosure")
	FMeshPresentationSlot CellFloorGrateMeshSlot;

	// ============================================================================
	// Diegetic Console Alignment Offsets
	// ============================================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ConsoleAlignment")
	FTransform ScaleConsoleLocalOffset = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ConsoleAlignment")
	FTransform BoothSignLocalOffset = FTransform::Identity;

	// ============================================================================
	// Niagara VFX
	// ============================================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TObjectPtr<UNiagaraSystem> LaserSliceSparksVFX = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TObjectPtr<UNiagaraSystem> SliverVaporizedVFX = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TObjectPtr<UNiagaraSystem> VeinDetonationVFX = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TObjectPtr<UNiagaraSystem> ScaleHeavyImpactVFX = nullptr;

	// ============================================================================
	// Optional Progression Override
	// ============================================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression")
	TObjectPtr<UShiftProgressionDataAsset> ShiftProgressionOverride = nullptr;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
