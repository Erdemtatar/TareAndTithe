// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/TareTitheTypes.h"
#include "MassPayloadInterface.generated.h"

class AActor;
class UPrimitiveComponent;

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

	UFUNCTION(BlueprintNativeEvent, Category = "Payload")
	UPrimitiveComponent* GetPayloadPhysicsPrimitive() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Payload")
	AActor* GetScaleSupportReceiverActor() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Payload")
	void SetScaleSupportReceiverActor(AActor* NewSupportReceiver);

	UFUNCTION(BlueprintNativeEvent, Category = "Payload")
	void ArmTareAbyssFailsafeTimer(float FailsafeDurationSeconds);
};
