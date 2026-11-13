"""
Phase 12-FIX2 Live PIE 4-Gate Verification & Composited Screenshot Harness.

Launches UnrealEditor on /Game/Maps/L_MainGameplay, enters PIE, and verifies:
  - Gate 1 (+50% Corner HUD Layout, Typography, Refinement C 720p Scaling & Non-Overlap):
    CornerPanelWidthPx == 525.0, CornerPanelMaxViewportWidthFraction == 0.34,
    EffectiveCornerWidth == min(525.0, LocalViewportW * 0.34), zero horizontal overflow on
    STopLeftIndenturePanel & STopRightDirectorPanel (both at 1080p and sub-500px 720p width),
    dynamic OnPaint divider non-bisection, Y_bottom / ViewportH < 0.17 above Panopticon slits,
    X strictly clear of SM_ClampSpringHousing (U outside [0.40, 0.60]), and zero AABB overlap
    across all 5 HUD widgets.
  - Gate 2 (Shift-01 3-Stage Progressive Tutorial, Refinements A & B, Preemption & Suppression):
    Stage 1 (Ctr 01 Box), Stage 2 (Ctr 02 CantileveredJHook), Stage 3 (Ctr 03 OffsetCChannel)
    all appear non-preempted on ActiveInspection start, self-stop their 2-pulse ActiveTimer
    by t > 0.85s (0.00 ms idle CPU, settled bg opacity ~ 0.76), ignore MissedTarget air misfires
    (Refinement A), dismiss immediately on Success slice / >= 3.5 deg tilt / TareChute diverter,
    never re-appear on same-contract fault retry, yield to P1 hazards (P0 > P1 > P2 > P3),
    suppress strictly on Shift 02, re-arm on ExecuteCareerResetToShiftOne(), and auto-dismiss
    after the 4.5s Tier-1 FTimerHandle when idle.
  - Gate 3 (Dark Oiled Cast-Iron Rodin PBR Outer Pipes, Zero-Cost Flags & GPU Budget):
    64 instances across 4 zero-cost ISMs (10 + 40 + 12 + 2) including OuterTieGussetsISM,
    OuterPipesMID parented to MI_Station_TareChute with ConductorMinF0 <= 0.02 and AlbedoTint
    in [0.48, 0.68], outer pipe ROI mean_luma in [3.2, 9.5] sRGB & peak_luma in [20.0, 58.0] sRGB,
    shadow_lights == 1, zero_shadow_lights == 10, VolumetricFog == False, steady frame <= 13.0 ms.
  - Gate 4 (3 Composited UI Screenshots in Saved/Screenshots/Phase12Fix2/):
    1. Phase12Fix2_Stage1_Box_HUD_Pipes.png
    2. Phase12Fix2_Stage2_JHook_Tutorial.png
    3. Phase12Fix2_Stage3_CChannel_Tutorial.png
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
LOGS_DIR = os.path.join(REPO_ROOT, "Saved", "Logs").replace("\\", "/")
SCREENSHOTS_DIR = os.path.join(REPO_ROOT, "Saved", "Screenshots", "Phase12Fix2")

SHOT_STAGE1 = os.path.join(SCREENSHOTS_DIR, "Phase12Fix2_Stage1_Box_HUD_Pipes.png")
SHOT_STAGE2 = os.path.join(SCREENSHOTS_DIR, "Phase12Fix2_Stage2_JHook_Tutorial.png")
SHOT_STAGE3 = os.path.join(SCREENSHOTS_DIR, "Phase12Fix2_Stage3_CChannel_Tutorial.png")

LIT_CLEAN_FILENAME = "_Phase12Fix2_OuterPipes_CleanProbe.png"
LIT_CLEAN_PATH = os.path.join(REPO_ROOT, "Saved", "Logs", LIT_CLEAN_FILENAME)


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
unreal.log("[VerifyPhase12Fix2] Requested BeginPlay PIE session.")
"""


REMOTE_WARMUP_AND_INITIAL_AUDIT_SNIPPET = """
import json
import math
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
framings = []
pawns = []
rigs = []
booths = []

for actor in all_actors:
    cls_name = actor.get_class().get_name()
    if "CellFramingArchitectureActor" in cls_name:
        framings.append(actor)
    elif "InspectorPawn" in cls_name:
        pawns.append(actor)
    elif "MonolithRigActor" in cls_name:
        rigs.append(actor)
    elif "DirectorBoothActor" in cls_name:
        booths.append(actor)

    for lc in actor.get_components_by_class(unreal.LightComponentBase):
        entry = f"{cls_name}.{lc.get_name()}"
        if bool(lc.get_editor_property("cast_shadows")):
            shadow_lights.append(entry)
        else:
            zero_shadow_lights.append(entry)

    for fog in actor.get_components_by_class(unreal.ExponentialHeightFogComponent):
        if bool(fog.get_editor_property("enable_volumetric_fog")):
            volumetric_fog_enabled = True

if len(framings) != 1 or len(pawns) != 1 or len(rigs) != 1 or len(booths) != 1:
    raise AssertionError(
        f"Expected 1 Framing, 1 Pawn, 1 Rig, 1 Booth; got {len(framings)}, {len(pawns)}, {len(rigs)}, {len(booths)}"
    )
if len(shadow_lights) != 1:
    raise AssertionError(f"Expected shadow_lights == 1, got {len(shadow_lights)}: {shadow_lights}")
if len(zero_shadow_lights) != 10:
    raise AssertionError(f"Expected zero_shadow_lights == 10, got {len(zero_shadow_lights)}: {zero_shadow_lights}")
if volumetric_fog_enabled:
    raise AssertionError("VolumetricFog hard ban violated!")

pawn = pawns[0]
rig = rigs[0]
booth = booths[0]
framing = framings[0]
hud_bridge = pawn.get_component_by_class(unreal.InspectorHudBridgeComponent)
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]

if hud_bridge is None or not bool(hud_bridge.is_overlay_mounted()):
    raise AssertionError("InspectorHudBridgeComponent missing or STareTitheHudOverlay not mounted!")
if int(unreal.InspectorHudBridgeComponent.get_active_mounted_overlay_count()) != 1:
    raise AssertionError(
        f"Expected GetActiveMountedOverlayCount() == 1, got {unreal.InspectorHudBridgeComponent.get_active_mounted_overlay_count()}"
    )

# Verify Gate 3 ISM counts, 6 zero-cost flags on all 4 ISMs (Refinement D), and Dark Cast-Iron Rodin PBR MID parameters
def assert_zero_cost_flags(comp, label):
    if comp is None:
        raise AssertionError(f"Missing required ISM component: {label}")
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

cols_ism = framing.get_outer_heavy_columns_ism()
pipes_ism = framing.get_outer_flanged_pipes_ism()
gussets_ism = framing.get_outer_tie_gussets_ism()
ribs_ism = framing.get_outer_structural_ribs_ism()

for comp, label in (
    (cols_ism, "OuterHeavyColumnsISM"),
    (pipes_ism, "OuterFlangedPipesISM"),
    (gussets_ism, "OuterTieGussetsISM"),
    (ribs_ism, "OuterStructuralRibsISM"),
):
    assert_zero_cost_flags(comp, label)

col_cnt = int(cols_ism.get_instance_count())
pipe_cnt = int(pipes_ism.get_instance_count())
gusset_cnt = int(gussets_ism.get_instance_count())
rib_cnt = int(ribs_ism.get_instance_count())
tot_cnt = int(framing.get_total_framing_instance_count())

if (col_cnt, pipe_cnt, gusset_cnt, rib_cnt, tot_cnt) != (10, 40, 12, 2, 64):
    raise AssertionError(
        f"Expected framing ISM counts (10, 40, 12, 2, total=64), got ({col_cnt}, {pipe_cnt}, {gusset_cnt}, {rib_cnt}, total={tot_cnt})"
    )

def get_vec_param(mid, param_name):
    for fn_name in ("k2_get_vector_parameter_value", "get_vector_parameter_value"):
        fn = getattr(mid, fn_name, None)
        if callable(fn):
            col = fn(param_name)
            return [round(float(col.r), 5), round(float(col.g), 5), round(float(col.b), 5)]
    for entry in list(mid.get_editor_property("vector_parameter_values") or []):
        pinfo = entry.get_editor_property("parameter_info")
        if str(pinfo.get_editor_property("name")) == param_name:
            col = entry.get_editor_property("parameter_value")
            return [round(float(col.r), 5), round(float(col.g), 5), round(float(col.b), 5)]
    raise KeyError(f"Missing vector parameter {param_name} on {mid.get_name()}")

pipes_mid = framing.get_outer_pipes_mid()
cols_mid = framing.get_outer_columns_mid()
if pipes_mid is None or cols_mid is None:
    raise AssertionError("OuterPipesMID or OuterColumnsMID is None!")

pipes_parent = pipes_mid.get_editor_property("parent")
cols_parent = cols_mid.get_editor_property("parent")
pipes_parent_name = "" if pipes_parent is None else pipes_parent.get_name()
cols_parent_name = "" if cols_parent is None else cols_parent.get_name()

if "MI_Station_TareChute" not in pipes_parent_name:
    raise AssertionError(f"Expected OuterPipesMID parent MI_Station_TareChute, got '{pipes_parent_name}'")
if "MI_Station_ClampSpringHousing" not in cols_parent_name:
    raise AssertionError(f"Expected OuterColumnsMID parent MI_Station_ClampSpringHousing, got '{cols_parent_name}'")

pipes_albedo = get_vec_param(pipes_mid, "AlbedoTint")
pipes_f0 = get_vec_param(pipes_mid, "ConductorMinF0")
cols_f0 = get_vec_param(cols_mid, "ConductorMinF0")

if max(pipes_f0) > 0.02 or max(cols_f0) > 0.02:
    raise AssertionError(f"ConductorMinF0 > 0.02 grain lift violation: pipes_f0={pipes_f0}, cols_f0={cols_f0}")
if not all(0.48 <= c <= 0.68 for c in pipes_albedo):
    raise AssertionError(f"OuterPipesMID AlbedoTint {pipes_albedo} outside approved [0.48, 0.68] range!")

# Project overhead SM_ClampSpringHousing and Panopticon flank slit top to normalized UV for Gate 1 screen-space clearance proof
cam = pawn.get_editor_property("inspector_camera")
cam_loc = cam.get_world_location()
cam_rot = cam.get_world_rotation()
cam_fov = float(cam.get_editor_property("field_of_view"))
local_vp = hud_bridge.get_last_local_viewport_size()
aspect = float(local_vp.x) / max(1.0, float(local_vp.y))
fwd = unreal.MathLibrary.get_forward_vector(cam_rot)
right = unreal.MathLibrary.get_right_vector(cam_rot)
up = unreal.MathLibrary.get_up_vector(cam_rot)
tan_h = math.tan(math.radians(cam_fov * 0.5))
tan_v = tan_h / max(0.5, aspect)

def project_uv(pt):
    rx = float(pt.x - cam_loc.x)
    ry = float(pt.y - cam_loc.y)
    rz = float(pt.z - cam_loc.z)
    d = rx * float(fwd.x) + ry * float(fwd.y) + rz * float(fwd.z)
    if d <= 1.0:
        return None
    cx = rx * float(right.x) + ry * float(right.y) + rz * float(right.z)
    cy = rx * float(up.x) + ry * float(up.y) + rz * float(up.z)
    u = 0.5 + 0.5 * (cx / (d * tan_h))
    v = 0.5 - 0.5 * (cy / (d * tan_v))
    return (u, v)

clamp_comp = rig.get_editor_property("clamp_spring_housing_visual")
clamp_sm = clamp_comp.get_editor_property("static_mesh")
clamp_bb = clamp_sm.get_bounding_box()
clamp_xf = clamp_comp.get_world_transform()
clamp_us = []
for bx in (clamp_bb.min.x, clamp_bb.max.x):
    for by in (clamp_bb.min.y, clamp_bb.max.y):
        for bz in (clamp_bb.min.z, clamp_bb.max.z):
            uv = project_uv(unreal.MathLibrary.transform_location(clamp_xf, unreal.Vector(bx, by, bz)))
            if uv is not None:
                clamp_us.append(uv[0])

l_sh_comp = booth.get_editor_property("left_blast_shutter_visual")
l_sh_sm = l_sh_comp.get_editor_property("static_mesh")
l_sh_bb = l_sh_sm.get_bounding_box()
l_sh_xf = l_sh_comp.get_world_transform()
shutter_vs = []
for bx in (l_sh_bb.min.x, l_sh_bb.max.x):
    for by in (l_sh_bb.min.y, l_sh_bb.max.y):
        for bz in (l_sh_bb.min.z, l_sh_bb.max.z):
            uv = project_uv(unreal.MathLibrary.transform_location(l_sh_xf, unreal.Vector(bx, by, bz)))
            if uv is not None:
                shutter_vs.append(uv[1])

pres = shift_sub.get_active_presentation_asset()
hud_cfg = pres.get_editor_property("hud_config") if pres is not None else unreal.HudPresentationConfig()

unreal.log("AUDIT_JSON=" + json.dumps({
    "world_time_sec": round(world_time_sec, 3),
    "initial_alert_state": str(hud_bridge.get_active_center_alert_state_name()),
    "initial_headline": str(hud_bridge.get_active_center_alert_headline()),
    "shadow_lights": len(shadow_lights),
    "zero_shadow_lights": len(zero_shadow_lights),
    "volumetric_fog": volumetric_fog_enabled,
    "framing_counts": [col_cnt, pipe_cnt, gusset_cnt, rib_cnt, tot_cnt],
    "pipes_parent": pipes_parent_name,
    "cols_parent": cols_parent_name,
    "pipes_albedo": pipes_albedo,
    "pipes_f0": pipes_f0,
    "cols_f0": cols_f0,
    "clamp_u_range": [round(min(clamp_us), 4), round(max(clamp_us), 4)],
    "panopticon_slit_top_v": round(max(shutter_vs), 4),
    "cfg_corner_width_px": round(float(hud_cfg.get_editor_property("corner_panel_width_px")), 2),
    "cfg_corner_max_frac": round(float(hud_cfg.get_editor_property("corner_panel_max_viewport_width_fraction")), 4),
    "cfg_fonts_pt": [
        int(hud_cfg.get_editor_property("corner_header_font_size_pt")),
        int(hud_cfg.get_editor_property("corner_body_font_size_pt")),
        int(hud_cfg.get_editor_property("corner_sub_font_size_pt")),
        int(hud_cfg.get_editor_property("corner_pill_font_size_pt")),
    ],
}))
"""


REMOTE_INSPECT_HUD_LAYOUT_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
hud_bridge = pawn.get_component_by_class(unreal.InspectorHudBridgeComponent)
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]

vp = hud_bridge.get_last_local_viewport_size()
eff_w = float(hud_bridge.get_last_effective_corner_panel_width())
eff_bot_w = float(hud_bridge.get_last_effective_bottom_console_width())

def v2(vec):
    return [round(float(vec.x), 2), round(float(vec.y), 2)]

tl_min = v2(hud_bridge.get_top_left_panel_bounds_min())
tl_max = v2(hud_bridge.get_top_left_panel_bounds_max())
tr_min = v2(hud_bridge.get_top_right_panel_bounds_min())
tr_max = v2(hud_bridge.get_top_right_panel_bounds_max())
cb_min = v2(hud_bridge.get_center_banner_bounds_min())
cb_max = v2(hud_bridge.get_center_banner_bounds_max())
rc_min = v2(hud_bridge.get_reticle_card_bounds_min())
rc_max = v2(hud_bridge.get_reticle_card_bounds_max())
bc_min = v2(hud_bridge.get_bottom_console_bounds_min())
bc_max = v2(hud_bridge.get_bottom_console_bounds_max())

named_boxes = []
for name, bmin, bmax in (
    ("TopLeft", tl_min, tl_max),
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

unreal.log("HUD_JSON=" + json.dumps({
    "shift_num": int(shift_sub.get_current_shift_number()),
    "phase": shift_sub.get_current_phase().name,
    "contract_idx": int(shift_sub.get_current_snapshot().get_editor_property("contract_index")),
    "route": shift_sub.get_current_snapshot().get_editor_property("active_diverter_route").name,
    "local_vp": [round(float(vp.x), 2), round(float(vp.y), 2)],
    "eff_corner_w": round(eff_w, 2),
    "eff_bottom_w": round(eff_bot_w, 2),
    "tl_desired_w": round(float(hud_bridge.get_top_left_content_desired_width()), 2),
    "tr_desired_w": round(float(hud_bridge.get_top_right_content_desired_width()), 2),
    "tl_hdr_bottom_y": round(float(hud_bridge.get_top_left_header_bottom_local_y()), 2),
    "tl_div_y": round(float(hud_bridge.get_top_left_divider_local_y()), 2),
    "tr_hdr_bottom_y": round(float(hud_bridge.get_top_right_header_bottom_local_y()), 2),
    "tr_div_y": round(float(hud_bridge.get_top_right_divider_local_y()), 2),
    "tl_box": [tl_min, tl_max],
    "tr_box": [tr_min, tr_max],
    "cb_box": [cb_min, cb_max],
    "rc_box": [rc_min, rc_max],
    "bc_box": [bc_min, bc_max],
    "overlaps": overlaps,
    "alert_state": str(hud_bridge.get_active_center_alert_state_name()),
    "headline": str(hud_bridge.get_active_center_alert_headline()),
    "subtext": str(hud_bridge.get_active_center_alert_sub_text()),
    "pulse_active": bool(hud_bridge.is_center_alert_pulse_timer_active()),
    "bg_opacity": round(float(hud_bridge.get_center_alert_bg_opacity()), 4),
    "text_opacity": round(float(hud_bridge.get_center_alert_text_opacity()), 4),
}))
"""


REMOTE_BEGIN_720P_SCALING_PROBE_SNIPPET = """
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
hud_bridge = pawn.get_component_by_class(unreal.InspectorHudBridgeComponent)
hud_bridge.override_corner_width_for_test(435.2, True)
unreal.log("[VerifyPhase12Fix2] Applied transient 435.2 px (720p equivalent) Refinement C probe.")
"""


REMOTE_RESTORE_PRESENTATION_DATA_SNIPPET = """
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
hud_bridge = pawn.get_component_by_class(unreal.InspectorHudBridgeComponent)
hud_bridge.override_corner_width_for_test(525.0, False)
unreal.log("[VerifyPhase12Fix2] Restored canonical PresentationData on InspectorHudBridgeComponent.")
"""


REMOTE_CAREER_RESET_SNIPPET = """
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
if not bool(shift_sub.execute_career_reset_to_shift_one()):
    raise RuntimeError("ExecuteCareerResetToShiftOne() returned False!")
unreal.log("[VerifyPhase12Fix2] Executed career reset to Shift 01 - Contract 01.")
"""


def make_start_contract_snippet(contract_index: int) -> str:
    return f"""
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
if not bool(shift_sub.start_shift_from_contract({contract_index})):
    raise RuntimeError("StartShiftFromContract({contract_index}) returned False!")
"""


def make_advance_to_shift_snippet(shift_num: int, contract_index: int) -> str:
    return f"""
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
if not bool(shift_sub.advance_to_shift_for_verification({shift_num}, {contract_index})):
    raise RuntimeError("AdvanceToShiftForVerification({shift_num}, {contract_index}) returned False!")
"""


def make_commit_slice_delta_z_snippet(z_offset_from_min_cm: float) -> str:
    return f"""
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
hud_bridge = pawn.get_component_by_class(unreal.InspectorHudBridgeComponent)

bounds = rig.get_target_world_bounds()
cx = 0.5 * (float(bounds.min.x) + float(bounds.max.x))
cy = 0.5 * (float(bounds.min.y) + float(bounds.max.y))
cut_z = float(bounds.min.z) + ({z_offset_from_min_cm:.4f})

aperture = unreal.LaserSliceAperture()
aperture.set_editor_property("enforce_finite_sheet_bounds", False)
summary = rig.commit_slice_plane(unreal.Vector(cx, cy, cut_z), unreal.Vector(0.0, 0.0, -1.0), aperture)

unreal.log("SLICE_TEST_JSON=" + json.dumps({{
    "cut_z": round(cut_z, 2),
    "outcome": summary.get_editor_property("outcome").name,
    "alert_state_after": str(hud_bridge.get_active_center_alert_state_name()),
    "headline_after": str(hud_bridge.get_active_center_alert_headline()),
}}))
"""


REMOTE_MANUAL_FAULT_RESET_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
hud_bridge = pawn.get_component_by_class(unreal.InspectorHudBridgeComponent)

shift_sub.request_manual_contract_reset()
unreal.log("FAULT_JSON=" + json.dumps({
    "phase": shift_sub.get_current_phase().name,
    "alert_state": str(hud_bridge.get_active_center_alert_state_name()),
}))
"""


REMOTE_INJECT_STAGE2_ROLL_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
hud_bridge = pawn.get_component_by_class(unreal.InspectorHudBridgeComponent)

hud_bridge.inject_roll_wheel_for_test(2.0, False)
unreal.log("ROLL_JSON=" + json.dumps({
    "alert_state_after": str(hud_bridge.get_active_center_alert_state_name()),
}))
"""


REMOTE_AIM_INTO_CTR03_VEIN_DROP_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
hud_bridge = pawn.get_component_by_class(unreal.InspectorHudBridgeComponent)

# Resting aim on Ctr 03 is Z = bounds.min.z - 5.0 cm; +30.0 input units * 1.25 cm/unit = +37.5 cm -> Z = bounds.min.z + 32.5 cm (cleanly above lower vein -> VeinInDrop)
pawn.inject_aim_cursor_delta_for_test(unreal.Vector2D(0.0, 30.0))
unreal.log("PREEMPT_JSON=" + json.dumps({
    "alert_state": str(hud_bridge.get_active_center_alert_state_name()),
    "headline": str(hud_bridge.get_active_center_alert_headline()),
}))
"""


REMOTE_TOGGLE_DIVERTER_AND_CHECK_STAGE3_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = [s for s in unreal.ObjectIterator(unreal.InspectionShiftSubsystem) if s.get_outer() == pie_world][0]
pawn = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "InspectorPawn" in a.get_class().get_name()][0]
hud_bridge = pawn.get_component_by_class(unreal.InspectorHudBridgeComponent)

ok = bool(shift_sub.request_toggle_diverter())
route = shift_sub.get_current_snapshot().get_editor_property("active_diverter_route").name
unreal.log("STAGE3_DIV_JSON=" + json.dumps({
    "toggled": ok,
    "route": route,
    "alert_state_after": str(hud_bridge.get_active_center_alert_state_name()),
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
unreal.log("[VerifyPhase12Fix2] Queued composited UI screenshot: " + out_path)
"""


def make_clean_lit_viewport_screenshot_snippet(png_path: str) -> str:
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
unreal.log("[VerifyPhase12Fix2] Queued clean 3D GameViewport Lit screenshot (bShowUI=False): " + out_path)
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
unreal.log("[VerifyPhase12Fix2] Restored laser overlay visibility after clean Lit probe.")
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
unreal.log("[VerifyPhase12Fix2] Cleanly ended PIE session without saving maps/assets.")
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


def compute_roi_stats(
    width: int,
    pixels: List[Tuple[int, int, int]],
    boxes: List[List[int]],
) -> Dict[str, float]:
    lumas: List[float] = []
    peak_luma = 0.0
    for x0, y0, x1, y1 in boxes:
        for y in range(y0, y1):
            row_idx = y * width
            for x in range(x0, x1):
                r, g, b = pixels[row_idx + x]
                luma = 0.2126 * r + 0.7152 * g + 0.0722 * b
                lumas.append(luma)
                if luma > peak_luma:
                    peak_luma = luma
    count = len(lumas)
    if count == 0:
        raise RuntimeError(f"Empty ROI pixel box: {boxes}")
    return {
        "count": float(count),
        "mean_luma": sum(lumas) / count,
        "peak_luma": peak_luma,
        "std_luma": statistics.pstdev(lumas) if count > 1 else 0.0,
    }


def run_remote_checked(
    remote: remote_execution.RemoteExecution,
    code: str,
    label: str,
    verbose: bool = True,
) -> List[str]:
    if verbose:
        print(f"[VerifyPhase12Fix2] >>> {label}")
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


def wait_for_file(path: str, min_bytes: int = 102400, timeout_sec: float = 6.0) -> None:
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        if os.path.exists(path) and os.path.getsize(path) >= min_bytes:
            return
        time.sleep(0.12)
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


def assert_corner_hud_layout(hud: Dict[str, Any], label: str, expect_width: Optional[float] = None) -> None:
    vp_w, vp_h = float(hud["local_vp"][0]), float(hud["local_vp"][1])
    eff_w = float(hud["eff_corner_w"])
    expected = expect_width if expect_width is not None else min(525.0, vp_w * 0.34)
    if abs(eff_w - expected) > 1.25:
        raise AssertionError(f"{label}: EffectiveCornerWidth={eff_w:.2f} != expected {expected:.2f} (vp={hud['local_vp']})")
    if float(hud["tl_desired_w"]) > eff_w + 1.0 or float(hud["tr_desired_w"]) > eff_w + 1.0:
        raise AssertionError(
            f"{label}: Corner panel horizontal overflow! eff_w={eff_w:.2f}, "
            f"tl_desired={hud['tl_desired_w']}, tr_desired={hud['tr_desired_w']}"
        )
    if float(hud["tl_div_y"]) < float(hud["tl_hdr_bottom_y"]) + 2.0:
        raise AssertionError(
            f"{label}: TopLeft divider bisection! hdr_bottom={hud['tl_hdr_bottom_y']}, div_y={hud['tl_div_y']}"
        )
    if float(hud["tr_div_y"]) < float(hud["tr_hdr_bottom_y"]) + 2.0:
        raise AssertionError(
            f"{label}: TopRight divider bisection! hdr_bottom={hud['tr_hdr_bottom_y']}, div_y={hud['tr_div_y']}"
        )
    tl_bottom_v = float(hud["tl_box"][1][1]) / max(1.0, vp_h)
    tr_bottom_v = float(hud["tr_box"][1][1]) / max(1.0, vp_h)
    if tl_bottom_v >= 0.17 or tr_bottom_v >= 0.17:
        raise AssertionError(
            f"{label}: Corner HUD encroaches on Panopticon slits (V >= 0.17)! tl_v={tl_bottom_v:.4f}, tr_v={tr_bottom_v:.4f}"
        )
    tl_right_u = float(hud["tl_box"][1][0]) / max(1.0, vp_w)
    tr_left_u = float(hud["tr_box"][0][0]) / max(1.0, vp_w)
    if tl_right_u >= 0.40 or tr_left_u <= 0.60:
        raise AssertionError(
            f"{label}: Corner HUD encroaches on overhead SM_ClampSpringHousing [0.40, 0.60]! "
            f"tl_right_u={tl_right_u:.4f}, tr_left_u={tr_left_u:.4f}"
        )
    if hud["overlaps"]:
        raise AssertionError(f"{label}: HUD widget AABB overlap detected: {hud['overlaps']}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Phase 12-FIX2 Live PIE 4-Gate Verification")
    parser.add_argument("--timeout", type=float, default=120.0)
    parser.add_argument("--shutdown", action="store_true", help="Cleanly quit UnrealEditor after verification")
    args = parser.parse_args()

    os.makedirs(os.path.join(REPO_ROOT, "Saved", "Logs"), exist_ok=True)
    os.makedirs(SCREENSHOTS_DIR, exist_ok=True)
    for stale in (SHOT_STAGE1, SHOT_STAGE2, SHOT_STAGE3, LIT_CLEAN_PATH):
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
            print(f"[VerifyPhase12Fix2] Launching UnrealEditor: {UNREAL_EDITOR_EXE} {UPROJECT_PATH}")
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

        audit_data: Optional[Dict[str, Any]] = None
        last_err = None
        for attempt in range(1, 15):
            time.sleep(0.85)
            try:
                lines = run_remote_checked(
                    remote,
                    REMOTE_WARMUP_AND_INITIAL_AUDIT_SNIPPET,
                    f"Audit Scene Budget, Outer Pipe MIDs & Initial Spawn (attempt {attempt})",
                )
                audit_data = extract_json_tag(lines, "AUDIT_JSON=")
                break
            except Exception as exc:
                last_err = exc
                print(f"[VerifyPhase12Fix2] Waiting for PIE warmup ({exc})...")

        if audit_data is None:
            raise RuntimeError(f"Failed PIE warmup audit: {last_err}")

        # Verify initial BeginPlay spawn armed Stage 1 without preemption
        if audit_data["initial_alert_state"] != "P3_TutorialHint":
            raise AssertionError(
                f"Initial BeginPlay spawn failed to arm P3_TutorialHint: got {audit_data['initial_alert_state']}"
            )
        if audit_data["cfg_corner_width_px"] != 525.0 or abs(audit_data["cfg_corner_max_frac"] - 0.34) > 1e-3:
            raise AssertionError(f"Unexpected FHudPresentationConfig corner defaults: {audit_data}")
        if audit_data["cfg_fonts_pt"] != [13, 14, 13, 11]:
            raise AssertionError(f"Unexpected FHudPresentationConfig font pt sizes: {audit_data['cfg_fonts_pt']}")

        # Let Lumen surface cache settle completely
        focus_pie_viewport_window()
        time.sleep(0.95)

        # =====================================================================
        # GATE 1 + GATE 2 STAGE 1 (Shift 01 - Contract 01: Onboarding Box)
        # =====================================================================
        run_remote_checked(remote, REMOTE_CAREER_RESET_SNIPPET, "Fresh Career Reset to Shift 01 - Ctr 01")
        time.sleep(0.15)
        hud_s1_entry = extract_json_tag(
            run_remote_checked(remote, REMOTE_INSPECT_HUD_LAYOUT_SNIPPET, "Gate 1 & Stage 1 Entry Check"),
            "HUD_JSON=",
        )
        assert_corner_hud_layout(hud_s1_entry, "Stage 1 Entry (1080p)")

        expected_s1_head = "MOVE MOUSE TO AIM \u2014 HOLD [LMB] TO SLICE"
        expected_s1_sub = "LAND CUTS ON SCALE TO FILL QUOTA, THEN PRESS [E] TO STAMP"
        if (
            hud_s1_entry["alert_state"] != "P3_TutorialHint"
            or hud_s1_entry["headline"] != expected_s1_head
            or hud_s1_entry["subtext"] != expected_s1_sub
        ):
            raise AssertionError(f"Stage 1 text/state mismatch at contract start: {hud_s1_entry}")

        # Wait past 0.80s 2-pulse envelope and verify ActiveTimer self-stops (0.00 ms idle CPU) & settles at 0.76 bg opacity
        time.sleep(0.85)
        hud_s1_settled = extract_json_tag(
            run_remote_checked(remote, REMOTE_INSPECT_HUD_LAYOUT_SNIPPET, "Stage 1 Settled Post-Pulse Check"),
            "HUD_JSON=",
        )
        assert_corner_hud_layout(hud_s1_settled, "Stage 1 Settled (1080p)")
        if (
            hud_s1_settled["alert_state"] != "P3_TutorialHint"
            or hud_s1_settled["pulse_active"]
            or abs(float(hud_s1_settled["bg_opacity"]) - 0.76) > 0.03
            or float(hud_s1_settled["text_opacity"]) < 0.95
        ):
            raise AssertionError(f"Stage 1 post-pulse ActiveTimer/opacity check failed: {hud_s1_settled}")

        # Capture Screenshot #1: Stage 1 Box + +50% Corner HUD + Dark Cast-Iron Outer Pipes
        focus_pie_viewport_window()
        run_remote_checked(
            remote,
            make_composited_screenshot_snippet(SHOT_STAGE1),
            "Capture Screenshot 1: Phase12Fix2_Stage1_Box_HUD_Pipes.png",
        )
        wait_for_file(SHOT_STAGE1)

        # Gate 1 Refinement C Probe: Verify sub-500px (435.2 px / 720p equivalent) proportional font scaling & zero overflow
        run_remote_checked(
            remote,
            REMOTE_BEGIN_720P_SCALING_PROBE_SNIPPET,
            "Gate 1 Refinement C: Apply 435.2 px (720p) Corner Scaling Probe",
        )
        time.sleep(0.22)
        hud_720p = extract_json_tag(
            run_remote_checked(remote, REMOTE_INSPECT_HUD_LAYOUT_SNIPPET, "Gate 1 Refinement C: Inspect 435.2 px Layout"),
            "HUD_JSON=",
        )
        assert_corner_hud_layout(hud_720p, "Refinement C 720p Probe (435.2 px)", expect_width=435.2)
        run_remote_checked(
            remote,
            REMOTE_RESTORE_PRESENTATION_DATA_SNIPPET,
            "Gate 1 Refinement C: Restore Canonical PresentationData",
        )
        time.sleep(0.15)

        # Gate 2 Refinement A: Air misfire (ECommitSliceOutcome::MissedTarget) MUST NOT dismiss Stage 1!
        miss_s1 = extract_json_tag(
            run_remote_checked(
                remote,
                make_commit_slice_delta_z_snippet(-40.0),
                "Gate 2 Refinement A: Air Misfire (MissedTarget) on Stage 1",
            ),
            "SLICE_TEST_JSON=",
        )
        if miss_s1["outcome"] != "MISSED_TARGET" or miss_s1["alert_state_after"] != "P3_TutorialHint":
            raise AssertionError(f"Refinement A violated on Stage 1: air misfire dismissed tutorial! {miss_s1}")

        # Gate 2 Stage 1 Dismissal: Valid slice (ECommitSliceOutcome::Success) MUST immediately dismiss Stage 1!
        hit_s1 = extract_json_tag(
            run_remote_checked(
                remote,
                make_commit_slice_delta_z_snippet(12.0),
                "Gate 2 Stage 1: Commit Valid Slice (Success) -> Immediate Dismiss",
            ),
            "SLICE_TEST_JSON=",
        )
        if hit_s1["outcome"] != "SUCCESS" or hit_s1["alert_state_after"] != "None":
            raise AssertionError(f"Stage 1 failed immediate dismissal on Success slice: {hit_s1}")

        # Gate 2 Once-Per-Contract Guard: Fault retry on Ctr 01 MUST NOT re-show Stage 1!
        fault_s1 = extract_json_tag(
            run_remote_checked(remote, REMOTE_MANUAL_FAULT_RESET_SNIPPET, "Gate 2: Trigger Ctr 01 Fault Reset"),
            "FAULT_JSON=",
        )
        if fault_s1["alert_state"] != "P0_FaultStamp":
            raise AssertionError(f"Expected P0_FaultStamp on manual fault reset, got {fault_s1}")

        retry_hud: Optional[Dict[str, Any]] = None
        retry_deadline = time.time() + 3.5
        while time.time() < retry_deadline:
            time.sleep(0.20)
            cand = extract_json_tag(
                run_remote_checked(remote, REMOTE_INSPECT_HUD_LAYOUT_SNIPPET, "Poll Ctr 01 Retry Recovery", verbose=False),
                "HUD_JSON=",
            )
            if cand["phase"] == "ACTIVE_INSPECTION" and cand["contract_idx"] == 0:
                retry_hud = cand
                break
        if retry_hud is None or retry_hud["alert_state"] != "None":
            raise AssertionError(f"Once-per-contract guard failed on Ctr 01 retry: {retry_hud}")

        # =====================================================================
        # GATE 2 STAGE 2 (Shift 01 - Contract 02: CantileveredJHook)
        # =====================================================================
        run_remote_checked(remote, make_start_contract_snippet(1), "Gate 2 Stage 2: Start DA_Contract_02 (CantileveredJHook)")
        time.sleep(0.95)
        hud_s2 = extract_json_tag(
            run_remote_checked(remote, REMOTE_INSPECT_HUD_LAYOUT_SNIPPET, "Stage 2 Settled Check"),
            "HUD_JSON=",
        )
        assert_corner_hud_layout(hud_s2, "Stage 2 Settled")
        expected_s2_head = "ASYMMETRIC LOAD \u2014 [SCROLL] ROLL | HOLD [RMB] BEVEL"
        expected_s2_sub = "HOLD [SHIFT] FOR FAST ROTATION \u2014 BALANCE OVERHANG TORQUE"
        if (
            hud_s2["alert_state"] != "P3_TutorialHint"
            or hud_s2["headline"] != expected_s2_head
            or hud_s2["subtext"] != expected_s2_sub
            or hud_s2["pulse_active"]
        ):
            raise AssertionError(f"Stage 2 check failed: {hud_s2}")

        # Capture Screenshot #2: Stage 2 CantileveredJHook + Tutorial Banner
        focus_pie_viewport_window()
        run_remote_checked(
            remote,
            make_composited_screenshot_snippet(SHOT_STAGE2),
            "Capture Screenshot 2: Phase12Fix2_Stage2_JHook_Tutorial.png",
        )
        wait_for_file(SHOT_STAGE2)

        # Refinement A on Stage 2: MissedTarget air misfire MUST NOT dismiss Stage 2
        miss_s2 = extract_json_tag(
            run_remote_checked(
                remote,
                make_commit_slice_delta_z_snippet(-40.0),
                "Gate 2 Refinement A: Air Misfire (MissedTarget) on Stage 2",
            ),
            "SLICE_TEST_JSON=",
        )
        if miss_s2["outcome"] != "MISSED_TARGET" or miss_s2["alert_state_after"] != "P3_TutorialHint":
            raise AssertionError(f"Refinement A violated on Stage 2: {miss_s2}")

        # Stage 2 Dismissal: Roll plane >= 3.5 deg (2 wheel steps = 5.0 deg) -> immediate dismissal
        roll_s2 = extract_json_tag(
            run_remote_checked(remote, REMOTE_INJECT_STAGE2_ROLL_SNIPPET, "Gate 2 Stage 2: Roll Plane 5.0 deg -> Dismiss"),
            "ROLL_JSON=",
        )
        if roll_s2["alert_state_after"] != "None":
            raise AssertionError(f"Stage 2 failed immediate dismissal on 5.0 deg plane roll: {roll_s2}")

        # =====================================================================
        # GATE 2 STAGE 3 (Shift 01 - Contract 03: OffsetCChannel + Volatile Vein)
        # =====================================================================
        run_remote_checked(remote, make_start_contract_snippet(2), "Gate 2 Stage 3: Start DA_Contract_03 (OffsetCChannel)")
        time.sleep(0.95)
        hud_s3 = extract_json_tag(
            run_remote_checked(remote, REMOTE_INSPECT_HUD_LAYOUT_SNIPPET, "Stage 3 Settled Check (Refinement B Non-Preempted Spawn)"),
            "HUD_JSON=",
        )
        assert_corner_hud_layout(hud_s3, "Stage 3 Settled")
        expected_s3_head = "VOLATILE VEIN \u2014 PRESS [SPACE] TO ROUTE TARE SCRAP"
        expected_s3_sub = "DIVERT CONTAMINATED CUTS INTO THE RIGHT TARE SHAFT"
        if (
            hud_s3["alert_state"] != "P3_TutorialHint"
            or hud_s3["headline"] != expected_s3_head
            or hud_s3["subtext"] != expected_s3_sub
            or hud_s3["pulse_active"]
        ):
            raise AssertionError(f"Stage 3 Refinement B non-preempted spawn check failed: {hud_s3}")

        # Capture Screenshot #3: Stage 3 OffsetCChannel + Tutorial Banner
        focus_pie_viewport_window()
        run_remote_checked(
            remote,
            make_composited_screenshot_snippet(SHOT_STAGE3),
            "Capture Screenshot 3: Phase12Fix2_Stage3_CChannel_Tutorial.png",
        )
        wait_for_file(SHOT_STAGE3)

        # Verify P1 > P3 Priority Preemption when aiming onto OffsetCChannel above the lower volatile vein
        preempt_s3 = extract_json_tag(
            run_remote_checked(remote, REMOTE_AIM_INTO_CTR03_VEIN_DROP_SNIPPET, "Gate 2: Aim Into Vein Drop -> P1 Preempts P3"),
            "PREEMPT_JSON=",
        )
        if preempt_s3["alert_state"] not in ("P1_VeinInDrop", "P1_IsotopeHazard"):
            raise AssertionError(f"Expected P1 hazard to preempt P3_TutorialHint, got {preempt_s3}")

        # Toggle [SPACE] to TareChute -> clears P1_VeinInDrop AND permanently dismisses Stage 3!
        div_s3 = extract_json_tag(
            run_remote_checked(
                remote,
                REMOTE_TOGGLE_DIVERTER_AND_CHECK_STAGE3_SNIPPET,
                "Gate 2 Stage 3: Toggle [SPACE] to TareChute -> Dismiss Stage 3",
            ),
            "STAGE3_DIV_JSON=",
        )
        if div_s3["route"] != "TARE_CHUTE" or div_s3["alert_state_after"] != "None":
            raise AssertionError(f"Stage 3 failed dismissal on TareChute diverter toggle: {div_s3}")

        # =====================================================================
        # GATE 2 SHIFT-02 SUPPRESSION & CAREER RESET + 4.5s TIMER AUTO-DISMISS
        # =====================================================================
        run_remote_checked(remote, make_advance_to_shift_snippet(2, 0), "Gate 2: Advance to Shift 02 - Ctr 01")
        for s2_idx in (0, 1, 2):
            if s2_idx > 0:
                run_remote_checked(remote, make_start_contract_snippet(s2_idx), f"Gate 2: Shift 02 - Ctr 0{s2_idx + 1}")
            time.sleep(0.15)
            hud_shift2 = extract_json_tag(
                run_remote_checked(remote, REMOTE_INSPECT_HUD_LAYOUT_SNIPPET, f"Inspect Shift 02 Ctr 0{s2_idx + 1}"),
                "HUD_JSON=",
            )
            if hud_shift2["shift_num"] != 2 or hud_shift2["alert_state"] != "None":
                raise AssertionError(f"Shift 02 tutorial suppression violated on Ctr 0{s2_idx + 1}: {hud_shift2}")

        # Career Reset back to Shift 01 -> Stage 1 MUST re-arm and auto-dismiss after 4.5s FTimerHandle!
        run_remote_checked(remote, REMOTE_CAREER_RESET_SNIPPET, "Gate 2: Career Reset Re-Arm to Shift 01")
        time.sleep(0.15)
        hud_rearm = extract_json_tag(
            run_remote_checked(remote, REMOTE_INSPECT_HUD_LAYOUT_SNIPPET, "Verify Stage 1 Re-Armed After Career Reset"),
            "HUD_JSON=",
        )
        if hud_rearm["shift_num"] != 1 or hud_rearm["alert_state"] != "P3_TutorialHint":
            raise AssertionError(f"Career reset failed to re-arm Stage 1 P3_TutorialHint: {hud_rearm}")

        time.sleep(4.65)
        hud_autodismiss = extract_json_tag(
            run_remote_checked(remote, REMOTE_INSPECT_HUD_LAYOUT_SNIPPET, "Verify 4.5s FTimerHandle Auto-Dismiss"),
            "HUD_JSON=",
        )
        if hud_autodismiss["alert_state"] != "None":
            raise AssertionError(f"Stage 1 failed 4.5s FTimerHandle auto-dismiss: {hud_autodismiss}")

        # =====================================================================
        # GATE 3: CLEAN GAMEVIEWPORT LIT CAPTURE PIXEL-ROI & GPU FRAME BUDGET
        # =====================================================================
        if os.path.exists(LIT_CLEAN_PATH):
            os.remove(LIT_CLEAN_PATH)
        focus_pie_viewport_window()
        time.sleep(0.15)
        run_remote_checked(
            remote,
            make_clean_lit_viewport_screenshot_snippet(LIT_CLEAN_PATH),
            "Gate 3: Queue Clean 3D GameViewport Lit Screenshot (bShowUI=False)",
        )
        wait_for_file(LIT_CLEAN_PATH)
        run_remote_checked(
            remote,
            REMOTE_RESTORE_LASER_OVERLAY_SNIPPET,
            "Gate 3: Restore Laser Overlay Visibility",
        )

        focus_pie_viewport_window()
        time.sleep(0.50)
        steady_samples_ms: List[float] = []
        for s_idx in range(5):
            time.sleep(0.20)
            f_lines = run_remote_checked(
                remote,
                REMOTE_MEASURE_STEADY_FRAME_TIME_SNIPPET,
                f"Gate 3: Sample Unthrottled PIE Frame Time #{s_idx + 1}",
            )
            steady_samples_ms.append(float(extract_json_tag(f_lines, "FRAME_MS_JSON=")["delta_ms"]))
        steady_median_ms = statistics.median(steady_samples_ms)

        run_remote_checked(remote, REMOTE_END_PIE_SNIPPET, "End PIE Session Cleanly")
        time.sleep(0.50)

        clean_w, clean_h, clean_px = decode_png_rgb8(LIT_CLEAN_PATH)
        if os.path.exists(LIT_CLEAN_PATH):
            os.remove(LIT_CLEAN_PATH)
        if clean_w < 960 or clean_h < 540:
            raise AssertionError(f"Expected >= 960x540 clean GameViewport Lit capture, got ({clean_w}, {clean_h})")

        outer_left_box = [1, int(0.22 * clean_h), int(0.15 * clean_w), int(0.90 * clean_h)]
        outer_right_box = [int(0.85 * clean_w), int(0.22 * clean_h), clean_w - 1, int(0.90 * clean_h)]
        hud_tl_box = [int(0.01 * clean_w), int(0.01 * clean_h), int(0.35 * clean_w), int(0.15 * clean_h)]
        hud_tr_box = [int(0.65 * clean_w), int(0.01 * clean_h), int(0.99 * clean_w), int(0.15 * clean_h)]

        st_outer = compute_roi_stats(clean_w, clean_px, [outer_left_box, outer_right_box])
        st_outer_l = compute_roi_stats(clean_w, clean_px, [outer_left_box])
        st_outer_r = compute_roi_stats(clean_w, clean_px, [outer_right_box])
        st_hud_corners = compute_roi_stats(clean_w, clean_px, [hud_tl_box, hud_tr_box])

        outer_mean = st_outer["mean_luma"]
        outer_peak = st_outer["peak_luma"]
        hud_backing_mean = st_hud_corners["mean_luma"]

        # Gate 4: Validate all 3 composited UI PNGs
        shot_meta = {}
        for shot_path in (SHOT_STAGE1, SHOT_STAGE2, SHOT_STAGE3):
            if not os.path.exists(shot_path):
                raise AssertionError(f"Gate 4 missing screenshot: {shot_path}")
            sz = os.path.getsize(shot_path)
            sw, sh, _ = decode_png_rgb8(shot_path)
            if sz < 102400 or sw < 960 or sh < 540:
                raise AssertionError(f"Gate 4 invalid screenshot {shot_path}: size={sz} B, dims={sw}x{sh}")
            shot_meta[os.path.basename(shot_path)] = f"{sw}x{sh} ({sz} B)"

        vp_w, vp_h = float(hud_s1_settled["local_vp"][0]), float(hud_s1_settled["local_vp"][1])
        tl_u_max = float(hud_s1_settled["tl_box"][1][0]) / vp_w
        tr_u_min = float(hud_s1_settled["tr_box"][0][0]) / vp_w
        tl_v_max = float(hud_s1_settled["tl_box"][1][1]) / vp_h
        tr_v_max = float(hud_s1_settled["tr_box"][1][1]) / vp_h

        print("\n================ PHASE 12-FIX2 QUANTITATIVE 4-GATE REPORT ================")
        print(
            f"  [Gate 1] +50% Corner HUD & Typography: local_vp={hud_s1_settled['local_vp']}, "
            f"eff_corner_w={hud_s1_settled['eff_corner_w']} px (720p_probe={hud_720p['eff_corner_w']} px), "
            f"tl_desired_w={hud_s1_settled['tl_desired_w']} px, tr_desired_w={hud_s1_settled['tr_desired_w']} px, "
            f"tl_div_gap={hud_s1_settled['tl_div_y'] - hud_s1_settled['tl_hdr_bottom_y']:.1f} px, "
            f"tr_div_gap={hud_s1_settled['tr_div_y'] - hud_s1_settled['tr_hdr_bottom_y']:.1f} px, "
            f"tl_U_max={tl_u_max:.4f} (<0.40, clamp_U={audit_data['clamp_u_range']}), "
            f"tr_U_min={tr_u_min:.4f} (>0.60), tl_V_max={tl_v_max:.4f} / tr_V_max={tr_v_max:.4f} (<0.17, slit_top_V={audit_data['panopticon_slit_top_v']}), "
            f"overlaps=0"
        )
        print(
            f"  [Gate 2] Shift-01 3-Stage Tutorial: Stage1/2/3 non-preempted=PASS, "
            f"post_pulse_timer_stopped=PASS (bg_opacity={hud_s1_settled['bg_opacity']}), "
            f"RefinementA_MissedTarget_ignored=(S1:{miss_s1['alert_state_after']}, S2:{miss_s2['alert_state_after']}), "
            f"S1_Success_dismiss={hit_s1['alert_state_after']}, S1_fault_retry={retry_hud['alert_state']}, "
            f"S2_5deg_roll_dismiss={roll_s2['alert_state_after']}, S3_P1_preempt={preempt_s3['alert_state']}, "
            f"S3_Space_dismiss={div_s3['alert_state_after']}, Shift02_suppressed=PASS, "
            f"CareerReset_rearm={hud_rearm['alert_state']} -> 4.5s_timer_autodismiss={hud_autodismiss['alert_state']}"
        )
        print(
            f"  [Gate 3] Dark Cast-Iron Outer Pipes & Budget: instances={audit_data['framing_counts']} (total=64), "
            f"pipes_MID_parent={audit_data['pipes_parent']}, AlbedoTint={audit_data['pipes_albedo']}, "
            f"ConductorMinF0={audit_data['pipes_f0']}, outer_mean={outer_mean:.2f} sRGB [target 3.2..9.5] "
            f"(L={st_outer_l['mean_luma']:.2f}, R={st_outer_r['mean_luma']:.2f}, std={st_outer['std_luma']:.2f}), "
            f"outer_peak={outer_peak:.2f} sRGB [target 20.0..58.0], hud_backing_mean={hud_backing_mean:.2f} sRGB [<=12.0], "
            f"shadow_lights={audit_data['shadow_lights']}, zero_shadow_lights={audit_data['zero_shadow_lights']}, "
            f"VolumetricFog={audit_data['volumetric_fog']}, steady_frame_ms={steady_median_ms:.2f} ms (samples={steady_samples_ms}) [<=13.0 ms]"
        )
        print(f"  [Gate 4] Composited UI Screenshots: {shot_meta}")
        print("==========================================================================\n")

        failures: List[str] = []
        if not (3.2 <= outer_mean <= 9.5):
            failures.append(f"Gate 3 outer_mean={outer_mean:.2f} sRGB outside [3.2, 9.5]")
        if not (20.0 <= outer_peak <= 58.0):
            failures.append(f"Gate 3 outer_peak={outer_peak:.2f} sRGB outside [20.0, 58.0]")
        if hud_backing_mean > 12.0:
            failures.append(f"Gate 3 hud_backing_mean={hud_backing_mean:.2f} sRGB > 12.0")
        if steady_median_ms > 13.0:
            failures.append(f"Gate 3 steady_median_ms={steady_median_ms:.2f} ms > 13.0 ms")

        if failures:
            raise AssertionError("PHASE 12-FIX2 GATE FAILURES:\n  - " + "\n  - ".join(failures))

        print("[VerifyPhase12Fix2] ALL 4 QUANTITATIVE GATES & REFINEMENTS A/B/C/D PASSED.")
        return 0
    finally:
        if os.path.exists(LIT_CLEAN_PATH):
            try:
                os.remove(LIT_CLEAN_PATH)
            except Exception:
                pass
        if args.shutdown or spawned_proc is not None:
            print("[VerifyPhase12Fix2] Sending clean quit_editor() command...")
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
