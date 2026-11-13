// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/ScaleWeighingComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Data/MonolithContractDataAsset.h"
#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"
#include "Interfaces/MassPayloadInterface.h"

UScaleWeighingComponent::UScaleWeighingComponent()
{
	// Tier 2: Activated strictly on payload enter/exit; sleeps once harmonic oscillation & rigid bodies settle.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UScaleWeighingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(PanContainmentTrigger))
	{
		PanContainmentTrigger->OnComponentBeginOverlap.RemoveDynamic(this, &UScaleWeighingComponent::HandlePanTriggerBeginOverlap);
		PanContainmentTrigger->OnComponentEndOverlap.RemoveDynamic(this, &UScaleWeighingComponent::HandlePanTriggerEndOverlap);
	}
	Super::EndPlay(EndPlayReason);
}

void UScaleWeighingComponent::SetPanPlatformComponent(USceneComponent* InPanPlatformComponent)
{
	PanPlatformComponent = InPanPlatformComponent;
	if (IsValid(PanPlatformComponent))
	{
		NeutralPanRelativeLocation = PanPlatformComponent->GetRelativeLocation();
	}
}

void UScaleWeighingComponent::BindPanContainmentTrigger(UPrimitiveComponent* InTriggerComponent)
{
	if (IsValid(PanContainmentTrigger))
	{
		PanContainmentTrigger->OnComponentBeginOverlap.RemoveDynamic(this, &UScaleWeighingComponent::HandlePanTriggerBeginOverlap);
		PanContainmentTrigger->OnComponentEndOverlap.RemoveDynamic(this, &UScaleWeighingComponent::HandlePanTriggerEndOverlap);
	}

	PanContainmentTrigger = InTriggerComponent;

	if (IsValid(PanContainmentTrigger))
	{
		PanContainmentTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		PanContainmentTrigger->SetCollisionObjectType(ECC_WorldDynamic);
		PanContainmentTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
		PanContainmentTrigger->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
		PanContainmentTrigger->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		PanContainmentTrigger->SetGenerateOverlapEvents(true);

		PanContainmentTrigger->OnComponentBeginOverlap.AddDynamic(this, &UScaleWeighingComponent::HandlePanTriggerBeginOverlap);
		PanContainmentTrigger->OnComponentEndOverlap.AddDynamic(this, &UScaleWeighingComponent::HandlePanTriggerEndOverlap);
		ReconcileOverlappingPayloads();
	}
}

void UScaleWeighingComponent::SetPanWellBoundsGeometry(
	const FVector2D& InHalfExtentXYCm,
	float InPanFloorOffsetZCm,
	float InRecoveryMaxHeightZCm)
{
	PanWellHalfExtentXYCm = FVector2D(
		FMath::Max(2.0, InHalfExtentXYCm.X),
		FMath::Max(2.0, InHalfExtentXYCm.Y));
	PanWellFloorOffsetZCm = InPanFloorOffsetZCm;
	PanWellMaxRecoveryHeightZCm = FMath::Max(20.0f, InRecoveryMaxHeightZCm);
}

void UScaleWeighingComponent::ConfigureForContract(const UMonolithContractDataAsset* Contract)
{
	if (IsValid(Contract))
	{
		TargetQuotaMassKg = FMath::Max(0.5f, Contract->TargetTitheMassKg);
	}
	ResetScaleState();
}

void UScaleWeighingComponent::SetTargetQuotaMassKg(float InTargetQuotaMassKg)
{
	TargetQuotaMassKg = FMath::Max(0.5f, InTargetQuotaMassKg);
	const float FillRatio = FMath::Clamp(PanMassKg / TargetQuotaMassKg, 0.0f, 1.0f);
	EmitFillRatioTelemetry(FillRatio);
}

void UScaleWeighingComponent::SetSettleDampingMultiplier(float InMultiplier)
{
	SettleDampingMultiplier = FMath::Clamp(InMultiplier, 0.5f, 4.0f);
}

void UScaleWeighingComponent::ResetScaleState()
{
	for (const FTrackedPanPayload& Entry : TrackedPayloads)
	{
		AActor* Actor = Entry.PayloadActor.Get();
		if (IsValid(Actor) && Actor->Implements<UMassPayloadInterface>())
		{
			IMassPayloadInterface::Execute_SetScaleSupportReceiverActor(Actor, nullptr);
		}
	}

	TrackedPayloads.Reset();
	PanMassKg = 0.0f;
	bHasContamination = false;
	CurrentDisplacementZCm = 0.0;
	TargetDisplacementZCm = 0.0;
	VelocityZCmPerSec = 0.0;
	SettleHoldElapsedSeconds = 0.0f;

	ApplyPanPlatformDisplacement(false);
	SetSettledState(true);
	EmitFillRatioTelemetry(0.0f, true);
	SetComponentTickEnabled(false);
}

int32 UScaleWeighingComponent::GetConfirmedPayloadCount() const
{
	int32 ConfirmedCount = 0;
	for (const FTrackedPanPayload& Entry : TrackedPayloads)
	{
		AActor* Actor = Entry.PayloadActor.Get();
		if (Entry.bHasConfirmedScaleImpact &&
			IsValid(Actor) &&
			Actor->Implements<UMassPayloadInterface>() &&
			IMassPayloadInterface::Execute_GetRoutingState(Actor) == EChunkRoutingState::OnScalePan)
		{
			++ConfirmedCount;
		}
	}
	return ConfirmedCount;
}

bool UScaleWeighingComponent::ConfirmPayloadScaleImpact(
	AActor* PayloadActor,
	UPrimitiveComponent* PayloadPrimitive,
	const FVector& WorldImpactPoint,
	const FVector& NormalImpulse)
{
	if (!IsValid(PayloadActor) || PayloadActor == GetOwner() || !PayloadActor->Implements<UMassPayloadInterface>())
	{
		return false;
	}

	const EChunkRoutingState IncomingState = IMassPayloadInterface::Execute_GetRoutingState(PayloadActor);
	if (IncomingState == EChunkRoutingState::PooledInactive ||
		IncomingState == EChunkRoutingState::ConsumedAsTare ||
		IncomingState == EChunkRoutingState::LostAsStrayTare)
	{
		return false;
	}

	if (IncomingState != EChunkRoutingState::OnScalePan &&
		OnShouldIgnorePayloadInPan.IsBound() &&
		OnShouldIgnorePayloadInPan.Execute(PayloadActor))
	{
		return false;
	}

	const bool bInSpatialPanZone =
		(IsValid(PanContainmentTrigger) && PanContainmentTrigger->IsOverlappingActor(PayloadActor)) ||
		IsPayloadWithinPanRecoveryWell(PayloadActor);
	if (!bInSpatialPanZone)
	{
		return false;
	}

	UPrimitiveComponent* ResolvedPrim = IsValid(PayloadPrimitive)
		? PayloadPrimitive
		: IMassPayloadInterface::Execute_GetPayloadPhysicsPrimitive(PayloadActor);

	int32 TrackedIdx = FindTrackedPayloadIndex(PayloadActor);
	if (TrackedIdx != INDEX_NONE &&
		TrackedPayloads[TrackedIdx].bHasConfirmedScaleImpact &&
		IncomingState == EChunkRoutingState::OnScalePan)
	{
		return false;
	}

	if (TrackedIdx == INDEX_NONE)
	{
		FTrackedPanPayload NewEntry;
		NewEntry.PayloadActor = PayloadActor;
		NewEntry.PayloadPrimitive = ResolvedPrim;
		NewEntry.bHasConfirmedScaleImpact = true;
		TrackedIdx = TrackedPayloads.Add(NewEntry);
	}
	else
	{
		if (!TrackedPayloads[TrackedIdx].PayloadPrimitive.IsValid() && IsValid(ResolvedPrim))
		{
			TrackedPayloads[TrackedIdx].PayloadPrimitive = ResolvedPrim;
		}
		TrackedPayloads[TrackedIdx].bHasConfirmedScaleImpact = true;
	}

	IMassPayloadInterface::Execute_SetScaleSupportReceiverActor(PayloadActor, GetOwner());
	IMassPayloadInterface::Execute_NotifyRoutingStateChanged(PayloadActor, EChunkRoutingState::OnScalePan);

	const float ChunkMassKg = FMath::Max(0.0f, IMassPayloadInterface::Execute_GetAnalyticalMassKg(PayloadActor));
	const bool bChunkContaminated = IMassPayloadInterface::Execute_IsVolatileContaminated(PayloadActor);

	// Impart downward rigid-body kinetic impulse kick to PanMotionRoot strictly at physical contact frame.
	const double SafeCarriageMassKg = FMath::Max(1.0, static_cast<double>(EffectivePanCarriageMassKg));
	const double ImpulseDerivedSpeedCmPerSec =
		(NormalImpulse.Size() / SafeCarriageMassKg) * static_cast<double>(ImpactImpulseTransferScale);
	const double MassFloorKickCmPerSec =
		static_cast<double>(ChunkMassKg) * static_cast<double>(MinImpactKickPerKgCmPerSec);
	const double ClampedKickCmPerSec = FMath::Clamp(
		FMath::Max(ImpulseDerivedSpeedCmPerSec, MassFloorKickCmPerSec),
		1.5,
		static_cast<double>(MaxImpactKickSpeedCmPerSec));
	VelocityZCmPerSec = FMath::Clamp(
		VelocityZCmPerSec - ClampedKickCmPerSec,
		-static_cast<double>(MaxImpactKickSpeedCmPerSec) * 1.35,
		static_cast<double>(MaxImpactKickSpeedCmPerSec));

	RecomputeAnalyticalPanMass();

	SettleHoldElapsedSeconds = 0.0f;
	SetSettledState(false);
	SetComponentTickEnabled(true);

	const float NormalizedImpact = FMath::Clamp(ChunkMassKg / FMath::Max(0.5f, TargetQuotaMassKg), 0.05f, 1.0f);
	FVector ImpactWorldLocation = !WorldImpactPoint.IsNearlyZero()
		? WorldImpactPoint
		: PayloadActor->GetActorLocation();
	if (WorldImpactPoint.IsNearlyZero() && IsValid(PanPlatformComponent))
	{
		ImpactWorldLocation.Z = PanPlatformComponent->GetComponentLocation().Z + static_cast<double>(PanWellFloorOffsetZCm + 6.0f);
	}

	OnTelemetryImpulse.Broadcast(ETelemetryImpulseId::ChunkScaleImpact, NormalizedImpact, ImpactWorldLocation);
	OnChunkLandedOnScale.Broadcast(PayloadActor, ChunkMassKg, PanMassKg);

	if (bChunkContaminated)
	{
		OnScaleContaminationDetected.Broadcast(PayloadActor);
	}

	CascadeRestingStackedPayloads();
	return true;
}

void UScaleWeighingComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const float ClampedDelta = FMath::Clamp(DeltaTime, 0.0f, MaxFrameDeltaSeconds);
	if (ClampedDelta <= 0.0f)
	{
		return;
	}

	// Prune invalidated, pooled, tare-routed, or detached entries while preserving airborne InFlight containment entries (which contribute 0.0 kg until impact).
	bool bConfirmedMembershipPruned = false;
	for (int32 Idx = TrackedPayloads.Num() - 1; Idx >= 0; --Idx)
	{
		const FTrackedPanPayload& Entry = TrackedPayloads[Idx];
		AActor* Actor = Entry.PayloadActor.Get();
		if (!IsValid(Actor) || !Actor->Implements<UMassPayloadInterface>())
		{
			bConfirmedMembershipPruned |= Entry.bHasConfirmedScaleImpact;
			TrackedPayloads.RemoveAtSwap(Idx, EAllowShrinking::No);
			continue;
		}

		const EChunkRoutingState State = IMassPayloadInterface::Execute_GetRoutingState(Actor);
		const bool bTerminalState =
			(State == EChunkRoutingState::PooledInactive) ||
			(State == EChunkRoutingState::ConsumedAsTare) ||
			(State == EChunkRoutingState::LostAsStrayTare);
		if (bTerminalState || (Entry.bHasConfirmedScaleImpact && State != EChunkRoutingState::OnScalePan))
		{
			bConfirmedMembershipPruned |= Entry.bHasConfirmedScaleImpact;
			TrackedPayloads.RemoveAtSwap(Idx, EAllowShrinking::No);
		}
	}

	if (bConfirmedMembershipPruned)
	{
		RecomputeAnalyticalPanMass();
	}

	const float SafeMaxSubstep = FMath::Clamp(MaxSubstepDeltaSeconds, 0.001f, 0.016667f);
	const int32 NumSubsteps = FMath::Max(1, FMath::CeilToInt(ClampedDelta / SafeMaxSubstep));
	const double SubDt = static_cast<double>(ClampedDelta) / static_cast<double>(NumSubsteps);

	const double SafeDampMult = FMath::Clamp(static_cast<double>(SettleDampingMultiplier), 0.5, 4.0);
	const double OmegaN = FMath::Max(1.0, static_cast<double>(NaturalFrequencyRadPerSec) * SafeDampMult);
	const double Zeta = FMath::Clamp(
		static_cast<double>(DampingRatio) * (1.0 + 0.14 * (SafeDampMult - 1.0)),
		0.05,
		0.96);
	const double MaxTravelDown = -static_cast<double>(MaxDepressionCm) * 1.25;

	for (int32 StepIdx = 0; StepIdx < NumSubsteps; ++StepIdx)
	{
		const double DisplacementError = CurrentDisplacementZCm - TargetDisplacementZCm;
		const double AccelZ = (-2.0 * Zeta * OmegaN * VelocityZCmPerSec) - (OmegaN * OmegaN * DisplacementError);
		VelocityZCmPerSec += AccelZ * SubDt;
		CurrentDisplacementZCm = FMath::Clamp(CurrentDisplacementZCm + (VelocityZCmPerSec * SubDt), MaxTravelDown, 2.0);
	}

	const bool bPanPosConverged = FMath::Abs(CurrentDisplacementZCm - TargetDisplacementZCm) < static_cast<double>(PanPositionSettleToleranceCm);
	const bool bPanVelConverged = FMath::Abs(VelocityZCmPerSec) < static_cast<double>(PanVelocitySettleToleranceCmPerSec);

	ApplyPanPlatformDisplacement(true);

	if (!bPanPosConverged || !bPanVelConverged)
	{
		WakeOverlappingPayloads();
	}

	const bool bPayloadsQuiet = AreAllPayloadsBelowSettleVelocity();
	const float EffectiveSettleHoldSeconds = FMath::Max(0.03f, SettleHoldRequiredSeconds / static_cast<float>(SafeDampMult));
	if (bPanPosConverged && bPanVelConverged && bPayloadsQuiet)
	{
		SettleHoldElapsedSeconds += ClampedDelta;
		if (SettleHoldElapsedSeconds >= EffectiveSettleHoldSeconds)
		{
			if (ReconcileOverlappingPayloads(false))
			{
				// A resting or micro-bounced chunk was reconciled into/out of PanMassKg; let the spring converge to the updated target.
				SettleHoldElapsedSeconds = 0.0f;
				SetSettledState(false);
			}
			else
			{
				CurrentDisplacementZCm = TargetDisplacementZCm;
				VelocityZCmPerSec = 0.0;
				ApplyPanPlatformDisplacement(false);
				SetSettledState(true);
				SetComponentTickEnabled(false);
			}
		}
	}
	else
	{
		SettleHoldElapsedSeconds = 0.0f;
		SetSettledState(false);
	}
}

void UScaleWeighingComponent::HandlePanTriggerBeginOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	if (!IsValid(OtherActor) || OtherActor == GetOwner() || !OtherActor->Implements<UMassPayloadInterface>())
	{
		return;
	}

	const EChunkRoutingState IncomingState = IMassPayloadInterface::Execute_GetRoutingState(OtherActor);
	if (IncomingState == EChunkRoutingState::PooledInactive ||
		IncomingState == EChunkRoutingState::ConsumedAsTare ||
		IncomingState == EChunkRoutingState::LostAsStrayTare)
	{
		return;
	}

	if (FindTrackedPayloadIndex(OtherActor) != INDEX_NONE)
	{
		return;
	}

	UPrimitiveComponent* ResolvedPrim = IsValid(OtherComp)
		? OtherComp
		: IMassPayloadInterface::Execute_GetPayloadPhysicsPrimitive(OtherActor);

	// Spatial containment vs. impact activation separation:
	// Entering the tall PanTriggerBox while InFlight records spatial containment ONLY (0.0 kg equilibrium feed, zero state change, zero tick wakeup).
	if (IncomingState != EChunkRoutingState::OnScalePan)
	{
		FTrackedPanPayload AirborneEntry;
		AirborneEntry.PayloadActor = OtherActor;
		AirborneEntry.PayloadPrimitive = ResolvedPrim;
		AirborneEntry.bHasConfirmedScaleImpact = false;
		TrackedPayloads.Add(AirborneEntry);

		if (IsInFlightPayloadRestingOnPanOrConfirmedStack(OtherActor, ResolvedPrim))
		{
			ConfirmPayloadScaleImpact(OtherActor, ResolvedPrim, OtherActor->GetActorLocation(), FVector::ZeroVector);
		}
		return;
	}

	FTrackedPanPayload ConfirmedEntry;
	ConfirmedEntry.PayloadActor = OtherActor;
	ConfirmedEntry.PayloadPrimitive = ResolvedPrim;
	ConfirmedEntry.bHasConfirmedScaleImpact = true;
	TrackedPayloads.Add(ConfirmedEntry);

	IMassPayloadInterface::Execute_SetScaleSupportReceiverActor(OtherActor, GetOwner());
	RecomputeAnalyticalPanMass();
	SettleHoldElapsedSeconds = 0.0f;
	SetSettledState(false);
	SetComponentTickEnabled(true);
}

void UScaleWeighingComponent::HandlePanTriggerEndOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/)
{
	if (!IsValid(OtherActor) || !OtherActor->Implements<UMassPayloadInterface>())
	{
		return;
	}

	const int32 ExistingIdx = FindTrackedPayloadIndex(OtherActor);
	if (ExistingIdx == INDEX_NONE)
	{
		return;
	}

	const bool bWasConfirmed = TrackedPayloads[ExistingIdx].bHasConfirmedScaleImpact;
	const EChunkRoutingState LeavingState = IMassPayloadInterface::Execute_GetRoutingState(OtherActor);

	// Multi-convex-hull & stacked recovery guard: retain if still overlapping or still resting inside the pan well column.
	if ((IsValid(PanContainmentTrigger) && PanContainmentTrigger->IsOverlappingActor(OtherActor)) ||
		(bWasConfirmed && LeavingState == EChunkRoutingState::OnScalePan && IsPayloadWithinPanRecoveryWell(OtherActor)))
	{
		return;
	}

	TrackedPayloads.RemoveAtSwap(ExistingIdx, EAllowShrinking::No);

	// Genuine bounce-out vs. pool reset guard: only transition back to InFlight if it had confirmed scale contact and is still marked OnScalePan.
	if (bWasConfirmed && LeavingState == EChunkRoutingState::OnScalePan)
	{
		const float BouncedMassKg = IMassPayloadInterface::Execute_GetAnalyticalMassKg(OtherActor);
		IMassPayloadInterface::Execute_SetScaleSupportReceiverActor(OtherActor, nullptr);
		IMassPayloadInterface::Execute_NotifyRoutingStateChanged(OtherActor, EChunkRoutingState::InFlight);

		RecomputeAnalyticalPanMass();
		SettleHoldElapsedSeconds = 0.0f;
		SetSettledState(false);
		SetComponentTickEnabled(true);

		OnChunkBouncedOffScale.Broadcast(OtherActor, BouncedMassKg, PanMassKg);
	}
	else if (bWasConfirmed)
	{
		IMassPayloadInterface::Execute_SetScaleSupportReceiverActor(OtherActor, nullptr);
		RecomputeAnalyticalPanMass();
	}
}

int32 UScaleWeighingComponent::FindTrackedPayloadIndex(const AActor* PayloadActor) const
{
	for (int32 i = 0; i < TrackedPayloads.Num(); ++i)
	{
		if (TrackedPayloads[i].PayloadActor.Get() == PayloadActor)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

bool UScaleWeighingComponent::IsPayloadWithinPanRecoveryWell(const AActor* PayloadActor) const
{
	if (!IsValid(PayloadActor))
	{
		return false;
	}

	const USceneComponent* ReferenceComp = IsValid(PanPlatformComponent)
		? PanPlatformComponent.Get()
		: PanContainmentTrigger.Get();
	if (!IsValid(ReferenceComp))
	{
		return false;
	}

	const FVector LocalPos = ReferenceComp->GetComponentTransform().InverseTransformPosition(PayloadActor->GetActorLocation());
	const double MaxHalfX = PanWellHalfExtentXYCm.X + static_cast<double>(PanWellRecoveryMarginXYCm);
	const double MaxHalfY = PanWellHalfExtentXYCm.Y + static_cast<double>(PanWellRecoveryMarginXYCm);

	return (FMath::Abs(LocalPos.X) <= MaxHalfX) &&
		(FMath::Abs(LocalPos.Y) <= MaxHalfY) &&
		(LocalPos.Z >= static_cast<double>(PanWellFloorOffsetZCm)) &&
		(LocalPos.Z <= static_cast<double>(PanWellMaxRecoveryHeightZCm));
}

bool UScaleWeighingComponent::IsInFlightPayloadRestingOnPanOrConfirmedStack(
	const AActor* CandidateActor,
	const UPrimitiveComponent* CandidatePrimitive) const
{
	if (!IsValid(CandidateActor) || !IsValid(CandidatePrimitive) || !CandidatePrimitive->IsSimulatingPhysics())
	{
		return false;
	}

	const double MaxRestSpeedSq = FMath::Square(static_cast<double>(RestingFallbackMaxLinearSpeedCmPerSec));
	if (CandidatePrimitive->GetPhysicsLinearVelocity().SizeSquared() > MaxRestSpeedSq)
	{
		return false;
	}

	const FBox CandidateBox = CandidatePrimitive->Bounds.GetBox();
	if (!CandidateBox.IsValid)
	{
		return false;
	}

	const double TolZ = static_cast<double>(RestingFallbackContactToleranceZCm);
	if (IsValid(PanPlatformComponent))
	{
		const double DeckTopWorldZ = PanPlatformComponent->GetComponentLocation().Z + static_cast<double>(PanWellFloorOffsetZCm + 6.0f);
		if (FMath::Abs(CandidateBox.Min.Z - DeckTopWorldZ) <= TolZ)
		{
			return true;
		}
	}

	for (const FTrackedPanPayload& Entry : TrackedPayloads)
	{
		if (!Entry.bHasConfirmedScaleImpact)
		{
			continue;
		}

		const AActor* SupportActor = Entry.PayloadActor.Get();
		const UPrimitiveComponent* SupportPrim = Entry.PayloadPrimitive.Get();
		if (!IsValid(SupportActor) || SupportActor == CandidateActor || !IsValid(SupportPrim))
		{
			continue;
		}

		if (!SupportActor->Implements<UMassPayloadInterface>() ||
			IMassPayloadInterface::Execute_GetRoutingState(SupportActor) != EChunkRoutingState::OnScalePan)
		{
			continue;
		}

		const FBox SupportBox = SupportPrim->Bounds.GetBox();
		if (!SupportBox.IsValid)
		{
			continue;
		}

		const bool bOverlapsXY =
			(CandidateBox.Max.X + 2.0 >= SupportBox.Min.X) && (CandidateBox.Min.X - 2.0 <= SupportBox.Max.X) &&
			(CandidateBox.Max.Y + 2.0 >= SupportBox.Min.Y) && (CandidateBox.Min.Y - 2.0 <= SupportBox.Max.Y);
		const bool bRestsOnSupportZ =
			(CandidateBox.Min.Z <= SupportBox.Max.Z + TolZ) &&
			(CandidateBox.Max.Z >= SupportBox.Min.Z);
		if (bOverlapsXY && bRestsOnSupportZ)
		{
			return true;
		}
	}

	return false;
}

void UScaleWeighingComponent::CascadeRestingStackedPayloads()
{
	bool bPromotedAny = true;
	int32 SafetyPass = 0;
	while (bPromotedAny && SafetyPass < 8)
	{
		bPromotedAny = false;
		++SafetyPass;

		for (int32 Idx = 0; Idx < TrackedPayloads.Num(); ++Idx)
		{
			if (TrackedPayloads[Idx].bHasConfirmedScaleImpact)
			{
				continue;
			}

			AActor* CandidateActor = TrackedPayloads[Idx].PayloadActor.Get();
			if (!IsValid(CandidateActor) || !CandidateActor->Implements<UMassPayloadInterface>())
			{
				continue;
			}

			if (IMassPayloadInterface::Execute_GetRoutingState(CandidateActor) != EChunkRoutingState::InFlight)
			{
				continue;
			}

			UPrimitiveComponent* CandidatePrim = TrackedPayloads[Idx].PayloadPrimitive.Get();
			if (!IsValid(CandidatePrim))
			{
				CandidatePrim = IMassPayloadInterface::Execute_GetPayloadPhysicsPrimitive(CandidateActor);
				TrackedPayloads[Idx].PayloadPrimitive = CandidatePrim;
			}

			if (IsInFlightPayloadRestingOnPanOrConfirmedStack(CandidateActor, CandidatePrim))
			{
				if (ConfirmPayloadScaleImpact(CandidateActor, CandidatePrim, CandidateActor->GetActorLocation(), FVector::ZeroVector))
				{
					bPromotedAny = true;
					break;
				}
			}
		}
	}
}

bool UScaleWeighingComponent::ReconcileOverlappingPayloads(bool bEnableTickOnChange)
{
	if (!IsValid(PanContainmentTrigger))
	{
		return false;
	}

	PanContainmentTrigger->UpdateOverlaps();

	TArray<AActor*> OverlappingActors;
	PanContainmentTrigger->GetOverlappingActors(OverlappingActors);

	const float PreviousPanMassKg = PanMassKg;
	bool bConfirmedMembershipChanged = false;

	struct FBouncedEntry
	{
		AActor* Actor = nullptr;
		float MassKg = 0.0f;
	};
	TArray<FBouncedEntry, TInlineAllocator<4>> NewlyBouncedEntries;

	for (int32 Idx = TrackedPayloads.Num() - 1; Idx >= 0; --Idx)
	{
		const bool bWasConfirmed = TrackedPayloads[Idx].bHasConfirmedScaleImpact;
		AActor* TrackedActor = TrackedPayloads[Idx].PayloadActor.Get();
		if (!IsValid(TrackedActor) || !TrackedActor->Implements<UMassPayloadInterface>())
		{
			TrackedPayloads.RemoveAtSwap(Idx, EAllowShrinking::No);
			bConfirmedMembershipChanged |= bWasConfirmed;
			continue;
		}

		const EChunkRoutingState CurrentState = IMassPayloadInterface::Execute_GetRoutingState(TrackedActor);
		const bool bStillOverlapping = OverlappingActors.Contains(TrackedActor);
		const bool bRetainedInPanWell = bStillOverlapping ||
			(bWasConfirmed && CurrentState == EChunkRoutingState::OnScalePan && IsPayloadWithinPanRecoveryWell(TrackedActor));

		if (!bRetainedInPanWell ||
			CurrentState == EChunkRoutingState::PooledInactive ||
			CurrentState == EChunkRoutingState::ConsumedAsTare ||
			CurrentState == EChunkRoutingState::LostAsStrayTare)
		{
			TrackedPayloads.RemoveAtSwap(Idx, EAllowShrinking::No);
			bConfirmedMembershipChanged |= bWasConfirmed;

			if (!bRetainedInPanWell && bWasConfirmed && CurrentState == EChunkRoutingState::OnScalePan)
			{
				const float BouncedMassKg = IMassPayloadInterface::Execute_GetAnalyticalMassKg(TrackedActor);
				IMassPayloadInterface::Execute_SetScaleSupportReceiverActor(TrackedActor, nullptr);
				IMassPayloadInterface::Execute_NotifyRoutingStateChanged(TrackedActor, EChunkRoutingState::InFlight);
				NewlyBouncedEntries.Add({TrackedActor, BouncedMassKg});
			}
			else if (bWasConfirmed)
			{
				IMassPayloadInterface::Execute_SetScaleSupportReceiverActor(TrackedActor, nullptr);
			}
		}
	}

	struct FLandedEntry
	{
		AActor* Actor = nullptr;
		float MassKg = 0.0f;
		bool bContaminated = false;
	};
	TArray<FLandedEntry, TInlineAllocator<4>> NewlyLandedEntries;

	for (AActor* OverlappedActor : OverlappingActors)
	{
		if (!IsValid(OverlappedActor) || OverlappedActor == GetOwner() || !OverlappedActor->Implements<UMassPayloadInterface>())
		{
			continue;
		}

		const EChunkRoutingState CurrentState = IMassPayloadInterface::Execute_GetRoutingState(OverlappedActor);
		if (CurrentState == EChunkRoutingState::PooledInactive ||
			CurrentState == EChunkRoutingState::ConsumedAsTare ||
			CurrentState == EChunkRoutingState::LostAsStrayTare)
		{
			continue;
		}

		UPrimitiveComponent* PayloadPrim = IMassPayloadInterface::Execute_GetPayloadPhysicsPrimitive(OverlappedActor);
		int32 ExistingIdx = FindTrackedPayloadIndex(OverlappedActor);

		if (CurrentState == EChunkRoutingState::OnScalePan)
		{
			const bool bWasUnconfirmed = (ExistingIdx == INDEX_NONE) || !TrackedPayloads[ExistingIdx].bHasConfirmedScaleImpact;
			if (ExistingIdx == INDEX_NONE)
			{
				FTrackedPanPayload ReconciledEntry;
				ReconciledEntry.PayloadActor = OverlappedActor;
				ReconciledEntry.PayloadPrimitive = PayloadPrim;
				ReconciledEntry.bHasConfirmedScaleImpact = true;
				TrackedPayloads.Add(ReconciledEntry);
			}
			else
			{
				TrackedPayloads[ExistingIdx].PayloadPrimitive = PayloadPrim;
				TrackedPayloads[ExistingIdx].bHasConfirmedScaleImpact = true;
			}
			IMassPayloadInterface::Execute_SetScaleSupportReceiverActor(OverlappedActor, GetOwner());
			bConfirmedMembershipChanged |= bWasUnconfirmed;
			continue;
		}

		// CurrentState == InFlight: NEVER prematurely promote an airborne falling chunk!
		// Record spatial containment with bHasConfirmedScaleImpact = false, and promote ONLY if physically resting at low speed on the pan or a confirmed stack.
		if (ExistingIdx == INDEX_NONE)
		{
			FTrackedPanPayload AirborneEntry;
			AirborneEntry.PayloadActor = OverlappedActor;
			AirborneEntry.PayloadPrimitive = PayloadPrim;
			AirborneEntry.bHasConfirmedScaleImpact = false;
			ExistingIdx = TrackedPayloads.Add(AirborneEntry);
		}
		else if (!TrackedPayloads[ExistingIdx].PayloadPrimitive.IsValid() && IsValid(PayloadPrim))
		{
			TrackedPayloads[ExistingIdx].PayloadPrimitive = PayloadPrim;
		}

		if (OnShouldIgnorePayloadInPan.IsBound() && OnShouldIgnorePayloadInPan.Execute(OverlappedActor))
		{
			continue;
		}

		if (IsInFlightPayloadRestingOnPanOrConfirmedStack(OverlappedActor, PayloadPrim))
		{
			TrackedPayloads[ExistingIdx].bHasConfirmedScaleImpact = true;
			IMassPayloadInterface::Execute_SetScaleSupportReceiverActor(OverlappedActor, GetOwner());
			IMassPayloadInterface::Execute_NotifyRoutingStateChanged(OverlappedActor, EChunkRoutingState::OnScalePan);
			const float ChunkMassKg = IMassPayloadInterface::Execute_GetAnalyticalMassKg(OverlappedActor);
			const bool bContaminated = IMassPayloadInterface::Execute_IsVolatileContaminated(OverlappedActor);
			NewlyLandedEntries.Add({OverlappedActor, ChunkMassKg, bContaminated});
			bConfirmedMembershipChanged = true;
		}
	}

	if (bConfirmedMembershipChanged)
	{
		RecomputeAnalyticalPanMass();

		if (bEnableTickOnChange)
		{
			SettleHoldElapsedSeconds = 0.0f;
			SetSettledState(false);
			SetComponentTickEnabled(true);
		}

		for (const FBouncedEntry& Bounced : NewlyBouncedEntries)
		{
			OnChunkBouncedOffScale.Broadcast(Bounced.Actor, Bounced.MassKg, PanMassKg);
		}

		for (const FLandedEntry& Landed : NewlyLandedEntries)
		{
			OnChunkLandedOnScale.Broadcast(Landed.Actor, Landed.MassKg, PanMassKg);
			if (Landed.bContaminated)
			{
				OnScaleContaminationDetected.Broadcast(Landed.Actor);
			}
		}
	}

	return bConfirmedMembershipChanged || !FMath::IsNearlyEqual(PreviousPanMassKg, PanMassKg, 1.0e-4f);
}

bool UScaleWeighingComponent::TryRecoverPayloadOnPan(AActor* PayloadActor)
{
	if (!IsValid(PayloadActor) || PayloadActor == GetOwner() || !PayloadActor->Implements<UMassPayloadInterface>())
	{
		return false;
	}

	const EChunkRoutingState CurrentState = IMassPayloadInterface::Execute_GetRoutingState(PayloadActor);
	if (CurrentState == EChunkRoutingState::PooledInactive ||
		CurrentState == EChunkRoutingState::ConsumedAsTare ||
		CurrentState == EChunkRoutingState::LostAsStrayTare)
	{
		return false;
	}

	const int32 ExistingIdx = FindTrackedPayloadIndex(PayloadActor);
	if (ExistingIdx != INDEX_NONE &&
		TrackedPayloads[ExistingIdx].bHasConfirmedScaleImpact &&
		CurrentState == EChunkRoutingState::OnScalePan)
	{
		RecomputeAnalyticalPanMass();
		return true;
	}

	if (!IsPayloadWithinPanRecoveryWell(PayloadActor))
	{
		return false;
	}

	UPrimitiveComponent* PayloadPrim = IMassPayloadInterface::Execute_GetPayloadPhysicsPrimitive(PayloadActor);
	if (IsValid(PayloadPrim) && PayloadPrim->IsSimulatingPhysics())
	{
		// Do not prematurely recover a chunk that is still actively free-falling downward inside the pan well.
		if (PayloadPrim->GetPhysicsLinearVelocity().Z < -25.0)
		{
			return false;
		}
	}

	return ConfirmPayloadScaleImpact(PayloadActor, PayloadPrim, PayloadActor->GetActorLocation(), FVector::ZeroVector);
}

void UScaleWeighingComponent::RecomputeAnalyticalPanMass()
{
	double TotalMassKg = 0.0;
	bool bAnyContaminated = false;

	for (const FTrackedPanPayload& Entry : TrackedPayloads)
	{
		if (!Entry.bHasConfirmedScaleImpact)
		{
			continue;
		}

		AActor* Actor = Entry.PayloadActor.Get();
		if (!IsValid(Actor) || !Actor->Implements<UMassPayloadInterface>())
		{
			continue;
		}

		if (IMassPayloadInterface::Execute_GetRoutingState(Actor) == EChunkRoutingState::OnScalePan)
		{
			TotalMassKg += FMath::Max(0.0, static_cast<double>(IMassPayloadInterface::Execute_GetAnalyticalMassKg(Actor)));
			if (IMassPayloadInterface::Execute_IsVolatileContaminated(Actor))
			{
				bAnyContaminated = true;
			}
		}
	}

	PanMassKg = static_cast<float>(TotalMassKg);
	bHasContamination = bAnyContaminated;
	TargetDisplacementZCm = -static_cast<double>(FMath::Clamp(PanMassKg * DepressionCmPerKg, 0.0f, MaxDepressionCm));

	const float NormalizedFill = FMath::Clamp(PanMassKg / FMath::Max(0.5f, TargetQuotaMassKg), 0.0f, 1.0f);
	EmitFillRatioTelemetry(NormalizedFill);
}

bool UScaleWeighingComponent::AreAllPayloadsBelowSettleVelocity() const
{
	const double MaxSpeedSq = FMath::Square(static_cast<double>(PayloadLinearSpeedSettleThresholdCmPerSec));

	for (const FTrackedPanPayload& Entry : TrackedPayloads)
	{
		if (!Entry.bHasConfirmedScaleImpact)
		{
			continue;
		}

		const UPrimitiveComponent* PrimComp = Entry.PayloadPrimitive.Get();
		if (IsValid(PrimComp) && PrimComp->IsSimulatingPhysics())
		{
			if (PrimComp->GetPhysicsLinearVelocity().SizeSquared() >= MaxSpeedSq)
			{
				return false;
			}
		}
	}

	return true;
}

void UScaleWeighingComponent::WakeOverlappingPayloads()
{
	for (const FTrackedPanPayload& Entry : TrackedPayloads)
	{
		if (!Entry.bHasConfirmedScaleImpact)
		{
			continue;
		}

		UPrimitiveComponent* PrimComp = Entry.PayloadPrimitive.Get();
		if (IsValid(PrimComp) && PrimComp->IsSimulatingPhysics())
		{
			PrimComp->WakeAllRigidBodies();
		}
	}
}

void UScaleWeighingComponent::ApplyPanPlatformDisplacement(bool bSweep)
{
	if (!IsValid(PanPlatformComponent))
	{
		return;
	}

	const FVector TargetRelLocation = NeutralPanRelativeLocation + FVector(0.0, 0.0, CurrentDisplacementZCm);
	PanPlatformComponent->SetRelativeLocation(TargetRelLocation, bSweep, nullptr, ETeleportType::None);
}

void UScaleWeighingComponent::SetSettledState(bool bNewSettled)
{
	if (bIsSettled == bNewSettled)
	{
		return;
	}

	bIsSettled = bNewSettled;
	OnScaleSettledChanged.Broadcast(bIsSettled, PanMassKg);
}

void UScaleWeighingComponent::EmitFillRatioTelemetry(float NormalizedFillRatio, bool bForceEmit)
{
	const float ClampedVal = FMath::Clamp(NormalizedFillRatio, 0.0f, 1.0f);
	const bool bIsEndpoint = (ClampedVal <= 0.0f) || (ClampedVal >= 1.0f);
	const bool bEndpointChanged = bIsEndpoint && !FMath::IsNearlyEqual(ClampedVal, LastEmittedFillRatio, KINDA_SMALL_NUMBER);
	const bool bExceededDeadband = FMath::Abs(ClampedVal - LastEmittedFillRatio) >= TelemetryDeadband;

	if (bForceEmit || LastEmittedFillRatio < 0.0f || bEndpointChanged || bExceededDeadband)
	{
		LastEmittedFillRatio = ClampedVal;
		OnTelemetryStream.Broadcast(ETelemetryStreamId::ScaleFillRatio, ClampedVal);
	}
}
