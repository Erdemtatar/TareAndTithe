// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/DirectorBoothActor.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Data/MonolithContractDataAsset.h"
#include "Data/ShiftProgressionDataAsset.h"
#include "Data/TareTithePresentationDataAsset.h"
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

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitTextMatFinder(
		TEXT("/Engine/EngineMaterials/UnlitText.UnlitText"));
	if (UnlitTextMatFinder.Succeeded())
	{
		UnlitSignboardTextMaterial = UnlitTextMatFinder.Object;
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
	if (IsValid(BaseGreyboxShapeMaterial))
	{
		BoothFrameVisual->SetMaterial(0, BaseGreyboxShapeMaterial);
	}
	BoothFrameVisual->SetRelativeLocation(FVector::ZeroVector);
	BoothFrameVisual->SetRelativeScale3D(FVector(0.24, 1.90, 0.85));

	BlastShutterMotionPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BlastShutterMotionPivot"));
	BlastShutterMotionPivot->SetupAttachment(BoothRoot);
	BlastShutterMotionPivot->PrimaryComponentTick.bCanEverTick = false;
	BlastShutterMotionPivot->SetRelativeLocation(ClosedShutterRelativeLocation);
	BlastShutterMotionPivot->SetRelativeScale3D(FVector::OneVector);

	BlastShutterVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlastShutterVisual"));
	BlastShutterVisual->SetupAttachment(BlastShutterMotionPivot);
	BlastShutterVisual->PrimaryComponentTick.bCanEverTick = false;
	BlastShutterVisual->SetSimulatePhysics(false);
	BlastShutterVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BlastShutterVisual->SetGenerateOverlapEvents(false);
	if (IsValid(CubeMesh))
	{
		BlastShutterVisual->SetStaticMesh(CubeMesh);
	}
	if (IsValid(BaseGreyboxShapeMaterial))
	{
		BlastShutterVisual->SetMaterial(0, BaseGreyboxShapeMaterial);
	}
	BlastShutterVisual->SetRelativeLocation(FVector::ZeroVector);
	BlastShutterVisual->SetRelativeScale3D(FVector(0.06, 1.65, 0.62));

	auto ConfigureSignboardPlate = [this, CubeMesh](UStaticMeshComponent* PlateComp, const FVector& Scale3D)
	{
		PlateComp->PrimaryComponentTick.bCanEverTick = false;
		PlateComp->SetSimulatePhysics(false);
		PlateComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PlateComp->SetGenerateOverlapEvents(false);
		PlateComp->SetCastShadow(false);
		PlateComp->bReceivesDecals = false;
		if (IsValid(CubeMesh))
		{
			PlateComp->SetStaticMesh(CubeMesh);
		}
		if (IsValid(BaseGreyboxShapeMaterial))
		{
			PlateComp->SetMaterial(0, BaseGreyboxShapeMaterial);
		}
		PlateComp->SetRelativeLocation(FVector::ZeroVector);
		PlateComp->SetRelativeScale3D(Scale3D);
	};

	// Unscaled (1,1,1) root grouping left + right wing boards so BoothSignLocalOffset moves signage without squashing scale.
	BoothSignRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BoothSignRoot"));
	BoothSignRoot->SetupAttachment(BoothRoot);
	BoothSignRoot->PrimaryComponentTick.bCanEverTick = false;
	BoothSignRoot->SetRelativeTransform(FTransform::Identity);

	auto ConfigureWingText = [this](
		UTextRenderComponent* TextComp,
		USceneComponent* ParentPivot,
		const FVector& RelLocation,
		float WorldSize,
		const FColor& Color,
		const TCHAR* DefaultText)
	{
		TextComp->SetupAttachment(ParentPivot);
		TextComp->PrimaryComponentTick.bCanEverTick = false;
		TextComp->SetRelativeLocation(RelLocation);
		TextComp->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
		TextComp->SetRelativeScale3D(FVector::OneVector);
		TextComp->SetHorizontalAlignment(EHTA_Center);
		TextComp->SetVerticalAlignment(EVRTA_TextCenter);
		TextComp->SetWorldSize(WorldSize);
		TextComp->SetCastShadow(false);
		if (IsValid(UnlitSignboardTextMaterial))
		{
			TextComp->SetTextMaterial(UnlitSignboardTextMaterial);
		}
		TextComp->SetTextRenderColor(Color);
		TextComp->SetText(FText::FromString(DefaultText));
	};

	const double LeftOutwardY = -static_cast<double>(WingSignboardOutwardOffsetCm);
	const double RightOutwardY = +static_cast<double>(WingSignboardOutwardOffsetCm);

	// Left Wing (Y = -66 cm pivot, plate & text shifted outward to clear central clamp & hanging monolith 100%).
	BoothHeaderPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BoothHeaderPivot"));
	BoothHeaderPivot->SetupAttachment(BoothSignRoot);
	BoothHeaderPivot->PrimaryComponentTick.bCanEverTick = false;
	BoothHeaderPivot->SetRelativeLocation(LeftWingPivotRelativeLocation);
	BoothHeaderPivot->SetRelativeRotation(FRotator(10.0f, 0.0f, 0.0f));
	BoothHeaderPivot->SetRelativeScale3D(FVector::OneVector);

	BoothHeaderBackingPlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoothHeaderBackingPlate"));
	BoothHeaderBackingPlate->SetupAttachment(BoothHeaderPivot);
	ConfigureSignboardPlate(BoothHeaderBackingPlate, WingSignboardPlateScale3D);
	BoothHeaderBackingPlate->SetRelativeLocation(FVector(0.0, LeftOutwardY, 0.0));

	BoothStencilText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("BoothStencilText"));
	ConfigureWingText(
		BoothStencilText,
		BoothHeaderPivot,
		FVector(-2.0, LeftOutwardY, 5.2),
		WingHeaderWorldSize,
		BoothHeaderCyanColor,
		TEXT("CONTRACT 01/06 // DIRECTIVE"));

	ContractDirectiveReadoutText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ContractDirectiveReadoutText"));
	ConfigureWingText(
		ContractDirectiveReadoutText,
		BoothHeaderPivot,
		FVector(-2.0, LeftOutwardY, -4.4),
		WingDirectiveWorldSize,
		BoothDirectiveGoldColor,
		TEXT("CUT 12.0 KG TO TITHE | STAMP [E]"));

	// Right Wing (Y = +66 cm pivot, plate & text shifted outward symmetrically to clear central clamp & hanging monolith 100%).
	BoothVerdictPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BoothVerdictPivot"));
	BoothVerdictPivot->SetupAttachment(BoothSignRoot);
	BoothVerdictPivot->PrimaryComponentTick.bCanEverTick = false;
	BoothVerdictPivot->SetRelativeLocation(RightWingPivotRelativeLocation);
	BoothVerdictPivot->SetRelativeRotation(FRotator(10.0f, 0.0f, 0.0f));
	BoothVerdictPivot->SetRelativeScale3D(FVector::OneVector);

	BoothVerdictBackingPlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoothVerdictBackingPlate"));
	BoothVerdictBackingPlate->SetupAttachment(BoothVerdictPivot);
	ConfigureSignboardPlate(BoothVerdictBackingPlate, WingSignboardPlateScale3D);
	BoothVerdictBackingPlate->SetRelativeLocation(FVector(0.0, RightOutwardY, 0.0));

	BoothRightHeaderText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("BoothRightHeaderText"));
	ConfigureWingText(
		BoothRightHeaderText,
		BoothVerdictPivot,
		FVector(-2.0, RightOutwardY, 5.2),
		WingHeaderWorldSize,
		BoothHeaderCyanColor,
		TEXT("OFFICE OF THE DIRECTOR"));

	VerdictBannerText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("VerdictBannerText"));
	ConfigureWingText(
		VerdictBannerText,
		BoothVerdictPivot,
		FVector(-2.0, RightOutwardY, -4.4),
		WingVerdictWorldSize,
		BoothNormalSkyBlueColor,
		TEXT("CELL STANDBY"));

	StatusPointLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusPointLight"));
	StatusPointLight->SetupAttachment(BoothRoot);
	StatusPointLight->PrimaryComponentTick.bCanEverTick = false;
	StatusPointLight->SetRelativeLocation(FVector(-38.0, 0.0, 18.0));
	StatusPointLight->SetIntensity(StatusLightIntensity);
	StatusPointLight->SetAttenuationRadius(480.0f);
	StatusPointLight->SetCastShadows(false);
	StatusPointLight->SetLightColor(ActiveInspectionLampColor);
}

void ADirectorBoothActor::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(StatusPointLight))
	{
		StatusPointLight->SetIntensity(StatusLightIntensity);
	}

	if (IsValid(UnlitSignboardTextMaterial))
	{
		UTextRenderComponent* const WingTexts[4] = {
			BoothStencilText,
			ContractDirectiveReadoutText,
			BoothRightHeaderText,
			VerdictBannerText
		};
		for (UTextRenderComponent* TextComp : WingTexts)
		{
			if (IsValid(TextComp))
			{
				TextComp->SetTextMaterial(UnlitSignboardTextMaterial);
				TextComp->SetCastShadow(false);
			}
		}
	}

	if (IsValid(BaseGreyboxShapeMaterial))
	{
		auto ApplyBoothMaterial = [this](
			UStaticMeshComponent* TargetMesh,
			const FLinearColor& Color,
			float Roughness,
			float Metallic)
		{
			if (IsValid(TargetMesh))
			{
				if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseGreyboxShapeMaterial, this))
				{
					MID->SetVectorParameterValue(TEXT("BaseColorConst"), Color);
					MID->SetVectorParameterValue(TEXT("Color"), Color);
					MID->SetVectorParameterValue(TEXT("EmissiveColorConst"), FLinearColor::Black);
					MID->SetScalarParameterValue(TEXT("EmissiveScale"), 0.0f);
					MID->SetScalarParameterValue(TEXT("RoughnessConst"), Roughness);
					MID->SetScalarParameterValue(TEXT("MetallicConst"), Metallic);
					TargetMesh->SetMaterial(0, MID);
				}
			}
		};

		ApplyBoothMaterial(BoothFrameVisual, FLinearColor(0.028f, 0.031f, 0.038f, 1.0f), 0.78f, 0.35f);
		ApplyBoothMaterial(BlastShutterVisual, FLinearColor(0.060f, 0.062f, 0.068f, 1.0f), 0.66f, 0.45f);
		ApplyBoothMaterial(BoothHeaderBackingPlate, SignboardBackingColor, 0.96f, 0.02f);
		ApplyBoothMaterial(BoothVerdictBackingPlate, SignboardBackingColor, 0.96f, 0.02f);
	}

	SetShutterTargetOpenAlpha(0.0f, true);

	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->RegisterDirectorBoothStation(this);
			ShiftSubsystem->OnPresentationAssetChanged.AddUObject(this, &ADirectorBoothActor::ApplyPresentationData);
			if (const UTareTithePresentationDataAsset* ExistingPresentation = ShiftSubsystem->GetActivePresentationAsset())
			{
				ApplyPresentationData(ExistingPresentation);
			}
			ShiftSubsystem->OnNativeContractSnapshotUpdated.AddUObject(this, &ADirectorBoothActor::HandleContractSnapshotUpdated);
			ShiftSubsystem->OnShiftVerdict.AddUniqueDynamic(this, &ADirectorBoothActor::HandleShiftVerdict);
			ShiftSubsystem->OnShiftPhaseChanged.AddUObject(this, &ADirectorBoothActor::HandleShiftPhaseChanged);
			ShiftSubsystem->OnActiveContractLoaded.AddUObject(this, &ADirectorBoothActor::HandleActiveContractLoaded);

			CachedContractIndex = ShiftSubsystem->GetCurrentContractIndex();
			CachedTotalContracts = FMath::Max(1, ShiftSubsystem->GetTotalContractCount());
			const FContractRuntimeSnapshot InitialSnapshot = ShiftSubsystem->GetCurrentSnapshot();
			CachedDirectiveText = InitialSnapshot.ContractDirectiveText;
			RefreshContractDirectiveWingReadout();

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
			ShiftSubsystem->OnPresentationAssetChanged.RemoveAll(this);
			ShiftSubsystem->OnNativeContractSnapshotUpdated.RemoveAll(this);
			ShiftSubsystem->OnShiftVerdict.RemoveDynamic(this, &ADirectorBoothActor::HandleShiftVerdict);
			ShiftSubsystem->OnShiftPhaseChanged.RemoveAll(this);
			ShiftSubsystem->OnActiveContractLoaded.RemoveAll(this);
			ShiftSubsystem->UnregisterDirectorBoothStation(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ADirectorBoothActor::ApplyPresentationData(const UTareTithePresentationDataAsset* InPresentationData)
{
	if (!IsValid(InPresentationData))
	{
		if (IsValid(BoothSignRoot))
		{
			BoothSignRoot->SetRelativeTransform(FTransform::Identity);
		}
		return;
	}

	InPresentationData->DirectorBoothFrameMeshSlot.ApplyToVisualComponent(BoothFrameVisual);
	InPresentationData->DirectorShutterMeshSlot.ApplyToVisualComponent(BlastShutterVisual);

	if (IsValid(BoothSignRoot))
	{
		FTransform UnscaledSignTransform = InPresentationData->BoothSignLocalOffset;
		UnscaledSignTransform.SetScale3D(FVector::OneVector);
		BoothSignRoot->SetRelativeTransform(UnscaledSignTransform);
	}
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
			VerdictBannerText->SetTextRenderColor(BoothFaultBannerColor);
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
			VerdictBannerText->SetTextRenderColor(BoothApprovedEmeraldColor);
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
			VerdictBannerText->SetTextRenderColor(BoothApprovedEmeraldColor);
			VerdictBannerText->SetText(FText::FromString(TEXT("SHIFT COMPLETE // HOLD [R] TO REPLAY")));
		}
		break;
	}
	default:
		break;
	}
}

void ADirectorBoothActor::HandleContractSnapshotUpdated(const FContractRuntimeSnapshot& Snapshot)
{
	CachedContractIndex = Snapshot.ContractIndex;
	if (!Snapshot.ContractDirectiveText.IsEmpty())
	{
		CachedDirectiveText = Snapshot.ContractDirectiveText;
	}

	if (UWorld* World = GetWorld())
	{
		if (const UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			CachedTotalContracts = FMath::Max(1, ShiftSubsystem->GetTotalContractCount());
		}
	}

	RefreshContractDirectiveWingReadout();
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
			VerdictBannerText->SetTextRenderColor(BoothNormalSkyBlueColor);
			VerdictBannerText->SetText(FText::FromString(TEXT("INSPECTION IN PROGRESS")));
		}
	}
}

void ADirectorBoothActor::HandleActiveContractLoaded(
	const UMonolithContractDataAsset* Contract,
	const UShiftProgressionDataAsset* ShiftConfig,
	int32 ContractIndex)
{
	CachedContractIndex = ContractIndex;
	if (IsValid(ShiftConfig))
	{
		CachedTotalContracts = FMath::Max(1, ShiftConfig->OrderedContracts.Num());
	}
	if (IsValid(Contract) && !Contract->ContractDirectiveText.IsEmpty())
	{
		CachedDirectiveText = Contract->ContractDirectiveText;
	}

	RefreshContractDirectiveWingReadout();
}

void ADirectorBoothActor::RefreshContractDirectiveWingReadout()
{
	const int32 DisplayContractNum = FMath::Clamp(CachedContractIndex + 1, 1, FMath::Max(1, CachedTotalContracts));
	const int32 DisplayTotalCount = FMath::Max(1, CachedTotalContracts);

	if (IsValid(BoothStencilText))
	{
		BoothStencilText->SetTextRenderColor(BoothHeaderCyanColor);
		BoothStencilText->SetText(
			FText::FromString(FString::Printf(TEXT("CONTRACT %02d/%02d // DIRECTIVE"), DisplayContractNum, DisplayTotalCount)));
	}

	if (IsValid(ContractDirectiveReadoutText))
	{
		const FString EffectiveDirective = CachedDirectiveText.IsEmpty()
			? TEXT("CUT TO QUOTA | STAMP [E]")
			: CachedDirectiveText;
		ContractDirectiveReadoutText->SetTextRenderColor(BoothDirectiveGoldColor);
		ContractDirectiveReadoutText->SetText(FText::FromString(EffectiveDirective));
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
	if (IsValid(BlastShutterMotionPivot))
	{
		const FVector NewRelativePos = FMath::Lerp(ClosedShutterRelativeLocation, OpenShutterRelativeLocation, CurrentShutterOpenAlpha);
		BlastShutterMotionPivot->SetRelativeLocation(NewRelativePos);
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
		return TEXT("FAULT: CONTAMINATED | TARE [SPACE]");
	case EContractFaultReason::QuotaOvershoot:
		return TEXT("FAULT: OVERWEIGHT | MAX QUOTA");
	case EContractFaultReason::TareAllowanceExceeded:
		return TEXT("FAULT: MAX TARE WASTE EXCEEDED");
	case EContractFaultReason::MathematicallyUnreachable:
		return TEXT("FAULT: INSUFFICIENT MASS LEFT");
	case EContractFaultReason::ManualOperatorReset:
		return TEXT("MANUAL RESET [R]");
	default:
		return TEXT("CELL RESETTING");
	}
}
