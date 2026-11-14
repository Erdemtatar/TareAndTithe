// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/TareTitheTypes.h"
#include "SliceableTargetInterface.generated.h"

USTRUCT(BlueprintType)
struct TAREANDTITHE_API FMonolithCutMaskVolume
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slice|Mask")
	bool bIsValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slice|Mask")
	FVector CenterWorld = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slice|Mask")
	FVector HalfExtentsCm = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slice|Mask")
	FVector AxisXWorld = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slice|Mask")
	FVector AxisYWorld = FVector::RightVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slice|Mask")
	FVector AxisZWorld = FVector::UpVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slice|Mask")
	float JawBottomLocalZ = 0.0f;
};

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

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Slice")
	FMonolithCutMaskVolume GetCuttableWorldMask() const;
};
