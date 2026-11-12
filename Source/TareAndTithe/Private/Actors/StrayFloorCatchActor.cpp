// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/StrayFloorCatchActor.h"
#include "Components/BoxComponent.h"
#include "Engine/HitResult.h"
#include "Interfaces/MassPayloadInterface.h"

AStrayFloorCatchActor::AStrayFloorCatchActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	CatchBoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("CatchBoxComponent"));
	SetRootComponent(CatchBoxComponent);

	CatchBoxComponent->PrimaryComponentTick.bCanEverTick = false;
	CatchBoxComponent->PrimaryComponentTick.bStartWithTickEnabled = false;
	CatchBoxComponent->InitBoxExtent(CatchBoxHalfExtentCm);
	CatchBoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CatchBoxComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CatchBoxComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CatchBoxComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	CatchBoxComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	CatchBoxComponent->SetGenerateOverlapEvents(true);
}

void AStrayFloorCatchActor::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(CatchBoxComponent))
	{
		CatchBoxComponent->SetBoxExtent(CatchBoxHalfExtentCm, true);
		CatchBoxComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &AStrayFloorCatchActor::HandleCatchBoxBeginOverlap);
	}
}

void AStrayFloorCatchActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(CatchBoxComponent))
	{
		CatchBoxComponent->OnComponentBeginOverlap.RemoveDynamic(this, &AStrayFloorCatchActor::HandleCatchBoxBeginOverlap);
	}
	Super::EndPlay(EndPlayReason);
}

void AStrayFloorCatchActor::HandleCatchBoxBeginOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	if (!IsValid(OtherActor) || OtherActor == this || !OtherActor->Implements<UMassPayloadInterface>())
	{
		return;
	}

	const EChunkRoutingState PreviousState = IMassPayloadInterface::Execute_GetRoutingState(OtherActor);
	if (PreviousState == EChunkRoutingState::PooledInactive ||
		PreviousState == EChunkRoutingState::ConsumedAsTare ||
		PreviousState == EChunkRoutingState::LostAsStrayTare)
	{
		return;
	}

	const float AnalyticalMassKg = IMassPayloadInterface::Execute_GetAnalyticalMassKg(OtherActor);
	const bool bWasContaminated = IMassPayloadInterface::Execute_IsVolatileContaminated(OtherActor);
	const FVector CatchLocationWorld = OtherActor->GetActorLocation();

	IMassPayloadInterface::Execute_NotifyRoutingStateChanged(OtherActor, EChunkRoutingState::LostAsStrayTare);
	OnStrayChunkCaught.Broadcast(OtherActor, AnalyticalMassKg, PreviousState, bWasContaminated);

	const float NormalizedMagnitude = FMath::Clamp(AnalyticalMassKg / FMath::Max(0.5f, ImpulseReferenceMassKg), 0.05f, 1.0f);
	OnTelemetryImpulse.Broadcast(ETelemetryImpulseId::ChunkTareDisposed, NormalizedMagnitude, CatchLocationWorld);

	IMassPayloadInterface::Execute_ReturnToChunkPool(OtherActor);
}
