// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/ScaleAssemblyActor.h"
#include "Components/BoxComponent.h"
#include "Components/DiverterGateComponent.h"
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
#include "Subsystems/InspectionShiftSubsystem.h"
#include "UObject/ConstructorHelpers.h"

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
	LeftHopperGuideVisual->SetRelativeScale3D(FVector(0.84, 0.03, 0.80));

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
	TareChuteVisual->SetRelativeLocation(FVector(0.0, 86.0, 4.0));
	TareChuteVisual->SetRelativeScale3D(FVector(0.86, 0.84, 0.08));

	TareChuteTriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TareChuteTriggerBox"));
	TareChuteTriggerBox->SetupAttachment(ScaleRoot);
	TareChuteTriggerBox->PrimaryComponentTick.bCanEverTick = false;
	TareChuteTriggerBox->SetRelativeScale3D(FVector::OneVector);
	TareChuteTriggerBox->SetRelativeLocation(FVector(0.0, 86.0, 36.0));
	TareChuteTriggerBox->InitBoxExtent(FVector(58.0, 49.0, 52.0));

	ScaleWeighingComponent = CreateDefaultSubobject<UScaleWeighingComponent>(TEXT("ScaleWeighingComponent"));
	DiverterGateComponent = CreateDefaultSubobject<UDiverterGateComponent>(TEXT("DiverterGateComponent"));

	// Front-apron console bezel mounted strictly below PanRimFrontBox top (+37.0 cm) and tilted 10 deg upward toward camera.
	ReadoutConsolePivot = CreateDefaultSubobject<USceneComponent>(TEXT("ReadoutConsolePivot"));
	ReadoutConsolePivot->SetupAttachment(ScaleRoot);
	ReadoutConsolePivot->PrimaryComponentTick.bCanEverTick = false;
	ReadoutConsolePivot->SetRelativeLocation(FVector(-43.5, 0.0, 18.2));
	ReadoutConsolePivot->SetRelativeRotation(FRotator(-10.0f, 0.0f, 0.0f));
	ReadoutConsolePivot->SetRelativeScale3D(FVector::OneVector);

	// Unscaled (1, 1, 1) grouping root translated/rotated by ScaleConsoleLocalOffset so custom mesh housings never squash text scale.
	ScaleConsoleRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ScaleConsoleRoot"));
	ScaleConsoleRoot->SetupAttachment(ReadoutConsolePivot);
	ScaleConsoleRoot->PrimaryComponentTick.bCanEverTick = false;
	ScaleConsoleRoot->SetRelativeTransform(FTransform::Identity);

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
	ReadoutBackingPlate->SetRelativeScale3D(FVector(0.016, 3.16, 0.37));

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
	};

	HeaderStencilText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("HeaderStencilText"));
	HeaderStencilText->SetupAttachment(ScaleConsoleRoot);
	ConfigureReadoutText(HeaderStencilText, FVector(-2.0, 0.0, 14.8), 5.4f, ReadoutHeaderCyanColor, TEXT("DIRECTOR'S CUT"));

	ContractCodeText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ContractCodeText"));
	ContractCodeText->SetupAttachment(ScaleConsoleRoot);
	ConfigureReadoutText(
		ContractCodeText,
		FVector(-2.0, 0.0, 9.6),
		4.6f,
		ReadoutHeaderCyanColor,
		TEXT("CONTRACT 01/06 | TARGET: 12.00 KG [10.75 - 13.25]"));

	StatusBannerText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusBannerText"));
	StatusBannerText->SetupAttachment(ScaleConsoleRoot);
	ConfigureReadoutText(
		StatusBannerText,
		FVector(-2.0, 0.0, 3.6),
		6.0f,
		ReadoutNormalSkyBlueColor,
		TEXT("HOLD [LMB] TO SLICE MONOLITH INTO TITHE"));

	MassReadoutText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("MassReadoutText"));
	MassReadoutText->SetupAttachment(ScaleConsoleRoot);
	ConfigureReadoutText(MassReadoutText, FVector(-2.0, 0.0, -3.8), 9.4f, ReadoutGoldColor, TEXT("00.00 / 12.00 KG"));

	TareReadoutText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TareReadoutText"));
	TareReadoutText->SetupAttachment(ScaleConsoleRoot);
	ConfigureReadoutText(TareReadoutText, FVector(-2.0, -58.0, -10.6), 5.6f, ReadoutGoldColor, TEXT("TARE: 0.0 / 14.0 KG"));

	RouteReadoutText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("RouteReadoutText"));
	RouteReadoutText->SetupAttachment(ScaleConsoleRoot);
	ConfigureReadoutText(RouteReadoutText, FVector(-2.0, 58.0, -10.6), 5.6f, ReadoutHeaderCyanColor, TEXT("ROUTE: TITHE [SPACE]"));

	ControlStencilText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ControlStencilText"));
	ControlStencilText->SetupAttachment(ScaleConsoleRoot);
	ConfigureReadoutText(
		ControlStencilText,
		FVector(-2.0, 0.0, -15.6),
		4.1f,
		ReadoutControlStencilColor,
		TEXT("SCROLL: ROTATE | RMB: BEVEL | [SPACE]: TITHE/TARE | [E]: STAMP | HOLD [R]: RESET"));
}

void AScaleAssemblyActor::BeginPlay()
{
	Super::BeginPlay();

	InitializeGreyboxVisualMaterials();
	EnforcePanTriggerGeometryAndCollision();

	if (IsValid(ScaleWeighingComponent))
	{
		ScaleWeighingComponent->SetPanPlatformComponent(PanMotionRoot);
		ScaleWeighingComponent->SetPanWellBoundsGeometry(
			FVector2D(PanTriggerBoxExtentCm.X, PanTriggerBoxExtentCm.Y),
			PanDeckTopLocalZCm - 6.0f,
			PanDeckTopLocalZCm + 135.0f);
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
		DiverterGateComponent->OnRouteChanged.AddUObject(this, &AScaleAssemblyActor::HandleDiverterRouteChanged);
		DiverterGateComponent->OnRouteSlewCompleted.AddUObject(this, &AScaleAssemblyActor::HandleDiverterRouteSlewCompleted);
		DiverterGateComponent->OnChunkRoutedToTare.AddUObject(this, &AScaleAssemblyActor::HandleChunkRoutedToTare);
		DiverterGateComponent->OnTelemetryImpulse.AddUObject(this, &AScaleAssemblyActor::HandleChildTelemetryImpulse);
	}

	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->RegisterTelemetryPublisher(this, this);
			ShiftSubsystem->OnContractSnapshotUpdated.AddUniqueDynamic(this, &AScaleAssemblyActor::HandleContractSnapshotUpdated);
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
			ShiftSubsystem->OnContractSnapshotUpdated.RemoveDynamic(this, &AScaleAssemblyActor::HandleContractSnapshotUpdated);
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

void AScaleAssemblyActor::ConfigureForContract(const UMonolithContractDataAsset* Contract, EDiverterRoute InitialRoute)
{
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
			CachedContractIndex = ShiftSubsystem->GetCurrentContractIndex();
			CachedTotalContracts = FMath::Max(1, ShiftSubsystem->GetTotalContractCount());
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

	if (IsValid(ScaleConsoleRoot))
	{
		ScaleConsoleRoot->SetRelativeTransform(
			IsValid(InPresentationData) ? InPresentationData->ScaleConsoleLocalOffset : FTransform::Identity);
	}

	if (!IsValid(InPresentationData))
	{
		return;
	}

	InPresentationData->ScaleBaseMeshSlot.ApplyToVisualComponent(ScaleBaseVisual);
	InPresentationData->ScalePanMeshSlot.ApplyToVisualComponent(PanPlatformVisual);
	InPresentationData->DiverterFlapperMeshSlot.ApplyToVisualComponent(DiverterFlapperVisual);
	InPresentationData->TareChuteMeshSlot.ApplyToVisualComponent(TareChuteVisual);
}

void AScaleAssemblyActor::HandleContractSnapshotUpdated(const FContractRuntimeSnapshot& Snapshot)
{
	CachedSnapshot = Snapshot;
	CachedContractIndex = Snapshot.ContractIndex;

	if (UWorld* World = GetWorld())
	{
		if (const UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			CachedTotalContracts = FMath::Max(1, ShiftSubsystem->GetTotalContractCount());
		}
	}

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
	UStaticMeshComponent* const PureVisualMeshes[6] = {
		ScaleBaseVisual,
		LeftHopperGuideVisual,
		PanPlatformVisual,
		DiverterFlapperVisual,
		TareChuteVisual,
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

void AScaleAssemblyActor::RefreshContractHeaderReadout()
{
	if (!IsValid(ContractCodeText))
	{
		return;
	}

	const int32 DisplayIndex = FMath::Clamp(CachedContractIndex + 1, 1, FMath::Max(1, CachedTotalContracts));
	const int32 DisplayTotal = FMath::Max(1, CachedTotalContracts);
	const float TargetKg = FMath::Max(0.0f, CachedSnapshot.TargetTitheMassKg);
	const float MinValidKg = FMath::Max(0.0f, TargetKg - CachedSnapshot.QuotaToleranceKg);
	const float MaxValidKg = TargetKg + CachedSnapshot.QuotaToleranceKg;

	const FString HeaderLine = FString::Printf(
		TEXT("CONTRACT %02d/%02d | TARGET: %.2f KG [%.2f - %.2f]"),
		DisplayIndex,
		DisplayTotal,
		TargetKg,
		MinValidKg,
		MaxValidKg);

	ContractCodeText->SetText(FText::FromString(HeaderLine));
	ContractCodeText->SetTextRenderColor(ReadoutHeaderCyanColor);
	if (IsValid(HeaderStencilText))
	{
		HeaderStencilText->SetTextRenderColor(ReadoutHeaderCyanColor);
	}
	if (IsValid(ControlStencilText))
	{
		ControlStencilText->SetTextRenderColor(ReadoutControlStencilColor);
	}
}

void AScaleAssemblyActor::RefreshStatusBannerReadout()
{
	if (!IsValid(StatusBannerText))
	{
		return;
	}

	if (bHasLatchedVerdictBanner ||
		CachedShiftPhase == EShiftPhase::FaultResetDelay ||
		CachedShiftPhase == EShiftPhase::ContractPassTransition ||
		CachedShiftPhase == EShiftPhase::ShiftCompleted)
	{
		if (LatchedVerdict == EShiftVerdictType::ContractFaultReset || CachedShiftPhase == EShiftPhase::FaultResetDelay)
		{
			StatusBannerText->SetTextRenderColor(ReadoutFaultBannerColor);
			StatusBannerText->SetText(FText::FromString(FormatFaultReasonBanner(LatchedFaultReason)));
			return;
		}
		if (LatchedVerdict == EShiftVerdictType::ShiftCompleted || CachedShiftPhase == EShiftPhase::ShiftCompleted)
		{
			StatusBannerText->SetTextRenderColor(ReadoutEmeraldColor);
			StatusBannerText->SetText(FText::FromString(TEXT("SHIFT COMPLETE - HOLD [R] TO RESTART")));
			return;
		}
		if (LatchedVerdict == EShiftVerdictType::ContractPassed || CachedShiftPhase == EShiftPhase::ContractPassTransition)
		{
			StatusBannerText->SetTextRenderColor(ReadoutEmeraldColor);
			StatusBannerText->SetText(FText::FromString(TEXT("QUOTA STAMPED - LOADING NEXT CONTRACT")));
			return;
		}
	}

	if (HasContaminationFault_Implementation())
	{
		StatusBannerText->SetTextRenderColor(ReadoutFaultBannerColor);
		StatusBannerText->SetText(FText::FromString(TEXT("FAULT: CONTAMINATED CHUNK - ROUTE TO [SPACE] TARE")));
		return;
	}

	if (CachedSnapshot.InFlightMassKg > 0.01f)
	{
		StatusBannerText->SetTextRenderColor(ReadoutNormalSkyBlueColor);
		StatusBannerText->SetText(FText::FromString(TEXT("CHUNK DROPPING...")));
		return;
	}

	if (!CachedSnapshot.bScaleSettledForStamp &&
		CachedSnapshot.SettledScaleMassKg > 0.0f &&
		!IsReceiverSettled_Implementation())
	{
		StatusBannerText->SetTextRenderColor(ReadoutGoldColor);
		StatusBannerText->SetText(FText::FromString(TEXT("SCALE SETTLING...")));
		return;
	}

	if (CachedSnapshot.bScaleSettledForStamp || CachedSnapshot.bIsWithinQuotaWindow)
	{
		StatusBannerText->SetTextRenderColor(ReadoutEmeraldColor);
		StatusBannerText->SetText(FText::FromString(TEXT("QUOTA MET - PRESS [E] TO STAMP")));
		return;
	}

	const float MinValidKg = FMath::Max(0.0f, CachedSnapshot.TargetTitheMassKg - CachedSnapshot.QuotaToleranceKg);
	if (CachedSnapshot.SettledScaleMassKg > KINDA_SMALL_NUMBER && CachedSnapshot.SettledScaleMassKg < MinValidKg)
	{
		const float RemainingToWindowKg = FMath::Max(0.01f, MinValidKg - CachedSnapshot.SettledScaleMassKg);
		StatusBannerText->SetTextRenderColor(ReadoutNormalSkyBlueColor);
		StatusBannerText->SetText(FText::FromString(
			FString::Printf(TEXT("UNDER QUOTA - ADD %.2f KG TO MIN WINDOW"), RemainingToWindowKg)));
		return;
	}

	StatusBannerText->SetTextRenderColor(ReadoutNormalSkyBlueColor);
	StatusBannerText->SetText(FText::FromString(TEXT("HOLD [LMB] TO SLICE MONOLITH INTO TITHE")));
}

FString AScaleAssemblyActor::FormatFaultReasonBanner(EContractFaultReason FaultReason)
{
	switch (FaultReason)
	{
	case EContractFaultReason::QuotaOvershoot:
		return TEXT("FAULT: OVERWEIGHT - EXCEEDED MAX QUOTA");
	case EContractFaultReason::VolatileDetonation:
		return TEXT("FAULT: VOLATILE VEIN SEVERED");
	case EContractFaultReason::ClampStructuralSnap:
		return TEXT("FAULT: CLAMP TORQUE SNAP");
	case EContractFaultReason::ScaleContaminated:
		return TEXT("FAULT: CONTAMINATED CHUNK - ROUTE TO [SPACE] TARE");
	case EContractFaultReason::TareAllowanceExceeded:
		return TEXT("FAULT: MAX TARE WASTE EXCEEDED");
	case EContractFaultReason::MathematicallyUnreachable:
		return TEXT("FAULT: INSUFFICIENT MASS REMAINING");
	case EContractFaultReason::ManualOperatorReset:
		return TEXT("MANUAL RESET [R]");
	default:
		return TEXT("FAULT: RESETTING INSPECTION CELL");
	}
}

void AScaleAssemblyActor::RefreshRouteReadout()
{
	if (!IsValid(RouteReadoutText) || !IsValid(DiverterGateComponent))
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

	ApplyTintedGreyboxMaterial(ReadoutBackingPlate, ConsoleBezelLinearColor, 0.96f, 0.02f);
	ApplyTintedGreyboxMaterial(ScaleBaseVisual, FLinearColor(0.055f, 0.062f, 0.072f, 1.0f), 0.76f, 0.35f);
	ApplyTintedGreyboxMaterial(LeftHopperGuideVisual, FLinearColor(0.082f, 0.090f, 0.105f, 1.0f), 0.70f, 0.40f);
	ApplyTintedGreyboxMaterial(PanPlatformVisual, FLinearColor(0.095f, 0.086f, 0.074f, 1.0f), 0.66f, 0.45f);
	ApplyTintedGreyboxMaterial(DiverterFlapperVisual, FLinearColor(0.18f, 0.095f, 0.024f, 1.0f), 0.54f, 0.48f);
	ApplyTintedGreyboxMaterial(TareChuteVisual, FLinearColor(0.068f, 0.032f, 0.028f, 1.0f), 0.80f, 0.28f);
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

void AScaleAssemblyActor::HandleChunkLandedOnScale(AActor* PayloadActor, float /*ChunkMassKg*/, float /*UpdatedPanMassKg*/)
{
	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->NotifyPayloadLandedOnScale(PayloadActor);
		}
	}
}

void AScaleAssemblyActor::HandleChunkBouncedOffScale(AActor* PayloadActor, float /*ChunkMassKg*/, float /*UpdatedPanMassKg*/)
{
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
