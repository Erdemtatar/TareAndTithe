// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/TareTithePresentationDataAsset.h"
#include "Components/StaticMeshComponent.h"
#include "Data/ShiftProgressionDataAsset.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

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
