import sys

sys.dont_write_bytecode = True

import argparse
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
SCREENSHOTS_DIR = os.path.join(REPO_ROOT, "Saved", "Screenshots", "Phase12B")

HUD_ACTIVE_PNG = os.path.join(SCREENSHOTS_DIR, "Phase12B_ActiveInspection_HUD.png")
HUD_FAULT_PNG = os.path.join(SCREENSHOTS_DIR, "Phase12B_FaultSlam_HUD.png")

LIT_FULL_FILENAME = "Phase12B_ActiveInspection_LitCapture.png"
LIT_CLEAN_FILENAME = "_Phase12B_ActiveInspection_CleanProbe.png"
LIT_FAULT_BASE = "_Phase12B_FaultStrobe"
LIT_FAULT_FINAL_FILENAME = "Phase12B_FaultSlam_LitCapture.png"

LIT_FULL_PATH = os.path.join(REPO_ROOT, "Saved", "Logs", LIT_FULL_FILENAME)
LIT_CLEAN_PATH = os.path.join(REPO_ROOT, "Saved", "Logs", LIT_CLEAN_FILENAME)
LIT_FAULT_FINAL_PATH = os.path.join(REPO_ROOT, "Saved", "Logs", LIT_FAULT_FINAL_FILENAME)


REMOTE_BEGIN_PIE_SNIPPET = """
import unreal

try:
    ed_perf = unreal.get_default_object(unreal.EditorPerformanceSettings)
    ed_perf.set_editor_property("throttle_cpu_when_not_foreground", False)
    unreal.log(f"[VerifyPhase12B] throttle_cpu_when_not_foreground={ed_perf.get_editor_property('throttle_cpu_when_not_foreground')}")
except Exception as exc:
    unreal.log_warning(f"[VerifyPhase12B] EditorPerformanceSettings warning: {exc}")

unreal.SystemLibrary.execute_console_command(None, "r.VSync 0")
unreal.SystemLibrary.execute_console_command(None, "r.VSyncEditor 0")
unreal.SystemLibrary.execute_console_command(None, "Slate.MaxFPS 240")
unreal.SystemLibrary.execute_console_command(None, "t.MaxFPS 240")
unreal.SystemLibrary.execute_console_command(None, "t.UnfocusedFrameRateLimit 0")

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les.is_in_play_in_editor():
    les.editor_request_end_play()
les.editor_request_begin_play()
unreal.log("[VerifyPhase12B] Requested BeginPlay PIE session.")
"""


REMOTE_AUDIT_PIE_HYGIENE_AND_CLEARANCE_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
if pie_world is None:
    raise RuntimeError("PIE GameWorld is None; PIE not ready yet.")

world_time_sec = float(unreal.GameplayStatics.get_time_seconds(pie_world))
if world_time_sec < 1.15:
    raise RuntimeError(f"PIE warmup in progress (world_time={world_time_sec:.3f}s < 1.15s)...")

unreal.SystemLibrary.execute_console_command(pie_world, "r.VSync 0")
unreal.SystemLibrary.execute_console_command(pie_world, "r.VSyncEditor 0")
unreal.SystemLibrary.execute_console_command(pie_world, "Slate.MaxFPS 240")
unreal.SystemLibrary.execute_console_command(pie_world, "t.MaxFPS 240")

all_actors = unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor)

shadow_lights = []
zero_shadow_lights = []
fog_comps = []
volumetric_fog_enabled = False
booths = []
framings = []
pawns = []
rigs = []

for actor in all_actors:
    cls_name = actor.get_class().get_name()
    if "DirectorBoothActor" in cls_name:
        booths.append(actor)
    elif "CellFramingArchitectureActor" in cls_name:
        framings.append(actor)
    elif "InspectorPawn" in cls_name:
        pawns.append(actor)
    elif "MonolithRigActor" in cls_name:
        rigs.append(actor)

    for lc in actor.get_components_by_class(unreal.LightComponentBase):
        entry = f"{cls_name}.{lc.get_name()}(int={float(lc.get_editor_property('intensity')):.1f})"
        if bool(lc.get_editor_property("cast_shadows")):
            shadow_lights.append(entry)
        else:
            zero_shadow_lights.append(entry)

    for fog in actor.get_components_by_class(unreal.ExponentialHeightFogComponent):
        fog_comps.append(fog.get_name())
        if bool(fog.get_editor_property("enable_volumetric_fog")):
            volumetric_fog_enabled = True

if len(booths) != 1:
    raise AssertionError(f"Expected exactly 1 ADirectorBoothActor, found {len(booths)}")
if len(framings) != 1:
    raise AssertionError(f"Expected exactly 1 ACellFramingArchitectureActor (Correction D), found {len(framings)}")
if len(pawns) != 1 or len(rigs) != 1:
    raise AssertionError(f"Expected 1 InspectorPawn and 1 MonolithRigActor, found {len(pawns)}, {len(rigs)}")

if len(shadow_lights) != 1:
    raise AssertionError(f"Expected shadow_lights == 1, got {len(shadow_lights)}: {shadow_lights}")
if len(zero_shadow_lights) != 10:
    raise AssertionError(f"Decision 2 violated: expected zero_shadow_lights == 10 (0 new lights), got {len(zero_shadow_lights)}: {zero_shadow_lights}")
if volumetric_fog_enabled:
    raise AssertionError(f"VolumetricFog hard ban violated: fog_comps={fog_comps}, enable_volumetric_fog={volumetric_fog_enabled}")

booth = booths[0]
framing = framings[0]

def assert_zero_cost_flags(comp, label):
    if comp is None:
        raise AssertionError(f"Missing required Phase 12B component: {label}")
    if comp.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION:
        raise AssertionError(f"{label} collision must be NoCollision!")
    if bool(comp.get_editor_property("generate_overlap_events")):
        raise AssertionError(f"{label} GenerateOverlapEvents must be False!")
    if bool(comp.get_editor_property("cast_shadow")):
        raise AssertionError(f"{label} CastShadow must be False!")
    if bool(comp.get_editor_property("affect_distance_field_lighting")):
        raise AssertionError(f"{label} bAffectDistanceFieldLighting must be False!")
    if bool(comp.get_editor_property("affect_dynamic_indirect_lighting")):
        raise AssertionError(f"{label} bAffectDynamicIndirectLighting must be False!")
    if bool(comp.get_editor_property("visible_in_ray_tracing")):
        raise AssertionError(f"{label} bVisibleInRayTracing must be False!")

louver_ism = booth.get_editor_property("panopticon_louver_slats_ism")
dir_cubes_ism = booth.get_editor_property("director_silhouette_cubes_ism")
dir_cyls_ism = booth.get_editor_property("director_silhouette_cylinders_ism")
left_card = booth.get_editor_property("left_window_glow_card")
center_card = booth.get_editor_property("center_window_glow_card")
right_card = booth.get_editor_property("right_window_glow_card")

outer_cols_ism = framing.get_editor_property("outer_heavy_columns_ism")
outer_pipes_ism = framing.get_editor_property("outer_flanged_pipes_ism")
outer_ribs_ism = framing.get_editor_property("outer_structural_ribs_ism")

for c_obj, c_lbl in (
    (louver_ism, "PanopticonLouverSlatsISM"),
    (dir_cubes_ism, "DirectorSilhouetteCubesISM"),
    (dir_cyls_ism, "DirectorSilhouetteCylindersISM"),
    (left_card, "LeftWindowGlowCard"),
    (center_card, "CenterWindowGlowCard"),
    (right_card, "RightWindowGlowCard"),
    (outer_cols_ism, "OuterHeavyColumnsISM"),
    (outer_pipes_ism, "OuterFlangedPipesISM"),
    (outer_ribs_ism, "OuterStructuralRibsISM"),
):
    assert_zero_cost_flags(c_obj, c_lbl)

louver_count = int(booth.get_louver_slat_instance_count())
dir_count = int(booth.get_director_silhouette_instance_count())
framing_count = int(framing.get_total_framing_instance_count())
if louver_count < 12 or dir_count < 8 or framing_count < 16:
    raise AssertionError(f"Unexpected instance counts: louvers={louver_count}, director={dir_count}, framing={framing_count}")

# Verify BoothRoot-local AABBs for Correction A (cavity Y/Z bounds & >= 0.85 cm X air gaps) and Decision 1 (Right-Bay exclusive Director)
booth_inv_xf = booth.get_actor_transform().inverse()

def mesh_local_aabb(smc):
    sm = smc.get_editor_property("static_mesh")
    bb = sm.get_bounding_box()
    wxf = smc.get_world_transform()
    pts = []
    for bx in (bb.min.x, bb.max.x):
        for by in (bb.min.y, bb.max.y):
            for bz in (bb.min.z, bb.max.z):
                wp = unreal.MathLibrary.transform_location(wxf, unreal.Vector(bx, by, bz))
                lp = unreal.MathLibrary.transform_location(booth_inv_xf, wp)
                pts.append((float(lp.x), float(lp.y), float(lp.z)))
    return (
        [min(p[0] for p in pts), min(p[1] for p in pts), min(p[2] for p in pts)],
        [max(p[0] for p in pts), max(p[1] for p in pts), max(p[2] for p in pts)],
    )

def ism_instance_local_aabbs(ism):
    sm = ism.get_editor_property("static_mesh")
    bb = sm.get_bounding_box()
    boxes = []
    for idx in range(ism.get_instance_count()):
        ixf = ism.get_instance_transform(idx, True)
        pts = []
        for bx in (bb.min.x, bb.max.x):
            for by in (bb.min.y, bb.max.y):
                for bz in (bb.min.z, bb.max.z):
                    wp = unreal.MathLibrary.transform_location(ixf, unreal.Vector(bx, by, bz))
                    lp = unreal.MathLibrary.transform_location(booth_inv_xf, wp)
                    pts.append((float(lp.x), float(lp.y), float(lp.z)))
        boxes.append((
            [min(p[0] for p in pts), min(p[1] for p in pts), min(p[2] for p in pts)],
            [max(p[0] for p in pts), max(p[1] for p in pts), max(p[2] for p in pts)],
        ))
    return boxes

def union_boxes(b_list):
    return (
        [min(b[0][0] for b in b_list), min(b[0][1] for b in b_list), min(b[0][2] for b in b_list)],
        [max(b[1][0] for b in b_list), max(b[1][1] for b in b_list), max(b[1][2] for b in b_list)],
    )

c_shutter_box = mesh_local_aabb(booth.get_editor_property("blast_shutter_visual"))
l_shutter_box = mesh_local_aabb(booth.get_editor_property("left_blast_shutter_visual"))
r_shutter_box = mesh_local_aabb(booth.get_editor_property("right_blast_shutter_visual"))

l_card_box = mesh_local_aabb(left_card)
c_card_box = mesh_local_aabb(center_card)
r_card_box = mesh_local_aabb(right_card)

all_louvers = ism_instance_local_aabbs(louver_ism)
left_louvers = [b for b in all_louvers if 0.5 * (b[0][1] + b[1][1]) < -80.0]
center_louvers = [b for b in all_louvers if abs(0.5 * (b[0][1] + b[1][1])) <= 80.0]
right_louvers = [b for b in all_louvers if 0.5 * (b[0][1] + b[1][1]) > 80.0]

l_louver_box = union_boxes(left_louvers)
c_louver_box = union_boxes(center_louvers)
r_louver_box = union_boxes(right_louvers)

dir_boxes = ism_instance_local_aabbs(dir_cubes_ism) + ism_instance_local_aabbs(dir_cyls_ism)
dir_union = union_boxes(dir_boxes)

# Decision 1: 100% of Watching Director instances exclusively inside Right Bay cavity
if dir_union[0][1] < 124.5 or dir_union[1][1] > 251.0 or dir_union[0][2] < -13.55 or dir_union[1][2] > 25.65:
    raise AssertionError(f"Decision 1 / Right-Bay Director cavity bounds violated: {dir_union}")

# Correction A: Center Bay Y in [-68.0, +68.0] (Width <= 136 cm), Z in [-5.5, +26.5]
for cname, cbox in (("CenterLouvers", c_louver_box), ("CenterWindowGlowCard", c_card_box)):
    if cbox[0][1] < -68.05 or cbox[1][1] > 68.05 or cbox[0][2] < -5.55 or cbox[1][2] > 26.55:
        raise AssertionError(f"Correction A Center Bay bezel/sill violation on {cname}: {cbox}")

# Correction A: Flank Bay Y in +-[124.5, 251.0], Z in [-13.5, +25.6]
for fname, fbox, sign in (
    ("LeftLouvers", l_louver_box, -1),
    ("LeftWindowGlowCard", l_card_box, -1),
    ("RightLouvers", r_louver_box, 1),
    ("RightWindowGlowCard", r_card_box, 1),
):
    y_abs_min = min(abs(fbox[0][1]), abs(fbox[1][1]))
    y_abs_max = max(abs(fbox[0][1]), abs(fbox[1][1]))
    if y_abs_min < 124.45 or y_abs_max > 251.05 or fbox[0][2] < -13.55 or fbox[1][2] > 25.65:
        raise AssertionError(f"Correction A Flank Bay cavity violation on {fname}: {fbox}")

# Correction A: >= 8.5 mm (0.85 cm) X air gaps across all 3 bays against shutter travel planes and solid frame cavity walls (+5.658 center, +10.05 flanks)
gaps_cm = {
    "center_shutter_to_louver": c_louver_box[0][0] - c_shutter_box[1][0],
    "center_louver_to_card": c_card_box[0][0] - c_louver_box[1][0],
    "center_card_to_frame_wall": 5.658 - c_card_box[1][0],
    "left_shutter_to_louver": l_louver_box[0][0] - l_shutter_box[1][0],
    "left_louver_to_card": l_card_box[0][0] - l_louver_box[1][0],
    "left_card_to_frame_wall": 10.050 - l_card_box[1][0],
    "right_shutter_to_louver": r_louver_box[0][0] - r_shutter_box[1][0],
    "right_louver_to_director": dir_union[0][0] - r_louver_box[1][0],
    "right_director_to_card": r_card_box[0][0] - dir_union[1][0],
    "right_card_to_frame_wall": 10.050 - r_card_box[1][0],
}
for gap_name, gap_val in gaps_cm.items():
    if gap_val < 0.85:
        raise AssertionError(f"Correction A X-clearance < 0.85 cm ({gap_val:.3f} cm) at {gap_name}!")

delta_ms = float(unreal.GameplayStatics.get_world_delta_seconds(pie_world)) * 1000.0
shutter_alpha = float(booth.get_current_shutter_open_alpha())
if shutter_alpha < 0.98:
    raise AssertionError(f"Expected open shutters (>=0.98) in ActiveInspection, got {shutter_alpha:.3f}")

audit_payload = {
    "world_time_sec": round(world_time_sec, 3),
    "delta_ms": round(delta_ms, 3),
    "shadow_lights": len(shadow_lights),
    "zero_shadow_lights": len(zero_shadow_lights),
    "volumetric_fog": volumetric_fog_enabled,
    "louver_instances": louver_count,
    "director_instances": dir_count,
    "outer_framing_instances": framing_count,
    "min_x_gap_cm": round(min(gaps_cm.values()), 3),
    "gaps_cm": {k: round(v, 3) for k, v in gaps_cm.items()},
    "open_flank_shutter_min_z": round(r_shutter_box[0][2], 3),
    "director_local_aabb": [[round(v, 2) for v in dir_union[0]], [round(v, 2) for v in dir_union[1]]],
}
unreal.log("AUDIT_JSON=" + json.dumps(audit_payload))
"""


def make_request_composited_screenshot_snippet(abs_png_path: str) -> str:
    return f"""
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
pawns = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()]
pawn = pawns[0]
hud_bridge = pawn.get_component_by_class(unreal.InspectorHudBridgeComponent)
if hud_bridge is None or not bool(hud_bridge.is_overlay_mounted()):
    raise AssertionError("InspectorHudBridgeComponent missing or STareTitheHudOverlay not mounted!")

ok = bool(hud_bridge.request_composited_viewport_screenshot({abs_png_path!r}))
if not ok:
    raise RuntimeError("RequestCompositedViewportScreenshot returned False!")
unreal.log(f"[VerifyPhase12B] Requested composited UI screenshot -> {abs_png_path}")
"""


REMOTE_SPAWN_TRANSIENT_CAPTURE_SNIPPET = """
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
cam = pawn.get_editor_property("inspector_camera")

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
unreal.log("[VerifyPhase12B] Spawned transient 1280x720 Lumen SceneCapture2D.")
"""


def make_export_active_full_and_hide_laser_snippet(logs_dir: str, full_filename: str) -> str:
    return f"""
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
caps = [
    a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.SceneCapture2D)
    if unreal.Name("Phase11AFixTransientLitCapture") in list(a.tags)
]
cap_actor = caps[0]
scc = cap_actor.get_editor_property("capture_component2d")
rt = scc.get_editor_property("texture_target")

unreal.RenderingLibrary.export_render_target(pie_world, rt, {logs_dir!r}, {full_filename!r})

for actor in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor):
    for tc in actor.get_components_by_class(unreal.TextRenderComponent):
        scc.hide_component(tc)
    if "InspectorPawn" in actor.get_class().get_name():
        for prop_name in (
            "laser_sheet_visual",
            "laser_sheet_underside_visual",
            "laser_frame_front_bar",
            "laser_frame_back_bar",
            "laser_frame_left_bar",
            "laser_frame_right_bar",
        ):
            comp = actor.get_editor_property(prop_name)
            if comp is not None:
                scc.hide_component(comp)
unreal.log("[VerifyPhase12B] Exported full Lit capture and hid laser overlay for clean ROI readback.")
"""


def make_export_clean_and_project_rois_snippet(logs_dir: str, clean_filename: str) -> str:
    return f"""
import json
import math
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
caps = [
    a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.SceneCapture2D)
    if unreal.Name("Phase11AFixTransientLitCapture") in list(a.tags)
]
cap_actor = caps[0]
scc = cap_actor.get_editor_property("capture_component2d")
rt = scc.get_editor_property("texture_target")

unreal.RenderingLibrary.export_render_target(pie_world, rt, {logs_dir!r}, {clean_filename!r})

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
    return px, py

def project_world_box(min_v, max_v, shrink_x=0.0, shrink_y=0.0):
    xs, ys = [], []
    for x in (min_v[0], max_v[0]):
        for y in (min_v[1], max_v[1]):
            for z in (min_v[2], max_v[2]):
                p = project_pt(unreal.Vector(x, y, z))
                if p is not None:
                    xs.append(p[0])
                    ys.append(p[1])
    x0, x1 = max(0, min(xs)), min(W, max(xs))
    y0, y1 = max(0, min(ys)), min(H, max(ys))
    dx = int(round((x1 - x0) * shrink_x))
    dy = int(round((y1 - y0) * shrink_y))
    return [max(0, x0 + dx), max(0, y0 + dy), min(W, x1 - dx), min(H, y1 - dy)]

def project_booth_local_box(booth_xf, min_l, max_l, shrink_x=0.0, shrink_y=0.0):
    xs, ys = [], []
    for x in (min_l[0], max_l[0]):
        for y in (min_l[1], max_l[1]):
            for z in (min_l[2], max_l[2]):
                wp = unreal.MathLibrary.transform_location(booth_xf, unreal.Vector(x, y, z))
                p = project_pt(wp)
                if p is not None:
                    xs.append(p[0])
                    ys.append(p[1])
    x0, x1 = max(0, min(xs)), min(W, max(xs))
    y0, y1 = max(0, min(ys)), min(H, max(ys))
    dx = int(round((x1 - x0) * shrink_x))
    dy = int(round((y1 - y0) * shrink_y))
    return [max(0, x0 + dx), max(0, y0 + dy), min(W, x1 - dx), min(H, y1 - dy)]

def comp_world_aabb(comp):
    sm = comp.get_editor_property("static_mesh")
    bb = sm.get_bounding_box()
    xf = comp.get_world_transform()
    pts = [
        unreal.MathLibrary.transform_location(xf, unreal.Vector(bx, by, bz))
        for bx in (bb.min.x, bb.max.x)
        for by in (bb.min.y, bb.max.y)
        for bz in (bb.min.z, bb.max.z)
    ]
    return (
        [min(float(p.x) for p in pts), min(float(p.y) for p in pts), min(float(p.z) for p in pts)],
        [max(float(p.x) for p in pts), max(float(p.y) for p in pts), max(float(p.z) for p in pts)],
    )

booth = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "DirectorBoothActor" in a.get_class().get_name()][0]
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]
booth_xf = booth.get_actor_transform()
booth_inv_xf = booth_xf.inverse()

# Measure exact open shutter bottom pixel Y so window slit ROIs exclude the hoisted blast shutter bottom lip
l_sh_w = comp_world_aabb(booth.get_editor_property("left_blast_shutter_visual"))
r_sh_w = comp_world_aabb(booth.get_editor_property("right_blast_shutter_visual"))
c_sh_w = comp_world_aabb(booth.get_editor_property("blast_shutter_visual"))

roi_left_shutter = project_world_box(l_sh_w[0], l_sh_w[1], shrink_x=0.04, shrink_y=0.04)
roi_right_shutter = project_world_box(r_sh_w[0], r_sh_w[1], shrink_x=0.04, shrink_y=0.04)
roi_center_shutter_full = project_world_box(c_sh_w[0], c_sh_w[1], shrink_x=0.04, shrink_y=0.04)

# Exclude foreground hero ClampSpringHousingVisual (X=260) occluding the middle of CenterShutter (X=415)
clamp_housing_w = comp_world_aabb(rig.get_editor_property("clamp_spring_housing_visual"))
fg_clamp_px = project_world_box(clamp_housing_w[0], clamp_housing_w[1])
roi_center_shutter_left = [
    roi_center_shutter_full[0],
    roi_center_shutter_full[1],
    max(roi_center_shutter_full[0] + 4, fg_clamp_px[0] - 4),
    roi_center_shutter_full[3],
]
roi_center_shutter_right = [
    min(roi_center_shutter_full[2] - 4, fg_clamp_px[2] + 4),
    roi_center_shutter_full[1],
    roi_center_shutter_full[2],
    roi_center_shutter_full[3],
]

roi_left_slit = project_booth_local_box(booth_xf, [4.0, -246.0, -13.0], [8.8, -130.0, 17.5])
roi_left_slit[1] = max(roi_left_slit[1], roi_left_shutter[3] + 2)

roi_right_slit = project_booth_local_box(booth_xf, [4.0, 130.0, -13.0], [8.8, 246.0, 17.5])
roi_right_slit[1] = max(roi_right_slit[1], roi_right_shutter[3] + 2)

roi_right_director = project_booth_local_box(booth_xf, [1.5, 156.0, -11.5], [7.8, 196.0, 15.5])
roi_right_director[1] = max(roi_right_director[1], roi_right_shutter[3] + 2)

roi_right_glass = project_booth_local_box(booth_xf, [4.0, 215.0, -11.5], [8.8, 245.0, 15.5])
roi_right_glass[1] = max(roi_right_glass[1], roi_right_shutter[3] + 2)

# Slammed bay ROI covering all 3 Panopticon window/shutter cavities for Gate 4 crimson strobe check
roi_slammed_bays = project_booth_local_box(booth_xf, [-16.0, -248.0, -13.0], [8.8, 248.0, 36.0])

tbounds = rig.get_target_world_bounds()
rem_box = (
    [float(tbounds.min.x), float(tbounds.min.y), float(tbounds.min.z)],
    [float(tbounds.max.x), float(tbounds.max.y), float(tbounds.max.z)],
)
roi_monolith = project_world_box(rem_box[0], rem_box[1], shrink_x=0.14, shrink_y=0.08)
roi_monolith_laser = project_world_box(
    [rem_box[0][0] - 25.0, rem_box[0][1] - 45.0, rem_box[0][2] - 4.0],
    [rem_box[1][0] + 25.0, rem_box[1][1] + 45.0, rem_box[1][2] + 4.0],
)

rois = {{
    "left_slit": roi_left_slit,
    "right_slit": roi_right_slit,
    "left_shutter": roi_left_shutter,
    "center_shutter_left": roi_center_shutter_left,
    "center_shutter_right": roi_center_shutter_right,
    "right_shutter": roi_right_shutter,
    "right_director": roi_right_director,
    "right_glass": roi_right_glass,
    "slammed_bays": roi_slammed_bays,
    "monolith": roi_monolith,
    "monolith_laser": roi_monolith_laser,
    "outer_left": [0, int(0.22 * H), int(0.15 * W), int(0.90 * H)],
    "outer_right": [int(0.85 * W), int(0.22 * H), W, int(0.90 * H)],
    "hud_top_left": [int(0.01 * W), int(0.01 * H), int(0.22 * W), int(0.16 * H)],
    "hud_top_right": [int(0.78 * W), int(0.01 * H), int(0.99 * W), int(0.16 * H)],
}}

scc.set_editor_property("capture_every_frame", False)
scc.set_editor_property("texture_target", None)
cap_actor.destroy_actor()

unreal.log("ROI_JSON=" + json.dumps(rois))
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


REMOTE_TRIGGER_CLAMP_SNAP_SNIPPET = """
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
booth = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "DirectorBoothActor" in a.get_class().get_name()][0]
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]

clamp_solver = rig.get_clamp_torque_solver_component()
bounds = rig.get_target_world_bounds()
bcenter = (bounds.min + bounds.max) * 0.5
cx, cy, cz = float(bcenter.x), float(bcenter.y), float(bcenter.z)

aperture = unreal.LaserSliceAperture()
aperture.set_editor_property("enforce_finite_sheet_bounds", False)
steep_right_normal = unreal.Vector(0.0, 0.9659258, -0.2588190)

snapped = False
for y_off in (8.2, 5.4, 2.6, -0.2, -2.8, -5.2):
    rig.commit_slice_plane(unreal.Vector(cx, cy + y_off, cz), steep_right_normal, aperture)
    if bool(clamp_solver.is_clamp_snapped()):
        snapped = True
        break

if not snapped:
    raise AssertionError("Failed to trigger clamp structural snap for Gate 4 fault slam test!")
unreal.log("[VerifyPhase12B] Triggered clamp snap fault; 0.14s 3-shutter slam & 9 Hz crimson strobe initiated.")
"""


def make_verify_fault_slam_and_export_strobe_frame_snippet(
    logs_dir: str,
    frame_filename: str,
    hud_png_path: Optional[str] = None,
) -> str:
    hud_call = ""
    if hud_png_path:
        hud_call = f"""
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
hud_bridge = pawn.get_component_by_class(unreal.InspectorHudBridgeComponent)
hud_bridge.request_composited_viewport_screenshot({hud_png_path!r})
"""
    return f"""
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
booth = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "DirectorBoothActor" in a.get_class().get_name()][0]

alpha_slammed = float(booth.get_current_shutter_open_alpha())
if alpha_slammed > 0.005:
    raise AssertionError(f"Gate 4 FAILED: Expected ShutterOpenAlpha == 0.00 after 0.14s slam, got {{alpha_slammed:.4f}}")

# Re-verify >= 8.5 mm X-clearance in slammed state (alpha == 0.00)
booth_inv_xf = booth.get_actor_transform().inverse()
def smc_max_x(smc):
    bb = smc.get_editor_property("static_mesh").get_bounding_box()
    wxf = smc.get_world_transform()
    xs = []
    for bx in (bb.min.x, bb.max.x):
        for by in (bb.min.y, bb.max.y):
            for bz in (bb.min.z, bb.max.z):
                wp = unreal.MathLibrary.transform_location(wxf, unreal.Vector(bx, by, bz))
                lp = unreal.MathLibrary.transform_location(booth_inv_xf, wp)
                xs.append(float(lp.x))
    return max(xs)

def ism_min_x_in_y(ism, y0, y1):
    bb = ism.get_editor_property("static_mesh").get_bounding_box()
    xs = []
    for idx in range(ism.get_instance_count()):
        ixf = ism.get_instance_transform(idx, True)
        pts = []
        for bx in (bb.min.x, bb.max.x):
            for by in (bb.min.y, bb.max.y):
                for bz in (bb.min.z, bb.max.z):
                    wp = unreal.MathLibrary.transform_location(ixf, unreal.Vector(bx, by, bz))
                    lp = unreal.MathLibrary.transform_location(booth_inv_xf, wp)
                    pts.append((float(lp.x), float(lp.y)))
        yc = 0.5 * (min(p[1] for p in pts) + max(p[1] for p in pts))
        if y0 <= yc <= y1:
            xs.extend(p[0] for p in pts)
    return min(xs)

louver_ism = booth.get_editor_property("panopticon_louver_slats_ism")
slammed_gaps = {{
    "center": ism_min_x_in_y(louver_ism, -80.0, 80.0) - smc_max_x(booth.get_editor_property("blast_shutter_visual")),
    "left": ism_min_x_in_y(louver_ism, -300.0, -80.0) - smc_max_x(booth.get_editor_property("left_blast_shutter_visual")),
    "right": ism_min_x_in_y(louver_ism, 80.0, 300.0) - smc_max_x(booth.get_editor_property("right_blast_shutter_visual")),
}}
for side, g_cm in slammed_gaps.items():
    if g_cm < 0.85:
        raise AssertionError(f"Gate 4 Slammed X-Clearance FAILED on {{side}}: {{g_cm:.3f}} cm < 0.85 cm")

caps = [
    a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.SceneCapture2D)
    if unreal.Name("Phase11AFixTransientLitCapture") in list(a.tags)
]
cap_actor = caps[0]
scc = cap_actor.get_editor_property("capture_component2d")
rt = scc.get_editor_property("texture_target")
unreal.RenderingLibrary.export_render_target(pie_world, rt, {logs_dir!r}, {frame_filename!r})
{hud_call}
lc = booth.get_editor_property("right_bay_point_light")
unreal.log("FAULT_SAMPLE_JSON=" + json.dumps({{
    "shutter_alpha": round(alpha_slammed, 4),
    "slammed_gaps_cm": {{k: round(v, 3) for k, v in slammed_gaps.items()}},
    "right_bay_intensity": round(float(lc.get_editor_property("intensity")), 2),
}}))
"""


REMOTE_DESTROY_TRANSIENT_CAPTURE_SNIPPET = """
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
if pie_world is not None:
    caps = [
        a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.SceneCapture2D)
        if unreal.Name("Phase11AFixTransientLitCapture") in list(a.tags)
    ]
    for cap_actor in caps:
        scc = cap_actor.get_editor_property("capture_component2d")
        scc.set_editor_property("capture_every_frame", False)
        scc.set_editor_property("texture_target", None)
        cap_actor.destroy_actor()
"""


REMOTE_END_PIE_SNIPPET = """
import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les.is_in_play_in_editor():
    les.editor_request_end_play()
unreal.log("[VerifyPhase12B] Cleanly ended PIE session without saving maps/assets.")
"""


REMOTE_QUIT_EDITOR_SNIPPET = """
import unreal
unreal.log("[VerifyPhase12B] Requesting clean Editor exit (zero map/asset save)...")
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


def compute_roi_stats(
    width: int,
    pixels: List[Tuple[int, int, int]],
    boxes: List[List[int]],
) -> Dict[str, float]:
    lumas: List[float] = []
    row_means: List[float] = []
    sum_avg = 0.0
    sum_r = 0.0
    sum_g = 0.0
    sum_b = 0.0
    peak_luma = 0.0
    peak_avg = 0.0
    peak_r = 0.0
    peak_r_b_ratio = 0.0

    for x0, y0, x1, y1 in boxes:
        for y in range(y0, y1):
            row_idx = y * width
            row_sum = 0.0
            row_cnt = 0
            for x in range(x0, x1):
                r, g, b = pixels[row_idx + x]
                luma = 0.2126 * r + 0.7152 * g + 0.0722 * b
                avg = (r + g + b) / 3.0
                lumas.append(luma)
                row_sum += luma
                row_cnt += 1
                sum_avg += avg
                sum_r += r
                sum_g += g
                sum_b += b
                if luma > peak_luma:
                    peak_luma = luma
                if avg > peak_avg:
                    peak_avg = avg
                if r > peak_r or (r == peak_r and (r / max(1.0, float(b))) > peak_r_b_ratio):
                    peak_r = float(r)
                    peak_r_b_ratio = float(r) / max(1.0, float(b))
            if row_cnt > 0:
                row_means.append(row_sum / row_cnt)

    count = len(lumas)
    if count == 0:
        raise RuntimeError(f"Empty ROI pixel box: {boxes}")
    return {
        "count": float(count),
        "mean_luma": sum(lumas) / count,
        "mean_avg": sum_avg / count,
        "peak_luma": peak_luma,
        "peak_avg": peak_avg,
        "std_luma": statistics.pstdev(lumas) if count > 1 else 0.0,
        "row_std_luma": statistics.pstdev(row_means) if len(row_means) > 1 else 0.0,
        "mean_r": sum_r / count,
        "mean_g": sum_g / count,
        "mean_b": sum_b / count,
        "peak_r": peak_r,
        "peak_r_b_ratio": peak_r_b_ratio,
    }


def run_remote_checked(remote: remote_execution.RemoteExecution, code: str, label: str) -> List[str]:
    print(f"[VerifyPhase12B] >>> {label}")
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
            print(f"  [UE] {msg}")
    if not res.get("success", False):
        raise RuntimeError(f"Remote step '{label}' FAILED: {res.get('result')}")
    return lines


def extract_json_tag(lines: List[str], tag: str) -> dict:
    for ln in lines:
        idx = ln.find(tag)
        if idx >= 0:
            return json.loads(ln[idx + len(tag) :])
    raise RuntimeError(f"Missing {tag} in remote output!")


def wait_for_file(path: str, timeout_sec: float = 5.0) -> None:
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        if os.path.exists(path) and os.path.getsize(path) > 4096:
            return
        time.sleep(0.15)
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
                    title = buf.value
                    if "Viewport 2" in title:
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
    parser = argparse.ArgumentParser(
        description="Read-Only Phase 12B Live PIE & 5-Gate Quantitative Pixel-ROI Verification"
    )
    parser.add_argument("--timeout", type=float, default=120.0, help="Seconds to wait for UnrealEditor remote node")
    parser.add_argument("--no-auto-launch", action="store_true", help="Do not launch UnrealEditor if not running")
    parser.add_argument("--shutdown", action="store_true", help="Cleanly quit UnrealEditor after verification")
    args = parser.parse_args()

    os.makedirs(os.path.join(REPO_ROOT, "Saved", "Logs"), exist_ok=True)
    os.makedirs(SCREENSHOTS_DIR, exist_ok=True)

    for stale in (HUD_ACTIVE_PNG, HUD_FAULT_PNG, LIT_FULL_PATH, LIT_CLEAN_PATH, LIT_FAULT_FINAL_PATH):
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
            print(f"[VerifyPhase12B] Launching UnrealEditor: {UNREAL_EDITOR_EXE} {UPROJECT_PATH} -NoSplash -Unattended")
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
            raise TimeoutError(f"Timed out after {args.timeout}s waiting for UnrealEditor remote node on 127.0.0.1")

        node_id = target_node["node_id"]
        print(f"[VerifyPhase12B] Connected to UnrealEditor node_id={node_id}")
        remote.open_command_connection(node_id)

        # 1. Start PIE & audit zero-cost primitive flags, light count, volumetric fog ban, and >= 8.5 mm X clearances
        run_remote_checked(remote, REMOTE_BEGIN_PIE_SNIPPET, "Begin PIE Session")
        time.sleep(0.5)
        focus_pie_viewport_window()
        audit_data = None
        last_err = None
        for attempt in range(1, 12):
            time.sleep(1.0)
            try:
                lines = run_remote_checked(
                    remote,
                    REMOTE_AUDIT_PIE_HYGIENE_AND_CLEARANCE_SNIPPET,
                    f"Audit PIE Hygiene, Light Budget & Clearances (attempt {attempt})",
                )
                audit_data = extract_json_tag(lines, "AUDIT_JSON=")
                break
            except Exception as exc:
                last_err = exc
                print(f"[VerifyPhase12B] Waiting for PIE warmup ({exc})...")

        if audit_data is None:
            run_remote_checked(remote, REMOTE_END_PIE_SNIPPET, "End PIE Session (after warmup failure)")
            raise RuntimeError(f"PIE hygiene/clearance audit failed: {last_err}")

        # Let initial PIE texture streaming / Lumen surface-cache warmup settle, then measure steady-state unthrottled PIE frame time
        focus_pie_viewport_window()
        time.sleep(1.25)
        steady_samples_ms: List[float] = []
        for s_idx in range(5):
            time.sleep(0.22)
            f_lines = run_remote_checked(
                remote,
                REMOTE_MEASURE_STEADY_FRAME_TIME_SNIPPET,
                f"Sample Unthrottled PIE Frame Time #{s_idx + 1}",
            )
            steady_samples_ms.append(float(extract_json_tag(f_lines, "FRAME_MS_JSON=")["delta_ms"]))
        steady_median_ms = statistics.median(steady_samples_ms)

        # 2. Request Composited UI Screenshot #1 (ActiveInspection)
        focus_pie_viewport_window()
        run_remote_checked(
            remote,
            make_request_composited_screenshot_snippet(HUD_ACTIVE_PNG),
            "Request Composited Screenshot: Phase12B_ActiveInspection_HUD.png",
        )
        time.sleep(0.55)
        wait_for_file(HUD_ACTIVE_PNG, timeout_sec=6.0)

        # 3. Capture 1280x720 Lit ActiveInspection frames & project 3D ROIs
        run_remote_checked(remote, REMOTE_SPAWN_TRANSIENT_CAPTURE_SNIPPET, "Spawn Transient 1280x720 Lit Capture")
        time.sleep(1.35)
        run_remote_checked(
            remote,
            make_export_active_full_and_hide_laser_snippet(LOGS_DIR, LIT_FULL_FILENAME),
            "Export ActiveInspection Full Lit PNG & Hide Laser Overlay",
        )
        time.sleep(0.45)
        roi_lines = run_remote_checked(
            remote,
            make_export_clean_and_project_rois_snippet(LOGS_DIR, LIT_CLEAN_FILENAME),
            "Export Clean Lit Probe, Project 3D ROIs & Destroy Transient Capture",
        )
        rois = extract_json_tag(roi_lines, "ROI_JSON=")

        # 4. Pre-warm Fault SceneCapture2D BEFORE triggering Clamp Snap so all 9 Hz strobe samples & HUD screenshot complete well inside 1.80s FaultResetDelay
        run_remote_checked(remote, REMOTE_SPAWN_TRANSIENT_CAPTURE_SNIPPET, "Spawn Transient Lit Capture for Fault Strobe")
        time.sleep(0.25)

        run_remote_checked(remote, REMOTE_TRIGGER_CLAMP_SNAP_SNIPPET, "Trigger Clamp Structural Snap Fault")
        time.sleep(0.20)  # > 0.14s shutter slam duration (alpha == 0.00), ~1.60s remaining in FaultResetDelay

        fault_frame_paths: List[str] = []
        fault_meta_list: List[dict] = []
        for k in range(4):
            fname = f"{LIT_FAULT_BASE}_{k}.png"
            fpath = os.path.join(REPO_ROOT, "Saved", "Logs", fname)
            if os.path.exists(fpath):
                os.remove(fpath)
            f_lines = run_remote_checked(
                remote,
                make_verify_fault_slam_and_export_strobe_frame_snippet(
                    LOGS_DIR,
                    fname,
                    None,
                ),
                f"Sample Slammed Bay 9 Hz Strobe Frame #{k + 1}",
            )
            fault_meta_list.append(extract_json_tag(f_lines, "FAULT_SAMPLE_JSON="))
            fault_frame_paths.append(fpath)
            time.sleep(0.024)

        run_remote_checked(remote, REMOTE_DESTROY_TRANSIENT_CAPTURE_SNIPPET, "Destroy Fault Transient Capture")

        for hud_try in range(2):
            if os.path.exists(HUD_FAULT_PNG):
                os.remove(HUD_FAULT_PNG)
            focus_pie_viewport_window()
            time.sleep(0.05)
            run_remote_checked(
                remote,
                make_request_composited_screenshot_snippet(HUD_FAULT_PNG),
                f"Request Composited Screenshot: Phase12B_FaultSlam_HUD.png (try {hud_try + 1})",
            )
            time.sleep(0.45)
            wait_for_file(HUD_FAULT_PNG, timeout_sec=5.0)
            if os.path.getsize(HUD_FAULT_PNG) >= 380000:
                break

        # Re-sample post-test steady-state PIE frame time after resetting back to ActiveInspection
        focus_pie_viewport_window()
        time.sleep(2.10)
        post_samples_ms: List[float] = []
        for s_idx in range(5):
            time.sleep(0.22)
            f_post = run_remote_checked(
                remote,
                REMOTE_MEASURE_STEADY_FRAME_TIME_SNIPPET,
                f"Sample Post-Fault Steady PIE Frame Time #{s_idx + 1}",
            )
            post_samples_ms.append(float(extract_json_tag(f_post, "FRAME_MS_JSON=")["delta_ms"]))
        post_ms = statistics.median(post_samples_ms)

        run_remote_checked(remote, REMOTE_END_PIE_SNIPPET, "End PIE Session Cleanly")
        time.sleep(0.60)

        # 5. Decode PNGs & Evaluate All 5 Quantitative Gates
        full_w, full_h, full_px = decode_png_rgb8(LIT_FULL_PATH)
        clean_w, clean_h, clean_px = decode_png_rgb8(LIT_CLEAN_PATH)
        if os.path.exists(LIT_CLEAN_PATH):
            os.remove(LIT_CLEAN_PATH)

        if (full_w, full_h) != (1280, 720) or (clean_w, clean_h) != (1280, 720):
            raise AssertionError(f"Expected 1280x720 Lit captures, got ({full_w},{full_h}) & ({clean_w},{clean_h})")

        # Pick strobe crest frame across the 9 Hz cycle
        best_fault_stats = None
        best_fault_meta = None
        for idx, fpath in enumerate(fault_frame_paths):
            fw, fh, fpx = decode_png_rgb8(fpath)
            st_f = compute_roi_stats(fw, fpx, [rois["slammed_bays"]])
            if best_fault_stats is None or st_f["peak_r"] > best_fault_stats["peak_r"]:
                best_fault_stats = st_f
                best_fault_meta = fault_meta_list[idx]
                if os.path.exists(LIT_FAULT_FINAL_PATH):
                    os.remove(LIT_FAULT_FINAL_PATH)
                os.replace(fpath, LIT_FAULT_FINAL_PATH)
            else:
                os.remove(fpath)

        assert best_fault_stats is not None and best_fault_meta is not None

        st_left_slit = compute_roi_stats(clean_w, clean_px, [rois["left_slit"]])
        st_right_slit = compute_roi_stats(clean_w, clean_px, [rois["right_slit"]])
        st_flank_slits = compute_roi_stats(clean_w, clean_px, [rois["left_slit"], rois["right_slit"]])
        st_booth_all = compute_roi_stats(
            clean_w,
            clean_px,
            [
                rois["left_slit"],
                rois["right_slit"],
                rois["left_shutter"],
                rois["center_shutter_left"],
                rois["center_shutter_right"],
                rois["right_shutter"],
            ],
        )
        st_monolith = compute_roi_stats(clean_w, clean_px, [rois["monolith"]])
        st_monolith_laser = compute_roi_stats(full_w, full_px, [rois["monolith_laser"]])

        st_right_director = compute_roi_stats(clean_w, clean_px, [rois["right_director"]])
        st_right_glass = compute_roi_stats(clean_w, clean_px, [rois["right_glass"]])

        st_outer_borders = compute_roi_stats(clean_w, clean_px, [rois["outer_left"], rois["outer_right"]])
        st_outer_left = compute_roi_stats(clean_w, clean_px, [rois["outer_left"]])
        st_outer_right = compute_roi_stats(clean_w, clean_px, [rois["outer_right"]])
        st_hud_corners = compute_roi_stats(clean_w, clean_px, [rois["hud_top_left"], rois["hud_top_right"]])

        # Metrics for the 5 Gates
        # Note: Left slit is unmanned louvered reference; also report combined flank slits
        window_slit_mean = st_left_slit["mean_luma"]
        combined_flank_mean = st_flank_slits["mean_luma"]
        booth_peak = max(st_booth_all["peak_luma"], st_booth_all["peak_avg"])

        monolith_mean = max(st_monolith["mean_luma"], st_monolith["mean_avg"])
        monolith_laser_peak = max(st_monolith_laser["peak_luma"], st_monolith_laser["peak_avg"])
        focal_mean_delta = monolith_mean - window_slit_mean
        focal_mean_ratio = monolith_mean / max(1.0e-3, window_slit_mean)
        focal_peak_delta = monolith_laser_peak - booth_peak

        louver_std_pixel = st_left_slit["std_luma"]
        louver_std_row = st_left_slit["row_std_luma"]
        director_cutout_delta = st_right_glass["mean_luma"] - st_right_director["mean_luma"]

        fault_peak_r = best_fault_stats["peak_r"]
        fault_r_b_ratio = best_fault_stats["peak_r_b_ratio"]
        fault_alpha = float(best_fault_meta["shutter_alpha"])

        outer_mean = st_outer_borders["mean_luma"]
        outer_peak = st_outer_borders["peak_luma"]
        hud_backing_mean = st_hud_corners["mean_luma"]

        print("\n================ PHASE 12B QUANTITATIVE 5-GATE REPORT ================")
        print(f"  [Audit] Clearances min_x_gap={audit_data['min_x_gap_cm']:.3f} cm (>=0.85 cm), gaps={audit_data['gaps_cm']}")
        print(f"  [Audit] Instances: louvers={audit_data['louver_instances']}, director={audit_data['director_instances']}, outer_framing={audit_data['outer_framing_instances']}")
        print(f"  [Gate 1] Panopticon Suppression: left_slit_mean={window_slit_mean:.2f} sRGB (combined_flank_mean={combined_flank_mean:.2f} sRGB) [target 36.0..48.0], booth_peak={booth_peak:.2f} sRGB [target <=135.0]")
        print(f"  [Gate 2] Focal Hierarchy: monolith_mean={monolith_mean:.2f} sRGB, delta=+{focal_mean_delta:.2f} sRGB (>=+22.0), ratio={focal_mean_ratio:.2f}x (>=1.50x), monolith_laser_peak={monolith_laser_peak:.2f} vs booth_peak={booth_peak:.2f} (delta=+{focal_peak_delta:.2f} >=+95.0)")
        print(f"  [Gate 3] Louvers & Director Cutout: louver_std_pixel={louver_std_pixel:.2f} sRGB (row_std={louver_std_row:.2f}) [target >=6.5], right_glass_mean={st_right_glass['mean_luma']:.2f} vs right_director_mean={st_right_director['mean_luma']:.2f} (cutout_delta=+{director_cutout_delta:.2f} sRGB >=+14.0)")
        print(f"  [Gate 4] 0.14s Fault Slam & 9 Hz Crimson Strobe: shutter_alpha={fault_alpha:.4f} (==0.00), slammed_gaps={best_fault_meta['slammed_gaps_cm']}, crimson_R_peak={fault_peak_r:.1f} sRGB (>=155.0), R/B={fault_r_b_ratio:.2f}x (>=3.0x)")
        print(f"  [Gate 5] Outer Framing, HUD Contrast & GPU Budget: outer_mean={outer_mean:.2f} sRGB [target 4.5..14.5] (L={st_outer_left['mean_luma']:.2f}, R={st_outer_right['mean_luma']:.2f}), outer_peak={outer_peak:.2f} sRGB [target 30.0..75.0], hud_corners_mean={hud_backing_mean:.2f} sRGB [target <=12.0], shadow_lights={audit_data['shadow_lights']}, zero_shadow_lights={audit_data['zero_shadow_lights']}, VolumetricFog={audit_data['volumetric_fog']}, steady_frame_ms={steady_median_ms:.2f} ms (samples={steady_samples_ms}), post_fault_ms={post_ms:.2f} ms (samples={post_samples_ms}) [target <=13.0 ms]")
        print(f"  [Screenshots] ActiveHUD={HUD_ACTIVE_PNG} ({os.path.getsize(HUD_ACTIVE_PNG)} B), FaultHUD={HUD_FAULT_PNG} ({os.path.getsize(HUD_FAULT_PNG)} B)")
        print("======================================================================\n")

        failures: List[str] = []
        if not (36.0 <= window_slit_mean <= 48.0):
            failures.append(f"Gate 1 window_slit_mean={window_slit_mean:.2f} outside [36.0, 48.0]")
        if booth_peak > 135.0:
            failures.append(f"Gate 1 booth_peak={booth_peak:.2f} > 135.0")
        if focal_mean_delta < 22.0 or focal_mean_ratio < 1.50:
            failures.append(f"Gate 2 monolith vs window mean delta={focal_mean_delta:.2f} (<22.0) or ratio={focal_mean_ratio:.2f}x (<1.50x)")
        if focal_peak_delta < 95.0:
            failures.append(f"Gate 2 monolith/laser peak - booth peak = {focal_peak_delta:.2f} (<95.0)")
        if louver_std_pixel < 6.5:
            failures.append(f"Gate 3 louver_std_pixel={louver_std_pixel:.2f} (<6.5)")
        if director_cutout_delta < 14.0:
            failures.append(f"Gate 3 director_cutout_delta={director_cutout_delta:.2f} (<14.0)")
        if fault_alpha > 0.005 or fault_peak_r < 155.0 or fault_r_b_ratio < 3.0:
            failures.append(f"Gate 4 fault_alpha={fault_alpha:.4f}, peak_r={fault_peak_r:.1f}, R/B={fault_r_b_ratio:.2f}x")
        if not (4.5 <= outer_mean <= 14.5) or not (30.0 <= outer_peak <= 75.0):
            failures.append(f"Gate 5 outer framing mean={outer_mean:.2f} [4.5..14.5] or peak={outer_peak:.2f} [30.0..75.0]")
        if hud_backing_mean > 12.0:
            failures.append(f"Gate 5 hud_backing_mean={hud_backing_mean:.2f} > 12.0")
        if steady_median_ms > 13.0 or post_ms > 13.0:
            failures.append(f"Gate 5 PIE frame time steady={steady_median_ms:.2f}ms / post={post_ms:.2f}ms > 13.0ms")

        if failures:
            raise AssertionError("PHASE 12B QUANTITATIVE GATE FAILURES:\n  - " + "\n  - ".join(failures))

        print("[VerifyPhase12B] ALL 5 QUANTITATIVE GATES & GEOMETRIC/PBR HYGIENE CHECKS PASSED.")
        return 0
    finally:
        if args.shutdown or spawned_proc is not None:
            print("[VerifyPhase12B] Sending clean quit_editor() command...")
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
