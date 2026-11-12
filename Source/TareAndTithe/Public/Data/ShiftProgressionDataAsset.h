// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/SoftObjectPtr.h"
#include "ShiftProgressionDataAsset.generated.h"

class UMonolithContractDataAsset;

UCLASS(BlueprintType)
class TAREANDTITHE_API UShiftProgressionDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shift")
	TArray<TSoftObjectPtr<UMonolithContractDataAsset>> OrderedContracts;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Laser", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float LaserCapacitorChargeSeconds = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Laser", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float LaserCooldownSeconds = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slicer", meta = (ClampMin = "5.0", ClampMax = "250.0"))
	float MinSliverVolumeCm3 = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slicer", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float MinChunkAxisThicknessCm = 0.30f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slicer", meta = (ClampMin = "1.0", ClampMax = "100.0"))
	float ChunkSeparationSpeedCmPerSec = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Safety", meta = (ClampMin = "1.5", ClampMax = "10.0"))
	float MaxInFlightWatchdogSeconds = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Safety", meta = (ClampMin = "0.2", ClampMax = "3.0"))
	float FaultAutoResetDelaySeconds = 1.8f;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
