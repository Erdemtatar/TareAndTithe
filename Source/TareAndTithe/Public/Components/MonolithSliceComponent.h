// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/TareTitheTypes.h"
#include "MonolithSliceComponent.generated.h"

class UDynamicMeshComponent;
class UMaterialInterface;
class UMonolithContractDataAsset;
class UShiftProgressionDataAsset;
class ASeveredChunkActor;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMonolithInitializedSignature, float /*InitialMassKg*/, const FVector& /*InitialLocalCoMCm*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSliceExecutedSignature, const FSliceResultSummary& /*ResultSummary*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnChunkSpawnedFromSliceSignature, ASeveredChunkActor* /*SpawnedChunk*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSliverVaporizedSignature, float /*VaporizedMassKg*/, const FVector& /*WorldLocation*/);

UCLASS(ClassGroup = (TareAndTithe), meta = (BlueprintSpawnableComponent))
class TAREANDTITHE_API UMonolithSliceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMonolithSliceComponent();

	void SetRemainderMeshComponent(UDynamicMeshComponent* InRemainderMeshComponent);
	UDynamicMeshComponent* GetRemainderMeshComponent() const { return RemainderMeshComponent; }

	void SetFallbackMaterials(UMaterialInterface* InOuterMaterial, UMaterialInterface* InCutCapMaterial);

	void SetClampGripBoxLocal(const FBox& InGripBoxLocal, bool bDisableAutoFit = true);
	const FBox& GetClampGripBoxLocal() const { return ClampGripBoxLocal; }

	bool InitializeMonolithFromContract(
		const UMonolithContractDataAsset* Contract,
		const UShiftProgressionDataAsset* ShiftConfig);

	FSliceResultSummary ExecuteSlice(
		const FVector& PlaneOriginWorld,
		const FVector& PlaneNormalWorld,
		TArray<FVolatileVeinDefinition>& InOutActiveVeins,
		TArray<ASeveredChunkActor*>& OutSpawnedChunks);

	bool DoesPlaneIntersectRemainder(const FVector& PlaneOriginWorld, const FVector& PlaneNormalWorld) const;
	FBox GetRemainderWorldBounds() const;

	float GetRemainderMassKg() const { return CurrentRemainderMassKg; }
	FVector GetRemainderLocalCoMCm() const { return CurrentRemainderLocalCoMCm; }

	FOnMonolithInitializedSignature OnMonolithInitialized;
	FOnSliceExecutedSignature OnSliceExecuted;
	FOnChunkSpawnedFromSliceSignature OnChunkSpawnedFromSlice;
	FOnSliverVaporizedSignature OnSliverVaporized;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monolith|Clamp")
	FBox ClampGripBoxLocal = FBox(FVector(-12.0, -12.0, 42.0), FVector(12.0, 12.0, 58.0));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monolith|Clamp")
	bool bAutoFitClampGripBoxToTopOfContract = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monolith|Clamp", meta = (ClampMin = "0.10", ClampMax = "0.90"))
	float AutoFitJawHorizontalCoverageRatio = 0.50f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monolith|Clamp", meta = (ClampMin = "0.02", ClampMax = "0.35"))
	float AutoFitJawVerticalDepthRatio = 0.08f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monolith|Defaults", meta = (ClampMin = "5.0", ClampMax = "250.0"))
	float FallbackMinSliverVolumeCm3 = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monolith|Defaults", meta = (ClampMin = "0.2", ClampMax = "5.0"))
	float FallbackMinChunkAxisThicknessCm = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monolith|Defaults", meta = (ClampMin = "1.0", ClampMax = "100.0"))
	float FallbackChunkSeparationSpeedCmPerSec = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monolith|Defaults", meta = (ClampMin = "1.5", ClampMax = "10.0"))
	float FallbackInFlightWatchdogSeconds = 4.0f;

private:
	bool EnsureRemainderMeshComponent();

	UPROPERTY(Transient)
	TObjectPtr<UDynamicMeshComponent> RemainderMeshComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ActiveOuterMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ActiveCutCapMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> FallbackOuterMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> FallbackCutCapMaterial;

	TWeakObjectPtr<const UMonolithContractDataAsset> ActiveContract;
	TWeakObjectPtr<const UShiftProgressionDataAsset> ActiveShiftConfig;

	float CurrentRemainderMassKg = 0.0f;
	FVector CurrentRemainderLocalCoMCm = FVector::ZeroVector;
};
