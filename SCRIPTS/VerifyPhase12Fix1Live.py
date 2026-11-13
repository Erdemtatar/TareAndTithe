"""
Phase 12-FIX1 Live PIE 4-Gate Verification & Composited Screenshot Harness.

Launches UnrealEditor on /Game/Maps/L_MainGameplay, enters PIE, and verifies:
  - Gate 1 (Airborne Scale Depression Fix): While an InFlight chunk overlaps PanTriggerBox ~60+ cm above the pan,
    SettledScaleMassKg == 0.00 kg, InFlightMassKg > 10 kg ([E] locked), and PanMotionRoot Z delta == 0.00 mm.
    On rigid-body impact with PanDeckCollider, chunk promotes to OnScalePan, PanMotionRoot depresses, and mass settles.
  - Gate 2 (Multi-Chunk Tall Stack Regression): 3 consecutive horizontal slabs stacked pancake-style on TitheScale
    all promote to OnScalePan via IMassPayloadInterface rigid-body chain propagation with 0 stray faults.
  - Gate 3 (Physical Tare Ramp Slide & Abyss Plunge): On DA_Contract_02 (CantileveredJHook) routed to TareChute,
    chunk remains visible & active at t_hit + 0.08s and t_hit + 0.20s while sliding into +Y, latches Tare mass strictly
    once, and only returns to UChunkPoolSubsystem after plunging deep into the open 84 x 76 cm shaft (World Z <= -42 cm).
  - Gate 4 (6-Contract Asymmetric Variety, Strain, Zero Floating Islands & Budget Hygiene):
    Verifies all 6 contracts in PIE (0 duplicate procedural shapes, Ctr 01 strain == 0.0, Ctr 02..06 strain in [0.45, 0.75],
    single-island horizontal slices on CantileveredJHook / OffsetCChannel / CounterweightedZBracket, visible collar Z <= +16 cm
    with Rodin PBR, invisible overshoot guide walls, shadow_lights == 1, zero_shadow_lights == 10, VolumetricFog == False,
    and unthrottled PIE frame time <= 13.0 ms).

Saves 3 composited (bShowUI = true) screenshots in Saved/Screenshots/Phase12Fix1/:
  1. Phase12Fix1_Ctr02_CantileveredJHook_TareShaft.png
  2. Phase12Fix1_Ctr03_OffsetCChannel.png
  3. Phase12Fix1_Ctr05_CounterweightedZBracket.png
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
SCREENSHOTS_DIR = os.path.join(REPO_ROOT, "Saved", "Screenshots", "Phase12Fix1")

SHOT_CTR02_TARE = os.path.join(SCREENSHOTS_DIR, "Phase12Fix1_Ctr02_CantileveredJHook_TareShaft.png")
SHOT_CTR03_CCHAN = os.path.join(SCREENSHOTS_DIR, "Phase12Fix1_Ctr03_OffsetCChannel.png")
SHOT_CTR05_ZBRK = os.path.join(SCREENSHOTS_DIR, "Phase12Fix1_Ctr05_CounterweightedZBracket.png")


REMOTE_BEGIN_PIE_SNIPPET = """
import unreal

for cmd in ("r.VSync 0", "r.VSyncEditor 0", "Slate.MaxFPS 240", "t.MaxFPS 240", "t.UnfocusedFrameRateLimit 0"):
    unreal.SystemLibrary.execute_console_command(None, cmd)

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les.is_in_play_in_editor():
    les.editor_request_end_play()
les.editor_request_begin_play()
unreal.log("[VerifyPhase12Fix1] Requested BeginPlay PIE session.")
"""


REMOTE_AUDIT_SCENE_AND_HOPPER_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
if pie_world is None:
    raise RuntimeError("PIE GameWorld is None; PIE not ready yet.")

world_time_sec = float(unreal.GameplayStatics.get_time_seconds(pie_world))
if world_time_sec < 1.0:
    raise RuntimeError(f"PIE warmup in progress (world_time={world_time_sec:.3f}s < 1.00s)...")

for cmd in ("r.VSync 0", "r.VSyncEditor 0", "Slate.MaxFPS 240", "t.MaxFPS 240"):
    unreal.SystemLibrary.execute_console_command(pie_world, cmd)

all_actors = unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor)
shadow_lights = []
zero_shadow_lights = []
volumetric_fog_enabled = False
scale_actors = []
rig_actors = []
pawns = []

for actor in all_actors:
    cls_name = actor.get_class().get_name()
    if "ScaleAssemblyActor" in cls_name:
        scale_actors.append(actor)
    elif "MonolithRigActor" in cls_name:
        rig_actors.append(actor)
    elif "InspectorPawn" in cls_name:
        pawns.append(actor)

    for lc in actor.get_components_by_class(unreal.LightComponentBase):
        entry = f"{cls_name}.{lc.get_name()}"
        if bool(lc.get_editor_property("cast_shadows")):
            shadow_lights.append(entry)
        else:
            zero_shadow_lights.append(entry)

    for fog in actor.get_components_by_class(unreal.ExponentialHeightFogComponent):
        if bool(fog.get_editor_property("enable_volumetric_fog")):
            volumetric_fog_enabled = True

if len(scale_actors) != 1 or len(rig_actors) != 1 or len(pawns) != 1:
    raise AssertionError(f"Expected 1 Scale, 1 Rig, 1 Pawn; found {len(scale_actors)}, {len(rig_actors)}, {len(pawns)}")
if len(shadow_lights) != 1:
    raise AssertionError(f"Expected shadow_lights == 1, got {len(shadow_lights)}: {shadow_lights}")
if len(zero_shadow_lights) != 10:
    raise AssertionError(f"Expected zero_shadow_lights == 10 (0 new lights), got {len(zero_shadow_lights)}: {zero_shadow_lights}")
if volumetric_fog_enabled:
    raise AssertionError("VolumetricFog hard ban violated!")

scale = scale_actors[0]
scale_inv = scale.get_actor_transform().inverse()

# Verify TareChuteVisual sunk to deep shaft basin floor (ScaleRoot Z ~ -82 cm, World Z ~ -68 cm)
tare_vis = scale.get_editor_property("tare_chute_visual")
tare_vis_rel_z = float(tare_vis.get_editor_property("relative_location").z)
tare_vis_world_z = float(tare_vis.get_world_location().z)
if tare_vis_rel_z > -75.0 or tare_vis_world_z > -60.0:
    raise AssertionError(f"TareChuteVisual not sunk to deep shaft basin: rel_z={tare_vis_rel_z:.2f}, world_z={tare_vis_world_z:.2f}")

# Verify visible TareHopperCollarISM ScaleRoot-local max Z <= +16.0 cm and Rodin PBR material binding
collar_ism = scale.get_editor_property("tare_hopper_collar_ism")
collar_sm = collar_ism.get_editor_property("static_mesh")
collar_bb = collar_sm.get_bounding_box()
collar_max_local_z = -999.0
for idx in range(collar_ism.get_instance_count()):
    ixf = collar_ism.get_instance_transform(idx, True)
    for bx in (collar_bb.min.x, collar_bb.max.x):
        for by in (collar_bb.min.y, collar_bb.max.y):
            for bz in (collar_bb.min.z, collar_bb.max.z):
                wp = unreal.MathLibrary.transform_location(ixf, unreal.Vector(bx, by, bz))
                lp = unreal.MathLibrary.transform_location(scale_inv, wp)
                collar_max_local_z = max(collar_max_local_z, float(lp.z))

if collar_max_local_z > 16.05:
    raise AssertionError(f"Correction A violated: visible TareHopperCollarISM max ScaleRoot Z = {collar_max_local_z:.2f} cm > +16.0 cm!")

collar_mat = collar_ism.get_material(0)
collar_mat_name = "" if collar_mat is None else collar_mat.get_name()
collar_base_name = "" if (collar_mat is None or collar_mat.get_base_material() is None) else collar_mat.get_base_material().get_name()
if ("StationPBR" not in collar_base_name and "TareChute" not in collar_mat_name and "ScaleBase" not in collar_mat_name):
    raise AssertionError(f"Correction A violated: TareHopperCollarISM expected Rodin StationPBR material, got mat='{collar_mat_name}' base='{collar_base_name}'")

# Verify all 8 tall physics catch walls are strictly invisible in game
guide_props = (
    "tare_guide_wall_outer_right_box",
    "tare_guide_wall_front_box",
    "tare_guide_wall_back_box",
    "tare_guide_wall_inner_left_lower_box",
    "tare_guide_feed_apron_slope_box",
    "tare_guide_chamfer_outer_right_box",
    "tare_guide_chamfer_front_box",
    "tare_guide_chamfer_back_box",
)
for gp in guide_props:
    gbox = scale.get_editor_property(gp)
    if gbox is None:
        raise AssertionError(f"Missing physics guide wall: {gp}")
    if not bool(gbox.get_editor_property("hidden_in_game")) or gbox.is_visible():
        raise AssertionError(f"Correction A violated: physics guide wall {gp} must be invisible!")

# Verify bloom-safe unlit furnace glow baffle Linear HDR <= 1.25
glow_card = scale.get_editor_property("tare_hopper_furnace_glow_card")
emissive_col = scale.get_editor_property("tare_furnace_baffle_emissive_color")
max_hdr = max(float(emissive_col.r), float(emissive_col.g), float(emissive_col.b))
if max_hdr > 1.25 or bool(glow_card.get_editor_property("cast_shadow")):
    raise AssertionError(f"Furnace baffle HDR/shadow violation: max_hdr={max_hdr:.3f}")

unreal.log("AUDIT_JSON=" + json.dumps({
    "shadow_lights": len(shadow_lights),
    "zero_shadow_lights": len(zero_shadow_lights),
    "volumetric_fog": volumetric_fog_enabled,
    "tare_vis_rel_z_cm": round(tare_vis_rel_z, 2),
    "tare_vis_world_z_cm": round(tare_vis_world_z, 2),
    "collar_instances": int(collar_ism.get_instance_count()),
    "collar_max_local_z_cm": round(collar_max_local_z, 2),
    "collar_mat": collar_mat_name,
    "collar_base_mat": collar_base_name,
    "furnace_max_hdr": round(max_hdr, 3),
}))
"""


def make_start_contract_snippet(contract_index: int) -> str:
    return f"""
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
ok = bool(shift_sub.start_shift_from_contract({contract_index}))
if not ok:
    raise AssertionError("StartShiftFromContract({contract_index}) returned False!")
scale = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "ScaleAssemblyActor" in a.get_class().get_name()][0]
pan_root = scale.get_editor_property("pan_motion_root")
snap = shift_sub.get_current_snapshot()
unreal.log("START_JSON=" + json.dumps({{
    "contract_index": int(snap.get_editor_property("contract_index")),
    "pan_base_rel_z": round(float(pan_root.get_editor_property("relative_location").z), 4),
    "remainder_mass_kg": round(float(snap.get_editor_property("remainder_mass_kg")), 3),
}}))
"""


def make_commit_horizontal_slice_snippet(height_above_bottom_cm: float) -> str:
    return f"""
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]
bounds = rig.get_target_world_bounds()
cx = 0.5 * (float(bounds.min.x) + float(bounds.max.x))
cy = 0.5 * (float(bounds.min.y) + float(bounds.max.y))
cut_z = float(bounds.min.z) + {height_above_bottom_cm:.4f}

aperture = unreal.LaserSliceAperture()
aperture.set_editor_property("enforce_finite_sheet_bounds", False)
summary = rig.commit_slice_plane(unreal.Vector(cx, cy, cut_z), unreal.Vector(0.0, 0.0, -1.0), aperture)
outcome = summary.get_editor_property("outcome")
if outcome != unreal.CommitSliceOutcome.SUCCESS:
    raise AssertionError(f"Horizontal slice at Z={{cut_z:.2f}} failed with outcome={{outcome}}")

rem_dyn = rig.get_remainder_mesh_component().get_dynamic_mesh()
rem_islands = int(unreal.GeometryScript_MeshQueries.get_num_connected_components(rem_dyn))
rem_closed = bool(unreal.GeometryScript_MeshQueries.get_is_closed_mesh(rem_dyn))

unreal.log("SLICE_JSON=" + json.dumps({{
    "cut_z_world": round(cut_z, 2),
    "spawned_chunk_count": int(summary.get_editor_property("spawned_chunk_count")),
    "spawned_chunk_mass_kg": round(float(summary.get_editor_property("spawned_chunk_mass_kg")), 4),
    "new_remainder_mass_kg": round(float(summary.get_editor_property("new_remainder_mass_kg")), 4),
    "rem_islands": rem_islands,
    "rem_closed": rem_closed,
}}))
"""


REMOTE_SAMPLE_SCALE_AND_CHUNKS_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
scale = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "ScaleAssemblyActor" in a.get_class().get_name()][0]
pan_root = scale.get_editor_property("pan_motion_root")
pan_trigger = scale.get_editor_property("pan_trigger_box")
snap = shift_sub.get_current_snapshot()

def _get_snap_prop(s, *candidates):
    for c in candidates:
        try:
            return s.get_editor_property(c)
        except Exception:
            pass
    raise KeyError(f"Missing {candidates} in {s.export_text()}")

active_chunks = []
for actor in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.SeveredChunkActor):
    st = actor.get_routing_state()
    is_hidden = bool(actor.get_editor_property("hidden"))
    if st != unreal.ChunkRoutingState.POOLED_INACTIVE or not is_hidden:
        loc = actor.get_actor_location()
        active_chunks.append({
            "name": actor.get_name(),
            "state": st.name,
            "hidden": is_hidden,
            "mass_kg": round(float(actor.get_analytical_mass_kg()), 4),
            "x": round(float(loc.x), 2),
            "y": round(float(loc.y), 2),
            "z": round(float(loc.z), 2),
            "in_pan_trigger": bool(pan_trigger.is_overlapping_actor(actor)),
        })

unreal.log("SAMPLE_JSON=" + json.dumps({
    "world_time": round(float(unreal.GameplayStatics.get_time_seconds(pie_world)), 4),
    "phase": shift_sub.get_current_phase().name,
    "fault": shift_sub.get_last_fault_reason().name,
    "pan_rel_z": round(float(pan_root.get_editor_property("relative_location").z), 4),
    "settled_mass_kg": round(float(_get_snap_prop(snap, "settled_scale_mass_kg")), 4),
    "in_flight_mass_kg": round(float(_get_snap_prop(snap, "in_flight_mass_kg", "flight_mass_kg")), 4),
    "tare_waste_kg": round(float(_get_snap_prop(snap, "accumulated_tare_waste_kg")), 4),
    "stamp_ready": bool(_get_snap_prop(snap, "scale_settled_for_stamp", "b_scale_settled_for_stamp")),
    "receiver_settled": bool(scale.is_receiver_settled()),
    "route": snap.get_editor_property("active_diverter_route").name,
    "active_chunks": active_chunks,
}))
"""


REMOTE_TOGGLE_DIVERTER_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
ok = bool(shift_sub.request_toggle_diverter())
snap = shift_sub.get_current_snapshot()
unreal.log("TOGGLE_JSON=" + json.dumps({
    "toggled": ok,
    "route": snap.get_editor_property("active_diverter_route").name,
}))
"""


REMOTE_INSPECT_ACTIVE_CONTRACT_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]
clamp_solver = rig.get_clamp_torque_solver_component()
rem_comp = rig.get_remainder_mesh_component()
rem_dyn = rem_comp.get_dynamic_mesh()
snap = shift_sub.get_current_snapshot()
c_idx = int(snap.get_editor_property("contract_index"))
c_asset = unreal.load_object(None, f"/Game/Presentation/Contracts/DA_Contract_0{c_idx + 1}.DA_Contract_0{c_idx + 1}")
tilt_deg = clamp_solver.get_tilt_angles_degrees()
delta_ms = float(unreal.GameplayStatics.get_world_delta_seconds(pie_world)) * 1000.0

unreal.log("CONTRACT_JSON=" + json.dumps({
    "contract_index": c_idx,
    "contract_name": c_asset.get_name(),
    "base_shape": c_asset.get_editor_property("base_shape").name,
    "grip_offset_y_cm": round(float(c_asset.get_editor_property("clamp_grip_horizontal_offset_cm").y), 2),
    "vein_count": len(list(c_asset.get_editor_property("hidden_veins") or [])),
    "remainder_mass_kg": round(float(snap.get_editor_property("remainder_mass_kg")), 3),
    "solver_strain": round(float(clamp_solver.get_strain_normalized()), 4),
    "hud_strain": round(float(snap.get_editor_property("clamp_strain_normalized")), 4),
    "tilt_deg": [round(float(tilt_deg.x), 3), round(float(tilt_deg.y), 3)],
    "is_closed": bool(unreal.GeometryScript_MeshQueries.get_is_closed_mesh(rem_dyn)),
    "islands": int(unreal.GeometryScript_MeshQueries.get_num_connected_components(rem_dyn)),
    "delta_ms": round(delta_ms, 3),
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
if not hud_bridge.request_composited_viewport_screenshot(out_path):
    raise RuntimeError("RequestCompositedViewportScreenshot returned False for " + out_path)
unreal.log("[VerifyPhase12Fix1] Queued composited UI screenshot: " + out_path)
"""


REMOTE_END_PIE_SNIPPET = """
import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les.is_in_play_in_editor():
    les.editor_request_end_play()
unreal.log("[VerifyPhase12Fix1] Cleanly ended PIE session without saving maps/assets.")
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
        print(f"[VerifyPhase12Fix1] >>> {label}")
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


def wait_for_file(path: str, timeout_sec: float = 6.0) -> None:
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        if os.path.exists(path) and os.path.getsize(path) > 32768:
            return
        time.sleep(0.12)
    raise RuntimeError(f"Timed out waiting for screenshot file: {path}")


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


def main() -> int:
    parser = argparse.ArgumentParser(description="Phase 12-FIX1 Live PIE 4-Gate Verification")
    parser.add_argument("--timeout", type=float, default=120.0)
    args = parser.parse_args()

    os.makedirs(SCREENSHOTS_DIR, exist_ok=True)
    for stale in (SHOT_CTR02_TARE, SHOT_CTR03_CCHAN, SHOT_CTR05_ZBRK):
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
            print(f"[VerifyPhase12Fix1] Launching UnrealEditor: {UNREAL_EDITOR_EXE} {UPROJECT_PATH}")
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

        audit_data = None
        last_err = None
        for attempt in range(1, 15):
            time.sleep(0.9)
            try:
                lines = run_remote_checked(
                    remote,
                    REMOTE_AUDIT_SCENE_AND_HOPPER_SNIPPET,
                    f"Audit Scene Budget & Hopper Architecture (attempt {attempt})",
                )
                audit_data = extract_json_tag(lines, "AUDIT_JSON=")
                break
            except Exception as exc:
                last_err = exc
                print(f"[VerifyPhase12Fix1] Waiting for PIE warmup ({exc})...")

        if audit_data is None:
            raise RuntimeError(f"Failed PIE warmup audit: {last_err}")

        # Let Lumen/textures settle
        focus_pie_viewport_window()
        time.sleep(1.0)

        # =====================================================================
        # GATE 1: Airborne Scale Depression Fix Proof (DA_Contract_01)
        # =====================================================================
        start0 = extract_json_tag(
            run_remote_checked(remote, make_start_contract_snippet(0), "Gate 1: Start DA_Contract_01"),
            "START_JSON=",
        )
        base_pan_z = float(start0["pan_base_rel_z"])
        time.sleep(0.25)

        slice1 = extract_json_tag(
            run_remote_checked(
                remote,
                make_commit_horizontal_slice_snippet(25.0),
                "Gate 1: Commit ~11.5 kg Horizontal Slice Above Scale Pan",
            ),
            "SLICE_JSON=",
        )
        expected_chunk_kg = float(slice1["spawned_chunk_mass_kg"])

        airborne_in_box_sample: Optional[Dict[str, Any]] = None
        landed_settled_sample: Optional[Dict[str, Any]] = None
        min_pan_z_observed = base_pan_z

        poll_start = time.time()
        while time.time() - poll_start < 2.5:
            time.sleep(0.020)
            samp = extract_json_tag(
                run_remote_checked(remote, REMOTE_SAMPLE_SCALE_AND_CHUNKS_SNIPPET, "Poll Gate 1", verbose=False),
                "SAMPLE_JSON=",
            )
            min_pan_z_observed = min(min_pan_z_observed, float(samp["pan_rel_z"]))
            chunks = samp["active_chunks"]
            if airborne_in_box_sample is None and len(chunks) == 1:
                c0 = chunks[0]
                if c0["in_pan_trigger"] and c0["state"] == "IN_FLIGHT" and float(c0["z"]) > 62.0:
                    airborne_in_box_sample = samp

            if (
                len(chunks) == 1
                and chunks[0]["state"] == "ON_SCALE_PAN"
                and samp["receiver_settled"]
                and float(samp["settled_mass_kg"]) > 10.0
            ):
                landed_settled_sample = samp
                break

        if airborne_in_box_sample is None:
            raise AssertionError("Gate 1 FAILED: Did not capture airborne InFlight chunk inside PanTriggerBox!")
        air_z_delta_mm = abs(float(airborne_in_box_sample["pan_rel_z"]) - base_pan_z) * 10.0
        if air_z_delta_mm > 0.05 or float(airborne_in_box_sample["settled_mass_kg"]) != 0.0:
            raise AssertionError(
                f"Gate 1 FAILED: Premature airborne scale depression detected! "
                f"pan_z_delta={air_z_delta_mm:.3f} mm, settled_mass={airborne_in_box_sample['settled_mass_kg']} kg"
            )
        if float(airborne_in_box_sample["in_flight_mass_kg"]) < 10.0 or bool(airborne_in_box_sample["stamp_ready"]):
            raise AssertionError(
                f"Gate 1 FAILED: InFlightMassKg did not lock [E] while airborne: {airborne_in_box_sample}"
            )
        if landed_settled_sample is None:
            raise AssertionError("Gate 1 FAILED: Chunk did not transition to OnScalePan and settle after impact!")
        settled_depress_cm = base_pan_z - float(landed_settled_sample["pan_rel_z"])
        if settled_depress_cm < 6.0 or abs(float(landed_settled_sample["settled_mass_kg"]) - expected_chunk_kg) > 0.03:
            raise AssertionError(
                f"Gate 1 FAILED: Post-impact settle mismatch: depress={settled_depress_cm:.2f} cm, "
                f"settled={landed_settled_sample['settled_mass_kg']} vs expected={expected_chunk_kg}"
            )
        if float(landed_settled_sample["in_flight_mass_kg"]) != 0.0:
            raise AssertionError(f"Gate 1 FAILED: InFlightMassKg not cleared on impact: {landed_settled_sample}")

        # =====================================================================
        # GATE 2: Multi-Chunk Tall Stack Regression Proof (3 Pancake Slabs)
        # =====================================================================
        run_remote_checked(remote, make_start_contract_snippet(0), "Gate 2: Fresh Reset DA_Contract_01")
        time.sleep(0.25)

        stack_slice_masses: List[float] = []
        for s_idx in range(3):
            sl_info = extract_json_tag(
                run_remote_checked(
                    remote,
                    make_commit_horizontal_slice_snippet(8.6),
                    f"Gate 2: Commit Stacked Slab #{s_idx + 1}/3 (8.6 cm / ~3.96 kg)",
                ),
                "SLICE_JSON=",
            )
            stack_slice_masses.append(float(sl_info["spawned_chunk_mass_kg"]))
            time.sleep(0.95)

        # Wait up to 1.5s for 3-slab stack to settle completely
        stack_final_sample: Optional[Dict[str, Any]] = None
        stack_deadline = time.time() + 1.8
        while time.time() < stack_deadline:
            samp = extract_json_tag(
                run_remote_checked(remote, REMOTE_SAMPLE_SCALE_AND_CHUNKS_SNIPPET, "Poll Gate 2 Stack", verbose=False),
                "SAMPLE_JSON=",
            )
            if samp["receiver_settled"] and len(samp["active_chunks"]) == 3:
                stack_final_sample = samp
                if all(c["state"] == "ON_SCALE_PAN" for c in samp["active_chunks"]):
                    break
            time.sleep(0.08)

        if stack_final_sample is None:
            raise AssertionError("Gate 2 FAILED: 3-chunk stack did not settle cleanly on scale pan!")

        stack_chunks = stack_final_sample["active_chunks"]
        expected_stack_sum = sum(stack_slice_masses)
        max_stack_chunk_z = max(float(c["z"]) for c in stack_chunks)
        if len(stack_chunks) != 3 or any(c["state"] != "ON_SCALE_PAN" for c in stack_chunks):
            raise AssertionError(f"Gate 2 FAILED: Not all 3 stacked chunks reached ON_SCALE_PAN: {stack_chunks}")
        if abs(float(stack_final_sample["settled_mass_kg"]) - expected_stack_sum) > 0.03:
            raise AssertionError(
                f"Gate 2 FAILED: Stacked mass {stack_final_sample['settled_mass_kg']} != analytical sum {expected_stack_sum:.4f}"
            )
        if float(stack_final_sample["tare_waste_kg"]) > 0.01 or stack_final_sample["fault"] != "NONE":
            raise AssertionError(f"Gate 2 FAILED: Unexpected stray tare or fault during 3-chunk stack: {stack_final_sample}")

        # =====================================================================
        # GATE 3: Physical Tare Ramp Slide & Abyss Plunge Proof (DA_Contract_02)
        # =====================================================================
        run_remote_checked(remote, make_start_contract_snippet(1), "Gate 3: Load DA_Contract_02 (CantileveredJHook)")
        time.sleep(0.35)

        tog = extract_json_tag(
            run_remote_checked(remote, REMOTE_TOGGLE_DIVERTER_SNIPPET, "Gate 3: Toggle Diverter to TareChute"),
            "TOGGLE_JSON=",
        )
        if not tog["toggled"] or tog["route"] != "TARE_CHUTE":
            raise AssertionError(f"Gate 3 FAILED: Diverter did not toggle to TARE_CHUTE: {tog}")
        focus_pie_viewport_window()
        time.sleep(0.25)  # Wait for 0.18s flapper slew to +45 deg

        tare_slice = extract_json_tag(
            run_remote_checked(
                remote,
                make_commit_horizontal_slice_snippet(11.0),
                "Gate 3: Commit J-Hook Belly Slice Onto +45 deg Tare Ramp",
            ),
            "SLICE_JSON=",
        )
        tare_chunk_kg = float(tare_slice["spawned_chunk_mass_kg"])

        t_ramp_contact: Optional[float] = None
        sample_at_80ms: Optional[Dict[str, Any]] = None
        sample_at_200ms: Optional[Dict[str, Any]] = None
        min_active_tare_z = 999.0
        max_active_tare_y = -999.0
        requested_shot_ctr02 = False
        pooled_cleanly = False
        final_tare_sample: Optional[Dict[str, Any]] = None

        tare_poll_start = time.time()
        while time.time() - tare_poll_start < 2.6:
            time.sleep(0.006)
            samp = extract_json_tag(
                run_remote_checked(remote, REMOTE_SAMPLE_SCALE_AND_CHUNKS_SNIPPET, "Poll Gate 3 Tare", verbose=False),
                "SAMPLE_JSON=",
            )
            final_tare_sample = samp
            chunks = samp["active_chunks"]
            if len(chunks) >= 1 and not chunks[0]["hidden"]:
                c0 = chunks[0]
                cy = float(c0["y"])
                cz = float(c0["z"])
                min_active_tare_z = min(min_active_tare_z, cz)
                max_active_tare_y = max(max_active_tare_y, cy)

                if t_ramp_contact is None and cy > 10.0:
                    t_ramp_contact = float(samp["world_time"])

                if t_ramp_contact is not None:
                    dt_ramp = float(samp["world_time"]) - t_ramp_contact
                    if sample_at_80ms is None and dt_ramp >= 0.080:
                        sample_at_80ms = samp
                    if sample_at_200ms is None and dt_ramp >= 0.200:
                        sample_at_200ms = samp

                if (not requested_shot_ctr02) and cy >= 30.0:
                    run_remote_checked(
                        remote,
                        make_composited_screenshot_snippet(SHOT_CTR02_TARE),
                        "Capture Screenshot 1: Phase12Fix1_Ctr02_CantileveredJHook_TareShaft.png",
                    )
                    requested_shot_ctr02 = True
            elif t_ramp_contact is not None and len(chunks) == 0:
                pooled_cleanly = True
                break

        if not requested_shot_ctr02:
            focus_pie_viewport_window()
            run_remote_checked(
                remote,
                make_composited_screenshot_snippet(SHOT_CTR02_TARE),
                "Capture Screenshot 1 (fallback): Phase12Fix1_Ctr02_CantileveredJHook_TareShaft.png",
            )
        wait_for_file(SHOT_CTR02_TARE, timeout_sec=6.0)

        if sample_at_80ms is None or sample_at_200ms is None:
            raise AssertionError(
                f"Gate 3 FAILED: Chunk vanished prematurely on flapper before +0.08s/+0.20s! "
                f"s80={sample_at_80ms}, s200={sample_at_200ms}, min_z={min_active_tare_z:.2f}"
            )
        if min_active_tare_z > -42.0:
            raise AssertionError(
                f"Gate 3 FAILED: Chunk pooled before plunging below World Z <= -42.0 cm "
                f"(min_active_world_z={min_active_tare_z:.2f} cm)!"
            )
        if not pooled_cleanly or final_tare_sample is None:
            raise AssertionError("Gate 3 FAILED: Tare chunk did not return cleanly to UChunkPoolSubsystem in abyss!")
        if abs(float(final_tare_sample["tare_waste_kg"]) - tare_chunk_kg) > 0.03:
            raise AssertionError(
                f"Gate 3 FAILED: Idempotent Tare accounting mismatch: tare_waste={final_tare_sample['tare_waste_kg']} "
                f"vs expected single-count {tare_chunk_kg:.4f} kg!"
            )
        if float(final_tare_sample["in_flight_mass_kg"]) != 0.0 or final_tare_sample["fault"] != "NONE":
            raise AssertionError(f"Gate 3 FAILED: Post-Tare hygiene failure: {final_tare_sample}")

        # =====================================================================
        # GATE 4: 6-Contract Asymmetric Variety, Strain, Single-Island & Screenshots 2..3
        # =====================================================================
        contract_metrics: List[Dict[str, Any]] = []
        cut_island_checks: Dict[str, Dict[str, Any]] = {}
        frame_ms_samples: List[float] = []
        seen_proc_shapes = set()

        for c_idx in range(6):
            run_remote_checked(
                remote,
                make_start_contract_snippet(c_idx),
                f"Gate 4: Load DA_Contract_0{c_idx + 1}",
            )
            focus_pie_viewport_window()
            time.sleep(0.45)  # Allow spring tilt & Lumen surface cache settle

            c_info = extract_json_tag(
                run_remote_checked(
                    remote,
                    REMOTE_INSPECT_ACTIVE_CONTRACT_SNIPPET,
                    f"Gate 4: Inspect Settled DA_Contract_0{c_idx + 1}",
                ),
                "CONTRACT_JSON=",
            )
            contract_metrics.append(c_info)
            frame_ms_samples.append(float(c_info["delta_ms"]))

            bshape = c_info["base_shape"]
            if bshape != "CUSTOM_STATIC_MESH":
                if bshape in seen_proc_shapes:
                    raise AssertionError(f"Gate 4 FAILED: Duplicate procedural shape {bshape} on {c_info['contract_name']}!")
                seen_proc_shapes.add(bshape)

            if not c_info["is_closed"] or int(c_info["islands"]) != 1:
                raise AssertionError(f"Gate 4 FAILED: Non-manifold initial mesh on {c_info['contract_name']}: {c_info}")

            strain = float(c_info["solver_strain"])
            if c_idx == 0:
                if strain > 0.02:
                    raise AssertionError(f"Gate 4 FAILED: Expected ~0.0 strain on Ctr 01, got {strain:.4f}")
            else:
                if not (0.45 <= strain <= 0.75):
                    raise AssertionError(
                        f"Gate 4 FAILED: {c_info['contract_name']} settled strain {strain:.4f} outside [0.45, 0.75]!"
                    )

            if c_idx == 1:
                sl02 = extract_json_tag(
                    run_remote_checked(
                        remote,
                        make_commit_horizontal_slice_snippet(22.0),
                        "Gate 4: Verify Single-Island Horizontal Cut on CantileveredJHook (Ctr 02)",
                    ),
                    "SLICE_JSON=",
                )
                cut_island_checks["DA_Contract_02"] = sl02
                if sl02["spawned_chunk_count"] != 1 or sl02["rem_islands"] != 1:
                    raise AssertionError(f"Gate 4 FAILED: Floating island on CantileveredJHook cut: {sl02}")

            elif c_idx == 2:
                focus_pie_viewport_window()
                run_remote_checked(
                    remote,
                    make_composited_screenshot_snippet(SHOT_CTR03_CCHAN),
                    "Capture Screenshot 2: Phase12Fix1_Ctr03_OffsetCChannel.png",
                )
                time.sleep(0.45)
                wait_for_file(SHOT_CTR03_CCHAN, timeout_sec=6.0)

                run_remote_checked(remote, REMOTE_TOGGLE_DIVERTER_SNIPPET, "Route Ctr 03 Cut to Tare")
                time.sleep(0.22)
                sl03 = extract_json_tag(
                    run_remote_checked(
                        remote,
                        make_commit_horizontal_slice_snippet(24.0),
                        "Gate 4: Verify Single-Island Horizontal Cut on OffsetCChannel (Ctr 03)",
                    ),
                    "SLICE_JSON=",
                )
                cut_island_checks["DA_Contract_03"] = sl03
                if sl03["spawned_chunk_count"] != 1 or sl03["rem_islands"] != 1:
                    raise AssertionError(f"Gate 4 FAILED: Floating island on OffsetCChannel cut: {sl03}")

            elif c_idx == 4:
                focus_pie_viewport_window()
                run_remote_checked(
                    remote,
                    make_composited_screenshot_snippet(SHOT_CTR05_ZBRK),
                    "Capture Screenshot 3: Phase12Fix1_Ctr05_CounterweightedZBracket.png",
                )
                time.sleep(0.45)
                wait_for_file(SHOT_CTR05_ZBRK, timeout_sec=6.0)

                run_remote_checked(remote, REMOTE_TOGGLE_DIVERTER_SNIPPET, "Route Ctr 05 Cut to Tare")
                time.sleep(0.22)
                sl05 = extract_json_tag(
                    run_remote_checked(
                        remote,
                        make_commit_horizontal_slice_snippet(33.0),
                        "Gate 4: Verify Single-Island Horizontal Cut on CounterweightedZBracket (Ctr 05)",
                    ),
                    "SLICE_JSON=",
                )
                cut_island_checks["DA_Contract_05"] = sl05
                if sl05["spawned_chunk_count"] != 1 or sl05["rem_islands"] != 1:
                    raise AssertionError(f"Gate 4 FAILED: Floating island on CounterweightedZBracket cut: {sl05}")

        median_frame_ms = statistics.median(frame_ms_samples)
        if median_frame_ms > 13.0:
            raise AssertionError(f"Gate 4 FAILED: Unthrottled PIE median frame time {median_frame_ms:.2f} ms > 13.0 ms!")

        run_remote_checked(remote, REMOTE_END_PIE_SNIPPET, "End PIE Session Cleanly")
        time.sleep(0.5)

        print("\n================ PHASE 12-FIX1 LIVE PIE 4-GATE REPORT ================")
        print(
            f"  [Audit] shadow_lights={audit_data['shadow_lights']}, zero_shadow_lights={audit_data['zero_shadow_lights']}, "
            f"VolumetricFog={audit_data['volumetric_fog']}, TareChuteVisual rel_z={audit_data['tare_vis_rel_z_cm']} cm "
            f"(world_z={audit_data['tare_vis_world_z_cm']} cm), collar_max_local_z={audit_data['collar_max_local_z_cm']} cm (<=16.0 cm), "
            f"collar_mat={audit_data['collar_mat']} ({audit_data['collar_base_mat']}), furnace_hdr={audit_data['furnace_max_hdr']}"
        )
        print(
            f"  [Gate 1] Airborne-in-PanTriggerBox: chunk_z={airborne_in_box_sample['active_chunks'][0]['z']} cm, "
            f"pan_z_delta={air_z_delta_mm:.3f} mm (==0.00 mm), settled_mass={airborne_in_box_sample['settled_mass_kg']:.2f} kg, "
            f"in_flight_mass={airborne_in_box_sample['in_flight_mass_kg']:.2f} kg -> Post-Impact Settled: "
            f"depress={settled_depress_cm:.2f} cm, settled_mass={landed_settled_sample['settled_mass_kg']:.2f} kg, "
            f"in_flight={landed_settled_sample['in_flight_mass_kg']:.2f} kg"
        )
        print(
            f"  [Gate 2] 3-Chunk Stack: chunks_on_pan={len(stack_chunks)}/3, top_chunk_z={max_stack_chunk_z:.2f} cm, "
            f"settled_mass={stack_final_sample['settled_mass_kg']:.3f} kg (expected={expected_stack_sum:.3f} kg), "
            f"stray_tare={stack_final_sample['tare_waste_kg']:.3f} kg, fault={stack_final_sample['fault']}"
        )
        print(
            f"  [Gate 3] Tare Slide & Abyss Plunge: +80ms=(y={sample_at_80ms['active_chunks'][0]['y']} cm, "
            f"z={sample_at_80ms['active_chunks'][0]['z']} cm, state={sample_at_80ms['active_chunks'][0]['state']}), "
            f"+200ms=(y={sample_at_200ms['active_chunks'][0]['y']} cm, z={sample_at_200ms['active_chunks'][0]['z']} cm, "
            f"state={sample_at_200ms['active_chunks'][0]['state']}), max_y={max_active_tare_y:.2f} cm, "
            f"min_active_world_z={min_active_tare_z:.2f} cm (<=-42.0 cm), tare_waste={final_tare_sample['tare_waste_kg']:.3f} kg "
            f"(expected={tare_chunk_kg:.3f} kg)"
        )
        for cm in contract_metrics:
            print(
                f"  [Gate 4] {cm['contract_name']} | shape={cm['base_shape']:25s} | grip_off_y={cm['grip_offset_y_cm']:+5.2f} cm | "
                f"mass={cm['remainder_mass_kg']:5.2f} kg | strain={cm['solver_strain']*100.0:5.1f}% | "
                f"tilt=({cm['tilt_deg'][0]:+5.2f},{cm['tilt_deg'][1]:+5.2f}) deg | veins={cm['vein_count']}"
            )
        print(f"  [Gate 4] Single-Island Cuts={cut_island_checks} | median_frame_ms={median_frame_ms:.2f} ms (samples={frame_ms_samples})")
        for spath in (SHOT_CTR02_TARE, SHOT_CTR03_CCHAN, SHOT_CTR05_ZBRK):
            print(f"  [Screenshot] {spath} ({os.path.getsize(spath)} B)")
        print("======================================================================\n")
        return 0
    finally:
        if spawned_proc is not None:
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
