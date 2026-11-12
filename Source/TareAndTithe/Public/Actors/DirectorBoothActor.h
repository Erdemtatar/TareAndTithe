// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/TareTitheTypes.h"
#include "Subsystems/InspectionShiftSubsystem.h"
#include "DirectorBoothActor.generated.h"

class UMaterialInterface;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class TAREANDTITHE_API ADirectorBoothActor : public AActor
{
	GENERATED_BODY()

public:
	ADirectorBoothActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth")
	TObjectPtr<USceneComponent> BoothRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth")
	TObjectPtr<UStaticMeshComponent> BoothFrameVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth")
	TObjectPtr<UStaticMeshComponent> BlastShutterVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	TObjectPtr<USceneComponent> BoothHeaderPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	TObjectPtr<UStaticMeshComponent> BoothHeaderBackingPlate;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	TObjectPtr<UTextRenderComponent> BoothStencilText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	TObjectPtr<USceneComponent> BoothVerdictPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	TObjectPtr<UStaticMeshComponent> BoothVerdictBackingPlate;

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
	float StatusLightIntensity = 1100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	FLinearColor SignboardBackingColor = FLinearColor(0.005f, 0.006f, 0.008f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	FColor BoothHeaderCyanColor = FColor(0, 230, 255, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	FColor BoothNormalSkyBlueColor = FColor(90, 210, 255, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	FColor BoothApprovedEmeraldColor = FColor(0, 255, 90, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Readout")
	FColor BoothFaultBannerColor = FColor(255, 55, 35, 255);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Lighting")
	FLinearColor ActiveInspectionLampColor = FLinearColor(1.0f, 0.68f, 0.16f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Lighting")
	FLinearColor FaultSlamLampColor = FLinearColor(1.0f, 0.10f, 0.06f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DirectorBooth|Lighting")
	FLinearColor VerdictApprovedLampColor = FLinearColor(0.16f, 0.96f, 0.38f, 1.0f);

private:
	UFUNCTION()
	void HandleShiftVerdict(EShiftVerdictType Verdict, EContractFaultReason FaultReason);

	void HandleShiftPhaseChanged(EShiftPhase NewPhase);
	void SetShutterTargetOpenAlpha(float InTargetOpenAlpha, bool bInstant = false);
	void ApplyShutterPositionFromAlpha();
	static FString DescribeFaultReason(EContractFaultReason FaultReason);

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseGreyboxShapeMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> UnlitSignboardTextMaterial;

	float CurrentShutterOpenAlpha = 0.0f;
	float TargetShutterOpenAlpha = 0.0f;
};
