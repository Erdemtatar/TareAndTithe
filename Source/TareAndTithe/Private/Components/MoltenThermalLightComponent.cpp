// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MoltenThermalLightComponent.h"
#include "Components/PointLightComponent.h"

UMoltenThermalLightComponent::UMoltenThermalLightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UMoltenThermalLightComponent::InitializeTargetLight(UPointLightComponent* InTargetLight)
{
	TargetLight = InTargetLight;
	if (IsValid(TargetLight))
	{
		TargetLight->SetCastShadows(false);
		TargetLight->SetVolumetricScatteringIntensity(0.0f);
		TargetLight->SetIntensity(0.0f);
		TargetLight->SetVisibility(false, false);
	}
	ResetThermalImmediate();
}

void UMoltenThermalLightComponent::ConfigureLightParameters(
	const FLinearColor& InBounceColor,
	float InAttenuationRadiusCm,
	float InFlashDecayDurationSeconds,
	float InSteadyInterpSpeed)
{
	FlashDecayDurationSeconds = FMath::Clamp(InFlashDecayDurationSeconds, 0.10f, 4.0f);
	SteadyInterpSpeed = FMath::Clamp(InSteadyInterpSpeed, 0.5f, 30.0f);

	if (IsValid(TargetLight))
	{
		TargetLight->SetCastShadows(false);
		TargetLight->SetVolumetricScatteringIntensity(0.0f);
		TargetLight->SetLightColor(InBounceColor);
		TargetLight->SetAttenuationRadius(FMath::Clamp(InAttenuationRadiusCm, 40.0f, 1000.0f));
	}
}

void UMoltenThermalLightComponent::TriggerThermalFlash(float PeakFlashIntensity, float NewSteadyIntensity)
{
	TargetSteadyIntensity = FMath::Max(0.0f, NewSteadyIntensity);
	const float ClampedPeak = FMath::Max(TargetSteadyIntensity, PeakFlashIntensity);
	const float DesiredSurge = FMath::Max(0.0f, ClampedPeak - CurrentSteadyIntensity);

	if (DesiredSurge >= FlashSurgeRemainingIntensity)
	{
		FlashInitialSurgeIntensity = DesiredSurge;
		FlashSurgeRemainingIntensity = DesiredSurge;
		FlashElapsedSeconds = 0.0f;
	}

	const float ImmediateCombined = CurrentSteadyIntensity + FlashSurgeRemainingIntensity;
	ApplyCurrentLightState(ImmediateCombined);

	if (FlashSurgeRemainingIntensity > ConvergenceEpsilonIntensity ||
		!FMath::IsNearlyEqual(CurrentSteadyIntensity, TargetSteadyIntensity, ConvergenceEpsilonIntensity))
	{
		SetComponentTickEnabled(true);
	}
}

void UMoltenThermalLightComponent::SetSteadyEmberIntensity(float NewSteadyIntensity)
{
	TargetSteadyIntensity = FMath::Max(0.0f, NewSteadyIntensity);

	if (FlashSurgeRemainingIntensity > ConvergenceEpsilonIntensity ||
		!FMath::IsNearlyEqual(CurrentSteadyIntensity, TargetSteadyIntensity, ConvergenceEpsilonIntensity))
	{
		if (IsValid(TargetLight) && TargetSteadyIntensity > KINDA_SMALL_NUMBER && !TargetLight->IsVisible())
		{
			TargetLight->SetVisibility(true, false);
		}
		SetComponentTickEnabled(true);
	}
	else
	{
		CurrentSteadyIntensity = TargetSteadyIntensity;
		ApplyCurrentLightState(CurrentSteadyIntensity);
		SetComponentTickEnabled(false);
	}
}

void UMoltenThermalLightComponent::ResetThermalImmediate()
{
	CurrentSteadyIntensity = 0.0f;
	TargetSteadyIntensity = 0.0f;
	FlashSurgeRemainingIntensity = 0.0f;
	FlashInitialSurgeIntensity = 0.0f;
	FlashElapsedSeconds = 0.0f;

	ApplyCurrentLightState(0.0f);
	SetComponentTickEnabled(false);
}

void UMoltenThermalLightComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsValid(TargetLight))
	{
		SetComponentTickEnabled(false);
		return;
	}

	if (DeltaTime <= 0.0f)
	{
		return;
	}

	if (FlashSurgeRemainingIntensity > 0.0f)
	{
		const float SafeDuration = FMath::Max(0.10f, FlashDecayDurationSeconds);
		FlashElapsedSeconds += DeltaTime;
		if (FlashElapsedSeconds >= SafeDuration)
		{
			FlashSurgeRemainingIntensity = 0.0f;
		}
		else
		{
			const float NormalizedT = FMath::Clamp(FlashElapsedSeconds / SafeDuration, 0.0f, 1.0f);
			const float OneMinusT = 1.0f - NormalizedT;
			// Smooth cubic-out thermal dissipation curve reaching exact 0.0 at NormalizedT == 1.0
			FlashSurgeRemainingIntensity = FlashInitialSurgeIntensity * (OneMinusT * OneMinusT * OneMinusT);
			if (FlashSurgeRemainingIntensity <= ConvergenceEpsilonIntensity)
			{
				FlashSurgeRemainingIntensity = 0.0f;
			}
		}
	}

	CurrentSteadyIntensity = FMath::FInterpTo(
		CurrentSteadyIntensity,
		TargetSteadyIntensity,
		DeltaTime,
		FMath::Max(0.5f, SteadyInterpSpeed));

	const bool bSteadyConverged = FMath::IsNearlyEqual(
		CurrentSteadyIntensity,
		TargetSteadyIntensity,
		ConvergenceEpsilonIntensity);
	if (bSteadyConverged)
	{
		CurrentSteadyIntensity = TargetSteadyIntensity;
	}

	const float CombinedIntensity = FMath::Max(0.0f, CurrentSteadyIntensity + FlashSurgeRemainingIntensity);
	ApplyCurrentLightState(CombinedIntensity);

	if (FlashSurgeRemainingIntensity <= 0.0f && bSteadyConverged)
	{
		SetComponentTickEnabled(false);
	}
}

void UMoltenThermalLightComponent::ApplyCurrentLightState(float CombinedIntensity)
{
	if (!IsValid(TargetLight))
	{
		return;
	}

	const float SafeIntensity = FMath::Max(0.0f, CombinedIntensity);
	const bool bShouldBeVisible = SafeIntensity > KINDA_SMALL_NUMBER;
	TargetLight->SetIntensity(bShouldBeVisible ? SafeIntensity : 0.0f);
	if (TargetLight->IsVisible() != bShouldBeVisible)
	{
		TargetLight->SetVisibility(bShouldBeVisible, false);
	}
}
