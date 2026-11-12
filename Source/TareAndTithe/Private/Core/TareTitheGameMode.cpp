// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/TareTitheGameMode.h"
#include "Actors/DirectorBoothActor.h"
#include "Actors/InspectorPawn.h"
#include "Actors/MonolithRigActor.h"
#include "Actors/ScaleAssemblyActor.h"
#include "Actors/StrayFloorCatchActor.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/ShiftProgressionDataAsset.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"
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

	ApplyBaseline6GbVramScalability();

	if (IsValid(DefaultShiftProgression))
	{
		if (UWorld* World = GetWorld())
		{
			if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
			{
				ShiftSubsystem->SetShiftProgressionAsset(DefaultShiftProgression);
			}
		}
	}
}

void ATareTitheGameMode::StartPlay()
{
	if (IsValid(DefaultShiftProgression))
	{
		if (UWorld* World = GetWorld())
		{
			if (UInspectionShiftSubsystem* ShiftSubsystem = World->GetSubsystem<UInspectionShiftSubsystem>())
			{
				ShiftSubsystem->SetShiftProgressionAsset(DefaultShiftProgression);
			}
		}
	}

	Super::StartPlay();

	if (bSpawnGreyboxCellIfStationsMissing)
	{
		SpawnMissingGreyboxStationsIfNeeded();
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

	auto SpawnEnclosureSlab = [&](const FVector& Location, const FVector& Scale3D, bool bEnableSolidCatchCollision)
	{
		if (!IsValid(GreyboxCellCubeMesh))
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
		MeshComp->SetStaticMesh(GreyboxCellCubeMesh);
		MeshComp->SetWorldScale3D(Scale3D);
		MeshComp->SetGenerateOverlapEvents(false);
		if (bEnableSolidCatchCollision)
		{
			MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			MeshComp->SetCollisionObjectType(ECC_WorldStatic);
			MeshComp->SetCollisionResponseToAllChannels(ECR_Block);
		}
		else
		{
			MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		if (IsValid(PitMID))
		{
			MeshComp->SetMaterial(0, PitMID);
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

	// Solid pit floor sits well below AStrayFloorCatchActor (-35 cm) so stray chunks trigger StrayFloorCatch first while snapped remainders land solidly.
	SpawnEnclosureSlab(FVector(EnclosureCenterX, DropY, -95.0), FVector(EnclosureSpanScaleX, EnclosureSpanScaleY, 0.20), true);
	// Front wall behind Director Booth and rear cell wall behind X = -150 cm so pulled-back camera (-68 cm) never clips level geometry.
	SpawnEnclosureSlab(FVector(FrontWallX, DropY, WallCenterZ), FVector(0.20, EnclosureSpanScaleY, WallHeightScaleZ), false);
	SpawnEnclosureSlab(FVector(RearWallX, DropY, WallCenterZ), FVector(0.20, EnclosureSpanScaleY, WallHeightScaleZ), false);
	SpawnEnclosureSlab(FVector(EnclosureCenterX, DropY - SideWallOffsetY, WallCenterZ), FVector(EnclosureSpanScaleX, 0.20, WallHeightScaleZ), false);
	SpawnEnclosureSlab(FVector(EnclosureCenterX, DropY + SideWallOffsetY, WallCenterZ), FVector(EnclosureSpanScaleX, 0.20, WallHeightScaleZ), false);
	SpawnEnclosureSlab(FVector(EnclosureCenterX, DropY, CeilingZ), FVector(EnclosureSpanScaleX, EnclosureSpanScaleY, 0.20), false);

	// Overhead key spotlight aimed directly down the monolith & scale drop column.
	const FVector KeySpotLocation(DropX - 85.0, DropY - 25.0, 335.0);
	const FVector KeySpotTarget(DropX, DropY, 120.0);
	const FRotator KeySpotRotation = FRotationMatrix::MakeFromX(KeySpotTarget - KeySpotLocation).Rotator();

	ASpotLight* CellKeySpotLight = World->SpawnActor<ASpotLight>(
		ASpotLight::StaticClass(),
		FTransform(KeySpotRotation, KeySpotLocation, FVector::OneVector),
		SpawnParams);
	if (IsValid(CellKeySpotLight) && IsValid(CellKeySpotLight->SpotLightComponent))
	{
		CellKeySpotLight->SpotLightComponent->SetMobility(EComponentMobility::Movable);
		CellKeySpotLight->SpotLightComponent->SetIntensity(KeySpotLightIntensity);
		CellKeySpotLight->SpotLightComponent->SetAttenuationRadius(1050.0f);
		CellKeySpotLight->SpotLightComponent->SetInnerConeAngle(26.0f);
		CellKeySpotLight->SpotLightComponent->SetOuterConeAngle(56.0f);
		CellKeySpotLight->SpotLightComponent->SetLightColor(FLinearColor(0.95f, 0.96f, 1.0f, 1.0f));
	}

	// Front fill point light for crisp underside bevel legibility without extra shadow map cost or wall washout.
	APointLight* CellFillLight = World->SpawnActor<APointLight>(
		APointLight::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(95.0, DropY + 40.0, 215.0), FVector::OneVector),
		SpawnParams);
	if (IsValid(CellFillLight) && IsValid(CellFillLight->PointLightComponent))
	{
		CellFillLight->PointLightComponent->SetMobility(EComponentMobility::Movable);
		CellFillLight->PointLightComponent->SetIntensity(FillPointLightIntensity);
		CellFillLight->PointLightComponent->SetAttenuationRadius(880.0f);
		CellFillLight->PointLightComponent->SetCastShadows(false);
		CellFillLight->PointLightComponent->SetLightColor(FLinearColor(0.86f, 0.91f, 1.0f, 1.0f));
	}
}
