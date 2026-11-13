// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DiverterGateComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"
#include "Interfaces/MassPayloadInterface.h"

namespace
{
	constexpr float FlapperAngleConvergenceEpsilonDeg = 0.01f;
}

UDiverterGateComponent::UDiverterGateComponent()
{
	// Tier 2: Enabled strictly during the 0.18s kinematic flapper deflection.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UDiverterGateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(FlapperPrimitiveComponent))
	{
		FlapperPrimitiveComponent->OnComponentHit.RemoveDynamic(this, &UDiverterGateComponent::HandleFlapperPrimitiveHit);
	}
	if (IsValid(TareChuteTriggerComponent))
	{
		TareChuteTriggerComponent->OnComponentBeginOverlap.RemoveDynamic(this, &UDiverterGateComponent::HandleTareTriggerBeginOverlap);
	}
	if (IsValid(TareAbyssDespawnTriggerComponent))
	{
		TareAbyssDespawnTriggerComponent->OnComponentBeginOverlap.RemoveDynamic(this, &UDiverterGateComponent::HandleTareAbyssDespawnBeginOverlap);
	}
	Super::EndPlay(EndPlayReason);
}

void UDiverterGateComponent::SetFlapperComponent(USceneComponent* InFlapperComponent)
{
	if (IsValid(FlapperPrimitiveComponent))
	{
		FlapperPrimitiveComponent->OnComponentHit.RemoveDynamic(this, &UDiverterGateComponent::HandleFlapperPrimitiveHit);
	}
	FlapperComponent = InFlapperComponent;
	FlapperPrimitiveComponent = nullptr;
	if (IsValid(FlapperComponent))
	{
		BaseFlapperRelativeRotation = FlapperComponent->GetRelativeRotation().Quaternion();
		ApplyFlapperRotation(false);
	}
}

void UDiverterGateComponent::SetFlapperComponent(UPrimitiveComponent* InFlapperPrimitive)
{
	SetFlapperPivotAndPrimitive(InFlapperPrimitive, InFlapperPrimitive);
}

void UDiverterGateComponent::SetFlapperPivotAndPrimitive(
	USceneComponent* InPivotComponent,
	UPrimitiveComponent* InFlapperPrimitive)
{
	if (IsValid(FlapperPrimitiveComponent))
	{
		FlapperPrimitiveComponent->OnComponentHit.RemoveDynamic(this, &UDiverterGateComponent::HandleFlapperPrimitiveHit);
	}

	FlapperComponent = InPivotComponent;
	FlapperPrimitiveComponent = InFlapperPrimitive;

	if (IsValid(FlapperPrimitiveComponent))
	{
		FlapperPrimitiveComponent->SetNotifyRigidBodyCollision(true);
		FlapperPrimitiveComponent->OnComponentHit.AddUniqueDynamic(this, &UDiverterGateComponent::HandleFlapperPrimitiveHit);
	}

	if (IsValid(FlapperComponent))
	{
		BaseFlapperRelativeRotation = FlapperComponent->GetRelativeRotation().Quaternion();
		ApplyFlapperRotation(false);
	}
}

void UDiverterGateComponent::BindTareChuteTrigger(UPrimitiveComponent* InTareTriggerComponent)
{
	if (IsValid(TareChuteTriggerComponent))
	{
		TareChuteTriggerComponent->OnComponentBeginOverlap.RemoveDynamic(this, &UDiverterGateComponent::HandleTareTriggerBeginOverlap);
	}

	TareChuteTriggerComponent = InTareTriggerComponent;

	if (IsValid(TareChuteTriggerComponent))
	{
		TareChuteTriggerComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		TareChuteTriggerComponent->SetCollisionObjectType(ECC_WorldDynamic);
		TareChuteTriggerComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		TareChuteTriggerComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
		TareChuteTriggerComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		TareChuteTriggerComponent->SetGenerateOverlapEvents(true);
		TareChuteTriggerComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &UDiverterGateComponent::HandleTareTriggerBeginOverlap);
	}
}

void UDiverterGateComponent::BindTareAbyssDespawnTrigger(UPrimitiveComponent* InAbyssTriggerComponent)
{
	if (IsValid(TareAbyssDespawnTriggerComponent))
	{
		TareAbyssDespawnTriggerComponent->OnComponentBeginOverlap.RemoveDynamic(this, &UDiverterGateComponent::HandleTareAbyssDespawnBeginOverlap);
	}

	TareAbyssDespawnTriggerComponent = InAbyssTriggerComponent;

	if (IsValid(TareAbyssDespawnTriggerComponent))
	{
		TareAbyssDespawnTriggerComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		TareAbyssDespawnTriggerComponent->SetCollisionObjectType(ECC_WorldDynamic);
		TareAbyssDespawnTriggerComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		TareAbyssDespawnTriggerComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
		TareAbyssDespawnTriggerComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		TareAbyssDespawnTriggerComponent->SetGenerateOverlapEvents(true);
		TareAbyssDespawnTriggerComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &UDiverterGateComponent::HandleTareAbyssDespawnBeginOverlap);
	}
}

bool UDiverterGateComponent::LatchPayloadAsTareOnce(AActor* OtherActor)
{
	if (!IsValid(OtherActor) || OtherActor == GetOwner() || !OtherActor->Implements<UMassPayloadInterface>())
	{
		return false;
	}

	const EChunkRoutingState PayloadState = IMassPayloadInterface::Execute_GetRoutingState(OtherActor);
	if (PayloadState == EChunkRoutingState::PooledInactive || PayloadState == EChunkRoutingState::ConsumedAsTare)
	{
		return false;
	}

	const float PayloadMassKg = IMassPayloadInterface::Execute_GetAnalyticalMassKg(OtherActor);
	const bool bWasContaminated = IMassPayloadInterface::Execute_IsVolatileContaminated(OtherActor);
	const FVector DisposalWorldLocation = OtherActor->GetActorLocation();

	IMassPayloadInterface::Execute_NotifyRoutingStateChanged(OtherActor, EChunkRoutingState::ConsumedAsTare);
	IMassPayloadInterface::Execute_ArmTareAbyssFailsafeTimer(OtherActor, TareAbyssFailsafeSeconds);
	OnChunkRoutedToTare.Broadcast(OtherActor, PayloadMassKg, bWasContaminated);

	const float NormalizedMagnitude = FMath::Clamp(PayloadMassKg / FMath::Max(0.5f, ImpulseReferenceMassKg), 0.05f, 1.0f);
	OnTelemetryImpulse.Broadcast(ETelemetryImpulseId::ChunkTareDisposed, NormalizedMagnitude, DisposalWorldLocation);
	return true;
}

void UDiverterGateComponent::ProcessTareChuteOverlap(AActor* OtherActor)
{
	// Mouth trigger latches ConsumedAsTare accounting once WITHOUT despawning, allowing the chunk to visibly plunge down the open shaft.
	LatchPayloadAsTareOnce(OtherActor);
}

void UDiverterGateComponent::ConfigureForContract(bool bInAllowToggle, EDiverterRoute InitialRoute)
{
	bAllowToggle = bInAllowToggle;
	SetRoute(InitialRoute, true);
}

bool UDiverterGateComponent::ToggleRoute()
{
	if (!bAllowToggle)
	{
		return false;
	}

	const EDiverterRoute NextRoute = (CurrentRoute == EDiverterRoute::TitheScale)
		? EDiverterRoute::TareChute
		: EDiverterRoute::TitheScale;
	return SetRoute(NextRoute, false);
}

bool UDiverterGateComponent::SetRoute(EDiverterRoute NewRoute, bool bInstant)
{
	if (!bInstant && (!bAllowToggle || CurrentRoute == NewRoute))
	{
		return false;
	}

	CurrentRoute = NewRoute;
	TargetAngleDegrees = GetAngleForRoute(CurrentRoute);

	if (bInstant)
	{
		CurrentAngleDegrees = TargetAngleDegrees;
		ApplyFlapperRotation(false);
		SetComponentTickEnabled(false);
		OnRouteChanged.Broadcast(CurrentRoute);
		OnRouteSlewCompleted.Broadcast(CurrentRoute);
		return true;
	}

	WakeTouchingBodies();
	SetComponentTickEnabled(true);

	const FVector ImpulseWorldLocation = IsValid(FlapperComponent)
		? FlapperComponent->GetComponentLocation()
		: (GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector);
	OnTelemetryImpulse.Broadcast(ETelemetryImpulseId::DiverterToggled, 1.0f, ImpulseWorldLocation);
	OnRouteChanged.Broadcast(CurrentRoute);
	return true;
}

void UDiverterGateComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (DeltaTime <= 0.0f)
	{
		return;
	}

	const float TotalSpanDegrees = FMath::Max(1.0f, FMath::Abs(TareAngleDegrees - TitheAngleDegrees));
	const float SafeDuration = FMath::Max(0.02f, TransitionDurationSeconds);
	const float AngularSpeedDegPerSec = TotalSpanDegrees / SafeDuration;

	CurrentAngleDegrees = FMath::FInterpConstantTo(
		CurrentAngleDegrees,
		TargetAngleDegrees,
		DeltaTime,
		AngularSpeedDegPerSec);

	const bool bConverged = FMath::IsNearlyEqual(CurrentAngleDegrees, TargetAngleDegrees, FlapperAngleConvergenceEpsilonDeg);
	if (bConverged)
	{
		CurrentAngleDegrees = TargetAngleDegrees;
	}

	ApplyFlapperRotation(true);
	WakeTouchingBodies();

	if (bConverged)
	{
		SetComponentTickEnabled(false);
		OnRouteSlewCompleted.Broadcast(CurrentRoute);
	}
}

void UDiverterGateComponent::HandleTareTriggerBeginOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	ProcessTareChuteOverlap(OtherActor);
}

void UDiverterGateComponent::HandleTareAbyssDespawnBeginOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	if (!IsValid(OtherActor) || OtherActor == GetOwner() || !OtherActor->Implements<UMassPayloadInterface>())
	{
		return;
	}

	const EChunkRoutingState PayloadState = IMassPayloadInterface::Execute_GetRoutingState(OtherActor);
	if (PayloadState == EChunkRoutingState::PooledInactive)
	{
		return;
	}

	LatchPayloadAsTareOnce(OtherActor);
	IMassPayloadInterface::Execute_ReturnToChunkPool(OtherActor);
}

void UDiverterGateComponent::HandleFlapperPrimitiveHit(
	UPrimitiveComponent* /*HitComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	FVector /*NormalImpulse*/,
	const FHitResult& /*Hit*/)
{
	if (CurrentRoute != EDiverterRoute::TareChute ||
		!IsValid(OtherActor) ||
		OtherActor == GetOwner() ||
		!OtherActor->Implements<UMassPayloadInterface>())
	{
		return;
	}

	const EChunkRoutingState PayloadState = IMassPayloadInterface::Execute_GetRoutingState(OtherActor);
	if (PayloadState == EChunkRoutingState::ConsumedAsTare)
	{
		IMassPayloadInterface::Execute_ArmTareAbyssFailsafeTimer(OtherActor, TareAbyssFailsafeSeconds);
	}
}

void UDiverterGateComponent::ApplyFlapperRotation(bool /*bSweep*/)
{
	if (!IsValid(FlapperComponent))
	{
		return;
	}

	FVector SafeAxis = FlapperLocalRotationAxis.GetSafeNormal();
	if (SafeAxis.IsNearlyZero())
	{
		SafeAxis = FVector::ForwardVector;
	}

	const FQuat DeltaQuat(SafeAxis, FMath::DegreesToRadians(static_cast<double>(CurrentAngleDegrees)));
	const FQuat ComposedQuat = BaseFlapperRelativeRotation * DeltaQuat;

	// Drive kinematic target pose via ETeleportType::None without sweep-halting so falling chunks never wedge the gate mid-swing.
	FlapperComponent->SetRelativeRotation(
		ComposedQuat,
		false,
		nullptr,
		ETeleportType::None);
}

void UDiverterGateComponent::WakeTouchingBodies()
{
	if (!IsValid(FlapperPrimitiveComponent))
	{
		return;
	}

	TArray<UPrimitiveComponent*> OverlappingComponents;
	FlapperPrimitiveComponent->GetOverlappingComponents(OverlappingComponents);
	for (UPrimitiveComponent* Comp : OverlappingComponents)
	{
		if (IsValid(Comp) && Comp->IsSimulatingPhysics())
		{
			Comp->WakeAllRigidBodies();
		}
	}
}

float UDiverterGateComponent::GetAngleForRoute(EDiverterRoute Route) const
{
	return (Route == EDiverterRoute::TareChute) ? TareAngleDegrees : TitheAngleDegrees;
}
