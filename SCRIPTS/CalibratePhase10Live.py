import argparse
import os
import struct
import sys
import time
import zlib
from typing import Dict, List, Tuple

UE5_ROOT = os.environ.get("UE5_ROOT", r"C:\Program Files\Epic Games\UE_5.8")
REMOTE_EXEC_DIR = os.path.join(
    UE5_ROOT, "Engine", "Plugins", "Experimental", "PythonScriptPlugin", "Content", "Python"
)
if REMOTE_EXEC_DIR not in sys.path:
    sys.path.insert(0, REMOTE_EXEC_DIR)

import remote_execution  # type: ignore  # noqa: E402


REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BUILD_MATERIALS_SCRIPT = os.path.join(REPO_ROOT, "Scripts", "BuildMaterialsAndBind.py").replace("\\", "/")
LOGS_DIR = os.path.join(REPO_ROOT, "Saved", "Logs").replace("\\", "/")
LIT_CAPTURE_FILENAME = "Phase11A_Fix_LitCapture.png"
CLEAN_PROBE_FILENAME = "_Phase11A_Fix_CleanMetalProbe.png"
LIT_CAPTURE_PATH = os.path.join(REPO_ROOT, "Saved", "Logs", LIT_CAPTURE_FILENAME)
CLEAN_PROBE_PATH = os.path.join(REPO_ROOT, "Saved", "Logs", CLEAN_PROBE_FILENAME)


REMOTE_BEGIN_PIE_SNIPPET = """
import unreal
try:
    perf_cdo = unreal.find_object(None, "/Script/UnrealEd.Default__EditorPerformanceSettings")
    if perf_cdo is not None:
        perf_cdo.set_editor_property("throttle_cpu_when_not_foreground", False)
except Exception:
    pass
unreal.SystemLibrary.execute_console_command(None, "r.VSync 0")
unreal.SystemLibrary.execute_console_command(None, "r.VSyncEditor 0")
unreal.SystemLibrary.execute_console_command(None, "Slate.MaxFPS 240")
unreal.SystemLibrary.execute_console_command(None, "t.MaxFPS 240")
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les.is_in_play_in_editor():
    les.editor_request_end_play()
les.editor_request_begin_play()
unreal.log("[CalibratePhase10Live] Requested BeginPlay PIE session.")
"""


REMOTE_VERIFY_PIE_SNIPPET = """
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
if pie_world is None:
    raise RuntimeError("PIE GameWorld is None; PIE did not start in time!")

world_time_sec = float(unreal.GameplayStatics.get_time_seconds(pie_world))
if world_time_sec < 0.35:
    raise RuntimeError(f"PIE warmup in progress (world_time={world_time_sec:.3f}s < 0.35s)...")

all_actors = unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor)

shadow_lights = []
zero_shadow_lights = []
sky_lights = []
spot_lights = []
point_lights = []
fog_comps = []
relief_isms = []
inspector_pawns = []
scale_actors = []
monolith_rigs = []
shell_slabs_unoccluded = []
floor_slabs_grounded = []
world_grid_offenders = []

for actor in all_actors:
    cls_name = actor.get_class().get_name()
    if "InspectorPawn" in cls_name:
        inspector_pawns.append(actor)
    if "ScaleAssemblyActor" in cls_name:
        scale_actors.append(actor)
    if "MonolithRigActor" in cls_name:
        monolith_rigs.append(actor)

    if cls_name == "StaticMeshActor":
        smc = actor.get_editor_property("static_mesh_component")
        if smc is not None and smc.is_visible():
            loc = actor.get_actor_location()
            df = bool(smc.get_editor_property("affect_distance_field_lighting"))
            ind = bool(smc.get_editor_property("affect_dynamic_indirect_lighting"))
            rt = bool(smc.get_editor_property("visible_in_ray_tracing"))
            if float(loc.z) < -80.0:
                floor_slabs_grounded.append((float(loc.x), float(loc.y), float(loc.z), df, ind, rt))
            else:
                shell_slabs_unoccluded.append((float(loc.x), float(loc.y), float(loc.z), df, ind, rt))

    for comp in actor.get_components_by_class(unreal.LightComponentBase):
        casts = bool(comp.get_editor_property("cast_shadows"))
        entry = f"{cls_name}.{comp.get_name()}(intensity={comp.get_editor_property('intensity')})"
        if casts:
            shadow_lights.append(entry)
        else:
            zero_shadow_lights.append(entry)
        if isinstance(comp, unreal.SkyLightComponent):
            sky_lights.append(comp)
        if isinstance(comp, unreal.SpotLightComponent):
            spot_lights.append(comp)
        elif isinstance(comp, unreal.PointLightComponent):
            point_lights.append(comp)

    for fog in actor.get_components_by_class(unreal.ExponentialHeightFogComponent):
        fog_comps.append(fog)

    for ism in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
        relief_isms.append(ism)

    for smc in actor.get_components_by_class(unreal.StaticMeshComponent):
        if smc.is_visible():
            for m_idx in range(max(1, smc.get_num_materials())):
                mat = smc.get_material(m_idx)
                if mat is not None:
                    base_mat = mat.get_base_material()
                    bname = "" if base_mat is None else base_mat.get_name()
                    if "WorldGridMaterial" in mat.get_name() or "WorldGridMaterial" in bname:
                        world_grid_offenders.append(f"{cls_name}.{smc.get_name()}[{m_idx}]")

unreal.log(f"[CalibratePhase10Live] Shadow-casting lights ({len(shadow_lights)}): {shadow_lights}")
unreal.log(f"[CalibratePhase10Live] Zero-shadow lights ({len(zero_shadow_lights)}): {zero_shadow_lights}")

if world_grid_offenders:
    raise AssertionError(f"WorldGridMaterial fallback detected in PIE: {world_grid_offenders}")

if len(shadow_lights) != 1:
    raise AssertionError(f"Expected exactly 1 shadow-casting light, found {len(shadow_lights)}: {shadow_lights}")
if len(zero_shadow_lights) != 8:
    raise AssertionError(f"Expected exactly 8 zero-shadow lights, found {len(zero_shadow_lights)}: {zero_shadow_lights}")

# Verify 5 perimeter shell slabs are unoccluded in SWRT + HWRT Lumen while Floor remains grounded
if len(shell_slabs_unoccluded) != 5 or len(floor_slabs_grounded) != 1:
    raise AssertionError(
        f"Expected 5 perimeter shell slabs + 1 floor slab, found shell={shell_slabs_unoccluded}, floor={floor_slabs_grounded}"
    )
for sx, sy, sz, df, ind, rt in shell_slabs_unoccluded:
    if df or ind or rt:
        raise AssertionError(
            f"Perimeter shell slab at ({sx:.1f},{sy:.1f},{sz:.1f}) still occludes Lumen: df={df}, indirect={ind}, ray_tracing={rt}"
        )

# Verify area-light SourceRadius / SourceLength on KeySpot, RimSpots, MidgroundWashSpot, and FillPoint
for s in spot_lights:
    sr = float(s.get_editor_property("source_radius"))
    sl = float(s.get_editor_property("source_length"))
    if sr < 15.0 or sl < 80.0:
        raise AssertionError(f"SpotLight {s.get_name()} missing industrial tube area dimensions: radius={sr}, length={sl}")

key_spots = [s for s in spot_lights if bool(s.get_editor_property("cast_shadows"))]
if len(key_spots) != 1:
    raise AssertionError(f"Expected 1 shadow-casting KeySpot, found {len(key_spots)}")
key_fwd = unreal.MathLibrary.get_forward_vector(key_spots[0].get_world_rotation())
if float(key_fwd.x) < 0.30 or float(key_fwd.z) > -0.80:
    raise AssertionError(f"KeySpot CDO -90deg rotation bug detected: fwd={key_fwd} (expected +X/-Z onto hero spine)")

pit_glow_spots = [
    s for s in spot_lights
    if not bool(s.get_editor_property("cast_shadows"))
    and abs(float(s.get_editor_property("attenuation_radius")) - 325.0) < 1.0
]
if len(pit_glow_spots) != 1:
    raise AssertionError(f"Expected 1 MidgroundWashSpot with R=325 (< 360), found {len(pit_glow_spots)}")
pit_spot = pit_glow_spots[0]
pit_loc = pit_spot.get_world_location()
pit_fwd = unreal.MathLibrary.get_forward_vector(pit_spot.get_world_rotation())
pit_outer = float(pit_spot.get_editor_property("outer_cone_angle"))
if float(pit_loc.x) > 350.0 or float(pit_fwd.x) < 0.98 or not (58.0 <= pit_outer <= 68.5):
    raise AssertionError(
        f"MidgroundWashSpot incidence guard failed: loc={pit_loc}, fwd={pit_fwd}, outer={pit_outer}"
    )

if len(sky_lights) != 1:
    raise AssertionError(f"Expected 1 SkyLightComponent in PIE, found {len(sky_lights)}")
sky = sky_lights[0]
sky_cubemap = sky.get_editor_property("cubemap")
sky_int = float(sky.get_editor_property("intensity"))
if sky_cubemap is None or "TC_CellAmbientCubemap" not in sky_cubemap.get_path_name():
    raise AssertionError(f"SkyLight cubemap invalid: {sky_cubemap}")
if bool(sky.get_editor_property("cast_shadows")) or sky_int < 0.85:
    raise AssertionError(f"SkyLight guard failed: cast_shadows={sky.get_editor_property('cast_shadows')}, intensity={sky_int}")

if len(fog_comps) != 1:
    raise AssertionError(f"Expected 1 ExponentialHeightFogComponent in PIE, found {len(fog_comps)}")
fog = fog_comps[0]
if bool(fog.get_editor_property("enable_volumetric_fog")):
    raise AssertionError("ExponentialHeightFog volumetric fog must be False!")
fog_density = float(fog.get_editor_property("fog_density"))
fog_start = float(fog.get_editor_property("start_distance"))
if abs(fog_density - 0.015) > 0.002 or fog_start < 385.0:
    raise AssertionError(f"Phase 11A Fog guard failed: density={fog_density}, start={fog_start}")

if len(relief_isms) != 2:
    raise AssertionError(f"Expected 2 Rodin Foundry Shaft ISMs, found {len(relief_isms)}")

global_min_relief_x = 1.0e9
global_max_relief_x = -1.0e9
ism_summary = []
for ism in relief_isms:
    if ism.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION:
        raise AssertionError(f"{ism.get_name()} collision is not NoCollision!")
    if bool(ism.get_editor_property("cast_shadow")):
        raise AssertionError(f"{ism.get_name()} cast_shadow must be False!")
    sm = ism.get_editor_property("static_mesh")
    if sm is None or "BasicShapes/Cube" in sm.get_path_name():
        raise AssertionError(f"{ism.get_name()} uses forbidden mesh: {sm}")
    mat0 = ism.get_material(0)
    if mat0 is None or not bool(mat0.get_base_material().get_editor_property("used_with_instanced_static_meshes")):
        raise AssertionError(f"{ism.get_name()} material missing MATUSAGE_INSTANCED_STATIC_MESHES: {mat0}")
    bbox = sm.get_bounding_box()
    bmin, bmax = bbox.min, bbox.max
    corners = [
        unreal.Vector(x, y, z)
        for x in (bmin.x, bmax.x)
        for y in (bmin.y, bmax.y)
        for z in (bmin.z, bmax.z)
    ]
    inst_count = ism.get_instance_count()
    for idx in range(inst_count):
        xf = ism.get_instance_transform(idx, world_space=True)
        for c in corners:
            wc = unreal.MathLibrary.transform_location(xf, c)
            global_min_relief_x = min(global_min_relief_x, float(wc.x))
            global_max_relief_x = max(global_max_relief_x, float(wc.x))
    ism_summary.append(f"{ism.get_name()}:{sm.get_name()}x{inst_count}({mat0.get_name()})")

if global_min_relief_x < 429.5 or global_max_relief_x > 465.5:
    raise AssertionError(
        f"Foundry Shaft X-bounds [{global_min_relief_x:.2f}, {global_max_relief_x:.2f}] outside [430, 465] cm!"
    )

if len(scale_actors) != 1 or len(inspector_pawns) != 1 or len(monolith_rigs) != 1:
    raise AssertionError(
        f"Expected 1 Scale, 1 Pawn, 1 MonolithRig; got scale={len(scale_actors)}, pawn={len(inspector_pawns)}, rig={len(monolith_rigs)}"
    )

scale_actor = scale_actors[0]
flapper_vis = scale_actor.get_editor_property("diverter_flapper_visual")
flapper_mat = flapper_vis.get_material(0)
if flapper_mat is None or "MI_Station_DiverterFlapper" not in flapper_mat.get_name():
    raise AssertionError(f"DiverterFlapperVisual material is {flapper_mat}, expected MI_Station_DiverterFlapper!")

pawn = inspector_pawns[0]
cam = pawn.get_editor_property("inspector_camera")
cam_fov = float(cam.get_editor_property("field_of_view"))
cam_loc = cam.get_world_location()
pp = cam.get_editor_property("post_process_settings")
bloom_thresh = float(pp.get_editor_property("bloom_threshold"))
exp_bias = float(pp.get_editor_property("auto_exposure_bias"))
le_hi = float(pp.get_editor_property("local_exposure_highlight_contrast_scale"))
le_sh = float(pp.get_editor_property("local_exposure_shadow_contrast_scale"))
vignette = float(pp.get_editor_property("vignette_intensity"))

if abs(cam_fov - 85.0) > 0.2:
    raise AssertionError(f"Expected InspectorCamera FOV=85.0, got {cam_fov:.2f}")
if bloom_thresh < 1.65 - 1.0e-3:
    raise AssertionError(f"HUD Firewall failed: bloom_threshold={bloom_thresh} < 1.65!")
if abs(exp_bias - 0.85) > 0.15:
    raise AssertionError(f"Expected auto_exposure_bias~0.85, got {exp_bias:.3f}")
if abs(vignette - 0.36) > 0.01:
    raise AssertionError(f"Expected vignette_intensity=0.36, got {vignette:.3f}")
if abs(le_hi - 1.0) > 1.0e-3 or abs(le_sh - 1.0) > 1.0e-3:
    raise AssertionError(f"LocalExposure neutral guard failed: hi={le_hi}, sh={le_sh}")

delta_ms = float(unreal.GameplayStatics.get_world_delta_seconds(pie_world)) * 1000.0
if delta_ms > 13.0:
    unreal.SystemLibrary.execute_console_command(pie_world, "r.VSync 0")
    unreal.SystemLibrary.execute_console_command(pie_world, "r.VSyncEditor 0")
    unreal.SystemLibrary.execute_console_command(pie_world, "Slate.MaxFPS 240")
    unreal.SystemLibrary.execute_console_command(pie_world, "t.MaxFPS 240")
    raise AssertionError(f"PIE steady-state frame/GPU time {delta_ms:.2f} ms exceeds 13.0 ms target!")

unreal.log(
    f"[CalibratePhase10Live] COMPONENT & FRAME BUDGET GUARDS PASSED: "
    f"world_time={world_time_sec:.2f}s, shadow_lights=1, zero_shadow_lights=8, "
    f"unoccluded_shell_slabs=5, sky_int={sky_int:.2f}, exp_bias={exp_bias:.2f}, "
    f"foundry_shaft={ism_summary}, frame_delta_ms={delta_ms:.2f}ms"
)
"""


def make_spawn_lit_capture_snippet() -> str:
    return """
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
if pie_world is None:
    raise RuntimeError("PIE GameWorld is None!")

pawns = [
    a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor)
    if "InspectorPawn" in a.get_class().get_name()
]
if len(pawns) != 1:
    raise RuntimeError(f"Expected 1 InspectorPawn, found {len(pawns)}")
pawn = pawns[0]
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
    raise RuntimeError("ATareTitheGameMode::SpawnTransientLitCapture2D returned None!")

t0 = float(unreal.GameplayStatics.get_time_seconds(pie_world))
unreal.log(f"[CalibratePhase10Live] Spawned Lumen-enabled SceneCapture2D (1280x720 SCS_FinalColorLDR) at world_time={t0:.3f}s")
"""


def make_export_full_hud_and_switch_clean_snippet(logs_dir: str, full_filename: str) -> str:
    return f"""
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
if pie_world is None:
    raise RuntimeError("PIE GameWorld is None!")

caps = [
    a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.SceneCapture2D)
    if unreal.Name("Phase11AFixTransientLitCapture") in list(a.tags)
]
if len(caps) != 1:
    raise RuntimeError(f"Expected 1 transient SceneCapture2D, found {{len(caps)}}")

cap_actor = caps[0]
scc = cap_actor.get_editor_property("capture_component2d")
rt = scc.get_editor_property("texture_target")

unreal.RenderingLibrary.export_render_target(pie_world, rt, {logs_dir!r}, {full_filename!r})
t1 = float(unreal.GameplayStatics.get_time_seconds(pie_world))
unreal.log(f"[CalibratePhase10Live] Exported Full-HUD Lit capture to {logs_dir}/{full_filename} at world_time={{t1:.3f}}s")

# Hide UnlitText HUD & translucent LaserSheet/LaserFrame bars for clean non-HUD metal ROI readback
hidden_count = 0
for actor in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor):
    for tc in actor.get_components_by_class(unreal.TextRenderComponent):
        scc.hide_component(tc)
        hidden_count += 1
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
                hidden_count += 1

unreal.log(f"[CalibratePhase10Live] Hid {{hidden_count}} HUD/laser overlay components on SceneCapture2D for clean metal ROI probe.")
"""


def make_export_clean_probe_and_destroy_snippet(logs_dir: str, clean_filename: str) -> str:
    return f"""
import json
import math
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
if pie_world is None:
    raise RuntimeError("PIE GameWorld is None!")

caps = [
    a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.SceneCapture2D)
    if unreal.Name("Phase11AFixTransientLitCapture") in list(a.tags)
]
if len(caps) != 1:
    raise RuntimeError(f"Expected 1 transient SceneCapture2D, found {{len(caps)}}")

cap_actor = caps[0]
scc = cap_actor.get_editor_property("capture_component2d")
rt = scc.get_editor_property("texture_target")

unreal.RenderingLibrary.export_render_target(pie_world, rt, {logs_dir!r}, {clean_filename!r})

# Project exact 3D station & foundry shaft world bounds onto 1280x720 camera screen space
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

def comp_world_aabb(comp):
    sm = comp.get_editor_property("static_mesh")
    bbox = sm.get_bounding_box()
    bmin, bmax = bbox.min, bbox.max
    xf = comp.get_world_transform()
    pts = [
        unreal.MathLibrary.transform_location(xf, unreal.Vector(x, y, z))
        for x in (bmin.x, bmax.x)
        for y in (bmin.y, bmax.y)
        for z in (bmin.z, bmax.z)
    ]
    return (
        [min(float(p.x) for p in pts), min(float(p.y) for p in pts), min(float(p.z) for p in pts)],
        [max(float(p.x) for p in pts), max(float(p.y) for p in pts), max(float(p.z) for p in pts)],
    )

def union_aabb(boxes):
    return (
        [min(b[0][0] for b in boxes), min(b[0][1] for b in boxes), min(b[0][2] for b in boxes)],
        [max(b[1][0] for b in boxes), max(b[1][1] for b in boxes), max(b[1][2] for b in boxes)],
    )

rigs = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()]
scales = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "ScaleAssemblyActor" in a.get_class().get_name()]
rig = rigs[0]
scale = scales[0]

clamp_boxes = [comp_world_aabb(rig.get_editor_property("clamp_jaw_visual"))]
housing_comp = rig.get_editor_property("clamp_spring_housing_visual")
if housing_comp is not None and housing_comp.is_visible() and housing_comp.get_editor_property("static_mesh") is not None:
    clamp_boxes.append(comp_world_aabb(housing_comp))
clamp_min, clamp_max = union_aabb(clamp_boxes)
roi_clamp = project_world_box(clamp_min, clamp_max, shrink_x=0.14, shrink_y=0.10)

flapper_box = comp_world_aabb(scale.get_editor_property("diverter_flapper_visual"))
pan_box = comp_world_aabb(scale.get_editor_property("pan_platform_visual"))
plate_box = comp_world_aabb(scale.get_editor_property("readout_backing_plate"))
plate_px = project_world_box(plate_box[0], plate_box[1])

roi_flapper = project_world_box(flapper_box[0], flapper_box[1], shrink_x=0.10, shrink_y=0.06)
roi_scale_pan = project_world_box(pan_box[0], pan_box[1], shrink_x=0.08, shrink_y=0.06)
# Keep strictly above ReadoutBackingPlate top edge and avoid double-counting overlap between flapper & scale pan
roi_scale_pan[3] = min(roi_scale_pan[3], max(roi_scale_pan[1] + 20, plate_px[1] - 2))
roi_flapper[3] = min(roi_flapper[3], roi_scale_pan[1])

# Unobstructed left & right bays of FoundryGrateShaftISM (X=444, |Y|<=140) + FoundryFlankColumnISM (X=432, Y=+-155)
roi_grate_left = project_world_box([432.0, -168.0, 35.0], [452.0, -58.0, 175.0])
roi_grate_right = project_world_box([432.0, 58.0, 35.0], [452.0, 168.0, 175.0])

roi_payload = {{
    "clamp": roi_clamp,
    "flapper": roi_flapper,
    "scale_pan": roi_scale_pan,
    "grate_left": roi_grate_left,
    "grate_right": roi_grate_right,
    "border_left": [0, int(0.10 * H), int(0.10 * W), int(0.90 * H)],
    "border_right": [int(0.90 * W), int(0.10 * H), W, int(0.90 * H)],
}}

# Cleanly destroy transient capture component & actor (unreferencing transient render target for GC)
scc.set_editor_property("capture_every_frame", False)
scc.set_editor_property("texture_target", None)
cap_actor.destroy_actor()

unreal.log("ROI_JSON=" + json.dumps(roi_payload))
"""


REMOTE_END_PIE_SNIPPET = """
import unreal
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les.is_in_play_in_editor():
    les.editor_request_end_play()
unreal.log("[CalibratePhase10Live] Ended PIE verification session cleanly without saving maps.")
"""


REMOTE_QUIT_EDITOR_SNIPPET = """
import unreal
unreal.log("[CalibratePhase10Live] Requesting clean Editor exit (no map save)...")
unreal.SystemLibrary.quit_editor()
"""


def decode_png_rgb8(png_path: str) -> Tuple[int, int, List[Tuple[int, int, int]]]:
    """Pure-Python 8-bit RGB/RGBA PNG reader returning (width, height, flat_rgb_tuples)."""
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
    sum_luma = 0.0
    sum_avg = 0.0
    sum_r = 0.0
    sum_g = 0.0
    sum_b = 0.0
    peak_luma = 0.0
    peak_avg = 0.0
    count = 0
    for x0, y0, x1, y1 in boxes:
        for y in range(y0, y1):
            row_idx = y * width
            for x in range(x0, x1):
                r, g, b = pixels[row_idx + x]
                luma = 0.2126 * r + 0.7152 * g + 0.0722 * b
                avg = (r + g + b) / 3.0
                sum_luma += luma
                sum_avg += avg
                sum_r += r
                sum_g += g
                sum_b += b
                if luma > peak_luma:
                    peak_luma = luma
                if avg > peak_avg:
                    peak_avg = avg
                count += 1
    if count == 0:
        raise RuntimeError(f"Empty ROI pixel box: {boxes}")
    return {
        "count": float(count),
        "mean_luma": sum_luma / count,
        "mean_avg": sum_avg / count,
        "peak_luma": peak_luma,
        "peak_avg": peak_avg,
        "mean_r": sum_r / count,
        "mean_g": sum_g / count,
        "mean_b": sum_b / count,
    }


def run_remote_checked(remote: remote_execution.RemoteExecution, code_or_file: str, label: str) -> List[str]:
    print(f"[CalibratePhase10Live] >>> Executing remote step: {label}")
    res = remote.run_command(
        code_or_file,
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
    print(f"[CalibratePhase10Live] <<< Step '{label}' SUCCEEDED.")
    return lines


def main() -> int:
    import json

    parser = argparse.ArgumentParser(
        description="Live 127.0.0.1 Python bridge calibration & Lit pixel-luminance PIE gate for Phase 11A-Fix"
    )
    parser.add_argument("--timeout", type=float, default=90.0, help="Seconds to wait for UnrealEditor remote node")
    parser.add_argument("--shutdown", action="store_true", help="Cleanly quit UnrealEditor after calibration/verification")
    args = parser.parse_args()

    os.makedirs(os.path.join(REPO_ROOT, "Saved", "Logs"), exist_ok=True)
    for stale_path in (LIT_CAPTURE_PATH, CLEAN_PROBE_PATH):
        if os.path.exists(stale_path):
            os.remove(stale_path)

    cfg = remote_execution.RemoteExecutionConfig()
    cfg.multicast_ttl = 0
    cfg.multicast_bind_address = "127.0.0.1"
    cfg.multicast_group_endpoint = ("239.0.0.1", 6766)
    cfg.command_endpoint = ("127.0.0.1", 6776)

    remote = remote_execution.RemoteExecution(cfg)
    remote.start()
    try:
        print("[CalibratePhase10Live] Discovering UnrealEditor Python node on 127.0.0.1...")
        deadline = time.time() + args.timeout
        target_node = None
        while time.time() < deadline:
            nodes = remote.remote_nodes
            for node in nodes:
                proj = str(node.get("project_name", ""))
                if "TareAndTithe" in proj or len(nodes) == 1:
                    target_node = node
                    break
            if target_node is not None:
                break
            time.sleep(1.0)

        if target_node is None:
            raise TimeoutError(
                f"Timed out after {args.timeout}s waiting for UnrealEditor bRemoteExecution node on 127.0.0.1"
            )

        node_id = target_node["node_id"]
        print(
            f"[CalibratePhase10Live] Connected to node_id={node_id} "
            f"(project={target_node.get('project_name')}, engine={target_node.get('engine_version')})"
        )
        remote.open_command_connection(node_id)

        # 1. Build/compile all Phase 11A-Fix materials, 9 semi-metallic MI_Station_* instances, cubemap & DA_Presentation
        run_remote_checked(remote, BUILD_MATERIALS_SCRIPT, "BuildMaterialsAndBind.py")

        # 2. Launch live PIE session and verify spawned components, Lumen shell flags, area lights & <=13ms frame budget
        run_remote_checked(remote, REMOTE_BEGIN_PIE_SNIPPET, "Begin PIE Session")

        pie_verified = False
        last_err = None
        for attempt in range(1, 10):
            time.sleep(1.0)
            try:
                run_remote_checked(remote, REMOTE_VERIFY_PIE_SNIPPET, f"Verify Live PIE World (attempt {attempt})")
                pie_verified = True
                break
            except Exception as exc:
                last_err = exc
                print(f"[CalibratePhase10Live] Waiting for PIE world initialization ({exc})...")

        if not pie_verified:
            run_remote_checked(remote, REMOTE_END_PIE_SNIPPET, "End PIE Session (after failure)")
            raise RuntimeError(f"Live PIE component verification failed: {last_err}")

        # 3. Spawn Lumen-enabled SceneCapture2D and pump >= 35 real PIE frames (~1.0s) for Lumen GI/Reflections & Exposure convergence
        run_remote_checked(remote, make_spawn_lit_capture_snippet(), "Spawn Lumen SceneCapture2D")
        time.sleep(1.0)
        run_remote_checked(
            remote,
            make_export_full_hud_and_switch_clean_snippet(LOGS_DIR, LIT_CAPTURE_FILENAME),
            "Export Full-HUD Lit PNG & Switch to Clean-Metal Probe",
        )
        time.sleep(0.5)
        probe_lines = run_remote_checked(
            remote,
            make_export_clean_probe_and_destroy_snippet(LOGS_DIR, CLEAN_PROBE_FILENAME),
            "Export Clean-Metal Lit Probe & Destroy Transient Capture",
        )

        run_remote_checked(remote, REMOTE_END_PIE_SNIPPET, "End PIE Session")
        time.sleep(0.8)

        roi_json_str = None
        for ln in probe_lines:
            idx = ln.find("ROI_JSON=")
            if idx >= 0:
                roi_json_str = ln[idx + len("ROI_JSON=") :]
        if not roi_json_str:
            raise RuntimeError("Missing ROI_JSON payload from PIE capture step!")
        rois = json.loads(roi_json_str)

        if not os.path.exists(LIT_CAPTURE_PATH):
            raise RuntimeError(f"Expected Lit capture PNG missing at {LIT_CAPTURE_PATH}")
        if not os.path.exists(CLEAN_PROBE_PATH):
            raise RuntimeError(f"Expected Clean Metal probe PNG missing at {CLEAN_PROBE_PATH}")

        try:
            full_w, full_h, full_pixels = decode_png_rgb8(LIT_CAPTURE_PATH)
            clean_w, clean_h, clean_pixels = decode_png_rgb8(CLEAN_PROBE_PATH)
        finally:
            if os.path.exists(CLEAN_PROBE_PATH):
                os.remove(CLEAN_PROBE_PATH)

        if (full_w, full_h) != (1280, 720) or (clean_w, clean_h) != (1280, 720):
            raise AssertionError(f"Unexpected capture dimensions: full=({full_w},{full_h}), clean=({clean_w},{clean_h})")

        st_clamp = compute_roi_stats(clean_w, clean_pixels, [rois["clamp"]])
        st_div = compute_roi_stats(clean_w, clean_pixels, [rois["flapper"], rois["scale_pan"]])
        st_grate = compute_roi_stats(clean_w, clean_pixels, [rois["grate_left"], rois["grate_right"]])
        st_border_clean = compute_roi_stats(clean_w, clean_pixels, [rois["border_left"], rois["border_right"]])
        st_border_full = compute_roi_stats(full_w, full_pixels, [rois["border_left"], rois["border_right"]])

        print("[CalibratePhase10Live] === ACTUAL RENDERED `LIT` PIXEL-LUMINANCE REPORT (0..255 sRGB) ===")
        print(
            f"  1. Upper Clamp Housing & Jaws ROI {rois['clamp']}: "
            f"mean_luma={st_clamp['mean_luma']:.2f}, mean_rgb={st_clamp['mean_avg']:.2f}, "
            f"peak_luma={st_clamp['peak_luma']:.2f}, peak_rgb={st_clamp['peak_avg']:.2f}, "
            f"RGB=({st_clamp['mean_r']:.1f},{st_clamp['mean_g']:.1f},{st_clamp['mean_b']:.1f})"
        )
        print(
            f"  2. Diverter Flapper & Scale Pan/Base ROI {rois['flapper']} + {rois['scale_pan']}: "
            f"mean_luma={st_div['mean_luma']:.2f}, mean_rgb={st_div['mean_avg']:.2f}, "
            f"peak_luma={st_div['peak_luma']:.2f}, peak_rgb={st_div['peak_avg']:.2f}, "
            f"RGB=({st_div['mean_r']:.1f},{st_div['mean_g']:.1f},{st_div['mean_b']:.1f})"
        )
        print(
            f"  3. Rear Foundry Grate Shaft & Flanking Columns ROI {rois['grate_left']} + {rois['grate_right']}: "
            f"mean_luma={st_grate['mean_luma']:.2f}, mean_rgb={st_grate['mean_avg']:.2f}, "
            f"peak_luma={st_grate['peak_luma']:.2f}, peak_rgb={st_grate['peak_avg']:.2f}, "
            f"RGB=({st_grate['mean_r']:.1f},{st_grate['mean_g']:.1f},{st_grate['mean_b']:.1f})"
        )
        print(
            f"  4. Outer Left/Right Vignette Borders (u<0.10, u>0.90): "
            f"full_mean_luma={st_border_full['mean_luma']:.2f}, full_mean_rgb={st_border_full['mean_avg']:.2f}, "
            f"clean_mean_luma={st_border_clean['mean_luma']:.2f}, clean_mean_rgb={st_border_clean['mean_avg']:.2f}"
        )

        # Gate 1: Upper Clamp Housing & Jaws >= 42 mean (<= 135 anti-blowout), >= 125 peak
        if (
            min(st_clamp["mean_luma"], st_clamp["mean_avg"]) < 42.0
            or max(st_clamp["mean_luma"], st_clamp["mean_avg"]) > 135.0
            or min(st_clamp["peak_luma"], st_clamp["peak_avg"]) < 125.0
        ):
            raise AssertionError(
                f"ROI 1 (Upper Clamp Housing & Jaws) FAILED gate (42..135 mean, >=125 peak): {st_clamp}"
            )

        # Gate 2: Diverter Flapper & Scale Pan/Base >= 36 mean (<= 135 anti-blowout), >= 110 peak
        if (
            min(st_div["mean_luma"], st_div["mean_avg"]) < 36.0
            or max(st_div["mean_luma"], st_div["mean_avg"]) > 135.0
            or min(st_div["peak_luma"], st_div["peak_avg"]) < 110.0
        ):
            raise AssertionError(
                f"ROI 2 (Diverter Flapper & Scale Pan/Base) FAILED gate (36..135 mean, >=110 peak): {st_div}"
            )

        # Gate 3: Rear Foundry Grate Shaft & Flanking Columns >= 26 mean (<= 115 anti-blowout), >= 85 peak + visibly warm amber (R > G > B)
        if (
            min(st_grate["mean_luma"], st_grate["mean_avg"]) < 26.0
            or max(st_grate["mean_luma"], st_grate["mean_avg"]) > 115.0
            or min(st_grate["peak_luma"], st_grate["peak_avg"]) < 85.0
            or not (st_grate["mean_r"] > st_grate["mean_g"] > st_grate["mean_b"])
        ):
            raise AssertionError(
                f"ROI 3 (Rear Foundry Grate Shaft & Flanking Columns) FAILED gate (26..115 mean, >=85 peak, warm R>G>B): {st_grate}"
            )

        # Gate 4: Outer Left/Right Vignette Borders <= 16 mean
        if max(st_border_full["mean_luma"], st_border_full["mean_avg"], st_border_clean["mean_luma"], st_border_clean["mean_avg"]) > 16.0:
            raise AssertionError(
                f"ROI 4 (Outer Left/Right Vignette Borders) FAILED gate (<=16 mean): full={st_border_full}, clean={st_border_clean}"
            )

        if args.shutdown:
            print("[CalibratePhase10Live] Sending clean quit_editor() command...")
            try:
                remote.run_command(
                    REMOTE_QUIT_EDITOR_SNIPPET,
                    unattended=True,
                    exec_mode=remote_execution.MODE_EXEC_FILE,
                    raise_on_failure=False,
                )
            except Exception:
                pass
    finally:
        remote.stop()

    print(f"[CalibratePhase10Live] ALL PHASE 11A-FIX COMPONENT, FRAME-TIME & LIT PIXEL-LUMINANCE GATES PASSED ({LIT_CAPTURE_PATH}).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
