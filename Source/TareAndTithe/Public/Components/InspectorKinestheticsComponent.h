// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/TareTitheTypes.h"
#include "InspectorKinestheticsComponent.generated.h"

class UAudioTelemetryBridgeComponent;
class UCameraComponent;
class USceneComponent;
class UTareTithePresentationDataAsset;

UCLASS(ClassGroup = (TareAndTithe), meta = (BlueprintSpawnableComponent))
class TAREANDTITHE_API UInspectorKinestheticsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInspectorKinestheticsComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void BindShakeAndCameraComponents(
		USceneComponent* InShakeOffsetComponent,
		UCameraComponent* InCameraComponent,
		float InBaseFieldOfView);

	void BindTelemetryBridge(UAudioTelemetryBridgeComponent* InTelemetryBridge);
	void SetPresentationData(const UTareTithePresentationDataAsset* InPresentationData);
	void ResetKinestheticsImmediate();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kinesthetics|Charge", meta = (ClampMin = "0.0", ClampMax = "0.28"))
	float MaxChargeHumTranslationCm = 0.24f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kinesthetics|Charge", meta = (ClampMin = "0.0", ClampMax = "0.18"))
	float MaxChargeHumRotationDegrees = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kinesthetics|Charge", meta = (ClampMin = "10.0", ClampMax = "90.0"))
	float ChargeHumPrimaryFrequencyHz = 37.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kinesthetics|Charge", meta = (ClampMin = "15.0", ClampMax = "120.0"))
	float ChargeHumSecondaryFrequencyHz = 61.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kinesthetics|Charge", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float MaxChargeFovPullDegrees = 1.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kinesthetics|Charge", meta = (ClampMin = "2.0", ClampMax = "60.0"))
	float ChargeFovPullInterpSpeed = 14.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kinesthetics|Charge", meta = (ClampMin = "5.0", ClampMax = "80.0"))
	float FovReleaseInterpSpeed = 28.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kinesthetics|Recoil", meta = (ClampMin = "20.0", ClampMax = "800.0"))
	float RecoilSpringStiffness = 260.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kinesthetics|Recoil", meta = (ClampMin = "4.0", ClampMax = "80.0"))
	float RecoilSpringDamping = 26.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kinesthetics|Recoil", meta = (ClampMin = "0.2", ClampMax = "6.0"))
	float MaxRecoilTranslationOffsetCm = 2.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kinesthetics|Recoil", meta = (ClampMin = "0.2", ClampMax = "6.0"))
	float MaxRecoilRotationOffsetDegrees = 1.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kinesthetics|Recoil", meta = (ClampMin = "0.0", ClampMax = "80.0"))
	float SliceRecoilBackImpulseCmPerSec = 24.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kinesthetics|Recoil", meta = (ClampMin = "0.0", ClampMax = "40.0"))
	float SliceRecoilUpImpulseCmPerSec = 8.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kinesthetics|Recoil", meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float SliceRecoilPitchImpulseDegPerSec = 13.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kinesthetics|Impacts", meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float ScaleImpactDownwardImpulseCmPerSec = 16.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kinesthetics|Impacts", meta = (ClampMin = "0.0", ClampMax = "25.0"))
	float ScaleImpactPitchImpulseDegPerSec = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kinesthetics|Impacts", meta = (ClampMin = "0.0", ClampMax = "80.0"))
	float FaultThudDownwardImpulseCmPerSec = 28.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kinesthetics|Impacts", meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float FaultThudPitchImpulseDegPerSec = 14.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kinesthetics|Impacts", meta = (ClampMin = "0.0", ClampMax = "25.0"))
	float FaultThudRollImpulseDegPerSec = 9.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kinesthetics|Impacts", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ManualResetFaultShakeScale = 0.30f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kinesthetics|VFX", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HeavyScaleImpactVfxMinMagnitude = 0.12f;

private:
	void HandleTelemetryStream(ETelemetryStreamId StreamId, float NormalizedValue);
	void HandleTelemetryImpulse(ETelemetryImpulseId ImpulseId, float NormalizedMagnitude, const FVector& WorldLocation);
	void SpawnImpulseVfxIfAssigned(ETelemetryImpulseId ImpulseId, float ClampedMagnitude, const FVector& WorldLocation);
	void EnsureActiveTickEnabled();
	bool HasConvergedToRest() const;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> ShakeOffsetComponent;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioTelemetryBridgeComponent> BoundTelemetryBridge;

	UPROPERTY(Transient)
	TObjectPtr<const UTareTithePresentationDataAsset> PresentationData;

	FVector RecoilOffsetCm = FVector::ZeroVector;
	FVector RecoilVelocityCmPerSec = FVector::ZeroVector;
	FVector RecoilRotationDeg = FVector::ZeroVector;
	FVector RecoilAngularVelocityDegPerSec = FVector::ZeroVector;

	float BaseFieldOfView = 90.0f;
	float CurrentFovOffsetDegrees = 0.0f;
	float CurrentChargeNormalized = 0.0f;
	double ChargeHumPhaseSeconds = 0.0;
	float FaultRollAlternatingSign = 1.0f;

	float StampFovPunchElapsedSeconds = 0.0f;
	float StampFovPunchScale = 1.0f;
	float CurrentStampFovPunchDegrees = 0.0f;
	bool bStampFovPunchActive = false;

	float BaseSceneFringeIntensity = 0.0f;
	float FaultChromaticElapsedSeconds = 0.0f;
	float FaultChromaticPeakActive = 0.0f;
	bool bBaseOverrideSceneFringe = false;
	bool bFaultChromaticPulseActive = false;
};
