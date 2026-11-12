// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/LaserPlaneAimComponent.h"
#include "Components/SceneComponent.h"
#include "Interfaces/SliceableTargetInterface.h"
#include "Math/RotationMatrix.h"

namespace
{
	constexpr double RayPlaneParallelEpsilon = 1.0e-4;
	constexpr double TargetBoundsChangeToleranceSq = 1.0e-4;
}

ULaserPlaneAimComponent::ULaserPlaneAimComponent()
{
	// Tier 2: Continuous visual / aiming state, strictly 0.0ms while idle.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	CurrentPlaneOriginWorld = FrameAnchorWorld;
	LockedPlaneOriginWorld = FrameAnchorWorld;
	CurrentPlaneNormalWorld = FVector::UpVector;
	CurrentPlaneOrientationWorld = FQuat::Identity;
}

void ULaserPlaneAimComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsAimingEnabled)
	{
		SetComponentTickEnabled(false);
		return;
	}

	bool bTargetMoved = false;
	if (UObject* TargetObj = ActiveSliceableTarget.Get())
	{
		if (IsValid(TargetObj) && TargetObj->Implements<USliceableTargetInterface>())
		{
			const FBox LiveBounds = ISliceableTargetInterface::Execute_GetTargetWorldBounds(TargetObj);
			if (LiveBounds.IsValid != CachedTargetWorldBounds.IsValid ||
				FVector::DistSquared(LiveBounds.Min, CachedTargetWorldBounds.Min) > TargetBoundsChangeToleranceSq ||
				FVector::DistSquared(LiveBounds.Max, CachedTargetWorldBounds.Max) > TargetBoundsChangeToleranceSq)
			{
				CachedTargetWorldBounds = LiveBounds;
				bTargetMoved = true;
			}
		}
	}

	if (bPlaneStateDirty)
	{
		RecomputePlaneAndDispatchPreview();
	}
	else if (bTargetMoved)
	{
		DispatchSlicePreview();
	}
}

void ULaserPlaneAimComponent::SetAimingEnabled(bool bEnabled)
{
	if (bIsAimingEnabled == bEnabled)
	{
		return;
	}

	bIsAimingEnabled = bEnabled;
	SetComponentTickEnabled(bIsAimingEnabled);

	if (!bIsAimingEnabled)
	{
		bIsBevelButtonHeld = false;
		bIsBevelLockActive = false;
		bSuppressBevelUntilRelease = false;

		if (bHideVisualWhenAimingDisabled && IsValid(LaserVisualComponent))
		{
			LaserVisualComponent->SetVisibility(false, true);
		}
	}
	else
	{
		if (IsValid(LaserVisualComponent))
		{
			LaserVisualComponent->SetVisibility(true, true);
		}
		bPlaneStateDirty = true;
		RecomputePlaneAndDispatchPreview();
	}
}

void ULaserPlaneAimComponent::SetSliceableTarget(UObject* InTarget)
{
	if (IsValid(InTarget) && InTarget->Implements<USliceableTargetInterface>())
	{
		ActiveSliceableTarget = InTarget;
		CachedTargetWorldBounds = ISliceableTargetInterface::Execute_GetTargetWorldBounds(InTarget);
	}
	else
	{
		ActiveSliceableTarget.Reset();
		CachedTargetWorldBounds = FBox(ForceInit);
	}

	bPlaneStateDirty = true;
	if (bIsAimingEnabled)
	{
		RecomputePlaneAndDispatchPreview();
	}
}

void ULaserPlaneAimComponent::SetLaserVisualComponent(USceneComponent* InVisualComponent)
{
	LaserVisualComponent = InVisualComponent;
	if (IsValid(LaserVisualComponent))
	{
		if (bHideVisualWhenAimingDisabled && !bIsAimingEnabled)
		{
			LaserVisualComponent->SetVisibility(false, true);
		}
		UpdateVisualComponentTransform();
	}
}

void ULaserPlaneAimComponent::SetFrameBasis(const FVector& InFrameAnchorWorld, const FVector& InFrameForwardWorld)
{
	FrameAnchorWorld = InFrameAnchorWorld;
	FrameForwardWorld = InFrameForwardWorld;

	if (!bIsBevelLockActive)
	{
		LockedPlaneOriginWorld = FrameAnchorWorld + (GetFrameRightUnit() * AimHorizontalCm) + (FVector::UpVector * AimVerticalCm);
	}

	bPlaneStateDirty = true;
	if (bIsAimingEnabled)
	{
		RecomputePlaneAndDispatchPreview();
	}
}

FVector ULaserPlaneAimComponent::GetFrameForwardUnit() const
{
	FVector PlanarForward(FrameForwardWorld.X, FrameForwardWorld.Y, 0.0);
	if (!PlanarForward.Normalize())
	{
		PlanarForward = FVector::ForwardVector;
	}
	return PlanarForward;
}

FVector ULaserPlaneAimComponent::GetFrameRightUnit() const
{
	const FVector Forward = GetFrameForwardUnit();
	FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	if (!Right.Normalize())
	{
		Right = FVector::RightVector;
	}
	return Right;
}

void ULaserPlaneAimComponent::AddAimCursorDelta(const FVector2D& MouseDelta)
{
	if (MouseDelta.IsNearlyZero())
	{
		return;
	}

	if (bIsBevelLockActive)
	{
		// Section 5.4 Priority 2: Freeze P_0 and route MouseDelta.Y strictly into PitchBevelDegrees without mutating (AimHorizontalCm, AimVerticalCm).
		const float DeltaPitch = static_cast<float>(MouseDelta.Y) * BevelPitchDegreesPerInputUnit;
		PitchBevelDegrees = FMath::Clamp(PitchBevelDegrees + DeltaPitch, -MaxPitchBevelDegrees, MaxPitchBevelDegrees);
	}
	else
	{
		const float DeltaHorizontal = static_cast<float>(MouseDelta.X) * AimTranslationCmPerInputUnit;
		const float DeltaVertical = static_cast<float>(MouseDelta.Y) * AimTranslationCmPerInputUnit;

		AimHorizontalCm = FMath::Clamp(AimHorizontalCm + DeltaHorizontal, -MaxHorizontalOffsetCm, MaxHorizontalOffsetCm);
		AimVerticalCm = FMath::Clamp(AimVerticalCm + DeltaVertical, -MaxVerticalOffsetCm, MaxVerticalOffsetCm);
	}

	bPlaneStateDirty = true;
	RecomputePlaneAndDispatchPreview();
}

bool ULaserPlaneAimComponent::SetAimFromWorldRay(const FVector& RayOriginWorld, const FVector& RayDirectionWorld)
{
	// Never mutate frame offsets or P_0 while RMB bevel lock is active.
	if (bIsBevelLockActive)
	{
		return false;
	}

	const FVector UnitRayDir = RayDirectionWorld.GetSafeNormal();
	if (UnitRayDir.IsNearlyZero())
	{
		return false;
	}

	const FVector FrameForward = GetFrameForwardUnit();
	const double Denom = FVector::DotProduct(UnitRayDir, FrameForward);
	if (FMath::Abs(Denom) <= RayPlaneParallelEpsilon)
	{
		return false;
	}

	const double RayParamT = FVector::DotProduct(FrameAnchorWorld - RayOriginWorld, FrameForward) / Denom;
	if (RayParamT < 0.0)
	{
		return false;
	}

	const FVector HitWorld = RayOriginWorld + (UnitRayDir * RayParamT);
	const FVector OffsetFromAnchor = HitWorld - FrameAnchorWorld;
	const FVector FrameRight = GetFrameRightUnit();

	AimHorizontalCm = FMath::Clamp(
		static_cast<float>(FVector::DotProduct(OffsetFromAnchor, FrameRight)),
		-MaxHorizontalOffsetCm,
		MaxHorizontalOffsetCm);

	AimVerticalCm = FMath::Clamp(
		static_cast<float>(FVector::DotProduct(OffsetFromAnchor, FVector::UpVector)),
		-MaxVerticalOffsetCm,
		MaxVerticalOffsetCm);

	bPlaneStateDirty = true;
	RecomputePlaneAndDispatchPreview();
	return true;
}

void ULaserPlaneAimComponent::AddScrollWheelInput(float WheelSteps, bool bFastSnapHeld)
{
	if (FMath::IsNearlyZero(WheelSteps))
	{
		return;
	}

	const float StepDegrees = bFastSnapHeld ? FastRollStepDegrees : FineRollStepDegrees;
	RollDegrees = static_cast<float>(FMath::UnwindDegrees(static_cast<double>(RollDegrees + (WheelSteps * StepDegrees))));

	bPlaneStateDirty = true;
	RecomputePlaneAndDispatchPreview();
}

void ULaserPlaneAimComponent::SetBevelHeld(bool bHeld)
{
	bIsBevelButtonHeld = bHeld;

	if (!bHeld)
	{
		bIsBevelLockActive = false;
		bSuppressBevelUntilRelease = false;
		bPlaneStateDirty = true;
		RecomputePlaneAndDispatchPreview();
		return;
	}

	if (bSuppressBevelUntilRelease)
	{
		bIsBevelLockActive = false;
		return;
	}

	// Ensure CurrentPlaneOriginWorld reflects current (AimHorizontalCm, AimVerticalCm) before freezing P_0.
	const FVector FrameRight = GetFrameRightUnit();
	CurrentPlaneOriginWorld = FrameAnchorWorld + (FrameRight * AimHorizontalCm) + (FVector::UpVector * AimVerticalCm);
	LockedPlaneOriginWorld = CurrentPlaneOriginWorld;
	bIsBevelLockActive = true;
}

void ULaserPlaneAimComponent::SuppressBevelUntilNextPress()
{
	bSuppressBevelUntilRelease = true;
	bIsBevelLockActive = false;
}

void ULaserPlaneAimComponent::ResetAimAndOrientation()
{
	AimHorizontalCm = 0.0f;
	AimVerticalCm = 0.0f;
	RollDegrees = 0.0f;
	PitchBevelDegrees = 0.0f;
	bIsBevelButtonHeld = false;
	bIsBevelLockActive = false;
	bSuppressBevelUntilRelease = false;

	bPlaneStateDirty = true;
	RecomputePlaneAndDispatchPreview();
}

void ULaserPlaneAimComponent::ForceRefreshPreview()
{
	bPlaneStateDirty = true;
	RecomputePlaneAndDispatchPreview();
}

void ULaserPlaneAimComponent::RecomputePlaneAndDispatchPreview()
{
	const FVector FrameForward = GetFrameForwardUnit();
	const FVector FrameRight = GetFrameRightUnit();
	const FVector FrameUp = FVector::UpVector;

	if (bIsBevelLockActive)
	{
		CurrentPlaneOriginWorld = LockedPlaneOriginWorld;
	}
	else
	{
		CurrentPlaneOriginWorld = FrameAnchorWorld + (FrameRight * AimHorizontalCm) + (FrameUp * AimVerticalCm);
		LockedPlaneOriginWorld = CurrentPlaneOriginWorld;
	}

	const double RollRad = FMath::DegreesToRadians(static_cast<double>(RollDegrees));
	const double PitchBevelRad = FMath::DegreesToRadians(static_cast<double>(PitchBevelDegrees));

	const FQuat RollQuat(FrameForward, RollRad);
	const FQuat PitchBevelQuat(FrameRight, PitchBevelRad);
	const FQuat CombinedTiltQuat = RollQuat * PitchBevelQuat;

	CurrentPlaneNormalWorld = CombinedTiltQuat.RotateVector(FrameUp).GetSafeNormal();
	if (CurrentPlaneNormalWorld.IsNearlyZero())
	{
		CurrentPlaneNormalWorld = FrameUp;
	}

	const FVector TiltedForward = CombinedTiltQuat.RotateVector(FrameForward).GetSafeNormal();
	CurrentPlaneOrientationWorld = FRotationMatrix::MakeFromZX(CurrentPlaneNormalWorld, TiltedForward).ToQuat();

	UpdateVisualComponentTransform();
	DispatchSlicePreview();
	bPlaneStateDirty = false;
}

void ULaserPlaneAimComponent::UpdateVisualComponentTransform()
{
	if (IsValid(LaserVisualComponent))
	{
		LaserVisualComponent->SetWorldLocationAndRotation(CurrentPlaneOriginWorld, CurrentPlaneOrientationWorld);
	}
}

void ULaserPlaneAimComponent::DispatchSlicePreview()
{
	UObject* TargetObj = ActiveSliceableTarget.Get();
	if (IsValid(TargetObj) && TargetObj->Implements<USliceableTargetInterface>())
	{
		LastPreviewTelemetry = ISliceableTargetInterface::Execute_PreviewSlicePlane(
			TargetObj,
			CurrentPlaneOriginWorld,
			CurrentPlaneNormalWorld);
	}
	else
	{
		LastPreviewTelemetry = FSlicePreviewTelemetry();
	}

	OnPreviewUpdated.Broadcast(LastPreviewTelemetry);
}
