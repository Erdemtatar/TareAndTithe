// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/InspectionShiftSubsystem.h"
#include "TareAndTithe.h"
#include "Actors/SeveredChunkActor.h"
#include "Components/AudioTelemetryBridgeComponent.h"
#include "Data/MonolithContractDataAsset.h"
#include "Data/ShiftProgressionDataAsset.h"
#include "Data/TareTithePresentationDataAsset.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Interfaces/MassPayloadInterface.h"
#include "Misc/PackageName.h"
#include "Subsystems/ChunkPoolSubsystem.h"
#include "TimerManager.h"

namespace
{
	constexpr float DefaultTransitionDelaySeconds = 1.8f;
	constexpr float DefaultContractBriefDurationSeconds = 3.5f;
	constexpr float RequisitionFeedbackDisplaySeconds = 2.2f;
	constexpr float DefaultTightTorqueBriefThresholdNm = 29.0f;
	constexpr float QuotaBoundaryFloatEpsilonKg = 1.0e-3f;
	constexpr float WatchdogActiveFallVelocityThresholdZCmPerSec = -25.0f;
	constexpr float WatchdogActiveFallMinCellFloorZCm = -75.0f;
}

bool UInspectionShiftSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	const UWorld* World = IsValid(Outer) ? Outer->GetWorld() : nullptr;
	return IsValid(World) && World->IsGameWorld();
}

void UInspectionShiftSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CurrentPhase = EShiftPhase::WaitingForStations;
	RegisterTelemetryPublisher(this, this);
}

void UInspectionShiftSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bWorldHasBegunPlay = true;

	if (UChunkPoolSubsystem* PoolSubsystem = InWorld.GetSubsystem<UChunkPoolSubsystem>())
	{
		PoolSubsystem->OnAnyChunkWatchdogExpired.RemoveAll(this);
		PoolSubsystem->OnAnyChunkWatchdogExpired.AddUObject(this, &UInspectionShiftSubsystem::HandleChunkWatchdogExpired);
	}

	TryAutoStartShiftIfReady();
}

void UInspectionShiftSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TransitionTimerHandle);
		World->GetTimerManager().ClearTimer(ContractBriefTimerHandle);
		World->GetTimerManager().ClearTimer(RequisitionFeedbackTimerHandle);
		if (UChunkPoolSubsystem* PoolSubsystem = World->GetSubsystem<UChunkPoolSubsystem>())
		{
			PoolSubsystem->OnAnyChunkWatchdogExpired.RemoveAll(this);
		}
	}
	bIsContractBriefActive = false;
	TransientRequisitionFeedback.Empty();
	bLastRequisitionSucceeded = false;

	UnregisterTelemetryPublisher(this);
	AudioBridges.Reset();
	TelemetryPublishers.Reset();
	ActiveInFlightPayloads.Reset();
	AccountedTarePayloads.Reset();
	RigBinding = FShiftSliceableRigBinding();
	ReceiverBinding = FShiftQuotaReceiverBinding();

	Super::Deinitialize();
}

void UInspectionShiftSubsystem::SetPresentationAsset(const UTareTithePresentationDataAsset* InPresentationAsset)
{
	if (ActivePresentationAsset == InPresentationAsset)
	{
		return;
	}

	ActivePresentationAsset = InPresentationAsset;

	if (IsValid(ActivePresentationAsset) &&
		IsValid(ActivePresentationAsset->ShiftProgressionOverride) &&
		!IsValid(ActiveProgressionAsset))
	{
		SetShiftProgressionAsset(ActivePresentationAsset->ShiftProgressionOverride);
	}

	OnPresentationAssetChanged.Broadcast(ActivePresentationAsset);
}

void UInspectionShiftSubsystem::SetShiftProgressionAsset(const UShiftProgressionDataAsset* InProgressionAsset)
{
	if (!IsValid(InProgressionAsset))
	{
		return;
	}

	const bool bChangedProgression = (ActiveProgressionAsset != InProgressionAsset);
	ActiveProgressionAsset = InProgressionAsset;

	if (bChangedProgression && bWorldHasBegunPlay && HasRegisteredSliceableRig() && HasRegisteredQuotaReceiver())
	{
		StartShiftFromContract(0);
	}
}

int32 UInspectionShiftSubsystem::GetTotalContractCount() const
{
	return IsValid(ActiveProgressionAsset) ? ActiveProgressionAsset->OrderedContracts.Num() : 0;
}

float UInspectionShiftSubsystem::GetEffectiveQuotaToleranceKg(const UMonolithContractDataAsset* Contract) const
{
	if (!IsValid(Contract))
	{
		return 0.5f;
	}

	const float ShiftScale = FMath::Max(
		0.60f,
		FMath::Pow(0.88f, static_cast<float>(FMath::Max(0, CurrentShiftNumber - 1))));
	return FMath::Max(0.20f, Contract->QuotaToleranceKg * ShiftScale);
}

float UInspectionShiftSubsystem::GetDirectorsCutToleranceKg(float EffectiveToleranceKg) const
{
	return FMath::Max(0.15f, EffectiveToleranceKg * 0.25f);
}

float UInspectionShiftSubsystem::GetEffectiveMaxTareWasteKg(const UMonolithContractDataAsset* Contract) const
{
	const float BaseTareKg = IsValid(Contract) ? Contract->MaxAllowedTareWasteKg : 15.0f;
	return BaseTareKg + (5.0f * static_cast<float>(TareHopperLevel));
}

int32 UInspectionShiftSubsystem::GetRequisitionNextCost(uint8 CurrentLevel)
{
	return (CurrentLevel >= 3) ? 0 : (static_cast<int32>(CurrentLevel) + 2);
}

void UInspectionShiftSubsystem::RegisterSliceableRig(const FShiftSliceableRigBinding& InBinding)
{
	if (!IsValid(InBinding.SliceableTarget.GetObject()))
	{
		return;
	}

	RigBinding = InBinding;
	TryAutoStartShiftIfReady();
}

void UInspectionShiftSubsystem::UnregisterSliceableRig(UObject* RigObject)
{
	if (RigBinding.SliceableTarget.GetObject() == RigObject)
	{
		RigBinding = FShiftSliceableRigBinding();
	}
}

void UInspectionShiftSubsystem::RegisterQuotaReceiver(const FShiftQuotaReceiverBinding& InBinding)
{
	if (!IsValid(InBinding.QuotaReceiver.GetObject()))
	{
		return;
	}

	ReceiverBinding = InBinding;
	TryAutoStartShiftIfReady();
}

void UInspectionShiftSubsystem::UnregisterQuotaReceiver(UObject* ReceiverObject)
{
	if (ReceiverBinding.QuotaReceiver.GetObject() == ReceiverObject)
	{
		ReceiverBinding = FShiftQuotaReceiverBinding();
	}
}

void UInspectionShiftSubsystem::RegisterStrayCatchStation(UObject* StationObject)
{
	if (IsValid(StationObject))
	{
		RegisteredStrayCatchStations.AddUnique(StationObject);
	}
}

void UInspectionShiftSubsystem::UnregisterStrayCatchStation(UObject* StationObject)
{
	RegisteredStrayCatchStations.RemoveAllSwap([StationObject](const TWeakObjectPtr<UObject>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == StationObject;
	}, EAllowShrinking::No);
}

bool UInspectionShiftSubsystem::HasRegisteredStrayCatchStation() const
{
	for (const TWeakObjectPtr<UObject>& Entry : RegisteredStrayCatchStations)
	{
		if (Entry.IsValid())
		{
			return true;
		}
	}
	return false;
}

void UInspectionShiftSubsystem::RegisterDirectorBoothStation(UObject* StationObject)
{
	if (IsValid(StationObject))
	{
		RegisteredDirectorBoothStations.AddUnique(StationObject);
	}
}

void UInspectionShiftSubsystem::UnregisterDirectorBoothStation(UObject* StationObject)
{
	RegisteredDirectorBoothStations.RemoveAllSwap([StationObject](const TWeakObjectPtr<UObject>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == StationObject;
	}, EAllowShrinking::No);
}

bool UInspectionShiftSubsystem::HasRegisteredDirectorBoothStation() const
{
	for (const TWeakObjectPtr<UObject>& Entry : RegisteredDirectorBoothStations)
	{
		if (Entry.IsValid())
		{
			return true;
		}
	}
	return false;
}

void UInspectionShiftSubsystem::RegisterTelemetryPublisher(
	ITelemetryPublisherInterface* Publisher,
	UObject* LifetimeOwner)
{
	if (Publisher == nullptr || !IsValid(LifetimeOwner))
	{
		return;
	}

	for (const FRegisteredTelemetryPublisherEntry& Existing : TelemetryPublishers)
	{
		if (Existing.Publisher == Publisher)
		{
			return;
		}
	}

	FRegisteredTelemetryPublisherEntry Entry;
	Entry.Publisher = Publisher;
	Entry.LifetimeOwner = LifetimeOwner;
	TelemetryPublishers.Add(Entry);

	if (Publisher != static_cast<ITelemetryPublisherInterface*>(this))
	{
		Publisher->GetTelemetryStreamDelegate().AddUObject(this, &UInspectionShiftSubsystem::HandlePublisherTelemetryStream);
	}

	for (int32 Idx = AudioBridges.Num() - 1; Idx >= 0; --Idx)
	{
		UAudioTelemetryBridgeComponent* Bridge = AudioBridges[Idx].Get();
		if (!IsValid(Bridge))
		{
			AudioBridges.RemoveAtSwap(Idx, EAllowShrinking::No);
			continue;
		}
		Bridge->RegisterPublisher(Publisher, LifetimeOwner);
	}
}

void UInspectionShiftSubsystem::UnregisterTelemetryPublisher(ITelemetryPublisherInterface* Publisher)
{
	if (Publisher == nullptr)
	{
		return;
	}

	if (Publisher != static_cast<ITelemetryPublisherInterface*>(this))
	{
		Publisher->GetTelemetryStreamDelegate().RemoveAll(this);
	}

	for (int32 Idx = AudioBridges.Num() - 1; Idx >= 0; --Idx)
	{
		if (UAudioTelemetryBridgeComponent* Bridge = AudioBridges[Idx].Get())
		{
			Bridge->UnregisterPublisher(Publisher);
		}
	}

	TelemetryPublishers.RemoveAllSwap([Publisher](const FRegisteredTelemetryPublisherEntry& Entry)
	{
		return Entry.Publisher == Publisher || !Entry.LifetimeOwner.IsValid();
	}, EAllowShrinking::No);
}

void UInspectionShiftSubsystem::RegisterAudioTelemetryBridge(UAudioTelemetryBridgeComponent* AudioBridge)
{
	if (!IsValid(AudioBridge))
	{
		return;
	}

	AudioBridges.AddUnique(AudioBridge);

	for (int32 Idx = TelemetryPublishers.Num() - 1; Idx >= 0; --Idx)
	{
		const FRegisteredTelemetryPublisherEntry& PubEntry = TelemetryPublishers[Idx];
		if (PubEntry.Publisher == nullptr || !PubEntry.LifetimeOwner.IsValid())
		{
			TelemetryPublishers.RemoveAtSwap(Idx, EAllowShrinking::No);
			continue;
		}
		AudioBridge->RegisterPublisher(PubEntry.Publisher, PubEntry.LifetimeOwner.Get());
	}
}

void UInspectionShiftSubsystem::UnregisterAudioTelemetryBridge(UAudioTelemetryBridgeComponent* AudioBridge)
{
	AudioBridges.RemoveAllSwap([AudioBridge](const TWeakObjectPtr<UAudioTelemetryBridgeComponent>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == AudioBridge;
	}, EAllowShrinking::No);
}

void UInspectionShiftSubsystem::NotifyChunkSpawnedInFlight(AActor* PayloadActor)
{
	if (!IsValid(PayloadActor) || !PayloadActor->Implements<UMassPayloadInterface>())
	{
		return;
	}

	// Clear any prior pool-cycle tare record for this recycled actor before tracking the new drop.
	AccountedTarePayloads.Remove(PayloadActor);
	ActiveInFlightPayloads.Add(PayloadActor);

	RecomputeInFlightMassFromSet();
	RefreshAndBroadcastSnapshot();
}

void UInspectionShiftSubsystem::NotifySliverVaporizedAsTare(float VaporizedMassKg, const FVector& /*WorldLocation*/)
{
	if (CurrentPhase != EShiftPhase::ActiveInspection)
	{
		return;
	}

	VaporizedTareMassKg += FMath::Max(0.0f, VaporizedMassKg);
	AccumulatedTareWasteKg = VaporizedTareMassKg + DisposedChunksTareMassKg;

	RefreshAndBroadcastSnapshot();
	EvaluateContractHealth();
}

void UInspectionShiftSubsystem::NotifySliceCommittedSummary(const FSliceResultSummary& SliceSummary)
{
	if (CurrentPhase != EShiftPhase::ActiveInspection)
	{
		return;
	}

	OnSliceCommittedSummary.Broadcast(SliceSummary);

	if (SliceSummary.Outcome == ECommitSliceOutcome::DetonatedVein)
	{
		TriggerContractFaultAtLocation(EContractFaultReason::VolatileDetonation, SliceSummary.WorldCutCentroid);
		return;
	}

	if (SliceSummary.Outcome == ECommitSliceOutcome::Success)
	{
		RemainderMassKg = FMath::Max(0.0f, SliceSummary.NewRemainderMassKg);
		RecomputeInFlightMassFromSet();
		RefreshAndBroadcastSnapshot();
		EvaluateContractHealth();
	}
}

void UInspectionShiftSubsystem::NotifyPayloadLandedOnScale(AActor* PayloadActor)
{
	if (IsValid(PayloadActor))
	{
		ActiveInFlightPayloads.Remove(PayloadActor);
	}

	RecomputeInFlightMassFromSet();
	RefreshAndBroadcastSnapshot();
	EvaluateContractHealth();
}

void UInspectionShiftSubsystem::NotifyPayloadBouncedOffScale(AActor* PayloadActor)
{
	if (IsValid(PayloadActor) &&
		PayloadActor->Implements<UMassPayloadInterface>() &&
		IMassPayloadInterface::Execute_GetRoutingState(PayloadActor) == EChunkRoutingState::InFlight)
	{
		ActiveInFlightPayloads.Add(PayloadActor);
	}

	RecomputeInFlightMassFromSet();
	RefreshAndBroadcastSnapshot();
	EvaluateContractHealth();
}

void UInspectionShiftSubsystem::NotifyPayloadDisposedToTare(AActor* PayloadActor, float ExplicitMassKg)
{
	AccountPayloadAsTareOnce(PayloadActor, ExplicitMassKg);
}

void UInspectionShiftSubsystem::NotifyPayloadLostAsStrayTare(AActor* PayloadActor, float ExplicitMassKg)
{
	AccountPayloadAsTareOnce(PayloadActor, ExplicitMassKg);
}

void UInspectionShiftSubsystem::NotifyScaleContaminationDetected(AActor* /*ContaminatedActor*/)
{
	if (CurrentPhase == EShiftPhase::ActiveInspection)
	{
		TriggerContractFault(EContractFaultReason::ScaleContaminated);
	}
}

void UInspectionShiftSubsystem::NotifyScaleSettledStateChanged(bool /*bIsSettled*/, float /*SettledMassKg*/)
{
	if (CurrentPhase != EShiftPhase::ActiveInspection)
	{
		return;
	}

	RecomputeInFlightMassFromSet();
	RefreshAndBroadcastSnapshot();
	EvaluateContractHealth();
}

bool UInspectionShiftSubsystem::StartShiftFromContract(int32 StartContractIndex)
{
	if (!HasRegisteredSliceableRig() || !HasRegisteredQuotaReceiver())
	{
		return false;
	}

	if (!EnsureShiftProgressionInitialized())
	{
		return false;
	}

	const int32 TotalCount = GetTotalContractCount();
	if (TotalCount <= 0)
	{
		return false;
	}

	const int32 ClampedIndex = FMath::Clamp(StartContractIndex, 0, TotalCount - 1);
	return LoadAndBeginContract(ClampedIndex);
}

bool UInspectionShiftSubsystem::RequestToggleDiverter()
{
	if (CurrentPhase != EShiftPhase::ActiveInspection)
	{
		return false;
	}

	if (!ReceiverBinding.OnToggleDiverter.IsBound())
	{
		return false;
	}

	const bool bToggled = ReceiverBinding.OnToggleDiverter.Execute();
	if (bToggled)
	{
		RefreshAndBroadcastSnapshot();
	}
	return bToggled;
}

bool UInspectionShiftSubsystem::TryStampQuotaVerdict()
{
	if (CurrentPhase != EShiftPhase::ActiveInspection || !IsValid(ActiveContract))
	{
		return false;
	}

	ReceiverBinding.OnReconcileOverlaps.ExecuteIfBound();
	RecomputeInFlightMassFromSet();
	if (ActiveInFlightPayloads.Num() > 0)
	{
		return false;
	}

	UObject* ReceiverObj = ReceiverBinding.QuotaReceiver.GetObject();
	if (!IsValid(ReceiverObj))
	{
		return false;
	}

	if (!IQuotaReceiverInterface::Execute_IsReceiverSettled(ReceiverObj))
	{
		return false;
	}

	if (IQuotaReceiverInterface::Execute_HasContaminationFault(ReceiverObj))
	{
		TriggerContractFault(EContractFaultReason::ScaleContaminated);
		return false;
	}

	const float EffectiveToleranceKg = GetEffectiveQuotaToleranceKg(ActiveContract);
	const float SettledMassKg = IQuotaReceiverInterface::Execute_GetSettledMassKg(ReceiverObj);
	const float LowerBoundKg = ActiveContract->TargetTitheMassKg - EffectiveToleranceKg - QuotaBoundaryFloatEpsilonKg;
	const float UpperBoundKg = ActiveContract->TargetTitheMassKg + EffectiveToleranceKg + QuotaBoundaryFloatEpsilonKg;

	if (SettledMassKg < LowerBoundKg || SettledMassKg > UpperBoundKg)
	{
		return false;
	}

	const float DirectorsCutTolKg = GetDirectorsCutToleranceKg(EffectiveToleranceKg);
	const bool bPrecisionStamp = FMath::Abs(SettledMassKg - ActiveContract->TargetTitheMassKg) <= (DirectorsCutTolKg + QuotaBoundaryFloatEpsilonKg);
	const int32 AwardedScrip = bPrecisionStamp ? 2 : 1;
	ScripBalance += AwardedScrip;
	LastStampScripAwarded = AwardedScrip;
	LastStampedDeviationKg = SettledMassKg - ActiveContract->TargetTitheMassKg;
	bLastStampWasDirectorsCut = bPrecisionStamp;
	ClearTransientRequisitionFeedback();

	OnTelemetryImpulse.Broadcast(ETelemetryImpulseId::VerdictStamped, 1.0f, QueryReceiverLocationOrZero());

	bIsContractBriefActive = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ContractBriefTimerHandle);
	}

	const int32 TotalContracts = GetTotalContractCount();
	if (CurrentContractIndex + 1 < TotalContracts)
	{
		const FPhase12EPresentationPolishConfig Phase12ECfg = IsValid(ActivePresentationAsset)
			? ActivePresentationAsset->Phase12EPolishConfig.GetSanitized()
			: FPhase12EPresentationPolishConfig();

		if (const UWorld* World = GetWorld())
		{
			ContractPassEnteredWorldTimeSec = World->GetTimeSeconds();
		}
		else
		{
			ContractPassEnteredWorldTimeSec = 0.0;
		}

		SetShiftPhase(EShiftPhase::ContractPassTransition);
		RefreshAndBroadcastSnapshot();
		OnShiftVerdict.Broadcast(
			bPrecisionStamp ? EShiftVerdictType::DirectorsCutPassed : EShiftVerdictType::ContractPassed,
			EContractFaultReason::None);

		if (UWorld* World = GetWorld())
		{
			const float FallbackDelaySeconds = IsValid(ActiveProgressionAsset)
				? FMath::Max(0.2f, ActiveProgressionAsset->FaultAutoResetDelaySeconds)
				: DefaultTransitionDelaySeconds;
			const float DelaySeconds = Phase12ECfg.bEnableSnappyContractTransition
				? Phase12ECfg.ContractPassTransitionDurationSeconds
				: FallbackDelaySeconds;

			World->GetTimerManager().SetTimer(
				TransitionTimerHandle,
				this,
				&UInspectionShiftSubsystem::HandleContractPassTimerElapsed,
				DelaySeconds,
				false);
		}
	}
	else if (CurrentShiftNumber == TargetBondShifts)
	{
		SetShiftPhase(EShiftPhase::BondCleared);
		RefreshAndBroadcastSnapshot();
		OnShiftVerdict.Broadcast(EShiftVerdictType::BondCleared, EContractFaultReason::None);
	}
	else
	{
		SetShiftPhase(EShiftPhase::ShiftCompleted);
		RefreshAndBroadcastSnapshot();
		OnShiftVerdict.Broadcast(EShiftVerdictType::ShiftCompleted, EContractFaultReason::None);
	}

	return true;
}

bool UInspectionShiftSubsystem::TryConfirmOrStampAction()
{
	if (CurrentPhase == EShiftPhase::ActiveInspection)
	{
		return TryStampQuotaVerdict();
	}

	if (CurrentPhase == EShiftPhase::ContractPassTransition)
	{
		const FPhase12EPresentationPolishConfig Phase12ECfg = IsValid(ActivePresentationAsset)
			? ActivePresentationAsset->Phase12EPolishConfig.GetSanitized()
			: FPhase12EPresentationPolishConfig();
		if (Phase12ECfg.bEnableContractPassFastSkip)
		{
			if (UWorld* World = GetWorld())
			{
				const double ElapsedSec = World->GetTimeSeconds() - ContractPassEnteredWorldTimeSec;
				if (ElapsedSec >= static_cast<double>(Phase12ECfg.ContractPassFastSkipDebounceSeconds))
				{
					World->GetTimerManager().ClearTimer(TransitionTimerHandle);
					HandleContractPassTimerElapsed();
					return true;
				}
			}
		}
		return false;
	}

	if (CurrentPhase == EShiftPhase::ShiftCompleted || CurrentPhase == EShiftPhase::BondCleared)
	{
		++CurrentShiftNumber;
		LastStampScripAwarded = 0;
		LastStampedDeviationKg = 0.0f;
		bLastStampWasDirectorsCut = false;
		ClearTransientRequisitionFeedback();
		OnTelemetryImpulse.Broadcast(ETelemetryImpulseId::VerdictStamped, 0.90f, QueryReceiverLocationOrZero());
		return StartShiftFromContract(0);
	}

	return false;
}

bool UInspectionShiftSubsystem::TryPurchaseRequisition(ERequisitionTrack Track)
{
	if (CurrentPhase == EShiftPhase::WaitingForStations || CurrentPhase == EShiftPhase::ContractIntro)
	{
		return false;
	}

	if (CurrentPhase == EShiftPhase::FaultResetDelay)
	{
		SetTransientRequisitionFeedback(TEXT("FAULT LOCKOUT - WAIT FOR CELL RESET"), false);
		RefreshAndBroadcastSnapshot();
		return false;
	}

	uint8* TargetLevelPtr = nullptr;
	const TCHAR* TrackName = TEXT("REQUISITION");
	switch (Track)
	{
	case ERequisitionTrack::ClampHydraulics:
		TargetLevelPtr = &ClampHydraulicsLevel;
		TrackName = TEXT("[1] HYDRAULICS");
		break;
	case ERequisitionTrack::TareHopper:
		TargetLevelPtr = &TareHopperLevel;
		TrackName = TEXT("[2] TARE HOPPER");
		break;
	case ERequisitionTrack::VeinScannerDamper:
		TargetLevelPtr = &VeinScannerDamperLevel;
		TrackName = TEXT("[3] SCAN+DAMPER");
		break;
	default:
		return false;
	}

	if (*TargetLevelPtr >= 3)
	{
		SetTransientRequisitionFeedback(
			FString::Printf(TEXT("REQUISITION AT MAX LEVEL (%s LV.3)"), TrackName),
			false);
		RefreshAndBroadcastSnapshot();
		return false;
	}

	const int32 Cost = GetRequisitionNextCost(*TargetLevelPtr);
	if (ScripBalance < Cost)
	{
		SetTransientRequisitionFeedback(
			FString::Printf(TEXT("INSUFFICIENT SCRIP (NEED %d, HAVE %d)"), Cost, ScripBalance),
			false);
		RefreshAndBroadcastSnapshot();
		return false;
	}

	ScripBalance -= Cost;
	++(*TargetLevelPtr);
	const int32 NewLv = static_cast<int32>(*TargetLevelPtr);

	FString ApprovalMessage;
	switch (Track)
	{
	case ERequisitionTrack::ClampHydraulics:
		ApprovalMessage = FString::Printf(TEXT("REQUISITION APPROVED: [1] HYDRAULICS LV.%d (+%d%% TORQUE)"), NewLv, NewLv * 25);
		break;
	case ERequisitionTrack::TareHopper:
		ApprovalMessage = FString::Printf(TEXT("REQUISITION APPROVED: [2] TARE HOPPER LV.%d (+%d KG)"), NewLv, NewLv * 5);
		break;
	case ERequisitionTrack::VeinScannerDamper:
		ApprovalMessage = FString::Printf(TEXT("REQUISITION APPROVED: [3] SCAN+DAMPER LV.%d (+%d%% SCAN)"), NewLv, NewLv * 30);
		break;
	}

	SetTransientRequisitionFeedback(ApprovalMessage, true);
	OnTelemetryImpulse.Broadcast(ETelemetryImpulseId::DiverterToggled, 0.85f, QueryReceiverLocationOrZero());
	RefreshAndBroadcastSnapshot();

	if (CurrentPhase == EShiftPhase::ActiveInspection)
	{
		EvaluateContractHealth();
	}

	return true;
}

void UInspectionShiftSubsystem::RequestManualContractReset()
{
	// Anti-farming guard: mid-shift HOLD [R] resets ONLY the active contract (0 Scrip earned; preserves Shift, Scrip, Upgrades).
	// At ShiftCompleted or BondCleared, HOLD [R] performs a full Career Reset (Shift 01, 0 Scrip, Lv 0/0/0).
	if (CurrentPhase == EShiftPhase::ActiveInspection)
	{
		LastStampScripAwarded = 0;
		LastStampedDeviationKg = 0.0f;
		bLastStampWasDirectorsCut = false;
		ClearTransientRequisitionFeedback();
		TriggerContractFault(EContractFaultReason::ManualOperatorReset);
	}
	else if (CurrentPhase == EShiftPhase::ShiftCompleted || CurrentPhase == EShiftPhase::BondCleared)
	{
		ExecuteCareerResetToShiftOne();
	}
}

bool UInspectionShiftSubsystem::ExecuteCareerResetToShiftOne()
{
	CurrentShiftNumber = 1;
	ScripBalance = 0;
	LastStampScripAwarded = 0;
	LastStampedDeviationKg = 0.0f;
	bLastStampWasDirectorsCut = false;
	ClampHydraulicsLevel = 0;
	TareHopperLevel = 0;
	VeinScannerDamperLevel = 0;
	ClearTransientRequisitionFeedback();
	OnCareerReset.Broadcast();
	return StartShiftFromContract(0);
}

bool UInspectionShiftSubsystem::AdvanceToShiftForVerification(int32 TargetShiftNumber, int32 StartContractIndex)
{
	CurrentShiftNumber = FMath::Max(1, TargetShiftNumber);
	LastStampScripAwarded = 0;
	LastStampedDeviationKg = 0.0f;
	bLastStampWasDirectorsCut = false;
	ClearTransientRequisitionFeedback();
	return StartShiftFromContract(StartContractIndex);
}

void UInspectionShiftSubsystem::TriggerContractFault(EContractFaultReason FaultReason)
{
	const bool bIsScaleOrTareFault =
		(FaultReason == EContractFaultReason::ScaleContaminated) ||
		(FaultReason == EContractFaultReason::QuotaOvershoot) ||
		(FaultReason == EContractFaultReason::TareAllowanceExceeded);

	const FVector Origin = bIsScaleOrTareFault ? QueryReceiverLocationOrZero() : QueryRigLocationOrZero();
	TriggerContractFaultAtLocation(FaultReason, Origin);
}

void UInspectionShiftSubsystem::TriggerContractFaultAtLocation(
	EContractFaultReason FaultReason,
	const FVector& FaultWorldLocation)
{
	if (FaultReason == EContractFaultReason::None || CurrentPhase != EShiftPhase::ActiveInspection)
	{
		return;
	}

	bIsContractBriefActive = false;
	ClearTransientRequisitionFeedback();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ContractBriefTimerHandle);
	}

	LastFaultReason = FaultReason;
	SetShiftPhase(EShiftPhase::FaultResetDelay);
	RefreshAndBroadcastSnapshot();

	OnTelemetryImpulse.Broadcast(ETelemetryImpulseId::FaultTriggered, 1.0f, FaultWorldLocation);
	OnShiftVerdict.Broadcast(EShiftVerdictType::ContractFaultReset, FaultReason);

	if (UWorld* World = GetWorld())
	{
		const float ResetDelaySeconds = IsValid(ActiveProgressionAsset)
			? FMath::Max(0.2f, ActiveProgressionAsset->FaultAutoResetDelaySeconds)
			: DefaultTransitionDelaySeconds;

		World->GetTimerManager().SetTimer(
			TransitionTimerHandle,
			this,
			&UInspectionShiftSubsystem::HandleFaultResetTimerElapsed,
			ResetDelaySeconds,
			false);
	}
}

void UInspectionShiftSubsystem::TryAutoStartShiftIfReady()
{
	if (!bWorldHasBegunPlay)
	{
		return;
	}

	if (CurrentPhase != EShiftPhase::WaitingForStations)
	{
		return;
	}

	if (HasRegisteredSliceableRig() && HasRegisteredQuotaReceiver())
	{
		StartShiftFromContract(0);
	}
}

bool UInspectionShiftSubsystem::EnsureShiftProgressionInitialized()
{
	if (IsValid(ActiveProgressionAsset) && ActiveProgressionAsset->OrderedContracts.Num() > 0)
	{
		return true;
	}

	if (IsValid(ActivePresentationAsset) &&
		IsValid(ActivePresentationAsset->ShiftProgressionOverride) &&
		ActivePresentationAsset->ShiftProgressionOverride->OrderedContracts.Num() > 0)
	{
		ActiveProgressionAsset = ActivePresentationAsset->ShiftProgressionOverride;
		return true;
	}

	if (FPackageName::DoesPackageExist(TEXT("/Game/Presentation/DA_ShiftProgression")))
	{
		UShiftProgressionDataAsset* DiscoveredProgression = LoadObject<UShiftProgressionDataAsset>(
			nullptr,
			TEXT("/Game/Presentation/DA_ShiftProgression.DA_ShiftProgression"));
		if (IsValid(DiscoveredProgression) && DiscoveredProgression->OrderedContracts.Num() > 0)
		{
			ActiveProgressionAsset = DiscoveredProgression;
			return true;
		}
	}

	ActiveProgressionAsset = BuildFallbackGreyboxProgression();
	return IsValid(ActiveProgressionAsset) && ActiveProgressionAsset->OrderedContracts.Num() > 0;
}

UShiftProgressionDataAsset* UInspectionShiftSubsystem::BuildFallbackGreyboxProgression()
{
	if (IsValid(FallbackGreyboxProgression) && FallbackGreyboxProgression->OrderedContracts.Num() == 6)
	{
		return FallbackGreyboxProgression;
	}

	FallbackGreyboxContracts.Reset();
	FallbackGreyboxProgression = NewObject<UShiftProgressionDataAsset>(this, TEXT("DA_FallbackGreyboxShiftProgression"));
	FallbackGreyboxProgression->LaserCapacitorChargeSeconds = 0.35f;
	FallbackGreyboxProgression->LaserCooldownSeconds = 0.25f;
	FallbackGreyboxProgression->MinSliverVolumeCm3 = 20.0f;
	FallbackGreyboxProgression->MinChunkAxisThicknessCm = 0.30f;
	FallbackGreyboxProgression->ChunkSeparationSpeedCmPerSec = 15.0f;
	FallbackGreyboxProgression->MaxInFlightWatchdogSeconds = 6.0f;
	FallbackGreyboxProgression->FaultAutoResetDelaySeconds = 1.8f;
	FallbackGreyboxProgression->ContractBriefDurationSeconds = 3.5f;
	FallbackGreyboxProgression->TightTorqueBriefThresholdNm = 29.0f;

	auto MakeVein = [](const FVector& StartCm, const FVector& EndCm, float RadiusCm, float WarnCm) -> FVolatileVeinDefinition
	{
		FVolatileVeinDefinition Vein;
		Vein.LocalStartCm = StartCm;
		Vein.LocalEndCm = EndCm;
		Vein.RadiusCm = RadiusCm;
		Vein.WarningRangeCm = WarnCm;
		return Vein;
	};

	// Contract 01 (Act I-A): Symmetric Box — pure mass calibration (~34.56 kg initial mass).
	{
		UMonolithContractDataAsset* C01 = NewObject<UMonolithContractDataAsset>(this, TEXT("DA_Fallback_Contract01_Box"));
		C01->ContractCode = FText::FromString(TEXT("ACT I-A // CTR-01: STANDARD SLAG BILLET"));
		C01->ContractDirectiveText = GetDefaultDirectiveForContractIndex(0);
		C01->BaseShape = EMonolithBaseShape::Box;
		C01->DimensionsCm = FVector(24.0f, 24.0f, 75.0f);
		C01->DensityGramsPerCm3 = 0.80f;
		C01->TargetTitheMassKg = 12.0f;
		C01->QuotaToleranceKg = 1.25f;
		C01->MaxAllowedTareWasteKg = 14.0f;
		C01->bAllowDiverterToggle = true;
		C01->ClampMaxSafeTorqueNm = 26.0f;
		C01->ClampFatigueGraceSeconds = 6.5f;
		FallbackGreyboxContracts.Add(C01);
	}

	// Contract 02 (Act I-B): Front-facing YZ Cantilevered J-Hook (~45.32 kg initial mass -> S_clamp ~ 0.59 on spawn).
	{
		UMonolithContractDataAsset* C02 = NewObject<UMonolithContractDataAsset>(this, TEXT("DA_Fallback_Contract02_JHook"));
		C02->ContractCode = FText::FromString(TEXT("ACT I-B // CTR-02: CANTILEVERED J-HOOK"));
		C02->ContractDirectiveText = GetDefaultDirectiveForContractIndex(1);
		C02->BaseShape = EMonolithBaseShape::CantileveredJHook;
		C02->DimensionsCm = FVector(24.0f, 34.0f, 80.0f);
		C02->DensityGramsPerCm3 = 0.85f;
		C02->TargetTitheMassKg = 14.5f;
		C02->QuotaToleranceKg = 1.35f;
		C02->MaxAllowedTareWasteKg = 16.0f;
		C02->bAllowDiverterToggle = true;
		C02->ClampMaxSafeTorqueNm = 29.0f;
		C02->ClampFatigueGraceSeconds = 6.5f;
		FallbackGreyboxContracts.Add(C02);
	}

	// Contract 03 (Act II-A): 1-Vein Offset C-Channel + Diverter routing (~57.69 kg initial mass -> S_clamp ~ 0.575 on spawn).
	{
		UMonolithContractDataAsset* C03 = NewObject<UMonolithContractDataAsset>(this, TEXT("DA_Fallback_Contract03_OffsetCChannel"));
		C03->ContractCode = FText::FromString(TEXT("ACT II-A // CTR-03: OFFSET C-CHANNEL CORE"));
		C03->ContractDirectiveText = GetDefaultDirectiveForContractIndex(2);
		C03->BaseShape = EMonolithBaseShape::OffsetCChannel;
		C03->DimensionsCm = FVector(28.0f, 34.0f, 84.0f);
		C03->DensityGramsPerCm3 = 0.92f;
		C03->TargetTitheMassKg = 15.0f;
		C03->QuotaToleranceKg = 1.00f;
		C03->MaxAllowedTareWasteKg = 18.0f;
		C03->bAllowDiverterToggle = true;
		C03->ClampMaxSafeTorqueNm = 30.0f;
		C03->ClampFatigueGraceSeconds = 6.5f;
		C03->HiddenVeins.Add(MakeVein(FVector(-5.5f, -8.5f, -33.5f), FVector(5.5f, 6.5f, -28.0f), 2.4f, 18.0f));
		FallbackGreyboxContracts.Add(C03);
	}

	// Contract 04 (Act II-B): 2-Vein Crossed Lattice with +1.85 cm eccentric clamp offset (~66.53 kg initial mass -> S_clamp ~ 0.51 on spawn).
	{
		UMonolithContractDataAsset* C04 = NewObject<UMonolithContractDataAsset>(this, TEXT("DA_Fallback_Contract04_CrossedLattice"));
		C04->ContractCode = FText::FromString(TEXT("ACT II-B // CTR-04: CROSSED ISOTOPE LATTICE"));
		C04->ContractDirectiveText = GetDefaultDirectiveForContractIndex(3);
		C04->BaseShape = EMonolithBaseShape::Box;
		C04->DimensionsCm = FVector(30.0f, 28.0f, 88.0f);
		C04->ClampGripHorizontalOffsetCm = FVector2D(0.0f, 1.85f);
		C04->DensityGramsPerCm3 = 0.90f;
		C04->TargetTitheMassKg = 18.0f;
		C04->QuotaToleranceKg = 0.95f;
		C04->MaxAllowedTareWasteKg = 22.0f;
		C04->bAllowDiverterToggle = true;
		C04->ClampMaxSafeTorqueNm = 30.0f;
		C04->ClampFatigueGraceSeconds = 6.5f;
		C04->HiddenVeins.Add(MakeVein(FVector(-6.0f, -9.0f, 6.0f), FVector(6.0f, -2.0f, 20.0f), 2.5f, 18.0f));
		C04->HiddenVeins.Add(MakeVein(FVector(6.0f, 3.0f, -24.0f), FVector(-6.0f, 9.0f, -10.0f), 2.5f, 18.0f));
		FallbackGreyboxContracts.Add(C04);
	}

	// Contract 05 (Act III-A): High-Density Bismuth Counterweighted Z-Bracket (~78.40 kg initial mass -> S_clamp ~ 0.616 on spawn).
	{
		UMonolithContractDataAsset* C05 = NewObject<UMonolithContractDataAsset>(this, TEXT("DA_Fallback_Contract05_BismuthZBracket"));
		C05->ContractCode = FText::FromString(TEXT("ACT III-A // CTR-05: BISMUTH Z-COUNTERWEIGHT"));
		C05->ContractDirectiveText = GetDefaultDirectiveForContractIndex(4);
		C05->BaseShape = EMonolithBaseShape::CounterweightedZBracket;
		C05->DimensionsCm = FVector(24.0f, 34.0f, 78.0f);
		C05->DensityGramsPerCm3 = 1.82f;
		C05->TargetTitheMassKg = 21.0f;
		C05->QuotaToleranceKg = 0.90f;
		C05->MaxAllowedTareWasteKg = 28.0f;
		C05->bAllowDiverterToggle = true;
		C05->ClampMaxSafeTorqueNm = 32.0f;
		C05->ClampFatigueGraceSeconds = 6.5f;
		C05->ClampFatigueCooldownSeconds = 3.0f;
		C05->ClampSpringStiffness = 210.0f;
		C05->ClampSpringDamping = 28.0f;
		C05->HiddenVeins.Add(MakeVein(FVector(-4.5f, -9.5f, -31.0f), FVector(4.5f, -2.5f, -16.0f), 2.3f, 17.0f));
		C05->HiddenVeins.Add(MakeVein(FVector(-4.5f, 2.5f, 5.5f), FVector(4.5f, 9.5f, 16.5f), 2.3f, 17.0f));
		FallbackGreyboxContracts.Add(C05);
	}

	// Contract 06 (Act III-B): 3-Vein YZ Stepped Core Finale with -2.50 cm eccentric clamp offset (~86.44 kg initial mass -> S_clamp ~ 0.62 on spawn).
	{
		UMonolithContractDataAsset* C06 = NewObject<UMonolithContractDataAsset>(this, TEXT("DA_Fallback_Contract06_SteppedFinale"));
		C06->ContractCode = FText::FromString(TEXT("ACT III-B // CTR-06: DIRECTORATE STEPPED CORE"));
		C06->ContractDirectiveText = GetDefaultDirectiveForContractIndex(5);
		C06->BaseShape = EMonolithBaseShape::SteppedCore;
		C06->DimensionsCm = FVector(24.0f, 34.0f, 84.0f);
		C06->ClampGripHorizontalOffsetCm = FVector2D(0.0f, -2.50f);
		C06->DensityGramsPerCm3 = 1.75f;
		C06->TargetTitheMassKg = 24.0f;
		C06->QuotaToleranceKg = 0.85f;
		C06->MaxAllowedTareWasteKg = 30.0f;
		C06->bAllowDiverterToggle = true;
		C06->ClampMaxSafeTorqueNm = 34.0f;
		C06->ClampFatigueGraceSeconds = 6.5f;
		C06->ClampFatigueCooldownSeconds = 3.0f;
		C06->ClampSpringStiffness = 220.0f;
		C06->ClampSpringDamping = 28.0f;
		C06->HiddenVeins.Add(MakeVein(FVector(-5.0f, -12.5f, -9.0f), FVector(5.0f, -7.5f, 7.0f), 2.3f, 16.0f));
		C06->HiddenVeins.Add(MakeVein(FVector(5.0f, 2.5f, -31.0f), FVector(-5.0f, 6.8f, -19.0f), 2.3f, 16.0f));
		C06->HiddenVeins.Add(MakeVein(FVector(-4.5f, -4.5f, 13.0f), FVector(4.5f, 4.5f, 20.5f), 2.2f, 16.0f));
		FallbackGreyboxContracts.Add(C06);
	}

	for (UMonolithContractDataAsset* FallbackContract : FallbackGreyboxContracts)
	{
		FallbackGreyboxProgression->OrderedContracts.Add(FallbackContract);
	}

	return FallbackGreyboxProgression;
}

const UMonolithContractDataAsset* UInspectionShiftSubsystem::ResolveContractAtIndex(int32 ContractIndex) const
{
	if (!IsValid(ActiveProgressionAsset) || !ActiveProgressionAsset->OrderedContracts.IsValidIndex(ContractIndex))
	{
		return nullptr;
	}

	const TObjectPtr<UMonolithContractDataAsset>& ContractRef = ActiveProgressionAsset->OrderedContracts[ContractIndex];
	return IsValid(ContractRef) ? ContractRef.Get() : nullptr;
}

bool UInspectionShiftSubsystem::LoadAndBeginContract(int32 ContractIndex)
{
	bIsContractBriefActive = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TransitionTimerHandle);
		World->GetTimerManager().ClearTimer(ContractBriefTimerHandle);
		if (UChunkPoolSubsystem* PoolSubsystem = World->GetSubsystem<UChunkPoolSubsystem>())
		{
			PoolSubsystem->ReclaimAllActiveChunks();
		}
	}

	const UMonolithContractDataAsset* TargetContract = ResolveContractAtIndex(ContractIndex);
	if (!IsValid(TargetContract))
	{
		UE_LOG(LogTareAndTithe, Error,
			TEXT("UInspectionShiftSubsystem: Failed to resolve contract at index %d."),
			ContractIndex);
		return false;
	}

	SetShiftPhase(EShiftPhase::ContractIntro);

	CurrentContractIndex = ContractIndex;
	ActiveContract = TargetContract;
	LastFaultReason = EContractFaultReason::None;

	ActiveInFlightPayloads.Reset();
	AccountedTarePayloads.Reset();
	InFlightMassKg = 0.0f;
	VaporizedTareMassKg = 0.0f;
	DisposedChunksTareMassKg = 0.0f;
	AccumulatedTareWasteKg = 0.0f;
	LatestClampStrainNormalized = 0.0f;

	for (int32 Idx = AudioBridges.Num() - 1; Idx >= 0; --Idx)
	{
		if (UAudioTelemetryBridgeComponent* Bridge = AudioBridges[Idx].Get())
		{
			Bridge->ResetStreamCache();
		}
	}

	if (UObject* ReceiverObj = ReceiverBinding.QuotaReceiver.GetObject())
	{
		IQuotaReceiverInterface::Execute_ResetReceiverState(ReceiverObj);
	}
	if (ReceiverBinding.OnConfigureContract.IsBound())
	{
		ReceiverBinding.OnConfigureContract.Execute(ActiveContract, EDiverterRoute::TitheScale);
	}

	float SpawnedInitialMassKg = 0.0f;
	if (!RigBinding.OnLoadContract.IsBound() ||
		!RigBinding.OnLoadContract.Execute(ActiveContract, ActiveProgressionAsset, SpawnedInitialMassKg))
	{
		UE_LOG(LogTareAndTithe, Error,
			TEXT("UInspectionShiftSubsystem: Monolith rig failed to initialize contract '%s'."),
			*ActiveContract->GetName());
		return false;
	}

	InitialContractMassKg = FMath::Max(0.0f, SpawnedInitialMassKg);
	RemainderMassKg = InitialContractMassKg;
	if (InitialContractMassKg <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTareAndTithe, Error,
			TEXT("UInspectionShiftSubsystem: Contract '%s' initialized with non-positive mass (%.3f kg)."),
			*ActiveContract->GetName(),
			InitialContractMassKg);
	}

	// Activate inspection phase and 3.5s operational brief BEFORE snapshot & contract-loaded broadcasts so aiming and live preview queries run immediately.
	bIsContractBriefActive = true;
	if (UWorld* World = GetWorld())
	{
		const float BriefDurationSeconds = IsValid(ActiveProgressionAsset)
			? FMath::Clamp(ActiveProgressionAsset->ContractBriefDurationSeconds, 0.5f, 10.0f)
			: DefaultContractBriefDurationSeconds;

		World->GetTimerManager().SetTimer(
			ContractBriefTimerHandle,
			this,
			&UInspectionShiftSubsystem::HandleContractBriefTimerElapsed,
			BriefDurationSeconds,
			false);
	}

	SetShiftPhase(EShiftPhase::ActiveInspection);
	RefreshAndBroadcastSnapshot();
	OnActiveContractLoaded.Broadcast(ActiveContract, ActiveProgressionAsset, CurrentContractIndex);

	return true;
}

void UInspectionShiftSubsystem::RecomputeInFlightMassFromSet()
{
	double SumInFlightMassKg = 0.0;

	for (auto It = ActiveInFlightPayloads.CreateIterator(); It; ++It)
	{
		AActor* Actor = It->Get();
		if (!IsValid(Actor) || !Actor->Implements<UMassPayloadInterface>())
		{
			It.RemoveCurrent();
			continue;
		}

		const EChunkRoutingState State = IMassPayloadInterface::Execute_GetRoutingState(Actor);
		if (State != EChunkRoutingState::InFlight)
		{
			It.RemoveCurrent();
			continue;
		}

		SumInFlightMassKg += FMath::Max(0.0, static_cast<double>(IMassPayloadInterface::Execute_GetAnalyticalMassKg(Actor)));
	}

	InFlightMassKg = static_cast<float>(SumInFlightMassKg);
}

void UInspectionShiftSubsystem::AccountPayloadAsTareOnce(AActor* PayloadActor, float ExplicitMassKg)
{
	if (IsValid(PayloadActor))
	{
		ActiveInFlightPayloads.Remove(PayloadActor);

		if (CurrentPhase == EShiftPhase::ActiveInspection && !AccountedTarePayloads.Contains(PayloadActor))
		{
			AccountedTarePayloads.Add(PayloadActor);

			float SafeMassKg = FMath::Max(0.0f, ExplicitMassKg);
			if (SafeMassKg <= 0.0f && PayloadActor->Implements<UMassPayloadInterface>())
			{
				SafeMassKg = FMath::Max(0.0f, IMassPayloadInterface::Execute_GetAnalyticalMassKg(PayloadActor));
			}

			DisposedChunksTareMassKg += SafeMassKg;
			AccumulatedTareWasteKg = VaporizedTareMassKg + DisposedChunksTareMassKg;
		}
	}

	RecomputeInFlightMassFromSet();

	if (CurrentPhase == EShiftPhase::ActiveInspection)
	{
		RefreshAndBroadcastSnapshot();
		EvaluateContractHealth();
	}
}

void UInspectionShiftSubsystem::RefreshAndBroadcastSnapshot()
{
	RemainderMassKg = QueryLiveRemainderMassKg();

	float SettledPanMassKg = 0.0f;
	bool bReceiverSettled = false;
	bool bReceiverContaminated = false;
	EDiverterRoute ActiveRoute = EDiverterRoute::TitheScale;

	if (UObject* ReceiverObj = ReceiverBinding.QuotaReceiver.GetObject())
	{
		SettledPanMassKg = FMath::Max(0.0f, IQuotaReceiverInterface::Execute_GetSettledMassKg(ReceiverObj));
		bReceiverSettled = IQuotaReceiverInterface::Execute_IsReceiverSettled(ReceiverObj);
		bReceiverContaminated = IQuotaReceiverInterface::Execute_HasContaminationFault(ReceiverObj);
		ActiveRoute = IQuotaReceiverInterface::Execute_GetActiveDiverterRoute(ReceiverObj);
	}

	const float TargetMassKg = IsValid(ActiveContract) ? ActiveContract->TargetTitheMassKg : 10.0f;
	const float ToleranceKg = GetEffectiveQuotaToleranceKg(ActiveContract);
	const float DirectorsCutTolKg = GetDirectorsCutToleranceKg(ToleranceKg);
	const float MaxTareKg = GetEffectiveMaxTareWasteKg(ActiveContract);

	const bool bWithinWindow =
		!bReceiverContaminated &&
		(SettledPanMassKg >= (TargetMassKg - ToleranceKg - QuotaBoundaryFloatEpsilonKg)) &&
		(SettledPanMassKg <= (TargetMassKg + ToleranceKg + QuotaBoundaryFloatEpsilonKg));

	const bool bWithinDirectorsCut =
		!bReceiverContaminated &&
		(FMath::Abs(SettledPanMassKg - TargetMassKg) <= (DirectorsCutTolKg + QuotaBoundaryFloatEpsilonKg));

	const bool bReadyForStamp =
		(CurrentPhase == EShiftPhase::ActiveInspection) &&
		(ActiveInFlightPayloads.Num() == 0) &&
		bReceiverSettled &&
		bWithinWindow;

	CachedSnapshot.ContractIndex = CurrentContractIndex;
	CachedSnapshot.CurrentShiftNumber = CurrentShiftNumber;
	CachedSnapshot.TargetBondShifts = TargetBondShifts;
	CachedSnapshot.ScripBalance = ScripBalance;
	CachedSnapshot.LastStampScripAwarded = LastStampScripAwarded;
	CachedSnapshot.TargetTitheMassKg = TargetMassKg;
	CachedSnapshot.QuotaToleranceKg = ToleranceKg;
	CachedSnapshot.DirectorsCutToleranceKg = DirectorsCutTolKg;
	CachedSnapshot.SettledScaleMassKg = SettledPanMassKg;
	CachedSnapshot.RemainderMassKg = RemainderMassKg;
	CachedSnapshot.InFlightMassKg = InFlightMassKg;
	CachedSnapshot.AccumulatedTareWasteKg = AccumulatedTareWasteKg;
	CachedSnapshot.MaxAllowedTareWasteKg = MaxTareKg;
	CachedSnapshot.ClampStrainNormalized = LatestClampStrainNormalized;
	CachedSnapshot.ClampHydraulicsLevel = ClampHydraulicsLevel;
	CachedSnapshot.TareHopperLevel = TareHopperLevel;
	CachedSnapshot.VeinScannerDamperLevel = VeinScannerDamperLevel;
	CachedSnapshot.ContractDirectiveText = ResolveActiveContractDirectiveText();
	CachedSnapshot.ContractBriefText = ResolveContractOperationalBriefText(ActiveContract);
	CachedSnapshot.DirectorIntercomLine = ResolveDirectorIntercomLine(bReadyForStamp, bWithinDirectorsCut);
	CachedSnapshot.TransientRequisitionFeedback = TransientRequisitionFeedback;
	CachedSnapshot.ActiveDiverterRoute = ActiveRoute;
	CachedSnapshot.bIsContractBriefActive = bIsContractBriefActive && (CurrentPhase == EShiftPhase::ActiveInspection);
	CachedSnapshot.bLastRequisitionSucceeded = bLastRequisitionSucceeded;
	CachedSnapshot.bIsWithinQuotaWindow = bWithinWindow;
	CachedSnapshot.bIsWithinDirectorsCutWindow = bWithinDirectorsCut;
	CachedSnapshot.bLastStampWasDirectorsCut = bLastStampWasDirectorsCut;
	CachedSnapshot.bScaleSettledForStamp = bReadyForStamp;
	CachedSnapshot.bIsReceiverPhysicallySettled = bReceiverSettled;
	CachedSnapshot.LastStampedDeviationKg = LastStampedDeviationKg;

	OnNativeContractSnapshotUpdated.Broadcast(CachedSnapshot);
	OnContractSnapshotUpdated.Broadcast(CachedSnapshot);
}

void UInspectionShiftSubsystem::EvaluateContractHealth()
{
	if (CurrentPhase != EShiftPhase::ActiveInspection || !IsValid(ActiveContract))
	{
		return;
	}

	UObject* ReceiverObj = ReceiverBinding.QuotaReceiver.GetObject();
	if (!IsValid(ReceiverObj))
	{
		return;
	}

	RecomputeInFlightMassFromSet();
	RemainderMassKg = QueryLiveRemainderMassKg();

	const bool bContaminated = IQuotaReceiverInterface::Execute_HasContaminationFault(ReceiverObj);
	if (bContaminated)
	{
		TriggerContractFault(EContractFaultReason::ScaleContaminated);
		return;
	}

	const float SettledScaleMassKg = FMath::Max(0.0f, IQuotaReceiverInterface::Execute_GetSettledMassKg(ReceiverObj));
	const bool bReceiverSettled = IQuotaReceiverInterface::Execute_IsReceiverSettled(ReceiverObj);
	const float EffectiveToleranceKg = GetEffectiveQuotaToleranceKg(ActiveContract);
	const float EffectiveMaxTareKg = GetEffectiveMaxTareWasteKg(ActiveContract);

	if (bReceiverSettled &&
		SettledScaleMassKg > (ActiveContract->TargetTitheMassKg + EffectiveToleranceKg + QuotaBoundaryFloatEpsilonKg))
	{
		TriggerContractFault(EContractFaultReason::QuotaOvershoot);
		return;
	}

	if (AccumulatedTareWasteKg > (EffectiveMaxTareKg + QuotaBoundaryFloatEpsilonKg))
	{
		TriggerContractFault(EContractFaultReason::TareAllowanceExceeded);
		return;
	}

	if (ActiveInFlightPayloads.Num() == 0 &&
		(SettledScaleMassKg + RemainderMassKg) < (ActiveContract->TargetTitheMassKg - EffectiveToleranceKg - QuotaBoundaryFloatEpsilonKg))
	{
		TriggerContractFault(EContractFaultReason::MathematicallyUnreachable);
		return;
	}
}

void UInspectionShiftSubsystem::HandleChunkWatchdogExpired(ASeveredChunkActor* ExpiredChunk)
{
	if (!IsValid(ExpiredChunk) || !ExpiredChunk->Implements<UMassPayloadInterface>())
	{
		return;
	}

	ReceiverBinding.OnReconcileOverlaps.ExecuteIfBound();

	const EChunkRoutingState CurrentRoutingState = IMassPayloadInterface::Execute_GetRoutingState(ExpiredChunk);
	if (CurrentRoutingState != EChunkRoutingState::InFlight)
	{
		if (ActiveInFlightPayloads.Remove(ExpiredChunk) > 0)
		{
			RecomputeInFlightMassFromSet();
			if (CurrentPhase == EShiftPhase::ActiveInspection)
			{
				RefreshAndBroadcastSnapshot();
				EvaluateContractHealth();
			}
		}
		return;
	}

	// Smart Watchdog Recovery Shield: never delete or penalize a chunk resting inside or stacked above the scale pan well.
	if (UObject* ScaleReceiverObj = ReceiverBinding.QuotaReceiver.GetObject())
	{
		if (IsValid(ScaleReceiverObj) && IQuotaReceiverInterface::Execute_TryRecoverPayloadOnReceiver(ScaleReceiverObj, ExpiredChunk))
		{
			ActiveInFlightPayloads.Remove(ExpiredChunk);
			RecomputeInFlightMassFromSet();
			if (CurrentPhase == EShiftPhase::ActiveInspection)
			{
				RefreshAndBroadcastSnapshot();
				EvaluateContractHealth();
			}
			return;
		}
	}

	// Single re-arm if the chunk is still actively falling downward inside the cell rather than wedged.
	if (ExpiredChunk->TryRearmWatchdogIfActivelyFalling(WatchdogActiveFallVelocityThresholdZCmPerSec, WatchdogActiveFallMinCellFloorZCm))
	{
		return;
	}

	const float ChunkMassKg = FMath::Max(0.0f, IMassPayloadInterface::Execute_GetAnalyticalMassKg(ExpiredChunk));
	const FVector ChunkWorldLocation = ExpiredChunk->GetActorLocation();

	IMassPayloadInterface::Execute_NotifyRoutingStateChanged(ExpiredChunk, EChunkRoutingState::LostAsStrayTare);

	const float ImpulseMagnitude = FMath::Clamp(ChunkMassKg / 10.0f, 0.05f, 1.0f);
	OnTelemetryImpulse.Broadcast(ETelemetryImpulseId::ChunkTareDisposed, ImpulseMagnitude, ChunkWorldLocation);

	AccountPayloadAsTareOnce(ExpiredChunk, ChunkMassKg);
	IMassPayloadInterface::Execute_ReturnToChunkPool(ExpiredChunk);
}

void UInspectionShiftSubsystem::HandleFaultResetTimerElapsed()
{
	if (CurrentPhase == EShiftPhase::FaultResetDelay)
	{
		LoadAndBeginContract(CurrentContractIndex);
	}
}

void UInspectionShiftSubsystem::HandleContractPassTimerElapsed()
{
	if (CurrentPhase == EShiftPhase::ContractPassTransition)
	{
		LoadAndBeginContract(CurrentContractIndex + 1);
	}
}

void UInspectionShiftSubsystem::HandleContractBriefTimerElapsed()
{
	if (!bIsContractBriefActive)
	{
		return;
	}

	bIsContractBriefActive = false;
	if (CurrentPhase == EShiftPhase::ActiveInspection)
	{
		RefreshAndBroadcastSnapshot();
	}
}

void UInspectionShiftSubsystem::HandleRequisitionFeedbackTimerElapsed()
{
	if (TransientRequisitionFeedback.IsEmpty())
	{
		return;
	}

	TransientRequisitionFeedback.Empty();
	bLastRequisitionSucceeded = false;
	RefreshAndBroadcastSnapshot();
}

void UInspectionShiftSubsystem::HandlePublisherTelemetryStream(ETelemetryStreamId StreamId, float NormalizedValue)
{
	if (StreamId == ETelemetryStreamId::ClampStrain)
	{
		LatestClampStrainNormalized = FMath::Clamp(NormalizedValue, 0.0f, 1.0f);
	}
}

void UInspectionShiftSubsystem::SetTransientRequisitionFeedback(const FString& FeedbackMessage, bool bSucceeded)
{
	TransientRequisitionFeedback = FeedbackMessage;
	bLastRequisitionSucceeded = bSucceeded;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RequisitionFeedbackTimerHandle,
			this,
			&UInspectionShiftSubsystem::HandleRequisitionFeedbackTimerElapsed,
			RequisitionFeedbackDisplaySeconds,
			false);
	}
}

void UInspectionShiftSubsystem::ClearTransientRequisitionFeedback()
{
	TransientRequisitionFeedback.Empty();
	bLastRequisitionSucceeded = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RequisitionFeedbackTimerHandle);
	}
}

void UInspectionShiftSubsystem::SetShiftPhase(EShiftPhase NewPhase)
{
	if (CurrentPhase != NewPhase)
	{
		CurrentPhase = NewPhase;
		OnShiftPhaseChanged.Broadcast(CurrentPhase);
	}
}

float UInspectionShiftSubsystem::QueryLiveRemainderMassKg() const
{
	if (RigBinding.OnQueryRemainderMassKg.IsBound())
	{
		return FMath::Max(0.0f, RigBinding.OnQueryRemainderMassKg.Execute());
	}
	return RemainderMassKg;
}

FString UInspectionShiftSubsystem::ResolveActiveContractDirectiveText() const
{
	if (IsValid(ActiveContract) && !ActiveContract->ContractDirectiveText.IsEmpty())
	{
		return ActiveContract->ContractDirectiveText;
	}
	return GetDefaultDirectiveForContractIndex(CurrentContractIndex);
}

FString UInspectionShiftSubsystem::ResolveContractOperationalBriefText(const UMonolithContractDataAsset* Contract) const
{
	if (!IsValid(Contract))
	{
		return TEXT("BRIEF: CUT WITHIN TARGET MASS WINDOW -> STAMP [E]");
	}

	if (Contract->HiddenVeins.Num() > 0)
	{
		return TEXT("BRIEF: [SPACE] -> TARE TO PURGE VEIN CHUNKS FIRST, THEN TITHE");
	}

	if (Contract->HasAsymmetricTorqueProfile() ||
		Contract->ClampMaxSafeTorqueNm <= 25.0f)
	{
		return TEXT("BRIEF: WATCH CLAMP TORQUE - BEVEL [RMB] OR TRIM LOAD");
	}

	return TEXT("BRIEF: CUT WITHIN TARGET MASS WINDOW -> STAMP [E]");
}

FString UInspectionShiftSubsystem::ResolveDirectorIntercomLine(bool bReadyForStamp, bool bWithinDirectorsCut) const
{
	if (!TransientRequisitionFeedback.IsEmpty())
	{
		return TransientRequisitionFeedback;
	}

	switch (CurrentPhase)
	{
	case EShiftPhase::FaultResetDelay:
		switch (LastFaultReason)
		{
		case EContractFaultReason::ScaleContaminated:
			return TEXT("CONTAMINATED TITHE! DUMP VEINS TO TARE!");
		case EContractFaultReason::VolatileDetonation:
			return TEXT("ISOTOPE DETONATED! DEBT PENALTY LOGGED!");
		case EContractFaultReason::QuotaOvershoot:
			return TEXT("OVERWEIGHT SLOP! READ YOUR TOLERANCE!");
		case EContractFaultReason::TareAllowanceExceeded:
			return TEXT("TARE HOPPER CHOKED! STOP WASTING ORE!");
		case EContractFaultReason::ClampStructuralSnap:
			return TEXT("YOU SHEARED MY CLAMP! BALANCE YOUR CUTS!");
		case EContractFaultReason::MathematicallyUnreachable:
			return TEXT("CORE RUINED! NOT ENOUGH MASS FOR QUOTA!");
		case EContractFaultReason::ManualOperatorReset:
			return TEXT("CELL PURGED BY #409. NO SCRIP ISSUED.");
		default:
			return TEXT("INSPECTION FAULT! RESETTING CELL 09!");
		}

	case EShiftPhase::ContractPassTransition:
		return bLastStampWasDirectorsCut
			? TEXT("DIRECTOR'S CUT! EXEMPLARY (+2 SCRIP)")
			: TEXT("ACCEPTABLE YIELD, #409 (+1 SCRIP)");

	case EShiftPhase::ShiftCompleted:
		if (CurrentShiftNumber == 1)
		{
			return TEXT("SHIFT 01 LOGGED. TOLERANCES TIGHTEN NEXT.");
		}
		if (CurrentShiftNumber == 2)
		{
			return TEXT("ONE SHIFT REMAINS ON YOUR BOND, #409.");
		}
		return FString::Printf(TEXT("OVERTIME S%02d LOGGED. KEEP CARVING, #409."), CurrentShiftNumber);

	case EShiftPhase::BondCleared:
		return TEXT("BOND CLEARED! #409 RELEASE AUTHORIZED.");

	case EShiftPhase::ActiveInspection:
	default:
		if (LatestClampStrainNormalized >= 0.78f)
		{
			return TEXT("STOP ONE-SIDED CUTS! JAW WILL SNAP!");
		}
		if (bReadyForStamp && bWithinDirectorsCut)
		{
			return TEXT("PRECISION LOCK! STAMP [E] FOR +2 SCRIP!");
		}
		if (bReadyForStamp)
		{
			return TEXT("WITHIN TOLERANCE. STAMP [E] OR REFINE.");
		}

		switch (CurrentContractIndex)
		{
		case 0:
			if (CurrentShiftNumber <= 1)
			{
				return TEXT("#409: WORK OFF YOUR 3-SHIFT BOND. CARVE.");
			}
			if (CurrentShiftNumber == 2)
			{
				return TEXT("SHIFT 02/03: TIGHTER QUOTAS. NO SLOP, #409.");
			}
			if (CurrentShiftNumber == 3)
			{
				return TEXT("FINAL SHIFT 03/03: EARN YOUR RELEASE, #409.");
			}
			return FString::Printf(TEXT("OVERTIME S%02d: VOLUNTARY HAZARD PAY ACTIVE."), CurrentShiftNumber);
		case 1:
			return TEXT("CANTILEVERED HOOK: TRIM OVERHANG FIRST!");
		case 2:
			return TEXT("LIVE ISOTOPE CORE: [SPACE] DUMPS VEIN SCRAP.");
		case 3:
			return TEXT("CROSSED VEIN LATTICE: LISTEN TO SCANNER.");
		case 4:
			return TEXT("DENSE BISMUTH: ALTERNATE SIDES OR SNAP!");
		case 5:
			return (CurrentShiftNumber == TargetBondShifts)
				? TEXT("FINAL INGOT, #409: THREE VEINS TO FREEDOM!")
				: TEXT("DIRECTORATE INGOT: 3 VEINS. ZERO MARGIN.");
		default:
			return TEXT("#409: MAINTAIN QUOTA DISCIPLINE.");
		}
	}
}

FString UInspectionShiftSubsystem::GetDefaultDirectiveForContractIndex(int32 ContractIndex)
{
	switch (ContractIndex)
	{
	case 0:
		return TEXT("CUT 12.0 KG TO TITHE | STAMP [E]");
	case 1:
		return TEXT("CUT GREEN YIELD | [SPACE] DUMPS TARE");
	case 2:
		return TEXT("HIDDEN VEIN: AUDIO SCAN & BEVEL [RMB]");
	case 3:
		return TEXT("SLAG COLUMN: DODGE VEIN | WATCH TARE");
	case 4:
		return TEXT("TORQUE LIMIT: KEEP CUTS BALANCED");
	case 5:
		return TEXT("DIRECTOR'S INGOT: DUAL VEINS + TORQUE");
	default:
		return TEXT("SLICE TO QUOTA | STAMP [E]");
	}
}

FVector UInspectionShiftSubsystem::QueryRigLocationOrZero() const
{
	if (RigBinding.OnQueryWorldLocation.IsBound())
	{
		return RigBinding.OnQueryWorldLocation.Execute();
	}
	return FVector::ZeroVector;
}

FVector UInspectionShiftSubsystem::QueryReceiverLocationOrZero() const
{
	if (ReceiverBinding.OnQueryWorldLocation.IsBound())
	{
		return ReceiverBinding.OnQueryWorldLocation.Execute();
	}
	return FVector::ZeroVector;
}
