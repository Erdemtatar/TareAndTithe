// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TareTitheGameMode.generated.h"

class UMaterialInterface;
class UShiftProgressionDataAsset;
class UStaticMesh;

UCLASS()
class TAREANDTITHE_API ATareTitheGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATareTitheGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shift")
	TObjectPtr<UShiftProgressionDataAsset> DefaultShiftProgression;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GreyboxCell")
	bool bSpawnGreyboxCellIfStationsMissing = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GreyboxCell")
	FVector DefaultInspectorLocation = FVector(0.0, 0.0, 160.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GreyboxCell")
	FVector DefaultMonolithRigLocation = FVector(260.0, 0.0, 236.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GreyboxCell")
	FVector DefaultScaleAssemblyLocation = FVector(260.0, 0.0, 14.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GreyboxCell")
	FVector DefaultStrayCatchLocation = FVector(260.0, 0.0, -35.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GreyboxCell")
	FVector DefaultDirectorBoothLocation = FVector(430.0, 0.0, 258.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GreyboxCell")
	FLinearColor GreyboxPitEnclosureColor = FLinearColor(0.035f, 0.038f, 0.043f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GreyboxCell", meta = (ClampMax = "-150.0"))
	float RearCellWallXCm = -200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GreyboxCell|Lighting", meta = (ClampMin = "100.0", ClampMax = "25000.0"))
	float KeySpotLightIntensity = 5800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GreyboxCell|Lighting", meta = (ClampMin = "50.0", ClampMax = "15000.0"))
	float FillPointLightIntensity = 2400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scalability", meta = (ClampMin = "0", ClampMax = "3"))
	int32 BaselineScalabilityLevel = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scalability", meta = (ClampMin = "50.0", ClampMax = "100.0"))
	float BaselineTsrResolutionScalePercent = 85.0f;

private:
	void ApplyBaseline6GbVramScalability();
	void SpawnMissingGreyboxStationsIfNeeded();
	void SpawnGreyboxPitEnclosureAndLighting(UWorld* World);

	UPROPERTY()
	TObjectPtr<UStaticMesh> GreyboxCellCubeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> GreyboxCellBaseMaterial;
};
