// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/ShiftProgressionDataAsset.h"
#include "Data/MonolithContractDataAsset.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "ShiftProgressionDataAsset"

FPrimaryAssetId UShiftProgressionDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("ShiftProgression")), GetFName());
}

#if WITH_EDITOR
EDataValidationResult UShiftProgressionDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	for (int32 Index = 0; Index < OrderedContracts.Num(); ++Index)
	{
		if (OrderedContracts[Index].IsNull())
		{
			Context.AddError(FText::Format(
				LOCTEXT("NullContractEntry", "OrderedContracts[{0}] must not be null."),
				FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
