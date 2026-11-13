// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/ScaleAssemblyActor.h"
#include "Components/BoxComponent.h"
#include "Components/DiverterGateComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/MoltenThermalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/ScaleWeighingComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Data/MonolithContractDataAsset.h"
#include "Data/ShiftProgressionDataAsset.h"
#include "Data/TareTithePresentationDataAsset.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Subsystems/InspectionShiftSubsystem.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Open 84 x 76 cm abyssal Tare shaft clear interior: X in [-42.0, +42.0], Y in [+52.0, +128.0], Z down to -98.0 cm (World Z = -84.0 cm).
	constexpr double TareShaftClearHalfWidthXCm = 42.0;
	constexpr double TareShaftInnerMinYCm = 52.0;
	constexpr double TareShaftOuterMaxYCm = 128.0;
	constexpr double TareShaftCenterYCm = 90.0;
	constexpr double TareShaftBottomLocalZCm = -98.0;

	// Low visible forged-PBR collar (strictly ScaleRoot Z <= +16.0 cm across every visible instance so sightlines & Bottom CRT HUD stay 100% clear).
	constexpr double TareCollarWallThicknessCm = 6.5;
	constexpr double TareCollarSideTopLocalZCm = 14.0;
	constexpr double TareCollarFrontTopLocalZCm = 6.0;
	constexpr double TareCollarBaseLocalZCm = -2.0;
	constexpr double TareCollarLipBevelDeg = 28.0;

	// Sloped -Y feed apron bridging from beneath DiverterFlapperPivot (Y=+34) into the -Y=+52 throat wall (top Z <= +15.8 cm).
	constexpr double TareFeedApronCenterYCm = 43.8;
	constexpr double TareFeedApronCenterZCm = 7.4;
	constexpr double TareFeedApronSlopeRollDeg = -38.0;
	constexpr double TareFeedApronSpanXCm = 88.0;
	constexpr double TareFeedApronLengthCm = 24.0;
	constexpr double TareFeedApronThicknessCm = 2.8;

	// Tall invisible physics guide walls (up to ScaleRoot Z = +62.0 cm on +Y, -X, +X; -Y above +16 cm stays 100% open for flapper ramp).
	constexpr double TareGuideWallTopLocalZCm = 62.0;
	constexpr double TareGuideWallHalfThickCm = 3.0;
	constexpr double TareGuideChamferCenterZCm = 58.0;
	constexpr double TareGuideChamferTiltDeg = 35.0;

	// Deep basin visual & bloom-safe (< 1.65 BloomThreshold) unlit warm furnace ember baffle card.
	constexpr double TareFurnaceCardOffsetZAboveBasinCm = 6.0;
	constexpr float MaxBloomSafeFurnaceEmissiveChannel = 1.25f;

	// Tall multi-chunk stack retention ceiling above PanDeckTopLocalZCm.
	constexpr float TallStackWellCeilingAboveDeckCm = 175.0f;
}

AScaleAssemblyActor::AScaleAssemblyActor()
	: CachedShiftPhase(EShiftPhase::WaitingForStations)
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	ScaleRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ScaleRoot"));
	SetRootComponent(ScaleRoot);
	ScaleRoot->PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		FallbackCubeMesh = CubeMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlattenMatFinder(
		TEXT("/Engine/EngineMaterials/FlattenMaterial.FlattenMaterial"));
	if (FlattenMatFinder.Succeeded())
	{
		BaseGreyboxShapeMaterial = FlattenMatFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitTextMatFinder(
		TEXT("/Engine/EngineMaterials/UnlitText.UnlitText"));
	if (UnlitTextMatFinder.Succeeded())
	{
		UnlitReadoutTextMaterial = UnlitTextMatFinder.Object;
	}

	auto ConfigureVisualOnlyMesh = [this](UStaticMeshComponent* MeshComp)
	{
		MeshComp->PrimaryComponentTick.bCanEverTick = false;
		MeshComp->SetSimulatePhysics(false);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComp->SetGenerateOverlapEvents(false);
		if (IsValid(FallbackCubeMesh))
		{
			MeshComp->SetStaticMesh(FallbackCubeMesh);
		}
		if (IsValid(BaseGreyboxShapeMaterial))
		{
			MeshComp->SetMaterial(0, BaseGreyboxShapeMaterial);
		}
	};

	auto ConfigureAuthoritativeBlockingBox = [](
		UBoxComponent* BoxComp,
		USceneComponent* ParentComp,
		const FVector& RelativeLocationCm,
		const FVector& ExtentCm)
	{
		BoxComp->SetupAttachment(ParentComp);
		BoxComp->PrimaryComponentTick.bCanEverTick = false;
		BoxComp->SetSimulatePhysics(false);
		BoxComp->SetRelativeScale3D(FVector::OneVector);
		BoxComp->SetRelativeLocation(RelativeLocationCm);
		BoxComp->InitBoxExtent(ExtentCm);
		BoxComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		BoxComp->SetCollisionObjectType(ECC_WorldDynamic);
		BoxComp->SetCollisionResponseToAllChannels(ECR_Ignore);
		BoxComp->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
		BoxComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		BoxComp->SetGenerateOverlapEvents(false);
	};

	ScaleBaseVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScaleBaseVisual"));
	ScaleBaseVisual->SetupAttachment(ScaleRoot);
	ConfigureVisualOnlyMesh(ScaleBaseVisual);
	ScaleBaseVisual->SetRelativeLocation(FVector(0.0, 0.0, 6.0));
	ScaleBaseVisual->SetRelativeScale3D(FVector(0.82, 0.68, 0.12));

	LeftHopperGuideVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftHopperGuideVisual"));
	LeftHopperGuideVisual->SetupAttachment(ScaleRoot);
	ConfigureVisualOnlyMesh(LeftHopperGuideVisual);
	LeftHopperGuideVisual->SetRelativeLocation(LeftHopperGuideOffsetCm);
	LeftHopperGuideVisual->SetRelativeScale3D((2.0 * LeftHopperGuideBoxExtentCm) / 100.0);

	LeftHopperGuideCollider = CreateDefaultSubobject<UBoxComponent>(TEXT("LeftHopperGuideCollider"));
	ConfigureAuthoritativeBlockingBox(LeftHopperGuideCollider, ScaleRoot, LeftHopperGuideOffsetCm, LeftHopperGuideBoxExtentCm);

	constexpr double PanScaleX = 0.76;
	constexpr double PanScaleY = 0.62;
	constexpr double PanScaleZ = 0.06;

	// Unscaled (1, 1, 1) motion root translated by UScaleWeighingComponent so child UBoxComponents operate in true world centimeters without non-uniform Z squashing.
	PanMotionRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PanMotionRoot"));
	PanMotionRoot->SetupAttachment(ScaleRoot);
	PanMotionRoot->PrimaryComponentTick.bCanEverTick = false;
	PanMotionRoot->SetRelativeLocation(FVector(0.0, 0.0, 22.0));
	PanMotionRoot->SetRelativeScale3D(FVector::OneVector);

	PanPlatformVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PanPlatformVisual"));
	PanPlatformVisual->SetupAttachment(PanMotionRoot);
	ConfigureVisualOnlyMesh(PanPlatformVisual);
	PanPlatformVisual->SetRelativeLocation(FVector::ZeroVector);
	PanPlatformVisual->SetRelativeScale3D(FVector(PanScaleX, PanScaleY, PanScaleZ));

	// Authoritative unscaled (1, 1, 1) physical deck collider decoupled from PanPlatformVisual mesh swaps.
	PanDeckCollider = CreateDefaultSubobject<UBoxComponent>(TEXT("PanDeckCollider"));
	ConfigureAuthoritativeBlockingBox(
		PanDeckCollider,
		PanMotionRoot,
		FVector(0.0, 0.0, static_cast<double>(PanDeckTopLocalZCm) - PanDeckBoxExtentCm.Z),
		PanDeckBoxExtentCm);

	auto ConfigurePanRimBox = [&](
		UBoxComponent* RimBox,
		double DxCm,
		double DyCm,
		double DzCm,
		const FVector& ExtentCm)
	{
		ConfigureAuthoritativeBlockingBox(RimBox, PanMotionRoot, FVector(DxCm, DyCm, DzCm), ExtentCm);
	};

	// 4 unscaled physical rim walls (22 cm total height [-3.0, +19.0], 16 cm raised curb above the +3.0 cm pan deck) moving with PanMotionRoot.
	const double RimBottomLocalZ = static_cast<double>(PanDeckTopLocalZCm) - 6.0;
	const double RimTopLocalZ = static_cast<double>(PanDeckTopLocalZCm) + static_cast<double>(PanRimCurbHeightAboveDeckCm);
	const double RimHalfHeightZ = 0.5 * (RimTopLocalZ - RimBottomLocalZ);
	const double RimCenterLocalZ = 0.5 * (RimTopLocalZ + RimBottomLocalZ);

	PanRimFrontBox = CreateDefaultSubobject<UBoxComponent>(TEXT("PanRimFrontBox"));
	ConfigurePanRimBox(PanRimFrontBox, -36.5, 0.0, RimCenterLocalZ, FVector(1.5, 31.0, RimHalfHeightZ));

	PanRimBackBox = CreateDefaultSubobject<UBoxComponent>(TEXT("PanRimBackBox"));
	ConfigurePanRimBox(PanRimBackBox, +36.5, 0.0, RimCenterLocalZ, FVector(1.5, 31.0, RimHalfHeightZ));

	PanRimLeftBox = CreateDefaultSubobject<UBoxComponent>(TEXT("PanRimLeftBox"));
	ConfigurePanRimBox(PanRimLeftBox, 0.0, -29.5, RimCenterLocalZ, FVector(35.0, 1.5, RimHalfHeightZ));

	PanRimRightBox = CreateDefaultSubobject<UBoxComponent>(TEXT("PanRimRightBox"));
	ConfigurePanRimBox(PanRimRightBox, 0.0, +29.5, RimCenterLocalZ, FVector(35.0, 1.5, RimHalfHeightZ));

	PanTriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("PanTriggerBox"));
	PanTriggerBox->SetupAttachment(PanMotionRoot);
	PanTriggerBox->PrimaryComponentTick.bCanEverTick = false;
	PanTriggerBox->SetRelativeScale3D(FVector::OneVector);
	// Bottom at -3.0 cm (6.0 cm below +3.0 cm top deck); top at +95.0 cm (+92.0 cm above deck) so 12+ stacked slices stay 100% inside.
	PanTriggerBox->SetRelativeLocation(FVector(0.0, 0.0, static_cast<double>(PanDeckTopLocalZCm + PanTriggerCenterAboveDeckZCm)));
	PanTriggerBox->InitBoxExtent(PanTriggerBoxExtentCm);
	PanTriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PanTriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
	PanTriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	PanTriggerBox->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	PanTriggerBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	PanTriggerBox->SetGenerateOverlapEvents(true);

	// Bottom-right intake hinge: 0 deg stands vertically at Y=+34 cm leaving PanPlatform completely unobstructed;
	// +45 deg swings across the throat to Y=-30.35 cm as an angled ramp channeling falling chunks directly into TareChuteTriggerBox.
	DiverterFlapperPivot = CreateDefaultSubobject<USceneComponent>(TEXT("DiverterFlapperPivot"));
	DiverterFlapperPivot->SetupAttachment(ScaleRoot);
	DiverterFlapperPivot->PrimaryComponentTick.bCanEverTick = false;
	DiverterFlapperPivot->SetRelativeLocation(FVector(0.0, 34.0, 50.0));

	DiverterFlapperVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DiverterFlapperVisual"));
	DiverterFlapperVisual->SetupAttachment(DiverterFlapperPivot);
	ConfigureVisualOnlyMesh(DiverterFlapperVisual);
	DiverterFlapperVisual->SetRelativeLocation(DiverterPlateColliderOffsetCm);
	DiverterFlapperVisual->SetRelativeScale3D(FVector(0.84, 0.03, 0.91));

	// Authoritative unscaled (1, 1, 1) ramp collider decoupled from DiverterFlapperVisual mesh swaps.
	DiverterPlateCollider = CreateDefaultSubobject<UBoxComponent>(TEXT("DiverterPlateCollider"));
	ConfigureAuthoritativeBlockingBox(
		DiverterPlateCollider,
		DiverterFlapperPivot,
		DiverterPlateColliderOffsetCm,
		DiverterPlateBoxExtentCm);

	TareChuteVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TareChuteVisual"));
	TareChuteVisual->SetupAttachment(ScaleRoot);
	ConfigureVisualOnlyMesh(TareChuteVisual);
	TareChuteVisual->SetCastShadow(false);
	TareChuteVisual->SetRelativeLocation(TareBasinVisualOffsetCm);
	TareChuteVisual->SetRelativeScale3D(FVector(0.74, 0.72, 0.08));

	auto ConfigureHopperVisualISM = [this](UInstancedStaticMeshComponent* TargetISM)
	{
		TargetISM->SetupAttachment(ScaleRoot);
		TargetISM->PrimaryComponentTick.bCanEverTick = false;
		TargetISM->SetSimulatePhysics(false);
		TargetISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TargetISM->SetGenerateOverlapEvents(false);
		TargetISM->SetCastShadow(false);
		TargetISM->bReceivesDecals = false;
		if (IsValid(FallbackCubeMesh))
		{
			TargetISM->SetStaticMesh(FallbackCubeMesh);
		}
		if (IsValid(BaseGreyboxShapeMaterial))
		{
			TargetISM->SetMaterial(0, BaseGreyboxShapeMaterial);
		}
	};

	TareHopperCollarISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TareHopperCollarISM"));
	ConfigureHopperVisualISM(TareHopperCollarISM);

	TareHopperThroatISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TareHopperThroatISM"));
	ConfigureHopperVisualISM(TareHopperThroatISM);

	TareHopperFurnaceGlowCard = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TareHopperFurnaceGlowCard"));
	TareHopperFurnaceGlowCard->SetupAttachment(ScaleRoot);
	ConfigureVisualOnlyMesh(TareHopperFurnaceGlowCard);
	TareHopperFurnaceGlowCard->SetCastShadow(false);
	TareHopperFurnaceGlowCard->SetAffectDistanceFieldLighting(false);
	TareHopperFurnaceGlowCard->SetAffectDynamicIndirectLighting(false);
	TareHopperFurnaceGlowCard->SetVisibleInRayTracing(false);
	TareHopperFurnaceGlowCard->bReceivesDecals = false;
	TareHopperFurnaceGlowCard->SetRelativeLocation(
		TareBasinVisualOffsetCm + FVector(0.0, 0.0, TareFurnaceCardOffsetZAboveBasinCm));
	TareHopperFurnaceGlowCard->SetRelativeScale3D(FVector(0.78, 0.70, 0.015));

	auto ConfigureInvisibleGuideBox = [&](
		UBoxComponent* GuideBox,
		const FVector& RelLocationCm,
		const FQuat& RelQuat,
		const FVector& ExtentCm)
	{
		ConfigureAuthoritativeBlockingBox(GuideBox, ScaleRoot, RelLocationCm, ExtentCm);
		GuideBox->SetRelativeRotation(RelQuat);
		GuideBox->SetHiddenInGame(true, false);
		GuideBox->SetVisibility(false, false);
		GuideBox->SetCastShadow(false);
	};

	const double GuideWallHeightZ = TareGuideWallTopLocalZCm - TareShaftBottomLocalZCm;
	const double GuideWallCenterZ = 0.5 * (TareGuideWallTopLocalZCm + TareShaftBottomLocalZCm);
	const double GuideWallHalfHeightZ = 0.5 * GuideWallHeightZ;

	TareGuideWallOuterRightBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TareGuideWallOuterRightBox"));
	ConfigureInvisibleGuideBox(
		TareGuideWallOuterRightBox,
		FVector(0.0, TareShaftOuterMaxYCm + TareGuideWallHalfThickCm, GuideWallCenterZ),
		FQuat::Identity,
		FVector(TareShaftClearHalfWidthXCm + (2.0 * TareGuideWallHalfThickCm), TareGuideWallHalfThickCm, GuideWallHalfHeightZ));

	TareGuideWallFrontBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TareGuideWallFrontBox"));
	ConfigureInvisibleGuideBox(
		TareGuideWallFrontBox,
		FVector(-(TareShaftClearHalfWidthXCm + TareGuideWallHalfThickCm), 85.0, GuideWallCenterZ),
		FQuat::Identity,
		FVector(TareGuideWallHalfThickCm, 49.0, GuideWallHalfHeightZ));

	TareGuideWallBackBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TareGuideWallBackBox"));
	ConfigureInvisibleGuideBox(
		TareGuideWallBackBox,
		FVector(+(TareShaftClearHalfWidthXCm + TareGuideWallHalfThickCm), 85.0, GuideWallCenterZ),
		FQuat::Identity,
		FVector(TareGuideWallHalfThickCm, 49.0, GuideWallHalfHeightZ));

	// Strictly below Z = +2.0 cm so -Y above the sloped feed apron remains 100% open for DiverterFlapper ramp travel.
	TareGuideWallInnerLeftLowerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TareGuideWallInnerLeftLowerBox"));
	ConfigureInvisibleGuideBox(
		TareGuideWallInnerLeftLowerBox,
		FVector(0.0, TareShaftInnerMinYCm - TareGuideWallHalfThickCm, -48.0),
		FQuat::Identity,
		FVector(TareShaftClearHalfWidthXCm + (2.0 * TareGuideWallHalfThickCm), TareGuideWallHalfThickCm, 50.0));

	TareGuideFeedApronSlopeBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TareGuideFeedApronSlopeBox"));
	ConfigureInvisibleGuideBox(
		TareGuideFeedApronSlopeBox,
		FVector(0.0, TareFeedApronCenterYCm, TareFeedApronCenterZCm),
		FQuat(FVector::ForwardVector, FMath::DegreesToRadians(TareFeedApronSlopeRollDeg)),
		FVector(0.5 * TareFeedApronSpanXCm, 0.5 * TareFeedApronLengthCm, 0.5 * TareFeedApronThicknessCm));

	TareGuideChamferOuterRightBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TareGuideChamferOuterRightBox"));
	ConfigureInvisibleGuideBox(
		TareGuideChamferOuterRightBox,
		FVector(0.0, TareShaftOuterMaxYCm - 2.5, TareGuideChamferCenterZCm),
		FQuat(FVector::ForwardVector, FMath::DegreesToRadians(+TareGuideChamferTiltDeg)),
		FVector(TareShaftClearHalfWidthXCm + 4.0, 1.5, 8.0));

	TareGuideChamferFrontBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TareGuideChamferFrontBox"));
	ConfigureInvisibleGuideBox(
		TareGuideChamferFrontBox,
		FVector(-(TareShaftClearHalfWidthXCm - 2.0), TareShaftCenterYCm, TareGuideChamferCenterZCm),
		FQuat(FVector::RightVector, FMath::DegreesToRadians(+TareGuideChamferTiltDeg)),
		FVector(1.5, 40.0, 8.0));

	TareGuideChamferBackBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TareGuideChamferBackBox"));
	ConfigureInvisibleGuideBox(
		TareGuideChamferBackBox,
		FVector(+(TareShaftClearHalfWidthXCm - 2.0), TareShaftCenterYCm, TareGuideChamferCenterZCm),
		FQuat(FVector::RightVector, FMath::DegreesToRadians(-TareGuideChamferTiltDeg)),
		FVector(1.5, 40.0, 8.0));

	TareChuteTriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TareChuteTriggerBox"));
	TareChuteTriggerBox->SetupAttachment(ScaleRoot);
	TareChuteTriggerBox->PrimaryComponentTick.bCanEverTick = false;
	TareChuteTriggerBox->SetRelativeScale3D(FVector::OneVector);
	TareChuteTriggerBox->SetRelativeLocation(TareMouthTriggerOffsetCm);
	TareChuteTriggerBox->InitBoxExtent(TareMouthTriggerExtentCm);

	TareAbyssDespawnTriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TareAbyssDespawnTriggerBox"));
	TareAbyssDespawnTriggerBox->SetupAttachment(ScaleRoot);
	TareAbyssDespawnTriggerBox->PrimaryComponentTick.bCanEverTick = false;
	TareAbyssDespawnTriggerBox->SetRelativeScale3D(FVector::OneVector);
	TareAbyssDespawnTriggerBox->SetRelativeLocation(TareAbyssDespawnTriggerOffsetCm);
	TareAbyssDespawnTriggerBox->InitBoxExtent(TareAbyssDespawnTriggerExtentCm);

	RebuildTareHopperVisualInstances();

	ScaleWeighingComponent = CreateDefaultSubobject<UScaleWeighingComponent>(TEXT("ScaleWeighingComponent"));
	DiverterGateComponent = CreateDefaultSubobject<UDiverterGateComponent>(TEXT("DiverterGateComponent"));

	const FMoltenBounceLightConfig DefaultMoltenCfg;
	ScaleWellEmberLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ScaleWellEmberLight"));
	ScaleWellEmberLight->SetupAttachment(PanMotionRoot);
	ScaleWellEmberLight->PrimaryComponentTick.bCanEverTick = false;
	ScaleWellEmberLight->SetRelativeLocation(DefaultMoltenCfg.ScaleWellLocalOffset);
	ScaleWellEmberLight->SetIntensity(0.0f);
	ScaleWellEmberLight->SetAttenuationRadius(DefaultMoltenCfg.ScaleWellAttenuationRadiusCm);
	ScaleWellEmberLight->SetLightColor(DefaultMoltenCfg.BounceColor);
	ScaleWellEmberLight->SetVolumetricScatteringIntensity(0.0f);
	ScaleWellEmberLight->SetCastShadows(false);
	ScaleWellEmberLight->SetVisibility(false, false);

	ScaleWellThermalLightComponent = CreateDefaultSubobject<UMoltenThermalLightComponent>(TEXT("ScaleWellThermalLightComponent"));

	// Front-apron console bezel mounted strictly below PanRimFrontBox top (+37.0 cm) and tilted 10 deg upward toward camera.
	ReadoutConsolePivot = CreateDefaultSubobject<USceneComponent>(TEXT("ReadoutConsolePivot"));
	ReadoutConsolePivot->SetupAttachment(ScaleRoot);
	ReadoutConsolePivot->PrimaryComponentTick.bCanEverTick = false;
	ReadoutConsolePivot->SetRelativeLocation(FVector(-43.5, 0.0, 18.2));
	ReadoutConsolePivot->SetRelativeRotation(FRotator(-10.0f, 0.0f, 0.0f));
	ReadoutConsolePivot->SetRelativeScale3D(FVector::OneVector);
	ReadoutConsolePivot->SetVisibility(false, false);
	ReadoutConsolePivot->SetHiddenInGame(true, false);

	// Unscaled (1, 1, 1) grouping root translated/rotated by ScaleConsoleLocalOffset so custom mesh housings never squash text scale.
	ScaleConsoleRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ScaleConsoleRoot"));
	ScaleConsoleRoot->SetupAttachment(ReadoutConsolePivot);
	ScaleConsoleRoot->PrimaryComponentTick.bCanEverTick = false;
	ScaleConsoleRoot->SetRelativeTransform(FTransform::Identity);
	ScaleConsoleRoot->SetVisibility(false, false);
	ScaleConsoleRoot->SetHiddenInGame(true, false);

	ReadoutBackingPlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReadoutBackingPlate"));
	ReadoutBackingPlate->SetupAttachment(ScaleConsoleRoot);
	ReadoutBackingPlate->PrimaryComponentTick.bCanEverTick = false;
	ReadoutBackingPlate->SetSimulatePhysics(false);
	ReadoutBackingPlate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ReadoutBackingPlate->SetGenerateOverlapEvents(false);
	ReadoutBackingPlate->SetCastShadow(false);
	ReadoutBackingPlate->bReceivesDecals = false;
	if (IsValid(FallbackCubeMesh))
	{
		ReadoutBackingPlate->SetStaticMesh(FallbackCubeMesh);
	}
	if (IsValid(BaseGreyboxShapeMaterial))
	{
		ReadoutBackingPlate->SetMaterial(0, BaseGreyboxShapeMaterial);
	}
	ReadoutBackingPlate->SetRelativeLocation(FVector::ZeroVector);
	ReadoutBackingPlate->SetRelativeScale3D(FVector(0.016, static_cast<double>(ReadoutBackingPlateWidthCm) / 100.0, 0.37));
	ReadoutBackingPlate->SetVisibility(false, false);
	ReadoutBackingPlate->SetHiddenInGame(true, false);

	// Backing plate front face is at local X = -0.8 cm; placing all text at local X = -2.0 cm positions glyphs exactly 1.2 cm in front.
	auto ConfigureReadoutText = [this](
		UTextRenderComponent* TextComp,
		const FVector& RelLoc,
		float WorldSize,
		const FColor& Color,
		const TCHAR* DefaultStr)
	{
		TextComp->PrimaryComponentTick.bCanEverTick = false;
		TextComp->SetRelativeLocation(RelLoc);
		TextComp->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
		TextComp->SetHorizontalAlignment(EHTA_Center);
		TextComp->SetVerticalAlignment(EVRTA_TextCenter);
		TextComp->SetWorldSize(WorldSize);
		TextComp->SetCastShadow(false);
		if (IsValid(UnlitReadoutTextMaterial))
		{
			TextComp->SetTextMaterial(UnlitReadoutTextMaterial);
		}
		TextComp->SetTextRenderColor(Color);
		TextComp->SetText(FText::FromString(DefaultStr));
		TextComp->SetVisibility(false, false);
		TextComp->SetHiddenInGame(true, false);
	};

	HeaderStencilText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("HeaderStencilText"));
	HeaderStencilText->SetupAttachment(ScaleConsoleRoot);
	ConfigureReadoutText(
		HeaderStencilText,
		FVector(-2.0, 0.0, 14.8),
		4.8f,
		ReadoutHeaderCyanColor,
		TEXT("TITHE WEIGHING CONSOLE // DIRECTOR'S CUT: +-0.31 KG (+2 SCRIP)"));

	ContractCodeText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ContractCodeText"));
	ContractCodeText->SetupAttachment(ScaleConsoleRoot);
	ConfigureReadoutText(
		ContractCodeText,
		FVector(-2.0, 0.0, 9.6),
		4.6f,
		ReadoutHeaderCyanColor,
		TEXT("TARGET QUOTA: 12.00 KG  [VALID WINDOW: 10.75 - 13.25 KG]"));

	StatusBannerText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusBannerText"));
	StatusBannerText->SetupAttachment(ScaleConsoleRoot);
	ConfigureReadoutText(
		StatusBannerText,
		FVector(-2.0, 0.0, 3.6),
		StatusBannerDefaultWorldSize,
		ReadoutNormalSkyBlueColor,
		TEXT("HOLD [LMB] TO SLICE MONOLITH INTO TITHE"));

	MassReadoutText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("MassReadoutText"));
	MassReadoutText->SetupAttachment(ScaleConsoleRoot);
	ConfigureReadoutText(MassReadoutText, FVector(-2.0, 0.0, -3.8), 9.4f, ReadoutGoldColor, TEXT("00.00 / 12.00 KG"));

	TareReadoutText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TareReadoutText"));
	TareReadoutText->SetupAttachment(ScaleConsoleRoot);
	ConfigureReadoutText(TareReadoutText, FVector(-2.0, -52.0, -10.6), 5.4f, ReadoutGoldColor, TEXT("TARE: 0.0 / 14.0 KG"));

	RouteReadoutText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("RouteReadoutText"));
	RouteReadoutText->SetupAttachment(ScaleConsoleRoot);
	ConfigureReadoutText(RouteReadoutText, FVector(-2.0, 52.0, -10.6), 5.4f, ReadoutHeaderCyanColor, TEXT("ROUTE: TITHE [SPACE]"));

	ControlStencilText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ControlStencilText"));
	ControlStencilText->SetupAttachment(ScaleConsoleRoot);
	ConfigureReadoutText(
		ControlStencilText,
		FVector(-2.0, 0.0, -15.6),
		ControlStencilDefaultWorldSize,
		ReadoutControlStencilColor,
		TEXT("LMB: SLICE | SCROLL: ROLL | RMB: BEVEL | [SPACE]: ROUTE | [E]: STAMP | HOLD [R]: RESET"));
}

void AScaleAssemblyActor::BeginPlay()
{
	Super::BeginPlay();

	InitializeGreyboxVisualMaterials();
	EnforcePanTriggerGeometryAndCollision();
	UpdateLegacyConsoleVisibility();

	if (IsValid(ScaleWellThermalLightComponent))
	{
		const FMoltenBounceLightConfig DefaultMoltenCfg;
		ScaleWellThermalLightComponent->InitializeTargetLight(ScaleWellEmberLight);
		ScaleWellThermalLightComponent->ConfigureLightParameters(
			DefaultMoltenCfg.BounceColor,
			DefaultMoltenCfg.ScaleWellAttenuationRadiusCm,
			DefaultMoltenCfg.FlashDecayDurationSeconds,
			DefaultMoltenCfg.SteadyInterpSpeed);
	}

	if (IsValid(ScaleWeighingComponent))
	{
		ScaleWeighingComponent->SetPanPlatformComponent(PanMotionRoot);
		ScaleWeighingComponent->SetPanWellBoundsGeometry(
			FVector2D(PanTriggerBoxExtentCm.X, PanTriggerBoxExtentCm.Y),
			PanDeckTopLocalZCm - 6.0f,
			PanDeckTopLocalZCm + TallStackWellCeilingAboveDeckCm);
		ScaleWeighingComponent->OnShouldIgnorePayloadInPan.BindUObject(this, &AScaleAssemblyActor::ShouldIgnorePayloadInPanForTareRamp);
		ScaleWeighingComponent->BindPanContainmentTrigger(PanTriggerBox);
		ScaleWeighingComponent->OnTelemetryStream.AddUObject(this, &AScaleAssemblyActor::HandleChildTelemetryStream);
		ScaleWeighingComponent->OnTelemetryImpulse.AddUObject(this, &AScaleAssemblyActor::HandleChildTelemetryImpulse);
		ScaleWeighingComponent->OnChunkLandedOnScale.AddUObject(this, &AScaleAssemblyActor::HandleChunkLandedOnScale);
		ScaleWeighingComponent->OnChunkBouncedOffScale.AddUObject(this, &AScaleAssemblyActor::HandleChunkBouncedOffScale);
		ScaleWeighingComponent->OnScaleContaminationDetected.AddUObject(this, &AScaleAssemblyActor::HandleScaleContaminationDetected);
		ScaleWeighingComponent->OnScaleSettledChanged.AddUObject(this, &AScaleAssemblyActor::HandleScaleSettledChanged);
	}

	if (IsValid(DiverterGateComponent))
	{
		DiverterGateComponent->SetFlapperPivotAndPrimitive(DiverterFlapperPivot, DiverterPlateCollider);
		DiverterGateComponent->BindTareChuteTrigger(TareChuteTriggerBox);
		DiverterGateComponent->BindTareAbyssDespawnTrigger(TareAbyssDespawnTriggerBox);
		DiverterGateComponent->OnRouteChanged.AddUObject(this, &AScaleAssemblyActor::HandleDiverterRouteChanged);
		DiverterGateComponent->OnRouteSlewCompleted.AddUObject(this, &AScaleAssemblyActor::HandleDiverterRouteSlewCompleted);
		DiverterGateComponent->OnChunkRoutedToTare.AddUObject(this, &AScaleAssemblyActor::HandleChunkRoutedToTare);
		DiverterGateComponent->OnTelemetryImpulse.AddUObject(this, &AScaleAssemblyActor::HandleChildTelemetryImpulse);
	}

	ApplyTareHopperPresentationMaterials();

	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->RegisterTelemetryPublisher(this, this);
			ShiftSubsystem->OnNativeContractSnapshotUpdated.AddUObject(this, &AScaleAssemblyActor::HandleContractSnapshotUpdated);
			ShiftSubsystem->OnShiftVerdict.AddUniqueDynamic(this, &AScaleAssemblyActor::HandleShiftVerdict);
			ShiftSubsystem->OnShiftPhaseChanged.AddUObject(this, &AScaleAssemblyActor::HandleShiftPhaseChanged);
			ShiftSubsystem->OnActiveContractLoaded.AddUObject(this, &AScaleAssemblyActor::HandleActiveContractLoaded);
			ShiftSubsystem->OnPresentationAssetChanged.AddUObject(this, &AScaleAssemblyActor::ApplyPresentationData);

			ApplyPresentationData(ShiftSubsystem->GetActivePresentationAsset());

			CachedShiftPhase = ShiftSubsystem->GetCurrentPhase();

			FShiftQuotaReceiverBinding Binding;
			Binding.QuotaReceiver.SetObject(this);
			Binding.QuotaReceiver.SetInterface(this);
			Binding.OnConfigureContract.BindUObject(this, &AScaleAssemblyActor::ConfigureForContract);
			Binding.OnToggleDiverter.BindUObject(this, &AScaleAssemblyActor::RequestToggleDiverterRoute);
			Binding.OnReconcileOverlaps.BindUObject(this, &AScaleAssemblyActor::ReconcileScaleOverlaps);
			Binding.OnQueryWorldLocation.BindUObject(this, &AScaleAssemblyActor::QueryScaleWorldLocation);

			ShiftSubsystem->RegisterQuotaReceiver(Binding);

			CachedShiftPhase = ShiftSubsystem->GetCurrentPhase();
			CachedContractIndex = ShiftSubsystem->GetCurrentContractIndex();
			CachedTotalContracts = FMath::Max(1, ShiftSubsystem->GetTotalContractCount());
			CachedSnapshot = ShiftSubsystem->GetCurrentSnapshot();
			RefreshContractHeaderReadout();
		}
	}

	RefreshRouteReadout();
	RefreshStatusBannerReadout();
}

void AScaleAssemblyActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->OnNativeContractSnapshotUpdated.RemoveAll(this);
			ShiftSubsystem->OnShiftVerdict.RemoveDynamic(this, &AScaleAssemblyActor::HandleShiftVerdict);
			ShiftSubsystem->OnShiftPhaseChanged.RemoveAll(this);
			ShiftSubsystem->OnActiveContractLoaded.RemoveAll(this);
			ShiftSubsystem->OnPresentationAssetChanged.RemoveAll(this);
			ShiftSubsystem->UnregisterQuotaReceiver(this);
			ShiftSubsystem->UnregisterTelemetryPublisher(this);
		}
	}

	if (IsValid(ScaleWeighingComponent))
	{
		ScaleWeighingComponent->OnShouldIgnorePayloadInPan.Unbind();
		ScaleWeighingComponent->OnTelemetryStream.RemoveAll(this);
		ScaleWeighingComponent->OnTelemetryImpulse.RemoveAll(this);
		ScaleWeighingComponent->OnChunkLandedOnScale.RemoveAll(this);
		ScaleWeighingComponent->OnChunkBouncedOffScale.RemoveAll(this);
		ScaleWeighingComponent->OnScaleContaminationDetected.RemoveAll(this);
		ScaleWeighingComponent->OnScaleSettledChanged.RemoveAll(this);
	}

	if (IsValid(DiverterGateComponent))
	{
		DiverterGateComponent->OnRouteChanged.RemoveAll(this);
		DiverterGateComponent->OnRouteSlewCompleted.RemoveAll(this);
		DiverterGateComponent->OnChunkRoutedToTare.RemoveAll(this);
		DiverterGateComponent->OnTelemetryImpulse.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

float AScaleAssemblyActor::GetSettledMassKg_Implementation() const
{
	return IsValid(ScaleWeighingComponent) ? ScaleWeighingComponent->GetSettledMassKg() : 0.0f;
}

bool AScaleAssemblyActor::IsReceiverSettled_Implementation() const
{
	return IsValid(ScaleWeighingComponent) ? ScaleWeighingComponent->IsScaleSettled() : true;
}

bool AScaleAssemblyActor::HasContaminationFault_Implementation() const
{
	return IsValid(ScaleWeighingComponent) ? ScaleWeighingComponent->HasContaminationFault() : false;
}

EDiverterRoute AScaleAssemblyActor::GetActiveDiverterRoute_Implementation() const
{
	return IsValid(DiverterGateComponent) ? DiverterGateComponent->GetCurrentRoute() : EDiverterRoute::TitheScale;
}

void AScaleAssemblyActor::ResetReceiverState_Implementation()
{
	if (IsValid(ScaleWeighingComponent))
	{
		ScaleWeighingComponent->ResetScaleState();
	}
	if (IsValid(DiverterGateComponent))
	{
		DiverterGateComponent->SetRoute(EDiverterRoute::TitheScale, true);
	}
	if (IsValid(ScaleWellThermalLightComponent))
	{
		ScaleWellThermalLightComponent->ResetThermalImmediate();
	}
	RefreshRouteReadout();
	RefreshStatusBannerReadout();
}

bool AScaleAssemblyActor::TryRecoverPayloadOnReceiver_Implementation(AActor* PayloadActor)
{
	if (!IsValid(ScaleWeighingComponent))
	{
		return false;
	}
	return ScaleWeighingComponent->TryRecoverPayloadOnPan(PayloadActor);
}

bool AScaleAssemblyActor::IsScalePanSupportComponent_Implementation(const UPrimitiveComponent* CandidateComponent) const
{
	if (!IsValid(CandidateComponent))
	{
		return false;
	}

	return (CandidateComponent == PanDeckCollider) ||
	       (CandidateComponent == PanRimFrontBox) ||
	       (CandidateComponent == PanRimBackBox) ||
	       (CandidateComponent == PanRimLeftBox) ||
	       (CandidateComponent == PanRimRightBox);
}

void AScaleAssemblyActor::NotifyPayloadScaleSupportImpact_Implementation(
	AActor* PayloadActor,
	UPrimitiveComponent* PayloadPrimitive,
	const FVector& WorldImpactPoint,
	const FVector& NormalImpulse)
{
	if (IsValid(ScaleWeighingComponent))
	{
		ScaleWeighingComponent->ConfirmPayloadScaleImpact(
			PayloadActor,
			PayloadPrimitive,
			WorldImpactPoint,
			NormalImpulse);
	}
}

void AScaleAssemblyActor::ConfigureForContract(const UMonolithContractDataAsset* Contract, EDiverterRoute InitialRoute)
{
	if (IsValid(ScaleWellThermalLightComponent))
	{
		ScaleWellThermalLightComponent->ResetThermalImmediate();
	}

	if (IsValid(ScaleWeighingComponent))
	{
		ScaleWeighingComponent->ConfigureForContract(Contract);
	}

	if (IsValid(DiverterGateComponent))
	{
		const bool bAllowToggle = IsValid(Contract) ? Contract->bAllowDiverterToggle : true;
		DiverterGateComponent->ConfigureForContract(bAllowToggle, InitialRoute);
	}

	if (IsValid(Contract))
	{
		CachedSnapshot.TargetTitheMassKg = Contract->TargetTitheMassKg;
		CachedSnapshot.QuotaToleranceKg = Contract->QuotaToleranceKg;
		CachedSnapshot.MaxAllowedTareWasteKg = Contract->MaxAllowedTareWasteKg;
	}

	if (UWorld* World = GetWorld())
	{
		if (const UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			CachedSnapshot = ShiftSubsystem->GetCurrentSnapshot();
			CachedContractIndex = ShiftSubsystem->GetCurrentContractIndex();
			CachedTotalContracts = FMath::Max(1, ShiftSubsystem->GetTotalContractCount());
			if (IsValid(Contract))
			{
				const float EffTolKg = ShiftSubsystem->GetEffectiveQuotaToleranceKg(Contract);
				CachedSnapshot.TargetTitheMassKg = Contract->TargetTitheMassKg;
				CachedSnapshot.QuotaToleranceKg = EffTolKg;
				CachedSnapshot.DirectorsCutToleranceKg = ShiftSubsystem->GetDirectorsCutToleranceKg(EffTolKg);
				CachedSnapshot.MaxAllowedTareWasteKg = ShiftSubsystem->GetEffectiveMaxTareWasteKg(Contract);
			}
		}
	}

	bHasLatchedVerdictBanner = false;
	LatchedFaultReason = EContractFaultReason::None;

	RefreshContractHeaderReadout();
	RefreshRouteReadout();
	RefreshStatusBannerReadout();
}

bool AScaleAssemblyActor::RequestToggleDiverterRoute()
{
	if (!IsValid(DiverterGateComponent))
	{
		return false;
	}

	const bool bToggled = DiverterGateComponent->ToggleRoute();
	RefreshRouteReadout();
	return bToggled;
}

void AScaleAssemblyActor::ReconcileScaleOverlaps()
{
	if (IsValid(ScaleWeighingComponent))
	{
		ScaleWeighingComponent->ReconcileOverlappingPayloads(true);
	}
}

void AScaleAssemblyActor::ApplyPresentationData(const UTareTithePresentationDataAsset* InPresentationData)
{
	PresentationData = InPresentationData;
	UpdateLegacyConsoleVisibility();

	if (IsValid(ScaleConsoleRoot))
	{
		ScaleConsoleRoot->SetRelativeTransform(
			IsValid(InPresentationData) ? InPresentationData->ScaleConsoleLocalOffset : FTransform::Identity);
	}

	if (!IsValid(InPresentationData))
	{
		const FMoltenBounceLightConfig DefaultMoltenCfg;
		if (IsValid(ScaleWellEmberLight))
		{
			ScaleWellEmberLight->SetRelativeLocation(DefaultMoltenCfg.ScaleWellLocalOffset);
		}
		if (IsValid(ScaleWellThermalLightComponent))
		{
			ScaleWellThermalLightComponent->ConfigureLightParameters(
				DefaultMoltenCfg.BounceColor,
				DefaultMoltenCfg.ScaleWellAttenuationRadiusCm,
				DefaultMoltenCfg.FlashDecayDurationSeconds,
				DefaultMoltenCfg.SteadyInterpSpeed);
		}
		if (IsValid(TareChuteVisual))
		{
			TareChuteVisual->SetRelativeLocation(TareBasinVisualOffsetCm);
		}
		ApplyTareHopperPresentationMaterials();
		return;
	}

	const FMoltenBounceLightConfig& MoltenCfg = InPresentationData->MoltenBounceConfig;
	if (IsValid(ScaleWellEmberLight))
	{
		ScaleWellEmberLight->SetRelativeLocation(MoltenCfg.ScaleWellLocalOffset);
	}
	if (IsValid(ScaleWellThermalLightComponent))
	{
		ScaleWellThermalLightComponent->ConfigureLightParameters(
			MoltenCfg.BounceColor,
			MoltenCfg.ScaleWellAttenuationRadiusCm,
			MoltenCfg.FlashDecayDurationSeconds,
			MoltenCfg.SteadyInterpSpeed);
	}

	InPresentationData->ScaleBaseMeshSlot.ApplyToVisualComponent(ScaleBaseVisual);
	InPresentationData->ScalePanMeshSlot.ApplyToVisualComponent(PanPlatformVisual);
	const bool bAppliedFlapperSlot = InPresentationData->DiverterFlapperMeshSlot.ApplyToVisualComponent(DiverterFlapperVisual);
	const bool bAppliedTareSlot = InPresentationData->TareChuteMeshSlot.ApplyToVisualComponent(TareChuteVisual);
	if (IsValid(TareChuteVisual))
	{
		TareChuteVisual->SetRelativeLocation(TareBasinVisualOffsetCm);
		if (bAppliedTareSlot)
		{
			const FVector SlotScale = TareChuteVisual->GetRelativeScale3D();
			const double SafeXYMult = static_cast<double>(FMath::Clamp(TareBasinVisualXYScaleMultiplier, 0.50f, 1.0f));
			TareChuteVisual->SetRelativeScale3D(FVector(SlotScale.X * SafeXYMult, SlotScale.Y * SafeXYMult, SlotScale.Z));
		}
		TareChuteVisual->SetCastShadow(false);
	}

	// Guard 2: Preserve MI_Station_DiverterFlapper baked bevel normals & grazing steel specular whenever DiverterFlapperMeshSlot assigns a valid mesh/material.
	const bool bHasExplicitFlapperOverride =
		(InPresentationData->DiverterFlapperMeshSlot.MaterialOverrides.Num() > 0) &&
		IsValid(InPresentationData->DiverterFlapperMeshSlot.MaterialOverrides[0]);
	const bool bFlapperHasValidMaterial =
		bAppliedFlapperSlot &&
		IsValid(DiverterFlapperVisual) &&
		IsValid(DiverterFlapperVisual->GetMaterial(0));
	if (!bHasExplicitFlapperOverride && !bFlapperHasValidMaterial)
	{
		ApplyDarkGunmetalMaterial(DiverterFlapperVisual);
	}
	ApplyDarkGunmetalMaterial(LeftHopperGuideVisual);
	ApplyTareHopperPresentationMaterials();

	if (IsLegacyWorldSpaceReadoutEnabled())
	{
		RefreshContractHeaderReadout();
		RefreshRouteReadout();
		RefreshStatusBannerReadout();
	}
}

void AScaleAssemblyActor::HandleContractSnapshotUpdated(const FContractRuntimeSnapshot& Snapshot)
{
	CachedSnapshot = Snapshot;
	CachedContractIndex = Snapshot.ContractIndex;

	if (IsValid(ScaleWeighingComponent))
	{
		const float SettleMultiplier = 1.0f + (0.25f * static_cast<float>(Snapshot.VeinScannerDamperLevel));
		ScaleWeighingComponent->SetSettleDampingMultiplier(SettleMultiplier);
	}

	if (UWorld* World = GetWorld())
	{
		if (const UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			CachedTotalContracts = FMath::Max(1, ShiftSubsystem->GetTotalContractCount());
			CachedShiftPhase = ShiftSubsystem->GetCurrentPhase();
		}
	}

	if (IsLegacyWorldSpaceReadoutEnabled())
	{
		const bool bContaminated = HasContaminationFault_Implementation();
		const bool bOvershoot = Snapshot.SettledScaleMassKg > (Snapshot.TargetTitheMassKg + Snapshot.QuotaToleranceKg + 1.0e-3f);

		RefreshContractHeaderReadout();

		if (IsValid(MassReadoutText))
		{
			const FString MassLine = FString::Printf(
				TEXT("%05.2f / %05.2f KG"),
				Snapshot.SettledScaleMassKg,
				Snapshot.TargetTitheMassKg);
			MassReadoutText->SetText(FText::FromString(MassLine));

			const bool bFaultActive = (CachedShiftPhase == EShiftPhase::FaultResetDelay);
			if (bContaminated || bOvershoot || bFaultActive)
			{
				MassReadoutText->SetTextRenderColor(ReadoutCrimsonColor);
			}
			else if (Snapshot.bIsWithinQuotaWindow)
			{
				MassReadoutText->SetTextRenderColor(ReadoutEmeraldColor);
			}
			else
			{
				MassReadoutText->SetTextRenderColor(ReadoutGoldColor);
			}
		}

		if (IsValid(TareReadoutText))
		{
			const FString TareLine = FString::Printf(
				TEXT("TARE: %.1f / %.1f KG"),
				Snapshot.AccumulatedTareWasteKg,
				Snapshot.MaxAllowedTareWasteKg);
			TareReadoutText->SetText(FText::FromString(TareLine));

			const bool bTareExceeded = Snapshot.AccumulatedTareWasteKg > (Snapshot.MaxAllowedTareWasteKg + 1.0e-3f);
			TareReadoutText->SetTextRenderColor(bTareExceeded ? ReadoutCrimsonColor : ReadoutGoldColor);
		}

		RefreshRouteReadout();
		RefreshStatusBannerReadout();
	}

	if (IsValid(ScaleWellThermalLightComponent))
	{
		const FMoltenBounceLightConfig MoltenCfg = IsValid(PresentationData)
			? PresentationData->MoltenBounceConfig
			: FMoltenBounceLightConfig();
		const float SafeTargetKg = FMath::Max(1.0f, Snapshot.TargetTitheMassKg);
		const float EmberRatio = (Snapshot.SettledScaleMassKg > 0.01f)
			? FMath::Clamp(Snapshot.SettledScaleMassKg / SafeTargetKg, 0.15f, 1.0f)
			: 0.0f;
		ScaleWellThermalLightComponent->SetSteadyEmberIntensity(MoltenCfg.ScaleWellMaxSteadyIntensity * EmberRatio);
	}
}

void AScaleAssemblyActor::HandleShiftVerdict(EShiftVerdictType Verdict, EContractFaultReason FaultReason)
{
	LatchedVerdict = Verdict;
	LatchedFaultReason = FaultReason;
	bHasLatchedVerdictBanner = true;
	RefreshStatusBannerReadout();
}

void AScaleAssemblyActor::HandleShiftPhaseChanged(EShiftPhase NewPhase)
{
	CachedShiftPhase = NewPhase;
	if (NewPhase == EShiftPhase::ContractIntro || NewPhase == EShiftPhase::ActiveInspection)
	{
		bHasLatchedVerdictBanner = false;
		LatchedFaultReason = EContractFaultReason::None;
	}
	else if (NewPhase == EShiftPhase::FaultResetDelay)
	{
		if (UWorld* World = GetWorld())
		{
			if (const UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
			{
				LatchedVerdict = EShiftVerdictType::ContractFaultReset;
				LatchedFaultReason = ShiftSubsystem->GetLastFaultReason();
				bHasLatchedVerdictBanner = (LatchedFaultReason != EContractFaultReason::None);
			}
		}
	}

	RefreshStatusBannerReadout();
}

void AScaleAssemblyActor::HandleActiveContractLoaded(
	const UMonolithContractDataAsset* Contract,
	const UShiftProgressionDataAsset* ShiftConfig,
	int32 ContractIndex)
{
	CachedContractIndex = ContractIndex;
	if (IsValid(ShiftConfig))
	{
		CachedTotalContracts = FMath::Max(1, ShiftConfig->OrderedContracts.Num());
	}
	if (IsValid(Contract))
	{
		CachedSnapshot.TargetTitheMassKg = Contract->TargetTitheMassKg;
		CachedSnapshot.QuotaToleranceKg = Contract->QuotaToleranceKg;
		CachedSnapshot.MaxAllowedTareWasteKg = Contract->MaxAllowedTareWasteKg;

		if (UWorld* World = GetWorld())
		{
			if (const UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
			{
				CachedSnapshot = ShiftSubsystem->GetCurrentSnapshot();
			}
		}
	}

	bHasLatchedVerdictBanner = false;
	LatchedFaultReason = EContractFaultReason::None;

	RefreshContractHeaderReadout();
	RefreshRouteReadout();
	RefreshStatusBannerReadout();
}

void AScaleAssemblyActor::HandleDiverterRouteChanged(EDiverterRoute NewRoute)
{
	RefreshRouteReadout();
	RefreshStatusBannerReadout();
	if (NewRoute == EDiverterRoute::TitheScale)
	{
		ReconcileScaleOverlaps();
	}
}

void AScaleAssemblyActor::HandleDiverterRouteSlewCompleted(EDiverterRoute ConvergedRoute)
{
	if (ConvergedRoute == EDiverterRoute::TitheScale)
	{
		ReconcileScaleOverlaps();
	}
}

bool AScaleAssemblyActor::ShouldIgnorePayloadInPanForTareRamp(const AActor* CandidateActor) const
{
	if (!IsValid(CandidateActor) || !IsValid(DiverterGateComponent) || !IsValid(DiverterFlapperPivot))
	{
		return false;
	}

	const bool bRampClosedOrSlewing =
		(DiverterGateComponent->GetCurrentRoute() == EDiverterRoute::TareChute) ||
		(DiverterGateComponent->GetCurrentFlapperAngleDegrees() > FlapperFilterEngagementAngleDegrees);

	if (!bRampClosedOrSlewing)
	{
		return false;
	}

	const FVector FlapperPivotWorld = DiverterFlapperPivot->GetComponentLocation();
	// DiverterFlapperPivot rotates around +X from 0 deg (vertical along +Z) to +45 deg (ramp toward -Y/+Z);
	// its right vector (+Y rotated around +X by +theta) points strictly toward the upper/chute half-space of the plate.
	const FVector FlapperUpperNormalWorld = DiverterFlapperPivot->GetRightVector().GetSafeNormal();
	if (FlapperUpperNormalWorld.IsNearlyZero())
	{
		return false;
	}

	const double SignedDistAboveRampCm = FVector::DotProduct(
		CandidateActor->GetActorLocation() - FlapperPivotWorld,
		FlapperUpperNormalWorld);

	return SignedDistAboveRampCm >= static_cast<double>(FlapperUpperSideSignedMarginCm);
}

void AScaleAssemblyActor::EnforcePanTriggerGeometryAndCollision()
{
	UStaticMeshComponent* const PureVisualMeshes[7] = {
		ScaleBaseVisual,
		LeftHopperGuideVisual,
		PanPlatformVisual,
		DiverterFlapperVisual,
		TareChuteVisual,
		TareHopperFurnaceGlowCard,
		ReadoutBackingPlate
	};
	for (UStaticMeshComponent* VisualComp : PureVisualMeshes)
	{
		if (IsValid(VisualComp))
		{
			VisualComp->SetSimulatePhysics(false);
			VisualComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			VisualComp->SetGenerateOverlapEvents(false);
		}
	}

	UInstancedStaticMeshComponent* const PureVisualISMs[2] = {
		TareHopperCollarISM,
		TareHopperThroatISM
	};
	for (UInstancedStaticMeshComponent* VisualISM : PureVisualISMs)
	{
		if (IsValid(VisualISM))
		{
			VisualISM->SetSimulatePhysics(false);
			VisualISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			VisualISM->SetGenerateOverlapEvents(false);
			VisualISM->SetCastShadow(false);
		}
	}

	RebuildTareHopperVisualInstances();
	if (IsValid(TareHopperFurnaceGlowCard))
	{
		TareHopperFurnaceGlowCard->SetRelativeLocation(
			TareBasinVisualOffsetCm + FVector(0.0, 0.0, TareFurnaceCardOffsetZAboveBasinCm));
		TareHopperFurnaceGlowCard->SetRelativeScale3D(FVector(0.78, 0.70, 0.015));
		TareHopperFurnaceGlowCard->SetCastShadow(false);
	}

	auto EnforceAuthoritativeCollider = [](
		UBoxComponent* ColliderBox,
		USceneComponent* ExpectedParent,
		const FVector& RelLocationCm,
		const FVector& ExtentCm)
	{
		if (!IsValid(ColliderBox) || !IsValid(ExpectedParent))
		{
			return;
		}
		if (ColliderBox->GetAttachParent() != ExpectedParent)
		{
			ColliderBox->AttachToComponent(ExpectedParent, FAttachmentTransformRules::KeepRelativeTransform);
		}
		ColliderBox->SetRelativeScale3D(FVector::OneVector);
		ColliderBox->SetRelativeLocation(RelLocationCm);
		ColliderBox->SetBoxExtent(ExtentCm, true);
		ColliderBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		ColliderBox->SetCollisionObjectType(ECC_WorldDynamic);
		ColliderBox->SetCollisionResponseToAllChannels(ECR_Ignore);
		ColliderBox->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
		ColliderBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		ColliderBox->SetGenerateOverlapEvents(false);
	};

	if (IsValid(PanMotionRoot))
	{
		PanMotionRoot->SetRelativeScale3D(FVector::OneVector);
	}
	if (IsValid(DiverterFlapperPivot))
	{
		DiverterFlapperPivot->SetRelativeScale3D(FVector::OneVector);
	}

	if (IsValid(LeftHopperGuideVisual))
	{
		LeftHopperGuideVisual->SetRelativeLocation(LeftHopperGuideOffsetCm);
		LeftHopperGuideVisual->SetRelativeScale3D((2.0 * LeftHopperGuideBoxExtentCm) / 100.0);
	}
	if (IsValid(ReadoutBackingPlate))
	{
		ReadoutBackingPlate->SetRelativeLocation(FVector::ZeroVector);
		ReadoutBackingPlate->SetRelativeScale3D(FVector(0.016, static_cast<double>(ReadoutBackingPlateWidthCm) / 100.0, 0.37));
	}
	if (IsLegacyWorldSpaceReadoutEnabled())
	{
		FitReadoutLineWidth(ControlStencilText, ControlStencilDefaultWorldSize, ControlStencilMinWorldSize, ControlStencilMaxSafeWidthCm);
	}

	EnforceAuthoritativeCollider(
		LeftHopperGuideCollider,
		ScaleRoot,
		LeftHopperGuideOffsetCm,
		LeftHopperGuideBoxExtentCm);

	EnforceAuthoritativeCollider(
		PanDeckCollider,
		PanMotionRoot,
		FVector(0.0, 0.0, static_cast<double>(PanDeckTopLocalZCm) - PanDeckBoxExtentCm.Z),
		PanDeckBoxExtentCm);

	EnforceAuthoritativeCollider(
		DiverterPlateCollider,
		DiverterFlapperPivot,
		DiverterPlateColliderOffsetCm,
		DiverterPlateBoxExtentCm);

	auto EnforceInvisibleGuideBox = [&](
		UBoxComponent* GuideBox,
		const FVector& RelLocationCm,
		const FQuat& RelQuat,
		const FVector& ExtentCm)
	{
		if (!IsValid(GuideBox) || !IsValid(ScaleRoot))
		{
			return;
		}
		EnforceAuthoritativeCollider(GuideBox, ScaleRoot, RelLocationCm, ExtentCm);
		GuideBox->SetRelativeRotation(RelQuat);
		GuideBox->SetHiddenInGame(true, false);
		GuideBox->SetVisibility(false, false);
		GuideBox->SetCastShadow(false);
	};

	const double GuideWallHeightZ = TareGuideWallTopLocalZCm - TareShaftBottomLocalZCm;
	const double GuideWallCenterZ = 0.5 * (TareGuideWallTopLocalZCm + TareShaftBottomLocalZCm);
	const double GuideWallHalfHeightZ = 0.5 * GuideWallHeightZ;

	EnforceInvisibleGuideBox(
		TareGuideWallOuterRightBox,
		FVector(0.0, TareShaftOuterMaxYCm + TareGuideWallHalfThickCm, GuideWallCenterZ),
		FQuat::Identity,
		FVector(TareShaftClearHalfWidthXCm + (2.0 * TareGuideWallHalfThickCm), TareGuideWallHalfThickCm, GuideWallHalfHeightZ));
	EnforceInvisibleGuideBox(
		TareGuideWallFrontBox,
		FVector(-(TareShaftClearHalfWidthXCm + TareGuideWallHalfThickCm), 85.0, GuideWallCenterZ),
		FQuat::Identity,
		FVector(TareGuideWallHalfThickCm, 49.0, GuideWallHalfHeightZ));
	EnforceInvisibleGuideBox(
		TareGuideWallBackBox,
		FVector(+(TareShaftClearHalfWidthXCm + TareGuideWallHalfThickCm), 85.0, GuideWallCenterZ),
		FQuat::Identity,
		FVector(TareGuideWallHalfThickCm, 49.0, GuideWallHalfHeightZ));
	EnforceInvisibleGuideBox(
		TareGuideWallInnerLeftLowerBox,
		FVector(0.0, TareShaftInnerMinYCm - TareGuideWallHalfThickCm, -48.0),
		FQuat::Identity,
		FVector(TareShaftClearHalfWidthXCm + (2.0 * TareGuideWallHalfThickCm), TareGuideWallHalfThickCm, 50.0));
	EnforceInvisibleGuideBox(
		TareGuideFeedApronSlopeBox,
		FVector(0.0, TareFeedApronCenterYCm, TareFeedApronCenterZCm),
		FQuat(FVector::ForwardVector, FMath::DegreesToRadians(TareFeedApronSlopeRollDeg)),
		FVector(0.5 * TareFeedApronSpanXCm, 0.5 * TareFeedApronLengthCm, 0.5 * TareFeedApronThicknessCm));
	EnforceInvisibleGuideBox(
		TareGuideChamferOuterRightBox,
		FVector(0.0, TareShaftOuterMaxYCm - 2.5, TareGuideChamferCenterZCm),
		FQuat(FVector::ForwardVector, FMath::DegreesToRadians(+TareGuideChamferTiltDeg)),
		FVector(TareShaftClearHalfWidthXCm + 4.0, 1.5, 8.0));
	EnforceInvisibleGuideBox(
		TareGuideChamferFrontBox,
		FVector(-(TareShaftClearHalfWidthXCm - 2.0), TareShaftCenterYCm, TareGuideChamferCenterZCm),
		FQuat(FVector::RightVector, FMath::DegreesToRadians(+TareGuideChamferTiltDeg)),
		FVector(1.5, 40.0, 8.0));
	EnforceInvisibleGuideBox(
		TareGuideChamferBackBox,
		FVector(+(TareShaftClearHalfWidthXCm - 2.0), TareShaftCenterYCm, TareGuideChamferCenterZCm),
		FQuat(FVector::RightVector, FMath::DegreesToRadians(-TareGuideChamferTiltDeg)),
		FVector(1.5, 40.0, 8.0));

	auto EnforceOverlapTriggerBox = [this](UBoxComponent* TriggerBox, const FVector& RelLocationCm, const FVector& ExtentCm)
	{
		if (!IsValid(TriggerBox) || !IsValid(ScaleRoot))
		{
			return;
		}
		if (TriggerBox->GetAttachParent() != ScaleRoot)
		{
			TriggerBox->AttachToComponent(ScaleRoot, FAttachmentTransformRules::KeepRelativeTransform);
		}
		TriggerBox->SetRelativeScale3D(FVector::OneVector);
		TriggerBox->SetRelativeRotation(FQuat::Identity);
		TriggerBox->SetRelativeLocation(RelLocationCm);
		TriggerBox->SetBoxExtent(ExtentCm, true);
		TriggerBox->SetHiddenInGame(true, false);
		TriggerBox->SetVisibility(false, false);
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		TriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
		TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
		TriggerBox->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
		TriggerBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		TriggerBox->SetGenerateOverlapEvents(true);
	};

	EnforceOverlapTriggerBox(TareChuteTriggerBox, TareMouthTriggerOffsetCm, TareMouthTriggerExtentCm);
	EnforceOverlapTriggerBox(TareAbyssDespawnTriggerBox, TareAbyssDespawnTriggerOffsetCm, TareAbyssDespawnTriggerExtentCm);

	if (!IsValid(PanTriggerBox) || !IsValid(PanMotionRoot))
	{
		return;
	}

	if (PanTriggerBox->GetAttachParent() != PanMotionRoot)
	{
		PanTriggerBox->AttachToComponent(PanMotionRoot, FAttachmentTransformRules::KeepRelativeTransform);
	}

	PanTriggerBox->SetRelativeScale3D(FVector::OneVector);
	PanTriggerBox->SetRelativeLocation(FVector(0.0, 0.0, static_cast<double>(PanDeckTopLocalZCm + PanTriggerCenterAboveDeckZCm)));
	PanTriggerBox->SetBoxExtent(PanTriggerBoxExtentCm, true);
	PanTriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PanTriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
	PanTriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	PanTriggerBox->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	PanTriggerBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	PanTriggerBox->SetGenerateOverlapEvents(true);
}

bool AScaleAssemblyActor::IsLegacyWorldSpaceReadoutEnabled() const
{
	return IsValid(PresentationData) && PresentationData->HudConfig.bEnableLegacyWorldSpaceReadouts;
}

void AScaleAssemblyActor::UpdateLegacyConsoleVisibility()
{
	const bool bShowLegacy = IsLegacyWorldSpaceReadoutEnabled();

	USceneComponent* const ConsoleRootsAndPlate[3] = {
		ReadoutConsolePivot,
		ScaleConsoleRoot,
		ReadoutBackingPlate
	};
	for (USceneComponent* Comp : ConsoleRootsAndPlate)
	{
		if (IsValid(Comp))
		{
			Comp->SetVisibility(bShowLegacy, false);
			Comp->SetHiddenInGame(!bShowLegacy, false);
		}
	}

	UTextRenderComponent* const ConsoleTexts[7] = {
		HeaderStencilText,
		ContractCodeText,
		StatusBannerText,
		MassReadoutText,
		TareReadoutText,
		RouteReadoutText,
		ControlStencilText
	};
	for (UTextRenderComponent* TextComp : ConsoleTexts)
	{
		if (IsValid(TextComp))
		{
			TextComp->SetVisibility(bShowLegacy, false);
			TextComp->SetHiddenInGame(!bShowLegacy, false);
		}
	}
}

void AScaleAssemblyActor::RefreshContractHeaderReadout()
{
	if (!IsLegacyWorldSpaceReadoutEnabled() || !IsValid(ContractCodeText))
	{
		return;
	}

	const float TargetKg = FMath::Max(0.0f, CachedSnapshot.TargetTitheMassKg);
	const float MinValidKg = FMath::Max(0.0f, TargetKg - CachedSnapshot.QuotaToleranceKg);
	const float MaxValidKg = TargetKg + CachedSnapshot.QuotaToleranceKg;
	const float DirCutTolKg = FMath::Max(0.15f, CachedSnapshot.DirectorsCutToleranceKg);
	const float DirCutMinKg = FMath::Max(0.0f, TargetKg - DirCutTolKg);
	const float DirCutMaxKg = TargetKg + DirCutTolKg;

	const FString HeaderLine = FString::Printf(
		TEXT("TARGET QUOTA: %.2f KG  [VALID WINDOW: %.2f - %.2f KG]"),
		TargetKg,
		MinValidKg,
		MaxValidKg);

	ContractCodeText->SetText(FText::FromString(HeaderLine));
	ContractCodeText->SetTextRenderColor(ReadoutHeaderCyanColor);
	FitReadoutLineWidth(ContractCodeText, 4.5f, 3.2f, StatusBannerMaxSafeWidthCm);

	if (IsValid(HeaderStencilText))
	{
		const FString TopStencilLine = FString::Printf(
			TEXT("TITHE WEIGHING CONSOLE // DIRECTOR'S CUT: %.2f - %.2f KG (+2 SCRIP)"),
			DirCutMinKg,
			DirCutMaxKg);

		HeaderStencilText->SetText(FText::FromString(TopStencilLine));
		HeaderStencilText->SetTextRenderColor(ReadoutHeaderCyanColor);
		FitReadoutLineWidth(HeaderStencilText, 4.8f, 3.1f, StatusBannerMaxSafeWidthCm);
	}
	if (IsValid(ControlStencilText))
	{
		ControlStencilText->SetTextRenderColor(ReadoutControlStencilColor);
		FitReadoutLineWidth(ControlStencilText, ControlStencilDefaultWorldSize, ControlStencilMinWorldSize, ControlStencilMaxSafeWidthCm);
	}
}

void AScaleAssemblyActor::RefreshStatusBannerReadout()
{
	if (!IsLegacyWorldSpaceReadoutEnabled() || !IsValid(StatusBannerText))
	{
		return;
	}

	if (CachedShiftPhase == EShiftPhase::FaultResetDelay ||
		(bHasLatchedVerdictBanner && LatchedVerdict == EShiftVerdictType::ContractFaultReset))
	{
		SetStatusBannerTextFitted(FormatFaultReasonBanner(LatchedFaultReason), ReadoutFaultBannerColor);
		return;
	}

	if (bHasLatchedVerdictBanner ||
		CachedShiftPhase == EShiftPhase::ContractPassTransition ||
		CachedShiftPhase == EShiftPhase::ShiftCompleted ||
		CachedShiftPhase == EShiftPhase::BondCleared)
	{
		if (LatchedVerdict == EShiftVerdictType::BondCleared || CachedShiftPhase == EShiftPhase::BondCleared)
		{
			SetStatusBannerTextFitted(
				TEXT("BOND CLEARED - #409 RELEASED | PRESS [E] OVERTIME | HOLD [R] CAREER RESET"),
				ReadoutGoldColor);
			return;
		}
		if (LatchedVerdict == EShiftVerdictType::ShiftCompleted || CachedShiftPhase == EShiftPhase::ShiftCompleted)
		{
			const int32 CurShift = FMath::Max(1, CachedSnapshot.CurrentShiftNumber);
			const int32 NextShift = CurShift + 1;
			const int32 TargetShifts = FMath::Max(1, CachedSnapshot.TargetBondShifts);
			const FString ShiftBanner = (NextShift <= TargetShifts)
				? FString::Printf(
					TEXT("SHIFT %02d/%02d COMPLETE | PRESS [E] BEGIN SHIFT %02d/%02d | HOLD [R] CAREER RESET"),
					CurShift,
					TargetShifts,
					NextShift,
					TargetShifts)
				: FString::Printf(
					TEXT("OVERTIME S%02d COMPLETE | PRESS [E] BEGIN OVT S%02d | HOLD [R] CAREER RESET"),
					CurShift,
					NextShift);
			SetStatusBannerTextFitted(ShiftBanner, ReadoutEmeraldColor);
			return;
		}
		if (LatchedVerdict == EShiftVerdictType::DirectorsCutPassed ||
			(CachedShiftPhase == EShiftPhase::ContractPassTransition && CachedSnapshot.bLastStampWasDirectorsCut))
		{
			SetStatusBannerTextFitted(
				TEXT("DIRECTOR'S CUT STAMPED (+2 SCRIP) - LOADING NEXT CONTRACT"),
				ReadoutGoldColor);
			return;
		}
		if (LatchedVerdict == EShiftVerdictType::ContractPassed || CachedShiftPhase == EShiftPhase::ContractPassTransition)
		{
			SetStatusBannerTextFitted(
				TEXT("QUOTA STAMPED (+1 SCRIP) - LOADING NEXT CONTRACT"),
				ReadoutEmeraldColor);
			return;
		}
	}

	if (HasContaminationFault_Implementation())
	{
		SetStatusBannerTextFitted(FormatFaultReasonBanner(EContractFaultReason::ScaleContaminated), ReadoutFaultBannerColor);
		return;
	}

	if (CachedSnapshot.InFlightMassKg > 0.01f)
	{
		SetStatusBannerTextFitted(TEXT("CHUNK DROPPING..."), ReadoutNormalSkyBlueColor);
		return;
	}

	if (!CachedSnapshot.bScaleSettledForStamp &&
		CachedSnapshot.SettledScaleMassKg > 0.0f &&
		!IsReceiverSettled_Implementation())
	{
		SetStatusBannerTextFitted(TEXT("SCALE SETTLING..."), ReadoutGoldColor);
		return;
	}

	if (CachedSnapshot.bScaleSettledForStamp || CachedSnapshot.bIsWithinQuotaWindow)
	{
		if (CachedSnapshot.bIsWithinDirectorsCutWindow)
		{
			SetStatusBannerTextFitted(
				TEXT("DIRECTOR'S CUT LOCKED! PRESS [E] STAMP (+2 SCRIP)"),
				ReadoutGoldColor);
		}
		else
		{
			SetStatusBannerTextFitted(
				TEXT("QUOTA MET - PRESS [E] TO STAMP (+1 SCRIP)"),
				ReadoutEmeraldColor);
		}
		return;
	}

	if (CachedSnapshot.ClampStrainNormalized >= 0.78f)
	{
		SetStatusBannerTextFitted(
			TEXT("!! JAW SHEAR CRITICAL - BALANCE CUTS TO OPPOSITE SIDE !!"),
			ReadoutFaultBannerColor);
		return;
	}

	const EDiverterRoute EffectiveRoute = IsValid(DiverterGateComponent)
		? DiverterGateComponent->GetCurrentRoute()
		: CachedSnapshot.ActiveDiverterRoute;
	if (EffectiveRoute == EDiverterRoute::TareChute)
	{
		SetStatusBannerTextFitted(TEXT("TARE CHUTE ACTIVE - HOLD [LMB] TO DUMP SCRAP | [SPACE] TITHE"), ReadoutTareOrangeColor);
		return;
	}

	const float MinValidKg = FMath::Max(0.0f, CachedSnapshot.TargetTitheMassKg - CachedSnapshot.QuotaToleranceKg);
	if (CachedSnapshot.SettledScaleMassKg > KINDA_SMALL_NUMBER && CachedSnapshot.SettledScaleMassKg < MinValidKg)
	{
		const float RemainingToWindowKg = FMath::Max(0.01f, MinValidKg - CachedSnapshot.SettledScaleMassKg);
		SetStatusBannerTextFitted(
			FString::Printf(TEXT("UNDER QUOTA - ADD %.2f KG TO MIN WINDOW"), RemainingToWindowKg),
			ReadoutNormalSkyBlueColor);
		return;
	}

	SetStatusBannerTextFitted(TEXT("SCALE READY - AWAITING TITHE PAYLOAD"), ReadoutNormalSkyBlueColor);
}

void AScaleAssemblyActor::SetStatusBannerTextFitted(const FString& BannerString, const FColor& BannerColor)
{
	if (!IsValid(StatusBannerText))
	{
		return;
	}

	StatusBannerText->SetTextRenderColor(BannerColor);
	StatusBannerText->SetText(FText::FromString(BannerString));
	FitReadoutLineWidth(
		StatusBannerText,
		StatusBannerDefaultWorldSize,
		StatusBannerMinWorldSize,
		StatusBannerMaxSafeWidthCm);
}

void AScaleAssemblyActor::FitReadoutLineWidth(
	UTextRenderComponent* TextComp,
	float DefaultWorldSize,
	float MinWorldSize,
	float MaxSafeWidthCm)
{
	if (!IsValid(TextComp))
	{
		return;
	}

	const float SafeDefaultSize = FMath::Max(0.1f, DefaultWorldSize);
	const float SafeCurrentSize = FMath::Max(0.1f, TextComp->WorldSize);
	const FVector LocalSize = TextComp->GetTextLocalSize();
	const float CurrentWidthYCm = static_cast<float>(FMath::Max(0.0, LocalSize.Y));
	const float WidthAtDefaultCm = CurrentWidthYCm * (SafeDefaultSize / SafeCurrentSize);

	float TargetWorldSize = SafeDefaultSize;
	if (WidthAtDefaultCm > MaxSafeWidthCm && WidthAtDefaultCm > KINDA_SMALL_NUMBER)
	{
		TargetWorldSize = FMath::Clamp(
			SafeDefaultSize * (MaxSafeWidthCm / WidthAtDefaultCm),
			MinWorldSize,
			SafeDefaultSize);
	}

	if (!FMath::IsNearlyEqual(SafeCurrentSize, TargetWorldSize, 0.01f))
	{
		TextComp->SetWorldSize(TargetWorldSize);
	}
}

FString AScaleAssemblyActor::FormatFaultReasonBanner(EContractFaultReason FaultReason)
{
	switch (FaultReason)
	{
	case EContractFaultReason::QuotaOvershoot:
		return TEXT("FAULT: OVERWEIGHT SLOP - EXCEEDED MAX TOLERANCE");
	case EContractFaultReason::VolatileDetonation:
		return TEXT("FAULT: VOLATILE ISOTOPE VEIN DETONATED");
	case EContractFaultReason::ClampStructuralSnap:
		return TEXT("FAULT: CLAMP JAW SHEARED - BALANCE YOUR CUTS!");
	case EContractFaultReason::ScaleContaminated:
		return TEXT("FAULT: CONTAMINATED TITHE - ROUTE VEINS TO TARE [SPACE]");
	case EContractFaultReason::TareAllowanceExceeded:
		return TEXT("FAULT: TARE HOPPER CHOKED - MAX WASTE EXCEEDED");
	case EContractFaultReason::MathematicallyUnreachable:
		return TEXT("FAULT: CORE RUINED - INSUFFICIENT MASS FOR QUOTA");
	case EContractFaultReason::ManualOperatorReset:
		return TEXT("CELL PURGED BY #409 [R] - NO SCRIP ISSUED");
	default:
		return TEXT("FAULT: RESETTING INSPECTION CELL 09");
	}
}

void AScaleAssemblyActor::RefreshRouteReadout()
{
	if (!IsLegacyWorldSpaceReadoutEnabled() || !IsValid(RouteReadoutText) || !IsValid(DiverterGateComponent))
	{
		return;
	}

	const EDiverterRoute ActiveRoute = DiverterGateComponent->GetCurrentRoute();
	if (ActiveRoute == EDiverterRoute::TareChute)
	{
		RouteReadoutText->SetText(FText::FromString(TEXT("ROUTE: TARE [SPACE]")));
		RouteReadoutText->SetTextRenderColor(ReadoutTareOrangeColor);
	}
	else
	{
		RouteReadoutText->SetText(FText::FromString(TEXT("ROUTE: TITHE [SPACE]")));
		RouteReadoutText->SetTextRenderColor(ReadoutHeaderCyanColor);
	}
}

void AScaleAssemblyActor::InitializeGreyboxVisualMaterials()
{
	if (IsValid(UnlitReadoutTextMaterial))
	{
		UTextRenderComponent* const ReadoutComponents[7] = {
			HeaderStencilText,
			ContractCodeText,
			StatusBannerText,
			MassReadoutText,
			TareReadoutText,
			RouteReadoutText,
			ControlStencilText
		};
		for (UTextRenderComponent* TextComp : ReadoutComponents)
		{
			if (IsValid(TextComp))
			{
				TextComp->SetTextMaterial(UnlitReadoutTextMaterial);
				TextComp->SetCastShadow(false);
			}
		}
	}

	if (!IsValid(BaseGreyboxShapeMaterial))
	{
		return;
	}

	auto ApplyTintedGreyboxMaterial = [this](
		UStaticMeshComponent* MeshComp,
		const FLinearColor& LinearColor,
		float Roughness,
		float Metallic)
	{
		if (IsValid(MeshComp))
		{
			if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseGreyboxShapeMaterial, this))
			{
				MID->SetVectorParameterValue(TEXT("BaseColorConst"), LinearColor);
				MID->SetVectorParameterValue(TEXT("Color"), LinearColor);
				MID->SetVectorParameterValue(TEXT("EmissiveColorConst"), FLinearColor::Black);
				MID->SetScalarParameterValue(TEXT("EmissiveScale"), 0.0f);
				MID->SetScalarParameterValue(TEXT("RoughnessConst"), Roughness);
				MID->SetScalarParameterValue(TEXT("MetallicConst"), Metallic);
				MeshComp->SetMaterial(0, MID);
			}
		}
	};

	ApplyTintedGreyboxMaterial(ReadoutBackingPlate, ConsoleBezelLinearColor, 0.99f, 0.0f);
	ApplyTintedGreyboxMaterial(ScaleBaseVisual, FLinearColor(0.055f, 0.062f, 0.072f, 1.0f), 0.76f, 0.35f);
	ApplyTintedGreyboxMaterial(PanPlatformVisual, FLinearColor(0.095f, 0.086f, 0.074f, 1.0f), 0.66f, 0.45f);
	ApplyTintedGreyboxMaterial(TareChuteVisual, FLinearColor(0.068f, 0.032f, 0.028f, 1.0f), 0.80f, 0.28f);

	ApplyDarkGunmetalMaterial(LeftHopperGuideVisual);
	ApplyDarkGunmetalMaterial(DiverterFlapperVisual);
	ApplyTareHopperPresentationMaterials();
}

void AScaleAssemblyActor::RebuildTareHopperVisualInstances()
{
	if (!IsValid(FallbackCubeMesh))
	{
		return;
	}

	const FBox CubeBox = FallbackCubeMesh->GetBoundingBox();
	const FVector CubeSize = CubeBox.IsValid ? CubeBox.GetSize() : FVector(100.0, 100.0, 100.0);
	const double InvCubeX = 1.0 / FMath::Max(1.0, CubeSize.X);
	const double InvCubeY = 1.0 / FMath::Max(1.0, CubeSize.Y);
	const double InvCubeZ = 1.0 / FMath::Max(1.0, CubeSize.Z);

	auto AddBoxInstance = [&](
		UInstancedStaticMeshComponent* TargetISM,
		const FVector& CenterCm,
		const FQuat& Quat,
		const FVector& SizeCm)
	{
		if (!IsValid(TargetISM))
		{
			return;
		}
		const FVector Scale(SizeCm.X * InvCubeX, SizeCm.Y * InvCubeY, SizeCm.Z * InvCubeZ);
		TargetISM->AddInstance(FTransform(Quat, CenterCm, Scale), false);
	};

	if (IsValid(TareHopperCollarISM))
	{
		TareHopperCollarISM->ClearInstances();
		TareHopperCollarISM->SetStaticMesh(FallbackCubeMesh);

		// 1. Sloped -Y feed apron bridging downward from beneath DiverterFlapperPivot (Y=+34) into the Y=+52 shaft throat (max Z <= +15.8 cm).
		AddBoxInstance(
			TareHopperCollarISM,
			FVector(0.0, TareFeedApronCenterYCm, TareFeedApronCenterZCm),
			FQuat(FVector::ForwardVector, FMath::DegreesToRadians(TareFeedApronSlopeRollDeg)),
			FVector(TareFeedApronSpanXCm, TareFeedApronLengthCm, TareFeedApronThicknessCm));

		const double SideCollarHeightZ = TareCollarSideTopLocalZCm - TareCollarBaseLocalZCm;
		const double SideCollarCenterZ = 0.5 * (TareCollarSideTopLocalZCm + TareCollarBaseLocalZCm);
		const double FrontCollarHeightZ = TareCollarFrontTopLocalZCm - TareCollarBaseLocalZCm;
		const double FrontCollarCenterZ = 0.5 * (TareCollarFrontTopLocalZCm + TareCollarBaseLocalZCm);

		// 2. Outer-right (+Y) low bevelled industrial collar curb & inward bevel lip (ScaleRoot Z <= +15.8 cm).
		AddBoxInstance(
			TareHopperCollarISM,
			FVector(0.0, TareShaftOuterMaxYCm + (0.5 * TareCollarWallThicknessCm), SideCollarCenterZ),
			FQuat::Identity,
			FVector((2.0 * TareShaftClearHalfWidthXCm) + (2.0 * TareCollarWallThicknessCm), TareCollarWallThicknessCm, SideCollarHeightZ));
		AddBoxInstance(
			TareHopperCollarISM,
			FVector(0.0, TareShaftOuterMaxYCm + 1.6, 13.2),
			FQuat(FVector::ForwardVector, FMath::DegreesToRadians(+TareCollarLipBevelDeg)),
			FVector((2.0 * TareShaftClearHalfWidthXCm) + (2.0 * TareCollarWallThicknessCm) - 1.0, 6.2, 2.4));

		// 3. Stadium-stepped front (-X) low bevelled collar curb & inward bevel lip (ScaleRoot Z <= +8.2 cm for wide camera sightline into throat).
		AddBoxInstance(
			TareHopperCollarISM,
			FVector(-(TareShaftClearHalfWidthXCm + (0.5 * TareCollarWallThicknessCm)), 85.0, FrontCollarCenterZ),
			FQuat::Identity,
			FVector(TareCollarWallThicknessCm, 99.0, FrontCollarHeightZ));
		AddBoxInstance(
			TareHopperCollarISM,
			FVector(-(TareShaftClearHalfWidthXCm + 1.6), TareShaftCenterYCm, 5.6),
			FQuat(FVector::RightVector, FMath::DegreesToRadians(+TareCollarLipBevelDeg)),
			FVector(6.2, 78.0, 2.4));

		// 4. Back (+X) low bevelled collar curb & inward bevel lip (ScaleRoot Z <= +15.8 cm).
		AddBoxInstance(
			TareHopperCollarISM,
			FVector(+(TareShaftClearHalfWidthXCm + (0.5 * TareCollarWallThicknessCm)), 85.0, SideCollarCenterZ),
			FQuat::Identity,
			FVector(TareCollarWallThicknessCm, 99.0, SideCollarHeightZ));
		AddBoxInstance(
			TareHopperCollarISM,
			FVector(+(TareShaftClearHalfWidthXCm + 1.6), TareShaftCenterYCm, 13.2),
			FQuat(FVector::RightVector, FMath::DegreesToRadians(-TareCollarLipBevelDeg)),
			FVector(6.2, 78.0, 2.4));

		TareHopperCollarISM->MarkRenderStateDirty();
	}

	if (IsValid(TareHopperThroatISM))
	{
		TareHopperThroatISM->ClearInstances();
		TareHopperThroatISM->SetStaticMesh(FallbackCubeMesh);

		// Deep dark gunmetal vertical shaft liner walls (84 x 76 cm clear opening down to ScaleRoot Z = -98.0 cm).
		AddBoxInstance(
			TareHopperThroatISM,
			FVector(0.0, TareShaftInnerMinYCm - 1.5, -48.5),
			FQuat::Identity,
			FVector((2.0 * TareShaftClearHalfWidthXCm) + 6.0, 3.0, 99.0));
		AddBoxInstance(
			TareHopperThroatISM,
			FVector(0.0, TareShaftOuterMaxYCm + 1.5, -49.5),
			FQuat::Identity,
			FVector((2.0 * TareShaftClearHalfWidthXCm) + 6.0, 3.0, 97.0));
		AddBoxInstance(
			TareHopperThroatISM,
			FVector(-(TareShaftClearHalfWidthXCm + 1.5), TareShaftCenterYCm, -49.5),
			FQuat::Identity,
			FVector(3.0, 82.0, 97.0));
		AddBoxInstance(
			TareHopperThroatISM,
			FVector(+(TareShaftClearHalfWidthXCm + 1.5), TareShaftCenterYCm, -49.5),
			FQuat::Identity,
			FVector(3.0, 82.0, 97.0));

		TareHopperThroatISM->MarkRenderStateDirty();
	}
}

void AScaleAssemblyActor::ApplyTareHopperPresentationMaterials()
{
	if (!IsValid(AutoResolvedHopperCollarMaterial))
	{
		if (FPackageName::DoesPackageExist(TEXT("/Game/Presentation/Materials/MI_Station_TareChute")))
		{
			AutoResolvedHopperCollarMaterial = LoadObject<UMaterialInterface>(
				nullptr,
				TEXT("/Game/Presentation/Materials/MI_Station_TareChute.MI_Station_TareChute"));
		}
		else if (FPackageName::DoesPackageExist(TEXT("/Game/Presentation/Materials/MI_Station_ScaleBase")))
		{
			AutoResolvedHopperCollarMaterial = LoadObject<UMaterialInterface>(
				nullptr,
				TEXT("/Game/Presentation/Materials/MI_Station_ScaleBase.MI_Station_ScaleBase"));
		}
	}

	if (IsValid(TareHopperCollarISM))
	{
		if (UMaterialInterface* CollarPbrMat = ResolveRodinHopperCollarMaterial())
		{
			TareHopperCollarISM->SetMaterial(0, CollarPbrMat);
		}
		else
		{
			ApplyDarkGunmetalMaterialToISM(TareHopperCollarISM);
		}
	}

	ApplyDarkGunmetalMaterialToISM(TareHopperThroatISM);

	if (IsValid(TareHopperFurnaceGlowCard) && IsValid(BaseGreyboxShapeMaterial))
	{
		if (!IsValid(TareFurnaceBaffleMID))
		{
			TareFurnaceBaffleMID = UMaterialInstanceDynamic::Create(BaseGreyboxShapeMaterial, this);
		}
		if (IsValid(TareFurnaceBaffleMID))
		{
			const FLinearColor ClampedEmissive(
				FMath::Clamp(TareFurnaceBaffleEmissiveColor.R, 0.0f, MaxBloomSafeFurnaceEmissiveChannel),
				FMath::Clamp(TareFurnaceBaffleEmissiveColor.G, 0.0f, MaxBloomSafeFurnaceEmissiveChannel),
				FMath::Clamp(TareFurnaceBaffleEmissiveColor.B, 0.0f, MaxBloomSafeFurnaceEmissiveChannel),
				1.0f);
			const FLinearColor FurnaceBaseColor(0.24f, 0.055f, 0.012f, 1.0f);

			TareFurnaceBaffleMID->SetScalarParameterValue(TEXT("RoughnessConst"), 0.94f);
			TareFurnaceBaffleMID->SetScalarParameterValue(TEXT("MetallicConst"), 0.04f);
			TareFurnaceBaffleMID->SetScalarParameterValue(TEXT("SpecularConst"), 0.02f);
			TareFurnaceBaffleMID->SetVectorParameterValue(TEXT("BaseColorConst"), FurnaceBaseColor);
			TareFurnaceBaffleMID->SetVectorParameterValue(TEXT("Color"), FurnaceBaseColor);
			TareFurnaceBaffleMID->SetVectorParameterValue(TEXT("EmissiveColorConst"), ClampedEmissive);
			TareFurnaceBaffleMID->SetScalarParameterValue(TEXT("EmissiveScale"), 1.0f);
			TareHopperFurnaceGlowCard->SetMaterial(0, TareFurnaceBaffleMID);
		}
	}
}

UMaterialInterface* AScaleAssemblyActor::ResolveDarkGunmetalMaterial()
{
	UMaterialInterface* ResolvedGunmetal =
		(IsValid(PresentationData) && IsValid(PresentationData->DarkGunmetalMaterial))
		? PresentationData->DarkGunmetalMaterial.Get()
		: nullptr;

	if (!IsValid(ResolvedGunmetal))
	{
		if (!IsValid(AutoResolvedDarkGunmetalMaterial) &&
			FPackageName::DoesPackageExist(TEXT("/Game/Presentation/Materials/M_DarkGunmetal_Master")))
		{
			AutoResolvedDarkGunmetalMaterial = LoadObject<UMaterialInterface>(
				nullptr,
				TEXT("/Game/Presentation/Materials/M_DarkGunmetal_Master.M_DarkGunmetal_Master"));
		}
		ResolvedGunmetal = AutoResolvedDarkGunmetalMaterial.Get();
	}

	return ResolvedGunmetal;
}

UMaterialInterface* AScaleAssemblyActor::ResolveRodinHopperCollarMaterial() const
{
	if (IsValid(PresentationData))
	{
		if (PresentationData->TareChuteMeshSlot.MaterialOverrides.Num() > 0 &&
			IsValid(PresentationData->TareChuteMeshSlot.MaterialOverrides[0]))
		{
			return PresentationData->TareChuteMeshSlot.MaterialOverrides[0].Get();
		}
		if (IsValid(PresentationData->TareChuteMeshSlot.Mesh))
		{
			if (UMaterialInterface* TareMeshMat = PresentationData->TareChuteMeshSlot.Mesh->GetMaterial(0))
			{
				return TareMeshMat;
			}
		}
		if (PresentationData->ScaleBaseMeshSlot.MaterialOverrides.Num() > 0 &&
			IsValid(PresentationData->ScaleBaseMeshSlot.MaterialOverrides[0]))
		{
			return PresentationData->ScaleBaseMeshSlot.MaterialOverrides[0].Get();
		}
		if (IsValid(PresentationData->ScaleBaseMeshSlot.Mesh))
		{
			if (UMaterialInterface* BaseMeshMat = PresentationData->ScaleBaseMeshSlot.Mesh->GetMaterial(0))
			{
				return BaseMeshMat;
			}
		}
	}

	if (IsValid(TareChuteVisual) &&
		TareChuteVisual->GetStaticMesh() != FallbackCubeMesh &&
		IsValid(TareChuteVisual->GetMaterial(0)))
	{
		return TareChuteVisual->GetMaterial(0);
	}

	return AutoResolvedHopperCollarMaterial.Get();
}

void AScaleAssemblyActor::ApplyDarkGunmetalMaterial(UStaticMeshComponent* TargetMesh)
{
	if (!IsValid(TargetMesh))
	{
		return;
	}

	if (UMaterialInterface* ResolvedGunmetal = ResolveDarkGunmetalMaterial())
	{
		const int32 NumSlots = FMath::Max(1, TargetMesh->GetNumMaterials());
		for (int32 SlotIdx = 0; SlotIdx < NumSlots; ++SlotIdx)
		{
			TargetMesh->SetMaterial(SlotIdx, ResolvedGunmetal);
		}
		return;
	}

	if (IsValid(BaseGreyboxShapeMaterial))
	{
		if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseGreyboxShapeMaterial, this))
		{
			MID->SetVectorParameterValue(TEXT("BaseColorConst"), DarkGunmetalLinearColor);
			MID->SetVectorParameterValue(TEXT("Color"), DarkGunmetalLinearColor);
			MID->SetVectorParameterValue(TEXT("EmissiveColorConst"), FLinearColor::Black);
			MID->SetScalarParameterValue(TEXT("EmissiveScale"), 0.0f);
			MID->SetScalarParameterValue(TEXT("RoughnessConst"), 0.42f);
			MID->SetScalarParameterValue(TEXT("MetallicConst"), 0.85f);
			const int32 NumSlots = FMath::Max(1, TargetMesh->GetNumMaterials());
			for (int32 SlotIdx = 0; SlotIdx < NumSlots; ++SlotIdx)
			{
				TargetMesh->SetMaterial(SlotIdx, MID);
			}
		}
	}
}

void AScaleAssemblyActor::ApplyDarkGunmetalMaterialToISM(UInstancedStaticMeshComponent* TargetISM)
{
	if (!IsValid(TargetISM))
	{
		return;
	}

	if (UMaterialInterface* ResolvedGunmetal = ResolveDarkGunmetalMaterial())
	{
		const int32 NumSlots = FMath::Max(1, TargetISM->GetNumMaterials());
		for (int32 SlotIdx = 0; SlotIdx < NumSlots; ++SlotIdx)
		{
			TargetISM->SetMaterial(SlotIdx, ResolvedGunmetal);
		}
		return;
	}

	if (IsValid(BaseGreyboxShapeMaterial))
	{
		if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseGreyboxShapeMaterial, this))
		{
			MID->SetVectorParameterValue(TEXT("BaseColorConst"), DarkGunmetalLinearColor);
			MID->SetVectorParameterValue(TEXT("Color"), DarkGunmetalLinearColor);
			MID->SetVectorParameterValue(TEXT("EmissiveColorConst"), FLinearColor::Black);
			MID->SetScalarParameterValue(TEXT("EmissiveScale"), 0.0f);
			MID->SetScalarParameterValue(TEXT("RoughnessConst"), 0.42f);
			MID->SetScalarParameterValue(TEXT("MetallicConst"), 0.85f);
			TargetISM->SetMaterial(0, MID);
		}
	}
}

FVector AScaleAssemblyActor::QueryScaleWorldLocation() const
{
	return GetActorLocation();
}

void AScaleAssemblyActor::HandleChildTelemetryStream(ETelemetryStreamId StreamId, float NormalizedValue)
{
	OnTelemetryStream.Broadcast(StreamId, NormalizedValue);
}

void AScaleAssemblyActor::HandleChildTelemetryImpulse(
	ETelemetryImpulseId ImpulseId,
	float NormalizedMagnitude,
	const FVector& WorldLocation)
{
	OnTelemetryImpulse.Broadcast(ImpulseId, NormalizedMagnitude, WorldLocation);
}

void AScaleAssemblyActor::HandleChunkLandedOnScale(AActor* PayloadActor, float ChunkMassKg, float UpdatedPanMassKg)
{
	if (IsValid(ScaleWellThermalLightComponent))
	{
		const FMoltenBounceLightConfig MoltenCfg = IsValid(PresentationData)
			? PresentationData->MoltenBounceConfig
			: FMoltenBounceLightConfig();
		const float SafeTargetKg = FMath::Max(1.0f, CachedSnapshot.TargetTitheMassKg);
		const float ImpactAlpha = FMath::Clamp(ChunkMassKg / SafeTargetKg, 0.25f, 1.0f);
		const float EmberRatio = (UpdatedPanMassKg > 0.01f)
			? FMath::Clamp(UpdatedPanMassKg / SafeTargetKg, 0.15f, 1.0f)
			: 0.0f;
		ScaleWellThermalLightComponent->TriggerThermalFlash(
			MoltenCfg.ScaleWellFlashIntensity * FMath::Lerp(0.55f, 1.0f, ImpactAlpha),
			MoltenCfg.ScaleWellMaxSteadyIntensity * EmberRatio);
	}

	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->NotifyPayloadLandedOnScale(PayloadActor);
		}
	}
}

void AScaleAssemblyActor::HandleChunkBouncedOffScale(AActor* PayloadActor, float /*ChunkMassKg*/, float UpdatedPanMassKg)
{
	if (IsValid(ScaleWellThermalLightComponent))
	{
		const FMoltenBounceLightConfig MoltenCfg = IsValid(PresentationData)
			? PresentationData->MoltenBounceConfig
			: FMoltenBounceLightConfig();
		const float SafeTargetKg = FMath::Max(1.0f, CachedSnapshot.TargetTitheMassKg);
		const float EmberRatio = (UpdatedPanMassKg > 0.01f)
			? FMath::Clamp(UpdatedPanMassKg / SafeTargetKg, 0.15f, 1.0f)
			: 0.0f;
		ScaleWellThermalLightComponent->SetSteadyEmberIntensity(MoltenCfg.ScaleWellMaxSteadyIntensity * EmberRatio);
	}

	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->NotifyPayloadBouncedOffScale(PayloadActor);
		}
	}
}

void AScaleAssemblyActor::HandleScaleContaminationDetected(AActor* ContaminatedActor)
{
	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->NotifyScaleContaminationDetected(ContaminatedActor);
		}
	}
}

void AScaleAssemblyActor::HandleScaleSettledChanged(bool bIsSettled, float SettledPanMassKg)
{
	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->NotifyScaleSettledStateChanged(bIsSettled, SettledPanMassKg);
		}
	}
	RefreshStatusBannerReadout();
}

void AScaleAssemblyActor::HandleChunkRoutedToTare(AActor* PayloadActor, float MassKg, bool /*bWasContaminated*/)
{
	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->NotifyPayloadDisposedToTare(PayloadActor, MassKg);
		}
	}
}
