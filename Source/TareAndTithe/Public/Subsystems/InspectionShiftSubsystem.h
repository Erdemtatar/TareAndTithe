// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ScriptInterface.h"
#include "Core/TareTitheTypes.h"
#include "Interfaces/QuotaReceiverInterface.h"
#include "Interfaces/SliceableTargetInterface.h"
#include "Interfaces/TelemetryPublisherInterface.h"
#include "InspectionShiftSubsystem.generated.h"

class AActor;
class ASeveredChunkActor;
class UAudioTelemetryBridgeComponent;
class UMonolithContractDataAsset;
class UShiftProgressionDataAsset;
class UTareTithePresentationDataAsset;

UENUM(BlueprintType)
enum class EShiftPhase : uint8
{
	WaitingForStations      UMETA(DisplayName = "Waiting For Stations"),
	ContractIntro           UMETA(DisplayName = "Contract Intro"),
	ActiveInspection        UMETA(DisplayName = "Active Inspection"),
	FaultResetDelay         UMETA(DisplayName = "Fault Reset Delay"),
	ContractPassTransition  UMETA(DisplayName = "Contract Pass Transition"),
	ShiftCompleted          UMETA(DisplayName = "Shift Completed")
};

DECLARE_DELEGATE_RetVal_ThreeParams(
	bool,
	FLoadRigContractDelegate,
	const UMonolithContractDataAsset* /*Contract*/,
	const UShiftProgressionDataAsset* /*ShiftConfig*/,
	float& /*OutInitialMassKg*/);

DECLARE_DELEGATE_TwoParams(
	FConfigureQuotaReceiverDelegate,
	const UMonolithContractDataAsset* /*Contract*/,
	EDiverterRoute /*InitialRoute*/);

DECLARE_DELEGATE_RetVal(bool, FToggleQuotaDiverterDelegate);
DECLARE_DELEGATE_RetVal(float, FQueryStationFloatDelegate);
DECLARE_DELEGATE_RetVal(FVector, FQueryStationVectorDelegate);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnShiftPhaseChangedSignature, EShiftPhase /*NewPhase*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPresentationAssetChangedSignature, const UTareTithePresentationDataAsset* /*PresentationAsset*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnActiveContractLoadedSignature,
	const UMonolithContractDataAsset* /*Contract*/,
	const UShiftProgressionDataAsset* /*ShiftConfig*/,
	int32 /*ContractIndex*/);

struct FShiftSliceableRigBinding
{
	TScriptInterface<ISliceableTargetInterface> SliceableTarget;
	FLoadRigContractDelegate OnLoadContract;
	FQueryStationFloatDelegate OnQueryRemainderMassKg;
	FQueryStationVectorDelegate OnQueryWorldLocation;
};

struct FShiftQuotaReceiverBinding
{
	TScriptInterface<IQuotaReceiverInterface> QuotaReceiver;
	FConfigureQuotaReceiverDelegate OnConfigureContract;
	FToggleQuotaDiverterDelegate OnToggleDiverter;
	FSimpleDelegate OnReconcileOverlaps;
	FQueryStationVectorDelegate OnQueryWorldLocation;
};

UCLASS()
class TAREANDTITHE_API UInspectionShiftSubsystem : public UWorldSubsystem, public ITelemetryPublisherInterface
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	// ITelemetryPublisherInterface
	virtual FOnTelemetryStreamSignature& GetTelemetryStreamDelegate() override { return OnTelemetryStream; }
	virtual FOnTelemetryImpulseSignature& GetTelemetryImpulseDelegate() override { return OnTelemetryImpulse; }

	// Progression & Configuration
	void SetPresentationAsset(const UTareTithePresentationDataAsset* InPresentationAsset);
	const UTareTithePresentationDataAsset* GetActivePresentationAsset() const { return ActivePresentationAsset; }
	void SetShiftProgressionAsset(const UShiftProgressionDataAsset* InProgressionAsset);
	const UShiftProgressionDataAsset* GetActiveShiftProgression() const { return ActiveProgressionAsset; }
	const UMonolithContractDataAsset* GetActiveContract() const { return ActiveContract; }
	int32 GetCurrentContractIndex() const { return CurrentContractIndex; }
	int32 GetTotalContractCount() const;
	EShiftPhase GetCurrentPhase() const { return CurrentPhase; }
	EContractFaultReason GetLastFaultReason() const { return LastFaultReason; }
	FContractRuntimeSnapshot GetCurrentSnapshot() const { return CachedSnapshot; }

	// Zero-Cast Station & Telemetry Self-Registration
	void RegisterSliceableRig(const FShiftSliceableRigBinding& InBinding);
	void UnregisterSliceableRig(UObject* RigObject);
	bool HasRegisteredSliceableRig() const { return IsValid(RigBinding.SliceableTarget.GetObject()); }
	UObject* GetRegisteredSliceableTargetObject() const { return RigBinding.SliceableTarget.GetObject(); }
	FVector GetSliceableRigWorldLocation() const { return QueryRigLocationOrZero(); }

	void RegisterQuotaReceiver(const FShiftQuotaReceiverBinding& InBinding);
	void UnregisterQuotaReceiver(UObject* ReceiverObject);
	bool HasRegisteredQuotaReceiver() const { return IsValid(ReceiverBinding.QuotaReceiver.GetObject()); }
	UObject* GetRegisteredQuotaReceiverObject() const { return ReceiverBinding.QuotaReceiver.GetObject(); }

	void RegisterStrayCatchStation(UObject* StationObject);
	void UnregisterStrayCatchStation(UObject* StationObject);
	bool HasRegisteredStrayCatchStation() const;

	void RegisterDirectorBoothStation(UObject* StationObject);
	void UnregisterDirectorBoothStation(UObject* StationObject);
	bool HasRegisteredDirectorBoothStation() const;

	void RegisterTelemetryPublisher(ITelemetryPublisherInterface* Publisher, UObject* LifetimeOwner);
	void UnregisterTelemetryPublisher(ITelemetryPublisherInterface* Publisher);
	void RegisterAudioTelemetryBridge(UAudioTelemetryBridgeComponent* AudioBridge);
	void UnregisterAudioTelemetryBridge(UAudioTelemetryBridgeComponent* AudioBridge);

	// Gameplay Event Ingress (Conservation of Mass & Fault Orchestration)
	void NotifyChunkSpawnedInFlight(AActor* PayloadActor);
	void NotifySliverVaporizedAsTare(float VaporizedMassKg, const FVector& WorldLocation);
	void NotifySliceCommittedSummary(const FSliceResultSummary& SliceSummary);
	void NotifyPayloadLandedOnScale(AActor* PayloadActor);
	void NotifyPayloadBouncedOffScale(AActor* PayloadActor);
	void NotifyPayloadDisposedToTare(AActor* PayloadActor, float ExplicitMassKg);
	void NotifyPayloadLostAsStrayTare(AActor* PayloadActor, float ExplicitMassKg);
	void NotifyScaleContaminationDetected(AActor* ContaminatedActor);
	void NotifyScaleSettledStateChanged(bool bIsSettled, float SettledMassKg);

	// Operator Controls & Fault / Verdict Resolution
	UFUNCTION(BlueprintCallable, Category = "Shift")
	bool StartShiftFromContract(int32 StartContractIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "Shift")
	bool RequestToggleDiverter();

	UFUNCTION(BlueprintCallable, Category = "Shift")
	bool TryStampQuotaVerdict();

	UFUNCTION(BlueprintCallable, Category = "Shift")
	void RequestManualContractReset();

	UFUNCTION(BlueprintCallable, Category = "Shift")
	void TriggerContractFault(EContractFaultReason FaultReason);

	void TriggerContractFaultAtLocation(EContractFaultReason FaultReason, const FVector& FaultWorldLocation);

	UPROPERTY(BlueprintAssignable, Category = "Shift")
	FOnContractSnapshotUpdatedSignature OnContractSnapshotUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Shift")
	FOnShiftVerdictSignature OnShiftVerdict;

	FOnNativeContractSnapshotUpdatedSignature OnNativeContractSnapshotUpdated;
	FOnShiftPhaseChangedSignature OnShiftPhaseChanged;
	FOnPresentationAssetChangedSignature OnPresentationAssetChanged;
	FOnActiveContractLoadedSignature OnActiveContractLoaded;
	FOnTelemetryStreamSignature OnTelemetryStream;
	FOnTelemetryImpulseSignature OnTelemetryImpulse;

private:
	struct FRegisteredTelemetryPublisherEntry
	{
		ITelemetryPublisherInterface* Publisher = nullptr;
		TWeakObjectPtr<UObject> LifetimeOwner;
	};

	void TryAutoStartShiftIfReady();
	bool EnsureShiftProgressionInitialized();
	UShiftProgressionDataAsset* BuildFallbackGreyboxProgression();
	const UMonolithContractDataAsset* ResolveContractAtIndex(int32 ContractIndex) const;
	bool LoadAndBeginContract(int32 ContractIndex);

	void RecomputeInFlightMassFromSet();
	void AccountPayloadAsTareOnce(AActor* PayloadActor, float ExplicitMassKg);
	void RefreshAndBroadcastSnapshot();
	void EvaluateContractHealth();

	void HandleChunkWatchdogExpired(ASeveredChunkActor* ExpiredChunk);
	void HandleFaultResetTimerElapsed();
	void HandleContractPassTimerElapsed();
	void SetShiftPhase(EShiftPhase NewPhase);

	float QueryLiveRemainderMassKg() const;
	FString ResolveActiveContractDirectiveText() const;
	static FString GetDefaultDirectiveForContractIndex(int32 ContractIndex);
	FVector QueryRigLocationOrZero() const;
	FVector QueryReceiverLocationOrZero() const;

	UPROPERTY(Transient)
	TObjectPtr<const UTareTithePresentationDataAsset> ActivePresentationAsset;

	UPROPERTY(Transient)
	TObjectPtr<const UShiftProgressionDataAsset> ActiveProgressionAsset;

	UPROPERTY(Transient)
	TObjectPtr<UShiftProgressionDataAsset> FallbackGreyboxProgression;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMonolithContractDataAsset>> FallbackGreyboxContracts;

	UPROPERTY(Transient)
	TObjectPtr<const UMonolithContractDataAsset> ActiveContract;

	FShiftSliceableRigBinding RigBinding;
	FShiftQuotaReceiverBinding ReceiverBinding;

	TArray<TWeakObjectPtr<UObject>> RegisteredStrayCatchStations;
	TArray<TWeakObjectPtr<UObject>> RegisteredDirectorBoothStations;
	TArray<FRegisteredTelemetryPublisherEntry> TelemetryPublishers;
	TArray<TWeakObjectPtr<UAudioTelemetryBridgeComponent>> AudioBridges;

	TSet<TWeakObjectPtr<AActor>> ActiveInFlightPayloads;
	TSet<TWeakObjectPtr<AActor>> AccountedTarePayloads;

	FContractRuntimeSnapshot CachedSnapshot;
	FTimerHandle TransitionTimerHandle;

	int32 CurrentContractIndex = 0;
	float InitialContractMassKg = 0.0f;
	float RemainderMassKg = 0.0f;
	float InFlightMassKg = 0.0f;
	float VaporizedTareMassKg = 0.0f;
	float DisposedChunksTareMassKg = 0.0f;
	float AccumulatedTareWasteKg = 0.0f;

	EShiftPhase CurrentPhase = EShiftPhase::WaitingForStations;
	EContractFaultReason LastFaultReason = EContractFaultReason::None;
	bool bWorldHasBegunPlay = false;
};
