// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TareTitheGameMode.generated.h"

class UShiftProgressionDataAsset;

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
	FVector DefaultInspectorLocation = FVector(0.0, 0.0, 150.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GreyboxCell")
	FVector DefaultMonolithRigLocation = FVector(220.0, 0.0, 235.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GreyboxCell")
	FVector DefaultScaleAssemblyLocation = FVector(220.0, 0.0, 20.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GreyboxCell")
	FVector DefaultStrayCatchLocation = FVector(220.0, 0.0, -35.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GreyboxCell")
	FVector DefaultDirectorBoothLocation = FVector(410.0, 0.0, 255.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scalability", meta = (ClampMin = "0", ClampMax = "3"))
	int32 BaselineScalabilityLevel = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scalability", meta = (ClampMin = "50.0", ClampMax = "100.0"))
	float BaselineTsrResolutionScalePercent = 85.0f;

private:
	void ApplyBaseline6GbVramScalability();
	void SpawnMissingGreyboxStationsIfNeeded();
};
