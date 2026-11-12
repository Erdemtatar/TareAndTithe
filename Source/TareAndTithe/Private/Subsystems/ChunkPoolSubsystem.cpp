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

	const UWorld* World = Cast<UWorld>(Outer);
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

	if (IsValid(DynamicMeshPool))
	{
		DynamicMeshPool->FreeAllMeshes();
		DynamicMeshPool = nullptr;
	}

	Super::Deinitialize();
}

ASeveredChunkActor* UChunkPoolSubsystem::AcquireChunk()
{
	while (AvailableChunks.Num() > 0)
	{
		TObjectPtr<ASeveredChunkActor> Candidate = AvailableChunks.Pop(EAllowShrinking::No);
		if (IsValid(Candidate))
		{
			return Candidate.Get();
		}
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	// Overflow path: dynamically expand pool so Conservation of Mass is never violated.
	return SpawnPooledChunkInstance(*World);
}

void UChunkPoolSubsystem::ReleaseChunk(ASeveredChunkActor* Chunk)
{
	if (!IsValid(Chunk))
	{
		return;
	}

	Chunk->DeactivateToPool();
	AvailableChunks.AddUnique(Chunk);
}

void UChunkPoolSubsystem::ReclaimAllActiveChunks()
{
	for (const TObjectPtr<ASeveredChunkActor>& ChunkPtr : AllPooledChunks)
	{
		ASeveredChunkActor* Chunk = ChunkPtr.Get();
		if (IsValid(Chunk) && IMassPayloadInterface::Execute_GetRoutingState(Chunk) != EChunkRoutingState::PooledInactive)
		{
			ReleaseChunk(Chunk);
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

void UChunkPoolSubsystem::HandlePooledChunkWatchdogExpired(ASeveredChunkActor* ExpiredChunk)
{
	if (IsValid(ExpiredChunk))
	{
		OnAnyChunkWatchdogExpired.Broadcast(ExpiredChunk);
	}
}
