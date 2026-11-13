// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/TareTitheGameMode.h"
#include "Actors/DirectorBoothActor.h"
#include "Actors/InspectorPawn.h"
#include "Actors/MonolithRigActor.h"
#include "Actors/ScaleAssemblyActor.h"
#include "Actors/StrayFloorCatchActor.h"
#include "Components/BoxComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/ShiftProgressionDataAsset.h"
#include "Data/TareTithePresentationDataAsset.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PointLight.h"
#include "Engine/SkyLight.h"
#include "Engine/SpotLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureCube.h"
#include "Engine/World.h"
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

	auto SpawnEnclosureSlab = [&](const FVector& Location, const FVector& GreyboxScale3D, bool bIsFloorSlab)
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

	// Visual-only pit floor and cell enclosure slabs (NoCollision).
	SpawnEnclosureSlab(FVector(EnclosureCenterX, DropY, -95.0), FVector(EnclosureSpanScaleX, EnclosureSpanScaleY, 0.20), true);
	// Front wall behind Director Booth and rear cell wall behind X = -150 cm so pulled-back camera never clips level geometry.
	SpawnEnclosureSlab(FVector(FrontWallX, DropY, WallCenterZ), FVector(0.20, EnclosureSpanScaleY, WallHeightScaleZ), false);
	SpawnEnclosureSlab(FVector(RearWallX, DropY, WallCenterZ), FVector(0.20, EnclosureSpanScaleY, WallHeightScaleZ), false);
	SpawnEnclosureSlab(FVector(EnclosureCenterX, DropY - SideWallOffsetY, WallCenterZ), FVector(EnclosureSpanScaleX, 0.20, WallHeightScaleZ), false);
	SpawnEnclosureSlab(FVector(EnclosureCenterX, DropY + SideWallOffsetY, WallCenterZ), FVector(EnclosureSpanScaleX, 0.20, WallHeightScaleZ), false);
	SpawnEnclosureSlab(FVector(EnclosureCenterX, DropY, CeilingZ), FVector(EnclosureSpanScaleX, EnclosureSpanScaleY, 0.20), false);

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

	// Midground architectural relief in 2 zero-shadow, zero-collision instanced batches (strictly X >= 415.0 cm).
	if (AtmosCfg.bSpawnMidgroundArchitectureRelief && IsValid(GreyboxCellCubeMesh))
	{
		if (AActor* ReliefHostActor = World->SpawnActor<AActor>(
			AActor::StaticClass(),
			FTransform::Identity,
			SpawnParams))
		{
			ReliefHostActor->PrimaryActorTick.bCanEverTick = false;

			USceneComponent* ReliefRoot = NewObject<USceneComponent>(ReliefHostActor, TEXT("MidgroundReliefRoot"));
			if (IsValid(ReliefRoot))
			{
				ReliefRoot->PrimaryComponentTick.bCanEverTick = false;
				ReliefRoot->SetMobility(EComponentMobility::Movable);
				ReliefHostActor->SetRootComponent(ReliefRoot);
				ReliefRoot->RegisterComponent();
			}

			auto CreateReliefISM = [&](const TCHAR* CompName, UMaterialInterface* AssignedMaterial) -> UInstancedStaticMeshComponent*
			{
				UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(ReliefHostActor, CompName);
				if (!IsValid(ISM))
				{
					return nullptr;
				}
				ISM->PrimaryComponentTick.bCanEverTick = false;
				ISM->SetMobility(EComponentMobility::Movable);
				ISM->SetupAttachment(ReliefHostActor->GetRootComponent());
				ISM->SetStaticMesh(GreyboxCellCubeMesh);
				ISM->SetSimulatePhysics(false);
				ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				ISM->SetGenerateOverlapEvents(false);
				ISM->SetCanEverAffectNavigation(false);
				ISM->SetCastShadow(false);
				ISM->bReceivesDecals = false;
				if (IsValid(AssignedMaterial))
				{
					ISM->SetMaterial(0, AssignedMaterial);
				}
				ISM->RegisterComponent();
				return ISM;
			};

			UMaterialInterface* WallReliefMat = (IsValid(PresentationData) && IsValid(PresentationData->CellWallMaterial))
				? PresentationData->CellWallMaterial.Get()
				: PitMID;

			UMaterialInterface* GunmetalReliefMat = (IsValid(PresentationData) && IsValid(PresentationData->DarkGunmetalMaterial))
				? PresentationData->DarkGunmetalMaterial.Get()
				: nullptr;
			if (!IsValid(GunmetalReliefMat) && FPackageName::DoesPackageExist(TEXT("/Game/Presentation/Materials/M_DarkGunmetal_Master")))
			{
				GunmetalReliefMat = LoadObject<UMaterialInterface>(
					nullptr,
					TEXT("/Game/Presentation/Materials/M_DarkGunmetal_Master.M_DarkGunmetal_Master"));
			}
			if (!IsValid(GunmetalReliefMat))
			{
				GunmetalReliefMat = WallReliefMat;
			}

			const double SafeMinReliefX = FMath::Max(415.0, static_cast<double>(MinMidgroundReliefXCm));
			auto AddSafeReliefInstance = [SafeMinReliefX](
				UInstancedStaticMeshComponent* ISM,
				const FVector& WorldCenter,
				const FVector& Scale3D)
			{
				if (!IsValid(ISM))
				{
					return;
				}
				const double HalfExtentX = 50.0 * FMath::Abs(Scale3D.X);
				const double ClampedCenterX = FMath::Max(WorldCenter.X, SafeMinReliefX + HalfExtentX);
				const FVector SafeCenter(ClampedCenterX, WorldCenter.Y, WorldCenter.Z);
				ISM->AddInstance(FTransform(FRotator::ZeroRotator, SafeCenter, Scale3D), true);
			};

			if (UInstancedStaticMeshComponent* WallReliefISM = CreateReliefISM(TEXT("WallBulkheadReliefISM"), WallReliefMat))
			{
				const double RearPilasterX = FrontWallX - 19.0; // X=511 -> [502..520]
				const double RearDadoX = FrontWallX - 17.0;     // X=513 -> [506..520]
				const double FarSidePilasterX = FrontWallX - 62.0; // X=468 -> [456..480]

				for (const double PilasterOffsetY : { -295.0, -165.0, 165.0, 295.0 })
				{
					AddSafeReliefInstance(
						WallReliefISM,
						FVector(RearPilasterX, DropY + PilasterOffsetY, 185.0),
						FVector(0.18, 0.28, 5.40));
				}

				// Horizontal bulkhead dado beams in the lower/mid wall zone below ADirectorBoothActor (Z < 215).
				AddSafeReliefInstance(WallReliefISM, FVector(RearDadoX, DropY, 142.0), FVector(0.14, 7.00, 0.22));
				AddSafeReliefInstance(WallReliefISM, FVector(RearDadoX, DropY, 38.0), FVector(0.14, 7.00, 0.24));

				// Far side-wall structural pilasters framing peripheral FOV at X >= 456 cm.
				AddSafeReliefInstance(
					WallReliefISM,
					FVector(FarSidePilasterX, DropY - (SideWallOffsetY - 19.0), 185.0),
					FVector(0.24, 0.18, 5.40));
				AddSafeReliefInstance(
					WallReliefISM,
					FVector(FarSidePilasterX, DropY + (SideWallOffsetY - 19.0), 185.0),
					FVector(0.24, 0.18, 5.40));
			}

			if (UInstancedStaticMeshComponent* GunmetalReliefISM = CreateReliefISM(TEXT("GunmetalStanchionReliefISM"), GunmetalReliefMat))
			{
				const double BoothStanchionX = FMath::Max(DefaultDirectorBoothLocation.X + 4.0, SafeMinReliefX + 10.0); // X=434 -> [425..443]
				const double BoothBottomZ = DefaultDirectorBoothLocation.Z - 44.0; // Z=214, strictly below BoothFrame & Wing Signboards
				const double FloorTopZ = -85.0;
				const double StanchionCenterZ = 0.5 * (BoothBottomZ + FloorTopZ);
				const double StanchionScaleZ = FMath::Max(0.50, (BoothBottomZ - FloorTopZ) / 100.0);

				// Twin vertical gunmetal support stanchions anchoring the floating Director Booth to the pit floor.
				AddSafeReliefInstance(
					GunmetalReliefISM,
					FVector(BoothStanchionX, DropY - 82.0, StanchionCenterZ),
					FVector(0.16, 0.14, StanchionScaleZ));
				AddSafeReliefInstance(
					GunmetalReliefISM,
					FVector(BoothStanchionX, DropY + 82.0, StanchionCenterZ),
					FVector(0.16, 0.14, StanchionScaleZ));

				// Sill header beam & lower cross-brace between under-booth stanchions.
				AddSafeReliefInstance(
					GunmetalReliefISM,
					FVector(BoothStanchionX, DropY, BoothBottomZ - 6.0),
					FVector(0.18, 1.80, 0.12));
				AddSafeReliefInstance(
					GunmetalReliefISM,
					FVector(BoothStanchionX, DropY, 92.0),
					FVector(0.14, 1.78, 0.09));
				AddSafeReliefInstance(
					GunmetalReliefISM,
					FVector(BoothStanchionX, DropY, -6.0),
					FVector(0.18, 1.80, 0.14));

				// Far rear-flank vertical steel ribs outside wing signboard Y-span (|Y| = 238 > 190).
				const double RearRibX = FrontWallX - 38.0; // X=492 -> [483..501]
				AddSafeReliefInstance(
					GunmetalReliefISM,
					FVector(RearRibX, DropY - 238.0, 185.0),
					FVector(0.18, 0.14, 5.40));
				AddSafeReliefInstance(
					GunmetalReliefISM,
					FVector(RearRibX, DropY + 238.0, 185.0),
					FVector(0.18, 0.14, 5.40));
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
		const FRotator SpotRotation = FRotationMatrix::MakeFromX(AimDir).Rotator();

		ASpotLight* SpawnedSpot = World->SpawnActor<ASpotLight>(
			ASpotLight::StaticClass(),
			FTransform(SpotRotation, SpotCfg.Location, FVector::OneVector),
			SpawnParams);
		if (IsValid(SpawnedSpot) && IsValid(SpawnedSpot->SpotLightComponent))
		{
			SpawnedSpot->SpotLightComponent->SetMobility(EComponentMobility::Movable);
			SpawnedSpot->SpotLightComponent->SetIntensity(SpotCfg.Intensity);
			SpawnedSpot->SpotLightComponent->SetAttenuationRadius(SpotCfg.AttenuationRadius);
			SpawnedSpot->SpotLightComponent->SetInnerConeAngle(SpotCfg.InnerConeAngle);
			SpawnedSpot->SpotLightComponent->SetOuterConeAngle(FMath::Max(SpotCfg.InnerConeAngle + 1.0f, SpotCfg.OuterConeAngle));
			SpawnedSpot->SpotLightComponent->SetLightColor(SpotCfg.LightColor);
			SpawnedSpot->SpotLightComponent->SetVolumetricScatteringIntensity(0.0f);
			SpawnedSpot->SpotLightComponent->SetCastShadows(bAllowShadowCasting);
		}
	};

	// 1. Sole shadow-casting VSM spotlight in the cell.
	SpawnConfiguredSpotLight(RigCfg.KeySpot, RigCfg.KeySpot.bCastShadows);

	// 2. Unshadowed front-side fill point light for crisp underside bevel legibility.
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
			CellFillLight->PointLightComponent->SetVolumetricScatteringIntensity(0.0f);
			CellFillLight->PointLightComponent->SetCastShadows(false);
			CellFillLight->PointLightComponent->SetLightColor(RigCfg.FillPointColor);
		}
	}

	// 3 & 4. Dual cool ~7600-7800K grazing rim spotlights raking left/right across Rodin station metal chamfers.
	SpawnConfiguredSpotLight(RigCfg.RimSpotLeft, false);
	SpawnConfiguredSpotLight(RigCfg.RimSpotRight, false);

	// 5. Unshadowed ~6800K midground architectural wash lifting rear wall pilasters & booth stanchions out of #000000 void.
	SpawnConfiguredSpotLight(RigCfg.MidgroundWashSpot, false);
}
