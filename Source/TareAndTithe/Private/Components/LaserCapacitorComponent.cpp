// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/LaserCapacitorComponent.h"
#include "Data/ShiftProgressionDataAsset.h"
#include "Interfaces/SliceableTargetInterface.h"

ULaserCapacitorComponent::ULaserCapacitorComponent()
{
	// Tier 2: Enabled strictly while ChargeNormalized > 0 / actively charging or CooldownRemaining > 0.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void ULaserCapacitorComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (DeltaTime <= 0.0f)
	{
		return;
	}

	if (bIsCharging)
	{
		const float SafeChargeDuration = FMath::Max(0.05f, LaserCapacitorChargeSeconds);
		ChargeNormalized = FMath::Clamp(ChargeNormalized + (DeltaTime / SafeChargeDuration), 0.0f, 1.0f);
		EmitChargeTelemetry(ChargeNormalized);

		if (ChargeNormalized >= 1.0f)
		{
			CommitFullChargeSlice();
		}
	}
	else if (CooldownRemainingSeconds > 0.0f)
	{
		CooldownRemainingSeconds = FMath::Max(0.0f, CooldownRemainingSeconds - DeltaTime);
	}

	UpdateTickEnablement();
}

void ULaserCapacitorComponent::ConfigureFromShiftProgression(const UShiftProgressionDataAsset* ShiftConfig)
{
	if (IsValid(ShiftConfig))
	{
		LaserCapacitorChargeSeconds = FMath::Max(0.05f, ShiftConfig->LaserCapacitorChargeSeconds);
		LaserCooldownSeconds = FMath::Max(0.01f, ShiftConfig->LaserCooldownSeconds);
	}
}

void ULaserCapacitorComponent::ResetCapacitorState()
{
	bIsCharging = false;
	ChargeNormalized = 0.0f;
	CooldownRemainingSeconds = 0.0f;
	EmitChargeTelemetry(0.0f, true);
	UpdateTickEnablement();
}

void ULaserCapacitorComponent::BeginCharge()
{
	if (bIsCharging || IsCoolingDown())
	{
		return;
	}

	bIsCharging = true;
	ChargeNormalized = 0.0f;
	EmitChargeTelemetry(0.0f, true);
	UpdateTickEnablement();
}

bool ULaserCapacitorComponent::CancelCharge()
{
	if (!bIsCharging && ChargeNormalized <= 0.0f)
	{
		return false;
	}

	const bool bWasActivelyCharging = bIsCharging;
	bIsCharging = false;
	ChargeNormalized = 0.0f;
	EmitChargeTelemetry(0.0f, true);
	UpdateTickEnablement();

	if (bWasActivelyCharging)
	{
		OnChargeCancelled.Broadcast();
	}

	return bWasActivelyCharging;
}

void ULaserCapacitorComponent::CommitFullChargeSlice()
{
	EmitChargeTelemetry(1.0f, true);

	bIsCharging = false;

	FSliceResultSummary SliceResult;
	SliceResult.Outcome = ECommitSliceOutcome::InvalidMeshState;

	UObject* TargetObj = nullptr;
	FVector PlaneOriginWorld = FVector::ZeroVector;
	FVector PlaneNormalWorld = FVector::UpVector;

	if (OnQuerySliceTarget.IsBound() &&
		OnQuerySliceTarget.Execute(TargetObj, PlaneOriginWorld, PlaneNormalWorld) &&
		IsValid(TargetObj) &&
		TargetObj->Implements<USliceableTargetInterface>())
	{
		SliceResult.WorldCutCentroid = PlaneOriginWorld;
		SliceResult = ISliceableTargetInterface::Execute_CommitSlicePlane(
			TargetObj,
			PlaneOriginWorld,
			PlaneNormalWorld);
	}

	ChargeNormalized = 0.0f;
	CooldownRemainingSeconds = FMath::Max(0.01f, LaserCooldownSeconds);
	EmitChargeTelemetry(0.0f, true);

	if (SliceResult.Outcome == ECommitSliceOutcome::Success)
	{
		OnTelemetryImpulse.Broadcast(ETelemetryImpulseId::SliceCommitted, 1.0f, SliceResult.WorldCutCentroid);
	}

	OnSliceCommittedResult.Broadcast(SliceResult);
}

void ULaserCapacitorComponent::EmitChargeTelemetry(float NormalizedCharge, bool bForceEmit)
{
	const float ClampedCharge = FMath::Clamp(NormalizedCharge, 0.0f, 1.0f);
	const bool bIsBoundary = (ClampedCharge <= 0.0f) || (ClampedCharge >= 1.0f);
	const bool bBoundaryChanged = bIsBoundary && !FMath::IsNearlyEqual(ClampedCharge, LastEmittedCharge, KINDA_SMALL_NUMBER);
	const bool bExceededDeadband = FMath::Abs(ClampedCharge - LastEmittedCharge) >= ChargeTelemetryDeadband;

	if (bForceEmit || LastEmittedCharge < 0.0f || bBoundaryChanged || bExceededDeadband)
	{
		LastEmittedCharge = ClampedCharge;
		OnTelemetryStream.Broadcast(ETelemetryStreamId::LaserCharge, ClampedCharge);
	}
}

void ULaserCapacitorComponent::UpdateTickEnablement()
{
	const bool bNeedsTick = bIsCharging || (ChargeNormalized > 0.0f) || (CooldownRemainingSeconds > 0.0f);
	if (IsComponentTickEnabled() != bNeedsTick)
	{
		SetComponentTickEnabled(bNeedsTick);
	}
}
