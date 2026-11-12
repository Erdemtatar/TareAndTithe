// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/TareTitheTypes.h"
#include "Interfaces/MassPayloadInterface.h"
#include "SeveredChunkActor.generated.h"

class UDynamicMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class ASeveredChunkActor;

namespace UE::Geometry
{
	class FDynamicMesh3;
}

DECLARE_MULTICAST_DELEGATE_OneParam(FOnChunkWatchdogExpired, ASeveredChunkActor* /*ExpiredChunk*/);

UCLASS()
class TAREANDTITHE_API ASeveredChunkActor : public AActor, public IMassPayloadInterface
{
	GENERATED_BODY()

public:
	ASeveredChunkActor();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void SetMinCollisionThicknessCm(float InMinThicknessCm);

	void ActivateFromSlice(
		UE::Geometry::FDynamicMesh3&& InCenteredMesh,
		const FTransform& InSpawnWorldTransform,
		float InAnalyticalMassKg,
		bool bInIsContaminated,
		UMaterialInterface* OuterMaterial,
		UMaterialInterface* CutCapMaterial,
		const FVector& InInitialLinearVelocityCmPerSec,
		float InWatchdogSeconds = 4.0f);

	void DeactivateToPool();

	UDynamicMeshComponent* GetChunkMeshComponent() const { return ChunkMeshComponent; }
	FOnChunkWatchdogExpired& GetOnChunkWatchdogExpired() { return OnChunkWatchdogExpired; }

	// IMassPayloadInterface
	virtual float GetAnalyticalMassKg_Implementation() const override;
	virtual bool IsVolatileContaminated_Implementation() const override;
	virtual EChunkRoutingState GetRoutingState_Implementation() const override;
	virtual void NotifyRoutingStateChanged_Implementation(EChunkRoutingState NewState) override;
	virtual void ReturnToChunkPool_Implementation() override;

	FOnChunkWatchdogExpired OnChunkWatchdogExpired;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chunk")
	TObjectPtr<UDynamicMeshComponent> ChunkMeshComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunk|Collision", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MaxConvexHulls = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunk|Collision", meta = (ClampMin = "4", ClampMax = "32"))
	int32 ConvexHullTargetFaceCount = 16;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunk|Collision", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float MinCollisionThicknessCm = 0.30f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunk|Safety", meta = (ClampMin = "1.5", ClampMax = "10.0"))
	float DefaultWatchdogSeconds = 4.0f;

private:
	void EnsureFallbackChunkMaterialsInitialized();
	void StartInFlightWatchdog(float DurationSeconds);
	void ClearInFlightWatchdog();
	void HandleWatchdogExpired();

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseRuntimeSurfaceMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FallbackChunkOuterMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FallbackChunkCutCapMID;

	float AnalyticalMassKg = 0.0f;
	float ActiveWatchdogSeconds = 4.0f;
	bool bVolatileContaminated = false;
	EChunkRoutingState RoutingState = EChunkRoutingState::PooledInactive;
	FTimerHandle WatchdogTimerHandle;
};
