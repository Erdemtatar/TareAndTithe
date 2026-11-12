// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/TareTitheTypes.h"
#include "TelemetryPublisherInterface.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UTelemetryPublisherInterface : public UInterface
{
	GENERATED_BODY()
};

class TAREANDTITHE_API ITelemetryPublisherInterface
{
	GENERATED_BODY()

public:
	virtual FOnTelemetryStreamSignature& GetTelemetryStreamDelegate() = 0;
	virtual FOnTelemetryImpulseSignature& GetTelemetryImpulseDelegate() = 0;
};
