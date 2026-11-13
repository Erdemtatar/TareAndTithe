import argparse
import os
import sys
import time

UE5_ROOT = os.environ.get("UE5_ROOT", r"C:\Program Files\Epic Games\UE_5.8")
REMOTE_EXEC_DIR = os.path.join(
    UE5_ROOT, "Engine", "Plugins", "Experimental", "PythonScriptPlugin", "Content", "Python"
)
if REMOTE_EXEC_DIR not in sys.path:
    sys.path.insert(0, REMOTE_EXEC_DIR)

import remote_execution  # type: ignore  # noqa: E402


REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BUILD_MATERIALS_SCRIPT = os.path.join(REPO_ROOT, "SCRIPTS", "BuildMaterialsAndBind.py").replace("\\", "/")


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

all_actors = unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor)

shadow_lights = []
zero_shadow_lights = []
sky_lights = []
spot_lights = []
fog_comps = []
relief_isms = []
inspector_pawns = []
scale_actors = []
world_grid_offenders = []

for actor in all_actors:
    cls_name = actor.get_class().get_name()
    if "InspectorPawn" in cls_name:
        inspector_pawns.append(actor)
    if "ScaleAssemblyActor" in cls_name:
        scale_actors.append(actor)

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

# Verify Guard 1: Warm ~2150K Foundry Pit Glow cone coverage (Inner=28, Outer=56, R=345 < 360)
pit_glow_spots = [
    s for s in spot_lights
    if not bool(s.get_editor_property("cast_shadows"))
    and abs(float(s.get_editor_property("attenuation_radius")) - 345.0) < 0.5
]
if len(pit_glow_spots) != 1:
    raise AssertionError(f"Guard 1 failed: Expected 1 MidgroundWashSpot with R=345, found {len(pit_glow_spots)}")
pit_spot = pit_glow_spots[0]
pit_inner = float(pit_spot.get_editor_property("inner_cone_angle"))
pit_outer = float(pit_spot.get_editor_property("outer_cone_angle"))
if abs(pit_inner - 28.0) > 0.1 or abs(pit_outer - 56.0) > 0.1:
    raise AssertionError(f"Guard 1 failed: MidgroundWashSpot cones=({pit_inner}, {pit_outer}), expected (28.0, 56.0)")

if len(sky_lights) != 1:
    raise AssertionError(f"Expected 1 SkyLightComponent in PIE, found {len(sky_lights)}")
sky = sky_lights[0]
sky_cubemap = sky.get_editor_property("cubemap")
if sky_cubemap is None or "TC_CellAmbientCubemap" not in sky_cubemap.get_path_name():
    raise AssertionError(f"SkyLight cubemap invalid: {sky_cubemap}")
if bool(sky.get_editor_property("cast_shadows")):
    raise AssertionError("SkyLight cast_shadows must be False!")

if len(fog_comps) != 1:
    raise AssertionError(f"Expected 1 ExponentialHeightFogComponent in PIE, found {len(fog_comps)}")
fog = fog_comps[0]
if bool(fog.get_editor_property("enable_volumetric_fog")):
    raise AssertionError("ExponentialHeightFog volumetric fog must be False!")
fog_density = float(fog.get_editor_property("fog_density"))
fog_start = float(fog.get_editor_property("start_distance"))
if abs(fog_density - 0.015) > 0.002 or fog_start < 385.0:
    raise AssertionError(f"Phase 11A Fog guard failed: density={fog_density}, start={fog_start}")

# Verify Phase 11A Rodin Foundry Grate Shaft & Flanking Columns (zero BasicShapes/Cube sticks)
if len(relief_isms) != 2:
    raise AssertionError(f"Expected 2 Rodin Foundry Shaft ISMs, found {len(relief_isms)}")

total_relief_instances = 0
global_min_relief_x = 1.0e9
global_max_relief_x = -1.0e9
ism_summary = []
for ism in relief_isms:
    if ism.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION:
        raise AssertionError(f"{ism.get_name()} collision is not NoCollision!")
    if bool(ism.get_editor_property("cast_shadow")):
        raise AssertionError(f"{ism.get_name()} cast_shadow must be False!")
    if bool(ism.get_editor_property("generate_overlap_events")):
        raise AssertionError(f"{ism.get_name()} generate_overlap_events must be False!")

    sm = ism.get_editor_property("static_mesh")
    if sm is None or "BasicShapes/Cube" in sm.get_path_name():
        raise AssertionError(f"{ism.get_name()} uses forbidden mesh: {sm}")
    mat0 = ism.get_material(0)
    if mat0 is None or not bool(mat0.get_base_material().get_editor_property("used_with_instanced_static_meshes")):
        raise AssertionError(f"{ism.get_name()} material missing MATUSAGE_INSTANCED_STATIC_MESHES: {mat0}")

    bbox = sm.get_bounding_box()
    bmin = bbox.min
    bmax = bbox.max
    corners = [
        unreal.Vector(x, y, z)
        for x in (bmin.x, bmax.x)
        for y in (bmin.y, bmax.y)
        for z in (bmin.z, bmax.z)
    ]
    inst_count = ism.get_instance_count()
    total_relief_instances += inst_count
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

# Verify Guard 2 & Compact Console Bezel on AScaleAssemblyActor
if len(scale_actors) != 1:
    raise AssertionError(f"Expected 1 ScaleAssemblyActor in PIE, found {len(scale_actors)}")
scale_actor = scale_actors[0]
flapper_vis = scale_actor.get_editor_property("diverter_flapper_visual")
flapper_mat = flapper_vis.get_material(0)
if flapper_mat is None or "MI_Station_DiverterFlapper" not in flapper_mat.get_name():
    raise AssertionError(f"Guard 2 failed: DiverterFlapperVisual material is {flapper_mat}, expected MI_Station_DiverterFlapper!")

backing_plate = scale_actor.get_editor_property("readout_backing_plate")
plate_width_cm = float(backing_plate.get_editor_property("relative_scale3d").y) * 100.0
stencil_text = scale_actor.get_editor_property("control_stencil_text")
stencil_width_cm = float(stencil_text.get_text_local_size().y)
if abs(plate_width_cm - 220.0) > 1.0 or stencil_width_cm > 203.0:
    raise AssertionError(
        f"Console bezel guard failed: plate_width={plate_width_cm:.1f}cm, stencil_width={stencil_width_cm:.1f}cm"
    )

# Verify Guard 3 & Frustum Camera on AInspectorPawn
if len(inspector_pawns) != 1:
    raise AssertionError(f"Expected 1 InspectorPawn in PIE, found {len(inspector_pawns)}")
pawn = inspector_pawns[0]
sheet_top = pawn.get_editor_property("laser_sheet_visual")
sheet_bot = pawn.get_editor_property("laser_sheet_underside_visual")
up_top = unreal.MathLibrary.get_up_vector(sheet_top.get_world_rotation())
up_bot = unreal.MathLibrary.get_up_vector(sheet_bot.get_world_rotation())
up_dot = float(unreal.MathLibrary.dot_vector_vector(up_top, up_bot))
sheet_mat = sheet_top.get_material(0)
if up_dot > -0.999 or sheet_mat is None or bool(sheet_mat.get_base_material().get_editor_property("two_sided")):
    raise AssertionError(
        f"Guard 3 failed: laser sheet up_dot={up_dot:.4f}, mat={sheet_mat}, "
        f"two_sided={None if sheet_mat is None else sheet_mat.get_base_material().get_editor_property('two_sided')}"
    )

cam = pawn.get_editor_property("inspector_camera")
cam_fov = float(cam.get_editor_property("field_of_view"))
cam_loc = cam.get_world_location()
pp = cam.get_editor_property("post_process_settings")
bloom_thresh = float(pp.get_editor_property("bloom_threshold"))
bloom_int = float(pp.get_editor_property("bloom_intensity"))
le_hi = float(pp.get_editor_property("local_exposure_highlight_contrast_scale"))
le_sh = float(pp.get_editor_property("local_exposure_shadow_contrast_scale"))
vignette = float(pp.get_editor_property("vignette_intensity"))

if abs(cam_fov - 85.0) > 0.2:
    raise AssertionError(f"Expected InspectorCamera FOV=85.0, got {cam_fov:.2f}")
if bloom_thresh < 1.65 - 1.0e-3:
    raise AssertionError(f"HUD Firewall failed: bloom_threshold={bloom_thresh} < 1.65!")
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
    raise AssertionError(f"PIE steady-state frame/GPU time {delta_ms:.2f} ms exceeds 13.0 ms target (disabling VSync/60Hz cap for next sample)!")
sharpen_cvar = unreal.SystemLibrary.get_console_variable_float_value("r.Tonemapper.Sharpen")

unreal.log(
    f"[CalibratePhase10Live] PHASE 11A PIE VERIFIED OK: "
    f"shadow_lights=1, zero_shadow_lights=8, fog=(density={fog_density:.3f}, start={fog_start:.1f}cm), "
    f"foundry_shaft={ism_summary} (x=[{global_min_relief_x:.1f},{global_max_relief_x:.1f}]cm), "
    f"flapper_mat={flapper_mat.get_name()}, laser_sheet_up_dot={up_dot:.3f}, "
    f"console=(plate={plate_width_cm:.1f}cm, stencil={stencil_width_cm:.1f}cm), "
    f"cam=(loc=({cam_loc.x:.1f},{cam_loc.y:.1f},{cam_loc.z:.1f}), fov={cam_fov:.1f}, vignette={vignette:.2f}), "
    f"bloom_threshold={bloom_thresh:.2f}, frame_delta_ms={delta_ms:.2f}ms, sharpen={sharpen_cvar:.2f}"
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


def run_remote_checked(remote: remote_execution.RemoteExecution, code_or_file: str, label: str) -> None:
    print(f"[CalibratePhase10Live] >>> Executing remote step: {label}")
    res = remote.run_command(
        code_or_file,
        unattended=True,
        exec_mode=remote_execution.MODE_EXEC_FILE,
        raise_on_failure=False,
    )
    for out_line in res.get("output", []) or []:
        msg = out_line.get("output", "").rstrip()
        if msg:
            print(f"  [UE] {msg}")
    if not res.get("success", False):
        raise RuntimeError(f"Remote step '{label}' FAILED: {res.get('result')}")
    print(f"[CalibratePhase10Live] <<< Step '{label}' SUCCEEDED.")


def main() -> int:
    parser = argparse.ArgumentParser(description="Live 127.0.0.1 Python bridge calibration & PIE verifier for Phase 10")
    parser.add_argument("--timeout", type=float, default=90.0, help="Seconds to wait for UnrealEditor remote node")
    parser.add_argument("--shutdown", action="store_true", help="Cleanly quit UnrealEditor after calibration/verification")
    args = parser.parse_args()

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

        # 1. Build/compile all Phase 10 materials, 8 MI_Station_* instances, cook-safe cubemap & DA_Presentation
        run_remote_checked(remote, BUILD_MATERIALS_SCRIPT, "BuildMaterialsAndBind.py")

        # 2. Launch live PIE session to verify actual runtime spawned lights, ISMs, SkyLight cubemap & HUD BloomThreshold
        run_remote_checked(remote, REMOTE_BEGIN_PIE_SNIPPET, "Begin PIE Session")

        pie_verified = False
        last_err = None
        for attempt in range(1, 9):
            time.sleep(1.5)
            try:
                run_remote_checked(remote, REMOTE_VERIFY_PIE_SNIPPET, f"Verify Live PIE World (attempt {attempt})")
                pie_verified = True
                break
            except Exception as exc:
                last_err = exc
                print(f"[CalibratePhase10Live] Waiting for PIE world initialization ({exc})...")

        run_remote_checked(remote, REMOTE_END_PIE_SNIPPET, "End PIE Session")
        time.sleep(1.0)

        if not pie_verified:
            raise RuntimeError(f"Live PIE verification failed: {last_err}")

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
                # Socket may close immediately as UnrealEditor exits
                pass
    finally:
        remote.stop()

    print("[CalibratePhase10Live] ALL PHASE 10 LIVE CALIBRATION & PIE GUARDS PASSED.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
