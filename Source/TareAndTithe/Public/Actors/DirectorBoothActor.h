// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/TareTitheTypes.h"
#include "Subsystems/InspectionShiftSubsystem.h"
#include "DirectorBoothActor.generated.h"

class UMaterialInterface;
class UMonolithContractDataAsset;
class UPointLightComponent;
class USceneComponent;
class UShiftProgressionDataAsset;
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

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth")
	TObjectPtr<USceneComponent> BoothRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth")
	TObjectPtr<UStaticMeshComponent> BoothFrameVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth")
	TObjectPtr<USceneComponent> BlastShutterMotionPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth")
	TObjectPtr<UStaticMeshComponent> BlastShutterVisual;

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
	TObjectPtr<USceneComponent> BoothVerdictPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	TObjectPtr<UStaticMeshComponent> BoothVerdictBackingPlate;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	TObjectPtr<UTextRenderComponent> BoothRightHeaderText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	TObjectPtr<UTextRenderComponent> VerdictBannerText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth")
	TObjectPtr<UPointLightComponent> StatusPointLight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Shutter", meta = (ClampMin = "0.08", ClampMax = "2.0"))
	float ShutterTransitionDurationSeconds = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Shutter")
	FVector ClosedShutterRelativeLocation = FVector(-10.0, 0.0, 0.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Shutter")
	FVector OpenShutterRelativeLocation = FVector(-10.0, 0.0, 44.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Lighting", meta = (ClampMin = "50.0", ClampMax = "8000.0"))
	float StatusLightIntensity = 360.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Lighting", meta = (ClampMin = "60.0", ClampMax = "600.0"))
	float StatusLightAttenuationRadiusCm = 210.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	FVector LeftWingPivotRelativeLocation = FVector(-15.5, -66.0, 44.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	FVector RightWingPivotRelativeLocation = FVector(-15.5, 66.0, 44.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float WingSignboardOutwardOffsetCm = 52.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	FVector WingSignboardPlateScale3D = FVector(0.016, 1.44, 0.23);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout", meta = (ClampMin = "3.0", ClampMax = "12.0"))
	float WingHeaderWorldSize = 5.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout", meta = (ClampMin = "3.0", ClampMax = "12.0"))
	float WingDirectiveWorldSize = 5.9f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout", meta = (ClampMin = "2.5", ClampMax = "10.0"))
	float WingDirectiveMinWorldSize = 3.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout", meta = (ClampMin = "60.0", ClampMax = "240.0"))
	float WingDirectiveMaxSafeWidthCm = 134.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout", meta = (ClampMin = "3.0", ClampMax = "12.0"))
	float WingVerdictWorldSize = 5.7f;

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
	FLinearColor FaultSlamLampColor = FLinearColor(1.0f, 0.10f, 0.06f, 1.0f);

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
	void RefreshContractDirectiveWingReadout();
	void SetShutterTargetOpenAlpha(float InTargetOpenAlpha, bool bInstant = false);
	void ApplyShutterPositionFromAlpha();
	void UpdateBoothTickEnabled();
	FColor ComputeBriefPulseColor() const;
	static void ApplyFittedWingText(
		UTextRenderComponent* TextComp,
		const FString& NewText,
		float DefaultWorldSize,
		float MinWorldSize,
		float MaxSafeWidthCm);
	static FString DescribeFaultReason(EContractFaultReason FaultReason);

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseGreyboxShapeMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> UnlitSignboardTextMaterial;

	FString CachedDirectiveText;
	FString CachedBriefText;
	EShiftPhase CachedShiftPhase = EShiftPhase::ContractIntro;
	int32 CachedContractIndex = 0;
	int32 CachedTotalContracts = 6;
	float CurrentShutterOpenAlpha = 0.0f;
	float TargetShutterOpenAlpha = 0.0f;
	float BriefPulseElapsedSeconds = 0.0f;
	float BriefFadeElapsedSeconds = 0.0f;
	FColor LastBriefDisplayColor = FColor(255, 210, 45, 255);
	bool bIsBriefPulseActive = false;
	bool bIsBriefFadingOut = false;
};
