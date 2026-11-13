// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/DirectorBoothActor.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
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
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float ShutterAlphaConvergenceEpsilon = 0.002f;
	constexpr int32 SingleLineBarkMaxChars = 32;
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
	ConfigureBayLight(StatusPointLight, FVector(-18.0, 0.0, 28.0), StatusLightIntensity, StatusLightAttenuationRadiusCm, 84.0f);

	LeftBayPointLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("LeftBayPointLight"));
	ConfigureBayLight(LeftBayPointLight, FVector(-14.0, -155.0, 22.0), FlankBayLightIntensity, FlankBayLightAttenuationRadiusCm, 76.0f);

	RightBayPointLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RightBayPointLight"));
	ConfigureBayLight(RightBayPointLight, FVector(-14.0, 155.0, 22.0), FlankBayLightIntensity, FlankBayLightAttenuationRadiusCm, 76.0f);

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
		SetPanopticonLampState(FaultSlamLampColor, FMath::Lerp(0.22f, 2.35f, StrobeWave));
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

	switch (Verdict)
	{
	case EShiftVerdictType::ContractFaultReset:
	{
		bUseFastFaultShutterSlam = true;
		FaultStrobeElapsedSeconds = 0.0f;
		SetShutterTargetOpenAlpha(0.0f, false);
		SetPanopticonLampState(FaultSlamLampColor, 2.25f);
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
		RefreshDirectorIntercomWingReadout();
		break;
	}
	case EShiftVerdictType::ContractPassed:
	{
		bUseFastFaultShutterSlam = false;
		SetShutterTargetOpenAlpha(1.0f, false);
		SetPanopticonLampState(VerdictApprovedLampColor, 1.0f);
		RefreshDirectorIntercomWingReadout();
		break;
	}
	case EShiftVerdictType::ShiftCompleted:
	{
		bUseFastFaultShutterSlam = false;
		SetShutterTargetOpenAlpha(0.0f, false);
		SetPanopticonLampState(VerdictApprovedLampColor, 1.0f);
		RefreshDirectorIntercomWingReadout();
		break;
	}
	case EShiftVerdictType::BondCleared:
	{
		bUseFastFaultShutterSlam = false;
		SetShutterTargetOpenAlpha(1.0f, false);
		SetPanopticonLampState(FLinearColor(1.00f, 0.82f, 0.22f, 1.0f), 1.50f);
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

	if (NewPhase == EShiftPhase::ActiveInspection)
	{
		bUseFastFaultShutterSlam = false;
		SetShutterTargetOpenAlpha(1.0f, false);
		SetPanopticonLampState(ActiveInspectionLampColor, 1.0f);
		RefreshDirectorIntercomWingReadout();
	}
	else if (NewPhase == EShiftPhase::FaultResetDelay)
	{
		bUseFastFaultShutterSlam = true;
		FaultStrobeElapsedSeconds = 0.0f;
		SetShutterTargetOpenAlpha(0.0f, false);
		SetPanopticonLampState(FaultSlamLampColor, 2.25f);
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
	if (IsValid(StatusPointLight))
	{
		StatusPointLight->SetLightColor(LampColor);
		StatusPointLight->SetIntensity(StatusLightIntensity * ClampedMult);
	}
	if (IsValid(LeftBayPointLight))
	{
		LeftBayPointLight->SetLightColor(LampColor);
		LeftBayPointLight->SetIntensity(FlankBayLightIntensity * ClampedMult);
	}
	if (IsValid(RightBayPointLight))
	{
		RightBayPointLight->SetLightColor(LampColor);
		RightBayPointLight->SetIntensity(FlankBayLightIntensity * ClampedMult);
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
