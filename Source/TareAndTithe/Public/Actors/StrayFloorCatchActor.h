// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/TareTitheTypes.h"
#include "Interfaces/TelemetryPublisherInterface.h"
#include "StrayFloorCatchActor.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
struct FHitResult;

DECLARE_MULTICAST_DELEGATE_FourParams(
	FOnStrayChunkCaughtSignature,
	AActor* /*CaughtPayloadActor*/,
	float /*AnalyticalMassKg*/,
	EChunkRoutingState /*PreviousRoutingState*/,
	bool /*bWasContaminated*/);

UCLASS()
class TAREANDTITHE_API AStrayFloorCatchActor : public AActor, public ITelemetryPublisherInterface
{
	GENERATED_BODY()

public:
	AStrayFloorCatchActor();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UBoxComponent* GetCatchBoxComponent() const { return CatchBoxComponent; }

	// ITelemetryPublisherInterface
	virtual FOnTelemetryStreamSignature& GetTelemetryStreamDelegate() override { return OnTelemetryStream; }
	virtual FOnTelemetryImpulseSignature& GetTelemetryImpulseDelegate() override { return OnTelemetryImpulse; }

	FOnStrayChunkCaughtSignature OnStrayChunkCaught;
	FOnTelemetryStreamSignature OnTelemetryStream;
	FOnTelemetryImpulseSignature OnTelemetryImpulse;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StrayCatch")
	TObjectPtr<UBoxComponent> CatchBoxComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StrayCatch", meta = (ClampMin = "10.0"))
	FVector CatchBoxHalfExtentCm = FVector(430.0, 420.0, 25.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StrayCatch")
	bool bKeepStrayChunksVisibleOnCellFloor = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StrayCatch|Telemetry", meta = (ClampMin = "0.5", ClampMax = "100.0"))
	float ImpulseReferenceMassKg = 10.0f;

private:
	UFUNCTION()
	void HandleCatchBoxBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
};
