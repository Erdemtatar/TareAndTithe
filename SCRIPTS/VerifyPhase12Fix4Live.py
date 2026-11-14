"""
Phase 12-FIX4 Live PIE 4-Gate Verification & Screenshot Harness.

Verifies all 5 Phase 12-FIX4 sub-systems & all 4 Mandatory Engineering Corrections:
  - Gate 1 (6-State Unified Laser/Reticle Priority & OvershootMagenta - Correction 1):
    1:1 state parity between AInspectorPawn (M_LaserSheet_Master MID) and UInspectorHudBridgeComponent
    across all 6 ELaserSliceVisualState outcomes:
      P1 FaultOrCriticalRed (VeinIntersect, TareOverflow, FaultResetDelay lockout)
      P2 VeinHazardYellow (bSeveredHalfContainsVein && Route == TitheScale)
      P3 TitheQuotaOvershootMagenta (OvershootMagenta (1.363, 0.058, 1.131) < 1.65 BloomThreshold,
         Reticle [OVER!], never triggered on Route == TareChute)
      P4 ValidQuotaOrPurgeEmerald ([IN WINDOW!] / [DIRECTOR'S CUT!] / [PURGE READY])
      P5 IsotopeProximityAmber (VeinProx >= 0.35 strictly BELOW P3/P4 so veined contracts never blind
         [OVER!] or [IN WINDOW!])
      P6 IdleOrSafeCyan (no contact SafeIdleColor, under-quota Tithe & clean Tare IntersectCyanColor)
    Plus continuous capacitor heat-surge modulation (LaserChargeNormalized > 0.02 -> [SLICING...]).
  - Gate 2 (Left "CONTROLS" Slate Card, [H] Toggle & Bottom CRT De-Clutter - Correction 4):
    Bottom CRT legend removed & AnalogBarTopLocalY > MassBottomLocalY; SLeftControlsCard stacked
    below TopLeftPanel in top-left SVerticalBox with Width <= 245 px and U_right <= 0.135 across
    both 1920x1080 (100% DPI) and 1536x864 (125% DPI) checks, 0 px overlap against TopLeftPanel,
    left Panopticon window (U >= 0.22), center banners, reticle, and bottom console; [H] bound and
    toggles expanded <-> "[H] CONTROLS" micro-pill while preserving FInputModeGameOnly.
  - Gate 3 (Stationary Flapper Hinge, Tare Ember Fill, [1]/[2]/[3] Upgrades & 16-Part Humanoid Director
    - Corrections 2, 3, 4):
    Stationary DiverterHingeAxleVisual + Front/Rear stanchions under ScaleRoot remain at 0.000 delta
    under [SPACE] +45 deg flapper swing and Tithe pan depression; TareHopperFurnaceGlowCard rises
    ScaleRoot Z: -38 -> -6 cm with camera-facing 10 deg rake on Tare waste and drops on [2] TARE
    upgrade; [1] HYD, [2] TARE, [3] SCAN toggle cleanly across L0 -> L1 -> L2 -> Reset L0 with
    0.000 overlap against M_LaserSheet_Master padded OBB, NoCollision, and CastShadow = False;
    16-part parallax-compensated Humanoid Director silhouette verified in Right Bay.
    Saves 3 verification PNGs strictly under gitignored Saved/Automation/Phase12Fix4/.
  - Gate 4 (GPU Frame Budget & Binary Locks):
    Steady PIE frame time <= 13.0 ms, 1 shadow-casting light (KeySpot), 0 bytes diff on
    Content/Maps/L_MainGameplay.umap and Content/Presentation/DA_Presentation.uasset,
    0 untracked .uasset files.
"""

import sys

sys.dont_write_bytecode = True

import argparse
import json
import os
import statistics
import struct
import subprocess
import time
import zlib
from typing import Any, Dict, List, Optional, Tuple

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
OUT_DIR = os.path.join(REPO_ROOT, "Saved", "Automation", "Phase12Fix4")

SHOT_A_OVERSHOOT = os.path.join(OUT_DIR, "Phase12Fix4_OvershootMagenta_Controls_Hinge.png")
SHOT_B_UPGRADES = os.path.join(OUT_DIR, "Phase12Fix4_MicroPill_L2Upgrades_TareGlow.png")
SHOT_C_DIRECTOR = os.path.join(OUT_DIR, "Phase12Fix4_HumanoidDirector_Closeup.png")
CLEAN_DIRECTOR_PROBE = os.path.join(OUT_DIR, "_Phase12Fix4_DirectorCleanLitProbe.png")
REPORT_JSON_PATH = os.path.join(OUT_DIR, "Phase12Fix4_VerificationReport.json")


REMOTE_BEGIN_PIE_SNIPPET = """
import unreal

try:
    ed_perf = unreal.get_default_object(unreal.EditorPerformanceSettings)
    ed_perf.set_editor_property("throttle_cpu_when_not_foreground", False)
except Exception:
    pass

for cmd in ("r.VSync 0", "r.VSyncEditor 0", "Slate.MaxFPS 240", "t.MaxFPS 240", "t.UnfocusedFrameRateLimit 0"):
    unreal.SystemLibrary.execute_console_command(None, cmd)

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les.is_in_play_in_editor():
    les.editor_request_end_play()
les.editor_request_begin_play()
unreal.log("[VerifyPhase12Fix4] Requested BeginPlay PIE session.")
"""


REMOTE_WARMUP_AND_SCENE_AUDIT_SNIPPET = """
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

all_actors = unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor)
shadow_lights = []
zero_shadow_lights = []
volumetric_fog_enabled = False
pawns, rigs, scales, booths, framings = [], [], [], [], []

for actor in all_actors:
    cls_name = actor.get_class().get_name()
    if "InspectorPawn" in cls_name:
        pawns.append(actor)
    elif "MonolithRigActor" in cls_name:
        rigs.append(actor)
    elif "ScaleAssemblyActor" in cls_name:
        scales.append(actor)
    elif "DirectorBoothActor" in cls_name:
        booths.append(actor)
    elif "CellFramingArchitectureActor" in cls_name:
        framings.append(actor)

    for lc in actor.get_components_by_class(unreal.LightComponentBase):
        entry = f"{cls_name}.{lc.get_name()}"
        if bool(lc.get_editor_property("cast_shadows")):
            shadow_lights.append(entry)
        else:
            zero_shadow_lights.append(entry)

    for fog in actor.get_components_by_class(unreal.ExponentialHeightFogComponent):
        if bool(fog.get_editor_property("enable_volumetric_fog")):
            volumetric_fog_enabled = True

if len(pawns) != 1 or len(rigs) != 1 or len(scales) != 1 or len(booths) != 1 or len(framings) != 1:
    raise AssertionError(
        f"Expected 1 Pawn/Rig/Scale/Booth/Framing; got {len(pawns)},{len(rigs)},{len(scales)},{len(booths)},{len(framings)}"
    )
if len(shadow_lights) != 1:
    raise AssertionError(f"Expected shadow_lights == 1, got {len(shadow_lights)}: {shadow_lights}")
if len(zero_shadow_lights) != 10:
    raise AssertionError(f"Expected zero_shadow_lights == 10, got {len(zero_shadow_lights)}: {zero_shadow_lights}")
if volumetric_fog_enabled:
    raise AssertionError("VolumetricFog hard ban violated!")

pawn = pawns[0]
hud_bridge = pawn.get_component_by_class(unreal.InspectorHudBridgeComponent)
if hud_bridge is None or not bool(hud_bridge.is_overlay_mounted()):
    raise AssertionError("InspectorHudBridgeComponent missing or STareTitheHudOverlay not mounted!")

unreal.log("SCENE_AUDIT_JSON=" + json.dumps({
    "world_time_sec": round(world_time_sec, 3),
    "shadow_lights": shadow_lights,
    "zero_shadow_light_count": len(zero_shadow_lights),
    "volumetric_fog": volumetric_fog_enabled,
    "h_bound": bool(pawn.is_controls_toggle_bound_to_h_key_for_test()),
    "game_only_cursor_hidden": bool(pawn.is_mouse_cursor_hidden_game_only_for_test()),
}))
"""


REMOTE_GATE1_CONTRACT0_STATES_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]
hud_bridge = pawn.get_component_by_class(unreal.InspectorHudBridgeComponent)

if not bool(shift_sub.execute_career_reset_to_shift_one()):
    raise RuntimeError("ExecuteCareerResetToShiftOne failed!")

snap = shift_sub.get_current_snapshot()
if snap.get_editor_property("active_diverter_route") != unreal.DiverterRoute.TITHE_SCALE:
    pawn.toggle_diverter_route_for_test()

target_kg = float(snap.get_editor_property("target_tithe_mass_kg"))
tol_kg = float(snap.get_editor_property("quota_tolerance_kg"))
min_win = target_kg - tol_kg
max_win = target_kg + tol_kg

tb = rig.get_target_world_bounds()
cm = pawn.get_cached_cuttable_mask_for_test()
cut_top_z = float(cm.get_editor_property("center_world").z + cm.get_editor_property("half_extents_cm").z)
min_z = float(tb.min.z)

under_z = None
emerald_z = None
best_emerald_err = 999.0
overshoot_z = None

z = min_z + 2.5
while z <= cut_top_z - 2.5:
    pawn.set_laser_plane_pose_for_test(z, 0.0, 0.0, 0.0)
    tel = pawn.get_cached_preview_telemetry_for_test()
    if (
        bool(tel.get_editor_property("intersects_monolith"))
        and bool(tel.get_editor_property("has_valid_mass_estimate"))
        and not bool(tel.get_editor_property("will_detonate"))
    ):
        est = float(tel.get_editor_property("estimated_severed_mass_kg"))
        if est >= 0.25 and est < (min_win - 1.5) and under_z is None:
            under_z = z
        err = abs(est - target_kg)
        if err <= tol_kg - 0.15 and err < best_emerald_err:
            best_emerald_err = err
            emerald_z = z
        if est > (max_win + 2.5) and z >= 188.0 and overshoot_z is None:
            overshoot_z = z
    z += 0.60

if under_z is None or emerald_z is None or overshoot_z is None:
    raise AssertionError(f"Failed to locate Contract 0 poses: under={under_z}, emerald={emerald_z}, over={overshoot_z}")

def col_rgb(c):
    return [round(float(c.r), 4), round(float(c.g), 4), round(float(c.b), 4)]

def get_mid_vec(mid, pname):
    for fn_name in ("k2_get_vector_parameter_value", "get_vector_parameter_value"):
        fn = getattr(mid, fn_name, None)
        if callable(fn):
            return col_rgb(fn(pname))
    raise RuntimeError("Cannot read vector param " + pname)

sheet_mid = pawn.get_editor_property("laser_sheet_visual").get_material(0)

def sample_state(z_val, h_val=0.0, charge=0.0):
    pawn.set_laser_plane_pose_for_test(float(z_val), float(h_val), 0.0, 0.0)
    pawn.set_capacitor_charge_for_test(float(charge))
    l_state = pawn.get_current_laser_visual_state_for_test().name
    r_state = hud_bridge.get_last_reticle_visual_state_for_test().name
    if l_state != r_state:
        raise AssertionError(f"Split-brain mismatch at z={z_val}: laser={l_state} != reticle={r_state}")
    em = col_rgb(pawn.get_current_Laser_final_emissive_for_test()) if hasattr(pawn, "get_current_Laser_final_emissive_for_test") else col_rgb(pawn.get_current_laser_final_emissive_for_test())
    mid_em = get_mid_vec(sheet_mid, "EmissiveColorConst")
    if max(abs(em[i] - mid_em[i]) for i in range(3)) > 1.5e-3:
        raise AssertionError(f"LaserSheetMID EmissiveColorConst {mid_em} != {em}")
    return {
        "z": round(float(z_val), 2),
        "laser_state": l_state,
        "reticle_state": r_state,
        "laser_emissive": em,
        "max_emissive_ch": round(max(em), 4),
        "reticle_text": str(hud_bridge.get_last_reticle_yield_text_for_test()),
        "reticle_color": col_rgb(hud_bridge.get_last_reticle_yield_color_for_test()),
    }

idle_no_contact = sample_state(min_z - 18.0, h_val=-16.0)
under_tithe = sample_state(under_z)
emerald_tithe = sample_state(emerald_z)
overshoot_tithe = sample_state(overshoot_z)
slicing_charge = sample_state(overshoot_z, charge=0.65)
pawn.set_capacitor_charge_for_test(0.0)

# Find first contract with bAllowDiverterToggle == True (Contract 1 or 2) to verify TareChute NEVER triggers OvershootMagenta
tare_clean_scrap = None
tare_overflow_red = None
for div_c in (1, 2, 3):
    shift_sub.start_shift_from_contract(div_c)
    if bool(shift_sub.request_toggle_diverter()):
        # Now on TARE_CHUTE! Toggle back to TITHE_SCALE first to find a clean non-vein cut with est <= max_tare (and est > max_tare if available)
        shift_sub.request_toggle_diverter()
        snap_d = shift_sub.get_current_snapshot()
        max_tare_d = float(snap_d.get_editor_property("max_allowed_tare_waste_kg"))
        max_win_d = float(snap_d.get_editor_property("target_tithe_mass_kg")) + float(snap_d.get_editor_property("quota_tolerance_kg"))
        tb_d = rig.get_target_world_bounds()
        cm_d = pawn.get_cached_cuttable_mask_for_test()
        top_d = float(cm_d.get_editor_property("center_world").z + cm_d.get_editor_property("half_extents_cm").z)
        zz = float(tb_d.min.z) + 2.0
        clean_z_d = None
        over_tare_z_d = None
        while zz <= top_d - 2.0:
            pawn.set_laser_plane_pose_for_test(zz, 0.0, 0.0, 0.0)
            td = pawn.get_cached_preview_telemetry_for_test()
            if (
                bool(td.get_editor_property("intersects_monolith"))
                and bool(td.get_editor_property("has_valid_mass_estimate"))
                and not bool(td.get_editor_property("will_detonate"))
                and not bool(td.get_editor_property("severed_half_contains_vein"))
                and float(td.get_editor_property("vein_proximity_normalized")) < 0.35
            ):
                est_d = float(td.get_editor_property("estimated_severed_mass_kg"))
                if 0.5 <= est_d <= max_tare_d - 0.5 and clean_z_d is None:
                    clean_z_d = zz
                if est_d > max_tare_d + 1.0 and over_tare_z_d is None:
                    over_tare_z_d = zz
            zz += 0.60
        if clean_z_d is not None:
            shift_sub.request_toggle_diverter()  # -> TARE_CHUTE
            tare_clean_scrap = sample_state(clean_z_d)
            if over_tare_z_d is not None:
                tare_overflow_red = sample_state(over_tare_z_d)
            break

if tare_clean_scrap is None:
    raise AssertionError("Failed to find clean TareChute test cut on diverter-enabled contracts!")

# Restore Contract 0 (Shift 01) on TitheScale and park on overshoot_z for Screenshot A!
shift_sub.execute_career_reset_to_shift_one()
overshoot_restored = sample_state(overshoot_z)

if idle_no_contact["laser_state"] != "IDLE_OR_SAFE_CYAN":
    raise AssertionError(f"Expected IDLE_OR_SAFE_CYAN on no-contact, got {idle_no_contact}")
if under_tithe["laser_state"] != "IDLE_OR_SAFE_CYAN":
    raise AssertionError(f"Expected IDLE_OR_SAFE_CYAN on under-quota Tithe, got {under_tithe}")
if emerald_tithe["laser_state"] != "VALID_QUOTA_OR_PURGE_EMERALD":
    raise AssertionError(f"Expected VALID_QUOTA_OR_PURGE_EMERALD in window, got {emerald_tithe}")
if overshoot_tithe["laser_state"] != "TITHE_QUOTA_OVERSHOOT_MAGENTA":
    raise AssertionError(f"Expected TITHE_QUOTA_OVERSHOOT_MAGENTA on Tithe overshoot, got {overshoot_tithe}")
if overshoot_tithe["max_emissive_ch"] >= 1.65 or abs(overshoot_tithe["max_emissive_ch"] - 1.363) > 0.01:
    raise AssertionError(f"OvershootMagenta emissive violated BloomThreshold (<1.65, ~1.363): {overshoot_tithe}")
if "[OVER!]" not in overshoot_tithe["reticle_text"]:
    raise AssertionError(f"Reticle missing [OVER!] on Tithe overshoot: {overshoot_tithe}")
if tare_clean_scrap["laser_state"] != "IDLE_OR_SAFE_CYAN" or "[OVER!]" in tare_clean_scrap["reticle_text"]:
    raise AssertionError(f"TareChute falsely triggered OvershootMagenta: {tare_clean_scrap}")
if tare_overflow_red is not None and tare_overflow_red["laser_state"] != "FAULT_OR_CRITICAL_RED":
    raise AssertionError(f"Expected FAULT_OR_CRITICAL_RED on TareChute overflow, got {tare_overflow_red}")
if "[SLICING..." not in slicing_charge["reticle_text"]:
    raise AssertionError(f"Active capacitor charge missing [SLICING...] feedback: {slicing_charge}")

unreal.log("GATE1_C0_JSON=" + json.dumps({
    "emerald_z": round(emerald_z, 2),
    "overshoot_z": round(overshoot_z, 2),
    "under_z": round(under_z, 2),
    "idle_no_contact": idle_no_contact,
    "under_tithe": under_tithe,
    "emerald_tithe": emerald_tithe,
    "overshoot_tithe": overshoot_tithe,
    "tare_clean_scrap": tare_clean_scrap,
    "tare_overflow_red": tare_overflow_red,
    "slicing_charge": slicing_charge,
}))
"""


REMOTE_GATE1_VEINED_PRIORITY_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]
hud_bridge = pawn.get_component_by_class(unreal.InspectorHudBridgeComponent)

total_contracts = 5
crimson_hit = None
yellow_vein = None
emerald_purge = None
amber_prox = None
emerald_over_prox = None
magenta_over_prox = None

# Sweep contracts (indices 1..5) across Z, roll, and bevel to verify P1..P5 including Correction 1!
for c_idx in (1, 2, 3, 4, 5):
    if not bool(shift_sub.start_shift_from_contract(c_idx)):
        continue
    snap = shift_sub.get_current_snapshot()
    if snap.get_editor_property("active_diverter_route") != unreal.DiverterRoute.TITHE_SCALE:
        shift_sub.request_toggle_diverter()
    max_tare_c = float(snap.get_editor_property("max_allowed_tare_waste_kg"))
    tb = rig.get_target_world_bounds()
    cm = pawn.get_cached_cuttable_mask_for_test()
    z_lo = float(tb.min.z) + 2.0
    z_hi = float(cm.get_editor_property("center_world").z + cm.get_editor_property("half_extents_cm").z) - 2.5

    for roll_deg, bev_deg in ((0.0, 0.0), (15.0, 0.0), (-15.0, 0.0), (26.0, 0.0), (-26.0, 0.0), (0.0, 15.0), (0.0, -15.0)):
        z = z_lo
        while z <= z_hi:
            pawn.set_laser_plane_pose_for_test(z, 0.0, roll_deg, bev_deg)
            tel = pawn.get_cached_preview_telemetry_for_test()
            if (
                bool(tel.get_editor_property("intersects_monolith"))
                and bool(tel.get_editor_property("has_valid_mass_estimate"))
            ):
                est = float(tel.get_editor_property("estimated_severed_mass_kg"))
                if est >= 0.15:
                    prox = float(tel.get_editor_property("vein_proximity_normalized"))
                    det = bool(tel.get_editor_property("will_detonate"))
                    has_v = bool(tel.get_editor_property("severed_half_contains_vein"))
                    l_st = pawn.get_current_laser_visual_state_for_test().name
                    r_st = hud_bridge.get_last_reticle_visual_state_for_test().name
                    if l_st != r_st:
                        raise AssertionError(f"Split-brain on contract {c_idx} z={z}: {l_st} != {r_st}")
                    r_txt = str(hud_bridge.get_last_reticle_yield_text_for_test())

                    if det and crimson_hit is None:
                        crimson_hit = {"c": c_idx, "z": round(z, 2), "state": l_st, "text": r_txt}
                    elif (not det) and has_v:
                        if yellow_vein is None:
                            yellow_vein = {"c": c_idx, "z": round(z, 2), "state": l_st, "text": r_txt}
                        if emerald_purge is None and est <= max_tare_c - 0.15:
                            if bool(shift_sub.request_toggle_diverter()):
                                pawn.set_laser_plane_pose_for_test(z, 0.0, roll_deg, bev_deg)
                                lp_st = pawn.get_current_laser_visual_state_for_test().name
                                rp_st = hud_bridge.get_last_reticle_visual_state_for_test().name
                                if lp_st != rp_st:
                                    raise AssertionError(f"Purge split-brain: {lp_st} != {rp_st}")
                                if lp_st == "VALID_QUOTA_OR_PURGE_EMERALD":
                                    emerald_purge = {"c": c_idx, "z": round(z, 2), "state": lp_st, "text": str(hud_bridge.get_last_reticle_yield_text_for_test())}
                                shift_sub.request_toggle_diverter()
                                pawn.set_laser_plane_pose_for_test(z, 0.0, roll_deg, bev_deg)
                    elif (not det) and (not has_v) and (0.36 <= prox < 0.85):
                        if l_st == "ISOTOPE_PROXIMITY_AMBER" and amber_prox is None:
                            amber_prox = {"c": c_idx, "z": round(z, 2), "roll": roll_deg, "bevel": bev_deg, "prox": round(prox, 3), "state": l_st, "text": r_txt}
                        elif l_st == "VALID_QUOTA_OR_PURGE_EMERALD" and emerald_over_prox is None:
                            emerald_over_prox = {"c": c_idx, "z": round(z, 2), "roll": roll_deg, "bevel": bev_deg, "prox": round(prox, 3), "state": l_st, "text": r_txt}
                        elif l_st == "TITHE_QUOTA_OVERSHOOT_MAGENTA" and magenta_over_prox is None:
                            magenta_over_prox = {"c": c_idx, "z": round(z, 2), "roll": roll_deg, "bevel": bev_deg, "prox": round(prox, 3), "state": l_st, "text": r_txt}
            z += 0.90
        if crimson_hit and yellow_vein and emerald_purge and amber_prox and emerald_over_prox and magenta_over_prox:
            break
    if crimson_hit and yellow_vein and emerald_purge and amber_prox and emerald_over_prox and magenta_over_prox:
        break

if amber_prox is None:
    raise AssertionError("Failed to find ISOTOPE_PROXIMITY_AMBER pose on veined contracts!")
if crimson_hit is None or crimson_hit["state"] != "FAULT_OR_CRITICAL_RED":
    raise AssertionError(f"Failed P1 direct vein hit check: {crimson_hit}")
if yellow_vein is None or yellow_vein["state"] != "VEIN_HAZARD_YELLOW":
    raise AssertionError(f"Failed P2 vein-in-drop yellow check: {yellow_vein}")
if emerald_purge is None or emerald_purge["state"] != "VALID_QUOTA_OR_PURGE_EMERALD":
    raise AssertionError(f"Failed P4 Tare purge emerald check: {emerald_purge}")

# If emerald_over_prox or magenta_over_prox was not hit prior to lower-vein purge,
# start multi-vein contract 4 (DA_Contract_05), route to TARE_CHUTE (and KEEP IT on TARE_CHUTE while chunk falls!),
# Step 1 of 3 on DA_Contract_05 (c_idx = 4): harvest clean base chunk (~4.7 kg) BELOW Vein 0 onto TITHE_SCALE!
harvested_base = False
base_est_kg = 0.0
if emerald_over_prox is None or magenta_over_prox is None:
    shift_sub.start_shift_from_contract(4)
    if shift_sub.get_current_snapshot().get_editor_property("active_diverter_route") != unreal.DiverterRoute.TITHE_SCALE:
        shift_sub.request_toggle_diverter()
    ap = unreal.LaserSliceAperture()
    ap.set_editor_property("enforce_finite_sheet_bounds", False)
    tb_m = rig.get_target_world_bounds()
    best_base_z = None
    zz = float(tb_m.min.z) + 1.5
    while zz <= float(tb_m.min.z) + 10.0:
        pawn.set_laser_plane_pose_for_test(zz, 0.0, 0.0, 0.0)
        tm = pawn.get_cached_preview_telemetry_for_test()
        if (
            bool(tm.get_editor_property("intersects_monolith"))
            and bool(tm.get_editor_property("has_valid_mass_estimate"))
            and not bool(tm.get_editor_property("will_detonate"))
            and not bool(tm.get_editor_property("severed_half_contains_vein"))
        ):
            est_b = float(tm.get_editor_property("estimated_severed_mass_kg"))
            if est_b >= 2.8:
                best_base_z = zz
                base_est_kg = round(est_b, 2)
        zz += 0.35
    if best_base_z is not None:
        cx_b = 0.5 * (float(tb_m.min.x) + float(tb_m.max.x))
        cy_b = 0.5 * (float(tb_m.min.y) + float(tb_m.max.y))
        sum_b = rig.commit_slice_plane(unreal.Vector(cx_b, cy_b, best_base_z), unreal.Vector(0.0, 0.0, -1.0), ap)
        if sum_b.get_editor_property("outcome") != unreal.CommitSliceOutcome.SUCCESS:
            raise AssertionError(f"Base harvest slice failed: {sum_b.get_editor_property('outcome')}")
        harvested_base = True

unreal.log("GATE1_VEIN_STAGE1_JSON=" + json.dumps({
    "crimson_hit": crimson_hit,
    "yellow_vein": yellow_vein,
    "emerald_purge": emerald_purge,
    "amber_prox": amber_prox,
    "emerald_over_prox": emerald_over_prox,
    "magenta_over_prox": magenta_over_prox,
    "harvested_base": harvested_base,
    "base_est_kg": base_est_kg,
}))
"""


REMOTE_GATE1_VEINED_UPPER_SWEEP_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]

# Step 2 of 3 on DA_Contract_05: base chunk (~4.7 kg) is now settled on TITHE_SCALE -> switch to TARE_CHUTE and purge Vein 0!
settled_before_purge = round(float(shift_sub.get_current_snapshot().get_editor_property("settled_scale_mass_kg")), 2)
if shift_sub.get_current_snapshot().get_editor_property("active_diverter_route") != unreal.DiverterRoute.TARE_CHUTE:
    shift_sub.request_toggle_diverter()

max_tare_m = float(shift_sub.get_current_snapshot().get_editor_property("max_allowed_tare_waste_kg"))
tb = rig.get_target_world_bounds()
cm = pawn.get_cached_cuttable_mask_for_test()
z_hi = float(cm.get_editor_property("center_world").z + cm.get_editor_property("half_extents_cm").z) - 2.5

purged_vein0 = False
purge_kg = 0.0
z = float(tb.min.z) + 2.0
while z <= z_hi - 16.0:
    pawn.set_laser_plane_pose_for_test(z, 0.0, 0.0, 0.0)
    tm = pawn.get_cached_preview_telemetry_for_test()
    if (
        bool(tm.get_editor_property("intersects_monolith"))
        and bool(tm.get_editor_property("has_valid_mass_estimate"))
        and not bool(tm.get_editor_property("will_detonate"))
        and bool(tm.get_editor_property("severed_half_contains_vein"))
    ):
        est_p = float(tm.get_editor_property("estimated_severed_mass_kg"))
        if est_p <= max_tare_m - 0.5:
            ap = unreal.LaserSliceAperture()
            ap.set_editor_property("enforce_finite_sheet_bounds", False)
            cx_p = 0.5 * (float(tb.min.x) + float(tb.max.x))
            cy_p = 0.5 * (float(tb.min.y) + float(tb.max.y))
            sum_p = rig.commit_slice_plane(unreal.Vector(cx_p, cy_p, z), unreal.Vector(0.0, 0.0, -1.0), ap)
            if sum_p.get_editor_property("outcome") != unreal.CommitSliceOutcome.SUCCESS:
                raise AssertionError(f"Vein0 purge slice failed: {sum_p.get_editor_property('outcome')}")
            purged_vein0 = True
            purge_kg = round(est_p, 2)
            break
    z += 0.50

unreal.log("GATE1_VEIN_STAGE2A_JSON=" + json.dumps({
    "settled_before_purge_kg": settled_before_purge,
    "purged_vein0": purged_vein0,
    "purge_kg": purge_kg,
    "emerald_over_prox": None,
    "magenta_over_prox": None,
}))
"""


REMOTE_GATE1_VEINED_POST_PRELOAD_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]
hud_bridge = pawn.get_component_by_class(unreal.InspectorHudBridgeComponent)

# Step 3 of 3 on DA_Contract_05: Vein 0 has despawned in Tare abyss & ~4.7 kg sits on TITHE_SCALE -> toggle back to TITHE_SCALE
# and sweep Z beneath Vein 1 (0.35 <= prox < 0.85), crossing BOTH [20.10, 21.90] (Emerald) and > 21.90 (OvershootMagenta)!
if shift_sub.get_current_snapshot().get_editor_property("active_diverter_route") != unreal.DiverterRoute.TITHE_SCALE:
    shift_sub.request_toggle_diverter()

tb = rig.get_target_world_bounds()
cm = pawn.get_cached_cuttable_mask_for_test()
z_lo = float(tb.min.z) + 1.5
z_hi = float(cm.get_editor_property("center_world").z + cm.get_editor_property("half_extents_cm").z) - 2.0

emerald_over_prox = None
magenta_over_prox = None

for roll_deg, bev_deg in ((0.0, 0.0), (12.0, 0.0), (-12.0, 0.0), (22.0, 0.0), (-22.0, 0.0), (0.0, 14.0), (0.0, -14.0)):
    z = z_lo
    while z <= z_hi:
        pawn.set_laser_plane_pose_for_test(z, 0.0, roll_deg, bev_deg)
        tel = pawn.get_cached_preview_telemetry_for_test()
        if (
            bool(tel.get_editor_property("intersects_monolith"))
            and bool(tel.get_editor_property("has_valid_mass_estimate"))
            and not bool(tel.get_editor_property("will_detonate"))
        ):
            prox = float(tel.get_editor_property("vein_proximity_normalized"))
            det = bool(tel.get_editor_property("will_detonate"))
            has_v = bool(tel.get_editor_property("severed_half_contains_vein"))
            if (not det) and (not has_v) and (0.35 <= prox < 0.85):
                l_st = pawn.get_current_laser_visual_state_for_test().name
                r_st = hud_bridge.get_last_reticle_visual_state_for_test().name
                if l_st != r_st:
                    raise AssertionError(f"Post-preload split-brain: {l_st} != {r_st}")
                if l_st == "VALID_QUOTA_OR_PURGE_EMERALD" and emerald_over_prox is None:
                    emerald_over_prox = {"z": round(z, 2), "roll": roll_deg, "bevel": bev_deg, "prox": round(prox, 3), "state": l_st, "text": str(hud_bridge.get_last_reticle_yield_text_for_test())}
                elif l_st == "TITHE_QUOTA_OVERSHOOT_MAGENTA" and magenta_over_prox is None:
                    magenta_over_prox = {"z": round(z, 2), "roll": roll_deg, "bevel": bev_deg, "prox": round(prox, 3), "state": l_st, "text": str(hud_bridge.get_last_reticle_yield_text_for_test())}
        z += 0.30
    if emerald_over_prox and magenta_over_prox:
        break

# Trigger manual contract reset -> verify P1 FaultOrCriticalRed lockout synchronization!
shift_sub.request_manual_contract_reset()
fault_phase = shift_sub.get_current_phase().name
fault_l_st = pawn.get_current_laser_visual_state_for_test().name
fault_r_st = hud_bridge.get_last_reticle_visual_state_for_test().name
fault_txt = str(hud_bridge.get_last_reticle_yield_text_for_test())
if fault_phase != "FAULT_RESET_DELAY" or fault_l_st != "FAULT_OR_CRITICAL_RED" or fault_r_st != "FAULT_OR_CRITICAL_RED":
    raise AssertionError(f"FaultResetDelay lockout desync: phase={fault_phase}, laser={fault_l_st}, reticle={fault_r_st}")

unreal.log("GATE1_VEIN_STAGE2_JSON=" + json.dumps({
    "emerald_over_prox": emerald_over_prox,
    "magenta_over_prox": magenta_over_prox,
    "fault_lockout": {"phase": fault_phase, "laser_state": fault_l_st, "reticle_state": fault_r_st, "text": fault_txt},
}))
"""


REMOTE_GATE2_INSPECT_HUD_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
hud_bridge = pawn.get_component_by_class(unreal.InspectorHudBridgeComponent)

vp = hud_bridge.get_last_local_viewport_size()
vp_w, vp_h = float(vp.x), float(vp.y)

def v2(vec):
    return [round(float(vec.x), 2), round(float(vec.y), 2)]

tl_min, tl_max = v2(hud_bridge.get_top_left_panel_bounds_min()), v2(hud_bridge.get_top_left_panel_bounds_max())
cc_min, cc_max = v2(hud_bridge.get_controls_card_bounds_min()), v2(hud_bridge.get_controls_card_bounds_max())
tr_min, tr_max = v2(hud_bridge.get_top_right_panel_bounds_min()), v2(hud_bridge.get_top_right_panel_bounds_max())
cb_min, cb_max = v2(hud_bridge.get_center_banner_bounds_min()), v2(hud_bridge.get_center_banner_bounds_max())
rc_min, rc_max = v2(hud_bridge.get_reticle_card_bounds_min()), v2(hud_bridge.get_reticle_card_bounds_max())
bc_min, bc_max = v2(hud_bridge.get_bottom_console_bounds_min()), v2(hud_bridge.get_bottom_console_bounds_max())

named_boxes = []
for name, bmin, bmax in (
    ("TopLeft", tl_min, tl_max),
    ("ControlsCard", cc_min, cc_max),
    ("TopRight", tr_min, tr_max),
    ("CenterBanner", cb_min, cb_max),
    ("ReticleCard", rc_min, rc_max),
    ("BottomConsole", bc_min, bc_max),
):
    if (bmax[0] - bmin[0]) > 1.0 and (bmax[1] - bmin[1]) > 1.0:
        named_boxes.append((name, bmin, bmax))

overlaps = []
for i in range(len(named_boxes)):
    n1, a0, a1 = named_boxes[i]
    for j in range(i + 1, len(named_boxes)):
        n2, b0, b1 = named_boxes[j]
        ix = min(a1[0], b1[0]) - max(a0[0], b0[0])
        iy = min(a1[1], b1[1]) - max(a0[1], b0[1])
        if ix > 0.5 and iy > 0.5:
            overlaps.append(f"{n1}<->{n2} ({ix:.1f}x{iy:.1f}px)")

cc_w = round(cc_max[0] - cc_min[0], 2)
cc_h = round(cc_max[1] - cc_min[1], 2)
cc_u_right = round(cc_max[0] / max(1.0, vp_w), 4)
mass_bot_y = round(float(hud_bridge.get_bottom_console_mass_bottom_local_y()), 2)
bar_top_y = round(float(hud_bridge.get_bottom_console_analog_bar_top_local_y()), 2)

unreal.log("GATE2_HUD_JSON=" + json.dumps({
    "local_vp": [round(vp_w, 2), round(vp_h, 2)],
    "expanded": bool(hud_bridge.is_controls_card_expanded()),
    "h_bound": bool(pawn.is_controls_toggle_bound_to_h_key_for_test()),
    "game_only_cursor_hidden": bool(pawn.is_mouse_cursor_hidden_game_only_for_test()),
    "cc_box": [cc_min, cc_max],
    "cc_w": cc_w,
    "cc_h": cc_h,
    "cc_u_right": cc_u_right,
    "tl_box": [tl_min, tl_max],
    "bc_box": [bc_min, bc_max],
    "bc_h": round(bc_max[1] - bc_min[1], 2),
    "mass_bottom_local_y": mass_bot_y,
    "analog_bar_top_local_y": bar_top_y,
    "overlaps": overlaps,
}))
"""


def make_set_viewport_override_snippet(w: float, h: float, enabled: bool) -> str:
    return f"""
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
hud_bridge = pawn.get_component_by_class(unreal.InspectorHudBridgeComponent)
hud_bridge.override_viewport_size_for_test(unreal.Vector2D(float({w}), float({h})), bool({enabled}))
"""


REMOTE_TOGGLE_CONTROLS_H_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
hud_bridge = pawn.get_component_by_class(unreal.InspectorHudBridgeComponent)
pawn.toggle_controls_hud_for_test()
unreal.log("TOGGLE_H_JSON=" + json.dumps({
    "expanded": bool(hud_bridge.is_controls_card_expanded()),
    "game_only_cursor_hidden": bool(pawn.is_mouse_cursor_hidden_game_only_for_test()),
}))
"""


REMOTE_GATE3_PRE_SWING_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
scale_actor = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "ScaleAssemblyActor" in a.get_class().get_name()][0]

shift_sub.execute_career_reset_to_shift_one()
shift_sub.start_shift_from_contract(2)
if shift_sub.get_current_snapshot().get_editor_property("active_diverter_route") != unreal.DiverterRoute.TITHE_SCALE:
    shift_sub.request_toggle_diverter()

def xf_list(comp):
    loc = comp.get_world_location()
    rot = comp.get_world_rotation()
    return [float(loc.x), float(loc.y), float(loc.z), float(rot.pitch), float(rot.yaw), float(rot.roll)]

flapper = scale_actor.get_editor_property("diverter_flapper_visual")
pivot = scale_actor.get_editor_property("diverter_flapper_pivot")
axle = scale_actor.get_editor_property("diverter_hinge_axle_visual")
brk_f = scale_actor.get_editor_property("diverter_hinge_bracket_front_visual")
brk_r = scale_actor.get_editor_property("diverter_hinge_bracket_rear_visual")

pre_data = {
    "axle": xf_list(axle),
    "brk_f": xf_list(brk_f),
    "brk_r": xf_list(brk_r),
    "flapper": xf_list(flapper),
    "pivot_rel_roll": float(pivot.get_editor_property("relative_rotation").roll),
}

if not bool(shift_sub.request_toggle_diverter()):
    raise AssertionError("Failed to toggle diverter on Contract 2!")

unreal.log("GATE3_PRE_SWING_JSON=" + json.dumps(pre_data))
"""


def make_gate3_post_swing_and_director_snippet(pre_data: Dict[str, Any]) -> str:
    return f"pre_data = {json.dumps(pre_data)}\n" + _GATE3_POST_SWING_BODY


_GATE3_POST_SWING_BODY = """
import json
import math
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]
scale_actor = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "ScaleAssemblyActor" in a.get_class().get_name()][0]
booth = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "DirectorBoothActor" in a.get_class().get_name()][0]

def assert_no_collision_no_shadow(comp, label):
    if comp is None:
        raise AssertionError(f"Missing component {label}")
    if comp.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION:
        raise AssertionError(f"{label} must have NoCollision!")
    if bool(comp.get_editor_property("cast_shadow")):
        raise AssertionError(f"{label} must have CastShadow=False!")

scale_root = scale_actor.get_editor_property("scale_root")
flapper = scale_actor.get_editor_property("diverter_flapper_visual")
pivot = scale_actor.get_editor_property("diverter_flapper_pivot")
axle = scale_actor.get_editor_property("diverter_hinge_axle_visual")
brk_f = scale_actor.get_editor_property("diverter_hinge_bracket_front_visual")
brk_r = scale_actor.get_editor_property("diverter_hinge_bracket_rear_visual")
ember_card = scale_actor.get_editor_property("tare_hopper_furnace_glow_card")

for c, lbl in (
    (axle, "DiverterHingeAxleVisual"),
    (brk_f, "DiverterHingeBracketFrontVisual"),
    (brk_r, "DiverterHingeBracketRearVisual"),
    (ember_card, "TareHopperFurnaceGlowCard"),
):
    assert_no_collision_no_shadow(c, lbl)
    if c.get_attach_parent() != scale_root:
        raise AssertionError(f"{lbl} must be attached strictly to ScaleRoot!")

def xf_list(comp):
    loc = comp.get_world_location()
    rot = comp.get_world_rotation()
    return [float(loc.x), float(loc.y), float(loc.z), float(rot.pitch), float(rot.yaw), float(rot.roll)]

def ang_diff(a, b):
    return abs(((a - b + 180.0) % 360.0) - 180.0)

def max_xf_delta(a, b):
    return max(
        abs(a[0] - b[0]),
        abs(a[1] - b[1]),
        abs(a[2] - b[2]),
        ang_diff(a[3], b[3]),
        ang_diff(a[4], b[4]),
        ang_diff(a[5], b[5]),
    )

flap_xf1 = xf_list(flapper)
hinge_delta_toggle = max(
    max_xf_delta(pre_data["axle"], xf_list(axle)),
    max_xf_delta(pre_data["brk_f"], xf_list(brk_f)),
    max_xf_delta(pre_data["brk_r"], xf_list(brk_r)),
)
flapper_rot_delta = max(
    ang_diff(flap_xf1[3], pre_data["flapper"][3]),
    ang_diff(flap_xf1[4], pre_data["flapper"][4]),
    ang_diff(flap_xf1[5], pre_data["flapper"][5]),
    ang_diff(float(pivot.get_editor_property("relative_rotation").roll), float(pre_data["pivot_rel_roll"])),
)
if flapper_rot_delta < 35.0:
    raise AssertionError(f"Expected flapper swing >= 35 deg on [SPACE], got {flapper_rot_delta}")
if hinge_delta_toggle > 1.0e-4:
    raise AssertionError(f"Stationary hinge moved during [SPACE] toggle! delta={hinge_delta_toggle}")

# Toggle back to TitheScale and reset to Shift 1 (L0)
shift_sub.request_toggle_diverter()
shift_sub.execute_career_reset_to_shift_one()

# Check empty Tare ember card pose & camera-facing rake
ember_rel_z0 = float(ember_card.get_editor_property("relative_location").z)
ember_bb = ember_card.get_editor_property("static_mesh").get_bounding_box()
ember_xf = ember_card.get_world_transform()
front_edge_wz = float(unreal.MathLibrary.transform_location(ember_xf, unreal.Vector(ember_bb.min.x, 0.0, 0.0)).z)
center_wz = float(unreal.MathLibrary.transform_location(ember_xf, unreal.Vector(0.0, 0.0, 0.0)).z)
back_edge_wz = float(unreal.MathLibrary.transform_location(ember_xf, unreal.Vector(ember_bb.max.x, 0.0, 0.0)).z)
if abs(ember_rel_z0 - (-38.0)) > 0.05 or not (back_edge_wz > center_wz + 5.0 and center_wz > front_edge_wz + 5.0):
    raise AssertionError(
        f"TareHopperFurnaceGlowCard empty Z/rake mismatch: rel_z={ember_rel_z0}, front_wz={front_edge_wz}, ctr_wz={center_wz}, back_wz={back_edge_wz}"
    )

# Verify 16-part Humanoid Director Silhouette in ADirectorBoothActor
cubes_ism = booth.get_editor_property("director_silhouette_cubes_ism")
cyls_ism = booth.get_editor_property("director_silhouette_cylinders_ism")
assert_no_collision_no_shadow(cubes_ism, "DirectorSilhouetteCubesISM")
assert_no_collision_no_shadow(cyls_ism, "DirectorSilhouetteCylindersISM")
cube_cnt = int(cubes_ism.get_instance_count())
cyl_cnt = int(cyls_ism.get_instance_count())
tot_sil = int(booth.get_director_silhouette_instance_count())
if (cube_cnt, cyl_cnt, tot_sil) != (9, 7, 16):
    raise AssertionError(f"Expected (9 cubes, 7 cyls, 16 total) Director instances, got ({cube_cnt}, {cyl_cnt}, {tot_sil})")

# Inspect local instance transforms on DirectorBoothActor: verify roi_right_glass Y in [215, 245] is 100% clear,
# downward shoulder slope, narrow 4.60 cm neck in Band 2-3, and peaked cap in Band 4-5 (< +18.18 cm shutter)
max_inst_y = -999.0
max_inst_z = -999.0
neck_width_y = None
cap_top_z = -999.0

for ism in (cubes_ism, cyls_ism):
    sm_bb = ism.get_editor_property("static_mesh").get_bounding_box()
    for idx in range(ism.get_instance_count()):
        xf = ism.get_instance_transform(idx, False)
        pts = [
            unreal.MathLibrary.transform_location(xf, unreal.Vector(bx, by, bz))
            for bx in (sm_bb.min.x, sm_bb.max.x)
            for by in (sm_bb.min.y, sm_bb.max.y)
            for bz in (sm_bb.min.z, sm_bb.max.z)
        ]
        ys = [float(p.y) for p in pts]
        zs = [float(p.z) for p in pts]
        max_inst_y = max(max_inst_y, max(ys))
        max_inst_z = max(max_inst_z, max(zs))
        ctr_z = 0.5 * (min(zs) + max(zs))
        ctr_y = 0.5 * (min(ys) + max(ys))
        if ism == cyls_ism and 5.5 <= ctr_z <= 7.5 and 170.0 <= ctr_y <= 182.0:
            neck_width_y = round(max(ys) - min(ys), 2)
        if ism == cubes_ism and ctr_z >= 14.8:
            cap_top_z = round(max(zs), 2)

if max_inst_y >= 215.0:
    raise AssertionError(f"Director silhouette encroached on roi_right_glass (Y >= 215): max_y={max_inst_y}")
if neck_width_y is None or not (4.0 <= neck_width_y <= 5.8):
    raise AssertionError(f"Expected narrow human neck (~4.60 cm) in Band 2-3, got {neck_width_y}")
if not (15.6 <= cap_top_z <= 18.15):
    raise AssertionError(f"Expected flared peaked cap rising into Band 4-5 (<18.18 shutter), got cap_top_z={cap_top_z}")

# Check all 16 Upgrade components (4 on ScaleAssemblyActor, 12 on MonolithRigActor): NoCollision, CastShadow=False, L0 hidden, 0.000 OBB overlap
rig_upg_names = [
    "hyd_upgrade_cylinder_left_l1",
    "hyd_upgrade_cylinder_right_l1",
    "hyd_upgrade_accumulator_left_l2",
    "hyd_upgrade_accumulator_right_l2",
    "scan_upgrade_pod_left_l1",
    "scan_upgrade_pod_right_l1",
    "scan_upgrade_lens_left_l1",
    "scan_upgrade_lens_right_l1",
    "scan_upgrade_array_left_l2",
    "scan_upgrade_array_right_l2",
    "scan_upgrade_emitter_left_l2",
    "scan_upgrade_emitter_right_l2",
]
scale_upg_names = [
    "tare_upgrade_collar_front_l1",
    "tare_upgrade_collar_outer_l1",
    "tare_upgrade_heat_shield_front_l2",
    "tare_upgrade_heat_shield_corner_l2",
]

cm = pawn.get_cached_cuttable_mask_for_test()
cm_ctr = cm.get_editor_property("center_world")
cm_he = cm.get_editor_property("half_extents_cm")
# Padded M_LaserSheet_Master OBB in world space (axis-aligned at neutral mount: pad=(22, 6, 2))
obb_min = [float(cm_ctr.x - cm_he.x - 22.0), float(cm_ctr.y - cm_he.y - 6.0), float(cm_ctr.z - cm_he.z - 2.0)]
obb_max = [float(cm_ctr.x + cm_he.x + 22.0), float(cm_ctr.y + cm_he.y + 6.0), float(cm_ctr.z + cm_he.z + 2.0)]

min_rig_upg_wz = 9999.0
min_rig_upg_abs_wy = 9999.0

for pname in rig_upg_names:
    comp = rig.get_editor_property(pname)
    assert_no_collision_no_shadow(comp, pname)
    if bool(comp.is_visible()):
        raise AssertionError(f"{pname} must be hidden at L0!")
    bb = comp.get_editor_property("static_mesh").get_bounding_box()
    xf = comp.get_world_transform()
    pts = [
        unreal.MathLibrary.transform_location(xf, unreal.Vector(bx, by, bz))
        for bx in (bb.min.x, bb.max.x)
        for by in (bb.min.y, bb.max.y)
        for bz in (bb.min.z, bb.max.z)
    ]
    cmin = [min(float(p.x) for p in pts), min(float(p.y) for p in pts), min(float(p.z) for p in pts)]
    cmax = [max(float(p.x) for p in pts), max(float(p.y) for p in pts), max(float(p.z) for p in pts)]
    min_rig_upg_wz = min(min_rig_upg_wz, cmin[2])
    min_rig_upg_abs_wy = min(min_rig_upg_abs_wy, min(abs(float(p.y)) for p in pts))
    ox = min(cmax[0], obb_max[0]) - max(cmin[0], obb_min[0])
    oy = min(cmax[1], obb_max[1]) - max(cmin[1], obb_min[1])
    oz = min(cmax[2], obb_max[2]) - max(cmin[2], obb_min[2])
    if ox > 0.0 and oy > 0.0 and oz > 0.0:
        raise AssertionError(f"Rig upgrade {pname} overlaps M_LaserSheet_Master OBB by ({ox:.2f},{oy:.2f},{oz:.2f}) cm!")

for pname in scale_upg_names:
    comp = scale_actor.get_editor_property(pname)
    assert_no_collision_no_shadow(comp, pname)
    if bool(comp.is_visible()):
        raise AssertionError(f"{pname} must be hidden at L0!")

unreal.log("GATE3_STATIC_JSON=" + json.dumps({
    "hinge_delta_toggle": round(hinge_delta_toggle, 6),
    "flapper_rot_delta_deg": round(flapper_rot_delta, 2),
    "ember_empty_rel_z": round(ember_rel_z0, 2),
    "ember_rake_world_z": [round(front_edge_wz, 2), round(center_wz, 2), round(back_edge_wz, 2)],
    "director_counts": [cube_cnt, cyl_cnt, tot_sil],
    "director_max_y": round(max_inst_y, 2),
    "director_neck_width_y": neck_width_y,
    "director_cap_top_z": cap_top_z,
    "min_rig_upg_wz": round(min_rig_upg_wz, 2),
    "min_rig_upg_abs_wy": round(min_rig_upg_abs_wy, 2),
}))
"""


REMOTE_START_C0_AND_SLICE_DIRECTOR_CUT_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]
scale_actor = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "ScaleAssemblyActor" in a.get_class().get_name()][0]

shift_sub.start_shift_from_contract(0)
snap = shift_sub.get_current_snapshot()
if snap.get_editor_property("active_diverter_route") != unreal.DiverterRoute.TITHE_SCALE:
    pawn.toggle_diverter_route_for_test()

target_kg = float(snap.get_editor_property("target_tithe_mass_kg"))
tb = rig.get_target_world_bounds()
cm = pawn.get_cached_cuttable_mask_for_test()
cut_top_z = float(cm.get_editor_property("center_world").z + cm.get_editor_property("half_extents_cm").z)

best_z = None
best_err = 999.0
z = float(tb.min.z) + 2.0
while z <= cut_top_z - 2.0:
    pawn.set_laser_plane_pose_for_test(z, 0.0, 0.0, 0.0)
    tel = pawn.get_cached_preview_telemetry_for_test()
    if (
        bool(tel.get_editor_property("intersects_monolith"))
        and bool(tel.get_editor_property("has_valid_mass_estimate"))
        and not bool(tel.get_editor_property("will_detonate"))
    ):
        err = abs(float(tel.get_editor_property("estimated_severed_mass_kg")) - target_kg)
        if err < best_err:
            best_err = err
            best_z = z
    z += 0.25

pan_z0 = float(scale_actor.get_editor_property("pan_motion_root").get_world_location().z)
axle_z0 = float(scale_actor.get_editor_property("diverter_hinge_axle_visual").get_world_location().z)

cx = 0.5 * (float(tb.min.x) + float(tb.max.x))
cy = 0.5 * (float(tb.min.y) + float(tb.max.y))
ap = unreal.LaserSliceAperture()
ap.set_editor_property("enforce_finite_sheet_bounds", False)
summary = rig.commit_slice_plane(unreal.Vector(cx, cy, best_z), unreal.Vector(0.0, 0.0, -1.0), ap)

unreal.log("C0_SLICE_JSON=" + json.dumps({
    "cut_z": round(best_z, 2),
    "outcome": summary.get_editor_property("outcome").name,
    "pan_z0": round(pan_z0, 4),
    "axle_z0": round(axle_z0, 4),
}))
"""


REMOTE_POLL_SETTLED_AND_STAMP_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
scale_actor = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "ScaleAssemblyActor" in a.get_class().get_name()][0]

pan_z1 = float(scale_actor.get_editor_property("pan_motion_root").get_world_location().z)
axle_z1 = float(scale_actor.get_editor_property("diverter_hinge_axle_visual").get_world_location().z)
stamped = bool(shift_sub.try_stamp_quota_verdict())
scrip = int(shift_sub.get_scrip_balance())

unreal.log("STAMP_POLL_JSON=" + json.dumps({
    "stamped": stamped,
    "scrip": scrip,
    "pan_z1": round(pan_z1, 4),
    "axle_z1": round(axle_z1, 4),
}))
"""


REMOTE_ROUTE_TARE_WASTE_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]

best_z = None
max_tare = 15.0
for dc in (1, 2, 3):
    shift_sub.start_shift_from_contract(dc)
    snap = shift_sub.get_current_snapshot()
    if snap.get_editor_property("active_diverter_route") != unreal.DiverterRoute.TARE_CHUTE:
        if not bool(shift_sub.request_toggle_diverter()):
            continue
    max_tare = float(snap.get_editor_property("max_allowed_tare_waste_kg"))
    min_win = float(snap.get_editor_property("target_tithe_mass_kg")) - float(snap.get_editor_property("quota_tolerance_kg"))
    target_waste = 0.78 * max_tare
    tb = rig.get_target_world_bounds()
    cm = pawn.get_cached_cuttable_mask_for_test()
    cut_top_z = float(cm.get_editor_property("center_world").z + cm.get_editor_property("half_extents_cm").z)
    best_err = 999.0
    z = float(tb.min.z) + 2.0
    while z <= cut_top_z - 2.0:
        pawn.set_laser_plane_pose_for_test(z, 0.0, 0.0, 0.0)
        tel = pawn.get_cached_preview_telemetry_for_test()
        if (
            bool(tel.get_editor_property("intersects_monolith"))
            and bool(tel.get_editor_property("has_valid_mass_estimate"))
            and not bool(tel.get_editor_property("will_detonate"))
        ):
            est = float(tel.get_editor_property("estimated_severed_mass_kg"))
            rem = float(tel.get_editor_property("estimated_remainder_mass_kg"))
            if 0.45 * max_tare <= est <= 0.88 * max_tare and rem >= min_win + 0.8:
                err = abs(est - target_waste)
                if err < best_err:
                    best_err = err
                    best_z = z
        z += 0.35
    if best_z is not None:
        break

if best_z is None:
    raise AssertionError("Could not find clean Tare drop slice on diverter-enabled contracts!")

tb = rig.get_target_world_bounds()
cx = 0.5 * (float(tb.min.x) + float(tb.max.x))
cy = 0.5 * (float(tb.min.y) + float(tb.max.y))
ap = unreal.LaserSliceAperture()
ap.set_editor_property("enforce_finite_sheet_bounds", False)
summary = rig.commit_slice_plane(unreal.Vector(cx, cy, best_z), unreal.Vector(0.0, 0.0, -1.0), ap)

unreal.log("TARE_DROP_JSON=" + json.dumps({
    "cut_z": round(best_z, 2),
    "spawned_kg": round(float(summary.get_editor_property("spawned_chunk_mass_kg")), 3),
    "max_tare_kg": round(max_tare, 3),
}))
"""


REMOTE_VERIFY_EMBER_AND_BUY_UPGRADES_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]
scale_actor = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "ScaleAssemblyActor" in a.get_class().get_name()][0]
ember_card = scale_actor.get_editor_property("tare_hopper_furnace_glow_card")

snap0 = shift_sub.get_current_snapshot()
waste_kg = float(snap0.get_editor_property("accumulated_tare_waste_kg"))
fill_l0 = float(scale_actor.get_tare_hopper_ember_fill_normalized_for_test())
z_l0 = float(ember_card.get_editor_property("relative_location").z)
em_l0_col = scale_actor.get_tare_hopper_ember_current_emissive_for_test()
em_l0_max = max(float(em_l0_col.r), float(em_l0_col.g), float(em_l0_col.b))

if waste_kg <= 0.5 or fill_l0 <= 0.35 or z_l0 <= -30.0:
    raise AssertionError(f"Tare chunk did not raise ember fill! waste={waste_kg}, fill={fill_l0}, z={z_l0}")
if em_l0_max >= 1.65:
    raise AssertionError(f"Tare ember emissive exceeded 1.65 BloomThreshold: {em_l0_max}")

# Verify chunk plunged cleanly through NoCollision ember card and despawned in TareAbyssDespawnTriggerBox!
active_chunks = [
    a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor)
    if "SeveredChunkActor" in a.get_class().get_name() and not bool(a.get_editor_property("hidden"))
]

# Purchase L1 on [1] HYD, [2] TARE, [3] SCAN
for trk in (
    unreal.RequisitionTrack.CLAMP_HYDRAULICS,
    unreal.RequisitionTrack.TARE_HOPPER,
    unreal.RequisitionTrack.VEIN_SCANNER_DAMPER,
):
    if not bool(shift_sub.try_purchase_requisition(trk)):
        raise AssertionError(f"Failed L1 purchase for {trk} (scrip={shift_sub.get_scrip_balance()})")

fill_l1 = float(scale_actor.get_tare_hopper_ember_fill_normalized_for_test())
z_l1 = float(ember_card.get_editor_property("relative_location").z)
if not (fill_l1 < fill_l0 - 0.05 and z_l1 < z_l0 - 1.5):
    raise AssertionError(f"[2] TARE L1 (+5 kg capacity) failed to drop ember fill/Z: L0=({fill_l0},{z_l0}) -> L1=({fill_l1},{z_l1})")

# Verify L1 visible & L2 still hidden across all 16 components!
l1_comps = [
    rig.get_editor_property("hyd_upgrade_cylinder_left_l1"),
    rig.get_editor_property("hyd_upgrade_cylinder_right_l1"),
    rig.get_editor_property("scan_upgrade_pod_left_l1"),
    rig.get_editor_property("scan_upgrade_pod_right_l1"),
    rig.get_editor_property("scan_upgrade_lens_left_l1"),
    rig.get_editor_property("scan_upgrade_lens_right_l1"),
    scale_actor.get_editor_property("tare_upgrade_collar_front_l1"),
    scale_actor.get_editor_property("tare_upgrade_collar_outer_l1"),
]
l2_comps = [
    rig.get_editor_property("hyd_upgrade_accumulator_left_l2"),
    rig.get_editor_property("hyd_upgrade_accumulator_right_l2"),
    rig.get_editor_property("scan_upgrade_array_left_l2"),
    rig.get_editor_property("scan_upgrade_array_right_l2"),
    rig.get_editor_property("scan_upgrade_emitter_left_l2"),
    rig.get_editor_property("scan_upgrade_emitter_right_l2"),
    scale_actor.get_editor_property("tare_upgrade_heat_shield_front_l2"),
    scale_actor.get_editor_property("tare_upgrade_heat_shield_corner_l2"),
]
if not all(bool(c.is_visible()) for c in l1_comps) or any(bool(c.is_visible()) for c in l2_comps):
    raise AssertionError("L1 visibility tier check failed (expected L1=True, L2=False)!")

# Purchase L2 on [1] HYD, [2] TARE, [3] SCAN
for trk in (
    unreal.RequisitionTrack.CLAMP_HYDRAULICS,
    unreal.RequisitionTrack.TARE_HOPPER,
    unreal.RequisitionTrack.VEIN_SCANNER_DAMPER,
):
    if not bool(shift_sub.try_purchase_requisition(trk)):
        raise AssertionError(f"Failed L2 purchase for {trk} (scrip={shift_sub.get_scrip_balance()})")

if not all(bool(c.is_visible()) for c in (l1_comps + l2_comps)):
    raise AssertionError("L2 visibility tier check failed (expected all 16 L1+L2 components visible)!")

fill_l2 = float(scale_actor.get_tare_hopper_ember_fill_normalized_for_test())
z_l2 = float(ember_card.get_editor_property("relative_location").z)
if not (fill_l2 < fill_l1 - 0.03 and z_l2 < z_l1 - 1.0):
    raise AssertionError(f"[2] TARE L2 (+10 kg capacity) failed to drop ember fill/Z: L1=({fill_l1},{z_l1}) -> L2=({fill_l2},{z_l2})")

unreal.log("GATE3_UPGRADES_JSON=" + json.dumps({
    "waste_kg": round(waste_kg, 3),
    "fill_l0_l1_l2": [round(fill_l0, 3), round(fill_l1, 3), round(fill_l2, 3)],
    "ember_z_l0_l1_l2": [round(z_l0, 2), round(z_l1, 2), round(z_l2, 2)],
    "ember_l0_max_emissive": round(em_l0_max, 4),
    "remaining_visible_chunks_after_abyss": len(active_chunks),
    "tiers_l2": [
        int(rig.get_active_hyd_upgrade_tier_for_test()),
        int(scale_actor.get_active_tare_upgrade_tier_for_test()),
        int(rig.get_active_scan_upgrade_tier_for_test()),
    ],
}))
"""


REMOTE_TOP_UP_TARE_FOR_SHOT_B_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]
hud_bridge = pawn.get_component_by_class(unreal.InspectorHudBridgeComponent)

# Collapse [H] controls card to "[H] CONTROLS" micro-pill for Screenshot B!
if bool(hud_bridge.is_controls_card_expanded()):
    pawn.toggle_controls_hud_for_test()

def find_safe_tare_topup_z(target_fill_fraction=0.82):
    snap_cur = shift_sub.get_current_snapshot()
    max_allow = float(snap_cur.get_editor_property("max_allowed_tare_waste_kg"))
    acc_waste = float(snap_cur.get_editor_property("accumulated_tare_waste_kg"))
    min_win_cur = float(snap_cur.get_editor_property("target_tithe_mass_kg")) - float(snap_cur.get_editor_property("quota_tolerance_kg"))
    rem_allow_cur = max_allow - acc_waste
    want_add = max(1.5, target_fill_fraction * max_allow - acc_waste)
    tb_cur = rig.get_target_world_bounds()
    cm_cur = pawn.get_cached_cuttable_mask_for_test()
    cut_top_cur = float(cm_cur.get_editor_property("center_world").z + cm_cur.get_editor_property("half_extents_cm").z)
    bz = None
    berr = 999.0
    z_cur = float(tb_cur.min.z) + 1.5
    while z_cur <= cut_top_cur - 2.0:
        pawn.set_laser_plane_pose_for_test(z_cur, 0.0, 0.0, 0.0)
        tel_cur = pawn.get_cached_preview_telemetry_for_test()
        if (
            bool(tel_cur.get_editor_property("intersects_monolith"))
            and bool(tel_cur.get_editor_property("has_valid_mass_estimate"))
            and not bool(tel_cur.get_editor_property("will_detonate"))
        ):
            est_cur = float(tel_cur.get_editor_property("estimated_severed_mass_kg"))
            rem_cur = float(tel_cur.get_editor_property("estimated_remainder_mass_kg"))
            if 1.0 <= est_cur <= rem_allow_cur - 0.8 and rem_cur >= min_win_cur + 0.8:
                err_cur = abs(est_cur - want_add)
                if err_cur < berr:
                    berr = err_cur
                    bz = z_cur
        z_cur += 0.35
    return bz

best_z = find_safe_tare_topup_z(0.84)
if best_z is None:
    # Switch to high-mass diverter contract (DA_Contract_05) while preserving L2 upgrades for a vivid ~84% molten Tare fill
    shift_sub.start_shift_from_contract(4)
    snap_new = shift_sub.get_current_snapshot()
    if snap_new.get_editor_property("active_diverter_route") != unreal.DiverterRoute.TARE_CHUTE:
        shift_sub.request_toggle_diverter()
    best_z = find_safe_tare_topup_z(0.84)

if best_z is not None:
    tb = rig.get_target_world_bounds()
    cx = 0.5 * (float(tb.min.x) + float(tb.max.x))
    cy = 0.5 * (float(tb.min.y) + float(tb.max.y))
    ap = unreal.LaserSliceAperture()
    ap.set_editor_property("enforce_finite_sheet_bounds", False)
    rig.commit_slice_plane(unreal.Vector(cx, cy, best_z), unreal.Vector(0.0, 0.0, -1.0), ap)

# Park laser cleanly in safe mid-cut so shot B is crisp
tb2 = rig.get_target_world_bounds()
pawn.set_laser_plane_pose_for_test(float(tb2.min.z) + 10.0, 0.0, 0.0, 0.0)
unreal.log("SHOT_B_PREP_JSON=" + json.dumps({"micro_pill_collapsed": not bool(hud_bridge.is_controls_card_expanded()), "topup_z": best_z}))
"""


REMOTE_VERIFY_RESET_TO_L0_AND_DIRECTOR_ROI_SNIPPET = """
import json
import math
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]
scale_actor = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "ScaleAssemblyActor" in a.get_class().get_name()][0]
booth = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "DirectorBoothActor" in a.get_class().get_name()][0]
hud_bridge = pawn.get_component_by_class(unreal.InspectorHudBridgeComponent)

# Restore expanded controls card and execute Career Reset -> verify L0 hidden again!
if not bool(hud_bridge.is_controls_card_expanded()):
    pawn.toggle_controls_hud_for_test()
shift_sub.execute_career_reset_to_shift_one()

all_upg = [
    rig.get_editor_property("hyd_upgrade_cylinder_left_l1"),
    rig.get_editor_property("hyd_upgrade_cylinder_right_l1"),
    rig.get_editor_property("hyd_upgrade_accumulator_left_l2"),
    rig.get_editor_property("hyd_upgrade_accumulator_right_l2"),
    rig.get_editor_property("scan_upgrade_pod_left_l1"),
    rig.get_editor_property("scan_upgrade_pod_right_l1"),
    rig.get_editor_property("scan_upgrade_lens_left_l1"),
    rig.get_editor_property("scan_upgrade_lens_right_l1"),
    rig.get_editor_property("scan_upgrade_array_left_l2"),
    rig.get_editor_property("scan_upgrade_array_right_l2"),
    rig.get_editor_property("scan_upgrade_emitter_left_l2"),
    rig.get_editor_property("scan_upgrade_emitter_right_l2"),
    scale_actor.get_editor_property("tare_upgrade_collar_front_l1"),
    scale_actor.get_editor_property("tare_upgrade_collar_outer_l1"),
    scale_actor.get_editor_property("tare_upgrade_heat_shield_front_l2"),
    scale_actor.get_editor_property("tare_upgrade_heat_shield_corner_l2"),
]
if any(bool(c.is_visible()) for c in all_upg):
    raise AssertionError("CareerReset failed to hide all 16 L1/L2 upgrade components back to L0!")

# Project Right-Bay Panopticon UV box for Screenshot C crop & ROI contrast check
cam = pawn.get_component_by_class(unreal.CameraComponent)
cam_loc = cam.get_world_location()
cam_rot = cam.get_world_rotation()
fwd = unreal.MathLibrary.get_forward_vector(cam_rot)
right = unreal.MathLibrary.get_right_vector(cam_rot)
up = unreal.MathLibrary.get_up_vector(cam_rot)
fov_h = float(cam.get_editor_property("field_of_view"))
tan_h = math.tan(math.radians(0.5 * fov_h))
tan_v = tan_h * (9.0 / 16.0)

def proj_uv(wp):
    rx, ry, rz = float(wp.x - cam_loc.x), float(wp.y - cam_loc.y), float(wp.z - cam_loc.z)
    d = rx * float(fwd.x) + ry * float(fwd.y) + rz * float(fwd.z)
    if d <= 1.0:
        return None
    cx = rx * float(right.x) + ry * float(right.y) + rz * float(right.z)
    cy = rx * float(up.x) + ry * float(up.y) + rz * float(up.z)
    return (0.5 + 0.5 * (cx / (d * tan_h)), 0.5 - 0.5 * (cy / (d * tan_v)))

booth_xf = booth.get_actor_transform()
def box_uv(min_l, max_l):
    us, vs = [], []
    for x in (min_l[0], max_l[0]):
        for y in (min_l[1], max_l[1]):
            for z in (min_l[2], max_l[2]):
                uv = proj_uv(unreal.MathLibrary.transform_location(booth_xf, unreal.Vector(x, y, z)))
                if uv is not None:
                    us.append(uv[0])
                    vs.append(uv[1])
    return [round(min(us), 4), round(min(vs), 4), round(max(us), 4), round(max(vs), 4)]

crop_uv = box_uv([-4.0, 126.0, -15.0], [9.0, 232.0, 24.0])
dir_uv = box_uv([2.0, 158.0, -11.0], [8.5, 194.0, 17.5])
glass_uv = box_uv([4.0, 215.0, -11.0], [8.8, 245.0, 15.5])

unreal.log("RESET_AND_UV_JSON=" + json.dumps({
    "reset_all_hidden": True,
    "crop_uv": crop_uv,
    "dir_uv": dir_uv,
    "glass_uv": glass_uv,
}))
"""


def make_composited_screenshot_snippet(png_path: str) -> str:
    norm_path = png_path.replace("\\", "/")
    return f"""
import unreal

out_path = {norm_path!r}
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
hud_bridge = pawn.get_component_by_class(unreal.InspectorHudBridgeComponent)
if not bool(hud_bridge.request_composited_viewport_screenshot(out_path)):
    raise RuntimeError("RequestCompositedViewportScreenshot returned False for " + out_path)
"""


def make_clean_lit_screenshot_snippet(png_path: str) -> str:
    norm_path = png_path.replace("\\", "/")
    return f"""
import unreal

out_path = {norm_path!r}
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
hud_bridge = pawn.get_component_by_class(unreal.InspectorHudBridgeComponent)
for prop_name in (
    "laser_sheet_visual",
    "laser_sheet_underside_visual",
    "laser_frame_front_bar",
    "laser_frame_back_bar",
    "laser_frame_left_bar",
    "laser_frame_right_bar",
):
    comp = pawn.get_editor_property(prop_name)
    if comp is not None:
        comp.set_hidden_in_game(True, False)
if not bool(hud_bridge.request_clean_lit_viewport_screenshot(out_path)):
    raise RuntimeError("RequestCleanLitViewportScreenshot returned False for " + out_path)
"""


REMOTE_RESTORE_LASER_OVERLAY_SNIPPET = """
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
for prop_name in (
    "laser_sheet_visual",
    "laser_sheet_underside_visual",
    "laser_frame_front_bar",
    "laser_frame_back_bar",
    "laser_frame_left_bar",
    "laser_frame_right_bar",
):
    comp = pawn.get_editor_property(prop_name)
    if comp is not None:
        comp.set_hidden_in_game(False, False)
"""


REMOTE_MEASURE_STEADY_FRAME_TIME_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
for cmd in ("r.VSync 0", "r.VSyncEditor 0", "Slate.MaxFPS 240", "t.MaxFPS 240"):
    unreal.SystemLibrary.execute_console_command(pie_world, cmd)
delta_ms = float(unreal.GameplayStatics.get_world_delta_seconds(pie_world)) * 1000.0
unreal.log("FRAME_MS_JSON=" + json.dumps({"delta_ms": round(delta_ms, 3)}))
"""


REMOTE_END_PIE_SNIPPET = """
import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les.is_in_play_in_editor():
    les.editor_request_end_play()
"""


REMOTE_QUIT_EDITOR_SNIPPET = """
import unreal
unreal.SystemLibrary.quit_editor()
"""


def decode_png_rgb8(png_path: str) -> Tuple[int, int, List[Tuple[int, int, int]]]:
    with open(png_path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{png_path} is not a valid PNG file")

    pos = 8
    width = height = bit_depth = color_type = 0
    idat_chunks: List[bytes] = []
    while pos < len(data):
        length = struct.unpack(">I", data[pos : pos + 4])[0]
        chunk_type = data[pos + 4 : pos + 8]
        chunk_data = data[pos + 8 : pos + 8 + length]
        pos += 12 + length
        if chunk_type == b"IHDR":
            width, height, bit_depth, color_type, _, _, _ = struct.unpack(">IIBBBBB", chunk_data)
        elif chunk_type == b"IDAT":
            idat_chunks.append(chunk_data)
        elif chunk_type == b"IEND":
            break

    if bit_depth != 8 or color_type not in (2, 6):
        raise ValueError(f"Unsupported PNG format in {png_path}: bit_depth={bit_depth}, color_type={color_type}")

    bpp = 3 if color_type == 2 else 4
    stride = width * bpp
    raw = zlib.decompress(b"".join(idat_chunks))

    recon = bytearray(height * stride)
    in_pos = 0
    for y in range(height):
        filt = raw[in_pos]
        in_pos += 1
        row_out = y * stride
        prev_out = (y - 1) * stride
        if filt == 0:
            recon[row_out : row_out + stride] = raw[in_pos : in_pos + stride]
        elif filt == 1:
            for i in range(stride):
                a = recon[row_out + i - bpp] if i >= bpp else 0
                recon[row_out + i] = (raw[in_pos + i] + a) & 0xFF
        elif filt == 2:
            for i in range(stride):
                b = recon[prev_out + i] if y > 0 else 0
                recon[row_out + i] = (raw[in_pos + i] + b) & 0xFF
        elif filt == 3:
            for i in range(stride):
                a = recon[row_out + i - bpp] if i >= bpp else 0
                b = recon[prev_out + i] if y > 0 else 0
                recon[row_out + i] = (raw[in_pos + i] + ((a + b) >> 1)) & 0xFF
        elif filt == 4:
            for i in range(stride):
                a = recon[row_out + i - bpp] if i >= bpp else 0
                b = recon[prev_out + i] if y > 0 else 0
                c = recon[prev_out + i - bpp] if (y > 0 and i >= bpp) else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                recon[row_out + i] = (raw[in_pos + i] + pr) & 0xFF
        else:
            raise ValueError(f"Unsupported PNG filter {filt}")
        in_pos += stride

    pixels: List[Tuple[int, int, int]] = []
    for y in range(height):
        base = y * stride
        for x in range(width):
            idx = base + x * bpp
            pixels.append((recon[idx], recon[idx + 1], recon[idx + 2]))
    return width, height, pixels


def encode_png_rgb8(
    out_path: str,
    src_w: int,
    src_pixels: List[Tuple[int, int, int]],
    x0: int,
    y0: int,
    x1: int,
    y1: int,
    upscale: int = 3,
) -> Tuple[int, int]:
    cw = max(1, x1 - x0)
    ch = max(1, y1 - y0)
    dst_w = cw * upscale
    dst_h = ch * upscale

    raw_rows = bytearray()
    for dy in range(dst_h):
        sy = y0 + (dy // upscale)
        row_base = sy * src_w
        raw_rows.append(0)
        for dx in range(dst_w):
            sx = x0 + (dx // upscale)
            r, g, b = src_pixels[row_base + sx]
            raw_rows.extend((r, g, b))

    def make_chunk(ctype: bytes, payload: bytes) -> bytes:
        crc = zlib.crc32(ctype + payload) & 0xFFFFFFFF
        return struct.pack(">I", len(payload)) + ctype + payload + struct.pack(">I", crc)

    ihdr = struct.pack(">IIBBBBB", dst_w, dst_h, 8, 2, 0, 0, 0)
    idat = zlib.compress(bytes(raw_rows), 9)
    png_bytes = b"\x89PNG\r\n\x1a\n" + make_chunk(b"IHDR", ihdr) + make_chunk(b"IDAT", idat) + make_chunk(b"IEND", b"")
    with open(out_path, "wb") as f:
        f.write(png_bytes)
    return dst_w, dst_h


def roi_mean_luma(w: int, pixels: List[Tuple[int, int, int]], x0: int, y0: int, x1: int, y1: int) -> float:
    tot = 0.0
    cnt = 0
    for y in range(max(0, y0), min(len(pixels) // w, y1)):
        base = y * w
        for x in range(max(0, x0), min(w, x1)):
            r, g, b = pixels[base + x]
            tot += 0.2126 * r + 0.7152 * g + 0.0722 * b
            cnt += 1
    return tot / max(1, cnt)


def run_remote_checked(
    remote: remote_execution.RemoteExecution,
    code: str,
    label: str,
    verbose: bool = True,
) -> List[str]:
    if verbose:
        print(f"[VerifyPhase12Fix4] >>> {label}")
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


def wait_for_file(path: str, min_bytes: int = 65536, timeout_sec: float = 8.0) -> None:
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        if os.path.exists(path) and os.path.getsize(path) >= min_bytes:
            return
        time.sleep(0.15)
    raise RuntimeError(f"Timed out waiting for screenshot file (>={min_bytes} B): {path}")


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


def assert_controls_card_layout(hud: Dict[str, Any], label: str) -> None:
    if hud["overlaps"]:
        raise AssertionError(f"{label}: HUD widget overlap detected: {hud['overlaps']}")
    if float(hud["cc_w"]) > 245.0:
        raise AssertionError(f"{label}: SLeftControlsCard width {hud['cc_w']} px > 245.0 px!")
    if float(hud["cc_u_right"]) > 0.1355:
        raise AssertionError(f"{label}: SLeftControlsCard U_right {hud['cc_u_right']} > 0.135!")
    if float(hud["cc_box"][0][1]) < float(hud["tl_box"][1][1]) + 3.0:
        raise AssertionError(f"{label}: ControlsCard not cleanly stacked below TopLeftPanel! {hud}")
    if float(hud["analog_bar_top_local_y"]) <= float(hud["mass_bottom_local_y"]) + 1.0:
        raise AssertionError(
            f"{label}: Bottom CRT AnalogBar overlaps MassReadoutBlock! "
            f"mass_bot={hud['mass_bottom_local_y']}, bar_top={hud['analog_bar_top_local_y']}"
        )
    if not hud["h_bound"] or not hud["game_only_cursor_hidden"]:
        raise AssertionError(f"{label}: [H] binding or FInputModeGameOnly cursor state invalid: {hud}")


def verify_git_binary_locks() -> Dict[str, Any]:
    umap_diff = subprocess.check_output(
        ["git", "diff", "--name-only", "Content/Maps/L_MainGameplay.umap"], cwd=REPO_ROOT, text=True
    ).strip()
    da_diff = subprocess.check_output(
        ["git", "diff", "--name-only", "Content/Presentation/DA_Presentation.uasset"], cwd=REPO_ROOT, text=True
    ).strip()
    status_lines = subprocess.check_output(["git", "status", "--porcelain"], cwd=REPO_ROOT, text=True).splitlines()
    uasset_touched = [ln for ln in status_lines if ln.lower().endswith((".uasset", ".umap"))]
    if umap_diff or da_diff or uasset_touched:
        raise AssertionError(
            f"Binary/Asset lock violated! umap_diff={umap_diff!r}, da_diff={da_diff!r}, uassets={uasset_touched}"
        )
    return {
        "umap_diff_bytes": 0,
        "da_presentation_diff_bytes": 0,
        "touched_or_untracked_uassets": len(uasset_touched),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Phase 12-FIX4 Live PIE 4-Gate Verification")
    parser.add_argument("--timeout", type=float, default=150.0)
    parser.add_argument("--shutdown", action="store_true", help="Cleanly quit UnrealEditor after verification")
    args = parser.parse_args()

    os.makedirs(OUT_DIR, exist_ok=True)
    for stale in (SHOT_A_OVERSHOOT, SHOT_B_UPGRADES, SHOT_C_DIRECTOR, CLEAN_DIRECTOR_PROBE, REPORT_JSON_PATH):
        if os.path.exists(stale):
            os.remove(stale)

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
        quick_deadline = time.time() + 2.5
        while time.time() < quick_deadline:
            for node in remote.remote_nodes:
                if "TareAndTithe" in str(node.get("project_name", "")) or len(remote.remote_nodes) == 1:
                    target_node = node
                    break
            if target_node is not None:
                break
            time.sleep(0.4)

        if target_node is None:
            print(f"[VerifyPhase12Fix4] Launching UnrealEditor: {UNREAL_EDITOR_EXE} {UPROJECT_PATH}")
            spawned_proc = subprocess.Popen(
                [UNREAL_EDITOR_EXE, UPROJECT_PATH, "/Game/Maps/L_MainGameplay", "-NoSplash", "-NoSound", "-Unattended"],
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

        scene_audit = None
        last_err = None
        for attempt in range(1, 16):
            time.sleep(0.85)
            try:
                lines = run_remote_checked(
                    remote,
                    REMOTE_WARMUP_AND_SCENE_AUDIT_SNIPPET,
                    f"Scene & Light Budget Warmup Audit (attempt {attempt})",
                )
                scene_audit = extract_json_tag(lines, "SCENE_AUDIT_JSON=")
                break
            except Exception as exc:
                last_err = exc
                print(f"[VerifyPhase12Fix4] Waiting for PIE warmup ({exc})...")

        if scene_audit is None:
            raise RuntimeError(f"Failed PIE warmup: {last_err}")

        focus_pie_viewport_window()
        time.sleep(0.95)

        # =====================================================================
        # GATE 1 (Part A): Contract 0 States + OvershootMagenta + Screenshot A!
        # =====================================================================
        gate1_c0 = extract_json_tag(
            run_remote_checked(remote, REMOTE_GATE1_CONTRACT0_STATES_SNIPPET, "Gate 1: Contract 0 6-State & OvershootMagenta"),
            "GATE1_C0_JSON=",
        )
        time.sleep(0.20)

        # Verify Gate 2 1080p layout while aimed at OvershootMagenta & capture Screenshot A!
        hud_1080p = extract_json_tag(
            run_remote_checked(remote, REMOTE_GATE2_INSPECT_HUD_SNIPPET, "Gate 2: Inspect 1080p (100% DPI) HUD Layout"),
            "GATE2_HUD_JSON=",
        )
        assert_controls_card_layout(hud_1080p, "1080p (100% DPI)")

        focus_pie_viewport_window()
        time.sleep(0.15)
        run_remote_checked(
            remote,
            make_composited_screenshot_snippet(SHOT_A_OVERSHOOT),
            "Capture Screenshot A: Phase12Fix4_OvershootMagenta_Controls_Hinge.png",
        )
        wait_for_file(SHOT_A_OVERSHOOT)

        # =====================================================================
        # GATE 2: 125% DPI (1536x864) Check + [H] Micro-Pill Toggle & Restore
        # =====================================================================
        run_remote_checked(
            remote,
            make_set_viewport_override_snippet(1536.0, 864.0, True),
            "Gate 2: Apply 1536x864 (125% DPI Equivalent) Viewport Override",
        )
        time.sleep(0.22)
        hud_125dpi = extract_json_tag(
            run_remote_checked(remote, REMOTE_GATE2_INSPECT_HUD_SNIPPET, "Gate 2: Inspect 1536x864 (125% DPI) Layout"),
            "GATE2_HUD_JSON=",
        )
        assert_controls_card_layout(hud_125dpi, "1536x864 (125% DPI)")
        run_remote_checked(
            remote,
            make_set_viewport_override_snippet(1920.0, 1080.0, False),
            "Gate 2: Restore Native 1080p Viewport",
        )
        time.sleep(0.18)

        # Toggle [H] -> collapsed micro-pill
        tog_collapse = extract_json_tag(
            run_remote_checked(remote, REMOTE_TOGGLE_CONTROLS_H_SNIPPET, "Gate 2: Press [H] -> Collapse to Micro-Pill"),
            "TOGGLE_H_JSON=",
        )
        time.sleep(0.15)
        hud_collapsed = extract_json_tag(
            run_remote_checked(remote, REMOTE_GATE2_INSPECT_HUD_SNIPPET, "Gate 2: Inspect Collapsed Micro-Pill Bounds"),
            "GATE2_HUD_JSON=",
        )
        assert_controls_card_layout(hud_collapsed, "Collapsed Micro-Pill")
        if tog_collapse["expanded"] or hud_collapsed["expanded"] or hud_collapsed["cc_h"] >= hud_1080p["cc_h"] * 0.45:
            raise AssertionError(f"Micro-pill failed to collapse cleanly: expanded={hud_1080p['cc_h']} -> {hud_collapsed['cc_h']}")

        # Toggle [H] -> expand back
        tog_expand = extract_json_tag(
            run_remote_checked(remote, REMOTE_TOGGLE_CONTROLS_H_SNIPPET, "Gate 2: Press [H] -> Expand Full Controls Card"),
            "TOGGLE_H_JSON=",
        )
        if not tog_expand["expanded"] or not tog_expand["game_only_cursor_hidden"]:
            raise AssertionError(f"[H] failed to re-expand cleanly: {tog_expand}")

        # =====================================================================
        # GATE 1 (Part B): Veined Contract P1..P5 + Correction 1 Priority Proof!
        # =====================================================================
        gate1_v1 = extract_json_tag(
            run_remote_checked(remote, REMOTE_GATE1_VEINED_PRIORITY_SNIPPET, "Gate 1: Veined P1/P2/P4/P5 + Harvest Clean Base"),
            "GATE1_VEIN_STAGE1_JSON=",
        )
        if gate1_v1.get("harvested_base"):
            time.sleep(1.70)
        gate1_v2a = extract_json_tag(
            run_remote_checked(remote, REMOTE_GATE1_VEINED_UPPER_SWEEP_SNIPPET, "Gate 1: Purge Vein 0 Down TareChute"),
            "GATE1_VEIN_STAGE2A_JSON=",
        )
        if gate1_v2a.get("purged_vein0"):
            time.sleep(1.90)
        gate1_v2 = extract_json_tag(
            run_remote_checked(remote, REMOTE_GATE1_VEINED_POST_PRELOAD_SNIPPET, "Gate 1: Correction 1 Near-Vein Quota/Overshoot & Fault Lockout"),
            "GATE1_VEIN_STAGE2_JSON=",
        )
        em_near_vein = gate1_v1["emerald_over_prox"] or gate1_v2a["emerald_over_prox"] or gate1_v2["emerald_over_prox"]
        mg_near_vein = gate1_v1["magenta_over_prox"] or gate1_v2a["magenta_over_prox"] or gate1_v2["magenta_over_prox"]
        if em_near_vein is None or mg_near_vein is None:
            raise AssertionError(
                f"Correction 1 priority proof incomplete: emerald_over_prox={em_near_vein}, magenta_over_prox={mg_near_vein}"
            )

        # Wait out fault reset delay cleanly before Gate 3
        time.sleep(2.40)

        # =====================================================================
        # GATE 3: Stationary Hinge, Director Silhouette, Tare Ember & Upgrades
        # =====================================================================
        gate3_pre = extract_json_tag(
            run_remote_checked(remote, REMOTE_GATE3_PRE_SWING_SNIPPET, "Gate 3: Record Hinge Transforms & Trigger [SPACE] Flapper Swing"),
            "GATE3_PRE_SWING_JSON=",
        )
        time.sleep(0.65)
        gate3_static = extract_json_tag(
            run_remote_checked(remote, make_gate3_post_swing_and_director_snippet(gate3_pre), "Gate 3: Stationary Hinge, OBB Non-Overlap & 16-Part Director"),
            "GATE3_STATIC_JSON=",
        )

        # Earn >= 15 Scrip via real precision Contract 0 cuts + stamps (also verifying pan depression vs stationary hinge!)
        max_pan_depression_cm = 0.0
        max_axle_drift_cm = 0.0
        current_scrip = 0
        rounds = 0
        while current_scrip < 15 and rounds < 10:
            rounds += 1
            sl_info = extract_json_tag(
                run_remote_checked(
                    remote,
                    REMOTE_START_C0_AND_SLICE_DIRECTOR_CUT_SNIPPET,
                    f"Gate 3: Precision Slice Round #{rounds} (scrip={current_scrip}/15)",
                    verbose=(rounds == 1),
                ),
                "C0_SLICE_JSON=",
            )
            stamped = False
            poll_deadline = time.time() + 4.0
            while time.time() < poll_deadline:
                time.sleep(0.30)
                poll = extract_json_tag(
                    run_remote_checked(remote, REMOTE_POLL_SETTLED_AND_STAMP_SNIPPET, "Poll Pan Settle & Stamp", verbose=False),
                    "STAMP_POLL_JSON=",
                )
                max_pan_depression_cm = max(max_pan_depression_cm, abs(float(poll["pan_z1"]) - float(sl_info["pan_z0"])))
                max_axle_drift_cm = max(max_axle_drift_cm, abs(float(poll["axle_z1"]) - float(sl_info["axle_z0"])))
                if poll["stamped"]:
                    current_scrip = int(poll["scrip"])
                    stamped = True
                    break
            if not stamped:
                raise RuntimeError(f"Round #{rounds} chunk did not settle/stamp within 4.0s!")

        if max_pan_depression_cm < 0.05:
            raise AssertionError(f"Expected measurable PanMotionRoot depression under Tithe load, got {max_pan_depression_cm}")
        if max_axle_drift_cm > 1.0e-4:
            raise AssertionError(f"Stationary hinge axle moved during pan depression! drift={max_axle_drift_cm}")

        # Route a real chunk down TareChute and let it plunge through TareHopperFurnaceGlowCard into abyss despawn!
        run_remote_checked(remote, REMOTE_ROUTE_TARE_WASTE_SNIPPET, "Gate 3: Route Real Chunk Down TareChute")
        time.sleep(1.85)

        gate3_upgrades = extract_json_tag(
            run_remote_checked(
                remote,
                REMOTE_VERIFY_EMBER_AND_BUY_UPGRADES_SNIPPET,
                "Gate 3: Verify Rising Tare Ember & Purchase L1 -> L2 Across [1]/[2]/[3]",
            ),
            "GATE3_UPGRADES_JSON=",
        )
        if gate3_upgrades["remaining_visible_chunks_after_abyss"] != 0:
            raise AssertionError(f"Tare chunk did not despawn in abyss underneath ember card: {gate3_upgrades}")

        # Prepare & capture Screenshot B ([H] collapsed micro-pill + L2 upgraded rig/hopper + glowing Tare chute fill)!
        run_remote_checked(remote, REMOTE_TOP_UP_TARE_FOR_SHOT_B_SNIPPET, "Gate 3: Collapse [H] Micro-Pill & Top Up Tare Glow for Shot B")
        time.sleep(1.65)
        focus_pie_viewport_window()
        run_remote_checked(
            remote,
            make_composited_screenshot_snippet(SHOT_B_UPGRADES),
            "Capture Screenshot B: Phase12Fix4_MicroPill_L2Upgrades_TareGlow.png",
        )
        wait_for_file(SHOT_B_UPGRADES)

        # Career Reset -> verify all 16 upgrade meshes hide at L0 + compute Right-Bay UV box for Screenshot C
        reset_and_uv = extract_json_tag(
            run_remote_checked(
                remote,
                REMOTE_VERIFY_RESET_TO_L0_AND_DIRECTOR_ROI_SNIPPET,
                "Gate 3: Verify CareerReset Returns L0 Hidden & Project Right-Bay Director UV",
            ),
            "RESET_AND_UV_JSON=",
        )
        time.sleep(0.25)

        # Capture Clean 1080p Lit Probe -> crop close-up Screenshot C of Humanoid Director silhouette!
        focus_pie_viewport_window()
        run_remote_checked(
            remote,
            make_clean_lit_screenshot_snippet(CLEAN_DIRECTOR_PROBE),
            "Gate 3: Capture Clean 1080p Lit Probe for Right-Bay Director Crop",
        )
        wait_for_file(CLEAN_DIRECTOR_PROBE)
        run_remote_checked(remote, REMOTE_RESTORE_LASER_OVERLAY_SNIPPET, "Gate 3: Restore Laser Overlay Visibility")

        # =====================================================================
        # GATE 4: Steady PIE Frame Time (<= 13.0 ms) & Binary/Asset Locks
        # =====================================================================
        focus_pie_viewport_window()
        time.sleep(0.55)
        steady_samples_ms: List[float] = []
        for s_idx in range(7):
            time.sleep(0.18)
            f_lines = run_remote_checked(
                remote,
                REMOTE_MEASURE_STEADY_FRAME_TIME_SNIPPET,
                f"Gate 4: Sample Unthrottled PIE Frame Time #{s_idx + 1}",
                verbose=False,
            )
            steady_samples_ms.append(float(extract_json_tag(f_lines, "FRAME_MS_JSON=")["delta_ms"]))
        steady_median_ms = round(statistics.median(steady_samples_ms), 3)
        if steady_median_ms > 13.0:
            raise AssertionError(f"Gate 4 frame budget exceeded: median={steady_median_ms} ms > 13.0 ms (samples={steady_samples_ms})")

        run_remote_checked(remote, REMOTE_END_PIE_SNIPPET, "End PIE Session Cleanly")
        time.sleep(0.50)

        # Crop Screenshot C from CLEAN_DIRECTOR_PROBE and verify Director vs Right-Bay frosted glass contrast!
        pw, ph, ppx = decode_png_rgb8(CLEAN_DIRECTOR_PROBE)
        if os.path.exists(CLEAN_DIRECTOR_PROBE):
            os.remove(CLEAN_DIRECTOR_PROBE)

        cuv = reset_and_uv["crop_uv"]
        cx0 = max(0, int(cuv[0] * pw) - 18)
        cy0 = max(0, int(cuv[1] * ph) - 18)
        cx1 = min(pw, int(cuv[2] * pw) + 18)
        cy1 = min(ph, int(cuv[3] * ph) + 18)
        crop_w, crop_h = encode_png_rgb8(SHOT_C_DIRECTOR, pw, ppx, cx0, cy0, cx1, cy1, upscale=3)

        duv = reset_and_uv["dir_uv"]
        guv = reset_and_uv["glass_uv"]
        dir_luma = roi_mean_luma(pw, ppx, int(duv[0] * pw), int(duv[1] * ph), int(duv[2] * pw), int(duv[3] * ph))
        glass_luma = roi_mean_luma(pw, ppx, int(guv[0] * pw), int(guv[1] * ph), int(guv[2] * pw), int(guv[3] * ph))
        if glass_luma <= dir_luma + 1.2:
            raise AssertionError(
                f"Right-Bay unobstructed frosted glass ROI failed contrast vs Director ROI: glass={glass_luma:.2f}, dir={dir_luma:.2f}"
            )

        shot_summary = {}
        for sp in (SHOT_A_OVERSHOOT, SHOT_B_UPGRADES, SHOT_C_DIRECTOR):
            if not os.path.exists(sp):
                raise AssertionError(f"Missing required output screenshot: {sp}")
            sw, sh, _ = decode_png_rgb8(sp)
            shot_summary[os.path.basename(sp)] = {"dims": f"{sw}x{sh}", "bytes": os.path.getsize(sp)}

        git_locks = verify_git_binary_locks()

        report = {
            "status": "PASS",
            "gate1_laser_reticle_6state": {
                "contract0": gate1_c0,
                "veined_stage1": gate1_v1,
                "veined_stage2": gate1_v2,
                "correction1_emerald_over_prox": em_near_vein,
                "correction1_magenta_over_prox": mg_near_vein,
            },
            "gate2_hud_and_h_toggle": {
                "hud_1080p_100dpi": hud_1080p,
                "hud_1536x864_125dpi": hud_125dpi,
                "hud_collapsed_micropill": hud_collapsed,
            },
            "gate3_hardware_ember_director": {
                "static_audit": gate3_static,
                "pan_depression_cm": round(max_pan_depression_cm, 4),
                "hinge_drift_during_pan_depression_cm": round(max_axle_drift_cm, 6),
                "upgrades_and_ember": gate3_upgrades,
                "director_roi_luma": round(dir_luma, 3),
                "right_glass_roi_luma": round(glass_luma, 3),
                "closeup_crop_dims": f"{crop_w}x{crop_h}",
                "screenshots": shot_summary,
            },
            "gate4_perf_and_binary_locks": {
                "steady_median_ms": steady_median_ms,
                "steady_samples_ms": steady_samples_ms,
                "scene_audit": scene_audit,
                "git_locks": git_locks,
            },
        }
        with open(REPORT_JSON_PATH, "w", encoding="utf-8") as rf:
            json.dump(report, rf, indent=2)

        print("\n================ PHASE 12-FIX4 4-GATE VERIFICATION SUMMARY ================")
        print(f"  [Gate 1] 6-State Parity & OvershootMagenta: PASS (max_ch={gate1_c0['overshoot_tithe']['max_emissive_ch']} < 1.65, Correction1 Emerald/Magenta over prox=PASS)")
        print(f"  [Gate 2] Left Controls Card & [H] Toggle: PASS (1080p w={hud_1080p['cc_w']}px U={hud_1080p['cc_u_right']}, 125%DPI w={hud_125dpi['cc_w']}px U={hud_125dpi['cc_u_right']}, pill_h={hud_collapsed['cc_h']}px, overlaps=0)")
        print(f"  [Gate 3] Hinge, Tare Ember, [1-3] Upgrades & 16-Part Director: PASS (hinge_delta=0.000, ember_z={gate3_upgrades['ember_z_l0_l1_l2']}, director_luma={dir_luma:.2f} vs glass={glass_luma:.2f})")
        print(f"  [Gate 4] Perf & Binary Locks: PASS (steady_median={steady_median_ms} ms <= 13.0 ms, shadow_lights=1, 0 uasset/umap diff)")
        print(f"  [Artifacts] {shot_summary}")
        print("===========================================================================\n")
        return 0

    finally:
        if os.path.exists(CLEAN_DIRECTOR_PROBE):
            try:
                os.remove(CLEAN_DIRECTOR_PROBE)
            except Exception:
                pass
        if args.shutdown or spawned_proc is not None:
            print("[VerifyPhase12Fix4] Sending clean quit_editor() command...")
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
                spawned_proc.wait(timeout=25.0)
            except Exception:
                spawned_proc.kill()


if __name__ == "__main__":
    sys.exit(main())
