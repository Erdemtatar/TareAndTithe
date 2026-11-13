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
	KeySpot.Location = FVector(182.0, -16.0, 336.0);
	KeySpot.TargetLocation = FVector(260.0, 4.0, 118.0);
	KeySpot.Intensity = 14800.0f;
	KeySpot.AttenuationRadius = 480.0f;
	KeySpot.InnerConeAngle = 19.0f;
	KeySpot.OuterConeAngle = 35.0f;
	KeySpot.LightColor = FLinearColor(0.96f, 0.97f, 1.00f, 1.0f);
	KeySpot.bCastShadows = true;

	RimSpotLeft.Location = FVector(318.0, -148.0, 242.0);
	RimSpotLeft.TargetLocation = FVector(260.0, -8.0, 148.0);
	RimSpotLeft.Intensity = 11200.0f;
	RimSpotLeft.AttenuationRadius = 385.0f;
	RimSpotLeft.InnerConeAngle = 18.0f;
	RimSpotLeft.OuterConeAngle = 36.0f;
	RimSpotLeft.LightColor = FLinearColor(0.66f, 0.84f, 1.00f, 1.0f);
	RimSpotLeft.bCastShadows = false;

	RimSpotRight.Location = FVector(318.0, 152.0, 236.0);
	RimSpotRight.TargetLocation = FVector(260.0, 14.0, 140.0);
	RimSpotRight.Intensity = 10800.0f;
	RimSpotRight.AttenuationRadius = 385.0f;
	RimSpotRight.InnerConeAngle = 18.0f;
	RimSpotRight.OuterConeAngle = 36.0f;
	RimSpotRight.LightColor = FLinearColor(0.64f, 0.82f, 1.00f, 1.0f);
	RimSpotRight.bCastShadows = false;

	// Guard 1: Upward-raking ~2150K foundry pit glow; R = 345 cm (< 360 cm) guarantees 0.000 side-wall irradiance while 56 deg cone illuminates Y in [-155, +155].
	MidgroundWashSpot.Location = FVector(405.0, 0.0, -55.0);
	MidgroundWashSpot.TargetLocation = FVector(448.0, 0.0, 110.0);
	MidgroundWashSpot.Intensity = 9600.0f;
	MidgroundWashSpot.AttenuationRadius = 345.0f;
	MidgroundWashSpot.InnerConeAngle = 28.0f;
	MidgroundWashSpot.OuterConeAngle = 56.0f;
	MidgroundWashSpot.LightColor = FLinearColor(1.00f, 0.34f, 0.07f, 1.0f);
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
