// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/TareTitheTypes.h"
#include "DiverterGateComponent.generated.h"

class USceneComponent;
class UPrimitiveComponent;
struct FHitResult;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDiverterRouteChangedSignature, EDiverterRoute /*NewRoute*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnChunkRoutedToTareSignature, AActor* /*PayloadActor*/, float /*MassKg*/, bool /*bWasContaminated*/);

UCLASS(ClassGroup = (TareAndTithe), meta = (BlueprintSpawnableComponent))
class TAREANDTITHE_API UDiverterGateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDiverterGateComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void SetFlapperComponent(USceneComponent* InFlapperComponent);
	void SetFlapperComponent(UPrimitiveComponent* InFlapperPrimitive);
	void SetFlapperPivotAndPrimitive(USceneComponent* InPivotComponent, UPrimitiveComponent* InFlapperPrimitive);
	USceneComponent* GetFlapperComponent() const { return FlapperComponent; }

	void BindTareChuteTrigger(UPrimitiveComponent* InTareTriggerComponent);
	void BindTareAbyssDespawnTrigger(UPrimitiveComponent* InAbyssTriggerComponent);
	void ProcessTareChuteOverlap(AActor* OtherActor);
	bool LatchPayloadAsTareOnce(AActor* OtherActor);

	void ConfigureForContract(bool bInAllowToggle, EDiverterRoute InitialRoute = EDiverterRoute::TitheScale);
	bool ToggleRoute();
	bool SetRoute(EDiverterRoute NewRoute, bool bInstant = false);

	EDiverterRoute GetCurrentRoute() const { return CurrentRoute; }
	bool IsToggleAllowed() const { return bAllowToggle; }
	float GetCurrentFlapperAngleDegrees() const { return CurrentAngleDegrees; }

	FOnDiverterRouteChangedSignature OnRouteChanged;
	FOnDiverterRouteChangedSignature OnRouteSlewCompleted;
	FOnChunkRoutedToTareSignature OnChunkRoutedToTare;
	FOnTelemetryImpulseSignature OnTelemetryImpulse;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Diverter|Kinematics")
	FVector FlapperLocalRotationAxis = FVector(1.0, 0.0, 0.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Diverter|Kinematics", meta = (ClampMin = "-90.0", ClampMax = "90.0"))
	float TitheAngleDegrees = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Diverter|Kinematics", meta = (ClampMin = "-90.0", ClampMax = "90.0"))
	float TareAngleDegrees = 45.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Diverter|Kinematics", meta = (ClampMin = "0.04", ClampMax = "1.0"))
	float TransitionDurationSeconds = 0.18f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Diverter|Telemetry", meta = (ClampMin = "0.5", ClampMax = "100.0"))
	float ImpulseReferenceMassKg = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Diverter|Safety", meta = (ClampMin = "0.4", ClampMax = "5.0"))
	float TareAbyssFailsafeSeconds = 1.60f;

private:
	UFUNCTION()
	void HandleTareTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleTareAbyssDespawnBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleFlapperPrimitiveHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit);

	void ApplyFlapperRotation(bool bSweep);
	void WakeTouchingBodies();
	float GetAngleForRoute(EDiverterRoute Route) const;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> FlapperComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> FlapperPrimitiveComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> TareChuteTriggerComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> TareAbyssDespawnTriggerComponent;

	FQuat BaseFlapperRelativeRotation = FQuat::Identity;
	EDiverterRoute CurrentRoute = EDiverterRoute::TitheScale;
	float CurrentAngleDegrees = 0.0f;
	float TargetAngleDegrees = 0.0f;
	bool bAllowToggle = true;
};
