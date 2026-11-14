// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "UObject/Interface.h"
#include "UObject/ScriptInterface.h"
#include "UpscalerControlInterface.generated.h"

struct FUpscalerPresentationConfig;

UENUM(BlueprintType)
enum class ETareUpscalerMode : uint8
{
	NativeTSR                = 0 UMETA(DisplayName = "Native TSR"),
	SuperRes_Auto            = 1 UMETA(DisplayName = "Super Resolution Auto"),
	SuperRes_Quality         = 2 UMETA(DisplayName = "Super Resolution Quality"),
	SuperRes_Balanced        = 3 UMETA(DisplayName = "Super Resolution Balanced"),
	SuperRes_Performance     = 4 UMETA(DisplayName = "Super Resolution Performance"),
	SuperRes_UltraPerformance = 5 UMETA(DisplayName = "Super Resolution Ultra Performance"),
	SuperRes_NativeAA        = 6 UMETA(DisplayName = "Super Resolution Native AA")
};

UENUM(BlueprintType)
enum class ETareSuperResAvailability : uint8
{
	CompiledOut                 = 0 UMETA(DisplayName = "Compiled Out"),
	UnsupportedHardwareOrDriver = 1 UMETA(DisplayName = "Unsupported Hardware Or Driver"),
	Available                   = 2 UMETA(DisplayName = "Available")
};

USTRUCT(BlueprintType)
struct TAREANDTITHE_API FTareGraphicsStateSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Graphics")
	int32 ScalabilityQualityLevel = 2;

	UPROPERTY(BlueprintReadOnly, Category = "Graphics")
	ETareUpscalerMode RequestedUpscalerMode = ETareUpscalerMode::NativeTSR;

	UPROPERTY(BlueprintReadOnly, Category = "Graphics")
	ETareUpscalerMode ActiveUpscalerMode = ETareUpscalerMode::NativeTSR;

	UPROPERTY(BlueprintReadOnly, Category = "Graphics")
	ETareSuperResAvailability SuperResAvailability = ETareSuperResAvailability::CompiledOut;

	UPROPERTY(BlueprintReadOnly, Category = "Graphics")
	float EffectiveScreenPercentage = 85.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Graphics")
	float NativeTsrScalePercent = 85.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Graphics")
	bool bDidFallbackToNativeTsr = false;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTareGraphicsStateChangedSignature, const FTareGraphicsStateSnapshot& /*NewState*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTareGraphicsStateChangedDynamicSignature, const FTareGraphicsStateSnapshot&, NewState);

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UUpscalerControlInterface : public UInterface
{
	GENERATED_BODY()
};

class TAREANDTITHE_API IUpscalerControlInterface
{
	GENERATED_BODY()

public:
	static TScriptInterface<IUpscalerControlInterface> Resolve(const UObject* WorldContextObject);

	virtual void ApplyScalabilityQualityLevel(int32 InQualityLevel) = 0;
	virtual bool RequestUpscalerMode(ETareUpscalerMode InMode) = 0;
	virtual ETareUpscalerMode CycleUpscalerMode() = 0;
	virtual void ApplyPresentationUpscalerConfig(const FUpscalerPresentationConfig& InConfig) = 0;
	virtual FTareGraphicsStateSnapshot GetGraphicsStateSnapshot() const = 0;
	virtual FOnTareGraphicsStateChangedSignature& OnGraphicsStateChanged() = 0;
};
