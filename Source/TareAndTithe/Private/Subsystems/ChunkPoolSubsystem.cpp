// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/ChunkPoolSubsystem.h"
#include "UDynamicMesh.h"
#include "Engine/World.h"

bool UChunkPoolSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	const UWorld* World = IsValid(Outer) ? Outer->GetWorld() : nullptr;
	return IsValid(World) && World->IsGameWorld();
}

void UChunkPoolSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (!IsValid(DynamicMeshPool))
	{
		DynamicMeshPool = NewObject<UDynamicMeshPool>(this);
	}

	AllPooledChunks.Reserve(DefaultPrewarmChunkCount);
	AvailableChunks.Reserve(DefaultPrewarmChunkCount);

	while (AllPooledChunks.Num() < DefaultPrewarmChunkCount)
	{
		ASeveredChunkActor* NewChunk = SpawnPooledChunkInstance(InWorld);
		if (IsValid(NewChunk))
		{
			AvailableChunks.Add(NewChunk);
		}
		else
		{
			break;
		}
	}
}

void UChunkPoolSubsystem::Deinitialize()
{
	for (const TObjectPtr<ASeveredChunkActor>& ChunkPtr : AllPooledChunks)
	{
		if (IsValid(ChunkPtr))
		{
			ChunkPtr->OnChunkWatchdogExpired.RemoveAll(this);
		}
	}

	AvailableChunks.Empty();
	AllPooledChunks.Empty();
	ActiveAcquisitionOrder.Empty();

	if (IsValid(DynamicMeshPool))
	{
		DynamicMeshPool->FreeAllMeshes();
		DynamicMeshPool = nullptr;
	}

	Super::Deinitialize();
}

ASeveredChunkActor* UChunkPoolSubsystem::AcquireChunk()
{
	TryRecycleOldestFloorStrayChunkIfLow();

	while (AvailableChunks.Num() > 0)
	{
		TObjectPtr<ASeveredChunkActor> Candidate = AvailableChunks.Pop(EAllowShrinking::No);
		if (IsValid(Candidate))
		{
			ActiveAcquisitionOrder.RemoveSingle(Candidate.Get());
			ActiveAcquisitionOrder.Add(Candidate.Get());
			return Candidate.Get();
		}
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	// Overflow path: dynamically expand pool so Conservation of Mass is never violated.
	ASeveredChunkActor* ExpandedChunk = SpawnPooledChunkInstance(*World);
	if (IsValid(ExpandedChunk))
	{
		ActiveAcquisitionOrder.Add(ExpandedChunk);
	}
	return ExpandedChunk;
}

void UChunkPoolSubsystem::ReleaseChunk(ASeveredChunkActor* Chunk)
{
	if (!IsValid(Chunk))
	{
		return;
	}

	ActiveAcquisitionOrder.RemoveSingle(Chunk);
	Chunk->DeactivateToPool();
	AvailableChunks.AddUnique(Chunk);
}

void UChunkPoolSubsystem::ReclaimAllActiveChunks()
{
	ActiveAcquisitionOrder.Reset();

	for (const TObjectPtr<ASeveredChunkActor>& ChunkPtr : AllPooledChunks)
	{
		ASeveredChunkActor* Chunk = ChunkPtr.Get();
		if (IsValid(Chunk))
		{
			if (IMassPayloadInterface::Execute_GetRoutingState(Chunk) != EChunkRoutingState::PooledInactive)
			{
				Chunk->DeactivateToPool();
			}
			AvailableChunks.AddUnique(Chunk);
		}
	}
}

UDynamicMesh* UChunkPoolSubsystem::RequestScratchDynamicMesh()
{
	if (!IsValid(DynamicMeshPool))
	{
		DynamicMeshPool = NewObject<UDynamicMeshPool>(this);
	}

	return IsValid(DynamicMeshPool) ? DynamicMeshPool->RequestMesh() : nullptr;
}

void UChunkPoolSubsystem::ReturnScratchDynamicMesh(UDynamicMesh* Mesh)
{
	if (IsValid(DynamicMeshPool) && IsValid(Mesh))
	{
		DynamicMeshPool->ReturnMesh(Mesh);
	}
}

ASeveredChunkActor* UChunkPoolSubsystem::SpawnPooledChunkInstance(UWorld& InWorld)
{
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	ASeveredChunkActor* ChunkActor = InWorld.SpawnActor<ASeveredChunkActor>(
		ASeveredChunkActor::StaticClass(),
		FTransform(FQuat::Identity, FVector(0.0, 0.0, -50000.0), FVector::OneVector),
		SpawnParams);

	if (IsValid(ChunkActor))
	{
		ChunkActor->DeactivateToPool();
		ChunkActor->OnChunkWatchdogExpired.AddUObject(this, &UChunkPoolSubsystem::HandlePooledChunkWatchdogExpired);
		AllPooledChunks.Add(ChunkActor);
	}

	return ChunkActor;
}

void UChunkPoolSubsystem::TryRecycleOldestFloorStrayChunkIfLow()
{
	AvailableChunks.RemoveAll([](const TObjectPtr<ASeveredChunkActor>& Entry)
	{
		return !IsValid(Entry);
	});

	if (AvailableChunks.Num() > MinFreeChunksBeforeStrayRecycle)
	{
		return;
	}

	for (int32 Idx = 0; Idx < ActiveAcquisitionOrder.Num(); )
	{
		ASeveredChunkActor* Candidate = ActiveAcquisitionOrder[Idx].Get();
		if (!IsValid(Candidate))
		{
			ActiveAcquisitionOrder.RemoveAt(Idx, EAllowShrinking::No);
			continue;
		}

		const EChunkRoutingState State = IMassPayloadInterface::Execute_GetRoutingState(Candidate);
		if (State == EChunkRoutingState::PooledInactive)
		{
			ActiveAcquisitionOrder.RemoveAt(Idx, EAllowShrinking::No);
			continue;
		}

		if (State == EChunkRoutingState::LostAsStrayTare)
		{
			ReleaseChunk(Candidate);
			if (AvailableChunks.Num() > MinFreeChunksBeforeStrayRecycle)
			{
				return;
			}
			continue;
		}

		++Idx;
	}
}

void UChunkPoolSubsystem::HandlePooledChunkWatchdogExpired(ASeveredChunkActor* ExpiredChunk)
{
	if (IsValid(ExpiredChunk))
	{
		OnAnyChunkWatchdogExpired.Broadcast(ExpiredChunk);
	}
}
