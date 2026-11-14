// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Actors/SeveredChunkActor.h"
#include "ChunkPoolSubsystem.generated.h"

class UDynamicMesh;
class UDynamicMeshPool;

UCLASS()
class TAREANDTITHE_API UChunkPoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 DefaultPrewarmChunkCount = 16;
	static constexpr int32 MinFreeChunksBeforeStrayRecycle = 2;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	ASeveredChunkActor* AcquireChunk();
	void ReleaseChunk(ASeveredChunkActor* Chunk);
	void ReclaimAllActiveChunks();

	UDynamicMeshPool* GetDynamicMeshPool() const { return DynamicMeshPool; }
	UDynamicMesh* RequestScratchDynamicMesh();
	void ReturnScratchDynamicMesh(UDynamicMesh* Mesh);

	int32 GetTotalPooledChunkCount() const { return AllPooledChunks.Num(); }
	int32 GetAvailableChunkCount() const { return AvailableChunks.Num(); }

	FOnChunkWatchdogExpired OnAnyChunkWatchdogExpired;

private:
	ASeveredChunkActor* SpawnPooledChunkInstance(UWorld& InWorld);
	void TryRecycleOldestFloorStrayChunkIfLow();
	void HandlePooledChunkWatchdogExpired(ASeveredChunkActor* ExpiredChunk);

	UPROPERTY(Transient)
	TObjectPtr<UDynamicMeshPool> DynamicMeshPool;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASeveredChunkActor>> AllPooledChunks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASeveredChunkActor>> AvailableChunks;

	TArray<TWeakObjectPtr<ASeveredChunkActor>> ActiveAcquisitionOrder;
};
