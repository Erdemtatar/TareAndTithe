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
class UPrimitiveComponent;
class ASeveredChunkActor;
struct FHitResult;

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
		float InWatchdogSeconds = 6.0f);

	bool TryRearmWatchdogIfActivelyFalling(float MaxDownwardVelocityZCmPerSec = -25.0f, float MinCellFloorZCm = -75.0f);

	void ConfigureCutCapThermalCooling(bool bEnableCooling, float InCoolingDurationSeconds, float InSpawnWorldTimeSeconds);

	void DeactivateToPool();

	UDynamicMeshComponent* GetChunkMeshComponent() const { return ChunkMeshComponent; }
	FOnChunkWatchdogExpired& GetOnChunkWatchdogExpired() { return OnChunkWatchdogExpired; }

	// IMassPayloadInterface
	virtual float GetAnalyticalMassKg_Implementation() const override;
	virtual bool IsVolatileContaminated_Implementation() const override;
	virtual EChunkRoutingState GetRoutingState_Implementation() const override;
	virtual void NotifyRoutingStateChanged_Implementation(EChunkRoutingState NewState) override;
	virtual void ReturnToChunkPool_Implementation() override;
	virtual UPrimitiveComponent* GetPayloadPhysicsPrimitive_Implementation() const override;
	virtual AActor* GetScaleSupportReceiverActor_Implementation() const override;
	virtual void SetScaleSupportReceiverActor_Implementation(AActor* NewSupportReceiver) override;
	virtual void ArmTareAbyssFailsafeTimer_Implementation(float FailsafeDurationSeconds) override;

	FOnChunkWatchdogExpired OnChunkWatchdogExpired;

protected:
	virtual void BeginPlay() override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chunk")
	TObjectPtr<UDynamicMeshComponent> ChunkMeshComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunk|Collision", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MaxConvexHulls = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunk|Collision", meta = (ClampMin = "4", ClampMax = "32"))
	int32 ConvexHullTargetFaceCount = 16;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunk|Collision", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float MinCollisionThicknessCm = 0.30f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunk|Safety", meta = (ClampMin = "1.5", ClampMax = "15.0"))
	float DefaultWatchdogSeconds = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunk|Safety", meta = (ClampMin = "-120.0", ClampMax = "-45.0"))
	float FloorStrayImpactMaxZCm = -72.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunk|Safety", meta = (ClampMin = "0.25", ClampMax = "3.0"))
	float ExteriorStationaryStrayTimeoutSeconds = 0.85f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunk|Safety", meta = (ClampMin = "0.5", ClampMax = "15.0"))
	float ExteriorStationaryLinearSpeedThresholdCmPerSec = 3.0f;

private:
	UFUNCTION()
	void HandleChunkMeshComponentHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit);

	void EnsureFallbackChunkMaterialsInitialized();
	UMaterialInterface* PrepareActiveChunkCutCapMaterial(UMaterialInterface* BaseCutCapMat, float InSpawnWorldTimeSeconds);
	void StartInFlightWatchdog(float DurationSeconds);
	void ClearInFlightWatchdog();
	void HandleWatchdogExpired();
	void StartExteriorStationaryCheckTimer();
	void ClearExteriorStationaryCheckTimer();
	void HandleExteriorStationaryCheckElapsed();
	bool IsOverlappingQuotaReceiverContainmentTrigger() const;
	void LatchInFlightAsExteriorStrayTare(const FVector& WorldLocation);
	void ClearTareAbyssFailsafeTimer();
	void HandleTareAbyssFailsafeExpired();

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseRuntimeSurfaceMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FallbackChunkOuterMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FallbackChunkCutCapMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CachedCutCapParentMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ActiveChunkCutCapMID;

	TWeakObjectPtr<AActor> CurrentScaleSupportReceiver;

	float AnalyticalMassKg = 0.0f;
	float ActiveWatchdogSeconds = 6.0f;
	bool bVolatileContaminated = false;
	bool bHasRearmedWatchdogOnce = false;
	EChunkRoutingState RoutingState = EChunkRoutingState::PooledInactive;
	FTimerHandle WatchdogTimerHandle;
	FTimerHandle ExteriorStationaryTimerHandle;
	FTimerHandle TareAbyssFailsafeTimerHandle;
};
