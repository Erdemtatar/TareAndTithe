// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/TareTitheTypes.h"
#include "ScaleWeighingComponent.generated.h"

class USceneComponent;
class UPrimitiveComponent;
class UMonolithContractDataAsset;
struct FHitResult;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnScaleChunkEventSignature, AActor* /*PayloadActor*/, float /*ChunkMassKg*/, float /*UpdatedPanMassKg*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnScaleContaminationSignature, AActor* /*ContaminatedActor*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnScaleSettledChangedSignature, bool /*bIsSettled*/, float /*SettledPanMassKg*/);

UCLASS(ClassGroup = (TareAndTithe), meta = (BlueprintSpawnableComponent))
class TAREANDTITHE_API UScaleWeighingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UScaleWeighingComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void SetPanPlatformComponent(USceneComponent* InPanPlatformComponent);
	USceneComponent* GetPanPlatformComponent() const { return PanPlatformComponent; }

	void BindPanContainmentTrigger(UPrimitiveComponent* InTriggerComponent);
	UPrimitiveComponent* GetPanContainmentTrigger() const { return PanContainmentTrigger; }

	void ConfigureForContract(const UMonolithContractDataAsset* Contract);
	void SetTargetQuotaMassKg(float InTargetQuotaMassKg);
	void ResetScaleState();

	float GetPanMassKg() const { return PanMassKg; }
	float GetSettledMassKg() const { return PanMassKg; }
	bool IsScaleSettled() const { return bIsSettled; }
	bool HasContaminationFault() const { return bHasContamination; }
	int32 GetOverlappingPayloadCount() const { return TrackedPayloads.Num(); }

	FOnScaleChunkEventSignature OnChunkLandedOnScale;
	FOnScaleChunkEventSignature OnChunkBouncedOffScale;
	FOnScaleContaminationSignature OnScaleContaminationDetected;
	FOnScaleSettledChangedSignature OnScaleSettledChanged;
	FOnTelemetryStreamSignature OnTelemetryStream;
	FOnTelemetryImpulseSignature OnTelemetryImpulse;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scale|Spring", meta = (ClampMin = "0.05", ClampMax = "5.0"))
	float DepressionCmPerKg = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scale|Spring", meta = (ClampMin = "1.0", ClampMax = "40.0"))
	float MaxDepressionCm = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scale|Spring", meta = (ClampMin = "2.0", ClampMax = "40.0"))
	float NaturalFrequencyRadPerSec = 14.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scale|Spring", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float DampingRatio = 0.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scale|Solver", meta = (ClampMin = "0.02", ClampMax = "0.25"))
	float MaxFrameDeltaSeconds = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scale|Solver", meta = (ClampMin = "0.002", ClampMax = "0.016667"))
	float MaxSubstepDeltaSeconds = 1.0f / 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scale|Settle", meta = (ClampMin = "0.005", ClampMax = "0.5"))
	float PanPositionSettleToleranceCm = 0.02f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scale|Settle", meta = (ClampMin = "0.01", ClampMax = "2.0"))
	float PanVelocitySettleToleranceCmPerSec = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scale|Settle", meta = (ClampMin = "0.2", ClampMax = "15.0"))
	float PayloadLinearSpeedSettleThresholdCmPerSec = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scale|Settle", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float SettleHoldRequiredSeconds = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scale|Telemetry", meta = (ClampMin = "0.001", ClampMax = "0.05"))
	float TelemetryDeadband = 0.01f;

private:
	struct FTrackedPanPayload
	{
		TWeakObjectPtr<AActor> PayloadActor;
		TWeakObjectPtr<UPrimitiveComponent> PayloadPrimitive;
	};

	UFUNCTION()
	void HandlePanTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandlePanTriggerEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	int32 FindTrackedPayloadIndex(const AActor* PayloadActor) const;
	void RecomputeAnalyticalPanMass();
	bool AreAllPayloadsBelowSettleVelocity() const;
	void WakeOverlappingPayloads();
	void ApplyPanPlatformDisplacement(bool bSweep);
	void SetSettledState(bool bNewSettled);
	void EmitFillRatioTelemetry(float NormalizedFillRatio, bool bForceEmit = false);

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> PanPlatformComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> PanContainmentTrigger;

	TArray<FTrackedPanPayload> TrackedPayloads;

	FVector NeutralPanRelativeLocation = FVector::ZeroVector;
	double CurrentDisplacementZCm = 0.0;
	double TargetDisplacementZCm = 0.0;
	double VelocityZCmPerSec = 0.0;

	float PanMassKg = 0.0f;
	float TargetQuotaMassKg = 10.0f;
	float SettleHoldElapsedSeconds = 0.0f;
	float LastEmittedFillRatio = -1.0f;

	bool bHasContamination = false;
	bool bIsSettled = true;
};
