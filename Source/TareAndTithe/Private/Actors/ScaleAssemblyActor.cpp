// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/ScaleAssemblyActor.h"
#include "Components/BoxComponent.h"
#include "Components/DiverterGateComponent.h"
#include "Components/ScaleWeighingComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Data/MonolithContractDataAsset.h"
#include "Data/ShiftProgressionDataAsset.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Subsystems/InspectionShiftSubsystem.h"
#include "UObject/ConstructorHelpers.h"

AScaleAssemblyActor::AScaleAssemblyActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	ScaleRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ScaleRoot"));
	SetRootComponent(ScaleRoot);
	ScaleRoot->PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* CubeMesh = CubeMeshFinder.Succeeded() ? CubeMeshFinder.Object : nullptr;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMatFinder(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMatFinder.Succeeded())
	{
		BaseGreyboxShapeMaterial = BasicMatFinder.Object;
	}

	auto ConfigureSolidKinematicMesh = [CubeMesh](UStaticMeshComponent* MeshComp)
	{
		MeshComp->PrimaryComponentTick.bCanEverTick = false;
		MeshComp->SetSimulatePhysics(false);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshComp->SetCollisionObjectType(ECC_WorldDynamic);
		MeshComp->SetCollisionResponseToAllChannels(ECR_Block);
		MeshComp->SetGenerateOverlapEvents(false);
		if (IsValid(CubeMesh))
		{
			MeshComp->SetStaticMesh(CubeMesh);
		}
	};

	ScaleBaseVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScaleBaseVisual"));
	ScaleBaseVisual->SetupAttachment(ScaleRoot);
	ConfigureSolidKinematicMesh(ScaleBaseVisual);
	ScaleBaseVisual->SetRelativeLocation(FVector(0.0, 0.0, 6.0));
	ScaleBaseVisual->SetRelativeScale3D(FVector(0.84, 0.72, 0.12));

	LeftHopperGuideVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftHopperGuideVisual"));
	LeftHopperGuideVisual->SetupAttachment(ScaleRoot);
	ConfigureSolidKinematicMesh(LeftHopperGuideVisual);
	LeftHopperGuideVisual->SetRelativeLocation(FVector(0.0, -36.0, 72.0));
	LeftHopperGuideVisual->SetRelativeScale3D(FVector(0.86, 0.03, 1.15));

	PanPlatform = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PanPlatform"));
	PanPlatform->SetupAttachment(ScaleRoot);
	ConfigureSolidKinematicMesh(PanPlatform);
	PanPlatform->SetRelativeLocation(FVector(0.0, 0.0, 22.0));
	PanPlatform->SetRelativeScale3D(FVector(0.78, 0.68, 0.06));

	PanTriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("PanTriggerBox"));
	PanTriggerBox->SetupAttachment(PanPlatform);
	PanTriggerBox->PrimaryComponentTick.bCanEverTick = false;
	PanTriggerBox->SetRelativeScale3D(FVector(1.0 / 0.78, 1.0 / 0.68, 1.0 / 0.06));
	PanTriggerBox->SetRelativeLocation(FVector(0.0, 0.0, 560.0));
	PanTriggerBox->InitBoxExtent(FVector(38.0, 33.0, 32.0));

	// Vertical right hopper wall at 0 deg (TitheScale); swings +45 deg around +X to shield pan & chute chunks into TareChuteTriggerBox.
	DiverterFlapperVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DiverterFlapperVisual"));
	DiverterFlapperVisual->SetupAttachment(ScaleRoot);
	ConfigureSolidKinematicMesh(DiverterFlapperVisual);
	DiverterFlapperVisual->SetRelativeLocation(FVector(0.0, 36.0, 94.0));
	DiverterFlapperVisual->SetRelativeScale3D(FVector(0.86, 0.03, 1.60));

	TareChuteVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TareChuteVisual"));
	TareChuteVisual->SetupAttachment(ScaleRoot);
	TareChuteVisual->PrimaryComponentTick.bCanEverTick = false;
	TareChuteVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TareChuteVisual->SetGenerateOverlapEvents(false);
	if (IsValid(CubeMesh))
	{
		TareChuteVisual->SetStaticMesh(CubeMesh);
	}
	TareChuteVisual->SetRelativeLocation(FVector(0.0, 96.0, 4.0));
	TareChuteVisual->SetRelativeScale3D(FVector(0.86, 0.72, 0.08));

	TareChuteTriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TareChuteTriggerBox"));
	TareChuteTriggerBox->SetupAttachment(ScaleRoot);
	TareChuteTriggerBox->PrimaryComponentTick.bCanEverTick = false;
	TareChuteTriggerBox->SetRelativeLocation(FVector(0.0, 96.0, 26.0));
	TareChuteTriggerBox->InitBoxExtent(FVector(52.0, 42.0, 34.0));

	ScaleWeighingComponent = CreateDefaultSubobject<UScaleWeighingComponent>(TEXT("ScaleWeighingComponent"));
	DiverterGateComponent = CreateDefaultSubobject<UDiverterGateComponent>(TEXT("DiverterGateComponent"));

	auto ConfigureReadoutText = [](UTextRenderComponent* TextComp, const FVector& RelLoc, float WorldSize, const FColor& Color, const TCHAR* DefaultStr)
	{
		TextComp->PrimaryComponentTick.bCanEverTick = false;
		TextComp->SetRelativeLocation(RelLoc);
		TextComp->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
		TextComp->SetHorizontalAlignment(EHTA_Center);
		TextComp->SetVerticalAlignment(EVRTA_TextCenter);
		TextComp->SetWorldSize(WorldSize);
		TextComp->SetTextRenderColor(Color);
		TextComp->SetText(FText::FromString(DefaultStr));
	};

	HeaderStencilText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("HeaderStencilText"));
	HeaderStencilText->SetupAttachment(ScaleRoot);
	ConfigureReadoutText(HeaderStencilText, FVector(-44.0, 0.0, 34.0), 6.5f, FColor(235, 238, 242, 255), TEXT("DIRECTOR'S CUT"));

	ContractCodeText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ContractCodeText"));
	ContractCodeText->SetupAttachment(ScaleRoot);
	ConfigureReadoutText(ContractCodeText, FVector(-44.0, 0.0, 27.5), 4.0f, ReadoutCyanColor, TEXT("AWAITING CONTRACT"));

	MassReadoutText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("MassReadoutText"));
	MassReadoutText->SetupAttachment(ScaleRoot);
	ConfigureReadoutText(MassReadoutText, FVector(-44.0, 0.0, 19.5), 8.2f, ReadoutAmberColor, TEXT("00.00 / 10.00 KG"));

	TareReadoutText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TareReadoutText"));
	TareReadoutText->SetupAttachment(ScaleRoot);
	ConfigureReadoutText(TareReadoutText, FVector(-44.0, 0.0, 11.5), 5.2f, ReadoutAmberColor, TEXT("TARE: 0.0 / 12.0 KG"));

	RouteReadoutText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("RouteReadoutText"));
	RouteReadoutText->SetupAttachment(ScaleRoot);
	ConfigureReadoutText(RouteReadoutText, FVector(-44.0, 0.0, 4.5), 5.2f, ReadoutCyanColor, TEXT("ROUTE: TITHE"));
}

void AScaleAssemblyActor::BeginPlay()
{
	Super::BeginPlay();

	InitializeGreyboxVisualMaterials();

	if (IsValid(ScaleWeighingComponent))
	{
		ScaleWeighingComponent->SetPanPlatformComponent(PanPlatform);
		ScaleWeighingComponent->BindPanContainmentTrigger(PanTriggerBox);
		ScaleWeighingComponent->OnTelemetryStream.AddUObject(this, &AScaleAssemblyActor::HandleChildTelemetryStream);
		ScaleWeighingComponent->OnTelemetryImpulse.AddUObject(this, &AScaleAssemblyActor::HandleChildTelemetryImpulse);
		ScaleWeighingComponent->OnChunkLandedOnScale.AddUObject(this, &AScaleAssemblyActor::HandleChunkLandedOnScale);
		ScaleWeighingComponent->OnChunkBouncedOffScale.AddUObject(this, &AScaleAssemblyActor::HandleChunkBouncedOffScale);
		ScaleWeighingComponent->OnScaleContaminationDetected.AddUObject(this, &AScaleAssemblyActor::HandleScaleContaminationDetected);
		ScaleWeighingComponent->OnScaleSettledChanged.AddUObject(this, &AScaleAssemblyActor::HandleScaleSettledChanged);
	}

	if (IsValid(DiverterGateComponent))
	{
		DiverterGateComponent->SetFlapperComponent(DiverterFlapperVisual);
		DiverterGateComponent->BindTareChuteTrigger(TareChuteTriggerBox);
		DiverterGateComponent->OnRouteChanged.AddUObject(this, &AScaleAssemblyActor::HandleDiverterRouteChanged);
		DiverterGateComponent->OnChunkRoutedToTare.AddUObject(this, &AScaleAssemblyActor::HandleChunkRoutedToTare);
		DiverterGateComponent->OnTelemetryImpulse.AddUObject(this, &AScaleAssemblyActor::HandleChildTelemetryImpulse);
	}

	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->RegisterTelemetryPublisher(this, this);
			ShiftSubsystem->OnContractSnapshotUpdated.AddUniqueDynamic(this, &AScaleAssemblyActor::HandleContractSnapshotUpdated);
			ShiftSubsystem->OnActiveContractLoaded.AddUObject(this, &AScaleAssemblyActor::HandleActiveContractLoaded);

			FShiftQuotaReceiverBinding Binding;
			Binding.QuotaReceiver.SetObject(this);
			Binding.QuotaReceiver.SetInterface(this);
			Binding.OnConfigureContract.BindUObject(this, &AScaleAssemblyActor::ConfigureForContract);
			Binding.OnToggleDiverter.BindUObject(this, &AScaleAssemblyActor::RequestToggleDiverterRoute);
			Binding.OnQueryWorldLocation.BindUObject(this, &AScaleAssemblyActor::QueryScaleWorldLocation);

			ShiftSubsystem->RegisterQuotaReceiver(Binding);
		}
	}

	RefreshRouteReadout();
}

void AScaleAssemblyActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->OnContractSnapshotUpdated.RemoveDynamic(this, &AScaleAssemblyActor::HandleContractSnapshotUpdated);
			ShiftSubsystem->OnActiveContractLoaded.RemoveAll(this);
			ShiftSubsystem->UnregisterQuotaReceiver(this);
			ShiftSubsystem->UnregisterTelemetryPublisher(this);
		}
	}

	if (IsValid(ScaleWeighingComponent))
	{
		ScaleWeighingComponent->OnTelemetryStream.RemoveAll(this);
		ScaleWeighingComponent->OnTelemetryImpulse.RemoveAll(this);
		ScaleWeighingComponent->OnChunkLandedOnScale.RemoveAll(this);
		ScaleWeighingComponent->OnChunkBouncedOffScale.RemoveAll(this);
		ScaleWeighingComponent->OnScaleContaminationDetected.RemoveAll(this);
		ScaleWeighingComponent->OnScaleSettledChanged.RemoveAll(this);
	}

	if (IsValid(DiverterGateComponent))
	{
		DiverterGateComponent->OnRouteChanged.RemoveAll(this);
		DiverterGateComponent->OnChunkRoutedToTare.RemoveAll(this);
		DiverterGateComponent->OnTelemetryImpulse.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

float AScaleAssemblyActor::GetSettledMassKg_Implementation() const
{
	return IsValid(ScaleWeighingComponent) ? ScaleWeighingComponent->GetSettledMassKg() : 0.0f;
}

bool AScaleAssemblyActor::IsReceiverSettled_Implementation() const
{
	return IsValid(ScaleWeighingComponent) ? ScaleWeighingComponent->IsScaleSettled() : true;
}

bool AScaleAssemblyActor::HasContaminationFault_Implementation() const
{
	return IsValid(ScaleWeighingComponent) ? ScaleWeighingComponent->HasContaminationFault() : false;
}

EDiverterRoute AScaleAssemblyActor::GetActiveDiverterRoute_Implementation() const
{
	return IsValid(DiverterGateComponent) ? DiverterGateComponent->GetCurrentRoute() : EDiverterRoute::TitheScale;
}

void AScaleAssemblyActor::ResetReceiverState_Implementation()
{
	if (IsValid(ScaleWeighingComponent))
	{
		ScaleWeighingComponent->ResetScaleState();
	}
	if (IsValid(DiverterGateComponent))
	{
		DiverterGateComponent->SetRoute(EDiverterRoute::TitheScale, true);
	}
	RefreshRouteReadout();
}

void AScaleAssemblyActor::ConfigureForContract(const UMonolithContractDataAsset* Contract, EDiverterRoute InitialRoute)
{
	if (IsValid(ScaleWeighingComponent))
	{
		ScaleWeighingComponent->ConfigureForContract(Contract);
	}

	if (IsValid(DiverterGateComponent))
	{
		const bool bAllowToggle = IsValid(Contract) ? Contract->bAllowDiverterToggle : true;
		DiverterGateComponent->ConfigureForContract(bAllowToggle, InitialRoute);
	}

	if (IsValid(ContractCodeText) && IsValid(Contract))
	{
		ContractCodeText->SetText(Contract->ContractCode);
	}

	RefreshRouteReadout();
}

bool AScaleAssemblyActor::RequestToggleDiverterRoute()
{
	if (!IsValid(DiverterGateComponent))
	{
		return false;
	}

	const bool bToggled = DiverterGateComponent->ToggleRoute();
	RefreshRouteReadout();
	return bToggled;
}

void AScaleAssemblyActor::HandleContractSnapshotUpdated(const FContractRuntimeSnapshot& Snapshot)
{
	const bool bContaminated = HasContaminationFault_Implementation();
	const bool bOvershoot = Snapshot.SettledScaleMassKg > (Snapshot.TargetTitheMassKg + Snapshot.QuotaToleranceKg + 1.0e-3f);

	if (IsValid(MassReadoutText))
	{
		const FString StampSuffix = Snapshot.bScaleSettledForStamp ? TEXT(" [STAMP READY: E]") : TEXT("");
		const FString MassLine = FString::Printf(
			TEXT("%05.2f / %05.2f KG%s"),
			Snapshot.SettledScaleMassKg,
			Snapshot.TargetTitheMassKg,
			*StampSuffix);
		MassReadoutText->SetText(FText::FromString(MassLine));

		if (bContaminated || bOvershoot)
		{
			MassReadoutText->SetTextRenderColor(ReadoutRedColor);
		}
		else if (Snapshot.bIsWithinQuotaWindow)
		{
			MassReadoutText->SetTextRenderColor(ReadoutGreenColor);
		}
		else
		{
			MassReadoutText->SetTextRenderColor(ReadoutAmberColor);
		}
	}

	if (IsValid(TareReadoutText))
	{
		const FString TareLine = FString::Printf(
			TEXT("TARE: %.1f / %.1f KG"),
			Snapshot.AccumulatedTareWasteKg,
			Snapshot.MaxAllowedTareWasteKg);
		TareReadoutText->SetText(FText::FromString(TareLine));

		const bool bTareExceeded = Snapshot.AccumulatedTareWasteKg > (Snapshot.MaxAllowedTareWasteKg + 1.0e-3f);
		TareReadoutText->SetTextRenderColor(bTareExceeded ? ReadoutRedColor : ReadoutAmberColor);
	}

	RefreshRouteReadout();
}

void AScaleAssemblyActor::HandleActiveContractLoaded(
	const UMonolithContractDataAsset* Contract,
	const UShiftProgressionDataAsset* /*ShiftConfig*/,
	int32 /*ContractIndex*/)
{
	if (IsValid(ContractCodeText) && IsValid(Contract))
	{
		ContractCodeText->SetText(Contract->ContractCode);
	}
	RefreshRouteReadout();
}

void AScaleAssemblyActor::HandleDiverterRouteChanged(EDiverterRoute /*NewRoute*/)
{
	RefreshRouteReadout();
}

void AScaleAssemblyActor::RefreshRouteReadout()
{
	if (!IsValid(RouteReadoutText) || !IsValid(DiverterGateComponent))
	{
		return;
	}

	const EDiverterRoute ActiveRoute = DiverterGateComponent->GetCurrentRoute();
	if (ActiveRoute == EDiverterRoute::TareChute)
	{
		RouteReadoutText->SetText(FText::FromString(TEXT("ROUTE: TARE")));
		RouteReadoutText->SetTextRenderColor(ReadoutAmberColor);
	}
	else
	{
		RouteReadoutText->SetText(FText::FromString(TEXT("ROUTE: TITHE")));
		RouteReadoutText->SetTextRenderColor(ReadoutCyanColor);
	}
}

void AScaleAssemblyActor::InitializeGreyboxVisualMaterials()
{
	if (!IsValid(BaseGreyboxShapeMaterial))
	{
		return;
	}

	auto ApplyTintedGreyboxMaterial = [this](UStaticMeshComponent* MeshComp, const FLinearColor& LinearColor)
	{
		if (IsValid(MeshComp))
		{
			if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseGreyboxShapeMaterial, this))
			{
				MID->SetVectorParameterValue(TEXT("Color"), LinearColor);
				MeshComp->SetMaterial(0, MID);
			}
		}
	};

	ApplyTintedGreyboxMaterial(ScaleBaseVisual, FLinearColor(0.05f, 0.055f, 0.065f, 1.0f));
	ApplyTintedGreyboxMaterial(LeftHopperGuideVisual, FLinearColor(0.11f, 0.12f, 0.14f, 1.0f));
	ApplyTintedGreyboxMaterial(PanPlatform, FLinearColor(0.22f, 0.19f, 0.12f, 1.0f));
	ApplyTintedGreyboxMaterial(DiverterFlapperVisual, FLinearColor(0.26f, 0.14f, 0.04f, 1.0f));
	ApplyTintedGreyboxMaterial(TareChuteVisual, FLinearColor(0.08f, 0.035f, 0.03f, 1.0f));
}

FVector AScaleAssemblyActor::QueryScaleWorldLocation() const
{
	return GetActorLocation();
}

void AScaleAssemblyActor::HandleChildTelemetryStream(ETelemetryStreamId StreamId, float NormalizedValue)
{
	OnTelemetryStream.Broadcast(StreamId, NormalizedValue);
}

void AScaleAssemblyActor::HandleChildTelemetryImpulse(
	ETelemetryImpulseId ImpulseId,
	float NormalizedMagnitude,
	const FVector& WorldLocation)
{
	OnTelemetryImpulse.Broadcast(ImpulseId, NormalizedMagnitude, WorldLocation);
}

void AScaleAssemblyActor::HandleChunkLandedOnScale(AActor* PayloadActor, float /*ChunkMassKg*/, float /*UpdatedPanMassKg*/)
{
	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->NotifyPayloadLandedOnScale(PayloadActor);
		}
	}
}

void AScaleAssemblyActor::HandleChunkBouncedOffScale(AActor* PayloadActor, float /*ChunkMassKg*/, float /*UpdatedPanMassKg*/)
{
	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->NotifyPayloadBouncedOffScale(PayloadActor);
		}
	}
}

void AScaleAssemblyActor::HandleScaleContaminationDetected(AActor* ContaminatedActor)
{
	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->NotifyScaleContaminationDetected(ContaminatedActor);
		}
	}
}

void AScaleAssemblyActor::HandleScaleSettledChanged(bool bIsSettled, float SettledPanMassKg)
{
	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->NotifyScaleSettledStateChanged(bIsSettled, SettledPanMassKg);
		}
	}
}

void AScaleAssemblyActor::HandleChunkRoutedToTare(AActor* PayloadActor, float MassKg, bool /*bWasContaminated*/)
{
	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->NotifyPayloadDisposedToTare(PayloadActor, MassKg);
		}
	}
}
