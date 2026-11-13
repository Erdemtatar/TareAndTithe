// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/TareTithePresentationDataAsset.h"
#include "CellFramingArchitectureActor.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class USceneComponent;
class UStaticMesh;

/**
 * Phase 12B / 12-FIX2 Zero-Tick Procedural Outer Cell Edge Architectural Framing Actor.
 * Populates symmetrical left (0%..18% X) and right (82%..100% X) industrial columns, modular flanged cast-iron conduit courses,
 * lateral structural tie-gussets, and a dark upper HUD contrast soffit in 4 zero-shadow, zero-HWRT ISM batches (64 instances).
 */
UCLASS()
class TAREANDTITHE_API ACellFramingArchitectureActor : public AActor
{
	GENERATED_BODY()

public:
	ACellFramingArchitectureActor();

	void ConfigureFraming(const UTareTithePresentationDataAsset* InPresentationData);

	UFUNCTION(BlueprintPure, Category = "OuterFraming|Diagnostics")
	int32 GetTotalFramingInstanceCount() const;

	UFUNCTION(BlueprintPure, Category = "OuterFraming|Diagnostics")
	UInstancedStaticMeshComponent* GetOuterHeavyColumnsISM() const { return OuterHeavyColumnsISM; }

	UFUNCTION(BlueprintPure, Category = "OuterFraming|Diagnostics")
	UInstancedStaticMeshComponent* GetOuterFlangedPipesISM() const { return OuterFlangedPipesISM; }

	UFUNCTION(BlueprintPure, Category = "OuterFraming|Diagnostics")
	UInstancedStaticMeshComponent* GetOuterTieGussetsISM() const { return OuterTieGussetsISM; }

	UFUNCTION(BlueprintPure, Category = "OuterFraming|Diagnostics")
	UInstancedStaticMeshComponent* GetOuterStructuralRibsISM() const { return OuterStructuralRibsISM; }

	UFUNCTION(BlueprintPure, Category = "OuterFraming|Diagnostics")
	UMaterialInstanceDynamic* GetOuterPipesMID() const { return OuterPipesMID; }

	UFUNCTION(BlueprintPure, Category = "OuterFraming|Diagnostics")
	UMaterialInstanceDynamic* GetOuterColumnsMID() const { return OuterColumnsMID; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OuterFraming")
	TObjectPtr<USceneComponent> FramingRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OuterFraming")
	TObjectPtr<UInstancedStaticMeshComponent> OuterHeavyColumnsISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OuterFraming")
	TObjectPtr<UInstancedStaticMeshComponent> OuterFlangedPipesISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OuterFraming")
	TObjectPtr<UInstancedStaticMeshComponent> OuterTieGussetsISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OuterFraming")
	TObjectPtr<UInstancedStaticMeshComponent> OuterStructuralRibsISM;

private:
	void RebuildOptionalDedicatedRimLights(const FOuterEdgeFramingConfig& Cfg);

	UPROPERTY()
	TObjectPtr<UStaticMesh> BasicCubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> BasicCylinderMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> FallbackFlattenMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OuterColumnsMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OuterPipesMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OuterDarkSoffitMID;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> LeftDedicatedRimLight;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> RightDedicatedRimLight;
};
