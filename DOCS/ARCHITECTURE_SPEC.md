# TARE & TITHE — UNREAL ENGINE 5.8 C++ TECHNICAL ARCHITECTURE SPECIFICATION

- **Contest Theme:** `DIRECTOR'S CUT`
- **Engine & Target:** Unreal Engine 5.8 (Standard Launcher), Win64 Standalone (`Development` / `Shipping`), `< 1 GB` compressed `.zip`
- **Hardware Baseline:** 6 GB VRAM GPU budget, Native UE5 TSR (`UGameUserSettings`), Zero Third-Party Upscalers, Zero Skeletal Meshes / Animations
- **Compliance Constraints:** 100% English strings/identifiers/logs, Zero third-party trademarks in UI/credits, ESRB Teen (13+) industrial framing (unlicensed deep-earth bismuth-tungsten slag encasing volatile synthetic isotope veins; zero illegal substances or gore)

---

## 1. PROJECT DESCRIPTOR (`.uproject`) & MODULE RULES (`TareAndTithe.Build.cs`)

### 1.1. Exact `TareAndTithe.uproject` Plugin Manifest
Enable ONLY runtime-safe built-in plugins. Never depend on `ModelingToolsEditorMode` or `GeometryScriptingEditor`.

```json
{
  "FileVersion": 3,
  "EngineAssociation": "5.8",
  "Category": "",
  "Description": "TARE & TITHE - Stationary Industrial Monolith Slicing Simulation",
  "Modules": [
    {
      "Name": "TareAndTithe",
      "Type": "Runtime",
      "LoadingPhase": "Default"
    }
  ],
  "Plugins": [
    {
      "Name": "EnhancedInput",
      "Enabled": true
    },
    {
      "Name": "GeometryProcessing",
      "Enabled": true
    },
    {
      "Name": "GeometryScripting",
      "Enabled": true
    },
    {
      "Name": "Metasound",
      "Enabled": true
    },
    {
      "Name": "Niagara",
      "Enabled": true
    }
  ]
}
```

### 1.2. Exact `Source/TareAndTithe/TareAndTithe.Build.cs` Specification
- Must call `SetupModulePhysicsSupport(Target);` to configure Chaos/PhysicsCore defines cleanly without fragile internal engine paths.
- Must expose `Public` and `Private` include roots so all `#include` directives use clean module-relative paths (e.g., `#include "Core/TareTitheTypes.h"`).
- **STRICT BAN:** Never add `UnrealEd`, `GeometryScriptingEditor`, or `ModelingComponentsEditorOnly` to runtime dependency lists.

```csharp
using UnrealBuildTool;
using System.IO;

public class TareAndTithe : ModuleRules
{
    public TareAndTithe(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(new string[] {
            Path.Combine(ModuleDirectory, "Public")
        });

        PrivateIncludePaths.AddRange(new string[] {
            Path.Combine(ModuleDirectory, "Private")
        });

        SetupModulePhysicsSupport(Target);

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "GeometryCore",
            "DynamicMesh",
            "GeometryFramework",
            "GeometryScriptingCore",
            "PhysicsCore",
            "AudioExtensions",
            "MetasoundEngine",
            "Niagara",
            "UMG"
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
            "Slate",
            "SlateCore",
            "RenderCore"
        });
    }
}
```

---

## 2. EXACT SOURCE DIRECTORY TREE & FILE PATHS

Every class has a strict Single Responsibility boundary. `AActor` and `APawn` subclasses are thin composition shells; all domain logic lives in `UActorComponent` or `UWorldSubsystem` classes.

```text
Source/TareAndTithe/
├── TareAndTithe.Build.cs
├── Public/
│   ├── TareAndTithe.h
│   ├── Core/
│   │   ├── TareTitheTypes.h
│   │   └── TareTitheGameMode.h
│   ├── Interfaces/
│   │   ├── SliceableTargetInterface.h
│   │   ├── MassPayloadInterface.h
│   │   ├── QuotaReceiverInterface.h
│   │   └── TelemetryPublisherInterface.h
│   ├── Data/
│   │   ├── MonolithContractDataAsset.h
│   │   └── ShiftProgressionDataAsset.h
│   ├── Subsystems/
│   │   ├── ChunkPoolSubsystem.h
│   │   └── InspectionShiftSubsystem.h
│   ├── Components/
│   │   ├── MonolithSliceComponent.h
│   │   ├── VeinScannerComponent.h
│   │   ├── ClampTorqueSolverComponent.h
│   │   ├── LaserPlaneAimComponent.h
│   │   ├── LaserCapacitorComponent.h
│   │   ├── DiverterGateComponent.h
│   │   ├── ScaleWeighingComponent.h
│   │   └── AudioTelemetryBridgeComponent.h
│   └── Actors/
│       ├── InspectorPawn.h
│       ├── MonolithRigActor.h
│       ├── SeveredChunkActor.h
│       ├── ScaleAssemblyActor.h
│       ├── StrayFloorCatchActor.h
│       └── DirectorBoothActor.h
└── Private/
    ├── TareAndTithe.cpp
    ├── Core/
    │   └── TareTitheGameMode.cpp
    ├── Data/
    │   ├── MonolithContractDataAsset.cpp
    │   └── ShiftProgressionDataAsset.cpp
    ├── Subsystems/
    │   ├── ChunkPoolSubsystem.cpp
    │   └── InspectionShiftSubsystem.cpp
    ├── Components/
    │   ├── MonolithSliceComponent.cpp
    │   ├── VeinScannerComponent.cpp
    │   ├── ClampTorqueSolverComponent.cpp
    │   ├── LaserPlaneAimComponent.cpp
    │   ├── LaserCapacitorComponent.cpp
    │   ├── DiverterGateComponent.cpp
    │   ├── ScaleWeighingComponent.cpp
    │   └── AudioTelemetryBridgeComponent.cpp
    └── Actors/
        ├── InspectorPawn.cpp
        ├── MonolithRigActor.cpp
        ├── SeveredChunkActor.cpp
        ├── ScaleAssemblyActor.cpp
        ├── StrayFloorCatchActor.cpp
        └── DirectorBoothActor.cpp
```

*(Note: `TareTitheTypes.h` and the 4 `Interfaces/*.h` headers are header-only contracts using `GENERATED_BODY()` inline without separate `.cpp` files.)*

---

## 3. EXACT TYPES, ISP INTERFACES & DATA ASSET CONTRACTS

### 3.1. `Public/Core/TareTitheTypes.h`
Define all shared enums, structs, and multicast delegate signatures here so components never `#include` concrete peer headers.

```cpp
#pragma once

#include "CoreMinimal.h"
#include "TareTitheTypes.generated.h"

UENUM(BlueprintType)
enum class EMonolithBaseShape : uint8
{
    Box                 UMETA(DisplayName = "Box"),
    OctagonalPrism      UMETA(DisplayName = "Octagonal Prism"),
    AsymmetricLBlock    UMETA(DisplayName = "Asymmetric L-Block"),
    SteppedCore         UMETA(DisplayName = "Stepped Core"),
    CustomStaticMesh    UMETA(DisplayName = "Custom Watertight StaticMesh")
};

UENUM(BlueprintType)
enum class EDiverterRoute : uint8
{
    TitheScale  UMETA(DisplayName = "Tithe Scale (Director's Cut)"),
    TareChute   UMETA(DisplayName = "Tare Scrap Chute")
};

UENUM(BlueprintType)
enum class EChunkRoutingState : uint8
{
    PooledInactive  UMETA(DisplayName = "Pooled Inactive"),
    InFlight        UMETA(DisplayName = "In Flight"),
    OnScalePan      UMETA(DisplayName = "On Scale Pan"),
    ConsumedAsTare  UMETA(DisplayName = "Consumed As Tare"),
    LostAsStrayTare UMETA(DisplayName = "Lost As Stray Tare")
};

UENUM(BlueprintType)
enum class ECommitSliceOutcome : uint8
{
    Success             UMETA(DisplayName = "Success"),
    MissedTarget        UMETA(DisplayName = "Missed Target (Zero Intersection)"),
    DetonatedVein       UMETA(DisplayName = "Volatile Vein Detonated"),
    InvalidMeshState    UMETA(DisplayName = "Invalid Mesh State")
};

UENUM(BlueprintType)
enum class EContractFaultReason : uint8
{
    None                        UMETA(DisplayName = "None"),
    VolatileDetonation          UMETA(DisplayName = "Volatile Isotope Vein Severed"),
    ClampStructuralSnap         UMETA(DisplayName = "Overhead Clamp Bolt Shear"),
    ScaleContaminated           UMETA(DisplayName = "Contaminated Mass On Tithe Scale"),
    QuotaOvershoot              UMETA(DisplayName = "Scale Mass Exceeded Upper Quota"),
    TareAllowanceExceeded       UMETA(DisplayName = "Tare Waste Exceeded Allowance"),
    MathematicallyUnreachable   UMETA(DisplayName = "Remaining Mass Insufficient For Quota"),
    ManualOperatorReset         UMETA(DisplayName = "Manual Operator Reset [R]")
};

UENUM(BlueprintType)
enum class EShiftVerdictType : uint8
{
    ContractPassed      UMETA(DisplayName = "Contract Passed"),
    ContractFaultReset  UMETA(DisplayName = "Contract Fault & Reset"),
    ShiftCompleted      UMETA(DisplayName = "Full Shift Completed")
};

UENUM(BlueprintType)
enum class ETelemetryStreamId : uint8
{
    VeinProximity   UMETA(DisplayName = "Vein Proximity [0..1]"),
    ClampStrain     UMETA(DisplayName = "Clamp Torque Strain [0..1]"),
    ClampFatigue    UMETA(DisplayName = "Clamp Bolt Fatigue [0..1]"),
    LaserCharge     UMETA(DisplayName = "Laser Capacitor Charge [0..1]"),
    ScaleFillRatio  UMETA(DisplayName = "Scale Quota Fill Ratio [0..1]")
};

UENUM(BlueprintType)
enum class ETelemetryImpulseId : uint8
{
    SliceCommitted      UMETA(DisplayName = "Laser Slice Committed"),
    SliverVaporized     UMETA(DisplayName = "Degenerate Sliver Vaporized"),
    ChunkScaleImpact    UMETA(DisplayName = "Chunk Landed On Scale"),
    ChunkTareDisposed   UMETA(DisplayName = "Chunk Routed To Tare"),
    DiverterToggled     UMETA(DisplayName = "Diverter Gate Toggled"),
    VerdictStamped      UMETA(DisplayName = "Quota Verdict Stamped"),
    FaultTriggered      UMETA(DisplayName = "Contract Fault Triggered")
};

USTRUCT(BlueprintType)
struct FVolatileVeinDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vein")
    FVector LocalStartCm = FVector(0.0f, 0.0f, -20.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vein")
    FVector LocalEndCm = FVector(0.0f, 0.0f, 20.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vein", meta = (ClampMin = "0.5", ClampMax = "25.0"))
    float RadiusCm = 3.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vein", meta = (ClampMin = "2.0", ClampMax = "60.0"))
    float WarningRangeCm = 18.0f;

    FVector GetMidpointCm() const { return 0.5f * (LocalStartCm + LocalEndCm); }
};

USTRUCT(BlueprintType)
struct FSlicePreviewTelemetry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Slice")
    bool bIntersectsMonolith = false;

    UPROPERTY(BlueprintReadOnly, Category = "Slice")
    float VeinProximityNormalized = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Slice")
    float MinCapsuleClearanceCm = MAX_FLT;

    UPROPERTY(BlueprintReadOnly, Category = "Slice")
    bool bWillDetonate = false;
};

USTRUCT(BlueprintType)
struct FSliceResultSummary
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Slice")
    ECommitSliceOutcome Outcome = ECommitSliceOutcome::InvalidMeshState;

    UPROPERTY(BlueprintReadOnly, Category = "Slice")
    float NewRemainderMassKg = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Slice")
    FVector NewRemainderLocalCoMCm = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Slice")
    float SpawnedChunkMassKg = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Slice")
    float VaporizedSliverMassKg = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Slice")
    float CutCrossSectionAreaCm2 = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Slice")
    int32 SpawnedChunkCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Slice")
    FVector WorldCutCentroid = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FContractRuntimeSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Shift")
    int32 ContractIndex = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Shift")
    float TargetTitheMassKg = 10.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Shift")
    float QuotaToleranceKg = 0.5f;

    UPROPERTY(BlueprintReadOnly, Category = "Shift")
    float SettledScaleMassKg = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Shift")
    float RemainderMassKg = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Shift")
    float InFlightMassKg = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Shift")
    float AccumulatedTareWasteKg = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Shift")
    float MaxAllowedTareWasteKg = 15.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Shift")
    bool bIsWithinQuotaWindow = false;

    UPROPERTY(BlueprintReadOnly, Category = "Shift")
    bool bScaleSettledForStamp = false;
};

// Native C++ Telemetry Signatures (Zero UHT overhead, zero concrete coupling)
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTelemetryStreamSignature, ETelemetryStreamId /*StreamId*/, float /*NormalizedValue*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnTelemetryImpulseSignature, ETelemetryImpulseId /*ImpulseId*/, float /*NormalizedMagnitude*/, const FVector& /*WorldLocation*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnContractSnapshotUpdatedSignature, const FContractRuntimeSnapshot&, Snapshot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShiftVerdictSignature, EShiftVerdictType, Verdict, EContractFaultReason, FaultReason);
```

### 3.2. Narrow ISP `UInterface` Contracts (`Public/Interfaces/*.h`)
**CRITICAL RULE:** Never `Cast<AConcreteActor>()` or `Cast<UConcreteComponent>()` across systems. Check `Obj->Implements<U*Interface>()` and invoke `I*Interface::Execute_*` (or pure C++ interface pointers on `ITelemetryPublisherInterface`).

1. **`Public/Interfaces/SliceableTargetInterface.h` (`USliceableTargetInterface` / `ISliceableTargetInterface`):**
   - `UFUNCTION(BlueprintNativeEvent, Category = "Slice")`
     `FSlicePreviewTelemetry PreviewSlicePlane(const FVector& PlaneOriginWorld, const FVector& PlaneNormalWorld);`
   - `UFUNCTION(BlueprintNativeEvent, Category = "Slice")`
     `FSliceResultSummary CommitSlicePlane(const FVector& PlaneOriginWorld, const FVector& PlaneNormalWorld);`
   - `UFUNCTION(BlueprintNativeEvent, Category = "Slice")`
     `FBox GetTargetWorldBounds() const;`

2. **`Public/Interfaces/MassPayloadInterface.h` (`UMassPayloadInterface` / `IMassPayloadInterface`):**
   - `UFUNCTION(BlueprintNativeEvent, Category = "Payload")`
     `float GetAnalyticalMassKg() const;`
   - `UFUNCTION(BlueprintNativeEvent, Category = "Payload")`
     `bool IsVolatileContaminated() const;`
   - `UFUNCTION(BlueprintNativeEvent, Category = "Payload")`
     `EChunkRoutingState GetRoutingState() const;`
   - `UFUNCTION(BlueprintNativeEvent, Category = "Payload")`
     `void NotifyRoutingStateChanged(EChunkRoutingState NewState);`
   - `UFUNCTION(BlueprintNativeEvent, Category = "Payload")`
     `void ReturnToChunkPool();`

3. **`Public/Interfaces/QuotaReceiverInterface.h` (`UQuotaReceiverInterface` / `IQuotaReceiverInterface`):**
   - `UFUNCTION(BlueprintNativeEvent, Category = "Quota")`
     `float GetSettledMassKg() const;`
   - `UFUNCTION(BlueprintNativeEvent, Category = "Quota")`
     `bool IsReceiverSettled() const;`
   - `UFUNCTION(BlueprintNativeEvent, Category = "Quota")`
     `bool HasContaminationFault() const;`
   - `UFUNCTION(BlueprintNativeEvent, Category = "Quota")`
     `EDiverterRoute GetActiveDiverterRoute() const;`
   - `UFUNCTION(BlueprintNativeEvent, Category = "Quota")`
     `void ResetReceiverState();`

4. **`Public/Interfaces/TelemetryPublisherInterface.h` (`UTelemetryPublisherInterface` / `ITelemetryPublisherInterface`):**
   - Uses `UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))` because UHT forbids C++ multicast delegates inside `UFUNCTION`:
   - `virtual FOnTelemetryStreamSignature& GetTelemetryStreamDelegate() = 0;`
   - `virtual FOnTelemetryImpulseSignature& GetTelemetryImpulseDelegate() = 0;`

### 3.3. Data Assets (`Public/Data/*.h`)

#### `UMonolithContractDataAsset` (`: public UPrimaryDataAsset`)
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity") FText ContractCode;`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Geometry") EMonolithBaseShape BaseShape = EMonolithBaseShape::Box;`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Geometry", meta = (EditCondition = "BaseShape == EMonolithBaseShape::CustomStaticMesh")) TSoftObjectPtr<UStaticMesh> CustomMonolithMesh;`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Geometry") FVector DimensionsCm = FVector(40.0f, 40.0f, 100.0f);`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Geometry", meta = (ClampMin = "10.0", ClampMax = "500.0")) float BoxProjectionUVScaleCm = 100.0f;`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Geometry", meta = (ClampMin = "10.0", ClampMax = "500.0")) float CutCapUVWorldDimensionCm = 100.0f;`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Physics", meta = (ClampMin = "0.5", ClampMax = "22.0")) float DensityGramsPerCm3 = 2.5f;`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quota", meta = (ClampMin = "0.5")) float TargetTitheMassKg = 10.0f;`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quota", meta = (ClampMin = "0.05")) float QuotaToleranceKg = 0.75f;`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quota", meta = (ClampMin = "0.0")) float MaxAllowedTareWasteKg = 12.0f;`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quota") bool bAllowDiverterToggle = true;`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clamp", meta = (ClampMin = "1.0")) float ClampMaxSafeTorqueNm = 45.0f;`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clamp", meta = (ClampMin = "0.5", ClampMax = "15.0")) float ClampFatigueGraceSeconds = 4.5f;`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clamp", meta = (ClampMin = "0.5", ClampMax = "15.0")) float ClampFatigueCooldownSeconds = 3.0f;`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clamp", meta = (ClampMin = "5.0")) float ClampSpringStiffness = 180.0f;`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clamp", meta = (ClampMin = "1.0")) float ClampSpringDamping = 24.0f;`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Veins") TArray<FVolatileVeinDefinition> HiddenVeins;`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Materials") TSoftObjectPtr<UMaterialInterface> OuterShellMaterial;`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Materials") TSoftObjectPtr<UMaterialInterface> MoltenCutCapMaterial;`
- Override `virtual FPrimaryAssetId GetPrimaryAssetId() const override;`
- **MANDATORY PACKAGING GUARD:** Wrap `IsDataValid` in BOTH `.h` and `.cpp` inside `#if WITH_EDITOR`:
  ```cpp
  #if WITH_EDITOR
  virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
  #endif
  ```
  In `MonolithContractDataAsset.cpp`, include `#include "Misc/DataValidation.h"` inside `#if WITH_EDITOR` and validate: `TargetTitheMassKg > QuotaToleranceKg`, `DimensionsCm.GetMin() >= 5.0f`, and if `BaseShape == EMonolithBaseShape::CustomStaticMesh`, `!CustomMonolithMesh.IsNull()`.

#### `UShiftProgressionDataAsset` (`: public UPrimaryDataAsset`)
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shift") TArray<TSoftObjectPtr<UMonolithContractDataAsset>> OrderedContracts;` *(6 contracts: 2 per Act)*
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Laser", meta = (ClampMin = "0.1", ClampMax = "2.0")) float LaserCapacitorChargeSeconds = 0.35f;`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Laser", meta = (ClampMin = "0.05", ClampMax = "2.0")) float LaserCooldownSeconds = 0.25f;`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slicer", meta = (ClampMin = "5.0", ClampMax = "250.0")) float MinSliverVolumeCm3 = 25.0f;`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slicer", meta = (ClampMin = "0.2", ClampMax = "5.0")) float MinChunkAxisThicknessCm = 0.6f;`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slicer", meta = (ClampMin = "1.0", ClampMax = "100.0")) float ChunkSeparationSpeedCmPerSec = 15.0f;`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Safety", meta = (ClampMin = "1.5", ClampMax = "10.0")) float MaxInFlightWatchdogSeconds = 4.0f;`
- `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Safety", meta = (ClampMin = "0.2", ClampMax = "3.0")) float FaultAutoResetDelaySeconds = 1.2f;`

---

## 4. EXACT UE 5.8 GEOMETRY C++ HEADERS & 8-STEP SLICE PIPELINE

Do NOT search the engine source tree. Include these exact headers in `Private/Components/MonolithSliceComponent.cpp`:

```cpp
#include "Components/MonolithSliceComponent.h"
#include "UDynamicMesh.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshSubmesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "DynamicMesh/MeshTransforms.h"
#include "DynamicMesh/Operations/MergeCoincidentMeshEdges.h"
#include "Operations/MeshPlaneCut.h"
#include "Selections/MeshConnectedComponents.h"
#include "Spatial/FastWinding.h"
#include "Intersection/IntrTriangle3AxisAlignedBox3.h"
#include "ConstrainedDelaunay2.h"
#include "MeshQueries.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshUVFunctions.h"
#include "GeometryScript/CollisionFunctions.h"
```

### Monolith Initialization (Procedural + Optional `UStaticMesh` + Non-Swimming UV0)
1. Reset `RemainderMeshComponent->GetDynamicMesh()`.
2. If `Contract->BaseShape == EMonolithBaseShape::CustomStaticMesh`, synchronously load `Contract->CustomMonolithMesh`, copy via `UGeometryScriptLibrary_MeshAssetFunctions::CopyMeshFromStaticMesh`, and inside `EditMesh` run `UE::Geometry::FMergeCoincidentMeshEdges(&Mesh)` (`MergeVertexTolerance = 0.001`) followed by verifying `Mesh.IsClosed()`. Otherwise, build the watertight procedural shape via `UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox` / `AppendCylinder` compositions (`<= 3,500` triangles).
3. Bake **static object-space Box-Projection UVs** into `UV0` once at spawn via `UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromBoxProjection` using `FTransform(FVector::ZeroVector)` scaled to `Contract->BoxProjectionUVScaleCm`. Because UVs are baked into vertex overlays before any slicing or rigid-body motion, tilting the clamp or translating chunk vertices NEVER causes texture swimming.
4. Inside `EditMesh`, call `Mesh.EnableAttributes(); Mesh.Attributes()->EnableMaterialID();` and initialize all outer triangles (`Mesh.TriangleIndicesItr()`) to `MaterialID = 0` (`OuterShellMaterial`), configuring Slot 0 = `OuterShellMaterial` and Slot 1 = `MoltenCutCapMaterial` on both `RemainderMeshComponent` and pooled `ASeveredChunkActor` mesh components.

### 8-Step Runtime Slicing Pipeline (`UMonolithSliceComponent::ExecuteSlice`)
- **Step 0 — Zero-Allocation Miss Check:** Transform `(PlaneOriginWorld, PlaneNormalWorld)` into `RemainderMeshComponent->GetComponentTransform()` local space `(LocalOrigin, LocalNormal)`. Iterate `RemainderMesh.VertexIndicesItr()` computing $s_v = (\mathbf{v} - \mathbf{p}_{\text{local}}) \cdot \hat{\mathbf{n}}_{\text{local}}$. If `MinS >= -1e-3` or `MaxS <= 1e-3`, return `ECommitSliceOutcome::MissedTarget` immediately without cloning any mesh.
- **Step 1 — Pre-Cut Vein Detonation Gate:** Query `UVeinScannerComponent::EvaluatePlane(LocalOrigin, LocalNormal)`. If `bWillDetonate == true` ($d_{\text{clear}} \le 0.0\text{ cm}$), broadcast `ETelemetryImpulseId::FaultTriggered`, notify `UInspectionShiftSubsystem` with `EContractFaultReason::VolatileDetonation`, and abort before running mesh booleans.
- **Step 2 — Dual Half-Space `FMeshPlaneCut` & `MaterialID = 1` Cap Assignment:**
  - Make two `UE::Geometry::FDynamicMesh3` working copies of the current `RemainderMesh`: `HalfA` (cut with `+LocalNormal`) and `HalfB` (cut with `-LocalNormal`).
  - On each copy:
    ```cpp
    UE::Geometry::FMeshPlaneCut Cut(&WorkMesh, LocalOrigin, CutNormal);
    Cut.UVScaleFactor = 1.0 / FMath::Max(1.0, (double)Contract->CutCapUVWorldDimensionCm);
    Cut.Cut();
    Cut.HoleFill(UE::Geometry::ConstrainedDelaunayTriangulate<double>, true);
    ```
  - Iterate `Cut.HoleFillTriangles` (`const TArray<int>& LoopTris` -> `int Tid`): stamp `MatIDAttrib->SetValue(Tid, 1)` on every new cap triangle and accumulate `WorkMesh.GetTriArea(Tid)` on `HalfA` to compute `CutCrossSectionAreaCm2`.
- **Step 3 — Connected Island Decomposition:** On both `HalfA` and `HalfB`, run `UE::Geometry::FMeshConnectedComponents Components(&WorkMesh); Components.FindConnectedTriangles();`. Discard any degenerate shell with `Component.Indices.Num() < 4`. Extract multi-island splits into standalone `FDynamicMesh3` instances via `UE::Geometry::FDynamicSubmesh3(&WorkMesh, Component.Indices, (int32)UE::Geometry::EMeshComponents::All, true)`.
- **Step 4 — Spatial Acceleration Build (`AABBTree` + `FastWinding`):** For each candidate `FDynamicMesh3` island, construct:
  - `UE::Geometry::FDynamicMeshAABBTree3 IslandBVH(&IslandMesh, true);`
  - `UE::Geometry::TFastWindingTree<UE::Geometry::FDynamicMesh3> IslandWinding(&IslandBVH, true);`
- **Step 5 — Exact Zero-AABB-Error Clamp Grip Classification:** Let `GripBox3d` (`UE::Geometry::FAxisAlignedBox3d`) be the overhead clamp jaw grip volume in Remainder local space:
  1. Broadphase reject: if `!IslandBVH.GetBoundingBox().Intersects(GripBox3d)`, the island is **Detached**.
  2. Exact surface SAT check: traverse `IslandBVH` overlapping nodes and test exact triangle-box intersection via `UE::Geometry::TIntrTriangle3AxisAlignedBox3<double>(Triangle3d, GripBox3d).Test()`. If any triangle intersects -> **Gripped**.
  3. Interior engulfment fallback: if zero surface triangles intersect, test `IslandWinding.IsInside(GripBox3d.Center(), 0.5)`. If `true` -> **Gripped**; else -> **Detached**.
  - Merge all **Gripped** islands into the updated kinematic `RemainderMeshComponent` (keep collision `NoCollision` or query-only kinematic; never enable Chaos simulation on Remainder).
- **Step 6 — Analytical Mass & CoM Extraction:** For the Remainder and each Detached island, call:
  `UE::Geometry::TMeshQueries<UE::Geometry::FDynamicMesh3>::GetVolumeAreaCenter(Mesh, VolumeCm3, AreaCm2, LocalCoMCm);`
  Clamp `VolumeCm3 = FMath::Max(0.0, VolumeCm3)` and compute exact deterministic mass:
  $$M_{\text{kg}} = V_{\text{cm}^3} \cdot \rho_{\text{g/cm}^3} \cdot 10^{-3}$$
- **Step 7 — Spatial Vein Midpoint Partitioning (`FastWinding`):** Before recentering detached islands, test every active `FVolatileVeinDefinition` midpoint `M = Vein.GetMidpointCm()` (cast to `FVector3d`) via `IslandWinding.IsInside(M, 0.5)`:
  - Veins inside Gripped islands stay in `UVeinScannerComponent`'s active Remainder vein array.
  - Veins inside a Detached island mark that detached chunk `bIsContaminated = true` and are removed from the Remainder's active vein array (eliminating phantom MetaSounds warnings in empty air).
- **Step 8 — Sliver Vaporization vs. CoM-Recentered Pooled Chaos Chunk Spawn:**
  - Compute axis extents of `IslandBVH.GetBoundingBox()`. If `VolumeCm3 < ShiftConfig->MinSliverVolumeCm3` (`25.0 cm³`) OR minimum box dimension `< ShiftConfig->MinChunkAxisThicknessCm` (`0.6 cm`), do NOT generate convex hulls: add $M_{\text{kg}}$ directly to `AccumulatedTareWasteKg` via `UInspectionShiftSubsystem`, emit `ETelemetryImpulseId::SliverVaporized`, and discard the island.
  - Otherwise:
    1. Compute world chunk CoM: `FVector WorldChunkCoM = RemainderTransform.TransformPosition((FVector)LocalCoMCm);`
    2. Recenter island vertices around `(0,0,0)` via `UE::Geometry::MeshTransforms::Translate(IslandMesh, -LocalCoMCm);` *(leaves pre-baked `UV0` untouched while centering Chaos inertia & pivot!)*.
    3. Acquire an `ASeveredChunkActor` from `UChunkPoolSubsystem` at transform `FTransform(RemainderTransform.GetRotation(), WorldChunkCoM, FVector::OneVector)`, move `IslandMesh` into its `UDynamicMesh`, and generate bounded runtime convex collision via `UGeometryScriptLibrary_CollisionFunctions::SetDynamicMeshCollisionFromMesh` (`Method = ConvexHulls`, `MaxConvexHullsPerMesh = 2`, `MaxShapeVertexCount = 16`, `bSimplifyHulls = true`, `bEmitTransaction = false`).
    4. Enforce simple collision & deterministic mass override:
       `ChunkComp->SetComplexAsSimpleCollisionEnabled(false, true);`
       `ChunkComp->SetMassOverrideInKg(NAME_None, AnalyticalMassKg, true);`
       `ChunkComp->SetSimulatePhysics(true);`
    5. Apply sign-invariant separation velocity (prevents $180^\circ$ rolled planes from pushing chunks into the remainder) and register the chunk as `EChunkRoutingState::InFlight` with `UInspectionShiftSubsystem`.

---

## 5. EXACT MATHEMATICAL FORMULAS & EDGE-CASE GUARDS

### 5.1. Sign-Invariant Separation Velocity
Given cut plane $(\mathbf{P}_0, \hat{\mathbf{n}}_{\text{world}})$ and detached chunk world CoM $\mathbf{C}_{\text{chunk}}$:
$$s_{\text{dir}} = \begin{cases} +1.0, & (\mathbf{C}_{\text{chunk}} - \mathbf{P}_0) \cdot \hat{\mathbf{n}}_{\text{world}} \ge 0 \\ -1.0, & \text{otherwise} \end{cases}, \qquad \mathbf{v}_{\text{sep}} = v_{\text{sep}} \cdot s_{\text{dir}} \, \hat{\mathbf{n}}_{\text{world}}$$

### 5.2. Un-Tilted Neutral-Frame Clamp Torque & Bidirectional Fatigue (`UClampTorqueSolverComponent`)
To prevent pendulum shortening of the horizontal lever arm when the clamp tilts by $(\theta_x, \theta_y)$, `AMonolithRigActor` caches its **un-tilted neutral pivot transform** $T_{\text{neutral}}$ (`Location = ClampPivotWorld`). Given Remainder local CoM $\mathbf{C}_{\text{local,cm}}$ and mass $M_{\text{rem}}$:
$$\mathbf{C}_{\text{neutral}} = T_{\text{neutral}}(\mathbf{C}_{\text{local,cm}}), \qquad \mathbf{r}_0 = \frac{\mathbf{C}_{\text{neutral}} - \mathbf{P}_{\text{clamp}}}{100.0} \text{ (meters)}$$
$$\boldsymbol{\tau}_0 = \begin{pmatrix} -r_{0,y} \, M_{\text{rem}} \, 9.81 \\ +r_{0,x} \, M_{\text{rem}} \, 9.81 \\ 0 \end{pmatrix} \text{ N}\cdot\text{m}, \qquad \tau_{\perp,0} = \|\boldsymbol{\tau}_0\| = 9.81 \, M_{\text{rem}} \sqrt{r_{0,x}^2 + r_{0,y}^2}$$
$$S_{\text{clamp}} = \text{Clamp}\left(\frac{\tau_{\perp,0}}{\tau_{\text{max}}}, 0.0, 1.5\right), \qquad \text{Telemetry}_{\text{strain}} = \text{Clamp}(S_{\text{clamp}}, 0.0, 1.0)$$

- **2-Axis Kinematic Spring-Damper (Sub-stepped with $\Delta t \le \frac{1}{60}\text{ s}$):**
  $$\ddot{\boldsymbol{\theta}} = \frac{\boldsymbol{\tau}_0 - k_{\text{spring}}\boldsymbol{\theta} - c_{\text{damp}}\dot{\boldsymbol{\theta}}}{I_{\text{eff}}}, \qquad \dot{\boldsymbol{\theta}} \leftarrow \dot{\boldsymbol{\theta}} + \ddot{\boldsymbol{\theta}}\Delta t, \qquad \boldsymbol{\theta} \leftarrow \text{Clamp}(\boldsymbol{\theta} + \dot{\boldsymbol{\theta}}\Delta t, -\theta_{\max}, +\theta_{\max})$$
  Apply `FRotator(FMath::RadiansToDegrees(Theta.Y), 0.0f, FMath::RadiansToDegrees(Theta.X))` to the rig's visual/kinematic mount scene component.

- **Bidirectional Structural Fatigue $F \in [0.0, 1.0]$ & Catastrophic Shear:**
  - If $S_{\text{clamp}} \ge 1.45$: immediate bolt shear (`F = 1.0f`).
  - Else integrate $F$:
    $$\dot{F} = \begin{cases} +\dfrac{S_{\text{clamp}} - 0.80}{T_{\text{grace}}}, & S_{\text{clamp}} \ge 0.80 \\[8pt] -\dfrac{0.80 - S_{\text{clamp}}}{0.80 \cdot T_{\text{cool}}}, & S_{\text{clamp}} < 0.80 \end{cases}, \qquad F \leftarrow \text{Clamp}(F + \dot{F}\Delta t, 0.0, 1.0)$$
  - If $F \ge 1.0$: mark `bClampSnapped = true`, enable freefall physics or drop remainder, and notify `UInspectionShiftSubsystem::TriggerContractFault(EContractFaultReason::ClampStructuralSnap)`.
  - **MANDATORY TIER-2 SLEEP GUARD:** Call `SetComponentTickEnabled(false)` ONLY when ALL three conditions hold simultaneously:
    `(\|\dot{\boldsymbol{\theta}}\| < 1e-3f) && (\|\boldsymbol{\tau}_0 - k_{\text{spring}}\boldsymbol{\theta}\| < 0.05f) && ((S_{\text{clamp}} < 0.80f && F <= KINDA_SMALL_NUMBER) || bClampSnapped)`.
    Never disable tick while $S_{\text{clamp}} \ge 0.80$ (fatigue rising) or while $F > 0$ under $S_{\text{clamp}} < 0.80$ (fatigue cooling).

### 5.3. 3D Capsule-to-Plane Clearance (`UVeinScannerComponent`)
For local plane $(\mathbf{p}_{\text{local}}, \hat{\mathbf{n}}_{\text{local}})$ with $d = \hat{\mathbf{n}}_{\text{local}} \cdot \mathbf{p}_{\text{local}}$ and each active vein capsule $(\mathbf{A}_i, \mathbf{B}_i, R_i, D_{\text{warn},i})$:
$$s_A = \hat{\mathbf{n}}_{\text{local}} \cdot \mathbf{A}_i - d, \qquad s_B = \hat{\mathbf{n}}_{\text{local}} \cdot \mathbf{B}_i - d$$
$$d_{\text{clear},i} = \begin{cases} 0.0, & s_A \cdot s_B \le 0.0 \text{ or } \min(|s_A|, |s_B|) \le R_i \\ \min(|s_A|, |s_B|) - R_i, & \text{otherwise} \end{cases}$$
$$\text{bWillDetonate} \iff \min_i(d_{\text{clear},i}) \le 0.0, \qquad P_{\text{vein}} = \max_i \left[ \text{Clamp}\left(1.0 - \frac{d_{\text{clear},i}}{D_{\text{warn},i}}, 0.0, 1.0\right)^{1.8} \right]$$

### 5.4. RMB Plane Origin ($\mathbf{P}_0$) Lock & Bevel / Cancel Rules (`ULaserPlaneAimComponent`)
- **Priority 1 (Charge Abort):** If `ULaserCapacitorComponent::IsCharging() == true`, pressing RMB immediately aborts capacitor charge (`CancelCharge()`), resets `LaserCharge = 0.0f`, and does NOT enter bevel mode until RMB is released and pressed again.
- **Priority 2 ($\mathbf{P}_0$ Spatial Lock + Pitch Bevel):** When `IsCharging() == false` and RMB is held (`bIsBevelLockActive = true`):
  - Freeze `LockedPlaneOriginWorld = CurrentPlaneOriginWorld` ($\mathbf{P}_0$ does NOT translate vertically or horizontally).
  - Route `MouseDelta.Y` exclusively into Pitch Bevel angle $\theta \leftarrow \text{Clamp}(\theta + \text{MouseDelta.Y} \cdot S_{\text{bevel}}, -40.0^\circ, +40.0^\circ)$.
  - Scroll wheel continues adjusting Roll angle $\phi$ in $2.5^\circ$ detents (`15.0^\circ` when Shift is held).
  - Releasing RMB preserves $(\phi, \theta)$ and resumes normal raycast $\mathbf{P}_0$ tracking on the inspection frame mid-plane.

### 5.5. Conservation of Mass, Stray Floor-Catch & Race-Condition-Free Softlock Guard
Every gram of the initial monolith is conserved across four mutually exclusive buckets:
$$M_{\text{initial}} = M_{\text{remainder}} + M_{\text{pan}} + M_{\text{inflight}} + M_{\text{tare\_waste}}$$
- **Resolving the `InFlight` Race Condition (`AStrayFloorCatchActor` + Watchdog):**
  1. When `UMonolithSliceComponent` spawns a chunk, `UInspectionShiftSubsystem::RegisterChunkInFlight(Chunk)` sets `InFlight` state (`InFlightChunkCount++`, `InFlightMassKg += M_chunk`) and starts a `4.0s` per-chunk stuck-geometry watchdog timer.
  2. **Scale Pan Overlap (`UScaleWeighingComponent`):** Transitions `InFlight -> OnScalePan` (`InFlightChunkCount--`, `InFlightMassKg -= M_chunk`, `PanMassKg += M_chunk`). If a chunk bounces back out over the pan rim (`OnComponentEndOverlap`), it transitions `OnScalePan -> InFlight` until caught below.
  3. **Tare Chute Trigger (`AScaleAssemblyActor`):** Transitions `InFlight -> ConsumedAsTare` (`InFlightChunkCount--`, `InFlightMassKg -= M_chunk`, `AccumulatedTareWasteKg += M_chunk`) and calls `ReturnToChunkPool()`.
  4. **Stray Floor-Catch Trigger (`AStrayFloorCatchActor` spanning the entire `4x4m` cell floor/walls beneath the scale & chute) OR `4.0s` Watchdog Expiry:** Catches any chunk that missed or bounced off the pan, transitions `InFlight -> LostAsStrayTare` (`InFlightChunkCount--`, `InFlightMassKg -= M_chunk`, `AccumulatedTareWasteKg += M_chunk`), recycles via `ReturnToChunkPool()`, and immediately invokes `EvaluateContractHealth()`.
- **6 Automatic Fault & Softlock Checks (`UInspectionShiftSubsystem::EvaluateContractHealth`):**
  1. `HasContaminationFault() == true` -> `EContractFaultReason::ScaleContaminated`
  2. `IsReceiverSettled() && SettledScaleMassKg > Target + Tolerance` -> `EContractFaultReason::QuotaOvershoot`
  3. `AccumulatedTareWasteKg > MaxAllowedTareWasteKg` -> `EContractFaultReason::TareAllowanceExceeded`
  4. `(InFlightChunkCount == 0) && (SettledScaleMassKg + RemainderMassKg < Target - Tolerance)` -> `EContractFaultReason::MathematicallyUnreachable`
  5. Plus event-driven `VolatileDetonation` and `ClampStructuralSnap`.
- **Manual `[R]` Reset Input:** Holding `[R]` for `0.35s` triggers `EContractFaultReason::ManualOperatorReset` at any time. All faults flush active chunks back to `UChunkPoolSubsystem` and respawn the current contract cleanly after `1.2s`.

---

## 6. EXECUTION TIERS & AUDIO TELEMETRY BRIDGE

### Component Execution Cadence Summary
- **Tier 1 (`PrimaryComponentTick.bCanEverTick = false`):** `UMonolithSliceComponent`, `UVeinScannerComponent`, `UAudioTelemetryBridgeComponent`, `UChunkPoolSubsystem`, `UInspectionShiftSubsystem`. *(Zero 60Hz timers allowed).*
- **Tier 2 (`bCanEverTick = true`, `bStartWithTickEnabled = false`, strictly toggled via `SetComponentTickEnabled`):**
  - `ULaserPlaneAimComponent`: Active only during active inspection state; disabled during transitions/menus.
  - `ULaserCapacitorComponent`: Enabled ONLY while LMB charge > 0 or thermal cooldown > 0.
  - `UClampTorqueSolverComponent`: Enabled on slice/load change; sleeps strictly under Section 5.2 guard.
  - `UDiverterGateComponent`: Enabled ONLY during `0.18s` kinematic flapper rotation (`0° <-> 45°`).
  - `UScaleWeighingComponent`: Enabled on chunk overlap/exit; drives damped harmonic pan displacement ($\ddot{z} + 2\zeta\omega_n\dot{z} + \omega_n^2(z - z_{\text{target}}) = 0$) and sleeps once $|z - z_{\text{target}}| < 0.02\text{ cm}$, $|\dot{z}| < 0.1\text{ cm/s}$, and all overlapping chunk rigid bodies have linear speed `< 2.0 cm/s` (`bIsSettled = true`).

### `UAudioTelemetryBridgeComponent` Deadband Contract
- Discovers actors implementing `UTelemetryPublisherInterface` at `BeginPlay` (passed via explicit `UPROPERTY` actor references or subsystem registration — never `GetAllActorsOfClass` in loops).
- Caches last pushed value per `ETelemetryStreamId`; forwards updates to `UAudioComponent::SetFloatParameter` ONLY when `FMath::Abs(NewVal - LastVal) >= 0.01f` OR `NewVal == 0.0f` OR `NewVal == 1.0f`.
- Maps `ETelemetryImpulseId` events to `UAudioComponent::SetTriggerParameter` + magnitude float parameters on UE5 MetaSounds sources.

---

## 7. STRICT 5-PHASE IMPLEMENTATION CHECKLIST (COMPILE-GATED)

Execute implementation strictly phase-by-phase. At the end of EVERY phase, run UnrealBuildTool (`TareAndTitheEditor Win64 Development`) and resolve all warnings/errors before starting the next phase.

### Phase 1: Core Types, ISP Interfaces & Validated DataAssets
- [ ] Configure `TareAndTithe.uproject` and `Source/TareAndTithe/TareAndTithe.Build.cs` (`SetupModulePhysicsSupport(Target)` + `PublicIncludePaths`).
- [ ] Implement `Public/Core/TareTitheTypes.h`.
- [ ] Implement all 4 narrow `UInterface` headers (`SliceableTargetInterface.h`, `MassPayloadInterface.h`, `QuotaReceiverInterface.h`, `TelemetryPublisherInterface.h`).
- [ ] Implement `UMonolithContractDataAsset` (with `#if WITH_EDITOR` guarded `IsDataValid`) and `UShiftProgressionDataAsset`.
- [ ] **UBT Compile Gate #1:** Must compile with 0 errors, 0 warnings.

### Phase 2: Object Pool & 8-Step Runtime `DynamicMesh` + Chaos Slicer
- [ ] Implement `ASeveredChunkActor` (`UDynamicMeshComponent` root, implements `IMassPayloadInterface`, `4.0s` `InFlight` watchdog timer).
- [ ] Implement `UChunkPoolSubsystem` (pre-warms 16 `ASeveredChunkActor` instances + `UDynamicMeshPool` on world initialization).
- [ ] Implement `UMonolithSliceComponent` (procedural + optional `UStaticMesh` loader with `FMergeCoincidentMeshEdges`, static Box-Projection `UV0`, `FMeshPlaneCut` + `HoleFillTriangles` `MaterialID = 1`, `FMeshConnectedComponents`, `FDynamicMeshAABBTree3` + `TIntrTriangle3AxisAlignedBox3` + `TFastWindingTree` grip & vein partitioning, `-LocalCoM` recentering, convex hull + `SetMassOverrideInKg`, sign-invariant separation velocity).
- [ ] **UBT Compile Gate #2:** Must compile with 0 errors, 0 warnings.

### Phase 3: 3D Laser Plane Aiming, Capacitor & Acoustic Vein Scanner
- [ ] Implement `UVeinScannerComponent` (exact 3D capsule-to-plane clearance $d_{\text{clear},i}$ and $P_{\text{vein}} \in [0, 1]$).
- [ ] Implement `ULaserPlaneAimComponent` (frame mid-plane raycast, Scroll Roll $\phi$, RMB $\mathbf{P}_0$ lock + Pitch Bevel $\theta$, RMB charge-cancel priority, laser plane preview mesh/decal transform).
- [ ] Implement `ULaserCapacitorComponent` (`0.35s` LMB hold charge, cooldown, `ISliceableTargetInterface::Execute_CommitSlicePlane` dispatch).
- [ ] **UBT Compile Gate #3:** Must compile with 0 errors, 0 warnings.

### Phase 4: Neutral-Frame Clamp Torque, Diverter Flapper, Scale & Stray Catch
- [ ] Implement `UClampTorqueSolverComponent` (neutral-frame $\boldsymbol{\tau}_0$, 2-axis kinematic spring-damper, bidirectional fatigue $F \in [0, 1]$, strict sleep guard).
- [ ] Implement `UDiverterGateComponent` (`[SPACE]` toggle between `TitheScale` and `TareChute`, `0.18s` Tier-2 kinematic flapper rotation).
- [ ] Implement `UScaleWeighingComponent` (analytical mass summation via `IMassPayloadInterface`, contamination detection, damped harmonic pan depression, settle detection).
- [ ] Implement `AStrayFloorCatchActor` (pit-wide catch box transitioning missed/bounced chunks from `InFlight` to `LostAsStrayTare` and recycling to pool).
- [ ] **UBT Compile Gate #4:** Must compile with 0 errors, 0 warnings.

### Phase 5: Shift State Machine, MetaSounds Bridge, Thin Shells & Scalability
- [ ] Implement `UInspectionShiftSubsystem` (sequences 6 contracts across 3 Acts: Act 1 mass/roll, Act 2 single/crossed vein + diverter, Act 3 high-density multi-vein Counter-Torque; enforces all 6 automatic fault resets, `[R]` `0.35s` hold reset, and `[E]` quota verification stamp).
- [ ] Implement `UAudioTelemetryBridgeComponent` (`0.01f` deadband stream forwarding + MetaSounds impulse triggers).
- [ ] Implement thin composition shell actors: `AInspectorPawn` (EnhancedInput bindings + clamped `±25°` stationary camera), `AMonolithRigActor`, `AScaleAssemblyActor`, `ADirectorBoothActor` (shutter/lamp reactions), and `ATareTitheGameMode`.
- [ ] Configure `Config/DefaultEngine.ini` & `Config/DefaultGame.ini` (`MapsToCook`, native UE5 TSR default scalability via `UGameUserSettings`, `r.Streaming.PoolSize=2000` for 6 GB VRAM safety).
- [ ] **UBT Compile Gate #5 & Greybox Verification:** Verify end-to-end 6-contract greybox loop, all 6 fault-recovery paths, zero softlocks, and clean `SCRIPTS\PackageBuild.bat` standalone Win64 archive `< 1 GB`.