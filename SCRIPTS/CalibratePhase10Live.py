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
LIT_CAPTURE_FILENAME = "Phase11C_LitCapture.png"
CLEAN_PROBE_FILENAME = "_Phase11C_CleanMetalProbe.png"
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
if world_time_sec < 0.50:
    raise RuntimeError(f"PIE warmup in progress (world_time={world_time_sec:.3f}s < 0.50s)...")

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
booth_actors = []
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
    if "DirectorBoothActor" in cls_name:
        booth_actors.append(actor)

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
if len(zero_shadow_lights) != 10:
    raise AssertionError(f"Expected exactly 10 zero-shadow lights (including 3 Panopticon bay lights), found {len(zero_shadow_lights)}: {zero_shadow_lights}")

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
if float(pit_loc.x) > 350.0 or float(pit_fwd.x) < 0.82 or not (58.0 <= pit_outer <= 68.5):
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

if len(scale_actors) != 1 or len(inspector_pawns) != 1 or len(monolith_rigs) != 1 or len(booth_actors) != 1:
    raise AssertionError(
        f"Expected 1 Scale, 1 Pawn, 1 MonolithRig, 1 Booth; got scale={len(scale_actors)}, pawn={len(inspector_pawns)}, rig={len(monolith_rigs)}, booth={len(booth_actors)}"
    )

booth_actor = booth_actors[0]
# Verify Phase 11C Panopticon 3-Bay Overlook Bridge envelope (Y in [-300,+300], Z in [212,382], X in [398,452]), NoCollision, and 3 synchronized shutters
panopticon_props = (
    "overlook_recess_backing_visual",
    "booth_frame_visual",
    "left_booth_frame_visual",
    "right_booth_frame_visual",
    "blast_shutter_visual",
    "left_blast_shutter_visual",
    "right_blast_shutter_visual",
    "armor_mullion_outer_left_visual",
    "armor_mullion_inner_left_visual",
    "armor_mullion_inner_right_visual",
    "armor_mullion_outer_right_visual",
)
pan_min = [1.0e9, 1.0e9, 1.0e9]
pan_max = [-1.0e9, -1.0e9, -1.0e9]
for p_name in panopticon_props:
    mc = booth_actor.get_editor_property(p_name)
    if mc is None or mc.get_editor_property("static_mesh") is None:
        raise AssertionError(f"Panopticon component {p_name} missing or has null static_mesh!")
    if mc.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION:
        raise AssertionError(f"Panopticon component {p_name} must be NoCollision!")
    if bool(mc.get_editor_property("cast_shadow")):
        raise AssertionError(f"Panopticon component {p_name} cast_shadow must be False!")
    sm = mc.get_editor_property("static_mesh")
    bbox = sm.get_bounding_box()
    xf = mc.get_world_transform()
    for bx in (bbox.min.x, bbox.max.x):
        for by in (bbox.min.y, bbox.max.y):
            for bz in (bbox.min.z, bbox.max.z):
                wp = unreal.MathLibrary.transform_location(xf, unreal.Vector(bx, by, bz))
                pan_min[0] = min(pan_min[0], float(wp.x))
                pan_min[1] = min(pan_min[1], float(wp.y))
                pan_min[2] = min(pan_min[2], float(wp.z))
                pan_max[0] = max(pan_max[0], float(wp.x))
                pan_max[1] = max(pan_max[1], float(wp.y))
                pan_max[2] = max(pan_max[2], float(wp.z))

if pan_min[1] > -296.0 or pan_max[1] < 296.0 or pan_max[2] < 380.0 or pan_min[0] < 395.0 or pan_max[0] > 455.0:
    raise AssertionError(
        f"Panopticon 3-bay world AABB {pan_min}..{pan_max} failed [398..452, -300..+300, 212..382] cm envelope!"
    )

shutter_alpha_open = float(booth_actor.get_current_shutter_open_alpha())
if shutter_alpha_open < 0.95:
    raise AssertionError(f"Expected all 3 Panopticon shutters open (>=0.95) in ActiveInspection, got {shutter_alpha_open:.3f}")

for bay_light_prop in ("status_point_light", "left_bay_point_light", "right_bay_point_light"):
    bl = booth_actor.get_editor_property(bay_light_prop)
    if bl is None or bool(bl.get_editor_property("cast_shadows")) or float(bl.get_editor_property("attenuation_radius")) > 235.1:
        raise AssertionError(f"Panopticon bay light {bay_light_prop} failed shadow/radius<=235 guard!")

# Verify Phase 11C Top-Left & Top-Right corner HUD render hygiene + 16:9 safe margin across ALL 18 camera aim/charge states
import math as _math
for plate_prop, side_label in (("booth_header_backing_plate", "left"), ("booth_verdict_backing_plate", "right")):
    plate_c = booth_actor.get_editor_property(plate_prop)
    if (
        plate_c.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION
        or bool(plate_c.get_editor_property("cast_shadow"))
        or bool(plate_c.get_editor_property("affect_distance_field_lighting"))
        or bool(plate_c.get_editor_property("affect_dynamic_indirect_lighting"))
        or bool(plate_c.get_editor_property("visible_in_ray_tracing"))
    ):
        raise AssertionError(f"Corner HUD plate {plate_prop} failed render hygiene flags!")
    pb = plate_c.get_editor_property("static_mesh").get_bounding_box()
    pxf = plate_c.get_world_transform()
    pts_w = [
        unreal.MathLibrary.transform_location(pxf, unreal.Vector(bx, by, bz))
        for bx in (pb.min.x, pb.max.x)
        for by in (pb.min.y, pb.max.y)
        for bz in (pb.min.z, pb.max.z)
    ]
    us_all, vs_all = [], []
    for cpitch in (-4.45, -3.65, -2.85):
        for cyaw in (-4.5, 0.0, 4.5):
            crot = unreal.Rotator(roll=0.0, pitch=cpitch, yaw=cyaw)
            cfwd = unreal.MathLibrary.get_forward_vector(crot)
            cright = unreal.MathLibrary.get_right_vector(crot)
            cup = unreal.MathLibrary.get_up_vector(crot)
            for cfov in (83.6, 85.0):
                th = _math.tan(_math.radians(cfov * 0.5))
                tv = th * (9.0 / 16.0)
                for wp in pts_w:
                    rx, ry, rz = float(wp.x + 66.0), float(wp.y), float(wp.z - 162.0)
                    d = rx * float(cfwd.x) + ry * float(cfwd.y) + rz * float(cfwd.z)
                    cx = rx * float(cright.x) + ry * float(cright.y) + rz * float(cright.z)
                    cy = rx * float(cup.x) + ry * float(cup.y) + rz * float(cup.z)
                    us_all.append(0.5 + 0.5 * (cx / (d * th)))
                    vs_all.append(0.5 - 0.5 * (cy / (d * tv)))
    umin, umax = min(us_all), max(us_all)
    vmin, vmax = min(vs_all), max(vs_all)
    if side_label == "left":
        if umin < 0.045 or umax > 0.355 or vmin < 0.050 or vmax > 0.215:
            raise AssertionError(f"Top-Left HUD corner bounds out of safe 16:9 zone: u=[{umin:.3f},{umax:.3f}], v=[{vmin:.3f},{vmax:.3f}]")
    else:
        if umin < 0.645 or umax > 0.955 or vmin < 0.050 or vmax > 0.215:
            raise AssertionError(f"Top-Right HUD corner bounds out of safe 16:9 zone: u=[{umin:.3f},{umax:.3f}], v=[{vmin:.3f},{vmax:.3f}]")

# Verify DA_Presentation PA Intercom USoundWave bindings are non-null in live PIE
da_pres_check = unreal.load_asset("/Game/Presentation/DA_Presentation")
if da_pres_check is None:
    raise AssertionError("Failed to load /Game/Presentation/DA_Presentation in live PIE!")
for pa_prop in ("director_intercom_directive_sound", "director_intercom_approve_sound", "director_intercom_reprimand_sound"):
    pa_asset = da_pres_check.get_editor_property(pa_prop)
    if pa_asset is None or not isinstance(pa_asset, unreal.SoundWave):
        raise AssertionError(f"DA_Presentation.{pa_prop} is invalid: {pa_asset}")

scale_actor = scale_actors[0]
scale_header_str = str(scale_actor.get_editor_property("header_stencil_text").text)
if any(tok in scale_header_str for tok in ("SHIFT", "SCRIP:", "[1]HYD", "[2]TARE", "[3]SCAN")):
    raise AssertionError(f"Bottom scale HeaderStencilText still cluttered: {scale_header_str!r}")
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
    f"world_time={world_time_sec:.2f}s, shadow_lights=1, zero_shadow_lights=10, "
    f"panopticon_aabb={pan_min}..{pan_max}, shutter_alpha={shutter_alpha_open:.2f}, "
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
    if "DirectorBoothActor" in actor.get_class().get_name():
        for prop_name in ("booth_header_backing_plate", "booth_verdict_backing_plate"):
            comp = actor.get_editor_property(prop_name)
            if comp is not None:
                scc.hide_component(comp)
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
    "border_left": [0, int(0.23 * H), int(0.10 * W), int(0.90 * H)],
    "border_right": [int(0.90 * W), int(0.23 * H), W, int(0.90 * H)],
}}

# Cleanly destroy transient capture component & actor (unreferencing transient render target for GC)
scc.set_editor_property("capture_every_frame", False)
scc.set_editor_property("texture_target", None)
cap_actor.destroy_actor()

unreal.log("ROI_JSON=" + json.dumps(roi_payload))
"""


REMOTE_VERIFY_DISK_CONTRACTS_SNIPPET = """
import unreal

# Guard 4: Verify all 6 DA_Contract_01..06 assets on disk have calibrated ClampMaxSafeTorqueNm & 6.5s grace (pre-PIE)
expected_torques = [
    ("DA_Contract_01", 26.0),
    ("DA_Contract_02", 26.5),
    ("DA_Contract_03", 28.0),
    ("DA_Contract_04", 30.0),
    ("DA_Contract_05", 31.0),
    ("DA_Contract_06", 34.0),
]
for cname, exp_nm in expected_torques:
    cpath = f"/Game/Presentation/Contracts/{cname}"
    casset = unreal.EditorAssetLibrary.load_asset(cpath)
    if casset is None:
        raise AssertionError(f"Missing {cpath} on disk!")
    actual_nm = float(casset.get_editor_property("clamp_max_safe_torque_nm"))
    actual_grace = float(casset.get_editor_property("clamp_fatigue_grace_seconds"))
    if abs(actual_nm - exp_nm) > 0.05 or abs(actual_grace - 6.5) > 0.05:
        raise AssertionError(
            f"{cpath} mismatch: max_torque_nm={actual_nm} (exp {exp_nm}), grace={actual_grace} (exp 6.5)"
        )
da_pres_disk = unreal.EditorAssetLibrary.load_asset("/Game/Presentation/DA_Presentation")
if da_pres_disk is None:
    raise AssertionError("Missing /Game/Presentation/DA_Presentation on disk!")
for pa_prop in ("director_intercom_directive_sound", "director_intercom_approve_sound", "director_intercom_reprimand_sound"):
    pa_snd = da_pres_disk.get_editor_property(pa_prop)
    if pa_snd is None or not isinstance(pa_snd, unreal.SoundWave):
        raise AssertionError(f"Disk DA_Presentation.{pa_prop} invalid: {pa_snd}")
unreal.log("[CalibratePhase10Live] Verified all 6 DA_Contract_01..06 calibrated torque limits & 3 PA SoundWave bindings on disk.")
"""


REMOTE_PHASE11C_AUDIO_AND_STAMP_PREP_SNIPPET = """
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = unreal.TareTitheGameMode.get_inspection_shift_subsystem(pie_world)
booths = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "DirectorBoothActor" in a.get_class().get_name()]
scales = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "ScaleAssemblyActor" in a.get_class().get_name()]
rigs = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()]
booth = booths[0]
scale = scales[0]
rig = rigs[0]

# 1. Verify initial directive PA cue triggered once on startup and routine updates do NOT spam PA audio
c_init = int(booth.get_intercom_audio_trigger_count())
last_snd = booth.get_last_triggered_intercom_sound()
if c_init != 1 or last_snd is None or "SFX_Director_PA_Directive" not in last_snd.get_name():
    raise AssertionError(f"Expected initial PA Directive trigger count=1, got count={c_init}, sound={last_snd}")

# Routine snapshot updates with unchanged DirectorIntercomLine (toggle diverter) + <0.35s distinct bark debounce gate
deb_init = int(booth.get_intercom_debounce_blocked_count())
shift_sub.start_shift_from_contract(1)
c_after_ctr2 = int(booth.get_intercom_audio_trigger_count())
if c_after_ctr2 != c_init + 1 or "SFX_Director_PA_Directive" not in booth.get_last_triggered_intercom_sound().get_name():
    raise AssertionError(f"Expected PA Directive cue on Contract 02 load, got count={c_after_ctr2}")

# Immediate (<0.35s) distinct contract bark MUST be blocked by >=0.35s debounce!
shift_sub.start_shift_from_contract(2)
if int(booth.get_intercom_audio_trigger_count()) != c_after_ctr2 or int(booth.get_intercom_debounce_blocked_count()) <= deb_init:
    raise AssertionError("PA >=0.35s debounce gate FAILED to block back-to-back <0.35s distinct intercom bark!")

for _ in range(4):
    shift_sub.request_toggle_diverter()
if int(booth.get_intercom_audio_trigger_count()) != c_after_ctr2:
    raise AssertionError("Anti-spam gate FAILED: routine diverter/snapshot updates spammed PA intercom audio!")

# Verify corner HUD plaque text bezel bounds (zero character clipping on <= 70 cm corner plates; <= 67.5 cm safe width)
for tc in booth.get_components_by_class(unreal.TextRenderComponent):
    w_cm = float(tc.get_text_local_size().y)
    if w_cm > 67.5:
        raise AssertionError(f"DirectorBooth corner HUD {tc.get_name()} width {w_cm:.1f} cm exceeds 67.5 cm safe plate bezel!")

for tc in scale.get_components_by_class(unreal.TextRenderComponent):
    w_cm = float(tc.get_text_local_size().y)
    if w_cm > 203.5:
        raise AssertionError(f"ScaleAssembly {tc.get_name()} width {w_cm:.1f} cm exceeds 203.5 cm safe console bezel!")

unreal.log(f"[CalibratePhase10Live] Verified PA Directive cue, <0.35s debounce & anti-spam gate (count={c_after_ctr2}, routine_spam=0).")
"""


REMOTE_PHASE11C_SLICE_QUOTA_SNIPPET = """
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = unreal.TareTitheGameMode.get_inspection_shift_subsystem(pie_world)
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]

# Reset cleanly to Contract 01 and drop an exact 12.00 kg precision chunk onto the Tithe Scale
shift_sub.start_shift_from_contract(0)
bounds = rig.get_target_world_bounds()
bcenter = (bounds.min + bounds.max) * 0.5
cut_z = float(bounds.min.z) + 26.0416
aperture = unreal.LaserSliceAperture()
aperture.set_editor_property("enforce_finite_sheet_bounds", False)
rig.commit_slice_plane(unreal.Vector(float(bcenter.x), float(bcenter.y), cut_z), unreal.Vector(0.0, 0.0, -1.0), aperture)
unreal.log("[CalibratePhase10Live] Dropped 12.00 kg precision chunk on Contract 01.")
"""


REMOTE_PHASE11C_STAMP_APPROVE_SNIPPET = """
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = unreal.TareTitheGameMode.get_inspection_shift_subsystem(pie_world)
booth = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "DirectorBoothActor" in a.get_class().get_name()][0]

c_pre_stamp = int(booth.get_intercom_audio_trigger_count())
stamped = bool(shift_sub.try_confirm_or_stamp_action())
snap_st = shift_sub.get_current_snapshot()
if not stamped or int(snap_st.scrip_balance) != 2 or not bool(snap_st.last_stamp_was_directors_cut):
    raise AssertionError(f"Expected DIRECTOR'S CUT (+2 SCRIP) stamp pass, got stamped={stamped}, scrip={snap_st.scrip_balance}, snap={snap_st}")

c_post_stamp = int(booth.get_intercom_audio_trigger_count())
last_snd = booth.get_last_triggered_intercom_sound()
if c_post_stamp != c_pre_stamp + 1 or last_snd is None or "SFX_Director_PA_Approve" not in last_snd.get_name():
    raise AssertionError(f"Expected SFX_Director_PA_Approve on stamp pass, got count={c_pre_stamp}->{c_post_stamp}, sound={last_snd}")

# Reload Contract 01 so all 3 blast shutters hoist open cleanly before requisition + fault slam test
shift_sub.start_shift_from_contract(0)
unreal.log("[CalibratePhase10Live] Verified SFX_Director_PA_Approve on DIRECTOR'S CUT (+2 SCRIP) precision stamp!")
"""


REMOTE_PHASE11C_REQUISITION_UPGRADE_SNIPPET = """
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = unreal.TareTitheGameMode.get_inspection_shift_subsystem(pie_world)
booth = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "DirectorBoothActor" in a.get_class().get_name()][0]

c_pre_upg = int(booth.get_intercom_audio_trigger_count())
upg_ok = bool(shift_sub.try_purchase_requisition(unreal.RequisitionTrack.TARE_HOPPER))
if not upg_ok or int(shift_sub.get_current_snapshot().scrip_balance) != 0:
    raise AssertionError("Expected TARE_HOPPER L1 purchase to succeed using 2 Scrip!")
if int(booth.get_intercom_audio_trigger_count()) != c_pre_upg + 1 or "SFX_Director_PA_Approve" not in booth.get_last_triggered_intercom_sound().get_name():
    raise AssertionError("Expected SFX_Director_PA_Approve on requisition upgrade!")
unreal.log("[CalibratePhase10Live] Verified requisition upgrade SFX_Director_PA_Approve (Scrip 2 -> 0).")
"""


REMOTE_PHASE11C_REQUISITION_REJECT_SNIPPET = """
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
shift_sub = unreal.TareTitheGameMode.get_inspection_shift_subsystem(pie_world)
booth = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "DirectorBoothActor" in a.get_class().get_name()][0]

c_before_req = int(booth.get_intercom_audio_trigger_count())
req_ok = bool(shift_sub.try_purchase_requisition(unreal.RequisitionTrack.TARE_HOPPER))
fb_str = str(shift_sub.get_current_snapshot().transient_requisition_feedback)
if req_ok or "INSUFFICIENT SCRIP" not in fb_str:
    raise AssertionError(f"Expected INSUFFICIENT SCRIP feedback at 0 Scrip, got ok={req_ok}, feedback={fb_str!r}")
c_after_req1 = int(booth.get_intercom_audio_trigger_count())
if c_after_req1 != c_before_req + 1 or "SFX_Director_PA_Reprimand" not in booth.get_last_triggered_intercom_sound().get_name():
    raise AssertionError(f"Expected PA Reprimand reject cue on INSUFFICIENT SCRIP, got count={c_after_req1}")

# Duplicate string suppression check
shift_sub.try_purchase_requisition(unreal.RequisitionTrack.TARE_HOPPER)
if int(booth.get_intercom_audio_trigger_count()) != c_after_req1:
    raise AssertionError("Duplicate-string gate FAILED: identical INSUFFICIENT SCRIP string re-triggered PA audio!")
unreal.log("[CalibratePhase10Live] Verified INSUFFICIENT SCRIP reject cue & duplicate-string suppression.")
"""


REMOTE_PHASE11B_TRIGGER_CLAMP_SNAP_SNIPPET = """
import json
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
if pie_world is None:
    raise RuntimeError("PIE GameWorld is None!")

shift_sub = unreal.TareTitheGameMode.get_inspection_shift_subsystem(pie_world)
booth = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "DirectorBoothActor" in a.get_class().get_name()][0]
rig = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()][0]

alpha_before_fault = float(booth.get_current_shutter_open_alpha())
if alpha_before_fault < 0.90:
    raise AssertionError(f"Expected Panopticon shutters open (>=0.90) prior to fault slam, got {alpha_before_fault:.3f}")

c_pre_snap = int(booth.get_intercom_audio_trigger_count())

# Consecutive one-sided (+Y) steep cuts escalate jaw shear and snap clamp!
rem_comp = rig.get_remainder_mesh_component()
clamp_solver = rig.get_clamp_torque_solver_component()
bounds = rig.get_target_world_bounds()
bcenter = (bounds.min + bounds.max) * 0.5
cx, cy, cz = float(bcenter.x), float(bcenter.y), float(bcenter.z)

aperture = unreal.LaserSliceAperture()
aperture.set_editor_property("enforce_finite_sheet_bounds", False)
steep_right_normal = unreal.Vector(0.0, 0.9659258, -0.2588190)

strain_progression = []
snapped = False
for y_off in (8.2, 5.4, 2.6, -0.2, -2.8, -5.2):
    origin = unreal.Vector(cx, cy + y_off, cz)
    rig.commit_slice_plane(origin, steep_right_normal, aperture)
    sr = float(clamp_solver.get_strain_ratio())
    streak = int(clamp_solver.get_same_side_cut_streak())
    snapped = bool(clamp_solver.is_clamp_snapped())
    strain_progression.append((round(y_off, 2), round(sr, 3), streak, snapped))
    if snapped:
        break

if not snapped:
    raise AssertionError(f"Consecutive same-side cuts failed to snap clamp! progression={strain_progression}")

c_post_snap = int(booth.get_intercom_audio_trigger_count())
snap_snd = booth.get_last_triggered_intercom_sound()
if c_post_snap <= c_pre_snap or snap_snd is None or "SFX_Director_PA_Reprimand" not in snap_snd.get_name():
    raise AssertionError(f"Expected SFX_Director_PA_Reprimand on clamp snap fault, got count={c_pre_snap}->{c_post_snap}, sound={snap_snd}")

# Re-verify corner HUD text widths on ALL-CAPS crimson fault reprimand (zero clipping)
for tc in booth.get_components_by_class(unreal.TextRenderComponent):
    w_cm = float(tc.get_text_local_size().y)
    if w_cm > 67.5:
        raise AssertionError(f"DirectorBooth corner HUD {tc.get_name()} fault bark width {w_cm:.1f} cm > 67.5 cm!")

snap_z0 = float(rem_comp.get_world_location().z)
snap_sim0 = bool(rem_comp.is_simulating_physics())
snap_phase = shift_sub.get_current_phase()
snap_bark = str(shift_sub.get_current_snapshot().director_intercom_line)
if not snap_sim0:
    raise AssertionError("RemainderMeshComponent.IsSimulatingPhysics() is False immediately after clamp snap!")

payload = {
    "snap_z0": snap_z0,
    "progression": strain_progression,
    "phase": str(snap_phase),
    "director_bark": snap_bark,
    "pa_sound": snap_snd.get_name(),
    "pa_triggers": c_post_snap,
}
unreal.log("SNAP_INIT_JSON=" + json.dumps(payload))
"""


def make_verify_chaos_freefall_snippet(snap_z0: float) -> str:
    return f"""
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
pie_world = ues.get_game_world()
if pie_world is None:
    raise RuntimeError("PIE GameWorld is None during Chaos freefall check!")

rigs = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "MonolithRigActor" in a.get_class().get_name()]
booths = [a for a in unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor) if "DirectorBoothActor" in a.get_class().get_name()]
if len(rigs) != 1 or len(booths) != 1:
    raise RuntimeError(f"Expected 1 MonolithRigActor & 1 Booth, found {{len(rigs)}}, {{len(booths)}}")

booth = booths[0]
alpha_slammed = float(booth.get_current_shutter_open_alpha())
if alpha_slammed > 0.02:
    raise AssertionError(f"Expected all 3 Panopticon shutters slammed shut (<=0.02) in 0.14s, got {{alpha_slammed:.3f}}!")

for piv_prop, closed_prop in (
    ("blast_shutter_motion_pivot", "closed_shutter_relative_location"),
    ("left_shutter_motion_pivot", "closed_left_shutter_relative_location"),
    ("right_shutter_motion_pivot", "closed_right_shutter_relative_location"),
):
    piv = booth.get_editor_property(piv_prop)
    c_loc = booth.get_editor_property(closed_prop)
    dz_s = abs(float(piv.relative_location.z) - float(c_loc.z))
    if dz_s > 0.5:
        raise AssertionError(f"Shutter pivot {{piv_prop}} failed simultaneous slam: dz={{dz_s:.2f}} cm")

for bay_l in ("status_point_light", "left_bay_point_light", "right_bay_point_light"):
    lc = booth.get_editor_property(bay_l).get_light_color()
    if float(lc.r) < 0.80 or float(lc.g) > 0.25 or float(lc.b) > 0.25:
        raise AssertionError(f"Panopticon bay {{bay_l}} not pulsing crimson emergency color: {{lc}}")

rem_comp = rigs[0].get_remainder_mesh_component()
z1 = float(rem_comp.get_world_location().z)
delta_z = z1 - float({snap_z0!r})
sim_now = bool(rem_comp.is_simulating_physics())
vel_z = float(rem_comp.get_component_velocity().z)

unreal.log(
    f"[CalibratePhase10Live] Panopticon 3-Shutter Slam (alpha={{alpha_slammed:.3f}}) & Chaos Freefall Check: "
    f"z0={snap_z0:.2f} -> z1={{z1:.2f}} cm (DeltaZ={{delta_z:.2f}} cm, vel_z={{vel_z:.1f}} cm/s, simulating={{sim_now}})"
)

if delta_z > -25.0:
    raise AssertionError(
        f"Guard 3 FAILED: Snapped RemainderMeshComponent did not fall under Chaos gravity! "
        f"z0={snap_z0:.2f}, z1={{z1:.2f}}, DeltaZ={{delta_z:.2f}} cm (required < -25.0 cm)"
    )
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

        # 1. Build/compile all Phase 11B materials, 9 semi-metallic MI_Station_* instances, cubemap, contracts & DA_Presentation
        run_remote_checked(remote, BUILD_MATERIALS_SCRIPT, "BuildMaterialsAndBind.py")
        run_remote_checked(remote, REMOTE_VERIFY_DISK_CONTRACTS_SNIPPET, "Verify Serialized DA_Contract_01..06 on Disk")

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

        # 4. Phase 11C Live PA Intercom Audio (Directive / Approve / Reprimand + >=0.35s Debounce), 3-Shutter Fault Slam & Chaos Drop Gate
        run_remote_checked(
            remote,
            REMOTE_PHASE11C_AUDIO_AND_STAMP_PREP_SNIPPET,
            "Phase 11C PA Directive Cue, <0.35s Debounce & Routine Anti-Spam Gate",
        )
        time.sleep(0.45)
        run_remote_checked(
            remote,
            REMOTE_PHASE11C_SLICE_QUOTA_SNIPPET,
            "Phase 11C 12.00 kg Precision Cut on Contract 01",
        )
        time.sleep(1.85)
        run_remote_checked(
            remote,
            REMOTE_PHASE11C_STAMP_APPROVE_SNIPPET,
            "Phase 11C Precision Stamp SFX_Director_PA_Approve Verification",
        )
        time.sleep(0.45)
        run_remote_checked(
            remote,
            REMOTE_PHASE11C_REQUISITION_UPGRADE_SNIPPET,
            "Phase 11C Requisition Upgrade SFX_Director_PA_Approve Verification",
        )
        time.sleep(0.45)
        run_remote_checked(
            remote,
            REMOTE_PHASE11C_REQUISITION_REJECT_SNIPPET,
            "Phase 11C INSUFFICIENT SCRIP Reject Cue & Duplicate-String Suppression",
        )
        time.sleep(0.48)
        snap_init_lines = run_remote_checked(
            remote,
            REMOTE_PHASE11B_TRIGGER_CLAMP_SNAP_SNIPPET,
            "Phase 11C Same-Side Clamp Snap Trigger & SFX_Director_PA_Reprimand",
        )
        snap_init_json = None
        for ln in snap_init_lines:
            idx = ln.find("SNAP_INIT_JSON=")
            if idx >= 0:
                snap_init_json = json.loads(ln[idx + len("SNAP_INIT_JSON=") :])
        if not snap_init_json:
            run_remote_checked(remote, REMOTE_END_PIE_SNIPPET, "End PIE Session (after missing SNAP_INIT_JSON)")
            raise RuntimeError("Missing SNAP_INIT_JSON payload from Phase 11C clamp snap trigger!")
        print(f"[CalibratePhase10Live] Same-side cut progression & snap bark: {snap_init_json}")

        # Let PIE tick ~0.48s (> 0.14s fault slam, < 1.8s FaultAutoResetDelaySeconds) for 3-shutter slam & Chaos freefall
        time.sleep(0.48)
        run_remote_checked(
            remote,
            make_verify_chaos_freefall_snippet(float(snap_init_json["snap_z0"])),
            "Verify 0.14s Simultaneous 3-Shutter Slam, Crimson Panopticon Strobe & Real Chaos Freefall Drop",
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

        # Gate 3: Rear Foundry Grate Shaft & Flanking Columns 18..26 mean_luma (subordinate to hero steel), >=65 peak_luma (< Clamp peak) + warm furnace ember (R > G > B)
        if (
            not (18.0 <= st_grate["mean_luma"] <= 26.0)
            or not (65.0 <= st_grate["peak_luma"] <= st_clamp["peak_luma"])
            or not (st_grate["mean_r"] > st_grate["mean_g"] > st_grate["mean_b"])
        ):
            raise AssertionError(
                f"ROI 3 (Rear Foundry Grate Shaft & Flanking Columns) FAILED Phase 11B hierarchy gate (18..26 mean_luma, 65..{st_clamp['peak_luma']:.1f} peak_luma, warm R>G>B): {st_grate}"
            )

        # Gate 4: Outer Left/Right Vignette Borders <= 16 mean
        if max(st_border_full["mean_luma"], st_border_full["mean_avg"], st_border_clean["mean_luma"], st_border_clean["mean_avg"]) > 16.0:
            raise AssertionError(
                f"ROI 4 (Outer Left/Right Vignette Borders) FAILED gate (<=16 mean): full={st_border_full}, clean={st_border_clean}"
            )
    finally:
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
        remote.stop()

    print(f"[CalibratePhase10Live] ALL PHASE 11C COMPONENT, HUD, PANOPTICON, PA AUDIO, FRAME-TIME & LIT LUMINANCE GATES PASSED ({LIT_CAPTURE_PATH}).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
