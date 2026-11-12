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
#include "Misc/PackageName.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AInspectorPawn::AInspectorPawn()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	AutoPossessPlayer = EAutoReceiveInput::Player0;
	SetActorEnableCollision(false);

	PawnRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PawnRoot"));
	SetRootComponent(PawnRoot);
	PawnRoot->PrimaryComponentTick.bCanEverTick = false;

	InspectorCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("InspectorCamera"));
	InspectorCamera->SetupAttachment(PawnRoot);
	InspectorCamera->PrimaryComponentTick.bCanEverTick = false;
	InspectorCamera->SetRelativeLocation(BaselineCameraRelativeLocation);
	InspectorCamera->SetRelativeRotation(FRotator(BaselineCameraPitchDegrees, 0.0f, 0.0f));
	InspectorCamera->FieldOfView = DefaultCameraFieldOfView;
	InspectorCamera->bUsePawnControlRotation = false;
	ApplyLockedCameraExposureSettings();

	LaserPlaneVisualPivot = CreateDefaultSubobject<USceneComponent>(TEXT("LaserPlaneVisualPivot"));
	LaserPlaneVisualPivot->SetupAttachment(PawnRoot);
	LaserPlaneVisualPivot->PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		FallbackCubeMesh = CubeMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMeshFinder.Succeeded())
	{
		FallbackPlaneMesh = PlaneMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlattenMatFinder(
		TEXT("/Engine/EngineMaterials/FlattenMaterial.FlattenMaterial"));
	if (FlattenMatFinder.Succeeded())
	{
		BaseGreyboxPlaneMaterial = FlattenMatFinder.Object;
	}

	auto ConfigureLaserVisualMesh = [this](UStaticMeshComponent* MeshComp, UStaticMesh* AssignedMesh)
	{
		MeshComp->PrimaryComponentTick.bCanEverTick = false;
		MeshComp->SetSimulatePhysics(false);
		MeshComp->SetEnableGravity(false);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComp->SetGenerateOverlapEvents(false);
		MeshComp->SetCastShadow(false);
		MeshComp->bReceivesDecals = false;
		if (IsValid(AssignedMesh))
		{
			MeshComp->SetStaticMesh(AssignedMesh);
		}
		if (IsValid(BaseGreyboxPlaneMaterial))
		{
			MeshComp->SetMaterial(0, BaseGreyboxPlaneMaterial);
		}
	};

	LaserFrameFrontBar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaserFrameFrontBar"));
	LaserFrameFrontBar->SetupAttachment(LaserPlaneVisualPivot);
	ConfigureLaserVisualMesh(LaserFrameFrontBar, FallbackCubeMesh);

	LaserFrameBackBar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaserFrameBackBar"));
	LaserFrameBackBar->SetupAttachment(LaserPlaneVisualPivot);
	ConfigureLaserVisualMesh(LaserFrameBackBar, FallbackCubeMesh);

	LaserFrameLeftBar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaserFrameLeftBar"));
	LaserFrameLeftBar->SetupAttachment(LaserPlaneVisualPivot);
	ConfigureLaserVisualMesh(LaserFrameLeftBar, FallbackCubeMesh);

	LaserFrameRightBar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaserFrameRightBar"));
	LaserFrameRightBar->SetupAttachment(LaserPlaneVisualPivot);
	ConfigureLaserVisualMesh(LaserFrameRightBar, FallbackCubeMesh);

	LaserSheetVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaserSheetVisual"));
	LaserSheetVisual->SetupAttachment(LaserPlaneVisualPivot);
	ConfigureLaserVisualMesh(LaserSheetVisual, FallbackPlaneMesh);

	LaserSheetUndersideVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaserSheetUndersideVisual"));
	LaserSheetUndersideVisual->SetupAttachment(LaserPlaneVisualPivot);
	ConfigureLaserVisualMesh(LaserSheetUndersideVisual, FallbackPlaneMesh);
	LaserSheetUndersideVisual->SetRelativeRotation(FRotator(180.0f, 0.0f, 0.0f));

	ConfigureLaserFrameGeometry();

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

	if (IsValid(InspectorCamera))
	{
		InspectorCamera->SetRelativeLocation(BaselineCameraRelativeLocation);
		InspectorCamera->SetRelativeRotation(FRotator(BaselineCameraPitchDegrees, 0.0f, 0.0f));
		InspectorCamera->SetFieldOfView(DefaultCameraFieldOfView);
		ApplyLockedCameraExposureSettings();
	}

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

void AInspectorPawn::ApplyLockedCameraExposureSettings()
{
	if (!IsValid(InspectorCamera))
	{
		return;
	}

	InspectorCamera->PostProcessBlendWeight = 1.0f;
	FPostProcessSettings& PP = InspectorCamera->PostProcessSettings;
	PP.bOverride_AutoExposureMinBrightness = true;
	PP.bOverride_AutoExposureMaxBrightness = true;
	PP.bOverride_AutoExposureBias = true;
	PP.bOverride_BloomIntensity = true;
	PP.AutoExposureMinBrightness = LockedAutoExposureMinBrightness;
	PP.AutoExposureMaxBrightness = LockedAutoExposureMaxBrightness;
	PP.AutoExposureBias = LockedAutoExposureBias;
	PP.BloomIntensity = LockedBloomIntensity;
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

void AInspectorPawn::ConfigureLaserFrameGeometry()
{
	const double SafeDepthCm = FMath::Max(20.0, static_cast<double>(LaserFrameDepthCm));
	const double SafeWidthCm = FMath::Max(20.0, static_cast<double>(LaserFrameWidthCm));
	const double SafeBarWidthCm = FMath::Max(0.2, static_cast<double>(LaserFrameBarWidthCm));
	const double SafeBarThickCm = FMath::Max(0.2, static_cast<double>(LaserFrameBarThicknessCm));

	const double HalfDepth = 0.5 * SafeDepthCm;
	const double HalfWidth = 0.5 * SafeWidthCm;
	const double BarWidthScale = SafeBarWidthCm / 100.0;
	const double BarThickScale = SafeBarThickCm / 100.0;
	const double SpanWidthScale = (SafeWidthCm + SafeBarWidthCm) / 100.0;
	const double SpanDepthScale = (SafeDepthCm + SafeBarWidthCm) / 100.0;

	if (IsValid(LaserFrameFrontBar))
	{
		LaserFrameFrontBar->SetRelativeLocation(FVector(-HalfDepth, 0.0, 0.0));
		LaserFrameFrontBar->SetRelativeScale3D(FVector(BarWidthScale, SpanWidthScale, BarThickScale));
	}
	if (IsValid(LaserFrameBackBar))
	{
		LaserFrameBackBar->SetRelativeLocation(FVector(+HalfDepth, 0.0, 0.0));
		LaserFrameBackBar->SetRelativeScale3D(FVector(BarWidthScale, SpanWidthScale, BarThickScale));
	}
	if (IsValid(LaserFrameLeftBar))
	{
		LaserFrameLeftBar->SetRelativeLocation(FVector(0.0, -HalfWidth, 0.0));
		LaserFrameLeftBar->SetRelativeScale3D(FVector(SpanDepthScale, BarWidthScale, BarThickScale));
	}
	if (IsValid(LaserFrameRightBar))
	{
		LaserFrameRightBar->SetRelativeLocation(FVector(0.0, +HalfWidth, 0.0));
		LaserFrameRightBar->SetRelativeScale3D(FVector(SpanDepthScale, BarWidthScale, BarThickScale));
	}

	const bool bHasCustomTranslucentSheet = IsValid(LaserSheetOverrideMaterial) || IsValid(AutoResolvedLaserSheetMasterMaterial);
	if (bHasCustomTranslucentSheet)
	{
		const FVector FullSheetScale(SafeDepthCm / 100.0, SafeWidthCm / 100.0, 1.0);
		if (IsValid(LaserSheetVisual))
		{
			LaserSheetVisual->SetRelativeLocation(FVector(0.0, 0.0, 0.02));
			LaserSheetVisual->SetRelativeScale3D(FullSheetScale);
		}
		if (IsValid(LaserSheetUndersideVisual))
		{
			LaserSheetUndersideVisual->SetRelativeLocation(FVector(0.0, 0.0, -0.02));
			LaserSheetUndersideVisual->SetRelativeScale3D(FullSheetScale);
		}
	}
	else
	{
		// Zero-asset fallback: thin two-sided front guide ribbon behind the front frame bar so opaque BasicShapeMaterial never occludes the monolith.
		const double GuideStripDepthCm = 1.6;
		const double GuideStripX = -HalfDepth + (0.5 * SafeBarWidthCm) + (0.5 * GuideStripDepthCm);
		const FVector GuideRibbonScale(GuideStripDepthCm / 100.0, SafeWidthCm / 100.0, 1.0);
		if (IsValid(LaserSheetVisual))
		{
			LaserSheetVisual->SetRelativeLocation(FVector(GuideStripX, 0.0, 0.02));
			LaserSheetVisual->SetRelativeScale3D(GuideRibbonScale);
		}
		if (IsValid(LaserSheetUndersideVisual))
		{
			LaserSheetUndersideVisual->SetRelativeLocation(FVector(GuideStripX, 0.0, -0.02));
			LaserSheetUndersideVisual->SetRelativeScale3D(GuideRibbonScale);
		}
	}
}

void AInspectorPawn::InitializeLaserSheetDynamicMaterial()
{
	if (!IsValid(LaserSheetOverrideMaterial) && !IsValid(AutoResolvedLaserSheetMasterMaterial))
	{
		if (FPackageName::DoesPackageExist(TEXT("/Game/Materials/M_LaserSheet_Master")))
		{
			AutoResolvedLaserSheetMasterMaterial = LoadObject<UMaterialInterface>(
				nullptr,
				TEXT("/Game/Materials/M_LaserSheet_Master.M_LaserSheet_Master"));
		}
	}

	ConfigureLaserFrameGeometry();

	UMaterialInterface* EffectiveCustomSheetMat = IsValid(LaserSheetOverrideMaterial)
		? LaserSheetOverrideMaterial.Get()
		: AutoResolvedLaserSheetMasterMaterial.Get();
	UMaterialInterface* SheetSourceMaterial = IsValid(EffectiveCustomSheetMat)
		? EffectiveCustomSheetMat
		: BaseGreyboxPlaneMaterial.Get();

	if (IsValid(SheetSourceMaterial))
	{
		LaserSheetMID = UMaterialInstanceDynamic::Create(SheetSourceMaterial, this);
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

	UMaterialInterface* FrameSourceMaterial = IsValid(BaseGreyboxPlaneMaterial)
		? BaseGreyboxPlaneMaterial.Get()
		: SheetSourceMaterial;

	if (IsValid(FrameSourceMaterial))
	{
		LaserFrameMID = UMaterialInstanceDynamic::Create(FrameSourceMaterial, this);
		if (IsValid(LaserFrameMID))
		{
			UStaticMeshComponent* const FrameBars[4] = {
				LaserFrameFrontBar,
				LaserFrameBackBar,
				LaserFrameLeftBar,
				LaserFrameRightBar
			};
			for (UStaticMeshComponent* BarComp : FrameBars)
			{
				if (IsValid(BarComp))
				{
					BarComp->SetMaterial(0, LaserFrameMID);
				}
			}
		}
	}
}

void AInspectorPawn::UpdateLaserSheetMaterialVisuals()
{
	if (!IsValid(LaserSheetMID) && !IsValid(LaserFrameMID))
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

	auto ApplyParameters = [&](UMaterialInstanceDynamic* TargetMID, float OpacityScalar)
	{
		if (!IsValid(TargetMID))
		{
			return;
		}
		TargetMID->SetVectorParameterValue(TEXT("BaseColorConst"), BaseHue);
		TargetMID->SetVectorParameterValue(TEXT("EmissiveColorConst"), FinalColor);
		TargetMID->SetScalarParameterValue(TEXT("EmissiveScale"), 1.0f);
		TargetMID->SetScalarParameterValue(TEXT("RoughnessConst"), 0.25f);
		TargetMID->SetScalarParameterValue(TEXT("MetallicConst"), 0.15f);
		TargetMID->SetVectorParameterValue(TEXT("Color"), FinalColor);
		TargetMID->SetVectorParameterValue(TEXT("BaseColor"), BaseHue);
		TargetMID->SetVectorParameterValue(TEXT("EmissiveColor"), FinalColor);
		TargetMID->SetScalarParameterValue(TEXT("Opacity"), OpacityScalar);
		TargetMID->SetScalarParameterValue(TEXT("VeinProximity"), CachedVeinProximityNormalized);
		TargetMID->SetScalarParameterValue(TEXT("LaserCharge"), CachedLaserChargeNormalized);
	};

	ApplyParameters(LaserSheetMID, bCachedIntersectsMonolith ? 0.42f : 0.18f);
	ApplyParameters(LaserFrameMID, 1.0f);
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
	const float FinalPitchDeg = FMath::Min(TargetPitchDeg + BaselineCameraPitchDegrees, MaxUpwardCameraPitchCeilingDegrees);

	// Subtle baseline downward look plus clamped aim-tracking parallax that never cuts off the bottom console bezel.
	InspectorCamera->SetRelativeRotation(FRotator(FinalPitchDeg, TargetYawDeg, 0.0f));
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
