// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShiftProgressionDataAsset.generated.h"

class UMonolithContractDataAsset;

UCLASS(BlueprintType)
class TAREANDTITHE_API UShiftProgressionDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shift")
	TArray<TObjectPtr<UMonolithContractDataAsset>> OrderedContracts;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float LaserCapacitorChargeSeconds = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float LaserCooldownSeconds = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Slicer", meta = (ClampMin = "5.0", ClampMax = "250.0"))
	float MinSliverVolumeCm3 = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Slicer", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float MinChunkAxisThicknessCm = 0.30f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Slicer", meta = (ClampMin = "1.0", ClampMax = "100.0"))
	float ChunkSeparationSpeedCmPerSec = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Safety", meta = (ClampMin = "1.5", ClampMax = "15.0"))
	float MaxInFlightWatchdogSeconds = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Safety", meta = (ClampMin = "0.2", ClampMax = "3.0"))
	float FaultAutoResetDelaySeconds = 1.8f;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
