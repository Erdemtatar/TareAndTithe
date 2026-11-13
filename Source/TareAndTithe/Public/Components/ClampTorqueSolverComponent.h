// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/TareTitheTypes.h"
#include "ClampTorqueSolverComponent.generated.h"

class USceneComponent;
class UMonolithContractDataAsset;

DECLARE_MULTICAST_DELEGATE(FOnClampStructuralSnapSignature);

UCLASS(ClassGroup = (TareAndTithe), meta = (BlueprintSpawnableComponent))
class TAREANDTITHE_API UClampTorqueSolverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UClampTorqueSolverComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void SetNeutralPivotTransform(const FTransform& InNeutralPivotWorld);
	const FTransform& GetNeutralPivotTransform() const { return NeutralPivotWorld; }

	void SetTiltingMountComponent(USceneComponent* InMountComponent, bool bCaptureNeutralPivotFromMount = true);
	USceneComponent* GetTiltingMountComponent() const { return TiltingMountComponent; }

	void InitializeFromContract(
		const UMonolithContractDataAsset* Contract,
		float InitialRemainderMassKg,
		const FVector& InitialRemainderLocalCoMCm);

	void ApplySliceLoadAndShear(
		float PrevRemainderMassKg,
		const FVector& PrevMountLocalCoMCm,
		float NewRemainderMassKg,
		const FVector& NewMountLocalCoMCm,
		float PreCutHorizontalHalfSpanCm,
		const FVector& PlaneNormalWorld);

	void UpdateRemainderLoad(float NewRemainderMassKg, const FVector& NewRemainderLocalCoMCm);
	void SetTorqueCapacityMultiplier(float InMultiplier);
	float GetTorqueCapacityMultiplier() const { return TorqueCapacityMultiplier; }
	void ResetClampState();

	UFUNCTION(BlueprintPure, Category = "Clamp")
	bool IsClampSnapped() const { return bClampSnapped; }

	UFUNCTION(BlueprintPure, Category = "Clamp")
	float GetStrainRatio() const { return StrainRatio; }

	UFUNCTION(BlueprintPure, Category = "Clamp")
	float GetStrainNormalized() const { return FMath::Clamp(StrainRatio, 0.0f, 1.0f); }

	UFUNCTION(BlueprintPure, Category = "Clamp")
	float GetFatigueNormalized() const { return FatigueNormalized; }

	UFUNCTION(BlueprintPure, Category = "Clamp")
	int32 GetSameSideCutStreak() const { return SameSideCutStreak; }

	UFUNCTION(BlueprintPure, Category = "Clamp")
	FVector2D GetTiltAnglesDegrees() const;

	FVector2D GetLastSnapTorqueDirection() const { return LastSnapTorqueDirection; }

	FOnClampStructuralSnapSignature OnClampStructuralSnap;
	FOnTelemetryStreamSignature OnTelemetryStream;
	FOnTelemetryImpulseSignature OnTelemetryImpulse;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clamp|ContractDefaults", meta = (ClampMin = "1.0"))
	float ClampMaxSafeTorqueNm = 28.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clamp|ContractDefaults", meta = (ClampMin = "0.5", ClampMax = "15.0"))
	float ClampFatigueGraceSeconds = 6.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clamp|ContractDefaults", meta = (ClampMin = "0.5", ClampMax = "15.0"))
	float ClampFatigueCooldownSeconds = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clamp|ContractDefaults", meta = (ClampMin = "5.0"))
	float ClampSpringStiffness = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clamp|ContractDefaults", meta = (ClampMin = "1.0"))
	float ClampSpringDamping = 24.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clamp|Solver", meta = (ClampMin = "0.25", ClampMax = "50.0"))
	float MinClampMountInertiaKgM2 = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clamp|Solver", meta = (ClampMin = "5.0", ClampMax = "45.0"))
	float ThetaMaxDegrees = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clamp|Solver", meta = (ClampMin = "1.0", ClampMax = "15.0"))
	float MaxPitchTiltDegrees = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clamp|Solver", meta = (ClampMin = "0.02", ClampMax = "0.25"))
	float MaxFrameDeltaSeconds = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clamp|Solver", meta = (ClampMin = "0.002", ClampMax = "0.016667"))
	float MaxSubstepDeltaSeconds = 1.0f / 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clamp|Thresholds", meta = (ClampMin = "0.5", ClampMax = "0.99"))
	float FatigueOnsetStrainRatio = 0.80f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clamp|Thresholds", meta = (ClampMin = "1.02", ClampMax = "2.5"))
	float CatastrophicShearStrainRatio = 1.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clamp|Telemetry", meta = (ClampMin = "0.001", ClampMax = "0.05"))
	float TelemetryDeadband = 0.01f;

private:
	void RecomputeNeutralFrameTorqueAndInertia();
	void ApplyTiltToMountComponent();
	void TriggerStructuralSnap();
	void EmitStrainTelemetry(float NormalizedValue, bool bForceEmit = false);
	void EmitFatigueTelemetry(float NormalizedValue, bool bForceEmit = false);

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> TiltingMountComponent;

	FTransform NeutralPivotWorld = FTransform::Identity;
	FVector RemainderLocalCoMCm = FVector::ZeroVector;
	FVector2d StaticGravityTorqueNm = FVector2d::ZeroVector;
	FVector2d AccumulatedAsymmetricShearNm = FVector2d::ZeroVector;
	FVector2d TorqueNeutralNm = FVector2d::ZeroVector;
	FVector2d ThetaRad = FVector2d::ZeroVector;
	FVector2d OmegaRadPerSec = FVector2d::ZeroVector;
	FVector2D LastSnapTorqueDirection = FVector2D(1.0f, 0.0f);

	float BaseClampMaxSafeTorqueNm = 28.0f;
	float TorqueCapacityMultiplier = 1.0f;
	float InitialContractMassKg = 35.0f;
	float RemainderMassKg = 0.0f;
	float EffectiveInertiaKgM2 = 2.5f;
	float StrainRatio = 0.0f;
	float FatigueNormalized = 0.0f;
	float LastEmittedStrain = -1.0f;
	float LastEmittedFatigue = -1.0f;
	int32 SameSideCutStreak = 0;

	bool bHasExplicitNeutralPivot = false;
	bool bClampSnapped = false;
};
