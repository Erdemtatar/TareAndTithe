// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/TareTitheTypes.h"
#include "VeinScannerComponent.generated.h"

class UMonolithContractDataAsset;
class USceneComponent;

UCLASS(ClassGroup = (TareAndTithe), meta = (BlueprintSpawnableComponent))
class TAREANDTITHE_API UVeinScannerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	static constexpr double VeinProximityFalloffExponent = 1.8;

	UVeinScannerComponent();

	void SetReferenceFrameComponent(USceneComponent* InReferenceComponent);
	USceneComponent* GetReferenceFrameComponent() const { return ReferenceFrameComponent; }

	void InitializeFromContract(const UMonolithContractDataAsset* Contract);
	void SetActiveVeins(const TArray<FVolatileVeinDefinition>& InVeins);
	void SyncPostSliceActiveVeins(const FBox& RemainderLocalBounds, float BoundsSafetyToleranceCm = 2.0f);
	void ClearActiveVeins();

	const TArray<FVolatileVeinDefinition>& GetActiveVeins() const { return ActiveVeins; }
	TArray<FVolatileVeinDefinition>& GetMutableActiveVeins() { return ActiveVeins; }
	int32 GetActiveVeinCount() const { return ActiveVeins.Num(); }

	static bool DoesLocalHalfSpaceContainVein(
		const FVector& LocalPlaneOrigin,
		const FVector& SeveredLocalUnitNormal,
		const TArray<FVolatileVeinDefinition>& Veins,
		const FBox& RemainderLocalBounds,
		float BoundsSafetyToleranceCm = 2.0f);

	bool DoesSeveredHalfSpaceContainActiveVein(
		const FVector& LocalPlaneOrigin,
		const FVector& SeveredLocalUnitNormal,
		const FBox& RemainderLocalBounds,
		float BoundsSafetyToleranceCm = 2.0f) const;

	static FSlicePreviewTelemetry EvaluateVeinsAgainstLocalPlane(
		const FVector& LocalPlaneOrigin,
		const FVector& LocalPlaneNormal,
		bool bPlaneIntersectsMesh,
		const TArray<FVolatileVeinDefinition>& Veins);

	FSlicePreviewTelemetry EvaluateWorldPlane(
		const FVector& PlaneOriginWorld,
		const FVector& PlaneNormalWorld,
		const FTransform& MonolithWorldTransform,
		bool bPlaneIntersectsMesh);

	FSlicePreviewTelemetry EvaluateWorldPlane(
		const FVector& PlaneOriginWorld,
		const FVector& PlaneNormalWorld,
		bool bPlaneIntersectsMesh);

	FOnTelemetryStreamSignature OnTelemetryStream;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VeinScanner|Telemetry", meta = (ClampMin = "0.0001", ClampMax = "0.05"))
	float StreamDeadband = 0.002f;

private:
	void EmitVeinProximityTelemetry(float NormalizedProximity, bool bForceEmit = false);

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> ReferenceFrameComponent;

	UPROPERTY(Transient)
	TArray<FVolatileVeinDefinition> ActiveVeins;

	float LastEmittedProximity = -1.0f;
};
