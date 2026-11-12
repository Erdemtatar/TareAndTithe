// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/TareTitheTypes.h"
#include "QuotaReceiverInterface.generated.h"

UINTERFACE(MinimalAPI)
class UQuotaReceiverInterface : public UInterface
{
	GENERATED_BODY()
};

class TAREANDTITHE_API IQuotaReceiverInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Quota")
	float GetSettledMassKg() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Quota")
	bool IsReceiverSettled() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Quota")
	bool HasContaminationFault() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Quota")
	EDiverterRoute GetActiveDiverterRoute() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Quota")
	void ResetReceiverState();
};
