# Audio, MetaSounds & Niagara VFX Presentation Contract (`Phase 7 -> Phase 8`)

## 1. Cook-Safe Auto-Discovery & Asset Paths

`ATareTitheGameMode` automatically discovers and binds `/Game/Presentation/DA_Presentation` (`UTareTithePresentationDataAsset`) and `/Game/Presentation/DA_ShiftProgression` (`UShiftProgressionDataAsset`) at startup via `FPackageName::DoesPackageExist` + `LoadObject`, and `Config/DefaultGame.ini` marks `/Game/Presentation` under `+DirectoriesToAlwaysCook`.

| Asset Role | Canonical Unreal Path | C++ Type | Persistence / Playback Mode |
| :--- | :--- | :--- | :--- |
| Master Presentation Bundle | `/Game/Presentation/DA_Presentation` | `UTareTithePresentationDataAsset` | Hard `TObjectPtr<>` graph root |
| Optional Shift Progression | `/Game/Presentation/DA_ShiftProgression` | `UShiftProgressionDataAsset` | Hard `TObjectPtr<>` array of `UMonolithContractDataAsset` |
| Continuous Telemetry Loops | Assigned to `ContinuousLoopsMetaSound` (`MS_TelemetryLoops`) | `USoundBase` (`UMetaSoundSource`) | Persistent 2D/Pawn `UAudioComponent` (`bAutoDestroy = false`, auto-played) |
| Discrete Telemetry Impulses | Assigned to `DiscreteImpulsesMetaSound` (`MS_TelemetryImpulses`) | `USoundBase` (`UMetaSoundSource`) | Persistent 2D/Pawn `UAudioComponent` (`bAutoDestroy = false`, guarded with `if (!IsPlaying()) Play()`) |
| Cell Ambient Bed | Assigned to `AmbientCellBedSound` | `USoundBase` (`UMetaSoundSource` or looping `USoundWave`) | Persistent 2D/Pawn `UAudioComponent` (`bAutoDestroy = false`, auto-played) |
| Polyphonic 3D Impulse Map | Assigned to `SpatialImpulseSounds` (`TMap<ETelemetryImpulseId, TObjectPtr<USoundBase>>`) | `USoundBase` (`UMetaSoundSource` or `USoundWave`) | Polyphonic 3D `SpawnSoundAtLocation` (`bAutoDestroy = true`) using `DefaultSpatialAttenuation` |

---

## 2. Exact C++ Telemetry Parameter Table (`UAudioTelemetryBridgeComponent`)

### 2.1 Continuous Float Inputs (`MS_TelemetryLoops`)

Filtered through deadband (`StreamDeadband = 0.005f`) with exact `0.0f` and `1.0f` boundary pass-through. Forwarded via `UAudioComponent::SetFloatParameter(ParameterName, ClampedValue)`.

| `ETelemetryStreamId` | Exact MetaSound `Float` Input `FName` | Range | Source Gameplay Signal | Recommended Procedural / Stem Mapping |
| :--- | :--- | :--- | :--- | :--- |
| `VeinProximity` | `VeinProximity` | `[0.0, 1.0]` | Laser plane signed distance to nearest `FHiddenVeinDefinition` (`0.0` outside `WarningProximityMarginCm`, `1.0` intersecting vein sphere) | Geiger click rate (`2 Hz -> 38 Hz`) + resonant bandpass warning whine (`620 Hz -> 1840 Hz`) |
| `ClampStrain` | `ClampStrain` | `[0.0, 1.0]` | Normalized overhanging gravity torque `\|tau\| / tau_max` on `UClampTorqueComponent` | Low-end structural steel groan gain + sub-bass LFO modulation (`38 Hz -> 72 Hz`) |
| `ClampFatigue` | `ClampFatigue` | `[0.0, 1.0]` | Accumulated structural fatigue when `ClampStrain >= 0.80` (`1.0` = imminent snap) | Metallic rivet creak / high-tension wire pinging density before structural failure |
| `LaserCharge` | `LaserCharge` | `[0.0, 1.0]` | `ULaserCapacitorComponent` charge ramp (`0.0 -> 1.0` over `0.45 s`, instant `0.0` on fire/abort) | Rising capacitor oscillator (`110 Hz -> 880 Hz` exponential sweep) + high-voltage transformer buzz |
| `ScaleFillRatio` | `ScaleFillRatio` | `[0.0, 1.0]` | `PanMassKg / TargetTitheMassKg` clamped `[0.0, 1.0]` on `UScaleWeighingComponent` | Mechanical spring-bed tension hum pitch/filter openness |

### 2.2 Discrete Trigger & Magnitude Inputs (`MS_TelemetryImpulses` & `SpatialImpulseSounds`)

Whenever `ETelemetryImpulseId` fires, `UAudioTelemetryBridgeComponent` executes **both** paths in order:
1. **Polyphonic 3D Spatial Path (`SpatialImpulseSounds[ImpulseId]`)**: If an entry is assigned in `DA_Presentation->SpatialImpulseSounds`, spawns a 3D one-shot at `WorldLocation` (`UGameplayStatics::SpawnSoundAtLocation`, `bAutoDestroy = true`, `AttenuationSettings = DefaultSpatialAttenuation`) with `VolumeMultiplier = Lerp(0.72, 1.08, NormalizedMagnitude)`, `PitchMultiplier = Lerp(0.94, 1.06, NormalizedMagnitude)`, and calls `SetFloatParameter("NormalizedMagnitude", NormalizedMagnitude)` on the spawned `UAudioComponent*`.
2. **Persistent Bus Path (`DiscreteImpulsesMetaSound`)**: Ensures `DiscreteImpulsesAudioComponent->IsPlaying()` (restarting if a non-looping graph finished), sets `<TriggerName>_Magnitude`, then fires `SetTriggerParameter(<TriggerName>)`.

| `ETelemetryImpulseId` | Exact `Trigger` Input `FName` (`MS_TelemetryImpulses`) | Exact `Float` Magnitude Input `FName` (`MS_TelemetryImpulses`) | 3D One-Shot `Float` Input `FName` (`SpatialImpulseSounds`) | Range | World Location Anchor & Normalization |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `SliceCommitted` | `OnSliceCommitted` | `OnSliceCommitted_Magnitude` | `NormalizedMagnitude` | `[0.12, 1.0]` | `WorldCutCentroid` (or plane origin); `CutCrossSectionAreaCm2 / 900.0` |
| `SliverVaporized` | `OnSliverVaporized` | `OnSliverVaporized_Magnitude` | `NormalizedMagnitude` | `[0.0, 1.0]` | Vaporized sliver world centroid; normalized against `20.0 cm^3` threshold |
| `ChunkScaleImpact` | `OnChunkScaleImpact` | `OnChunkScaleImpact_Magnitude` | `NormalizedMagnitude` | `[0.05, 1.0]` | `PanDeck` top surface under impacting chunk XY; `ChunkMassKg / TargetTitheMassKg` |
| `ChunkTareDisposed` | `OnChunkTareDisposed` | `OnChunkTareDisposed_Magnitude` | `NormalizedMagnitude` | `[0.05, 1.0]` | `TareChuteTriggerBox` disposal point; `ChunkMassKg / 15.0` |
| `DiverterToggled` | `OnDiverterToggled` | `OnDiverterToggled_Magnitude` | `NormalizedMagnitude` | `0.65` (`Tare`) / `1.0` (`Tithe`) | `DiverterFlapperPivot` hinge world coordinates |
| `VerdictStamped` | `OnVerdictStamped` | `OnVerdictStamped_Magnitude` | `NormalizedMagnitude` | `1.0` | `ADirectorBoothActor` world coordinates |
| `FaultTriggered` | `OnFaultTriggered` | `OnFaultTriggered_Magnitude` | `NormalizedMagnitude` | `1.0` (`0.45` manual `[R]`) | Cut centroid (`VolatileDetonation`), rig (`ClampStructuralSnap`), or scale (`QuotaOvershoot` / `ScaleContaminated` / `TareAllowanceExceeded`) |

---

## 3. Pure-Procedural MetaSounds Node Graph Blueprints (Zero External WAV Option)

Every graph below MUST set **MetaSound Source -> Output Format = Stereo** (for `MS_TelemetryLoops` / `MS_TelemetryImpulses`) or **Mono** (for 3D `SpatialImpulseSounds` assets spatialized via `DefaultSpatialAttenuation`). For `MS_TelemetryLoops` and `MS_TelemetryImpulses`, **DO NOT wire `On Finished`**, keeping the graph alive indefinitely.

### 3.1 `MS_TelemetryLoops` (Persistent Stereo Graph)
1. **Capacitor Charge Layer (`LaserCharge`)**:
   - `LaserCharge` -> `Map Range (0.0..1.0 -> 95.0..760.0)` -> `Saw` + `Sine` oscillators -> `Ladder Filter` (Cutoff `350 Hz -> 4200 Hz` driven by `LaserCharge`) -> multiply by `InterpTo(LaserCharge)` (`Attack = 0.01s, Release = 0.04s`).
2. **Volatile Vein Proximity Layer (`VeinProximity`)**:
   - `VeinProximity` -> `Map Range (0.0..1.0 -> 0.5..34.0)` -> `Trigger Repeat` (gated by `VeinProximity > 0.02`) -> `AD Envelope (0.001s, 0.018s)` * `White Noise` -> `Bandpass Filter (2400 Hz, Q = 6.0)` for diegetic Geiger ticks.
3. **Structural Clamp Strain & Fatigue Layer (`ClampStrain`, `ClampFatigue`)**:
   - `ClampStrain` -> `Triangle (46 Hz)` + `Pink Noise` -> `Lowpass Filter (180 Hz + ClampStrain * 420 Hz)` -> soft-clip `WaveShaper` * `Map Range (ClampStrain, 0.15..1.0 -> 0.0..0.45)`.
   - `ClampFatigue` -> `Trigger Repeat (Period = Lerp(0.45, 0.08, ClampFatigue))` -> `Modal / Karplus-Strong` metallic ping at `1120 Hz` & `1680 Hz` * `ClampFatigue`.
4. **Master Output**:
   - Sum all sub-layers through `Stereo Mixer` -> `Limiter (-1.5 dB ceiling)` -> `Out Left` / `Out Right`.

### 3.2 `SpatialImpulseSounds` One-Shot Mono Graphs (3D Polyphonic)
- **`MS_Imp_SliceCommitted`**: `On Play` -> `AD Envelope (0.002s, Lerp(0.22s, 0.55s, NormalizedMagnitude))` driving FM plasma shear (`Carrier 220->65 Hz`, `Modulator 640 Hz`) + high-voltage filtered noise burst -> `On Envelope Finished` -> `On Finished`.
- **`MS_Imp_ChunkScaleImpact`**: `On Play` -> heavy damped iron plate modal bank (`82 Hz`, `196 Hz`, `430 Hz`, `910 Hz`) scaled in decay (`0.18s -> 0.65s`) and low-pass brightness by `NormalizedMagnitude` -> `On Finished`.
- **`MS_Imp_DiverterToggled`**: `On Play` -> pneumatic hiss (`Bandpass White Noise`, `0.09s`) + solenoid latch clack (`0.03s` delayed impulse) -> `On Finished`.
- **`MS_Imp_VerdictStamped`**: `On Play` -> hydraulic press thud (`58 Hz` sine pitch-drop) + brass stamp ring (`1420 Hz` decaying partial) -> `On Finished`.
- **`MS_Imp_FaultTriggered`**: `On Play` -> dual-tone industrial klaxon (`440 Hz` + `554 Hz` pulse-width wave, `2` bursts) + sub-impact transient -> `On Finished`.

### 3.3 Commercial-Safe WAV Stem Option
If authoring with external `.wav` stems instead of pure synthesis:
- Import `48 kHz / 16-bit PCM` mono `.wav` files for 3D `SpatialImpulseSounds` (and stereo seamless loops for `AmbientCellBedSound`).
- Wrap multi-variation `.wav` stems inside a `UMetaSoundSource` (`Wave Player` + `Random Get` array + `NormalizedMagnitude` low-pass filter) or `USoundCue` and assign directly into `DA_Presentation`.

---

## 4. Niagara VFX User Parameter Contract (`UInspectorKinestheticsComponent`)

Spawned via `UNiagaraFunctionLibrary::SpawnSystemAtLocation` with `bAutoDestroy = true` (`ENCPoolMethod::AutoRelease`).

| `DA_Presentation` Slot | Triggering `ETelemetryImpulseId` | Condition | Spawn Location | Niagara `User.*` Float Parameter |
| :--- | :--- | :--- | :--- | :--- |
| `LaserSliceSparksVFX` | `SliceCommitted` | Every committed slice | `WorldCutCentroid` | `User.CutAreaNormalized` (`[0.12, 1.0]`) |
| `SliverVaporizedVFX` | `SliverVaporized` | Micro-sliver `< 20 cm^3` vaporized | Sliver world centroid | `User.NormalizedMagnitude` (`[0.0, 1.0]`) |
| `VeinDetonationVFX` | `FaultTriggered` | `LastFaultReason == EContractFaultReason::VolatileDetonation` | Severed vein cut centroid | `User.NormalizedMagnitude` (`1.0`) |
| `ScaleHeavyImpactVFX` | `ChunkScaleImpact` | `NormalizedMagnitude >= 0.12` | `PanDeck` top surface under chunk XY | `User.NormalizedMagnitude` (`[0.12, 1.0]`) |
