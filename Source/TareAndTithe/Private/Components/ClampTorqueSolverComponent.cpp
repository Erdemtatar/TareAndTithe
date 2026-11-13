// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/ClampTorqueSolverComponent.h"
#include "Components/SceneComponent.h"
#include "Data/MonolithContractDataAsset.h"
#include "GameFramework/Actor.h"

namespace
{
	constexpr double GravityAccelerationMps2 = 9.81;
	constexpr double CentimetersToMeters = 0.01;
	constexpr double MaxClampedStrainRatio = 1.5;
	constexpr double SleepAngularSpeedThresholdRadPerSec = 1.0e-3;
	constexpr double SleepResidualTorqueThresholdNm = 0.05;
}

UClampTorqueSolverComponent::UClampTorqueSolverComponent()
{
	// Tier 2: Activated strictly on slice/load transitions; sleeps under Section 5.2 3-condition guard.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UClampTorqueSolverComponent::SetNeutralPivotTransform(const FTransform& InNeutralPivotWorld)
{
	NeutralPivotWorld = InNeutralPivotWorld;
	bHasExplicitNeutralPivot = true;
}

void UClampTorqueSolverComponent::SetTiltingMountComponent(
	USceneComponent* InMountComponent,
	bool bCaptureNeutralPivotFromMount)
{
	TiltingMountComponent = InMountComponent;
	if (bCaptureNeutralPivotFromMount && IsValid(TiltingMountComponent))
	{
		NeutralPivotWorld = TiltingMountComponent->GetComponentTransform();
		bHasExplicitNeutralPivot = true;
	}
}

void UClampTorqueSolverComponent::InitializeFromContract(
	const UMonolithContractDataAsset* Contract,
	float InitialRemainderMassKg,
	const FVector& InitialRemainderLocalCoMCm)
{
	if (IsValid(Contract))
	{
		ClampMaxSafeTorqueNm = FMath::Max(1.0f, Contract->ClampMaxSafeTorqueNm);
		ClampFatigueGraceSeconds = FMath::Clamp(Contract->ClampFatigueGraceSeconds, 0.5f, 15.0f);
		ClampFatigueCooldownSeconds = FMath::Clamp(Contract->ClampFatigueCooldownSeconds, 0.5f, 15.0f);
		ClampSpringStiffness = FMath::Max(5.0f, Contract->ClampSpringStiffness);
		ClampSpringDamping = FMath::Max(1.0f, Contract->ClampSpringDamping);
	}

	if (!bHasExplicitNeutralPivot)
	{
		if (IsValid(TiltingMountComponent))
		{
			NeutralPivotWorld = TiltingMountComponent->GetComponentTransform();
			bHasExplicitNeutralPivot = true;
		}
		else if (const AActor* OwnerActor = GetOwner())
		{
			NeutralPivotWorld = OwnerActor->GetActorTransform();
			bHasExplicitNeutralPivot = true;
		}
	}

	ResetClampState();
	UpdateRemainderLoad(InitialRemainderMassKg, InitialRemainderLocalCoMCm);
}

void UClampTorqueSolverComponent::UpdateRemainderLoad(
	float NewRemainderMassKg,
	const FVector& NewRemainderLocalCoMCm)
{
	if (bClampSnapped)
	{
		return;
	}

	RemainderMassKg = FMath::Max(0.0f, NewRemainderMassKg);
	RemainderLocalCoMCm = NewRemainderLocalCoMCm;

	RecomputeNeutralFrameTorqueAndInertia();
	EmitStrainTelemetry(GetStrainNormalized());

	if (StrainRatio >= CatastrophicShearStrainRatio)
	{
		TriggerStructuralSnap();
		return;
	}

	SetComponentTickEnabled(true);
}

void UClampTorqueSolverComponent::ResetClampState()
{
	bClampSnapped = false;
	RemainderMassKg = 0.0f;
	RemainderLocalCoMCm = FVector::ZeroVector;
	TorqueNeutralNm = FVector2d::ZeroVector;
	ThetaRad = FVector2d::ZeroVector;
	OmegaRadPerSec = FVector2d::ZeroVector;
	EffectiveInertiaKgM2 = FMath::Max(0.25f, MinClampMountInertiaKgM2);
	StrainRatio = 0.0f;
	FatigueNormalized = 0.0f;

	ApplyTiltToMountComponent();
	EmitStrainTelemetry(0.0f, true);
	EmitFatigueTelemetry(0.0f, true);
	SetComponentTickEnabled(false);
}

FVector2D UClampTorqueSolverComponent::GetTiltAnglesDegrees() const
{
	return FVector2D(
		FMath::RadiansToDegrees(ThetaRad.X),
		FMath::RadiansToDegrees(ThetaRad.Y));
}

void UClampTorqueSolverComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bClampSnapped)
	{
		SetComponentTickEnabled(false);
		return;
	}

	const float ClampedDelta = FMath::Clamp(DeltaTime, 0.0f, MaxFrameDeltaSeconds);
	if (ClampedDelta <= 0.0f)
	{
		return;
	}

	const float SafeMaxSubstep = FMath::Clamp(MaxSubstepDeltaSeconds, 0.001f, 0.016667f);
	const int32 NumSubsteps = FMath::Max(1, FMath::CeilToInt(ClampedDelta / SafeMaxSubstep));
	const double SubDt = static_cast<double>(ClampedDelta) / static_cast<double>(NumSubsteps);

	const double Stiffness = static_cast<double>(ClampSpringStiffness);
	const double Damping = static_cast<double>(ClampSpringDamping);
	const double InvInertia = 1.0 / FMath::Max(0.25, static_cast<double>(EffectiveInertiaKgM2));
	const double MaxRollRad = FMath::DegreesToRadians(static_cast<double>(ThetaMaxDegrees));
	const double MaxPitchRad = FMath::DegreesToRadians(static_cast<double>(FMath::Min(ThetaMaxDegrees, MaxPitchTiltDegrees)));
	const double OnsetThreshold = static_cast<double>(FatigueOnsetStrainRatio);
	const double ShearThreshold = static_cast<double>(CatastrophicShearStrainRatio);
	const double GraceSeconds = FMath::Max(0.1, static_cast<double>(ClampFatigueGraceSeconds));
	const double CooldownSeconds = FMath::Max(0.1, static_cast<double>(ClampFatigueCooldownSeconds));

	for (int32 StepIdx = 0; StepIdx < NumSubsteps; ++StepIdx)
	{
		// 2-axis semi-implicit Euler spring-damper integration (X = Roll around +X, Y = Pitch around +Y).
		const FVector2d AccelRadPerSec2(
			(TorqueNeutralNm.X - (Stiffness * ThetaRad.X) - (Damping * OmegaRadPerSec.X)) * InvInertia,
			(TorqueNeutralNm.Y - (Stiffness * ThetaRad.Y) - (Damping * OmegaRadPerSec.Y)) * InvInertia);

		OmegaRadPerSec += AccelRadPerSec2 * SubDt;
		ThetaRad += OmegaRadPerSec * SubDt;

		if (ThetaRad.X > MaxRollRad)
		{
			ThetaRad.X = MaxRollRad;
			OmegaRadPerSec.X = FMath::Min(0.0, OmegaRadPerSec.X);
		}
		else if (ThetaRad.X < -MaxRollRad)
		{
			ThetaRad.X = -MaxRollRad;
			OmegaRadPerSec.X = FMath::Max(0.0, OmegaRadPerSec.X);
		}

		if (ThetaRad.Y > MaxPitchRad)
		{
			ThetaRad.Y = MaxPitchRad;
			OmegaRadPerSec.Y = FMath::Min(0.0, OmegaRadPerSec.Y);
		}
		else if (ThetaRad.Y < -MaxPitchRad)
		{
			ThetaRad.Y = -MaxPitchRad;
			OmegaRadPerSec.Y = FMath::Max(0.0, OmegaRadPerSec.Y);
		}

		// Bidirectional structural fatigue & catastrophic bolt shear.
		const double CurrentStrain = static_cast<double>(StrainRatio);
		if (CurrentStrain >= ShearThreshold)
		{
			FatigueNormalized = 1.0f;
		}
		else if (CurrentStrain >= OnsetThreshold)
		{
			const double FatigueRiseRate = (CurrentStrain - OnsetThreshold) / GraceSeconds;
			FatigueNormalized = static_cast<float>(FMath::Clamp(
				static_cast<double>(FatigueNormalized) + (FatigueRiseRate * SubDt),
				0.0,
				1.0));
		}
		else if (FatigueNormalized > 0.0f)
		{
			const double FatigueCoolRate = (OnsetThreshold - CurrentStrain) / FMath::Max(0.05, OnsetThreshold * CooldownSeconds);
			FatigueNormalized = static_cast<float>(FMath::Clamp(
				static_cast<double>(FatigueNormalized) - (FatigueCoolRate * SubDt),
				0.0,
				1.0));
		}

		if (FatigueNormalized >= 1.0f)
		{
			ApplyTiltToMountComponent();
			TriggerStructuralSnap();
			return;
		}
	}

	ApplyTiltToMountComponent();
	EmitStrainTelemetry(GetStrainNormalized());
	EmitFatigueTelemetry(FatigueNormalized);

	// Section 5.2 Mandatory 3-Condition Tier-2 Sleep Guard:
	// (||theta_dot|| < 1e-3f) && (||tau_0 - k_spring * theta|| < 0.05f) && ((S_clamp < 0.80f && F <= KINDA_SMALL_NUMBER) || bClampSnapped)
	const double AngularSpeedMag = OmegaRadPerSec.Length();
	const FVector2d ResidualTorque(
		TorqueNeutralNm.X - (Stiffness * ThetaRad.X),
		TorqueNeutralNm.Y - (Stiffness * ThetaRad.Y));
	const double ResidualTorqueMag = ResidualTorque.Length();

	const bool bVelocityConverged = (AngularSpeedMag < SleepAngularSpeedThresholdRadPerSec);
	const bool bSpringEquilibrated = (ResidualTorqueMag < SleepResidualTorqueThresholdNm);
	const bool bFatigueSettled = ((StrainRatio < FatigueOnsetStrainRatio) && (FatigueNormalized <= KINDA_SMALL_NUMBER)) || bClampSnapped;

	if (bVelocityConverged && bSpringEquilibrated && bFatigueSettled)
	{
		OmegaRadPerSec = FVector2d::ZeroVector;
		if (FatigueNormalized <= KINDA_SMALL_NUMBER)
		{
			FatigueNormalized = 0.0f;
			EmitFatigueTelemetry(0.0f);
		}
		SetComponentTickEnabled(false);
	}
}

void UClampTorqueSolverComponent::RecomputeNeutralFrameTorqueAndInertia()
{
	const FVector NeutralWorldCoMCm = NeutralPivotWorld.TransformPosition(RemainderLocalCoMCm);
	const FVector PivotWorldCm = NeutralPivotWorld.GetLocation();
	const FVector LeverArmMeters = (NeutralWorldCoMCm - PivotWorldCm) * CentimetersToMeters;

	// UE5 FRotationTranslationMatrix(FRotator(Pitch, 0, Roll)): z_world = x*sin(Pitch) - y*sin(Roll)*cos(Pitch) + z*cos(Roll)*cos(Pitch).
	// Generalized gravity torque -d(m*g*z_world)/d(Roll, Pitch) at neutral (0, 0):
	//   tau_Roll  (X) = +LeverArmMeters.Y * m * g  (+Y foot overhang pulls +Y jaw downward in Roll, zero Pitch)
	//   tau_Pitch (Y) = -LeverArmMeters.X * m * g  (+X depth overhang pulls +X jaw downward in Pitch)
	const double MassKg = static_cast<double>(RemainderMassKg);
	TorqueNeutralNm.X = +LeverArmMeters.Y * MassKg * GravityAccelerationMps2;
	TorqueNeutralNm.Y = -LeverArmMeters.X * MassKg * GravityAccelerationMps2;

	const double HorizontalTorqueMagNm = TorqueNeutralNm.Length();
	const double SafeMaxTorqueNm = FMath::Max(1.0, static_cast<double>(ClampMaxSafeTorqueNm));
	StrainRatio = static_cast<float>(FMath::Clamp(HorizontalTorqueMagNm / SafeMaxTorqueNm, 0.0, MaxClampedStrainRatio));

	const float LeverArm3DMetersSq = static_cast<float>(LeverArmMeters.SizeSquared());
	EffectiveInertiaKgM2 = FMath::Max(MinClampMountInertiaKgM2, RemainderMassKg * LeverArm3DMetersSq);
}

void UClampTorqueSolverComponent::ApplyTiltToMountComponent()
{
	if (IsValid(TiltingMountComponent))
	{
		const FRotator MountTiltRotator(
			FMath::RadiansToDegrees(ThetaRad.Y),
			0.0,
			FMath::RadiansToDegrees(ThetaRad.X));
		TiltingMountComponent->SetRelativeRotation(MountTiltRotator);
	}
}

void UClampTorqueSolverComponent::TriggerStructuralSnap()
{
	if (bClampSnapped)
	{
		return;
	}

	bClampSnapped = true;
	FatigueNormalized = 1.0f;
	OmegaRadPerSec = FVector2d::ZeroVector;
	TorqueNeutralNm = FVector2d::ZeroVector;

	EmitStrainTelemetry(GetStrainNormalized(), true);
	EmitFatigueTelemetry(1.0f, true);
	OnTelemetryImpulse.Broadcast(ETelemetryImpulseId::FaultTriggered, 1.0f, NeutralPivotWorld.GetLocation());
	OnClampStructuralSnap.Broadcast();

	SetComponentTickEnabled(false);
}

void UClampTorqueSolverComponent::EmitStrainTelemetry(float NormalizedValue, bool bForceEmit)
{
	const float ClampedVal = FMath::Clamp(NormalizedValue, 0.0f, 1.0f);
	const bool bIsEndpoint = (ClampedVal <= 0.0f) || (ClampedVal >= 1.0f);
	const bool bEndpointChanged = bIsEndpoint && !FMath::IsNearlyEqual(ClampedVal, LastEmittedStrain, KINDA_SMALL_NUMBER);
	const bool bExceededDeadband = FMath::Abs(ClampedVal - LastEmittedStrain) >= TelemetryDeadband;

	if (bForceEmit || LastEmittedStrain < 0.0f || bEndpointChanged || bExceededDeadband)
	{
		LastEmittedStrain = ClampedVal;
		OnTelemetryStream.Broadcast(ETelemetryStreamId::ClampStrain, ClampedVal);
	}
}

void UClampTorqueSolverComponent::EmitFatigueTelemetry(float NormalizedValue, bool bForceEmit)
{
	const float ClampedVal = FMath::Clamp(NormalizedValue, 0.0f, 1.0f);
	const bool bIsEndpoint = (ClampedVal <= 0.0f) || (ClampedVal >= 1.0f);
	const bool bEndpointChanged = bIsEndpoint && !FMath::IsNearlyEqual(ClampedVal, LastEmittedFatigue, KINDA_SMALL_NUMBER);
	const bool bExceededDeadband = FMath::Abs(ClampedVal - LastEmittedFatigue) >= TelemetryDeadband;

	if (bForceEmit || LastEmittedFatigue < 0.0f || bEndpointChanged || bExceededDeadband)
	{
		LastEmittedFatigue = ClampedVal;
		OnTelemetryStream.Broadcast(ETelemetryStreamId::ClampFatigue, ClampedVal);
	}
}
