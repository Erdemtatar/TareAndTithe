// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/AudioTelemetryBridgeComponent.h"
#include "Components/AudioComponent.h"
#include "CoreGlobals.h"
#include "Data/TareTithePresentationDataAsset.h"
#include "Engine/World.h"
#include "Interfaces/TelemetryPublisherInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Subsystems/InspectionShiftSubsystem.h"
#include "TimerManager.h"

UAudioTelemetryBridgeComponent::UAudioTelemetryBridgeComponent()
{
	// Tier 1: Strictly event-driven telemetry deadband router & two-zone groan controller; 0.00 ms steady-state tick overhead.
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	CachedGroanConfig = FClampGroanAudioConfig().GetSanitized();
	SpatialMinVolumeMultiplier = CachedGroanConfig.SpatialImpulseMinVolumeMultiplier;
	SpatialMaxVolumeMultiplier = CachedGroanConfig.SpatialImpulseMaxVolumeMultiplier;
	CurrentAppliedGroanPitch = CachedGroanConfig.GroanZone1MinPitch;
	LastAmbientBedVolumeMultiplier = CachedGroanConfig.AmbientBedVolumeMultiplier;
	LastMetaSoundClampStrainParam = CachedGroanConfig.MetaSoundClampStrainNeutralValue;

	InitializeDefaultParameterMappings();
}

void UAudioTelemetryBridgeComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeDefaultParameterMappings();

	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			CachedShiftPhase = ShiftSubsystem->GetCurrentPhase();
			bClampGroanMutedByPhaseOrFault =
				(CachedShiftPhase != EShiftPhase::ActiveInspection) ||
				(ShiftSubsystem->GetLastFaultReason() != EContractFaultReason::None);

			ShiftSubsystem->RegisterAudioTelemetryBridge(this);
			ShiftSubsystem->OnPresentationAssetChanged.AddUObject(this, &UAudioTelemetryBridgeComponent::ApplyPresentationData);
			ShiftSubsystem->OnShiftPhaseChanged.AddUObject(this, &UAudioTelemetryBridgeComponent::HandleShiftPhaseChanged);
			ShiftSubsystem->OnActiveContractLoaded.AddUObject(this, &UAudioTelemetryBridgeComponent::HandleActiveContractLoaded);

			if (const UTareTithePresentationDataAsset* ExistingPresentation = ShiftSubsystem->GetActivePresentationAsset())
			{
				ApplyPresentationData(ExistingPresentation);
			}
		}
	}
}

void UAudioTelemetryBridgeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearGroanSurgeTimer();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GroanPostFadeZeroTimerHandle);

		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->OnPresentationAssetChanged.RemoveAll(this);
			ShiftSubsystem->OnShiftPhaseChanged.RemoveAll(this);
			ShiftSubsystem->OnActiveContractLoaded.RemoveAll(this);
			ShiftSubsystem->UnregisterAudioTelemetryBridge(this);
		}
	}

	if (IsValid(ClampGroanAudioComponent) && ClampGroanAudioComponent->IsPlaying())
	{
		ClampGroanAudioComponent->Stop();
	}
	bGroanVoiceActive = false;
	CurrentAppliedGroanVolume = 0.0f;

	UnregisterAllPublishers();
	BoundAudioComponents.Reset();
	ContinuousLoopsAudioComponent = nullptr;
	DiscreteImpulsesAudioComponent = nullptr;
	AmbientBedAudioComponent = nullptr;
	ClampGroanAudioComponent = nullptr;
	CachedClampGroanSound = nullptr;

	Super::EndPlay(EndPlayReason);
}

void UAudioTelemetryBridgeComponent::RegisterPublisher(ITelemetryPublisherInterface* Publisher, UObject* LifetimeOwner)
{
	if (Publisher == nullptr)
	{
		return;
	}

	if (LifetimeOwner != nullptr && !IsValid(LifetimeOwner))
	{
		return;
	}

	for (const FPublisherSubscription& Existing : ActiveSubscriptions)
	{
		if (Existing.Publisher == Publisher)
		{
			return;
		}
	}

	FPublisherSubscription NewSub;
	NewSub.Publisher = Publisher;
	NewSub.LifetimeOwner = LifetimeOwner;
	NewSub.bTrackOwnerLifetime = (LifetimeOwner != nullptr);
	NewSub.StreamDelegateHandle = Publisher->GetTelemetryStreamDelegate().AddUObject(
		this,
		&UAudioTelemetryBridgeComponent::HandlePublisherStream);
	NewSub.ImpulseDelegateHandle = Publisher->GetTelemetryImpulseDelegate().AddUObject(
		this,
		&UAudioTelemetryBridgeComponent::HandlePublisherImpulse);

	ActiveSubscriptions.Add(NewSub);
}

void UAudioTelemetryBridgeComponent::UnregisterPublisher(ITelemetryPublisherInterface* Publisher)
{
	if (Publisher == nullptr)
	{
		return;
	}

	for (int32 Idx = ActiveSubscriptions.Num() - 1; Idx >= 0; --Idx)
	{
		FPublisherSubscription& Sub = ActiveSubscriptions[Idx];
		if (Sub.Publisher == Publisher)
		{
			if (!Sub.bTrackOwnerLifetime || Sub.LifetimeOwner.IsValid())
			{
				Sub.Publisher->GetTelemetryStreamDelegate().Remove(Sub.StreamDelegateHandle);
				Sub.Publisher->GetTelemetryImpulseDelegate().Remove(Sub.ImpulseDelegateHandle);
			}
			ActiveSubscriptions.RemoveAtSwap(Idx, EAllowShrinking::No);
		}
	}
}

void UAudioTelemetryBridgeComponent::UnregisterAllPublishers()
{
	for (FPublisherSubscription& Sub : ActiveSubscriptions)
	{
		if (Sub.Publisher != nullptr && (!Sub.bTrackOwnerLifetime || Sub.LifetimeOwner.IsValid()))
		{
			Sub.Publisher->GetTelemetryStreamDelegate().Remove(Sub.StreamDelegateHandle);
			Sub.Publisher->GetTelemetryImpulseDelegate().Remove(Sub.ImpulseDelegateHandle);
		}
	}
	ActiveSubscriptions.Reset();
}

void UAudioTelemetryBridgeComponent::SetContinuousLoopsAudioComponent(UAudioComponent* InAudioComponent)
{
	ContinuousLoopsAudioComponent = InAudioComponent;
	ConfigurePersistentAudioComponent(ContinuousLoopsAudioComponent);

	if (bAutoPlayAssignedAudioComponents &&
		IsValid(ContinuousLoopsAudioComponent) &&
		IsValid(ContinuousLoopsAudioComponent->GetSound()) &&
		!ContinuousLoopsAudioComponent->IsPlaying())
	{
		ContinuousLoopsAudioComponent->Play();
	}

	PushNeutralClampStrainToMetaSound();
}

void UAudioTelemetryBridgeComponent::SetDiscreteImpulsesAudioComponent(UAudioComponent* InAudioComponent)
{
	DiscreteImpulsesAudioComponent = InAudioComponent;
	ConfigurePersistentAudioComponent(DiscreteImpulsesAudioComponent);
}

void UAudioTelemetryBridgeComponent::SetAmbientBedAudioComponent(UAudioComponent* InAudioComponent)
{
	AmbientBedAudioComponent = InAudioComponent;
	ConfigurePersistentAudioComponent(AmbientBedAudioComponent);

	if (IsValid(AmbientBedAudioComponent))
	{
		AmbientBedAudioComponent->bAllowSpatialization = false;
		AmbientBedAudioComponent->SetVolumeMultiplier(CachedGroanConfig.AmbientBedVolumeMultiplier);
		LastAmbientBedVolumeMultiplier = CachedGroanConfig.AmbientBedVolumeMultiplier;

		if (bAutoPlayAssignedAudioComponents &&
			IsValid(AmbientBedAudioComponent->GetSound()) &&
			!AmbientBedAudioComponent->IsPlaying())
		{
			AmbientBedAudioComponent->Play();
		}
	}
}

void UAudioTelemetryBridgeComponent::SetClampGroanAudioComponent(UAudioComponent* InAudioComponent)
{
	ClampGroanAudioComponent = InAudioComponent;
	ConfigurePersistentAudioComponent(ClampGroanAudioComponent);

	if (IsValid(ClampGroanAudioComponent))
	{
		ClampGroanAudioComponent->bAllowSpatialization = false;
		ClampGroanAudioComponent->SetVolumeMultiplier(0.0f);
		ClampGroanAudioComponent->SetPitchMultiplier(CachedGroanConfig.GroanZone1MinPitch);

		if (USoundBase* GroanSound = ResolveClampGroanSoundAsset())
		{
			CachedClampGroanSound = GroanSound;
			if (ClampGroanAudioComponent->GetSound() != GroanSound)
			{
				ClampGroanAudioComponent->SetSound(GroanSound);
			}
		}
	}

	bGroanVoiceActive = false;
	CurrentAppliedGroanVolume = 0.0f;
	RefreshClampGroanEnvelope();
}

void UAudioTelemetryBridgeComponent::ApplyPresentationData(const UTareTithePresentationDataAsset* InPresentationData)
{
	PresentationData = InPresentationData;
	CachedGroanConfig = IsValid(PresentationData)
		? PresentationData->ClampGroanAudio.GetSanitized()
		: FClampGroanAudioConfig().GetSanitized();

	SpatialMinVolumeMultiplier = CachedGroanConfig.SpatialImpulseMinVolumeMultiplier;
	SpatialMaxVolumeMultiplier = CachedGroanConfig.SpatialImpulseMaxVolumeMultiplier;

	if (IsValid(ClampGroanAudioComponent))
	{
		ConfigurePersistentAudioComponent(ClampGroanAudioComponent);
		ClampGroanAudioComponent->bAllowSpatialization = false;
		if (USoundBase* GroanSound = ResolveClampGroanSoundAsset())
		{
			CachedClampGroanSound = GroanSound;
			if (ClampGroanAudioComponent->GetSound() != GroanSound)
			{
				ClampGroanAudioComponent->SetSound(GroanSound);
			}
		}
	}

	if (!IsValid(PresentationData))
	{
		RefreshClampGroanEnvelope();
		return;
	}

	if (IsValid(ContinuousLoopsAudioComponent) && IsValid(PresentationData->ContinuousLoopsMetaSound))
	{
		ConfigurePersistentAudioComponent(ContinuousLoopsAudioComponent);
		if (ContinuousLoopsAudioComponent->GetSound() != PresentationData->ContinuousLoopsMetaSound)
		{
			ContinuousLoopsAudioComponent->SetSound(PresentationData->ContinuousLoopsMetaSound);
		}
		if (bAutoPlayAssignedAudioComponents && !ContinuousLoopsAudioComponent->IsPlaying())
		{
			ContinuousLoopsAudioComponent->Play();
		}
		PushNeutralClampStrainToMetaSound();
	}

	if (IsValid(AmbientBedAudioComponent) && IsValid(PresentationData->AmbientCellBedSound))
	{
		ConfigurePersistentAudioComponent(AmbientBedAudioComponent);
		AmbientBedAudioComponent->bAllowSpatialization = false;
		if (AmbientBedAudioComponent->GetSound() != PresentationData->AmbientCellBedSound)
		{
			AmbientBedAudioComponent->SetSound(PresentationData->AmbientCellBedSound);
		}
		AmbientBedAudioComponent->SetVolumeMultiplier(CachedGroanConfig.AmbientBedVolumeMultiplier);
		LastAmbientBedVolumeMultiplier = CachedGroanConfig.AmbientBedVolumeMultiplier;
		if (bAutoPlayAssignedAudioComponents && !AmbientBedAudioComponent->IsPlaying())
		{
			AmbientBedAudioComponent->Play();
		}
	}

	if (IsValid(DiscreteImpulsesAudioComponent) && IsValid(PresentationData->DiscreteImpulsesMetaSound))
	{
		ConfigurePersistentAudioComponent(DiscreteImpulsesAudioComponent);
		if (DiscreteImpulsesAudioComponent->GetSound() != PresentationData->DiscreteImpulsesMetaSound)
		{
			DiscreteImpulsesAudioComponent->SetSound(PresentationData->DiscreteImpulsesMetaSound);
		}
	}

	RefreshClampGroanEnvelope();
}

void UAudioTelemetryBridgeComponent::RegisterAudioComponent(UAudioComponent* InAudioComponent)
{
	if (!IsValid(InAudioComponent))
	{
		return;
	}

	ConfigurePersistentAudioComponent(InAudioComponent);
	BoundAudioComponents.AddUnique(InAudioComponent);

	if (bAutoPlayAssignedAudioComponents && IsValid(InAudioComponent->GetSound()) && !InAudioComponent->IsPlaying())
	{
		InAudioComponent->Play();
	}
}

void UAudioTelemetryBridgeComponent::UnregisterAudioComponent(UAudioComponent* InAudioComponent)
{
	BoundAudioComponents.RemoveAllSwap([InAudioComponent](const TWeakObjectPtr<UAudioComponent>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == InAudioComponent;
	}, EAllowShrinking::No);
}

void UAudioTelemetryBridgeComponent::ClearAudioComponents()
{
	ClearGroanSurgeTimer();
	BoundAudioComponents.Reset();
	ContinuousLoopsAudioComponent = nullptr;
	DiscreteImpulsesAudioComponent = nullptr;
	AmbientBedAudioComponent = nullptr;
	ClampGroanAudioComponent = nullptr;
	bGroanVoiceActive = false;
	CurrentAppliedGroanVolume = 0.0f;
}

void UAudioTelemetryBridgeComponent::ResetStreamCache()
{
	LastFlushedStreamValues.Reset();
	CurrentRawClampStrain = 0.0f;
	CurrentRawClampFatigue = 0.0f;
	bHasBaselineContractStrainSample = false;
	bHasBaselineContractFatigueSample = false;
	ClearGroanSurgeTimer();
	PushZeroedLoopsToMetaSound();
}

float UAudioTelemetryBridgeComponent::GetLastFlushedStreamValue(ETelemetryStreamId StreamId) const
{
	if (const float* FoundValue = LastFlushedStreamValues.Find(StreamId))
	{
		return *FoundValue;
	}
	return 0.0f;
}

void UAudioTelemetryBridgeComponent::InitializeDefaultParameterMappings()
{
	if (!StreamParameterNames.Contains(ETelemetryStreamId::VeinProximity))
	{
		StreamParameterNames.Add(ETelemetryStreamId::VeinProximity, TEXT("VeinProximity"));
	}
	if (!StreamParameterNames.Contains(ETelemetryStreamId::ClampStrain))
	{
		StreamParameterNames.Add(ETelemetryStreamId::ClampStrain, TEXT("ClampStrain"));
	}
	if (!StreamParameterNames.Contains(ETelemetryStreamId::ClampFatigue))
	{
		StreamParameterNames.Add(ETelemetryStreamId::ClampFatigue, TEXT("ClampFatigue"));
	}
	if (!StreamParameterNames.Contains(ETelemetryStreamId::LaserCharge))
	{
		StreamParameterNames.Add(ETelemetryStreamId::LaserCharge, TEXT("LaserCharge"));
	}
	if (!StreamParameterNames.Contains(ETelemetryStreamId::ScaleFillRatio))
	{
		StreamParameterNames.Add(ETelemetryStreamId::ScaleFillRatio, TEXT("ScaleFillRatio"));
	}

	if (!ImpulseTriggerNames.Contains(ETelemetryImpulseId::SliceCommitted))
	{
		ImpulseTriggerNames.Add(ETelemetryImpulseId::SliceCommitted, TEXT("OnSliceCommitted"));
	}
	if (!ImpulseTriggerNames.Contains(ETelemetryImpulseId::SliverVaporized))
	{
		ImpulseTriggerNames.Add(ETelemetryImpulseId::SliverVaporized, TEXT("OnSliverVaporized"));
	}
	if (!ImpulseTriggerNames.Contains(ETelemetryImpulseId::ChunkScaleImpact))
	{
		ImpulseTriggerNames.Add(ETelemetryImpulseId::ChunkScaleImpact, TEXT("OnChunkScaleImpact"));
	}
	if (!ImpulseTriggerNames.Contains(ETelemetryImpulseId::ChunkTareDisposed))
	{
		ImpulseTriggerNames.Add(ETelemetryImpulseId::ChunkTareDisposed, TEXT("OnChunkTareDisposed"));
	}
	if (!ImpulseTriggerNames.Contains(ETelemetryImpulseId::DiverterToggled))
	{
		ImpulseTriggerNames.Add(ETelemetryImpulseId::DiverterToggled, TEXT("OnDiverterToggled"));
	}
	if (!ImpulseTriggerNames.Contains(ETelemetryImpulseId::VerdictStamped))
	{
		ImpulseTriggerNames.Add(ETelemetryImpulseId::VerdictStamped, TEXT("OnVerdictStamped"));
	}
	if (!ImpulseTriggerNames.Contains(ETelemetryImpulseId::FaultTriggered))
	{
		ImpulseTriggerNames.Add(ETelemetryImpulseId::FaultTriggered, TEXT("OnFaultTriggered"));
	}
}

void UAudioTelemetryBridgeComponent::HandlePublisherStream(ETelemetryStreamId StreamId, float NormalizedValue)
{
	const float ClampedValue = FMath::Clamp(NormalizedValue, 0.0f, 1.0f);
	const float SafeDeadband = FMath::Max(0.0001f, StreamDeadband);
	const float* PreviousPtr = LastFlushedStreamValues.Find(StreamId);

	const bool bIsExactBoundary = (ClampedValue == 0.0f) || (ClampedValue == 1.0f);
	const bool bShouldForward =
		(PreviousPtr == nullptr) ||
		(FMath::Abs(ClampedValue - *PreviousPtr) >= SafeDeadband) ||
		bIsExactBoundary;

	if (!bShouldForward)
	{
		return;
	}

	LastFlushedStreamValues.Add(StreamId, ClampedValue);

	const FName ParameterName = ResolveStreamParameterName(StreamId);
	if (!ParameterName.IsNone())
	{
		if (StreamId == ETelemetryStreamId::ClampStrain)
		{
			// Pin MS_TelemetryLoops "ClampStrain" to 0.15f so its unclamped MapRange(0.15..1.0 -> 0.0..0.65) outputs exact 0.000f mono bleed,
			// while dedicated 2D Stereo ClampGroanAudioComponent handles all SFX_ClampSteelGroan_Loop playback & pitch tension.
			LastMetaSoundClampStrainParam = CachedGroanConfig.MetaSoundClampStrainNeutralValue;
			ForwardFloatToBoundAudio(ParameterName, LastMetaSoundClampStrainParam);
		}
		else
		{
			const float SafeLoopParam = (bClampGroanMutedByPhaseOrFault || !IsInActiveInspectionPhase()) ? 0.0f : ClampedValue;
			ForwardFloatToBoundAudio(ParameterName, SafeLoopParam);
		}
	}

	if (StreamId == ETelemetryStreamId::ClampStrain)
	{
		const float DeltaStrain = ClampedValue - CurrentRawClampStrain;
		CurrentRawClampStrain = ClampedValue;

		if (!bClampGroanMutedByPhaseOrFault && IsInActiveInspectionPhase())
		{
			if (bHasBaselineContractStrainSample &&
				ClampedValue >= CachedGroanConfig.GroanOnsetStrainThreshold &&
				DeltaStrain >= CachedGroanConfig.GroanTorqueSpikeMinDelta)
			{
				ArmClampGroanSurge();
			}
			bHasBaselineContractStrainSample = true;
			RefreshClampGroanEnvelope();
		}
		else
		{
			// Latch initial contract load strain emitted during ContractIntro so ActiveInspection entry starts at calm steady tension.
			bHasBaselineContractStrainSample = true;
		}
	}
	else if (StreamId == ETelemetryStreamId::ClampFatigue)
	{
		const float DeltaFatigue = ClampedValue - CurrentRawClampFatigue;
		CurrentRawClampFatigue = ClampedValue;

		if (!bClampGroanMutedByPhaseOrFault && IsInActiveInspectionPhase())
		{
			if (bHasBaselineContractFatigueSample &&
				DeltaFatigue >= CachedGroanConfig.GroanFatigueSpikeMinDelta)
			{
				ArmClampGroanSurge();
			}
			bHasBaselineContractFatigueSample = true;
			RefreshClampGroanEnvelope();
		}
		else
		{
			bHasBaselineContractFatigueSample = true;
		}
	}

	OnFilteredTelemetryStream.Broadcast(StreamId, ClampedValue);
}

void UAudioTelemetryBridgeComponent::HandlePublisherImpulse(
	ETelemetryImpulseId ImpulseId,
	float NormalizedMagnitude,
	const FVector& WorldLocation)
{
	if (ImpulseId == ETelemetryImpulseId::FaultTriggered)
	{
		// Phase 12-FIX5: Debounce same-frame duplicate FaultTriggered broadcasts (e.g., ClampTorqueSolverComponent
		// followed immediately by UInspectionShiftSubsystem::TriggerContractFault) while keeping ClampTorqueSolverComponent untouched.
		if (LastFaultImpulseFrameNumber == GFrameCounter)
		{
			++SuppressedDuplicateFaultImpulseCount;
			return;
		}
		LastFaultImpulseFrameNumber = GFrameCounter;
		LastFaultFrameDispatchedImpulseCount = 1;
	}

	const float ClampedMagnitude = FMath::Clamp(NormalizedMagnitude, 0.0f, 1.0f);

	SpawnSpatialImpulseSoundIfAssigned(ImpulseId, ClampedMagnitude, WorldLocation);

	const FName TriggerName = ResolveImpulseTriggerName(ImpulseId);
	if (!TriggerName.IsNone())
	{
		ForwardImpulseToBoundAudio(TriggerName, ClampedMagnitude);
	}

	if (ImpulseId == ETelemetryImpulseId::FaultTriggered)
	{
		MuteAndCutoffLoopsForFaultOrPhaseExit();
	}
	else if (ImpulseId == ETelemetryImpulseId::SliceCommitted)
	{
		if (!bClampGroanMutedByPhaseOrFault &&
			IsInActiveInspectionPhase() &&
			CurrentRawClampStrain >= CachedGroanConfig.GroanOnsetStrainThreshold)
		{
			ArmClampGroanSurge();
			RefreshClampGroanEnvelope();
		}
	}

	OnForwardedTelemetryImpulse.Broadcast(ImpulseId, ClampedMagnitude, WorldLocation);
}

void UAudioTelemetryBridgeComponent::HandleShiftPhaseChanged(EShiftPhase NewPhase)
{
	CachedShiftPhase = NewPhase;

	if (NewPhase != EShiftPhase::ActiveInspection)
	{
		MuteAndCutoffLoopsForFaultOrPhaseExit();
		return;
	}

	LastFaultImpulseFrameNumber = MAX_uint64;

	bool bHasActiveFault = false;
	if (UWorld* World = GetWorld())
	{
		if (const UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			bHasActiveFault = (ShiftSubsystem->GetLastFaultReason() != EContractFaultReason::None);
		}
	}

	if (bHasActiveFault)
	{
		MuteAndCutoffLoopsForFaultOrPhaseExit();
		return;
	}

	bClampGroanMutedByPhaseOrFault = false;
	bHasBaselineContractStrainSample = true;
	bHasBaselineContractFatigueSample = true;
	PushNeutralClampStrainToMetaSound();
	RefreshClampGroanEnvelope();
}

void UAudioTelemetryBridgeComponent::HandleActiveContractLoaded(
	const UMonolithContractDataAsset* /*Contract*/,
	const UShiftProgressionDataAsset* /*ShiftConfig*/,
	int32 /*ContractIndex*/)
{
	ClearGroanSurgeTimer();

	if (UWorld* World = GetWorld())
	{
		if (const UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			CachedShiftPhase = ShiftSubsystem->GetCurrentPhase();
			const bool bCleanActive =
				(CachedShiftPhase == EShiftPhase::ActiveInspection) &&
				(ShiftSubsystem->GetLastFaultReason() == EContractFaultReason::None);

			if (!bCleanActive)
			{
				MuteAndCutoffLoopsForFaultOrPhaseExit();
				return;
			}
		}
	}

	bClampGroanMutedByPhaseOrFault = false;
	bHasBaselineContractStrainSample = true;
	bHasBaselineContractFatigueSample = true;
	PushNeutralClampStrainToMetaSound();
	RefreshClampGroanEnvelope();
}

bool UAudioTelemetryBridgeComponent::IsInActiveInspectionPhase() const
{
	return CachedShiftPhase == EShiftPhase::ActiveInspection;
}

USoundBase* UAudioTelemetryBridgeComponent::ResolveClampGroanSoundAsset() const
{
	if (IsValid(CachedClampGroanSound))
	{
		return CachedClampGroanSound;
	}

	if (!CachedGroanConfig.ClampSteelGroanLoopSound.ToSoftObjectPath().IsNull())
	{
		if (USoundBase* LoadedSoftSound = CachedGroanConfig.ClampSteelGroanLoopSound.LoadSynchronous())
		{
			return LoadedSoftSound;
		}
	}

	static const TCHAR* FallbackGroanWavePath = TEXT("/Game/Presentation/Audio/Waves/SFX_ClampSteelGroan_Loop.SFX_ClampSteelGroan_Loop");
	if (FPackageName::DoesPackageExist(TEXT("/Game/Presentation/Audio/Waves/SFX_ClampSteelGroan_Loop")))
	{
		return LoadObject<USoundBase>(nullptr, FallbackGroanWavePath);
	}

	return nullptr;
}

void UAudioTelemetryBridgeComponent::PushNeutralClampStrainToMetaSound()
{
	LastMetaSoundClampStrainParam = CachedGroanConfig.MetaSoundClampStrainNeutralValue;
	const FName ClampStrainParam = ResolveStreamParameterName(ETelemetryStreamId::ClampStrain);
	if (!ClampStrainParam.IsNone())
	{
		ForwardFloatToBoundAudio(ClampStrainParam, LastMetaSoundClampStrainParam);
	}
}

void UAudioTelemetryBridgeComponent::PushZeroedLoopsToMetaSound()
{
	PushNeutralClampStrainToMetaSound();

	const ETelemetryStreamId ZeroedStreams[] = {
		ETelemetryStreamId::LaserCharge,
		ETelemetryStreamId::VeinProximity,
		ETelemetryStreamId::ClampFatigue,
		ETelemetryStreamId::ScaleFillRatio
	};
	for (ETelemetryStreamId StreamId : ZeroedStreams)
	{
		const FName ParamName = ResolveStreamParameterName(StreamId);
		if (!ParamName.IsNone())
		{
			ForwardFloatToBoundAudio(ParamName, 0.0f);
		}
	}
}

void UAudioTelemetryBridgeComponent::EvaluateSteadyClampGroanCurve(
	float& OutSteadyVolume,
	float& OutSteadyPitch,
	float& OutEffectiveDanger) const
{
	const FClampGroanAudioConfig& Cfg = CachedGroanConfig;
	const float S = FMath::Clamp(CurrentRawClampStrain, 0.0f, 1.0f);
	const float F = FMath::Clamp(CurrentRawClampFatigue, 0.0f, 1.0f);

	// 1. Silent Balanced Zone: ClampStrain < 0.46f and ClampFatigue < 0.05f -> 0.0f volume.
	if (S < Cfg.GroanOnsetStrainThreshold && F < Cfg.GroanOnsetFatigueThreshold)
	{
		OutSteadyVolume = 0.0f;
		OutSteadyPitch = Cfg.GroanZone1MinPitch;
		OutEffectiveDanger = 0.0f;
		return;
	}

	// 2. Zone 1 — Moderate Resting Overhang: ClampStrain in [0.46f, 0.66f) and ClampFatigue < 0.15f.
	if (S < Cfg.GroanZone2StrainThreshold && F < Cfg.GroanZone2FatigueThreshold)
	{
		const float StrainSpan = FMath::Max(0.01f, Cfg.GroanZone2StrainThreshold - Cfg.GroanOnsetStrainThreshold);
		const float FatigueSpan = FMath::Max(0.01f, Cfg.GroanZone2FatigueThreshold - Cfg.GroanOnsetFatigueThreshold);
		const float StrainAlpha = FMath::Clamp((S - Cfg.GroanOnsetStrainThreshold) / StrainSpan, 0.0f, 1.0f);
		const float FatigueAlpha = FMath::Clamp((F - Cfg.GroanOnsetFatigueThreshold) / FatigueSpan, 0.0f, 1.0f);
		const float Zone1Alpha = FMath::Max(StrainAlpha, FatigueAlpha);

		OutSteadyVolume = FMath::Lerp(Cfg.GroanZone1MinVolume, Cfg.GroanZone1MaxVolume, Zone1Alpha);
		OutSteadyPitch = FMath::Lerp(Cfg.GroanZone1MinPitch, Cfg.GroanZone1MaxPitch, Zone1Alpha);
		OutEffectiveDanger = FMath::Lerp(0.08f, 0.45f, Zone1Alpha);
		return;
	}

	// 3. Zone 2 — Imminent Tip-Over / Shear Danger: ClampStrain >= 0.66f OR ClampFatigue >= 0.15f.
	const float MidFatiguePoint = 0.5f * (Cfg.GroanZone2FatigueThreshold + Cfg.GroanCriticalFatigueThreshold);
	const float CritStrainSpan = FMath::Max(0.01f, Cfg.GroanCriticalStrainThreshold - Cfg.GroanZone2StrainThreshold);
	const float CritFatigueSpan = FMath::Max(0.01f, MidFatiguePoint - Cfg.GroanZone2FatigueThreshold);

	const float CritAlpha = FMath::Max(
		FMath::Clamp((S - Cfg.GroanZone2StrainThreshold) / CritStrainSpan, 0.0f, 1.0f),
		FMath::Clamp((F - Cfg.GroanZone2FatigueThreshold) / CritFatigueSpan, 0.0f, 1.0f));

	const float OverdriveStrainSpan = FMath::Max(0.01f, 1.0f - Cfg.GroanCriticalStrainThreshold);
	const float OverdriveFatigueSpan = FMath::Max(0.01f, Cfg.GroanCriticalFatigueThreshold - MidFatiguePoint);

	const float OverdriveAlpha = FMath::Max(
		FMath::Clamp((S - Cfg.GroanCriticalStrainThreshold) / OverdriveStrainSpan, 0.0f, 1.0f),
		FMath::Clamp((F - MidFatiguePoint) / OverdriveFatigueSpan, 0.0f, 1.0f));

	OutSteadyVolume =
		FMath::Lerp(Cfg.GroanZone2EntryVolume, Cfg.GroanCriticalVolume, CritAlpha) +
		(OverdriveAlpha * (Cfg.GroanPreSnapVolume - Cfg.GroanCriticalVolume));

	OutSteadyPitch =
		FMath::Lerp(Cfg.GroanZone2EntryPitch, Cfg.GroanCriticalPitch, CritAlpha) +
		(OverdriveAlpha * (Cfg.GroanMaxPitchMultiplier - Cfg.GroanCriticalPitch));

	OutEffectiveDanger = FMath::Clamp(FMath::Lerp(0.50f, 0.85f, CritAlpha) + (0.15f * OverdriveAlpha), 0.0f, 1.0f);
}

void UAudioTelemetryBridgeComponent::RefreshClampGroanEnvelope()
{
	if (bClampGroanMutedByPhaseOrFault || !IsInActiveInspectionPhase())
	{
		SilenceClampGroanFast();
		return;
	}

	float SteadyVolume = 0.0f;
	float SteadyPitch = CachedGroanConfig.GroanZone1MinPitch;
	float SteadyDanger = 0.0f;
	EvaluateSteadyClampGroanCurve(SteadyVolume, SteadyPitch, SteadyDanger);

	const FClampGroanAudioConfig& Cfg = CachedGroanConfig;
	float EffectiveVolume = SteadyVolume;
	float EffectivePitch = SteadyPitch;

	if (SurgeStage > 0)
	{
		const float PeakSurgeVolume = FMath::Clamp(
			FMath::Max(SteadyVolume, Cfg.GroanSurgeBaseFloorVolume) + Cfg.GroanSliceSurgeVolumeBoost,
			Cfg.GroanSurgeMinClampVolume,
			Cfg.GroanMaxVolumeMultiplier);

		if (SurgeStage >= 2)
		{
			EffectiveVolume = PeakSurgeVolume;
			EffectivePitch = FMath::Clamp(SteadyPitch + Cfg.GroanSliceSurgePitchBoost, 0.50f, 1.18f);
			SteadyDanger = FMath::Max(SteadyDanger, 0.65f);
		}
		else
		{
			EffectiveVolume = 0.5f * (PeakSurgeVolume + SteadyVolume);
			EffectivePitch = FMath::Clamp(SteadyPitch + (0.5f * Cfg.GroanSliceSurgePitchBoost), 0.50f, 1.18f);
			SteadyDanger = FMath::Max(SteadyDanger, 0.40f);
		}
	}

	CurrentEffectiveDanger = SteadyDanger;

	if (EffectiveVolume <= KINDA_SMALL_NUMBER)
	{
		SilenceClampGroanFast();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GroanPostFadeZeroTimerHandle);
	}

	CurrentAppliedGroanVolume = EffectiveVolume;
	CurrentAppliedGroanPitch = EffectivePitch;

	if (!IsValid(ClampGroanAudioComponent))
	{
		return;
	}

	if (!IsValid(ClampGroanAudioComponent->GetSound()))
	{
		if (USoundBase* GroanSound = ResolveClampGroanSoundAsset())
		{
			CachedClampGroanSound = GroanSound;
			ClampGroanAudioComponent->SetSound(GroanSound);
		}
	}

	if (!IsValid(ClampGroanAudioComponent->GetSound()))
	{
		return;
	}

	if (!bGroanVoiceActive || !ClampGroanAudioComponent->IsPlaying())
	{
		// Retire any in-progress <= 0.08s FadeOut FActiveSound before re-arming so a pending fade-stop never kills the new voice.
		if (ClampGroanAudioComponent->IsPlaying())
		{
			ClampGroanAudioComponent->Stop();
		}

		ClampGroanAudioComponent->SetVolumeMultiplier(EffectiveVolume);
		ClampGroanAudioComponent->SetPitchMultiplier(EffectivePitch);
		ClampGroanAudioComponent->FadeIn(Cfg.GroanFadeInSeconds, 1.0f, 0.0f);
		bGroanVoiceActive = true;
	}
	else
	{
		// Correction C: Internal fade envelope stays strictly at 1.0f; modulate level exclusively via SetVolumeMultiplier / SetPitchMultiplier.
		ClampGroanAudioComponent->SetVolumeMultiplier(EffectiveVolume);
		ClampGroanAudioComponent->SetPitchMultiplier(EffectivePitch);
	}
}

void UAudioTelemetryBridgeComponent::ArmClampGroanSurge()
{
	if (bClampGroanMutedByPhaseOrFault || !IsInActiveInspectionPhase())
	{
		return;
	}

	SurgeStage = 2;

	if (UWorld* World = GetWorld())
	{
		const float TotalSurgeSec = FMath::Clamp(CachedGroanConfig.GroanSliceSurgeDurationSeconds, 0.10f, 2.00f);
		const float PeakHoldSec = FMath::Max(0.05f, TotalSurgeSec * 0.65f);
		World->GetTimerManager().SetTimer(
			GroanSurgeTimerHandle,
			this,
			&UAudioTelemetryBridgeComponent::HandleGroanSurgeTimerStep,
			PeakHoldSec,
			false);
	}
}

void UAudioTelemetryBridgeComponent::HandleGroanSurgeTimerStep()
{
	if (bClampGroanMutedByPhaseOrFault || !IsInActiveInspectionPhase())
	{
		ClearGroanSurgeTimer();
		SilenceClampGroanFast();
		return;
	}

	if (SurgeStage >= 2)
	{
		SurgeStage = 1;
		RefreshClampGroanEnvelope();

		if (UWorld* World = GetWorld())
		{
			const float TotalSurgeSec = FMath::Clamp(CachedGroanConfig.GroanSliceSurgeDurationSeconds, 0.10f, 2.00f);
			const float TailStepSec = FMath::Max(0.04f, TotalSurgeSec * 0.35f);
			World->GetTimerManager().SetTimer(
				GroanSurgeTimerHandle,
				this,
				&UAudioTelemetryBridgeComponent::HandleGroanSurgeTimerStep,
				TailStepSec,
				false);
		}
	}
	else
	{
		SurgeStage = 0;
		RefreshClampGroanEnvelope();
	}
}

void UAudioTelemetryBridgeComponent::ClearGroanSurgeTimer()
{
	SurgeStage = 0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GroanSurgeTimerHandle);
	}
}

void UAudioTelemetryBridgeComponent::SilenceClampGroanFast()
{
	CurrentAppliedGroanVolume = 0.0f;
	CurrentAppliedGroanPitch = CachedGroanConfig.GroanZone1MinPitch;
	CurrentEffectiveDanger = 0.0f;

	const bool bWasActive = bGroanVoiceActive;
	bGroanVoiceActive = false;

	if (!IsValid(ClampGroanAudioComponent))
	{
		return;
	}

	const float CutoffSec = FMath::Clamp(CachedGroanConfig.GroanFastCutoffSeconds, 0.01f, 0.12f);
	if (bWasActive || ClampGroanAudioComponent->IsPlaying())
	{
		ClampGroanAudioComponent->FadeOut(CutoffSec, 0.0f);

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				GroanPostFadeZeroTimerHandle,
				this,
				&UAudioTelemetryBridgeComponent::HandlePostFadeZeroVolumeElapsed,
				CutoffSec + 0.01f,
				false);
		}
	}
	else
	{
		ClampGroanAudioComponent->SetVolumeMultiplier(0.0f);
	}
}

void UAudioTelemetryBridgeComponent::HandlePostFadeZeroVolumeElapsed()
{
	if (!bGroanVoiceActive && IsValid(ClampGroanAudioComponent))
	{
		if (ClampGroanAudioComponent->IsPlaying())
		{
			ClampGroanAudioComponent->Stop();
		}
		ClampGroanAudioComponent->SetVolumeMultiplier(0.0f);
	}
}

void UAudioTelemetryBridgeComponent::MuteAndCutoffLoopsForFaultOrPhaseExit()
{
	bClampGroanMutedByPhaseOrFault = true;
	ClearGroanSurgeTimer();
	PushZeroedLoopsToMetaSound();
	SilenceClampGroanFast();
}

void UAudioTelemetryBridgeComponent::SpawnSpatialImpulseSoundIfAssigned(
	ETelemetryImpulseId ImpulseId,
	float ClampedMagnitude,
	const FVector& WorldLocation)
{
	if (!IsValid(PresentationData))
	{
		return;
	}

	const TObjectPtr<USoundBase>* FoundSpatialSound = PresentationData->SpatialImpulseSounds.Find(ImpulseId);
	if (FoundSpatialSound == nullptr || !IsValid(*FoundSpatialSound))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const float VolumeMultiplier = FMath::Lerp(SpatialMinVolumeMultiplier, SpatialMaxVolumeMultiplier, ClampedMagnitude);
	const float PitchMultiplier = FMath::Lerp(SpatialMinPitchMultiplier, SpatialMaxPitchMultiplier, ClampedMagnitude);

	UAudioComponent* SpawnedSpatialAudioComp = UGameplayStatics::SpawnSoundAtLocation(
		World,
		FoundSpatialSound->Get(),
		WorldLocation,
		FRotator::ZeroRotator,
		VolumeMultiplier,
		PitchMultiplier,
		0.0f,
		PresentationData->DefaultSpatialAttenuation.Get(),
		nullptr,
		true);

	if (IsValid(SpawnedSpatialAudioComp) && !SpatialOneShotMagnitudeParameterName.IsNone())
	{
		SpawnedSpatialAudioComp->SetFloatParameter(SpatialOneShotMagnitudeParameterName, ClampedMagnitude);
	}
}

void UAudioTelemetryBridgeComponent::ForwardFloatToBoundAudio(FName ParameterName, float Value)
{
	if (IsValid(ContinuousLoopsAudioComponent) && IsValid(ContinuousLoopsAudioComponent->GetSound()))
	{
		if (bAutoPlayAssignedAudioComponents && !ContinuousLoopsAudioComponent->IsPlaying())
		{
			ContinuousLoopsAudioComponent->Play();
		}
		ContinuousLoopsAudioComponent->SetFloatParameter(ParameterName, Value);
	}

	for (int32 Idx = BoundAudioComponents.Num() - 1; Idx >= 0; --Idx)
	{
		UAudioComponent* AudioComp = BoundAudioComponents[Idx].Get();
		if (!IsValid(AudioComp))
		{
			BoundAudioComponents.RemoveAtSwap(Idx, EAllowShrinking::No);
			continue;
		}

		// Safe greybox no-op when no USoundBase / UMetaSoundSource is assigned yet.
		if (!IsValid(AudioComp->GetSound()))
		{
			continue;
		}

		if (bAutoPlayAssignedAudioComponents && !AudioComp->IsPlaying())
		{
			AudioComp->Play();
		}

		AudioComp->SetFloatParameter(ParameterName, Value);
	}
}

void UAudioTelemetryBridgeComponent::ForwardImpulseToBoundAudio(FName TriggerName, float NormalizedMagnitude)
{
	const FName MagnitudeParameterName(*FString::Printf(TEXT("%s_Magnitude"), *TriggerName.ToString()));

	if (IsValid(DiscreteImpulsesAudioComponent) && IsValid(DiscreteImpulsesAudioComponent->GetSound()))
	{
		if (!DiscreteImpulsesAudioComponent->IsPlaying())
		{
			DiscreteImpulsesAudioComponent->Play();
		}
		DiscreteImpulsesAudioComponent->SetFloatParameter(MagnitudeParameterName, NormalizedMagnitude);
		DiscreteImpulsesAudioComponent->SetTriggerParameter(TriggerName);
	}

	for (int32 Idx = BoundAudioComponents.Num() - 1; Idx >= 0; --Idx)
	{
		UAudioComponent* AudioComp = BoundAudioComponents[Idx].Get();
		if (!IsValid(AudioComp))
		{
			BoundAudioComponents.RemoveAtSwap(Idx, EAllowShrinking::No);
			continue;
		}

		// Safe greybox no-op when no USoundBase / UMetaSoundSource is assigned yet.
		if (!IsValid(AudioComp->GetSound()))
		{
			continue;
		}

		if (!AudioComp->IsPlaying())
		{
			AudioComp->Play();
		}

		AudioComp->SetFloatParameter(MagnitudeParameterName, NormalizedMagnitude);
		AudioComp->SetTriggerParameter(TriggerName);
	}
}

void UAudioTelemetryBridgeComponent::ConfigurePersistentAudioComponent(UAudioComponent* AudioComp)
{
	if (IsValid(AudioComp))
	{
		AudioComp->PrimaryComponentTick.bCanEverTick = false;
		AudioComp->bAutoDestroy = false;
	}
}

FName UAudioTelemetryBridgeComponent::ResolveStreamParameterName(ETelemetryStreamId StreamId) const
{
	if (const FName* FoundName = StreamParameterNames.Find(StreamId))
	{
		return *FoundName;
	}
	return NAME_None;
}

FName UAudioTelemetryBridgeComponent::ResolveImpulseTriggerName(ETelemetryImpulseId ImpulseId) const
{
	if (const FName* FoundName = ImpulseTriggerNames.Find(ImpulseId))
	{
		return *FoundName;
	}
	return NAME_None;
}
