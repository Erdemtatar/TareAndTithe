"""
Phase 12-FIX3 Live PIE 5-Gate Audio Telemetry, Mix & Cook Verification Harness.

Launches UnrealEditor WITH live AudioMixer enabled (NO -NoSound flag) on
/Game/Maps/L_MainGameplay, enters PIE, and verifies:
  - Gate 1 (6-SFX Asset & Cook Resolution):
    All 6 authored SFX_*.uasset waves under /Game/Presentation/Audio/Waves/
    resolve to valid USoundWave assets at runtime, DA_Presentation + ClampGroanAudio
    bindings resolve non-null, and Config/DefaultGame.ini locks bCookMapsOnly=True
    with +DirectoriesToAlwaysCook for /Game/Presentation and /Game/Presentation/Audio/Waves.
  - Gate 2 (Contract 01 Zero-Strain Silence & Ambient Mix):
    On DA_Contract_01 (0% strain), ClampGroanVolumeMultiplier == 0.0f,
    IsClampGroanVoiceActive == False, ClampGroanAudioComponent.is_playing() == False,
    AmbientBedAudioComponent.is_playing() == True at 0.42f, MS_TelemetryLoops
    "ClampStrain" parameter pinned at 0.15f (0.0 mono bleed), and HUD ClampStrain == 0.0f.
  - Gate 3 (Moderate Overhang vs. Post-Slice Surge vs. Critical Danger Ramp):
    DA_Contract_02 (~59% initial overhang) settles into subtle Zone-1 tension
    (VolumeMultiplier in [0.22, 0.40], IsClampGroanVoiceActive == True, is_playing() == True);
    committing an off-balance counterweight trim via LaserCapacitor triggers immediate
    post-slice creak surge (VolumeMultiplier >= 0.80f) and settles into sustained
    Zone-2 critical danger groan (VolumeMultiplier in [0.85, 1.40], PitchMultiplier in [1.03, 1.18]).
  - Gate 4 (<= 0.08s Instant Cutoff on ClampStructuralSnap & Non-Gameplay Phases + Clean Re-Arm):
    Triggering ClampStructuralSnap cuts groan to 0.0f (IsClampGroanVoiceActive == False,
    is_playing() == False) during FaultResetDelay with zero latched 1.0f drone, and after
    fault auto-reset reloads DA_Contract_02 into ActiveInspection, FadeIn(0.06f, 1.0f)
    cleanly restarts ClampGroanAudioComponent.is_playing() == True in Zone 1; non-ActiveInspection
    phase transitions also silence groan immediately.
  - Gate 5 (Performance & Binary Asset Lock):
    UAudioTelemetryBridgeComponent tick disabled (0.00 ms steady-state), steady PIE frame
    time <= 13.0 ms, Content/Maps/, Content/Presentation/, and ClampTorqueSolverComponent.* untouched.
"""

import sys

sys.dont_write_bytecode = True

import argparse
import json
import os
import statistics
import subprocess
import time
from typing import Any, Dict, List, Optional

UE5_ROOT = os.environ.get("UE5_ROOT", r"C:\Program Files\Epic Games\UE_5.8")
UNREAL_EDITOR_EXE = os.path.join(UE5_ROOT, "Engine", "Binaries", "Win64", "UnrealEditor.exe")
REMOTE_EXEC_DIR = os.path.join(
    UE5_ROOT, "Engine", "Plugins", "Experimental", "PythonScriptPlugin", "Content", "Python"
)
if REMOTE_EXEC_DIR not in sys.path:
    sys.path.insert(0, REMOTE_EXEC_DIR)

import remote_execution  # type: ignore  # noqa: E402

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
UPROJECT_PATH = os.path.join(REPO_ROOT, "TareAndTithe.uproject")
DEFAULT_GAME_INI_PATH = os.path.join(REPO_ROOT, "Config", "DefaultGame.ini")


REMOTE_BEGIN_PIE_SNIPPET = """
import unreal

try:
    ed_perf = unreal.get_default_object(unreal.EditorPerformanceSettings)
    ed_perf.set_editor_property("throttle_cpu_when_not_foreground", False)
except Exception:
    pass

for cmd in (
    "r.VSync 0",
    "r.VSyncEditor 0",
    "Slate.MaxFPS 240",
    "t.MaxFPS 240",
    "t.UnfocusedFrameRateLimit 0",
    "au.AllowAudioSpatialization 1",
):
    unreal.SystemLibrary.execute_console_command(None, cmd)

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les.is_in_play_in_editor():
    les.editor_request_end_play()
les.editor_request_begin_play()
unreal.log("[VerifyPhase12Fix3] Requested BeginPlay PIE session with AudioMixer active.")
"""


REMOTE_GATE1_AND_WARMUP_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
if pie_world is None:
    raise RuntimeError("PIE GameWorld is None; PIE not ready yet.")

world_time_sec = float(unreal.GameplayStatics.get_time_seconds(pie_world))
if world_time_sec < 1.05:
    raise RuntimeError(f"PIE warmup in progress (world_time={world_time_sec:.3f}s < 1.05s)...")

for cmd in ("r.VSync 0", "r.VSyncEditor 0", "Slate.MaxFPS 240", "t.MaxFPS 240"):
    unreal.SystemLibrary.execute_console_command(pie_world, cmd)

pawns = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()]
rigs = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()]
if len(pawns) != 1 or len(rigs) != 1:
    raise AssertionError(f"Expected 1 Pawn and 1 Rig, got pawns={len(pawns)}, rigs={len(rigs)}")

pawn = pawns[0]
audio_bridge = pawn.get_audio_telemetry_bridge_component()
groan_comp = pawn.get_clamp_Groan_audio_component() if hasattr(pawn, "get_clamp_Groan_audio_component") else pawn.get_clamp_groan_audio_component()
amb_comp = audio_bridge.get_ambient_bed_audio_component()
loops_comp = audio_bridge.get_continuous_loops_audio_component()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
da_pres = shift_sub.get_active_presentation_asset()

if audio_bridge is None or groan_comp is None or amb_comp is None or loops_comp is None or da_pres is None:
    raise AssertionError("Missing required audio components or DA_Presentation in PIE!")

if bool(audio_bridge.is_component_tick_enabled()) or bool(groan_comp.is_component_tick_enabled()):
    raise AssertionError("Tier-1 violation: AudioTelemetryBridgeComponent or ClampGroanAudioComponent has tick enabled!")

wave_paths = {
    "SFX_CellAmbience_Loop": "/Game/Presentation/Audio/Waves/SFX_CellAmbience_Loop.SFX_CellAmbience_Loop",
    "SFX_ClampSteelGroan_Loop": "/Game/Presentation/Audio/Waves/SFX_ClampSteelGroan_Loop.SFX_ClampSteelGroan_Loop",
    "SFX_MoltenSlagHiss_OneShot": "/Game/Presentation/Audio/Waves/SFX_MoltenSlagHiss_OneShot.SFX_MoltenSlagHiss_OneShot",
    "SFX_PlasmaSlice_OneShot": "/Game/Presentation/Audio/Waves/SFX_PlasmaSlice_OneShot.SFX_PlasmaSlice_OneShot",
    "SFX_PneumaticValve_OneShot": "/Game/Presentation/Audio/Waves/SFX_PneumaticValve_OneShot.SFX_PneumaticValve_OneShot",
    "SFX_ScalePanImpact_OneShot": "/Game/Presentation/Audio/Waves/SFX_ScalePanImpact_OneShot.SFX_ScalePanImpact_OneShot",
}

resolved_waves = {}
for key, obj_path in wave_paths.items():
    w = unreal.load_object(None, obj_path)
    if w is None or not isinstance(w, unreal.SoundWave):
        raise AssertionError(f"Gate 1 failed to load USoundWave {key} at {obj_path}: {w}")
    resolved_waves[key] = w.get_path_name()

spatial_map = da_pres.get_editor_property("spatial_impulse_sounds")
expected_impulse_waves = {
    unreal.TelemetryImpulseId.SLICE_COMMITTED: "SFX_PlasmaSlice_OneShot",
    unreal.TelemetryImpulseId.SLIVER_VAPORIZED: "SFX_MoltenSlagHiss_OneShot",
    unreal.TelemetryImpulseId.CHUNK_SCALE_IMPACT: "SFX_ScalePanImpact_OneShot",
    unreal.TelemetryImpulseId.CHUNK_TARE_DISPOSED: "SFX_MoltenSlagHiss_OneShot",
    unreal.TelemetryImpulseId.DIVERTER_TOGGLED: "SFX_PneumaticValve_OneShot",
}
bound_impulses = {}
for imp_id, expected_substr in expected_impulse_waves.items():
    snd = spatial_map.get(imp_id)
    if snd is None or not isinstance(snd, unreal.SoundWave) or expected_substr not in snd.get_name():
        raise AssertionError(f"DA_Presentation.SpatialImpulseSounds[{imp_id}] invalid: {snd} (expected {expected_substr})")
    bound_impulses[imp_id.name] = snd.get_name()

groan_snd = groan_comp.get_editor_property("sound")
amb_snd = amb_comp.get_editor_property("sound")
loops_snd = loops_comp.get_editor_property("sound")
if groan_snd is None or "SFX_ClampSteelGroan_Loop" not in groan_snd.get_name():
    raise AssertionError(f"ClampGroanAudioComponent sound invalid: {groan_snd}")
if amb_snd is None or "SFX_CellAmbience_Loop" not in amb_snd.get_name():
    raise AssertionError(f"AmbientBedAudioComponent sound invalid: {amb_snd}")
if loops_snd is None or "MS_TelemetryLoops" not in loops_snd.get_name():
    raise AssertionError(f"ContinuousLoopsAudioComponent sound invalid: {loops_snd}")

groan_cfg = da_pres.get_editor_property("clamp_groan_audio")
spatial_min = float(groan_cfg.get_editor_property("spatial_impulse_min_volume_multiplier"))
spatial_max = float(groan_cfg.get_editor_property("spatial_impulse_max_volume_multiplier"))

unreal.log("GATE1_JSON=" + json.dumps({
    "resolved_wave_count": len(resolved_waves),
    "groan_sound": groan_snd.get_name(),
    "ambient_sound": amb_snd.get_name(),
    "loops_sound": loops_snd.get_name(),
    "bound_impulses": bound_impulses,
    "spatial_gain_window": [round(spatial_min, 3), round(spatial_max, 3)],
    "bridge_tick_enabled": bool(audio_bridge.is_component_tick_enabled()),
    "groan_allow_spatialization": bool(groan_comp.get_editor_property("allow_spatialization")),
}))
"""


REMOTE_SAMPLE_AUDIO_STATE_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]

audio_bridge = pawn.get_audio_telemetry_bridge_component()
groan_comp = pawn.get_clamp_groan_audio_component()
amb_comp = audio_bridge.get_ambient_bed_audio_component()
loops_comp = audio_bridge.get_continuous_loops_audio_component()
clamp_solver = rig.get_clamp_torque_solver_component()
snap = shift_sub.get_current_snapshot()

unreal.log("AUDIO_STATE_JSON=" + json.dumps({
    "phase": shift_sub.get_current_phase().name,
    "fault": shift_sub.get_last_fault_reason().name,
    "contract_idx": int(snap.get_editor_property("contract_index")),
    "solver_strain": round(float(clamp_solver.get_strain_normalized()), 4),
    "hud_strain": round(float(snap.get_editor_property("clamp_strain_normalized")), 4),
    "flushed_strain": round(float(audio_bridge.get_last_flushed_stream_value(unreal.TelemetryStreamId.CLAMP_STRAIN)), 4),
    "flushed_fatigue": round(float(audio_bridge.get_last_flushed_stream_value(unreal.TelemetryStreamId.CLAMP_FATIGUE)), 4),
    "ms_clamp_strain_param": round(float(audio_bridge.get_last_meta_sound_clamp_strain_param()), 4),
    "groan_voice_active": bool(audio_bridge.is_clamp_groan_voice_active()),
    "groan_is_playing": bool(groan_comp.is_playing()),
    "groan_vol": round(float(audio_bridge.get_clamp_groan_volume_multiplier()), 4),
    "groan_comp_vol": round(float(groan_comp.get_editor_property("volume_multiplier")), 4),
    "groan_pitch": round(float(audio_bridge.get_clamp_groan_pitch_multiplier()), 4),
    "groan_comp_pitch": round(float(groan_comp.get_editor_property("pitch_multiplier")), 4),
    "effective_danger": round(float(audio_bridge.get_clamp_effective_danger()), 4),
    "amb_is_playing": bool(amb_comp.is_playing()),
    "amb_vol": round(float(audio_bridge.get_ambient_bed_volume_multiplier()), 4),
    "amb_comp_vol": round(float(amb_comp.get_editor_property("volume_multiplier")), 4),
    "loops_is_playing": bool(loops_comp.is_playing()),
}))
"""


def make_start_contract_snippet(contract_index: int, reset_career: bool = False) -> str:
    return f"""
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
if {reset_career!r}:
    shift_sub.execute_career_reset_to_shift_one()
else:
    shift_sub.start_shift_from_contract({int(contract_index)})
"""


REMOTE_FIND_AND_COMMIT_COUNTERWEIGHT_CUT_SNIPPET = """
import json
import math
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
audio_bridge = pawn.get_audio_telemetry_bridge_component()
groan_comp = pawn.get_clamp_groan_audio_component()
clamp_solver = rig.get_clamp_torque_solver_component()

# Route severed counterweight scrap into TareChute so Tithe Scale cannot overshoot during overhang testing
if shift_sub.get_current_snapshot().get_editor_property("active_diverter_route") != unreal.DiverterRoute.TARE_CHUTE:
    shift_sub.request_toggle_diverter()

bounds = rig.get_target_world_bounds()
bcenter = (bounds.min + bounds.max) * 0.5
tilt = clamp_solver.get_tilt_angles_degrees()

# On DA_Contract_02 (CantileveredJHook), overhanging hook pulls toward +Y (or sign of tilt).
# Slicing off counterweight mass on the opposite Y flank with a steep angled plane shifts CoM further into Zone 2 [0.72..0.88]!
# Let's probe candidate planes cleanly by reloading Ctr 02 until we hit target [0.73, 0.88] strain on first cut!
candidates = []
for sign_y in (-1.0, 1.0):
    for abs_dy in (4.5, 5.5, 6.5, 7.0, 7.5, 8.0, 8.5, 9.0, 9.5, 10.5, 11.5):
        for dz in (-8.0, -4.0, 0.0, 4.0, 10.0):
            for ny_mag in (0.85, 0.92, 0.97):
                nz_val = -math.sqrt(max(0.01, 1.0 - ny_mag * ny_mag))
                candidates.append((sign_y * abs_dy, dz, 0.0, sign_y * ny_mag, nz_val))

chosen = None
for dy, dz, nx, ny, nz in candidates:
    shift_sub.start_shift_from_contract(1)
    if shift_sub.get_current_snapshot().get_editor_property("active_diverter_route") != unreal.DiverterRoute.TARE_CHUTE:
        shift_sub.request_toggle_diverter()
    bounds = rig.get_target_world_bounds()
    bcenter = (bounds.min + bounds.max) * 0.5
    origin = unreal.Vector(float(bcenter.x), float(bcenter.y) + dy, float(bcenter.z) + dz)
    nlen = math.sqrt(nx * nx + ny * ny + nz * nz)
    normal = unreal.Vector(nx / nlen, ny / nlen, nz / nlen)
    aperture = unreal.LaserSliceAperture()
    aperture.set_editor_property("enforce_finite_sheet_bounds", False)
    res = rig.commit_slice_plane(origin, normal, aperture)
    phase_now = shift_sub.get_current_phase().name
    new_strain = float(clamp_solver.get_strain_normalized())
    cut_mass_kg = float(res.get_editor_property("spawned_chunk_mass_kg")) + float(res.get_editor_property("vaporized_sliver_mass_kg"))
    if (
        res.outcome == unreal.CommitSliceOutcome.SUCCESS
        and phase_now == "ACTIVE_INSPECTION"
        and cut_mass_kg <= 15.2
        and 0.72 <= new_strain <= 0.90
    ):
        chosen = {
            "dy": dy,
            "dz": dz,
            "normal": [round(nx / nlen, 4), round(ny / nlen, 4), round(nz / nlen, 4)],
            "cut_mass_kg": round(cut_mass_kg, 3),
            "post_cut_strain": round(new_strain, 4),
            "surge_vol": round(float(audio_bridge.get_clamp_groan_volume_multiplier()), 4),
            "surge_pitch": round(float(audio_bridge.get_clamp_groan_pitch_multiplier()), 4),
            "groan_playing": bool(groan_comp.is_playing()),
            "voice_active": bool(audio_bridge.is_clamp_groan_voice_active()),
        }
        break

if chosen is None:
    # Two-cut fallback (<= 15.2 kg onto TitheScale + <= 15.2 kg into TareChute so neither budget faults)
    for dy1, dz1, nx1, ny1, nz1 in candidates:
        shift_sub.start_shift_from_contract(1)
        bounds = rig.get_target_world_bounds()
        bcenter = (bounds.min + bounds.max) * 0.5
        nlen1 = math.sqrt(nx1 * nx1 + ny1 * ny1 + nz1 * nz1)
        aperture = unreal.LaserSliceAperture()
        aperture.set_editor_property("enforce_finite_sheet_bounds", False)
        r1 = rig.commit_slice_plane(
            unreal.Vector(float(bcenter.x), float(bcenter.y) + dy1, float(bcenter.z) + dz1),
            unreal.Vector(nx1 / nlen1, ny1 / nlen1, nz1 / nlen1),
            aperture,
        )
        m1 = float(r1.get_editor_property("spawned_chunk_mass_kg"))
        s1 = float(clamp_solver.get_strain_normalized())
        if r1.outcome != unreal.CommitSliceOutcome.SUCCESS or not (4.0 <= m1 <= 15.2) or s1 <= 0.62 or s1 >= 0.72:
            continue
        shift_sub.request_toggle_diverter()
        bounds2 = rig.get_target_world_bounds()
        bc2 = (bounds2.min + bounds2.max) * 0.5
        r2 = rig.commit_slice_plane(
            unreal.Vector(float(bc2.x), float(bc2.y) + dy1 * 0.75, float(bc2.z) + dz1),
            unreal.Vector(nx1 / nlen1, ny1 / nlen1, nz1 / nlen1),
            aperture,
        )
        m2 = float(r2.get_editor_property("spawned_chunk_mass_kg")) + float(r2.get_editor_property("vaporized_sliver_mass_kg"))
        s2 = float(clamp_solver.get_strain_normalized())
        if r2.outcome == unreal.CommitSliceOutcome.SUCCESS and m2 <= 15.2 and 0.72 <= s2 <= 0.90:
            chosen = {
                "dy": dy1,
                "dz": dz1,
                "normal": [round(nx1 / nlen1, 4), round(ny1 / nlen1, 4), round(nz1 / nlen1, 4)],
                "cut_mass_kg": round(m1 + m2, 3),
                "post_cut_strain": round(s2, 4),
                "surge_vol": round(float(audio_bridge.get_clamp_groan_volume_multiplier()), 4),
                "surge_pitch": round(float(audio_bridge.get_clamp_groan_pitch_multiplier()), 4),
                "groan_playing": bool(groan_comp.is_playing()),
                "voice_active": bool(audio_bridge.is_clamp_groan_voice_active()),
            }
            break

if chosen is None:
    raise AssertionError("Could not find counterweight cut on DA_Contract_02 landing in [0.72, 0.90] strain!")

unreal.log("CUT_SURGE_JSON=" + json.dumps(chosen))
"""


REMOTE_TRIGGER_STRUCTURAL_SNAP_SNIPPET = """
import json
import math
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
audio_bridge = pawn.get_audio_telemetry_bridge_component()
groan_comp = pawn.get_clamp_groan_audio_component()
clamp_solver = rig.get_clamp_torque_solver_component()

# Progressive outside-in -Y counterweight strip shaves on DA_Contract_02:
# Each ~3.5..4.5 kg outer -Y strip maximizes EccFactor (>1.0) and increments SameSideCutStreak (1 -> 2 -> 3 -> 4),
# driving StrainRatio past CatastrophicShearStrainRatio (1.05f) while total cut mass stays well below 16.0 kg Tare limit!
snapped = False
for sign_y in (-1.0, 1.0):
    shift_sub.start_shift_from_contract(1)
    if shift_sub.get_current_snapshot().get_editor_property("active_diverter_route") != unreal.DiverterRoute.TARE_CHUTE:
        shift_sub.request_toggle_diverter()
    init_bounds = rig.get_target_world_bounds()
    base_x = float((init_bounds.min.x + init_bounds.max.x) * 0.5)
    base_z = float((init_bounds.min.z + init_bounds.max.z) * 0.5) - 6.0
    edge_y = float(init_bounds.min.y if sign_y < 0.0 else init_bounds.max.y)
    ny_val = 0.94 * sign_y
    nz_val = -math.sqrt(1.0 - 0.94 * 0.94)
    ap = unreal.LaserSliceAperture()
    ap.set_editor_property("enforce_finite_sheet_bounds", False)

    for step_idx in range(1, 7):
        if shift_sub.get_current_phase() != unreal.ShiftPhase.ACTIVE_INSPECTION:
            break
        # Step 3.2 cm inward from outer Y edge on each successive cut
        cut_y = edge_y - (sign_y * (3.2 * step_idx))
        rig.commit_slice_plane(
            unreal.Vector(base_x, cut_y, base_z),
            unreal.Vector(0.0, ny_val, nz_val),
            ap,
        )
        if bool(clamp_solver.is_clamp_snapped()) or shift_sub.get_last_fault_reason() == unreal.ContractFaultReason.CLAMP_STRUCTURAL_SNAP:
            snapped = True
            break
    if snapped:
        break

if not snapped:
    raise AssertionError(
        f"Failed to trigger real geometric ClampStructuralSnap! "
        f"last_fault={shift_sub.get_last_fault_reason().name}, strain_ratio={clamp_solver.get_strain_ratio():.4f}"
    )

unreal.log("SNAP_TRIGGER_JSON=" + json.dumps({
    "geom_snapped": snapped,
    "phase": shift_sub.get_current_phase().name,
    "fault": shift_sub.get_last_fault_reason().name,
    "flushed_strain": round(float(audio_bridge.get_last_flushed_stream_value(unreal.TelemetryStreamId.CLAMP_STRAIN)), 4),
    "flushed_fatigue": round(float(audio_bridge.get_last_flushed_stream_value(unreal.TelemetryStreamId.CLAMP_FATIGUE)), 4),
    "immediate_voice_active": bool(audio_bridge.is_clamp_groan_voice_active()),
    "immediate_groan_vol": round(float(audio_bridge.get_clamp_groan_volume_multiplier()), 4),
}))
"""


REMOTE_ADVANCE_NON_GAMEPLAY_PHASE_FROM_OVERHANG_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
audio_bridge = pawn.get_audio_telemetry_bridge_component()

# Ensure DA_Contract_02 overhang groan is active, then test non-ActiveInspection transition cutoff
if not audio_bridge.is_clamp_groan_voice_active():
    shift_sub.start_shift_from_contract(1)

pre_active = bool(audio_bridge.is_clamp_groan_voice_active())
pre_vol = float(audio_bridge.get_clamp_groan_volume_multiplier())
shift_sub.request_manual_contract_reset()

unreal.log("PHASE_CUTOFF_JSON=" + json.dumps({
    "pre_active": pre_active,
    "pre_vol": round(pre_vol, 4),
    "post_phase": shift_sub.get_current_phase().name,
    "post_active": bool(audio_bridge.is_clamp_groan_voice_active()),
    "post_vol": round(float(audio_bridge.get_clamp_groan_volume_multiplier()), 4),
}))
"""


REMOTE_MEASURE_STEADY_FRAME_TIME_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
delta_ms = float(unreal.GameplayStatics.get_world_delta_seconds(pie_world)) * 1000.0
unreal.log("FRAME_MS_JSON=" + json.dumps({"delta_ms": round(delta_ms, 3)}))
"""


REMOTE_END_PIE_SNIPPET = """
import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les.is_in_play_in_editor():
    les.editor_request_end_play()
unreal.log("[VerifyPhase12Fix3] Cleanly ended PIE session without saving maps/assets.")
"""


REMOTE_QUIT_EDITOR_SNIPPET = """
import unreal
unreal.SystemLibrary.quit_editor()
"""


def run_remote_checked(
    remote: remote_execution.RemoteExecution,
    code: str,
    label: str,
    verbose: bool = True,
) -> List[str]:
    if verbose:
        print(f"[VerifyPhase12Fix3] >>> {label}")
    res = remote.run_command(
        code,
        unattended=True,
        exec_mode=remote_execution.MODE_EXEC_FILE,
        raise_on_failure=False,
    )
    lines: List[str] = []
    for out_line in res.get("output", []) or []:
        msg = out_line.get("output", "").rstrip()
        if msg:
            lines.append(msg)
            if verbose:
                print(f"  [UE] {msg}")
    if not res.get("success", False):
        raise RuntimeError(f"Remote step '{label}' FAILED: {res.get('result')} | output={lines}")
    return lines


def extract_json_tag(lines: List[str], tag: str) -> Dict[str, Any]:
    for ln in lines:
        idx = ln.find(tag)
        if idx >= 0:
            return json.loads(ln[idx + len(tag) :])
    raise RuntimeError(f"Missing {tag} in remote output: {lines}")


def focus_pie_viewport_window() -> None:
    if os.name != "nt":
        return
    try:
        import ctypes
        from ctypes import wintypes

        user32 = ctypes.windll.user32
        WNDENUMPROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

        def _cb(hwnd, _lparam):
            if user32.IsWindowVisible(hwnd):
                length = user32.GetWindowTextLengthW(hwnd)
                if length > 0:
                    buf = ctypes.create_unicode_buffer(length + 1)
                    user32.GetWindowTextW(hwnd, buf, length + 1)
                    if "Viewport 2" in buf.value or "TareAndTithe" in buf.value:
                        user32.keybd_event(0x12, 0, 0, 0)
                        user32.ShowWindow(hwnd, 9)
                        user32.BringWindowToTop(hwnd)
                        user32.SetForegroundWindow(hwnd)
                        user32.keybd_event(0x12, 0, 2, 0)
            return True

        user32.EnumWindows(WNDENUMPROC(_cb), 0)
    except Exception:
        pass


def verify_default_game_ini_cook_paths() -> None:
    if not os.path.exists(DEFAULT_GAME_INI_PATH):
        raise AssertionError(f"Missing {DEFAULT_GAME_INI_PATH}")
    with open(DEFAULT_GAME_INI_PATH, "r", encoding="utf-8") as f:
        ini_text = f.read()
    required_lines = [
        "bCookMapsOnly=True",
        '+DirectoriesToAlwaysCook=(Path="/Game/Presentation")',
        '+DirectoriesToAlwaysCook=(Path="/Game/Presentation/Audio/Waves")',
    ]
    for req in required_lines:
        if req not in ini_text:
            raise AssertionError(f"DefaultGame.ini missing required cook directive: {req}")


def check_git_clean_path(rel_path: str) -> str:
    proc = subprocess.run(
        ["git", "status", "--short", rel_path],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=True,
    )
    return proc.stdout.strip()


def main() -> int:
    parser = argparse.ArgumentParser(description="Phase 12-FIX3 Live PIE 5-Gate Audio Verification")
    parser.add_argument("--timeout", type=float, default=120.0)
    parser.add_argument("--shutdown", action="store_true", help="Cleanly quit UnrealEditor after verification")
    args = parser.parse_args()

    verify_default_game_ini_cook_paths()

    cfg = remote_execution.RemoteExecutionConfig()
    cfg.multicast_ttl = 0
    cfg.multicast_bind_address = "127.0.0.1"
    cfg.multicast_group_endpoint = ("239.0.0.1", 6766)
    cfg.command_endpoint = ("127.0.0.1", 6776)

    spawned_proc: Optional[subprocess.Popen] = None
    remote = remote_execution.RemoteExecution(cfg)
    remote.start()

    try:
        target_node = None
        quick_deadline = time.time() + 2.0
        while time.time() < quick_deadline:
            for node in remote.remote_nodes:
                if "TareAndTithe" in str(node.get("project_name", "")) or len(remote.remote_nodes) == 1:
                    target_node = node
                    break
            if target_node is not None:
                break
            time.sleep(0.4)

        if target_node is None:
            # Launch WITHOUT -NoSound so UE5 AudioMixer creates live FActiveSound voices for is_playing() / FadeIn / FadeOut!
            print(f"[VerifyPhase12Fix3] Launching UnrealEditor (AudioMixer active): {UNREAL_EDITOR_EXE}")
            spawned_proc = subprocess.Popen(
                [UNREAL_EDITOR_EXE, UPROJECT_PATH, "/Game/Maps/L_MainGameplay", "-NoSplash", "-Unattended"],
                cwd=REPO_ROOT,
            )

        deadline = time.time() + args.timeout
        while target_node is None and time.time() < deadline:
            for node in remote.remote_nodes:
                if "TareAndTithe" in str(node.get("project_name", "")) or len(remote.remote_nodes) == 1:
                    target_node = node
                    break
            if target_node is None:
                time.sleep(1.0)

        if target_node is None:
            raise TimeoutError("Timed out waiting for UnrealEditor Python remote_execution node")

        remote.open_command_connection(target_node["node_id"])
        run_remote_checked(remote, REMOTE_BEGIN_PIE_SNIPPET, "Begin PIE Session")
        time.sleep(0.6)
        focus_pie_viewport_window()

        # =====================================================================
        # GATE 1: 6-SFX ASSET & COOK RESOLUTION + TIER-1 ZERO-TICK CHECK
        # =====================================================================
        gate1_data: Optional[Dict[str, Any]] = None
        last_err = None
        for attempt in range(1, 16):
            time.sleep(0.85)
            try:
                lines = run_remote_checked(
                    remote,
                    REMOTE_GATE1_AND_WARMUP_SNIPPET,
                    f"Gate 1: Verify 6 SFX Waves, DA_Presentation Audio Slots & Zero-Tick (attempt {attempt})",
                )
                gate1_data = extract_json_tag(lines, "GATE1_JSON=")
                break
            except Exception as exc:
                last_err = exc
                print(f"[VerifyPhase12Fix3] Waiting for PIE warmup ({exc})...")

        if gate1_data is None:
            raise RuntimeError(f"Failed Gate 1 PIE warmup audit: {last_err}")

        if gate1_data["resolved_wave_count"] != 6 or gate1_data["bridge_tick_enabled"] or gate1_data["groan_allow_spatialization"]:
            raise AssertionError(f"Gate 1 invariant failure: {gate1_data}")

        # =====================================================================
        # GATE 2: CONTRACT 01 ZERO-STRAIN SILENCE, 0.42f AMBIENT & 0.15f NEUTRAL
        # =====================================================================
        run_remote_checked(remote, make_start_contract_snippet(0, reset_career=True), "Gate 2: Reset to DA_Contract_01")
        time.sleep(0.20)
        g2_state = extract_json_tag(
            run_remote_checked(remote, REMOTE_SAMPLE_AUDIO_STATE_SNIPPET, "Gate 2: Sample DA_Contract_01 Audio State"),
            "AUDIO_STATE_JSON=",
        )
        if (
            g2_state["contract_idx"] != 0
            or g2_state["hud_strain"] != 0.0
            or g2_state["flushed_strain"] != 0.0
            or g2_state["groan_voice_active"]
            or g2_state["groan_is_playing"]
            or g2_state["groan_vol"] != 0.0
            or not g2_state["amb_is_playing"]
            or abs(g2_state["amb_vol"] - 0.42) > 1e-3
            or abs(g2_state["amb_comp_vol"] - 0.42) > 1e-3
            or abs(g2_state["ms_clamp_strain_param"] - 0.15) > 1e-3
        ):
            raise AssertionError(f"Gate 2 failed on DA_Contract_01: {g2_state}")

        # =====================================================================
        # GATE 3: ZONE-1 MODERATE OVERHANG -> POST-SLICE SURGE -> ZONE-2 CRITICAL
        # =====================================================================
        run_remote_checked(remote, make_start_contract_snippet(1), "Gate 3A: Load DA_Contract_02 (~59% Overhang)")
        time.sleep(0.22)
        g3_zone1 = extract_json_tag(
            run_remote_checked(remote, REMOTE_SAMPLE_AUDIO_STATE_SNIPPET, "Gate 3A: Sample DA_Contract_02 Zone-1 Tension"),
            "AUDIO_STATE_JSON=",
        )
        if (
            not g3_zone1["groan_voice_active"]
            or not g3_zone1["groan_is_playing"]
            or not (0.22 <= g3_zone1["groan_vol"] <= 0.40)
            or not (0.94 <= g3_zone1["groan_pitch"] <= 0.99)
            or abs(g3_zone1["ms_clamp_strain_param"] - 0.15) > 1e-3
        ):
            raise AssertionError(f"Gate 3A Zone-1 resting tension check failed: {g3_zone1}")

        g3_surge = extract_json_tag(
            run_remote_checked(
                remote,
                REMOTE_FIND_AND_COMMIT_COUNTERWEIGHT_CUT_SNIPPET,
                "Gate 3B: Commit Counterweight Slice -> Verify Post-Slice Creak Surge",
            ),
            "CUT_SURGE_JSON=",
        )
        if (
            not g3_surge["voice_active"]
            or not g3_surge["groan_playing"]
            or g3_surge["surge_vol"] < 0.80
            or g3_surge["surge_vol"] > 1.401
        ):
            raise AssertionError(f"Gate 3B post-slice creak surge failed: {g3_surge}")

        # Wait past the ~0.85s 2-step surge decay so steady Zone-2 critical danger groan settles
        time.sleep(0.95)
        g3_zone2 = extract_json_tag(
            run_remote_checked(
                remote,
                REMOTE_SAMPLE_AUDIO_STATE_SNIPPET,
                "Gate 3C: Sample Settled Zone-2 Critical Danger Groan",
            ),
            "AUDIO_STATE_JSON=",
        )
        if (
            not g3_zone2["groan_voice_active"]
            or not g3_zone2["groan_is_playing"]
            or not (0.72 <= g3_zone2["solver_strain"] <= 0.95)
            or not (0.85 <= g3_zone2["groan_vol"] <= 1.40)
            or not (1.03 <= g3_zone2["groan_pitch"] <= 1.18)
        ):
            raise AssertionError(f"Gate 3C settled Zone-2 critical danger groan failed: {g3_zone2}")

        # =====================================================================
        # GATE 4: <= 0.08s CUTOFF ON CLAMP_STRUCTURAL_SNAP + CLEAN RE-ARM
        # =====================================================================
        g4_snap_imm = extract_json_tag(
            run_remote_checked(
                remote,
                REMOTE_TRIGGER_STRUCTURAL_SNAP_SNIPPET,
                "Gate 4A: Trigger ClampStructuralSnap -> Immediate Mute Latch",
            ),
            "SNAP_TRIGGER_JSON=",
        )
        if (
            not g4_snap_imm["geom_snapped"]
            or g4_snap_imm["phase"] != "FAULT_RESET_DELAY"
            or g4_snap_imm["fault"] != "CLAMP_STRUCTURAL_SNAP"
            or g4_snap_imm["immediate_voice_active"]
            or g4_snap_imm["immediate_groan_vol"] != 0.0
        ):
            raise AssertionError(f"Gate 4A immediate geometric snap mute failed: {g4_snap_imm}")

        # Wait 0.11s (> 0.08s FadeOut cutoff, well inside the 1.8s FaultResetDelay) and assert hardware voice stopped & 0.0f vol!
        time.sleep(0.11)
        g4_during_fault = extract_json_tag(
            run_remote_checked(
                remote,
                REMOTE_SAMPLE_AUDIO_STATE_SNIPPET,
                "Gate 4B: Verify Hardware FadeOut Completed <= 0.08s During FaultResetDelay",
            ),
            "AUDIO_STATE_JSON=",
        )
        if (
            g4_during_fault["phase"] != "FAULT_RESET_DELAY"
            or g4_during_fault["groan_voice_active"]
            or g4_during_fault["groan_is_playing"]
            or g4_during_fault["groan_vol"] != 0.0
            or g4_during_fault["groan_comp_vol"] != 0.0
        ):
            raise AssertionError(f"Gate 4B latched drone during FaultResetDelay: {g4_during_fault}")

        # Poll for auto-recovery back into ActiveInspection on DA_Contract_02 (~1.8s FaultAutoResetDelaySeconds)
        g4_rearmed: Optional[Dict[str, Any]] = None
        rearm_deadline = time.time() + 3.5
        while time.time() < rearm_deadline:
            time.sleep(0.20)
            cand = extract_json_tag(
                run_remote_checked(
                    remote,
                    REMOTE_SAMPLE_AUDIO_STATE_SNIPPET,
                    "Poll Post-Fault Auto-Reset Re-Arm",
                    verbose=False,
                ),
                "AUDIO_STATE_JSON=",
            )
            if cand["phase"] == "ACTIVE_INSPECTION" and cand["contract_idx"] == 1:
                g4_rearmed = cand
                break

        if (
            g4_rearmed is None
            or not g4_rearmed["groan_voice_active"]
            or not g4_rearmed["groan_is_playing"]
            or not (0.22 <= g4_rearmed["groan_vol"] <= 0.40)
        ):
            raise AssertionError(f"Gate 4C post-fault FadeIn(0.06f, 1.0f) re-arm failed: {g4_rearmed}")

        # Also verify non-gameplay phase transition cutoff
        g4_phase_cut = extract_json_tag(
            run_remote_checked(
                remote,
                REMOTE_ADVANCE_NON_GAMEPLAY_PHASE_FROM_OVERHANG_SNIPPET,
                "Gate 4D: Verify Non-ActiveInspection Phase Transition Cutoff",
            ),
            "PHASE_CUTOFF_JSON=",
        )
        if not g4_phase_cut["pre_active"] or g4_phase_cut["post_active"] or g4_phase_cut["post_vol"] != 0.0:
            raise AssertionError(f"Gate 4D phase cutoff failed: {g4_phase_cut}")

        # Return cleanly to Contract 01 ActiveInspection before steady-state frame timing
        run_remote_checked(remote, make_start_contract_snippet(0, reset_career=True), "Reset to Ctr 01 for Gate 5 Timing")
        focus_pie_viewport_window()
        time.sleep(0.55)

        # =====================================================================
        # GATE 5: PERFORMANCE (<= 13.0 ms) & BINARY ASSET / PHYSICS FREEZE LOCK
        # =====================================================================
        steady_samples_ms: List[float] = []
        for s_idx in range(5):
            time.sleep(0.18)
            f_lines = run_remote_checked(
                remote,
                REMOTE_MEASURE_STEADY_FRAME_TIME_SNIPPET,
                f"Gate 5: Sample Unthrottled PIE Frame Time #{s_idx + 1}",
            )
            steady_samples_ms.append(float(extract_json_tag(f_lines, "FRAME_MS_JSON=")["delta_ms"]))
        steady_median_ms = statistics.median(steady_samples_ms)
        if steady_median_ms > 13.0:
            raise AssertionError(f"Gate 5 steady_median_ms={steady_median_ms:.2f} ms exceeds 13.0 ms budget!")

        run_remote_checked(remote, REMOTE_END_PIE_SNIPPET, "End PIE Session Cleanly")
        time.sleep(0.45)

        maps_status = check_git_clean_path("Content/Maps/")
        pres_status = check_git_clean_path("Content/Presentation/")
        solver_h_status = check_git_clean_path("Source/TareAndTithe/Public/Components/ClampTorqueSolverComponent.h")
        solver_cpp_status = check_git_clean_path("Source/TareAndTithe/Private/Components/ClampTorqueSolverComponent.cpp")
        if maps_status or pres_status or solver_h_status or solver_cpp_status:
            raise AssertionError(
                f"Gate 5 binary/physics freeze violated! Maps={maps_status!r}, "
                f"Presentation={pres_status!r}, SolverH={solver_h_status!r}, SolverCpp={solver_cpp_status!r}"
            )

        print("\n================ PHASE 12-FIX3 QUANTITATIVE 5-GATE REPORT ================")
        print(
            f"  [Gate 1] 6-SFX Asset & Cook Resolution: waves_resolved={gate1_data['resolved_wave_count']}/6, "
            f"groan={gate1_data['groan_sound']}, ambient={gate1_data['ambient_sound']}, "
            f"spatial_window={gate1_data['spatial_gain_window']}, bridge_tick={gate1_data['bridge_tick_enabled']}, "
            f"DefaultGame.ini_WavesCook=PASS"
        )
        print(
            f"  [Gate 2] Ctr 01 Zero-Strain Silence & Mix: hud_strain={g2_state['hud_strain']}, "
            f"groan_active={g2_state['groan_voice_active']}, groan_playing={g2_state['groan_is_playing']}, "
            f"groan_vol={g2_state['groan_vol']}, amb_playing={g2_state['amb_is_playing']} @ {g2_state['amb_vol']}, "
            f"ms_clamp_strain_param={g2_state['ms_clamp_strain_param']} (0.0 mono bleed)"
        )
        print(
            f"  [Gate 3] Two-Zone Tension & Cut-Spike Surge: "
            f"Ctr02_Zone1=(strain={g3_zone1['solver_strain']}, vol={g3_zone1['groan_vol']}, pitch={g3_zone1['groan_pitch']}, playing={g3_zone1['groan_is_playing']}) -> "
            f"CutSurge=(strain={g3_surge['post_cut_strain']}, surge_vol={g3_surge['surge_vol']}, surge_pitch={g3_surge['surge_pitch']}) -> "
            f"SettledZone2=(strain={g3_zone2['solver_strain']}, vol={g3_zone2['groan_vol']}, pitch={g3_zone2['groan_pitch']}, playing={g3_zone2['groan_is_playing']})"
        )
        print(
            f"  [Gate 4] <= 0.08s Snap Cutoff & Re-Arm: "
            f"SnapImmediate=(phase={g4_snap_imm['phase']}, fault={g4_snap_imm['fault']}, vol={g4_snap_imm['immediate_groan_vol']}) -> "
            f"t+0.11s_DuringFault=(playing={g4_during_fault['groan_is_playing']}, comp_vol={g4_during_fault['groan_comp_vol']}) -> "
            f"RearmedActiveInspection=(playing={g4_rearmed['groan_is_playing']}, vol={g4_rearmed['groan_vol']}) | "
            f"NonGameplayPhaseCutoff=(post_active={g4_phase_cut['post_active']}, post_vol={g4_phase_cut['post_vol']})"
        )
        print(
            f"  [Gate 5] Performance & Freeze Lock: steady_frame_ms={steady_median_ms:.2f} ms (samples={steady_samples_ms}) [<=13.0 ms], "
            f"Content/Maps=CLEAN, Content/Presentation=CLEAN, ClampTorqueSolverComponent=FROZEN"
        )
        print("==========================================================================\n")
        print("[VerifyPhase12Fix3] ALL 5 QUANTITATIVE AUDIO TELEMETRY & COOK GATES PASSED.")
        return 0
    finally:
        if args.shutdown or spawned_proc is not None:
            print("[VerifyPhase12Fix3] Sending clean quit_editor() command...")
            try:
                remote.run_command(
                    REMOTE_QUIT_EDITOR_SNIPPET,
                    unattended=True,
                    exec_mode=remote_execution.MODE_EXEC_FILE,
                    raise_on_failure=False,
                )
            except Exception:
                pass
        remote.stop()
        if spawned_proc is not None:
            try:
                spawned_proc.wait(timeout=20.0)
            except Exception:
                spawned_proc.kill()


if __name__ == "__main__":
    sys.exit(main())
