// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/TareTitheTypes.h"
#include "AudioTelemetryBridgeComponent.generated.h"

class ITelemetryPublisherInterface;
class UAudioComponent;
class UTareTithePresentationDataAsset;

UCLASS(ClassGroup = (TareAndTithe), meta = (BlueprintSpawnableComponent))
class TAREANDTITHE_API UAudioTelemetryBridgeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAudioTelemetryBridgeComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void RegisterPublisher(ITelemetryPublisherInterface* Publisher, UObject* LifetimeOwner = nullptr);
	void UnregisterPublisher(ITelemetryPublisherInterface* Publisher);
	void UnregisterAllPublishers();

	void SetContinuousLoopsAudioComponent(UAudioComponent* InAudioComponent);
	void SetDiscreteImpulsesAudioComponent(UAudioComponent* InAudioComponent);
	void SetAmbientBedAudioComponent(UAudioComponent* InAudioComponent);
	void ApplyPresentationData(const UTareTithePresentationDataAsset* InPresentationData);

	void RegisterAudioComponent(UAudioComponent* InAudioComponent);
	void UnregisterAudioComponent(UAudioComponent* InAudioComponent);
	void ClearAudioComponents();

	void ResetStreamCache();
	float GetLastFlushedStreamValue(ETelemetryStreamId StreamId) const;

	FOnTelemetryStreamSignature OnFilteredTelemetryStream;
	FOnTelemetryImpulseSignature OnForwardedTelemetryImpulse;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AudioTelemetry|Filtering", meta = (ClampMin = "0.001", ClampMax = "0.1"))
	float StreamDeadband = 0.01f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AudioTelemetry|Playback")
	bool bAutoPlayAssignedAudioComponents = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AudioTelemetry|Mapping")
	TMap<ETelemetryStreamId, FName> StreamParameterNames;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AudioTelemetry|Mapping")
	TMap<ETelemetryImpulseId, FName> ImpulseTriggerNames;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AudioTelemetry|Spatial")
	FName SpatialOneShotMagnitudeParameterName = FName(TEXT("NormalizedMagnitude"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AudioTelemetry|Spatial", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float SpatialMinVolumeMultiplier = 0.72f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AudioTelemetry|Spatial", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float SpatialMaxVolumeMultiplier = 1.08f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AudioTelemetry|Spatial", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float SpatialMinPitchMultiplier = 0.94f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AudioTelemetry|Spatial", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float SpatialMaxPitchMultiplier = 1.06f;

private:
	struct FPublisherSubscription
	{
		ITelemetryPublisherInterface* Publisher = nullptr;
		TWeakObjectPtr<UObject> LifetimeOwner;
		bool bTrackOwnerLifetime = false;
		FDelegateHandle StreamDelegateHandle;
		FDelegateHandle ImpulseDelegateHandle;
	};

	void InitializeDefaultParameterMappings();
	void HandlePublisherStream(ETelemetryStreamId StreamId, float NormalizedValue);
	void HandlePublisherImpulse(ETelemetryImpulseId ImpulseId, float NormalizedMagnitude, const FVector& WorldLocation);
	void SpawnSpatialImpulseSoundIfAssigned(ETelemetryImpulseId ImpulseId, float ClampedMagnitude, const FVector& WorldLocation);
	void ForwardFloatToBoundAudio(FName ParameterName, float Value);
	void ForwardImpulseToBoundAudio(FName TriggerName, float NormalizedMagnitude);
	static void ConfigurePersistentAudioComponent(UAudioComponent* AudioComp);
	FName ResolveStreamParameterName(ETelemetryStreamId StreamId) const;
	FName ResolveImpulseTriggerName(ETelemetryImpulseId ImpulseId) const;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ContinuousLoopsAudioComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> DiscreteImpulsesAudioComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> AmbientBedAudioComponent;

	UPROPERTY(Transient)
	TObjectPtr<const UTareTithePresentationDataAsset> PresentationData;

	TArray<FPublisherSubscription> ActiveSubscriptions;
	TArray<TWeakObjectPtr<UAudioComponent>> BoundAudioComponents;
	TMap<ETelemetryStreamId, float> LastFlushedStreamValues;
};
