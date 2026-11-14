// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/InspectorHudBridgeComponent.h"
#include "Components/AudioTelemetryBridgeComponent.h"
#include "Components/LaserCapacitorComponent.h"
#include "Components/LaserPlaneAimComponent.h"
#include "Data/MonolithContractDataAsset.h"
#include "Data/ShiftProgressionDataAsset.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UI/STareTitheHudOverlay.h"
#include "UnrealClient.h"

#define LOCTEXT_NAMESPACE "TareAndTitheHud"

int32 UInspectorHudBridgeComponent::GActiveMountedOverlayCount = 0;

namespace
{
	constexpr float QuotaEpsilonKg = 1.0e-3f;
	constexpr float MinPredictiveMassKg = 0.05f;
	constexpr double VeinDropEdgeDebounceSeconds = 0.18;

	uint64 GLatchedFaultPushFrameCounter = MAX_uint64;
	EContractFaultReason GLatchedFaultPushReason = EContractFaultReason::None;

	TAutoConsoleVariable<FString> CVarDiagBottomCrtJson(
		TEXT("TareTithe.Diag.BottomCrtJson"),
		TEXT("{}"),
		TEXT("Read-only Phase 12-FIX5 Bottom CRT console telemetry JSON"),
		ECVF_Default);

	FString EscapeDiagJsonField(const FString& Raw)
	{
		return Raw.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));
	}
}

UInspectorHudBridgeComponent::UInspectorHudBridgeComponent()
{
	// Tier 2 strictly enforced: 0.00 ms idle CPU cost; wakes ONLY while rolling displayed scale mass toward target.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

int32 UInspectorHudBridgeComponent::GetActiveMountedOverlayCount()
{
	return GActiveMountedOverlayCount;
}

void UInspectorHudBridgeComponent::BeginPlay()
{
	Super::BeginPlay();

	bActiveTutorialHint = false;
	bControlsCardExpanded = true;
	ActiveTutorialStageIndex = -1;
	for (int32 Idx = 0; Idx < 3; ++Idx)
	{
		bShift1TutorialStageSeen[Idx] = false;
	}
	LastPushedCenterHeadline.Reset();
	LastPushedCenterSubText.Reset();

	ResetTransientTelemetryAndHysteresis();

	CachedGraphicsQualityLevel = 2;
	CachedTsrScalePercent = 85.0f;
	if (UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
	{
		const int32 OverallQuality = Settings->GetOverallScalabilityLevel();
		if (OverallQuality >= 0 && OverallQuality <= 3)
		{
			CachedGraphicsQualityLevel = OverallQuality;
			static const float PresetTsrScaleByLevel[4] = { 65.0f, 75.0f, 85.0f, 100.0f };
			CachedTsrScalePercent = PresetTsrScaleByLevel[OverallQuality];
		}
		float CurrentScaleNormalized = 0.0f;
		float CurrentScaleValue = 0.0f;
		float MinScaleValue = 50.0f;
		float MaxScaleValue = 100.0f;
		Settings->GetResolutionScaleInformationEx(CurrentScaleNormalized, CurrentScaleValue, MinScaleValue, MaxScaleValue);
		if (CurrentScaleValue >= 50.0f && CurrentScaleValue <= 100.0f)
		{
			CachedTsrScalePercent = CurrentScaleValue;
		}
	}

	UWorld* World = GetWorld();
	if (!IsValid(World) || !World->IsGameWorld())
	{
		return;
	}

	if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
	{
		ShiftSubsystem->OnNativeContractSnapshotUpdated.AddUObject(this, &UInspectorHudBridgeComponent::HandleContractSnapshotUpdated);
		ShiftSubsystem->OnShiftVerdict.AddUniqueDynamic(this, &UInspectorHudBridgeComponent::HandleShiftVerdict);
		ShiftSubsystem->OnShiftPhaseChanged.AddUObject(this, &UInspectorHudBridgeComponent::HandleShiftPhaseChanged);
		ShiftSubsystem->OnActiveContractLoaded.AddUObject(this, &UInspectorHudBridgeComponent::HandleActiveContractLoaded);
		ShiftSubsystem->OnPresentationAssetChanged.AddUObject(this, &UInspectorHudBridgeComponent::ApplyPresentationData);
		ShiftSubsystem->OnSliceCommittedSummary.AddUObject(this, &UInspectorHudBridgeComponent::HandleSliceCommittedResult);
		ShiftSubsystem->OnCareerReset.AddUObject(this, &UInspectorHudBridgeComponent::HandleCareerReset);

		ApplyPresentationData(ShiftSubsystem->GetActivePresentationAsset());
		CachedShiftPhase = ShiftSubsystem->GetCurrentPhase();
		CachedContractIndex = ShiftSubsystem->GetCurrentContractIndex();
		CachedTotalContracts = FMath::Max(1, ShiftSubsystem->GetTotalContractCount());
		CachedSnapshot = ShiftSubsystem->GetCurrentSnapshot();
		SnapRollingMassImmediate(CachedSnapshot.SettledScaleMassKg);
		bCachedReticleVisible = (CachedShiftPhase == EShiftPhase::ActiveInspection);
		TryArmShift1TutorialForCurrentContract();
	}

	MountOverlayIfNeeded();
	PushAllPanels();
}

void UInspectorHudBridgeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SnapRollingMassImmediate(0.0f);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TutorialAutoDismissTimerHandle);

		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->OnNativeContractSnapshotUpdated.RemoveAll(this);
			ShiftSubsystem->OnShiftVerdict.RemoveDynamic(this, &UInspectorHudBridgeComponent::HandleShiftVerdict);
			ShiftSubsystem->OnShiftPhaseChanged.RemoveAll(this);
			ShiftSubsystem->OnActiveContractLoaded.RemoveAll(this);
			ShiftSubsystem->OnPresentationAssetChanged.RemoveAll(this);
			ShiftSubsystem->OnSliceCommittedSummary.RemoveAll(this);
			ShiftSubsystem->OnCareerReset.RemoveAll(this);
		}
	}

	if (IsValid(BoundLaserAimComp))
	{
		BoundLaserAimComp->OnPreviewUpdated.RemoveAll(this);
		BoundLaserAimComp = nullptr;
	}
	if (IsValid(BoundLaserCapacitorComp))
	{
		BoundLaserCapacitorComp->OnSliceCommittedResult.RemoveAll(this);
		BoundLaserCapacitorComp->OnTelemetryStream.RemoveAll(this);
		BoundLaserCapacitorComp = nullptr;
	}
	if (IsValid(BoundAudioBridgeComp))
	{
		BoundAudioBridgeComp->OnFilteredTelemetryStream.RemoveAll(this);
		BoundAudioBridgeComp = nullptr;
	}

	if (ViewportResizeDelegateHandle.IsValid())
	{
		FViewport::ViewportResizedEvent.Remove(ViewportResizeDelegateHandle);
		ViewportResizeDelegateHandle.Reset();
	}

	UnmountOverlayDeterministic();

	Super::EndPlay(EndPlayReason);
}

void UInspectorHudBridgeComponent::BindPawnTelemetrySources(
	ULaserPlaneAimComponent* InLaserAimComp,
	ULaserCapacitorComponent* InLaserCapacitorComp,
	UAudioTelemetryBridgeComponent* InAudioBridgeComp,
	FOnInspectorReticleProjectionUpdatedSignature& InReticleProjectionDelegate)
{
	if (IsValid(BoundLaserAimComp))
	{
		BoundLaserAimComp->OnPreviewUpdated.RemoveAll(this);
	}
	BoundLaserAimComp = InLaserAimComp;
	if (IsValid(BoundLaserAimComp))
	{
		BoundLaserAimComp->OnPreviewUpdated.AddUObject(this, &UInspectorHudBridgeComponent::HandlePreviewUpdated);
		CachedPreview = BoundLaserAimComp->GetLastPreviewTelemetry();
	}

	if (IsValid(BoundLaserCapacitorComp))
	{
		BoundLaserCapacitorComp->OnSliceCommittedResult.RemoveAll(this);
		BoundLaserCapacitorComp->OnTelemetryStream.RemoveAll(this);
	}
	BoundLaserCapacitorComp = InLaserCapacitorComp;
	if (IsValid(BoundLaserCapacitorComp))
	{
		BoundLaserCapacitorComp->OnSliceCommittedResult.AddUObject(this, &UInspectorHudBridgeComponent::HandleSliceCommittedResult);
		BoundLaserCapacitorComp->OnTelemetryStream.AddUObject(this, &UInspectorHudBridgeComponent::HandleCapacitorTelemetryStream);
	}

	if (IsValid(BoundAudioBridgeComp))
	{
		BoundAudioBridgeComp->OnFilteredTelemetryStream.RemoveAll(this);
	}
	BoundAudioBridgeComp = InAudioBridgeComp;
	if (IsValid(BoundAudioBridgeComp))
	{
		BoundAudioBridgeComp->OnFilteredTelemetryStream.AddUObject(this, &UInspectorHudBridgeComponent::HandleFilteredTelemetryStream);
		CachedClampStrain = FMath::Clamp(
			BoundAudioBridgeComp->GetLastFlushedStreamValue(ETelemetryStreamId::ClampStrain),
			0.0f,
			1.0f);
		CachedClampFatigue = FMath::Clamp(
			BoundAudioBridgeComp->GetLastFlushedStreamValue(ETelemetryStreamId::ClampFatigue),
			0.0f,
			1.0f);
	}

	if (ReticleProjectionDelegateHandle.IsValid())
	{
		InReticleProjectionDelegate.Remove(ReticleProjectionDelegateHandle);
		ReticleProjectionDelegateHandle.Reset();
	}
	ReticleProjectionDelegateHandle = InReticleProjectionDelegate.AddUObject(
		this,
		&UInspectorHudBridgeComponent::HandleReticleProjectionUpdated);

	PushAllPanels();
}

void UInspectorHudBridgeComponent::ApplyPresentationData(const UTareTithePresentationDataAsset* InPresentationData)
{
	PresentationData = InPresentationData;
	ActiveHudConfig = IsValid(InPresentationData)
		? InPresentationData->HudConfig
		: FHudPresentationConfig();

	if (ActiveHudConfig.bEnableLegacyWorldSpaceReadouts)
	{
		UnmountOverlayDeterministic();
		return;
	}

	MountOverlayIfNeeded();
	if (OverlayWidget.IsValid())
	{
		OverlayWidget->ApplyHudConfig(ActiveHudConfig);
		PushAllPanels();
	}
}

void UInspectorHudBridgeComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const FPhase12EPresentationPolishConfig Phase12ECfg = ResolvePhase12EConfig();
	if (!Phase12ECfg.bEnableDampedWeightReadout || !OverlayWidget.IsValid())
	{
		DisplayedPanMassKg = TargetPanMassKg;
		PushBottomCrtConsole();
		SetComponentTickEnabled(false);
		return;
	}

	const float ClampedDt = FMath::Clamp(DeltaTime, 0.0f, 0.05f);
	DisplayedPanMassKg = FMath::FInterpTo(
		DisplayedPanMassKg,
		TargetPanMassKg,
		ClampedDt,
		Phase12ECfg.WeightReadoutInterpSpeed);

	if (FMath::IsNearlyEqual(DisplayedPanMassKg, TargetPanMassKg, 0.004f))
	{
		DisplayedPanMassKg = TargetPanMassKg;
		SetComponentTickEnabled(false);
	}

	PushBottomCrtConsole();
}

void UInspectorHudBridgeComponent::NotifyScalabilityPresetApplied(int32 QualityLevel, float TsrScalePercent)
{
	CachedGraphicsQualityLevel = FMath::Clamp(QualityLevel, 0, 3);
	CachedTsrScalePercent = FMath::Clamp(TsrScalePercent, 50.0f, 100.0f);
	PushGraphicsQualityRow();
}

void UInspectorHudBridgeComponent::MountOverlayIfNeeded()
{
	if (bIsMountedToViewport || ActiveHudConfig.bEnableLegacyWorldSpaceReadouts)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World) || !World->IsGameWorld())
	{
		return;
	}

	UGameViewportClient* ViewportClient = World->GetGameViewport();
	if (!IsValid(ViewportClient))
	{
		return;
	}

	const FVector2D InitialLocalSize = ComputeCurrentViewportLocalSize();
	SAssignNew(OverlayWidget, STareTitheHudOverlay)
		.HudConfig(ActiveHudConfig)
		.InitialLocalViewportSize(InitialLocalSize);
	OverlayWidget->SetControlsCardExpanded(bControlsCardExpanded);
	PushGraphicsQualityRow();

	ViewportClient->AddViewportWidgetContent(OverlayWidget.ToSharedRef(), 10);
	MountedViewportClient = ViewportClient;
	bIsMountedToViewport = true;
	++GActiveMountedOverlayCount;

	if (!ViewportResizeDelegateHandle.IsValid())
	{
		ViewportResizeDelegateHandle = FViewport::ViewportResizedEvent.AddUObject(
			this,
			&UInspectorHudBridgeComponent::HandleViewportResized);
	}
}

void UInspectorHudBridgeComponent::UnmountOverlayDeterministic()
{
	if (bIsMountedToViewport)
	{
		UGameViewportClient* ViewportClient = MountedViewportClient.Get();
		if (!IsValid(ViewportClient))
		{
			if (UWorld* World = GetWorld())
			{
				ViewportClient = World->GetGameViewport();
			}
		}

		if (IsValid(ViewportClient) && OverlayWidget.IsValid())
		{
			ViewportClient->RemoveViewportWidgetContent(OverlayWidget.ToSharedRef());
		}

		bIsMountedToViewport = false;
		GActiveMountedOverlayCount = FMath::Max(0, GActiveMountedOverlayCount - 1);
	}

	MountedViewportClient.Reset();
	OverlayWidget.Reset();
}

FVector2D UInspectorHudBridgeComponent::ComputeCurrentViewportLocalSize() const
{
	const UWorld* World = GetWorld();
	const UGameViewportClient* ViewportClient = IsValid(World) ? World->GetGameViewport() : nullptr;
	if (!IsValid(ViewportClient))
	{
		return FVector2D(1536.0f, 864.0f);
	}

	FVector2D PixelSize(1920.0f, 1080.0f);
	ViewportClient->GetViewportSize(PixelSize);
	const float DpiScale = FMath::Max(0.25f, ViewportClient->GetDPIScale());
	return FVector2D(
		FMath::Max(320.0f, PixelSize.X / DpiScale),
		FMath::Max(240.0f, PixelSize.Y / DpiScale));
}

void UInspectorHudBridgeComponent::HandleViewportResized(FViewport* /*Viewport*/, uint32 /*UnusedResizeCode*/)
{
	if (OverlayWidget.IsValid())
	{
		OverlayWidget->SetLocalViewportSize(ComputeCurrentViewportLocalSize());
	}
}

void UInspectorHudBridgeComponent::ResetTransientTelemetryAndHysteresis()
{
	// Correction E: Explicit contract & intro reset hygiene so stale hazard/strain latches never bleed across resets.
	CachedClampStrain = 0.0f;
	CachedClampFatigue = 0.0f;
	CachedLaserCharge = 0.0f;
	CachedPreview = FSlicePreviewTelemetry();
	SnapRollingMassImmediate(0.0f);

	bLatchedIsotopeHazard = false;
	bLatchedVeinInDrop = false;
	bLatchedClampCritical = false;
	bLatchedDirectorsCutReady = false;
	bLatchedQuotaMetReady = false;
	bHasLatchedTerminalVerdict = false;
	LatchedFaultReason = EContractFaultReason::None;
	if (!(bActiveTutorialHint && ActiveCenterAlertState == EHudCenterAlertState::P3_TutorialHint))
	{
		ActiveCenterAlertState = EHudCenterAlertState::None;
	}
	LastVeinInDropAutoClearTimeSec = -10.0;
}

void UInspectorHudBridgeComponent::TryArmShift1TutorialForCurrentContract()
{
	if (!ActiveHudConfig.bEnableShift1MicroTutorial)
	{
		DismissActiveTutorialHint(false);
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World) || !World->IsGameWorld())
	{
		return;
	}

	const UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>();
	const EShiftPhase EffectivePhase = IsValid(ShiftSubsystem) ? ShiftSubsystem->GetCurrentPhase() : CachedShiftPhase;
	if (EffectivePhase != EShiftPhase::ActiveInspection)
	{
		return;
	}

	const int32 ShiftNum = IsValid(ShiftSubsystem)
		? ShiftSubsystem->GetCurrentShiftNumber()
		: FMath::Max(1, CachedSnapshot.CurrentShiftNumber);
	const int32 ContractIdx = IsValid(ShiftSubsystem)
		? ShiftSubsystem->GetCurrentContractIndex()
		: CachedContractIndex;

	if (ShiftNum != 1 || ContractIdx < 0 || ContractIdx >= 3)
	{
		DismissActiveTutorialHint(false);
		return;
	}

	if (bActiveTutorialHint && ActiveTutorialStageIndex == ContractIdx)
	{
		return;
	}

	// Once-per-contract guard: fault retries on the same contract never re-spam the tutorial hint.
	if (bShift1TutorialStageSeen[ContractIdx])
	{
		return;
	}

	World->GetTimerManager().ClearTimer(TutorialAutoDismissTimerHandle);

	bShift1TutorialStageSeen[ContractIdx] = true;
	bActiveTutorialHint = true;
	ActiveTutorialStageIndex = ContractIdx;

	float DurationSec = ActiveHudConfig.Shift1Stage1DurationSeconds;
	if (ContractIdx == 1)
	{
		DurationSec = ActiveHudConfig.Shift1Stage2DurationSeconds;
	}
	else if (ContractIdx == 2)
	{
		DurationSec = ActiveHudConfig.Shift1Stage3DurationSeconds;
	}
	DurationSec = FMath::Clamp(DurationSec, 1.0f, 15.0f);

	World->GetTimerManager().SetTimer(
		TutorialAutoDismissTimerHandle,
		this,
		&UInspectorHudBridgeComponent::HandleTutorialAutoDismissTimerElapsed,
		DurationSec,
		false);
}

void UInspectorHudBridgeComponent::DismissActiveTutorialHint(bool bRefreshBanner)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TutorialAutoDismissTimerHandle);
	}

	const bool bWasActive = bActiveTutorialHint;
	bActiveTutorialHint = false;
	ActiveTutorialStageIndex = -1;

	if (bWasActive && bRefreshBanner)
	{
		PushCenterAlertBanner();
	}
}

void UInspectorHudBridgeComponent::HandleTutorialAutoDismissTimerElapsed()
{
	DismissActiveTutorialHint(true);
}

void UInspectorHudBridgeComponent::HandleCareerReset()
{
	DismissActiveTutorialHint(false);
	SnapRollingMassImmediate(0.0f);
	for (int32 Idx = 0; Idx < 3; ++Idx)
	{
		bShift1TutorialStageSeen[Idx] = false;
	}
}

void UInspectorHudBridgeComponent::HandleSliceCommittedResult(const FSliceResultSummary& Summary)
{
	// Refinement A: Dismiss Stage 1 & Stage 2 strictly on ECommitSliceOutcome::Success (never on MissedTarget air misfires).
	if (Summary.Outcome != ECommitSliceOutcome::Success)
	{
		return;
	}

	if (bActiveTutorialHint && (ActiveTutorialStageIndex == 0 || ActiveTutorialStageIndex == 1))
	{
		DismissActiveTutorialHint(true);
	}
}

void UInspectorHudBridgeComponent::HandleShiftVerdict(EShiftVerdictType Verdict, EContractFaultReason FaultReason)
{
	if (bHasLatchedTerminalVerdict &&
		LatchedVerdict == Verdict &&
		LatchedFaultReason == FaultReason &&
		GFrameCounter == GLatchedFaultPushFrameCounter)
	{
		return;
	}

	DismissActiveTutorialHint(false);

	LatchedVerdict = Verdict;
	LatchedFaultReason = FaultReason;
	bHasLatchedTerminalVerdict = true;

	bLatchedIsotopeHazard = false;
	bLatchedVeinInDrop = false;
	bLatchedClampCritical = false;
	bLatchedDirectorsCutReady = false;
	bLatchedQuotaMetReady = false;

	PushTopRightPanel();
	PushCenterAlertBanner();
	PushBottomCrtConsole();
}

void UInspectorHudBridgeComponent::HandleContractSnapshotUpdated(const FContractRuntimeSnapshot& Snapshot)
{
	const EDiverterRoute PreviousRoute = CachedSnapshot.ActiveDiverterRoute;
	CachedSnapshot = Snapshot;
	CachedContractIndex = Snapshot.ContractIndex;

	if (UWorld* World = GetWorld())
	{
		if (const UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			CachedTotalContracts = FMath::Max(1, ShiftSubsystem->GetTotalContractCount());
			CachedShiftPhase = ShiftSubsystem->GetCurrentPhase();
		}
	}

	// Stage 3 instant dismissal the frame the operator toggles [SPACE] to route into the right Tare shaft.
	if (bActiveTutorialHint && ActiveTutorialStageIndex == 2 && Snapshot.ActiveDiverterRoute == EDiverterRoute::TareChute)
	{
		DismissActiveTutorialHint(false);
	}

	// Allow immediate re-arm if operator manually toggles diverter back to TitheScale while hovering a vein drop
	if (PreviousRoute != Snapshot.ActiveDiverterRoute && Snapshot.ActiveDiverterRoute == EDiverterRoute::TitheScale)
	{
		LastVeinInDropAutoClearTimeSec = -10.0;
	}

	if (CachedShiftPhase == EShiftPhase::FaultResetDelay &&
		GFrameCounter == GLatchedFaultPushFrameCounter &&
		LatchedFaultReason == GLatchedFaultPushReason)
	{
		return;
	}

	SyncTargetPanMassFromSnapshot(false);
	PushAllPanels();
}

void UInspectorHudBridgeComponent::HandleShiftPhaseChanged(EShiftPhase NewPhase)
{
	CachedShiftPhase = NewPhase;
	bCachedReticleVisible = (NewPhase == EShiftPhase::ActiveInspection);
	if (NewPhase != EShiftPhase::ActiveInspection)
	{
		CachedLaserCharge = 0.0f;
	}

	if (NewPhase == EShiftPhase::ContractIntro)
	{
		DismissActiveTutorialHint(false);
		ResetTransientTelemetryAndHysteresis();
		// Safeguard 2: ContractIntro has 0.0s IntroDelay and transitions immediately into ActiveInspection in LoadAndBeginContract;
		// skip throwaway intermediate PushAllPanels() while keeping the ContractIntro -> ActiveInspection transition intact for Hold [R].
		return;
	}
	else if (NewPhase == EShiftPhase::ActiveInspection)
	{
		bHasLatchedTerminalVerdict = false;
		LatchedFaultReason = EContractFaultReason::None;
		if (UWorld* World = GetWorld())
		{
			if (const UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
			{
				CachedContractIndex = ShiftSubsystem->GetCurrentContractIndex();
				CachedTotalContracts = FMath::Max(1, ShiftSubsystem->GetTotalContractCount());
				CachedSnapshot = ShiftSubsystem->GetCurrentSnapshot();
			}
		}
		SyncTargetPanMassFromSnapshot(CachedSnapshot.SettledScaleMassKg <= KINDA_SMALL_NUMBER);
		// Refinement B: Arm Shift-01 tutorial strictly when aiming input unlocks in ActiveInspection.
		TryArmShift1TutorialForCurrentContract();
	}
	else if (NewPhase == EShiftPhase::FaultResetDelay)
	{
		DismissActiveTutorialHint(false);
		bLatchedIsotopeHazard = false;
		bLatchedVeinInDrop = false;
		bLatchedClampCritical = false;
		bLatchedDirectorsCutReady = false;
		bLatchedQuotaMetReady = false;

		if (UWorld* World = GetWorld())
		{
			if (const UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
			{
				CachedSnapshot = ShiftSubsystem->GetCurrentSnapshot();
				LatchedVerdict = EShiftVerdictType::ContractFaultReset;
				LatchedFaultReason = ShiftSubsystem->GetLastFaultReason();
				bHasLatchedTerminalVerdict = true;
				GLatchedFaultPushFrameCounter = GFrameCounter;
				GLatchedFaultPushReason = LatchedFaultReason;
			}
		}
	}
	else
	{
		DismissActiveTutorialHint(false);
		bLatchedIsotopeHazard = false;
		bLatchedVeinInDrop = false;
		bLatchedClampCritical = false;
		bLatchedDirectorsCutReady = false;
		bLatchedQuotaMetReady = false;
	}

	PushAllPanels();
}

void UInspectorHudBridgeComponent::HandleActiveContractLoaded(
	const UMonolithContractDataAsset* Contract,
	const UShiftProgressionDataAsset* ShiftConfig,
	int32 ContractIndex)
{
	if (ActiveTutorialStageIndex != ContractIndex)
	{
		DismissActiveTutorialHint(false);
	}

	ResetTransientTelemetryAndHysteresis();

	CachedContractIndex = ContractIndex;
	if (IsValid(ShiftConfig))
	{
		CachedTotalContracts = FMath::Max(1, ShiftConfig->OrderedContracts.Num());
	}

	if (UWorld* World = GetWorld())
	{
		if (const UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			CachedSnapshot = ShiftSubsystem->GetCurrentSnapshot();
			CachedShiftPhase = ShiftSubsystem->GetCurrentPhase();
			bCachedReticleVisible = (CachedShiftPhase == EShiftPhase::ActiveInspection);
		}
	}

	SnapRollingMassImmediate(CachedSnapshot.SettledScaleMassKg);

	if (IsValid(Contract) && CachedSnapshot.ContractDirectiveText.IsEmpty())
	{
		CachedSnapshot.ContractDirectiveText = Contract->ContractDirectiveText;
	}

	TryArmShift1TutorialForCurrentContract();
	PushAllPanels();
}

void UInspectorHudBridgeComponent::HandlePreviewUpdated(const FSlicePreviewTelemetry& PreviewTelemetry)
{
	CachedPreview = PreviewTelemetry;

	if (bActiveTutorialHint && ActiveTutorialStageIndex == 1 && IsValid(BoundLaserAimComp))
	{
		const FVector PlaneNormal = BoundLaserAimComp->GetCurrentPlaneNormalWorld().GetSafeNormal();
		const double AbsDotUp = FMath::Clamp(FMath::Abs(FVector::DotProduct(PlaneNormal, FVector::UpVector)), 0.0, 1.0);
		const float TiltDegrees = FMath::RadiansToDegrees(static_cast<float>(FMath::Acos(AbsDotUp)));
		if (TiltDegrees >= ActiveHudConfig.Shift1Stage2DismissTiltDegrees)
		{
			DismissActiveTutorialHint(false);
		}
	}

	PushReticleCard();
	PushCenterAlertBanner();
}

void UInspectorHudBridgeComponent::HandleFilteredTelemetryStream(ETelemetryStreamId StreamId, float NormalizedValue)
{
	if (StreamId == ETelemetryStreamId::ClampStrain)
	{
		CachedClampStrain = FMath::Clamp(NormalizedValue, 0.0f, 1.0f);
		PushReticleCard();
		PushCenterAlertBanner();
		PushTopRightPanel();
		PushBottomCrtConsole();
	}
	else if (StreamId == ETelemetryStreamId::ClampFatigue)
	{
		CachedClampFatigue = FMath::Clamp(NormalizedValue, 0.0f, 1.0f);
		PushReticleCard();
		PushCenterAlertBanner();
	}
}

void UInspectorHudBridgeComponent::HandleCapacitorTelemetryStream(ETelemetryStreamId StreamId, float NormalizedValue)
{
	if (StreamId == ETelemetryStreamId::LaserCharge)
	{
		CachedLaserCharge = FMath::Clamp(NormalizedValue, 0.0f, 1.0f);
		PushReticleCard();
	}
}

void UInspectorHudBridgeComponent::HandleReticleProjectionUpdated(
	const FVector2D& NormalizedViewportUV,
	bool bReticleVisible,
	const FVector& /*SteadyWorldAnchor*/)
{
	CachedReticleNormalizedUV = NormalizedViewportUV;
	bCachedReticleVisible = bReticleVisible;
	PushReticleCard();
}

void UInspectorHudBridgeComponent::PushAllPanels()
{
	if (!OverlayWidget.IsValid())
	{
		return;
	}
	PushTopLeftPanel();
	PushTopRightPanel();
	PushBottomCrtConsole();
	PushCenterAlertBanner();
	PushReticleCard();
	PushGraphicsQualityRow();
}

void UInspectorHudBridgeComponent::PushTopLeftPanel()
{
	if (!OverlayWidget.IsValid())
	{
		return;
	}

	FTopLeftIndentureViewModel VM;

	const int32 DisplayCtr = FMath::Clamp(CachedContractIndex + 1, 1, FMath::Max(1, CachedTotalContracts));
	const int32 TotalCtrs = FMath::Max(1, CachedTotalContracts);
	const int32 ShiftNum = FMath::Max(1, CachedSnapshot.CurrentShiftNumber);
	const int32 TargetShifts = FMath::Max(1, CachedSnapshot.TargetBondShifts);

	const FString HeaderStr = (ShiftNum <= TargetShifts)
		? FString::Printf(TEXT("INDENTURE #409 // SHIFT %02d/%02d - CTR %02d/%02d"), ShiftNum, TargetShifts, DisplayCtr, TotalCtrs)
		: FString::Printf(TEXT("INDENTURE #409 // OVT-%02d - CTR %02d/%02d"), ShiftNum, DisplayCtr, TotalCtrs);
	VM.HeaderText = FText::FromString(HeaderStr);

	const bool bShowBrief = (CachedShiftPhase == EShiftPhase::ActiveInspection) &&
		CachedSnapshot.bIsContractBriefActive &&
		!CachedSnapshot.ContractBriefText.IsEmpty();

	if (bShowBrief)
	{
		VM.DirectiveBodyText = FText::FromString(CachedSnapshot.ContractBriefText);
		VM.DirectiveTextColor = FLinearColor(1.00f, 0.78f, 0.16f, 1.0f);
		VM.AccentBarColor = FLinearColor(1.00f, 0.72f, 0.10f, 1.0f);
	}
	else
	{
		const FString DirectiveStr = CachedSnapshot.ContractDirectiveText.IsEmpty()
			? TEXT("CUT TO QUOTA | STAMP [E]")
			: CachedSnapshot.ContractDirectiveText;
		VM.DirectiveBodyText = FText::FromString(DirectiveStr);
		VM.DirectiveTextColor = FLinearColor(1.00f, 0.84f, 0.22f, 1.0f);
		VM.AccentBarColor = FLinearColor(0.05f, 0.88f, 1.00f, 1.0f);
	}

	const int32 Scrip = FMath::Max(0, CachedSnapshot.ScripBalance);
	VM.ScripBalanceText = FText::FromString(FString::Printf(TEXT("SCRIP: %02d"), Scrip));

	const uint8 Levels[3] = {
		CachedSnapshot.ClampHydraulicsLevel,
		CachedSnapshot.TareHopperLevel,
		CachedSnapshot.VeinScannerDamperLevel
	};
	const TCHAR* Labels[3] = { TEXT("[1]HYD"), TEXT("[2]TARE"), TEXT("[3]SCAN") };

	for (int32 Idx = 0; Idx < 3; ++Idx)
	{
		const int32 Lv = FMath::Clamp(static_cast<int32>(Levels[Idx]), 0, 3);
		if (Lv >= 3)
		{
			VM.UpgradePillTexts[Idx] = FText::FromString(FString::Printf(TEXT("%s MAX"), Labels[Idx]));
			VM.UpgradePillColors[Idx] = FLinearColor(1.00f, 0.82f, 0.18f, 1.0f);
		}
		else
		{
			const int32 Cost = UInspectionShiftSubsystem::GetRequisitionNextCost(static_cast<uint8>(Lv));
			VM.UpgradePillTexts[Idx] = FText::FromString(FString::Printf(TEXT("%s L%d(%d)"), Labels[Idx], Lv, Cost));
			VM.UpgradePillColors[Idx] = (Scrip >= Cost)
				? FLinearColor(0.10f, 0.98f, 0.44f, 1.0f)
				: FLinearColor(0.58f, 0.66f, 0.74f, 0.92f);
		}
	}

	if (!CachedSnapshot.TransientRequisitionFeedback.IsEmpty())
	{
		VM.bHasTransientFeedback = true;
		VM.TransientFeedbackText = FText::FromString(CachedSnapshot.TransientRequisitionFeedback);
		VM.TransientFeedbackColor = CachedSnapshot.bLastRequisitionSucceeded
			? FLinearColor(0.10f, 0.98f, 0.44f, 1.0f)
			: FLinearColor(1.00f, 0.24f, 0.18f, 1.0f);
	}

	OverlayWidget->UpdateTopLeftPanel(VM);
}

void UInspectorHudBridgeComponent::PushTopRightPanel()
{
	if (!OverlayWidget.IsValid())
	{
		return;
	}

	FTopRightDirectorViewModel VM;
	VM.HeaderText = LOCTEXT("DirectorHeader", "OFFICE OF THE DIRECTOR // OVERLOOK DECK");

	const FString IntercomStr = CachedSnapshot.DirectorIntercomLine.IsEmpty()
		? TEXT("#409: WORK OFF YOUR 3-SHIFT BOND. CARVE.")
		: CachedSnapshot.DirectorIntercomLine;
	VM.IntercomBodyText = FText::FromString(IntercomStr);

	const float EffectiveStrain = FMath::Max(CachedClampStrain, CachedSnapshot.ClampStrainNormalized);

	const bool bPhysicallySettling = IsScaleVisuallySettling();

	if (CachedShiftPhase == EShiftPhase::FaultResetDelay)
	{
		VM.StatusBadgeText = LOCTEXT("BadgeFault", "[FAULT]");
		VM.AccentBarColor = FLinearColor(1.00f, 0.20f, 0.14f, 1.0f);
		VM.IntercomTextColor = FLinearColor(1.00f, 0.34f, 0.26f, 1.0f);
	}
	else if (CachedShiftPhase == EShiftPhase::BondCleared)
	{
		VM.StatusBadgeText = LOCTEXT("BadgeCleared", "[CLEARED]");
		VM.AccentBarColor = FLinearColor(1.00f, 0.82f, 0.18f, 1.0f);
		VM.IntercomTextColor = FLinearColor(1.00f, 0.86f, 0.30f, 1.0f);
	}
	else if (CachedShiftPhase == EShiftPhase::ContractPassTransition || CachedShiftPhase == EShiftPhase::ShiftCompleted)
	{
		const bool bGold = CachedSnapshot.bLastStampWasDirectorsCut;
		VM.StatusBadgeText = LOCTEXT("BadgeStamped", "[STAMPED]");
		VM.AccentBarColor = bGold ? FLinearColor(1.00f, 0.82f, 0.18f, 1.0f) : FLinearColor(0.10f, 0.98f, 0.44f, 1.0f);
		VM.IntercomTextColor = VM.AccentBarColor;
	}
	else if (EffectiveStrain >= ActiveHudConfig.ClampCriticalEnterThreshold)
	{
		VM.StatusBadgeText = LOCTEXT("BadgeShear", "[WARN]");
		VM.AccentBarColor = FLinearColor(1.00f, 0.24f, 0.16f, 1.0f);
		VM.IntercomTextColor = FLinearColor(1.00f, 0.42f, 0.24f, 1.0f);
	}
	else if (!bPhysicallySettling && CachedSnapshot.bScaleSettledForStamp && CachedSnapshot.bIsWithinDirectorsCutWindow)
	{
		VM.StatusBadgeText = LOCTEXT("BadgeDirCut", "[DIR CUT]");
		VM.AccentBarColor = FLinearColor(1.00f, 0.82f, 0.18f, 1.0f);
		VM.IntercomTextColor = FLinearColor(1.00f, 0.86f, 0.28f, 1.0f);
	}
	else if (!bPhysicallySettling && CachedSnapshot.bScaleSettledForStamp)
	{
		VM.StatusBadgeText = LOCTEXT("BadgeReady", "[READY]");
		VM.AccentBarColor = FLinearColor(0.10f, 0.98f, 0.44f, 1.0f);
		VM.IntercomTextColor = FLinearColor(0.24f, 0.98f, 0.54f, 1.0f);
	}
	else
	{
		VM.StatusBadgeText = LOCTEXT("BadgeLive", "[PA LIVE]");
		VM.AccentBarColor = FLinearColor(0.05f, 0.86f, 1.00f, 1.0f);
		VM.IntercomTextColor = FLinearColor(0.46f, 0.84f, 1.00f, 1.0f);
	}

	OverlayWidget->UpdateTopRightPanel(VM);
}

void UInspectorHudBridgeComponent::PushCenterAlertBanner()
{
	if (!OverlayWidget.IsValid())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const double NowSec = IsValid(World) ? World->GetTimeSeconds() : 0.0;

	FCenterAlertViewModel VM;
	EHudCenterAlertState NextState = EHudCenterAlertState::None;

	// ========================================================================
	// PRIORITY P0: TERMINAL VERDICT / FAULT / SHIFT / BOND STAMPS
	// ========================================================================
	if (CachedShiftPhase == EShiftPhase::FaultResetDelay ||
		(bHasLatchedTerminalVerdict && LatchedVerdict == EShiftVerdictType::ContractFaultReset))
	{
		DismissActiveTutorialHint(false);
		NextState = EHudCenterAlertState::P0_FaultStamp;
		VM.bIsTerminalStamp = true;
		VM.PrimaryBannerText = FormatFaultStampHeadline(LatchedFaultReason);
		VM.SecondarySubText = LOCTEXT("FaultSubPrompt", "AUTO-RESETTING CELL // HOLD [R] TO RESET");
		VM.AccentColor = FLinearColor(1.00f, 0.20f, 0.14f, 1.0f);
	}
	else if (CachedShiftPhase == EShiftPhase::BondCleared ||
		(bHasLatchedTerminalVerdict && LatchedVerdict == EShiftVerdictType::BondCleared))
	{
		DismissActiveTutorialHint(false);
		NextState = EHudCenterAlertState::P0_BondCleared;
		VM.bIsTerminalStamp = true;
		VM.PrimaryBannerText = LOCTEXT("BondClearedStamp", "3-SHIFT INDENTURE BOND CLEARED - #409 RELEASED");
		VM.SecondarySubText = LOCTEXT("BondClearedSub", "PRESS [E] OVERTIME  |  [1-3] SPEND SCRIP  |  HOLD [R] CAREER RESET");
		VM.AccentColor = FLinearColor(1.00f, 0.82f, 0.18f, 1.0f);
	}
	else if (CachedShiftPhase == EShiftPhase::ShiftCompleted ||
		(bHasLatchedTerminalVerdict && LatchedVerdict == EShiftVerdictType::ShiftCompleted))
	{
		DismissActiveTutorialHint(false);
		NextState = EHudCenterAlertState::P0_ShiftSummary;
		VM.bIsTerminalStamp = true;
		const int32 CurShift = FMath::Max(1, CachedSnapshot.CurrentShiftNumber);
		const int32 TargetShifts = FMath::Max(1, CachedSnapshot.TargetBondShifts);
		VM.PrimaryBannerText = (CurShift <= TargetShifts)
			? FText::FromString(FString::Printf(TEXT("SHIFT %02d/%02d COMPLETE - REQUISITION TERMINAL OPEN"), CurShift, TargetShifts))
			: FText::FromString(FString::Printf(TEXT("OVERTIME SHIFT %02d COMPLETE - REQUISITION OPEN"), CurShift));
		VM.SecondarySubText = LOCTEXT("ShiftCompleteSub", "PRESS [E] NEXT SHIFT  |  [1-3] SPEND SCRIP  |  HOLD [R] CAREER RESET");
		VM.AccentColor = FLinearColor(0.10f, 0.98f, 0.44f, 1.0f);
	}
	else if (CachedShiftPhase == EShiftPhase::ContractPassTransition ||
		(bHasLatchedTerminalVerdict &&
			(LatchedVerdict == EShiftVerdictType::ContractPassed || LatchedVerdict == EShiftVerdictType::DirectorsCutPassed)))
	{
		DismissActiveTutorialHint(false);
		NextState = EHudCenterAlertState::P0_PassStamp;
		VM.bIsTerminalStamp = true;
		const bool bDirCut = (LatchedVerdict == EShiftVerdictType::DirectorsCutPassed) || CachedSnapshot.bLastStampWasDirectorsCut;
		VM.PrimaryBannerText = bDirCut
			? LOCTEXT("DirCutPassStamp", "DIRECTOR'S CUT STAMPED (+2 SCRIP)")
			: LOCTEXT("StandardPassStamp", "QUOTA STAMPED (+1 SCRIP)");

		const FPhase12EPresentationPolishConfig Phase12ECfg = ResolvePhase12EConfig();
		if (Phase12ECfg.bEnablePassReceiptSubline)
		{
			const float SignedDevKg = CachedSnapshot.LastStampedDeviationKg;
			const int32 AwardedScrip = (CachedSnapshot.LastStampScripAwarded > 0)
				? CachedSnapshot.LastStampScripAwarded
				: (bDirCut ? 2 : 1);
			const int32 CurShift = FMath::Max(1, CachedSnapshot.CurrentShiftNumber);
			const int32 TargetShifts = FMath::Max(1, CachedSnapshot.TargetBondShifts);
			const int32 TotalPerShift = FMath::Max(1, CachedTotalContracts);
			const int32 TotalBondContracts = FMath::Max(1, TargetShifts * TotalPerShift);

			FString BondSegment;
			if (CurShift <= TargetShifts)
			{
				const int32 CompletedBondContracts = ((CurShift - 1) * TotalPerShift) + (CachedSnapshot.ContractIndex + 1);
				const int32 RemainingBondContracts = FMath::Clamp(TotalBondContracts - CompletedBondContracts, 0, TotalBondContracts);
				const int32 BondRemainingPct = FMath::Clamp(
					FMath::RoundToInt((static_cast<float>(RemainingBondContracts) / static_cast<float>(TotalBondContracts)) * 100.0f),
					0,
					100);
				BondSegment = FString::Printf(TEXT("BOND REMAINING: %d%%"), BondRemainingPct);
			}
			else
			{
				BondSegment = TEXT("OVERTIME (BOND 0%)");
			}

			const TCHAR* FastSkipHint = Phase12ECfg.bEnableContractPassFastSkip ? TEXT(" | [E] NEXT") : TEXT("");
			VM.SecondarySubText = FText::FromString(
				FString::Printf(
					TEXT("DEVIATION: %+0.2f KG | SCRIP: +%d | %s%s"),
					SignedDevKg,
					AwardedScrip,
					*BondSegment,
					FastSkipHint));
		}
		else
		{
			VM.SecondarySubText = LOCTEXT("PassSub", "ADVANCING TO NEXT MONOLITH CONTRACT...");
		}

		VM.AccentColor = bDirCut
			? FLinearColor(1.00f, 0.82f, 0.18f, 1.0f)
			: FLinearColor(0.10f, 0.98f, 0.44f, 1.0f);
	}
	else if (CachedShiftPhase == EShiftPhase::ActiveInspection)
	{
		// ====================================================================
		// PRIORITY P1: CRITICAL CUTTING HAZARDS (SCHMITT-TRIGGER HYSTERESIS)
		// ====================================================================

		// P1a: Isotope Proximity / Detonation
		if (!CachedPreview.bIntersectsMonolith ||
			(!CachedPreview.bWillDetonate && CachedPreview.VeinProximityNormalized <= ActiveHudConfig.VeinHazardExitThreshold))
		{
			bLatchedIsotopeHazard = false;
		}
		else if (CachedPreview.bIntersectsMonolith &&
			(CachedPreview.bWillDetonate || CachedPreview.VeinProximityNormalized >= ActiveHudConfig.VeinHazardEnterThreshold))
		{
			bLatchedIsotopeHazard = true;
		}

		// P1b: Severed Drop Contains Vein While Routed to Tithe (Instant Clear on [SPACE])
		const bool bValidContact = CachedPreview.bIntersectsMonolith &&
			CachedPreview.bHasValidMassEstimate &&
			(CachedPreview.EstimatedSeveredMassKg >= MinPredictiveMassKg);
		const bool bRawVeinInTitheDrop = bValidContact &&
			CachedPreview.bSeveredHalfContainsVein &&
			(CachedSnapshot.ActiveDiverterRoute == EDiverterRoute::TitheScale);

		if (!bRawVeinInTitheDrop)
		{
			if (bLatchedVeinInDrop)
			{
				bLatchedVeinInDrop = false;
				if (CachedSnapshot.ActiveDiverterRoute == EDiverterRoute::TitheScale)
				{
					LastVeinInDropAutoClearTimeSec = NowSec;
				}
			}
		}
		else if (!bLatchedVeinInDrop)
		{
			if ((NowSec - LastVeinInDropAutoClearTimeSec) >= VeinDropEdgeDebounceSeconds)
			{
				bLatchedVeinInDrop = true;
			}
		}

		// P1c: Critical Clamp Torque Strain OR Rising Bolt Fatigue
		const float EffectiveStrain = FMath::Max(CachedClampStrain, CachedSnapshot.ClampStrainNormalized);
		if (EffectiveStrain <= ActiveHudConfig.ClampCriticalExitThreshold &&
			CachedClampFatigue <= ActiveHudConfig.ClampFatigueWarnExitThreshold)
		{
			bLatchedClampCritical = false;
		}
		else if (EffectiveStrain >= ActiveHudConfig.ClampCriticalEnterThreshold ||
			CachedClampFatigue >= ActiveHudConfig.ClampFatigueWarnEnterThreshold)
		{
			bLatchedClampCritical = true;
		}

		// P2: Settled Quota Ready Callout
		const bool bPhysicallySettling = IsScaleVisuallySettling();
		bLatchedDirectorsCutReady = !bPhysicallySettling &&
			CachedSnapshot.bScaleSettledForStamp &&
			CachedSnapshot.bIsWithinDirectorsCutWindow;
		bLatchedQuotaMetReady = !bPhysicallySettling &&
			CachedSnapshot.bScaleSettledForStamp &&
			CachedSnapshot.bIsWithinQuotaWindow &&
			!CachedSnapshot.bIsWithinDirectorsCutWindow;

		if (bLatchedIsotopeHazard)
		{
			NextState = EHudCenterAlertState::P1_IsotopeHazard;
			VM.PrimaryBannerText = CachedPreview.bWillDetonate
				? LOCTEXT("IsotopeIntersectAlert", "ISOTOPE INTERSECT - ADJUST PLANE!")
				: LOCTEXT("IsotopeProximityAlert", "ISOTOPE PROXIMITY - ADJUST PLANE!");
			VM.SecondarySubText = LOCTEXT("IsotopeAlertSub", "VOLATILE VEIN CLEARANCE CRITICAL");
			VM.AccentColor = CachedPreview.bWillDetonate
				? FLinearColor(1.00f, 0.16f, 0.12f, 1.0f)
				: FLinearColor(1.00f, 0.56f, 0.08f, 1.0f);
		}
		else if (bLatchedVeinInDrop)
		{
			NextState = EHudCenterAlertState::P1_VeinInDrop;
			VM.PrimaryBannerText = LOCTEXT("VeinInDropAlert", "VEIN IN DROP - [SPACE] TO TARE!");
			VM.SecondarySubText = LOCTEXT("VeinInDropSub", "CONTAMINATED DROP ROUTED TO TITHE SCALE");
			VM.AccentColor = FLinearColor(1.00f, 0.62f, 0.08f, 1.0f);
		}
		else if (bLatchedClampCritical)
		{
			NextState = EHudCenterAlertState::P1_ClampShearCritical;
			VM.PrimaryBannerText = LOCTEXT("JawShearAlert", "JAW SHEAR CRITICAL - BALANCE CUT!");
			const int32 StrainPct = FMath::Clamp(FMath::RoundToInt(EffectiveStrain * 100.0f), 0, 999);
			const int32 FatiguePct = FMath::Clamp(FMath::RoundToInt(CachedClampFatigue * 100.0f), 0, 100);
			VM.SecondarySubText = FText::FromString(
				FString::Printf(TEXT("TORQUE LOAD: %d%%  |  BOLT FATIGUE: %d%%"), StrainPct, FatiguePct));
			VM.AccentColor = FLinearColor(1.00f, 0.22f, 0.14f, 1.0f);
		}
		else if (bLatchedDirectorsCutReady)
		{
			NextState = EHudCenterAlertState::P2_DirectorsCutReady;
			VM.PrimaryBannerText = LOCTEXT("DirectorsCutReadyAlert", "DIRECTOR'S CUT - PRESS [E] (+2 SCRIP)!");
			VM.SecondarySubText = LOCTEXT("DirectorsCutReadySub", "PRECISION TOLERANCE LOCKED ON TITHE SCALE");
			VM.AccentColor = FLinearColor(1.00f, 0.84f, 0.18f, 1.0f);
		}
		else if (bLatchedQuotaMetReady)
		{
			NextState = EHudCenterAlertState::P2_QuotaMetReady;
			VM.PrimaryBannerText = LOCTEXT("QuotaMetReadyAlert", "QUOTA MET - PRESS [E] TO STAMP (+1 SCRIP)");
			VM.SecondarySubText = LOCTEXT("QuotaMetReadySub", "VALID TITHE PAYLOAD SETTLED ON SCALE");
			VM.AccentColor = FLinearColor(0.10f, 0.98f, 0.44f, 1.0f);
		}
		else if (bActiveTutorialHint && ActiveTutorialStageIndex >= 0 && ActiveTutorialStageIndex < 3)
		{
			// PRIORITY P3: SHIFT-01 3-STAGE PROGRESSIVE ONBOARDING HINT (P0 > P1 > P2 > P3)
			NextState = EHudCenterAlertState::P3_TutorialHint;
			VM.bIsTutorialHint = true;
			VM.AccentColor = ActiveHudConfig.TutorialIceCyanTint;

			if (ActiveTutorialStageIndex == 0)
			{
				VM.PrimaryBannerText = LOCTEXT("Shift1TutStage1Headline", "MOVE MOUSE TO AIM \u2014 HOLD [LMB] TO SLICE");
				VM.SecondarySubText = LOCTEXT("Shift1TutStage1Sub", "LAND CUTS ON SCALE TO FILL QUOTA, THEN PRESS [E] TO STAMP");
			}
			else if (ActiveTutorialStageIndex == 1)
			{
				VM.PrimaryBannerText = LOCTEXT("Shift1TutStage2Headline", "ASYMMETRIC LOAD \u2014 [SCROLL] ROLL | HOLD [RMB] BEVEL");
				VM.SecondarySubText = LOCTEXT("Shift1TutStage2Sub", "HOLD [SHIFT] FOR FAST ROTATION \u2014 BALANCE OVERHANG TORQUE");
			}
			else
			{
				VM.PrimaryBannerText = LOCTEXT("Shift1TutStage3Headline", "VOLATILE VEIN \u2014 PRESS [SPACE] TO ROUTE TARE SCRAP");
				VM.SecondarySubText = LOCTEXT("Shift1TutStage3Sub", "DIVERT CONTAMINATED CUTS INTO THE RIGHT TARE SHAFT");
			}
		}
	}

	if (NextState != EHudCenterAlertState::None && NextState != ActiveCenterAlertState)
	{
		++AlertTriggerGeneration;
	}
	ActiveCenterAlertState = NextState;

	if (NextState == EHudCenterAlertState::None)
	{
		LastPushedCenterHeadline.Reset();
		LastPushedCenterSubText.Reset();
	}
	else
	{
		LastPushedCenterHeadline = VM.PrimaryBannerText.ToString();
		LastPushedCenterSubText = VM.SecondarySubText.ToString();
	}

	VM.AlertState = NextState;
	VM.TriggerGeneration = AlertTriggerGeneration;
	OverlayWidget->UpdateCenterAlertBanner(VM);
}

void UInspectorHudBridgeComponent::PushReticleCard()
{
	const FLaserSheetPresentationConfig SheetCfg = IsValid(PresentationData)
		? PresentationData->LaserSheetConfig.GetSanitized()
		: FLaserSheetPresentationConfig();
	FLaserSlicePaletteColors Palette;
	Palette.VeinDropYellowColor = SheetCfg.VeinDropYellowColor;
	Palette.IsotopeProximityAmberColor = SheetCfg.IsotopeProximityAmberColor;
	Palette.OvershootMagentaColor = SheetCfg.OvershootMagentaColor;
	Palette.OvershootEmissiveGain = SheetCfg.OvershootEmissiveGain;

	const FLaserSliceVisualEvaluation Eval = EvaluateLaserSliceVisualState(
		CachedSnapshot,
		CachedPreview,
		CachedPreview.bIntersectsMonolith,
		CachedShiftPhase == EShiftPhase::FaultResetDelay,
		CachedPreview.VeinProximityNormalized,
		CachedLaserCharge,
		Palette);

	LastEvaluatedReticleState = Eval.AimState;
	LastEvaluatedReticleYieldText = Eval.ReticleYieldLine;
	LastEvaluatedReticleYieldColor = Eval.ReticleYieldColor;

	if (!OverlayWidget.IsValid())
	{
		return;
	}

	FReticleReadoutViewModel VM;
	VM.bIsVisible = bCachedReticleVisible && (CachedShiftPhase == EShiftPhase::ActiveInspection);
	VM.NormalizedViewportUV = CachedReticleNormalizedUV;

	if (!VM.bIsVisible)
	{
		OverlayWidget->UpdateReticleCard(VM);
		return;
	}

	const float SettledKg = FMath::Max(0.0f, CachedSnapshot.SettledScaleMassKg);
	const float TargetKg = FMath::Max(0.0f, CachedSnapshot.TargetTitheMassKg);
	const float ToleranceKg = FMath::Max(0.0f, CachedSnapshot.QuotaToleranceKg);

	const FLinearColor CyanColor(0.05f, 0.90f, 1.00f, 1.0f);
	const FLinearColor EmeraldColor(0.10f, 0.98f, 0.44f, 1.0f);
	const FLinearColor GoldColor(1.00f, 0.84f, 0.18f, 1.0f);
	const FLinearColor AmberColor(1.00f, 0.66f, 0.10f, 1.0f);
	const FLinearColor CrimsonColor(1.00f, 0.22f, 0.16f, 1.0f);
	const FLinearColor NeutralIceColor(0.82f, 0.94f, 1.00f, 1.0f);

	const bool bPhysicallySettling = IsScaleVisuallySettling();

	// Line 1: Primary Need / Route / Stamp Status
	if (CachedSnapshot.ActiveDiverterRoute == EDiverterRoute::TareChute)
	{
		VM.PrimaryNeedOrStatusText = LOCTEXT("ReticleTareRoute", "ROUTE: TARE SCRAP [SPACE]");
		VM.PrimaryColor = AmberColor;
	}
	else if (SettledKg > (TargetKg + ToleranceKg + QuotaEpsilonKg))
	{
		VM.PrimaryNeedOrStatusText = LOCTEXT("ReticleOverQuota", "OVER QUOTA!");
		VM.PrimaryColor = CrimsonColor;
	}
	else if (!bPhysicallySettling && CachedSnapshot.bScaleSettledForStamp && CachedSnapshot.bIsWithinDirectorsCutWindow)
	{
		VM.PrimaryNeedOrStatusText = FText::FromString(
			FString::Printf(TEXT("DIRECTOR'S CUT [E] +2 SCRIP (%.2f KG)"), SettledKg));
		VM.PrimaryColor = GoldColor;
	}
	else if (!bPhysicallySettling && CachedSnapshot.bScaleSettledForStamp && CachedSnapshot.bIsWithinQuotaWindow)
	{
		VM.PrimaryNeedOrStatusText = FText::FromString(
			FString::Printf(TEXT("STAMP READY [E] +1 SCRIP (%.2f KG)"), SettledKg));
		VM.PrimaryColor = EmeraldColor;
	}
	else if (bPhysicallySettling)
	{
		VM.PrimaryNeedOrStatusText = FText::FromString(
			FString::Printf(TEXT("SETTLING... (%.2f KG)"), SettledKg));
		VM.PrimaryColor = GoldColor;
	}
	else
	{
		const float NeededDeltaKg = FMath::Max(0.01f, TargetKg - SettledKg);
		VM.PrimaryNeedOrStatusText = FText::FromString(
			FString::Printf(TEXT("TITHE | NEED +%.2f KG"), NeededDeltaKg));
		VM.PrimaryColor = CyanColor;
	}
	VM.AccentColor = VM.PrimaryColor;

	// Line 2: Shared 6-State Predictive Slice / Scrap Yield Estimate (1:1 synchronized with M_LaserSheet_Master MID)
	VM.YieldEstimateText = FText::FromString(Eval.ReticleYieldLine);
	VM.YieldColor = Eval.ReticleYieldColor;
	if (Eval.bOverrideReticleAccent)
	{
		VM.AccentColor = Eval.ReticleYieldColor;
	}

	// Line 3: Clamp Load & Torque Strain
	const float ClampLoadKg = FMath::Max(0.0f, CachedSnapshot.RemainderMassKg);
	const float EffectiveStrain = FMath::Max(CachedClampStrain, CachedSnapshot.ClampStrainNormalized);
	const int32 TorquePct = FMath::Clamp(FMath::RoundToInt(EffectiveStrain * 100.0f), 0, 999);

	if (EffectiveStrain >= ActiveHudConfig.ClampCriticalEnterThreshold ||
		CachedClampFatigue >= ActiveHudConfig.ClampFatigueWarnEnterThreshold)
	{
		VM.ClampLoadText = FText::FromString(
			FString::Printf(TEXT("CLAMP LOAD: %.1f KG  |  SHEAR: %d%%!"), ClampLoadKg, TorquePct));
		VM.ClampLoadColor = CrimsonColor;
	}
	else if (EffectiveStrain >= 0.42f)
	{
		VM.ClampLoadText = FText::FromString(
			FString::Printf(TEXT("CLAMP LOAD: %.1f KG  |  TORQUE: %d%%"), ClampLoadKg, TorquePct));
		VM.ClampLoadColor = AmberColor;
	}
	else
	{
		VM.ClampLoadText = FText::FromString(
			FString::Printf(TEXT("CLAMP LOAD: %.1f KG"), ClampLoadKg));
		VM.ClampLoadColor = NeutralIceColor;
	}

	OverlayWidget->UpdateReticleCard(VM);
}

void UInspectorHudBridgeComponent::PushBottomCrtConsole()
{
	if (!OverlayWidget.IsValid())
	{
		return;
	}

	FBottomCrtConsoleViewModel VM;
	VM.ConsoleHeaderText = LOCTEXT("CrtConsoleHeader", "WEIGHING CONSOLE");

	const float TargetKg = FMath::Max(0.0f, CachedSnapshot.TargetTitheMassKg);
	const float TolKg = FMath::Max(0.0f, CachedSnapshot.QuotaToleranceKg);
	const float MinValidKg = FMath::Max(0.0f, TargetKg - TolKg);
	const float MaxValidKg = TargetKg + TolKg;
	const float DirTolKg = FMath::Max(0.15f, CachedSnapshot.DirectorsCutToleranceKg);
	const float MinDirKg = FMath::Max(0.0f, TargetKg - DirTolKg);
	const float MaxDirKg = TargetKg + DirTolKg;

	VM.WindowsSummaryText = FText::FromString(
		FString::Printf(
			TEXT("WINDOW: +/-%.2f KG  |  DIR.CUT: +/-%.2f KG (+2 SCRIP)"),
			TolKg,
			DirTolKg));

	const FPhase12EPresentationPolishConfig Phase12ECfg = ResolvePhase12EConfig();
	const float SettledKg = FMath::Max(0.0f, CachedSnapshot.SettledScaleMassKg);
	const float ShownKg = Phase12ECfg.bEnableDampedWeightReadout
		? FMath::Max(0.0f, DisplayedPanMassKg)
		: SettledKg;
	VM.MassReadoutText = FText::FromString(FString::Printf(TEXT("%05.2f / %05.2f KG"), ShownKg, TargetKg));

	const bool bOvershoot = SettledKg > (MaxValidKg + QuotaEpsilonKg);
	const bool bFaultActive = (CachedShiftPhase == EShiftPhase::FaultResetDelay);
	const bool bPhysicallySettling = IsScaleVisuallySettling();

	const FLinearColor SkyBlue(0.45f, 0.84f, 1.00f, 1.0f);
	const FLinearColor Emerald(0.10f, 0.98f, 0.44f, 1.0f);
	const FLinearColor Gold(1.00f, 0.84f, 0.18f, 1.0f);
	const FLinearColor Amber(1.00f, 0.78f, 0.14f, 1.0f);
	const FLinearColor Crimson(1.00f, 0.24f, 0.18f, 1.0f);
	const FLinearColor Orange(1.00f, 0.56f, 0.06f, 1.0f);
	const FLinearColor CoolSlate(0.64f, 0.76f, 0.86f, 0.94f);

	if (bOvershoot || bFaultActive)
	{
		VM.MassReadoutColor = FLinearColor(1.00f, 0.22f, 0.16f, 1.0f);
	}
	else if (CachedSnapshot.bIsWithinDirectorsCutWindow)
	{
		VM.MassReadoutColor = FLinearColor(1.00f, 0.86f, 0.20f, 1.0f);
	}
	else if (CachedSnapshot.bIsWithinQuotaWindow)
	{
		VM.MassReadoutColor = Emerald;
	}
	else
	{
		VM.MassReadoutColor = Amber;
	}

	VM.bShowMassStatusBadge = Phase12ECfg.bEnableAsciiMassStatusBadge;
	if (bOvershoot || (bFaultActive && LatchedFaultReason == EContractFaultReason::QuotaOvershoot))
	{
		VM.MassStatusBadgeText = FText::FromString(TEXT("[OVER]"));
		VM.MassStatusBadgeColor = Crimson;
	}
	else if (bFaultActive)
	{
		VM.MassStatusBadgeText = FText::FromString(TEXT("[FAULT]"));
		VM.MassStatusBadgeColor = Crimson;
	}
	else if (bPhysicallySettling)
	{
		VM.MassStatusBadgeText = FText::FromString(TEXT("[SETTLING]"));
		VM.MassStatusBadgeColor = Gold;
	}
	else if (SettledKg <= 0.005f)
	{
		VM.MassStatusBadgeText = FText::FromString(TEXT("[EMPTY]"));
		VM.MassStatusBadgeColor = CoolSlate;
	}
	else if (CachedSnapshot.bIsWithinDirectorsCutWindow)
	{
		VM.MassStatusBadgeText = FText::FromString(TEXT("[* DIR.CUT]"));
		VM.MassStatusBadgeColor = Gold;
	}
	else if (CachedSnapshot.bIsWithinQuotaWindow)
	{
		VM.MassStatusBadgeText = FText::FromString(TEXT("[VALID]"));
		VM.MassStatusBadgeColor = Emerald;
	}
	else
	{
		VM.MassStatusBadgeText = FText::FromString(TEXT("[UNDER]"));
		VM.MassStatusBadgeColor = Amber;
	}

	const float GaugeMaxKg = FMath::Max(1.0f, MaxValidKg * 1.15f);
	VM.NormalizedMassFill = FMath::Clamp(ShownKg / GaugeMaxKg, 0.0f, 1.0f);
	VM.NormalizedValidMin = FMath::Clamp(MinValidKg / GaugeMaxKg, 0.0f, 1.0f);
	VM.NormalizedValidMax = FMath::Clamp(MaxValidKg / GaugeMaxKg, 0.0f, 1.0f);
	VM.NormalizedDirCutMin = FMath::Clamp(MinDirKg / GaugeMaxKg, 0.0f, 1.0f);
	VM.NormalizedDirCutMax = FMath::Clamp(MaxDirKg / GaugeMaxKg, 0.0f, 1.0f);

	const float TareKg = FMath::Max(0.0f, CachedSnapshot.AccumulatedTareWasteKg);
	const float MaxTareKg = FMath::Max(0.0f, CachedSnapshot.MaxAllowedTareWasteKg);
	const bool bTareExceeded = TareKg > (MaxTareKg + QuotaEpsilonKg);
	VM.TareBadgeText = FText::FromString(FString::Printf(TEXT("TARE: %.1f / %.1f KG"), TareKg, MaxTareKg));
	VM.TareBadgeColor = bTareExceeded
		? FLinearColor(1.00f, 0.22f, 0.16f, 1.0f)
		: Amber;

	if (CachedSnapshot.ActiveDiverterRoute == EDiverterRoute::TareChute)
	{
		VM.RouteBadgeText = LOCTEXT("CrtRouteTare", "ROUTE: TARE [SPACE]");
		VM.RouteBadgeColor = Orange;
	}
	else
	{
		VM.RouteBadgeText = LOCTEXT("CrtRouteTithe", "ROUTE: TITHE [SPACE]");
		VM.RouteBadgeColor = FLinearColor(0.05f, 0.90f, 1.00f, 1.0f);
	}

	if (bFaultActive)
	{
		VM.OperationalStatusText = FormatFaultStampHeadline(LatchedFaultReason);
		VM.OperationalStatusColor = Crimson;
	}
	else if (CachedShiftPhase == EShiftPhase::BondCleared)
	{
		VM.OperationalStatusText = LOCTEXT("CrtStatusBondCleared", "BOND CLEARED - PRESS [E] OVERTIME | HOLD [R] RESET");
		VM.OperationalStatusColor = Gold;
	}
	else if (CachedShiftPhase == EShiftPhase::ShiftCompleted)
	{
		VM.OperationalStatusText = LOCTEXT("CrtStatusShiftDone", "SHIFT COMPLETE - [E] NEXT SHIFT | [1-3] UPGRADES");
		VM.OperationalStatusColor = Emerald;
	}
	else if (CachedShiftPhase == EShiftPhase::ContractPassTransition)
	{
		if (Phase12ECfg.bEnableContractPassFastSkip)
		{
			VM.OperationalStatusText = CachedSnapshot.bLastStampWasDirectorsCut
				? LOCTEXT("CrtStatusDirPassSkip", "DIRECTOR'S CUT (+2) - HOISTING NEXT ([E] SKIP)")
				: LOCTEXT("CrtStatusStdPassSkip", "STAMPED - HOISTING NEXT ([E] SKIP)");
		}
		else
		{
			VM.OperationalStatusText = CachedSnapshot.bLastStampWasDirectorsCut
				? LOCTEXT("CrtStatusDirPass", "DIRECTOR'S CUT (+2 SCRIP) - ADVANCING...")
				: LOCTEXT("CrtStatusStdPass", "QUOTA STAMPED (+1 SCRIP) - ADVANCING...");
		}
		VM.OperationalStatusColor = CachedSnapshot.bLastStampWasDirectorsCut ? Gold : Emerald;
	}
	else if (bOvershoot)
	{
		VM.OperationalStatusText = LOCTEXT("CrtStatusOverQuota", "OVER QUOTA - HOLD [R] TO PURGE CELL");
		VM.OperationalStatusColor = Crimson;
	}
	else if (bPhysicallySettling)
	{
		VM.OperationalStatusText = LOCTEXT("CrtStatusSettling", "SETTLING...");
		VM.OperationalStatusColor = Gold;
	}
	else if (CachedSnapshot.bScaleSettledForStamp && CachedSnapshot.bIsWithinDirectorsCutWindow)
	{
		VM.OperationalStatusText = LOCTEXT("CrtStatusDirReady", "DIRECTOR'S CUT - PRESS [E] (+2 SCRIP)");
		VM.OperationalStatusColor = Gold;
	}
	else if (CachedSnapshot.bScaleSettledForStamp && CachedSnapshot.bIsWithinQuotaWindow)
	{
		VM.OperationalStatusText = LOCTEXT("CrtStatusQuotaReady", "QUOTA VALID - PRESS [E] TO STAMP");
		VM.OperationalStatusColor = Emerald;
	}
	else if (CachedSnapshot.ActiveDiverterRoute == EDiverterRoute::TareChute)
	{
		VM.OperationalStatusText = LOCTEXT("CrtStatusTareActive", "TARE ROUTE ACTIVE - [SPACE] FOR TITHE");
		VM.OperationalStatusColor = Orange;
	}
	else if (SettledKg > KINDA_SMALL_NUMBER && SettledKg < MinValidKg)
	{
		const float NeedToMinKg = FMath::Max(0.01f, MinValidKg - SettledKg);
		VM.OperationalStatusText = FText::FromString(
			FString::Printf(TEXT("[UNDER QUOTA] +%.2f KG NEEDED"), NeedToMinKg));
		VM.OperationalStatusColor = SkyBlue;
	}
	else
	{
		VM.OperationalStatusText = LOCTEXT("CrtStatusReady", "AWAITING TITHE CUT");
		VM.OperationalStatusColor = FLinearColor(0.45f, 0.84f, 1.00f, 0.86f);
	}

	const FString DiagJson = FString::Printf(
		TEXT("{\"header\":\"%s\",\"windows\":\"%s\",\"mass\":\"%s\",\"badge\":\"%s\",\"tare\":\"%s\",\"route\":\"%s\",\"status\":\"%s\"}"),
		*EscapeDiagJsonField(VM.ConsoleHeaderText.ToString()),
		*EscapeDiagJsonField(VM.WindowsSummaryText.ToString()),
		*EscapeDiagJsonField(VM.MassReadoutText.ToString()),
		*EscapeDiagJsonField(VM.MassStatusBadgeText.ToString()),
		*EscapeDiagJsonField(VM.TareBadgeText.ToString()),
		*EscapeDiagJsonField(VM.RouteBadgeText.ToString()),
		*EscapeDiagJsonField(VM.OperationalStatusText.ToString()));
	CVarDiagBottomCrtJson->Set(*DiagJson, ECVF_SetByCode);

	OverlayWidget->UpdateBottomCrtConsole(VM);
}

void UInspectorHudBridgeComponent::PushGraphicsQualityRow()
{
	if (!OverlayWidget.IsValid())
	{
		return;
	}

	const FPhase12EPresentationPolishConfig Phase12ECfg = ResolvePhase12EConfig();
	OverlayWidget->SetGraphicsQualityReadout(
		FormatGraphicsQualityReadout(CachedGraphicsQualityLevel, CachedTsrScalePercent),
		Phase12ECfg.bEnableControlsGraphicsRow);
}

bool UInspectorHudBridgeComponent::IsScaleVisuallySettling() const
{
	if (CachedShiftPhase != EShiftPhase::ActiveInspection)
	{
		return false;
	}

	const float SettledKg = FMath::Max(0.0f, CachedSnapshot.SettledScaleMassKg);
	const float TargetKg = FMath::Max(0.0f, CachedSnapshot.TargetTitheMassKg);
	const float TolKg = FMath::Max(0.0f, CachedSnapshot.QuotaToleranceKg);
	const float MaxValidKg = TargetKg + TolKg;
	const bool bOvershoot = SettledKg > (MaxValidKg + QuotaEpsilonKg);

	return (CachedSnapshot.InFlightMassKg > 0.01f) ||
		(SettledKg > 0.01f && !CachedSnapshot.bIsReceiverPhysicallySettled && !bOvershoot);
}

void UInspectorHudBridgeComponent::SnapRollingMassImmediate(float NewMassKg)
{
	TargetPanMassKg = FMath::Max(0.0f, NewMassKg);
	DisplayedPanMassKg = TargetPanMassKg;
	if (IsComponentTickEnabled())
	{
		SetComponentTickEnabled(false);
	}
}

void UInspectorHudBridgeComponent::SyncTargetPanMassFromSnapshot(bool bForceSnap)
{
	const FPhase12EPresentationPolishConfig Phase12ECfg = ResolvePhase12EConfig();
	const float AuthSettledKg = FMath::Max(0.0f, CachedSnapshot.SettledScaleMassKg);
	if (bForceSnap || !Phase12ECfg.bEnableDampedWeightReadout)
	{
		SnapRollingMassImmediate(AuthSettledKg);
		return;
	}

	TargetPanMassKg = AuthSettledKg;
	if (FMath::Abs(TargetPanMassKg - DisplayedPanMassKg) > 0.005f)
	{
		if (!IsComponentTickEnabled())
		{
			SetComponentTickEnabled(true);
		}
	}
	else
	{
		DisplayedPanMassKg = TargetPanMassKg;
		if (IsComponentTickEnabled())
		{
			SetComponentTickEnabled(false);
		}
	}
}

FPhase12EPresentationPolishConfig UInspectorHudBridgeComponent::ResolvePhase12EConfig() const
{
	return IsValid(PresentationData)
		? PresentationData->Phase12EPolishConfig.GetSanitized()
		: FPhase12EPresentationPolishConfig();
}

FText UInspectorHudBridgeComponent::FormatGraphicsQualityReadout(int32 QualityLevel, float TsrScalePercent)
{
	static const TCHAR* TierNames[4] = { TEXT("LOW"), TEXT("MED"), TEXT("HIGH"), TEXT("EPIC") };
	const int32 SafeTier = FMath::Clamp(QualityLevel, 0, 3);
	const int32 RoundedPct = FMath::Clamp(FMath::RoundToInt(TsrScalePercent), 50, 100);
	if (SafeTier == 3 && RoundedPct >= 100)
	{
		return FText::FromString(FString::Printf(TEXT("GFX: %s (%d%%)"), TierNames[SafeTier], RoundedPct));
	}
	return FText::FromString(FString::Printf(TEXT("GFX: %s (TSR %d%%)"), TierNames[SafeTier], RoundedPct));
}

FText UInspectorHudBridgeComponent::FormatFaultStampHeadline(EContractFaultReason FaultReason)
{
	switch (FaultReason)
	{
	case EContractFaultReason::ClampStructuralSnap:
		return LOCTEXT("FaultClampSheared", "FAULT: CLAMP SHEARED");
	case EContractFaultReason::ScaleContaminated:
		return LOCTEXT("FaultContaminatedTithe", "FAULT: CONTAMINATED TITHE");
	case EContractFaultReason::VolatileDetonation:
		return LOCTEXT("FaultIsotopeDetonated", "FAULT: ISOTOPE DETONATED");
	case EContractFaultReason::QuotaOvershoot:
		return LOCTEXT("FaultQuotaOvershoot", "FAULT: QUOTA OVERSHOOT");
	case EContractFaultReason::TareAllowanceExceeded:
		return LOCTEXT("FaultTareExceeded", "FAULT: TARE EXCEEDED");
	case EContractFaultReason::MathematicallyUnreachable:
		return LOCTEXT("FaultUnreachableQuota", "FAULT: UNREACHABLE QUOTA");
	case EContractFaultReason::ManualOperatorReset:
		return LOCTEXT("FaultManualReset", "CELL PURGED BY OPERATOR #409");
	default:
		return LOCTEXT("FaultGeneric", "FAULT: INSPECTION ABORTED");
	}
}

FString UInspectorHudBridgeComponent::AlertStateToDiagnosticString(EHudCenterAlertState State)
{
	switch (State)
	{
	case EHudCenterAlertState::None:
		return TEXT("None");
	case EHudCenterAlertState::P0_FaultStamp:
		return TEXT("P0_FaultStamp");
	case EHudCenterAlertState::P0_PassStamp:
		return TEXT("P0_PassStamp");
	case EHudCenterAlertState::P0_ShiftSummary:
		return TEXT("P0_ShiftSummary");
	case EHudCenterAlertState::P0_BondCleared:
		return TEXT("P0_BondCleared");
	case EHudCenterAlertState::P1_IsotopeHazard:
		return TEXT("P1_IsotopeHazard");
	case EHudCenterAlertState::P1_VeinInDrop:
		return TEXT("P1_VeinInDrop");
	case EHudCenterAlertState::P1_ClampShearCritical:
		return TEXT("P1_ClampShearCritical");
	case EHudCenterAlertState::P2_DirectorsCutReady:
		return TEXT("P2_DirectorsCutReady");
	case EHudCenterAlertState::P2_QuotaMetReady:
		return TEXT("P2_QuotaMetReady");
	case EHudCenterAlertState::P3_TutorialHint:
		return TEXT("P3_TutorialHint");
	default:
		return TEXT("Unknown");
	}
}

bool UInspectorHudBridgeComponent::RequestCompositedViewportScreenshot(const FString& AbsoluteFilePath)
{
	if (AbsoluteFilePath.IsEmpty())
	{
		return false;
	}

	const FString DirectoryPath = FPaths::GetPath(AbsoluteFilePath);
	if (!DirectoryPath.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*DirectoryPath, true);
	}

	// Decision 2: Capture both the HWRT 3D cell and post-tonemap pure C++ Slate HUD in one composited frame.
	FScreenshotRequest::RequestScreenshot(AbsoluteFilePath, /*bShowUI=*/true, /*bAddFilenameSuffix=*/false);
	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* ViewportClient = World->GetGameViewport())
		{
			if (FViewport* Viewport = ViewportClient->Viewport)
			{
				Viewport->Invalidate();
			}
		}
	}
	return true;
}

bool UInspectorHudBridgeComponent::RequestCleanLitViewportScreenshot(const FString& AbsoluteFilePath)
{
	if (AbsoluteFilePath.IsEmpty())
	{
		return false;
	}

	const FString DirectoryPath = FPaths::GetPath(AbsoluteFilePath);
	if (!DirectoryPath.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*DirectoryPath, true);
	}

	// Capture pure 3D GameViewport RenderTarget (bShowUI=false) with full Lumen reflections & TSR and zero Slate/window chrome.
	FScreenshotRequest::RequestScreenshot(AbsoluteFilePath, /*bShowUI=*/false, /*bAddFilenameSuffix=*/false);
	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* ViewportClient = World->GetGameViewport())
		{
			if (FViewport* Viewport = ViewportClient->Viewport)
			{
				Viewport->Invalidate();
			}
		}
	}
	return true;
}

FVector2D UInspectorHudBridgeComponent::GetLastLocalViewportSize() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().LocalViewportSize : FVector2D::ZeroVector;
}

float UInspectorHudBridgeComponent::GetLastEffectiveCornerPanelWidth() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().EffectiveCornerPanelWidth : 0.0f;
}

float UInspectorHudBridgeComponent::GetLastEffectiveBottomConsoleWidth() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().EffectiveBottomConsoleWidth : 0.0f;
}

FVector2D UInspectorHudBridgeComponent::GetTopLeftPanelBoundsMin() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().TopLeftMin : FVector2D::ZeroVector;
}

FVector2D UInspectorHudBridgeComponent::GetTopLeftPanelBoundsMax() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().TopLeftMax : FVector2D::ZeroVector;
}

FVector2D UInspectorHudBridgeComponent::GetTopRightPanelBoundsMin() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().TopRightMin : FVector2D::ZeroVector;
}

FVector2D UInspectorHudBridgeComponent::GetTopRightPanelBoundsMax() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().TopRightMax : FVector2D::ZeroVector;
}

FVector2D UInspectorHudBridgeComponent::GetCenterBannerBoundsMin() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().CenterBannerMin : FVector2D::ZeroVector;
}

FVector2D UInspectorHudBridgeComponent::GetCenterBannerBoundsMax() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().CenterBannerMax : FVector2D::ZeroVector;
}

FVector2D UInspectorHudBridgeComponent::GetReticleCardBoundsMin() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().ReticleCardMin : FVector2D::ZeroVector;
}

FVector2D UInspectorHudBridgeComponent::GetReticleCardBoundsMax() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().ReticleCardMax : FVector2D::ZeroVector;
}

FVector2D UInspectorHudBridgeComponent::GetBottomConsoleBoundsMin() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().BottomConsoleMin : FVector2D::ZeroVector;
}

FVector2D UInspectorHudBridgeComponent::GetBottomConsoleBoundsMax() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().BottomConsoleMax : FVector2D::ZeroVector;
}

FString UInspectorHudBridgeComponent::GetActiveCenterAlertStateName() const
{
	return AlertStateToDiagnosticString(ActiveCenterAlertState);
}

bool UInspectorHudBridgeComponent::IsCenterAlertPulseTimerActive() const
{
	return OverlayWidget.IsValid() && OverlayWidget->IsCenterAlertPulseTimerActive();
}

float UInspectorHudBridgeComponent::GetCenterAlertBgOpacity() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetCenterAlertCurrentBgOpacity() : 0.0f;
}

float UInspectorHudBridgeComponent::GetCenterAlertTextOpacity() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetCenterAlertCurrentTextOpacity() : 0.0f;
}

float UInspectorHudBridgeComponent::GetTopLeftContentDesiredWidth() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().TopLeftDesiredWidth : 0.0f;
}

float UInspectorHudBridgeComponent::GetTopRightContentDesiredWidth() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().TopRightDesiredWidth : 0.0f;
}

float UInspectorHudBridgeComponent::GetTopLeftHeaderBottomLocalY() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().TopLeftHeaderBottomLocalY : 0.0f;
}

float UInspectorHudBridgeComponent::GetTopLeftDividerLocalY() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().TopLeftDividerLocalY : 0.0f;
}

float UInspectorHudBridgeComponent::GetTopRightHeaderBottomLocalY() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().TopRightHeaderBottomLocalY : 0.0f;
}

float UInspectorHudBridgeComponent::GetTopRightDividerLocalY() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().TopRightDividerLocalY : 0.0f;
}

void UInspectorHudBridgeComponent::InjectRollWheelForTest(float WheelSteps, bool bFastSnap)
{
	if (IsValid(BoundLaserAimComp))
	{
		BoundLaserAimComp->AddScrollWheelInput(WheelSteps, bFastSnap);
	}
}

void UInspectorHudBridgeComponent::OverrideCornerWidthForTest(float TestCornerWidthPx, bool bEnableOverride)
{
	FHudPresentationConfig EffectiveCfg = IsValid(PresentationData)
		? PresentationData->HudConfig
		: FHudPresentationConfig();
	if (bEnableOverride)
	{
		EffectiveCfg.CornerPanelWidthPx = FMath::Clamp(TestCornerWidthPx, 220.0f, 680.0f);
	}
	ActiveHudConfig = EffectiveCfg;
	if (OverlayWidget.IsValid())
	{
		OverlayWidget->ApplyHudConfig(ActiveHudConfig);
		PushAllPanels();
	}
}

void UInspectorHudBridgeComponent::ToggleControlsCardVisibility()
{
	SetControlsCardExpanded(!bControlsCardExpanded);
}

void UInspectorHudBridgeComponent::SetControlsCardExpanded(bool bExpanded)
{
	bControlsCardExpanded = bExpanded;
	if (OverlayWidget.IsValid())
	{
		OverlayWidget->SetControlsCardExpanded(bControlsCardExpanded);
		PushGraphicsQualityRow();
		PushReticleCard();
	}
}

FVector2D UInspectorHudBridgeComponent::GetControlsCardBoundsMin() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().ControlsCardMin : FVector2D::ZeroVector;
}

FVector2D UInspectorHudBridgeComponent::GetControlsCardBoundsMax() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().ControlsCardMax : FVector2D::ZeroVector;
}

float UInspectorHudBridgeComponent::GetLastEffectiveControlsCardWidth() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().EffectiveControlsCardWidth : 0.0f;
}

float UInspectorHudBridgeComponent::GetBottomConsoleMassBottomLocalY() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().BottomConsoleMassBottomLocalY : 0.0f;
}

float UInspectorHudBridgeComponent::GetBottomConsoleAnalogBarTopLocalY() const
{
	return OverlayWidget.IsValid() ? OverlayWidget->GetResolvedLayoutTelemetry().BottomConsoleAnalogBarTopLocalY : 0.0f;
}

void UInspectorHudBridgeComponent::SetLaserChargeForTest(float NormalizedCharge)
{
	CachedLaserCharge = FMath::Clamp(NormalizedCharge, 0.0f, 1.0f);
	PushReticleCard();
}

void UInspectorHudBridgeComponent::OverrideViewportSizeForTest(FVector2D TestLocalViewportSize, bool bEnableOverride)
{
	if (OverlayWidget.IsValid())
	{
		OverlayWidget->SetViewportSizeOverrideForTest(TestLocalViewportSize, bEnableOverride);
		PushAllPanels();
	}
}

#undef LOCTEXT_NAMESPACE
