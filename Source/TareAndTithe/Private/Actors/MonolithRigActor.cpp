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
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GeometryScript/CollisionFunctions.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Subsystems/InspectionShiftSubsystem.h"
#include "UObject/ConstructorHelpers.h"

AMonolithRigActor::AMonolithRigActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	ClampPivotRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ClampPivotRoot"));
	SetRootComponent(ClampPivotRoot);
	ClampPivotRoot->PrimaryComponentTick.bCanEverTick = false;

	TiltingMountComponent = CreateDefaultSubobject<USceneComponent>(TEXT("TiltingMountComponent"));
	TiltingMountComponent->SetupAttachment(ClampPivotRoot);
	TiltingMountComponent->PrimaryComponentTick.bCanEverTick = false;

	ClampJawVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ClampJawVisual"));
	ClampJawVisual->SetupAttachment(TiltingMountComponent);
	ClampJawVisual->PrimaryComponentTick.bCanEverTick = false;
	ClampJawVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ClampJawVisual->SetGenerateOverlapEvents(false);
	ClampJawVisual->SetRelativeLocation(FVector(0.0, 0.0, 2.0));
	ClampJawVisual->SetRelativeScale3D(FVector(0.30, 0.30, 0.14));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		ClampJawVisual->SetStaticMesh(CubeMeshFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMatFinder(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMatFinder.Succeeded())
	{
		BaseGreyboxShapeMaterial = BasicMatFinder.Object;
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
		MonolithSliceComponent->SetFallbackMaterials(FallbackOuterMID, FallbackCutCapMID);
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

	EnsureGreyboxMaterialsInitialized();
	MonolithSliceComponent->SetFallbackMaterials(FallbackOuterMID, FallbackCutCapMID);

	// Restore kinematic hierarchy in case previous contract ended via ClampStructuralSnap freefall.
	RemainderMeshComponent->SetSimulatePhysics(false);
	RemainderMeshComponent->SetGenerateOverlapEvents(false);
	RemainderMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RemainderMeshComponent->AttachToComponent(
		TiltingMountComponent,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	const double HalfHeightCm = 0.5 * FMath::Max(5.0, static_cast<double>(Contract->DimensionsCm.Z));
	RemainderMeshComponent->SetRelativeLocationAndRotation(FVector(0.0, 0.0, -HalfHeightCm), FQuat::Identity);
	RemainderMeshComponent->SetRelativeScale3D(FVector::OneVector);

	if (IsValid(ClampJawVisual))
	{
		const double JawScaleX = FMath::Max(0.12, (static_cast<double>(Contract->DimensionsCm.X) * 0.62) / 100.0);
		const double JawScaleY = FMath::Max(0.12, (static_cast<double>(Contract->DimensionsCm.Y) * 0.62) / 100.0);
		ClampJawVisual->SetRelativeLocation(FVector(0.0, 0.0, 2.0));
		ClampJawVisual->SetRelativeScale3D(FVector(JawScaleX, JawScaleY, 0.14));
	}

	ClampTorqueSolverComponent->SetNeutralPivotTransform(ClampPivotRoot->GetComponentTransform());
	ClampTorqueSolverComponent->ResetClampState();

	if (!MonolithSliceComponent->InitializeMonolithFromContract(Contract, ShiftConfig))
	{
		return false;
	}

	VeinScannerComponent->InitializeFromContract(Contract);

	const float InitialMassKg = MonolithSliceComponent->GetRemainderMassKg();
	const FVector MountLocalCoMCm = TransformRemainderCoMToMountLocal(MonolithSliceComponent->GetRemainderLocalCoMCm());
	ClampTorqueSolverComponent->InitializeFromContract(Contract, InitialMassKg, MountLocalCoMCm);

	OutInitialMassKg = InitialMassKg;
	return true;
}

void AMonolithRigActor::EnsureGreyboxMaterialsInitialized()
{
	if (!IsValid(BaseGreyboxShapeMaterial))
	{
		return;
	}

	if (!IsValid(FallbackOuterMID))
	{
		FallbackOuterMID = UMaterialInstanceDynamic::Create(BaseGreyboxShapeMaterial, this);
		if (IsValid(FallbackOuterMID))
		{
			FallbackOuterMID->SetVectorParameterValue(TEXT("Color"), GreyboxOuterShellColor);
		}
	}

	if (!IsValid(FallbackCutCapMID))
	{
		FallbackCutCapMID = UMaterialInstanceDynamic::Create(BaseGreyboxShapeMaterial, this);
		if (IsValid(FallbackCutCapMID))
		{
			FallbackCutCapMID->SetVectorParameterValue(TEXT("Color"), GreyboxMoltenCapColor);
		}
	}

	if (!IsValid(ClampJawMID) && IsValid(ClampJawVisual))
	{
		ClampJawMID = UMaterialInstanceDynamic::Create(BaseGreyboxShapeMaterial, this);
		if (IsValid(ClampJawMID))
		{
			ClampJawMID->SetVectorParameterValue(TEXT("Color"), GreyboxClampJawColor);
			ClampJawVisual->SetMaterial(0, ClampJawMID);
		}
	}
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
	// Drop snapped remainder under Chaos freefall without overlap generation during the 1.2s fault reset window.
	if (IsValid(RemainderMeshComponent))
	{
		FGeometryScriptCollisionFromMeshOptions SnapCollisionOptions;
		SnapCollisionOptions.bEmitTransaction = false;
		SnapCollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
		SnapCollisionOptions.bSimplifyHulls = true;
		SnapCollisionOptions.MaxConvexHullsPerMesh = 1;
		SnapCollisionOptions.ConvexHullTargetFaceCount = 12;

		UGeometryScriptLibrary_CollisionFunctions::SetDynamicMeshCollisionFromMesh(
			RemainderMeshComponent->GetDynamicMesh(),
			RemainderMeshComponent,
			SnapCollisionOptions,
			nullptr);

		RemainderMeshComponent->SetComplexAsSimpleCollisionEnabled(false, true);
		RemainderMeshComponent->SetGenerateOverlapEvents(false);
		RemainderMeshComponent->SetCollisionProfileName(TEXT("PhysicsActor"));
		const float SafeMassKg = FMath::Max(1.0f, QueryCurrentRemainderMassKg());
		RemainderMeshComponent->SetMassOverrideInKg(NAME_None, SafeMassKg, true);
		RemainderMeshComponent->SetSimulatePhysics(true);
	}

	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->TriggerContractFault(EContractFaultReason::ClampStructuralSnap);
		}
	}
}
