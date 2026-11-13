// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/DirectorBoothActor.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Data/MonolithContractDataAsset.h"
#include "Data/ShiftProgressionDataAsset.h"
#include "Data/TareTithePresentationDataAsset.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float ShutterAlphaConvergenceEpsilon = 0.002f;
	constexpr int32 SingleLineBarkMaxChars = 32;

	void EnforceZeroCostPresentationFlags(UPrimitiveComponent* PrimComp)
	{
		if (!IsValid(PrimComp))
		{
			return;
		}
		PrimComp->PrimaryComponentTick.bCanEverTick = false;
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

ADirectorBoothActor::ADirectorBoothActor()
{
	// Tier 2: Strictly disabled while idle (0.0 ms); active ONLY during bounded shutter motion, 3.5s brief pulse/fade, or FaultResetDelay strobe.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	BoothRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BoothRoot"));
	SetRootComponent(BoothRoot);
	BoothRoot->PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* CubeMesh = CubeMeshFinder.Succeeded() ? CubeMeshFinder.Object : nullptr;
	BasicCubeMesh = CubeMesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMeshFinder.Succeeded())
	{
		BasicCylinderMesh = CylinderMeshFinder.Object;
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
		UnlitSignboardTextMaterial = UnlitTextMatFinder.Object;
	}

	auto ConfigurePanopticonMesh = [this, CubeMesh](
		UStaticMeshComponent* MeshComp,
		USceneComponent* ParentComp,
		const FVector& RelLocation,
		const FVector& RelScale3D,
		bool bHideFromLumenScene = false)
	{
		MeshComp->SetupAttachment(ParentComp);
		MeshComp->PrimaryComponentTick.bCanEverTick = false;
		MeshComp->SetSimulatePhysics(false);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComp->SetGenerateOverlapEvents(false);
		MeshComp->SetCastShadow(false);
		MeshComp->bReceivesDecals = false;
		if (bHideFromLumenScene)
		{
			MeshComp->SetAffectDistanceFieldLighting(false);
			MeshComp->SetAffectDynamicIndirectLighting(false);
			MeshComp->SetVisibleInRayTracing(false);
		}
		if (IsValid(CubeMesh))
		{
			MeshComp->SetStaticMesh(CubeMesh);
		}
		if (IsValid(BaseGreyboxShapeMaterial))
		{
			MeshComp->SetMaterial(0, BaseGreyboxShapeMaterial);
		}
		MeshComp->SetRelativeLocation(RelLocation);
		MeshComp->SetRelativeScale3D(RelScale3D);
	};

	// Recessed dark Panopticon observation slit interior spanning behind all 3 bays (World X=[444,452], Y=[-296,+296], Z=[214,380]).
	OverlookRecessBackingVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OverlookRecessBackingVisual"));
	ConfigurePanopticonMesh(
		OverlookRecessBackingVisual,
		BoothRoot,
		FVector(18.0, 0.0, 39.0),
		FVector(0.08, 5.92, 1.66),
		true);
	EnforceZeroCostPresentationFlags(OverlookRecessBackingVisual);

	// Phase 12B Diffused Frosted Overlook Backlight Cards seated strictly in front of SM_DirectorBoothFrame solid recessed cavities (>= 9.5 mm air gap).
	LeftWindowGlowCard = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftWindowGlowCard"));
	ConfigurePanopticonMesh(
		LeftWindowGlowCard,
		BoothRoot,
		FVector(8.80, -187.75, 6.05),
		FVector(0.006, 1.255, 0.388),
		true);
	EnforceZeroCostPresentationFlags(LeftWindowGlowCard);

	CenterWindowGlowCard = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CenterWindowGlowCard"));
	ConfigurePanopticonMesh(
		CenterWindowGlowCard,
		BoothRoot,
		FVector(4.40, 0.0, 10.50),
		FVector(0.006, 1.340, 0.316),
		true);
	EnforceZeroCostPresentationFlags(CenterWindowGlowCard);

	RightWindowGlowCard = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightWindowGlowCard"));
	ConfigurePanopticonMesh(
		RightWindowGlowCard,
		BoothRoot,
		FVector(8.80, 187.75, 6.05),
		FVector(0.006, 1.255, 0.388),
		true);
	EnforceZeroCostPresentationFlags(RightWindowGlowCard);

	// Phase 12B Batched Horizontal Industrial Louver Slats (1 ISM draw call across all 3 bays, strictly >= 14.6 mm behind shutter travel planes).
	PanopticonLouverSlatsISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PanopticonLouverSlatsISM"));
	PanopticonLouverSlatsISM->SetupAttachment(BoothRoot);
	EnforceZeroCostPresentationFlags(PanopticonLouverSlatsISM);
	if (IsValid(BasicCubeMesh))
	{
		PanopticonLouverSlatsISM->SetStaticMesh(BasicCubeMesh);
	}
	if (IsValid(BaseGreyboxShapeMaterial))
	{
		PanopticonLouverSlatsISM->SetMaterial(0, BaseGreyboxShapeMaterial);
	}

	// Phase 12B Exclusive Right-Bay Watching Director Silhouette + Telemetry Desk/Optics Stanchion (2 ISM batches, zero tick).
	DirectorSilhouetteCubesISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DirectorSilhouetteCubesISM"));
	DirectorSilhouetteCubesISM->SetupAttachment(BoothRoot);
	EnforceZeroCostPresentationFlags(DirectorSilhouetteCubesISM);
	if (IsValid(BasicCubeMesh))
	{
		DirectorSilhouetteCubesISM->SetStaticMesh(BasicCubeMesh);
	}
	if (IsValid(BaseGreyboxShapeMaterial))
	{
		DirectorSilhouetteCubesISM->SetMaterial(0, BaseGreyboxShapeMaterial);
	}

	DirectorSilhouetteCylindersISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DirectorSilhouetteCylindersISM"));
	DirectorSilhouetteCylindersISM->SetupAttachment(BoothRoot);
	EnforceZeroCostPresentationFlags(DirectorSilhouetteCylindersISM);
	if (IsValid(BasicCylinderMesh))
	{
		DirectorSilhouetteCylindersISM->SetStaticMesh(BasicCylinderMesh);
	}
	if (IsValid(BaseGreyboxShapeMaterial))
	{
		DirectorSilhouetteCylindersISM->SetMaterial(0, BaseGreyboxShapeMaterial);
	}

	// Towering Central Command Bay (World X=[406.6,441.4], Y=[-110.2,+110.2], Z=[212.0,382.0]; rises +106 cm above clamp housing).
	CenterBayPivot = CreateDefaultSubobject<USceneComponent>(TEXT("CenterBayPivot"));
	CenterBayPivot->SetupAttachment(BoothRoot);
	CenterBayPivot->PrimaryComponentTick.bCanEverTick = false;
	CenterBayPivot->SetRelativeLocation(FVector(-6.0, 0.0, 39.0));
	CenterBayPivot->SetRelativeScale3D(FVector(1.45, 1.16, 2.00));

	BoothFrameVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoothFrameVisual"));
	ConfigurePanopticonMesh(BoothFrameVisual, CenterBayPivot, FVector::ZeroVector, FVector(0.24, 1.90, 0.85));

	// Left Observation Bay (World X=[415,445], Y=[-300,-36], Z=[212,352]).
	LeftBayPivot = CreateDefaultSubobject<USceneComponent>(TEXT("LeftBayPivot"));
	LeftBayPivot->SetupAttachment(BoothRoot);
	LeftBayPivot->PrimaryComponentTick.bCanEverTick = false;
	LeftBayPivot->SetRelativeLocation(FVector(0.0, -168.0, 24.0));
	LeftBayPivot->SetRelativeScale3D(FVector(1.25, 1.3895, 1.647));

	LeftBoothFrameVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftBoothFrameVisual"));
	ConfigurePanopticonMesh(LeftBoothFrameVisual, LeftBayPivot, FVector::ZeroVector, FVector(0.24, 1.90, 0.85));

	// Right Observation Bay (World X=[415,445], Y=[+36,+300], Z=[212,352]).
	RightBayPivot = CreateDefaultSubobject<USceneComponent>(TEXT("RightBayPivot"));
	RightBayPivot->SetupAttachment(BoothRoot);
	RightBayPivot->PrimaryComponentTick.bCanEverTick = false;
	RightBayPivot->SetRelativeLocation(FVector(0.0, 168.0, 24.0));
	RightBayPivot->SetRelativeScale3D(FVector(1.25, 1.3895, 1.647));

	RightBoothFrameVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightBoothFrameVisual"));
	ConfigurePanopticonMesh(RightBoothFrameVisual, RightBayPivot, FVector::ZeroVector, FVector(0.24, 1.90, 0.85));

	// 3 Synchronized Giant Armored Blast Shutters (Center, Left, Right).
	BlastShutterMotionPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BlastShutterMotionPivot"));
	BlastShutterMotionPivot->SetupAttachment(BoothRoot);
	BlastShutterMotionPivot->PrimaryComponentTick.bCanEverTick = false;
	BlastShutterMotionPivot->SetRelativeLocation(ClosedShutterRelativeLocation);
	BlastShutterMotionPivot->SetRelativeScale3D(FVector(1.50, 1.08, 1.38));

	BlastShutterVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlastShutterVisual"));
	ConfigurePanopticonMesh(BlastShutterVisual, BlastShutterMotionPivot, FVector::ZeroVector, FVector(0.06, 1.65, 0.62));

	LeftShutterMotionPivot = CreateDefaultSubobject<USceneComponent>(TEXT("LeftShutterMotionPivot"));
	LeftShutterMotionPivot->SetupAttachment(BoothRoot);
	LeftShutterMotionPivot->PrimaryComponentTick.bCanEverTick = false;
	LeftShutterMotionPivot->SetRelativeLocation(ClosedLeftShutterRelativeLocation);
	LeftShutterMotionPivot->SetRelativeScale3D(FVector(1.35, 1.18, 1.22));

	LeftBlastShutterVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftBlastShutterVisual"));
	ConfigurePanopticonMesh(LeftBlastShutterVisual, LeftShutterMotionPivot, FVector::ZeroVector, FVector(0.06, 1.65, 0.62));

	RightShutterMotionPivot = CreateDefaultSubobject<USceneComponent>(TEXT("RightShutterMotionPivot"));
	RightShutterMotionPivot->SetupAttachment(BoothRoot);
	RightShutterMotionPivot->PrimaryComponentTick.bCanEverTick = false;
	RightShutterMotionPivot->SetRelativeLocation(ClosedRightShutterRelativeLocation);
	RightShutterMotionPivot->SetRelativeScale3D(FVector(1.35, 1.18, 1.22));

	RightBlastShutterVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightBlastShutterVisual"));
	ConfigurePanopticonMesh(RightBlastShutterVisual, RightShutterMotionPivot, FVector::ZeroVector, FVector(0.06, 1.65, 0.62));

	// 4 Heavy Vertical Armor Mullions (SM_ClampSpringHousing slot: Z=[0,+40] local -> World X=[398,442], Y=[-300,+300], Z=[212,376]).
	auto SetupMullionPivotAndMesh = [&](
		TObjectPtr<USceneComponent>& OutPivot,
		const TCHAR* PivotName,
		TObjectPtr<UStaticMeshComponent>& OutVisual,
		const TCHAR* VisualName,
		const FVector& PivotLoc,
		const FVector& PivotScale)
	{
		OutPivot = CreateDefaultSubobject<USceneComponent>(PivotName);
		OutPivot->SetupAttachment(BoothRoot);
		OutPivot->PrimaryComponentTick.bCanEverTick = false;
		OutPivot->SetRelativeLocation(PivotLoc);
		OutPivot->SetRelativeScale3D(PivotScale);

		OutVisual = CreateDefaultSubobject<UStaticMeshComponent>(VisualName);
		ConfigurePanopticonMesh(OutVisual, OutPivot, FVector(0.0, 0.0, 20.0), FVector(0.48, 0.48, 0.40));
	};

	SetupMullionPivotAndMesh(
		ArmorMullionOuterLeftPivot,
		TEXT("ArmorMullionOuterLeftPivot"),
		ArmorMullionOuterLeftVisual,
		TEXT("ArmorMullionOuterLeftVisual"),
		FVector(-6.0, -282.0, -46.0),
		FVector(0.75, 0.75, 3.65));

	SetupMullionPivotAndMesh(
		ArmorMullionInnerLeftPivot,
		TEXT("ArmorMullionInnerLeftPivot"),
		ArmorMullionInnerLeftVisual,
		TEXT("ArmorMullionInnerLeftVisual"),
		FVector(-12.0, -106.0, -46.0),
		FVector(0.833, 0.72, 4.10));

	SetupMullionPivotAndMesh(
		ArmorMullionInnerRightPivot,
		TEXT("ArmorMullionInnerRightPivot"),
		ArmorMullionInnerRightVisual,
		TEXT("ArmorMullionInnerRightVisual"),
		FVector(-12.0, 106.0, -46.0),
		FVector(0.833, 0.72, 4.10));

	SetupMullionPivotAndMesh(
		ArmorMullionOuterRightPivot,
		TEXT("ArmorMullionOuterRightPivot"),
		ArmorMullionOuterRightVisual,
		TEXT("ArmorMullionOuterRightVisual"),
		FVector(-6.0, 282.0, -46.0),
		FVector(0.75, 0.75, 3.65));

	// Strict render hygiene on suspended corner HUD plaques: zero collision, zero shadows, zero Lumen/SDF/RT occlusion.
	auto ConfigureSignboardPlate = [this, CubeMesh](UStaticMeshComponent* PlateComp, const FVector& Scale3D)
	{
		PlateComp->PrimaryComponentTick.bCanEverTick = false;
		PlateComp->SetSimulatePhysics(false);
		PlateComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PlateComp->SetGenerateOverlapEvents(false);
		PlateComp->SetCastShadow(false);
		PlateComp->SetAffectDistanceFieldLighting(false);
		PlateComp->SetAffectDynamicIndirectLighting(false);
		PlateComp->SetVisibleInRayTracing(false);
		PlateComp->bReceivesDecals = false;
		if (IsValid(CubeMesh))
		{
			PlateComp->SetStaticMesh(CubeMesh);
		}
		if (IsValid(BaseGreyboxShapeMaterial))
		{
			PlateComp->SetMaterial(0, BaseGreyboxShapeMaterial);
		}
		PlateComp->SetRelativeLocation(FVector::ZeroVector);
		PlateComp->SetRelativeScale3D(Scale3D);
		PlateComp->SetVisibility(false, false);
		PlateComp->SetHiddenInGame(true, false);
	};

	// Unscaled (1,1,1) root grouping Top-Left & Top-Right corner HUD terminal plaques.
	BoothSignRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BoothSignRoot"));
	BoothSignRoot->SetupAttachment(BoothRoot);
	BoothSignRoot->PrimaryComponentTick.bCanEverTick = false;
	BoothSignRoot->SetRelativeTransform(FTransform::Identity);
	BoothSignRoot->SetVisibility(false, false);
	BoothSignRoot->SetHiddenInGame(true, false);

	auto ConfigureWingText = [this](
		UTextRenderComponent* TextComp,
		USceneComponent* ParentPivot,
		const FVector& RelLocation,
		float WorldSize,
		const FColor& Color,
		const TCHAR* DefaultText)
	{
		TextComp->SetupAttachment(ParentPivot);
		TextComp->PrimaryComponentTick.bCanEverTick = false;
		TextComp->SetSimulatePhysics(false);
		TextComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TextComp->SetGenerateOverlapEvents(false);
		TextComp->SetCastShadow(false);
		TextComp->SetAffectDistanceFieldLighting(false);
		TextComp->SetAffectDynamicIndirectLighting(false);
		TextComp->SetVisibleInRayTracing(false);
		TextComp->bReceivesDecals = false;
		TextComp->SetRelativeLocation(RelLocation);
		TextComp->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
		TextComp->SetRelativeScale3D(FVector::OneVector);
		TextComp->SetHorizontalAlignment(EHTA_Center);
		TextComp->SetVerticalAlignment(EVRTA_TextCenter);
		TextComp->SetWorldSize(WorldSize);
		if (IsValid(UnlitSignboardTextMaterial))
		{
			TextComp->SetTextMaterial(UnlitSignboardTextMaterial);
		}
		TextComp->SetTextRenderColor(Color);
		TextComp->SetText(FText::FromString(DefaultText));
		TextComp->SetVisibility(false, false);
		TextComp->SetHiddenInGame(true, false);
	};

	// Top-Left Viewport Corner Plaque (World X=192, Y=-129.0, Z=237.5; pitched +12 deg down & yawed -14 deg inward toward camera).
	BoothHeaderPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BoothHeaderPivot"));
	BoothHeaderPivot->SetupAttachment(BoothSignRoot);
	BoothHeaderPivot->PrimaryComponentTick.bCanEverTick = false;
	BoothHeaderPivot->SetRelativeLocation(LeftWingPivotRelativeLocation);
	BoothHeaderPivot->SetRelativeRotation(LeftWingPivotRelativeRotation);
	BoothHeaderPivot->SetRelativeScale3D(FVector::OneVector);

	BoothHeaderBackingPlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoothHeaderBackingPlate"));
	BoothHeaderBackingPlate->SetupAttachment(BoothHeaderPivot);
	ConfigureSignboardPlate(BoothHeaderBackingPlate, WingSignboardPlateScale3D);

	BoothStencilText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("BoothStencilText"));
	ConfigureWingText(
		BoothStencilText,
		BoothHeaderPivot,
		FVector(-1.6, 0.0, 4.8),
		WingHeaderWorldSize,
		BoothHeaderCyanColor,
		TEXT("INDENTURE #409 // SHIFT 01/03 - CTR 01/06"));

	ContractDirectiveReadoutText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ContractDirectiveReadoutText"));
	ConfigureWingText(
		ContractDirectiveReadoutText,
		BoothHeaderPivot,
		FVector(-1.6, 0.0, 0.1),
		WingDirectiveWorldSize,
		BoothDirectiveGoldColor,
		TEXT("CUT 12.0 KG TO TITHE | STAMP [E]"));

	RequisitionReadoutText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("RequisitionReadoutText"));
	ConfigureWingText(
		RequisitionReadoutText,
		BoothHeaderPivot,
		FVector(-1.6, 0.0, -4.7),
		WingRequisitionWorldSize,
		BoothDirectiveGoldColor,
		TEXT("SCRIP: 00 | [1]HYD L0(2) [2]TARE L0(2) [3]SCAN L0(2)"));

	// Top-Right Viewport Corner Plaque (World X=192, Y=+129.0, Z=237.5; pitched +12 deg down & yawed +14 deg inward toward camera).
	BoothVerdictPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BoothVerdictPivot"));
	BoothVerdictPivot->SetupAttachment(BoothSignRoot);
	BoothVerdictPivot->PrimaryComponentTick.bCanEverTick = false;
	BoothVerdictPivot->SetRelativeLocation(RightWingPivotRelativeLocation);
	BoothVerdictPivot->SetRelativeRotation(RightWingPivotRelativeRotation);
	BoothVerdictPivot->SetRelativeScale3D(FVector::OneVector);

	BoothVerdictBackingPlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoothVerdictBackingPlate"));
	BoothVerdictBackingPlate->SetupAttachment(BoothVerdictPivot);
	ConfigureSignboardPlate(BoothVerdictBackingPlate, WingSignboardPlateScale3D);

	BoothRightHeaderText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("BoothRightHeaderText"));
	ConfigureWingText(
		BoothRightHeaderText,
		BoothVerdictPivot,
		FVector(-1.6, 0.0, 4.8),
		WingHeaderWorldSize,
		BoothHeaderCyanColor,
		TEXT("OFFICE OF THE DIRECTOR // OVERLOOK DECK"));

	VerdictBannerText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("VerdictBannerText"));
	ConfigureWingText(
		VerdictBannerText,
		BoothVerdictPivot,
		FVector(-1.6, 0.0, 0.2),
		WingVerdictWrappedWorldSize,
		BoothNormalSkyBlueColor,
		TEXT("#409: WORK OFF YOUR 3-SHIFT"));

	VerdictBannerSubText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("VerdictBannerSubText"));
	ConfigureWingText(
		VerdictBannerSubText,
		BoothVerdictPivot,
		FVector(-1.6, 0.0, -4.5),
		WingVerdictWrappedWorldSize,
		BoothNormalSkyBlueColor,
		TEXT("BOND. CARVE."));

	// 3 Recessed Cold Steel-Blue Interior Overlook Lamps (bCastShadows = false, AttenuationRadius <= 235 cm; zero side-wall/grate spill).
	auto ConfigureBayLight = [this](
		UPointLightComponent* LightComp,
		const FVector& RelLocation,
		float Intensity,
		float RadiusCm,
		float SourceLengthCm)
	{
		LightComp->SetupAttachment(BoothRoot);
		LightComp->PrimaryComponentTick.bCanEverTick = false;
		LightComp->SetRelativeLocation(RelLocation);
		LightComp->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
		LightComp->SetIntensity(Intensity);
		LightComp->SetAttenuationRadius(FMath::Min(235.0f, RadiusCm));
		LightComp->SetSourceRadius(14.0f);
		LightComp->SetSourceLength(SourceLengthCm);
		LightComp->SetCastShadows(false);
		LightComp->SetLightColor(ActiveInspectionLampColor);
	};

	StatusPointLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusPointLight"));
	ConfigureBayLight(StatusPointLight, FVector(-26.0, 0.0, 14.0), StatusLightIntensity * 0.36f, 175.0f, 84.0f);

	LeftBayPointLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("LeftBayPointLight"));
	ConfigureBayLight(LeftBayPointLight, FVector(-26.0, -184.0, 4.0), FlankBayLightIntensity * 0.36f, 160.0f, 76.0f);

	RightBayPointLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RightBayPointLight"));
	ConfigureBayLight(RightBayPointLight, FVector(-26.0, 184.0, 4.0), FlankBayLightIntensity * 0.36f, 160.0f, 76.0f);

	// Dedicated Spatial PA / Intercom Audio Component anchored on the Central Command Bay overlook deck.
	IntercomAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("IntercomAudioComponent"));
	IntercomAudioComponent->SetupAttachment(BoothRoot);
	IntercomAudioComponent->PrimaryComponentTick.bCanEverTick = false;
	IntercomAudioComponent->SetRelativeLocation(FVector(-22.0, 0.0, 32.0));
	IntercomAudioComponent->bAutoActivate = false;
	IntercomAudioComponent->bAutoDestroy = false;
}

void ADirectorBoothActor::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(StatusPointLight))
	{
		StatusPointLight->SetAttenuationRadius(FMath::Min(235.0f, StatusLightAttenuationRadiusCm));
		StatusPointLight->SetCastShadows(false);
	}
	if (IsValid(LeftBayPointLight))
	{
		LeftBayPointLight->SetAttenuationRadius(FMath::Min(235.0f, FlankBayLightAttenuationRadiusCm));
		LeftBayPointLight->SetCastShadows(false);
	}
	if (IsValid(RightBayPointLight))
	{
		RightBayPointLight->SetAttenuationRadius(FMath::Min(235.0f, FlankBayLightAttenuationRadiusCm));
		RightBayPointLight->SetCastShadows(false);
	}
	SetPanopticonLampState(ActiveInspectionLampColor, 1.0f);

	UTextRenderComponent* const CornerTexts[6] = {
		BoothStencilText,
		ContractDirectiveReadoutText,
		RequisitionReadoutText,
		BoothRightHeaderText,
		VerdictBannerText,
		VerdictBannerSubText
	};
	for (UTextRenderComponent* TextComp : CornerTexts)
	{
		if (IsValid(TextComp))
		{
			if (IsValid(UnlitSignboardTextMaterial))
			{
				TextComp->SetTextMaterial(UnlitSignboardTextMaterial);
			}
			TextComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			TextComp->SetCastShadow(false);
			TextComp->SetAffectDistanceFieldLighting(false);
			TextComp->SetAffectDynamicIndirectLighting(false);
			TextComp->SetVisibleInRayTracing(false);
		}
	}

	UStaticMeshComponent* const CornerPlates[2] = {
		BoothHeaderBackingPlate,
		BoothVerdictBackingPlate
	};
	for (UStaticMeshComponent* PlateComp : CornerPlates)
	{
		if (IsValid(PlateComp))
		{
			PlateComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PlateComp->SetCastShadow(false);
			PlateComp->SetAffectDistanceFieldLighting(false);
			PlateComp->SetAffectDynamicIndirectLighting(false);
			PlateComp->SetVisibleInRayTracing(false);
		}
	}

	if (IsValid(BaseGreyboxShapeMaterial))
	{
		auto ApplyBoothMaterial = [this](
			UStaticMeshComponent* TargetMesh,
			const FLinearColor& Color,
			float Roughness,
			float Metallic)
		{
			if (IsValid(TargetMesh))
			{
				if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseGreyboxShapeMaterial, this))
				{
					MID->SetVectorParameterValue(TEXT("BaseColorConst"), Color);
					MID->SetVectorParameterValue(TEXT("Color"), Color);
					MID->SetVectorParameterValue(TEXT("EmissiveColorConst"), FLinearColor::Black);
					MID->SetScalarParameterValue(TEXT("EmissiveScale"), 0.0f);
					MID->SetScalarParameterValue(TEXT("RoughnessConst"), Roughness);
					MID->SetScalarParameterValue(TEXT("MetallicConst"), Metallic);
					TargetMesh->SetMaterial(0, MID);
				}
			}
		};

		ApplyBoothMaterial(OverlookRecessBackingVisual, FLinearColor(0.006f, 0.008f, 0.012f, 1.0f), 0.92f, 0.22f);
		ApplyBoothMaterial(BoothFrameVisual, FLinearColor(0.028f, 0.031f, 0.038f, 1.0f), 0.78f, 0.35f);
		ApplyBoothMaterial(LeftBoothFrameVisual, FLinearColor(0.028f, 0.031f, 0.038f, 1.0f), 0.78f, 0.35f);
		ApplyBoothMaterial(RightBoothFrameVisual, FLinearColor(0.028f, 0.031f, 0.038f, 1.0f), 0.78f, 0.35f);
		ApplyBoothMaterial(BlastShutterVisual, FLinearColor(0.060f, 0.062f, 0.068f, 1.0f), 0.66f, 0.45f);
		ApplyBoothMaterial(LeftBlastShutterVisual, FLinearColor(0.060f, 0.062f, 0.068f, 1.0f), 0.66f, 0.45f);
		ApplyBoothMaterial(RightBlastShutterVisual, FLinearColor(0.060f, 0.062f, 0.068f, 1.0f), 0.66f, 0.45f);
		ApplyBoothMaterial(ArmorMullionOuterLeftVisual, FLinearColor(0.035f, 0.039f, 0.046f, 1.0f), 0.72f, 0.42f);
		ApplyBoothMaterial(ArmorMullionInnerLeftVisual, FLinearColor(0.035f, 0.039f, 0.046f, 1.0f), 0.72f, 0.42f);
		ApplyBoothMaterial(ArmorMullionInnerRightVisual, FLinearColor(0.035f, 0.039f, 0.046f, 1.0f), 0.72f, 0.42f);
		ApplyBoothMaterial(ArmorMullionOuterRightVisual, FLinearColor(0.035f, 0.039f, 0.046f, 1.0f), 0.72f, 0.42f);
		ApplyBoothMaterial(BoothHeaderBackingPlate, SignboardBackingColor, 0.99f, 0.0f);
		ApplyBoothMaterial(BoothVerdictBackingPlate, SignboardBackingColor, 0.99f, 0.0f);
	}

	SetShutterTargetOpenAlpha(0.0f, true);
	UpdateLegacySignboardVisibility();
	RebuildPanopticonSurveillanceVisuals();

	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->RegisterDirectorBoothStation(this);
			ShiftSubsystem->OnPresentationAssetChanged.AddUObject(this, &ADirectorBoothActor::ApplyPresentationData);
			if (const UTareTithePresentationDataAsset* ExistingPresentation = ShiftSubsystem->GetActivePresentationAsset())
			{
				ApplyPresentationData(ExistingPresentation);
			}
			ShiftSubsystem->OnNativeContractSnapshotUpdated.AddUObject(this, &ADirectorBoothActor::HandleContractSnapshotUpdated);
			ShiftSubsystem->OnShiftVerdict.AddUniqueDynamic(this, &ADirectorBoothActor::HandleShiftVerdict);
			ShiftSubsystem->OnShiftPhaseChanged.AddUObject(this, &ADirectorBoothActor::HandleShiftPhaseChanged);
			ShiftSubsystem->OnActiveContractLoaded.AddUObject(this, &ADirectorBoothActor::HandleActiveContractLoaded);

			CachedContractIndex = ShiftSubsystem->GetCurrentContractIndex();
			CachedTotalContracts = FMath::Max(1, ShiftSubsystem->GetTotalContractCount());
			CachedShiftPhase = ShiftSubsystem->GetCurrentPhase();
			const FContractRuntimeSnapshot InitialSnapshot = ShiftSubsystem->GetCurrentSnapshot();
			CachedSnapshot = InitialSnapshot;
			CachedDirectiveText = InitialSnapshot.ContractDirectiveText;
			CachedBriefText = InitialSnapshot.ContractBriefText;
			bIsBriefPulseActive = IsLegacyWorldSpaceReadoutEnabled() &&
				(CachedShiftPhase == EShiftPhase::ActiveInspection) &&
				InitialSnapshot.bIsContractBriefActive &&
				!CachedBriefText.IsEmpty();
			bIsBriefFadingOut = false;
			BriefPulseElapsedSeconds = 0.0f;
			BriefFadeElapsedSeconds = 0.0f;

			HandleShiftPhaseChanged(CachedShiftPhase);
			UpdateLegacySignboardVisibility();
			RefreshContractDirectiveWingReadout();
			RefreshRequisitionCornerReadout();
			RefreshDirectorIntercomWingReadout();
			TryTriggerIntercomAudioForSnapshot();
			UpdateBoothTickEnabled();
		}
	}
}

void ADirectorBoothActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(IntercomAudioComponent))
	{
		IntercomAudioComponent->Stop();
	}

	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->OnPresentationAssetChanged.RemoveAll(this);
			ShiftSubsystem->OnNativeContractSnapshotUpdated.RemoveAll(this);
			ShiftSubsystem->OnShiftVerdict.RemoveDynamic(this, &ADirectorBoothActor::HandleShiftVerdict);
			ShiftSubsystem->OnShiftPhaseChanged.RemoveAll(this);
			ShiftSubsystem->OnActiveContractLoaded.RemoveAll(this);
			ShiftSubsystem->UnregisterDirectorBoothStation(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ADirectorBoothActor::ApplyPresentationData(const UTareTithePresentationDataAsset* InPresentationData)
{
	PresentationData = InPresentationData;
	UpdateLegacySignboardVisibility();

	if (!IsValid(InPresentationData))
	{
		if (IsValid(BoothSignRoot))
		{
			BoothSignRoot->SetRelativeTransform(FTransform::Identity);
		}
		RebuildPanopticonSurveillanceVisuals();
		UpdateBoothTickEnabled();
		return;
	}

	auto ApplySlotToPanopticonMesh = [](const FMeshPresentationSlot& Slot, UStaticMeshComponent* TargetMesh)
	{
		if (IsValid(TargetMesh))
		{
			Slot.ApplyToVisualComponent(TargetMesh);
			TargetMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			TargetMesh->SetCastShadow(false);
		}
	};

	ApplySlotToPanopticonMesh(InPresentationData->DirectorBoothFrameMeshSlot, BoothFrameVisual);
	ApplySlotToPanopticonMesh(InPresentationData->DirectorBoothFrameMeshSlot, LeftBoothFrameVisual);
	ApplySlotToPanopticonMesh(InPresentationData->DirectorBoothFrameMeshSlot, RightBoothFrameVisual);

	ApplySlotToPanopticonMesh(InPresentationData->DirectorShutterMeshSlot, BlastShutterVisual);
	ApplySlotToPanopticonMesh(InPresentationData->DirectorShutterMeshSlot, LeftBlastShutterVisual);
	ApplySlotToPanopticonMesh(InPresentationData->DirectorShutterMeshSlot, RightBlastShutterVisual);

	ApplySlotToPanopticonMesh(InPresentationData->ClampSpringHousingMeshSlot, ArmorMullionOuterLeftVisual);
	ApplySlotToPanopticonMesh(InPresentationData->ClampSpringHousingMeshSlot, ArmorMullionInnerLeftVisual);
	ApplySlotToPanopticonMesh(InPresentationData->ClampSpringHousingMeshSlot, ArmorMullionInnerRightVisual);
	ApplySlotToPanopticonMesh(InPresentationData->ClampSpringHousingMeshSlot, ArmorMullionOuterRightVisual);

	RebuildPanopticonSurveillanceVisuals();

	if (IsValid(IntercomAudioComponent))
	{
		IntercomAudioComponent->AttenuationSettings = InPresentationData->DefaultSpatialAttenuation;
	}

	if (IsValid(BoothSignRoot))
	{
		FTransform UnscaledSignTransform = InPresentationData->BoothSignLocalOffset;
		UnscaledSignTransform.SetScale3D(FVector::OneVector);
		BoothSignRoot->SetRelativeTransform(UnscaledSignTransform);
	}

	if (IsLegacyWorldSpaceReadoutEnabled())
	{
		RefreshContractDirectiveWingReadout();
		RefreshRequisitionCornerReadout();
		RefreshDirectorIntercomWingReadout();
	}
	UpdateBoothTickEnabled();
}

void ADirectorBoothActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (DeltaSeconds <= 0.0f)
	{
		return;
	}

	if (!FMath::IsNearlyEqual(CurrentShutterOpenAlpha, TargetShutterOpenAlpha, ShutterAlphaConvergenceEpsilon))
	{
		const float ActiveDuration = bUseFastFaultShutterSlam
			? FMath::Max(0.04f, FaultShutterSlamDurationSeconds)
			: FMath::Max(0.05f, ShutterTransitionDurationSeconds);
		const float AlphaSpeedPerSec = 1.0f / ActiveDuration;

		CurrentShutterOpenAlpha = FMath::FInterpConstantTo(
			CurrentShutterOpenAlpha,
			TargetShutterOpenAlpha,
			DeltaSeconds,
			AlphaSpeedPerSec);

		if (FMath::IsNearlyEqual(CurrentShutterOpenAlpha, TargetShutterOpenAlpha, ShutterAlphaConvergenceEpsilon))
		{
			CurrentShutterOpenAlpha = TargetShutterOpenAlpha;
		}

		ApplyShutterPositionFromAlpha();
	}

	// Bounded Tier 2 9 Hz crimson alarm strobe across ALL 3 Panopticon overlook bays during FaultResetDelay (~1.8s), 0.0 ms otherwise.
	if (CachedShiftPhase == EShiftPhase::FaultResetDelay)
	{
		FaultStrobeElapsedSeconds += DeltaSeconds;
		const float SafeStrobeHz = FMath::Max(1.0f, FaultStrobeFrequencyHz);
		const float StrobeWave = 0.5f + 0.5f * FMath::Cos(FaultStrobeElapsedSeconds * SafeStrobeHz * 2.0f * UE_PI);
		const FPanopticonSurveillanceConfig SurvCfg = IsValid(PresentationData)
			? PresentationData->PanopticonSurveillance.GetSanitized()
			: FPanopticonSurveillanceConfig();
		SetPanopticonLampState(FaultSlamLampColor, FMath::Lerp(0.55f, SurvCfg.FaultStrobePeakMultiplier, StrobeWave));
		SetWindowGlowCardsState(
			FMath::Lerp(SurvCfg.FaultWindowCardBaseColor * 0.22f, SurvCfg.FaultWindowCardBaseColor, StrobeWave),
			FMath::Lerp(SurvCfg.FaultWindowCardEmissive * 0.18f, SurvCfg.FaultWindowCardEmissive, StrobeWave));
	}

	// Guard 2: Mutate ONLY SetTextRenderColor during bounded Tier 2 brief pulse/fade (NEVER SetText or SetWorldSize in Tick).
	if (bIsBriefPulseActive)
	{
		BriefPulseElapsedSeconds += DeltaSeconds;
		LastBriefDisplayColor = ComputeBriefPulseColor();
		if (IsValid(ContractDirectiveReadoutText))
		{
			ContractDirectiveReadoutText->SetTextRenderColor(LastBriefDisplayColor);
		}
	}
	else if (bIsBriefFadingOut)
	{
		BriefFadeElapsedSeconds += DeltaSeconds;
		const float SafeFadeDuration = FMath::Max(0.05f, BriefFadeOutDurationSeconds);
		const float FadeAlpha = FMath::Clamp(BriefFadeElapsedSeconds / SafeFadeDuration, 0.0f, 1.0f);
		const FLinearColor BlendedLinear = FMath::Lerp(
			FLinearColor(LastBriefDisplayColor),
			FLinearColor(BoothDirectiveGoldColor),
			FadeAlpha);
		const FColor FinalColor = (FadeAlpha >= 1.0f - KINDA_SMALL_NUMBER)
			? BoothDirectiveGoldColor
			: BlendedLinear.ToFColor(true);

		if (IsValid(ContractDirectiveReadoutText))
		{
			ContractDirectiveReadoutText->SetTextRenderColor(FinalColor);
		}

		if (FadeAlpha >= 1.0f - KINDA_SMALL_NUMBER)
		{
			bIsBriefFadingOut = false;
			BriefFadeElapsedSeconds = 0.0f;
		}
	}

	UpdateBoothTickEnabled();
}

void ADirectorBoothActor::HandleShiftVerdict(EShiftVerdictType Verdict, EContractFaultReason FaultReason)
{
	if (bIsBriefPulseActive || bIsBriefFadingOut)
	{
		bIsBriefPulseActive = false;
		bIsBriefFadingOut = false;
		BriefFadeElapsedSeconds = 0.0f;
		RefreshContractDirectiveWingReadout();
	}

	const FPanopticonSurveillanceConfig SurvCfg = IsValid(PresentationData)
		? PresentationData->PanopticonSurveillance.GetSanitized()
		: FPanopticonSurveillanceConfig();

	switch (Verdict)
	{
	case EShiftVerdictType::ContractFaultReset:
	{
		bUseFastFaultShutterSlam = true;
		FaultStrobeElapsedSeconds = 0.0f;
		SetShutterTargetOpenAlpha(0.0f, false);
		SetPanopticonLampState(FaultSlamLampColor, SurvCfg.FaultStrobePeakMultiplier);
		SetWindowGlowCardsState(SurvCfg.FaultWindowCardBaseColor, SurvCfg.FaultWindowCardEmissive);
		if (CachedSnapshot.DirectorIntercomLine.IsEmpty())
		{
			CachedSnapshot.DirectorIntercomLine = DescribeFaultReason(FaultReason);
		}
		RefreshDirectorIntercomWingReadout();
		break;
	}
	case EShiftVerdictType::DirectorsCutPassed:
	{
		bUseFastFaultShutterSlam = false;
		SetShutterTargetOpenAlpha(1.0f, false);
		SetPanopticonLampState(FLinearColor(1.00f, 0.78f, 0.16f, 1.0f), 1.45f);
		SetWindowGlowCardsState(SurvCfg.IdleWindowCardBaseColor, SurvCfg.IdleWindowCardEmissive);
		RefreshDirectorIntercomWingReadout();
		break;
	}
	case EShiftVerdictType::ContractPassed:
	{
		bUseFastFaultShutterSlam = false;
		SetShutterTargetOpenAlpha(1.0f, false);
		SetPanopticonLampState(VerdictApprovedLampColor, 1.0f);
		SetWindowGlowCardsState(SurvCfg.IdleWindowCardBaseColor, SurvCfg.IdleWindowCardEmissive);
		RefreshDirectorIntercomWingReadout();
		break;
	}
	case EShiftVerdictType::ShiftCompleted:
	{
		bUseFastFaultShutterSlam = false;
		SetShutterTargetOpenAlpha(0.0f, false);
		SetPanopticonLampState(VerdictApprovedLampColor, 1.0f);
		SetWindowGlowCardsState(SurvCfg.IdleWindowCardBaseColor, SurvCfg.IdleWindowCardEmissive);
		RefreshDirectorIntercomWingReadout();
		break;
	}
	case EShiftVerdictType::BondCleared:
	{
		bUseFastFaultShutterSlam = false;
		SetShutterTargetOpenAlpha(1.0f, false);
		SetPanopticonLampState(FLinearColor(1.00f, 0.82f, 0.22f, 1.0f), 1.50f);
		SetWindowGlowCardsState(SurvCfg.IdleWindowCardBaseColor, SurvCfg.IdleWindowCardEmissive);
		RefreshDirectorIntercomWingReadout();
		break;
	}
	default:
		break;
	}

	UpdateBoothTickEnabled();
}

void ADirectorBoothActor::HandleContractSnapshotUpdated(const FContractRuntimeSnapshot& Snapshot)
{
	CachedSnapshot = Snapshot;
	CachedContractIndex = Snapshot.ContractIndex;
	if (!Snapshot.ContractDirectiveText.IsEmpty())
	{
		CachedDirectiveText = Snapshot.ContractDirectiveText;
	}

	if (UWorld* World = GetWorld())
	{
		if (const UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			CachedTotalContracts = FMath::Max(1, ShiftSubsystem->GetTotalContractCount());
			CachedShiftPhase = ShiftSubsystem->GetCurrentPhase();
		}
	}

	const bool bShouldShowBrief = IsLegacyWorldSpaceReadoutEnabled() &&
		(CachedShiftPhase == EShiftPhase::ActiveInspection) &&
		Snapshot.bIsContractBriefActive &&
		!Snapshot.ContractBriefText.IsEmpty();

	if (bShouldShowBrief)
	{
		const bool bBriefJustStartedOrChanged = !bIsBriefPulseActive || (CachedBriefText != Snapshot.ContractBriefText);
		CachedBriefText = Snapshot.ContractBriefText;
		if (bBriefJustStartedOrChanged)
		{
			BriefPulseElapsedSeconds = 0.0f;
		}
		bIsBriefPulseActive = true;
		bIsBriefFadingOut = false;
		BriefFadeElapsedSeconds = 0.0f;
	}
	else if (bIsBriefPulseActive)
	{
		bIsBriefPulseActive = false;
		if (CachedShiftPhase == EShiftPhase::ActiveInspection)
		{
			bIsBriefFadingOut = true;
			BriefFadeElapsedSeconds = 0.0f;
		}
		else
		{
			bIsBriefFadingOut = false;
		}
	}

	RefreshContractDirectiveWingReadout();
	RefreshRequisitionCornerReadout();
	RefreshDirectorIntercomWingReadout();
	TryTriggerIntercomAudioForSnapshot();
	UpdateBoothTickEnabled();
}

void ADirectorBoothActor::HandleShiftPhaseChanged(EShiftPhase NewPhase)
{
	CachedShiftPhase = NewPhase;
	const FPanopticonSurveillanceConfig SurvCfg = IsValid(PresentationData)
		? PresentationData->PanopticonSurveillance.GetSanitized()
		: FPanopticonSurveillanceConfig();

	if (NewPhase == EShiftPhase::ActiveInspection)
	{
		bUseFastFaultShutterSlam = false;
		SetShutterTargetOpenAlpha(1.0f, false);
		SetPanopticonLampState(ActiveInspectionLampColor, 1.0f);
		SetWindowGlowCardsState(SurvCfg.IdleWindowCardBaseColor, SurvCfg.IdleWindowCardEmissive);
		RefreshDirectorIntercomWingReadout();
	}
	else if (NewPhase == EShiftPhase::FaultResetDelay)
	{
		bUseFastFaultShutterSlam = true;
		FaultStrobeElapsedSeconds = 0.0f;
		SetShutterTargetOpenAlpha(0.0f, false);
		SetPanopticonLampState(FaultSlamLampColor, SurvCfg.FaultStrobePeakMultiplier);
		SetWindowGlowCardsState(SurvCfg.FaultWindowCardBaseColor, SurvCfg.FaultWindowCardEmissive);
	}
	else if (bIsBriefPulseActive || bIsBriefFadingOut)
	{
		bIsBriefPulseActive = false;
		bIsBriefFadingOut = false;
		BriefFadeElapsedSeconds = 0.0f;
		RefreshContractDirectiveWingReadout();
	}

	UpdateBoothTickEnabled();
}

void ADirectorBoothActor::HandleActiveContractLoaded(
	const UMonolithContractDataAsset* Contract,
	const UShiftProgressionDataAsset* ShiftConfig,
	int32 ContractIndex)
{
	CachedContractIndex = ContractIndex;
	if (IsValid(ShiftConfig))
	{
		CachedTotalContracts = FMath::Max(1, ShiftConfig->OrderedContracts.Num());
	}
	if (IsValid(Contract) && !Contract->ContractDirectiveText.IsEmpty())
	{
		CachedDirectiveText = Contract->ContractDirectiveText;
	}

	RefreshContractDirectiveWingReadout();
	RefreshRequisitionCornerReadout();
	UpdateBoothTickEnabled();
}

bool ADirectorBoothActor::IsLegacyWorldSpaceReadoutEnabled() const
{
	return IsValid(PresentationData) && PresentationData->HudConfig.bEnableLegacyWorldSpaceReadouts;
}

void ADirectorBoothActor::UpdateLegacySignboardVisibility()
{
	const bool bShowLegacy = IsLegacyWorldSpaceReadoutEnabled();
	if (!bShowLegacy)
	{
		bIsBriefPulseActive = false;
		bIsBriefFadingOut = false;
	}

	USceneComponent* const SignRootsAndPlates[3] = {
		BoothSignRoot,
		BoothHeaderBackingPlate,
		BoothVerdictBackingPlate
	};
	for (USceneComponent* Comp : SignRootsAndPlates)
	{
		if (IsValid(Comp))
		{
			Comp->SetVisibility(bShowLegacy, false);
			Comp->SetHiddenInGame(!bShowLegacy, false);
		}
	}

	UTextRenderComponent* const CornerTexts[6] = {
		BoothStencilText,
		ContractDirectiveReadoutText,
		RequisitionReadoutText,
		BoothRightHeaderText,
		VerdictBannerText,
		VerdictBannerSubText
	};
	for (UTextRenderComponent* TextComp : CornerTexts)
	{
		if (IsValid(TextComp))
		{
			TextComp->SetVisibility(bShowLegacy, false);
			TextComp->SetHiddenInGame(!bShowLegacy, false);
		}
	}
}

void ADirectorBoothActor::RefreshContractDirectiveWingReadout()
{
	if (!IsLegacyWorldSpaceReadoutEnabled())
	{
		return;
	}

	const int32 DisplayContractNum = FMath::Clamp(CachedContractIndex + 1, 1, FMath::Max(1, CachedTotalContracts));
	const int32 DisplayTotalCount = FMath::Max(1, CachedTotalContracts);
	const int32 ShiftNum = FMath::Max(1, CachedSnapshot.CurrentShiftNumber);
	const int32 TargetShifts = FMath::Max(1, CachedSnapshot.TargetBondShifts);

	if (IsValid(BoothStencilText))
	{
		BoothStencilText->SetTextRenderColor(BoothHeaderCyanColor);
		const FString LeftHeader = (ShiftNum <= TargetShifts)
			? FString::Printf(TEXT("INDENTURE #409 // SHIFT %02d/%02d - CTR %02d/%02d"), ShiftNum, TargetShifts, DisplayContractNum, DisplayTotalCount)
			: FString::Printf(TEXT("INDENTURE #409 // OVT-%02d - CTR %02d/%02d"), ShiftNum, DisplayContractNum, DisplayTotalCount);
		ApplyFittedWingText(
			BoothStencilText,
			LeftHeader,
			WingHeaderWorldSize,
			WingDirectiveMinWorldSize,
			WingDirectiveMaxSafeWidthCm);
	}

	if (IsValid(ContractDirectiveReadoutText))
	{
		if (bIsBriefPulseActive && !CachedBriefText.IsEmpty())
		{
			LastBriefDisplayColor = ComputeBriefPulseColor();
			ContractDirectiveReadoutText->SetTextRenderColor(LastBriefDisplayColor);
			ApplyFittedWingText(
				ContractDirectiveReadoutText,
				CachedBriefText,
				WingDirectiveWorldSize,
				WingDirectiveMinWorldSize,
				WingDirectiveMaxSafeWidthCm);
		}
		else
		{
			const FString EffectiveDirective = CachedDirectiveText.IsEmpty()
				? TEXT("CUT TO QUOTA | STAMP [E]")
				: CachedDirectiveText;
			ContractDirectiveReadoutText->SetTextRenderColor(
				bIsBriefFadingOut ? LastBriefDisplayColor : BoothDirectiveGoldColor);
			ApplyFittedWingText(
				ContractDirectiveReadoutText,
				EffectiveDirective,
				WingDirectiveWorldSize,
				WingDirectiveMinWorldSize,
				WingDirectiveMaxSafeWidthCm);
		}
	}
}

void ADirectorBoothActor::RefreshRequisitionCornerReadout()
{
	if (!IsLegacyWorldSpaceReadoutEnabled() || !IsValid(RequisitionReadoutText))
	{
		return;
	}

	if (!CachedSnapshot.TransientRequisitionFeedback.IsEmpty())
	{
		const FColor FeedbackColor = CachedSnapshot.bLastRequisitionSucceeded
			? BoothApprovedEmeraldColor
			: BoothFaultBannerColor;
		RequisitionReadoutText->SetTextRenderColor(FeedbackColor);
		ApplyFittedWingText(
			RequisitionReadoutText,
			CachedSnapshot.TransientRequisitionFeedback,
			WingRequisitionWorldSize,
			WingDirectiveMinWorldSize,
			WingDirectiveMaxSafeWidthCm);
		return;
	}

	auto FormatUpgradeSlot = [](const TCHAR* KeyLabel, uint8 Level) -> FString
	{
		const int32 ClampedLv = FMath::Clamp(static_cast<int32>(Level), 0, 3);
		if (ClampedLv >= 3)
		{
			return FString::Printf(TEXT("%s L3(MAX)"), KeyLabel);
		}
		return FString::Printf(TEXT("%s L%d(%d)"), KeyLabel, ClampedLv, ClampedLv + 2);
	};

	const int32 Scrip = FMath::Max(0, CachedSnapshot.ScripBalance);
	const int32 MinAvailableCost = FMath::Min3(
		CachedSnapshot.ClampHydraulicsLevel < 3 ? static_cast<int32>(CachedSnapshot.ClampHydraulicsLevel) + 2 : 99,
		CachedSnapshot.TareHopperLevel < 3 ? static_cast<int32>(CachedSnapshot.TareHopperLevel) + 2 : 99,
		CachedSnapshot.VeinScannerDamperLevel < 3 ? static_cast<int32>(CachedSnapshot.VeinScannerDamperLevel) + 2 : 99);

	const FColor BarColor = (Scrip >= MinAvailableCost)
		? BoothApprovedEmeraldColor
		: BoothDirectiveGoldColor;

	const FString RequisitionBar = FString::Printf(
		TEXT("SCRIP: %02d | %s %s %s"),
		Scrip,
		*FormatUpgradeSlot(TEXT("[1]HYD"), CachedSnapshot.ClampHydraulicsLevel),
		*FormatUpgradeSlot(TEXT("[2]TARE"), CachedSnapshot.TareHopperLevel),
		*FormatUpgradeSlot(TEXT("[3]SCAN"), CachedSnapshot.VeinScannerDamperLevel));

	RequisitionReadoutText->SetTextRenderColor(BarColor);
	ApplyFittedWingText(
		RequisitionReadoutText,
		RequisitionBar,
		WingRequisitionWorldSize,
		WingDirectiveMinWorldSize,
		WingDirectiveMaxSafeWidthCm);
}

void ADirectorBoothActor::RefreshDirectorIntercomWingReadout()
{
	if (!IsLegacyWorldSpaceReadoutEnabled())
	{
		return;
	}

	if (IsValid(BoothRightHeaderText))
	{
		BoothRightHeaderText->SetTextRenderColor(BoothHeaderCyanColor);
		ApplyFittedWingText(
			BoothRightHeaderText,
			TEXT("OFFICE OF THE DIRECTOR // OVERLOOK DECK"),
			WingHeaderWorldSize,
			WingDirectiveMinWorldSize,
			WingDirectiveMaxSafeWidthCm);
	}

	if (!IsValid(VerdictBannerText))
	{
		return;
	}

	const FString IntercomLine = CachedSnapshot.DirectorIntercomLine.IsEmpty()
		? TEXT("#409: WORK OFF YOUR 3-SHIFT BOND. CARVE.")
		: CachedSnapshot.DirectorIntercomLine;

	FColor BannerColor = BoothNormalSkyBlueColor;
	if (!CachedSnapshot.TransientRequisitionFeedback.IsEmpty())
	{
		BannerColor = CachedSnapshot.bLastRequisitionSucceeded ? BoothApprovedEmeraldColor : BoothFaultBannerColor;
	}
	else if (CachedShiftPhase == EShiftPhase::FaultResetDelay)
	{
		BannerColor = BoothFaultBannerColor;
	}
	else if (CachedShiftPhase == EShiftPhase::ContractPassTransition)
	{
		BannerColor = CachedSnapshot.bLastStampWasDirectorsCut ? BoothDirectiveGoldColor : BoothApprovedEmeraldColor;
	}
	else if (CachedShiftPhase == EShiftPhase::BondCleared)
	{
		BannerColor = BoothDirectiveGoldColor;
	}
	else if (CachedShiftPhase == EShiftPhase::ShiftCompleted)
	{
		BannerColor = BoothApprovedEmeraldColor;
	}
	else if (CachedSnapshot.ClampStrainNormalized >= 0.78f)
	{
		BannerColor = BoothFaultBannerColor;
	}
	else if (CachedSnapshot.bScaleSettledForStamp && CachedSnapshot.bIsWithinDirectorsCutWindow)
	{
		BannerColor = BoothDirectiveGoldColor;
	}
	else if (CachedSnapshot.bScaleSettledForStamp)
	{
		BannerColor = BoothApprovedEmeraldColor;
	}

	FString LineOne;
	FString LineTwo;
	SplitIntercomBarkTwoLines(IntercomLine, LineOne, LineTwo);

	VerdictBannerText->SetTextRenderColor(BannerColor);
	if (LineTwo.IsEmpty())
	{
		VerdictBannerText->SetRelativeLocation(FVector(-1.6, 0.0, -2.0));
		ApplyFittedWingText(
			VerdictBannerText,
			LineOne,
			WingVerdictWorldSize,
			WingDirectiveMinWorldSize,
			WingDirectiveMaxSafeWidthCm);
		if (IsValid(VerdictBannerSubText))
		{
			VerdictBannerSubText->SetText(FText::GetEmpty());
		}
	}
	else
	{
		VerdictBannerText->SetRelativeLocation(FVector(-1.6, 0.0, 0.2));
		ApplyFittedWingText(
			VerdictBannerText,
			LineOne,
			WingVerdictWrappedWorldSize,
			WingDirectiveMinWorldSize,
			WingDirectiveMaxSafeWidthCm);

		if (IsValid(VerdictBannerSubText))
		{
			VerdictBannerSubText->SetTextRenderColor(BannerColor);
			VerdictBannerSubText->SetRelativeLocation(FVector(-1.6, 0.0, -4.5));
			ApplyFittedWingText(
				VerdictBannerSubText,
				LineTwo,
				WingVerdictWrappedWorldSize,
				WingDirectiveMinWorldSize,
				WingDirectiveMaxSafeWidthCm);
		}
	}
}

void ADirectorBoothActor::SetPanopticonLampState(const FLinearColor& LampColor, float IntensityMultiplier)
{
	const float ClampedMult = FMath::Max(0.0f, IntensityMultiplier);
	const float IdleScale = IsValid(PresentationData)
		? PresentationData->PanopticonSurveillance.GetSanitized().IdleBayLightIntensityScale
		: FPanopticonSurveillanceConfig().IdleBayLightIntensityScale;

	if (IsValid(StatusPointLight))
	{
		StatusPointLight->SetLightColor(LampColor);
		StatusPointLight->SetIntensity(StatusLightIntensity * IdleScale * ClampedMult);
	}
	if (IsValid(LeftBayPointLight))
	{
		LeftBayPointLight->SetLightColor(LampColor);
		LeftBayPointLight->SetIntensity(FlankBayLightIntensity * IdleScale * ClampedMult);
	}
	if (IsValid(RightBayPointLight))
	{
		RightBayPointLight->SetLightColor(LampColor);
		RightBayPointLight->SetIntensity(FlankBayLightIntensity * IdleScale * ClampedMult);
	}
}

int32 ADirectorBoothActor::GetLouverSlatInstanceCount() const
{
	return IsValid(PanopticonLouverSlatsISM) ? PanopticonLouverSlatsISM->GetInstanceCount() : 0;
}

int32 ADirectorBoothActor::GetDirectorSilhouetteInstanceCount() const
{
	const int32 CubeCount = IsValid(DirectorSilhouetteCubesISM) ? DirectorSilhouetteCubesISM->GetInstanceCount() : 0;
	const int32 CylCount = IsValid(DirectorSilhouetteCylindersISM) ? DirectorSilhouetteCylindersISM->GetInstanceCount() : 0;
	return CubeCount + CylCount;
}

void ADirectorBoothActor::SetWindowGlowCardsState(const FLinearColor& BaseColor, const FLinearColor& EmissiveColor)
{
	UMaterialInstanceDynamic* const CardMIDs[3] = {
		LeftWindowGlowCardMID,
		CenterWindowGlowCardMID,
		RightWindowGlowCardMID
	};
	for (UMaterialInstanceDynamic* MID : CardMIDs)
	{
		if (IsValid(MID))
		{
			MID->SetVectorParameterValue(TEXT("BaseColorConst"), BaseColor);
			MID->SetVectorParameterValue(TEXT("Color"), BaseColor);
			MID->SetVectorParameterValue(TEXT("EmissiveColorConst"), EmissiveColor);
			MID->SetScalarParameterValue(TEXT("EmissiveScale"), 1.0f);
		}
	}
}

void ADirectorBoothActor::RebuildPanopticonSurveillanceVisuals()
{
	EnforceZeroCostPresentationFlags(OverlookRecessBackingVisual);
	EnforceZeroCostPresentationFlags(LeftWindowGlowCard);
	EnforceZeroCostPresentationFlags(CenterWindowGlowCard);
	EnforceZeroCostPresentationFlags(RightWindowGlowCard);
	EnforceZeroCostPresentationFlags(PanopticonLouverSlatsISM);
	EnforceZeroCostPresentationFlags(DirectorSilhouetteCubesISM);
	EnforceZeroCostPresentationFlags(DirectorSilhouetteCylindersISM);

	const FPanopticonSurveillanceConfig SurvCfg = IsValid(PresentationData)
		? PresentationData->PanopticonSurveillance.GetSanitized()
		: FPanopticonSurveillanceConfig();

	// 1. Reposition bay lights forward of shutters (eliminating 0.95 cm specular singularity while keeping N.L > 0 on slammed shutters).
	if (IsValid(StatusPointLight))
	{
		StatusPointLight->SetRelativeLocation(FVector(SurvCfg.BayLightForwardLocationX, 0.0, SurvCfg.CenterBayLightLocationZ));
		StatusPointLight->SetAttenuationRadius(FMath::Min(235.0f, 175.0f));
		StatusPointLight->SetCastShadows(false);
	}
	if (IsValid(LeftBayPointLight))
	{
		LeftBayPointLight->SetRelativeLocation(FVector(SurvCfg.BayLightForwardLocationX, -SurvCfg.FlankBayLightLocationY, SurvCfg.FlankBayLightLocationZ));
		LeftBayPointLight->SetAttenuationRadius(FMath::Min(235.0f, 160.0f));
		LeftBayPointLight->SetCastShadows(false);
	}
	if (IsValid(RightBayPointLight))
	{
		RightBayPointLight->SetRelativeLocation(FVector(SurvCfg.BayLightForwardLocationX, SurvCfg.FlankBayLightLocationY, SurvCfg.FlankBayLightLocationZ));
		RightBayPointLight->SetAttenuationRadius(FMath::Min(235.0f, 160.0f));
		RightBayPointLight->SetCastShadows(false);
	}

	auto UnwrapBaseInterface = [](UMaterialInterface* InMat) -> UMaterialInterface*
	{
		if (const UMaterialInstanceDynamic* ExistingMID = Cast<UMaterialInstanceDynamic>(InMat))
		{
			return ExistingMID->Parent;
		}
		return InMat;
	};

	UMaterialInterface* FrameParentMat = nullptr;
	UMaterialInterface* ShutterParentMat = nullptr;
	if (IsValid(PresentationData))
	{
		if (PresentationData->DirectorBoothFrameMeshSlot.MaterialOverrides.Num() > 0)
		{
			FrameParentMat = UnwrapBaseInterface(PresentationData->DirectorBoothFrameMeshSlot.MaterialOverrides[0]);
		}
		if (PresentationData->DirectorShutterMeshSlot.MaterialOverrides.Num() > 0)
		{
			ShutterParentMat = UnwrapBaseInterface(PresentationData->DirectorShutterMeshSlot.MaterialOverrides[0]);
		}
	}
	if (!IsValid(FrameParentMat) && IsValid(BoothFrameVisual))
	{
		FrameParentMat = UnwrapBaseInterface(BoothFrameVisual->GetMaterial(0));
	}
	if (!IsValid(ShutterParentMat) && IsValid(BlastShutterVisual))
	{
		ShutterParentMat = UnwrapBaseInterface(BlastShutterVisual->GetMaterial(0));
	}

	// 2. Dampen booth frame & shutter specular/albedo via C++ MIDs (~45% idle Panopticon luminance suppression, zero .uasset edits).
	if (IsValid(FrameParentMat))
	{
		BoothFrameMID = UMaterialInstanceDynamic::Create(FrameParentMat, this);
		if (IsValid(BoothFrameMID))
		{
			const float FS = SurvCfg.BoothFrameAlbedoScale;
			BoothFrameMID->SetVectorParameterValue(TEXT("AlbedoTint"), FLinearColor(1.22f * FS, 1.30f * FS, 1.40f * FS, 1.0f));
			BoothFrameMID->SetVectorParameterValue(TEXT("ConductorMinF0"), FLinearColor(0.15f * FS, 0.17f * FS, 0.20f * FS, 1.0f));
			BoothFrameMID->SetScalarParameterValue(TEXT("SpecularScale"), SurvCfg.BoothFrameSpecularScale);
			BoothFrameMID->SetScalarParameterValue(TEXT("RoughnessMin"), 0.38f);
			BoothFrameMID->SetScalarParameterValue(TEXT("RoughnessMax"), 0.62f);

			UStaticMeshComponent* const FrameComps[3] = { BoothFrameVisual, LeftBoothFrameVisual, RightBoothFrameVisual };
			for (UStaticMeshComponent* Comp : FrameComps)
			{
				if (IsValid(Comp))
				{
					Comp->SetMaterial(0, BoothFrameMID);
				}
			}
		}
	}

	if (IsValid(ShutterParentMat))
	{
		DirectorShutterMID = UMaterialInstanceDynamic::Create(ShutterParentMat, this);
		if (IsValid(DirectorShutterMID))
		{
			const float SS = SurvCfg.BoothShutterAlbedoScale;
			DirectorShutterMID->SetVectorParameterValue(TEXT("AlbedoTint"), FLinearColor(1.18f * SS, 1.24f * SS, 1.32f * SS, 1.0f));
			DirectorShutterMID->SetVectorParameterValue(TEXT("ConductorMinF0"), FLinearColor(0.14f * SS, 0.16f * SS, 0.19f * SS, 1.0f));
			DirectorShutterMID->SetScalarParameterValue(TEXT("SpecularScale"), SurvCfg.BoothShutterSpecularScale);
			DirectorShutterMID->SetScalarParameterValue(TEXT("RoughnessMin"), 0.40f);
			DirectorShutterMID->SetScalarParameterValue(TEXT("RoughnessMax"), 0.64f);

			UStaticMeshComponent* const ShutterComps[3] = { BlastShutterVisual, LeftBlastShutterVisual, RightBlastShutterVisual };
			for (UStaticMeshComponent* Comp : ShutterComps)
			{
				if (IsValid(Comp))
				{
					Comp->SetMaterial(0, DirectorShutterMID);
				}
			}
		}
	}

	// 3. Diffused Frosted Glass Backlight Cards (Correction A cavity bounds + Correction C Roughness=0.82, Metallic=0.04).
	if (IsValid(BasicCubeMesh))
	{
		const FBox CubeBox = BasicCubeMesh->GetBoundingBox();
		const FVector CubeSize = CubeBox.GetSize();
		auto SizeGlowCard = [&](UStaticMeshComponent* CardComp, const FVector& CenterCm, const FVector& SizeCm)
		{
			if (!IsValid(CardComp))
			{
				return;
			}
			CardComp->SetStaticMesh(BasicCubeMesh);
			CardComp->SetRelativeLocation(CenterCm);
			CardComp->SetRelativeRotation(FRotator::ZeroRotator);
			CardComp->SetRelativeScale3D(FVector(
				SizeCm.X / FMath::Max(1.0, CubeSize.X),
				SizeCm.Y / FMath::Max(1.0, CubeSize.Y),
				SizeCm.Z / FMath::Max(1.0, CubeSize.Z)));
		};

		// Flank cavities: Y in +-[125.00, 250.50] (inside +-[124.5, 251.0]), Z in [-13.35, +25.45] (inside [-13.5, +25.6]), X in [+8.50, +9.10] (9.50 mm before +10.05 solid pane).
		SizeGlowCard(LeftWindowGlowCard, FVector(8.80, -187.75, 6.05), FVector(0.60, 125.50, 38.80));
		SizeGlowCard(RightWindowGlowCard, FVector(8.80, 187.75, 6.05), FVector(0.60, 125.50, 38.80));
		// Center cavity: Y in [-67.00, +67.00] (Width=134 <= 136), Z in [-5.30, +26.30] (inside [-5.5, +26.5]), X in [+4.10, +4.70] (9.58 mm before +5.658 solid pane).
		SizeGlowCard(CenterWindowGlowCard, FVector(4.40, 0.0, 10.50), FVector(0.60, 134.00, 31.60));
	}

	if (IsValid(BaseGreyboxShapeMaterial))
	{
		auto CreateFrostedCardMID = [&](UStaticMeshComponent* TargetCard) -> UMaterialInstanceDynamic*
		{
			if (!IsValid(TargetCard))
			{
				return nullptr;
			}
			UMaterialInstanceDynamic* CardMID = UMaterialInstanceDynamic::Create(BaseGreyboxShapeMaterial, this);
			if (IsValid(CardMID))
			{
				CardMID->SetScalarParameterValue(TEXT("RoughnessConst"), SurvCfg.WindowCardRoughness);
				CardMID->SetScalarParameterValue(TEXT("MetallicConst"), SurvCfg.WindowCardMetallic);
				CardMID->SetScalarParameterValue(TEXT("SpecularConst"), 0.04f);
				CardMID->SetVectorParameterValue(TEXT("BaseColorConst"), SurvCfg.IdleWindowCardBaseColor);
				CardMID->SetVectorParameterValue(TEXT("Color"), SurvCfg.IdleWindowCardBaseColor);
				CardMID->SetVectorParameterValue(TEXT("EmissiveColorConst"), SurvCfg.IdleWindowCardEmissive);
				CardMID->SetScalarParameterValue(TEXT("EmissiveScale"), 1.0f);
				TargetCard->SetMaterial(0, CardMID);
			}
			return CardMID;
		};

		LeftWindowGlowCardMID = CreateFrostedCardMID(LeftWindowGlowCard);
		CenterWindowGlowCardMID = CreateFrostedCardMID(CenterWindowGlowCard);
		RightWindowGlowCardMID = CreateFrostedCardMID(RightWindowGlowCard);
	}

	// 4. ISM-Cook-Safe Material Resolution for Louvers & Director Silhouette (M_StationPBR_Master instances have MATUSAGE_INSTANCED_STATIC_MESHES=True).
	UMaterialInterface* IsmPbrBaseMat = IsValid(ShutterParentMat) ? ShutterParentMat : FrameParentMat;
	if (!IsValid(IsmPbrBaseMat) && IsValid(PresentationData) && IsValid(PresentationData->DarkGunmetalMaterial))
	{
		IsmPbrBaseMat = PresentationData->DarkGunmetalMaterial.Get();
	}
	if (!IsValid(IsmPbrBaseMat))
	{
		IsmPbrBaseMat = BaseGreyboxShapeMaterial.Get();
	}

	if (IsValid(IsmPbrBaseMat))
	{
		LouverSlatMID = UMaterialInstanceDynamic::Create(IsmPbrBaseMat, this);
		if (IsValid(LouverSlatMID))
		{
			LouverSlatMID->SetVectorParameterValue(TEXT("AlbedoTint"), FLinearColor(0.020f, 0.026f, 0.034f, 1.0f));
			LouverSlatMID->SetVectorParameterValue(TEXT("ConductorMinF0"), FLinearColor(0.009f, 0.012f, 0.016f, 1.0f));
			LouverSlatMID->SetScalarParameterValue(TEXT("SpecularScale"), 0.06f);
			LouverSlatMID->SetScalarParameterValue(TEXT("MetallicScale"), 1.0f);
			LouverSlatMID->SetScalarParameterValue(TEXT("MetallicMin"), 0.72f);
			LouverSlatMID->SetScalarParameterValue(TEXT("MetallicMax"), 0.82f);
			LouverSlatMID->SetScalarParameterValue(TEXT("RoughnessMin"), 0.68f);
			LouverSlatMID->SetScalarParameterValue(TEXT("RoughnessMax"), 0.82f);
			LouverSlatMID->SetScalarParameterValue(TEXT("NormalStrength"), 0.15f);
			LouverSlatMID->SetVectorParameterValue(TEXT("BaseColorConst"), FLinearColor(0.015f, 0.019f, 0.025f, 1.0f));
			LouverSlatMID->SetScalarParameterValue(TEXT("RoughnessConst"), 0.76f);
			LouverSlatMID->SetScalarParameterValue(TEXT("MetallicConst"), 0.76f);
			LouverSlatMID->SetScalarParameterValue(TEXT("SpecularConst"), 0.06f);
			if (IsValid(PanopticonLouverSlatsISM))
			{
				PanopticonLouverSlatsISM->SetMaterial(0, LouverSlatMID);
			}
		}

		// Correction B: UE5 PBR F0 Specular Kill on DirectorSilhouetteMID (Specular<=0.02 + Metallic=0.92..0.95 + BaseColor/ConductorMinF0=(0.002,0.003,0.004) + Roughness=0.85..0.95).
		DirectorSilhouetteMID = UMaterialInstanceDynamic::Create(IsmPbrBaseMat, this);
		if (IsValid(DirectorSilhouetteMID))
		{
			const FLinearColor SootBlackF0(0.002f, 0.003f, 0.004f, 1.0f);
			DirectorSilhouetteMID->SetVectorParameterValue(TEXT("AlbedoTint"), SootBlackF0);
			DirectorSilhouetteMID->SetVectorParameterValue(TEXT("ConductorMinF0"), SootBlackF0);
			DirectorSilhouetteMID->SetScalarParameterValue(TEXT("SpecularScale"), 0.01f);
			DirectorSilhouetteMID->SetScalarParameterValue(TEXT("MetallicScale"), 1.0f);
			DirectorSilhouetteMID->SetScalarParameterValue(TEXT("MetallicMin"), 0.92f);
			DirectorSilhouetteMID->SetScalarParameterValue(TEXT("MetallicMax"), 0.95f);
			DirectorSilhouetteMID->SetScalarParameterValue(TEXT("RoughnessMin"), 0.85f);
			DirectorSilhouetteMID->SetScalarParameterValue(TEXT("RoughnessMax"), 0.85f);
			DirectorSilhouetteMID->SetScalarParameterValue(TEXT("NormalStrength"), 0.0f);
			DirectorSilhouetteMID->SetScalarParameterValue(TEXT("AOStrength"), 1.0f);
			DirectorSilhouetteMID->SetVectorParameterValue(TEXT("BaseColorConst"), SootBlackF0);
			DirectorSilhouetteMID->SetVectorParameterValue(TEXT("Color"), SootBlackF0);
			DirectorSilhouetteMID->SetScalarParameterValue(TEXT("SpecularConst"), 0.01f);
			DirectorSilhouetteMID->SetScalarParameterValue(TEXT("MetallicConst"), 0.92f);
			DirectorSilhouetteMID->SetScalarParameterValue(TEXT("RoughnessConst"), 0.95f);
			DirectorSilhouetteMID->SetScalarParameterValue(TEXT("EmissiveScale"), 0.0f);

			if (IsValid(DirectorSilhouetteCubesISM))
			{
				DirectorSilhouetteCubesISM->SetMaterial(0, DirectorSilhouetteMID);
			}
			if (IsValid(DirectorSilhouetteCylindersISM))
			{
				DirectorSilhouetteCylindersISM->SetMaterial(0, DirectorSilhouetteMID);
			}
		}
	}

	// 5. Populate PanopticonLouverSlatsISM (1 batched draw call across Left, Right, and Center bays; >= 14.6 mm behind shutter planes).
	double RightBayLouverRearMaxX = -4.95;
	if (IsValid(PanopticonLouverSlatsISM))
	{
		PanopticonLouverSlatsISM->ClearInstances();
		if (SurvCfg.bEnablePanopticonLouvers && IsValid(BasicCubeMesh))
		{
			PanopticonLouverSlatsISM->SetStaticMesh(BasicCubeMesh);
			const FBox CubeBox = BasicCubeMesh->GetBoundingBox();
			const FVector CubeSize = CubeBox.GetSize();
			const FVector CubeCenter = CubeBox.GetCenter();

			const FQuat SlatQuat = FRotator(SurvCfg.LouverPitchDegrees, 0.0f, 0.0f).Quaternion();
			const double PitchRad = FMath::DegreesToRadians(FMath::Abs(static_cast<double>(SurvCfg.LouverPitchDegrees)));
			const double DepthX = static_cast<double>(SurvCfg.LouverDepthCm);
			const double ThickZ = static_cast<double>(SurvCfg.LouverThicknessCm);
			const double SlatHalfX = 0.5 * (DepthX * FMath::Cos(PitchRad) + ThickZ * FMath::Sin(PitchRad));
			const double SlatHalfZ = 0.5 * (ThickZ * FMath::Cos(PitchRad) + DepthX * FMath::Sin(PitchRad));

			// Flank shutter rear is -4.95 cm; clamp flank slat center so front edge stays >= -4.05 cm (>= 9.0 mm gap).
			const double FlankSlatCenterX = FMath::Max(-2.20, -4.95 + 0.90 + SlatHalfX);
			RightBayLouverRearMaxX = FlankSlatCenterX + SlatHalfX;

			// Center shutter rear is -10.50 cm, Center glow card front is +4.10 cm.
			const double CenterSlatCenterX = FMath::Clamp(-7.20, -10.50 + 0.90 + SlatHalfX, 4.10 - 0.90 - SlatHalfX);

			auto AddBaySlats = [&](
				double CenterX,
				double CenterY,
				double WidthY,
				int32 SlatCount,
				double MinCavityZ,
				double MaxCavityZ,
				double PreferredBottomCenterZ)
			{
				const double SafeMinZ = MinCavityZ + SlatHalfZ + 0.15;
				const double SafeMaxZ = MaxCavityZ - SlatHalfZ - 0.15;
				if (SlatCount <= 0 || SafeMaxZ <= SafeMinZ)
				{
					return;
				}
				const FVector Scale(
					DepthX / FMath::Max(1.0, CubeSize.X),
					WidthY / FMath::Max(1.0, CubeSize.Y),
					ThickZ / FMath::Max(1.0, CubeSize.Z));

				const double DesiredSpan = static_cast<double>(FMath::Max(0, SlatCount - 1)) * static_cast<double>(SurvCfg.LouverVerticalPitchCm);
				const double MaxAvailSpan = SafeMaxZ - SafeMinZ;
				const double EffectiveStepZ = (SlatCount > 1)
					? (FMath::Min(DesiredSpan, MaxAvailSpan) / static_cast<double>(SlatCount - 1))
					: 0.0;
				const double StartZ = FMath::Clamp(PreferredBottomCenterZ, SafeMinZ, SafeMaxZ - (EffectiveStepZ * static_cast<double>(SlatCount - 1)));

				for (int32 Idx = 0; Idx < SlatCount; ++Idx)
				{
					const double SlatZ = FMath::Clamp(StartZ + (static_cast<double>(Idx) * EffectiveStepZ), SafeMinZ, SafeMaxZ);
					const FVector DesiredPos(CenterX, CenterY, SlatZ);
					const FVector PivotPos = DesiredPos - SlatQuat.RotateVector(Scale * CubeCenter);
					PanopticonLouverSlatsISM->AddInstance(FTransform(SlatQuat, PivotPos, Scale), false);
				}
			};

			// Flank bays: Width=125.0 cm -> Y in +-[125.25, 250.25] (inside +-[124.5, 251.0]), Z in [-13.5, +25.6].
			AddBaySlats(FlankSlatCenterX, -187.75, 125.0, SurvCfg.FlankLouverSlatCount, -13.50, 25.60, -11.20);
			AddBaySlats(FlankSlatCenterX, 187.75, 125.0, SurvCfg.FlankLouverSlatCount, -13.50, 25.60, -11.20);
			// Center bay: Width=134.0 cm -> Y in [-67.0, +67.0] (inside [-68.0, +68.0]), Z in [-5.5, +26.5].
			AddBaySlats(CenterSlatCenterX, 0.0, 134.0, SurvCfg.CenterLouverSlatCount, -5.50, 26.50, -3.20);
		}
	}

	// 6. Populate Exclusive Right-Bay Watching Director Silhouette + Telemetry Console & Optics Stanchion (Decision 1).
	if (IsValid(DirectorSilhouetteCubesISM))
	{
		DirectorSilhouetteCubesISM->ClearInstances();
	}
	if (IsValid(DirectorSilhouetteCylindersISM))
	{
		DirectorSilhouetteCylindersISM->ClearInstances();
	}

	if (SurvCfg.bEnableWatchingDirectorSilhouette && IsValid(BasicCubeMesh) && IsValid(BasicCylinderMesh) &&
		IsValid(DirectorSilhouetteCubesISM) && IsValid(DirectorSilhouetteCylindersISM))
	{
		DirectorSilhouetteCubesISM->SetStaticMesh(BasicCubeMesh);
		DirectorSilhouetteCylindersISM->SetStaticMesh(BasicCylinderMesh);

		// Strict >= 9.0 mm (>= 8.5 mm required) air gap behind Right Bay louver rear (-0.92 cm) and before RightWindowGlowCard front (+8.50 cm).
		const double SafeMinX = RightBayLouverRearMaxX + 0.92;
		constexpr double SafeMaxX = 8.50 - 0.92;
		constexpr double SafeMinY = 125.00;
		constexpr double SafeMaxY = 250.50;
		constexpr double SafeMinZ = -13.35;
		constexpr double SafeMaxZ = 25.30;

		auto AddClampedSilhouettePart = [&](
			UInstancedStaticMeshComponent* TargetISM,
			const UStaticMesh* SourceMesh,
			FVector DesiredCenter,
			FVector SizeCm,
			const FRotator& PartRot)
		{
			const FBox MeshBox = SourceMesh->GetBoundingBox();
			const FVector MeshSize = MeshBox.GetSize();
			const FVector MeshCenter = MeshBox.GetCenter();
			const FQuat Quat = PartRot.Quaternion();
			const FVector AxisX = Quat.GetAxisX();
			const FVector AxisY = Quat.GetAxisY();
			const FVector AxisZ = Quat.GetAxisZ();

			const double PocketHalfSpanX = FMath::Max(0.40, 0.5 * (SafeMaxX - SafeMinX) - 0.04);
			const double HalfXFromYZ = 0.5 * (FMath::Abs(AxisY.X) * SizeCm.Y + FMath::Abs(AxisZ.X) * SizeCm.Z);
			if (FMath::Abs(AxisX.X) > 1.0e-3)
			{
				const double MaxAllowedDepthX = FMath::Max(0.40, 2.0 * (PocketHalfSpanX - HalfXFromYZ) / FMath::Abs(AxisX.X));
				SizeCm.X = FMath::Min(SizeCm.X, MaxAllowedDepthX);
			}

			const FVector RotHalfExtents(
				0.5 * (FMath::Abs(AxisX.X) * SizeCm.X + FMath::Abs(AxisY.X) * SizeCm.Y + FMath::Abs(AxisZ.X) * SizeCm.Z),
				0.5 * (FMath::Abs(AxisX.Y) * SizeCm.X + FMath::Abs(AxisY.Y) * SizeCm.Y + FMath::Abs(AxisZ.Y) * SizeCm.Z),
				0.5 * (FMath::Abs(AxisX.Z) * SizeCm.X + FMath::Abs(AxisY.Z) * SizeCm.Y + FMath::Abs(AxisZ.Z) * SizeCm.Z));

			DesiredCenter.X = FMath::Clamp(DesiredCenter.X, SafeMinX + RotHalfExtents.X, SafeMaxX - RotHalfExtents.X);
			DesiredCenter.Y = FMath::Clamp(DesiredCenter.Y, SafeMinY + RotHalfExtents.Y, SafeMaxY - RotHalfExtents.Y);
			DesiredCenter.Z = FMath::Clamp(DesiredCenter.Z, SafeMinZ + RotHalfExtents.Z, SafeMaxZ - RotHalfExtents.Z);

			const FVector Scale(
				SizeCm.X / FMath::Max(1.0, MeshSize.X),
				SizeCm.Y / FMath::Max(1.0, MeshSize.Y),
				SizeCm.Z / FMath::Max(1.0, MeshSize.Z));
			const FVector PivotPos = DesiredCenter - Quat.RotateVector(Scale * MeshCenter);
			TargetISM->AddInstance(FTransform(Quat, PivotPos, Scale), false);
		};

		const double BaseY = static_cast<double>(SurvCfg.DirectorBayLocalY);
		const double S = static_cast<double>(SurvCfg.DirectorScale);
		const float InwardTurnDeg = FMath::UnwindDegrees(SurvCfg.DirectorInwardYawDegrees - (-180.0f));
		const FRotator FigRot(0.0f, InwardTurnDeg, 0.0f);
		const double MidX = 0.5 * (SafeMinX + SafeMaxX);
		const double WingStepX = 8.8 * S * FMath::Sin(FMath::DegreesToRadians(static_cast<double>(InwardTurnDeg))) * 0.5;

		// Overlook telemetry console plinth & sloped monitor hood along the lower Right-Bay sill.
		AddClampedSilhouettePart(DirectorSilhouetteCubesISM, BasicCubeMesh, FVector(MidX, BaseY, -10.80), FVector(1.60, 58.0 * S, 5.00), FRotator::ZeroRotator);
		AddClampedSilhouettePart(DirectorSilhouetteCubesISM, BasicCubeMesh, FVector(MidX - 0.10, BaseY + 22.0 * S, -6.90), FVector(1.60, 13.5 * S, 3.80), FRotator(12.0f, 0.0f, 0.0f));

		// Surveillance optics stanchion & dual-barrel scope aimed toward the inspection rig.
		AddClampedSilhouettePart(DirectorSilhouetteCylindersISM, BasicCylinderMesh, FVector(MidX, BaseY - 22.5 * S, -4.20), FVector(2.00, 2.40 * S, 9.20 * S), FRotator::ZeroRotator);
		AddClampedSilhouettePart(DirectorSilhouetteCylindersISM, BasicCylinderMesh, FVector(MidX - 0.15, BaseY - 22.5 * S, 0.90 * S), FVector(3.60 * S, 4.40 * S, 3.00), FRotator(90.0f, 0.0f, 0.0f));

		// Authoritative greatcoat torso, stepped broad shoulder wings (continuous -168 deg inward stance), upturned collar, cranium & visor cap.
		AddClampedSilhouettePart(DirectorSilhouetteCubesISM, BasicCubeMesh, FVector(MidX, BaseY, -5.60 * S), FVector(1.25, 27.5 * S, 11.20 * S), FigRot);
		AddClampedSilhouettePart(DirectorSilhouetteCubesISM, BasicCubeMesh, FVector(MidX + WingStepX, BaseY - 9.2 * S, 1.50 * S), FVector(1.25, 20.5 * S, 6.80 * S), FigRot);
		AddClampedSilhouettePart(DirectorSilhouetteCubesISM, BasicCubeMesh, FVector(MidX - WingStepX, BaseY + 9.2 * S, 1.50 * S), FVector(1.25, 20.5 * S, 6.80 * S), FigRot);
		AddClampedSilhouettePart(DirectorSilhouetteCubesISM, BasicCubeMesh, FVector(MidX + WingStepX, BaseY - 12.0 * S, 4.10 * S), FVector(1.20, 15.0 * S, 3.40 * S), FRotator(0.0f, InwardTurnDeg, 12.0f));
		AddClampedSilhouettePart(DirectorSilhouetteCubesISM, BasicCubeMesh, FVector(MidX - WingStepX, BaseY + 12.0 * S, 4.10 * S), FVector(1.20, 15.0 * S, 3.40 * S), FRotator(0.0f, InwardTurnDeg, -12.0f));
		AddClampedSilhouettePart(DirectorSilhouetteCubesISM, BasicCubeMesh, FVector(MidX, BaseY, 6.40 * S), FVector(1.30, 16.5 * S, 4.20 * S), FigRot);
		AddClampedSilhouettePart(DirectorSilhouetteCylindersISM, BasicCylinderMesh, FVector(MidX, BaseY, 11.10 * S), FVector(2.50, 11.4 * S, 7.40 * S), FigRot);
		AddClampedSilhouettePart(DirectorSilhouetteCubesISM, BasicCubeMesh, FVector(MidX - 0.20, BaseY, 14.70 * S), FVector(2.70, 13.8 * S, 2.30 * S), FRotator(-8.0f, InwardTurnDeg, 0.0f));
	}

	// 7. Re-synchronize bay lamp intensity & frosted window card state with current phase.
	if (CachedShiftPhase == EShiftPhase::FaultResetDelay)
	{
		SetPanopticonLampState(FaultSlamLampColor, SurvCfg.FaultStrobePeakMultiplier);
		SetWindowGlowCardsState(SurvCfg.FaultWindowCardBaseColor, SurvCfg.FaultWindowCardEmissive);
	}
	else
	{
		SetPanopticonLampState(ActiveInspectionLampColor, 1.0f);
		SetWindowGlowCardsState(SurvCfg.IdleWindowCardBaseColor, SurvCfg.IdleWindowCardEmissive);
	}
}

void ADirectorBoothActor::TryTriggerIntercomAudioForSnapshot()
{
	const FString IntercomLine = CachedSnapshot.DirectorIntercomLine.IsEmpty()
		? TEXT("#409: WORK OFF YOUR 3-SHIFT BOND. CARVE.")
		: CachedSnapshot.DirectorIntercomLine;

	if (IntercomLine.IsEmpty() || IntercomLine == LastSpokenIntercomLine)
	{
		return;
	}

	const bool bHasTransientReq = !CachedSnapshot.TransientRequisitionFeedback.IsEmpty();

	// Silent revert when transient requisition feedback expires back to the unchanged baseline contract intercom line.
	if (!bHasTransientReq && bWasShowingTransientRequisitionFeedback && IntercomLine == LastBaselineIntercomLine)
	{
		LastSpokenIntercomLine = IntercomLine;
		bWasShowingTransientRequisitionFeedback = false;
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(IntercomAudioComponent) || !IsValid(PresentationData))
	{
		LastSpokenIntercomLine = IntercomLine;
		if (!bHasTransientReq)
		{
			LastBaselineIntercomLine = IntercomLine;
		}
		bWasShowingTransientRequisitionFeedback = bHasTransientReq;
		return;
	}

	const double CurrentWorldTime = World->GetTimeSeconds();
	const double SafeDebounceSec = static_cast<double>(FMath::Max(0.10f, IntercomMinDebounceSeconds));
	if ((CurrentWorldTime - LastIntercomCueWorldTimeSeconds) < SafeDebounceSec)
	{
		++IntercomDebounceBlockedCount;
		LastSpokenIntercomLine = IntercomLine;
		if (!bHasTransientReq)
		{
			LastBaselineIntercomLine = IntercomLine;
		}
		bWasShowingTransientRequisitionFeedback = bHasTransientReq;
		return;
	}

	USoundBase* SelectedSound = nullptr;
	float VolumeMult = 0.92f;
	float PitchMult = 1.0f;
	float StartTimeSec = 0.0f;

	if (bHasTransientReq && !CachedSnapshot.bLastRequisitionSucceeded)
	{
		// Short sharp low-volume cabinet buzz & mic-cut click reject cue for INSUFFICIENT SCRIP / MAX LEVEL.
		SelectedSound = PresentationData->DirectorIntercomReprimandSound;
		VolumeMult = 0.44f;
		PitchMult = 1.30f;
		StartTimeSec = 0.48f;
	}
	else if (CachedShiftPhase == EShiftPhase::FaultResetDelay || CachedSnapshot.ClampStrainNormalized >= 0.78f)
	{
		SelectedSound = PresentationData->DirectorIntercomReprimandSound;
		VolumeMult = 1.0f;
		PitchMult = (CachedShiftPhase == EShiftPhase::FaultResetDelay) ? 1.0f : 1.08f;
		StartTimeSec = 0.0f;
	}
	else if (
		CachedShiftPhase == EShiftPhase::ContractPassTransition ||
		CachedShiftPhase == EShiftPhase::ShiftCompleted ||
		CachedShiftPhase == EShiftPhase::BondCleared ||
		(bHasTransientReq && CachedSnapshot.bLastRequisitionSucceeded))
	{
		SelectedSound = PresentationData->DirectorIntercomApproveSound;
		const bool bBoostedPrecision = CachedSnapshot.bLastStampWasDirectorsCut || (CachedShiftPhase == EShiftPhase::BondCleared);
		VolumeMult = bBoostedPrecision ? 1.16f : 0.95f;
		PitchMult = bBoostedPrecision ? 1.10f : 1.0f;
		StartTimeSec = 0.0f;
	}
	else
	{
		SelectedSound = PresentationData->DirectorIntercomDirectiveSound;
		VolumeMult = 0.90f;
		PitchMult = (CachedSnapshot.bScaleSettledForStamp && CachedSnapshot.bIsWithinDirectorsCutWindow) ? 1.06f : 1.0f;
		StartTimeSec = 0.0f;
	}

	LastSpokenIntercomLine = IntercomLine;
	if (!bHasTransientReq)
	{
		LastBaselineIntercomLine = IntercomLine;
	}
	bWasShowingTransientRequisitionFeedback = bHasTransientReq;

	if (!IsValid(SelectedSound))
	{
		return;
	}

	LastIntercomCueWorldTimeSeconds = CurrentWorldTime;
	LastTriggeredIntercomSound = SelectedSound;
	++IntercomAudioTriggerCount;

	IntercomAudioComponent->Stop();
	IntercomAudioComponent->AttenuationSettings = PresentationData->DefaultSpatialAttenuation;
	IntercomAudioComponent->SetSound(SelectedSound);
	IntercomAudioComponent->SetVolumeMultiplier(VolumeMult);
	IntercomAudioComponent->SetPitchMultiplier(PitchMult);
	IntercomAudioComponent->Play(StartTimeSec);
}

void ADirectorBoothActor::SetShutterTargetOpenAlpha(float InTargetOpenAlpha, bool bInstant)
{
	TargetShutterOpenAlpha = FMath::Clamp(InTargetOpenAlpha, 0.0f, 1.0f);

	if (bInstant)
	{
		CurrentShutterOpenAlpha = TargetShutterOpenAlpha;
		ApplyShutterPositionFromAlpha();
		UpdateBoothTickEnabled();
		return;
	}

	UpdateBoothTickEnabled();
}

void ADirectorBoothActor::ApplyShutterPositionFromAlpha()
{
	if (IsValid(BlastShutterMotionPivot))
	{
		BlastShutterMotionPivot->SetRelativeLocation(
			FMath::Lerp(ClosedShutterRelativeLocation, OpenShutterRelativeLocation, CurrentShutterOpenAlpha));
	}
	if (IsValid(LeftShutterMotionPivot))
	{
		LeftShutterMotionPivot->SetRelativeLocation(
			FMath::Lerp(ClosedLeftShutterRelativeLocation, OpenLeftShutterRelativeLocation, CurrentShutterOpenAlpha));
	}
	if (IsValid(RightShutterMotionPivot))
	{
		RightShutterMotionPivot->SetRelativeLocation(
			FMath::Lerp(ClosedRightShutterRelativeLocation, OpenRightShutterRelativeLocation, CurrentShutterOpenAlpha));
	}
}

void ADirectorBoothActor::UpdateBoothTickEnabled()
{
	const bool bNeedShutterTick = !FMath::IsNearlyEqual(
		CurrentShutterOpenAlpha,
		TargetShutterOpenAlpha,
		ShutterAlphaConvergenceEpsilon);
	const bool bNeedFaultStrobeTick = (CachedShiftPhase == EShiftPhase::FaultResetDelay);
	const bool bShouldTick = bNeedShutterTick || bIsBriefPulseActive || bIsBriefFadingOut || bNeedFaultStrobeTick;
	SetActorTickEnabled(bShouldTick);
}

FColor ADirectorBoothActor::ComputeBriefPulseColor() const
{
	const float SafeFreqHz = FMath::Max(0.1f, BriefPulseFrequencyHz);
	// Start near peak visibility at t=0 via cosine wave so the operational brief catches eye immediately on spawn.
	const float PulseAlpha = 0.5f + 0.5f * FMath::Cos(BriefPulseElapsedSeconds * SafeFreqHz * 2.0f * UE_PI);
	const FLinearColor BlendedLinear = FMath::Lerp(
		FLinearColor(BoothBriefAmberMinColor),
		FLinearColor(BoothBriefAmberPeakColor),
		PulseAlpha);
	return BlendedLinear.ToFColor(true);
}

void ADirectorBoothActor::SplitIntercomBarkTwoLines(const FString& InBark, FString& OutLineOne, FString& OutLineTwo)
{
	const FString Trimmed = InBark.TrimStartAndEnd();
	if (Trimmed.Len() <= SingleLineBarkMaxChars)
	{
		OutLineOne = Trimmed;
		OutLineTwo.Empty();
		return;
	}

	const int32 MidIndex = Trimmed.Len() / 2;
	int32 BestSplitIndex = INDEX_NONE;
	int32 BestDistFromMid = MAX_int32;

	for (int32 Idx = 4; Idx < Trimmed.Len() - 4; ++Idx)
	{
		if (Trimmed[Idx] == TEXT(' '))
		{
			const int32 Dist = FMath::Abs(Idx - MidIndex);
			if (Dist < BestDistFromMid)
			{
				BestDistFromMid = Dist;
				BestSplitIndex = Idx;
			}
		}
	}

	if (BestSplitIndex == INDEX_NONE)
	{
		OutLineOne = Trimmed;
		OutLineTwo.Empty();
		return;
	}

	OutLineOne = Trimmed.Left(BestSplitIndex).TrimStartAndEnd();
	OutLineTwo = Trimmed.Mid(BestSplitIndex + 1).TrimStartAndEnd();
}

void ADirectorBoothActor::ApplyFittedWingText(
	UTextRenderComponent* TextComp,
	const FString& NewText,
	float DefaultWorldSize,
	float MinWorldSize,
	float MaxSafeWidthCm)
{
	if (!IsValid(TextComp))
	{
		return;
	}

	TextComp->SetText(FText::FromString(NewText));

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

FString ADirectorBoothActor::DescribeFaultReason(EContractFaultReason FaultReason)
{
	switch (FaultReason)
	{
	case EContractFaultReason::VolatileDetonation:
		return TEXT("ISOTOPE DETONATED! DEBT PENALTY LOGGED!");
	case EContractFaultReason::ClampStructuralSnap:
		return TEXT("YOU SHEARED MY CLAMP! BALANCE YOUR CUTS!");
	case EContractFaultReason::ScaleContaminated:
		return TEXT("CONTAMINATED TITHE! DUMP VEINS TO TARE!");
	case EContractFaultReason::QuotaOvershoot:
		return TEXT("OVERWEIGHT SLOP! READ YOUR TOLERANCE!");
	case EContractFaultReason::TareAllowanceExceeded:
		return TEXT("TARE HOPPER CHOKED! STOP WASTING ORE!");
	case EContractFaultReason::MathematicallyUnreachable:
		return TEXT("CORE RUINED! NOT ENOUGH MASS FOR QUOTA!");
	case EContractFaultReason::ManualOperatorReset:
		return TEXT("CELL PURGED BY #409. NO SCRIP ISSUED.");
	default:
		return TEXT("INSPECTION FAULT! RESETTING CELL 09!");
	}
}
