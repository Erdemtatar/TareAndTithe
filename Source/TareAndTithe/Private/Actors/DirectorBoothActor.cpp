// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/DirectorBoothActor.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float ShutterAlphaConvergenceEpsilon = 0.002f;
}

ADirectorBoothActor::ADirectorBoothActor()
{
	// Tier 2: Strictly disabled while idle; active ONLY during 0.35s kinematic blast shutter motion.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	BoothRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BoothRoot"));
	SetRootComponent(BoothRoot);
	BoothRoot->PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* CubeMesh = CubeMeshFinder.Succeeded() ? CubeMeshFinder.Object : nullptr;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlattenMatFinder(
		TEXT("/Engine/EngineMaterials/FlattenMaterial.FlattenMaterial"));
	if (FlattenMatFinder.Succeeded())
	{
		BaseGreyboxShapeMaterial = FlattenMatFinder.Object;
	}

	BoothFrameVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoothFrameVisual"));
	BoothFrameVisual->SetupAttachment(BoothRoot);
	BoothFrameVisual->PrimaryComponentTick.bCanEverTick = false;
	BoothFrameVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoothFrameVisual->SetGenerateOverlapEvents(false);
	if (IsValid(CubeMesh))
	{
		BoothFrameVisual->SetStaticMesh(CubeMesh);
	}
	BoothFrameVisual->SetRelativeLocation(FVector::ZeroVector);
	BoothFrameVisual->SetRelativeScale3D(FVector(0.24, 1.90, 0.85));

	BlastShutterVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlastShutterVisual"));
	BlastShutterVisual->SetupAttachment(BoothRoot);
	BlastShutterVisual->PrimaryComponentTick.bCanEverTick = false;
	BlastShutterVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BlastShutterVisual->SetGenerateOverlapEvents(false);
	if (IsValid(CubeMesh))
	{
		BlastShutterVisual->SetStaticMesh(CubeMesh);
	}
	BlastShutterVisual->SetRelativeLocation(ClosedShutterRelativeLocation);
	BlastShutterVisual->SetRelativeScale3D(FVector(0.06, 1.65, 0.62));

	BoothStencilText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("BoothStencilText"));
	BoothStencilText->SetupAttachment(BoothRoot);
	BoothStencilText->PrimaryComponentTick.bCanEverTick = false;
	BoothStencilText->SetRelativeLocation(FVector(-16.0, 0.0, 48.0));
	BoothStencilText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	BoothStencilText->SetHorizontalAlignment(EHTA_Center);
	BoothStencilText->SetVerticalAlignment(EVRTA_TextCenter);
	BoothStencilText->SetWorldSize(9.0f);
	BoothStencilText->SetTextRenderColor(FColor(232, 236, 240, 255));
	BoothStencilText->SetText(FText::FromString(TEXT("OFFICE OF THE DIRECTOR")));

	VerdictBannerText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("VerdictBannerText"));
	VerdictBannerText->SetupAttachment(BoothRoot);
	VerdictBannerText->PrimaryComponentTick.bCanEverTick = false;
	VerdictBannerText->SetRelativeLocation(FVector(-16.0, 0.0, -48.0));
	VerdictBannerText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	VerdictBannerText->SetHorizontalAlignment(EHTA_Center);
	VerdictBannerText->SetVerticalAlignment(EVRTA_TextCenter);
	VerdictBannerText->SetWorldSize(6.5f);
	VerdictBannerText->SetTextRenderColor(FColor(255, 188, 54, 255));
	VerdictBannerText->SetText(FText::FromString(TEXT("CELL STANDBY")));

	StatusPointLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusPointLight"));
	StatusPointLight->SetupAttachment(BoothRoot);
	StatusPointLight->PrimaryComponentTick.bCanEverTick = false;
	StatusPointLight->SetRelativeLocation(FVector(-32.0, 0.0, 24.0));
	StatusPointLight->SetIntensity(3200.0f);
	StatusPointLight->SetAttenuationRadius(680.0f);
	StatusPointLight->SetCastShadows(false);
	StatusPointLight->SetLightColor(ActiveInspectionLampColor);
}

void ADirectorBoothActor::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(BaseGreyboxShapeMaterial))
	{
		if (IsValid(BoothFrameVisual))
		{
			if (UMaterialInstanceDynamic* FrameMID = UMaterialInstanceDynamic::Create(BaseGreyboxShapeMaterial, this))
			{
				const FLinearColor FrameColor(0.04f, 0.045f, 0.055f, 1.0f);
				FrameMID->SetVectorParameterValue(TEXT("BaseColorConst"), FrameColor);
				FrameMID->SetVectorParameterValue(TEXT("Color"), FrameColor);
				FrameMID->SetVectorParameterValue(TEXT("EmissiveColorConst"), FLinearColor::Black);
				FrameMID->SetScalarParameterValue(TEXT("EmissiveScale"), 0.0f);
				FrameMID->SetScalarParameterValue(TEXT("RoughnessConst"), 0.72f);
				FrameMID->SetScalarParameterValue(TEXT("MetallicConst"), 0.45f);
				BoothFrameVisual->SetMaterial(0, FrameMID);
			}
		}
		if (IsValid(BlastShutterVisual))
		{
			if (UMaterialInstanceDynamic* ShutterMID = UMaterialInstanceDynamic::Create(BaseGreyboxShapeMaterial, this))
			{
				const FLinearColor ShutterColor(0.14f, 0.13f, 0.11f, 1.0f);
				ShutterMID->SetVectorParameterValue(TEXT("BaseColorConst"), ShutterColor);
				ShutterMID->SetVectorParameterValue(TEXT("Color"), ShutterColor);
				ShutterMID->SetVectorParameterValue(TEXT("EmissiveColorConst"), FLinearColor::Black);
				ShutterMID->SetScalarParameterValue(TEXT("EmissiveScale"), 0.0f);
				ShutterMID->SetScalarParameterValue(TEXT("RoughnessConst"), 0.58f);
				ShutterMID->SetScalarParameterValue(TEXT("MetallicConst"), 0.55f);
				BlastShutterVisual->SetMaterial(0, ShutterMID);
			}
		}
	}

	SetShutterTargetOpenAlpha(0.0f, true);

	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->RegisterDirectorBoothStation(this);
			ShiftSubsystem->OnShiftVerdict.AddUniqueDynamic(this, &ADirectorBoothActor::HandleShiftVerdict);
			ShiftSubsystem->OnShiftPhaseChanged.AddUObject(this, &ADirectorBoothActor::HandleShiftPhaseChanged);
			HandleShiftPhaseChanged(ShiftSubsystem->GetCurrentPhase());
		}
	}
}

void ADirectorBoothActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->OnShiftVerdict.RemoveDynamic(this, &ADirectorBoothActor::HandleShiftVerdict);
			ShiftSubsystem->OnShiftPhaseChanged.RemoveAll(this);
			ShiftSubsystem->UnregisterDirectorBoothStation(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ADirectorBoothActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (DeltaSeconds <= 0.0f)
	{
		return;
	}

	const float SafeDuration = FMath::Max(0.05f, ShutterTransitionDurationSeconds);
	const float AlphaSpeedPerSec = 1.0f / SafeDuration;

	CurrentShutterOpenAlpha = FMath::FInterpConstantTo(
		CurrentShutterOpenAlpha,
		TargetShutterOpenAlpha,
		DeltaSeconds,
		AlphaSpeedPerSec);

	const bool bConverged = FMath::IsNearlyEqual(CurrentShutterOpenAlpha, TargetShutterOpenAlpha, ShutterAlphaConvergenceEpsilon);
	if (bConverged)
	{
		CurrentShutterOpenAlpha = TargetShutterOpenAlpha;
	}

	ApplyShutterPositionFromAlpha();

	if (bConverged)
	{
		SetActorTickEnabled(false);
	}
}

void ADirectorBoothActor::HandleShiftVerdict(EShiftVerdictType Verdict, EContractFaultReason FaultReason)
{
	switch (Verdict)
	{
	case EShiftVerdictType::ContractFaultReset:
	{
		SetShutterTargetOpenAlpha(0.0f, false);
		if (IsValid(StatusPointLight))
		{
			StatusPointLight->SetLightColor(FaultSlamLampColor);
		}
		if (IsValid(VerdictBannerText))
		{
			VerdictBannerText->SetTextRenderColor(FColor(255, 62, 54, 255));
			VerdictBannerText->SetText(FText::FromString(DescribeFaultReason(FaultReason)));
		}
		break;
	}
	case EShiftVerdictType::ContractPassed:
	{
		SetShutterTargetOpenAlpha(1.0f, false);
		if (IsValid(StatusPointLight))
		{
			StatusPointLight->SetLightColor(VerdictApprovedLampColor);
		}
		if (IsValid(VerdictBannerText))
		{
			VerdictBannerText->SetTextRenderColor(FColor(64, 242, 118, 255));
			VerdictBannerText->SetText(FText::FromString(TEXT("TITHE VERIFIED // ADVANCING")));
		}
		break;
	}
	case EShiftVerdictType::ShiftCompleted:
	{
		SetShutterTargetOpenAlpha(0.0f, false);
		if (IsValid(StatusPointLight))
		{
			StatusPointLight->SetLightColor(VerdictApprovedLampColor);
		}
		if (IsValid(VerdictBannerText))
		{
			VerdictBannerText->SetTextRenderColor(FColor(64, 242, 118, 255));
			VerdictBannerText->SetText(FText::FromString(TEXT("SHIFT QUOTA COMPLETE // HOLD [R] TO REPLAY")));
		}
		break;
	}
	default:
		break;
	}
}

void ADirectorBoothActor::HandleShiftPhaseChanged(EShiftPhase NewPhase)
{
	if (NewPhase == EShiftPhase::ActiveInspection)
	{
		SetShutterTargetOpenAlpha(1.0f, false);
		if (IsValid(StatusPointLight))
		{
			StatusPointLight->SetLightColor(ActiveInspectionLampColor);
		}
		if (IsValid(VerdictBannerText))
		{
			VerdictBannerText->SetTextRenderColor(FColor(255, 188, 54, 255));
			VerdictBannerText->SetText(FText::FromString(TEXT("INSPECTION IN PROGRESS")));
		}
	}
}

void ADirectorBoothActor::SetShutterTargetOpenAlpha(float InTargetOpenAlpha, bool bInstant)
{
	TargetShutterOpenAlpha = FMath::Clamp(InTargetOpenAlpha, 0.0f, 1.0f);

	if (bInstant)
	{
		CurrentShutterOpenAlpha = TargetShutterOpenAlpha;
		ApplyShutterPositionFromAlpha();
		SetActorTickEnabled(false);
		return;
	}

	if (!FMath::IsNearlyEqual(CurrentShutterOpenAlpha, TargetShutterOpenAlpha, ShutterAlphaConvergenceEpsilon))
	{
		SetActorTickEnabled(true);
	}
}

void ADirectorBoothActor::ApplyShutterPositionFromAlpha()
{
	if (IsValid(BlastShutterVisual))
	{
		const FVector NewRelativePos = FMath::Lerp(ClosedShutterRelativeLocation, OpenShutterRelativeLocation, CurrentShutterOpenAlpha);
		BlastShutterVisual->SetRelativeLocation(NewRelativePos);
	}
}

FString ADirectorBoothActor::DescribeFaultReason(EContractFaultReason FaultReason)
{
	switch (FaultReason)
	{
	case EContractFaultReason::VolatileDetonation:
		return TEXT("FAULT: VOLATILE VEIN SEVERED");
	case EContractFaultReason::ClampStructuralSnap:
		return TEXT("FAULT: CLAMP TORQUE SNAP");
	case EContractFaultReason::ScaleContaminated:
		return TEXT("FAULT: CONTAMINATED CHUNK - ROUTE TO [SPACE] TARE");
	case EContractFaultReason::QuotaOvershoot:
		return TEXT("FAULT: OVERWEIGHT - EXCEEDED MAX QUOTA");
	case EContractFaultReason::TareAllowanceExceeded:
		return TEXT("FAULT: MAX TARE WASTE EXCEEDED");
	case EContractFaultReason::MathematicallyUnreachable:
		return TEXT("FAULT: INSUFFICIENT MASS REMAINING");
	case EContractFaultReason::ManualOperatorReset:
		return TEXT("MANUAL RESET [R]");
	default:
		return TEXT("CELL RESETTING");
	}
}
