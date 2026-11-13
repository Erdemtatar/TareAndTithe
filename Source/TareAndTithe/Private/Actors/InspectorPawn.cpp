// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/InspectorPawn.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Components/AudioComponent.h"
#include "Components/AudioTelemetryBridgeComponent.h"
#include "Components/InspectorHudBridgeComponent.h"
#include "Components/InspectorKinestheticsComponent.h"
#include "Components/LaserCapacitorComponent.h"
#include "Components/LaserPlaneAimComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Data/MonolithContractDataAsset.h"
#include "Data/ShiftProgressionDataAsset.h"
#include "Data/TareTithePresentationDataAsset.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Interfaces/SliceableTargetInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"
#include "Misc/PackageName.h"
#include "SceneView.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UnrealClient.h"
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

	// Steady view pivot (baseline pullback + smooth aim parallax) strictly isolated from kinesthetic camera shake.
	CameraViewPivot = CreateDefaultSubobject<USceneComponent>(TEXT("CameraViewPivot"));
	CameraViewPivot->SetupAttachment(PawnRoot);
	CameraViewPivot->PrimaryComponentTick.bCanEverTick = false;
	CameraViewPivot->SetRelativeLocation(BaselineCameraRelativeLocation);
	CameraViewPivot->SetRelativeRotation(FRotator(BaselineCameraPitchDegrees, 0.0f, 0.0f));
	CameraViewPivot->SetRelativeScale3D(FVector::OneVector);

	CameraShakeOffsetComponent = CreateDefaultSubobject<USceneComponent>(TEXT("CameraShakeOffsetComponent"));
	CameraShakeOffsetComponent->SetupAttachment(CameraViewPivot);
	CameraShakeOffsetComponent->PrimaryComponentTick.bCanEverTick = false;
	CameraShakeOffsetComponent->SetRelativeTransform(FTransform::Identity);

	InspectorCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("InspectorCamera"));
	InspectorCamera->SetupAttachment(CameraShakeOffsetComponent);
	InspectorCamera->PrimaryComponentTick.bCanEverTick = false;
	InspectorCamera->SetRelativeTransform(FTransform::Identity);
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

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitTextMatFinder(
		TEXT("/Engine/EngineMaterials/UnlitText.UnlitText"));
	if (UnlitTextMatFinder.Succeeded())
	{
		UnlitReticleTextMaterial = UnlitTextMatFinder.Object;
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

	auto ConfigureReticleTextLine = [this](
		UTextRenderComponent* TextComp,
		USceneComponent* ParentComp,
		const FVector& RelLocation,
		const FRotator& RelRotation,
		float WorldSize,
		const FColor& Color,
		const TCHAR* InitialText)
	{
		TextComp->SetupAttachment(ParentComp);
		TextComp->PrimaryComponentTick.bCanEverTick = false;
		TextComp->SetSimulatePhysics(false);
		TextComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TextComp->SetGenerateOverlapEvents(false);
		TextComp->SetCastShadow(false);
		TextComp->bReceivesDecals = false;
		TextComp->SetRelativeLocation(RelLocation);
		TextComp->SetRelativeRotation(RelRotation);
		TextComp->SetRelativeScale3D(FVector::OneVector);
		TextComp->SetHorizontalAlignment(EHTA_Left);
		TextComp->SetVerticalAlignment(EVRTA_TextCenter);
		TextComp->SetWorldSize(WorldSize);
		if (IsValid(UnlitReticleTextMaterial))
		{
			TextComp->SetTextMaterial(UnlitReticleTextMaterial);
		}
		TextComp->SetTextRenderColor(Color);
		TextComp->SetText(FText::FromString(InitialText));
		TextComp->SetVisibility(false, false);
		TextComp->SetHiddenInGame(true, false);
	};

	// Roll-stabilized reticle readout attached to PawnRoot (strictly isolated from CameraShakeOffsetComponent and LaserPlaneVisualPivot).
	LaserReticleReadout = CreateDefaultSubobject<UTextRenderComponent>(TEXT("LaserReticleReadout"));
	ConfigureReticleTextLine(
		LaserReticleReadout,
		PawnRoot,
		FVector(214.0, static_cast<double>(LaserReticleLateralOffsetCm), 172.0),
		FRotator(0.0f, 180.0f, 0.0f),
		LaserReticlePrimaryWorldSize,
		ReticleCyanColor,
		TEXT("TITHE | NEED +12.00 KG"));

	LaserReticleYieldReadout = CreateDefaultSubobject<UTextRenderComponent>(TEXT("LaserReticleYieldReadout"));
	ConfigureReticleTextLine(
		LaserReticleYieldReadout,
		LaserReticleReadout,
		FVector(0.0, 0.0, -static_cast<double>(LaserReticleLineSpacingCm)),
		FRotator::ZeroRotator,
		LaserReticleYieldWorldSize,
		ReticleCyanColor,
		TEXT("CUT EST: ---"));

	LaserReticleLoadReadout = CreateDefaultSubobject<UTextRenderComponent>(TEXT("LaserReticleLoadReadout"));
	ConfigureReticleTextLine(
		LaserReticleLoadReadout,
		LaserReticleReadout,
		FVector(0.0, 0.0, -2.0 * static_cast<double>(LaserReticleLineSpacingCm)),
		FRotator::ZeroRotator,
		LaserReticleLoadWorldSize,
		ReticleLoadCellColor,
		TEXT("CLAMP LOAD: 0.0 KG"));

	LaserReticleHazardReadout = CreateDefaultSubobject<UTextRenderComponent>(TEXT("LaserReticleHazardReadout"));
	ConfigureReticleTextLine(
		LaserReticleHazardReadout,
		LaserReticleReadout,
		FVector(0.0, 0.0, -3.0 * static_cast<double>(LaserReticleLineSpacingCm)),
		FRotator::ZeroRotator,
		LaserReticleHazardWorldSize,
		ReticleCrimsonColor,
		TEXT(""));

	ConfigureLaserFrameGeometry();

	LaserPlaneAimComponent = CreateDefaultSubobject<ULaserPlaneAimComponent>(TEXT("LaserPlaneAimComponent"));
	LaserCapacitorComponent = CreateDefaultSubobject<ULaserCapacitorComponent>(TEXT("LaserCapacitorComponent"));
	InspectorKinestheticsComponent = CreateDefaultSubobject<UInspectorKinestheticsComponent>(TEXT("InspectorKinestheticsComponent"));
	AudioTelemetryBridgeComponent = CreateDefaultSubobject<UAudioTelemetryBridgeComponent>(TEXT("AudioTelemetryBridgeComponent"));
	InspectorHudBridgeComponent = CreateDefaultSubobject<UInspectorHudBridgeComponent>(TEXT("InspectorHudBridgeComponent"));

	ContinuousMetaSoundAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("ContinuousMetaSoundAudioComponent"));
	ContinuousMetaSoundAudioComponent->SetupAttachment(PawnRoot);
	ContinuousMetaSoundAudioComponent->PrimaryComponentTick.bCanEverTick = false;
	ContinuousMetaSoundAudioComponent->bAutoActivate = false;
	ContinuousMetaSoundAudioComponent->bAutoDestroy = false;

	ImpulseMetaSoundAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("ImpulseMetaSoundAudioComponent"));
	ImpulseMetaSoundAudioComponent->SetupAttachment(PawnRoot);
	ImpulseMetaSoundAudioComponent->PrimaryComponentTick.bCanEverTick = false;
	ImpulseMetaSoundAudioComponent->bAutoActivate = false;
	ImpulseMetaSoundAudioComponent->bAutoDestroy = false;

	AmbientBedAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AmbientBedAudioComponent"));
	AmbientBedAudioComponent->SetupAttachment(PawnRoot);
	AmbientBedAudioComponent->PrimaryComponentTick.bCanEverTick = false;
	AmbientBedAudioComponent->bAutoActivate = false;
	AmbientBedAudioComponent->bAutoDestroy = false;
}

void AInspectorPawn::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(CameraViewPivot))
	{
		CameraViewPivot->SetRelativeLocation(BaselineCameraRelativeLocation);
		CameraViewPivot->SetRelativeRotation(FRotator(BaselineCameraPitchDegrees, 0.0f, 0.0f));
		CameraViewPivot->SetRelativeScale3D(FVector::OneVector);
	}
	if (IsValid(CameraShakeOffsetComponent))
	{
		CameraShakeOffsetComponent->SetRelativeTransform(FTransform::Identity);
	}
	if (IsValid(InspectorCamera))
	{
		InspectorCamera->SetRelativeTransform(FTransform::Identity);
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

	if (IsValid(ContinuousMetaSoundAudioComponent))
	{
		ContinuousMetaSoundAudioComponent->bAutoDestroy = false;
		if (IsValid(ContinuousMetaSoundAsset))
		{
			ContinuousMetaSoundAudioComponent->SetSound(ContinuousMetaSoundAsset);
		}
	}
	if (IsValid(ImpulseMetaSoundAudioComponent))
	{
		ImpulseMetaSoundAudioComponent->bAutoDestroy = false;
		if (IsValid(ImpulseMetaSoundAsset))
		{
			ImpulseMetaSoundAudioComponent->SetSound(ImpulseMetaSoundAsset);
		}
	}
	if (IsValid(AmbientBedAudioComponent))
	{
		AmbientBedAudioComponent->bAutoDestroy = false;
	}

	if (IsValid(AudioTelemetryBridgeComponent))
	{
		AudioTelemetryBridgeComponent->SetContinuousLoopsAudioComponent(ContinuousMetaSoundAudioComponent);
		AudioTelemetryBridgeComponent->SetDiscreteImpulsesAudioComponent(ImpulseMetaSoundAudioComponent);
		AudioTelemetryBridgeComponent->SetAmbientBedAudioComponent(AmbientBedAudioComponent);
		AudioTelemetryBridgeComponent->OnFilteredTelemetryStream.AddUObject(this, &AInspectorPawn::HandleFilteredTelemetryStream);
	}

	if (IsValid(UnlitReticleTextMaterial))
	{
		UTextRenderComponent* const ReticleLines[4] = {
			LaserReticleReadout,
			LaserReticleYieldReadout,
			LaserReticleLoadReadout,
			LaserReticleHazardReadout
		};
		for (UTextRenderComponent* LineComp : ReticleLines)
		{
			if (IsValid(LineComp))
			{
				LineComp->SetTextMaterial(UnlitReticleTextMaterial);
				LineComp->SetCastShadow(false);
			}
		}
	}

	if (IsValid(InspectorKinestheticsComponent))
	{
		InspectorKinestheticsComponent->BindShakeAndCameraComponents(
			CameraShakeOffsetComponent,
			InspectorCamera,
			DefaultCameraFieldOfView);
		InspectorKinestheticsComponent->BindTelemetryBridge(AudioTelemetryBridgeComponent);
	}

	if (IsValid(LaserPlaneAimComponent))
	{
		LaserPlaneAimComponent->SetLaserVisualComponent(LaserPlaneVisualPivot);
		LaserPlaneAimComponent->SetSheetApertureExtents(
			0.5f * FMath::Max(20.0f, LaserFrameDepthCm),
			0.5f * FMath::Max(20.0f, LaserFrameWidthCm));
		LaserPlaneAimComponent->OnPreviewUpdated.AddUObject(this, &AInspectorPawn::HandlePreviewTelemetryUpdated);
	}

	if (IsValid(LaserCapacitorComponent))
	{
		LaserCapacitorComponent->OnQuerySliceTarget.BindUObject(this, &AInspectorPawn::HandleQueryLaserSliceTarget);
		LaserCapacitorComponent->OnChargeStateChanged.AddUObject(this, &AInspectorPawn::HandleCapacitorChargeStateChanged);
		LaserCapacitorComponent->OnTelemetryStream.AddUObject(this, &AInspectorPawn::HandleCapacitorTelemetryStream);
		LaserCapacitorComponent->OnTelemetryImpulse.AddUObject(this, &AInspectorPawn::HandleCapacitorTelemetryImpulse);
	}

	if (IsValid(InspectorHudBridgeComponent))
	{
		InspectorHudBridgeComponent->BindPawnTelemetrySources(
			LaserPlaneAimComponent,
			LaserCapacitorComponent,
			AudioTelemetryBridgeComponent,
			OnReticleProjectionUpdated);
	}

	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->RegisterTelemetryPublisher(this, this);
			ShiftSubsystem->OnNativeContractSnapshotUpdated.AddUObject(this, &AInspectorPawn::HandleContractSnapshotUpdated);
			ShiftSubsystem->OnShiftPhaseChanged.AddUObject(this, &AInspectorPawn::HandleShiftPhaseChanged);
			ShiftSubsystem->OnActiveContractLoaded.AddUObject(this, &AInspectorPawn::HandleActiveContractLoaded);
			ShiftSubsystem->OnPresentationAssetChanged.AddUObject(this, &AInspectorPawn::ApplyPresentationData);

			ApplyPresentationData(ShiftSubsystem->GetActivePresentationAsset());

			if (const UShiftProgressionDataAsset* ShiftConfig = ShiftSubsystem->GetActiveShiftProgression())
			{
				if (IsValid(LaserCapacitorComponent))
				{
					LaserCapacitorComponent->ConfigureFromShiftProgression(ShiftConfig);
				}
			}

			CachedHudSnapshot = ShiftSubsystem->GetCurrentSnapshot();
			if (IsValid(AudioTelemetryBridgeComponent))
			{
				CachedClampStrainNormalized = FMath::Clamp(
					AudioTelemetryBridgeComponent->GetLastFlushedStreamValue(ETelemetryStreamId::ClampStrain),
					0.0f,
					1.0f);
			}

			RefreshAimTargetAndFrameFromShift();
			HandleShiftPhaseChanged(ShiftSubsystem->GetCurrentPhase());
		}
	}

	UpdateLegacyReticleVisibility();
	UpdateLaserSheetMaterialVisuals();
	UpdateLaserReticleTransform();
	UpdateLaserReticleReadoutText();
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
	if (IsValid(ActiveRequisitionHydraulicsAction))
	{
		EnhancedInput->BindAction(ActiveRequisitionHydraulicsAction, ETriggerEvent::Started, this, &AInspectorPawn::Input_RequisitionHydraulics);
	}
	if (IsValid(ActiveRequisitionTareAction))
	{
		EnhancedInput->BindAction(ActiveRequisitionTareAction, ETriggerEvent::Started, this, &AInspectorPawn::Input_RequisitionTare);
	}
	if (IsValid(ActiveRequisitionScannerAction))
	{
		EnhancedInput->BindAction(ActiveRequisitionScannerAction, ETriggerEvent::Started, this, &AInspectorPawn::Input_RequisitionScanner);
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
			ShiftSubsystem->OnNativeContractSnapshotUpdated.RemoveAll(this);
			ShiftSubsystem->OnShiftPhaseChanged.RemoveAll(this);
			ShiftSubsystem->OnActiveContractLoaded.RemoveAll(this);
			ShiftSubsystem->OnPresentationAssetChanged.RemoveAll(this);
			ShiftSubsystem->UnregisterTelemetryPublisher(this);
		}
	}

	if (IsValid(AudioTelemetryBridgeComponent))
	{
		AudioTelemetryBridgeComponent->OnFilteredTelemetryStream.RemoveAll(this);
	}

	if (IsValid(LaserPlaneAimComponent))
	{
		LaserPlaneAimComponent->OnPreviewUpdated.RemoveAll(this);
	}
	if (IsValid(LaserCapacitorComponent))
	{
		LaserCapacitorComponent->OnQuerySliceTarget.Unbind();
		LaserCapacitorComponent->OnChargeStateChanged.RemoveAll(this);
		LaserCapacitorComponent->OnTelemetryStream.RemoveAll(this);
		LaserCapacitorComponent->OnTelemetryImpulse.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AInspectorPawn::ApplyPresentationData(const UTareTithePresentationDataAsset* InPresentationData)
{
	PresentationData = InPresentationData;

	if (IsValid(InspectorKinestheticsComponent))
	{
		InspectorKinestheticsComponent->SetPresentationData(InPresentationData);
	}

	if (IsValid(AudioTelemetryBridgeComponent))
	{
		AudioTelemetryBridgeComponent->ApplyPresentationData(InPresentationData);
	}

	if (IsValid(InspectorHudBridgeComponent))
	{
		InspectorHudBridgeComponent->ApplyPresentationData(InPresentationData);
	}

	UpdateLegacyReticleVisibility();
	ApplyLockedCameraExposureSettings();
	InitializeLaserSheetDynamicMaterial();
	UpdateLaserSheetMaterialVisuals();
	if (IsLegacyWorldSpaceReadoutEnabled())
	{
		UpdateLaserReticleTransform();
		UpdateLaserReticleReadoutText();
	}
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

	FVector AnchorWorld = GetActorLocation() + (GetActorForwardVector() * 260.0);
	const FVector RigWorldLoc = IsValid(ShiftSubsystem) ? ShiftSubsystem->GetSliceableRigWorldLocation() : FVector::ZeroVector;
	const bool bHasRigWorldLoc = !RigWorldLoc.IsNearlyZero();
	if (bHasRigWorldLoc)
	{
		AnchorWorld.X = RigWorldLoc.X;
		AnchorWorld.Y = RigWorldLoc.Y;
	}

	if (IsValid(TargetObj) && TargetObj->Implements<USliceableTargetInterface>())
	{
		const FBox TargetBounds = ISliceableTargetInterface::Execute_GetTargetWorldBounds(TargetObj);
		if (TargetBounds.IsValid)
		{
			const FVector BoundsCenter = TargetBounds.GetCenter();
			AnchorWorld.Z = BoundsCenter.Z;
			if (!bHasRigWorldLoc)
			{
				AnchorWorld.X = BoundsCenter.X;
				AnchorWorld.Y = BoundsCenter.Y;
			}
		}
	}

	LaserPlaneAimComponent->SetFrameBasis(AnchorWorld, GetActorForwardVector());
	UpdateLaserReticleTransform();
}

void AInspectorPawn::ApplyLockedCameraExposureSettings()
{
	if (!IsValid(InspectorCamera))
	{
		return;
	}

	FCameraPostProcessPolishConfig Cfg;
	if (IsValid(PresentationData))
	{
		Cfg = PresentationData->CameraPostProcessConfig;
	}
	else
	{
		Cfg.LockedEV100MinMax = LockedAutoExposureMinBrightness;
		Cfg.ExposureCompensationBias = LockedAutoExposureBias;
		Cfg.BloomIntensity = LockedBloomIntensity;
		Cfg.BloomThreshold = LockedBloomThreshold;
	}

	InspectorCamera->PostProcessBlendWeight = 1.0f;
	FPostProcessSettings& PP = InspectorCamera->PostProcessSettings;
	PP.bOverride_AutoExposureMinBrightness = true;
	PP.bOverride_AutoExposureMaxBrightness = true;
	PP.bOverride_AutoExposureBias = true;
	PP.bOverride_LocalExposureHighlightContrastScale = true;
	PP.bOverride_LocalExposureShadowContrastScale = true;
	PP.bOverride_BloomIntensity = true;
	PP.bOverride_BloomThreshold = true;
	PP.bOverride_FilmSlope = true;
	PP.bOverride_FilmToe = true;
	PP.bOverride_FilmShoulder = true;
	PP.bOverride_ColorContrast = true;
	PP.bOverride_ColorSaturation = true;
	PP.bOverride_VignetteIntensity = true;

	PP.AutoExposureMinBrightness = FMath::Max(0.05f, Cfg.LockedEV100MinMax);
	PP.AutoExposureMaxBrightness = FMath::Max(0.05f, Cfg.LockedEV100MinMax);
	PP.AutoExposureBias = Cfg.ExposureCompensationBias;
	PP.LocalExposureHighlightContrastScale = FMath::Clamp(Cfg.LocalExposureHighlightContrast, 0.5f, 1.0f);
	PP.LocalExposureShadowContrastScale = FMath::Clamp(Cfg.LocalExposureShadowContrast, 0.5f, 1.0f);
	PP.BloomIntensity = FMath::Clamp(Cfg.BloomIntensity, 0.0f, 2.0f);
	PP.BloomThreshold = FMath::Max(1.15f, Cfg.BloomThreshold);
	PP.FilmSlope = FMath::Clamp(Cfg.FilmSlope, 0.5f, 1.0f);
	PP.FilmToe = FMath::Clamp(Cfg.FilmToe, 0.3f, 0.8f);
	PP.FilmShoulder = FMath::Clamp(Cfg.FilmShoulder, 0.1f, 0.8f);
	PP.ColorContrast = FVector4(1.0, 1.0, 1.0, static_cast<double>(FMath::Clamp(Cfg.ColorContrast, 0.8f, 1.4f)));
	PP.ColorSaturation = FVector4(1.0, 1.0, 1.0, static_cast<double>(FMath::Clamp(Cfg.ColorSaturation, 0.5f, 1.4f)));
	PP.VignetteIntensity = FMath::Clamp(Cfg.VignetteIntensity, 0.0f, 0.6f);
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
	ResolveOrCreateAction(ActiveRequisitionHydraulicsAction, AssignedRequisitionHydraulicsAction, TEXT("IA_Transient_RequisitionHydraulics"), EInputActionValueType::Boolean);
	ResolveOrCreateAction(ActiveRequisitionTareAction, AssignedRequisitionTareAction, TEXT("IA_Transient_RequisitionTare"), EInputActionValueType::Boolean);
	ResolveOrCreateAction(ActiveRequisitionScannerAction, AssignedRequisitionScannerAction, TEXT("IA_Transient_RequisitionScanner"), EInputActionValueType::Boolean);
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
			// Hardware Requisitions [1] HYD, [2] TARE, [3] SCAN (Guard 1: F6..F9 used for Scalability to avoid F5 PIE Shader Complexity collision)
			TransientIMC->MapKey(ActiveRequisitionHydraulicsAction, EKeys::One);
			TransientIMC->MapKey(ActiveRequisitionTareAction, EKeys::Two);
			TransientIMC->MapKey(ActiveRequisitionScannerAction, EKeys::Three);
			TransientIMC->MapKey(ActiveScalabilityLowAction, EKeys::F6);
			TransientIMC->MapKey(ActiveScalabilityMedAction, EKeys::F7);
			TransientIMC->MapKey(ActiveScalabilityHighAction, EKeys::F8);
			TransientIMC->MapKey(ActiveScalabilityEpicAction, EKeys::F9);
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
	if (IsValid(LaserPlaneAimComponent))
	{
		LaserPlaneAimComponent->SetSheetApertureExtents(
			static_cast<float>(HalfDepth),
			static_cast<float>(HalfWidth));
	}
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

	const bool bHasPresentationSheetMaterial = IsValid(PresentationData) && IsValid(PresentationData->LaserSheetMaterial);
	const bool bHasCustomTranslucentSheet =
		IsValid(LaserSheetOverrideMaterial) ||
		bHasPresentationSheetMaterial ||
		IsValid(AutoResolvedLaserSheetMasterMaterial);
	if (bHasCustomTranslucentSheet)
	{
		// Guard 3: Positive scale (det > 0) + exact 0 deg / 180 deg rigid rotations guarantee opposite outward front-face normals (+Z / -Z)
		// so single-sided M_LaserSheet_Master (two_sided = False) renders cleanly both above and below camera eye-level without double-draw opacity stacking.
		const FVector FullSheetScale(SafeDepthCm / 100.0, SafeWidthCm / 100.0, 1.0);
		if (IsValid(LaserSheetVisual))
		{
			LaserSheetVisual->SetRelativeLocation(FVector(0.0, 0.0, 0.02));
			LaserSheetVisual->SetRelativeRotation(FRotator::ZeroRotator);
			LaserSheetVisual->SetRelativeScale3D(FullSheetScale);
		}
		if (IsValid(LaserSheetUndersideVisual))
		{
			LaserSheetUndersideVisual->SetRelativeLocation(FVector(0.0, 0.0, -0.02));
			LaserSheetUndersideVisual->SetRelativeRotation(FRotator(180.0f, 0.0f, 0.0f));
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
			LaserSheetVisual->SetRelativeRotation(FRotator::ZeroRotator);
			LaserSheetVisual->SetRelativeScale3D(GuideRibbonScale);
		}
		if (IsValid(LaserSheetUndersideVisual))
		{
			LaserSheetUndersideVisual->SetRelativeLocation(FVector(GuideStripX, 0.0, -0.02));
			LaserSheetUndersideVisual->SetRelativeRotation(FRotator(180.0f, 0.0f, 0.0f));
			LaserSheetUndersideVisual->SetRelativeScale3D(GuideRibbonScale);
		}
	}
}

void AInspectorPawn::InitializeLaserSheetDynamicMaterial()
{
	UMaterialInterface* PresentationSheetMat =
		(IsValid(PresentationData) && IsValid(PresentationData->LaserSheetMaterial))
		? PresentationData->LaserSheetMaterial.Get()
		: nullptr;

	if (!IsValid(LaserSheetOverrideMaterial) && !IsValid(PresentationSheetMat) && !IsValid(AutoResolvedLaserSheetMasterMaterial))
	{
		if (FPackageName::DoesPackageExist(TEXT("/Game/Presentation/Materials/M_LaserSheet_Master")))
		{
			AutoResolvedLaserSheetMasterMaterial = LoadObject<UMaterialInterface>(
				nullptr,
				TEXT("/Game/Presentation/Materials/M_LaserSheet_Master.M_LaserSheet_Master"));
		}
		else if (FPackageName::DoesPackageExist(TEXT("/Game/Materials/M_LaserSheet_Master")))
		{
			AutoResolvedLaserSheetMasterMaterial = LoadObject<UMaterialInterface>(
				nullptr,
				TEXT("/Game/Materials/M_LaserSheet_Master.M_LaserSheet_Master"));
		}
	}

	ConfigureLaserFrameGeometry();

	UMaterialInterface* EffectiveCustomSheetMat = IsValid(LaserSheetOverrideMaterial)
		? LaserSheetOverrideMaterial.Get()
		: (IsValid(PresentationSheetMat) ? PresentationSheetMat : AutoResolvedLaserSheetMasterMaterial.Get());
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

	constexpr float QuotaFloatEpsilonKg = 1.0e-3f;
	const FLinearColor SafeIdleColor(0.04f, 0.52f, 0.78f, 1.0f);
	const FLinearColor IntersectCyanColor(0.08f, 0.88f, 1.00f, 1.0f);
	const FLinearColor WindowMatchEmeraldColor(0.04f, 0.96f, 0.36f, 1.0f);
	const FLinearColor TarePurgeReadyColor(0.06f, 0.94f, 0.62f, 1.0f);
	const FLinearColor WarningAmberColor(1.00f, 0.56f, 0.04f, 1.0f);
	const FLinearColor DangerCrimsonColor(1.00f, 0.05f, 0.03f, 1.0f);
	const FLinearColor CapacitorHotWhiteColor(1.00f, 0.94f, 0.78f, 1.0f);

	const bool bValidContact = HasValidPredictiveSliceContact();
	const bool bDirectVeinHit = bCachedIntersectsMonolith && CachedPreviewTelemetry.bWillDetonate;
	const bool bMatchesQuotaWindow = DoesCurrentSliceEstimateLandInQuotaWindow();
	const float EstSeveredKg = FMath::Max(0.0f, CachedPreviewTelemetry.EstimatedSeveredMassKg);
	const float ProjectedTareKg = FMath::Max(0.0f, CachedHudSnapshot.AccumulatedTareWasteKg) + EstSeveredKg;
	const float MaxTareKg = FMath::Max(0.0f, CachedHudSnapshot.MaxAllowedTareWasteKg);
	const bool bTareOverflowRisk = ProjectedTareKg > (MaxTareKg + QuotaFloatEpsilonKg);

	FLinearColor BaseHue = SafeIdleColor;
	bool bHighlightReadySheet = bMatchesQuotaWindow;

	if (bDirectVeinHit)
	{
		BaseHue = DangerCrimsonColor;
	}
	else if (bValidContact)
	{
		if (CachedPreviewTelemetry.bSeveredHalfContainsVein)
		{
			if (CachedHudSnapshot.ActiveDiverterRoute == EDiverterRoute::TitheScale)
			{
				const float DangerAlpha = FMath::Clamp((CachedVeinProximityNormalized - 0.5f) * 2.0f, 0.0f, 1.0f);
				BaseHue = FMath::Lerp(WarningAmberColor, DangerCrimsonColor, DangerAlpha);
			}
			else if (bTareOverflowRisk)
			{
				BaseHue = DangerCrimsonColor;
			}
			else
			{
				const float CriticalVeinAlpha = FMath::Clamp((CachedVeinProximityNormalized - 0.65f) / 0.35f, 0.0f, 1.0f);
				BaseHue = FMath::Lerp(TarePurgeReadyColor, DangerCrimsonColor, CriticalVeinAlpha);
				bHighlightReadySheet = (CriticalVeinAlpha <= KINDA_SMALL_NUMBER);
			}
		}
		else if (CachedHudSnapshot.ActiveDiverterRoute == EDiverterRoute::TareChute && bTareOverflowRisk)
		{
			BaseHue = DangerCrimsonColor;
		}
		else
		{
			const FLinearColor CleanContactHue = bMatchesQuotaWindow ? WindowMatchEmeraldColor : IntersectCyanColor;
			if (CachedVeinProximityNormalized <= 0.0f)
			{
				BaseHue = CleanContactHue;
			}
			else if (CachedVeinProximityNormalized < 0.5f)
			{
				BaseHue = FMath::Lerp(CleanContactHue, WarningAmberColor, CachedVeinProximityNormalized * 2.0f);
			}
			else
			{
				BaseHue = FMath::Lerp(WarningAmberColor, DangerCrimsonColor, (CachedVeinProximityNormalized - 0.5f) * 2.0f);
			}
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

	const float SheetOpacity = (bValidContact || bDirectVeinHit)
		? (bHighlightReadySheet ? 0.48f : 0.42f)
		: 0.18f;
	ApplyParameters(LaserSheetMID, SheetOpacity);
	ApplyParameters(LaserFrameMID, 1.0f);
}

bool AInspectorPawn::HasValidPredictiveSliceContact() const
{
	return bCachedIntersectsMonolith &&
		CachedPreviewTelemetry.bHasValidMassEstimate &&
		(CachedPreviewTelemetry.EstimatedSeveredMassKg > MinPredictiveContactMassKg);
}

bool AInspectorPawn::DoesCurrentSliceEstimateLandInQuotaWindow() const
{
	if (!HasValidPredictiveSliceContact() ||
		CachedPreviewTelemetry.bWillDetonate ||
		CachedPreviewTelemetry.bSeveredHalfContainsVein ||
		CachedHudSnapshot.ActiveDiverterRoute != EDiverterRoute::TitheScale)
	{
		return false;
	}

	constexpr float QuotaFloatEpsilonKg = 1.0e-3f;
	const float ProjectedScaleKg =
		FMath::Max(0.0f, CachedHudSnapshot.SettledScaleMassKg) +
		FMath::Max(0.0f, CachedHudSnapshot.InFlightMassKg) +
		FMath::Max(0.0f, CachedPreviewTelemetry.EstimatedSeveredMassKg);
	const float TargetKg = FMath::Max(0.0f, CachedHudSnapshot.TargetTitheMassKg);
	const float ToleranceKg = FMath::Max(0.0f, CachedHudSnapshot.QuotaToleranceKg);
	const float MinWinKg = FMath::Max(0.0f, TargetKg - ToleranceKg);
	const float MaxWinKg = TargetKg + ToleranceKg;

	return (ProjectedScaleKg >= (MinWinKg - QuotaFloatEpsilonKg)) &&
	       (ProjectedScaleKg <= (MaxWinKg + QuotaFloatEpsilonKg));
}

void AInspectorPawn::UpdateCameraParallaxFromAim()
{
	if (!IsValid(CameraViewPivot) || !IsValid(LaserPlaneAimComponent))
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

	// Subtle baseline downward look plus clamped aim-tracking parallax applied strictly to CameraViewPivot, never CameraShakeOffsetComponent.
	CameraViewPivot->SetRelativeRotation(FRotator(FinalPitchDeg, TargetYawDeg, 0.0f));
}

bool AInspectorPawn::IsLegacyWorldSpaceReadoutEnabled() const
{
	return IsValid(PresentationData) && PresentationData->HudConfig.bEnableLegacyWorldSpaceReadouts;
}

void AInspectorPawn::UpdateLegacyReticleVisibility()
{
	const bool bShowLegacy = IsLegacyWorldSpaceReadoutEnabled() && (CachedShiftPhase == EShiftPhase::ActiveInspection);
	UTextRenderComponent* const ReticleLines[4] = {
		LaserReticleReadout,
		LaserReticleYieldReadout,
		LaserReticleLoadReadout,
		LaserReticleHazardReadout
	};
	for (UTextRenderComponent* LineComp : ReticleLines)
	{
		if (IsValid(LineComp))
		{
			LineComp->SetVisibility(bShowLegacy, false);
			LineComp->SetHiddenInGame(!bShowLegacy, false);
		}
	}
}

bool AInspectorPawn::ProjectSteadyWorldAnchorToNormalizedUV(
	const FVector& SteadyWorldAnchor,
	FVector2D& OutNormalizedUV) const
{
	const FTransform SteadyCamTransform = (IsValid(InspectorCamera) && IsValid(CameraViewPivot))
		? (InspectorCamera->GetRelativeTransform() * CameraViewPivot->GetComponentTransform())
		: (IsValid(CameraViewPivot) ? CameraViewPivot->GetComponentTransform() : GetActorTransform());

	UWorld* World = GetWorld();
	UGameViewportClient* ViewportClient = IsValid(World) ? World->GetGameViewport() : nullptr;
	FViewport* Viewport = IsValid(ViewportClient) ? ViewportClient->Viewport : nullptr;

	if (Viewport != nullptr)
	{
		const FIntPoint ViewportSize = Viewport->GetSizeXY();
		if (ViewportSize.X > 0 && ViewportSize.Y > 0)
		{
			FMinimalViewInfo ViewInfo;
			if (IsValid(InspectorCamera))
			{
				InspectorCamera->GetCameraView(0.0f, ViewInfo);
			}
			else
			{
				ViewInfo.FOV = DefaultCameraFieldOfView;
			}
			ViewInfo.Location = SteadyCamTransform.GetLocation();
			ViewInfo.Rotation = SteadyCamTransform.Rotator();

			FSceneViewProjectionData ProjectionData;
			ProjectionData.SetViewRectangle(FIntRect(0, 0, ViewportSize.X, ViewportSize.Y));
			ProjectionData.ViewOrigin = ViewInfo.Location;
			ProjectionData.ViewRotationMatrix = FInverseRotationMatrix(ViewInfo.Rotation) * FMatrix(
				FPlane(0.0, 0.0, 1.0, 0.0),
				FPlane(1.0, 0.0, 0.0, 0.0),
				FPlane(0.0, 1.0, 0.0, 0.0),
				FPlane(0.0, 0.0, 0.0, 1.0));

			TEnumAsByte<EAspectRatioAxisConstraint> AspectConstraint = AspectRatio_MaintainXFOV;
			if (const APlayerController* PC = GetController<APlayerController>())
			{
				if (const ULocalPlayer* LP = PC->GetLocalPlayer())
				{
					AspectConstraint = LP->AspectRatioAxisConstraint;
				}
			}

			FMinimalViewInfo::CalculateProjectionMatrixGivenView(
				ViewInfo,
				AspectConstraint,
				Viewport,
				ProjectionData);

			FVector2D ScreenPixelPos = FVector2D::ZeroVector;
			if (FSceneView::ProjectWorldToScreen(
				SteadyWorldAnchor,
				ProjectionData.GetConstrainedViewRect(),
				ProjectionData.ComputeViewProjectionMatrix(),
				ScreenPixelPos))
			{
				OutNormalizedUV.X = FMath::Clamp(ScreenPixelPos.X / static_cast<double>(ViewportSize.X), 0.0, 1.0);
				OutNormalizedUV.Y = FMath::Clamp(ScreenPixelPos.Y / static_cast<double>(ViewportSize.Y), 0.0, 1.0);
				return true;
			}
			return false;
		}
	}

	// Pre-viewport-init analytical fallback (strictly unshaken steady camera space).
	const FVector RelCam = SteadyCamTransform.InverseTransformPositionNoScale(SteadyWorldAnchor);
	if (RelCam.X <= 1.0)
	{
		return false;
	}

	const double FovDeg = IsValid(InspectorCamera)
		? static_cast<double>(InspectorCamera->FieldOfView)
		: static_cast<double>(DefaultCameraFieldOfView);
	const double TanHalfHoriz = FMath::Max(0.1, FMath::Tan(FMath::DegreesToRadians(0.5 * FovDeg)));
	const double TanHalfVert = TanHalfHoriz * (9.0 / 16.0);

	const double NdcX = RelCam.Y / (RelCam.X * TanHalfHoriz);
	const double NdcY = RelCam.Z / (RelCam.X * TanHalfVert);
	OutNormalizedUV.X = FMath::Clamp(0.5 + (0.5 * NdcX), 0.0, 1.0);
	OutNormalizedUV.Y = FMath::Clamp(0.5 - (0.5 * NdcY), 0.0, 1.0);
	return true;
}

void AInspectorPawn::UpdateLaserReticleTransform()
{
	if (!IsValid(LaserPlaneAimComponent))
	{
		return;
	}

	const FVector FrameAnchor = LaserPlaneAimComponent->GetFrameAnchorWorld();
	const FVector FrameForward = LaserPlaneAimComponent->GetFrameForwardUnit();
	const FVector FrameRight = LaserPlaneAimComponent->GetFrameRightUnit();
	const FVector PlaneOrigin = LaserPlaneAimComponent->GetCurrentPlaneOriginWorld();
	const FQuat PlaneOrient = LaserPlaneAimComponent->GetCurrentPlaneOrientationWorld();

	// Dynamically keep reticle readout clear of both world and perspective-projected right-most edges of the 3D laser frame.
	const double HalfSpanDepthCm = 0.5 * (
		FMath::Max(20.0, static_cast<double>(LaserFrameDepthCm)) +
		FMath::Max(0.2, static_cast<double>(LaserFrameBarWidthCm)));
	const double HalfSpanWidthCm = 0.5 * (
		FMath::Max(20.0, static_cast<double>(LaserFrameWidthCm)) +
		FMath::Max(0.2, static_cast<double>(LaserFrameBarWidthCm)));

	const FVector ReticlePlanePoint = FrameAnchor - (FrameForward * static_cast<double>(LaserReticleFrontOffsetCm));
	const FTransform SteadyCamTransform = (IsValid(InspectorCamera) && IsValid(CameraViewPivot))
		? (InspectorCamera->GetRelativeTransform() * CameraViewPivot->GetComponentTransform())
		: (IsValid(CameraViewPivot) ? CameraViewPivot->GetComponentTransform() : GetActorTransform());
	const FVector CamWorldLoc = SteadyCamTransform.GetLocation();
	const double DepthReticleFromCam = FMath::Max(20.0, FVector::DotProduct(ReticlePlanePoint - CamWorldLoc, FrameForward));
	const double CamRightOffsetFromAnchor = FVector::DotProduct(CamWorldLoc - FrameAnchor, FrameRight);

	const FVector AxisU = PlaneOrient.GetAxisX();
	const FVector AxisV = PlaneOrient.GetAxisY();
	double MaxSheetRightAtReticleCm = FVector::DotProduct(PlaneOrigin - FrameAnchor, FrameRight);

	for (int32 SignU = -1; SignU <= 1; SignU += 2)
	{
		for (int32 SignV = -1; SignV <= 1; SignV += 2)
		{
			const FVector CornerWorld =
				PlaneOrigin +
				(AxisU * (static_cast<double>(SignU) * HalfSpanDepthCm)) +
				(AxisV * (static_cast<double>(SignV) * HalfSpanWidthCm));

			const double WorldRightOffsetCm = FVector::DotProduct(CornerWorld - FrameAnchor, FrameRight);
			const double DepthCornerFromCam = FMath::Max(20.0, FVector::DotProduct(CornerWorld - CamWorldLoc, FrameForward));
			const double CornerRightFromCam = FVector::DotProduct(CornerWorld - CamWorldLoc, FrameRight);
			const double PerspRightAtReticleCm =
				CamRightOffsetFromAnchor + (CornerRightFromCam * (DepthReticleFromCam / DepthCornerFromCam));

			MaxSheetRightAtReticleCm = FMath::Max3(MaxSheetRightAtReticleCm, WorldRightOffsetCm, PerspRightAtReticleCm);
		}
	}

	const double MinClearLateralCm =
		MaxSheetRightAtReticleCm +
		static_cast<double>(LaserReticleSheetClearanceGapCm);
	const double BaseLateralCm = static_cast<double>(LaserReticleLateralOffsetCm);
	const double MaxLateralCm = FMath::Max(BaseLateralCm, static_cast<double>(LaserReticleMaxLateralOffsetCm));
	const double EffectiveLateralOffsetCm = FMath::Clamp(FMath::Max(BaseLateralCm, MinClearLateralCm), BaseLateralCm, MaxLateralCm);

	FVector ReticleWorldLocation = ReticlePlanePoint + (FrameRight * EffectiveLateralOffsetCm);
	ReticleWorldLocation.Z = PlaneOrigin.Z;

	if (IsLegacyWorldSpaceReadoutEnabled() && IsValid(LaserReticleReadout))
	{
		FRotator ReticleWorldRotation = FRotationMatrix::MakeFromX(-FrameForward).Rotator();
		ReticleWorldRotation.Pitch = 0.0f;
		ReticleWorldRotation.Roll = 0.0f;

		LaserReticleReadout->SetWorldLocationAndRotation(ReticleWorldLocation, ReticleWorldRotation);
		LaserReticleReadout->SetWorldScale3D(FVector::OneVector);
	}

	const float UvDeadband = IsValid(PresentationData)
		? FMath::Max(0.0001f, PresentationData->HudConfig.ReticleNormalizedUvDeadband)
		: 0.001f;
	const bool bActivePhase = (CachedShiftPhase == EShiftPhase::ActiveInspection);
	FVector2D NormalizedUV = LastBroadcastReticleUV;
	const bool bProjected = bActivePhase && ProjectSteadyWorldAnchorToNormalizedUV(ReticleWorldLocation, NormalizedUV);
	const bool bShouldShow = bActivePhase && bProjected;

	const bool bVisibilityChanged = (bShouldShow != bLastBroadcastReticleVisible);
	const bool bMovedPastDeadband =
		(FMath::Abs(NormalizedUV.X - LastBroadcastReticleUV.X) >= static_cast<double>(UvDeadband)) ||
		(FMath::Abs(NormalizedUV.Y - LastBroadcastReticleUV.Y) >= static_cast<double>(UvDeadband));

	if (bVisibilityChanged || (bShouldShow && bMovedPastDeadband))
	{
		LastBroadcastReticleUV = NormalizedUV;
		bLastBroadcastReticleVisible = bShouldShow;
		OnReticleProjectionUpdated.Broadcast(NormalizedUV, bShouldShow, ReticleWorldLocation);
	}
}

void AInspectorPawn::UpdateLaserReticleReadoutText()
{
	if (!IsLegacyWorldSpaceReadoutEnabled())
	{
		return;
	}

	if (IsValid(LaserReticleReadout))
	{
		constexpr float QuotaFloatEpsilonKg = 1.0e-3f;
		const float SettledKg = FMath::Max(0.0f, CachedHudSnapshot.SettledScaleMassKg);
		const float TargetKg = FMath::Max(0.0f, CachedHudSnapshot.TargetTitheMassKg);
		const float ToleranceKg = FMath::Max(0.0f, CachedHudSnapshot.QuotaToleranceKg);

		if (CachedHudSnapshot.ActiveDiverterRoute == EDiverterRoute::TareChute)
		{
			LaserReticleReadout->SetTextRenderColor(ReticleAmberColor);
			LaserReticleReadout->SetText(FText::FromString(TEXT("ROUTE: TARE SCRAP [SPACE]")));
		}
		else if (SettledKg > (TargetKg + ToleranceKg + QuotaFloatEpsilonKg))
		{
			LaserReticleReadout->SetTextRenderColor(ReticleCrimsonColor);
			LaserReticleReadout->SetText(FText::FromString(TEXT("OVER QUOTA!")));
		}
		else if (CachedHudSnapshot.bIsWithinDirectorsCutWindow)
		{
			LaserReticleReadout->SetTextRenderColor(ReticleGoldColor);
			LaserReticleReadout->SetText(
				FText::FromString(FString::Printf(TEXT("DIRECTOR'S CUT [E] +2 SCRIP (%.2f KG)"), SettledKg)));
		}
		else if (CachedHudSnapshot.bIsWithinQuotaWindow ||
			SettledKg >= (TargetKg - ToleranceKg - QuotaFloatEpsilonKg))
		{
			LaserReticleReadout->SetTextRenderColor(ReticleEmeraldColor);
			LaserReticleReadout->SetText(
				FText::FromString(FString::Printf(TEXT("STAMP READY [E] +1 SCRIP (%.2f KG)"), SettledKg)));
		}
		else
		{
			const float NeededDeltaKg = FMath::Max(0.01f, TargetKg - SettledKg);
			LaserReticleReadout->SetTextRenderColor(ReticleCyanColor);
			LaserReticleReadout->SetText(
				FText::FromString(FString::Printf(TEXT("TITHE | NEED +%.2f KG"), NeededDeltaKg)));
		}
	}

	const bool bValidContact = HasValidPredictiveSliceContact();
	const bool bDirectVeinHit = bCachedIntersectsMonolith && CachedPreviewTelemetry.bWillDetonate;

	if (IsValid(LaserReticleYieldReadout))
	{
		if (!bValidContact && !bDirectVeinHit)
		{
			LaserReticleYieldReadout->SetTextRenderColor(ReticleLoadCellColor);
			LaserReticleYieldReadout->SetText(FText::FromString(TEXT("CUT EST: --- (NO CONTACT)")));
		}
		else
		{
			constexpr float QuotaFloatEpsilonKg = 1.0e-3f;
			const float EstSeveredKg = FMath::Max(0.0f, CachedPreviewTelemetry.EstimatedSeveredMassKg);
			const float EstRemainderKg = FMath::Max(0.0f, CachedPreviewTelemetry.EstimatedRemainderMassKg);
			const float ProjectedTareKg = FMath::Max(0.0f, CachedHudSnapshot.AccumulatedTareWasteKg) + EstSeveredKg;
			const float MaxTareKg = FMath::Max(0.0f, CachedHudSnapshot.MaxAllowedTareWasteKg);
			const bool bTareOverflow = ProjectedTareKg > (MaxTareKg + QuotaFloatEpsilonKg);

			if (bDirectVeinHit)
			{
				LaserReticleYieldReadout->SetTextRenderColor(ReticleCrimsonColor);
				LaserReticleYieldReadout->SetText(
					FText::FromString(FString::Printf(TEXT("CUT EST: +%.2f KG [VEIN INTERSECT]"), EstSeveredKg)));
			}
			else if (CachedPreviewTelemetry.bSeveredHalfContainsVein)
			{
				if (CachedHudSnapshot.ActiveDiverterRoute == EDiverterRoute::TitheScale)
				{
					LaserReticleYieldReadout->SetTextRenderColor(ReticleVeinDropWarningColor);
					LaserReticleYieldReadout->SetText(
						FText::FromString(FString::Printf(TEXT("CUT EST: +%.2f KG | VEIN IN DROP! [SPACE] -> TARE"), EstSeveredKg)));
				}
				else if (bTareOverflow)
				{
					LaserReticleYieldReadout->SetTextRenderColor(ReticleCrimsonColor);
					LaserReticleYieldReadout->SetText(
						FText::FromString(FString::Printf(TEXT("SCRAP EST: %.2f KG [TARE OVERFLOW RISK!]"), EstSeveredKg)));
				}
				else
				{
					LaserReticleYieldReadout->SetTextRenderColor(ReticlePurgeReadyColor);
					LaserReticleYieldReadout->SetText(
						FText::FromString(FString::Printf(TEXT("SCRAP EST: %.2f KG [TARE PURGE READY]"), EstSeveredKg)));
				}
			}
			else if (CachedHudSnapshot.ActiveDiverterRoute == EDiverterRoute::TareChute)
			{
				if (bTareOverflow)
				{
					LaserReticleYieldReadout->SetTextRenderColor(ReticleCrimsonColor);
					LaserReticleYieldReadout->SetText(
						FText::FromString(FString::Printf(TEXT("SCRAP EST: %.2f KG [TARE OVERFLOW RISK!]"), EstSeveredKg)));
				}
				else
				{
					LaserReticleYieldReadout->SetTextRenderColor(ReticleAmberColor);
					LaserReticleYieldReadout->SetText(
						FText::FromString(FString::Printf(TEXT("SCRAP EST: %.2f KG (LEFT %.1f KG)"), EstSeveredKg, EstRemainderKg)));
				}
			}
			else
			{
				const float SettledKg = FMath::Max(0.0f, CachedHudSnapshot.SettledScaleMassKg);
				const float InFlightKg = FMath::Max(0.0f, CachedHudSnapshot.InFlightMassKg);
				const float TargetKg = FMath::Max(0.0f, CachedHudSnapshot.TargetTitheMassKg);
				const float ToleranceKg = FMath::Max(0.0f, CachedHudSnapshot.QuotaToleranceKg);
				const float MinWinKg = FMath::Max(0.0f, TargetKg - ToleranceKg);
				const float MaxWinKg = TargetKg + ToleranceKg;
				const float ProjectedScaleKg = SettledKg + InFlightKg + EstSeveredKg;

				if (ProjectedScaleKg > (MaxWinKg + QuotaFloatEpsilonKg))
				{
					LaserReticleYieldReadout->SetTextRenderColor(ReticleCrimsonColor);
					LaserReticleYieldReadout->SetText(
						FText::FromString(FString::Printf(TEXT("CUT EST: +%.2f KG -> %.2f [OVER!]"), EstSeveredKg, ProjectedScaleKg)));
				}
				else if (ProjectedScaleKg >= (MinWinKg - QuotaFloatEpsilonKg))
				{
					const float DirCutTolKg = FMath::Max(0.15f, CachedHudSnapshot.DirectorsCutToleranceKg);
					if (FMath::Abs(ProjectedScaleKg - TargetKg) <= (DirCutTolKg + QuotaFloatEpsilonKg))
					{
						LaserReticleYieldReadout->SetTextRenderColor(ReticleGoldColor);
						LaserReticleYieldReadout->SetText(
							FText::FromString(FString::Printf(TEXT("CUT EST: +%.2f KG [DIRECTOR'S CUT!]"), EstSeveredKg)));
					}
					else
					{
						LaserReticleYieldReadout->SetTextRenderColor(ReticleEmeraldColor);
						LaserReticleYieldReadout->SetText(
							FText::FromString(FString::Printf(TEXT("CUT EST: +%.2f KG [IN WINDOW!]"), EstSeveredKg)));
					}
				}
				else
				{
					const float StillNeedKg = FMath::Max(0.01f, TargetKg - ProjectedScaleKg);
					LaserReticleYieldReadout->SetTextRenderColor(ReticleCyanColor);
					LaserReticleYieldReadout->SetText(
						FText::FromString(FString::Printf(TEXT("CUT EST: +%.2f KG (NEED +%.2f)"), EstSeveredKg, StillNeedKg)));
				}
			}
		}
	}

	if (IsValid(LaserReticleLoadReadout))
	{
		const float ClampLoadKg = FMath::Max(0.0f, CachedHudSnapshot.RemainderMassKg);
		LaserReticleLoadReadout->SetTextRenderColor(ReticleLoadCellColor);
		LaserReticleLoadReadout->SetText(
			FText::FromString(FString::Printf(TEXT("CLAMP LOAD: %.1f KG"), ClampLoadKg)));
	}

	if (IsValid(LaserReticleHazardReadout))
	{
		if (bDirectVeinHit)
		{
			LaserReticleHazardReadout->SetTextRenderColor(ReticleCrimsonColor);
			LaserReticleHazardReadout->SetText(FText::FromString(TEXT("!! VEIN INTERSECT !!")));
		}
		else if (CachedVeinProximityNormalized > LaserReticleVeinWarningThreshold)
		{
			LaserReticleHazardReadout->SetTextRenderColor(ReticleCrimsonColor);
			LaserReticleHazardReadout->SetText(FText::FromString(TEXT("!! VOLATILE VEIN !!")));
		}
		else if (bValidContact &&
			CachedPreviewTelemetry.bSeveredHalfContainsVein &&
			CachedHudSnapshot.ActiveDiverterRoute == EDiverterRoute::TitheScale)
		{
			LaserReticleHazardReadout->SetTextRenderColor(ReticleVeinDropWarningColor);
			LaserReticleHazardReadout->SetText(FText::FromString(TEXT("!! VEIN IN DROP -> [SPACE] TARE !!")));
		}
		else
		{
			const float EffectiveStrain = FMath::Max(CachedClampStrainNormalized, CachedHudSnapshot.ClampStrainNormalized);
			if (EffectiveStrain >= LaserReticleClampTorqueCriticalThreshold)
			{
				const int32 TorquePercent = FMath::Clamp(FMath::RoundToInt(EffectiveStrain * 100.0f), 0, 999);
				LaserReticleHazardReadout->SetTextRenderColor(ReticleCrimsonColor);
				LaserReticleHazardReadout->SetText(
					FText::FromString(FString::Printf(TEXT("!! JAW SHEAR CRITICAL (%d%%) - BALANCE CUT !!"), TorquePercent)));
			}
			else if (EffectiveStrain >= LaserReticleClampTorqueWarningThreshold)
			{
				const int32 TorquePercent = FMath::Clamp(FMath::RoundToInt(EffectiveStrain * 100.0f), 0, 999);
				LaserReticleHazardReadout->SetTextRenderColor(ReticleAmberColor);
				LaserReticleHazardReadout->SetText(
					FText::FromString(FString::Printf(TEXT("JAW TORQUE: %d%% - BALANCE CUT"), TorquePercent)));
			}
			else
			{
				LaserReticleHazardReadout->SetText(FText::GetEmpty());
			}
		}
	}
}

void AInspectorPawn::HandleContractSnapshotUpdated(const FContractRuntimeSnapshot& Snapshot)
{
	const uint8 PrevScannerLevel = CachedHudSnapshot.VeinScannerDamperLevel;
	CachedHudSnapshot = Snapshot;
	if (Snapshot.VeinScannerDamperLevel != PrevScannerLevel &&
		IsValid(LaserPlaneAimComponent) &&
		LaserPlaneAimComponent->IsAimingEnabled())
	{
		LaserPlaneAimComponent->ForceRefreshPreview();
	}
	UpdateLaserSheetMaterialVisuals();
	UpdateLaserReticleReadoutText();
}

void AInspectorPawn::HandleFilteredTelemetryStream(ETelemetryStreamId StreamId, float NormalizedValue)
{
	if (StreamId == ETelemetryStreamId::ClampStrain)
	{
		CachedClampStrainNormalized = FMath::Clamp(NormalizedValue, 0.0f, 1.0f);
		UpdateLaserReticleReadoutText();
	}
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

	const float TargetSharpen = IsValid(PresentationData)
		? FMath::Clamp(PresentationData->CameraPostProcessConfig.TonemapperSharpen, 0.0f, 1.0f)
		: FCameraPostProcessPolishConfig().TonemapperSharpen;
	if (IConsoleVariable* TonemapSharpenCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Tonemapper.Sharpen")))
	{
		TonemapSharpenCVar->Set(TargetSharpen, ECVF_SetByGameSetting);
	}
}

bool AInspectorPawn::HandleQueryLaserSliceTarget(
	UObject*& OutSliceableTarget,
	FVector& OutPlaneOriginWorld,
	FVector& OutPlaneNormalWorld,
	FLaserSliceAperture& OutSheetAperture)
{
	if (!IsValid(LaserPlaneAimComponent) || !LaserPlaneAimComponent->IsAimingEnabled())
	{
		return false;
	}

	if (!LaserPlaneAimComponent->IsChargePlaneLocked())
	{
		LaserPlaneAimComponent->ForceRefreshPreview();
	}

	OutSliceableTarget = LaserPlaneAimComponent->GetSliceableTarget();
	OutPlaneOriginWorld = LaserPlaneAimComponent->GetCurrentPlaneOriginWorld();
	OutPlaneNormalWorld = LaserPlaneAimComponent->GetCurrentPlaneNormalWorld();
	OutSheetAperture = LaserPlaneAimComponent->GetCurrentSliceAperture();
	return IsValid(OutSliceableTarget) && LaserPlaneAimComponent->GetLastPreviewTelemetry().bIntersectsMonolith;
}

void AInspectorPawn::HandlePreviewTelemetryUpdated(const FSlicePreviewTelemetry& PreviewTelemetry)
{
	CachedPreviewTelemetry = PreviewTelemetry;
	bCachedIntersectsMonolith = PreviewTelemetry.bIntersectsMonolith;
	CachedVeinProximityNormalized = PreviewTelemetry.VeinProximityNormalized;

	if (!PreviewTelemetry.bIntersectsMonolith &&
		IsValid(LaserCapacitorComponent) &&
		LaserCapacitorComponent->IsCharging())
	{
		LaserCapacitorComponent->CancelCharge();
	}

	UpdateCameraParallaxFromAim();
	UpdateLaserSheetMaterialVisuals();
	UpdateLaserReticleTransform();
	UpdateLaserReticleReadoutText();
}

void AInspectorPawn::HandleCapacitorChargeStateChanged(bool bIsNowCharging)
{
	if (IsValid(LaserPlaneAimComponent))
	{
		LaserPlaneAimComponent->SetChargePlaneLocked(bIsNowCharging);
	}
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

namespace
{
	void ApplySpawnHazardSafeRestingAim(
		ULaserPlaneAimComponent* AimComp,
		UWorld* World,
		const UMonolithContractDataAsset* ExplicitContract = nullptr)
	{
		if (!IsValid(AimComp) || !IsValid(World))
		{
			return;
		}

		const UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>();
		const UMonolithContractDataAsset* Contract = IsValid(ExplicitContract)
			? ExplicitContract
			: (IsValid(ShiftSubsystem) ? ShiftSubsystem->GetActiveContract() : nullptr);
		if (!IsValid(Contract) || Contract->HiddenVeins.Num() == 0)
		{
			return;
		}

		bool bMidpointRisksVeinHazard = false;
		for (const FVolatileVeinDefinition& Vein : Contract->HiddenVeins)
		{
			const double MinVeinLocalZ = FMath::Min(Vein.LocalStartCm.Z, Vein.LocalEndCm.Z);
			const double SafeClearanceCm = static_cast<double>(FMath::Max(2.0f, Vein.RadiusCm + Vein.WarningRangeCm));
			if (MinVeinLocalZ <= SafeClearanceCm)
			{
				bMidpointRisksVeinHazard = true;
				break;
			}
		}

		if (!bMidpointRisksVeinHazard)
		{
			return;
		}

		UObject* TargetObj = IsValid(ShiftSubsystem) ? ShiftSubsystem->GetRegisteredSliceableTargetObject() : nullptr;
		if (!IsValid(TargetObj) || !TargetObj->Implements<USliceableTargetInterface>())
		{
			return;
		}

		const FBox TargetBounds = ISliceableTargetInterface::Execute_GetTargetWorldBounds(TargetObj);
		if (!TargetBounds.IsValid)
		{
			return;
		}

		// Refinement B: Park initial resting laser plane 5.0 cm cleanly below the monolith bottom flange when a lower/mid vein is present
		// so neutral spawn never trips P1_VeinInDrop or P1_IsotopeHazard before the operator moves the mouse.
		const FVector FrameAnchor = AimComp->GetFrameAnchorWorld();
		const FVector FrameForward = AimComp->GetFrameForwardUnit();
		const double SafeRestingWorldZ = TargetBounds.Min.Z - 5.0;
		const FVector RayOrigin = FrameAnchor - (FrameForward * 100.0) + FVector(0.0, 0.0, SafeRestingWorldZ - FrameAnchor.Z);
		AimComp->SetAimFromWorldRay(RayOrigin, FrameForward);
	}
}

void AInspectorPawn::HandleShiftPhaseChanged(EShiftPhase NewPhase)
{
	CachedShiftPhase = NewPhase;
	const bool bActiveInspection = (NewPhase == EShiftPhase::ActiveInspection);

	if (!bActiveInspection && IsValid(LaserCapacitorComponent))
	{
		LaserCapacitorComponent->ResetCapacitorState();
	}

	if (IsValid(LaserPlaneAimComponent))
	{
		if (bActiveInspection)
		{
			LaserPlaneAimComponent->ResetAimAndOrientation();
			RefreshAimTargetAndFrameFromShift();
			ApplySpawnHazardSafeRestingAim(LaserPlaneAimComponent, GetWorld());
		}
		LaserPlaneAimComponent->SetAimingEnabled(bActiveInspection);
	}

	UpdateLegacyReticleVisibility();
	if (bActiveInspection)
	{
		LastBroadcastReticleUV = FVector2D(-1.0f, -1.0f);
		UpdateLaserReticleTransform();
		UpdateLaserReticleReadoutText();
	}
	else if (bLastBroadcastReticleVisible)
	{
		bLastBroadcastReticleVisible = false;
		OnReticleProjectionUpdated.Broadcast(LastBroadcastReticleUV, false, FVector::ZeroVector);
	}
}

void AInspectorPawn::HandleActiveContractLoaded(
	const UMonolithContractDataAsset* Contract,
	const UShiftProgressionDataAsset* ShiftConfig,
	int32 /*ContractIndex*/)
{
	bool bActiveInspectionPhase = true;
	if (UWorld* World = GetWorld())
	{
		if (const UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			CachedHudSnapshot = ShiftSubsystem->GetCurrentSnapshot();
			CachedShiftPhase = ShiftSubsystem->GetCurrentPhase();
			bActiveInspectionPhase = (CachedShiftPhase == EShiftPhase::ActiveInspection);
		}
	}
	LastBroadcastReticleUV = FVector2D(-1.0f, -1.0f);

	if (IsValid(LaserCapacitorComponent))
	{
		LaserCapacitorComponent->ConfigureFromShiftProgression(ShiftConfig);
		LaserCapacitorComponent->ResetCapacitorState();
	}

	if (IsValid(LaserPlaneAimComponent))
	{
		LaserPlaneAimComponent->SetAimingEnabled(false);
		LaserPlaneAimComponent->ResetAimAndOrientation();
	}

	if (IsValid(InspectorKinestheticsComponent))
	{
		InspectorKinestheticsComponent->ResetKinestheticsImmediate();
	}

	if (IsValid(AudioTelemetryBridgeComponent))
	{
		CachedClampStrainNormalized = FMath::Clamp(
			AudioTelemetryBridgeComponent->GetLastFlushedStreamValue(ETelemetryStreamId::ClampStrain),
			0.0f,
			1.0f);
	}
	else
	{
		CachedClampStrainNormalized = 0.0f;
	}

	RefreshAimTargetAndFrameFromShift();
	if (IsValid(LaserPlaneAimComponent))
	{
		ApplySpawnHazardSafeRestingAim(LaserPlaneAimComponent, GetWorld(), Contract);
		LaserPlaneAimComponent->SetAimingEnabled(bActiveInspectionPhase);
		if (bActiveInspectionPhase)
		{
			LaserPlaneAimComponent->ForceRefreshPreview();
		}
	}
	UpdateCameraParallaxFromAim();
	UpdateLegacyReticleVisibility();
	UpdateLaserSheetMaterialVisuals();
	UpdateLaserReticleTransform();
	UpdateLaserReticleReadoutText();
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

void AInspectorPawn::InjectAimCursorDeltaForTest(FVector2D MouseDelta)
{
	if (IsValid(LaserPlaneAimComponent) && LaserPlaneAimComponent->IsAimingEnabled())
	{
		LaserPlaneAimComponent->AddAimCursorDelta(MouseDelta);
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
			ShiftSubsystem->TryConfirmOrStampAction();
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

void AInspectorPawn::Input_RequisitionHydraulics(const FInputActionValue& /*Value*/)
{
	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->TryPurchaseRequisition(ERequisitionTrack::ClampHydraulics);
		}
	}
}

void AInspectorPawn::Input_RequisitionTare(const FInputActionValue& /*Value*/)
{
	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->TryPurchaseRequisition(ERequisitionTrack::TareHopper);
		}
	}
}

void AInspectorPawn::Input_RequisitionScanner(const FInputActionValue& /*Value*/)
{
	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->TryPurchaseRequisition(ERequisitionTrack::VeinScannerDamper);
		}
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
