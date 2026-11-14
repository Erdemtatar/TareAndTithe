// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/UpscalerSettingsSubsystem.h"

#ifndef WITH_TARETITHE_DLSS
#define WITH_TARETITHE_DLSS 0
#endif

#if WITH_TARETITHE_DLSS
#include "DLSSLibrary.h"
#endif

#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "RHI.h"
#include "UnrealClient.h"

namespace
{
	constexpr const TCHAR* UpscalerIniSection = TEXT("/Script/TareAndTithe.UpscalerSettings");
	constexpr const TCHAR* UpscalerIniModeKey = TEXT("PreferredUpscalerMode");
	constexpr const TCHAR* UpscalerIniQualityKey = TEXT("PreferredScalabilityQuality");

	TAutoConsoleVariable<FString> CVarDiagUpscalerJson(
		TEXT("TareTithe.Diag.UpscalerJson"),
		TEXT("{}"),
		TEXT("Read-only graphics scalability and reconstruction state JSON"),
		ECVF_Default);

	template <typename ValueType>
	void SetRendererCVarSafe(const TCHAR* CVarName, ValueType InValue)
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(CVarName))
		{
			CVar->ReplaceCurrentPriorityAndTag(InValue, ECVF_SetByConsole, ECVF_SetByGameSetting);
		}
	}

	const TCHAR* ModeToDiagToken(ETareUpscalerMode Mode)
	{
		switch (Mode)
		{
		case ETareUpscalerMode::NativeTSR:
			return TEXT("NativeTSR");
		case ETareUpscalerMode::SuperRes_Auto:
			return TEXT("SuperRes_Auto");
		case ETareUpscalerMode::SuperRes_Quality:
			return TEXT("SuperRes_Quality");
		case ETareUpscalerMode::SuperRes_Balanced:
			return TEXT("SuperRes_Balanced");
		case ETareUpscalerMode::SuperRes_Performance:
			return TEXT("SuperRes_Performance");
		case ETareUpscalerMode::SuperRes_UltraPerformance:
			return TEXT("SuperRes_UltraPerformance");
		case ETareUpscalerMode::SuperRes_NativeAA:
			return TEXT("SuperRes_NativeAA");
		default:
			return TEXT("NativeTSR");
		}
	}

	const TCHAR* AvailabilityToDiagToken(ETareSuperResAvailability Availability)
	{
		switch (Availability)
		{
		case ETareSuperResAvailability::CompiledOut:
			return TEXT("CompiledOut");
		case ETareSuperResAvailability::UnsupportedHardwareOrDriver:
			return TEXT("UnsupportedHardwareOrDriver");
		case ETareSuperResAvailability::Available:
			return TEXT("Available");
		default:
			return TEXT("CompiledOut");
		}
	}

	bool IsValidSavedUpscalerModeInt(int32 RawValue)
	{
		return RawValue >= static_cast<int32>(ETareUpscalerMode::NativeTSR) &&
			RawValue <= static_cast<int32>(ETareUpscalerMode::SuperRes_NativeAA);
	}
}

TScriptInterface<IUpscalerControlInterface> IUpscalerControlInterface::Resolve(const UObject* WorldContextObject)
{
	if (!IsValid(WorldContextObject) || GEngine == nullptr)
	{
		return TScriptInterface<IUpscalerControlInterface>();
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (!IsValid(World))
	{
		return TScriptInterface<IUpscalerControlInterface>();
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!IsValid(GameInstance))
	{
		return TScriptInterface<IUpscalerControlInterface>();
	}

	UUpscalerSettingsSubsystem* Subsystem = GameInstance->GetSubsystem<UUpscalerSettingsSubsystem>();
	if (!IsValid(Subsystem))
	{
		return TScriptInterface<IUpscalerControlInterface>();
	}

	TScriptInterface<IUpscalerControlInterface> Result;
	Result.SetObject(Subsystem);
	Result.SetInterface(static_cast<IUpscalerControlInterface*>(Subsystem));
	return Result;
}

UUpscalerSettingsSubsystem::UUpscalerSettingsSubsystem()
{
	ActiveConfig = FUpscalerPresentationConfig().GetSanitized();
	CurrentSnapshot.ScalabilityQualityLevel = 2;
	CurrentSnapshot.RequestedUpscalerMode = ETareUpscalerMode::NativeTSR;
	CurrentSnapshot.ActiveUpscalerMode = ETareUpscalerMode::NativeTSR;
	CurrentSnapshot.SuperResAvailability = ETareSuperResAvailability::CompiledOut;
	CurrentSnapshot.EffectiveScreenPercentage = ActiveConfig.NativeTsrScaleHighPercent;
	CurrentSnapshot.NativeTsrScalePercent = ActiveConfig.NativeTsrScaleHighPercent;
	CurrentSnapshot.bDidFallbackToNativeTsr = false;
}

void UUpscalerSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ActiveConfig = FUpscalerPresentationConfig().GetSanitized();
	const int32 InitialQuality = ResolveInitialQualityLevel();
	const ETareSuperResAvailability Availability = EvaluateHardwareSupport();

	bHasExplicitUserModePreference = false;
	ETareUpscalerMode InitialRequestedMode = (Availability == ETareSuperResAvailability::Available)
		? ActiveConfig.DefaultModeOnSupportedHardware
		: ActiveConfig.FallbackModeOnUnsupportedHardware;

	if (Availability == ETareSuperResAvailability::Available && GConfig != nullptr && !GGameUserSettingsIni.IsEmpty())
	{
		int32 SavedModeInt = -1;
		if (GConfig->GetInt(UpscalerIniSection, UpscalerIniModeKey, SavedModeInt, GGameUserSettingsIni) &&
			IsValidSavedUpscalerModeInt(SavedModeInt))
		{
			InitialRequestedMode = static_cast<ETareUpscalerMode>(SavedModeInt);
			bHasExplicitUserModePreference = true;
		}
	}

	ApplyInternalState(InitialQuality, InitialRequestedMode, false);

	if (!IsRunningCommandlet() && FApp::CanEverRender() && !ViewportResizedDelegateHandle.IsValid())
	{
		ViewportResizedDelegateHandle = FViewport::ViewportResizedEvent.AddUObject(
			this,
			&UUpscalerSettingsSubsystem::HandleViewportResized);
	}
}

void UUpscalerSettingsSubsystem::Deinitialize()
{
	if (ViewportResizedDelegateHandle.IsValid())
	{
		FViewport::ViewportResizedEvent.Remove(ViewportResizedDelegateHandle);
		ViewportResizedDelegateHandle.Reset();
	}

	Super::Deinitialize();
}

void UUpscalerSettingsSubsystem::ApplyScalabilityQualityLevel(int32 InQualityLevel)
{
	const int32 ClampedQuality = FMath::Clamp(InQualityLevel, 0, 3);
	ApplyInternalState(ClampedQuality, CurrentSnapshot.RequestedUpscalerMode, true);
}

bool UUpscalerSettingsSubsystem::RequestUpscalerMode(ETareUpscalerMode InMode)
{
	bHasExplicitUserModePreference = true;
	return ApplyInternalState(CurrentSnapshot.ScalabilityQualityLevel, InMode, true);
}

ETareUpscalerMode UUpscalerSettingsSubsystem::CycleUpscalerMode()
{
	const ETareSuperResAvailability Availability = EvaluateHardwareSupport();
	if (Availability != ETareSuperResAvailability::Available)
	{
		ApplyInternalState(CurrentSnapshot.ScalabilityQualityLevel, ETareUpscalerMode::NativeTSR, false);
		return ETareUpscalerMode::NativeTSR;
	}

	static const ETareUpscalerMode OrderedCycle[7] = {
		ETareUpscalerMode::NativeTSR,
		ETareUpscalerMode::SuperRes_Auto,
		ETareUpscalerMode::SuperRes_Quality,
		ETareUpscalerMode::SuperRes_Balanced,
		ETareUpscalerMode::SuperRes_Performance,
		ETareUpscalerMode::SuperRes_UltraPerformance,
		ETareUpscalerMode::SuperRes_NativeAA
	};
	constexpr int32 CycleCount = UE_ARRAY_COUNT(OrderedCycle);

	int32 CurrentIdx = 0;
	for (int32 Idx = 0; Idx < CycleCount; ++Idx)
	{
		if (OrderedCycle[Idx] == CurrentSnapshot.ActiveUpscalerMode)
		{
			CurrentIdx = Idx;
			break;
		}
	}

	const FVector2D RenderRes = ResolveCurrentViewportPixelResolution();
	ETareUpscalerMode NextSelectedMode = ETareUpscalerMode::NativeTSR;

	for (int32 Offset = 1; Offset <= CycleCount; ++Offset)
	{
		const ETareUpscalerMode Candidate = OrderedCycle[(CurrentIdx + Offset) % CycleCount];
		if (Candidate == ETareUpscalerMode::SuperRes_UltraPerformance && !ActiveConfig.bIncludeUltraPerformanceInCycle)
		{
			continue;
		}

		if (Candidate == ETareUpscalerMode::NativeTSR)
		{
			NextSelectedMode = ETareUpscalerMode::NativeTSR;
			break;
		}

		float CandidateScreenPct = 0.0f;
		if (QueryHardwareModeScreenPercentage(Candidate, RenderRes, CandidateScreenPct))
		{
			NextSelectedMode = Candidate;
			break;
		}
	}

	bHasExplicitUserModePreference = true;
	ApplyInternalState(CurrentSnapshot.ScalabilityQualityLevel, NextSelectedMode, true);
	return CurrentSnapshot.ActiveUpscalerMode;
}

void UUpscalerSettingsSubsystem::ApplyPresentationUpscalerConfig(const FUpscalerPresentationConfig& InConfig)
{
	ActiveConfig = InConfig.GetSanitized();

	const ETareSuperResAvailability Availability = EvaluateHardwareSupport();
	ETareUpscalerMode DesiredMode = CurrentSnapshot.RequestedUpscalerMode;
	if (!bHasExplicitUserModePreference)
	{
		DesiredMode = (Availability == ETareSuperResAvailability::Available)
			? ActiveConfig.DefaultModeOnSupportedHardware
			: ActiveConfig.FallbackModeOnUnsupportedHardware;
	}

	ApplyInternalState(CurrentSnapshot.ScalabilityQualityLevel, DesiredMode, false);
}

ETareSuperResAvailability UUpscalerSettingsSubsystem::EvaluateHardwareSupport() const
{
#if WITH_TARETITHE_DLSS
	if (IsRunningCommandlet() || !FApp::CanEverRender() || GDynamicRHI == nullptr)
	{
		return ETareSuperResAvailability::UnsupportedHardwareOrDriver;
	}

	if (!IsRHIDeviceNVIDIA())
	{
		return ETareSuperResAvailability::UnsupportedHardwareOrDriver;
	}

	if (!FModuleManager::Get().IsModuleLoaded(TEXT("DLSS")))
	{
		return ETareSuperResAvailability::UnsupportedHardwareOrDriver;
	}

	if (UDLSSLibrary::QueryDLSSSupport() != UDLSSSupport::Supported || !UDLSSLibrary::IsDLSSSupported())
	{
		return ETareSuperResAvailability::UnsupportedHardwareOrDriver;
	}

	return ETareSuperResAvailability::Available;
#else
	return ETareSuperResAvailability::CompiledOut;
#endif
}

bool UUpscalerSettingsSubsystem::QueryHardwareModeScreenPercentage(
	ETareUpscalerMode CandidateMode,
	const FVector2D& RenderResolution,
	float& OutOptimalScreenPercentage) const
{
	OutOptimalScreenPercentage = 0.0f;

	if (CandidateMode == ETareUpscalerMode::NativeTSR)
	{
		OutOptimalScreenPercentage = GetNativeTsrScaleForQualityLevel(CurrentSnapshot.ScalabilityQualityLevel);
		return true;
	}

#if WITH_TARETITHE_DLSS
	if (EvaluateHardwareSupport() != ETareSuperResAvailability::Available)
	{
		return false;
	}

	if (CandidateMode == ETareUpscalerMode::SuperRes_NativeAA)
	{
		if (!UDLSSLibrary::IsDLSSModeSupported(UDLSSMode::DLAA))
		{
			return false;
		}
		OutOptimalScreenPercentage = 100.0f;
		return true;
	}

	UDLSSMode MappedMode = UDLSSMode::Off;
	switch (CandidateMode)
	{
	case ETareUpscalerMode::SuperRes_Auto:
		MappedMode = UDLSSMode::Auto;
		break;
	case ETareUpscalerMode::SuperRes_Quality:
		MappedMode = UDLSSMode::Quality;
		break;
	case ETareUpscalerMode::SuperRes_Balanced:
		MappedMode = UDLSSMode::Balanced;
		break;
	case ETareUpscalerMode::SuperRes_Performance:
		MappedMode = UDLSSMode::Performance;
		break;
	case ETareUpscalerMode::SuperRes_UltraPerformance:
		MappedMode = UDLSSMode::UltraPerformance;
		break;
	default:
		return false;
	}

	if (!UDLSSLibrary::IsDLSSModeSupported(MappedMode))
	{
		return false;
	}

	bool bIsSupported = false;
	float OptimalPct = 0.0f;
	bool bIsFixed = false;
	float MinPct = 50.0f;
	float MaxPct = 100.0f;
	float UnusedDeprecatedSharpness = 0.0f;

	UDLSSLibrary::GetDLSSModeInformation(
		MappedMode,
		RenderResolution,
		bIsSupported,
		OptimalPct,
		bIsFixed,
		MinPct,
		MaxPct,
		UnusedDeprecatedSharpness);

	if (!bIsSupported || OptimalPct <= 0.0f)
	{
		return false;
	}

	OutOptimalScreenPercentage = FMath::Clamp(OptimalPct, 25.0f, 100.0f);
	return true;
#else
	(void)RenderResolution;
	return false;
#endif
}

bool UUpscalerSettingsSubsystem::ResolveModeAndScreenPercentage(
	ETareUpscalerMode RequestedMode,
	ETareSuperResAvailability Availability,
	const FVector2D& RenderResolution,
	ETareUpscalerMode& OutActiveMode,
	float& OutEffectiveScreenPct,
	bool& OutDidFallbackToTsr) const
{
	const float TsrScale = GetNativeTsrScaleForQualityLevel(CurrentSnapshot.ScalabilityQualityLevel);
	OutDidFallbackToTsr = false;

	if (RequestedMode == ETareUpscalerMode::NativeTSR)
	{
		OutActiveMode = ETareUpscalerMode::NativeTSR;
		OutEffectiveScreenPct = TsrScale;
		return true;
	}

	if (Availability != ETareSuperResAvailability::Available)
	{
		OutActiveMode = ETareUpscalerMode::NativeTSR;
		OutEffectiveScreenPct = TsrScale;
		OutDidFallbackToTsr = true;
		return false;
	}

	float ResolvedOptimalPct = 0.0f;
	if (QueryHardwareModeScreenPercentage(RequestedMode, RenderResolution, ResolvedOptimalPct))
	{
		OutActiveMode = RequestedMode;
		OutEffectiveScreenPct = ResolvedOptimalPct;
		return true;
	}

	// PIE viewports and sub-1080p windows (< 2.03M pixels) disable Auto or return OptimalScreenPercentage == 0.0f:
	// transparently resolve SuperRes_Auto to SuperRes_Quality.
	if (RequestedMode == ETareUpscalerMode::SuperRes_Auto &&
		QueryHardwareModeScreenPercentage(ETareUpscalerMode::SuperRes_Quality, RenderResolution, ResolvedOptimalPct))
	{
		OutActiveMode = ETareUpscalerMode::SuperRes_Quality;
		OutEffectiveScreenPct = ResolvedOptimalPct;
		return true;
	}

	OutActiveMode = ETareUpscalerMode::NativeTSR;
	OutEffectiveScreenPct = TsrScale;
	OutDidFallbackToTsr = true;
	return false;
}

float UUpscalerSettingsSubsystem::GetNativeTsrScaleForQualityLevel(int32 QualityLevel) const
{
	switch (FMath::Clamp(QualityLevel, 0, 3))
	{
	case 0:
		return ActiveConfig.NativeTsrScaleLowPercent;
	case 1:
		return ActiveConfig.NativeTsrScaleMediumPercent;
	case 2:
		return ActiveConfig.NativeTsrScaleHighPercent;
	case 3:
	default:
		return ActiveConfig.NativeTsrScaleEpicPercent;
	}
}

FVector2D UUpscalerSettingsSubsystem::ResolveCurrentViewportPixelResolution() const
{
	if (GEngine != nullptr && IsValid(GEngine->GameViewport) && GEngine->GameViewport->Viewport != nullptr)
	{
		const FIntPoint ViewportSize = GEngine->GameViewport->Viewport->GetSizeXY();
		if (ViewportSize.X >= 64 && ViewportSize.Y >= 64)
		{
			return FVector2D(static_cast<float>(ViewportSize.X), static_cast<float>(ViewportSize.Y));
		}
	}

	if (const UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
	{
		const FIntPoint ConfiguredSize = (Settings->GetFullscreenMode() == EWindowMode::WindowedFullscreen)
			? Settings->GetDesktopResolution()
			: Settings->GetScreenResolution();
		if (ConfiguredSize.X >= 64 && ConfiguredSize.Y >= 64)
		{
			return FVector2D(static_cast<float>(ConfiguredSize.X), static_cast<float>(ConfiguredSize.Y));
		}
	}

	return FVector2D(1920.0f, 1080.0f);
}

int32 UUpscalerSettingsSubsystem::ResolveInitialQualityLevel() const
{
	if (GConfig != nullptr && !GGameUserSettingsIni.IsEmpty())
	{
		int32 SavedQuality = -1;
		if (GConfig->GetInt(UpscalerIniSection, UpscalerIniQualityKey, SavedQuality, GGameUserSettingsIni) &&
			SavedQuality >= 0 && SavedQuality <= 3)
		{
			return SavedQuality;
		}
	}

	if (UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
	{
		const int32 Overall = Settings->GetOverallScalabilityLevel();
		if (Overall >= 0 && Overall <= 3)
		{
			return Overall;
		}
		const int32 ShadowTier = Settings->GetShadowQuality();
		if (ShadowTier >= 0 && ShadowTier <= 3)
		{
			return ShadowTier;
		}
	}

	return 2;
}

bool UUpscalerSettingsSubsystem::ApplyInternalState(
	int32 InQualityLevel,
	ETareUpscalerMode InRequestedMode,
	bool bPersistUserPreference)
{
	TGuardValue<bool> ReentrancyGuard(bIsApplyingInternalState, true);

	const int32 ClampedQuality = FMath::Clamp(InQualityLevel, 0, 3);
	CurrentSnapshot.ScalabilityQualityLevel = ClampedQuality;

	const float TsrScale = GetNativeTsrScaleForQualityLevel(ClampedQuality);
	CurrentSnapshot.NativeTsrScalePercent = TsrScale;

	const ETareSuperResAvailability Availability = EvaluateHardwareSupport();
	CurrentSnapshot.SuperResAvailability = Availability;

	const ETareUpscalerMode EffectiveRequested = (Availability == ETareSuperResAvailability::Available)
		? InRequestedMode
		: ETareUpscalerMode::NativeTSR;
	CurrentSnapshot.RequestedUpscalerMode = EffectiveRequested;

	const FVector2D RenderRes = ResolveCurrentViewportPixelResolution();
	ETareUpscalerMode ResolvedActiveMode = ETareUpscalerMode::NativeTSR;
	float ResolvedScreenPct = TsrScale;
	bool bDidFallback = false;

	const bool bModeAppliedWithoutFallback = ResolveModeAndScreenPercentage(
		EffectiveRequested,
		Availability,
		RenderRes,
		ResolvedActiveMode,
		ResolvedScreenPct,
		bDidFallback);

	if (Availability != ETareSuperResAvailability::Available && InRequestedMode != ETareUpscalerMode::NativeTSR)
	{
		bDidFallback = true;
	}

	CurrentSnapshot.ActiveUpscalerMode = ResolvedActiveMode;
	CurrentSnapshot.EffectiveScreenPercentage = ResolvedScreenPct;
	CurrentSnapshot.bDidFallbackToNativeTsr = bDidFallback;

	if (!IsRunningCommandlet() && FApp::CanEverRender())
	{
		if (UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
		{
			Settings->SetOverallScalabilityLevel(ClampedQuality);
			if (ResolvedActiveMode == ETareUpscalerMode::NativeTSR)
			{
				Settings->SetResolutionScaleValueEx(TsrScale);
			}
			else
			{
				Settings->SetResolutionScaleValueEx(FMath::Clamp(ResolvedScreenPct, 50.0f, 100.0f));
			}
			Settings->ApplySettings(false);
		}

		if (ResolvedActiveMode == ETareUpscalerMode::NativeTSR)
		{
			// v8.9.0 Bug 6907033 workaround & clean TSR restoration:
			// Explicitly disable hardware SuperRes, force r.NGX.DLSS.Enable = 0, restore r.AntiAliasingMethod = 4 (TSR),
			// restore tier TSR r.ScreenPercentage, and restore native TSR tonemapper sharpen (0.35f).
#if WITH_TARETITHE_DLSS
			if (GDynamicRHI != nullptr && IsRHIDeviceNVIDIA() && FModuleManager::Get().IsModuleLoaded(TEXT("DLSS")))
			{
				UDLSSLibrary::EnableDLSS(false);
			}
#endif
			SetRendererCVarSafe(TEXT("r.NGX.DLSS.Enable"), 0);
			SetRendererCVarSafe(TEXT("r.NGX.DLSS.DenoiserMode"), 0);
			SetRendererCVarSafe(TEXT("r.AntiAliasingMethod"), 4);
			SetRendererCVarSafe(TEXT("r.TemporalAA.Upscaler"), 1);
			SetRendererCVarSafe(TEXT("r.ScreenPercentage"), TsrScale);
			SetRendererCVarSafe(TEXT("r.Tonemapper.Sharpen"), ActiveConfig.TonemapperSharpenNativeTsr);
		}
		else
		{
			// Re-enforce TemporalUpscaler hooks immediately after SetOverallScalabilityLevel (which sets r.AntiAliasingMethod=0 on Low)
			// and suppress tonemapper sharpen (0.0f) to prevent HDR laser seam negative-lobe ringing.
			SetRendererCVarSafe(TEXT("r.AntiAliasingMethod"), 4);
			SetRendererCVarSafe(TEXT("r.TemporalAA.Upscaler"), 1);
			SetRendererCVarSafe(TEXT("r.NGX.DLSS.DenoiserMode"), 0);
#if WITH_TARETITHE_DLSS
			UDLSSLibrary::EnableDLSS(true);
#endif
			SetRendererCVarSafe(TEXT("r.ScreenPercentage"), ResolvedScreenPct);
			SetRendererCVarSafe(TEXT("r.Tonemapper.Sharpen"), ActiveConfig.TonemapperSharpenSuperResActive);
		}
	}

	if (bPersistUserPreference && Availability == ETareSuperResAvailability::Available)
	{
		PersistPreferencesToIni(ClampedQuality, EffectiveRequested);
	}
	else if (bPersistUserPreference)
	{
		PersistPreferencesToIni(ClampedQuality, ETareUpscalerMode::NativeTSR);
	}

	BroadcastSnapshotUpdate();
	return bModeAppliedWithoutFallback && !bDidFallback;
}

void UUpscalerSettingsSubsystem::PersistPreferencesToIni(int32 InQualityLevel, ETareUpscalerMode InRequestedMode) const
{
	if (IsRunningCommandlet() || GConfig == nullptr || GGameUserSettingsIni.IsEmpty())
	{
		return;
	}

	GConfig->SetInt(UpscalerIniSection, UpscalerIniQualityKey, FMath::Clamp(InQualityLevel, 0, 3), GGameUserSettingsIni);
	GConfig->SetInt(UpscalerIniSection, UpscalerIniModeKey, static_cast<int32>(InRequestedMode), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UUpscalerSettingsSubsystem::BroadcastSnapshotUpdate()
{
	const FString DiagJson = FString::Printf(
		TEXT("{\"quality\":%d,\"requested\":\"%s\",\"active\":\"%s\",\"availability\":\"%s\",\"screen_pct\":%.2f,\"tsr_pct\":%.2f,\"fallback\":%s}"),
		CurrentSnapshot.ScalabilityQualityLevel,
		ModeToDiagToken(CurrentSnapshot.RequestedUpscalerMode),
		ModeToDiagToken(CurrentSnapshot.ActiveUpscalerMode),
		AvailabilityToDiagToken(CurrentSnapshot.SuperResAvailability),
		CurrentSnapshot.EffectiveScreenPercentage,
		CurrentSnapshot.NativeTsrScalePercent,
		CurrentSnapshot.bDidFallbackToNativeTsr ? TEXT("true") : TEXT("false"));
	CVarDiagUpscalerJson->Set(*DiagJson, ECVF_SetByCode);

	OnGraphicsStateChangedNative.Broadcast(CurrentSnapshot);
	OnGraphicsStateChangedDynamic.Broadcast(CurrentSnapshot);
}

void UUpscalerSettingsSubsystem::HandleViewportResized(FViewport* Viewport, uint32 /*UnusedResizeCode*/)
{
	if (bIsApplyingInternalState || Viewport == nullptr)
	{
		return;
	}

	if (CurrentSnapshot.SuperResAvailability != ETareSuperResAvailability::Available ||
		CurrentSnapshot.RequestedUpscalerMode == ETareUpscalerMode::NativeTSR)
	{
		return;
	}

	const FIntPoint NewSize = Viewport->GetSizeXY();
	if (NewSize.X < 64 || NewSize.Y < 64)
	{
		return;
	}

	ETareUpscalerMode ReevaluatedMode = CurrentSnapshot.ActiveUpscalerMode;
	float ReevaluatedScreenPct = CurrentSnapshot.EffectiveScreenPercentage;
	bool bDidFallback = false;
	ResolveModeAndScreenPercentage(
		CurrentSnapshot.RequestedUpscalerMode,
		CurrentSnapshot.SuperResAvailability,
		FVector2D(static_cast<float>(NewSize.X), static_cast<float>(NewSize.Y)),
		ReevaluatedMode,
		ReevaluatedScreenPct,
		bDidFallback);

	if (ReevaluatedMode != CurrentSnapshot.ActiveUpscalerMode ||
		!FMath::IsNearlyEqual(ReevaluatedScreenPct, CurrentSnapshot.EffectiveScreenPercentage, 0.05f))
	{
		ApplyInternalState(CurrentSnapshot.ScalabilityQualityLevel, CurrentSnapshot.RequestedUpscalerMode, false);
	}
}
