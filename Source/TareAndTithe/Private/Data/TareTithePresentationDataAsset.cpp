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
	KeySpot.Location = FVector(175.0, -25.0, 335.0);
	KeySpot.TargetLocation = FVector(260.0, 0.0, 120.0);
	KeySpot.Intensity = 12200.0f;
	KeySpot.AttenuationRadius = 1080.0f;
	KeySpot.InnerConeAngle = 26.0f;
	KeySpot.OuterConeAngle = 58.0f;
	KeySpot.LightColor = FLinearColor(0.95f, 0.96f, 1.00f, 1.0f);
	KeySpot.bCastShadows = true;

	RimSpotLeft.Location = FVector(338.0, -190.0, 248.0);
	RimSpotLeft.TargetLocation = FVector(255.0, -14.0, 112.0);
	RimSpotLeft.Intensity = 9400.0f;
	RimSpotLeft.AttenuationRadius = 780.0f;
	RimSpotLeft.InnerConeAngle = 20.0f;
	RimSpotLeft.OuterConeAngle = 52.0f;
	RimSpotLeft.LightColor = FLinearColor(0.68f, 0.85f, 1.00f, 1.0f);
	RimSpotLeft.bCastShadows = false;

	RimSpotRight.Location = FVector(338.0, 195.0, 228.0);
	RimSpotRight.TargetLocation = FVector(255.0, 14.0, 102.0);
	RimSpotRight.Intensity = 9000.0f;
	RimSpotRight.AttenuationRadius = 780.0f;
	RimSpotRight.InnerConeAngle = 20.0f;
	RimSpotRight.OuterConeAngle = 52.0f;
	RimSpotRight.LightColor = FLinearColor(0.65f, 0.83f, 1.00f, 1.0f);
	RimSpotRight.bCastShadows = false;

	MidgroundWashSpot.Location = FVector(190.0, 0.0, 352.0);
	MidgroundWashSpot.TargetLocation = FVector(495.0, 0.0, 96.0);
	MidgroundWashSpot.Intensity = 5800.0f;
	MidgroundWashSpot.AttenuationRadius = 720.0f;
	MidgroundWashSpot.InnerConeAngle = 28.0f;
	MidgroundWashSpot.OuterConeAngle = 64.0f;
	MidgroundWashSpot.LightColor = FLinearColor(0.70f, 0.84f, 1.00f, 1.0f);
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
