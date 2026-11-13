// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/MonolithContractDataAsset.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "MonolithContractDataAsset"

FPrimaryAssetId UMonolithContractDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("MonolithContract")), GetFName());
}

bool UMonolithContractDataAsset::HasAsymmetricTorqueProfile() const
{
	if (!ClampGripHorizontalOffsetCm.IsNearlyZero(0.05f))
	{
		return true;
	}

	return (BaseShape == EMonolithBaseShape::AsymmetricLBlock) ||
		(BaseShape == EMonolithBaseShape::CantileveredJHook) ||
		(BaseShape == EMonolithBaseShape::OffsetCChannel) ||
		(BaseShape == EMonolithBaseShape::CounterweightedZBracket);
}

#if WITH_EDITOR
EDataValidationResult UMonolithContractDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	if (TargetTitheMassKg <= QuotaToleranceKg)
	{
		Context.AddError(LOCTEXT("InvalidTargetTitheMass", "TargetTitheMassKg must be strictly greater than QuotaToleranceKg."));
		Result = EDataValidationResult::Invalid;
	}

	if (DimensionsCm.GetMin() < 5.0)
	{
		Context.AddError(LOCTEXT("InvalidMonolithDimensions", "DimensionsCm minimum axis must be at least 5.0 cm."));
		Result = EDataValidationResult::Invalid;
	}

	if (BaseShape == EMonolithBaseShape::CustomStaticMesh && !IsValid(CustomMonolithMesh))
	{
		Context.AddError(LOCTEXT("MissingCustomStaticMesh", "CustomMonolithMesh must be assigned when BaseShape is CustomStaticMesh."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
