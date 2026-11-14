// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "TareTitheTypes.generated.h"

UENUM(BlueprintType)
enum class EMonolithBaseShape : uint8
{
	Box                     UMETA(DisplayName = "Box"),
	OctagonalPrism          UMETA(DisplayName = "Octagonal Prism"),
	AsymmetricLBlock        UMETA(DisplayName = "Asymmetric L-Block"),
	SteppedCore             UMETA(DisplayName = "Stepped Core"),
	CustomStaticMesh        UMETA(DisplayName = "Custom Watertight StaticMesh"),
	CantileveredJHook       UMETA(DisplayName = "Cantilevered J-Hook"),
	OffsetCChannel          UMETA(DisplayName = "Offset C-Channel"),
	CounterweightedZBracket UMETA(DisplayName = "Counterweighted Z-Bracket")
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
	ContractPassed      UMETA(DisplayName = "Contract Passed (+1 Scrip)"),
	DirectorsCutPassed  UMETA(DisplayName = "Director's Cut Precision Passed (+2 Scrip)"),
	ContractFaultReset  UMETA(DisplayName = "Contract Fault & Reset"),
	ShiftCompleted      UMETA(DisplayName = "Shift Completed (Summary & Requisition)"),
	BondCleared         UMETA(DisplayName = "3-Shift Indenture Bond Cleared")
};

UENUM(BlueprintType)
enum class ERequisitionTrack : uint8
{
	ClampHydraulics     UMETA(DisplayName = "[1] Clamp Hydraulics (+25% Torque Cap)"),
	TareHopper          UMETA(DisplayName = "[2] Tare Hopper (+5.0 kg Capacity)"),
	VeinScannerDamper   UMETA(DisplayName = "[3] Vein Scanner & Pan Damper (+30% Scan / +25% Settle)")
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

UENUM(BlueprintType)
enum class ELaserSliceVisualState : uint8
{
	IdleOrSafeCyan             UMETA(DisplayName = "P6 Idle / Under-Quota Safe Cyan"),
	IsotopeProximityAmber      UMETA(DisplayName = "P5 Isotope Proximity Amber"),
	ValidQuotaOrPurgeEmerald   UMETA(DisplayName = "P4 Valid Quota Window / Tare Purge Emerald"),
	TitheQuotaOvershootMagenta UMETA(DisplayName = "P3 Tithe Quota Overshoot Magenta"),
	VeinHazardYellow           UMETA(DisplayName = "P2 Vein In Tithe Drop Yellow"),
	FaultOrCriticalRed         UMETA(DisplayName = "P1 Fault / Vein Intersect / Tare Overflow Red")
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
	bool bSeveredHalfContainsVein = false;

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
	int32 CurrentShiftNumber = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	int32 TargetBondShifts = 3;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	int32 ScripBalance = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	int32 LastStampScripAwarded = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	float TargetTitheMassKg = 10.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	float QuotaToleranceKg = 0.5f;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	float DirectorsCutToleranceKg = 0.25f;

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
	float ClampStrainNormalized = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	uint8 ClampHydraulicsLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	uint8 TareHopperLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	uint8 VeinScannerDamperLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	FString ContractDirectiveText;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	FString ContractBriefText;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	FString DirectorIntercomLine;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	FString TransientRequisitionFeedback;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	EDiverterRoute ActiveDiverterRoute = EDiverterRoute::TitheScale;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	bool bIsContractBriefActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	bool bLastRequisitionSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	bool bIsWithinQuotaWindow = false;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	bool bIsWithinDirectorsCutWindow = false;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	bool bLastStampWasDirectorsCut = false;

	UPROPERTY(BlueprintReadOnly, Category = "Shift")
	bool bScaleSettledForStamp = false;
};

using FShiftHudSnapshot = FContractRuntimeSnapshot;

struct FLaserSlicePaletteColors
{
	FLinearColor SafeIdleColor = FLinearColor(0.04f, 0.52f, 0.78f, 1.0f);
	FLinearColor IntersectCyanColor = FLinearColor(0.08f, 0.88f, 1.00f, 1.0f);
	FLinearColor WindowMatchEmeraldColor = FLinearColor(0.04f, 0.96f, 0.36f, 1.0f);
	FLinearColor TarePurgeReadyColor = FLinearColor(0.06f, 0.94f, 0.62f, 1.0f);
	FLinearColor VeinDropYellowColor = FLinearColor(1.00f, 0.88f, 0.04f, 1.0f);
	FLinearColor IsotopeProximityAmberColor = FLinearColor(1.00f, 0.36f, 0.02f, 1.0f);
	FLinearColor OvershootMagentaColor = FLinearColor(0.94f, 0.04f, 0.78f, 1.0f);
	float OvershootEmissiveGain = 1.45f;
	FLinearColor DangerCrimsonColor = FLinearColor(1.00f, 0.05f, 0.03f, 1.0f);
	FLinearColor CapacitorHotWhiteColor = FLinearColor(1.00f, 0.94f, 0.78f, 1.0f);
};

struct FLaserSliceVisualEvaluation
{
	ELaserSliceVisualState AimState = ELaserSliceVisualState::IdleOrSafeCyan;
	FLinearColor LaserBaseHue = FLinearColor(0.04f, 0.52f, 0.78f, 1.0f);
	FLinearColor LaserFinalEmissive = FLinearColor(0.04f, 0.52f, 0.78f, 1.0f);
	float SheetOpacity = 0.18f;
	FString ReticleYieldLine;
	FLinearColor ReticleYieldColor = FLinearColor(0.82f, 0.94f, 1.00f, 1.0f);
	bool bOverrideReticleAccent = false;
	bool bValidContact = false;
	bool bDirectVeinHit = false;
	bool bIsCapacitorSlicing = false;
};

/**
 * Single shared source-of-truth 6-state evaluator consumed identically by 3D M_LaserSheet_Master MID
 * and 2D STareTitheHudOverlay reticle readout so laser hue and reticle text never diverge.
 * Priority: P1 FaultOrCriticalRed > P2 VeinHazardYellow > P3 TitheQuotaOvershootMagenta
 *         > P4 ValidQuotaOrPurgeEmerald > P5 IsotopeProximityAmber > P6 IdleOrSafeCyan,
 * plus continuous active capacitor heat-surge modulation when LaserChargeNormalized > 0.02f.
 */
inline FLaserSliceVisualEvaluation EvaluateLaserSliceVisualState(
	const FContractRuntimeSnapshot& Snapshot,
	const FSlicePreviewTelemetry& Preview,
	bool bIntersectsMonolith,
	bool bIsFaultLockoutPhase,
	float VeinProximityNormalized,
	float LaserChargeNormalized,
	const FLaserSlicePaletteColors& Palette = FLaserSlicePaletteColors())
{
	constexpr float QuotaEps = 1.0e-3f;
	constexpr float MinValidContactMassKg = 0.05f;
	constexpr float MaxUnchargedBloomSafeEmissive = 1.58f;

	FLaserSliceVisualEvaluation Out;
	Out.bValidContact = bIntersectsMonolith &&
		Preview.bHasValidMassEstimate &&
		(Preview.EstimatedSeveredMassKg >= MinValidContactMassKg);
	Out.bDirectVeinHit = bIntersectsMonolith && Preview.bWillDetonate;

	const float EstSeveredKg = FMath::Max(0.0f, Preview.EstimatedSeveredMassKg);
	const float EstRemainderKg = FMath::Max(0.0f, Preview.EstimatedRemainderMassKg);
	const float SettledKg = FMath::Max(0.0f, Snapshot.SettledScaleMassKg);
	const float InFlightKg = FMath::Max(0.0f, Snapshot.InFlightMassKg);
	const float TargetKg = FMath::Max(0.0f, Snapshot.TargetTitheMassKg);
	const float ToleranceKg = FMath::Max(0.0f, Snapshot.QuotaToleranceKg);
	const float DirCutTolKg = FMath::Max(0.15f, Snapshot.DirectorsCutToleranceKg);
	const float MinWinKg = FMath::Max(0.0f, TargetKg - ToleranceKg);
	const float MaxWinKg = TargetKg + ToleranceKg;
	const float ProjectedScaleKg = SettledKg + InFlightKg + EstSeveredKg;
	const float ProjectedTareKg = FMath::Max(0.0f, Snapshot.AccumulatedTareWasteKg) + EstSeveredKg;
	const float MaxTareKg = FMath::Max(0.0f, Snapshot.MaxAllowedTareWasteKg);
	const bool bTareOverflowRisk = ProjectedTareKg > (MaxTareKg + QuotaEps);
	const EDiverterRoute Route = Snapshot.ActiveDiverterRoute;
	const float SafeProx = FMath::Clamp(VeinProximityNormalized, 0.0f, 1.0f);
	const float SafeCharge = FMath::Clamp(LaserChargeNormalized, 0.0f, 1.0f);

	const FLinearColor UiCyan(0.05f, 0.90f, 1.00f, 1.0f);
	const FLinearColor UiEmerald(0.10f, 0.98f, 0.44f, 1.0f);
	const FLinearColor UiGold(1.00f, 0.84f, 0.18f, 1.0f);
	const FLinearColor UiYellowGold(1.00f, 0.88f, 0.08f, 1.0f);
	const FLinearColor UiAmber(1.00f, 0.66f, 0.10f, 1.0f);
	const FLinearColor UiCrimson(1.00f, 0.22f, 0.16f, 1.0f);
	const FLinearColor UiPurgeMint(0.08f, 0.96f, 0.66f, 1.0f);
	const FLinearColor UiOvershootMagenta(0.98f, 0.22f, 0.86f, 1.0f);
	const FLinearColor UiNeutralIce(0.82f, 0.94f, 1.00f, 1.0f);

	float AimEmissiveGain = 1.0f;

	// P1: Fault Lockout / Direct Vein Intersect / Tare Overflow
	if (bIsFaultLockoutPhase || Out.bDirectVeinHit || (Out.bValidContact && Route == EDiverterRoute::TareChute && bTareOverflowRisk))
	{
		Out.AimState = ELaserSliceVisualState::FaultOrCriticalRed;
		Out.LaserBaseHue = Palette.DangerCrimsonColor;
		AimEmissiveGain = 1.0f + (SafeProx * 0.45f);
		Out.SheetOpacity = (bIsFaultLockoutPhase && !Out.bValidContact && !Out.bDirectVeinHit) ? 0.24f : 0.46f;
		Out.ReticleYieldColor = UiCrimson;
		Out.bOverrideReticleAccent = true;

		if (bIsFaultLockoutPhase)
		{
			Out.ReticleYieldLine = TEXT("STATION FAULT - AUTO-RESET");
		}
		else if (Out.bDirectVeinHit)
		{
			Out.ReticleYieldLine = FString::Printf(TEXT("CUT EST: +%.2f KG [VEIN INTERSECT]"), EstSeveredKg);
		}
		else
		{
			Out.ReticleYieldLine = FString::Printf(TEXT("SCRAP EST: %.2f KG [TARE OVERFLOW!]"), EstSeveredKg);
		}
	}
	// P2: Contaminated Severed Half Routed to Tithe Scale
	else if (Out.bValidContact && Preview.bSeveredHalfContainsVein && Route == EDiverterRoute::TitheScale)
	{
		Out.AimState = ELaserSliceVisualState::VeinHazardYellow;
		const float CrimsonAlpha = FMath::Clamp((SafeProx - 0.65f) / 0.35f, 0.0f, 1.0f);
		Out.LaserBaseHue = FMath::Lerp(Palette.VeinDropYellowColor, Palette.DangerCrimsonColor, CrimsonAlpha);
		AimEmissiveGain = 1.15f;
		Out.SheetOpacity = 0.46f;
		Out.ReticleYieldLine = FString::Printf(TEXT("CUT EST: +%.2f KG [VEIN -> SPACE]"), EstSeveredKg);
		Out.ReticleYieldColor = UiYellowGold;
		Out.bOverrideReticleAccent = true;
	}
	// P3: Tithe Quota Overshoot (OvershootMagenta — strictly never triggers on TareChute)
	else if (Out.bValidContact && Route == EDiverterRoute::TitheScale && !Preview.bSeveredHalfContainsVein && ProjectedScaleKg > (MaxWinKg + QuotaEps))
	{
		Out.AimState = ELaserSliceVisualState::TitheQuotaOvershootMagenta;
		Out.LaserBaseHue = Palette.OvershootMagentaColor;
		AimEmissiveGain = Palette.OvershootEmissiveGain;
		Out.SheetOpacity = 0.46f;
		Out.ReticleYieldLine = FString::Printf(TEXT("CUT EST: +%.2f -> %.2f KG [OVER!]"), EstSeveredKg, ProjectedScaleKg);
		Out.ReticleYieldColor = UiOvershootMagenta;
		Out.bOverrideReticleAccent = true;
	}
	// P4: Valid Tithe Quota Window (including Director's Cut) OR Clean Tare Vein Purge
	else if (Out.bValidContact &&
		((Route == EDiverterRoute::TitheScale && !Preview.bSeveredHalfContainsVein && ProjectedScaleKg >= (MinWinKg - QuotaEps) && ProjectedScaleKg <= (MaxWinKg + QuotaEps)) ||
		 (Route == EDiverterRoute::TareChute && Preview.bSeveredHalfContainsVein && !bTareOverflowRisk)))
	{
		Out.AimState = ELaserSliceVisualState::ValidQuotaOrPurgeEmerald;
		Out.SheetOpacity = 0.48f;
		AimEmissiveGain = 1.15f;

		if (Route == EDiverterRoute::TareChute)
		{
			Out.LaserBaseHue = Palette.TarePurgeReadyColor;
			Out.ReticleYieldLine = FString::Printf(TEXT("SCRAP EST: %.2f KG [PURGE READY]"), EstSeveredKg);
			Out.ReticleYieldColor = UiPurgeMint;
		}
		else
		{
			Out.LaserBaseHue = Palette.WindowMatchEmeraldColor;
			Out.bOverrideReticleAccent = true;
			if (FMath::Abs(ProjectedScaleKg - TargetKg) <= (DirCutTolKg + QuotaEps))
			{
				Out.ReticleYieldLine = FString::Printf(TEXT("CUT EST: +%.2f KG [DIRECTOR'S CUT!]"), EstSeveredKg);
				Out.ReticleYieldColor = UiGold;
			}
			else
			{
				Out.ReticleYieldLine = FString::Printf(TEXT("CUT EST: +%.2f KG [IN WINDOW!]"), EstSeveredKg);
				Out.ReticleYieldColor = UiEmerald;
			}
		}
	}
	// P5: Passive Isotope Proximity Warning (>= 0.35f) on Under-Quota Tithe or Clean Tare Scrap
	else if (Out.bValidContact && !Preview.bSeveredHalfContainsVein && SafeProx >= 0.35f)
	{
		Out.AimState = ELaserSliceVisualState::IsotopeProximityAmber;
		const float CrimsonAlpha = FMath::Clamp((SafeProx - 0.35f) / 0.65f, 0.0f, 1.0f);
		Out.LaserBaseHue = FMath::Lerp(Palette.IsotopeProximityAmberColor, Palette.DangerCrimsonColor, CrimsonAlpha * 0.65f);
		AimEmissiveGain = 1.25f;
		Out.SheetOpacity = 0.46f;
		Out.ReticleYieldLine = FString::Printf(TEXT("CUT EST: +%.2f KG [ISOTOPE PROX!]"), EstSeveredKg);
		Out.ReticleYieldColor = UiAmber;
		Out.bOverrideReticleAccent = true;
	}
	// P6: Idle No-Contact OR Safe Under-Quota Tithe OR Safe Non-Vein Tare Scrap
	else
	{
		Out.AimState = ELaserSliceVisualState::IdleOrSafeCyan;
		if (!Out.bValidContact)
		{
			Out.LaserBaseHue = Palette.SafeIdleColor;
			Out.SheetOpacity = 0.18f;
			Out.ReticleYieldLine = TEXT("CUT EST: --- (NO CONTACT)");
			Out.ReticleYieldColor = UiNeutralIce;
		}
		else if (Route == EDiverterRoute::TareChute)
		{
			Out.LaserBaseHue = Palette.IntersectCyanColor;
			Out.SheetOpacity = 0.42f;
			Out.ReticleYieldLine = FString::Printf(TEXT("SCRAP EST: %.2f KG (LEFT %.1f KG)"), EstSeveredKg, EstRemainderKg);
			Out.ReticleYieldColor = UiCyan;
		}
		else
		{
			Out.LaserBaseHue = Palette.IntersectCyanColor;
			Out.SheetOpacity = 0.42f;
			const float StillNeedKg = FMath::Max(0.01f, TargetKg - ProjectedScaleKg);
			Out.ReticleYieldLine = FString::Printf(TEXT("CUT EST: +%.2f KG (NEED +%.2f)"), EstSeveredKg, StillNeedKg);
			Out.ReticleYieldColor = UiCyan;
		}
	}

	// Continuous Active Capacitor Firing Heat-Surge Modulation (applied ON TOP of the 6-state aiming evaluation)
	if (SafeCharge > 0.02f)
	{
		Out.bIsCapacitorSlicing = true;
		const float ChargeAlpha = FMath::Clamp((SafeCharge - 0.02f) / 0.98f, 0.0f, 1.0f);
		const float SurgeEmissiveBoost = 1.0f + (SafeCharge * 3.0f);
		const int32 ChargePct = FMath::Clamp(FMath::RoundToInt(SafeCharge * 100.0f), 1, 100);

		if (Out.AimState == ELaserSliceVisualState::FaultOrCriticalRed)
		{
			const FLinearColor HotCrimson(1.00f, 0.24f, 0.16f, 1.0f);
			Out.LaserFinalEmissive = FMath::Lerp(Palette.DangerCrimsonColor, HotCrimson, ChargeAlpha * 0.65f) * SurgeEmissiveBoost;
			Out.ReticleYieldLine += FString::Printf(TEXT(" [SLICING... %d%%]"), ChargePct);
		}
		else
		{
			const float ToAmberAlpha = FMath::Clamp(ChargeAlpha * 2.2f, 0.0f, 1.0f);
			const float ToHotWhiteAlpha = FMath::Clamp((ChargeAlpha - 0.35f) / 0.65f, 0.0f, 1.0f);
			const FLinearColor HeatedHue = FMath::Lerp(
				FMath::Lerp(Out.LaserBaseHue, Palette.IsotopeProximityAmberColor, ToAmberAlpha),
				Palette.CapacitorHotWhiteColor,
				ToHotWhiteAlpha * 0.78f);
			Out.LaserFinalEmissive = HeatedHue * SurgeEmissiveBoost;
			Out.ReticleYieldLine += FString::Printf(TEXT(" [SLICING... %d%%]"), ChargePct);
			Out.ReticleYieldColor = FMath::Lerp(Out.ReticleYieldColor, UiAmber, FMath::Clamp(ChargeAlpha * 1.4f, 0.35f, 1.0f));
			Out.bOverrideReticleAccent = true;
		}
		Out.LaserFinalEmissive.A = 1.0f;
	}
	else
	{
		FLinearColor UnchargedEmissive = Out.LaserBaseHue * AimEmissiveGain;
		const float MaxChannel = FMath::Max3(UnchargedEmissive.R, UnchargedEmissive.G, UnchargedEmissive.B);
		if (MaxChannel > MaxUnchargedBloomSafeEmissive)
		{
			const float ClampScale = MaxUnchargedBloomSafeEmissive / MaxChannel;
			UnchargedEmissive.R *= ClampScale;
			UnchargedEmissive.G *= ClampScale;
			UnchargedEmissive.B *= ClampScale;
		}
		UnchargedEmissive.A = 1.0f;
		Out.LaserFinalEmissive = UnchargedEmissive;
	}

	return Out;
}

// Native C++ Telemetry Signatures (Zero UHT overhead, zero concrete coupling)
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTelemetryStreamSignature, ETelemetryStreamId /*StreamId*/, float /*NormalizedValue*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnTelemetryImpulseSignature, ETelemetryImpulseId /*ImpulseId*/, float /*NormalizedMagnitude*/, const FVector& /*WorldLocation*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnNativeContractSnapshotUpdatedSignature, const FContractRuntimeSnapshot& /*Snapshot*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnContractSnapshotUpdatedSignature, const FContractRuntimeSnapshot&, Snapshot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShiftVerdictSignature, EShiftVerdictType, Verdict, EContractFaultReason, FaultReason);
