// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/TareTitheTypes.h"
#include "MonolithContractDataAsset.generated.h"

class UStaticMesh;
class UMaterialInterface;

UCLASS(BlueprintType)
class TAREANDTITHE_API UMonolithContractDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Identity")
	FText ContractCode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FString ContractDirectiveText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Geometry")
	EMonolithBaseShape BaseShape = EMonolithBaseShape::Box;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", meta = (EditCondition = "BaseShape == EMonolithBaseShape::CustomStaticMesh"))
	TObjectPtr<UStaticMesh> CustomMonolithMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Geometry")
	FVector DimensionsCm = FVector(40.0f, 40.0f, 100.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Geometry", meta = (ClampMin = "10.0", ClampMax = "500.0"))
	float BoxProjectionUVScaleCm = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Geometry", meta = (ClampMin = "10.0", ClampMax = "500.0"))
	float CutCapUVWorldDimensionCm = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Physics", meta = (ClampMin = "0.5", ClampMax = "22.0"))
	float DensityGramsPerCm3 = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Quota", meta = (ClampMin = "0.5"))
	float TargetTitheMassKg = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Quota", meta = (ClampMin = "0.05"))
	float QuotaToleranceKg = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Quota", meta = (ClampMin = "0.0"))
	float MaxAllowedTareWasteKg = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Quota")
	bool bAllowDiverterToggle = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Clamp", meta = (ClampMin = "1.0"))
	float ClampMaxSafeTorqueNm = 45.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Clamp", meta = (ClampMin = "0.5", ClampMax = "15.0"))
	float ClampFatigueGraceSeconds = 4.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Clamp", meta = (ClampMin = "0.5", ClampMax = "15.0"))
	float ClampFatigueCooldownSeconds = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Clamp", meta = (ClampMin = "5.0"))
	float ClampSpringStiffness = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Clamp", meta = (ClampMin = "1.0"))
	float ClampSpringDamping = 24.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Clamp")
	FVector2D ClampGripHorizontalOffsetCm = FVector2D::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Veins")
	TArray<FVolatileVeinDefinition> HiddenVeins;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
	TObjectPtr<UMaterialInterface> OuterShellMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
	TObjectPtr<UMaterialInterface> MoltenCutCapMaterial = nullptr;

	bool HasAsymmetricTorqueProfile() const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
