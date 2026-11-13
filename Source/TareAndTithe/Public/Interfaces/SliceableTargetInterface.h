// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/TareTitheTypes.h"
#include "SliceableTargetInterface.generated.h"

UINTERFACE(MinimalAPI)
class USliceableTargetInterface : public UInterface
{
	GENERATED_BODY()
};

class TAREANDTITHE_API ISliceableTargetInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Slice")
	FSlicePreviewTelemetry PreviewSlicePlane(
		const FVector& PlaneOriginWorld,
		const FVector& PlaneNormalWorld,
		const FLaserSliceAperture& SheetAperture);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Slice")
	FSliceResultSummary CommitSlicePlane(
		const FVector& PlaneOriginWorld,
		const FVector& PlaneNormalWorld,
		const FLaserSliceAperture& SheetAperture);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Slice")
	FBox GetTargetWorldBounds() const;
};
