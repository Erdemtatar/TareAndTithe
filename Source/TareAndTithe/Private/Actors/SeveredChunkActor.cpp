// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/SeveredChunkActor.h"
#include "UDynamicMesh.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "GeometryScript/CollisionFunctions.h"
#include "Interfaces/QuotaReceiverInterface.h"
#include "Subsystems/ChunkPoolSubsystem.h"
#include "Engine/CollisionProfile.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FVector PooledParkingLocationCm(0.0, 0.0, -50000.0);

	bool BuildFast26DopSingleConvexChunkCollision(
		UDynamicMesh* DynMesh,
		UDynamicMeshComponent* TargetComponent,
		float MinThicknessCm)
	{
		if (!IsValid(DynMesh) || !IsValid(TargetComponent))
		{
			return false;
		}

		TArray<FVector, TInlineAllocator<32>> HullVertices;
		FBox MeshBounds(ForceInit);

		DynMesh->ProcessMesh([&HullVertices, &MeshBounds, MinThicknessCm](const UE::Geometry::FDynamicMesh3& ReadMesh)
		{
			if (ReadMesh.VertexCount() < 4)
			{
				return;
			}

			FVector Directions[26];
			double MaxDots[26];
			int32 BestVids[26];
			int32 DirCount = 0;
			for (int32 Ix = -1; Ix <= 1; ++Ix)
			{
				for (int32 Iy = -1; Iy <= 1; ++Iy)
				{
					for (int32 Iz = -1; Iz <= 1; ++Iz)
					{
						if (Ix == 0 && Iy == 0 && Iz == 0)
						{
							continue;
						}
						Directions[DirCount] = FVector(static_cast<double>(Ix), static_cast<double>(Iy), static_cast<double>(Iz));
						MaxDots[DirCount] = -TNumericLimits<double>::Max();
						BestVids[DirCount] = INDEX_NONE;
						++DirCount;
					}
				}
			}

			for (int32 Vid : ReadMesh.VertexIndicesItr())
			{
				const FVector Pos(ReadMesh.GetVertex(Vid));
				MeshBounds += Pos;
				for (int32 DirIdx = 0; DirIdx < 26; ++DirIdx)
				{
					const double Dot =
						Directions[DirIdx].X * Pos.X +
						Directions[DirIdx].Y * Pos.Y +
						Directions[DirIdx].Z * Pos.Z;
					if (Dot > MaxDots[DirIdx])
					{
						MaxDots[DirIdx] = Dot;
						BestVids[DirIdx] = Vid;
					}
				}
			}

			for (int32 DirIdx = 0; DirIdx < 26; ++DirIdx)
			{
				const int32 Vid = BestVids[DirIdx];
				if (Vid == INDEX_NONE)
				{
					continue;
				}
				const FVector Candidate(ReadMesh.GetVertex(Vid));
				bool bDuplicate = false;
				for (const FVector& Existing : HullVertices)
				{
					if (FVector::DistSquared(Candidate, Existing) <= 0.0025)
					{
						bDuplicate = true;
						break;
					}
				}
				if (!bDuplicate)
				{
					HullVertices.Add(Candidate);
				}
			}

			if (MeshBounds.IsValid && HullVertices.Num() >= 3)
			{
				const double SafeMinThick = FMath::Max(0.05, static_cast<double>(MinThicknessCm));
				const FVector Center = MeshBounds.GetCenter();
				const FVector Size = MeshBounds.GetSize();
				for (int32 Axis = 0; Axis < 3; ++Axis)
				{
					if (Size[Axis] < SafeMinThick)
					{
						FVector Offset = FVector::ZeroVector;
						Offset[Axis] = 0.5 * SafeMinThick;
						HullVertices.Add(Center - Offset);
						HullVertices.Add(Center + Offset);
					}
				}
			}
		});

		if (HullVertices.Num() < 4 || !MeshBounds.IsValid)
		{
			return false;
		}

		FKConvexElem ConvexElem;
		ConvexElem.VertexData.Reserve(HullVertices.Num());
		for (const FVector& Vert : HullVertices)
		{
			ConvexElem.VertexData.Add(Vert);
		}
		ConvexElem.UpdateElemBox();

		FKAggregateGeom FastAggGeom;
		FastAggGeom.ConvexElems.Add(MoveTemp(ConvexElem));

		TargetComponent->bUseAsyncCooking = false;
		TargetComponent->SetSimpleCollisionShapes(FastAggGeom, true);

		const UBodySetup* BodySetup = TargetComponent->GetBodySetup();
		return IsValid(BodySetup) &&
			BodySetup->AggGeom.ConvexElems.Num() > 0 &&
			BodySetup->AggGeom.ConvexElems[0].GetChaosConvexMesh().IsValid();
	}
}

ASeveredChunkActor::ASeveredChunkActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	ChunkMeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("ChunkMeshComponent"));
	SetRootComponent(ChunkMeshComponent);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlattenMatFinder(
		TEXT("/Engine/EngineMaterials/FlattenMaterial.FlattenMaterial"));
	if (FlattenMatFinder.Succeeded())
	{
		BaseRuntimeSurfaceMaterial = FlattenMatFinder.Object;
	}

	ChunkMeshComponent->PrimaryComponentTick.bCanEverTick = false;
	ChunkMeshComponent->PrimaryComponentTick.bStartWithTickEnabled = false;
	ChunkMeshComponent->BodyInstance.bUseCCD = true;
	ChunkMeshComponent->SetUseCCD(true);
	ChunkMeshComponent->SetSimulatePhysics(false);
	ChunkMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ChunkMeshComponent->SetGenerateOverlapEvents(false);
	ChunkMeshComponent->SetNotifyRigidBodyCollision(false);
	ChunkMeshComponent->SetComplexAsSimpleCollisionEnabled(false, false);

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void ASeveredChunkActor::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(ChunkMeshComponent))
	{
		ChunkMeshComponent->OnComponentHit.AddUniqueDynamic(this, &ASeveredChunkActor::HandleChunkMeshComponentHit);
	}
}

void ASeveredChunkActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearInFlightWatchdog();
	ClearTareAbyssFailsafeTimer();
	CurrentScaleSupportReceiver.Reset();

	if (IsValid(ChunkMeshComponent))
	{
		ChunkMeshComponent->OnComponentHit.RemoveDynamic(this, &ASeveredChunkActor::HandleChunkMeshComponentHit);
	}

	Super::EndPlay(EndPlayReason);
}

void ASeveredChunkActor::SetMinCollisionThicknessCm(float InMinThicknessCm)
{
	MinCollisionThicknessCm = FMath::Clamp(InMinThicknessCm, 0.05f, 2.0f);
}

void ASeveredChunkActor::ActivateFromSlice(
	UE::Geometry::FDynamicMesh3&& InCenteredMesh,
	const FTransform& InSpawnWorldTransform,
	float InAnalyticalMassKg,
	bool bInIsContaminated,
	UMaterialInterface* OuterMaterial,
	UMaterialInterface* CutCapMaterial,
	const FVector& InInitialLinearVelocityCmPerSec,
	float InWatchdogSeconds)
{
	ClearInFlightWatchdog();
	ClearTareAbyssFailsafeTimer();
	CurrentScaleSupportReceiver.Reset();

	AnalyticalMassKg = FMath::Max(0.001f, InAnalyticalMassKg);
	bVolatileContaminated = bInIsContaminated;
	bHasRearmedWatchdogOnce = false;
	ActiveWatchdogSeconds = (InWatchdogSeconds > 0.0f) ? InWatchdogSeconds : DefaultWatchdogSeconds;
	RoutingState = EChunkRoutingState::InFlight;

	SetActorTransform(InSpawnWorldTransform, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	if (IsValid(ChunkMeshComponent))
	{
		ChunkMeshComponent->OnComponentHit.AddUniqueDynamic(this, &ASeveredChunkActor::HandleChunkMeshComponentHit);

		if (!IsValid(OuterMaterial) || !IsValid(CutCapMaterial))
		{
			EnsureFallbackChunkMaterialsInitialized();
		}

		UMaterialInterface* EffectiveOuterMat = IsValid(OuterMaterial) ? OuterMaterial : FallbackChunkOuterMID.Get();
		UMaterialInterface* EffectiveCutCapMat = IsValid(CutCapMaterial) ? CutCapMaterial : FallbackChunkCutCapMID.Get();
		const float SpawnWorldTime = IsValid(GetWorld()) ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.0f;
		UMaterialInterface* StampedCutCapMat = PrepareActiveChunkCutCapMaterial(EffectiveCutCapMat, SpawnWorldTime);

		if (UDynamicMesh* DynMesh = ChunkMeshComponent->GetDynamicMesh())
		{
			const int32 SourceTriCount = InCenteredMesh.TriangleCount();
			DynMesh->SetMesh(MoveTemp(InCenteredMesh));
			ChunkMeshComponent->ConfigureMaterialSet({EffectiveOuterMat, StampedCutCapMat});
			ChunkMeshComponent->SetComplexAsSimpleCollisionEnabled(false, false);

			// Phase 12-FIX5: Sculpted Rodin chunks (> 48 tris on Contracts 04/05/06, including corner cuts) cook a single
			// 26-DOP extremal-vertex Chaos convex hull (< 0.20 ms) to bypass synchronous FConvexDecomposition3
			// and QEM hull-simplification stalls while preserving MaxConvexHulls = 2 for low-poly procedural L/J/C/Z brackets (<= 48 tris).
			const bool bHighPolySculptedChunk = (SourceTriCount > 48);
			bool bBuiltFastHighPolyHull = false;
			if (bHighPolySculptedChunk)
			{
				bBuiltFastHighPolyHull = BuildFast26DopSingleConvexChunkCollision(
					DynMesh,
					ChunkMeshComponent,
					MinCollisionThicknessCm);
			}

			if (!bBuiltFastHighPolyHull)
			{
				FGeometryScriptCollisionFromMeshOptions CollisionOptions;
				CollisionOptions.bEmitTransaction = false;
				CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
				CollisionOptions.bSimplifyHulls = true;
				CollisionOptions.ConvexHullTargetFaceCount = bHighPolySculptedChunk
					? FMath::Min(16, ConvexHullTargetFaceCount)
					: ConvexHullTargetFaceCount;
				CollisionOptions.MaxConvexHullsPerMesh = bHighPolySculptedChunk ? 1 : MaxConvexHulls;
				CollisionOptions.MinThickness = MinCollisionThicknessCm;

				UGeometryScriptLibrary_CollisionFunctions::SetDynamicMeshCollisionFromMesh(
					DynMesh,
					ChunkMeshComponent,
					CollisionOptions,
					nullptr);
			}
		}
		else
		{
			ChunkMeshComponent->ConfigureMaterialSet({EffectiveOuterMat, StampedCutCapMat});
			ChunkMeshComponent->SetComplexAsSimpleCollisionEnabled(false, false);
		}

		ChunkMeshComponent->BodyInstance.bUseCCD = true;
		ChunkMeshComponent->BodyInstance.bEnableGravity = true;
		ChunkMeshComponent->BodyInstance.SetMassOverride(AnalyticalMassKg, true);
		ChunkMeshComponent->BodyInstance.bSimulatePhysics = true;
		ChunkMeshComponent->SetGenerateOverlapEvents(true);
		ChunkMeshComponent->SetNotifyRigidBodyCollision(true);
		ChunkMeshComponent->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
		ChunkMeshComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		if (!ChunkMeshComponent->IsSimulatingPhysics())
		{
			ChunkMeshComponent->SetSimulatePhysics(true);
		}
		ChunkMeshComponent->WakeAllRigidBodies();
		ChunkMeshComponent->SetPhysicsLinearVelocity(InInitialLinearVelocityCmPerSec, false);
		ChunkMeshComponent->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector, false);
	}

	StartInFlightWatchdog(ActiveWatchdogSeconds);
}

bool ASeveredChunkActor::TryRearmWatchdogIfActivelyFalling(float MaxDownwardVelocityZCmPerSec, float MinCellFloorZCm)
{
	if (RoutingState != EChunkRoutingState::InFlight || bHasRearmedWatchdogOnce)
	{
		return false;
	}

	const FVector WorldLoc = GetActorLocation();
	if (WorldLoc.Z <= static_cast<double>(MinCellFloorZCm))
	{
		return false;
	}

	FVector LinearVel = GetVelocity();
	if (IsValid(ChunkMeshComponent) && ChunkMeshComponent->IsSimulatingPhysics())
	{
		LinearVel = ChunkMeshComponent->GetPhysicsLinearVelocity();
	}

	if (LinearVel.Z < static_cast<double>(MaxDownwardVelocityZCmPerSec))
	{
		bHasRearmedWatchdogOnce = true;
		StartInFlightWatchdog(ActiveWatchdogSeconds);
		return true;
	}

	return false;
}

void ASeveredChunkActor::ConfigureCutCapThermalCooling(
	bool bEnableCooling,
	float InCoolingDurationSeconds,
	float InSpawnWorldTimeSeconds)
{
	if (IsValid(ActiveChunkCutCapMID))
	{
		ActiveChunkCutCapMID->SetScalarParameterValue(TEXT("CoolingCurveEnabled"), bEnableCooling ? 1.0f : 0.0f);
		ActiveChunkCutCapMID->SetScalarParameterValue(
			TEXT("CoolingDurationSeconds"),
			FMath::Clamp(InCoolingDurationSeconds, 0.50f, 8.00f));
		ActiveChunkCutCapMID->SetScalarParameterValue(TEXT("CutSpawnTimeSeconds"), InSpawnWorldTimeSeconds);
	}
}

UMaterialInterface* ASeveredChunkActor::PrepareActiveChunkCutCapMaterial(
	UMaterialInterface* BaseCutCapMat,
	float InSpawnWorldTimeSeconds)
{
	if (!IsValid(BaseCutCapMat))
	{
		return nullptr;
	}
	if (BaseCutCapMat == FallbackChunkCutCapMID.Get())
	{
		FallbackChunkCutCapMID->SetScalarParameterValue(TEXT("CutSpawnTimeSeconds"), InSpawnWorldTimeSeconds);
		return FallbackChunkCutCapMID.Get();
	}

	UMaterialInterface* ResolvedParent = BaseCutCapMat;
	if (const UMaterialInstanceDynamic* ExistingMID = Cast<UMaterialInstanceDynamic>(BaseCutCapMat))
	{
		if (IsValid(ExistingMID->Parent))
		{
			ResolvedParent = ExistingMID->Parent;
		}
	}

	if (!IsValid(ActiveChunkCutCapMID) || CachedCutCapParentMaterial != ResolvedParent)
	{
		ActiveChunkCutCapMID = UMaterialInstanceDynamic::Create(ResolvedParent, this);
		CachedCutCapParentMaterial = ResolvedParent;
	}

	if (IsValid(ActiveChunkCutCapMID))
	{
		ActiveChunkCutCapMID->SetScalarParameterValue(TEXT("CoolingCurveEnabled"), 1.0f);
		ActiveChunkCutCapMID->SetScalarParameterValue(TEXT("CoolingDurationSeconds"), 3.85f);
		ActiveChunkCutCapMID->SetScalarParameterValue(TEXT("CutSpawnTimeSeconds"), InSpawnWorldTimeSeconds);
		return ActiveChunkCutCapMID.Get();
	}

	return BaseCutCapMat;
}

void ASeveredChunkActor::DeactivateToPool()
{
	ClearInFlightWatchdog();
	ClearTareAbyssFailsafeTimer();
	CurrentScaleSupportReceiver.Reset();

	RoutingState = EChunkRoutingState::PooledInactive;
	AnalyticalMassKg = 0.0f;
	bVolatileContaminated = false;
	bHasRearmedWatchdogOnce = false;

	if (IsValid(ActiveChunkCutCapMID))
	{
		ActiveChunkCutCapMID->SetScalarParameterValue(TEXT("CutSpawnTimeSeconds"), -100.0f);
	}

	if (IsValid(ChunkMeshComponent))
	{
		ChunkMeshComponent->SetGenerateOverlapEvents(false);
		ChunkMeshComponent->SetNotifyRigidBodyCollision(false);

		if (ChunkMeshComponent->IsSimulatingPhysics())
		{
			ChunkMeshComponent->SetPhysicsLinearVelocity(FVector::ZeroVector, false);
			ChunkMeshComponent->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector, false);
			ChunkMeshComponent->PutAllRigidBodiesToSleep();
		}

		ChunkMeshComponent->SetSimulatePhysics(false);
		ChunkMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		UGeometryScriptLibrary_CollisionFunctions::ResetDynamicMeshCollision(ChunkMeshComponent, false, nullptr);

		if (UDynamicMesh* DynMesh = ChunkMeshComponent->GetDynamicMesh())
		{
			DynMesh->Reset();
		}
	}

	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	SetActorLocation(PooledParkingLocationCm, false, nullptr, ETeleportType::TeleportPhysics);
}

float ASeveredChunkActor::GetAnalyticalMassKg_Implementation() const
{
	return AnalyticalMassKg;
}

bool ASeveredChunkActor::IsVolatileContaminated_Implementation() const
{
	return bVolatileContaminated;
}

EChunkRoutingState ASeveredChunkActor::GetRoutingState_Implementation() const
{
	return RoutingState;
}

void ASeveredChunkActor::NotifyRoutingStateChanged_Implementation(EChunkRoutingState NewState)
{
	if (RoutingState == NewState)
	{
		return;
	}

	RoutingState = NewState;

	if (RoutingState != EChunkRoutingState::OnScalePan)
	{
		CurrentScaleSupportReceiver.Reset();
	}

	if (RoutingState != EChunkRoutingState::ConsumedAsTare)
	{
		ClearTareAbyssFailsafeTimer();
	}

	if (RoutingState == EChunkRoutingState::InFlight)
	{
		bHasRearmedWatchdogOnce = false;
		StartInFlightWatchdog(ActiveWatchdogSeconds);
	}
	else
	{
		ClearInFlightWatchdog();
	}
}

void ASeveredChunkActor::ReturnToChunkPool_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		if (UChunkPoolSubsystem* PoolSubsystem = World->GetSubsystem<UChunkPoolSubsystem>())
		{
			PoolSubsystem->ReleaseChunk(this);
			return;
		}
	}

	DeactivateToPool();
}

UPrimitiveComponent* ASeveredChunkActor::GetPayloadPhysicsPrimitive_Implementation() const
{
	return ChunkMeshComponent;
}

AActor* ASeveredChunkActor::GetScaleSupportReceiverActor_Implementation() const
{
	return CurrentScaleSupportReceiver.Get();
}

void ASeveredChunkActor::SetScaleSupportReceiverActor_Implementation(AActor* NewSupportReceiver)
{
	CurrentScaleSupportReceiver = NewSupportReceiver;
}

void ASeveredChunkActor::ArmTareAbyssFailsafeTimer_Implementation(float FailsafeDurationSeconds)
{
	if (RoutingState == EChunkRoutingState::PooledInactive)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (!World->GetTimerManager().IsTimerActive(TareAbyssFailsafeTimerHandle))
		{
			const float SafeDuration = FMath::Max(0.25f, FailsafeDurationSeconds);
			World->GetTimerManager().SetTimer(
				TareAbyssFailsafeTimerHandle,
				this,
				&ASeveredChunkActor::HandleTareAbyssFailsafeExpired,
				SafeDuration,
				false);
		}
	}
}

void ASeveredChunkActor::HandleChunkMeshComponentHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (!IsValid(OtherActor) || OtherActor == this)
	{
		return;
	}

	if (RoutingState == EChunkRoutingState::InFlight)
	{
		if (IsValid(OtherComp) &&
			OtherActor->Implements<UQuotaReceiverInterface>() &&
			IQuotaReceiverInterface::Execute_IsScalePanSupportComponent(OtherActor, OtherComp))
		{
			IQuotaReceiverInterface::Execute_NotifyPayloadScaleSupportImpact(
				OtherActor,
				this,
				HitComponent,
				FVector(Hit.ImpactPoint),
				NormalImpulse);
			return;
		}

		if (OtherActor->Implements<UMassPayloadInterface>() &&
			IMassPayloadInterface::Execute_GetRoutingState(OtherActor) == EChunkRoutingState::OnScalePan)
		{
			AActor* SupportReceiver = IMassPayloadInterface::Execute_GetScaleSupportReceiverActor(OtherActor);
			if (IsValid(SupportReceiver) && SupportReceiver->Implements<UQuotaReceiverInterface>())
			{
				IQuotaReceiverInterface::Execute_NotifyPayloadScaleSupportImpact(
					SupportReceiver,
					this,
					HitComponent,
					FVector(Hit.ImpactPoint),
					NormalImpulse);
			}
		}
		return;
	}

	// Bidirectional stack contact guard: if this chunk is already OnScalePan and Chaos reports a hit with an InFlight chunk resting/landing on top of us, propagate upward via interface.
	if (RoutingState == EChunkRoutingState::OnScalePan)
	{
		AActor* SupportReceiver = CurrentScaleSupportReceiver.Get();
		if (IsValid(SupportReceiver) &&
			SupportReceiver->Implements<UQuotaReceiverInterface>() &&
			OtherActor->Implements<UMassPayloadInterface>() &&
			IMassPayloadInterface::Execute_GetRoutingState(OtherActor) == EChunkRoutingState::InFlight)
		{
			UPrimitiveComponent* OtherPayloadPrim = IsValid(OtherComp)
				? OtherComp
				: IMassPayloadInterface::Execute_GetPayloadPhysicsPrimitive(OtherActor);
			IQuotaReceiverInterface::Execute_NotifyPayloadScaleSupportImpact(
				SupportReceiver,
				OtherActor,
				OtherPayloadPrim,
				FVector(Hit.ImpactPoint),
				-NormalImpulse);
		}
	}
}

void ASeveredChunkActor::EnsureFallbackChunkMaterialsInitialized()
{
	if (!IsValid(BaseRuntimeSurfaceMaterial))
	{
		return;
	}

	if (!IsValid(FallbackChunkOuterMID))
	{
		FallbackChunkOuterMID = UMaterialInstanceDynamic::Create(BaseRuntimeSurfaceMaterial, this);
		if (IsValid(FallbackChunkOuterMID))
		{
			const FLinearColor OuterSlagColor(0.065f, 0.072f, 0.085f, 1.0f);
			FallbackChunkOuterMID->SetVectorParameterValue(TEXT("BaseColorConst"), OuterSlagColor);
			FallbackChunkOuterMID->SetVectorParameterValue(TEXT("Color"), OuterSlagColor);
			FallbackChunkOuterMID->SetVectorParameterValue(TEXT("EmissiveColorConst"), FLinearColor::Black);
			FallbackChunkOuterMID->SetScalarParameterValue(TEXT("EmissiveScale"), 0.0f);
			FallbackChunkOuterMID->SetScalarParameterValue(TEXT("RoughnessConst"), 0.72f);
			FallbackChunkOuterMID->SetScalarParameterValue(TEXT("MetallicConst"), 0.28f);
		}
	}

	if (!IsValid(FallbackChunkCutCapMID))
	{
		FallbackChunkCutCapMID = UMaterialInstanceDynamic::Create(BaseRuntimeSurfaceMaterial, this);
		if (IsValid(FallbackChunkCutCapMID))
		{
			const FLinearColor MoltenBaseColor(1.00f, 0.30f, 0.015f, 1.0f);
			const FLinearColor MoltenEmissiveColor(1.65f, 0.36f, 0.015f, 1.0f);
			FallbackChunkCutCapMID->SetVectorParameterValue(TEXT("BaseColorConst"), MoltenBaseColor);
			FallbackChunkCutCapMID->SetVectorParameterValue(TEXT("Color"), MoltenBaseColor);
			FallbackChunkCutCapMID->SetVectorParameterValue(TEXT("EmissiveColorConst"), MoltenEmissiveColor);
			FallbackChunkCutCapMID->SetScalarParameterValue(TEXT("EmissiveScale"), 1.0f);
			FallbackChunkCutCapMID->SetScalarParameterValue(TEXT("RoughnessConst"), 0.28f);
			FallbackChunkCutCapMID->SetScalarParameterValue(TEXT("MetallicConst"), 0.10f);
		}
	}
}

void ASeveredChunkActor::StartInFlightWatchdog(float DurationSeconds)
{
	ClearInFlightWatchdog();

	if (UWorld* World = GetWorld())
	{
		const float ClampedDuration = FMath::Max(0.2f, DurationSeconds);
		World->GetTimerManager().SetTimer(
			WatchdogTimerHandle,
			this,
			&ASeveredChunkActor::HandleWatchdogExpired,
			ClampedDuration,
			false);
	}
}

void ASeveredChunkActor::ClearInFlightWatchdog()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WatchdogTimerHandle);
	}
}

void ASeveredChunkActor::HandleWatchdogExpired()
{
	if (RoutingState == EChunkRoutingState::InFlight)
	{
		OnChunkWatchdogExpired.Broadcast(this);
	}
}

void ASeveredChunkActor::ClearTareAbyssFailsafeTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TareAbyssFailsafeTimerHandle);
	}
}

void ASeveredChunkActor::HandleTareAbyssFailsafeExpired()
{
	if (RoutingState == EChunkRoutingState::ConsumedAsTare)
	{
		ReturnToChunkPool_Implementation();
	}
}
