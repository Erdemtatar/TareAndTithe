// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/InspectorKinestheticsComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioTelemetryBridgeComponent.h"
#include "Components/SceneComponent.h"
#include "Data/TareTithePresentationDataAsset.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Subsystems/InspectionShiftSubsystem.h"

namespace
{
	constexpr float MaxKinestheticSubstepSeconds = 1.0f / 120.0f;
	constexpr double RestPositionEpsilonCm = 0.002;
	constexpr double RestVelocityEpsilonCmPerSec = 0.02;
	constexpr double RestAngleEpsilonDeg = 0.002;
	constexpr double RestAngVelEpsilonDegPerSec = 0.02;
	constexpr float RestFovEpsilonDeg = 0.004f;
}

UInspectorKinestheticsComponent::UInspectorKinestheticsComponent()
{
	// Tier 2: Strictly disabled (0.0 ms) at rest; active ONLY during capacitor charge or damped spring recoil settlement.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UInspectorKinestheticsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(BoundTelemetryBridge))
	{
		BoundTelemetryBridge->OnFilteredTelemetryStream.RemoveAll(this);
		BoundTelemetryBridge->OnForwardedTelemetryImpulse.RemoveAll(this);
		BoundTelemetryBridge = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UInspectorKinestheticsComponent::BindShakeAndCameraComponents(
	USceneComponent* InShakeOffsetComponent,
	UCameraComponent* InCameraComponent,
	float InBaseFieldOfView)
{
	ShakeOffsetComponent = InShakeOffsetComponent;
	CameraComponent = InCameraComponent;
	BaseFieldOfView = FMath::Clamp(InBaseFieldOfView, 60.0f, 115.0f);
	if (IsValid(CameraComponent))
	{
		bBaseOverrideSceneFringe = CameraComponent->PostProcessSettings.bOverride_SceneFringeIntensity != 0;
		BaseSceneFringeIntensity = bBaseOverrideSceneFringe
			? CameraComponent->PostProcessSettings.SceneFringeIntensity
			: 0.0f;
	}
	else
	{
		bBaseOverrideSceneFringe = false;
		BaseSceneFringeIntensity = 0.0f;
	}
	ResetKinestheticsImmediate();
}

void UInspectorKinestheticsComponent::BindTelemetryBridge(UAudioTelemetryBridgeComponent* InTelemetryBridge)
{
	if (BoundTelemetryBridge == InTelemetryBridge)
	{
		return;
	}

	if (IsValid(BoundTelemetryBridge))
	{
		BoundTelemetryBridge->OnFilteredTelemetryStream.RemoveAll(this);
		BoundTelemetryBridge->OnForwardedTelemetryImpulse.RemoveAll(this);
	}

	BoundTelemetryBridge = InTelemetryBridge;

	if (IsValid(BoundTelemetryBridge))
	{
		BoundTelemetryBridge->OnFilteredTelemetryStream.AddUObject(this, &UInspectorKinestheticsComponent::HandleTelemetryStream);
		BoundTelemetryBridge->OnForwardedTelemetryImpulse.AddUObject(this, &UInspectorKinestheticsComponent::HandleTelemetryImpulse);
	}
}

void UInspectorKinestheticsComponent::SetPresentationData(const UTareTithePresentationDataAsset* InPresentationData)
{
	PresentationData = InPresentationData;
}

void UInspectorKinestheticsComponent::ResetKinestheticsImmediate()
{
	RecoilOffsetCm = FVector::ZeroVector;
	RecoilVelocityCmPerSec = FVector::ZeroVector;
	RecoilRotationDeg = FVector::ZeroVector;
	RecoilAngularVelocityDegPerSec = FVector::ZeroVector;
	CurrentFovOffsetDegrees = 0.0f;
	CurrentChargeNormalized = 0.0f;
	ChargeHumPhaseSeconds = 0.0;
	StampFovPunchElapsedSeconds = 0.0f;
	StampFovPunchScale = 1.0f;
	CurrentStampFovPunchDegrees = 0.0f;
	bStampFovPunchActive = false;
	FaultChromaticElapsedSeconds = 0.0f;
	FaultChromaticPeakActive = 0.0f;
	bFaultChromaticPulseActive = false;

	if (IsValid(ShakeOffsetComponent))
	{
		ShakeOffsetComponent->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	}
	if (IsValid(CameraComponent))
	{
		CameraComponent->SetFieldOfView(BaseFieldOfView);
		CameraComponent->PostProcessSettings.SceneFringeIntensity = BaseSceneFringeIntensity;
		CameraComponent->PostProcessSettings.bOverride_SceneFringeIntensity = bBaseOverrideSceneFringe ? 1 : 0;
	}

	SetComponentTickEnabled(false);
}

void UInspectorKinestheticsComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const float ClampedDelta = FMath::Clamp(DeltaTime, 0.0f, 0.1f);
	if (ClampedDelta <= 0.0f)
	{
		return;
	}

	// Sub-stepped semi-implicit damped spring integration for frame-rate-independent recoil settlement.
	float RemainingTime = ClampedDelta;
	const double Stiffness = static_cast<double>(FMath::Max(10.0f, RecoilSpringStiffness));
	const double Damping = static_cast<double>(FMath::Max(1.0f, RecoilSpringDamping));

	while (RemainingTime > 0.0f)
	{
		const float StepDt = FMath::Min(RemainingTime, MaxKinestheticSubstepSeconds);
		const double Dt = static_cast<double>(StepDt);

		const FVector LinearAccel = (-Stiffness * RecoilOffsetCm) - (Damping * RecoilVelocityCmPerSec);
		RecoilVelocityCmPerSec += LinearAccel * Dt;
		RecoilOffsetCm += RecoilVelocityCmPerSec * Dt;

		const FVector AngularAccel = (-Stiffness * RecoilRotationDeg) - (Damping * RecoilAngularVelocityDegPerSec);
		RecoilAngularVelocityDegPerSec += AngularAccel * Dt;
		RecoilRotationDeg += RecoilAngularVelocityDegPerSec * Dt;

		RemainingTime -= StepDt;
	}

	const double MaxPosCm = static_cast<double>(FMath::Max(0.1f, MaxRecoilTranslationOffsetCm));
	const double MaxRotDeg = static_cast<double>(FMath::Max(0.1f, MaxRecoilRotationOffsetDegrees));
	RecoilOffsetCm = RecoilOffsetCm.GetClampedToMaxSize(MaxPosCm);
	RecoilRotationDeg.X = FMath::Clamp(RecoilRotationDeg.X, -MaxRotDeg, MaxRotDeg);
	RecoilRotationDeg.Y = FMath::Clamp(RecoilRotationDeg.Y, -MaxRotDeg, MaxRotDeg);
	RecoilRotationDeg.Z = FMath::Clamp(RecoilRotationDeg.Z, -MaxRotDeg, MaxRotDeg);

	// High-frequency industrial capacitor hum micro-vibration strictly bounded <= 0.28 cm and <= 0.18 deg.
	FVector HumTranslationCm = FVector::ZeroVector;
	FRotator HumRotationDeg = FRotator::ZeroRotator;

	if (CurrentChargeNormalized > KINDA_SMALL_NUMBER)
	{
		ChargeHumPhaseSeconds += static_cast<double>(ClampedDelta);
		const float SafeMaxPos = FMath::Clamp(MaxChargeHumTranslationCm, 0.0f, 0.28f);
		const float SafeMaxRot = FMath::Clamp(MaxChargeHumRotationDegrees, 0.0f, 0.18f);
		const float Envelope = CurrentChargeNormalized * CurrentChargeNormalized;

		const double Omega1 = 2.0 * UE_DOUBLE_PI * static_cast<double>(ChargeHumPrimaryFrequencyHz) * ChargeHumPhaseSeconds;
		const double Omega2 = 2.0 * UE_DOUBLE_PI * static_cast<double>(ChargeHumSecondaryFrequencyHz) * ChargeHumPhaseSeconds;

		const double HumY = (0.62 * FMath::Sin(Omega1) + 0.38 * FMath::Sin(Omega2 * 1.13)) * static_cast<double>(SafeMaxPos * Envelope);
		const double HumZ = (0.58 * FMath::Cos(Omega2) + 0.42 * FMath::Sin(Omega1 * 0.89)) * static_cast<double>(SafeMaxPos * Envelope);
		HumTranslationCm = FVector(0.0, HumY, HumZ);
		HumTranslationCm = HumTranslationCm.GetClampedToMaxSize(0.28);

		const float HumPitch = static_cast<float>(0.65 * FMath::Sin(Omega2 + 0.7) * SafeMaxRot * Envelope);
		const float HumRoll = static_cast<float>(0.85 * FMath::Cos(Omega1 + 0.3) * SafeMaxRot * Envelope);
		HumRotationDeg = FRotator(
			FMath::Clamp(HumPitch, -0.18f, 0.18f),
			0.0f,
			FMath::Clamp(HumRoll, -0.18f, 0.18f));
	}
	else
	{
		ChargeHumPhaseSeconds = 0.0;
	}

	const FPhase12EPresentationPolishConfig Phase12ECfg = IsValid(PresentationData)
		? PresentationData->Phase12EPolishConfig.GetSanitized()
		: FPhase12EPresentationPolishConfig();

	// Tension FOV pull (85.0 -> 83.6 deg) during charge and crisp release back to baseline.
	const float TargetFovOffset = (CurrentChargeNormalized > KINDA_SMALL_NUMBER)
		? (-MaxChargeFovPullDegrees * CurrentChargeNormalized)
		: 0.0f;
	const float ActiveFovSpeed = (CurrentChargeNormalized > KINDA_SMALL_NUMBER)
		? ChargeFovPullInterpSpeed
		: FovReleaseInterpSpeed;
	CurrentFovOffsetDegrees = FMath::FInterpTo(CurrentFovOffsetDegrees, TargetFovOffset, ClampedDelta, ActiveFovSpeed);

	// Phase 12E: 0.18s 1.5 deg camera FOV punch on valid [E] pass stamp (85.0 -> 83.5 -> 85.0 deg).
	if (bStampFovPunchActive)
	{
		StampFovPunchElapsedSeconds += ClampedDelta;
		const float PunchDuration = Phase12ECfg.StampFovPunchDurationSeconds;
		if (!Phase12ECfg.bEnableStampFovPunch || StampFovPunchElapsedSeconds >= PunchDuration)
		{
			bStampFovPunchActive = false;
			StampFovPunchElapsedSeconds = 0.0f;
			CurrentStampFovPunchDegrees = 0.0f;
		}
		else
		{
			const float NormT = FMath::Clamp(StampFovPunchElapsedSeconds / PunchDuration, 0.0f, 1.0f);
			const float RawEnv = (NormT < 0.30f) ? (NormT / 0.30f) : ((1.0f - NormT) / 0.70f);
			const float SmoothEnv = RawEnv * RawEnv * (3.0f - 2.0f * RawEnv);
			CurrentStampFovPunchDegrees = -Phase12ECfg.StampFovPunchDegrees * StampFovPunchScale * SmoothEnv;
		}
	}
	else
	{
		CurrentStampFovPunchDegrees = 0.0f;
	}

	// Phase 12E: 0.25s Chromatic Aberration camera glitch pulse on FaultTriggered with exact baseline restore.
	float ActiveSceneFringe = BaseSceneFringeIntensity;
	bool bOverrideSceneFringeNow = bBaseOverrideSceneFringe;
	if (bFaultChromaticPulseActive)
	{
		FaultChromaticElapsedSeconds += ClampedDelta;
		const float FringeDuration = Phase12ECfg.FaultChromaticDurationSeconds;
		if (!Phase12ECfg.bEnableFaultChromaticPulse || FaultChromaticElapsedSeconds >= FringeDuration)
		{
			bFaultChromaticPulseActive = false;
			FaultChromaticElapsedSeconds = 0.0f;
			FaultChromaticPeakActive = 0.0f;
		}
		else
		{
			const float NormT = FMath::Clamp(FaultChromaticElapsedSeconds / FringeDuration, 0.0f, 1.0f);
			const float FringeEnv = (NormT < 0.16f)
				? (NormT / 0.16f)
				: (0.5f * (1.0f + FMath::Cos(UE_PI * ((NormT - 0.16f) / 0.84f))));
			ActiveSceneFringe = BaseSceneFringeIntensity + (FaultChromaticPeakActive * FringeEnv);
			bOverrideSceneFringeNow = true;
		}
	}

	if (HasConvergedToRest())
	{
		ResetKinestheticsImmediate();
		return;
	}

	if (IsValid(ShakeOffsetComponent))
	{
		const FVector FinalOffsetCm = RecoilOffsetCm + HumTranslationCm;
		const FRotator FinalRotDeg(
			static_cast<float>(RecoilRotationDeg.Y) + HumRotationDeg.Pitch,
			static_cast<float>(RecoilRotationDeg.Z) + HumRotationDeg.Yaw,
			static_cast<float>(RecoilRotationDeg.X) + HumRotationDeg.Roll);
		ShakeOffsetComponent->SetRelativeLocationAndRotation(FinalOffsetCm, FinalRotDeg);
	}

	if (IsValid(CameraComponent))
	{
		CameraComponent->SetFieldOfView(BaseFieldOfView + CurrentFovOffsetDegrees + CurrentStampFovPunchDegrees);
		CameraComponent->PostProcessSettings.SceneFringeIntensity = ActiveSceneFringe;
		CameraComponent->PostProcessSettings.bOverride_SceneFringeIntensity = bOverrideSceneFringeNow ? 1 : 0;
	}
}

void UInspectorKinestheticsComponent::HandleTelemetryStream(ETelemetryStreamId StreamId, float NormalizedValue)
{
	if (StreamId != ETelemetryStreamId::LaserCharge)
	{
		return;
	}

	CurrentChargeNormalized = FMath::Clamp(NormalizedValue, 0.0f, 1.0f);
	if (CurrentChargeNormalized > KINDA_SMALL_NUMBER || !HasConvergedToRest())
	{
		EnsureActiveTickEnabled();
	}
}

void UInspectorKinestheticsComponent::HandleTelemetryImpulse(
	ETelemetryImpulseId ImpulseId,
	float NormalizedMagnitude,
	const FVector& WorldLocation)
{
	const float ClampedMag = FMath::Clamp(NormalizedMagnitude, 0.0f, 1.0f);
	const FPhase12EPresentationPolishConfig Phase12ECfg = IsValid(PresentationData)
		? PresentationData->Phase12EPolishConfig.GetSanitized()
		: FPhase12EPresentationPolishConfig();

	switch (ImpulseId)
	{
	case ETelemetryImpulseId::SliceCommitted:
	{
		CurrentChargeNormalized = 0.0f;
		RecoilVelocityCmPerSec += FVector(
			-SliceRecoilBackImpulseCmPerSec * ClampedMag,
			0.0,
			SliceRecoilUpImpulseCmPerSec * ClampedMag);
		RecoilAngularVelocityDegPerSec.Y += SliceRecoilPitchImpulseDegPerSec * ClampedMag;
		EnsureActiveTickEnabled();
		break;
	}
	case ETelemetryImpulseId::ChunkScaleImpact:
	{
		RecoilVelocityCmPerSec.Z -= ScaleImpactDownwardImpulseCmPerSec * ClampedMag;
		RecoilAngularVelocityDegPerSec.Y -= ScaleImpactPitchImpulseDegPerSec * ClampedMag;
		EnsureActiveTickEnabled();
		break;
	}
	case ETelemetryImpulseId::VerdictStamped:
	{
		if (Phase12ECfg.bEnableStampFovPunch && Phase12ECfg.StampFovPunchDegrees > KINDA_SMALL_NUMBER)
		{
			bStampFovPunchActive = true;
			StampFovPunchElapsedSeconds = 0.0f;
			StampFovPunchScale = FMath::Max(0.25f, ClampedMag);
			EnsureActiveTickEnabled();
		}
		break;
	}
	case ETelemetryImpulseId::FaultTriggered:
	{
		CurrentChargeNormalized = 0.0f;
		bStampFovPunchActive = false;
		StampFovPunchElapsedSeconds = 0.0f;
		CurrentStampFovPunchDegrees = 0.0f;
		float EffectiveFaultScale = ClampedMag;
		if (const UWorld* World = GetWorld())
		{
			if (const UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
			{
				if (ShiftSubsystem->GetLastFaultReason() == EContractFaultReason::ManualOperatorReset)
				{
					EffectiveFaultScale *= ManualResetFaultShakeScale;
				}
			}
		}

		FaultRollAlternatingSign = -FaultRollAlternatingSign;
		RecoilVelocityCmPerSec.Z -= FaultThudDownwardImpulseCmPerSec * EffectiveFaultScale;
		RecoilAngularVelocityDegPerSec.Y -= FaultThudPitchImpulseDegPerSec * EffectiveFaultScale;
		RecoilAngularVelocityDegPerSec.X += FaultThudRollImpulseDegPerSec * EffectiveFaultScale * FaultRollAlternatingSign;
		if (Phase12ECfg.bEnableFaultChromaticPulse && Phase12ECfg.FaultChromaticPeakIntensity > KINDA_SMALL_NUMBER)
		{
			bFaultChromaticPulseActive = true;
			FaultChromaticElapsedSeconds = 0.0f;
			FaultChromaticPeakActive = Phase12ECfg.FaultChromaticPeakIntensity * EffectiveFaultScale;
		}
		EnsureActiveTickEnabled();
		break;
	}
	default:
		break;
	}

	SpawnImpulseVfxIfAssigned(ImpulseId, ClampedMag, WorldLocation);
}

void UInspectorKinestheticsComponent::SpawnImpulseVfxIfAssigned(
	ETelemetryImpulseId ImpulseId,
	float ClampedMagnitude,
	const FVector& WorldLocation)
{
	if (!IsValid(PresentationData))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	switch (ImpulseId)
	{
	case ETelemetryImpulseId::SliceCommitted:
	{
		if (IsValid(PresentationData->LaserSliceSparksVFX))
		{
			UNiagaraComponent* SpawnedSparks = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World,
				PresentationData->LaserSliceSparksVFX,
				WorldLocation,
				FRotator::ZeroRotator,
				FVector::OneVector,
				true,
				true,
				ENCPoolMethod::AutoRelease,
				true);
			if (IsValid(SpawnedSparks))
			{
				SpawnedSparks->SetVariableFloat(FName(TEXT("User.CutAreaNormalized")), ClampedMagnitude);
			}
		}
		break;
	}
	case ETelemetryImpulseId::SliverVaporized:
	{
		if (IsValid(PresentationData->SliverVaporizedVFX))
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World,
				PresentationData->SliverVaporizedVFX,
				WorldLocation,
				FRotator::ZeroRotator,
				FVector::OneVector,
				true,
				true,
				ENCPoolMethod::AutoRelease,
				true);
		}
		break;
	}
	case ETelemetryImpulseId::FaultTriggered:
	{
		const UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>();
		const bool bIsVeinDetonation = IsValid(ShiftSubsystem) &&
			(ShiftSubsystem->GetLastFaultReason() == EContractFaultReason::VolatileDetonation);
		if (bIsVeinDetonation && IsValid(PresentationData->VeinDetonationVFX))
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World,
				PresentationData->VeinDetonationVFX,
				WorldLocation,
				FRotator::ZeroRotator,
				FVector::OneVector,
				true,
				true,
				ENCPoolMethod::AutoRelease,
				true);
		}
		break;
	}
	case ETelemetryImpulseId::ChunkScaleImpact:
	{
		if (ClampedMagnitude >= HeavyScaleImpactVfxMinMagnitude && IsValid(PresentationData->ScaleHeavyImpactVFX))
		{
			UNiagaraComponent* SpawnedImpact = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World,
				PresentationData->ScaleHeavyImpactVFX,
				WorldLocation,
				FRotator::ZeroRotator,
				FVector::OneVector,
				true,
				true,
				ENCPoolMethod::AutoRelease,
				true);
			if (IsValid(SpawnedImpact))
			{
				SpawnedImpact->SetVariableFloat(FName(TEXT("User.NormalizedMagnitude")), ClampedMagnitude);
			}
		}
		break;
	}
	default:
		break;
	}
}

void UInspectorKinestheticsComponent::EnsureActiveTickEnabled()
{
	if (!IsComponentTickEnabled())
	{
		SetComponentTickEnabled(true);
	}
}

bool UInspectorKinestheticsComponent::HasConvergedToRest() const
{
	return !bStampFovPunchActive &&
		!bFaultChromaticPulseActive &&
		(CurrentChargeNormalized <= KINDA_SMALL_NUMBER) &&
		(RecoilOffsetCm.SizeSquared() <= (RestPositionEpsilonCm * RestPositionEpsilonCm)) &&
		(RecoilVelocityCmPerSec.SizeSquared() <= (RestVelocityEpsilonCmPerSec * RestVelocityEpsilonCmPerSec)) &&
		(RecoilRotationDeg.SizeSquared() <= (RestAngleEpsilonDeg * RestAngleEpsilonDeg)) &&
		(RecoilAngularVelocityDegPerSec.SizeSquared() <= (RestAngVelEpsilonDegPerSec * RestAngVelEpsilonDegPerSec)) &&
		(FMath::Abs(CurrentFovOffsetDegrees) <= RestFovEpsilonDeg) &&
		(FMath::Abs(CurrentStampFovPunchDegrees) <= RestFovEpsilonDeg);
}
