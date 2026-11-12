// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/AudioTelemetryBridgeComponent.h"
#include "Components/AudioComponent.h"
#include "Data/TareTithePresentationDataAsset.h"
#include "Engine/World.h"
#include "Interfaces/TelemetryPublisherInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Subsystems/InspectionShiftSubsystem.h"

UAudioTelemetryBridgeComponent::UAudioTelemetryBridgeComponent()
{
	// Tier 1: Strictly event-driven telemetry deadband router; zero tick overhead.
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

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
			ShiftSubsystem->RegisterAudioTelemetryBridge(this);
			ShiftSubsystem->OnPresentationAssetChanged.AddUObject(this, &UAudioTelemetryBridgeComponent::ApplyPresentationData);
			if (const UTareTithePresentationDataAsset* ExistingPresentation = ShiftSubsystem->GetActivePresentationAsset())
			{
				ApplyPresentationData(ExistingPresentation);
			}
		}
	}
}

void UAudioTelemetryBridgeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->OnPresentationAssetChanged.RemoveAll(this);
			ShiftSubsystem->UnregisterAudioTelemetryBridge(this);
		}
	}

	UnregisterAllPublishers();
	BoundAudioComponents.Reset();
	ContinuousLoopsAudioComponent = nullptr;
	DiscreteImpulsesAudioComponent = nullptr;
	AmbientBedAudioComponent = nullptr;

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

	if (bAutoPlayAssignedAudioComponents &&
		IsValid(AmbientBedAudioComponent) &&
		IsValid(AmbientBedAudioComponent->GetSound()) &&
		!AmbientBedAudioComponent->IsPlaying())
	{
		AmbientBedAudioComponent->Play();
	}
}

void UAudioTelemetryBridgeComponent::ApplyPresentationData(const UTareTithePresentationDataAsset* InPresentationData)
{
	PresentationData = InPresentationData;
	if (!IsValid(PresentationData))
	{
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
	}

	if (IsValid(AmbientBedAudioComponent) && IsValid(PresentationData->AmbientCellBedSound))
	{
		ConfigurePersistentAudioComponent(AmbientBedAudioComponent);
		if (AmbientBedAudioComponent->GetSound() != PresentationData->AmbientCellBedSound)
		{
			AmbientBedAudioComponent->SetSound(PresentationData->AmbientCellBedSound);
		}
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
	BoundAudioComponents.Reset();
	ContinuousLoopsAudioComponent = nullptr;
	DiscreteImpulsesAudioComponent = nullptr;
	AmbientBedAudioComponent = nullptr;
}

void UAudioTelemetryBridgeComponent::ResetStreamCache()
{
	LastFlushedStreamValues.Reset();
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
	float ClampedValue = FMath::Clamp(NormalizedValue, 0.0f, 1.0f);
	if (ClampedValue <= KINDA_SMALL_NUMBER)
	{
		ClampedValue = 0.0f;
	}
	else if (ClampedValue >= (1.0f - KINDA_SMALL_NUMBER))
	{
		ClampedValue = 1.0f;
	}

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
		ForwardFloatToBoundAudio(ParameterName, ClampedValue);
	}

	OnFilteredTelemetryStream.Broadcast(StreamId, ClampedValue);
}

void UAudioTelemetryBridgeComponent::HandlePublisherImpulse(
	ETelemetryImpulseId ImpulseId,
	float NormalizedMagnitude,
	const FVector& WorldLocation)
{
	const float ClampedMagnitude = FMath::Clamp(NormalizedMagnitude, 0.0f, 1.0f);

	SpawnSpatialImpulseSoundIfAssigned(ImpulseId, ClampedMagnitude, WorldLocation);

	const FName TriggerName = ResolveImpulseTriggerName(ImpulseId);
	if (!TriggerName.IsNone())
	{
		ForwardImpulseToBoundAudio(TriggerName, ClampedMagnitude);
	}

	OnForwardedTelemetryImpulse.Broadcast(ImpulseId, ClampedMagnitude, WorldLocation);
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
