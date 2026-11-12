// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/TareTitheGameMode.h"
#include "Actors/DirectorBoothActor.h"
#include "Actors/InspectorPawn.h"
#include "Actors/MonolithRigActor.h"
#include "Actors/ScaleAssemblyActor.h"
#include "Actors/StrayFloorCatchActor.h"
#include "Data/ShiftProgressionDataAsset.h"
#include "Engine/PointLight.h"
#include "Engine/World.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Subsystems/InspectionShiftSubsystem.h"

ATareTitheGameMode::ATareTitheGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	DefaultPawnClass = AInspectorPawn::StaticClass();
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

	// Provide overhead industrial cell illumination when launching in an unlit blank map.
	APointLight* CellWorkLight = World->SpawnActor<APointLight>(
		APointLight::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(120.0, -40.0, 290.0), FVector::OneVector),
		SpawnParams);
	if (IsValid(CellWorkLight) && IsValid(CellWorkLight->PointLightComponent))
	{
		CellWorkLight->PointLightComponent->SetIntensity(6500.0f);
		CellWorkLight->PointLightComponent->SetAttenuationRadius(950.0f);
		CellWorkLight->PointLightComponent->SetLightColor(FLinearColor(0.92f, 0.95f, 1.0f, 1.0f));
	}

	if (ShiftSubsystem->GetCurrentPhase() == EShiftPhase::WaitingForStations)
	{
		ShiftSubsystem->StartShiftFromContract(0);
	}
}
