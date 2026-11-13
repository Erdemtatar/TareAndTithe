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

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlattenMatFinder(
		TEXT("/Engine/EngineMaterials/FlattenMaterial.FlattenMaterial"));
	if (FlattenMatFinder.Succeeded())
	{
		BaseGreyboxShapeMaterial = FlattenMatFinder.Object;
		ClampJawVisual->SetMaterial(0, BaseGreyboxShapeMaterial);
	}

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

	// Align actual upper grip cross-section centroid directly under TiltingMountComponent (0, 0) seated inside the jaw pocket,
	// so asymmetric shapes (L-Blocks, tapered Rodin rocks) hang physically by their upper stem while overhangs generate real torque.
	const FBox LocalBounds = MonolithSliceComponent->GetRemainderLocalBounds();
	const FBox TopGripBounds = MonolithSliceComponent->GetTopGripLocalBounds();
	const FVector TopGripCenter = TopGripBounds.IsValid ? TopGripBounds.GetCenter() : FVector::ZeroVector;
	const double TopLocalZ = LocalBounds.IsValid
		? LocalBounds.Max.Z
		: (0.5 * FMath::Max(5.0, static_cast<double>(Contract->DimensionsCm.Z)));
	RemainderMeshComponent->SetRelativeLocationAndRotation(
		FVector(-TopGripCenter.X, -TopGripCenter.Y, -TopLocalZ + static_cast<double>(JawPocketSeatEmbedZCm)),
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
			double RatioMountY)
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

			VisualComp->SetRelativeLocation(FVector(SlotLoc.X * RatioMountX, SlotLoc.Y * RatioMountY, SlotLoc.Z));
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
		ApplyMountXYScaleToSlot(ClampJawVisual, JawSlotXf, JawRatioX, JawRatioY);

		if (IsValid(ClampSpringHousingVisual) && IsValid(PresentationData->ClampSpringHousingMeshSlot.Mesh))
		{
			const FTransform& HousingSlotXf = PresentationData->ClampSpringHousingMeshSlot.VisualLocalTransform;
			const FVector BaseHousingOuterCm = ComputeSlotOuterSizeCm(
				PresentationData->ClampSpringHousingMeshSlot.Mesh,
				HousingSlotXf,
				FVector(48.0, 48.0, 40.0));

			const double TargetHousingXCm = FMath::Clamp(
				FMath::Max(BaseHousingOuterCm.X, TargetOuterXCm * 1.06),
				BaseHousingOuterCm.X,
				MaxJawOuterCm + 4.0);
			const double TargetHousingYCm = FMath::Clamp(
				FMath::Max(BaseHousingOuterCm.Y, TargetOuterYCm * 1.06),
				BaseHousingOuterCm.Y,
				MaxJawOuterCm + 4.0);

			const double HousingRatioX = TargetHousingXCm / FMath::Max(1.0, BaseHousingOuterCm.X);
			const double HousingRatioY = TargetHousingYCm / FMath::Max(1.0, BaseHousingOuterCm.Y);
			ApplyMountXYScaleToSlot(ClampSpringHousingVisual, HousingSlotXf, HousingRatioX, HousingRatioY);
		}
		return;
	}

	const double GreyboxJawScaleX = FMath::Clamp((GripWidthX + (2.0 * ProtrusionCm)) / 100.0, 0.16, 0.72);
	const double GreyboxJawScaleY = FMath::Clamp((GripWidthY + (2.0 * ProtrusionCm)) / 100.0, 0.16, 0.72);
	ClampJawVisual->SetRelativeLocation(FVector(0.0, 0.0, -2.0));
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
