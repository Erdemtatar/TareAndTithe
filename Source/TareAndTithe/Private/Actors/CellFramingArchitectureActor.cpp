// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/CellFramingArchitectureActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	void ApplyZeroCostFramingFlags(UPrimitiveComponent* PrimComp)
	{
		if (!IsValid(PrimComp))
		{
			return;
		}
		PrimComp->PrimaryComponentTick.bCanEverTick = false;
		PrimComp->SetMobility(EComponentMobility::Movable);
		PrimComp->SetSimulatePhysics(false);
		PrimComp->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PrimComp->SetGenerateOverlapEvents(false);
		PrimComp->SetCanEverAffectNavigation(false);
		PrimComp->SetCastShadow(false);
		PrimComp->SetAffectDistanceFieldLighting(false);
		PrimComp->SetAffectDynamicIndirectLighting(false);
		PrimComp->SetVisibleInRayTracing(false);
		PrimComp->bReceivesDecals = false;
	}
}

ACellFramingArchitectureActor::ACellFramingArchitectureActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	FramingRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FramingRoot"));
	FramingRoot->PrimaryComponentTick.bCanEverTick = false;
	FramingRoot->SetMobility(EComponentMobility::Movable);
	SetRootComponent(FramingRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		BasicCubeMesh = CubeFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderFinder.Succeeded())
	{
		BasicCylinderMesh = CylinderFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlattenFinder(
		TEXT("/Engine/EngineMaterials/FlattenMaterial.FlattenMaterial"));
	if (FlattenFinder.Succeeded())
	{
		FallbackFlattenMaterial = FlattenFinder.Object;
	}

	OuterHeavyColumnsISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("OuterHeavyColumnsISM"));
	OuterHeavyColumnsISM->SetupAttachment(FramingRoot);
	ApplyZeroCostFramingFlags(OuterHeavyColumnsISM);

	OuterFlangedPipesISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("OuterFlangedPipesISM"));
	OuterFlangedPipesISM->SetupAttachment(FramingRoot);
	ApplyZeroCostFramingFlags(OuterFlangedPipesISM);

	OuterStructuralRibsISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("OuterStructuralRibsISM"));
	OuterStructuralRibsISM->SetupAttachment(FramingRoot);
	ApplyZeroCostFramingFlags(OuterStructuralRibsISM);
}

int32 ACellFramingArchitectureActor::GetTotalFramingInstanceCount() const
{
	const int32 ColCount = IsValid(OuterHeavyColumnsISM) ? OuterHeavyColumnsISM->GetInstanceCount() : 0;
	const int32 PipeCount = IsValid(OuterFlangedPipesISM) ? OuterFlangedPipesISM->GetInstanceCount() : 0;
	const int32 RibCount = IsValid(OuterStructuralRibsISM) ? OuterStructuralRibsISM->GetInstanceCount() : 0;
	return ColCount + PipeCount + RibCount;
}

void ACellFramingArchitectureActor::ConfigureFraming(const UTareTithePresentationDataAsset* InPresentationData)
{
	ApplyZeroCostFramingFlags(OuterHeavyColumnsISM);
	ApplyZeroCostFramingFlags(OuterFlangedPipesISM);
	ApplyZeroCostFramingFlags(OuterStructuralRibsISM);

	if (IsValid(OuterHeavyColumnsISM))
	{
		OuterHeavyColumnsISM->ClearInstances();
	}
	if (IsValid(OuterFlangedPipesISM))
	{
		OuterFlangedPipesISM->ClearInstances();
	}
	if (IsValid(OuterStructuralRibsISM))
	{
		OuterStructuralRibsISM->ClearInstances();
	}

	const FOuterEdgeFramingConfig Cfg = IsValid(InPresentationData)
		? InPresentationData->OuterEdgeFraming.GetSanitized()
		: FOuterEdgeFramingConfig();

	RebuildOptionalDedicatedRimLights(Cfg);

	if (!Cfg.bEnableOuterArchitecturalFraming)
	{
		return;
	}

	// Resolve ISM-compiled PBR material (MI_Station_ClampSpringHousing / M_DarkGunmetal_Master both have MATUSAGE_INSTANCED_STATIC_MESHES=True).
	UStaticMesh* ColumnMesh = BasicCubeMesh.Get();
	UMaterialInterface* PbrIsmBaseMat = nullptr;
	if (IsValid(InPresentationData))
	{
		if (IsValid(InPresentationData->ClampSpringHousingMeshSlot.Mesh))
		{
			ColumnMesh = InPresentationData->ClampSpringHousingMeshSlot.Mesh;
		}
		if (InPresentationData->ClampSpringHousingMeshSlot.MaterialOverrides.Num() > 0 &&
			IsValid(InPresentationData->ClampSpringHousingMeshSlot.MaterialOverrides[0]))
		{
			PbrIsmBaseMat = InPresentationData->ClampSpringHousingMeshSlot.MaterialOverrides[0];
		}
		else if (IsValid(InPresentationData->DarkGunmetalMaterial))
		{
			PbrIsmBaseMat = InPresentationData->DarkGunmetalMaterial.Get();
		}
	}
	if (!IsValid(PbrIsmBaseMat))
	{
		PbrIsmBaseMat = FallbackFlattenMaterial.Get();
	}

	if (IsValid(PbrIsmBaseMat))
	{
		OuterColumnsAndPipesMID = UMaterialInstanceDynamic::Create(PbrIsmBaseMat, this);
		if (IsValid(OuterColumnsAndPipesMID))
		{
			const float MS = Cfg.OuterMetalAlbedoScale;
			OuterColumnsAndPipesMID->SetVectorParameterValue(TEXT("AlbedoTint"), FLinearColor(0.54f * MS, 0.62f * MS, 0.74f * MS, 1.0f));
			OuterColumnsAndPipesMID->SetVectorParameterValue(TEXT("ConductorMinF0"), FLinearColor(0.22f * MS, 0.26f * MS, 0.31f * MS, 1.0f));
			OuterColumnsAndPipesMID->SetScalarParameterValue(TEXT("AOStrength"), 0.35f);
			OuterColumnsAndPipesMID->SetScalarParameterValue(TEXT("RoughnessMin"), Cfg.OuterMetalRoughnessMin);
			OuterColumnsAndPipesMID->SetScalarParameterValue(TEXT("RoughnessMax"), Cfg.OuterMetalRoughnessMax);
			OuterColumnsAndPipesMID->SetScalarParameterValue(TEXT("MetallicScale"), 1.0f);
			OuterColumnsAndPipesMID->SetScalarParameterValue(TEXT("MetallicMin"), Cfg.OuterMetalMetallic * 0.85f);
			OuterColumnsAndPipesMID->SetScalarParameterValue(TEXT("MetallicMax"), Cfg.OuterMetalMetallic);
			OuterColumnsAndPipesMID->SetScalarParameterValue(TEXT("SpecularScale"), 0.75f * Cfg.OuterMetalSpecularScale);
			OuterColumnsAndPipesMID->SetScalarParameterValue(TEXT("NormalStrength"), 1.05f);
			OuterColumnsAndPipesMID->SetVectorParameterValue(TEXT("BaseColorConst"), FLinearColor(0.16f * MS, 0.19f * MS, 0.23f * MS, 1.0f));
			OuterColumnsAndPipesMID->SetScalarParameterValue(TEXT("RoughnessConst"), 0.52f);
			OuterColumnsAndPipesMID->SetScalarParameterValue(TEXT("MetallicConst"), Cfg.OuterMetalMetallic);
		}

		// Dark matte structural tie-gusset & upper HUD soffit material ensuring V in [0.01, 0.16] stays calm (mean <= 12 sRGB) behind Top-Left/Right HUD cards.
		OuterDarkSoffitMID = UMaterialInstanceDynamic::Create(PbrIsmBaseMat, this);
		if (IsValid(OuterDarkSoffitMID))
		{
			OuterDarkSoffitMID->SetVectorParameterValue(TEXT("AlbedoTint"), FLinearColor(0.016f, 0.020f, 0.026f, 1.0f));
			OuterDarkSoffitMID->SetVectorParameterValue(TEXT("ConductorMinF0"), FLinearColor(0.008f, 0.010f, 0.013f, 1.0f));
			OuterDarkSoffitMID->SetScalarParameterValue(TEXT("RoughnessMin"), 0.78f);
			OuterDarkSoffitMID->SetScalarParameterValue(TEXT("RoughnessMax"), 0.85f);
			OuterDarkSoffitMID->SetScalarParameterValue(TEXT("MetallicScale"), 1.0f);
			OuterDarkSoffitMID->SetScalarParameterValue(TEXT("MetallicMin"), 0.45f);
			OuterDarkSoffitMID->SetScalarParameterValue(TEXT("MetallicMax"), 0.55f);
			OuterDarkSoffitMID->SetScalarParameterValue(TEXT("SpecularScale"), 0.05f);
			OuterDarkSoffitMID->SetScalarParameterValue(TEXT("NormalStrength"), 0.25f);
			OuterDarkSoffitMID->SetVectorParameterValue(TEXT("BaseColorConst"), FLinearColor(0.008f, 0.010f, 0.014f, 1.0f));
			OuterDarkSoffitMID->SetScalarParameterValue(TEXT("RoughnessConst"), 0.85f);
			OuterDarkSoffitMID->SetScalarParameterValue(TEXT("SpecularConst"), 0.05f);
		}
	}

	auto AddSizedWorldInstance = [](
		UInstancedStaticMeshComponent* TargetISM,
		const UStaticMesh* SourceMesh,
		const FVector& WorldCenter,
		const FVector& WorldSizeCm,
		const FRotator& WorldRot = FRotator::ZeroRotator)
	{
		if (!IsValid(TargetISM) || !IsValid(SourceMesh))
		{
			return;
		}
		const FBox Box = SourceMesh->GetBoundingBox();
		const FVector LocalSize = Box.GetSize();
		const FVector LocalCenter = Box.GetCenter();
		const FQuat Quat = WorldRot.Quaternion();
		const FVector Scale(
			WorldSizeCm.X / FMath::Max(1.0, LocalSize.X),
			WorldSizeCm.Y / FMath::Max(1.0, LocalSize.Y),
			WorldSizeCm.Z / FMath::Max(1.0, LocalSize.Z));
		const FVector PivotTranslation = WorldCenter - Quat.RotateVector(Scale * LocalCenter);
		TargetISM->AddInstance(FTransform(Quat, PivotTranslation, Scale), true);
	};

	// 1. Midground Heavy Chamfered Stanchion Columns (SM_ClampSpringHousing, Z in [-95, +255] cm).
	if (IsValid(OuterHeavyColumnsISM) && IsValid(ColumnMesh))
	{
		OuterHeavyColumnsISM->SetStaticMesh(ColumnMesh);
		if (IsValid(OuterColumnsAndPipesMID))
		{
			OuterHeavyColumnsISM->SetMaterial(0, OuterColumnsAndPipesMID);
		}

		const double ColX = static_cast<double>(Cfg.MidgroundColumnWorldX);
		const double ColAbsY = static_cast<double>(Cfg.MidgroundColumnAbsY);
		const double ColWidth = 48.0 * static_cast<double>(Cfg.MidgroundColumnXYScale);
		constexpr double FloorZ = -95.0;
		constexpr double TopZ = 216.0;
		constexpr int32 ModuleCount = 5;
		const double ModH = (TopZ - FloorZ) / static_cast<double>(ModuleCount);

		for (const double SideSign : { -1.0, 1.0 })
		{
			// Angle heavy chamfered stanchions 35 deg inward so front/bevel faces catch cross-axis RimSpotLeft/Right (N.L > 0.55) + CellFillPoint.
			const FRotator InwardColRot(0.0f, static_cast<float>(SideSign * 35.0), 0.0f);
			for (int32 ModIdx = 0; ModIdx < ModuleCount; ++ModIdx)
			{
				const double ModCenterZ = FloorZ + ((static_cast<double>(ModIdx) + 0.5) * ModH);
				AddSizedWorldInstance(
					OuterHeavyColumnsISM,
					ColumnMesh,
					FVector(ColX, SideSign * ColAbsY, ModCenterZ),
					FVector(ColWidth, ColWidth, ModH),
					InwardColRot);
			}
		}
	}

	// 2. Foreground & Companion Cylindrical Conduit Risers + Bolted Flange Collars (BasicCylinderMesh, Z <= +218..236 cm).
	if (IsValid(OuterFlangedPipesISM) && IsValid(BasicCylinderMesh))
	{
		OuterFlangedPipesISM->SetStaticMesh(BasicCylinderMesh);
		if (IsValid(OuterColumnsAndPipesMID))
		{
			OuterFlangedPipesISM->SetMaterial(0, OuterColumnsAndPipesMID);
		}

		const double ForeX = static_cast<double>(Cfg.ForegroundPipeWorldX);
		const double ForeAbsY = static_cast<double>(Cfg.ForegroundPipeAbsY);
		const double ForeDiam = 2.0 * static_cast<double>(Cfg.ForegroundPipeRadiusCm);
		const double ForeFlangeDiam = ForeDiam * 1.36;

		const double CompX = static_cast<double>(Cfg.CompanionPipeWorldX);
		const double CompAbsY = static_cast<double>(Cfg.CompanionPipeAbsY);
		const double CompDiam = 2.0 * static_cast<double>(Cfg.CompanionPipeRadiusCm);
		const double CompFlangeDiam = CompDiam * 1.38;

		for (const double SideSign : { -1.0, 1.0 })
		{
			// Foreground heavy vertical riser (Z in [-95, +218] cm -> stops cleanly below Top-Left/Top-Right HUD V < 0.16 zone).
			AddSizedWorldInstance(
				OuterFlangedPipesISM,
				BasicCylinderMesh,
				FVector(ForeX, SideSign * ForeAbsY, 61.5),
				FVector(ForeDiam, ForeDiam, 313.0));

			for (const double FlangeZ : { -42.0, 12.0, 66.0, 120.0, 174.0 })
			{
				AddSizedWorldInstance(
					OuterFlangedPipesISM,
					BasicCylinderMesh,
					FVector(ForeX, SideSign * ForeAbsY, FlangeZ),
					FVector(ForeFlangeDiam, ForeFlangeDiam, 6.5));
			}

			// Companion high-pressure riser (Z in [-95, +216] cm -> stops cleanly below Top-Left/Top-Right HUD V < 0.16 zone).
			AddSizedWorldInstance(
				OuterFlangedPipesISM,
				BasicCylinderMesh,
				FVector(CompX, SideSign * CompAbsY, 60.5),
				FVector(CompDiam, CompDiam, 311.0));

			for (const double FlangeZ : { -18.0, 42.0, 102.0, 162.0 })
			{
				AddSizedWorldInstance(
					OuterFlangedPipesISM,
					BasicCylinderMesh,
					FVector(CompX, SideSign * CompAbsY, FlangeZ),
					FVector(CompFlangeDiam, CompFlangeDiam, 5.0));
			}
		}
	}

	// 3. Horizontal Structural Tie-Gussets & Upper Matte HUD Contrast Soffits (BasicCubeMesh).
	if (IsValid(OuterStructuralRibsISM) && IsValid(BasicCubeMesh))
	{
		OuterStructuralRibsISM->SetStaticMesh(BasicCubeMesh);
		if (IsValid(OuterDarkSoffitMID))
		{
			OuterStructuralRibsISM->SetMaterial(0, OuterDarkSoffitMID);
		}

		const double ForeX = static_cast<double>(Cfg.ForegroundPipeWorldX);
		const double ForeAbsY = static_cast<double>(Cfg.ForegroundPipeAbsY);
		const double ColX = static_cast<double>(Cfg.MidgroundColumnWorldX);

		for (const double SideSign : { -1.0, 1.0 })
		{
			// Lateral structural tie-braces locking risers into the outer cell wall at 3 elevations (strictly outside |Y| >= 208 cm -> >130 px clear of Panopticon bays).
			for (const double BraceZ : { -42.0, 66.0, 174.0 })
			{
				AddSizedWorldInstance(
					OuterStructuralRibsISM,
					BasicCubeMesh,
					FVector(ForeX + 4.0, SideSign * 276.0, BraceZ),
					FVector(16.0, 136.0, 9.5));

				AddSizedWorldInstance(
					OuterStructuralRibsISM,
					BasicCubeMesh,
					FVector(0.5 * (ForeX + ColX), SideSign * 236.0, BraceZ),
					FVector(FMath::Max(24.0, ColX - ForeX), 12.0, 8.5));
			}

			// Upper dark matte manifold header box above Z = +218 cm (screens V in [0.00, 0.16] behind Top-Left/Right Slate HUD panels while staying >= 137 px clear of Panopticon slits).
			AddSizedWorldInstance(
				OuterStructuralRibsISM,
				BasicCubeMesh,
				FVector(0.5 * (ForeX + ColX), SideSign * 290.0, 322.0),
				FVector(FMath::Max(40.0, (ColX - ForeX) + 36.0), 140.0, 208.0));
		}
	}
}

void ACellFramingArchitectureActor::RebuildOptionalDedicatedRimLights(const FOuterEdgeFramingConfig& Cfg)
{
	auto DestroyOptionalLight = [](TObjectPtr<UPointLightComponent>& LightPtr)
	{
		if (IsValid(LightPtr))
		{
			LightPtr->DestroyComponent();
			LightPtr = nullptr;
		}
	};

	if (!Cfg.bEnableOuterArchitecturalFraming || !Cfg.bEnableDedicatedOuterRimLights || Cfg.DedicatedOuterRimIntensity <= KINDA_SMALL_NUMBER)
	{
		DestroyOptionalLight(LeftDedicatedRimLight);
		DestroyOptionalLight(RightDedicatedRimLight);
		return;
	}

	auto EnsureRimLight = [&](TObjectPtr<UPointLightComponent>& LightPtr, const TCHAR* Name, double SignY)
	{
		if (!IsValid(LightPtr))
		{
			LightPtr = NewObject<UPointLightComponent>(this, Name);
			if (IsValid(LightPtr))
			{
				LightPtr->PrimaryComponentTick.bCanEverTick = false;
				LightPtr->SetMobility(EComponentMobility::Movable);
				LightPtr->SetupAttachment(FramingRoot);
				LightPtr->RegisterComponent();
			}
		}
		if (IsValid(LightPtr))
		{
			LightPtr->SetWorldLocation(FVector(
				static_cast<double>(Cfg.ForegroundPipeWorldX) - 28.0,
				SignY * (static_cast<double>(Cfg.ForegroundPipeAbsY) - 26.0),
				85.0));
			LightPtr->SetIntensity(Cfg.DedicatedOuterRimIntensity);
			LightPtr->SetAttenuationRadius(Cfg.DedicatedOuterRimRadius);
			LightPtr->SetSourceRadius(12.0f);
			LightPtr->SetSourceLength(90.0f);
			LightPtr->SetCastShadows(false);
			LightPtr->SetVolumetricScatteringIntensity(0.0f);
			LightPtr->SetLightColor(FLinearColor(0.70f, 0.84f, 1.00f, 1.0f));
		}
	};

	EnsureRimLight(LeftDedicatedRimLight, TEXT("LeftDedicatedOuterRimLight"), -1.0);
	EnsureRimLight(RightDedicatedRimLight, TEXT("RightDedicatedOuterRimLight"), 1.0);
}
