// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/TareTitheTypes.h"
#include "MassPayloadInterface.generated.h"

UINTERFACE(MinimalAPI)
class UMassPayloadInterface : public UInterface
{
	GENERATED_BODY()
};

class TAREANDTITHE_API IMassPayloadInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Payload")
	float GetAnalyticalMassKg() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Payload")
	bool IsVolatileContaminated() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Payload")
	EChunkRoutingState GetRoutingState() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Payload")
	void NotifyRoutingStateChanged(EChunkRoutingState NewState);

	UFUNCTION(BlueprintNativeEvent, Category = "Payload")
	void ReturnToChunkPool();
};
