import sys

sys.dont_write_bytecode = True

import argparse
import ctypes
import json
import math
import os
import statistics
import struct
import subprocess
import time
import zlib
from typing import Dict, List, Optional, Tuple

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
LOGS_DIR = os.path.join(REPO_ROOT, "Saved", "Logs").replace("\\", "/")
SCREENSHOTS_DIR = os.path.join(REPO_ROOT, "Saved", "Screenshots", "Phase12C")

SHOT_1_EMERALD = os.path.join(SCREENSHOTS_DIR, "Phase12C_FlatEmerald_MoltenSeam.png")
SHOT_2_FLAPPER = os.path.join(SCREENSHOTS_DIR, "Phase12C_Beveled12Deg_FlapperImmunity.png")
SHOT_3_YELLOW = os.path.join(SCREENSHOTS_DIR, "Phase12C_YellowVeinDrop_vs_AmberProximity.png")
SHOT_4_CRIMSON = os.path.join(SCREENSHOTS_DIR, "Phase12C_CrimsonVeinIntersect_ArcSeam.png")


def focus_pie_viewport_window() -> None:
    try:
        user32 = ctypes.windll.user32
        WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)

        def _cb(hwnd, _lparam):
            if user32.IsWindowVisible(hwnd):
                length = user32.GetWindowTextLengthW(hwnd)
                if length > 0:
                    buf = ctypes.create_unicode_buffer(length + 1)
                    user32.GetWindowTextW(hwnd, buf, length + 1)
                    title = buf.value
                    if "Viewport" in title or "TareAndTithe" in title:
                        user32.keybd_event(0x12, 0, 0, 0)
                        user32.ShowWindow(hwnd, 9)
                        user32.BringWindowToTop(hwnd)
                        user32.SetForegroundWindow(hwnd)
                        user32.keybd_event(0x12, 0, 2, 0)
            return True

        user32.EnumWindows(WNDENUMPROC(_cb), 0)
    except Exception:
        pass


def run_remote_checked(remote: remote_execution.RemoteExecution, snippet: str, label: str) -> List[str]:
    res = remote.run_command(
        snippet,
        unattended=True,
        exec_mode=remote_execution.MODE_EXEC_FILE,
        raise_on_failure=False,
    )
    out_lines: List[str] = []
    for item in res.get("output", []):
        out_lines.append(str(item.get("output", "")).rstrip())
    if not res.get("success", False):
        joined = "\n".join(out_lines)
        raise RuntimeError(f"[{label}] Remote Python failed: {res.get('result')}\n{joined}")
    return out_lines


def extract_json_tag(lines: List[str], tag: str) -> dict:
    for line in reversed(lines):
        idx = line.find(tag)
        if idx >= 0:
            return json.loads(line[idx + len(tag) :])
    raise RuntimeError(f"Missing {tag} in remote output:\n" + "\n".join(lines[-20:]))


def wait_for_file(path: str, timeout_sec: float = 6.0) -> None:
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        if os.path.exists(path) and os.path.getsize(path) > 4096:
            return
        time.sleep(0.12)
    raise TimeoutError(f"Timed out waiting for screenshot file: {path}")


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
        ftype = raw[in_pos]
        in_pos += 1
        row_out = y * stride
        prev_out = (y - 1) * stride
        if ftype == 0:
            recon[row_out : row_out + stride] = raw[in_pos : in_pos + stride]
        elif ftype == 1:
            for i in range(stride):
                a = recon[row_out + i - bpp] if i >= bpp else 0
                recon[row_out + i] = (raw[in_pos + i] + a) & 0xFF
        elif ftype == 2:
            for i in range(stride):
                b = recon[prev_out + i] if y > 0 else 0
                recon[row_out + i] = (raw[in_pos + i] + b) & 0xFF
        elif ftype == 3:
            for i in range(stride):
                a = recon[row_out + i - bpp] if i >= bpp else 0
                b = recon[prev_out + i] if y > 0 else 0
                recon[row_out + i] = (raw[in_pos + i] + ((a + b) >> 1)) & 0xFF
        elif ftype == 4:
            for i in range(stride):
                a = recon[row_out + i - bpp] if i >= bpp else 0
                b = recon[prev_out + i] if y > 0 else 0
                c = recon[prev_out + i - bpp] if (y > 0 and i >= bpp) else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                recon[row_out + i] = (raw[in_pos + i] + pr) & 0xFF
        else:
            raise ValueError(f"Unsupported PNG filter {ftype}")
        in_pos += stride

    pixels: List[Tuple[int, int, int]] = []
    for y in range(height):
        base = y * stride
        for x in range(width):
            idx = base + x * bpp
            pixels.append((recon[idx], recon[idx + 1], recon[idx + 2]))
    return width, height, pixels


REMOTE_BEGIN_PIE_SNIPPET = """
import unreal

try:
    ed_perf = unreal.get_default_object(unreal.EditorPerformanceSettings)
    ed_perf.set_editor_property("throttle_cpu_when_not_foreground", False)
except Exception:
    pass

unreal.SystemLibrary.execute_console_command(None, "r.VSync 0")
unreal.SystemLibrary.execute_console_command(None, "r.VSyncEditor 0")
unreal.SystemLibrary.execute_console_command(None, "Slate.MaxFPS 240")
unreal.SystemLibrary.execute_console_command(None, "t.MaxFPS 240")
unreal.SystemLibrary.execute_console_command(None, "t.UnfocusedFrameRateLimit 0")

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les.is_in_play_in_editor():
    les.editor_request_end_play()
les.editor_request_begin_play()
unreal.log("[VerifyPhase12C] Requested BeginPlay PIE session.")
"""


REMOTE_WAIT_WARMUP_AND_AUDIT_CONTRACTS_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
if pie_world is None:
    raise RuntimeError("PIE GameWorld is None; PIE not ready yet.")

world_time_sec = float(unreal.GameplayStatics.get_time_seconds(pie_world))
if world_time_sec < 1.25:
    raise RuntimeError(f"PIE warmup in progress (world_time={world_time_sec:.3f}s < 1.25s)...")

unreal.SystemLibrary.execute_console_command(pie_world, "r.VSync 0")
unreal.SystemLibrary.execute_console_command(pie_world, "r.VSyncEditor 0")
unreal.SystemLibrary.execute_console_command(pie_world, "Slate.MaxFPS 240")
unreal.SystemLibrary.execute_console_command(pie_world, "t.MaxFPS 240")

shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
pawns = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()]
rigs = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()]
scales = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "ScaleAssemblyActor" in a.get_class().get_name()]
if len(pawns) != 1 or len(rigs) != 1 or len(scales) != 1:
    raise AssertionError(f"Expected 1 Pawn, 1 Rig, 1 Scale; found {len(pawns)}, {len(rigs)}, {len(scales)}")

pawn = pawns[0]
rig = rigs[0]
scale_actor = scales[0]

flapper_comp = scale_actor.get_editor_property("diverter_flapper_visual")
flapper_sm = flapper_comp.get_editor_property("static_mesh")
flapper_bb = flapper_sm.get_bounding_box()
flapper_xf = flapper_comp.get_world_transform()
f_pts = [
    unreal.MathLibrary.transform_location(flapper_xf, unreal.Vector(bx, by, bz))
    for bx in (flapper_bb.min.x, flapper_bb.max.x)
    for by in (flapper_bb.min.y, flapper_bb.max.y)
    for bz in (flapper_bb.min.z, flapper_bb.max.z)
]
flapper_world_box = {
    "min": [min(float(p.x) for p in f_pts), min(float(p.y) for p in f_pts), min(float(p.z) for p in f_pts)],
    "max": [max(float(p.x) for p in f_pts), max(float(p.y) for p in f_pts), max(float(p.z) for p in f_pts)],
}

# Correction B: Verify CuttableLocalBox height > 10.0 cm across ALL 6 contracts and catalog states!
contract_summaries = []
for c_idx in range(6):
    ok = bool(shift_sub.start_shift_from_contract(c_idx))
    if not ok:
        raise AssertionError(f"StartShiftFromContract({c_idx}) returned False!")
    tb = rig.get_target_world_bounds()
    cmask = pawn.get_cached_cuttable_mask_for_test()
    cm_valid = bool(cmask.get_editor_property("is_valid"))
    cm_he = cmask.get_editor_property("half_extents_cm")
    cm_ctr = cmask.get_editor_property("center_world")
    cm_jaw_z = float(cmask.get_editor_property("jaw_bottom_local_z"))
    cut_h = float(cm_he.z) * 2.0
    if not cm_valid or cut_h <= 10.0:
        raise AssertionError(
            f"Correction B FAILED on contract {c_idx}: valid={cm_valid}, cuttable_height_cm={cut_h:.2f} <= 10.0"
        )
    jaw_comp = rig.get_editor_property("clamp_jaw_visual")
    jaw_bb = jaw_comp.get_editor_property("static_mesh").get_bounding_box()
    jaw_xf = jaw_comp.get_world_transform()
    j_pts = [
        unreal.MathLibrary.transform_location(jaw_xf, unreal.Vector(bx, by, bz))
        for bx in (jaw_bb.min.x, jaw_bb.max.x)
        for by in (jaw_bb.min.y, jaw_bb.max.y)
        for bz in (jaw_bb.min.z, jaw_bb.max.z)
    ]
    jaw_min_z = min(float(p.z) for p in j_pts)
    jaw_max_z = max(float(p.z) for p in j_pts)

    contract_summaries.append({
        "index": c_idx,
        "bounds_min": [round(float(tb.min.x), 2), round(float(tb.min.y), 2), round(float(tb.min.z), 2)],
        "bounds_max": [round(float(tb.max.x), 2), round(float(tb.max.y), 2), round(float(tb.max.z), 2)],
        "mask_center": [round(float(cm_ctr.x), 2), round(float(cm_ctr.y), 2), round(float(cm_ctr.z), 2)],
        "mask_half_extents": [round(float(cm_he.x), 2), round(float(cm_he.y), 2), round(float(cm_he.z), 2)],
        "cuttable_height_cm": round(cut_h, 2),
        "jaw_bottom_local_z": round(cm_jaw_z, 2),
        "jaw_world_z": [round(jaw_min_z, 2), round(jaw_max_z, 2)],
    })

# Return to Contract 0 initially
shift_sub.start_shift_from_contract(0)
unreal.log("CONTRACTS_AUDIT_JSON=" + json.dumps({
    "flapper_world_box": flapper_world_box,
    "contracts": contract_summaries,
}))
"""


def make_switch_contract_snippet(contract_idx: int) -> str:
    return f"""
import unreal
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
if int(shift_sub.get_current_snapshot().get_editor_property("contract_index")) != int({contract_idx}):
    ok = bool(shift_sub.start_shift_from_contract(int({contract_idx})))
    if not ok:
        raise AssertionError("StartShiftFromContract({contract_idx}) returned False!")
snap = shift_sub.get_current_snapshot()
if snap.get_editor_property("active_diverter_route") != unreal.DiverterRoute.TITHE_SCALE:
    pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
    pawn.toggle_diverter_route_for_test()
"""


REMOTE_DISCOVER_CONTRACT0_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]

snap0 = shift_sub.get_current_snapshot()
target_kg_0 = float(snap0.get_editor_property("target_tithe_mass_kg"))
tol_kg_0 = float(snap0.get_editor_property("quota_tolerance_kg"))
tb0 = rig.get_target_world_bounds()
cm0 = pawn.get_cached_cuttable_mask_for_test()
cm0_ctr = cm0.get_editor_property("center_world")
cm0_he = cm0.get_editor_property("half_extents_cm")
min_z0, max_z0 = float(tb0.min.z), float(tb0.max.z)
cut_top_z0 = float(cm0_ctr.z + cm0_he.z)

emerald_z = None
best_emerald_err = 999.0
cyan_contact_z = None

z = min_z0 + 3.0
while z <= cut_top_z0 - 3.0:
    pawn.set_laser_plane_pose_for_test(z, 0.0, 0.0, 0.0)
    tel = pawn.get_cached_preview_telemetry_for_test()
    if bool(tel.get_editor_property("intersects_monolith")) and bool(tel.get_editor_property("has_valid_mass_estimate")):
        est = float(tel.get_editor_property("estimated_severed_mass_kg"))
        err = abs(est - target_kg_0)
        if err <= tol_kg_0 and err < best_emerald_err:
            best_emerald_err = err
            emerald_z = z
        # Pick Cyan standard cut ABOVE the quota window (z ~ 200+ cm, wide-open 20px sheet view above camera Z=162)
        if cyan_contact_z is None and (est > target_kg_0 + tol_kg_0 + 3.5) and z >= 198.0:
            cyan_contact_z = z
    z += 0.75

if emerald_z is None:
    raise AssertionError("Could not find Emerald quota window cut Z on Contract 0!")
if cyan_contact_z is None:
    cyan_contact_z = min(cut_top_z0 - 5.0, emerald_z + 14.0)

idle_no_contact_z = min_z0 - 18.0
jaw_overlap_z = max_z0 - 5.0

unreal.log("C0_POSES_JSON=" + json.dumps({
    "emerald_flat": {"contract": 0, "z": round(emerald_z, 2), "h": 0.0, "roll": 0.0, "pitch": 0.0},
    "cyan_flat": {"contract": 0, "z": round(cyan_contact_z, 2), "h": 0.0, "roll": 0.0, "pitch": 0.0},
    "beveled_12deg": {"contract": 0, "z": round(emerald_z, 2), "h": 0.0, "roll": 12.0, "pitch": 0.0},
    "no_contact": {"contract": 0, "z": round(idle_no_contact_z, 2), "h": -16.0, "roll": 0.0, "pitch": 0.0},
    "jaw_overlap": {"contract": 0, "z": round(jaw_overlap_z, 2), "h": 0.0, "roll": 0.0, "pitch": 0.0},
}))
"""


REMOTE_DISCOVER_FLAPPER_ON_SETTLED_CONTRACT_SNIPPET = """
import json
import math
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]
scale_actor = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "ScaleAssemblyActor" in a.get_class().get_name()][0]

c_idx = int(shift_sub.get_current_snapshot().get_editor_property("contract_index"))
flapper_comp = scale_actor.get_editor_property("diverter_flapper_visual")
flapper_bb = flapper_comp.get_editor_property("static_mesh").get_bounding_box()
flapper_xf = flapper_comp.get_world_transform()
f_pts = [
    unreal.MathLibrary.transform_location(flapper_xf, unreal.Vector(bx, by, bz))
    for bx in (flapper_bb.min.x, flapper_bb.max.x)
    for by in (flapper_bb.min.y, flapper_bb.max.y)
    for bz in (flapper_bb.min.z, flapper_bb.max.z)
]
flapper_min_z = min(float(p.z) for p in f_pts)
flapper_max_z = max(float(p.z) for p in f_pts)
flapper_cx = 0.5 * (min(float(p.x) for p in f_pts) + max(float(p.x) for p in f_pts))
flapper_cy = 0.5 * (min(float(p.y) for p in f_pts) + max(float(p.y) for p in f_pts))

pivot = pawn.get_editor_property("laser_plane_visual_pivot")
tb_f = rig.get_target_world_bounds()
flat_z = None
bev_z = None
bev_roll = -12.0

# Sweep settled monolith for simultaneous monolith + SM_DiverterFlapper intersection (flat 0 deg)
z = float(tb_f.min.z) + 0.8
while z <= flapper_max_z - 0.8:
    pawn.set_laser_plane_pose_for_test(z, 10.0, 0.0, 0.0)
    tel = pawn.get_cached_preview_telemetry_for_test()
    if bool(tel.get_editor_property("intersects_monolith")) and float(tel.get_editor_property("estimated_severed_mass_kg")) > 0.15:
        flat_z = z
        if float(tel.get_editor_property("estimated_severed_mass_kg")) >= 0.60 and z <= flapper_max_z - 1.5:
            break
    z += 0.40

# Sweep 12 deg roll (testing -12.0 then +12.0 using exact pivot plane normal at flapper_cx, flapper_cy!)
for cand_roll in (-12.0, 12.0):
    z = float(tb_f.min.z) - 2.0
    while z <= flapper_max_z + 8.0:
        pawn.set_laser_plane_pose_for_test(z, 10.0, cand_roll, 0.0)
        tel = pawn.get_cached_preview_telemetry_for_test()
        p_orig = pivot.get_world_location()
        p_norm = unreal.MathLibrary.get_up_vector(pivot.get_world_rotation())
        nz = float(p_norm.z) if abs(float(p_norm.z)) > 0.1 else 1.0
        z_at_flapper = float(p_orig.z) - (
            ((flapper_cx - float(p_orig.x)) * float(p_norm.x) + (flapper_cy - float(p_orig.y)) * float(p_norm.y)) / nz
        )
        if (
            bool(tel.get_editor_property("intersects_monolith"))
            and float(tel.get_editor_property("estimated_severed_mass_kg")) > 0.35
            and (flapper_min_z + 2.0 <= z_at_flapper <= flapper_max_z - 1.5)
        ):
            bev_z = z
            bev_roll = cand_roll
            break
        z += 0.50
    if bev_z is not None:
        break

unreal.log("FLAPPER_CANDIDATE_JSON=" + json.dumps({
    "contract": c_idx,
    "tb_min_z": round(float(tb_f.min.z), 2),
    "flapper_max_z": round(flapper_max_z, 2),
    "flat_z": None if flat_z is None else round(flat_z, 2),
    "bev_z": None if bev_z is None else round(bev_z, 2),
    "bev_roll": bev_roll,
}))
"""


REMOTE_SWEEP_SETTLED_VEINS_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]

c_idx = int(shift_sub.get_current_snapshot().get_editor_property("contract_index"))
tb = rig.get_target_world_bounds()
cm = pawn.get_cached_cuttable_mask_for_test()
cm_ctr = cm.get_editor_property("center_world")
cm_he = cm.get_editor_property("half_extents_cm")
z_lo = float(tb.min.z) + 2.5
z_hi = float(cm_ctr.z + cm_he.z) - 4.0

yellow_cand = None
amber_cand = None
best_amber_err = 999.0
crimson_cand = None
best_crimson_clr = 999.0

z = z_lo
while z <= z_hi:
    pawn.set_laser_plane_pose_for_test(z, 0.0, 0.0, 0.0)
    tel = pawn.get_cached_preview_telemetry_for_test()
    if (
        bool(tel.get_editor_property("intersects_monolith"))
        and bool(tel.get_editor_property("has_valid_mass_estimate"))
        and float(tel.get_editor_property("estimated_severed_mass_kg")) > 0.35
    ):
        prox = float(tel.get_editor_property("vein_proximity_normalized"))
        clr = float(tel.get_editor_property("min_capsule_clearance_cm"))
        has_vein = bool(tel.get_editor_property("severed_half_contains_vein"))
        det = bool(tel.get_editor_property("will_detonate"))

        if det and clr < best_crimson_clr:
            best_crimson_clr = clr
            crimson_cand = {"contract": c_idx, "z": round(z, 2), "prox": round(prox, 3), "clr": round(clr, 3)}
        elif (not det) and has_vein and prox <= 0.12 and yellow_cand is None:
            yellow_cand = {"contract": c_idx, "z": round(z, 2), "prox": round(prox, 3)}
        elif (not det) and (not has_vein) and (0.45 <= prox <= 0.56):
            err = abs(prox - 0.50)
            if err < best_amber_err:
                best_amber_err = err
                amber_cand = {"contract": c_idx, "z": round(z, 2), "prox": round(prox, 3)}
    z += 0.50

unreal.log("VEIN_CANDIDATE_JSON=" + json.dumps({
    "yellow": yellow_cand,
    "amber": amber_cand,
    "crimson": crimson_cand,
}))
"""


def make_apply_pose_snippet(
    contract_idx: int, z: float, h: float, roll: float, pitch: float, charge: float = 0.0
) -> str:
    return f"""
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]

cur_idx = int(shift_sub.get_current_snapshot().get_editor_property("contract_index"))
if cur_idx != int({contract_idx}):
    raise AssertionError(f"Expected contract {{int({contract_idx})}} already settled, found {{cur_idx}}!")

snap = shift_sub.get_current_snapshot()
if snap.get_editor_property("active_diverter_route") != unreal.DiverterRoute.TITHE_SCALE:
    pawn.toggle_diverter_route_for_test()

pawn.set_laser_plane_pose_for_test(float({z}), float({h}), float({roll}), float({pitch}))
pawn.set_capacitor_charge_for_test(float({charge}))

tel = pawn.get_cached_preview_telemetry_for_test()
unreal.log("POSE_APPLIED_JSON=" + json.dumps({{
    "intersects": bool(tel.get_editor_property("intersects_monolith")),
    "detonate": bool(tel.get_editor_property("will_detonate")),
    "vein_in_half": bool(tel.get_editor_property("severed_half_contains_vein")),
    "prox": round(float(tel.get_editor_property("vein_proximity_normalized")), 4),
    "mass_kg": round(float(tel.get_editor_property("estimated_severed_mass_kg")), 3),
}}))
"""


REMOTE_SPAWN_CAPTURE_SNIPPET = """
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
cam = pawn.get_editor_property("inspector_camera")

# Remove any existing transient capture first
for old_cap in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.SceneCapture2D):
    if unreal.Name("Phase11AFixTransientLitCapture") in list(old_cap.tags):
        old_cap.destroy_actor()

rt = unreal.RenderingLibrary.create_render_target2d(
    pie_world,
    1280,
    720,
    unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB,
    unreal.LinearColor(0.0, 0.0, 0.0, 1.0),
    False,
    False,
)
cap_actor = unreal.TareTitheGameMode.spawn_transient_lit_capture2d(cam, rt)
if cap_actor is None:
    raise RuntimeError("SpawnTransientLitCapture2D returned None!")
"""


def make_sync_and_export_pair_snippet(logs_dir: str, with_laser_file: str, clean_bg_file: Optional[str]) -> str:
    clean_block = ""
    if clean_bg_file:
        clean_block = f"""
for prop_name in laser_props:
    comp = pawn.get_editor_property(prop_name)
    if comp is not None:
        scc.hide_component(comp)
scc.capture_scene()
unreal.RenderingLibrary.export_render_target(pie_world, rt, {logs_dir!r}, {clean_bg_file!r})
scc.clear_hidden_components()
for tc in pawn.get_components_by_class(unreal.TextRenderComponent):
    scc.hide_component(tc)
"""
    return f"""
import json
import math
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]
scale_actor = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "ScaleAssemblyActor" in a.get_class().get_name()][0]
cam = pawn.get_editor_property("inspector_camera")

caps = [
    a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.SceneCapture2D)
    if unreal.Name("Phase11AFixTransientLitCapture") in list(a.tags)
]
cap_actor = caps[0]
scc = cap_actor.get_editor_property("capture_component2d")
scc.set_editor_property("capture_every_frame", False)
cap_actor.set_actor_location_and_rotation(cam.get_world_location(), cam.get_world_rotation(), False, False)
scc.set_editor_property("fov_angle", float(cam.get_editor_property("field_of_view")))
scc.clear_hidden_components()
for tc in pawn.get_components_by_class(unreal.TextRenderComponent):
    scc.hide_component(tc)

laser_props = (
    "laser_sheet_visual",
    "laser_sheet_underside_visual",
    "laser_frame_front_bar",
    "laser_frame_back_bar",
    "laser_frame_left_bar",
    "laser_frame_right_bar",
)

rt = scc.get_editor_property("texture_target")
scc.capture_scene()
unreal.RenderingLibrary.export_render_target(pie_world, rt, {logs_dir!r}, {with_laser_file!r})
{clean_block}

# Compute exact projected screen coordinates for ROIs at this exact camera & laser pose
W, H = 1280, 720
cam_loc = scc.get_world_location()
cam_rot = scc.get_world_rotation()
cam_fov = float(scc.get_editor_property("fov_angle"))
fwd = unreal.MathLibrary.get_forward_vector(cam_rot)
right = unreal.MathLibrary.get_right_vector(cam_rot)
up = unreal.MathLibrary.get_up_vector(cam_rot)
tan_h = math.tan(math.radians(cam_fov * 0.5))
tan_v = tan_h * (float(H) / float(W))

def project_pt(pt):
    rx = float(pt.x - cam_loc.x)
    ry = float(pt.y - cam_loc.y)
    rz = float(pt.z - cam_loc.z)
    d = rx * float(fwd.x) + ry * float(fwd.y) + rz * float(fwd.z)
    if d <= 1.0:
        return None
    cx = rx * float(right.x) + ry * float(right.y) + rz * float(right.z)
    cy = rx * float(up.x) + ry * float(up.y) + rz * float(up.z)
    px = int(round((0.5 + 0.5 * (cx / (d * tan_h))) * W))
    py = int(round((0.5 - 0.5 * (cy / (d * tan_v))) * H))
    return [px, py]

pivot = pawn.get_editor_property("laser_plane_visual_pivot")
piv_xf = pivot.get_world_transform()

def sheet_uv_to_px(u, v):
    # Plane mesh is 100x100 scaled (0.72, 0.68, 1.0): local X in [-36, +36], local Y in [-34, +34]
    lx = (u - 0.5) * 72.0
    ly = (v - 0.5) * 68.0
    wp = unreal.MathLibrary.transform_location(piv_xf, unreal.Vector(lx, ly, 0.0))
    return project_pt(wp)

# 1. Sample points along front perimeter border (U = 0.005, V in [0.15..0.85]) and left/right borders
border_pts = []
for i in range(25):
    vv = 0.12 + (0.76 * i / 24.0)
    p_front = sheet_uv_to_px(0.004, vv)
    if p_front:
        border_pts.append(p_front)
for i in range(15):
    uu = 0.06 + (0.45 * i / 14.0)
    p_left = sheet_uv_to_px(uu, 0.004)
    p_right = sheet_uv_to_px(uu, 0.996)
    if p_left:
        border_pts.append(p_left)
    if p_right:
        border_pts.append(p_right)

# 2. Sample clean interior scanline grid on open left wing (U in [0.14..0.78], V in [0.08..0.24])
#    100% clear of 0.018 UV perimeter frame, LaserFrameFrontBar, monolith seam, and SM_DiverterFlapper!
interior_pts = []
for iu in range(13):
    uu = 0.14 + (0.64 * iu / 12.0)
    for iv in range(17):
        vv = 0.08 + (0.16 * iv / 16.0)
        p_in = sheet_uv_to_px(uu, vv)
        if p_in:
            interior_pts.append(p_in)

# 3. Monolith front contact curve points: intersect vertical lines on monolith front face with laser plane
tb = rig.get_target_world_bounds()
cm = pawn.get_cached_cuttable_mask_for_test()
piv_inv = piv_xf.inverse()
p_orig = pivot.get_world_location()
p_norm = unreal.MathLibrary.get_up_vector(pivot.get_world_rotation())
nz = float(p_norm.z) if abs(float(p_norm.z)) > 0.1 else 1.0

mono_front_x = float(tb.min.x)
mono_y0 = float(tb.min.y) + 2.0
mono_y1 = float(tb.max.y) - 2.0
seam_pts = []
for i in range(25):
    yy = mono_y0 + ((mono_y1 - mono_y0) * i / 24.0)
    # Plane eq: dot(P - p_orig, p_norm) = 0 -> z = p_orig.z - ((x - p_orig.x)*nx + (y - p_orig.y)*ny)/nz
    zz = float(p_orig.z) - (((mono_front_x - float(p_orig.x)) * float(p_norm.x) + (yy - float(p_orig.y)) * float(p_norm.y)) / nz)
    p_s = project_pt(unreal.Vector(mono_front_x - 0.6, yy, zz))
    if p_s:
        seam_pts.append(p_s)

# 4. SM_DiverterFlapper intersection points across actual flapper world bounds inside sheet interior
flapper_comp = scale_actor.get_editor_property("diverter_flapper_visual")
flapper_bb = flapper_comp.get_editor_property("static_mesh").get_bounding_box()
flapper_xf = flapper_comp.get_world_transform()
flp_w_pts = [
    unreal.MathLibrary.transform_location(flapper_xf, unreal.Vector(bx, by, bz))
    for bx in (flapper_bb.min.x, flapper_bb.max.x)
    for by in (flapper_bb.min.y, flapper_bb.max.y)
    for bz in (flapper_bb.min.z, flapper_bb.max.z)
]
flp_min_x = min(float(p.x) for p in flp_w_pts) + 1.5
flp_max_x = max(float(p.x) for p in flp_w_pts) - 1.5
flp_cy = 0.5 * (min(float(p.y) for p in flp_w_pts) + max(float(p.y) for p in flp_w_pts))

flapper_pts = []
for i in range(19):
    xx = flp_min_x + ((flp_max_x - flp_min_x) * i / 18.0)
    for yy in (flp_cy - 1.0, flp_cy, flp_cy + 1.0):
        zz = float(p_orig.z) - (((xx - float(p_orig.x)) * float(p_norm.x) + (yy - float(p_orig.y)) * float(p_norm.y)) / nz)
        lp_exact = unreal.MathLibrary.transform_location(piv_inv, unreal.Vector(xx, yy, zz))
        u_chk = (float(lp_exact.x) / 72.0) + 0.5
        v_chk = (float(lp_exact.y) / 68.0) + 0.5
        if 0.04 <= u_chk <= 0.96 and 0.04 <= v_chk <= 0.94:
            p_f = project_pt(unreal.Vector(xx, yy, zz))
            if p_f:
                flapper_pts.append(p_f)

# 5. SM_ClampJaw front intersection points (X = jaw_front_x, Y across jaw width)
jaw_comp = rig.get_editor_property("clamp_jaw_visual")
jaw_bb = jaw_comp.get_editor_property("static_mesh").get_bounding_box()
jaw_xf = jaw_comp.get_world_transform()
j_pts_w = [
    unreal.MathLibrary.transform_location(jaw_xf, unreal.Vector(bx, by, bz))
    for bx in (jaw_bb.min.x, jaw_bb.max.x)
    for by in (jaw_bb.min.y, jaw_bb.max.y)
    for bz in (jaw_bb.min.z, jaw_bb.max.z)
]
jaw_front_x = min(float(p.x) for p in j_pts_w)
jaw_min_y = min(float(p.y) for p in j_pts_w) + 1.5
jaw_max_y = max(float(p.y) for p in j_pts_w) - 1.5
jaw_pts = []
for i in range(21):
    yy = jaw_min_y + ((jaw_max_y - jaw_min_y) * i / 20.0)
    zz = float(p_orig.z) - (((jaw_front_x - float(p_orig.x)) * float(p_norm.x) + (yy - float(p_orig.y)) * float(p_norm.y)) / nz)
    p_j = project_pt(unreal.Vector(jaw_front_x - 0.5, yy, zz))
    if p_j:
        jaw_pts.append(p_j)

unreal.log("CAPTURE_PTS_JSON=" + json.dumps({{
    "border_pts": border_pts,
    "interior_pts": interior_pts,
    "seam_pts": seam_pts,
    "flapper_pts": flapper_pts,
    "jaw_pts": jaw_pts,
}}))
"""


def make_request_composited_screenshot_snippet(abs_png_path: str) -> str:
    return f"""
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
hud_bridge = pawn.get_component_by_class(unreal.InspectorHudBridgeComponent)
if hud_bridge is None or not bool(hud_bridge.is_overlay_mounted()):
    raise AssertionError("InspectorHudBridgeComponent missing or STareTitheHudOverlay not mounted!")

ok = bool(hud_bridge.request_composited_viewport_screenshot({abs_png_path!r}))
if not ok:
    raise RuntimeError("RequestCompositedViewportScreenshot returned False!")
"""


REMOTE_MEASURE_STEADY_FRAME_TIME_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
unreal.SystemLibrary.execute_console_command(pie_world, "r.VSync 0")
unreal.SystemLibrary.execute_console_command(pie_world, "r.VSyncEditor 0")
unreal.SystemLibrary.execute_console_command(pie_world, "Slate.MaxFPS 240")
unreal.SystemLibrary.execute_console_command(pie_world, "t.MaxFPS 240")
delta_ms = float(unreal.GameplayStatics.get_world_delta_seconds(pie_world)) * 1000.0
unreal.log("FRAME_MS_JSON=" + json.dumps({"delta_ms": round(delta_ms, 3)}))
"""


REMOTE_CLEANUP_AND_END_PIE_SNIPPET = """
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
if pie_world is not None:
    for cap in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.SceneCapture2D):
        if unreal.Name("Phase11AFixTransientLitCapture") in list(cap.tags):
            scc = cap.get_editor_property("capture_component2d")
            scc.set_editor_property("capture_every_frame", False)
            scc.set_editor_property("texture_target", None)
            cap.destroy_actor()

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les.is_in_play_in_editor():
    les.editor_request_end_play()
"""


REMOTE_QUIT_EDITOR_SNIPPET = """
import unreal
unreal.SystemLibrary.quit_editor()
"""


def sample_band_around_pts(
    w: int,
    h: int,
    px: List[Tuple[int, int, int]],
    pts: List[List[int]],
    rx: int = 1,
    ry: int = 3,
) -> List[Tuple[int, int, int, int, int]]:
    seen = set()
    samples: List[Tuple[int, int, int, int, int]] = []
    for cx, cy in pts:
        for dy in range(-ry, ry + 1):
            yy = cy + dy
            if yy < 0 or yy >= h:
                continue
            for dx in range(-rx, rx + 1):
                xx = cx + dx
                if xx < 0 or xx >= w:
                    continue
                if (xx, yy) not in seen:
                    seen.add((xx, yy))
                    r, g, b = px[yy * w + xx]
                    samples.append((xx, yy, r, g, b))
    return samples


def measure_seam_metrics(
    w: int,
    h: int,
    px_with: List[Tuple[int, int, int]],
    px_clean: List[Tuple[int, int, int]],
    pts: List[List[int]],
    ry: int = 4,
) -> dict:
    samples = sample_band_around_pts(w, h, px_with, pts, rx=1, ry=ry)
    peak_r = 0
    best_rgb = (0, 0, 0)
    best_delta = (0, 0, 0)
    warm_incandescent_count = 0
    crimson_arc_count = 0
    total_seam_energy = 0.0

    for xx, yy, r, g, b in samples:
        cr, cg, cb = px_clean[yy * w + xx]
        dr, dg, db = r - cr, g - cg, b - cb
        if dr > 0:
            total_seam_energy += float(dr + max(0, dg) + max(0, db))
        if r >= 215 and dr >= 65 and r >= g and g >= b:
            warm_incandescent_count += 1
        if r >= 215 and dr >= 65 and r > g:
            crimson_arc_count += 1
        if r > peak_r and dr >= 40:
            peak_r = r
            best_rgb = (r, g, b)
            best_delta = (dr, dg, db)

    return {
        "sample_count": len(samples),
        "peak_r": peak_r,
        "best_rgb": list(best_rgb),
        "best_delta": list(best_delta),
        "warm_incandescent_px": warm_incandescent_count,
        "crimson_arc_px": crimson_arc_count,
        "total_energy": round(total_seam_energy, 1),
    }


def measure_false_molten_glow(
    w: int,
    h: int,
    px_with: List[Tuple[int, int, int]],
    px_clean: List[Tuple[int, int, int]],
    pts: List[List[int]],
    ry: int = 3,
) -> dict:
    """
    On a Cyan or Emerald laser sheet, the base sheet adds near-zero Red (DeltaR <= ~12 sRGB),
    whereas ANY false molten contact seam adds +80..+210 sRGB of warm Red (R >= 185, R > G).
    """
    samples = sample_band_around_pts(w, h, px_with, pts, rx=1, ry=ry)
    molten_false_px = 0
    max_delta_r = 0
    max_r_on_warm = 0
    for xx, yy, r, g, b in samples:
        cr, cg, cb = px_clean[yy * w + xx]
        dr = r - cr
        if dr > max_delta_r:
            max_delta_r = dr
        if r >= 185 and dr >= 38 and r > g:
            molten_false_px += 1
            max_r_on_warm = max(max_r_on_warm, r)
    return {
        "sample_count": len(samples),
        "molten_false_px": molten_false_px,
        "max_delta_r": max_delta_r,
        "max_r_on_warm": max_r_on_warm,
    }


def measure_perimeter_signature(
    w: int,
    h: int,
    px_with: List[Tuple[int, int, int]],
    px_clean: List[Tuple[int, int, int]],
    border_pts: List[List[int]],
) -> Tuple[float, float, float]:
    # Pick bright perimeter pixels along the projected 0.018 UV border / hardware bar (ry=2)
    samples = sample_band_around_pts(w, h, px_with, border_pts, rx=1, ry=2)
    scored: List[Tuple[float, int, int, int]] = []
    for xx, yy, r, g, b in samples:
        cr, cg, cb = px_clean[yy * w + xx]
        lift = max(0, r - cr) + max(0, g - cg) + max(0, b - cb)
        scored.append((float(lift), r, g, b))
    scored.sort(key=lambda t: t[0], reverse=True)
    top = scored[: max(12, len(scored) // 5)]
    mr = sum(t[1] for t in top) / len(top)
    mg = sum(t[2] for t in top) / len(top)
    mb = sum(t[3] for t in top) / len(top)
    return (round(mr, 1), round(mg, 1), round(mb, 1))


def measure_interior_scanline_metrics(
    w: int,
    h: int,
    px_with: List[Tuple[int, int, int]],
    px_clean: List[Tuple[int, int, int]],
    interior_pts: List[List[int]],
) -> dict:
    rgb_lifts: List[float] = []
    dom_lifts: List[float] = []
    seen = set()
    for cx, cy in interior_pts:
        if 0 <= cx < w and 0 <= cy < h and (cx, cy) not in seen:
            seen.add((cx, cy))
            r, g, b = px_with[cy * w + cx]
            cr, cg, cb = px_clean[cy * w + cx]
            dr, dg, db = float(r - cr), float(g - cg), float(b - cb)
            rgb_lifts.append((dr + dg + db) / 3.0)
            dom_lifts.append(max(dr, dg, db))
    mean_lift = statistics.mean(rgb_lifts) if rgb_lifts else 999.0
    std_dom = statistics.pstdev(dom_lifts) if len(dom_lifts) >= 2 else 0.0
    std_rgb = statistics.pstdev(rgb_lifts) if len(rgb_lifts) >= 2 else 0.0
    return {
        "count": len(rgb_lifts),
        "mean_bg_lift_srgb": round(mean_lift, 2),
        "std_dom_srgb": round(std_dom, 2),
        "std_rgb_srgb": round(std_rgb, 2),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Phase 12C Live PIE 4-Gate Verification")
    parser.add_argument("--timeout", type=float, default=120.0)
    parser.add_argument("--no-auto-launch", action="store_true")
    parser.add_argument("--shutdown", action="store_true")
    args = parser.parse_args()

    os.makedirs(os.path.join(REPO_ROOT, "Saved", "Logs"), exist_ok=True)
    os.makedirs(SCREENSHOTS_DIR, exist_ok=True)

    for shot in (SHOT_1_EMERALD, SHOT_2_FLAPPER, SHOT_3_YELLOW, SHOT_4_CRIMSON):
        if os.path.exists(shot):
            os.remove(shot)

    cfg = remote_execution.RemoteExecutionConfig()
    cfg.multicast_ttl = 0
    cfg.multicast_bind_address = "127.0.0.1"
    cfg.multicast_group_endpoint = ("239.0.0.1", 6766)
    cfg.command_endpoint = ("127.0.0.1", 6776)

    spawned_proc: Optional[subprocess.Popen] = None
    remote = remote_execution.RemoteExecution(cfg)
    remote.start()
    temp_files: List[str] = []

    try:
        target_node = None
        quick_deadline = time.time() + 3.0
        while time.time() < quick_deadline:
            for node in remote.remote_nodes:
                if "TareAndTithe" in str(node.get("project_name", "")) or len(remote.remote_nodes) == 1:
                    target_node = node
                    break
            if target_node is not None:
                break
            time.sleep(0.5)

        if target_node is None and not args.no_auto_launch:
            print(f"[VerifyPhase12C] Launching UnrealEditor: {UNREAL_EDITOR_EXE}")
            spawned_proc = subprocess.Popen(
                [UNREAL_EDITOR_EXE, UPROJECT_PATH, "-NoSplash", "-Unattended"],
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
            raise TimeoutError(f"Timed out after {args.timeout}s waiting for UnrealEditor remote node")

        remote.open_command_connection(target_node["node_id"])
        print(f"[VerifyPhase12C] Connected to node_id={target_node['node_id']}")

        run_remote_checked(remote, REMOTE_BEGIN_PIE_SNIPPET, "Begin PIE Session")
        time.sleep(0.6)
        focus_pie_viewport_window()

        contracts_audit = None
        last_err = None
        for attempt in range(1, 14):
            time.sleep(1.0)
            try:
                lines = run_remote_checked(
                    remote,
                    REMOTE_WAIT_WARMUP_AND_AUDIT_CONTRACTS_SNIPPET,
                    f"Warmup & All-6-Contract CutMask Audit (attempt {attempt})",
                )
                contracts_audit = extract_json_tag(lines, "CONTRACTS_AUDIT_JSON=")
                break
            except Exception as exc:
                last_err = exc
                print(f"[VerifyPhase12C] Waiting for PIE warmup ({exc})...")

        if contracts_audit is None:
            raise RuntimeError(f"Warmup/Contracts audit failed: {last_err}")

        def switch_and_settle_contract(contract_idx: int) -> None:
            run_remote_checked(
                remote,
                make_switch_contract_snippet(contract_idx),
                f"Switch to Contract {contract_idx}",
            )
            time.sleep(0.65)

        # 1. Discover Contract 0 poses after settling Contract 0
        switch_and_settle_contract(0)
        c0_lines = run_remote_checked(remote, REMOTE_DISCOVER_CONTRACT0_SNIPPET, "Discover Contract 0 Poses")
        poses = extract_json_tag(c0_lines, "C0_POSES_JSON=")

        # 2. Discover settled Flapper + Monolith simultaneous overlap poses (flat 0 deg AND -12 deg roll!)
        flapper_found = None
        for c_idx in (5, 3, 2, 1, 4, 0):
            switch_and_settle_contract(c_idx)
            fl_lines = run_remote_checked(
                remote,
                REMOTE_DISCOVER_FLAPPER_ON_SETTLED_CONTRACT_SNIPPET,
                f"Check Settled Flapper Overlap on Contract {c_idx}",
            )
            fl_cand = extract_json_tag(fl_lines, "FLAPPER_CANDIDATE_JSON=")
            print(f"[VerifyPhase12C] Flapper candidate C{c_idx}: {fl_cand}")
            if fl_cand["flat_z"] is not None and fl_cand["bev_z"] is not None:
                flapper_found = fl_cand
                break
        if flapper_found is None:
            raise AssertionError("Could not find settled contract with both flat & 12 deg flapper+monolith overlap!")

        poses["flapper_flat"] = {
            "contract": flapper_found["contract"],
            "z": flapper_found["flat_z"],
            "h": 10.0,
            "roll": 0.0,
            "pitch": 0.0,
        }
        poses["flapper_beveled_12deg"] = {
            "contract": flapper_found["contract"],
            "z": flapper_found["bev_z"],
            "h": 10.0,
            "roll": flapper_found["bev_roll"],
            "pitch": 0.0,
        }

        # 3. Discover settled Yellow [VEIN -> SPACE], Amber ISOTOPE PROXIMITY, and Crimson VEIN INTERSECT!
        yellow_pose = None
        amber_pose = None
        crimson_pose = None
        for c_idx in (2, 3, 4, 5):
            switch_and_settle_contract(c_idx)
            vn_lines = run_remote_checked(
                remote,
                REMOTE_SWEEP_SETTLED_VEINS_SNIPPET,
                f"Sweep Settled Vein States on Contract {c_idx}",
            )
            vc = extract_json_tag(vn_lines, "VEIN_CANDIDATE_JSON=")
            if yellow_pose is None and vc["yellow"] is not None:
                yellow_pose = vc["yellow"]
            if amber_pose is None and vc["amber"] is not None:
                amber_pose = vc["amber"]
            if crimson_pose is None and vc["crimson"] is not None:
                crimson_pose = vc["crimson"]
            if yellow_pose and amber_pose and crimson_pose:
                break

        if yellow_pose is None or amber_pose is None or crimson_pose is None:
            raise AssertionError(
                f"Failed discovering settled vein states: yellow={yellow_pose}, amber={amber_pose}, crimson={crimson_pose}"
            )

        poses["yellow_vein_drop"] = {
            "contract": yellow_pose["contract"],
            "z": yellow_pose["z"],
            "h": 0.0,
            "roll": 0.0,
            "pitch": 0.0,
        }
        poses["amber_proximity"] = {
            "contract": amber_pose["contract"],
            "z": amber_pose["z"],
            "h": 0.0,
            "roll": 0.0,
            "pitch": 0.0,
        }
        poses["crimson_intersect"] = {
            "contract": crimson_pose["contract"],
            "z": crimson_pose["z"],
            "h": 0.0,
            "roll": 0.0,
            "pitch": 0.0,
        }

        print(f"[VerifyPhase12C] Discovered settled poses: {json.dumps(poses)}")

        run_remote_checked(remote, REMOTE_SPAWN_CAPTURE_SNIPPET, "Spawn Transient 1280x720 Lit Capture")
        time.sleep(0.5)
        active_settled_contract = [-1]

        def capture_pose_pair(
            tag: str,
            pose: dict,
            charge: float = 0.0,
            ui_shot_path: Optional[str] = None,
        ) -> Tuple[dict, Tuple[int, int, List[Tuple[int, int, int]]], Tuple[int, int, List[Tuple[int, int, int]]]]:
            if active_settled_contract[0] != int(pose["contract"]):
                switch_and_settle_contract(int(pose["contract"]))
                active_settled_contract[0] = int(pose["contract"])

            ap_lines = run_remote_checked(
                remote,
                make_apply_pose_snippet(
                    pose["contract"], pose["z"], pose["h"], pose["roll"], pose["pitch"], charge=charge
                ),
                f"Apply Pose {tag}",
            )
            ap_tel = extract_json_tag(ap_lines, "POSE_APPLIED_JSON=")
            if tag in ("emerald_flat", "beveled_12deg", "flapper_flat", "flapper_bev12", "cyan_flat"):
                if not ap_tel["intersects"]:
                    raise AssertionError(f"Pose {tag} lost monolith intersection after settle: {ap_tel}")
            elif tag == "crimson_hit":
                if not ap_tel["detonate"]:
                    raise AssertionError(f"Pose crimson_hit did not trigger will_detonate after settle: {ap_tel}")
            elif tag == "yellow_vein":
                if not ap_tel["vein_in_half"] or ap_tel["detonate"]:
                    raise AssertionError(f"Pose yellow_vein invalid telemetry after settle: {ap_tel}")
            elif tag == "amber_prox":
                if ap_tel["detonate"] or ap_tel["vein_in_half"] or not (0.38 <= ap_tel["prox"] <= 0.62):
                    raise AssertionError(f"Pose amber_prox invalid telemetry after settle: {ap_tel}")

            time.sleep(0.10)
            f_with = f"_P12C_{tag}_with.png"
            f_clean = f"_P12C_{tag}_clean.png"
            p_with = os.path.join(REPO_ROOT, "Saved", "Logs", f_with)
            p_clean = os.path.join(REPO_ROOT, "Saved", "Logs", f_clean)
            temp_files.extend([p_with, p_clean])
            for p in (p_with, p_clean):
                if os.path.exists(p):
                    os.remove(p)

            cap_lines = run_remote_checked(
                remote,
                make_sync_and_export_pair_snippet(LOGS_DIR, f_with, f_clean),
                f"Export Lit Pair {tag}",
            )
            pts_data = extract_json_tag(cap_lines, "CAPTURE_PTS_JSON=")
            wait_for_file(p_with, 5.0)
            wait_for_file(p_clean, 5.0)

            if ui_shot_path:
                focus_pie_viewport_window()
                time.sleep(0.08)
                run_remote_checked(
                    remote,
                    make_request_composited_screenshot_snippet(ui_shot_path),
                    f"Request UI Screenshot {os.path.basename(ui_shot_path)}",
                )
                wait_for_file(ui_shot_path, 6.0)

            return pts_data, decode_png_rgb8(p_with), decode_png_rgb8(p_clean)

        # Capture all scenarios + 4 required UI-composited screenshots
        pts_em, (w, h, px_em), (_, _, cl_em) = capture_pose_pair(
            "emerald_flat", poses["emerald_flat"], charge=0.0, ui_shot_path=SHOT_1_EMERALD
        )
        pts_bev, (_, _, px_bev), (_, _, cl_bev) = capture_pose_pair(
            "beveled_12deg", poses["beveled_12deg"], charge=0.0
        )
        pts_noc, (_, _, px_noc), (_, _, cl_noc) = capture_pose_pair(
            "no_contact", poses["no_contact"], charge=0.0
        )
        pts_flp_flat, (_, _, px_flp_flat), (_, _, cl_flp_flat) = capture_pose_pair(
            "flapper_flat", poses["flapper_flat"], charge=0.0
        )
        pts_flp_bev, (_, _, px_flp_bev), (_, _, cl_flp_bev) = capture_pose_pair(
            "flapper_bev12", poses["flapper_beveled_12deg"], charge=0.0, ui_shot_path=SHOT_2_FLAPPER
        )
        pts_jaw, (_, _, px_jaw), (_, _, cl_jaw) = capture_pose_pair(
            "jaw_overlap", poses["jaw_overlap"], charge=0.0
        )
        pts_cy, (_, _, px_cy), (_, _, cl_cy) = capture_pose_pair(
            "cyan_flat", poses["cyan_flat"], charge=0.0
        )
        pts_yel, (_, _, px_yel), (_, _, cl_yel) = capture_pose_pair(
            "yellow_vein", poses["yellow_vein_drop"], charge=0.0, ui_shot_path=SHOT_3_YELLOW
        )
        pts_amb, (_, _, px_amb), (_, _, cl_amb) = capture_pose_pair(
            "amber_prox", poses["amber_proximity"], charge=0.0
        )
        pts_crim, (_, _, px_crim), (_, _, cl_crim) = capture_pose_pair(
            "crimson_hit", poses["crimson_intersect"], charge=0.0, ui_shot_path=SHOT_4_CRIMSON
        )

        # Capacitor charge surge progression (0.0 -> 0.45 -> 0.85) on cyan_flat
        pts_ch45, (_, _, px_ch45), (_, _, cl_ch45) = capture_pose_pair(
            "charge_45", poses["cyan_flat"], charge=0.45
        )
        pts_ch85, (_, _, px_ch85), (_, _, cl_ch85) = capture_pose_pair(
            "charge_85", poses["cyan_flat"], charge=0.85
        )
        # Reset capacitor charge to 0.0 and sample steady unthrottled PIE frame times
        run_remote_checked(
            remote,
            make_apply_pose_snippet(
                poses["emerald_flat"]["contract"],
                poses["emerald_flat"]["z"],
                0.0,
                0.0,
                0.0,
                charge=0.0,
            ),
            "Restore Clean Active Pose for Steady Frame Timing",
        )
        focus_pie_viewport_window()
        time.sleep(0.8)
        frame_samples_ms: List[float] = []
        for s_i in range(6):
            time.sleep(0.18)
            fl = run_remote_checked(remote, REMOTE_MEASURE_STEADY_FRAME_TIME_SNIPPET, f"Frame Sample #{s_i + 1}")
            frame_samples_ms.append(float(extract_json_tag(fl, "FRAME_MS_JSON=")["delta_ms"]))
        steady_median_ms = statistics.median(frame_samples_ms)

        run_remote_checked(remote, REMOTE_CLEANUP_AND_END_PIE_SNIPPET, "Cleanup & End PIE")
        time.sleep(0.5)

        # =====================================================================
        # Evaluate Quantitative Gates
        # =====================================================================
        # Gate 1: Flat 0 deg & Beveled/Rolled 12 deg incandescent seam + 0 seam in NO CONTACT
        seam_flat = measure_seam_metrics(w, h, px_em, cl_em, pts_em["seam_pts"], ry=5)
        seam_bev12 = measure_seam_metrics(w, h, px_bev, cl_bev, pts_bev["seam_pts"], ry=5)
        seam_noc_check = measure_false_molten_glow(w, h, px_noc, cl_noc, pts_noc["interior_pts"], ry=1)

        # Gate 2: SM_DiverterFlapper & SM_ClampJaw overlap immunity
        flp_mono_flat = measure_seam_metrics(w, h, px_flp_flat, cl_flp_flat, pts_flp_flat["seam_pts"], ry=5)
        flp_false_flat = measure_false_molten_glow(w, h, px_flp_flat, cl_flp_flat, pts_flp_flat["flapper_pts"], ry=3)
        flp_mono_bev = measure_seam_metrics(w, h, px_flp_bev, cl_flp_bev, pts_flp_bev["seam_pts"], ry=5)
        flp_false_bev = measure_false_molten_glow(w, h, px_flp_bev, cl_flp_bev, pts_flp_bev["flapper_pts"], ry=3)
        jaw_false = measure_false_molten_glow(w, h, px_jaw, cl_jaw, pts_jaw["jaw_pts"], ry=3)

        # Gate 3: 5-State Perimeter RGB Signatures & Subdued 0.08..0.12 Interior Scanlines
        sig_cyan = measure_perimeter_signature(w, h, px_cy, cl_cy, pts_cy["border_pts"])
        sig_emerald = measure_perimeter_signature(w, h, px_em, cl_em, pts_em["border_pts"])
        sig_yellow = measure_perimeter_signature(w, h, px_yel, cl_yel, pts_yel["border_pts"])
        sig_amber = measure_perimeter_signature(w, h, px_amb, cl_amb, pts_amb["border_pts"])
        sig_crimson = measure_perimeter_signature(w, h, px_crim, cl_crim, pts_crim["border_pts"])
        seam_crimson = measure_seam_metrics(w, h, px_crim, cl_crim, pts_crim["seam_pts"], ry=5)

        interior_em = measure_interior_scanline_metrics(w, h, px_em, cl_em, pts_em["interior_pts"])
        interior_cy = measure_interior_scanline_metrics(w, h, px_cy, cl_cy, pts_cy["interior_pts"])

        state_sigs = {
            "Cyan": sig_cyan,
            "Emerald": sig_emerald,
            "Yellow": sig_yellow,
            "Amber": sig_amber,
            "Crimson": sig_crimson,
        }
        min_pair_dist = 9999.0
        min_pair_names = ("", "")
        names = list(state_sigs.keys())
        for i in range(len(names)):
            for j in range(i + 1, len(names)):
                c1, c2 = state_sigs[names[i]], state_sigs[names[j]]
                dist = math.sqrt((c1[0] - c2[0]) ** 2 + (c1[1] - c2[1]) ** 2 + (c1[2] - c2[2]) ** 2)
                if dist < min_pair_dist:
                    min_pair_dist = dist
                    min_pair_names = (names[i], names[j])

        # Gate 4: Capacitor Charge Surge, Performance & Binary Hygiene
        seam_ch0 = measure_seam_metrics(w, h, px_cy, cl_cy, pts_cy["seam_pts"], ry=5)
        seam_ch45 = measure_seam_metrics(w, h, px_ch45, cl_ch45, pts_ch45["seam_pts"], ry=5)
        seam_ch85 = measure_seam_metrics(w, h, px_ch85, cl_ch85, pts_ch85["seam_pts"], ry=5)
        int_ch0 = interior_cy["mean_bg_lift_srgb"]
        int_ch45 = measure_interior_scanline_metrics(w, h, px_ch45, cl_ch45, pts_ch45["interior_pts"])["mean_bg_lift_srgb"]
        int_ch85 = measure_interior_scanline_metrics(w, h, px_ch85, cl_ch85, pts_ch85["interior_pts"])["mean_bg_lift_srgb"]

        maps_status = subprocess.check_output(
            ["git", "status", "--short", "Content/Maps/"], cwd=REPO_ROOT, text=True
        ).strip()
        da_status = subprocess.check_output(
            ["git", "status", "--short", "Content/Presentation/DA_Presentation.uasset"], cwd=REPO_ROOT, text=True
        ).strip()

        print("\n================ PHASE 12C QUANTITATIVE 4-GATE REPORT ================")
        print(
            f"  [Contracts Audit] All 6 contracts cuttable_height_cm="
            f"{[c['cuttable_height_cm'] for c in contracts_audit['contracts']]} (all > 10.0 cm)"
        )
        print(
            f"  [Gate 1] Incandescent Monolith Seam: flat_0deg peak_R={seam_flat['peak_r']} sRGB "
            f"rgb={seam_flat['best_rgb']} warm_px={seam_flat['warm_incandescent_px']} | "
            f"beveled_12deg peak_R={seam_bev12['peak_r']} sRGB rgb={seam_bev12['best_rgb']} "
            f"warm_px={seam_bev12['warm_incandescent_px']} | "
            f"no_contact molten_px={seam_noc_check['molten_false_px']}"
        )
        print(
            f"  [Gate 2] Overlap Immunity: flapper_flat(mono_peak_R={flp_mono_flat['peak_r']}, "
            f"flapper_molten_px={flp_false_flat['molten_false_px']}, flapper_max_dR={flp_false_flat['max_delta_r']}, "
            f"n={flp_false_flat['sample_count']}) | "
            f"flapper_12deg(mono_peak_R={flp_mono_bev['peak_r']}, "
            f"flapper_molten_px={flp_false_bev['molten_false_px']}, flapper_max_dR={flp_false_bev['max_delta_r']}) | "
            f"clamp_jaw(jaw_molten_px={jaw_false['molten_false_px']}, jaw_max_dR={jaw_false['max_delta_r']}, "
            f"n={jaw_false['sample_count']})"
        )
        print(
            f"  [Gate 3] 5-State Perimeter RGB: Cyan={sig_cyan}, Emerald={sig_emerald}, "
            f"Yellow={sig_yellow}, Amber={sig_amber}, Crimson={sig_crimson} "
            f"(min_pair_dist={min_pair_dist:.1f} sRGB between {min_pair_names[0]} & {min_pair_names[1]}) | "
            f"Crimson Arc Seam rgb={seam_crimson['best_rgb']} (arc_px={seam_crimson['crimson_arc_px']}) | "
            f"Interior Scanlines: emerald_lift=+{interior_em['mean_bg_lift_srgb']} sRGB "
            f"(std={interior_em['std_dom_srgb']}), cyan_lift=+{interior_cy['mean_bg_lift_srgb']} sRGB "
            f"(std={interior_cy['std_dom_srgb']})"
        )
        print(
            f"  [Gate 4] Charge Surge & Budget: seam_energy(0.0->{seam_ch0['total_energy']}, "
            f"0.45->{seam_ch45['total_energy']}, 0.85->{seam_ch85['total_energy']}), "
            f"interior_lift(0.0->+{int_ch0}, 0.45->+{int_ch45}, 0.85->+{int_ch85}) | "
            f"steady_frame_ms={steady_median_ms:.2f} ms (samples={frame_samples_ms}) | "
            f"maps_diff={maps_status!r} | da_presentation_diff={da_status!r}"
        )
        for sp in (SHOT_1_EMERALD, SHOT_2_FLAPPER, SHOT_3_YELLOW, SHOT_4_CRIMSON):
            print(f"  [Screenshot] {sp} ({os.path.getsize(sp)} bytes)")
        print("======================================================================\n")

        failures: List[str] = []
        # Gate 1 assertions
        if seam_flat["peak_r"] < 215 or seam_flat["warm_incandescent_px"] < 4:
            failures.append(f"Gate 1 flat seam failed: {seam_flat}")
        if seam_bev12["peak_r"] < 215 or seam_bev12["warm_incandescent_px"] < 4:
            failures.append(f"Gate 1 beveled 12deg seam failed: {seam_bev12}")
        if seam_noc_check["molten_false_px"] != 0:
            failures.append(f"Gate 1 NO CONTACT had false molten pixels: {seam_noc_check}")

        # Gate 2 assertions
        if flp_mono_flat["peak_r"] < 215 or flp_mono_bev["peak_r"] < 215:
            failures.append(
                f"Gate 2 monolith seam missing during flapper overlap: flat={flp_mono_flat}, bev={flp_mono_bev}"
            )
        if flp_false_flat["sample_count"] < 15 or flp_false_flat["molten_false_px"] != 0:
            failures.append(f"Gate 2 SM_DiverterFlapper flat overlap immunity failed: {flp_false_flat}")
        if flp_false_bev["sample_count"] < 10 or flp_false_bev["molten_false_px"] != 0:
            failures.append(f"Gate 2 SM_DiverterFlapper 12deg overlap immunity failed: {flp_false_bev}")
        if jaw_false["sample_count"] < 15 or jaw_false["molten_false_px"] != 0:
            failures.append(f"Gate 2 SM_ClampJaw prong immunity failed: {jaw_false}")

        # Gate 3 assertions
        if min_pair_dist < 22.0:
            failures.append(
                f"Gate 3 5-state perimeter separation too small ({min_pair_dist:.1f} between {min_pair_names}): {state_sigs}"
            )
        if not (sig_cyan[2] > sig_cyan[0] + 60.0 and sig_cyan[1] > sig_cyan[0] + 50.0):
            failures.append(f"Gate 3 Cyan signature invalid: {sig_cyan}")
        if not (sig_emerald[1] > sig_emerald[0] + 70.0 and sig_emerald[1] > sig_emerald[2] + 35.0):
            failures.append(f"Gate 3 Emerald signature invalid: {sig_emerald}")
        if not (sig_yellow[0] >= 185.0 and sig_yellow[1] >= 165.0 and sig_yellow[1] > sig_amber[1] + 20.0):
            failures.append(f"Gate 3 Yellow vs Amber separation invalid: Yellow={sig_yellow}, Amber={sig_amber}")
        if not (sig_amber[0] >= 185.0 and sig_amber[1] > sig_crimson[1] + 22.0):
            failures.append(f"Gate 3 Amber vs Crimson separation invalid: Amber={sig_amber}, Crimson={sig_crimson}")
        if seam_crimson["peak_r"] < 215 or seam_crimson["crimson_arc_px"] < 4:
            failures.append(f"Gate 3 Crimson VEIN INTERSECT arc seam failed: {seam_crimson}")
        if interior_em["mean_bg_lift_srgb"] > 18.0 or interior_cy["mean_bg_lift_srgb"] > 18.0:
            failures.append(f"Gate 3 interior mean bg lift > +18 sRGB: em={interior_em}, cy={interior_cy}")
        if interior_em["std_dom_srgb"] <= 1.2 or interior_cy["std_dom_srgb"] <= 1.2:
            failures.append(f"Gate 3 interior scanline std <= 1.2 sRGB: em={interior_em}, cy={interior_cy}")

        # Gate 4 assertions
        if not (seam_ch85["total_energy"] > seam_ch45["total_energy"] > seam_ch0["total_energy"]):
            failures.append(
                f"Gate 4 non-monotonic seam charge surge: 0={seam_ch0['total_energy']}, "
                f"0.45={seam_ch45['total_energy']}, 0.85={seam_ch85['total_energy']}"
            )
        if not (int_ch85 > int_ch45 > int_ch0):
            failures.append(f"Gate 4 non-monotonic scanline charge surge: 0={int_ch0}, 0.45={int_ch45}, 0.85={int_ch85}")
        if steady_median_ms > 13.0:
            failures.append(f"Gate 4 steady PIE frame time {steady_median_ms:.2f} ms > 13.0 ms")
        if maps_status != "" or da_status != "":
            failures.append(f"Gate 4 binary hygiene violated: maps={maps_status!r}, da={da_status!r}")

        if failures:
            raise AssertionError("PHASE 12C GATE FAILURES:\n  - " + "\n  - ".join(failures))

        print("[VerifyPhase12C] ALL 4 QUANTITATIVE GATES PASSED WITH ZERO FAILURES.")
        return 0
    finally:
        for tf in temp_files:
            if os.path.exists(tf):
                try:
                    os.remove(tf)
                except Exception:
                    pass
        if args.shutdown or spawned_proc is not None:
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
