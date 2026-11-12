// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/SeveredChunkActor.h"
#include "UDynamicMesh.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "GeometryScript/CollisionFunctions.h"
#include "Subsystems/ChunkPoolSubsystem.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

namespace
{
	const FVector PooledParkingLocationCm(0.0, 0.0, -50000.0);
}

ASeveredChunkActor::ASeveredChunkActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	ChunkMeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("ChunkMeshComponent"));
	SetRootComponent(ChunkMeshComponent);

	ChunkMeshComponent->PrimaryComponentTick.bCanEverTick = false;
	ChunkMeshComponent->PrimaryComponentTick.bStartWithTickEnabled = false;
	ChunkMeshComponent->SetSimulatePhysics(false);
	ChunkMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ChunkMeshComponent->SetGenerateOverlapEvents(false);
	ChunkMeshComponent->SetComplexAsSimpleCollisionEnabled(false, false);

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void ASeveredChunkActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearInFlightWatchdog();
	Super::EndPlay(EndPlayReason);
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

	AnalyticalMassKg = FMath::Max(0.001f, InAnalyticalMassKg);
	bVolatileContaminated = bInIsContaminated;
	ActiveWatchdogSeconds = (InWatchdogSeconds > 0.0f) ? InWatchdogSeconds : DefaultWatchdogSeconds;

	SetActorTransform(InSpawnWorldTransform, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	if (IsValid(ChunkMeshComponent))
	{
		ChunkMeshComponent->SetMaterial(0, OuterMaterial);
		ChunkMeshComponent->SetMaterial(1, CutCapMaterial);

		if (UDynamicMesh* DynMesh = ChunkMeshComponent->GetDynamicMesh())
		{
			DynMesh->SetMesh(MoveTemp(InCenteredMesh));

			FGeometryScriptCollisionFromMeshOptions CollisionOptions;
			CollisionOptions.bEmitTransaction = false;
			CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
			CollisionOptions.bSimplifyHulls = true;
			CollisionOptions.ConvexHullTargetFaceCount = ConvexHullTargetFaceCount;
			CollisionOptions.MaxConvexHullsPerMesh = MaxConvexHulls;
			CollisionOptions.MinThickness = MinCollisionThicknessCm;

			UGeometryScriptLibrary_CollisionFunctions::SetDynamicMeshCollisionFromMesh(
				DynMesh,
				ChunkMeshComponent,
				CollisionOptions,
				nullptr);
		}

		ChunkMeshComponent->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
		ChunkMeshComponent->SetGenerateOverlapEvents(true);
		ChunkMeshComponent->SetComplexAsSimpleCollisionEnabled(false, true);
		ChunkMeshComponent->SetMassOverrideInKg(NAME_None, AnalyticalMassKg, true);
		ChunkMeshComponent->SetSimulatePhysics(true);
		ChunkMeshComponent->WakeAllRigidBodies();
		ChunkMeshComponent->SetPhysicsLinearVelocity(InInitialLinearVelocityCmPerSec, false);
		ChunkMeshComponent->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector, false);
	}

	RoutingState = EChunkRoutingState::InFlight;
	StartInFlightWatchdog(ActiveWatchdogSeconds);
}

void ASeveredChunkActor::DeactivateToPool()
{
	ClearInFlightWatchdog();

	RoutingState = EChunkRoutingState::PooledInactive;
	AnalyticalMassKg = 0.0f;
	bVolatileContaminated = false;

	if (IsValid(ChunkMeshComponent))
	{
		if (ChunkMeshComponent->IsSimulatingPhysics())
		{
			ChunkMeshComponent->SetPhysicsLinearVelocity(FVector::ZeroVector, false);
			ChunkMeshComponent->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector, false);
			ChunkMeshComponent->PutAllRigidBodiesToSleep();
		}

		ChunkMeshComponent->SetSimulatePhysics(false);
		ChunkMeshComponent->SetGenerateOverlapEvents(false);
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

	if (RoutingState == EChunkRoutingState::InFlight)
	{
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
