// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/TareTitheTypes.h"
#include "Data/TareTithePresentationDataAsset.h"
#include "Subsystems/InspectionShiftSubsystem.h"
#include "AudioTelemetryBridgeComponent.generated.h"

class ITelemetryPublisherInterface;
class UAudioComponent;
class UMonolithContractDataAsset;
class UShiftProgressionDataAsset;
class USoundBase;

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
	void SetClampGroanAudioComponent(UAudioComponent* InAudioComponent);
	void ApplyPresentationData(const UTareTithePresentationDataAsset* InPresentationData);

	void RegisterAudioComponent(UAudioComponent* InAudioComponent);
	void UnregisterAudioComponent(UAudioComponent* InAudioComponent);
	void ClearAudioComponents();

	void ResetStreamCache();

	UFUNCTION(BlueprintPure, Category = "AudioTelemetry|Diagnostics")
	float GetLastFlushedStreamValue(ETelemetryStreamId StreamId) const;

	UFUNCTION(BlueprintPure, Category = "AudioTelemetry|Diagnostics")
	bool IsClampGroanVoiceActive() const { return bGroanVoiceActive; }

	UFUNCTION(BlueprintPure, Category = "AudioTelemetry|Diagnostics")
	float GetClampGroanVolumeMultiplier() const { return bGroanVoiceActive ? CurrentAppliedGroanVolume : 0.0f; }

	UFUNCTION(BlueprintPure, Category = "AudioTelemetry|Diagnostics")
	float GetClampGroanPitchMultiplier() const { return CurrentAppliedGroanPitch; }

	UFUNCTION(BlueprintPure, Category = "AudioTelemetry|Diagnostics")
	float GetClampEffectiveDanger() const { return CurrentEffectiveDanger; }

	UFUNCTION(BlueprintPure, Category = "AudioTelemetry|Diagnostics")
	float GetAmbientBedVolumeMultiplier() const { return LastAmbientBedVolumeMultiplier; }

	UFUNCTION(BlueprintPure, Category = "AudioTelemetry|Diagnostics")
	float GetLastMetaSoundClampStrainParam() const { return LastMetaSoundClampStrainParam; }

	UFUNCTION(BlueprintPure, Category = "AudioTelemetry|Diagnostics")
	UAudioComponent* GetClampGroanAudioComponent() const { return ClampGroanAudioComponent; }

	UFUNCTION(BlueprintPure, Category = "AudioTelemetry|Diagnostics")
	UAudioComponent* GetAmbientBedAudioComponent() const { return AmbientBedAudioComponent; }

	UFUNCTION(BlueprintPure, Category = "AudioTelemetry|Diagnostics")
	UAudioComponent* GetContinuousLoopsAudioComponent() const { return ContinuousLoopsAudioComponent; }

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AudioTelemetry|Spatial", meta = (ClampMin = "0.1", ClampMax = "2.5"))
	float SpatialMinVolumeMultiplier = 0.92f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AudioTelemetry|Spatial", meta = (ClampMin = "0.1", ClampMax = "2.5"))
	float SpatialMaxVolumeMultiplier = 1.25f;

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
	void HandleShiftPhaseChanged(EShiftPhase NewPhase);
	void HandleActiveContractLoaded(
		const UMonolithContractDataAsset* Contract,
		const UShiftProgressionDataAsset* ShiftConfig,
		int32 ContractIndex);

	bool IsInActiveInspectionPhase() const;
	USoundBase* ResolveClampGroanSoundAsset() const;
	void PushNeutralClampStrainToMetaSound();
	void PushZeroedLoopsToMetaSound();
	void EvaluateSteadyClampGroanCurve(float& OutSteadyVolume, float& OutSteadyPitch, float& OutEffectiveDanger) const;
	void RefreshClampGroanEnvelope();
	void ArmClampGroanSurge();
	void HandleGroanSurgeTimerStep();
	void ClearGroanSurgeTimer();
	void SilenceClampGroanFast();
	void HandlePostFadeZeroVolumeElapsed();
	void MuteAndCutoffLoopsForFaultOrPhaseExit();

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
	TObjectPtr<UAudioComponent> ClampGroanAudioComponent;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> CachedClampGroanSound;

	UPROPERTY(Transient)
	TObjectPtr<const UTareTithePresentationDataAsset> PresentationData;

	FClampGroanAudioConfig CachedGroanConfig;
	EShiftPhase CachedShiftPhase = EShiftPhase::WaitingForStations;
	bool bClampGroanMutedByPhaseOrFault = true;
	bool bGroanVoiceActive = false;
	bool bHasBaselineContractStrainSample = false;
	bool bHasBaselineContractFatigueSample = false;

	float CurrentRawClampStrain = 0.0f;
	float CurrentRawClampFatigue = 0.0f;
	float CurrentAppliedGroanVolume = 0.0f;
	float CurrentAppliedGroanPitch = 0.94f;
	float CurrentEffectiveDanger = 0.0f;
	float LastAmbientBedVolumeMultiplier = 0.42f;
	float LastMetaSoundClampStrainParam = 0.15f;

	uint8 SurgeStage = 0;
	FTimerHandle GroanSurgeTimerHandle;
	FTimerHandle GroanPostFadeZeroTimerHandle;

	TArray<FPublisherSubscription> ActiveSubscriptions;
	TArray<TWeakObjectPtr<UAudioComponent>> BoundAudioComponents;
	TMap<ETelemetryStreamId, float> LastFlushedStreamValues;
};
