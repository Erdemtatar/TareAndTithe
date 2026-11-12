// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/MonolithRigActor.h"
#include "Actors/SeveredChunkActor.h"
#include "Components/ClampTorqueSolverComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/MonolithSliceComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VeinScannerComponent.h"
#include "Data/MonolithContractDataAsset.h"
#include "Data/ShiftProgressionDataAsset.h"
#include "Data/TareTithePresentationDataAsset.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GeometryScript/CollisionFunctions.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
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
	RemainderMeshComponent->SetSimulatePhysics(false);
	RemainderMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RemainderMeshComponent->SetGenerateOverlapEvents(false);
	RemainderMeshComponent->SetRelativeLocation(FVector(0.0, 0.0, -40.0));

	MonolithSliceComponent = CreateDefaultSubobject<UMonolithSliceComponent>(TEXT("MonolithSliceComponent"));
	VeinScannerComponent = CreateDefaultSubobject<UVeinScannerComponent>(TEXT("VeinScannerComponent"));
	ClampTorqueSolverComponent = CreateDefaultSubobject<UClampTorqueSolverComponent>(TEXT("ClampTorqueSolverComponent"));
}

void AMonolithRigActor::BeginPlay()
{
	Super::BeginPlay();

	EnsureGreyboxMaterialsInitialized();

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
			if (const UTareTithePresentationDataAsset* ExistingPresentation = ShiftSubsystem->GetActivePresentationAsset())
			{
				ApplyPresentationData(ExistingPresentation);
			}

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
	const FVector& PlaneNormalWorld)
{
	if (!IsValid(MonolithSliceComponent) || !IsValid(VeinScannerComponent))
	{
		return FSlicePreviewTelemetry();
	}

	const bool bIntersects = MonolithSliceComponent->DoesPlaneIntersectRemainder(PlaneOriginWorld, PlaneNormalWorld);
	return VeinScannerComponent->EvaluateWorldPlane(PlaneOriginWorld, PlaneNormalWorld, bIntersects);
}

FSliceResultSummary AMonolithRigActor::CommitSlicePlane_Implementation(
	const FVector& PlaneOriginWorld,
	const FVector& PlaneNormalWorld)
{
	FSliceResultSummary Summary;
	if (!IsValid(MonolithSliceComponent) || !IsValid(VeinScannerComponent))
	{
		return Summary;
	}

	TArray<ASeveredChunkActor*> SpawnedChunks;
	Summary = MonolithSliceComponent->ExecuteSlice(
		PlaneOriginWorld,
		PlaneNormalWorld,
		VeinScannerComponent->GetMutableActiveVeins(),
		SpawnedChunks);

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
		ClampTorqueSolverComponent->UpdateRemainderLoad(Summary.NewRemainderMassKg, MountLocalCoMCm);
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
	RemainderMeshComponent->SetSimulatePhysics(false);
	RemainderMeshComponent->SetEnableGravity(false);
	RemainderMeshComponent->SetGenerateOverlapEvents(false);
	RemainderMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RemainderMeshComponent->AttachToComponent(
		TiltingMountComponent,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	RemainderMeshComponent->SetRelativeScale3D(FVector::OneVector);

	ClampTorqueSolverComponent->SetNeutralPivotTransform(ClampPivotRoot->GetComponentTransform());
	ClampTorqueSolverComponent->ResetClampState();

	if (!MonolithSliceComponent->InitializeMonolithFromContract(Contract, ShiftConfig))
	{
		return false;
	}

	RemainderMeshComponent->ConfigureMaterialSet({
		ResolveEffectiveOuterMaterial(Contract),
		ResolveEffectiveCutCapMaterial(Contract)
	});

	// Align top of generated/imported mesh (whether Center or Base origin) cleanly inside TiltingMountComponent's clamp jaws.
	const FBox LocalBounds = MonolithSliceComponent->GetRemainderLocalBounds();
	const double TopLocalZ = LocalBounds.IsValid
		? LocalBounds.Max.Z
		: (0.5 * FMath::Max(5.0, static_cast<double>(Contract->DimensionsCm.Z)));
	RemainderMeshComponent->SetRelativeLocationAndRotation(FVector(0.0, 0.0, -TopLocalZ), FQuat::Identity);
	RemainderMeshComponent->UpdateBounds();

	if (IsValid(ClampJawVisual) && !bHasCustomClampJawMesh)
	{
		const double JawScaleX = FMath::Max(0.14, (static_cast<double>(Contract->DimensionsCm.X) * 0.68) / 100.0);
		const double JawScaleY = FMath::Max(0.14, (static_cast<double>(Contract->DimensionsCm.Y) * 0.68) / 100.0);
		ClampJawVisual->SetRelativeLocation(FVector(0.0, 0.0, -2.0));
		ClampJawVisual->SetRelativeScale3D(FVector(JawScaleX, JawScaleY, 0.16));
	}

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
	if (IsValid(RemainderMeshComponent))
	{
		return RemainderMeshComponent->GetRelativeTransform().TransformPosition(RemainderLocalCoMCm);
	}
	return RemainderLocalCoMCm;
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
	// Detach and drop snapped remainder under bounded convex Chaos freefall without overlap generation during the fault reset window.
	if (IsValid(RemainderMeshComponent))
	{
		RemainderMeshComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

		FGeometryScriptCollisionFromMeshOptions SnapCollisionOptions;
		SnapCollisionOptions.bEmitTransaction = false;
		SnapCollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
		SnapCollisionOptions.bSimplifyHulls = true;
		SnapCollisionOptions.MaxConvexHullsPerMesh = 2;
		SnapCollisionOptions.ConvexHullTargetFaceCount = 16;

		UGeometryScriptLibrary_CollisionFunctions::SetDynamicMeshCollisionFromMesh(
			RemainderMeshComponent->GetDynamicMesh(),
			RemainderMeshComponent,
			SnapCollisionOptions,
			nullptr);

		RemainderMeshComponent->SetComplexAsSimpleCollisionEnabled(false, true);
		RemainderMeshComponent->SetGenerateOverlapEvents(false);
		RemainderMeshComponent->SetCollisionProfileName(TEXT("PhysicsActor"));
		RemainderMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		RemainderMeshComponent->SetEnableGravity(true);
		RemainderMeshComponent->RecreatePhysicsState();

		const float SafeMassKg = FMath::Max(1.0f, QueryCurrentRemainderMassKg());
		RemainderMeshComponent->SetMassOverrideInKg(NAME_None, SafeMassKg, true);
		RemainderMeshComponent->SetSimulatePhysics(true);
		RemainderMeshComponent->WakeAllRigidBodies();
	}

	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->TriggerContractFault(EContractFaultReason::ClampStructuralSnap);
		}
	}
}
