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
