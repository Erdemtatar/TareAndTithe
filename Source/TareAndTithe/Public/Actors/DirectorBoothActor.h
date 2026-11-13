// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/TareTitheTypes.h"
#include "Subsystems/InspectionShiftSubsystem.h"
#include "DirectorBoothActor.generated.h"

class UAudioComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMonolithContractDataAsset;
class UPointLightComponent;
class USceneComponent;
class UShiftProgressionDataAsset;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;
class UTareTithePresentationDataAsset;
class UTextRenderComponent;

UCLASS()
class TAREANDTITHE_API ADirectorBoothActor : public AActor
{
	GENERATED_BODY()

public:
	ADirectorBoothActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ApplyPresentationData(const UTareTithePresentationDataAsset* InPresentationData);

	UFUNCTION(BlueprintPure, Category = "DirectorBooth|Diagnostics")
	int32 GetIntercomAudioTriggerCount() const { return IntercomAudioTriggerCount; }

	UFUNCTION(BlueprintPure, Category = "DirectorBooth|Diagnostics")
	int32 GetIntercomDebounceBlockedCount() const { return IntercomDebounceBlockedCount; }

	UFUNCTION(BlueprintPure, Category = "DirectorBooth|Diagnostics")
	USoundBase* GetLastTriggeredIntercomSound() const { return LastTriggeredIntercomSound; }

	UFUNCTION(BlueprintPure, Category = "DirectorBooth|Diagnostics")
	float GetCurrentShutterOpenAlpha() const { return CurrentShutterOpenAlpha; }

	UFUNCTION(BlueprintPure, Category = "DirectorBooth|Diagnostics")
	int32 GetLouverSlatInstanceCount() const;

	UFUNCTION(BlueprintPure, Category = "DirectorBooth|Diagnostics")
	int32 GetDirectorSilhouetteInstanceCount() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth")
	TObjectPtr<USceneComponent> BoothRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<UStaticMeshComponent> OverlookRecessBackingVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<UStaticMeshComponent> LeftWindowGlowCard;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<UStaticMeshComponent> CenterWindowGlowCard;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<UStaticMeshComponent> RightWindowGlowCard;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<UInstancedStaticMeshComponent> PanopticonLouverSlatsISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<UInstancedStaticMeshComponent> DirectorSilhouetteCubesISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<UInstancedStaticMeshComponent> DirectorSilhouetteCylindersISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<USceneComponent> CenterBayPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<UStaticMeshComponent> BoothFrameVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<USceneComponent> LeftBayPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<UStaticMeshComponent> LeftBoothFrameVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<USceneComponent> RightBayPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<UStaticMeshComponent> RightBoothFrameVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<USceneComponent> BlastShutterMotionPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<UStaticMeshComponent> BlastShutterVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<USceneComponent> LeftShutterMotionPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<UStaticMeshComponent> LeftBlastShutterVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<USceneComponent> RightShutterMotionPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<UStaticMeshComponent> RightBlastShutterVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<USceneComponent> ArmorMullionOuterLeftPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<UStaticMeshComponent> ArmorMullionOuterLeftVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<USceneComponent> ArmorMullionInnerLeftPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<UStaticMeshComponent> ArmorMullionInnerLeftVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<USceneComponent> ArmorMullionInnerRightPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<UStaticMeshComponent> ArmorMullionInnerRightVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<USceneComponent> ArmorMullionOuterRightPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Panopticon")
	TObjectPtr<UStaticMeshComponent> ArmorMullionOuterRightVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	TObjectPtr<USceneComponent> BoothSignRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	TObjectPtr<USceneComponent> BoothHeaderPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	TObjectPtr<UStaticMeshComponent> BoothHeaderBackingPlate;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	TObjectPtr<UTextRenderComponent> BoothStencilText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	TObjectPtr<UTextRenderComponent> ContractDirectiveReadoutText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	TObjectPtr<UTextRenderComponent> RequisitionReadoutText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	TObjectPtr<USceneComponent> BoothVerdictPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	TObjectPtr<UStaticMeshComponent> BoothVerdictBackingPlate;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	TObjectPtr<UTextRenderComponent> BoothRightHeaderText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	TObjectPtr<UTextRenderComponent> VerdictBannerText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	TObjectPtr<UTextRenderComponent> VerdictBannerSubText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Lighting")
	TObjectPtr<UPointLightComponent> StatusPointLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Lighting")
	TObjectPtr<UPointLightComponent> LeftBayPointLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Lighting")
	TObjectPtr<UPointLightComponent> RightBayPointLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Audio")
	TObjectPtr<UAudioComponent> IntercomAudioComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Shutter", meta = (ClampMin = "0.08", ClampMax = "2.0"))
	float ShutterTransitionDurationSeconds = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Shutter", meta = (ClampMin = "0.05", ClampMax = "0.5"))
	float FaultShutterSlamDurationSeconds = 0.14f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Shutter")
	FVector ClosedShutterRelativeLocation = FVector(-15.0, 0.0, 18.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Shutter")
	FVector OpenShutterRelativeLocation = FVector(-15.0, 0.0, 68.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Shutter")
	FVector ClosedLeftShutterRelativeLocation = FVector(-9.0, -162.0, 12.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Shutter")
	FVector OpenLeftShutterRelativeLocation = FVector(-9.0, -162.0, 56.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Shutter")
	FVector ClosedRightShutterRelativeLocation = FVector(-9.0, 162.0, 12.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Shutter")
	FVector OpenRightShutterRelativeLocation = FVector(-9.0, 162.0, 56.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Lighting", meta = (ClampMin = "50.0", ClampMax = "8000.0"))
	float StatusLightIntensity = 400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Lighting", meta = (ClampMin = "50.0", ClampMax = "8000.0"))
	float FlankBayLightIntensity = 330.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Lighting", meta = (ClampMin = "2.0", ClampMax = "20.0"))
	float FaultStrobeFrequencyHz = 9.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Lighting", meta = (ClampMin = "60.0", ClampMax = "235.0"))
	float StatusLightAttenuationRadiusCm = 195.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Lighting", meta = (ClampMin = "60.0", ClampMax = "235.0"))
	float FlankBayLightAttenuationRadiusCm = 175.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Audio", meta = (ClampMin = "0.10", ClampMax = "2.0"))
	float IntercomMinDebounceSeconds = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	FVector LeftWingPivotRelativeLocation = FVector(-238.0, -129.0, -22.5);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	FVector RightWingPivotRelativeLocation = FVector(-238.0, 129.0, -22.5);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	FRotator LeftWingPivotRelativeRotation = FRotator(12.0f, -14.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	FRotator RightWingPivotRelativeRotation = FRotator(12.0f, 14.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	FVector WingSignboardPlateScale3D = FVector(0.014, 0.70, 0.150);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout", meta = (ClampMin = "1.5", ClampMax = "8.0"))
	float WingHeaderWorldSize = 2.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout", meta = (ClampMin = "1.5", ClampMax = "8.0"))
	float WingDirectiveWorldSize = 2.95f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout", meta = (ClampMin = "1.5", ClampMax = "8.0"))
	float WingRequisitionWorldSize = 2.55f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout", meta = (ClampMin = "1.0", ClampMax = "6.0"))
	float WingDirectiveMinWorldSize = 1.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout", meta = (ClampMin = "40.0", ClampMax = "160.0"))
	float WingDirectiveMaxSafeWidthCm = 65.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout", meta = (ClampMin = "1.5", ClampMax = "8.0"))
	float WingVerdictWorldSize = 2.95f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout", meta = (ClampMin = "1.5", ClampMax = "8.0"))
	float WingVerdictWrappedWorldSize = 2.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout", meta = (ClampMin = "0.2", ClampMax = "6.0"))
	float BriefPulseFrequencyHz = 1.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float BriefFadeOutDurationSeconds = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	FLinearColor SignboardBackingColor = FLinearColor(0.005f, 0.006f, 0.008f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	FColor BoothHeaderCyanColor = FColor(0, 230, 255, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	FColor BoothDirectiveGoldColor = FColor(255, 210, 45, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	FColor BoothBriefAmberMinColor = FColor(255, 165, 15, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	FColor BoothBriefAmberPeakColor = FColor(255, 235, 130, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	FColor BoothNormalSkyBlueColor = FColor(90, 210, 255, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	FColor BoothApprovedEmeraldColor = FColor(0, 255, 90, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	FColor BoothFaultBannerColor = FColor(255, 55, 35, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Lighting")
	FLinearColor ActiveInspectionLampColor = FLinearColor(0.54f, 0.78f, 1.00f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Lighting")
	FLinearColor FaultSlamLampColor = FLinearColor(1.0f, 0.05f, 0.015f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Lighting")
	FLinearColor VerdictApprovedLampColor = FLinearColor(0.16f, 0.96f, 0.38f, 1.0f);

private:
	UFUNCTION()
	void HandleContractSnapshotUpdated(const FContractRuntimeSnapshot& Snapshot);

	UFUNCTION()
	void HandleShiftVerdict(EShiftVerdictType Verdict, EContractFaultReason FaultReason);

	void HandleShiftPhaseChanged(EShiftPhase NewPhase);
	void HandleActiveContractLoaded(
		const UMonolithContractDataAsset* Contract,
		const UShiftProgressionDataAsset* ShiftConfig,
		int32 ContractIndex);
	bool IsLegacyWorldSpaceReadoutEnabled() const;
	void UpdateLegacySignboardVisibility();
	void RebuildPanopticonSurveillanceVisuals();
	void SetWindowGlowCardsState(const FLinearColor& BaseColor, const FLinearColor& EmissiveColor);
	void RefreshContractDirectiveWingReadout();
	void RefreshRequisitionCornerReadout();
	void RefreshDirectorIntercomWingReadout();
	void SetPanopticonLampState(const FLinearColor& LampColor, float IntensityMultiplier);
	void TryTriggerIntercomAudioForSnapshot();
	void SetShutterTargetOpenAlpha(float InTargetOpenAlpha, bool bInstant = false);
	void ApplyShutterPositionFromAlpha();
	void UpdateBoothTickEnabled();
	FColor ComputeBriefPulseColor() const;
	static void SplitIntercomBarkTwoLines(const FString& InBark, FString& OutLineOne, FString& OutLineTwo);
	static void ApplyFittedWingText(
		UTextRenderComponent* TextComp,
		const FString& NewText,
		float DefaultWorldSize,
		float MinWorldSize,
		float MaxSafeWidthCm);
	static FString DescribeFaultReason(EContractFaultReason FaultReason);

	UPROPERTY()
	TObjectPtr<UStaticMesh> BasicCubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> BasicCylinderMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseGreyboxShapeMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> UnlitSignboardTextMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BoothFrameMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DirectorShutterMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LouverSlatMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DirectorSilhouetteMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LeftWindowGlowCardMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CenterWindowGlowCardMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RightWindowGlowCardMID;

	UPROPERTY(Transient)
	TObjectPtr<const UTareTithePresentationDataAsset> PresentationData;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> LastTriggeredIntercomSound;

	FContractRuntimeSnapshot CachedSnapshot;
	FString CachedDirectiveText;
	FString CachedBriefText;
	FString LastSpokenIntercomLine;
	FString LastBaselineIntercomLine;
	double LastIntercomCueWorldTimeSeconds = -10.0;
	int32 IntercomAudioTriggerCount = 0;
	int32 IntercomDebounceBlockedCount = 0;
	EShiftPhase CachedShiftPhase = EShiftPhase::ContractIntro;
	int32 CachedContractIndex = 0;
	int32 CachedTotalContracts = 6;
	float CurrentShutterOpenAlpha = 0.0f;
	float TargetShutterOpenAlpha = 0.0f;
	float BriefPulseElapsedSeconds = 0.0f;
	float BriefFadeElapsedSeconds = 0.0f;
	float FaultStrobeElapsedSeconds = 0.0f;
	FColor LastBriefDisplayColor = FColor(255, 210, 45, 255);
	bool bIsBriefPulseActive = false;
	bool bIsBriefFadingOut = false;
	bool bUseFastFaultShutterSlam = false;
	bool bWasShowingTransientRequisitionFeedback = false;
};
