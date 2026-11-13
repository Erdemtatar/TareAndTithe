// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/TareTithePresentationDataAsset.h"
#include "Components/StaticMeshComponent.h"
#include "Data/ShiftProgressionDataAsset.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureCube.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

FCellLightingRigConfig::FCellLightingRigConfig()
{
	KeySpot.Location = FVector(165.0, -22.0, 330.0);
	KeySpot.TargetLocation = FVector(260.0, 0.0, 105.0);
	KeySpot.Intensity = 8800.0f;
	KeySpot.AttenuationRadius = 580.0f;
	KeySpot.InnerConeAngle = 22.0f;
	KeySpot.OuterConeAngle = 40.0f;
	KeySpot.SourceRadius = 26.0f;
	KeySpot.SourceLength = 90.0f;
	KeySpot.LightColor = FLinearColor(0.96f, 0.97f, 1.00f, 1.0f);
	KeySpot.bCastShadows = true;

	RimSpotLeft.Location = FVector(305.0, -135.0, 220.0);
	RimSpotLeft.TargetLocation = FVector(260.0, 0.0, 108.0);
	RimSpotLeft.Intensity = 6800.0f;
	RimSpotLeft.AttenuationRadius = 430.0f;
	RimSpotLeft.InnerConeAngle = 20.0f;
	RimSpotLeft.OuterConeAngle = 38.0f;
	RimSpotLeft.SourceRadius = 20.0f;
	RimSpotLeft.SourceLength = 115.0f;
	RimSpotLeft.LightColor = FLinearColor(0.72f, 0.87f, 1.00f, 1.0f);
	RimSpotLeft.bCastShadows = false;

	RimSpotRight.Location = FVector(305.0, 135.0, 212.0);
	RimSpotRight.TargetLocation = FVector(260.0, 0.0, 108.0);
	RimSpotRight.Intensity = 6800.0f;
	RimSpotRight.AttenuationRadius = 430.0f;
	RimSpotRight.InnerConeAngle = 20.0f;
	RimSpotRight.OuterConeAngle = 38.0f;
	RimSpotRight.SourceRadius = 20.0f;
	RimSpotRight.SourceLength = 115.0f;
	RimSpotRight.LightColor = FLinearColor(0.72f, 0.87f, 1.00f, 1.0f);
	RimSpotRight.bCastShadows = false;

	// Direct +X frontal foundry shaft glow (lowered & cooled deep furnace ember so foreground hero steel dominates ROI hierarchy; R = 325 < 360 -> 0.000 side-wall spill).
	MidgroundWashSpot.Location = FVector(332.0, 0.0, 44.0);
	MidgroundWashSpot.TargetLocation = FVector(445.0, 0.0, 94.0);
	MidgroundWashSpot.Intensity = 5700.0f;
	MidgroundWashSpot.AttenuationRadius = 325.0f;
	MidgroundWashSpot.InnerConeAngle = 38.0f;
	MidgroundWashSpot.OuterConeAngle = 67.0f;
	MidgroundWashSpot.SourceRadius = 32.0f;
	MidgroundWashSpot.SourceLength = 120.0f;
	MidgroundWashSpot.LightColor = FLinearColor(1.00f, 0.32f, 0.065f, 1.0f);
	MidgroundWashSpot.bCastShadows = false;
}

bool FMeshPresentationSlot::ApplyToVisualComponent(UStaticMeshComponent* TargetVisualComp) const
{
	if (!IsValid(TargetVisualComp) || !IsValid(Mesh))
	{
		return false;
	}

	TargetVisualComp->SetSimulatePhysics(false);
	TargetVisualComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TargetVisualComp->SetGenerateOverlapEvents(false);
	TargetVisualComp->SetStaticMesh(Mesh);
	TargetVisualComp->EmptyOverrideMaterials();
	TargetVisualComp->SetRelativeTransform(VisualLocalTransform);

	for (int32 SlotIdx = 0; SlotIdx < MaterialOverrides.Num(); ++SlotIdx)
	{
		if (IsValid(MaterialOverrides[SlotIdx]))
		{
			TargetVisualComp->SetMaterial(SlotIdx, MaterialOverrides[SlotIdx]);
		}
	}

	return true;
}

FPrimaryAssetId UTareTithePresentationDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("TareTithePresentation")), GetFName());
}

FPanopticonSurveillanceConfig FPanopticonSurveillanceConfig::GetSanitized() const
{
	FPanopticonSurveillanceConfig Out = *this;
	Out.IdleBayLightIntensityScale = FMath::Clamp(Out.IdleBayLightIntensityScale, 0.05f, 1.00f);
	Out.FaultStrobePeakMultiplier = FMath::Clamp(Out.FaultStrobePeakMultiplier, 1.00f, 15.00f);
	Out.BayLightForwardLocationX = FMath::Clamp(Out.BayLightForwardLocationX, -45.0f, -16.0f);
	Out.FlankBayLightLocationY = FMath::Clamp(Out.FlankBayLightLocationY, 140.0f, 230.0f);
	Out.FlankBayLightLocationZ = FMath::Clamp(Out.FlankBayLightLocationZ, -12.0f, 28.0f);
	Out.CenterBayLightLocationZ = FMath::Clamp(Out.CenterBayLightLocationZ, -5.0f, 36.0f);
	Out.BoothFrameAlbedoScale = FMath::Clamp(Out.BoothFrameAlbedoScale, 0.20f, 1.20f);
	Out.BoothFrameSpecularScale = FMath::Clamp(Out.BoothFrameSpecularScale, 0.05f, 1.00f);
	Out.BoothShutterAlbedoScale = FMath::Clamp(Out.BoothShutterAlbedoScale, 0.20f, 1.20f);
	Out.BoothShutterSpecularScale = FMath::Clamp(Out.BoothShutterSpecularScale, 0.05f, 1.00f);
	Out.WindowCardRoughness = FMath::Clamp(Out.WindowCardRoughness, 0.50f, 1.00f);
	Out.WindowCardMetallic = FMath::Clamp(Out.WindowCardMetallic, 0.00f, 0.25f);
	Out.FlankLouverSlatCount = FMath::Clamp(Out.FlankLouverSlatCount, 4, 12);
	Out.CenterLouverSlatCount = FMath::Clamp(Out.CenterLouverSlatCount, 3, 10);
	Out.LouverVerticalPitchCm = FMath::Clamp(Out.LouverVerticalPitchCm, 3.80f, 9.00f);
	Out.LouverDepthCm = FMath::Clamp(Out.LouverDepthCm, 1.20f, 3.40f);
	Out.LouverThicknessCm = FMath::Clamp(Out.LouverThicknessCm, 0.50f, 1.60f);
	Out.LouverPitchDegrees = FMath::Clamp(Out.LouverPitchDegrees, -25.0f, 10.0f);
	Out.DirectorBayLocalY = FMath::Clamp(Out.DirectorBayLocalY, 150.0f, 205.0f);
	Out.DirectorScale = FMath::Clamp(Out.DirectorScale, 0.75f, 1.15f);
	Out.DirectorInwardYawDegrees = FMath::Clamp(Out.DirectorInwardYawDegrees, -180.0f, -155.0f);
	return Out;
}

FOuterEdgeFramingConfig FOuterEdgeFramingConfig::GetSanitized() const
{
	FOuterEdgeFramingConfig Out = *this;
	Out.ForegroundPipeWorldX = FMath::Clamp(Out.ForegroundPipeWorldX, 155.0f, 225.0f);
	Out.ForegroundPipeAbsY = FMath::Clamp(Out.ForegroundPipeAbsY, 188.0f, 245.0f);
	Out.ForegroundPipeRadiusCm = FMath::Clamp(Out.ForegroundPipeRadiusCm, 8.0f, 19.0f);
	Out.MidgroundColumnWorldX = FMath::Clamp(Out.MidgroundColumnWorldX, 215.0f, 335.0f);
	Out.MidgroundColumnAbsY = FMath::Clamp(Out.MidgroundColumnAbsY, 210.0f, 315.0f);
	Out.MidgroundColumnXYScale = FMath::Clamp(Out.MidgroundColumnXYScale, 0.60f, 1.30f);
	Out.CompanionPipeWorldX = FMath::Clamp(Out.CompanionPipeWorldX, 205.0f, 320.0f);
	Out.CompanionPipeAbsY = FMath::Clamp(Out.CompanionPipeAbsY, 200.0f, 275.0f);
	Out.CompanionPipeRadiusCm = FMath::Clamp(Out.CompanionPipeRadiusCm, 5.0f, 15.0f);
	Out.OuterMetalAlbedoScale = FMath::Clamp(Out.OuterMetalAlbedoScale, 0.30f, 2.20f);
	Out.OuterMetalRoughnessMin = FMath::Clamp(Out.OuterMetalRoughnessMin, 0.16f, 0.70f);
	Out.OuterMetalRoughnessMax = FMath::Clamp(Out.OuterMetalRoughnessMax, FMath::Max(0.20f, Out.OuterMetalRoughnessMin), 0.85f);
	Out.OuterMetalMetallic = FMath::Clamp(Out.OuterMetalMetallic, 0.20f, 0.95f);
	Out.OuterMetalSpecularScale = FMath::Clamp(Out.OuterMetalSpecularScale, 0.15f, 1.00f);
	Out.DedicatedOuterRimIntensity = FMath::Clamp(Out.DedicatedOuterRimIntensity, 0.0f, 600.0f);
	Out.DedicatedOuterRimRadius = FMath::Clamp(Out.DedicatedOuterRimRadius, 60.0f, 220.0f);
	return Out;
}
