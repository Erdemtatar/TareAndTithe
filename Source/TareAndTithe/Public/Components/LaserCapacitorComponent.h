// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/TareTitheTypes.h"
#include "LaserCapacitorComponent.generated.h"

class UShiftProgressionDataAsset;

DECLARE_DELEGATE_RetVal_ThreeParams(
	bool,
	FQueryLaserSliceTargetSignature,
	UObject*& /*OutSliceableTarget*/,
	FVector& /*OutPlaneOriginWorld*/,
	FVector& /*OutPlaneNormalWorld*/);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnLaserSliceCommittedSignature, const FSliceResultSummary& /*SliceResult*/);

UCLASS(ClassGroup = (TareAndTithe), meta = (BlueprintSpawnableComponent))
class TAREANDTITHE_API ULaserCapacitorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULaserCapacitorComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void ConfigureFromShiftProgression(const UShiftProgressionDataAsset* ShiftConfig);
	void ResetCapacitorState();

	void BeginCharge();
	bool CancelCharge();

	bool IsCharging() const { return bIsCharging; }
	bool IsCoolingDown() const { return CooldownRemainingSeconds > 0.0f; }
	float GetChargeNormalized() const { return ChargeNormalized; }
	float GetCooldownRemainingSeconds() const { return CooldownRemainingSeconds; }

	FQueryLaserSliceTargetSignature OnQuerySliceTarget;
	FOnLaserSliceCommittedSignature OnSliceCommittedResult;
	FSimpleMulticastDelegate OnChargeCancelled;

	FOnTelemetryStreamSignature OnTelemetryStream;
	FOnTelemetryImpulseSignature OnTelemetryImpulse;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LaserCapacitor", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float LaserCapacitorChargeSeconds = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LaserCapacitor", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float LaserCooldownSeconds = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LaserCapacitor|Telemetry", meta = (ClampMin = "0.0001", ClampMax = "0.05"))
	float ChargeTelemetryDeadband = 0.005f;

private:
	void CommitFullChargeSlice();
	void EmitChargeTelemetry(float NormalizedCharge, bool bForceEmit = false);
	void UpdateTickEnablement();

	float ChargeNormalized = 0.0f;
	float CooldownRemainingSeconds = 0.0f;
	float LastEmittedCharge = -1.0f;
	bool bIsCharging = false;
};
