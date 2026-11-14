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
	Out.ForegroundPipeAbsY = FMath::Clamp(Out.ForegroundPipeAbsY, 185.0f, 245.0f);
	Out.ForegroundPipeRadiusCm = FMath::Clamp(Out.ForegroundPipeRadiusCm, 8.0f, 20.0f);
	Out.MidgroundColumnWorldX = FMath::Clamp(Out.MidgroundColumnWorldX, 205.0f, 335.0f);
	Out.MidgroundColumnAbsY = FMath::Clamp(Out.MidgroundColumnAbsY, 175.0f, 315.0f);
	Out.MidgroundColumnXYScale = FMath::Clamp(Out.MidgroundColumnXYScale, 0.60f, 1.30f);
	Out.CompanionPipeWorldX = FMath::Clamp(Out.CompanionPipeWorldX, 175.0f, 320.0f);
	Out.CompanionPipeAbsY = FMath::Clamp(Out.CompanionPipeAbsY, 170.0f, 275.0f);
	Out.CompanionPipeRadiusCm = FMath::Clamp(Out.CompanionPipeRadiusCm, 5.0f, 16.0f);
	Out.OuterMetalAlbedoScale = FMath::Clamp(Out.OuterMetalAlbedoScale, 0.15f, 2.20f);
	Out.OuterMetalConductorMinF0Scale = FMath::Clamp(Out.OuterMetalConductorMinF0Scale, 0.004f, 0.25f);
	Out.OuterMetalRoughnessMin = FMath::Clamp(Out.OuterMetalRoughnessMin, 0.16f, 0.70f);
	Out.OuterMetalRoughnessMax = FMath::Clamp(Out.OuterMetalRoughnessMax, FMath::Max(0.20f, Out.OuterMetalRoughnessMin), 0.85f);
	Out.OuterMetalMetallic = FMath::Clamp(Out.OuterMetalMetallic, 0.15f, 0.95f);
	Out.OuterMetalSpecularScale = FMath::Clamp(Out.OuterMetalSpecularScale, 0.15f, 1.00f);
	Out.OuterMetalAOStrength = FMath::Clamp(Out.OuterMetalAOStrength, 0.10f, 1.00f);
	Out.OuterMetalNormalStrength = FMath::Clamp(Out.OuterMetalNormalStrength, 0.25f, 2.50f);
	Out.DedicatedOuterRimIntensity = FMath::Clamp(Out.DedicatedOuterRimIntensity, 0.0f, 600.0f);
	Out.DedicatedOuterRimRadius = FMath::Clamp(Out.DedicatedOuterRimRadius, 60.0f, 220.0f);
	return Out;
}

FClampGroanAudioConfig::FClampGroanAudioConfig()
	: ClampSteelGroanLoopSound(FSoftObjectPath(TEXT("/Game/Presentation/Audio/Waves/SFX_ClampSteelGroan_Loop.SFX_ClampSteelGroan_Loop")))
{
}

FClampGroanAudioConfig FClampGroanAudioConfig::GetSanitized() const
{
	FClampGroanAudioConfig Out = *this;
	if (Out.ClampSteelGroanLoopSound.ToSoftObjectPath().IsNull())
	{
		Out.ClampSteelGroanLoopSound = TSoftObjectPtr<USoundBase>(
			FSoftObjectPath(TEXT("/Game/Presentation/Audio/Waves/SFX_ClampSteelGroan_Loop.SFX_ClampSteelGroan_Loop")));
	}

	Out.AmbientBedVolumeMultiplier = FMath::Clamp(Out.AmbientBedVolumeMultiplier, 0.0f, 1.5f);
	Out.SpatialImpulseMinVolumeMultiplier = FMath::Clamp(Out.SpatialImpulseMinVolumeMultiplier, 0.1f, 2.5f);
	Out.SpatialImpulseMaxVolumeMultiplier = FMath::Clamp(
		Out.SpatialImpulseMaxVolumeMultiplier,
		Out.SpatialImpulseMinVolumeMultiplier,
		2.5f);
	Out.MetaSoundClampStrainNeutralValue = FMath::Clamp(Out.MetaSoundClampStrainNeutralValue, 0.0f, 1.0f);

	Out.GroanOnsetStrainThreshold = FMath::Clamp(Out.GroanOnsetStrainThreshold, 0.10f, 0.75f);
	Out.GroanOnsetFatigueThreshold = FMath::Clamp(Out.GroanOnsetFatigueThreshold, 0.01f, 0.50f);
	Out.GroanZone2StrainThreshold = FMath::Clamp(
		Out.GroanZone2StrainThreshold,
		Out.GroanOnsetStrainThreshold + 0.04f,
		0.92f);
	Out.GroanZone2FatigueThreshold = FMath::Clamp(
		Out.GroanZone2FatigueThreshold,
		Out.GroanOnsetFatigueThreshold + 0.02f,
		0.85f);
	Out.GroanCriticalStrainThreshold = FMath::Clamp(
		Out.GroanCriticalStrainThreshold,
		Out.GroanZone2StrainThreshold + 0.04f,
		0.99f);
	Out.GroanCriticalFatigueThreshold = FMath::Clamp(
		Out.GroanCriticalFatigueThreshold,
		Out.GroanZone2FatigueThreshold + 0.04f,
		1.00f);

	Out.GroanZone1MinVolume = FMath::Clamp(Out.GroanZone1MinVolume, 0.02f, 1.00f);
	Out.GroanZone1MaxVolume = FMath::Clamp(Out.GroanZone1MaxVolume, Out.GroanZone1MinVolume, 1.20f);
	Out.GroanZone1MinPitch = FMath::Clamp(Out.GroanZone1MinPitch, 0.50f, 1.50f);
	Out.GroanZone1MaxPitch = FMath::Clamp(Out.GroanZone1MaxPitch, Out.GroanZone1MinPitch, 1.50f);

	Out.GroanZone2EntryVolume = FMath::Clamp(Out.GroanZone2EntryVolume, Out.GroanZone1MaxVolume, 2.00f);
	Out.GroanCriticalVolume = FMath::Clamp(Out.GroanCriticalVolume, Out.GroanZone2EntryVolume, 2.20f);
	Out.GroanPreSnapVolume = FMath::Clamp(Out.GroanPreSnapVolume, Out.GroanCriticalVolume, 2.50f);

	Out.GroanZone2EntryPitch = FMath::Clamp(Out.GroanZone2EntryPitch, Out.GroanZone1MaxPitch, 2.00f);
	Out.GroanCriticalPitch = FMath::Clamp(Out.GroanCriticalPitch, Out.GroanZone2EntryPitch, 2.00f);
	Out.GroanMaxPitchMultiplier = FMath::Clamp(Out.GroanMaxPitchMultiplier, Out.GroanCriticalPitch, 2.00f);

	Out.GroanTorqueSpikeMinDelta = FMath::Clamp(Out.GroanTorqueSpikeMinDelta, 0.005f, 0.25f);
	Out.GroanFatigueSpikeMinDelta = FMath::Clamp(Out.GroanFatigueSpikeMinDelta, 0.01f, 0.50f);
	Out.GroanSurgeBaseFloorVolume = FMath::Clamp(Out.GroanSurgeBaseFloorVolume, 0.10f, 1.50f);
	Out.GroanSliceSurgeVolumeBoost = FMath::Clamp(Out.GroanSliceSurgeVolumeBoost, 0.00f, 1.20f);
	Out.GroanSurgeMinClampVolume = FMath::Clamp(Out.GroanSurgeMinClampVolume, 0.20f, 2.00f);
	Out.GroanMaxVolumeMultiplier = FMath::Clamp(Out.GroanMaxVolumeMultiplier, Out.GroanSurgeMinClampVolume, 2.50f);
	Out.GroanSliceSurgePitchBoost = FMath::Clamp(Out.GroanSliceSurgePitchBoost, 0.00f, 0.30f);
	Out.GroanSliceSurgeDurationSeconds = FMath::Clamp(Out.GroanSliceSurgeDurationSeconds, 0.10f, 2.00f);

	Out.GroanFadeInSeconds = FMath::Clamp(Out.GroanFadeInSeconds, 0.01f, 0.25f);
	Out.GroanFastCutoffSeconds = FMath::Clamp(Out.GroanFastCutoffSeconds, 0.01f, 0.12f);
	return Out;
}

FLaserSheetPresentationConfig FLaserSheetPresentationConfig::GetSanitized() const
{
	FLaserSheetPresentationConfig Out = *this;
	Out.CoreSeamWidthCm = FMath::Clamp(Out.CoreSeamWidthCm, 0.35f, 3.00f);
	Out.ShoulderSeamWidthCm = FMath::Clamp(Out.ShoulderSeamWidthCm, Out.CoreSeamWidthCm + 0.35f, 6.00f);
	Out.GrazingReferenceSin = FMath::Clamp(Out.GrazingReferenceSin, 0.10f, 0.60f);
	Out.MinGrazingScale = FMath::Clamp(Out.MinGrazingScale, 0.02f, 1.00f);
	Out.JawSeamExclusionMarginCm = FMath::Clamp(Out.JawSeamExclusionMarginCm, 0.00f, 4.00f);
	Out.SceneMaskFeatherCm = FMath::Clamp(Out.SceneMaskFeatherCm, 0.10f, 2.50f);
	Out.SheetMaskPadXCm = FMath::Clamp(Out.SheetMaskPadXCm, 1.00f, 32.00f);
	Out.SheetMaskPadYCm = FMath::Clamp(Out.SheetMaskPadYCm, 0.50f, 16.00f);
	Out.InteriorOpacityMultiplier = FMath::Clamp(Out.InteriorOpacityMultiplier, 0.10f, 0.35f);
	Out.InteriorTroughEmissiveGain = FMath::Clamp(Out.InteriorTroughEmissiveGain, 0.02f, 0.60f);
	Out.InteriorCrestEmissiveGain = FMath::Clamp(Out.InteriorCrestEmissiveGain, Out.InteriorTroughEmissiveGain, 1.20f);
	Out.BorderEmissiveGain = FMath::Clamp(Out.BorderEmissiveGain, 1.00f, 5.00f);
	Out.OvershootEmissiveGain = FMath::Clamp(Out.OvershootEmissiveGain, 1.00f, 1.65f);
	return Out;
}

FPhase12DPresentationPolishConfig FPhase12DPresentationPolishConfig::GetSanitized() const
{
	FPhase12DPresentationPolishConfig Out = *this;
	Out.MidgroundWashIntensityMultiplier = FMath::Clamp(Out.MidgroundWashIntensityMultiplier, 0.40f, 1.20f);
	Out.MidgroundWashChromaDesaturation = FMath::Clamp(Out.MidgroundWashChromaDesaturation, 0.00f, 0.50f);
	Out.RearGridAlbedoMultiplier = FMath::Clamp(Out.RearGridAlbedoMultiplier, 0.45f, 1.15f);
	Out.RearGridConductorF0Multiplier = FMath::Clamp(Out.RearGridConductorF0Multiplier, 0.40f, 1.15f);
	Out.RearGridSpecularMultiplier = FMath::Clamp(Out.RearGridSpecularMultiplier, 0.35f, 1.15f);
	Out.RearGridProceduralWearWeight = FMath::Clamp(Out.RearGridProceduralWearWeight, 0.00f, 1.00f);
	Out.RearGridMacroSootWeight = FMath::Clamp(Out.RearGridMacroSootWeight, 0.00f, 1.00f);

	Out.ConduitRoughnessMin = FMath::Clamp(Out.ConduitRoughnessMin, 0.20f, 0.80f);
	Out.ConduitRoughnessMax = FMath::Clamp(Out.ConduitRoughnessMax, FMath::Max(0.30f, Out.ConduitRoughnessMin), 0.85f);
	Out.ConduitSpecularScale = FMath::Clamp(Out.ConduitSpecularScale, 0.10f, 1.00f);
	Out.ConduitProceduralWearWeight = FMath::Clamp(Out.ConduitProceduralWearWeight, 0.00f, 1.00f);

	Out.RightBayWindowEmissiveMultiplier = FMath::Clamp(Out.RightBayWindowEmissiveMultiplier, 1.00f, 4.00f);
	Out.RightBayWindowBaseColorMultiplier = FMath::Clamp(Out.RightBayWindowBaseColorMultiplier, 1.00f, 2.50f);
	// Strictly clamp optical lens glint emissive below 1.65f BloomThreshold and zero red Dominance (cold cyan-white only).
	Out.OpticalLensGlintEmissive.R = FMath::Clamp(Out.OpticalLensGlintEmissive.R, 0.00f, 1.55f);
	Out.OpticalLensGlintEmissive.G = FMath::Clamp(Out.OpticalLensGlintEmissive.G, 0.00f, 1.58f);
	Out.OpticalLensGlintEmissive.B = FMath::Clamp(Out.OpticalLensGlintEmissive.B, 0.00f, 1.60f);
	Out.OpticalLensGlintEmissive.A = 1.0f;
	Out.OpticalLensGlintSizeCm.X = FMath::Clamp(Out.OpticalLensGlintSizeCm.X, 0.25, 1.20);
	Out.OpticalLensGlintSizeCm.Y = FMath::Clamp(Out.OpticalLensGlintSizeCm.Y, 1.10, 2.60);
	Out.OpticalLensGlintSizeCm.Z = FMath::Clamp(Out.OpticalLensGlintSizeCm.Z, 0.95, 2.00);
	Out.OpticalLensGlintBoothLocalZ = FMath::Clamp(Out.OpticalLensGlintBoothLocalZ, 9.80f, 12.00f);
	Out.OpticalLensGlintHalfSpacingCm = FMath::Clamp(Out.OpticalLensGlintHalfSpacingCm, 1.20f, 2.80f);
	Out.OpticalLensGlintForwardOffsetCm = FMath::Clamp(Out.OpticalLensGlintForwardOffsetCm, 1.10f, 2.20f);

	Out.CutCapCoolingDurationSeconds = FMath::Clamp(Out.CutCapCoolingDurationSeconds, 0.75f, 8.00f);
	Out.CutCapCooledBounceMultiplier = FMath::Clamp(Out.CutCapCooledBounceMultiplier, 0.00f, 1.00f);
	Out.CutCapThermalDecayDurationSeconds = FMath::Clamp(Out.CutCapThermalDecayDurationSeconds, 0.50f, 8.00f);
	return Out;
}
