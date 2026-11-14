// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/TareTitheGameMode.h"
#include "Actors/CellFramingArchitectureActor.h"
#include "Actors/DirectorBoothActor.h"
#include "Actors/InspectorPawn.h"
#include "Actors/MonolithRigActor.h"
#include "Actors/ScaleAssemblyActor.h"
#include "Actors/StrayFloorCatchActor.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/ShiftProgressionDataAsset.h"
#include "Data/TareTithePresentationDataAsset.h"
#include "Engine/Engine.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PointLight.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/SkyLight.h"
#include "Engine/SpotLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"
#include "Misc/PackageName.h"
#include "Subsystems/InspectionShiftSubsystem.h"
#include "UObject/ConstructorHelpers.h"

ATareTitheGameMode::ATareTitheGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	DefaultPawnClass = AInspectorPawn::StaticClass();

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		GreyboxCellCubeMesh = CubeMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMeshFinder.Succeeded())
	{
		GreyboxCellCylinderMesh = CylinderMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlattenMatFinder(
		TEXT("/Engine/EngineMaterials/FlattenMaterial.FlattenMaterial"));
	if (FlattenMatFinder.Succeeded())
	{
		GreyboxCellBaseMaterial = FlattenMatFinder.Object;
	}
}

void ATareTitheGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	ResolveAndPublishPresentationAndProgression();
	ApplyBaseline6GbVramScalability();
}

void ATareTitheGameMode::StartPlay()
{
	ResolveAndPublishPresentationAndProgression();
	ApplyBaseline6GbVramScalability();

	Super::StartPlay();

	if (bSpawnGreyboxCellIfStationsMissing)
	{
		SpawnMissingGreyboxStationsIfNeeded();
	}
}

UInspectionShiftSubsystem* ATareTitheGameMode::GetInspectionShiftSubsystem(const UObject* WorldContextObject)
{
	if (!IsValid(WorldContextObject))
	{
		return nullptr;
	}
	UWorld* World = (GEngine != nullptr)
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: WorldContextObject->GetWorld();
	return IsValid(World) ? World->GetSubsystem<UInspectionShiftSubsystem>() : nullptr;
}

ASceneCapture2D* ATareTitheGameMode::SpawnTransientLitCapture2D(
	UCameraComponent* SourceCamera,
	UTextureRenderTarget2D* RenderTarget)
{
	if (!IsValid(SourceCamera) || !IsValid(RenderTarget))
	{
		return nullptr;
	}

	UWorld* World = SourceCamera->GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	const FVector CamLocation = SourceCamera->GetComponentLocation();
	const FRotator CamRotation = SourceCamera->GetComponentRotation();
	ASceneCapture2D* CaptureActor = World->SpawnActor<ASceneCapture2D>(
		ASceneCapture2D::StaticClass(),
		FTransform(CamRotation, CamLocation, FVector::OneVector),
		SpawnParams);
	if (!IsValid(CaptureActor))
	{
		return nullptr;
	}

	CaptureActor->PrimaryActorTick.bCanEverTick = false;
	CaptureActor->Tags.AddUnique(TEXT("Phase11AFixTransientLitCapture"));

	if (USceneCaptureComponent2D* CaptureComp = CaptureActor->GetCaptureComponent2D())
	{
		CaptureComp->PrimaryComponentTick.bCanEverTick = false;
		CaptureComp->SetMobility(EComponentMobility::Movable);
		CaptureComp->SetWorldLocationAndRotation(CamLocation, CamRotation);
		CaptureComp->ProjectionType = ECameraProjectionMode::Perspective;
		CaptureComp->FOVAngle = SourceCamera->FieldOfView;
		CaptureComp->TextureTarget = RenderTarget;
		CaptureComp->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		CaptureComp->bCaptureEveryFrame = true;
		CaptureComp->bCaptureOnMovement = false;
		CaptureComp->bAlwaysPersistRenderingState = true;
		CaptureComp->bUseRayTracingIfEnabled = true;
		CaptureComp->PostProcessBlendWeight = 1.0f;

		FPostProcessSettings CapturePP = SourceCamera->PostProcessSettings;
		CapturePP.bOverride_DynamicGlobalIlluminationMethod = true;
		CapturePP.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
		CapturePP.bOverride_ReflectionMethod = true;
		CapturePP.ReflectionMethod = EReflectionMethod::Lumen;
		CapturePP.bOverride_LumenSurfaceCacheResolution = true;
		CapturePP.LumenSurfaceCacheResolution = 1.0f;
		CapturePP.bOverride_LumenFinalGatherScreenTraces = true;
		CapturePP.LumenFinalGatherScreenTraces = 0;
		CapturePP.bOverride_LumenReflectionsScreenTraces = true;
		CapturePP.LumenReflectionsScreenTraces = 0;
		CapturePP.bOverride_LumenSceneLightingQuality = true;
		CapturePP.LumenSceneLightingQuality = 2.0f;
		CapturePP.bOverride_LumenFinalGatherQuality = true;
		CapturePP.LumenFinalGatherQuality = 2.0f;
		CaptureComp->PostProcessSettings = CapturePP;

		static const TCHAR* const EnabledFlags[] = {
			TEXT("Lighting"),
			TEXT("GlobalIllumination"),
			TEXT("LumenGlobalIllumination"),
			TEXT("LumenReflections"),
			TEXT("ReflectionEnvironment"),
			TEXT("SkyLighting"),
			TEXT("DynamicShadows"),
			TEXT("PostProcessing"),
			TEXT("Tonemapper"),
			TEXT("ToneCurve"),
			TEXT("EyeAdaptation"),
			TEXT("Fog"),
			TEXT("AntiAliasing"),
			TEXT("TemporalAA"),
		};
		TArray<FEngineShowFlagsSetting> FlagSettings;
		FlagSettings.Reserve(UE_ARRAY_COUNT(EnabledFlags));
		for (const TCHAR* FlagName : EnabledFlags)
		{
			FEngineShowFlagsSetting Entry;
			Entry.ShowFlagName = FlagName;
			Entry.Enabled = true;
			FlagSettings.Add(Entry);
		}
		CaptureComp->SetShowFlagSettings(FlagSettings);
		CaptureComp->ShowFlags.SetTemporalAA(true);
	}

	return CaptureActor;
}

void ATareTitheGameMode::ResolveAndPublishPresentationAndProgression()
{
	if (!IsValid(PresentationData) && FPackageName::DoesPackageExist(TEXT("/Game/Presentation/DA_Presentation")))
	{
		PresentationData = LoadObject<UTareTithePresentationDataAsset>(
			nullptr,
			TEXT("/Game/Presentation/DA_Presentation.DA_Presentation"));
	}

	UShiftProgressionDataAsset* ResolvedProgression = DefaultShiftProgression;
	if (!IsValid(ResolvedProgression) && IsValid(PresentationData) && IsValid(PresentationData->ShiftProgressionOverride))
	{
		ResolvedProgression = PresentationData->ShiftProgressionOverride;
	}
	if (!IsValid(ResolvedProgression) && FPackageName::DoesPackageExist(TEXT("/Game/Presentation/DA_ShiftProgression")))
	{
		ResolvedProgression = LoadObject<UShiftProgressionDataAsset>(
			nullptr,
			TEXT("/Game/Presentation/DA_ShiftProgression.DA_ShiftProgression"));
	}

	if (UWorld* World = GetWorld())
	{
		if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
		{
			ShiftSubsystem->SetPresentationAsset(PresentationData);
			if (IsValid(ResolvedProgression))
			{
				ShiftSubsystem->SetShiftProgressionAsset(ResolvedProgression);
			}
		}
	}
}

void ATareTitheGameMode::ApplyBaseline6GbVramScalability()
{
	// Enforce native UE5 TSR (Method 4) and 6 GB VRAM safe streaming pool without third-party upscalers.
	if (IConsoleVariable* AntiAliasingCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod")))
	{
		AntiAliasingCVar->Set(4, ECVF_SetByGameSetting);
	}

	if (IConsoleVariable* StreamingPoolCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streaming.PoolSize")))
	{
		StreamingPoolCVar->Set(2000, ECVF_SetByGameSetting);
	}

	if (UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
	{
		Settings->SetOverallScalabilityLevel(FMath::Clamp(BaselineScalabilityLevel, 0, 3));
		Settings->SetResolutionScaleValueEx(FMath::Clamp(BaselineTsrResolutionScalePercent, 50.0f, 100.0f));
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

void ATareTitheGameMode::SpawnMissingGreyboxStationsIfNeeded()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>();
	if (!IsValid(ShiftSubsystem))
	{
		return;
	}

	const bool bNeedsCoreStations =
		!ShiftSubsystem->HasRegisteredSliceableRig() ||
		!ShiftSubsystem->HasRegisteredQuotaReceiver();

	if (!bNeedsCoreStations)
	{
		EnsureOuterCellFramingArchitecture(World);
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Reposition default-origin pawn prior to station registration so its world transform is final when Contract 0 loads.
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			if (Pawn->GetActorLocation().IsNearlyZero(1.0f))
			{
				Pawn->SetActorLocationAndRotation(DefaultInspectorLocation, FRotator::ZeroRotator);
			}
		}
	}

	if (!ShiftSubsystem->HasRegisteredDirectorBoothStation())
	{
		World->SpawnActor<ADirectorBoothActor>(
			ADirectorBoothActor::StaticClass(),
			FTransform(FRotator::ZeroRotator, DefaultDirectorBoothLocation, FVector::OneVector),
			SpawnParams);
	}

	if (!ShiftSubsystem->HasRegisteredStrayCatchStation())
	{
		World->SpawnActor<AStrayFloorCatchActor>(
			AStrayFloorCatchActor::StaticClass(),
			FTransform(FRotator::ZeroRotator, DefaultStrayCatchLocation, FVector::OneVector),
			SpawnParams);
	}

	if (!ShiftSubsystem->HasRegisteredQuotaReceiver())
	{
		World->SpawnActor<AScaleAssemblyActor>(
			AScaleAssemblyActor::StaticClass(),
			FTransform(FRotator::ZeroRotator, DefaultScaleAssemblyLocation, FVector::OneVector),
			SpawnParams);
	}

	if (!ShiftSubsystem->HasRegisteredSliceableRig())
	{
		World->SpawnActor<AMonolithRigActor>(
			AMonolithRigActor::StaticClass(),
			FTransform(FRotator::ZeroRotator, DefaultMonolithRigLocation, FVector::OneVector),
			SpawnParams);
	}

	SpawnGreyboxPitEnclosureAndLighting(World);

	if (ShiftSubsystem->GetCurrentPhase() == EShiftPhase::WaitingForStations)
	{
		ShiftSubsystem->StartShiftFromContract(0);
	}
}

void ATareTitheGameMode::SpawnGreyboxPitEnclosureAndLighting(UWorld* World)
{
	if (!IsValid(World))
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UMaterialInstanceDynamic* PitMID = IsValid(GreyboxCellBaseMaterial)
		? UMaterialInstanceDynamic::Create(GreyboxCellBaseMaterial, this)
		: nullptr;
	if (IsValid(PitMID))
	{
		PitMID->SetVectorParameterValue(TEXT("BaseColorConst"), GreyboxPitEnclosureColor);
		PitMID->SetVectorParameterValue(TEXT("Color"), GreyboxPitEnclosureColor);
		PitMID->SetVectorParameterValue(TEXT("EmissiveColorConst"), FLinearColor::Black);
		PitMID->SetScalarParameterValue(TEXT("EmissiveScale"), 0.0f);
		PitMID->SetScalarParameterValue(TEXT("RoughnessConst"), 0.88f);
		PitMID->SetScalarParameterValue(TEXT("MetallicConst"), 0.14f);
	}

	auto SpawnEnclosureSlab = [&](
		const FVector& Location,
		const FVector& GreyboxScale3D,
		bool bIsFloorSlab,
		bool bUnoccludeLumenShell)
	{
		const FMeshPresentationSlot* AssignedSlot = nullptr;
		UMaterialInterface* CustomCellMaterial = nullptr;
		if (IsValid(PresentationData))
		{
			AssignedSlot = bIsFloorSlab
				? &PresentationData->CellFloorGrateMeshSlot
				: &PresentationData->CellWallPanelMeshSlot;
			CustomCellMaterial = bIsFloorSlab
				? PresentationData->CellFloorMaterial.Get()
				: PresentationData->CellWallMaterial.Get();
		}

		const bool bHasCustomMesh = (AssignedSlot != nullptr) && IsValid(AssignedSlot->Mesh);
		if (!bHasCustomMesh && !IsValid(GreyboxCellCubeMesh))
		{
			return;
		}

		AStaticMeshActor* SlabActor = World->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(),
			FTransform(FRotator::ZeroRotator, Location, FVector::OneVector),
			SpawnParams);
		if (!IsValid(SlabActor))
		{
			return;
		}

		UStaticMeshComponent* MeshComp = SlabActor->GetStaticMeshComponent();
		if (!IsValid(MeshComp))
		{
			return;
		}

		MeshComp->PrimaryComponentTick.bCanEverTick = false;
		MeshComp->SetMobility(EComponentMobility::Movable);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComp->SetGenerateOverlapEvents(false);

		if (bHasCustomMesh)
		{
			AssignedSlot->ApplyToVisualComponent(MeshComp);
			const FTransform WorldSlabTransform =
				AssignedSlot->VisualLocalTransform * FTransform(FRotator::ZeroRotator, Location, FVector::OneVector);
			MeshComp->SetWorldTransform(WorldSlabTransform);
			if (AssignedSlot->MaterialOverrides.Num() == 0 && IsValid(CustomCellMaterial) && !IsValid(MeshComp->GetMaterial(0)))
			{
				MeshComp->SetMaterial(0, CustomCellMaterial);
			}
		}
		else
		{
			MeshComp->SetStaticMesh(GreyboxCellCubeMesh);
			MeshComp->SetWorldScale3D(GreyboxScale3D);
			UMaterialInterface* ResolvedMat = IsValid(CustomCellMaterial) ? CustomCellMaterial : PitMID;
			if (IsValid(ResolvedMat))
			{
				MeshComp->SetMaterial(0, ResolvedMat);
			}
		}

		if (bUnoccludeLumenShell)
		{
			// Keep perimeter shell visible to primary raster camera rays so the outer frame stays dark,
			// while unoccluding both SWRT Distance Fields / Surface Cache and HWRT RayTracingScene for Lumen GI & SkyLight cubemap reflections.
			MeshComp->SetCastShadow(false);
			MeshComp->SetAffectDistanceFieldLighting(false);
			MeshComp->SetAffectDynamicIndirectLighting(false);
			MeshComp->SetVisibleInRayTracing(false);
		}
	};

	const double DropX = DefaultScaleAssemblyLocation.X;
	const double DropY = DefaultScaleAssemblyLocation.Y;
	const double FrontWallX = DropX + 270.0;
	const double RearWallX = FMath::Min(-160.0, static_cast<double>(RearCellWallXCm));
	const double EnclosureCenterX = 0.5 * (FrontWallX + RearWallX);
	const double EnclosureSpanScaleX = ((FrontWallX - RearWallX) + 40.0) / 100.0;
	constexpr double SideWallOffsetY = 360.0;
	constexpr double EnclosureSpanScaleY = 7.60;
	constexpr double WallCenterZ = 190.0;
	constexpr double WallHeightScaleZ = 6.00;
	constexpr double CeilingZ = 475.0;

	// Authoritative invisible unscaled (1, 1, 1) pit floor catch collider below AStrayFloorCatchActor (-35 cm), decoupled from visual grate mesh swaps.
	if (AActor* PitFloorCatchActor = World->SpawnActor<AActor>(
		AActor::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(EnclosureCenterX, DropY, -95.0), FVector::OneVector),
		SpawnParams))
	{
		UBoxComponent* PitFloorCatchBox = NewObject<UBoxComponent>(PitFloorCatchActor, TEXT("PitFloorCatchCollider"));
		if (IsValid(PitFloorCatchBox))
		{
			PitFloorCatchBox->PrimaryComponentTick.bCanEverTick = false;
			PitFloorCatchBox->SetMobility(EComponentMobility::Movable);
			PitFloorCatchActor->SetRootComponent(PitFloorCatchBox);
			PitFloorCatchBox->SetWorldLocationAndRotation(FVector(EnclosureCenterX, DropY, -95.0), FRotator::ZeroRotator);
			PitFloorCatchBox->SetWorldScale3D(FVector::OneVector);
			PitFloorCatchBox->InitBoxExtent(FVector(50.0 * EnclosureSpanScaleX, 50.0 * EnclosureSpanScaleY, 10.0));
			PitFloorCatchBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			PitFloorCatchBox->SetCollisionObjectType(ECC_WorldStatic);
			PitFloorCatchBox->SetCollisionResponseToAllChannels(ECR_Block);
			PitFloorCatchBox->SetGenerateOverlapEvents(false);
			PitFloorCatchBox->RegisterComponent();
		}
	}

	// Visual-only pit floor (grounding intact) and 5 perimeter shell slabs (Lumen SWRT + HWRT unoccluded).
	SpawnEnclosureSlab(FVector(EnclosureCenterX, DropY, -95.0), FVector(EnclosureSpanScaleX, EnclosureSpanScaleY, 0.20), true, false);
	// Front wall behind Director Booth (+530) and behind-camera rear cell wall (-200) + Left/Right/Ceiling shell slabs.
	SpawnEnclosureSlab(FVector(FrontWallX, DropY, WallCenterZ), FVector(0.20, EnclosureSpanScaleY, WallHeightScaleZ), false, true);
	SpawnEnclosureSlab(FVector(RearWallX, DropY, WallCenterZ), FVector(0.20, EnclosureSpanScaleY, WallHeightScaleZ), false, true);
	SpawnEnclosureSlab(FVector(EnclosureCenterX, DropY - SideWallOffsetY, WallCenterZ), FVector(EnclosureSpanScaleX, 0.20, WallHeightScaleZ), false, true);
	SpawnEnclosureSlab(FVector(EnclosureCenterX, DropY + SideWallOffsetY, WallCenterZ), FVector(EnclosureSpanScaleX, 0.20, WallHeightScaleZ), false, true);
	SpawnEnclosureSlab(FVector(EnclosureCenterX, DropY, CeilingZ), FVector(EnclosureSpanScaleX, EnclosureSpanScaleY, 0.20), false, true);

	FCellLightingRigConfig RigCfg;
	if (IsValid(PresentationData))
	{
		RigCfg = PresentationData->LightingRigConfig;
	}
	else
	{
		RigCfg.KeySpot.Intensity = KeySpotLightIntensity;
		RigCfg.FillPointIntensity = FillPointLightIntensity;
		RigCfg.RimSpotLeft.Location = RimPointLightLocation;
		RigCfg.RimSpotLeft.Intensity = RimPointLightIntensity;
	}

	const FCellAtmosphereConfig AtmosCfg = IsValid(PresentationData)
		? PresentationData->AtmosphereConfig
		: FCellAtmosphereConfig();
	const FPhase12DPresentationPolishConfig Phase12DCfg = IsValid(PresentationData)
		? PresentationData->Phase12DPolishConfig.GetSanitized()
		: FPhase12DPresentationPolishConfig();

	if (Phase12DCfg.bEnableRearGridLuminanceSuppression)
	{
		RigCfg.MidgroundWashSpot.Intensity *= Phase12DCfg.MidgroundWashIntensityMultiplier;
		const float WashLum = (0.2126f * RigCfg.MidgroundWashSpot.LightColor.R)
			+ (0.7152f * RigCfg.MidgroundWashSpot.LightColor.G)
			+ (0.0722f * RigCfg.MidgroundWashSpot.LightColor.B);
		RigCfg.MidgroundWashSpot.LightColor = FMath::Lerp(
			RigCfg.MidgroundWashSpot.LightColor,
			FLinearColor(WashLum, WashLum, WashLum, 1.0f),
			Phase12DCfg.MidgroundWashChromaDesaturation);
	}

	// Recessed central Industrial Foundry Grate Shaft & heavy vertical mechanical columns in 2 zero-shadow, zero-collision Rodin PBR ISM batches (X in [430, 465] cm).
	if (AtmosCfg.bSpawnMidgroundArchitectureRelief && IsValid(PresentationData))
	{
		const FMeshPresentationSlot& GrateSlot = PresentationData->CellFloorGrateMeshSlot;
		const FMeshPresentationSlot* ColumnSlot = IsValid(PresentationData->ClampSpringHousingMeshSlot.Mesh)
			? &PresentationData->ClampSpringHousingMeshSlot
			: (IsValid(PresentationData->DirectorBoothFrameMeshSlot.Mesh) ? &PresentationData->DirectorBoothFrameMeshSlot : nullptr);

		if (IsValid(GrateSlot.Mesh) || (ColumnSlot != nullptr && IsValid(ColumnSlot->Mesh)))
		{
			if (AActor* FoundryShaftHostActor = World->SpawnActor<AActor>(
				AActor::StaticClass(),
				FTransform::Identity,
				SpawnParams))
			{
				FoundryShaftHostActor->PrimaryActorTick.bCanEverTick = false;

				USceneComponent* ShaftRoot = NewObject<USceneComponent>(FoundryShaftHostActor, TEXT("FoundryShaftRoot"));
				if (IsValid(ShaftRoot))
				{
					ShaftRoot->PrimaryComponentTick.bCanEverTick = false;
					ShaftRoot->SetMobility(EComponentMobility::Movable);
					FoundryShaftHostActor->SetRootComponent(ShaftRoot);
					ShaftRoot->RegisterComponent();
				}

				auto CreateRodinShaftISM = [&](
					const TCHAR* CompName,
					const FMeshPresentationSlot& SourceSlot,
					UMaterialInterface* FallbackMaterial) -> UInstancedStaticMeshComponent*
				{
					if (!IsValid(SourceSlot.Mesh))
					{
						return nullptr;
					}
					UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(FoundryShaftHostActor, CompName);
					if (!IsValid(ISM))
					{
						return nullptr;
					}
					ISM->PrimaryComponentTick.bCanEverTick = false;
					ISM->SetMobility(EComponentMobility::Movable);
					ISM->SetupAttachment(FoundryShaftHostActor->GetRootComponent());
					ISM->SetStaticMesh(SourceSlot.Mesh);
					ISM->SetSimulatePhysics(false);
					ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					ISM->SetGenerateOverlapEvents(false);
					ISM->SetCanEverAffectNavigation(false);
					ISM->SetCastShadow(false);
					ISM->bReceivesDecals = false;

					bool bAppliedSlotOverride = false;
					for (int32 SlotIdx = 0; SlotIdx < SourceSlot.MaterialOverrides.Num(); ++SlotIdx)
					{
						if (IsValid(SourceSlot.MaterialOverrides[SlotIdx]))
						{
							ISM->SetMaterial(SlotIdx, SourceSlot.MaterialOverrides[SlotIdx]);
							bAppliedSlotOverride = true;
						}
					}
					if (!bAppliedSlotOverride && IsValid(FallbackMaterial) && !IsValid(ISM->GetMaterial(0)))
					{
						ISM->SetMaterial(0, FallbackMaterial);
					}
					ISM->RegisterComponent();
					return ISM;
				};

				const double SafeMinShaftX = FMath::Max3(430.0, 415.0, static_cast<double>(MinMidgroundReliefXCm));
				constexpr double SafeMaxShaftX = 465.0;

				// 1. Vertical 2x2 Rodin grate wall behind cutting/diverter spine: Pitch = +90 deg maps Local +Z (sculpted grate face) -> World -X toward camera.
				if (UInstancedStaticMeshComponent* GrateShaftISM = CreateRodinShaftISM(
					TEXT("FoundryGrateShaftISM"),
					GrateSlot,
					PresentationData->CellFloorMaterial.Get()))
				{
					if (UMaterialInterface* GrateBaseMat = GrateShaftISM->GetMaterial(0))
					{
						if (UMaterialInstanceDynamic* FoundryRearGridMID = UMaterialInstanceDynamic::Create(GrateBaseMat, FoundryShaftHostActor))
						{
							FLinearColor BaseAlbedo(1.64f, 1.68f, 1.74f, 1.0f);
							FLinearColor BaseF0(0.19f, 0.20f, 0.22f, 1.0f);
							float BaseSpec = 0.72f;
							GrateBaseMat->GetVectorParameterValue(FMaterialParameterInfo(TEXT("AlbedoTint")), BaseAlbedo);
							GrateBaseMat->GetVectorParameterValue(FMaterialParameterInfo(TEXT("ConductorMinF0")), BaseF0);
							GrateBaseMat->GetScalarParameterValue(FMaterialParameterInfo(TEXT("SpecularScale")), BaseSpec);

							const float AlbedoMult = Phase12DCfg.bEnableRearGridLuminanceSuppression ? Phase12DCfg.RearGridAlbedoMultiplier : 1.0f;
							const float F0Mult = Phase12DCfg.bEnableRearGridLuminanceSuppression ? Phase12DCfg.RearGridConductorF0Multiplier : 1.0f;
							const float SpecMult = Phase12DCfg.bEnableRearGridLuminanceSuppression ? Phase12DCfg.RearGridSpecularMultiplier : 1.0f;

							FoundryRearGridMID->SetVectorParameterValue(
								TEXT("AlbedoTint"),
								FLinearColor(BaseAlbedo.R * AlbedoMult, BaseAlbedo.G * AlbedoMult, BaseAlbedo.B * AlbedoMult, 1.0f));
							FoundryRearGridMID->SetVectorParameterValue(
								TEXT("ConductorMinF0"),
								FLinearColor(BaseF0.R * F0Mult, BaseF0.G * F0Mult, BaseF0.B * F0Mult, 1.0f));
							FoundryRearGridMID->SetScalarParameterValue(TEXT("SpecularScale"), BaseSpec * SpecMult);
							if (Phase12DCfg.bEnableRearGridLuminanceSuppression)
							{
								FoundryRearGridMID->SetScalarParameterValue(TEXT("RoughnessMin"), 0.34f);
								FoundryRearGridMID->SetScalarParameterValue(TEXT("RoughnessMax"), 0.58f);
							}
							FoundryRearGridMID->SetScalarParameterValue(
								TEXT("ProceduralWearWeight"),
								Phase12DCfg.bEnableProceduralStationWear ? Phase12DCfg.RearGridProceduralWearWeight : 0.0f);
							FoundryRearGridMID->SetScalarParameterValue(
								TEXT("RearGridMacroSootWeight"),
								Phase12DCfg.bEnableRearGridLuminanceSuppression ? Phase12DCfg.RearGridMacroSootWeight : 0.0f);
							GrateShaftISM->SetMaterial(0, FoundryRearGridMID);
						}
					}

					const FBox GrateLocalBox = GrateSlot.Mesh->GetBoundingBox();
					const FVector GrateLocalSize = GrateLocalBox.GetSize();
					const FVector GrateLocalCenter = GrateLocalBox.GetCenter();

					const double TotalSpanY = 2.0 * FMath::Max(80.0, static_cast<double>(FoundryGrateHalfSpanYCm));
					const double BottomZ = static_cast<double>(FoundryGrateBottomZCm);
					const double TopZ = FMath::Max(BottomZ + 80.0, static_cast<double>(FoundryGrateTopZCm));
					const double TotalSpanZ = TopZ - BottomZ;

					const double TileSpanY = 0.5 * TotalSpanY;
					const double TileSpanZ = 0.5 * TotalSpanZ;
					const double TileThicknessX = FMath::Clamp(static_cast<double>(FoundryGrateThicknessXCm), 6.0, 20.0);
					const double ClampedFrontX = FMath::Clamp(
						static_cast<double>(FoundryGrateFrontXCm),
						SafeMinShaftX,
						SafeMaxShaftX - TileThicknessX);
					const double GrateWorldCenterX = ClampedFrontX + (0.5 * TileThicknessX);

					const FQuat GrateQuat = FRotator(90.0f, 0.0f, 0.0f).Quaternion();
					const FVector GrateInstanceScale(
						TileSpanZ / FMath::Max(1.0, GrateLocalSize.X),
						TileSpanY / FMath::Max(1.0, GrateLocalSize.Y),
						TileThicknessX / FMath::Max(1.0, GrateLocalSize.Z));

					for (int32 RowZ = 0; RowZ < 2; ++RowZ)
					{
						const double TileCenterZ = BottomZ + ((static_cast<double>(RowZ) + 0.5) * TileSpanZ);
						for (int32 ColY = 0; ColY < 2; ++ColY)
						{
							const double TileCenterY = DropY + ((-0.5 + static_cast<double>(ColY)) * TileSpanY);
							const FVector DesiredWorldCenter(GrateWorldCenterX, TileCenterY, TileCenterZ);
							const FVector PivotCompensatedTranslation =
								DesiredWorldCenter - GrateQuat.RotateVector(GrateInstanceScale * GrateLocalCenter);
							GrateShaftISM->AddInstance(FTransform(GrateQuat, PivotCompensatedTranslation, GrateInstanceScale), true);
						}
					}
				}

				// 2. Heavy vertical Rodin mechanical columns flanking the recessed grate shaft at Y = +-155 cm and supporting ADirectorBoothActor.
				if (ColumnSlot != nullptr)
				{
					if (UInstancedStaticMeshComponent* FlankColumnISM = CreateRodinShaftISM(
						TEXT("FoundryFlankColumnISM"),
						*ColumnSlot,
						PresentationData->DarkGunmetalMaterial.Get()))
					{
						const FBox ColLocalBox = ColumnSlot->Mesh->GetBoundingBox();
						const FVector ColLocalSize = ColLocalBox.GetSize();
						const FVector ColLocalCenter = ColLocalBox.GetCenter();

						const double ModuleDepthX = FMath::Clamp(FoundryColumnModuleSizeCm.X, 16.0, 32.0);
						const double ModuleWidthY = FMath::Clamp(FoundryColumnModuleSizeCm.Y, 24.0, 64.0);
						const double ColumnTopZ = static_cast<double>(FoundryGrateTopZCm);
						constexpr double ColumnFloorZ = -85.0;
						constexpr int32 ModulesPerColumn = 3;
						const double ModuleHeightZ = (ColumnTopZ - ColumnFloorZ) / static_cast<double>(ModulesPerColumn);

						const double ColumnFrontX = FMath::Clamp(SafeMinShaftX + 2.0, SafeMinShaftX, SafeMaxShaftX - ModuleDepthX);
						const double ColumnWorldCenterX = ColumnFrontX + (0.5 * ModuleDepthX);

						const FQuat ColumnQuat = FQuat::Identity;
						const FVector ColumnInstanceScale(
							ModuleDepthX / FMath::Max(1.0, ColLocalSize.X),
							ModuleWidthY / FMath::Max(1.0, ColLocalSize.Y),
							ModuleHeightZ / FMath::Max(1.0, ColLocalSize.Z));

						const double ColumnAbsY = FMath::Max(120.0, static_cast<double>(FoundryColumnOffsetYCm));
						if (!Phase12DCfg.bEnableProceduralStationWear)
						{
							if (UMaterialInterface* ColBaseMat = FlankColumnISM->GetMaterial(0))
							{
								if (UMaterialInstanceDynamic* FlankColMID = UMaterialInstanceDynamic::Create(ColBaseMat, FoundryShaftHostActor))
								{
									FlankColMID->SetScalarParameterValue(TEXT("ProceduralWearWeight"), 0.0f);
									FlankColMID->SetScalarParameterValue(TEXT("RearGridMacroSootWeight"), 0.0f);
									FlankColumnISM->SetMaterial(0, FlankColMID);
								}
							}
						}
						for (const double SideSign : { -1.0, 1.0 })
						{
							const double ColumnCenterY = DropY + (SideSign * ColumnAbsY);
							for (int32 TierIdx = 0; TierIdx < ModulesPerColumn; ++TierIdx)
							{
								const double TierCenterZ = ColumnFloorZ + ((static_cast<double>(TierIdx) + 0.5) * ModuleHeightZ);
								const FVector DesiredWorldCenter(ColumnWorldCenterX, ColumnCenterY, TierCenterZ);
								const FVector PivotCompensatedTranslation =
									DesiredWorldCenter - ColumnQuat.RotateVector(ColumnInstanceScale * ColLocalCenter);
								FlankColumnISM->AddInstance(FTransform(ColumnQuat, PivotCompensatedTranslation, ColumnInstanceScale), true);
							}
						}
					}
				}

				// 3. Safeguard 4: 3 asymmetric vertical industrial risers + flange collars (X in [420.5, 438.0] cm, Z in [-66.0, +210.0] cm)
				// breaking up the rear grate repetition in 1 zero-shadow, zero-collision dark soot-iron ISM batch.
				if (Phase12DCfg.bEnableRearShaftHeavyConduits && IsValid(GreyboxCellCylinderMesh))
				{
					if (UInstancedStaticMeshComponent* ConduitISM = NewObject<UInstancedStaticMeshComponent>(
						FoundryShaftHostActor,
						TEXT("FoundryRearConduitISM")))
					{
						ConduitISM->PrimaryComponentTick.bCanEverTick = false;
						ConduitISM->SetMobility(EComponentMobility::Movable);
						ConduitISM->SetupAttachment(FoundryShaftHostActor->GetRootComponent());
						ConduitISM->SetStaticMesh(GreyboxCellCylinderMesh);
						ConduitISM->SetSimulatePhysics(false);
						ConduitISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						ConduitISM->SetGenerateOverlapEvents(false);
						ConduitISM->SetCanEverAffectNavigation(false);
						ConduitISM->SetCastShadow(false);
						ConduitISM->bReceivesDecals = false;

						UMaterialInterface* ConduitParentMat = nullptr;
						if (PresentationData->TareChuteMeshSlot.MaterialOverrides.Num() > 0 &&
							IsValid(PresentationData->TareChuteMeshSlot.MaterialOverrides[0]))
						{
							ConduitParentMat = PresentationData->TareChuteMeshSlot.MaterialOverrides[0].Get();
						}
						else if (IsValid(PresentationData->DarkGunmetalMaterial))
						{
							ConduitParentMat = PresentationData->DarkGunmetalMaterial.Get();
						}
						else
						{
							ConduitParentMat = GreyboxCellBaseMaterial.Get();
						}

						if (IsValid(ConduitParentMat))
						{
							if (UMaterialInstanceDynamic* FoundryRearConduitMID = UMaterialInstanceDynamic::Create(ConduitParentMat, FoundryShaftHostActor))
							{
								FoundryRearConduitMID->SetVectorParameterValue(TEXT("AlbedoTint"), Phase12DCfg.ConduitAlbedoTint);
								FoundryRearConduitMID->SetVectorParameterValue(TEXT("ConductorMinF0"), Phase12DCfg.ConduitConductorMinF0);
								FoundryRearConduitMID->SetScalarParameterValue(TEXT("RoughnessMin"), Phase12DCfg.ConduitRoughnessMin);
								FoundryRearConduitMID->SetScalarParameterValue(TEXT("RoughnessMax"), Phase12DCfg.ConduitRoughnessMax);
								FoundryRearConduitMID->SetScalarParameterValue(TEXT("SpecularScale"), Phase12DCfg.ConduitSpecularScale);
								FoundryRearConduitMID->SetScalarParameterValue(TEXT("MetallicScale"), 1.0f);
								FoundryRearConduitMID->SetScalarParameterValue(TEXT("MetallicMin"), 0.44f);
								FoundryRearConduitMID->SetScalarParameterValue(TEXT("MetallicMax"), 0.60f);
								FoundryRearConduitMID->SetScalarParameterValue(TEXT("AOStrength"), 0.82f);
								FoundryRearConduitMID->SetScalarParameterValue(TEXT("NormalStrength"), 1.35f);
								FoundryRearConduitMID->SetScalarParameterValue(
									TEXT("ProceduralWearWeight"),
									Phase12DCfg.bEnableProceduralStationWear ? Phase12DCfg.ConduitProceduralWearWeight : 0.0f);
								FoundryRearConduitMID->SetScalarParameterValue(TEXT("RearGridMacroSootWeight"), 0.0f);
								FoundryRearConduitMID->SetVectorParameterValue(TEXT("BaseColorConst"), FLinearColor(0.045f, 0.048f, 0.052f, 1.0f));
								FoundryRearConduitMID->SetScalarParameterValue(TEXT("RoughnessConst"), 0.56f);
								FoundryRearConduitMID->SetScalarParameterValue(TEXT("MetallicConst"), 0.52f);
								FoundryRearConduitMID->SetScalarParameterValue(TEXT("SpecularConst"), Phase12DCfg.ConduitSpecularScale);
								ConduitISM->SetMaterial(0, FoundryRearConduitMID);
							}
						}

						ConduitISM->RegisterComponent();

						const FBox CylBox = GreyboxCellCylinderMesh->GetBoundingBox();
						const FVector CylSize = CylBox.GetSize();
						const FVector CylCenter = CylBox.GetCenter();

						auto AddVerticalCylinderCm = [&](double CenterX, double CenterY, double CenterZ, double RadiusCm, double HeightZCm)
						{
							const double DiameterCm = 2.0 * RadiusCm;
							const FVector Scale(
								DiameterCm / FMath::Max(1.0, CylSize.X),
								DiameterCm / FMath::Max(1.0, CylSize.Y),
								HeightZCm / FMath::Max(1.0, CylSize.Z));
							const FVector PivotWorld = FVector(CenterX, CenterY, CenterZ) - (Scale * CylCenter);
							ConduitISM->AddInstance(FTransform(FQuat::Identity, PivotWorld, Scale), true);
						};

						struct FConduitRiserSpec
						{
							double WorldX;
							double OffsetY;
							double PipeRadiusCm;
							double CollarRadiusCm;
						};

						const FConduitRiserSpec RiserSpecs[3] = {
							{ 429.5, -92.0, 6.5, 8.0 },
							{ 433.0, -110.0, 3.8, 5.0 },
							{ 429.0, +98.0, 7.0, 8.5 }
						};
						constexpr double RiserBottomZ = -66.0;
						constexpr double RiserTopZ = 210.0;
						constexpr double RiserCenterZ = 0.5 * (RiserBottomZ + RiserTopZ);
						constexpr double RiserHeightZ = RiserTopZ - RiserBottomZ;
						constexpr double CollarHeightZ = 5.5;
						const double CollarCentersZ[3] = { 16.0, 96.0, 176.0 };

						for (const FConduitRiserSpec& Spec : RiserSpecs)
						{
							const double ConduitWorldY = DropY + Spec.OffsetY;
							AddVerticalCylinderCm(Spec.WorldX, ConduitWorldY, RiserCenterZ, Spec.PipeRadiusCm, RiserHeightZ);
							for (const double CollarZ : CollarCentersZ)
							{
								AddVerticalCylinderCm(Spec.WorldX, ConduitWorldY, CollarZ, Spec.CollarRadiusCm, CollarHeightZ);
							}
						}
					}
				}
			}
		}
	}

	// Cook-safe specified-cubemap SkyLight inside the sealed enclosure (CastShadows = false + immediate RecaptureSky).
	if (AtmosCfg.SkyLightIntensity > KINDA_SMALL_NUMBER)
	{
		UTextureCube* ResolvedCubemap = AtmosCfg.AmbientSkyCubemap.Get();
		if (!IsValid(ResolvedCubemap) && FPackageName::DoesPackageExist(TEXT("/Game/Presentation/Lighting/TC_CellAmbientCubemap")))
		{
			ResolvedCubemap = LoadObject<UTextureCube>(
				nullptr,
				TEXT("/Game/Presentation/Lighting/TC_CellAmbientCubemap.TC_CellAmbientCubemap"));
		}
		if (!IsValid(ResolvedCubemap) && FPackageName::DoesPackageExist(TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap")))
		{
			ResolvedCubemap = LoadObject<UTextureCube>(
				nullptr,
				TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap"));
		}
		if (!IsValid(ResolvedCubemap) && FPackageName::DoesPackageExist(TEXT("/Engine/Functions/Engine_MaterialFunctions01/Cubemaps/InteriorCubemap")))
		{
			ResolvedCubemap = LoadObject<UTextureCube>(
				nullptr,
				TEXT("/Engine/Functions/Engine_MaterialFunctions01/Cubemaps/InteriorCubemap.InteriorCubemap"));
		}

		if (IsValid(ResolvedCubemap))
		{
			ASkyLight* CellSkyLight = World->SpawnActor<ASkyLight>(
				ASkyLight::StaticClass(),
				FTransform(FRotator::ZeroRotator, FVector(EnclosureCenterX, DropY, 260.0), FVector::OneVector),
				SpawnParams);
			if (IsValid(CellSkyLight) && IsValid(CellSkyLight->GetLightComponent()))
			{
				USkyLightComponent* SkyComp = CellSkyLight->GetLightComponent();
				SkyComp->SetMobility(EComponentMobility::Movable);
				SkyComp->SetRealTimeCapture(false);
				SkyComp->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
				SkyComp->bLowerHemisphereIsBlack = false;
				SkyComp->SetLowerHemisphereColor(AtmosCfg.LowerHemisphereColor);
				SkyComp->SetCubemap(ResolvedCubemap);
				SkyComp->SetIntensity(AtmosCfg.SkyLightIntensity);
				SkyComp->SetLightColor(AtmosCfg.SkyTintColor);
				SkyComp->SetVolumetricScatteringIntensity(0.0f);
				SkyComp->SetCastShadows(false);
				SkyComp->RecaptureSky();
			}
		}
	}

	// Non-volumetric analytical depth fog starting behind the X=260 monolith/console plane (~0.05 ms GPU).
	if (AtmosCfg.bEnableDepthFog && AtmosCfg.FogDensity > KINDA_SMALL_NUMBER)
	{
		AExponentialHeightFog* CellDepthFog = World->SpawnActor<AExponentialHeightFog>(
			AExponentialHeightFog::StaticClass(),
			FTransform(FRotator::ZeroRotator, FVector(EnclosureCenterX, DropY, AtmosCfg.FogActorZCm), FVector::OneVector),
			SpawnParams);
		if (IsValid(CellDepthFog) && IsValid(CellDepthFog->GetComponent()))
		{
			UExponentialHeightFogComponent* FogComp = CellDepthFog->GetComponent();
			FogComp->SetMobility(EComponentMobility::Movable);
			FogComp->SetVolumetricFog(false);
			FogComp->SetFogDensity(AtmosCfg.FogDensity);
			FogComp->SetFogHeightFalloff(AtmosCfg.FogHeightFalloff);
			FogComp->SetStartDistance(AtmosCfg.FogStartDistanceCm);
			FogComp->SetFogMaxOpacity(AtmosCfg.FogMaxOpacity);
			FogComp->SetFogInscatteringColor(AtmosCfg.FogInscatteringColor);
		}
	}

	auto SpawnConfiguredSpotLight = [&](const FCellDirectionalSpotConfig& SpotCfg, bool bAllowShadowCasting)
	{
		if (SpotCfg.Intensity <= KINDA_SMALL_NUMBER)
		{
			return;
		}
		const FVector AimDir = (SpotCfg.TargetLocation - SpotCfg.Location).GetSafeNormal(1.0e-4f, FVector::DownVector);
		// Orient centered overhead/shaft strip lights (|Location.Y| < 50 cm) with capsule tangent (Local +Z) horizontal along World +Y,
		// while lateral rim lights keep vertical raking capsule alignment along World +Z.
		const FRotator SpotRotation = (FMath::Abs(SpotCfg.Location.Y) < 50.0)
			? FRotationMatrix::MakeFromXZ(AimDir, FVector::RightVector).Rotator()
			: FRotationMatrix::MakeFromX(AimDir).Rotator();

		ASpotLight* SpawnedSpot = World->SpawnActor<ASpotLight>(
			ASpotLight::StaticClass(),
			FTransform(SpotRotation, SpotCfg.Location, FVector::OneVector),
			SpawnParams);
		if (IsValid(SpawnedSpot) && IsValid(SpawnedSpot->SpotLightComponent))
		{
			SpawnedSpot->SpotLightComponent->SetMobility(EComponentMobility::Movable);
			// Override ASpotLight CDO's default (-90, 0, 0) RelativeRotation so GetDirection() (+X) matches AimDir 1:1.
			SpawnedSpot->SpotLightComponent->SetWorldLocationAndRotation(SpotCfg.Location, SpotRotation);
			SpawnedSpot->SpotLightComponent->SetIntensity(SpotCfg.Intensity);
			SpawnedSpot->SpotLightComponent->SetAttenuationRadius(SpotCfg.AttenuationRadius);
			SpawnedSpot->SpotLightComponent->SetInnerConeAngle(SpotCfg.InnerConeAngle);
			SpawnedSpot->SpotLightComponent->SetOuterConeAngle(FMath::Max(SpotCfg.InnerConeAngle + 1.0f, SpotCfg.OuterConeAngle));
			SpawnedSpot->SpotLightComponent->SetSourceRadius(FMath::Max(0.0f, SpotCfg.SourceRadius));
			SpawnedSpot->SpotLightComponent->SetSourceLength(FMath::Max(0.0f, SpotCfg.SourceLength));
			SpawnedSpot->SpotLightComponent->SetLightColor(SpotCfg.LightColor);
			SpawnedSpot->SpotLightComponent->SetVolumetricScatteringIntensity(0.0f);
			SpawnedSpot->SpotLightComponent->SetCastShadows(bAllowShadowCasting);
		}
	};

	// 1. Sole shadow-casting VSM spotlight in the cell.
	SpawnConfiguredSpotLight(RigCfg.KeySpot, RigCfg.KeySpot.bCastShadows);

	// 2. Unshadowed front-side area fill point light for crisp frontal bevel & scale pan legibility.
	if (RigCfg.FillPointIntensity > KINDA_SMALL_NUMBER)
	{
		APointLight* CellFillLight = World->SpawnActor<APointLight>(
			APointLight::StaticClass(),
			FTransform(FRotator::ZeroRotator, RigCfg.FillPointLocation, FVector::OneVector),
			SpawnParams);
		if (IsValid(CellFillLight) && IsValid(CellFillLight->PointLightComponent))
		{
			CellFillLight->PointLightComponent->SetMobility(EComponentMobility::Movable);
			CellFillLight->PointLightComponent->SetIntensity(RigCfg.FillPointIntensity);
			CellFillLight->PointLightComponent->SetAttenuationRadius(RigCfg.FillPointRadius);
			CellFillLight->PointLightComponent->SetSourceRadius(FMath::Max(0.0f, RigCfg.FillPointSourceRadius));
			CellFillLight->PointLightComponent->SetSourceLength(FMath::Max(0.0f, RigCfg.FillPointSourceLength));
			CellFillLight->PointLightComponent->SetVolumetricScatteringIntensity(0.0f);
			CellFillLight->PointLightComponent->SetCastShadows(false);
			CellFillLight->PointLightComponent->SetLightColor(RigCfg.FillPointColor);
		}
	}

	// 3 & 4. Dual cool ~7400K industrial tube rim spotlights raking left/right across Rodin station metal chamfers.
	SpawnConfiguredSpotLight(RigCfg.RimSpotLeft, false);
	SpawnConfiguredSpotLight(RigCfg.RimSpotRight, false);

	// 5. Unshadowed ~2150K warm foundry shaft tube light pointed +X onto the rear grate shaft & flanking columns.
	SpawnConfiguredSpotLight(RigCfg.MidgroundWashSpot, false);

	// 6. Phase 12B Outer Left & Right Cell Edge Architectural Pipe/Column Framing (idempotent duplicate-guarded spawn).
	EnsureOuterCellFramingArchitecture(World);
}

void ATareTitheGameMode::EnsureOuterCellFramingArchitecture(UWorld* World)
{
	if (!IsValid(World))
	{
		return;
	}

	ACellFramingArchitectureActor* ResolvedFramingActor = ActiveOuterFramingActor.Get();
	for (TActorIterator<ACellFramingArchitectureActor> It(World); It; ++It)
	{
		ACellFramingArchitectureActor* Candidate = *It;
		if (!IsValid(Candidate))
		{
			continue;
		}
		if (!IsValid(ResolvedFramingActor))
		{
			ResolvedFramingActor = Candidate;
		}
		else if (Candidate != ResolvedFramingActor)
		{
			Candidate->Destroy();
		}
	}

	if (!IsValid(ResolvedFramingActor))
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ResolvedFramingActor = World->SpawnActor<ACellFramingArchitectureActor>(
			ACellFramingArchitectureActor::StaticClass(),
			FTransform::Identity,
			SpawnParams);
	}

	if (IsValid(ResolvedFramingActor))
	{
		ActiveOuterFramingActor = ResolvedFramingActor;
		ResolvedFramingActor->ConfigureFraming(PresentationData);
	}
}
