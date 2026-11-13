// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MoltenThermalLightComponent.generated.h"

class UPointLightComponent;

/**
 * Tier 2 transient thermal bounce-light driver:
 * - PrimaryComponentTick.bStartWithTickEnabled = false (strictly 0.00 ms CPU when idle).
 * - Wakes ONLY during bounded slice/impact thermal flash decay or steady ember interpolation,
 *   disabling tick immediately upon convergence and hiding the unshadowed light when dark.
 */
UCLASS(ClassGroup = (Presentation), meta = (BlueprintSpawnableComponent))
class TAREANDTITHE_API UMoltenThermalLightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMoltenThermalLightComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	void InitializeTargetLight(UPointLightComponent* InTargetLight);

	void ConfigureLightParameters(
		const FLinearColor& InBounceColor,
		float InAttenuationRadiusCm,
		float InFlashDecayDurationSeconds,
		float InSteadyInterpSpeed);

	void TriggerThermalFlash(float PeakFlashIntensity, float NewSteadyIntensity);
	void SetSteadyEmberIntensity(float NewSteadyIntensity);
	void ResetThermalImmediate();

	float GetTargetSteadyIntensity() const { return TargetSteadyIntensity; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoltenThermal", meta = (ClampMin = "0.05", ClampMax = "5.0"))
	float ConvergenceEpsilonIntensity = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoltenThermal", meta = (ClampMin = "0.10", ClampMax = "4.0"))
	float FlashDecayDurationSeconds = 0.85f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoltenThermal", meta = (ClampMin = "0.5", ClampMax = "30.0"))
	float SteadyInterpSpeed = 6.0f;

private:
	void ApplyCurrentLightState(float CombinedIntensity);

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> TargetLight = nullptr;

	float CurrentSteadyIntensity = 0.0f;
	float TargetSteadyIntensity = 0.0f;
	float FlashSurgeRemainingIntensity = 0.0f;
	float FlashInitialSurgeIntensity = 0.0f;
	float FlashElapsedSeconds = 0.0f;
};
