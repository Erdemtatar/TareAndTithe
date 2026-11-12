// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/TareTitheTypes.h"
#include "LaserPlaneAimComponent.generated.h"

class USceneComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSlicePreviewUpdatedSignature, const FSlicePreviewTelemetry& /*PreviewTelemetry*/);

UCLASS(ClassGroup = (TareAndTithe), meta = (BlueprintSpawnableComponent))
class TAREANDTITHE_API ULaserPlaneAimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULaserPlaneAimComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void SetAimingEnabled(bool bEnabled);
	bool IsAimingEnabled() const { return bIsAimingEnabled; }

	void SetSliceableTarget(UObject* InTarget);
	UObject* GetSliceableTarget() const { return ActiveSliceableTarget.Get(); }

	void SetLaserVisualComponent(USceneComponent* InVisualComponent);
	USceneComponent* GetLaserVisualComponent() const { return LaserVisualComponent; }

	void SetFrameBasis(const FVector& InFrameAnchorWorld, const FVector& InFrameForwardWorld);
	const FVector& GetFrameAnchorWorld() const { return FrameAnchorWorld; }
	FVector GetFrameForwardUnit() const;
	FVector GetFrameRightUnit() const;

	void AddAimCursorDelta(const FVector2D& MouseDelta);
	bool SetAimFromWorldRay(const FVector& RayOriginWorld, const FVector& RayDirectionWorld);
	void AddScrollWheelInput(float WheelSteps, bool bFastSnapHeld);

	void SetBevelHeld(bool bHeld);
	void SuppressBevelUntilNextPress();
	bool IsBevelLockActive() const { return bIsBevelLockActive; }

	void ResetAimAndOrientation();
	void ForceRefreshPreview();

	const FVector& GetCurrentPlaneOriginWorld() const { return CurrentPlaneOriginWorld; }
	const FVector& GetCurrentPlaneNormalWorld() const { return CurrentPlaneNormalWorld; }
	const FQuat& GetCurrentPlaneOrientationWorld() const { return CurrentPlaneOrientationWorld; }
	const FSlicePreviewTelemetry& GetLastPreviewTelemetry() const { return LastPreviewTelemetry; }

	float GetAimHorizontalCm() const { return AimHorizontalCm; }
	float GetAimVerticalCm() const { return AimVerticalCm; }
	float GetRollDegrees() const { return RollDegrees; }
	float GetPitchBevelDegrees() const { return PitchBevelDegrees; }

	FOnSlicePreviewUpdatedSignature OnPreviewUpdated;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaserAim|Frame")
	FVector FrameAnchorWorld = FVector(200.0, 0.0, 120.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaserAim|Frame")
	FVector FrameForwardWorld = FVector(1.0, 0.0, 0.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LaserAim|Frame", meta = (ClampMin = "10.0", ClampMax = "250.0"))
	float MaxHorizontalOffsetCm = 75.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LaserAim|Frame", meta = (ClampMin = "10.0", ClampMax = "250.0"))
	float MaxVerticalOffsetCm = 85.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LaserAim|Sensitivity", meta = (ClampMin = "0.05", ClampMax = "10.0"))
	float AimTranslationCmPerInputUnit = 1.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LaserAim|Sensitivity", meta = (ClampMin = "0.05", ClampMax = "5.0"))
	float BevelPitchDegreesPerInputUnit = 0.45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LaserAim|Angles", meta = (ClampMin = "0.5", ClampMax = "45.0"))
	float FineRollStepDegrees = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LaserAim|Angles", meta = (ClampMin = "1.0", ClampMax = "90.0"))
	float FastRollStepDegrees = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LaserAim|Angles", meta = (ClampMin = "5.0", ClampMax = "60.0"))
	float MaxPitchBevelDegrees = 40.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LaserAim|Visuals")
	bool bHideVisualWhenAimingDisabled = true;

private:
	void RecomputePlaneAndDispatchPreview();
	void UpdateVisualComponentTransform();
	void DispatchSlicePreview();

	TWeakObjectPtr<UObject> ActiveSliceableTarget;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> LaserVisualComponent;

	FVector CurrentPlaneOriginWorld = FVector::ZeroVector;
	FVector LockedPlaneOriginWorld = FVector::ZeroVector;
	FVector CurrentPlaneNormalWorld = FVector::UpVector;
	FQuat CurrentPlaneOrientationWorld = FQuat::Identity;
	FBox CachedTargetWorldBounds = FBox(ForceInit);
	FSlicePreviewTelemetry LastPreviewTelemetry;

	float AimHorizontalCm = 0.0f;
	float AimVerticalCm = 0.0f;
	float RollDegrees = 0.0f;
	float PitchBevelDegrees = 0.0f;

	bool bIsAimingEnabled = false;
	bool bIsBevelButtonHeld = false;
	bool bIsBevelLockActive = false;
	bool bSuppressBevelUntilRelease = false;
	bool bPlaneStateDirty = true;
};
