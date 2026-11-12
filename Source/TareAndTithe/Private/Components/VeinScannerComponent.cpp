// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/VeinScannerComponent.h"
#include "Components/SceneComponent.h"
#include "Data/MonolithContractDataAsset.h"
#include "GameFramework/Actor.h"

UVeinScannerComponent::UVeinScannerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UVeinScannerComponent::SetReferenceFrameComponent(USceneComponent* InReferenceComponent)
{
	ReferenceFrameComponent = InReferenceComponent;
}

void UVeinScannerComponent::InitializeFromContract(const UMonolithContractDataAsset* Contract)
{
	if (IsValid(Contract))
	{
		ActiveVeins = Contract->HiddenVeins;
	}
	else
	{
		ActiveVeins.Reset();
	}

	EmitVeinProximityTelemetry(0.0f, true);
}

void UVeinScannerComponent::SetActiveVeins(const TArray<FVolatileVeinDefinition>& InVeins)
{
	ActiveVeins = InVeins;
	if (ActiveVeins.IsEmpty())
	{
		EmitVeinProximityTelemetry(0.0f, true);
	}
}

void UVeinScannerComponent::ClearActiveVeins()
{
	ActiveVeins.Reset();
	EmitVeinProximityTelemetry(0.0f, true);
}

FSlicePreviewTelemetry UVeinScannerComponent::EvaluateVeinsAgainstLocalPlane(
	const FVector& LocalPlaneOrigin,
	const FVector& LocalPlaneNormal,
	bool bPlaneIntersectsMesh,
	const TArray<FVolatileVeinDefinition>& Veins)
{
	FSlicePreviewTelemetry Result;
	Result.bIntersectsMonolith = false;
	Result.VeinProximityNormalized = 0.0f;
	Result.MinCapsuleClearanceCm = MAX_FLT;
	Result.bWillDetonate = false;

	if (!bPlaneIntersectsMesh)
	{
		return Result;
	}

	const FVector UnitNormal = LocalPlaneNormal.GetSafeNormal();
	if (UnitNormal.IsNearlyZero())
	{
		return Result;
	}

	Result.bIntersectsMonolith = true;

	if (Veins.IsEmpty())
	{
		return Result;
	}

	const double PlaneD = FVector::DotProduct(UnitNormal, LocalPlaneOrigin);
	double MinClearanceCm = static_cast<double>(MAX_FLT);
	float MaxProximityNormalized = 0.0f;
	bool bDetonated = false;

	for (const FVolatileVeinDefinition& Vein : Veins)
	{
		const double SignedDistA = FVector::DotProduct(UnitNormal, Vein.LocalStartCm) - PlaneD;
		const double SignedDistB = FVector::DotProduct(UnitNormal, Vein.LocalEndCm) - PlaneD;
		const double RadiusCm = FMath::Max(0.0, static_cast<double>(Vein.RadiusCm));
		const double MinEndpointAbsDistCm = FMath::Min(FMath::Abs(SignedDistA), FMath::Abs(SignedDistB));

		double ClearanceCm = 0.0;
		if ((SignedDistA * SignedDistB <= 0.0) || (MinEndpointAbsDistCm <= RadiusCm))
		{
			ClearanceCm = 0.0;
			bDetonated = true;
		}
		else
		{
			ClearanceCm = MinEndpointAbsDistCm - RadiusCm;
		}

		MinClearanceCm = FMath::Min(MinClearanceCm, ClearanceCm);

		const double WarningRangeCm = FMath::Max(0.001, static_cast<double>(Vein.WarningRangeCm));
		const double ClampedLinearProximity = FMath::Clamp(1.0 - (ClearanceCm / WarningRangeCm), 0.0, 1.0);
		const float CurvedProximity = static_cast<float>(FMath::Pow(ClampedLinearProximity, VeinProximityFalloffExponent));
		MaxProximityNormalized = FMath::Max(MaxProximityNormalized, FMath::Clamp(CurvedProximity, 0.0f, 1.0f));
	}

	Result.MinCapsuleClearanceCm = (MinClearanceCm >= static_cast<double>(MAX_FLT))
		? MAX_FLT
		: static_cast<float>(MinClearanceCm);
	Result.bWillDetonate = bDetonated || (Result.MinCapsuleClearanceCm <= 0.0f);
	Result.VeinProximityNormalized = FMath::Clamp(MaxProximityNormalized, 0.0f, 1.0f);

	return Result;
}

FSlicePreviewTelemetry UVeinScannerComponent::EvaluateWorldPlane(
	const FVector& PlaneOriginWorld,
	const FVector& PlaneNormalWorld,
	const FTransform& MonolithWorldTransform,
	bool bPlaneIntersectsMesh)
{
	if (!bPlaneIntersectsMesh)
	{
		const FSlicePreviewTelemetry MissTelemetry = EvaluateVeinsAgainstLocalPlane(
			FVector::ZeroVector,
			FVector::UpVector,
			false,
			ActiveVeins);
		EmitVeinProximityTelemetry(0.0f);
		return MissTelemetry;
	}

	const FVector UnitWorldNormal = PlaneNormalWorld.GetSafeNormal();
	const FVector LocalOrigin = MonolithWorldTransform.InverseTransformPosition(PlaneOriginWorld);
	const FVector LocalNormal = MonolithWorldTransform.InverseTransformVector(UnitWorldNormal).GetSafeNormal();

	const FSlicePreviewTelemetry Telemetry = EvaluateVeinsAgainstLocalPlane(
		LocalOrigin,
		LocalNormal,
		true,
		ActiveVeins);

	EmitVeinProximityTelemetry(Telemetry.VeinProximityNormalized);
	return Telemetry;
}

FSlicePreviewTelemetry UVeinScannerComponent::EvaluateWorldPlane(
	const FVector& PlaneOriginWorld,
	const FVector& PlaneNormalWorld,
	bool bPlaneIntersectsMesh)
{
	FTransform ReferenceTransform = FTransform::Identity;
	if (IsValid(ReferenceFrameComponent))
	{
		ReferenceTransform = ReferenceFrameComponent->GetComponentTransform();
	}
	else if (const AActor* OwnerActor = GetOwner())
	{
		ReferenceTransform = OwnerActor->GetActorTransform();
	}

	return EvaluateWorldPlane(PlaneOriginWorld, PlaneNormalWorld, ReferenceTransform, bPlaneIntersectsMesh);
}

void UVeinScannerComponent::EmitVeinProximityTelemetry(float NormalizedProximity, bool bForceEmit)
{
	const float ClampedValue = FMath::Clamp(NormalizedProximity, 0.0f, 1.0f);
	const bool bIsBoundary = (ClampedValue <= 0.0f) || (ClampedValue >= 1.0f);
	const bool bBoundaryChanged = bIsBoundary && !FMath::IsNearlyEqual(ClampedValue, LastEmittedProximity, KINDA_SMALL_NUMBER);
	const bool bExceededDeadband = FMath::Abs(ClampedValue - LastEmittedProximity) >= StreamDeadband;

	if (bForceEmit || LastEmittedProximity < 0.0f || bBoundaryChanged || bExceededDeadband)
	{
		LastEmittedProximity = ClampedValue;
		OnTelemetryStream.Broadcast(ETelemetryStreamId::VeinProximity, ClampedValue);
	}
}
