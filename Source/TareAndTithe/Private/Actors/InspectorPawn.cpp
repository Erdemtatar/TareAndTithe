// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/InspectorPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/AudioTelemetryBridgeComponent.h"
#include "Components/LaserCapacitorComponent.h"
#include "Components/LaserPlaneAimComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/MonolithContractDataAsset.h"
#include "Data/ShiftProgressionDataAsset.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Interfaces/SliceableTargetInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AInspectorPawn::AInspectorPawn()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	AutoPossessPlayer = EAutoReceiveInput::Player0;

	PawnRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PawnRoot"));
	SetRootComponent(PawnRoot);
	PawnRoot->PrimaryComponentTick.bCanEverTick = false;

	InspectorCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("InspectorCamera"));
	InspectorCamera->SetupAttachment(PawnRoot);
	InspectorCamera->PrimaryComponentTick.bCanEverTick = false;
	InspectorCamera->SetRelativeLocation(FVector(0.0, 0.0, 0.0));
	InspectorCamera->FieldOfView = 78.0f;
	InspectorCamera->bUsePawnControlRotation = false;

	LaserPlaneVisualPivot = CreateDefaultSubobject<USceneComponent>(TEXT("LaserPlaneVisualPivot"));
	LaserPlaneVisualPivot->SetupAttachment(PawnRoot);
	LaserPlaneVisualPivot->PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	UStaticMesh* PlaneMesh = PlaneMeshFinder.Succeeded() ? PlaneMeshFinder.Object : nullptr;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMatFinder(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMatFinder.Succeeded())
	{
		BaseGreyboxPlaneMaterial = BasicMatFinder.Object;
	}

	auto ConfigureLaserSheetMesh = [PlaneMesh](UStaticMeshComponent* MeshComp)
	{
		MeshComp->PrimaryComponentTick.bCanEverTick = false;
		MeshComp->SetSimulatePhysics(false);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComp->SetGenerateOverlapEvents(false);
		MeshComp->SetCastShadow(false);
		MeshComp->bReceivesDecals = false;
		if (IsValid(PlaneMesh))
		{
			MeshComp->SetStaticMesh(PlaneMesh);
		}
	};

	LaserSheetVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaserSheetVisual"));
	LaserSheetVisual->SetupAttachment(LaserPlaneVisualPivot);
	ConfigureLaserSheetMesh(LaserSheetVisual);
	LaserSheetVisual->SetRelativeScale3D(FVector(0.66, 0.72, 1.0));

	LaserSheetUndersideVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaserSheetUndersideVisual"));
	LaserSheetUndersideVisual->SetupAttachment(LaserPlaneVisualPivot);
	ConfigureLaserSheetMesh(LaserSheetUndersideVisual);
	LaserSheetUndersideVisual->SetRelativeRotation(FRotator(180.0f, 0.0f, 0.0f));
	LaserSheetUndersideVisual->SetRelativeScale3D(FVector(0.66, 0.72, 1.0));

	LaserPlaneAimComponent = CreateDefaultSubobject<ULaserPlaneAimComponent>(TEXT("LaserPlaneAimComponent"));
	LaserCapacitorComponent = CreateDefaultSubobject<ULaserCapacitorComponent>(TEXT("LaserCapacitorComponent"));
	AudioTelemetryBridgeComponent = CreateDefaultSubobject<UAudioTelemetryBridgeComponent>(TEXT("AudioTelemetryBridgeComponent"));

	ContinuousMetaSoundAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("ContinuousMetaSoundAudioComponent"));
	ContinuousMetaSoundAudioComponent->SetupAttachment(PawnRoot);
	ContinuousMetaSoundAudioComponent->PrimaryComponentTick.bCanEverTick = false;
	ContinuousMetaSoundAudioComponent->bAutoActivate = false;

	ImpulseMetaSoundAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("ImpulseMetaSoundAudioComponent"));
	ImpulseMetaSoundAudioComponent->SetupAttachment(PawnRoot);
	ImpulseMetaSoundAudioComponent->PrimaryComponentTick.bCanEverTick = false;
	ImpulseMetaSoundAudioComponent->bAutoActivate = false;
}

void AInspectorPawn::BeginPlay()
{
	Super::BeginPlay();

	EnsureInputActionsAndMappingInitialized();
	ApplyMappingContextToLocalPlayer();
	InitializeLaserSheetDynamicMaterial();

	if (APlayerController* PC = GetController<APlayerController>())
	{
		FInputModeGameOnly GameOnlyInput;
		PC->SetInputMode(GameOnlyInput);
		PC->SetShowMouseCursor(false);
	}

	if (IsValid(ContinuousMetaSoundAudioComponent) && IsValid(ContinuousMetaSoundAsset))
	{
		ContinuousMetaSoundAudioComponent->SetSound(ContinuousMetaSoundAsset);
	}
	if (IsValid(ImpulseMetaSoundAudioComponent) && IsValid(ImpulseMetaSoundAsset))
	{
		ImpulseMetaSoundAudioComponent->SetSound(ImpulseMetaSoundAsset);
	}

	if (IsValid(AudioTelemetryBridgeComponent))
	{
		AudioTelemetryBridgeComponent->RegisterAudioComponent(ContinuousMetaSoundAudioComponent);
		AudioTelemetryBridgeComponent->RegisterAudioComponent(ImpulseMetaSoundAudioComponent);
	}

	if (IsValid(LaserPlaneAimComponent))
	{
		LaserPlaneAimComponent->SetLaserVisualComponent(LaserPlaneVisualPivot);
		LaserPlaneAimComponent->OnPreviewUpdated.AddUObject(this, &AInspectorPawn::HandlePreviewTelemetryUpdated);
	}

	if (IsValid(LaserCapacitorComponent))
	{
		LaserCapacitorComponent->OnQuerySliceTarget.BindUObject(this, &AInspectorPawn::HandleQueryLaserSliceTarget);
		LaserCapacitorComponent->OnTelemetryStream.AddUObject(this, &AInspectorPawn::HandleCapacitorTelemetryStream);
		LaserCapacitorComponent->OnTelemetryImpulse.AddUObject(this, &AInspectorPawn::HandleCapacitorTelemetryImpulse);
	}

	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->RegisterTelemetryPublisher(this, this);
			ShiftSubsystem->OnShiftPhaseChanged.AddUObject(this, &AInspectorPawn::HandleShiftPhaseChanged);
			ShiftSubsystem->OnActiveContractLoaded.AddUObject(this, &AInspectorPawn::HandleActiveContractLoaded);

			if (const UShiftProgressionDataAsset* ShiftConfig = ShiftSubsystem->GetActiveShiftProgression())
			{
				if (IsValid(LaserCapacitorComponent))
				{
					LaserCapacitorComponent->ConfigureFromShiftProgression(ShiftConfig);
				}
			}

			RefreshAimTargetAndFrameFromShift();
			HandleShiftPhaseChanged(ShiftSubsystem->GetCurrentPhase());
		}
	}

	UpdateLaserSheetMaterialVisuals();
}

void AInspectorPawn::PawnClientRestart()
{
	Super::PawnClientRestart();

	EnsureInputActionsAndMappingInitialized();
	ApplyMappingContextToLocalPlayer();

	if (APlayerController* PC = GetController<APlayerController>())
	{
		FInputModeGameOnly GameOnlyInput;
		PC->SetInputMode(GameOnlyInput);
		PC->SetShowMouseCursor(false);
	}
}

void AInspectorPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	EnsureInputActionsAndMappingInitialized();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!IsValid(EnhancedInput))
	{
		return;
	}

	if (IsValid(ActiveAimCursorAction))
	{
		EnhancedInput->BindAction(ActiveAimCursorAction, ETriggerEvent::Triggered, this, &AInspectorPawn::Input_AimCursor);
	}
	if (IsValid(ActiveScrollRollAction))
	{
		EnhancedInput->BindAction(ActiveScrollRollAction, ETriggerEvent::Triggered, this, &AInspectorPawn::Input_ScrollRoll);
	}
	if (IsValid(ActiveFastSnapModifierAction))
	{
		EnhancedInput->BindAction(ActiveFastSnapModifierAction, ETriggerEvent::Started, this, &AInspectorPawn::Input_FastSnapStarted);
		EnhancedInput->BindAction(ActiveFastSnapModifierAction, ETriggerEvent::Completed, this, &AInspectorPawn::Input_FastSnapEnded);
		EnhancedInput->BindAction(ActiveFastSnapModifierAction, ETriggerEvent::Canceled, this, &AInspectorPawn::Input_FastSnapEnded);
	}
	if (IsValid(ActiveFireCapacitorAction))
	{
		EnhancedInput->BindAction(ActiveFireCapacitorAction, ETriggerEvent::Started, this, &AInspectorPawn::Input_FireStarted);
		EnhancedInput->BindAction(ActiveFireCapacitorAction, ETriggerEvent::Completed, this, &AInspectorPawn::Input_FireReleased);
		EnhancedInput->BindAction(ActiveFireCapacitorAction, ETriggerEvent::Canceled, this, &AInspectorPawn::Input_FireReleased);
	}
	if (IsValid(ActiveBevelOrCancelAction))
	{
		EnhancedInput->BindAction(ActiveBevelOrCancelAction, ETriggerEvent::Started, this, &AInspectorPawn::Input_BevelOrCancelStarted);
		EnhancedInput->BindAction(ActiveBevelOrCancelAction, ETriggerEvent::Completed, this, &AInspectorPawn::Input_BevelOrCancelReleased);
		EnhancedInput->BindAction(ActiveBevelOrCancelAction, ETriggerEvent::Canceled, this, &AInspectorPawn::Input_BevelOrCancelReleased);
	}
	if (IsValid(ActiveToggleDiverterAction))
	{
		EnhancedInput->BindAction(ActiveToggleDiverterAction, ETriggerEvent::Started, this, &AInspectorPawn::Input_ToggleDiverter);
	}
	if (IsValid(ActiveStampVerdictAction))
	{
		EnhancedInput->BindAction(ActiveStampVerdictAction, ETriggerEvent::Started, this, &AInspectorPawn::Input_StampVerdict);
	}
	if (IsValid(ActiveResetContractAction))
	{
		EnhancedInput->BindAction(ActiveResetContractAction, ETriggerEvent::Started, this, &AInspectorPawn::Input_ResetHoldStarted);
		EnhancedInput->BindAction(ActiveResetContractAction, ETriggerEvent::Completed, this, &AInspectorPawn::Input_ResetHoldReleased);
		EnhancedInput->BindAction(ActiveResetContractAction, ETriggerEvent::Canceled, this, &AInspectorPawn::Input_ResetHoldReleased);
	}
	if (IsValid(ActiveScalabilityLowAction))
	{
		EnhancedInput->BindAction(ActiveScalabilityLowAction, ETriggerEvent::Started, this, &AInspectorPawn::Input_ScalabilityLow);
	}
	if (IsValid(ActiveScalabilityMedAction))
	{
		EnhancedInput->BindAction(ActiveScalabilityMedAction, ETriggerEvent::Started, this, &AInspectorPawn::Input_ScalabilityMed);
	}
	if (IsValid(ActiveScalabilityHighAction))
	{
		EnhancedInput->BindAction(ActiveScalabilityHighAction, ETriggerEvent::Started, this, &AInspectorPawn::Input_ScalabilityHigh);
	}
	if (IsValid(ActiveScalabilityEpicAction))
	{
		EnhancedInput->BindAction(ActiveScalabilityEpicAction, ETriggerEvent::Started, this, &AInspectorPawn::Input_ScalabilityEpic);
	}
}

void AInspectorPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ManualResetHoldTimerHandle);

		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->OnShiftPhaseChanged.RemoveAll(this);
			ShiftSubsystem->OnActiveContractLoaded.RemoveAll(this);
			ShiftSubsystem->UnregisterTelemetryPublisher(this);
		}
	}

	if (IsValid(LaserPlaneAimComponent))
	{
		LaserPlaneAimComponent->OnPreviewUpdated.RemoveAll(this);
	}
	if (IsValid(LaserCapacitorComponent))
	{
		LaserCapacitorComponent->OnQuerySliceTarget.Unbind();
		LaserCapacitorComponent->OnTelemetryStream.RemoveAll(this);
		LaserCapacitorComponent->OnTelemetryImpulse.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AInspectorPawn::RefreshAimTargetAndFrameFromShift()
{
	if (!IsValid(LaserPlaneAimComponent))
	{
		return;
	}

	UWorld* World = GetWorld();
	UInspectionShiftSubsystem* ShiftSubsystem = IsValid(World) ? World->GetSubsystem<UInspectionShiftSubsystem>() : nullptr;
	UObject* TargetObj = IsValid(ShiftSubsystem) ? ShiftSubsystem->GetRegisteredSliceableTargetObject() : nullptr;

	LaserPlaneAimComponent->SetSliceableTarget(TargetObj);

	FVector AnchorWorld = GetActorLocation() + (GetActorForwardVector() * 220.0);
	if (IsValid(TargetObj) && TargetObj->Implements<USliceableTargetInterface>())
	{
		const FBox TargetBounds = ISliceableTargetInterface::Execute_GetTargetWorldBounds(TargetObj);
		if (TargetBounds.IsValid)
		{
			AnchorWorld = TargetBounds.GetCenter();
		}
	}

	LaserPlaneAimComponent->SetFrameBasis(AnchorWorld, GetActorForwardVector());
}

void AInspectorPawn::EnsureInputActionsAndMappingInitialized()
{
	auto ResolveOrCreateAction = [this](
		TObjectPtr<UInputAction>& ActiveSlot,
		UInputAction* AssignedAsset,
		const TCHAR* TransientName,
		EInputActionValueType ValueType)
	{
		if (IsValid(ActiveSlot))
		{
			return;
		}
		if (IsValid(AssignedAsset))
		{
			ActiveSlot = AssignedAsset;
			return;
		}
		UInputAction* CreatedAction = NewObject<UInputAction>(this, TransientName);
		CreatedAction->ValueType = ValueType;
		ActiveSlot = CreatedAction;
	};

	ResolveOrCreateAction(ActiveAimCursorAction, AssignedAimCursorAction, TEXT("IA_Transient_AimCursor"), EInputActionValueType::Axis2D);
	ResolveOrCreateAction(ActiveScrollRollAction, AssignedScrollRollAction, TEXT("IA_Transient_ScrollRoll"), EInputActionValueType::Axis1D);
	ResolveOrCreateAction(ActiveFastSnapModifierAction, AssignedFastSnapModifierAction, TEXT("IA_Transient_FastSnapMod"), EInputActionValueType::Boolean);
	ResolveOrCreateAction(ActiveFireCapacitorAction, AssignedFireCapacitorAction, TEXT("IA_Transient_FireCapacitor"), EInputActionValueType::Boolean);
	ResolveOrCreateAction(ActiveBevelOrCancelAction, AssignedBevelOrCancelAction, TEXT("IA_Transient_BevelOrCancel"), EInputActionValueType::Boolean);
	ResolveOrCreateAction(ActiveToggleDiverterAction, AssignedToggleDiverterAction, TEXT("IA_Transient_ToggleDiverter"), EInputActionValueType::Boolean);
	ResolveOrCreateAction(ActiveStampVerdictAction, AssignedStampVerdictAction, TEXT("IA_Transient_StampVerdict"), EInputActionValueType::Boolean);
	ResolveOrCreateAction(ActiveResetContractAction, AssignedResetContractAction, TEXT("IA_Transient_ResetContract"), EInputActionValueType::Boolean);
	ResolveOrCreateAction(ActiveScalabilityLowAction, AssignedScalabilityLowAction, TEXT("IA_Transient_ScalabilityLow"), EInputActionValueType::Boolean);
	ResolveOrCreateAction(ActiveScalabilityMedAction, AssignedScalabilityMedAction, TEXT("IA_Transient_ScalabilityMed"), EInputActionValueType::Boolean);
	ResolveOrCreateAction(ActiveScalabilityHighAction, AssignedScalabilityHighAction, TEXT("IA_Transient_ScalabilityHigh"), EInputActionValueType::Boolean);
	ResolveOrCreateAction(ActiveScalabilityEpicAction, AssignedScalabilityEpicAction, TEXT("IA_Transient_ScalabilityEpic"), EInputActionValueType::Boolean);

	if (!IsValid(ActiveMappingContext))
	{
		if (IsValid(AssignedMappingContext))
		{
			ActiveMappingContext = AssignedMappingContext;
		}
		else
		{
			UInputMappingContext* TransientIMC = NewObject<UInputMappingContext>(this, TEXT("IMC_Transient_InspectorControls"));
			TransientIMC->MapKey(ActiveAimCursorAction, EKeys::Mouse2D);
			TransientIMC->MapKey(ActiveScrollRollAction, EKeys::MouseWheelAxis);
			TransientIMC->MapKey(ActiveFastSnapModifierAction, EKeys::LeftShift);
			TransientIMC->MapKey(ActiveFastSnapModifierAction, EKeys::RightShift);
			TransientIMC->MapKey(ActiveFireCapacitorAction, EKeys::LeftMouseButton);
			TransientIMC->MapKey(ActiveBevelOrCancelAction, EKeys::RightMouseButton);
			TransientIMC->MapKey(ActiveToggleDiverterAction, EKeys::SpaceBar);
			TransientIMC->MapKey(ActiveStampVerdictAction, EKeys::E);
			TransientIMC->MapKey(ActiveResetContractAction, EKeys::R);
			TransientIMC->MapKey(ActiveScalabilityLowAction, EKeys::One);
			TransientIMC->MapKey(ActiveScalabilityMedAction, EKeys::Two);
			TransientIMC->MapKey(ActiveScalabilityHighAction, EKeys::Three);
			TransientIMC->MapKey(ActiveScalabilityEpicAction, EKeys::Four);
			ActiveMappingContext = TransientIMC;
		}
	}
}

void AInspectorPawn::ApplyMappingContextToLocalPlayer()
{
	if (!IsValid(ActiveMappingContext))
	{
		return;
	}

	const APlayerController* PC = GetController<APlayerController>();
	const ULocalPlayer* LocalPlayer = IsValid(PC) ? PC->GetLocalPlayer() : nullptr;
	if (!IsValid(LocalPlayer))
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		if (!InputSubsystem->HasMappingContext(ActiveMappingContext))
		{
			InputSubsystem->AddMappingContext(ActiveMappingContext, 0);
		}
	}
}

void AInspectorPawn::InitializeLaserSheetDynamicMaterial()
{
	UMaterialInterface* SourceMaterial = IsValid(LaserSheetOverrideMaterial)
		? LaserSheetOverrideMaterial.Get()
		: BaseGreyboxPlaneMaterial.Get();

	if (!IsValid(SourceMaterial))
	{
		return;
	}

	LaserSheetMID = UMaterialInstanceDynamic::Create(SourceMaterial, this);
	if (IsValid(LaserSheetMID))
	{
		if (IsValid(LaserSheetVisual))
		{
			LaserSheetVisual->SetMaterial(0, LaserSheetMID);
		}
		if (IsValid(LaserSheetUndersideVisual))
		{
			LaserSheetUndersideVisual->SetMaterial(0, LaserSheetMID);
		}
	}
}

void AInspectorPawn::UpdateLaserSheetMaterialVisuals()
{
	if (!IsValid(LaserSheetMID))
	{
		return;
	}

	const FLinearColor SafeIdleColor(0.04f, 0.52f, 0.78f, 1.0f);
	const FLinearColor IntersectCyanColor(0.08f, 0.88f, 1.00f, 1.0f);
	const FLinearColor WarningAmberColor(1.00f, 0.56f, 0.04f, 1.0f);
	const FLinearColor DangerCrimsonColor(1.00f, 0.05f, 0.03f, 1.0f);
	const FLinearColor CapacitorHotWhiteColor(1.00f, 0.94f, 0.78f, 1.0f);

	FLinearColor BaseHue = bCachedIntersectsMonolith ? IntersectCyanColor : SafeIdleColor;
	if (CachedVeinProximityNormalized > 0.0f)
	{
		if (CachedVeinProximityNormalized < 0.5f)
		{
			BaseHue = FMath::Lerp(IntersectCyanColor, WarningAmberColor, CachedVeinProximityNormalized * 2.0f);
		}
		else
		{
			BaseHue = FMath::Lerp(WarningAmberColor, DangerCrimsonColor, (CachedVeinProximityNormalized - 0.5f) * 2.0f);
		}
	}

	const float ChargeBlend = FMath::Clamp(CachedLaserChargeNormalized * 0.75f, 0.0f, 0.75f);
	const float EmissiveBoost = 1.0f + (CachedVeinProximityNormalized * 1.25f) + (CachedLaserChargeNormalized * 3.0f);
	const FLinearColor FinalColor = FMath::Lerp(BaseHue, CapacitorHotWhiteColor, ChargeBlend) * EmissiveBoost;

	LaserSheetMID->SetVectorParameterValue(TEXT("Color"), FinalColor);
	LaserSheetMID->SetVectorParameterValue(TEXT("BaseColor"), FinalColor);
	LaserSheetMID->SetVectorParameterValue(TEXT("EmissiveColor"), FinalColor);
	LaserSheetMID->SetScalarParameterValue(TEXT("Opacity"), bCachedIntersectsMonolith ? 0.42f : 0.18f);
	LaserSheetMID->SetScalarParameterValue(TEXT("VeinProximity"), CachedVeinProximityNormalized);
	LaserSheetMID->SetScalarParameterValue(TEXT("LaserCharge"), CachedLaserChargeNormalized);
}

void AInspectorPawn::UpdateCameraParallaxFromAim()
{
	if (!IsValid(InspectorCamera) || !IsValid(LaserPlaneAimComponent))
	{
		return;
	}

	const float NormH = FMath::Clamp(
		LaserPlaneAimComponent->GetAimHorizontalCm() / FMath::Max(10.0f, ParallaxReferenceHorizontalSpanCm),
		-1.0f,
		1.0f);
	const float NormV = FMath::Clamp(
		LaserPlaneAimComponent->GetAimVerticalCm() / FMath::Max(10.0f, ParallaxReferenceVerticalSpanCm),
		-1.0f,
		1.0f);

	const float TargetYawDeg = FMath::Clamp(NormH * MaxCameraParallaxYawDegrees, -MaxCameraParallaxYawDegrees, MaxCameraParallaxYawDegrees);
	const float TargetPitchDeg = FMath::Clamp(NormV * MaxCameraParallaxPitchDegrees, -MaxCameraParallaxPitchDegrees, MaxCameraParallaxPitchDegrees);

	// Subtle baseline downward look (-6 deg) plus clamped aim-tracking parallax.
	InspectorCamera->SetRelativeRotation(FRotator(TargetPitchDeg - 6.0f, TargetYawDeg, 0.0f));
}

void AInspectorPawn::ApplyNativeScalabilityPreset(int32 QualityLevel, float TsrResolutionScalePercent)
{
	if (UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
	{
		const int32 ClampedQuality = FMath::Clamp(QualityLevel, 0, 3);
		Settings->SetOverallScalabilityLevel(ClampedQuality);
		Settings->SetResolutionScaleValueEx(FMath::Clamp(TsrResolutionScalePercent, 50.0f, 100.0f));
		Settings->ApplySettings(false);
	}
}

bool AInspectorPawn::HandleQueryLaserSliceTarget(
	UObject*& OutSliceableTarget,
	FVector& OutPlaneOriginWorld,
	FVector& OutPlaneNormalWorld)
{
	if (!IsValid(LaserPlaneAimComponent) || !LaserPlaneAimComponent->IsAimingEnabled())
	{
		return false;
	}

	OutSliceableTarget = LaserPlaneAimComponent->GetSliceableTarget();
	OutPlaneOriginWorld = LaserPlaneAimComponent->GetCurrentPlaneOriginWorld();
	OutPlaneNormalWorld = LaserPlaneAimComponent->GetCurrentPlaneNormalWorld();
	return IsValid(OutSliceableTarget);
}

void AInspectorPawn::HandlePreviewTelemetryUpdated(const FSlicePreviewTelemetry& PreviewTelemetry)
{
	bCachedIntersectsMonolith = PreviewTelemetry.bIntersectsMonolith;
	CachedVeinProximityNormalized = PreviewTelemetry.VeinProximityNormalized;
	UpdateCameraParallaxFromAim();
	UpdateLaserSheetMaterialVisuals();
}

void AInspectorPawn::HandleCapacitorTelemetryStream(ETelemetryStreamId StreamId, float NormalizedValue)
{
	if (StreamId == ETelemetryStreamId::LaserCharge)
	{
		CachedLaserChargeNormalized = FMath::Clamp(NormalizedValue, 0.0f, 1.0f);
		UpdateLaserSheetMaterialVisuals();
	}

	OnTelemetryStream.Broadcast(StreamId, NormalizedValue);
}

void AInspectorPawn::HandleCapacitorTelemetryImpulse(
	ETelemetryImpulseId ImpulseId,
	float NormalizedMagnitude,
	const FVector& WorldLocation)
{
	if (IsValid(LaserPlaneAimComponent))
	{
		LaserPlaneAimComponent->ForceRefreshPreview();
	}
	OnTelemetryImpulse.Broadcast(ImpulseId, NormalizedMagnitude, WorldLocation);
}

void AInspectorPawn::HandleShiftPhaseChanged(EShiftPhase NewPhase)
{
	const bool bActiveInspection = (NewPhase == EShiftPhase::ActiveInspection);

	if (!bActiveInspection && IsValid(LaserCapacitorComponent))
	{
		LaserCapacitorComponent->ResetCapacitorState();
	}

	if (IsValid(LaserPlaneAimComponent))
	{
		if (bActiveInspection)
		{
			RefreshAimTargetAndFrameFromShift();
		}
		LaserPlaneAimComponent->SetAimingEnabled(bActiveInspection);
	}
}

void AInspectorPawn::HandleActiveContractLoaded(
	const UMonolithContractDataAsset* /*Contract*/,
	const UShiftProgressionDataAsset* ShiftConfig,
	int32 /*ContractIndex*/)
{
	if (IsValid(LaserCapacitorComponent))
	{
		LaserCapacitorComponent->ConfigureFromShiftProgression(ShiftConfig);
		LaserCapacitorComponent->ResetCapacitorState();
	}

	if (IsValid(LaserPlaneAimComponent))
	{
		LaserPlaneAimComponent->ResetAimAndOrientation();
	}

	RefreshAimTargetAndFrameFromShift();
	UpdateCameraParallaxFromAim();
	UpdateLaserSheetMaterialVisuals();
}

void AInspectorPawn::HandleManualResetHoldElapsed()
{
	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->RequestManualContractReset();
		}
	}
}

void AInspectorPawn::Input_AimCursor(const FInputActionValue& Value)
{
	if (IsValid(LaserPlaneAimComponent) && LaserPlaneAimComponent->IsAimingEnabled())
	{
		const FVector2D MouseDelta = Value.Get<FVector2D>();
		LaserPlaneAimComponent->AddAimCursorDelta(MouseDelta);
	}
}

void AInspectorPawn::Input_ScrollRoll(const FInputActionValue& Value)
{
	if (IsValid(LaserPlaneAimComponent) && LaserPlaneAimComponent->IsAimingEnabled())
	{
		const float WheelAxis = Value.Get<float>();
		LaserPlaneAimComponent->AddScrollWheelInput(WheelAxis, bIsFastRollSnapHeld);
	}
}

void AInspectorPawn::Input_FastSnapStarted(const FInputActionValue& /*Value*/)
{
	bIsFastRollSnapHeld = true;
}

void AInspectorPawn::Input_FastSnapEnded(const FInputActionValue& /*Value*/)
{
	bIsFastRollSnapHeld = false;
}

void AInspectorPawn::Input_FireStarted(const FInputActionValue& /*Value*/)
{
	if (!IsValid(LaserCapacitorComponent) || !IsValid(LaserPlaneAimComponent) || !LaserPlaneAimComponent->IsAimingEnabled())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (const UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			if (ShiftSubsystem->GetCurrentPhase() == EShiftPhase::ActiveInspection)
			{
				LaserCapacitorComponent->BeginCharge();
			}
		}
	}
}

void AInspectorPawn::Input_FireReleased(const FInputActionValue& /*Value*/)
{
	if (IsValid(LaserCapacitorComponent) && LaserCapacitorComponent->IsCharging())
	{
		LaserCapacitorComponent->CancelCharge();
	}
}

void AInspectorPawn::Input_BevelOrCancelStarted(const FInputActionValue& /*Value*/)
{
	// Section 5.4 Priority 1: Charge Abort takes strict priority and suppresses bevel until RMB is released and pressed again.
	if (IsValid(LaserCapacitorComponent) && LaserCapacitorComponent->IsCharging())
	{
		LaserCapacitorComponent->CancelCharge();
		if (IsValid(LaserPlaneAimComponent))
		{
			LaserPlaneAimComponent->SuppressBevelUntilNextPress();
			LaserPlaneAimComponent->SetBevelHeld(true);
		}
		return;
	}

	// Section 5.4 Priority 2: Freeze P_0 and route MouseDelta.Y into Pitch Bevel [-40, +40] deg.
	if (IsValid(LaserPlaneAimComponent))
	{
		LaserPlaneAimComponent->SetBevelHeld(true);
	}
}

void AInspectorPawn::Input_BevelOrCancelReleased(const FInputActionValue& /*Value*/)
{
	if (IsValid(LaserPlaneAimComponent))
	{
		LaserPlaneAimComponent->SetBevelHeld(false);
	}
}

void AInspectorPawn::Input_ToggleDiverter(const FInputActionValue& /*Value*/)
{
	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->RequestToggleDiverter();
		}
	}
}

void AInspectorPawn::Input_StampVerdict(const FInputActionValue& /*Value*/)
{
	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->TryStampQuotaVerdict();
		}
	}
}

void AInspectorPawn::Input_ResetHoldStarted(const FInputActionValue& /*Value*/)
{
	if (UWorld* World = GetWorld())
	{
		const float SafeHoldDuration = FMath::Max(0.1f, ManualResetHoldSeconds);
		World->GetTimerManager().SetTimer(
			ManualResetHoldTimerHandle,
			this,
			&AInspectorPawn::HandleManualResetHoldElapsed,
			SafeHoldDuration,
			false);
	}
}

void AInspectorPawn::Input_ResetHoldReleased(const FInputActionValue& /*Value*/)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ManualResetHoldTimerHandle);
	}
}

void AInspectorPawn::Input_ScalabilityLow(const FInputActionValue& /*Value*/)
{
	ApplyNativeScalabilityPreset(0, 65.0f);
}

void AInspectorPawn::Input_ScalabilityMed(const FInputActionValue& /*Value*/)
{
	ApplyNativeScalabilityPreset(1, 75.0f);
}

void AInspectorPawn::Input_ScalabilityHigh(const FInputActionValue& /*Value*/)
{
	ApplyNativeScalabilityPreset(2, 85.0f);
}

void AInspectorPawn::Input_ScalabilityEpic(const FInputActionValue& /*Value*/)
{
	ApplyNativeScalabilityPreset(3, 100.0f);
}
