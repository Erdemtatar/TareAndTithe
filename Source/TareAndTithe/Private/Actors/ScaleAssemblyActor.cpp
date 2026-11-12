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

	auto ConfigureSolidKinematicMesh = [this](UStaticMeshComponent* MeshComp)
	{
		MeshComp->PrimaryComponentTick.bCanEverTick = false;
		MeshComp->SetSimulatePhysics(false);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshComp->SetCollisionObjectType(ECC_WorldDynamic);
		MeshComp->SetCollisionResponseToAllChannels(ECR_Block);
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

	ScaleBaseVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScaleBaseVisual"));
	ScaleBaseVisual->SetupAttachment(ScaleRoot);
	ConfigureSolidKinematicMesh(ScaleBaseVisual);
	ScaleBaseVisual->SetRelativeLocation(FVector(0.0, 0.0, 6.0));
	ScaleBaseVisual->SetRelativeScale3D(FVector(0.82, 0.68, 0.12));

	LeftHopperGuideVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftHopperGuideVisual"));
	LeftHopperGuideVisual->SetupAttachment(ScaleRoot);
	ConfigureSolidKinematicMesh(LeftHopperGuideVisual);
	LeftHopperGuideVisual->SetRelativeLocation(FVector(0.0, -32.5, 78.0));
	LeftHopperGuideVisual->SetRelativeScale3D(FVector(0.84, 0.03, 0.80));

	constexpr double PanScaleX = 0.76;
	constexpr double PanScaleY = 0.62;
	constexpr double PanScaleZ = 0.06;
	const FVector InvPanScale(1.0 / PanScaleX, 1.0 / PanScaleY, 1.0 / PanScaleZ);
	auto ToPanChildRelLoc = [PanScaleX, PanScaleY, PanScaleZ](double DxCm, double DyCm, double DzCm) -> FVector
	{
		return FVector(DxCm / PanScaleX, DyCm / PanScaleY, DzCm / PanScaleZ);
	};

	PanPlatform = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PanPlatform"));
	PanPlatform->SetupAttachment(ScaleRoot);
	ConfigureSolidKinematicMesh(PanPlatform);
	PanPlatform->SetRelativeLocation(FVector(0.0, 0.0, 22.0));
	PanPlatform->SetRelativeScale3D(FVector(PanScaleX, PanScaleY, PanScaleZ));

	auto ConfigurePanRimBox = [this, &InvPanScale, &ToPanChildRelLoc](
		UBoxComponent* RimBox,
		double DxCm,
		double DyCm,
		double DzCm,
		const FVector& ExtentCm)
	{
		RimBox->SetupAttachment(PanPlatform);
		RimBox->PrimaryComponentTick.bCanEverTick = false;
		RimBox->SetSimulatePhysics(false);
		RimBox->SetRelativeScale3D(InvPanScale);
		RimBox->SetRelativeLocation(ToPanChildRelLoc(DxCm, DyCm, DzCm));
		RimBox->InitBoxExtent(ExtentCm);
		RimBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		RimBox->SetCollisionObjectType(ECC_WorldDynamic);
		RimBox->SetCollisionResponseToAllChannels(ECR_Ignore);
		RimBox->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
		RimBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		RimBox->SetGenerateOverlapEvents(false);
	};

	// 4 raised physical rim walls (15 cm total height, 12 cm curb above pan deck) moving with PanPlatform without self-overlapping PanTriggerBox.
	PanRimFrontBox = CreateDefaultSubobject<UBoxComponent>(TEXT("PanRimFrontBox"));
	ConfigurePanRimBox(PanRimFrontBox, -36.5, 0.0, 7.5, FVector(1.5, 31.0, 7.5));

	PanRimBackBox = CreateDefaultSubobject<UBoxComponent>(TEXT("PanRimBackBox"));
	ConfigurePanRimBox(PanRimBackBox, +36.5, 0.0, 7.5, FVector(1.5, 31.0, 7.5));

	PanRimLeftBox = CreateDefaultSubobject<UBoxComponent>(TEXT("PanRimLeftBox"));
	ConfigurePanRimBox(PanRimLeftBox, 0.0, -29.5, 7.5, FVector(35.0, 1.5, 7.5));

	PanRimRightBox = CreateDefaultSubobject<UBoxComponent>(TEXT("PanRimRightBox"));
	ConfigurePanRimBox(PanRimRightBox, 0.0, +29.5, 7.5, FVector(35.0, 1.5, 7.5));

	PanTriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("PanTriggerBox"));
	PanTriggerBox->SetupAttachment(PanPlatform);
	PanTriggerBox->PrimaryComponentTick.bCanEverTick = false;
	PanTriggerBox->SetRelativeScale3D(InvPanScale);
	// Bottom face at -3.0 cm (6.0 cm below the +3.0 cm top surface of PanPlatform); top face at +19.0 cm (>= 13.8 cm below +45 deg DiverterFlapper ramp).
	PanTriggerBox->SetRelativeLocation(ToPanChildRelLoc(0.0, 0.0, static_cast<double>(PanTriggerCenterOffsetZCm)));
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
	ConfigureSolidKinematicMesh(DiverterFlapperVisual);
	DiverterFlapperVisual->SetCollisionResponseToAllChannels(ECR_Ignore);
	DiverterFlapperVisual->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	DiverterFlapperVisual->SetRelativeLocation(FVector(0.0, 0.0, 45.5));
	DiverterFlapperVisual->SetRelativeScale3D(FVector(0.84, 0.03, 0.91));

	TareChuteVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TareChuteVisual"));
	TareChuteVisual->SetupAttachment(ScaleRoot);
	TareChuteVisual->PrimaryComponentTick.bCanEverTick = false;
	TareChuteVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TareChuteVisual->SetGenerateOverlapEvents(false);
	if (IsValid(FallbackCubeMesh))
	{
		TareChuteVisual->SetStaticMesh(FallbackCubeMesh);
	}
	if (IsValid(BaseGreyboxShapeMaterial))
	{
		TareChuteVisual->SetMaterial(0, BaseGreyboxShapeMaterial);
	}
	TareChuteVisual->SetRelativeLocation(FVector(0.0, 86.0, 4.0));
	TareChuteVisual->SetRelativeScale3D(FVector(0.86, 0.84, 0.08));

	TareChuteTriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TareChuteTriggerBox"));
	TareChuteTriggerBox->SetupAttachment(ScaleRoot);
	TareChuteTriggerBox->PrimaryComponentTick.bCanEverTick = false;
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

	ReadoutBackingPlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReadoutBackingPlate"));
	ReadoutBackingPlate->SetupAttachment(ReadoutConsolePivot);
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
	ReadoutBackingPlate->SetRelativeScale3D(FVector(0.016, 2.52, 0.37));

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
	HeaderStencilText->SetupAttachment(ReadoutConsolePivot);
	ConfigureReadoutText(HeaderStencilText, FVector(-2.0, 0.0, 14.8), 5.4f, ReadoutHeaderCyanColor, TEXT("DIRECTOR'S CUT"));

	ContractCodeText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ContractCodeText"));
	ContractCodeText->SetupAttachment(ReadoutConsolePivot);
	ConfigureReadoutText(
		ContractCodeText,
		FVector(-2.0, 0.0, 9.6),
		5.2f,
		ReadoutHeaderCyanColor,
		TEXT("CONTRACT 01/06 | TARGET: 12.00 KG [10.75 - 13.25 KG]"));

	StatusBannerText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusBannerText"));
	StatusBannerText->SetupAttachment(ReadoutConsolePivot);
	ConfigureReadoutText(
		StatusBannerText,
		FVector(-2.0, 0.0, 3.6),
		6.2f,
		ReadoutNormalSkyBlueColor,
		TEXT("HOLD [LMB] TO SLICE MONOLITH INTO TITHE"));

	MassReadoutText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("MassReadoutText"));
	MassReadoutText->SetupAttachment(ReadoutConsolePivot);
	ConfigureReadoutText(MassReadoutText, FVector(-2.0, 0.0, -3.8), 9.4f, ReadoutGoldColor, TEXT("00.00 / 12.00 KG"));

	TareReadoutText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TareReadoutText"));
	TareReadoutText->SetupAttachment(ReadoutConsolePivot);
	ConfigureReadoutText(TareReadoutText, FVector(-2.0, -52.0, -10.6), 5.8f, ReadoutGoldColor, TEXT("TARE: 0.0 / 14.0 KG"));

	RouteReadoutText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("RouteReadoutText"));
	RouteReadoutText->SetupAttachment(ReadoutConsolePivot);
	ConfigureReadoutText(RouteReadoutText, FVector(-2.0, 52.0, -10.6), 5.8f, ReadoutHeaderCyanColor, TEXT("ROUTE: TITHE [SPACE]"));

	ControlStencilText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ControlStencilText"));
	ControlStencilText->SetupAttachment(ReadoutConsolePivot);
	ConfigureReadoutText(
		ControlStencilText,
		FVector(-2.0, 0.0, -15.6),
		4.5f,
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
		ScaleWeighingComponent->SetPanPlatformComponent(PanPlatform);
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
		DiverterGateComponent->SetFlapperPivotAndPrimitive(DiverterFlapperPivot, DiverterFlapperVisual);
		DiverterGateComponent->BindTareChuteTrigger(TareChuteTriggerBox);
		DiverterGateComponent->OnRouteChanged.AddUObject(this, &AScaleAssemblyActor::HandleDiverterRouteChanged);
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
			ShiftSubsystem->UnregisterQuotaReceiver(this);
			ShiftSubsystem->UnregisterTelemetryPublisher(this);
		}
	}

	if (IsValid(ScaleWeighingComponent))
	{
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

void AScaleAssemblyActor::HandleDiverterRouteChanged(EDiverterRoute /*NewRoute*/)
{
	RefreshRouteReadout();
}

void AScaleAssemblyActor::EnforcePanTriggerGeometryAndCollision()
{
	if (!IsValid(PanTriggerBox) || !IsValid(PanPlatform))
	{
		return;
	}

	const FVector PlatformScale = PanPlatform->GetRelativeScale3D();
	const double SafeScaleX = FMath::Max(0.01, FMath::Abs(PlatformScale.X));
	const double SafeScaleY = FMath::Max(0.01, FMath::Abs(PlatformScale.Y));
	const double SafeScaleZ = FMath::Max(0.01, FMath::Abs(PlatformScale.Z));

	if (PanTriggerBox->GetAttachParent() != PanPlatform)
	{
		PanTriggerBox->AttachToComponent(PanPlatform, FAttachmentTransformRules::KeepRelativeTransform);
	}

	PanTriggerBox->SetRelativeScale3D(FVector(1.0 / SafeScaleX, 1.0 / SafeScaleY, 1.0 / SafeScaleZ));
	PanTriggerBox->SetRelativeLocation(FVector(0.0, 0.0, static_cast<double>(PanTriggerCenterOffsetZCm) / SafeScaleZ));
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
		TEXT("CONTRACT %02d/%02d | TARGET: %.2f KG [%.2f - %.2f KG]"),
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

	const bool bChunksInFlight = CachedSnapshot.InFlightMassKg > KINDA_SMALL_NUMBER;
	const bool bScaleOscillating = !IsReceiverSettled_Implementation();
	if (bChunksInFlight || bScaleOscillating)
	{
		StatusBannerText->SetTextRenderColor(ReadoutGoldColor);
		StatusBannerText->SetText(FText::FromString(TEXT("WEIGHING / SETTLING...")));
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
	ApplyTintedGreyboxMaterial(ScaleBaseVisual, FLinearColor(0.032f, 0.036f, 0.042f, 1.0f), 0.78f, 0.35f);
	ApplyTintedGreyboxMaterial(LeftHopperGuideVisual, FLinearColor(0.055f, 0.060f, 0.070f, 1.0f), 0.72f, 0.40f);
	ApplyTintedGreyboxMaterial(PanPlatform, FLinearColor(0.055f, 0.050f, 0.042f, 1.0f), 0.68f, 0.45f);
	ApplyTintedGreyboxMaterial(DiverterFlapperVisual, FLinearColor(0.14f, 0.075f, 0.020f, 1.0f), 0.56f, 0.48f);
	ApplyTintedGreyboxMaterial(TareChuteVisual, FLinearColor(0.045f, 0.022f, 0.020f, 1.0f), 0.82f, 0.28f);
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
