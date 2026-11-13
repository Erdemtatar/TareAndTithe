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
fog_comps = []
relief_isms = []
inspector_cameras = []

for actor in all_actors:
    for comp in actor.get_components_by_class(unreal.LightComponentBase):
        casts = bool(comp.get_editor_property("cast_shadows"))
        entry = f"{actor.get_class().get_name()}.{comp.get_name()}(intensity={comp.get_editor_property('intensity')})"
        if casts:
            shadow_lights.append(entry)
        else:
            zero_shadow_lights.append(entry)
        if isinstance(comp, unreal.SkyLightComponent):
            sky_lights.append(comp)

    for fog in actor.get_components_by_class(unreal.ExponentialHeightFogComponent):
        fog_comps.append(fog)

    for ism in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
        relief_isms.append(ism)

    for cam in actor.get_components_by_class(unreal.CameraComponent):
        if "Inspector" in actor.get_class().get_name():
            inspector_cameras.append(cam)

unreal.log(f"[CalibratePhase10Live] Shadow-casting lights ({len(shadow_lights)}): {shadow_lights}")
unreal.log(f"[CalibratePhase10Live] Zero-shadow lights ({len(zero_shadow_lights)}): {zero_shadow_lights}")

if len(shadow_lights) != 1:
    raise AssertionError(f"Expected exactly 1 shadow-casting light, found {len(shadow_lights)}: {shadow_lights}")
if len(zero_shadow_lights) != 8:
    raise AssertionError(f"Expected exactly 8 zero-shadow lights, found {len(zero_shadow_lights)}: {zero_shadow_lights}")

if len(sky_lights) != 1:
    raise AssertionError(f"Expected 1 SkyLightComponent in PIE, found {len(sky_lights)}")
sky = sky_lights[0]
sky_cubemap = sky.get_editor_property("cubemap")
if sky_cubemap is None or "TC_CellAmbientCubemap" not in sky_cubemap.get_path_name():
    raise AssertionError(f"Guard 1 failed: SkyLight cubemap={sky_cubemap}")
if bool(sky.get_editor_property("cast_shadows")):
    raise AssertionError("Guard 1 failed: SkyLight cast_shadows must be False!")
if bool(sky.get_editor_property("lower_hemisphere_is_black")):
    raise AssertionError("Guard 1 failed: SkyLight lower_hemisphere_is_black must be False!")

if len(fog_comps) != 1:
    raise AssertionError(f"Expected 1 ExponentialHeightFogComponent in PIE, found {len(fog_comps)}")
fog = fog_comps[0]
if bool(fog.get_editor_property("enable_volumetric_fog")):
    raise AssertionError("ExponentialHeightFog volumetric fog must be False!")
fog_start = float(fog.get_editor_property("start_distance"))
if fog_start < 295.0:
    raise AssertionError(f"Fog start_distance {fog_start} must be behind hero monolith plane (>= 295 cm)!")

if len(relief_isms) != 2:
    raise AssertionError(f"Guard 2 failed: Expected 2 midground relief ISMs, found {len(relief_isms)}")

total_relief_instances = 0
global_min_relief_x = 1.0e9
for ism in relief_isms:
    if ism.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION:
        raise AssertionError(f"Guard 2 failed: {ism.get_name()} collision is not NoCollision!")
    if bool(ism.get_editor_property("cast_shadow")):
        raise AssertionError(f"Guard 2 failed: {ism.get_name()} cast_shadow must be False!")
    if bool(ism.get_editor_property("generate_overlap_events")):
        raise AssertionError(f"Guard 2 failed: {ism.get_name()} generate_overlap_events must be False!")

    inst_count = ism.get_instance_count()
    total_relief_instances += inst_count
    for idx in range(inst_count):
        xf = ism.get_instance_transform(idx, world_space=True)
        min_x = float(xf.translation.x) - 50.0 * abs(float(xf.scale3d.x))
        if min_x < global_min_relief_x:
            global_min_relief_x = min_x
        if min_x < 414.99:
            raise AssertionError(
                f"Guard 2 failed: {ism.get_name()}[{idx}] min_x={min_x:.2f} < 415.0 cm!"
            )

if len(inspector_cameras) != 1:
    raise AssertionError(f"Expected 1 InspectorCamera in PIE, found {len(inspector_cameras)}")

pp = inspector_cameras[0].get_editor_property("post_process_settings")
bloom_thresh = float(pp.get_editor_property("bloom_threshold"))
bloom_int = float(pp.get_editor_property("bloom_intensity"))
le_hi = float(pp.get_editor_property("local_exposure_highlight_contrast_scale"))
le_sh = float(pp.get_editor_property("local_exposure_shadow_contrast_scale"))
vignette = float(pp.get_editor_property("vignette_intensity"))

if bloom_thresh < 1.65 - 1.0e-3:
    raise AssertionError(f"HUD Firewall failed: bloom_threshold={bloom_thresh} < 1.65!")
if abs(le_hi - 1.0) > 1.0e-3 or abs(le_sh - 1.0) > 1.0e-3:
    raise AssertionError(f"LocalExposure neutral guard failed: hi={le_hi}, sh={le_sh}")

sharpen_cvar = unreal.SystemLibrary.get_console_variable_float_value("r.Tonemapper.Sharpen")

unreal.log(
    f"[CalibratePhase10Live] PIE VERIFIED OK: "
    f"shadow_lights=1, zero_shadow_lights=8, sky_cubemap={sky_cubemap.get_path_name()}, "
    f"fog_start={fog_start:.1f}cm, relief_isms=2 (instances={total_relief_instances}, min_x={global_min_relief_x:.1f}cm), "
    f"bloom_threshold={bloom_thresh:.2f}, bloom_intensity={bloom_int:.2f}, "
    f"local_exposure=({le_hi:.2f},{le_sh:.2f}), vignette={vignette:.2f}, sharpen={sharpen_cvar:.2f}"
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
