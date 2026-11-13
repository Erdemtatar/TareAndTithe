// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "TareTitheTypes.generated.h"

UENUM(BlueprintType)
enum class EMonolithBaseShape : uint8
{
	Box                 UMETA(DisplayName = "Box"),
	OctagonalPrism      UMETA(DisplayName = "Octagonal Prism"),
	AsymmetricLBlock    UMETA(DisplayName = "Asymmetric L-Block"),
	SteppedCore         UMETA(DisplayName = "Stepped Core"),
	CustomStaticMesh    UMETA(DisplayName = "Custom Watertight StaticMesh")
};

UENUM(BlueprintType)
enum class EDiverterRoute : uint8
{
	TitheScale  UMETA(DisplayName = "Tithe Scale (Director's Cut)"),
	TareChute   UMETA(DisplayName = "Tare Scrap Chute")
};

UENUM(BlueprintType)
enum class EChunkRoutingState : uint8
{
	PooledInactive  UMETA(DisplayName = "Pooled Inactive"),
	InFlight        UMETA(DisplayName = "In Flight"),
	OnScalePan      UMETA(DisplayName = "On Scale Pan"),
	ConsumedAsTare  UMETA(DisplayName = "Consumed As Tare"),
	LostAsStrayTare UMETA(DisplayName = "Lost As Stray Tare")
};

UENUM(BlueprintType)
enum class ECommitSliceOutcome : uint8
{
	Success             UMETA(DisplayName = "Success"),
	MissedTarget        UMETA(DisplayName = "Missed Target (Zero Intersection)"),
	DetonatedVein       UMETA(DisplayName = "Volatile Vein Detonated"),
	InvalidMeshState    UMETA(DisplayName = "Invalid Mesh State")
};

UENUM(BlueprintType)
enum class EContractFaultReason : uint8
{
	None                        UMETA(DisplayName = "None"),
	VolatileDetonation          UMETA(DisplayName = "Volatile Isotope Vein Severed"),
	ClampStructuralSnap         UMETA(DisplayName = "Overhead Clamp Bolt Shear"),
	ScaleContaminated           UMETA(DisplayName = "Contaminated Mass On Tithe Scale"),
	QuotaOvershoot              UMETA(DisplayName = "Scale Mass Exceeded Upper Quota"),
	TareAllowanceExceeded       UMETA(DisplayName = "Tare Waste Exceeded Allowance"),
	MathematicallyUnreachable   UMETA(DisplayName = "Remaining Mass Insufficient For Quota"),
	ManualOperatorReset         UMETA(DisplayName = "Manual Operator Reset [R]")
};

UENUM(BlueprintType)
enum class EShiftVerdictType : uint8
{
	ContractPassed      UMETA(DisplayName = "Contract Passed"),
	ContractFaultReset  UMETA(DisplayName = "Contract Fault & Reset"),
	ShiftCompleted      UMETA(DisplayName = "Full Shift Completed")
};

UENUM(BlueprintType)
enum class ETelemetryStreamId : uint8
{
	VeinProximity   UMETA(DisplayName = "Vein Proximity [0..1]"),
	ClampStrain     UMETA(DisplayName = "Clamp Torque Strain [0..1]"),
	ClampFatigue    UMETA(DisplayName = "Clamp Bolt Fatigue [0..1]"),
	LaserCharge     UMETA(DisplayName = "Laser Capacitor Charge [0..1]"),
	ScaleFillRatio  UMETA(DisplayName = "Scale Quota Fill Ratio [0..1]")
};

UENUM(BlueprintType)
enum class ETelemetryImpulseId : uint8
{
	SliceCommitted      UMETA(DisplayName = "Laser Slice Committed"),
	SliverVaporized     UMETA(DisplayName = "Degenerate Sliver Vaporized"),
	ChunkScaleImpact    UMETA(DisplayName = "Chunk Landed On Scale"),
	ChunkTareDisposed   UMETA(DisplayName = "Chunk Routed To Tare"),
	DiverterToggled     UMETA(DisplayName = "Diverter Gate Toggled"),
	VerdictStamped      UMETA(DisplayName = "Quota Verdict Stamped"),
	FaultTriggered      UMETA(DisplayName = "Contract Fault Triggered")
};

USTRUCT(BlueprintType)
struct FVolatileVeinDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vein")
	FVector LocalStartCm = FVector(0.0f, 0.0f, -20.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vein")
	FVector LocalEndCm = FVector(0.0f, 0.0f, 20.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vein", meta = (ClampMin = "0.5", ClampMax = "25.0"))
	float RadiusCm = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vein", meta = (ClampMin = "2.0", ClampMax = "60.0"))
	float WarningRangeCm = 18.0f;

	FVector GetMidpointCm() const
	{
		return 0.5 * (LocalStartCm + LocalEndCm);
	}
};

USTRUCT(BlueprintType)
struct TAREANDTITHE_API FLaserSliceAperture
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slice")
	FVector SheetForwardWorld = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slice")
	FVector SheetRightWorld = FVector::RightVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slice")
	float HalfDepthCm = 36.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slice")
	float HalfWidthCm = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slice")
	bool bEnforceFiniteSheetBounds = false;
};

USTRUCT(BlueprintType)
struct FSlicePreviewTelemetry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Slice")
	bool bIntersectsMonolith = false;

	UPROPERTY(BlueprintReadOnly, Category = "Slice")
	float VeinProximityNormalized = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Slice")
	float MinCapsuleClearanceCm = MAX_FLT;

	UPROPERTY(BlueprintReadOnly, Category = "Slice")
	bool bWillDetonate = false;

	UPROPERTY(BlueprintReadOnly, Category = "Slice")
	float EstimatedSeveredMassKg = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Slice")
	float EstimatedRemainderMassKg = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Slice")
	bool bHasValidMassEstimate = false;
};

USTRUCT(BlueprintType)
struct FSliceResultSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Slice")
	ECommitSliceOutcome Outcome = ECommitSliceOutcome::InvalidMeshState;

	UPROPERTY(BlueprintReadOnly, Category = "Slice")
	float NewRemainderMassKg = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Slice")
	FVector NewRemainderLocalCoMCm = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Slice")
	float SpawnedChunkMassKg = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Slice")
	float VaporizedSliverMassKg = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Slice")
	float CutCrossSectionAreaCm2 = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Slice")
	int32 SpawnedChunkCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slice")
	FVector WorldCutCentroid = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FContractRuntimeSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	int32 ContractIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	float TargetTitheMassKg = 10.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	float QuotaToleranceKg = 0.5f;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	float SettledScaleMassKg = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	float RemainderMassKg = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	float InFlightMassKg = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	float AccumulatedTareWasteKg = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	float MaxAllowedTareWasteKg = 15.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	FString ContractDirectiveText;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	EDiverterRoute ActiveDiverterRoute = EDiverterRoute::TitheScale;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	bool bIsWithinQuotaWindow = false;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	bool bScaleSettledForStamp = false;
};

using FShiftHudSnapshot = FContractRuntimeSnapshot;

// Native C++ Telemetry Signatures (Zero UHT overhead, zero concrete coupling)
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTelemetryStreamSignature, ETelemetryStreamId /*StreamId*/, float /*NormalizedValue*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnTelemetryImpulseSignature, ETelemetryImpulseId /*ImpulseId*/, float /*NormalizedMagnitude*/, const FVector& /*WorldLocation*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnNativeContractSnapshotUpdatedSignature, const FContractRuntimeSnapshot& /*Snapshot*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnContractSnapshotUpdatedSignature, const FContractRuntimeSnapshot&, Snapshot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShiftVerdictSignature, EShiftVerdictType, Verdict, EContractFaultReason, FaultReason);
