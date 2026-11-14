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

	OuterTieGussetsISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("OuterTieGussetsISM"));
	OuterTieGussetsISM->SetupAttachment(FramingRoot);
	ApplyZeroCostFramingFlags(OuterTieGussetsISM);

	OuterStructuralRibsISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("OuterStructuralRibsISM"));
	OuterStructuralRibsISM->SetupAttachment(FramingRoot);
	ApplyZeroCostFramingFlags(OuterStructuralRibsISM);
}

int32 ACellFramingArchitectureActor::GetTotalFramingInstanceCount() const
{
	const int32 ColCount = IsValid(OuterHeavyColumnsISM) ? OuterHeavyColumnsISM->GetInstanceCount() : 0;
	const int32 PipeCount = IsValid(OuterFlangedPipesISM) ? OuterFlangedPipesISM->GetInstanceCount() : 0;
	const int32 GussetCount = IsValid(OuterTieGussetsISM) ? OuterTieGussetsISM->GetInstanceCount() : 0;
	const int32 RibCount = IsValid(OuterStructuralRibsISM) ? OuterStructuralRibsISM->GetInstanceCount() : 0;
	return ColCount + PipeCount + GussetCount + RibCount;
}

void ACellFramingArchitectureActor::ConfigureFraming(const UTareTithePresentationDataAsset* InPresentationData)
{
	ApplyZeroCostFramingFlags(OuterHeavyColumnsISM);
	ApplyZeroCostFramingFlags(OuterFlangedPipesISM);
	ApplyZeroCostFramingFlags(OuterTieGussetsISM);
	ApplyZeroCostFramingFlags(OuterStructuralRibsISM);

	if (IsValid(OuterHeavyColumnsISM))
	{
		OuterHeavyColumnsISM->ClearInstances();
	}
	if (IsValid(OuterFlangedPipesISM))
	{
		OuterFlangedPipesISM->ClearInstances();
	}
	if (IsValid(OuterTieGussetsISM))
	{
		OuterTieGussetsISM->ClearInstances();
	}
	if (IsValid(OuterStructuralRibsISM))
	{
		OuterStructuralRibsISM->ClearInstances();
	}

	const FOuterEdgeFramingConfig Cfg = IsValid(InPresentationData)
		? InPresentationData->OuterEdgeFraming.GetSanitized()
		: FOuterEdgeFramingConfig();
	const FPhase12DPresentationPolishConfig Phase12DCfg = IsValid(InPresentationData)
		? InPresentationData->Phase12DPolishConfig.GetSanitized()
		: FPhase12DPresentationPolishConfig();

	RebuildOptionalDedicatedRimLights(Cfg);

	if (!Cfg.bEnableOuterArchitecturalFraming)
	{
		return;
	}

	// Resolve ISM-compiled Rodin PBR materials (MI_Station_ClampSpringHousing for columns, MI_Station_TareChute for dark cast-iron pipes & gussets).
	UStaticMesh* ColumnMesh = BasicCubeMesh.Get();
	UMaterialInterface* ColumnBaseMat = nullptr;
	UMaterialInterface* PipeBaseMat = nullptr;
	if (IsValid(InPresentationData))
	{
		if (IsValid(InPresentationData->ClampSpringHousingMeshSlot.Mesh))
		{
			ColumnMesh = InPresentationData->ClampSpringHousingMeshSlot.Mesh;
		}
		if (InPresentationData->ClampSpringHousingMeshSlot.MaterialOverrides.Num() > 0 &&
			IsValid(InPresentationData->ClampSpringHousingMeshSlot.MaterialOverrides[0]))
		{
			ColumnBaseMat = InPresentationData->ClampSpringHousingMeshSlot.MaterialOverrides[0];
		}
		else if (IsValid(InPresentationData->DarkGunmetalMaterial))
		{
			ColumnBaseMat = InPresentationData->DarkGunmetalMaterial.Get();
		}

		if (InPresentationData->TareChuteMeshSlot.MaterialOverrides.Num() > 0 &&
			IsValid(InPresentationData->TareChuteMeshSlot.MaterialOverrides[0]))
		{
			PipeBaseMat = InPresentationData->TareChuteMeshSlot.MaterialOverrides[0];
		}
	}
	if (!IsValid(ColumnBaseMat))
	{
		ColumnBaseMat = FallbackFlattenMaterial.Get();
	}
	if (!IsValid(PipeBaseMat))
	{
		PipeBaseMat = ColumnBaseMat;
	}

	auto ApplyDarkOiledCastIronParams = [&Cfg, &Phase12DCfg](UMaterialInstanceDynamic* TargetMID)
	{
		if (!IsValid(TargetMID))
		{
			return;
		}
		const float MS = Cfg.OuterMetalAlbedoScale;
		const float F0S = Cfg.OuterMetalConductorMinF0Scale;
		// Phase 12-FIX2: Preserve 100% of Rodin cast-iron grain (ConductorMinF0 <= 0.02) while oiled Fresnel F0 catches grazing steel highlights.
		TargetMID->SetVectorParameterValue(TEXT("AlbedoTint"), FLinearColor(0.90f * MS, 1.00f * MS, 1.12f * MS, 1.0f));
		TargetMID->SetVectorParameterValue(TEXT("ConductorMinF0"), FLinearColor(0.85f * F0S, 1.00f * F0S, 1.18f * F0S, 1.0f));
		TargetMID->SetScalarParameterValue(TEXT("AOStrength"), Cfg.OuterMetalAOStrength);
		TargetMID->SetScalarParameterValue(TEXT("RoughnessMin"), Cfg.OuterMetalRoughnessMin);
		TargetMID->SetScalarParameterValue(TEXT("RoughnessMax"), Cfg.OuterMetalRoughnessMax);
		TargetMID->SetScalarParameterValue(TEXT("RoughnessContrastExp"), 1.00f);
		TargetMID->SetScalarParameterValue(TEXT("MetallicScale"), 1.00f);
		TargetMID->SetScalarParameterValue(TEXT("MetallicMin"), FMath::Clamp(Cfg.OuterMetalMetallic * 0.85f, 0.15f, Cfg.OuterMetalMetallic));
		TargetMID->SetScalarParameterValue(TEXT("MetallicMax"), Cfg.OuterMetalMetallic);
		TargetMID->SetScalarParameterValue(TEXT("SpecularScale"), Cfg.OuterMetalSpecularScale);
		TargetMID->SetScalarParameterValue(TEXT("NormalStrength"), Cfg.OuterMetalNormalStrength);
		TargetMID->SetScalarParameterValue(TEXT("ProceduralWearWeight"), Phase12DCfg.bEnableProceduralStationWear ? 0.55f : 0.0f);
		TargetMID->SetScalarParameterValue(TEXT("RearGridMacroSootWeight"), 0.0f);
		TargetMID->SetVectorParameterValue(TEXT("BaseColorConst"), FLinearColor(0.055f * MS, 0.064f * MS, 0.075f * MS, 1.0f));
		TargetMID->SetScalarParameterValue(TEXT("RoughnessConst"), Cfg.OuterMetalRoughnessMax);
		TargetMID->SetScalarParameterValue(TEXT("MetallicConst"), Cfg.OuterMetalMetallic);
	};

	if (IsValid(ColumnBaseMat))
	{
		OuterColumnsMID = UMaterialInstanceDynamic::Create(ColumnBaseMat, this);
		ApplyDarkOiledCastIronParams(OuterColumnsMID);
		if (IsValid(OuterColumnsMID))
		{
			OuterColumnsMID->SetScalarParameterValue(TEXT("RoughnessMin"), FMath::Max(0.56f, Cfg.OuterMetalRoughnessMin));
			OuterColumnsMID->SetScalarParameterValue(TEXT("RoughnessMax"), FMath::Max(0.70f, Cfg.OuterMetalRoughnessMax));
		}

		// Dark matte upper HUD soffit material ensuring V in [0.00, 0.16] stays calm (mean <= 12 sRGB) behind Top-Left/Right HUD cards.
		OuterDarkSoffitMID = UMaterialInstanceDynamic::Create(ColumnBaseMat, this);
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
			OuterDarkSoffitMID->SetScalarParameterValue(TEXT("ProceduralWearWeight"), 0.0f);
			OuterDarkSoffitMID->SetScalarParameterValue(TEXT("RearGridMacroSootWeight"), 0.0f);
			OuterDarkSoffitMID->SetVectorParameterValue(TEXT("BaseColorConst"), FLinearColor(0.008f, 0.010f, 0.014f, 1.0f));
			OuterDarkSoffitMID->SetScalarParameterValue(TEXT("RoughnessConst"), 0.85f);
			OuterDarkSoffitMID->SetScalarParameterValue(TEXT("SpecularConst"), 0.05f);
		}
	}

	if (IsValid(PipeBaseMat))
	{
		OuterPipesMID = UMaterialInstanceDynamic::Create(PipeBaseMat, this);
		ApplyDarkOiledCastIronParams(OuterPipesMID);
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

	// 1. Midground Heavy Chamfered Stanchion Columns (SM_ClampSpringHousing, 2 x 5 = 10 instances).
	if (IsValid(OuterHeavyColumnsISM) && IsValid(ColumnMesh))
	{
		OuterHeavyColumnsISM->SetStaticMesh(ColumnMesh);
		if (IsValid(OuterColumnsMID))
		{
			OuterHeavyColumnsISM->SetMaterial(0, OuterColumnsMID);
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

	// 2. Modular Dark Cast-Iron Cylindrical Conduit Courses + Bolted Flange Collars (BasicCylinderMesh, 22 + 18 = 40 instances).
	if (IsValid(OuterFlangedPipesISM) && IsValid(BasicCylinderMesh))
	{
		OuterFlangedPipesISM->SetStaticMesh(BasicCylinderMesh);
		if (IsValid(OuterPipesMID))
		{
			OuterFlangedPipesISM->SetMaterial(0, OuterPipesMID);
		}

		const double ForeX = static_cast<double>(Cfg.ForegroundPipeWorldX);
		const double ForeAbsY = static_cast<double>(Cfg.ForegroundPipeAbsY);
		const double ForeDiam = 2.0 * static_cast<double>(Cfg.ForegroundPipeRadiusCm);
		const double ForeFlangeDiam = ForeDiam * 1.36;

		const double CompX = static_cast<double>(Cfg.CompanionPipeWorldX);
		const double CompAbsY = static_cast<double>(Cfg.CompanionPipeAbsY);
		const double CompDiam = 2.0 * static_cast<double>(Cfg.CompanionPipeRadiusCm);
		const double CompFlangeDiam = CompDiam * 1.38;

		constexpr double ForeCourseBreaks[7] = { -95.0, -42.0, 12.0, 66.0, 120.0, 174.0, 218.0 };
		constexpr double CompCourseBreaks[6] = { -95.0, -18.0, 42.0, 102.0, 162.0, 216.0 };

		for (const double SideSign : { -1.0, 1.0 })
		{
			const double SideYawBias = (SideSign > 0.0) ? 68.75 : 0.0;

			// Foreground heavy vertical riser segmented into 6 square-UV cast-iron courses + 5 bolted flange collars (Z in [-95, +218] cm).
			for (int32 CourseIdx = 0; CourseIdx < 6; ++CourseIdx)
			{
				const double Z0 = ForeCourseBreaks[CourseIdx];
				const double Z1 = ForeCourseBreaks[CourseIdx + 1];
				const FRotator CourseRot(0.0f, static_cast<float>((CourseIdx * 137.5) + SideYawBias), 0.0f);
				AddSizedWorldInstance(
					OuterFlangedPipesISM,
					BasicCylinderMesh,
					FVector(ForeX, SideSign * ForeAbsY, 0.5 * (Z0 + Z1)),
					FVector(ForeDiam, ForeDiam, Z1 - Z0),
					CourseRot);
			}

			for (int32 FlangeIdx = 0; FlangeIdx < 5; ++FlangeIdx)
			{
				const double FlangeZ = ForeCourseBreaks[FlangeIdx + 1];
				const FRotator CollarRot(0.0f, static_cast<float>((FlangeIdx * 137.5) + 43.0 + SideYawBias), 0.0f);
				AddSizedWorldInstance(
					OuterFlangedPipesISM,
					BasicCylinderMesh,
					FVector(ForeX, SideSign * ForeAbsY, FlangeZ),
					FVector(ForeFlangeDiam, ForeFlangeDiam, 6.5),
					CollarRot);
			}

			// Companion high-pressure riser segmented into 5 square-UV cast-iron courses + 4 bolted flange collars (Z in [-95, +216] cm).
			for (int32 CourseIdx = 0; CourseIdx < 5; ++CourseIdx)
			{
				const double Z0 = CompCourseBreaks[CourseIdx];
				const double Z1 = CompCourseBreaks[CourseIdx + 1];
				const FRotator CourseRot(0.0f, static_cast<float>((CourseIdx * 137.5) + 27.0 + SideYawBias), 0.0f);
				AddSizedWorldInstance(
					OuterFlangedPipesISM,
					BasicCylinderMesh,
					FVector(CompX, SideSign * CompAbsY, 0.5 * (Z0 + Z1)),
					FVector(CompDiam, CompDiam, Z1 - Z0),
					CourseRot);
			}

			for (int32 FlangeIdx = 0; FlangeIdx < 4; ++FlangeIdx)
			{
				const double FlangeZ = CompCourseBreaks[FlangeIdx + 1];
				const FRotator CollarRot(0.0f, static_cast<float>((FlangeIdx * 137.5) + 82.0 + SideYawBias), 0.0f);
				AddSizedWorldInstance(
					OuterFlangedPipesISM,
					BasicCylinderMesh,
					FVector(CompX, SideSign * CompAbsY, FlangeZ),
					FVector(CompFlangeDiam, CompFlangeDiam, 5.0),
					CollarRot);
			}
		}
	}

	// 3. Lateral Cast-Iron Structural Tie-Gussets (OuterTieGussetsISM, BasicCubeMesh, 2 x 3 x 2 = 12 instances).
	if (IsValid(OuterTieGussetsISM) && IsValid(BasicCubeMesh))
	{
		OuterTieGussetsISM->SetStaticMesh(BasicCubeMesh);
		if (IsValid(OuterPipesMID))
		{
			OuterTieGussetsISM->SetMaterial(0, OuterPipesMID);
		}

		const double ForeX = static_cast<double>(Cfg.ForegroundPipeWorldX);
		const double ColX = static_cast<double>(Cfg.MidgroundColumnWorldX);
		const double ForeAbsY = static_cast<double>(Cfg.ForegroundPipeAbsY);
		const double CompAbsY = static_cast<double>(Cfg.CompanionPipeAbsY);

		for (const double SideSign : { -1.0, 1.0 })
		{
			for (const double BraceZ : { -42.0, 66.0, 174.0 })
			{
				AddSizedWorldInstance(
					OuterTieGussetsISM,
					BasicCubeMesh,
					FVector(ForeX + 6.0, SideSign * (0.5 * (ForeAbsY + CompAbsY) + 12.0), BraceZ),
					FVector(16.0, 48.0, 9.5));

				AddSizedWorldInstance(
					OuterTieGussetsISM,
					BasicCubeMesh,
					FVector(0.5 * (ForeX + ColX), SideSign * (0.5 * (ForeAbsY + CompAbsY)), BraceZ),
					FVector(FMath::Max(24.0, ColX - ForeX), 12.0, 8.5));
			}
		}
	}

	// 4. Upper Dark Matte HUD Contrast Soffits (OuterStructuralRibsISM, BasicCubeMesh, 2 instances).
	if (IsValid(OuterStructuralRibsISM) && IsValid(BasicCubeMesh))
	{
		OuterStructuralRibsISM->SetStaticMesh(BasicCubeMesh);
		if (IsValid(OuterDarkSoffitMID))
		{
			OuterStructuralRibsISM->SetMaterial(0, OuterDarkSoffitMID);
		}

		const double ForeX = static_cast<double>(Cfg.ForegroundPipeWorldX);
		const double ColX = static_cast<double>(Cfg.MidgroundColumnWorldX);

		for (const double SideSign : { -1.0, 1.0 })
		{
			AddSizedWorldInstance(
				OuterStructuralRibsISM,
				BasicCubeMesh,
				FVector(0.5 * (ForeX + ColX), SideSign * 225.0, 322.0),
				FVector(FMath::Max(40.0, (ColX - ForeX) + 36.0), 160.0, 208.0));
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
