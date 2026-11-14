// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/MonolithRigActor.h"
#include "Actors/SeveredChunkActor.h"
#include "Components/ClampTorqueSolverComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/MoltenThermalLightComponent.h"
#include "Components/MonolithSliceComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VeinScannerComponent.h"
#include "Data/MonolithContractDataAsset.h"
#include "Data/ShiftProgressionDataAsset.h"
#include "Data/TareTithePresentationDataAsset.h"
#include "UDynamicMesh.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GeometryScript/CollisionFunctions.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/BodySetup.h"
#include "Subsystems/InspectionShiftSubsystem.h"
#include "UObject/ConstructorHelpers.h"

AMonolithRigActor::AMonolithRigActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	ClampPivotRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ClampPivotRoot"));
	SetRootComponent(ClampPivotRoot);
	ClampPivotRoot->PrimaryComponentTick.bCanEverTick = false;

	ClampSpringHousingVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ClampSpringHousingVisual"));
	ClampSpringHousingVisual->SetupAttachment(ClampPivotRoot);
	ClampSpringHousingVisual->PrimaryComponentTick.bCanEverTick = false;
	ClampSpringHousingVisual->SetSimulatePhysics(false);
	ClampSpringHousingVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ClampSpringHousingVisual->SetGenerateOverlapEvents(false);
	ClampSpringHousingVisual->SetVisibility(false, false);

	TiltingMountComponent = CreateDefaultSubobject<USceneComponent>(TEXT("TiltingMountComponent"));
	TiltingMountComponent->SetupAttachment(ClampPivotRoot);
	TiltingMountComponent->PrimaryComponentTick.bCanEverTick = false;

	ClampJawVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ClampJawVisual"));
	ClampJawVisual->SetupAttachment(TiltingMountComponent);
	ClampJawVisual->PrimaryComponentTick.bCanEverTick = false;
	ClampJawVisual->SetSimulatePhysics(false);
	ClampJawVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ClampJawVisual->SetGenerateOverlapEvents(false);
	ClampJawVisual->SetRelativeLocation(FVector(0.0, 0.0, -2.0));
	ClampJawVisual->SetRelativeScale3D(FVector(0.30, 0.30, 0.16));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		FallbackCubeMesh = CubeMeshFinder.Object;
		ClampJawVisual->SetStaticMesh(FallbackCubeMesh);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMeshFinder.Succeeded())
	{
		FallbackCylinderMesh = CylinderMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlattenMatFinder(
		TEXT("/Engine/EngineMaterials/FlattenMaterial.FlattenMaterial"));
	if (FlattenMatFinder.Succeeded())
	{
		BaseGreyboxShapeMaterial = FlattenMatFinder.Object;
		ClampJawVisual->SetMaterial(0, BaseGreyboxShapeMaterial);
	}

	auto ConfigureStationaryUpgradeVisual = [this](
		UStaticMeshComponent* UpgradeComp,
		UStaticMesh* MeshToUse,
		const FVector& RelLocationCm,
		const FRotator& RelRotationDeg,
		const FVector& RelScale)
	{
		UpgradeComp->SetupAttachment(ClampPivotRoot);
		UpgradeComp->PrimaryComponentTick.bCanEverTick = false;
		UpgradeComp->PrimaryComponentTick.bStartWithTickEnabled = false;
		UpgradeComp->SetSimulatePhysics(false);
		UpgradeComp->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		UpgradeComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		UpgradeComp->SetGenerateOverlapEvents(false);
		UpgradeComp->SetCanEverAffectNavigation(false);
		UpgradeComp->SetCastShadow(false);
		UpgradeComp->SetAffectDistanceFieldLighting(false);
		UpgradeComp->SetAffectDynamicIndirectLighting(false);
		UpgradeComp->SetVisibleInRayTracing(false);
		UpgradeComp->bReceivesDecals = false;
		if (IsValid(MeshToUse))
		{
			UpgradeComp->SetStaticMesh(MeshToUse);
		}
		if (IsValid(BaseGreyboxShapeMaterial))
		{
			UpgradeComp->SetMaterial(0, BaseGreyboxShapeMaterial);
		}
		UpgradeComp->SetRelativeLocation(RelLocationCm);
		UpgradeComp->SetRelativeRotation(RelRotationDeg);
		UpgradeComp->SetRelativeScale3D(RelScale);
		UpgradeComp->SetVisibility(false, false);
		UpgradeComp->SetHiddenInGame(true, false);
	};

	UStaticMesh* CylOrCubeMesh = IsValid(FallbackCylinderMesh) ? FallbackCylinderMesh.Get() : FallbackCubeMesh.Get();

	// [1] HYD L1 & L2: Brushed-steel stabilization cylinders & outboard accumulator tanks flanking ClampSpringHousing (World Z >= 240.4 cm, |Y| >= 26.0 cm).
	HydUpgradeCylinderLeftL1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HydUpgradeCylinderLeftL1"));
	ConfigureStationaryUpgradeVisual(
		HydUpgradeCylinderLeftL1,
		CylOrCubeMesh,
		FVector(-10.0, -33.0, -8.5),
		FRotator(0.0f, 0.0f, -20.0f),
		FVector(0.072, 0.072, 0.210));

	HydUpgradeCylinderRightL1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HydUpgradeCylinderRightL1"));
	ConfigureStationaryUpgradeVisual(
		HydUpgradeCylinderRightL1,
		CylOrCubeMesh,
		FVector(-10.0, +33.0, -8.5),
		FRotator(0.0f, 0.0f, +20.0f),
		FVector(0.072, 0.072, 0.210));

	HydUpgradeAccumulatorLeftL2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HydUpgradeAccumulatorLeftL2"));
	ConfigureStationaryUpgradeVisual(
		HydUpgradeAccumulatorLeftL2,
		CylOrCubeMesh,
		FVector(-12.0, -40.5, -6.0),
		FRotator::ZeroRotator,
		FVector(0.088, 0.088, 0.200));

	HydUpgradeAccumulatorRightL2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HydUpgradeAccumulatorRightL2"));
	ConfigureStationaryUpgradeVisual(
		HydUpgradeAccumulatorRightL2,
		CylOrCubeMesh,
		FVector(-12.0, +40.5, -6.0),
		FRotator::ZeroRotator,
		FVector(0.088, 0.088, 0.200));

	// [3] SCAN L1 & L2: Angled optical scanner pods + bloom-safe cyan sensor lenses & resonance damper arrays (World Z >= 238.2 cm, |Y| >= 27.6 cm).
	ScanUpgradePodLeftL1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScanUpgradePodLeftL1"));
	ConfigureStationaryUpgradeVisual(
		ScanUpgradePodLeftL1,
		FallbackCubeMesh,
		FVector(-15.0, -31.5, -18.0),
		FRotator(0.0f, 0.0f, +16.0f),
		FVector(0.090, 0.064, 0.060));

	ScanUpgradePodRightL1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScanUpgradePodRightL1"));
	ConfigureStationaryUpgradeVisual(
		ScanUpgradePodRightL1,
		FallbackCubeMesh,
		FVector(-15.0, +31.5, -18.0),
		FRotator(0.0f, 0.0f, -16.0f),
		FVector(0.090, 0.064, 0.060));

	ScanUpgradeLensLeftL1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScanUpgradeLensLeftL1"));
	ConfigureStationaryUpgradeVisual(
		ScanUpgradeLensLeftL1,
		CylOrCubeMesh,
		FVector(-19.8, -31.5, -18.0),
		FRotator(90.0f, 0.0f, 0.0f),
		FVector(0.044, 0.044, 0.018));

	ScanUpgradeLensRightL1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScanUpgradeLensRightL1"));
	ConfigureStationaryUpgradeVisual(
		ScanUpgradeLensRightL1,
		CylOrCubeMesh,
		FVector(-19.8, +31.5, -18.0),
		FRotator(90.0f, 0.0f, 0.0f),
		FVector(0.044, 0.044, 0.018));

	ScanUpgradeArrayLeftL2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScanUpgradeArrayLeftL2"));
	ConfigureStationaryUpgradeVisual(
		ScanUpgradeArrayLeftL2,
		FallbackCubeMesh,
		FVector(-16.0, -39.5, -16.5),
		FRotator(0.0f, 0.0f, +12.0f),
		FVector(0.082, 0.054, 0.092));

	ScanUpgradeArrayRightL2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScanUpgradeArrayRightL2"));
	ConfigureStationaryUpgradeVisual(
		ScanUpgradeArrayRightL2,
		FallbackCubeMesh,
		FVector(-16.0, +39.5, -16.5),
		FRotator(0.0f, 0.0f, -12.0f),
		FVector(0.082, 0.054, 0.092));

	ScanUpgradeEmitterLeftL2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScanUpgradeEmitterLeftL2"));
	ConfigureStationaryUpgradeVisual(
		ScanUpgradeEmitterLeftL2,
		FallbackCubeMesh,
		FVector(-20.4, -39.5, -16.5),
		FRotator(0.0f, 0.0f, +12.0f),
		FVector(0.018, 0.038, 0.074));

	ScanUpgradeEmitterRightL2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScanUpgradeEmitterRightL2"));
	ConfigureStationaryUpgradeVisual(
		ScanUpgradeEmitterRightL2,
		FallbackCubeMesh,
		FVector(-20.4, +39.5, -16.5),
		FRotator(0.0f, 0.0f, -12.0f),
		FVector(0.018, 0.038, 0.074));

	RemainderMeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("RemainderMeshComponent"));
	RemainderMeshComponent->SetupAttachment(TiltingMountComponent);
	RemainderMeshComponent->PrimaryComponentTick.bCanEverTick = false;
	RemainderMeshComponent->PrimaryComponentTick.bStartWithTickEnabled = false;
	RemainderMeshComponent->BodyInstance.bUseCCD = true;
	RemainderMeshComponent->SetUseCCD(true);
	RemainderMeshComponent->SetSimulatePhysics(false);
	RemainderMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RemainderMeshComponent->SetGenerateOverlapEvents(false);
	RemainderMeshComponent->SetComplexAsSimpleCollisionEnabled(false, false);
	RemainderMeshComponent->SetRelativeLocation(FVector(0.0, 0.0, -40.0));

	MonolithSliceComponent = CreateDefaultSubobject<UMonolithSliceComponent>(TEXT("MonolithSliceComponent"));
	VeinScannerComponent = CreateDefaultSubobject<UVeinScannerComponent>(TEXT("VeinScannerComponent"));
	ClampTorqueSolverComponent = CreateDefaultSubobject<UClampTorqueSolverComponent>(TEXT("ClampTorqueSolverComponent"));

	const FMoltenBounceLightConfig DefaultMoltenCfg;
	MoltenCutBounceLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MoltenCutBounceLight"));
	MoltenCutBounceLight->SetupAttachment(TiltingMountComponent);
	MoltenCutBounceLight->PrimaryComponentTick.bCanEverTick = false;
	MoltenCutBounceLight->SetRelativeLocation(DefaultMoltenCfg.CutLightLocalOffset);
	MoltenCutBounceLight->SetIntensity(0.0f);
	MoltenCutBounceLight->SetAttenuationRadius(DefaultMoltenCfg.CutAttenuationRadiusCm);
	MoltenCutBounceLight->SetLightColor(DefaultMoltenCfg.BounceColor);
	MoltenCutBounceLight->SetVolumetricScatteringIntensity(0.0f);
	MoltenCutBounceLight->SetCastShadows(false);
	MoltenCutBounceLight->SetVisibility(false, false);

	MoltenThermalLightComponent = CreateDefaultSubobject<UMoltenThermalLightComponent>(TEXT("MoltenThermalLightComponent"));
}

void AMonolithRigActor::BeginPlay()
{
	Super::BeginPlay();

	EnsureGreyboxMaterialsInitialized();
	ApplyUpgradeHardwareMaterials();

	if (IsValid(MoltenThermalLightComponent))
	{
		const FMoltenBounceLightConfig DefaultMoltenCfg;
		MoltenThermalLightComponent->InitializeTargetLight(MoltenCutBounceLight);
		MoltenThermalLightComponent->ConfigureLightParameters(
			DefaultMoltenCfg.BounceColor,
			DefaultMoltenCfg.CutAttenuationRadiusCm,
			DefaultMoltenCfg.FlashDecayDurationSeconds,
			DefaultMoltenCfg.SteadyInterpSpeed);
	}

	if (IsValid(MonolithSliceComponent))
	{
		MonolithSliceComponent->SetRemainderMeshComponent(RemainderMeshComponent);
		MonolithSliceComponent->SetFallbackMaterials(ResolveEffectiveOuterMaterial(), ResolveEffectiveCutCapMaterial());
		MonolithSliceComponent->OnChunkSpawnedFromSlice.AddUObject(this, &AMonolithRigActor::HandleChunkSpawnedFromSlice);
		MonolithSliceComponent->OnSliverVaporized.AddUObject(this, &AMonolithRigActor::HandleSliverVaporized);
	}

	if (IsValid(VeinScannerComponent))
	{
		VeinScannerComponent->SetReferenceFrameComponent(RemainderMeshComponent);
		VeinScannerComponent->OnTelemetryStream.AddUObject(this, &AMonolithRigActor::HandleChildTelemetryStream);
	}

	if (IsValid(ClampTorqueSolverComponent))
	{
		ClampTorqueSolverComponent->SetTiltingMountComponent(TiltingMountComponent, false);
		ClampTorqueSolverComponent->SetNeutralPivotTransform(ClampPivotRoot->GetComponentTransform());
		ClampTorqueSolverComponent->OnTelemetryStream.AddUObject(this, &AMonolithRigActor::HandleChildTelemetryStream);
		ClampTorqueSolverComponent->OnTelemetryImpulse.AddUObject(this, &AMonolithRigActor::HandleChildTelemetryImpulse);
		ClampTorqueSolverComponent->OnClampStructuralSnap.AddUObject(this, &AMonolithRigActor::HandleClampStructuralSnap);
	}

	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->OnPresentationAssetChanged.AddUObject(this, &AMonolithRigActor::ApplyPresentationData);
			ShiftSubsystem->OnNativeContractSnapshotUpdated.AddUObject(this, &AMonolithRigActor::HandleContractSnapshotUpdated);
			if (const UTareTithePresentationDataAsset* ExistingPresentation = ShiftSubsystem->GetActivePresentationAsset())
			{
				ApplyPresentationData(ExistingPresentation);
			}
			HandleContractSnapshotUpdated(ShiftSubsystem->GetCurrentSnapshot());

			ShiftSubsystem->RegisterTelemetryPublisher(this, this);

			FShiftSliceableRigBinding Binding;
			Binding.SliceableTarget.SetObject(this);
			Binding.SliceableTarget.SetInterface(this);
			Binding.OnLoadContract.BindUObject(this, &AMonolithRigActor::LoadContractOnRig);
			Binding.OnQueryRemainderMassKg.BindUObject(this, &AMonolithRigActor::QueryCurrentRemainderMassKg);
			Binding.OnQueryWorldLocation.BindUObject(this, &AMonolithRigActor::QueryRigWorldLocation);

			ShiftSubsystem->RegisterSliceableRig(Binding);
		}
	}
}

void AMonolithRigActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->OnPresentationAssetChanged.RemoveAll(this);
			ShiftSubsystem->OnNativeContractSnapshotUpdated.RemoveAll(this);
			ShiftSubsystem->UnregisterSliceableRig(this);
			ShiftSubsystem->UnregisterTelemetryPublisher(this);
		}
	}

	if (IsValid(MonolithSliceComponent))
	{
		MonolithSliceComponent->OnChunkSpawnedFromSlice.RemoveAll(this);
		MonolithSliceComponent->OnSliverVaporized.RemoveAll(this);
	}
	if (IsValid(VeinScannerComponent))
	{
		VeinScannerComponent->OnTelemetryStream.RemoveAll(this);
	}
	if (IsValid(ClampTorqueSolverComponent))
	{
		ClampTorqueSolverComponent->OnTelemetryStream.RemoveAll(this);
		ClampTorqueSolverComponent->OnTelemetryImpulse.RemoveAll(this);
		ClampTorqueSolverComponent->OnClampStructuralSnap.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

FSlicePreviewTelemetry AMonolithRigActor::PreviewSlicePlane_Implementation(
	const FVector& PlaneOriginWorld,
	const FVector& PlaneNormalWorld,
	const FLaserSliceAperture& SheetAperture)
{
	if (!IsValid(MonolithSliceComponent) || !IsValid(VeinScannerComponent))
	{
		return FSlicePreviewTelemetry();
	}

	const bool bIntersects = MonolithSliceComponent->DoesPlaneIntersectRemainder(
		PlaneOriginWorld,
		PlaneNormalWorld,
		SheetAperture);
	FSlicePreviewTelemetry Telemetry = VeinScannerComponent->EvaluateWorldPlane(PlaneOriginWorld, PlaneNormalWorld, bIntersects);
	if (bIntersects)
	{
		float SeveredMassKg = 0.0f;
		float RemainderMassKg = MonolithSliceComponent->GetRemainderMassKg();
		if (MonolithSliceComponent->EstimateSeveredMassKg(PlaneOriginWorld, PlaneNormalWorld, SeveredMassKg, RemainderMassKg))
		{
			Telemetry.EstimatedSeveredMassKg = SeveredMassKg;
			Telemetry.EstimatedRemainderMassKg = RemainderMassKg;
			Telemetry.bHasValidMassEstimate = true;

			if ((SeveredMassKg > MinPredictiveContactMassKg) && !Telemetry.bWillDetonate)
			{
				FVector LocalOrigin = FVector::ZeroVector;
				FVector SeveredLocalNormal = FVector::ZeroVector;
				if (MonolithSliceComponent->ComputeSeveredHalfSpacePlaneLocal(
					PlaneOriginWorld,
					PlaneNormalWorld,
					LocalOrigin,
					SeveredLocalNormal))
				{
					Telemetry.bSeveredHalfContainsVein = VeinScannerComponent->DoesSeveredHalfSpaceContainActiveVein(
						LocalOrigin,
						SeveredLocalNormal,
						MonolithSliceComponent->GetRemainderLocalBounds(),
						VeinRemainderBoundsToleranceCm);
				}
			}
		}
	}
	return Telemetry;
}

FSliceResultSummary AMonolithRigActor::CommitSlicePlane_Implementation(
	const FVector& PlaneOriginWorld,
	const FVector& PlaneNormalWorld,
	const FLaserSliceAperture& SheetAperture)
{
	FSliceResultSummary Summary;
	if (!IsValid(MonolithSliceComponent) || !IsValid(VeinScannerComponent))
	{
		return Summary;
	}

	const float PreCutRemainderMassKg = MonolithSliceComponent->GetRemainderMassKg();
	const FVector PreCutMountLocalCoMCm = TransformRemainderCoMToMountLocal(MonolithSliceComponent->GetRemainderLocalCoMCm());
	const FBox PreCutLocalBounds = MonolithSliceComponent->GetRemainderLocalBounds();
	const float PreCutHorizontalHalfSpanCm = PreCutLocalBounds.IsValid
		? static_cast<float>(0.5 * FMath::Max(PreCutLocalBounds.GetSize().X, PreCutLocalBounds.GetSize().Y))
		: 14.0f;

	TArray<ASeveredChunkActor*> SpawnedChunks;
	Summary = MonolithSliceComponent->ExecuteSlice(
		PlaneOriginWorld,
		PlaneNormalWorld,
		VeinScannerComponent->GetMutableActiveVeins(),
		SpawnedChunks,
		SheetAperture);

	if (Summary.Outcome == ECommitSliceOutcome::Success)
	{
		VeinScannerComponent->SyncPostSliceActiveVeins(
			MonolithSliceComponent->GetRemainderLocalBounds(),
			VeinRemainderBoundsToleranceCm);
	}

	if (Summary.Outcome == ECommitSliceOutcome::Success && IsValid(RemainderMeshComponent))
	{
		RemainderMeshComponent->ConfigureMaterialSet({
			ResolveEffectiveOuterMaterial(CachedLoadedContract),
			ResolveEffectiveCutCapMaterial(CachedLoadedContract)
		});
	}

	if (Summary.Outcome == ECommitSliceOutcome::Success && IsValid(ClampTorqueSolverComponent))
	{
		const FVector MountLocalCoMCm = TransformRemainderCoMToMountLocal(Summary.NewRemainderLocalCoMCm);
		ClampTorqueSolverComponent->ApplySliceLoadAndShear(
			PreCutRemainderMassKg,
			PreCutMountLocalCoMCm,
			Summary.NewRemainderMassKg,
			MountLocalCoMCm,
			PreCutHorizontalHalfSpanCm,
			PlaneNormalWorld);

		// Guard 3: If cumulative jaw shear snapped the clamp during ApplySliceLoadAndShear,
		// HandleClampStructuralSnap has already quenched thermal light, detached the remainder into Chaos freefall,
		// and triggered FaultResetDelay on UInspectionShiftSubsystem. Return immediately without re-igniting MoltenThermalLight!
		if (ClampTorqueSolverComponent->IsClampSnapped())
		{
			return Summary;
		}
	}

	if (Summary.Outcome == ECommitSliceOutcome::Success && IsValid(MoltenThermalLightComponent))
	{
		const FMoltenBounceLightConfig MoltenCfg = IsValid(PresentationData)
			? PresentationData->MoltenBounceConfig
			: FMoltenBounceLightConfig();
		if (IsValid(MoltenCutBounceLight) && IsValid(MonolithSliceComponent))
		{
			const FBox RemWorldBounds = MonolithSliceComponent->GetRemainderWorldBounds();
			if (RemWorldBounds.IsValid && IsValid(TiltingMountComponent))
			{
				const FVector MountLocalBottom = TiltingMountComponent->GetComponentTransform().InverseTransformPosition(
					FVector(RemWorldBounds.GetCenter().X, RemWorldBounds.GetCenter().Y, RemWorldBounds.Min.Z));
				MoltenCutBounceLight->SetRelativeLocation(MountLocalBottom + MoltenCfg.CutLightLocalOffset);
			}
		}
		const float NewSteady = (Summary.NewRemainderMassKg > KINDA_SMALL_NUMBER)
			? MoltenCfg.CutSteadyRemainderIntensity
			: 0.0f;
		MoltenThermalLightComponent->TriggerThermalFlash(MoltenCfg.CutFlashPeakIntensity, NewSteady);
	}

	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->NotifySliceCommittedSummary(Summary);
		}
	}

	return Summary;
}

FBox AMonolithRigActor::GetTargetWorldBounds_Implementation() const
{
	if (IsValid(MonolithSliceComponent))
	{
		return MonolithSliceComponent->GetRemainderWorldBounds();
	}
	return FBox(ForceInit);
}

FMonolithCutMaskVolume AMonolithRigActor::GetCuttableWorldMask_Implementation() const
{
	FMonolithCutMaskVolume OutVolume;
	if (!IsValid(MonolithSliceComponent) || !IsValid(RemainderMeshComponent))
	{
		return OutVolume;
	}

	const FBox RemainderLocalBox = MonolithSliceComponent->GetRemainderLocalBounds();
	if (!RemainderLocalBox.IsValid)
	{
		return OutVolume;
	}

	const FTransform RemainderXf = RemainderMeshComponent->GetComponentTransform();
	const FLaserSheetPresentationConfig SheetCfg = IsValid(PresentationData)
		? PresentationData->LaserSheetConfig.GetSanitized()
		: FLaserSheetPresentationConfig();

	double JawBottomLocalZ = RemainderLocalBox.Max.Z;
	bool bResolvedJawMeshBounds = false;

	auto AccumulateJawBottomFromComponent = [&](const UStaticMeshComponent* JawComp)
	{
		if (!IsValid(JawComp) || !JawComp->IsVisible())
		{
			return;
		}
		const UStaticMesh* JawMesh = JawComp->GetStaticMesh();
		if (!IsValid(JawMesh))
		{
			return;
		}
		const FBox JawMeshBox = JawMesh->GetBoundingBox();
		if (!JawMeshBox.IsValid)
		{
			return;
		}

		// Correction B: Sibling transform from ClampJawVisual space directly into RemainderMeshComponent local space.
		const FTransform JawToRemainderXf = JawComp->GetComponentTransform().GetRelativeTransform(RemainderXf);
		double MinCornerLocalZ = TNumericLimits<double>::Max();
		for (int32 CornerIdx = 0; CornerIdx < 8; ++CornerIdx)
		{
			const FVector JawCorner(
				(CornerIdx & 1) ? JawMeshBox.Max.X : JawMeshBox.Min.X,
				(CornerIdx & 2) ? JawMeshBox.Max.Y : JawMeshBox.Min.Y,
				(CornerIdx & 4) ? JawMeshBox.Max.Z : JawMeshBox.Min.Z);
			const FVector CornerInRemainderLocal = JawToRemainderXf.TransformPosition(JawCorner);
			MinCornerLocalZ = FMath::Min(MinCornerLocalZ, CornerInRemainderLocal.Z);
		}
		JawBottomLocalZ = FMath::Min(JawBottomLocalZ, MinCornerLocalZ);
		bResolvedJawMeshBounds = true;
	};

	AccumulateJawBottomFromComponent(ClampJawVisual);

	if (!bResolvedJawMeshBounds)
	{
		const FBox GripBoxLocal = MonolithSliceComponent->GetClampGripBoxLocal();
		if (GripBoxLocal.IsValid)
		{
			JawBottomLocalZ = FMath::Min(JawBottomLocalZ, GripBoxLocal.Min.Z);
		}
	}

	FBox CuttableLocalBox = RemainderLocalBox;
	CuttableLocalBox.Max.Z = FMath::Min(
		RemainderLocalBox.Max.Z,
		JawBottomLocalZ - static_cast<double>(SheetCfg.JawSeamExclusionMarginCm));

	if (CuttableLocalBox.Max.Z <= (CuttableLocalBox.Min.Z + 0.5))
	{
		return OutVolume;
	}

	const FVector AbsScale = RemainderXf.GetScale3D().GetAbs();
	OutVolume.bIsValid = true;
	OutVolume.CenterWorld = RemainderXf.TransformPosition(CuttableLocalBox.GetCenter());
	OutVolume.HalfExtentsCm = CuttableLocalBox.GetExtent() * AbsScale;
	OutVolume.AxisXWorld = RemainderXf.GetUnitAxis(EAxis::X);
	OutVolume.AxisYWorld = RemainderXf.GetUnitAxis(EAxis::Y);
	OutVolume.AxisZWorld = RemainderXf.GetUnitAxis(EAxis::Z);
	OutVolume.JawBottomLocalZ = static_cast<float>(JawBottomLocalZ);
	return OutVolume;
}

bool AMonolithRigActor::LoadContractOnRig(
	const UMonolithContractDataAsset* Contract,
	const UShiftProgressionDataAsset* ShiftConfig,
	float& OutInitialMassKg)
{
	OutInitialMassKg = 0.0f;

	if (!IsValid(Contract) ||
		!IsValid(MonolithSliceComponent) ||
		!IsValid(VeinScannerComponent) ||
		!IsValid(ClampTorqueSolverComponent) ||
		!IsValid(RemainderMeshComponent) ||
		!IsValid(TiltingMountComponent))
	{
		return false;
	}

	CachedLoadedContract = Contract;
	EnsureGreyboxMaterialsInitialized();
	MonolithSliceComponent->SetFallbackMaterials(
		ResolveEffectiveOuterMaterial(nullptr),
		ResolveEffectiveCutCapMaterial(nullptr));

	// Restore kinematic hierarchy in case previous contract ended via ClampStructuralSnap freefall.
	RemainderMeshComponent->SetGenerateOverlapEvents(false);
	if (RemainderMeshComponent->IsSimulatingPhysics())
	{
		RemainderMeshComponent->SetPhysicsLinearVelocity(FVector::ZeroVector, false);
		RemainderMeshComponent->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector, false);
		RemainderMeshComponent->PutAllRigidBodiesToSleep();
	}
	RemainderMeshComponent->SetSimulatePhysics(false);
	RemainderMeshComponent->SetEnableGravity(false);
	RemainderMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UGeometryScriptLibrary_CollisionFunctions::ResetDynamicMeshCollision(RemainderMeshComponent, false, nullptr);
	RemainderMeshComponent->AttachToComponent(
		TiltingMountComponent,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	RemainderMeshComponent->SetRelativeScale3D(FVector::OneVector);

	if (UWorld* World = GetWorld())
	{
		if (const UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			HandleContractSnapshotUpdated(ShiftSubsystem->GetCurrentSnapshot());
		}
	}

	ClampTorqueSolverComponent->SetNeutralPivotTransform(ClampPivotRoot->GetComponentTransform());
	ClampTorqueSolverComponent->ResetClampState();
	if (IsValid(MoltenThermalLightComponent))
	{
		MoltenThermalLightComponent->ResetThermalImmediate();
	}

	if (!MonolithSliceComponent->InitializeMonolithFromContract(Contract, ShiftConfig))
	{
		return false;
	}

	RemainderMeshComponent->ConfigureMaterialSet({
		ResolveEffectiveOuterMaterial(Contract),
		ResolveEffectiveCutCapMaterial(Contract)
	});

	// Align actual upper grip cross-section centroid plus any contract eccentric grip offset directly under TiltingMountComponent (0, 0)
	// seated inside the jaw pocket, so asymmetric castings and eccentric-clamped Rodin monoliths generate real gravitational lever torque.
	const FBox LocalBounds = MonolithSliceComponent->GetRemainderLocalBounds();
	const FBox TopGripBounds = MonolithSliceComponent->GetTopGripLocalBounds();
	const FVector TopGripCenter = TopGripBounds.IsValid ? TopGripBounds.GetCenter() : FVector::ZeroVector;
	const FVector GripHorizontalOffsetCm(
		static_cast<double>(Contract->ClampGripHorizontalOffsetCm.X),
		static_cast<double>(Contract->ClampGripHorizontalOffsetCm.Y),
		0.0);
	const FVector EffectiveGripPivotLocal = TopGripCenter + GripHorizontalOffsetCm;
	const double TopLocalZ = LocalBounds.IsValid
		? LocalBounds.Max.Z
		: (0.5 * FMath::Max(5.0, static_cast<double>(Contract->DimensionsCm.Z)));
	RemainderMeshComponent->SetRelativeLocationAndRotation(
		FVector(-EffectiveGripPivotLocal.X, -EffectiveGripPivotLocal.Y, -TopLocalZ + static_cast<double>(JawPocketSeatEmbedZCm)),
		FQuat::Identity);
	RemainderMeshComponent->UpdateBounds();

	FitClampJawToActiveMonolith();

	VeinScannerComponent->InitializeFromContract(Contract);

	const float InitialMassKg = MonolithSliceComponent->GetRemainderMassKg();
	const FVector MountLocalCoMCm = TransformRemainderCoMToMountLocal(MonolithSliceComponent->GetRemainderLocalCoMCm());
	ClampTorqueSolverComponent->InitializeFromContract(Contract, InitialMassKg, MountLocalCoMCm);

	OutInitialMassKg = InitialMassKg;
	return true;
}

void AMonolithRigActor::ApplyPresentationData(const UTareTithePresentationDataAsset* InPresentationData)
{
	PresentationData = InPresentationData;
	EnsureGreyboxMaterialsInitialized();

	bHasCustomClampJawMesh = false;
	if (IsValid(PresentationData))
	{
		bHasCustomClampJawMesh = PresentationData->ClampJawMeshSlot.ApplyToVisualComponent(ClampJawVisual);

		if (IsValid(ClampSpringHousingVisual))
		{
			const bool bHasHousingMesh = PresentationData->ClampSpringHousingMeshSlot.ApplyToVisualComponent(ClampSpringHousingVisual);
			ClampSpringHousingVisual->SetVisibility(bHasHousingMesh, false);
		}
	}
	else if (IsValid(ClampSpringHousingVisual))
	{
		ClampSpringHousingVisual->SetVisibility(false, false);
	}

	const FMoltenBounceLightConfig MoltenCfg = IsValid(PresentationData)
		? PresentationData->MoltenBounceConfig
		: FMoltenBounceLightConfig();
	if (IsValid(MoltenCutBounceLight))
	{
		MoltenCutBounceLight->SetRelativeLocation(MoltenCfg.CutLightLocalOffset);
	}
	if (IsValid(MoltenThermalLightComponent))
	{
		MoltenThermalLightComponent->ConfigureLightParameters(
			MoltenCfg.BounceColor,
			MoltenCfg.CutAttenuationRadiusCm,
			MoltenCfg.FlashDecayDurationSeconds,
			MoltenCfg.SteadyInterpSpeed);
	}

	if (IsValid(CachedLoadedContract))
	{
		FitClampJawToActiveMonolith();
	}

	if (IsValid(MonolithSliceComponent))
	{
		MonolithSliceComponent->SetFallbackMaterials(
			ResolveEffectiveOuterMaterial(nullptr),
			ResolveEffectiveCutCapMaterial(nullptr));
		MonolithSliceComponent->RefreshActiveMaterials();
	}
	else if (IsValid(RemainderMeshComponent))
	{
		RemainderMeshComponent->ConfigureMaterialSet({
			ResolveEffectiveOuterMaterial(CachedLoadedContract),
			ResolveEffectiveCutCapMaterial(CachedLoadedContract)
		});
	}

	ApplyUpgradeHardwareMaterials();
	UpdateRigUpgradeHardwareVisuals(CachedHydUpgradeLevel, CachedScanUpgradeLevel);
}

void AMonolithRigActor::EnsureGreyboxMaterialsInitialized()
{
	if (!IsValid(OuterShellOverrideMaterial) && !IsValid(AutoResolvedOuterMasterMaterial))
	{
		if (FPackageName::DoesPackageExist(TEXT("/Game/Materials/M_SlagOuter_Master")))
		{
			AutoResolvedOuterMasterMaterial = LoadObject<UMaterialInterface>(
				nullptr,
				TEXT("/Game/Materials/M_SlagOuter_Master.M_SlagOuter_Master"));
		}
	}

	if (!IsValid(MoltenCutCapOverrideMaterial) && !IsValid(AutoResolvedCutCapMasterMaterial))
	{
		if (FPackageName::DoesPackageExist(TEXT("/Game/Materials/M_MoltenCutCap_Master")))
		{
			AutoResolvedCutCapMasterMaterial = LoadObject<UMaterialInterface>(
				nullptr,
				TEXT("/Game/Materials/M_MoltenCutCap_Master.M_MoltenCutCap_Master"));
		}
	}

	if (!IsValid(BaseGreyboxShapeMaterial))
	{
		return;
	}

	if (!IsValid(FallbackOuterMID))
	{
		FallbackOuterMID = UMaterialInstanceDynamic::Create(BaseGreyboxShapeMaterial, this);
		if (IsValid(FallbackOuterMID))
		{
			FallbackOuterMID->SetVectorParameterValue(TEXT("BaseColorConst"), GreyboxOuterShellColor);
			FallbackOuterMID->SetVectorParameterValue(TEXT("Color"), GreyboxOuterShellColor);
			FallbackOuterMID->SetVectorParameterValue(TEXT("EmissiveColorConst"), FLinearColor::Black);
			FallbackOuterMID->SetScalarParameterValue(TEXT("EmissiveScale"), 0.0f);
			FallbackOuterMID->SetScalarParameterValue(TEXT("RoughnessConst"), 0.72f);
			FallbackOuterMID->SetScalarParameterValue(TEXT("MetallicConst"), 0.28f);
		}
	}

	if (!IsValid(FallbackCutCapMID))
	{
		FallbackCutCapMID = UMaterialInstanceDynamic::Create(BaseGreyboxShapeMaterial, this);
		if (IsValid(FallbackCutCapMID))
		{
			FallbackCutCapMID->SetVectorParameterValue(TEXT("BaseColorConst"), GreyboxMoltenCapColor);
			FallbackCutCapMID->SetVectorParameterValue(TEXT("Color"), GreyboxMoltenCapColor);
			FallbackCutCapMID->SetVectorParameterValue(TEXT("EmissiveColorConst"), GreyboxMoltenCapEmissiveColor);
			FallbackCutCapMID->SetScalarParameterValue(TEXT("EmissiveScale"), 1.0f);
			FallbackCutCapMID->SetScalarParameterValue(TEXT("RoughnessConst"), 0.28f);
			FallbackCutCapMID->SetScalarParameterValue(TEXT("MetallicConst"), 0.10f);
		}
	}

	if (!IsValid(ClampJawMID) && IsValid(ClampJawVisual))
	{
		ClampJawMID = UMaterialInstanceDynamic::Create(BaseGreyboxShapeMaterial, this);
		if (IsValid(ClampJawMID))
		{
			ClampJawMID->SetVectorParameterValue(TEXT("BaseColorConst"), GreyboxClampJawColor);
			ClampJawMID->SetVectorParameterValue(TEXT("Color"), GreyboxClampJawColor);
			ClampJawMID->SetVectorParameterValue(TEXT("EmissiveColorConst"), FLinearColor::Black);
			ClampJawMID->SetScalarParameterValue(TEXT("EmissiveScale"), 0.0f);
			ClampJawMID->SetScalarParameterValue(TEXT("RoughnessConst"), 0.52f);
			ClampJawMID->SetScalarParameterValue(TEXT("MetallicConst"), 0.65f);
			ClampJawVisual->SetMaterial(0, ClampJawMID);
		}
	}

	if (!IsValid(FallbackUpgradeSteelMID))
	{
		FallbackUpgradeSteelMID = UMaterialInstanceDynamic::Create(BaseGreyboxShapeMaterial, this);
		if (IsValid(FallbackUpgradeSteelMID))
		{
			const FLinearColor BrushedSteelColor(0.34f, 0.36f, 0.40f, 1.0f);
			FallbackUpgradeSteelMID->SetVectorParameterValue(TEXT("BaseColorConst"), BrushedSteelColor);
			FallbackUpgradeSteelMID->SetVectorParameterValue(TEXT("Color"), BrushedSteelColor);
			FallbackUpgradeSteelMID->SetVectorParameterValue(TEXT("EmissiveColorConst"), FLinearColor::Black);
			FallbackUpgradeSteelMID->SetScalarParameterValue(TEXT("EmissiveScale"), 0.0f);
			FallbackUpgradeSteelMID->SetScalarParameterValue(TEXT("RoughnessConst"), 0.36f);
			FallbackUpgradeSteelMID->SetScalarParameterValue(TEXT("MetallicConst"), 0.88f);
		}
	}

	if (!IsValid(ScanOpticalSensorMID))
	{
		ScanOpticalSensorMID = UMaterialInstanceDynamic::Create(BaseGreyboxShapeMaterial, this);
		if (IsValid(ScanOpticalSensorMID))
		{
			// Correction 4: Bloom-safe (< 1.65f BloomThreshold) optical cyan sensor MID (MaxChannel = 0.85f).
			const FLinearColor SensorBaseColor(0.04f, 0.36f, 0.44f, 1.0f);
			const FLinearColor SensorEmissiveColor(0.06f, 0.72f, 0.85f, 1.0f);
			ScanOpticalSensorMID->SetVectorParameterValue(TEXT("BaseColorConst"), SensorBaseColor);
			ScanOpticalSensorMID->SetVectorParameterValue(TEXT("Color"), SensorBaseColor);
			ScanOpticalSensorMID->SetVectorParameterValue(TEXT("EmissiveColorConst"), SensorEmissiveColor);
			ScanOpticalSensorMID->SetScalarParameterValue(TEXT("EmissiveScale"), 1.0f);
			ScanOpticalSensorMID->SetScalarParameterValue(TEXT("RoughnessConst"), 0.22f);
			ScanOpticalSensorMID->SetScalarParameterValue(TEXT("MetallicConst"), 0.15f);
		}
	}
}

void AMonolithRigActor::ApplyUpgradeHardwareMaterials()
{
	if (!IsValid(AutoResolvedUpgradeSteelMaterial))
	{
		if (FPackageName::DoesPackageExist(TEXT("/Game/Presentation/Materials/MI_Station_ClampSpringHousing")))
		{
			AutoResolvedUpgradeSteelMaterial = LoadObject<UMaterialInterface>(
				nullptr,
				TEXT("/Game/Presentation/Materials/MI_Station_ClampSpringHousing.MI_Station_ClampSpringHousing"));
		}
	}

	UMaterialInterface* EffectiveSteelMat = AutoResolvedUpgradeSteelMaterial.Get();
	if (!IsValid(EffectiveSteelMat) && IsValid(PresentationData))
	{
		if (PresentationData->ClampSpringHousingMeshSlot.MaterialOverrides.Num() > 0 &&
			IsValid(PresentationData->ClampSpringHousingMeshSlot.MaterialOverrides[0]))
		{
			EffectiveSteelMat = PresentationData->ClampSpringHousingMeshSlot.MaterialOverrides[0].Get();
		}
		else if (IsValid(PresentationData->ClampSpringHousingMeshSlot.Mesh))
		{
			EffectiveSteelMat = PresentationData->ClampSpringHousingMeshSlot.Mesh->GetMaterial(0);
		}
	}
	if (!IsValid(EffectiveSteelMat))
	{
		EffectiveSteelMat = FallbackUpgradeSteelMID.Get();
	}

	auto AssignUpgradeMat = [](UStaticMeshComponent* Comp, UMaterialInterface* Mat)
	{
		if (IsValid(Comp) && IsValid(Mat))
		{
			const int32 NumSlots = FMath::Max(1, Comp->GetNumMaterials());
			for (int32 SlotIdx = 0; SlotIdx < NumSlots; ++SlotIdx)
			{
				Comp->SetMaterial(SlotIdx, Mat);
			}
		}
	};

	AssignUpgradeMat(HydUpgradeCylinderLeftL1, EffectiveSteelMat);
	AssignUpgradeMat(HydUpgradeCylinderRightL1, EffectiveSteelMat);
	AssignUpgradeMat(HydUpgradeAccumulatorLeftL2, EffectiveSteelMat);
	AssignUpgradeMat(HydUpgradeAccumulatorRightL2, EffectiveSteelMat);

	AssignUpgradeMat(ScanUpgradePodLeftL1, EffectiveSteelMat);
	AssignUpgradeMat(ScanUpgradePodRightL1, EffectiveSteelMat);
	AssignUpgradeMat(ScanUpgradeArrayLeftL2, EffectiveSteelMat);
	AssignUpgradeMat(ScanUpgradeArrayRightL2, EffectiveSteelMat);

	UMaterialInterface* EffectiveSensorMat = IsValid(ScanOpticalSensorMID)
		? static_cast<UMaterialInterface*>(ScanOpticalSensorMID.Get())
		: EffectiveSteelMat;
	AssignUpgradeMat(ScanUpgradeLensLeftL1, EffectiveSensorMat);
	AssignUpgradeMat(ScanUpgradeLensRightL1, EffectiveSensorMat);
	AssignUpgradeMat(ScanUpgradeEmitterLeftL2, EffectiveSensorMat);
	AssignUpgradeMat(ScanUpgradeEmitterRightL2, EffectiveSensorMat);
}

void AMonolithRigActor::UpdateRigUpgradeHardwareVisuals(uint8 HydLevel, uint8 ScanLevel)
{
	CachedHydUpgradeLevel = HydLevel;
	CachedScanUpgradeLevel = ScanLevel;

	auto SetUpgradeCompVisible = [](UStaticMeshComponent* Comp, bool bVisible)
	{
		if (IsValid(Comp))
		{
			Comp->SetVisibility(bVisible, false);
			Comp->SetHiddenInGame(!bVisible, false);
		}
	};

	const bool bHydL1 = (HydLevel >= 1);
	const bool bHydL2 = (HydLevel >= 2);
	SetUpgradeCompVisible(HydUpgradeCylinderLeftL1, bHydL1);
	SetUpgradeCompVisible(HydUpgradeCylinderRightL1, bHydL1);
	SetUpgradeCompVisible(HydUpgradeAccumulatorLeftL2, bHydL2);
	SetUpgradeCompVisible(HydUpgradeAccumulatorRightL2, bHydL2);

	const bool bScanL1 = (ScanLevel >= 1);
	const bool bScanL2 = (ScanLevel >= 2);
	SetUpgradeCompVisible(ScanUpgradePodLeftL1, bScanL1);
	SetUpgradeCompVisible(ScanUpgradePodRightL1, bScanL1);
	SetUpgradeCompVisible(ScanUpgradeLensLeftL1, bScanL1);
	SetUpgradeCompVisible(ScanUpgradeLensRightL1, bScanL1);
	SetUpgradeCompVisible(ScanUpgradeArrayLeftL2, bScanL2);
	SetUpgradeCompVisible(ScanUpgradeArrayRightL2, bScanL2);
	SetUpgradeCompVisible(ScanUpgradeEmitterLeftL2, bScanL2);
	SetUpgradeCompVisible(ScanUpgradeEmitterRightL2, bScanL2);
}

UMaterialInterface* AMonolithRigActor::ResolveEffectiveOuterMaterial(const UMonolithContractDataAsset* Contract) const
{
	if (IsValid(Contract) && IsValid(Contract->OuterShellMaterial))
	{
		return Contract->OuterShellMaterial.Get();
	}
	if (IsValid(PresentationData) && IsValid(PresentationData->DefaultMonolithOuterMaterial))
	{
		return PresentationData->DefaultMonolithOuterMaterial.Get();
	}
	if (IsValid(OuterShellOverrideMaterial))
	{
		return OuterShellOverrideMaterial.Get();
	}
	if (IsValid(AutoResolvedOuterMasterMaterial))
	{
		return AutoResolvedOuterMasterMaterial.Get();
	}
	return FallbackOuterMID.Get();
}

UMaterialInterface* AMonolithRigActor::ResolveEffectiveCutCapMaterial(const UMonolithContractDataAsset* Contract) const
{
	if (IsValid(Contract) && IsValid(Contract->MoltenCutCapMaterial))
	{
		return Contract->MoltenCutCapMaterial.Get();
	}
	if (IsValid(PresentationData) && IsValid(PresentationData->DefaultMoltenCutCapMaterial))
	{
		return PresentationData->DefaultMoltenCutCapMaterial.Get();
	}
	if (IsValid(MoltenCutCapOverrideMaterial))
	{
		return MoltenCutCapOverrideMaterial.Get();
	}
	if (IsValid(AutoResolvedCutCapMasterMaterial))
	{
		return AutoResolvedCutCapMasterMaterial.Get();
	}
	return FallbackCutCapMID.Get();
}

FVector AMonolithRigActor::TransformRemainderCoMToMountLocal(const FVector& RemainderLocalCoMCm) const
{
	FVector MountLocalPos = RemainderLocalCoMCm;
	if (IsValid(RemainderMeshComponent))
	{
		MountLocalPos = RemainderMeshComponent->GetRelativeTransform().TransformPosition(RemainderLocalCoMCm);
	}
	if (IsValid(TiltingMountComponent))
	{
		MountLocalPos += TiltingMountComponent->GetRelativeLocation();
	}
	return MountLocalPos;
}

void AMonolithRigActor::FitClampJawToActiveMonolith()
{
	if (!IsValid(ClampJawVisual) || !IsValid(CachedLoadedContract))
	{
		return;
	}

	FVector GripSizeCm = CachedLoadedContract->DimensionsCm;
	if (IsValid(MonolithSliceComponent))
	{
		const FBox TopGripBounds = MonolithSliceComponent->GetTopGripLocalBounds();
		if (TopGripBounds.IsValid)
		{
			GripSizeCm = TopGripBounds.GetSize();
		}
	}

	const double GripWidthX = FMath::Max(6.0, static_cast<double>(GripSizeCm.X));
	const double GripWidthY = FMath::Max(6.0, static_cast<double>(GripSizeCm.Y));
	const double ProtrusionCm = FMath::Max(0.5, static_cast<double>(JawProngOuterProtrusionCm));
	const double EccentricOffsetXCm = static_cast<double>(CachedLoadedContract->ClampGripHorizontalOffsetCm.X);
	const double EccentricOffsetYCm = static_cast<double>(CachedLoadedContract->ClampGripHorizontalOffsetCm.Y);
	const FVector JawCenterMountOffsetCm(-EccentricOffsetXCm, -EccentricOffsetYCm, 0.0);

	if (bHasCustomClampJawMesh &&
		IsValid(PresentationData) &&
		IsValid(PresentationData->ClampJawMeshSlot.Mesh))
	{
		auto ComputeSlotOuterSizeCm = [](const UStaticMesh* Mesh, const FTransform& SlotTransform, const FVector& FallbackSize) -> FVector
		{
			if (!IsValid(Mesh))
			{
				return FallbackSize;
			}
			const FBox RawBox = Mesh->GetBoundingBox();
			return RawBox.IsValid ? RawBox.TransformBy(SlotTransform).GetSize() : FallbackSize;
		};

		auto ApplyMountXYScaleToSlot = [](
			UStaticMeshComponent* VisualComp,
			const FTransform& BaseSlotTransform,
			double RatioMountX,
			double RatioMountY,
			const FVector& MountXYTranslationCm)
		{
			if (!IsValid(VisualComp))
			{
				return;
			}
			const FQuat SlotRot = BaseSlotTransform.GetRotation();
			const FVector SlotScale = BaseSlotTransform.GetScale3D();
			const FVector SlotLoc = BaseSlotTransform.GetLocation();
			const bool bLocalXMapsToMountY = FMath::Abs(SlotRot.GetAxisX().Y) > 0.707;
			const FVector LocalScaleMult = bLocalXMapsToMountY
				? FVector(RatioMountY, RatioMountX, 1.0)
				: FVector(RatioMountX, RatioMountY, 1.0);

			VisualComp->SetRelativeLocation(
				FVector(
					(SlotLoc.X * RatioMountX) + MountXYTranslationCm.X,
					(SlotLoc.Y * RatioMountY) + MountXYTranslationCm.Y,
					SlotLoc.Z));
			VisualComp->SetRelativeRotation(SlotRot);
			VisualComp->SetRelativeScale3D(SlotScale * LocalScaleMult);
		};

		const FTransform& JawSlotXf = PresentationData->ClampJawMeshSlot.VisualLocalTransform;
		const FVector BaseJawOuterCm = ComputeSlotOuterSizeCm(
			PresentationData->ClampJawMeshSlot.Mesh,
			JawSlotXf,
			FVector(30.0, 30.0, 16.0));

		const double SafeOuterRatioX = FMath::Clamp(static_cast<double>(CustomJawProngOuterRatioX), 0.35, 1.0);
		const double SafeInnerRatioX = FMath::Clamp(static_cast<double>(CustomJawProngInnerRatioX), 0.20, SafeOuterRatioX - 0.05);
		const double SafeOuterRatioY = FMath::Clamp(static_cast<double>(CustomJawProngOuterRatioY), 0.35, 1.0);
		const double SafeInnerRatioY = FMath::Clamp(static_cast<double>(CustomJawProngInnerRatioY), 0.20, SafeOuterRatioY - 0.05);
		const double MinJawOuterCm = static_cast<double>(MinAdaptiveJawOuterWidthCm);
		const double MaxJawOuterCm = FMath::Max(MinJawOuterCm + 4.0, static_cast<double>(MaxAdaptiveJawOuterWidthCm));

		const double DesiredMinOuterX = (GripWidthX + (2.0 * ProtrusionCm)) / SafeOuterRatioX;
		const double DesiredMaxOuterX = FMath::Max(DesiredMinOuterX, FMath::Max(4.0, GripWidthX - 1.2) / SafeInnerRatioX);
		const double TargetOuterXCm = FMath::Clamp(DesiredMinOuterX, MinJawOuterCm, FMath::Min(DesiredMaxOuterX, MaxJawOuterCm));

		const double DesiredMinOuterY = (GripWidthY + (2.0 * ProtrusionCm)) / SafeOuterRatioY;
		const double DesiredMaxOuterY = FMath::Max(DesiredMinOuterY, FMath::Max(4.0, GripWidthY - 1.2) / SafeInnerRatioY);
		const double TargetOuterYCm = FMath::Clamp(DesiredMinOuterY, MinJawOuterCm, FMath::Min(DesiredMaxOuterY, MaxJawOuterCm));

		const double JawRatioX = TargetOuterXCm / FMath::Max(1.0, BaseJawOuterCm.X);
		const double JawRatioY = TargetOuterYCm / FMath::Max(1.0, BaseJawOuterCm.Y);
		ApplyMountXYScaleToSlot(ClampJawVisual, JawSlotXf, JawRatioX, JawRatioY, JawCenterMountOffsetCm);

		if (IsValid(ClampSpringHousingVisual) && IsValid(PresentationData->ClampSpringHousingMeshSlot.Mesh))
		{
			const FTransform& HousingSlotXf = PresentationData->ClampSpringHousingMeshSlot.VisualLocalTransform;
			const FVector BaseHousingOuterCm = ComputeSlotOuterSizeCm(
				PresentationData->ClampSpringHousingMeshSlot.Mesh,
				HousingSlotXf,
				FVector(48.0, 48.0, 40.0));

			const double RequiredEnvelopeXCm = FMath::Max(
				TargetOuterXCm * 1.06,
				TargetOuterXCm + (2.0 * FMath::Abs(EccentricOffsetXCm)) + 1.5);
			const double RequiredEnvelopeYCm = FMath::Max(
				TargetOuterYCm * 1.06,
				TargetOuterYCm + (2.0 * FMath::Abs(EccentricOffsetYCm)) + 1.5);

			const double TargetHousingXCm = FMath::Clamp(
				FMath::Max(BaseHousingOuterCm.X, RequiredEnvelopeXCm),
				BaseHousingOuterCm.X,
				MaxJawOuterCm + 10.0);
			const double TargetHousingYCm = FMath::Clamp(
				FMath::Max(BaseHousingOuterCm.Y, RequiredEnvelopeYCm),
				BaseHousingOuterCm.Y,
				MaxJawOuterCm + 10.0);

			const double HousingRatioX = TargetHousingXCm / FMath::Max(1.0, BaseHousingOuterCm.X);
			const double HousingRatioY = TargetHousingYCm / FMath::Max(1.0, BaseHousingOuterCm.Y);
			ApplyMountXYScaleToSlot(ClampSpringHousingVisual, HousingSlotXf, HousingRatioX, HousingRatioY, FVector::ZeroVector);
		}
		return;
	}

	const double GreyboxJawScaleX = FMath::Clamp((GripWidthX + (2.0 * ProtrusionCm)) / 100.0, 0.16, 0.72);
	const double GreyboxJawScaleY = FMath::Clamp((GripWidthY + (2.0 * ProtrusionCm)) / 100.0, 0.16, 0.72);
	ClampJawVisual->SetRelativeLocation(FVector(JawCenterMountOffsetCm.X, JawCenterMountOffsetCm.Y, -2.0));
	ClampJawVisual->SetRelativeRotation(FRotator::ZeroRotator);
	ClampJawVisual->SetRelativeScale3D(FVector(GreyboxJawScaleX, GreyboxJawScaleY, 0.16));
}

float AMonolithRigActor::QueryCurrentRemainderMassKg() const
{
	return IsValid(MonolithSliceComponent) ? MonolithSliceComponent->GetRemainderMassKg() : 0.0f;
}

FVector AMonolithRigActor::QueryRigWorldLocation() const
{
	return GetActorLocation();
}

void AMonolithRigActor::HandleChildTelemetryStream(ETelemetryStreamId StreamId, float NormalizedValue)
{
	OnTelemetryStream.Broadcast(StreamId, NormalizedValue);
}

void AMonolithRigActor::HandleChildTelemetryImpulse(
	ETelemetryImpulseId ImpulseId,
	float NormalizedMagnitude,
	const FVector& WorldLocation)
{
	OnTelemetryImpulse.Broadcast(ImpulseId, NormalizedMagnitude, WorldLocation);
}

void AMonolithRigActor::HandleContractSnapshotUpdated(const FContractRuntimeSnapshot& Snapshot)
{
	if (IsValid(ClampTorqueSolverComponent))
	{
		const float HydMultiplier = 1.0f + (0.25f * static_cast<float>(Snapshot.ClampHydraulicsLevel));
		ClampTorqueSolverComponent->SetTorqueCapacityMultiplier(HydMultiplier);
	}
	if (IsValid(VeinScannerComponent))
	{
		const float ScanMultiplier = 1.0f + (0.30f * static_cast<float>(Snapshot.VeinScannerDamperLevel));
		VeinScannerComponent->SetWarningRangeMultiplier(ScanMultiplier);
	}
	UpdateRigUpgradeHardwareVisuals(Snapshot.ClampHydraulicsLevel, Snapshot.VeinScannerDamperLevel);
}

void AMonolithRigActor::HandleChunkSpawnedFromSlice(ASeveredChunkActor* SpawnedChunk)
{
	if (!IsValid(SpawnedChunk))
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->NotifyChunkSpawnedInFlight(SpawnedChunk);
		}
	}
}

void AMonolithRigActor::HandleSliverVaporized(float VaporizedMassKg, const FVector& WorldLocation)
{
	const float RefMass = FMath::Max(0.05f, VaporizedImpulseReferenceMassKg);
	const float NormalizedMagnitude = FMath::Clamp(VaporizedMassKg / RefMass, 0.1f, 1.0f);
	OnTelemetryImpulse.Broadcast(ETelemetryImpulseId::SliverVaporized, NormalizedMagnitude, WorldLocation);

	if (IsValid(MoltenThermalLightComponent))
	{
		const FMoltenBounceLightConfig MoltenCfg = IsValid(PresentationData)
			? PresentationData->MoltenBounceConfig
			: FMoltenBounceLightConfig();
		const float VaporFlash = MoltenCfg.CutFlashPeakIntensity * FMath::Lerp(0.45f, 1.0f, NormalizedMagnitude);
		MoltenThermalLightComponent->TriggerThermalFlash(VaporFlash, MoltenThermalLightComponent->GetTargetSteadyIntensity());
	}

	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->NotifySliverVaporizedAsTare(VaporizedMassKg, WorldLocation);
		}
	}
}

void AMonolithRigActor::HandleClampStructuralSnap()
{
	if (IsValid(MoltenThermalLightComponent))
	{
		MoltenThermalLightComponent->SetSteadyEmberIntensity(0.0f);
	}

	// Guard 3: Detach and drop snapped remainder under guaranteed non-empty simple Chaos collision + gravity + shear tumble kick.
	if (IsValid(RemainderMeshComponent))
	{
		SetActorEnableCollision(true);
		RemainderMeshComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

		if (UDynamicMesh* DynMesh = RemainderMeshComponent->GetDynamicMesh())
		{
			FGeometryScriptCollisionFromMeshOptions SnapCollisionOptions;
			SnapCollisionOptions.bEmitTransaction = false;
			SnapCollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
			SnapCollisionOptions.bSimplifyHulls = true;
			SnapCollisionOptions.MaxConvexHullsPerMesh = 4;
			SnapCollisionOptions.ConvexHullTargetFaceCount = 20;
			SnapCollisionOptions.MinThickness = 0.5f;

			UGeometryScriptLibrary_CollisionFunctions::SetDynamicMeshCollisionFromMesh(
				DynMesh,
				RemainderMeshComponent,
				SnapCollisionOptions,
				nullptr);

			const UBodySetup* BodySetup = RemainderMeshComponent->GetBodySetup();
			if (!IsValid(BodySetup) || BodySetup->AggGeom.GetElementCount() == 0)
			{
				SnapCollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::AlignedBoxes;
				UGeometryScriptLibrary_CollisionFunctions::SetDynamicMeshCollisionFromMesh(
					DynMesh,
					RemainderMeshComponent,
					SnapCollisionOptions,
					nullptr);
			}
		}

		RemainderMeshComponent->BodyInstance.bUseCCD = true;
		RemainderMeshComponent->SetUseCCD(true);
		RemainderMeshComponent->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
		RemainderMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		RemainderMeshComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		RemainderMeshComponent->SetGenerateOverlapEvents(false);
		RemainderMeshComponent->SetComplexAsSimpleCollisionEnabled(false, true);
		RemainderMeshComponent->SetEnableGravity(true);

		const float SafeMassKg = FMath::Max(1.0f, QueryCurrentRemainderMassKg());
		RemainderMeshComponent->SetMassOverrideInKg(NAME_None, SafeMassKg, true);
		RemainderMeshComponent->SetSimulatePhysics(true);
		RemainderMeshComponent->WakeAllRigidBodies();

		const FVector2D SnapDir = IsValid(ClampTorqueSolverComponent)
			? ClampTorqueSolverComponent->GetLastSnapTorqueDirection()
			: FVector2D(1.0f, 0.0f);
		RemainderMeshComponent->SetPhysicsLinearVelocity(FVector(0.0, 0.0, -45.0), false);
		RemainderMeshComponent->SetPhysicsAngularVelocityInDegrees(
			FVector(static_cast<double>(SnapDir.X) * 65.0, static_cast<double>(SnapDir.Y) * 50.0, 0.0),
			false);
	}

	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->TriggerContractFault(EContractFaultReason::ClampStructuralSnap);
		}
	}
}
