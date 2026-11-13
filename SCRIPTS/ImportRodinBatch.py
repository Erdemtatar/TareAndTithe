import math
import os
import re
from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple

import unreal


RAW_RODIN_DIR = r"C:\MegaJam2026\RawAssets\Rodin"
DEST_MESHES_DIR = "/Game/Presentation/Meshes"
DEST_PBR_DIR = "/Game/Presentation/Meshes/PBR"
CONTRACTS_DIR = "/Game/Presentation/Contracts"
DA_PRESENTATION_PATH = "/Game/Presentation/DA_Presentation"
DA_PROGRESSION_PATH = "/Game/Presentation/DA_ShiftProgression"
M_SLAG_OUTER_PATH = "/Game/Presentation/Materials/M_SlagOuter_Master"
M_MOLTEN_CUT_CAP_PATH = "/Game/Presentation/Materials/M_MoltenCutCap_Master"

MAX_TEXTURE_SIZE_PRIMARY = 2048
MAX_TEXTURE_SIZE_SECONDARY = 2048
MAX_UASSET_SIZE_MB = 45.0
MAX_MONOLITH_TRIANGLES = 2600  # Trap C target <= 2600 tris (safely under C++ 3500 budget)
TARGET_SIMPLIFY_TRIANGLES = 2500
MIN_VEIN_SOLID_MARGIN_CM = 1.80
TARGET_VEIN_TUCK_MARGIN_CM = 2.30


@dataclass(frozen=True)
class StationPropSpec:
    canonical_name: str
    slot_property: str
    envelope_cm: Tuple[float, float, float]
    target_xy_cm: Tuple[float, float]
    z_align_mode: str  # "min", "center", "max", or "deck_top"
    target_z_cm: float
    fit_exact_z: bool  # True for structural/collider-matched walls/frames; False for horizontal trays/bases
    is_secondary_prop: bool = False


@dataclass(frozen=True)
class MonolithMeshSpec:
    canonical_name: str
    contract_index: int  # 0-based index in OrderedContracts (3 -> Contract04, 5 -> Contract06)
    envelope_cm: Tuple[float, float, float]
    target_mass_kg: float


@dataclass
class ImportedAssetSummary:
    canonical_name: str
    asset_path: str
    triangle_count: int
    fitted_dims_cm: Tuple[float, float, float]
    yaw_deg: float
    is_monolith: bool = False
    is_closed: bool = False
    num_islands: int = 0
    volume_cm3: float = 0.0
    density_g_cm3: float = 0.0
    calibrated_mass_kg: float = 0.0
    grip_verified: bool = False
    min_vein_margin_cm: float = 0.0


# 9 Canonical Station Props (NEVER populate cell_wall_panel_mesh_slot per VISUAL_PRESENTATION_CONTRACT.md)
STATION_PROP_SPECS: Dict[str, StationPropSpec] = {
    "SM_ClampJaw": StationPropSpec(
        canonical_name="SM_ClampJaw",
        slot_property="clamp_jaw_mesh_slot",
        envelope_cm=(30.0, 30.0, 16.0),
        target_xy_cm=(0.0, 0.0),
        z_align_mode="max",
        target_z_cm=6.0,  # Mount-Top: collar in [0, +6], jaws bite down into [-10, 0] around monolith top Z=0
        fit_exact_z=True,
    ),
    "SM_ClampSpringHousing": StationPropSpec(
        canonical_name="SM_ClampSpringHousing",
        slot_property="clamp_spring_housing_mesh_slot",
        envelope_cm=(48.0, 48.0, 40.0),
        target_xy_cm=(0.0, 0.0),
        z_align_mode="min",
        target_z_cm=0.0,  # Bottom-Center at ClampPivotRoot Z=0
        fit_exact_z=True,
        is_secondary_prop=True,
    ),
    "SM_ScaleBase": StationPropSpec(
        canonical_name="SM_ScaleBase",
        slot_property="scale_base_mesh_slot",
        envelope_cm=(82.0, 68.0, 12.0),
        target_xy_cm=(0.0, 0.0),
        z_align_mode="min",
        target_z_cm=0.0,  # Bottom-Center on ScaleRoot Z=0
        fit_exact_z=False,
    ),
    "SM_ScalePan": StationPropSpec(
        canonical_name="SM_ScalePan",
        slot_property="scale_pan_mesh_slot",
        envelope_cm=(76.0, 62.0, 22.0),
        target_xy_cm=(0.0, 0.0),
        z_align_mode="deck_top",
        target_z_cm=3.0,  # Deck-Center Z=+3 flush with PanDeckCollider top surface
        fit_exact_z=False,
    ),
    "SM_DiverterFlapper": StationPropSpec(
        canonical_name="SM_DiverterFlapper",
        slot_property="diverter_flapper_mesh_slot",
        envelope_cm=(84.0, 3.0, 91.0),
        target_xy_cm=(0.0, 0.0),
        z_align_mode="min",
        target_z_cm=0.0,  # Bottom-Hinge Z=0 on DiverterFlapperPivot
        fit_exact_z=True,
    ),
    "SM_TareChute": StationPropSpec(
        canonical_name="SM_TareChute",
        slot_property="tare_chute_mesh_slot",
        envelope_cm=(86.0, 84.0, 24.0),
        target_xy_cm=(0.0, 86.0),  # Mandatory (0, 86, 0) offset on ScaleRoot per VISUAL_PRESENTATION_CONTRACT.md
        z_align_mode="min",
        target_z_cm=0.0,
        fit_exact_z=False,
        is_secondary_prop=True,
    ),
    "SM_DirectorBoothFrame": StationPropSpec(
        canonical_name="SM_DirectorBoothFrame",
        slot_property="director_booth_frame_mesh_slot",
        envelope_cm=(24.0, 190.0, 85.0),
        target_xy_cm=(0.0, 0.0),
        z_align_mode="center",
        target_z_cm=0.0,
        fit_exact_z=True,
    ),
    "SM_DirectorShutter": StationPropSpec(
        canonical_name="SM_DirectorShutter",
        slot_property="director_shutter_mesh_slot",
        envelope_cm=(6.0, 165.0, 62.0),
        target_xy_cm=(0.0, 0.0),
        z_align_mode="center",
        target_z_cm=0.0,
        fit_exact_z=True,
    ),
    "SM_CellFloorGrate": StationPropSpec(
        canonical_name="SM_CellFloorGrate",
        slot_property="cell_floor_grate_mesh_slot",
        envelope_cm=(770.0, 760.0, 20.0),
        target_xy_cm=(0.0, 0.0),
        z_align_mode="max",
        target_z_cm=10.0,  # Flush with PitFloorCatchCollider top at Local Z = +10.0 cm
        fit_exact_z=True,
        is_secondary_prop=True,
    ),
}

MONOLITH_MESH_SPECS: Dict[str, MonolithMeshSpec] = {
    "SM_Monolith_Contract04": MonolithMeshSpec(
        canonical_name="SM_Monolith_Contract04",
        contract_index=3,
        envelope_cm=(42.0, 42.0, 82.0),
        target_mass_kg=66.528,  # Exact fallback Box (30 * 28 * 88) * 0.90 * 0.001 = 66.528 kg (~66.53 kg)
    ),
    "SM_Monolith_Contract06": MonolithMeshSpec(
        canonical_name="SM_Monolith_Contract06",
        contract_index=5,
        envelope_cm=(45.0, 46.0, 86.0),
        target_mass_kg=83.560,  # Calibrated custom monolith target mass (~83.56 kg)
    ),
}

BASELINE_CONTRACT_SPECS = [
    {
        "name": "DA_Contract_01",
        "code": "ACT I-A // CTR-01: STANDARD SLAG BILLET",
        "directive": "CUT 12.0 KG TO TITHE | STAMP [E]",
        "shape": "BOX",
        "dims": (24.0, 24.0, 75.0),
        "density": 0.80,
        "target_kg": 12.0,
        "tol_kg": 1.25,
        "max_tare_kg": 14.0,
        "allow_diverter": False,
        "max_torque_nm": 26.0,
        "grace_s": 6.5,
        "cooldown_s": 3.0,
        "stiff": 180.0,
        "damp": 24.0,
        "veins": [],
    },
    {
        "name": "DA_Contract_02",
        "code": "ACT I-B // CTR-02: ECCENTRIC L-CASTING",
        "directive": "CUT GREEN YIELD | [SPACE] DUMPS TARE",
        "shape": "ASYMMETRIC_L_BLOCK",
        "dims": (24.0, 32.0, 80.0),
        "density": 0.85,
        "target_kg": 14.5,
        "tol_kg": 1.35,
        "max_tare_kg": 16.0,
        "allow_diverter": True,
        "max_torque_nm": 26.5,
        "grace_s": 6.5,
        "cooldown_s": 3.0,
        "stiff": 180.0,
        "damp": 24.0,
        "veins": [],
    },
    {
        "name": "DA_Contract_03",
        "code": "ACT II-A // CTR-03: SINGLE-VEIN OCTAGON CORE",
        "directive": "HIDDEN VEIN: AUDIO SCAN & BEVEL [RMB]",
        "shape": "OCTAGONAL_PRISM",
        "dims": (30.0, 30.0, 84.0),
        "density": 0.95,
        "target_kg": 15.0,
        "tol_kg": 1.00,
        "max_tare_kg": 18.0,
        "allow_diverter": True,
        "max_torque_nm": 28.0,
        "grace_s": 6.5,
        "cooldown_s": 3.0,
        "stiff": 180.0,
        "damp": 24.0,
        "veins": [((-4.0, -7.0, -14.0), (4.0, 7.0, -6.0), 2.8, 18.0)],
    },
    {
        "name": "DA_Contract_04",
        "code": "ACT II-B // CTR-04: CROSSED ISOTOPE LATTICE",
        "directive": "SLAG COLUMN: DODGE VEIN | WATCH TARE",
        "shape": "BOX",
        "dims": (30.0, 28.0, 88.0),
        "density": 0.90,
        "target_kg": 18.0,
        "tol_kg": 0.95,
        "max_tare_kg": 22.0,
        "allow_diverter": True,
        "max_torque_nm": 30.0,
        "grace_s": 6.5,
        "cooldown_s": 3.0,
        "stiff": 180.0,
        "damp": 24.0,
        "veins": [
            ((-6.0, -9.0, 6.0), (6.0, -2.0, 20.0), 2.5, 18.0),
            ((6.0, 3.0, -24.0), (-6.0, 9.0, -10.0), 2.5, 18.0),
        ],
    },
    {
        "name": "DA_Contract_05",
        "code": "ACT III-A // CTR-05: DENSE BISMUTH COUNTER-TORQUE",
        "directive": "TORQUE LIMIT: KEEP CUTS BALANCED",
        "shape": "ASYMMETRIC_L_BLOCK",
        "dims": (22.0, 28.0, 76.0),
        "density": 1.95,
        "target_kg": 21.0,
        "tol_kg": 0.90,
        "max_tare_kg": 28.0,
        "allow_diverter": True,
        "max_torque_nm": 31.0,
        "grace_s": 6.5,
        "cooldown_s": 3.0,
        "stiff": 210.0,
        "damp": 28.0,
        "veins": [
            ((-4.0, 4.8, -29.0), (4.0, 9.8, -16.0), 2.4, 17.0),
            ((-4.0, -9.8, 4.0), (4.0, -3.0, 18.0), 2.4, 17.0),
        ],
    },
    {
        "name": "DA_Contract_06",
        "code": "ACT III-B // CTR-06: DIRECTORATE STEPPED CORE",
        "directive": "DIRECTOR'S INGOT: DUAL VEINS + TORQUE",
        "shape": "STEPPED_CORE",
        "dims": (24.0, 34.0, 84.0),
        "density": 1.75,
        "target_kg": 24.0,
        "tol_kg": 0.85,
        "max_tare_kg": 30.0,
        "allow_diverter": True,
        "max_torque_nm": 34.0,
        "grace_s": 6.5,
        "cooldown_s": 3.0,
        "stiff": 220.0,
        "damp": 28.0,
        "veins": [
            ((-5.0, -12.5, -9.0), (5.0, -7.5, 7.0), 2.3, 16.0),
            ((5.0, 2.5, -31.0), (-5.0, 6.8, -19.0), 2.3, 16.0),
            ((-4.5, -4.5, 18.5), (4.5, 4.5, 27.0), 2.2, 16.0),
        ],
    },
]

SUPPORTED_EXTENSIONS = (".glb", ".fbx")


def log_info(msg: str) -> None:
    unreal.log(f"[ImportRodinBatch] {msg}")


def log_warn(msg: str) -> None:
    unreal.log_warning(f"[ImportRodinBatch] {msg}")


def log_error(msg: str) -> None:
    unreal.log_error(f"[ImportRodinBatch] {msg}")


def normalize_raw_rodin_filenames(raw_dir: str) -> None:
    os.makedirs(raw_dir, exist_ok=True)
    all_canonical = set(STATION_PROP_SPECS.keys()) | set(MONOLITH_MESH_SPECS.keys())
    compact_to_canonical = {
        re.sub(r"[^a-z0-9]", "", name.lower()): name for name in all_canonical
    }

    for entry in sorted(os.listdir(raw_dir)):
        src_path = os.path.join(raw_dir, entry)
        if not os.path.isfile(src_path):
            continue

        lower_name = entry.lower()
        ext_found = None
        stem_raw = entry
        # Strip repeated .glb.glb or .fbx.fbx extensions
        for ext in SUPPORTED_EXTENSIONS:
            while stem_raw.lower().endswith(ext):
                stem_raw = stem_raw[: -len(ext)]
                ext_found = ext

        if ext_found is None:
            continue

        # Strip common browser download suffixes like " (1)" or "_1" at the end if base matches canonical
        cleaned_stem = re.sub(r"\s*\(\d+\)\s*$", "", stem_raw).strip()
        compact_key = re.sub(r"[^a-z0-9]", "", cleaned_stem.lower())
        canonical = compact_to_canonical.get(compact_key)
        if canonical is None:
            continue

        desired_name = f"{canonical}{ext_found}"
        dst_path = os.path.join(raw_dir, desired_name)
        if os.path.normcase(src_path) != os.path.normcase(dst_path) and not os.path.exists(dst_path):
            os.rename(src_path, dst_path)
            log_info(f"Normalized raw Rodin filename '{entry}' -> '{desired_name}'")


def discover_source_files(raw_dir: str) -> Dict[str, str]:
    normalize_raw_rodin_filenames(raw_dir)
    all_canonical = set(STATION_PROP_SPECS.keys()) | set(MONOLITH_MESH_SPECS.keys())
    discovered: Dict[str, str] = {}
    entries = sorted(os.listdir(raw_dir))
    lower_to_canonical = {name.lower(): name for name in all_canonical}

    for ext in SUPPORTED_EXTENSIONS:
        for entry in entries:
            full_path = os.path.join(raw_dir, entry)
            if not os.path.isfile(full_path):
                continue
            stem, file_ext = os.path.splitext(entry)
            if file_ext.lower() != ext:
                continue
            if stem.lower() == "sm_cellwallpanel":
                log_warn(
                    f"Skipping '{entry}': VISUAL_PRESENTATION_CONTRACT.md forbids populating CellWallPanelMeshSlot."
                )
                continue
            canonical = lower_to_canonical.get(stem.lower())
            if canonical and canonical not in discovered:
                discovered[canonical] = full_path

    for entry in entries:
        subdir = os.path.join(raw_dir, entry)
        if not os.path.isdir(subdir):
            continue
        canonical = lower_to_canonical.get(entry.lower())
        if not canonical or canonical in discovered:
            continue
        sub_entries = sorted(os.listdir(subdir))
        for ext in SUPPORTED_EXTENSIONS:
            matched = [
                os.path.join(subdir, f)
                for f in sub_entries
                if os.path.isfile(os.path.join(subdir, f)) and os.path.splitext(f)[1].lower() == ext
            ]
            if matched:
                discovered[canonical] = matched[0]
                break

    return discovered


def clamp_texture_max_size(tex: unreal.Texture2D, max_size: int) -> bool:
    modified = False
    for prop_name in ("maximum_texture_size", "max_texture_size"):
        try:
            current_max = int(tex.get_editor_property(prop_name))
            if current_max == 0 or current_max > max_size:
                tex.set_editor_property(prop_name, max_size)
                modified = True
            break
        except Exception:
            continue
    return modified


def build_interchange_pipeline(canonical_name: str, is_sliceable_monolith: bool) -> Optional[unreal.Object]:
    if not hasattr(unreal, "InterchangeGenericAssetsPipeline"):
        return None
    pipeline = unreal.InterchangeGenericAssetsPipeline()
    pipeline.set_editor_property("use_source_name_for_asset", True)
    pipeline.set_editor_property("asset_name", canonical_name)
    pipeline.set_editor_property("scene_name_sub_folder", False)
    pipeline.set_editor_property("asset_type_sub_folders", False)

    mesh_pipeline = pipeline.get_editor_property("mesh_pipeline")
    if mesh_pipeline is not None:
        mesh_pipeline.set_editor_property("import_static_meshes", True)
        if hasattr(unreal, "InterchangeCombineStaticMeshesBehavior"):
            mesh_pipeline.set_editor_property(
                "combine_static_meshes_behavior",
                unreal.InterchangeCombineStaticMeshesBehavior.ALL,
            )
        mesh_pipeline.set_editor_property("collision", False)
        mesh_pipeline.set_editor_property("build_nanite", not is_sliceable_monolith)
        mesh_pipeline.set_editor_property("generate_lightmap_u_vs", False)

    if is_sliceable_monolith:
        # Sliceable monoliths overwrite UV0 and bind M_SlagOuter_Master + M_MoltenCutCap_Master directly
        for sub_prop, flag_prop in (
            ("material_pipeline", "import_materials"),
            ("texture_pipeline", "import_textures"),
        ):
            try:
                sub_pipe = pipeline.get_editor_property(sub_prop)
                if sub_pipe is not None:
                    sub_pipe.set_editor_property(flag_prop, False)
            except Exception:
                pass

    return pipeline


def import_mesh_bundle(
    src_file: str,
    canonical_name: str,
    is_sliceable_monolith: bool,
    max_tex_size: int = MAX_TEXTURE_SIZE_PRIMARY,
) -> Tuple[unreal.StaticMesh, List[unreal.Object]]:
    if not unreal.EditorAssetLibrary.does_directory_exist(DEST_MESHES_DIR):
        unreal.EditorAssetLibrary.make_directory(DEST_MESHES_DIR)

    # Isolate per-prop PBR materials & textures in /Game/Presentation/Meshes/PBR/<CanonicalName>
    # so identical internal glTF names ('model', 'texture_diffuse', etc.) NEVER collide across props!
    bundle_dir = (
        f"{DEST_MESHES_DIR}/_Tmp_{canonical_name}"
        if is_sliceable_monolith
        else f"{DEST_PBR_DIR}/{canonical_name}"
    )
    if unreal.EditorAssetLibrary.does_directory_exist(bundle_dir):
        unreal.EditorAssetLibrary.delete_directory(bundle_dir)
    unreal.EditorAssetLibrary.make_directory(bundle_dir)

    target_mesh_path = f"{DEST_MESHES_DIR}/{canonical_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(target_mesh_path):
        unreal.EditorAssetLibrary.delete_asset(target_mesh_path)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", src_file)
    task.set_editor_property("destination_path", bundle_dir)
    task.set_editor_property("destination_name", canonical_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", False)
    try:
        task.set_editor_property("async_", False)
    except Exception:
        pass

    pipeline = build_interchange_pipeline(canonical_name, is_sliceable_monolith)
    if pipeline is not None:
        try:
            task.set_editor_property("options", pipeline)
        except Exception:
            pass

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_tools.import_asset_tasks([task])

    imported_objects: List[unreal.Object] = list(task.get_objects() or [])
    for obj_path in list(task.get_editor_property("imported_object_paths") or []):
        loaded = unreal.EditorAssetLibrary.load_asset(obj_path)
        if loaded is not None and loaded not in imported_objects:
            imported_objects.append(loaded)

    for asset_pkg in list(unreal.EditorAssetLibrary.list_assets(bundle_dir, recursive=True, include_folder=False) or []):
        loaded = unreal.EditorAssetLibrary.load_asset(asset_pkg)
        if loaded is not None and loaded not in imported_objects:
            imported_objects.append(loaded)

    static_meshes = [obj for obj in imported_objects if isinstance(obj, unreal.StaticMesh)]
    if not static_meshes:
        raise RuntimeError(f"Import of '{src_file}' produced zero UStaticMesh assets.")

    primary_mesh: unreal.StaticMesh = static_meshes[0]
    for sm in static_meshes:
        if sm.get_name() == canonical_name:
            primary_mesh = sm
            break

    src_pkg_path = primary_mesh.get_path_name().split(".")[0]
    if src_pkg_path != target_mesh_path:
        if not unreal.EditorAssetLibrary.rename_asset(src_pkg_path, target_mesh_path):
            raise RuntimeError(f"Failed moving imported mesh {src_pkg_path} -> {target_mesh_path}")
        reloaded = unreal.EditorAssetLibrary.load_asset(target_mesh_path)
        if not isinstance(reloaded, unreal.StaticMesh):
            raise RuntimeError(f"Failed reloading moved mesh at {target_mesh_path}")
        primary_mesh = reloaded

    final_objects: List[unreal.Object] = [primary_mesh]

    if is_sliceable_monolith:
        # Remove any unused Rodin PBR textures/materials imported for custom monoliths
        if unreal.EditorAssetLibrary.does_directory_exist(bundle_dir):
            unreal.EditorAssetLibrary.delete_directory(bundle_dir)
    else:
        clamped_count = 0
        for asset_pkg in list(unreal.EditorAssetLibrary.list_assets(bundle_dir, recursive=True, include_folder=False) or []):
            obj = unreal.EditorAssetLibrary.load_asset(asset_pkg)
            if obj is None:
                continue
            if isinstance(obj, unreal.Texture2D):
                if clamp_texture_max_size(obj, max_tex_size):
                    clamped_count += 1
            if obj not in final_objects:
                final_objects.append(obj)
        if clamped_count > 0:
            log_info(f"Clamped {clamped_count} Texture2D asset(s) to {max_tex_size}px for {canonical_name}")

    return primary_mesh, final_objects


def copy_static_mesh_to_dynamic(static_mesh: unreal.StaticMesh) -> unreal.DynamicMesh:
    dyn_mesh = unreal.DynamicMesh()
    copy_from_opts = unreal.GeometryScriptCopyMeshFromAssetOptions()
    copy_from_opts.set_editor_property("apply_build_settings", True)
    copy_from_opts.set_editor_property("request_tangents", False)
    copy_from_opts.set_editor_property("ignore_remove_degenerates", False)

    read_lod = unreal.GeometryScriptMeshReadLOD()
    read_lod.set_editor_property("lod_type", unreal.GeometryScriptLODType.MAX_AVAILABLE)
    read_lod.set_editor_property("lod_index", 0)

    dyn_mesh, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(
        static_mesh, dyn_mesh, copy_from_opts, read_lod, False
    )
    if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError(f"GeometryScript failed reading LOD0 from {static_mesh.get_name()}")
    return dyn_mesh


def should_swap_xy_90_deg(
    raw_x: float,
    raw_y: float,
    target_x: float,
    target_y: float,
) -> Tuple[bool, float, float]:
    sx0 = target_x / max(raw_x, 1.0e-3)
    sy0 = target_y / max(raw_y, 1.0e-3)
    anis_0 = max(sx0 / max(sy0, 1.0e-6), sy0 / max(sx0, 1.0e-6))

    sx90 = target_x / max(raw_y, 1.0e-3)
    sy90 = target_y / max(raw_x, 1.0e-3)
    anis_90 = max(sx90 / max(sy90, 1.0e-6), sy90 / max(sx90, 1.0e-6))

    return (anis_90 < anis_0 * 0.99), anis_0, anis_90


def compute_envelope_scale(
    size_xyz: Tuple[float, float, float],
    envelope_xyz: Tuple[float, float, float],
) -> Tuple[float, float, float]:
    sx = envelope_xyz[0] / max(size_xyz[0], 1.0e-3)
    sy = envelope_xyz[1] / max(size_xyz[1], 1.0e-3)
    sz = envelope_xyz[2] / max(size_xyz[2], 1.0e-3)

    s_base = sorted((sx, sy, sz))[1]
    return (min(s_base, sx), min(s_base, sy), min(s_base, sz))


def get_dynamic_mesh_vertices(dyn_mesh: unreal.DynamicMesh) -> List[unreal.Vector]:
    _, vec_list, _ = unreal.GeometryScript_MeshQueries.get_all_vertex_positions(
        dyn_mesh, True
    )
    return list(unreal.GeometryScript_List.convert_vector_list_to_array(vec_list))


def choose_booth_frame_front_yaw(
    verts: List[unreal.Vector],
    raw_min: unreal.Vector,
    raw_max: unreal.Vector,
    swap_90: bool,
) -> float:
    # BlastShutterVisual slides at Local X = -10 cm (front/camera side).
    # Choose the 180-deg sign so the central window pane recess sits toward +X (clearing X = -10 cm).
    candidates = [90.0, -90.0] if swap_90 else [0.0, 180.0]
    cx = 0.5 * (float(raw_min.x) + float(raw_max.x))
    cy = 0.5 * (float(raw_min.y) + float(raw_max.y))
    cz = 0.5 * (float(raw_min.z) + float(raw_max.z))
    rx = max(float(raw_max.x) - float(raw_min.x), 1.0e-3)
    ry = max(float(raw_max.y) - float(raw_min.y), 1.0e-3)
    rz = max(float(raw_max.z) - float(raw_min.z), 1.0e-3)

    best_yaw = candidates[0]
    best_mean_x = -1.0e9
    for yaw in candidates:
        xf = unreal.Transform(
            location=unreal.Vector(0.0, 0.0, 0.0),
            rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw),
            scale=unreal.Vector(1.0, 1.0, 1.0),
        )
        c_rot = xf.transform_location(unreal.Vector(cx, cy, cz))
        span_y = rx if abs(yaw) == 90.0 else ry
        span_z = rz
        window_xs: List[float] = []
        for v in verts:
            p = xf.transform_location(v)
            if abs(float(p.y) - float(c_rot.y)) <= 0.25 * span_y and abs(float(p.z) - float(c_rot.z)) <= 0.25 * span_z:
                window_xs.append(float(p.x) - float(c_rot.x))
        if window_xs:
            mean_x = sum(window_xs) / len(window_xs)
            if mean_x > best_mean_x:
                best_mean_x = mean_x
                best_yaw = yaw
    return best_yaw


def compute_station_visual_transform(
    static_mesh: unreal.StaticMesh,
    spec: StationPropSpec,
) -> Tuple[unreal.Transform, Tuple[float, float, float], int, float]:
    dyn_mesh = copy_static_mesh_to_dynamic(static_mesh)
    tri_count = int(dyn_mesh.get_triangle_count())
    bbox = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(dyn_mesh)
    raw_min = bbox.min
    raw_max = bbox.max

    raw_x = max(float(raw_max.x) - float(raw_min.x), 1.0e-3)
    raw_y = max(float(raw_max.y) - float(raw_min.y), 1.0e-3)
    raw_z = max(float(raw_max.z) - float(raw_min.z), 1.0e-3)

    # Trap A: Horizontal 90-Deg Axis-Swap Guard (Zero Aspect Crushing)
    swap_90, anis_0, anis_90 = should_swap_xy_90_deg(
        raw_x, raw_y, spec.envelope_cm[0], spec.envelope_cm[1]
    )

    verts: Optional[List[unreal.Vector]] = None
    if spec.canonical_name == "SM_DirectorBoothFrame":
        verts = get_dynamic_mesh_vertices(dyn_mesh)
        yaw_deg = choose_booth_frame_front_yaw(verts, raw_min, raw_max, swap_90)
    else:
        yaw_deg = 90.0 if swap_90 else 0.0

    rot_x = raw_y if abs(yaw_deg) == 90.0 else raw_x
    rot_y = raw_x if abs(yaw_deg) == 90.0 else raw_y
    rot_z = raw_z

    parent_sx = spec.envelope_cm[0] / rot_x
    parent_sy = spec.envelope_cm[1] / rot_y
    raw_sz_ratio = spec.envelope_cm[2] / rot_z
    if spec.fit_exact_z:
        parent_sz = raw_sz_ratio
    else:
        # Preserve sculpted curb/base proportions on horizontal trays & bases without vertical stretching
        horiz_mean = 0.5 * (parent_sx + parent_sy)
        parent_sz = min(raw_sz_ratio, horiz_mean * 1.05)

    if abs(yaw_deg) == 90.0:
        mesh_scale = unreal.Vector(parent_sy, parent_sx, parent_sz)
    else:
        mesh_scale = unreal.Vector(parent_sx, parent_sy, parent_sz)

    rot_scale_xf = unreal.Transform(
        location=unreal.Vector(0.0, 0.0, 0.0),
        rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw_deg),
        scale=mesh_scale,
    )

    corners = [
        unreal.Vector(x, y, z)
        for x in (float(raw_min.x), float(raw_max.x))
        for y in (float(raw_min.y), float(raw_max.y))
        for z in (float(raw_min.z), float(raw_max.z))
    ]
    xf_corners = [rot_scale_xf.transform_location(c) for c in corners]
    xf_min_x = min(float(p.x) for p in xf_corners)
    xf_max_x = max(float(p.x) for p in xf_corners)
    xf_min_y = min(float(p.y) for p in xf_corners)
    xf_max_y = max(float(p.y) for p in xf_corners)
    xf_min_z = min(float(p.z) for p in xf_corners)
    xf_max_z = max(float(p.z) for p in xf_corners)

    tx = spec.target_xy_cm[0] - 0.5 * (xf_min_x + xf_max_x)
    ty = spec.target_xy_cm[1] - 0.5 * (xf_min_y + xf_max_y)

    if spec.z_align_mode == "min":
        tz = spec.target_z_cm - xf_min_z
    elif spec.z_align_mode == "max":
        tz = spec.target_z_cm - xf_max_z
    elif spec.z_align_mode == "deck_top":
        # Align interior central pan deck surface flush with PanDeckCollider top at Z = +3.0 cm
        if verts is None:
            verts = get_dynamic_mesh_vertices(dyn_mesh)
        cx_raw = 0.5 * (float(raw_min.x) + float(raw_max.x))
        cy_raw = 0.5 * (float(raw_min.y) + float(raw_max.y))
        central_zs = sorted(
            float(v.z)
            for v in verts
            if abs(float(v.x) - cx_raw) <= 0.175 * raw_x and abs(float(v.y) - cy_raw) <= 0.175 * raw_y
        )
        if central_zs:
            raw_deck_top_z = central_zs[min(len(central_zs) - 1, int(0.98 * len(central_zs)))]
        else:
            raw_deck_top_z = float(raw_min.z) + 0.15 * raw_z
        xf_deck_pt = rot_scale_xf.transform_location(unreal.Vector(cx_raw, cy_raw, raw_deck_top_z))
        tz = spec.target_z_cm - float(xf_deck_pt.z)
    else:
        tz = spec.target_z_cm - 0.5 * (xf_min_z + xf_max_z)

    final_xf = unreal.Transform(
        location=unreal.Vector(tx, ty, tz),
        rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw_deg),
        scale=mesh_scale,
    )

    final_corners = [final_xf.transform_location(c) for c in corners]
    fitted_dims = (
        max(float(p.x) for p in final_corners) - min(float(p.x) for p in final_corners),
        max(float(p.y) for p in final_corners) - min(float(p.y) for p in final_corners),
        max(float(p.z) for p in final_corners) - min(float(p.z) for p in final_corners),
    )

    # Hard envelope & pivot assertions
    assert fitted_dims[0] <= spec.envelope_cm[0] + 0.05, f"{spec.canonical_name} X overflow: {fitted_dims}"
    assert fitted_dims[1] <= spec.envelope_cm[1] + 0.05, f"{spec.canonical_name} Y overflow: {fitted_dims}"
    assert fitted_dims[2] <= spec.envelope_cm[2] + 0.05, f"{spec.canonical_name} Z overflow: {fitted_dims}"

    log_info(
        f"Fit {spec.canonical_name}: raw=({raw_x:.1f},{raw_y:.1f},{raw_z:.1f}) -> "
        f"yaw={yaw_deg:.0f}deg (anis0={anis_0:.2f}, anis90={anis_90:.2f}) -> "
        f"fitted=({fitted_dims[0]:.1f},{fitted_dims[1]:.1f},{fitted_dims[2]:.1f}) cm"
    )
    return final_xf, fitted_dims, tri_count, yaw_deg


def clear_dynamic_mesh_material_ids(dyn_mesh: unreal.DynamicMesh, clear_val: int = 0) -> unreal.DynamicMesh:
    mat_lib = unreal.GeometryScript_Materials
    for fn_name in ("clear_material_i_ds", "clear_material_ids"):
        fn = getattr(mat_lib, fn_name, None)
        if fn is not None:
            return fn(dyn_mesh, clear_val)
    return dyn_mesh


def solidify_dynamic_mesh(dyn_mesh: unreal.DynamicMesh, grid_res: int = 96) -> unreal.DynamicMesh:
    grid_params = unreal.GeometryScript3DGridParameters()
    grid_params.set_editor_property("size_method", unreal.GeometryScriptGridSizingMethod.GRID_RESOLUTION)
    grid_params.set_editor_property("grid_resolution", grid_res)

    solidify_opts = unreal.GeometryScriptSolidifyOptions()
    solidify_opts.set_editor_property("grid_parameters", grid_params)
    solidify_opts.set_editor_property("winding_threshold", 0.5)
    solidify_opts.set_editor_property("solid_at_boundaries", True)
    solidify_opts.set_editor_property("extend_bounds", 2.0)
    solidify_opts.set_editor_property("surface_search_steps", 4)

    dyn_mesh = unreal.GeometryScript_MeshVoxelProcessing.apply_mesh_solidify(dyn_mesh, solidify_opts)

    # Keep strictly the primary monolith body if voxelization generated any tiny speck island
    _, total_vol, _ = unreal.GeometryScript_MeshQueries.get_mesh_volume_area_center(dyn_mesh)
    if unreal.GeometryScript_MeshQueries.get_num_connected_components(dyn_mesh) > 1:
        rm_opts = unreal.GeometryScriptRemoveSmallComponentOptions()
        rm_opts.set_editor_property("min_volume", max(1.0, 0.05 * abs(float(total_vol))))
        rm_opts.set_editor_property("min_triangle_count", 16)
        dyn_mesh = unreal.GeometryScript_MeshRepair.remove_small_components(dyn_mesh, rm_opts)

    dyn_mesh = unreal.GeometryScript_MeshRepair.compact_mesh(dyn_mesh)
    return dyn_mesh


def center_and_fit_monolith_mesh(
    dyn_mesh: unreal.DynamicMesh,
    envelope_cm: Tuple[float, float, float],
) -> Tuple[unreal.DynamicMesh, float]:
    bbox = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(dyn_mesh)
    bmin, bmax = bbox.min, bbox.max
    raw_x = max(float(bmax.x) - float(bmin.x), 1.0e-3)
    raw_y = max(float(bmax.y) - float(bmin.y), 1.0e-3)

    swap_90, _, _ = should_swap_xy_90_deg(raw_x, raw_y, envelope_cm[0], envelope_cm[1])
    yaw_deg = 90.0 if swap_90 else 0.0
    if swap_90:
        dyn_mesh = unreal.GeometryScript_MeshTransforms.rotate_mesh(
            dyn_mesh,
            unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0),
            unreal.Vector(0.0, 0.0, 0.0),
        )

    bbox = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(dyn_mesh)
    bmin, bmax = bbox.min, bbox.max
    center = unreal.Vector(
        0.5 * (float(bmin.x) + float(bmax.x)),
        0.5 * (float(bmin.y) + float(bmax.y)),
        0.5 * (float(bmin.z) + float(bmax.z)),
    )
    size_xyz = (
        max(float(bmax.x) - float(bmin.x), 1.0e-3),
        max(float(bmax.y) - float(bmin.y), 1.0e-3),
        max(float(bmax.z) - float(bmin.z), 1.0e-3),
    )
    sx, sy, sz = compute_envelope_scale(size_xyz, envelope_cm)

    dyn_mesh = unreal.GeometryScript_MeshTransforms.translate_mesh(
        dyn_mesh, unreal.Vector(-center.x, -center.y, -center.z)
    )
    dyn_mesh = unreal.GeometryScript_MeshTransforms.scale_mesh(
        dyn_mesh,
        unreal.Vector(sx, sy, sz),
        unreal.Vector(0.0, 0.0, 0.0),
    )
    return dyn_mesh, yaw_deg


def query_signed_solid_distance_cm(
    dyn_mesh: unreal.DynamicMesh,
    bvh: unreal.GeometryScriptDynamicMeshBVH,
    pt: unreal.Vector,
) -> float:
    spatial_opts = unreal.GeometryScriptSpatialQueryOptions()
    spatial_opts.set_editor_property("winding_iso_threshold", 0.5)

    _, is_inside, _ = unreal.GeometryScript_MeshSpatial.is_point_inside_mesh(
        dyn_mesh, bvh, pt, spatial_opts
    )
    _, nearest_pt, _ = unreal.GeometryScript_MeshSpatial.find_nearest_point_on_mesh(
        dyn_mesh, bvh, pt, spatial_opts
    )
    pos = nearest_pt.position
    surf_dist = math.sqrt(
        (float(pt.x) - float(pos.x)) ** 2
        + (float(pt.y) - float(pos.y)) ** 2
        + (float(pt.z) - float(pos.z)) ** 2
    )
    return surf_dist if is_inside else -surf_dist


def condition_and_verify_custom_monolith(
    static_mesh: unreal.StaticMesh,
    spec: MonolithMeshSpec,
    m_slag_outer: unreal.MaterialInterface,
    m_molten_cut_cap: unreal.MaterialInterface,
    vein_tuples: List[Tuple[Tuple[float, float, float], Tuple[float, float, float], float, float]],
) -> Tuple[
    unreal.Vector,
    float,
    List[Tuple[Tuple[float, float, float], Tuple[float, float, float], float, float]],
    ImportedAssetSummary,
]:
    dyn_mesh = copy_static_mesh_to_dynamic(static_mesh)

    weld_opts = unreal.GeometryScriptWeldEdgesOptions()
    weld_opts.set_editor_property("tolerance", 0.001)
    dyn_mesh = unreal.GeometryScript_MeshRepair.weld_mesh_edges(dyn_mesh, weld_opts)

    # First fit into target envelope so voxel resolution & simplification operate in true centimeter units
    dyn_mesh, yaw_deg = center_and_fit_monolith_mesh(dyn_mesh, spec.envelope_cm)

    is_closed = unreal.GeometryScript_MeshQueries.get_is_closed_mesh(dyn_mesh)
    num_islands = int(unreal.GeometryScript_MeshQueries.get_num_connected_components(dyn_mesh))
    _, pre_vol, _ = unreal.GeometryScript_MeshQueries.get_mesh_volume_area_center(dyn_mesh)

    if (not is_closed) or (num_islands != 1) or (float(pre_vol) <= 100.0):
        log_info(
            f"{spec.canonical_name} pre-check (closed={is_closed}, islands={num_islands}, vol={pre_vol:.1f}): "
            f"running ApplyMeshSolidify voxel wrap..."
        )
        dyn_mesh = solidify_dynamic_mesh(dyn_mesh, grid_res=96)

    if dyn_mesh.get_triangle_count() > MAX_MONOLITH_TRIANGLES:
        before_tris = int(dyn_mesh.get_triangle_count())
        simp_opts = unreal.GeometryScriptSimplifyMeshOptions()
        dyn_mesh = unreal.GeometryScript_MeshSimplification.apply_simplify_to_triangle_count(
            dyn_mesh, TARGET_SIMPLIFY_TRIANGLES, simp_opts
        )
        log_info(
            f"Simplified {spec.canonical_name}: {before_tris} -> {dyn_mesh.get_triangle_count()} triangles"
        )

    # Post-simplify watertightness & single-island fallback guard
    if (
        not unreal.GeometryScript_MeshQueries.get_is_closed_mesh(dyn_mesh)
        or int(unreal.GeometryScript_MeshQueries.get_num_connected_components(dyn_mesh)) != 1
    ):
        log_warn(f"{spec.canonical_name} lost manifold closure during simplify; re-solidifying cleanly...")
        dyn_mesh = solidify_dynamic_mesh(dyn_mesh, grid_res=84)
        simp_opts = unreal.GeometryScriptSimplifyMeshOptions()
        dyn_mesh = unreal.GeometryScript_MeshSimplification.apply_simplify_to_triangle_count(
            dyn_mesh, TARGET_SIMPLIFY_TRIANGLES, simp_opts
        )

    # Re-center bounding box precisely at (0, 0, 0) inside spec.envelope_cm
    bbox = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(dyn_mesh)
    bmin, bmax = bbox.min, bbox.max
    recenter = unreal.Vector(
        0.5 * (float(bmin.x) + float(bmax.x)),
        0.5 * (float(bmin.y) + float(bmax.y)),
        0.5 * (float(bmin.z) + float(bmax.z)),
    )
    dyn_mesh = unreal.GeometryScript_MeshTransforms.translate_mesh(
        dyn_mesh, unreal.Vector(-recenter.x, -recenter.y, -recenter.z)
    )
    cur_size = (
        max(float(bmax.x) - float(bmin.x), 1.0e-3),
        max(float(bmax.y) - float(bmin.y), 1.0e-3),
        max(float(bmax.z) - float(bmin.z), 1.0e-3),
    )
    clamp_s = min(
        1.0,
        spec.envelope_cm[0] / cur_size[0],
        spec.envelope_cm[1] / cur_size[1],
        spec.envelope_cm[2] / cur_size[2],
    )
    if clamp_s < 0.9999:
        dyn_mesh = unreal.GeometryScript_MeshTransforms.scale_mesh(
            dyn_mesh,
            unreal.Vector(clamp_s, clamp_s, clamp_s),
            unreal.Vector(0.0, 0.0, 0.0),
        )

    # Ensure positive outward winding
    _, signed_vol, _ = unreal.GeometryScript_MeshQueries.get_mesh_volume_area_center(dyn_mesh)
    if float(signed_vol) < 0.0:
        dyn_mesh = unreal.GeometryScript_Normals.flip_normals(dyn_mesh)

    # Compute crisp split normals, non-degenerate box-projected UVs, and Slot 0 across exterior surface
    split_opts = unreal.GeometryScriptSplitNormalsOptions()
    split_opts.set_editor_property("split_by_opening_angle", True)
    split_opts.set_editor_property("opening_angle_deg", 25.0)
    calc_opts = unreal.GeometryScriptCalculateNormalsOptions()
    calc_opts.set_editor_property("angle_weighted", True)
    calc_opts.set_editor_property("area_weighted", True)
    dyn_mesh = unreal.GeometryScript_Normals.compute_split_normals(dyn_mesh, split_opts, calc_opts)
    dyn_mesh = unreal.GeometryScript_UVs.set_num_uv_sets(dyn_mesh, 1)
    dyn_mesh = unreal.GeometryScript_UVs.set_mesh_u_vs_from_box_projection(
        dyn_mesh,
        0,
        unreal.Transform(
            location=unreal.Vector(0.0, 0.0, 0.0),
            rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0),
            scale=unreal.Vector(50.0, 50.0, 50.0),
        ),
        unreal.GeometryScriptMeshSelection(),
        2,
    )
    dyn_mesh = clear_dynamic_mesh_material_ids(dyn_mesh, 0)

    copy_to_opts = unreal.GeometryScriptCopyMeshToAssetOptions()
    copy_to_opts.set_editor_property("enable_recompute_normals", False)
    copy_to_opts.set_editor_property("enable_recompute_tangents", True)
    copy_to_opts.set_editor_property("enable_remove_degenerates", True)
    copy_to_opts.set_editor_property("replace_materials", True)
    copy_to_opts.set_editor_property(
        "new_material_usage",
        unreal.CopyMeshToAssetUseNewMaterialsOptions.REPLACE_ASSET_MATERIALS,
    )
    copy_to_opts.set_editor_property("new_materials", [m_slag_outer, m_molten_cut_cap])
    copy_to_opts.set_editor_property("new_material_slot_names", ["OuterShell", "MoltenCutCap"])
    copy_to_opts.set_editor_property("clean_assigned_materials", False)
    copy_to_opts.set_editor_property("apply_nanite_settings", True)
    nanite_settings = copy_to_opts.get_editor_property("new_nanite_settings")
    nanite_settings.set_editor_property("enabled", False)
    copy_to_opts.set_editor_property("new_nanite_settings", nanite_settings)

    write_lod = unreal.GeometryScriptMeshWriteLOD()
    write_lod.set_editor_property("write_hi_res_source", False)
    write_lod.set_editor_property("lod_index", 0)

    dyn_mesh, write_outcome = unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(
        dyn_mesh, static_mesh, copy_to_opts, write_lod, True
    )
    if write_outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError(f"GeometryScript failed writing conditioned LOD0 to {static_mesh.get_name()}")

    # Guarantee 2 material slots (Slot 0 = M_SlagOuter_Master, Slot 1 = M_MoltenCutCap_Master) & CPU access
    static_mats = list(static_mesh.get_editor_property("static_materials") or [])
    if len(static_mats) < 2:
        slot0 = unreal.StaticMaterial()
        slot0.set_editor_property("material_interface", m_slag_outer)
        slot0.set_editor_property("material_slot_name", "OuterShell")
        slot1 = unreal.StaticMaterial()
        slot1.set_editor_property("material_interface", m_molten_cut_cap)
        slot1.set_editor_property("material_slot_name", "MoltenCutCap")
        static_mesh.set_editor_property("static_materials", [slot0, slot1])
    else:
        static_mats[0].set_editor_property("material_interface", m_slag_outer)
        static_mats[1].set_editor_property("material_interface", m_molten_cut_cap)
        static_mesh.set_editor_property("static_materials", static_mats[:2])

    static_mesh.set_editor_property("allow_cpu_access", True)

    # Read back LOD0 exactly as UMonolithSliceComponent::InitializeMonolithFromContract does at runtime
    verify_mesh = copy_static_mesh_to_dynamic(static_mesh)
    weld_verify = unreal.GeometryScriptWeldEdgesOptions()
    weld_verify.set_editor_property("tolerance", 0.001)
    verify_mesh = unreal.GeometryScript_MeshRepair.weld_mesh_edges(verify_mesh, weld_verify)

    final_closed = bool(unreal.GeometryScript_MeshQueries.get_is_closed_mesh(verify_mesh))
    final_islands = int(unreal.GeometryScript_MeshQueries.get_num_connected_components(verify_mesh))
    final_tris = int(verify_mesh.get_triangle_count())
    _, exact_vol_cm3, exact_com = unreal.GeometryScript_MeshQueries.get_mesh_volume_area_center(verify_mesh)
    exact_vol_cm3 = float(exact_vol_cm3)

    if not final_closed or final_islands != 1 or final_tris > MAX_MONOLITH_TRIANGLES or exact_vol_cm3 <= 100.0:
        raise AssertionError(
            f"{spec.canonical_name} failed watertight readback gate: closed={final_closed}, "
            f"islands={final_islands}, tris={final_tris}, vol={exact_vol_cm3:.1f}"
        )

    v_bbox = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(verify_mesh)
    dim_x = max(5.0, float(v_bbox.max.x) - float(v_bbox.min.x))
    dim_y = max(5.0, float(v_bbox.max.y) - float(v_bbox.min.y))
    dim_z = max(5.0, float(v_bbox.max.z) - float(v_bbox.min.z))
    final_dims = unreal.Vector(dim_x, dim_y, dim_z)

    verify_mesh, bvh = unreal.GeometryScript_MeshSpatial.build_bvh_for_mesh(verify_mesh)

    # Verify Top 17.5% Z Center Grip Region (ClampGripBoxLocal in MonolithSliceComponent.cpp:525-534)
    half_jaw_x = max(4.0, 0.325 * dim_x)
    half_jaw_y = max(4.0, 0.325 * dim_y)
    jaw_embed_z = max(5.0, 0.175 * dim_z)
    grip_min_z = float(v_bbox.max.z) - jaw_embed_z
    grip_max_z = float(v_bbox.max.z) + 8.0
    grip_probe_z = float(v_bbox.max.z) - 0.5 * jaw_embed_z
    grip_center_dist = query_signed_solid_distance_cm(
        verify_mesh, bvh, unreal.Vector(0.0, 0.0, grip_probe_z)
    )
    verify_verts = get_dynamic_mesh_vertices(verify_mesh)
    grip_vert_count = sum(
        1
        for v in verify_verts
        if abs(float(v.x)) <= half_jaw_x
        and abs(float(v.y)) <= half_jaw_y
        and grip_min_z <= float(v.z) <= grip_max_z
    )
    grip_verified = (grip_center_dist > 0.5) and (grip_vert_count > 0)
    if not grip_verified:
        raise AssertionError(
            f"{spec.canonical_name} failed top 17.5% ClampGripBoxLocal occupancy check "
            f"(center_dist={grip_center_dist:.2f} cm, grip_verts={grip_vert_count})!"
        )

    # Calibrate DensityGramsPerCm3 so VolumeCm3 * Density * 0.001 matches target fallback mass
    calibrated_density = (spec.target_mass_kg * 1000.0) / exact_vol_cm3
    if not (0.5 <= calibrated_density <= 22.0):
        raise AssertionError(
            f"{spec.canonical_name} calibrated density {calibrated_density:.4f} outside [0.5, 22.0] g/cm3"
        )
    calibrated_mass_kg = exact_vol_cm3 * calibrated_density * 0.001

    # Verify & tuck HiddenVeins sphere centers + radii comfortably inside solid mesh bounds
    calibrated_veins: List[Tuple[Tuple[float, float, float], Tuple[float, float, float], float, float]] = []
    min_vein_margin = 1.0e9

    for v_idx, (s_xyz, e_xyz, rad_cm, warn_cm) in enumerate(vein_tuples):
        cur_s = list(s_xyz)
        cur_e = list(e_xyz)

        for _iter in range(28):
            worst_s = query_signed_solid_distance_cm(
                verify_mesh, bvh, unreal.Vector(cur_s[0], cur_s[1], cur_s[2])
            ) - rad_cm
            worst_e = query_signed_solid_distance_cm(
                verify_mesh, bvh, unreal.Vector(cur_e[0], cur_e[1], cur_e[2])
            ) - rad_cm
            worst_seg = min(worst_s, worst_e)
            for step_i in range(1, 8):
                alpha = step_i / 8.0
                mid_pt = unreal.Vector(
                    cur_s[0] + alpha * (cur_e[0] - cur_s[0]),
                    cur_s[1] + alpha * (cur_e[1] - cur_s[1]),
                    cur_s[2] + alpha * (cur_e[2] - cur_s[2]),
                )
                d_mid = query_signed_solid_distance_cm(verify_mesh, bvh, mid_pt) - rad_cm
                worst_seg = min(worst_seg, d_mid)

            if worst_seg >= MIN_VEIN_SOLID_MARGIN_CM:
                break

            # Pull horizontal (X, Y) coordinates smoothly toward interior axis; nudge Z slightly if near cap
            if worst_s < TARGET_VEIN_TUCK_MARGIN_CM:
                cur_s[0] *= 0.90
                cur_s[1] *= 0.90
                if abs(cur_s[2]) > 0.38 * dim_z:
                    cur_s[2] *= 0.95
            if worst_e < TARGET_VEIN_TUCK_MARGIN_CM:
                cur_e[0] *= 0.90
                cur_e[1] *= 0.90
                if abs(cur_e[2]) > 0.38 * dim_z:
                    cur_e[2] *= 0.95

        # Final 9-point capsule verification
        vein_worst = 1.0e9
        for step_i in range(9):
            alpha = step_i / 8.0
            pt = unreal.Vector(
                cur_s[0] + alpha * (cur_e[0] - cur_s[0]),
                cur_s[1] + alpha * (cur_e[1] - cur_s[1]),
                cur_s[2] + alpha * (cur_e[2] - cur_s[2]),
            )
            margin = query_signed_solid_distance_cm(verify_mesh, bvh, pt) - rad_cm
            vein_worst = min(vein_worst, margin)

        if vein_worst < 1.0:
            raise AssertionError(
                f"{spec.canonical_name} vein[{v_idx}] failed solid containment check (min margin={vein_worst:.2f} cm)"
            )

        if (tuple(cur_s) != s_xyz) or (tuple(cur_e) != e_xyz):
            log_info(
                f"Tucked {spec.canonical_name} vein[{v_idx}] inside solid silhouette: "
                f"({cur_s[0]:.2f},{cur_s[1]:.2f},{cur_s[2]:.2f})->({cur_e[0]:.2f},{cur_e[1]:.2f},{cur_e[2]:.2f}) "
                f"[min_solid_margin={vein_worst:.2f} cm]"
            )
        min_vein_margin = min(min_vein_margin, vein_worst)
        calibrated_veins.append(((cur_s[0], cur_s[1], cur_s[2]), (cur_e[0], cur_e[1], cur_e[2]), rad_cm, warn_cm))

    summary = ImportedAssetSummary(
        canonical_name=spec.canonical_name,
        asset_path=static_mesh.get_path_name().split(".")[0],
        triangle_count=final_tris,
        fitted_dims_cm=(dim_x, dim_y, dim_z),
        yaw_deg=yaw_deg,
        is_monolith=True,
        is_closed=final_closed,
        num_islands=final_islands,
        volume_cm3=exact_vol_cm3,
        density_g_cm3=calibrated_density,
        calibrated_mass_kg=calibrated_mass_kg,
        grip_verified=grip_verified,
        min_vein_margin_cm=min_vein_margin,
    )

    log_info(
        f"Conditioned {spec.canonical_name}: closed={final_closed}, islands={final_islands}, "
        f"tris={final_tris}, dims=({dim_x:.1f},{dim_y:.1f},{dim_z:.1f}) cm, "
        f"vol={exact_vol_cm3:.1f} cm3, density={calibrated_density:.4f} g/cm3, "
        f"mass={calibrated_mass_kg:.2f} kg, com=({exact_com.x:.2f},{exact_com.y:.2f},{exact_com.z:.2f}) cm, "
        f"min_vein_margin={min_vein_margin:.2f} cm"
    )
    return final_dims, calibrated_density, calibrated_veins, summary


def make_vein_struct(
    start_xyz: Tuple[float, float, float],
    end_xyz: Tuple[float, float, float],
    radius_cm: float,
    warn_cm: float,
) -> unreal.VolatileVeinDefinition:
    vein = unreal.VolatileVeinDefinition()
    vein_text = (
        f"(LocalStartCm=(X={start_xyz[0]:.4f},Y={start_xyz[1]:.4f},Z={start_xyz[2]:.4f}),"
        f"LocalEndCm=(X={end_xyz[0]:.4f},Y={end_xyz[1]:.4f},Z={end_xyz[2]:.4f}),"
        f"RadiusCm={radius_cm:.4f},WarningRangeCm={warn_cm:.4f})"
    )
    vein.import_text(vein_text)
    return vein


def get_or_create_data_asset(asset_path: str, asset_cls) -> Tuple[unreal.Object, bool]:
    pkg_dir, asset_name = asset_path.rsplit("/", 1)
    if not unreal.EditorAssetLibrary.does_directory_exist(pkg_dir):
        unreal.EditorAssetLibrary.make_directory(pkg_dir)

    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        loaded = unreal.EditorAssetLibrary.load_asset(asset_path)
        if loaded is None:
            raise RuntimeError(f"Failed loading existing DataAsset {asset_path}")
        return loaded, False

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_cls)
    created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name, pkg_dir, asset_cls, factory
    )
    if created is None:
        raise RuntimeError(f"Failed creating DataAsset {asset_path}")
    return created, True


def ensure_shift_progression_and_contracts(da_pres: unreal.Object) -> Tuple[unreal.Object, List[unreal.Object]]:
    shape_map = {
        "BOX": unreal.MonolithBaseShape.BOX,
        "OCTAGONAL_PRISM": unreal.MonolithBaseShape.OCTAGONAL_PRISM,
        "ASYMMETRIC_L_BLOCK": unreal.MonolithBaseShape.ASYMMETRIC_L_BLOCK,
        "STEPPED_CORE": unreal.MonolithBaseShape.STEPPED_CORE,
    }

    contracts: List[unreal.Object] = []
    for entry in BASELINE_CONTRACT_SPECS:
        c_path = f"{CONTRACTS_DIR}/{entry['name']}"
        c_asset, _ = get_or_create_data_asset(c_path, unreal.MonolithContractDataAsset)
        c_asset.set_editor_property("contract_code", unreal.Text(entry["code"]))
        c_asset.set_editor_property("contract_directive_text", entry["directive"])
        c_asset.set_editor_property("base_shape", shape_map[entry["shape"]])
        c_asset.set_editor_property("custom_monolith_mesh", None)
        c_asset.set_editor_property(
            "dimensions_cm",
            unreal.Vector(entry["dims"][0], entry["dims"][1], entry["dims"][2]),
        )
        c_asset.set_editor_property("density_grams_per_cm3", entry["density"])
        c_asset.set_editor_property("target_tithe_mass_kg", entry["target_kg"])
        c_asset.set_editor_property("quota_tolerance_kg", entry["tol_kg"])
        c_asset.set_editor_property("max_allowed_tare_waste_kg", entry["max_tare_kg"])
        c_asset.set_editor_property("allow_diverter_toggle", entry["allow_diverter"])
        c_asset.set_editor_property("clamp_max_safe_torque_nm", entry["max_torque_nm"])
        c_asset.set_editor_property("clamp_fatigue_grace_seconds", entry["grace_s"])
        c_asset.set_editor_property("clamp_fatigue_cooldown_seconds", entry["cooldown_s"])
        c_asset.set_editor_property("clamp_spring_stiffness", entry["stiff"])
        c_asset.set_editor_property("clamp_spring_damping", entry["damp"])
        veins_list = [make_vein_struct(s, e, r, w) for (s, e, r, w) in entry["veins"]]
        c_asset.set_editor_property("hidden_veins", veins_list)
        contracts.append(c_asset)

    da_prog, _ = get_or_create_data_asset(DA_PROGRESSION_PATH, unreal.ShiftProgressionDataAsset)
    da_prog.set_editor_property("ordered_contracts", contracts)
    da_prog.set_editor_property("laser_capacitor_charge_seconds", 0.35)
    da_prog.set_editor_property("laser_cooldown_seconds", 0.25)
    da_prog.set_editor_property("min_sliver_volume_cm3", 20.0)
    da_prog.set_editor_property("min_chunk_axis_thickness_cm", 0.30)
    da_prog.set_editor_property("chunk_separation_speed_cm_per_sec", 15.0)
    da_prog.set_editor_property("max_in_flight_watchdog_seconds", 6.0)
    da_prog.set_editor_property("fault_auto_reset_delay_seconds", 1.8)

    da_pres.set_editor_property("shift_progression_override", da_prog)
    return da_prog, contracts


def verify_presentation_integrity_and_disk_budget(da_pres: unreal.Object) -> Tuple[str, float]:
    # Verify Phase 8 Audio bindings remain 100% intact
    loops_ms = da_pres.get_editor_property("continuous_loops_meta_sound")
    ambient_bed = da_pres.get_editor_property("ambient_cell_bed_sound")
    spatial_att = da_pres.get_editor_property("default_spatial_attenuation")
    spatial_impulses = da_pres.get_editor_property("spatial_impulse_sounds")
    if loops_ms is None or ambient_bed is None or spatial_att is None:
        raise AssertionError("Phase 8 audio bindings missing on DA_Presentation!")
    if spatial_impulses is None or len(spatial_impulses) != 7 or any(v is None for v in spatial_impulses.values()):
        raise AssertionError("Phase 8 spatial_impulse_sounds incomplete on DA_Presentation!")

    # Verify Phase 9.2 Master Material bindings remain 100% intact
    for mat_prop in (
        "laser_sheet_material",
        "default_monolith_outer_material",
        "default_molten_cut_cap_material",
        "cell_wall_material",
        "cell_floor_material",
    ):
        if da_pres.get_editor_property(mat_prop) is None:
            raise AssertionError(f"Phase 9.2 material '{mat_prop}' is None on DA_Presentation!")

    if da_pres.get_editor_property("cell_wall_panel_mesh_slot").get_editor_property("mesh") is not None:
        raise AssertionError("CellWallPanelMeshSlot.mesh must NEVER be populated!")

    grate_overrides = list(
        da_pres.get_editor_property("cell_floor_grate_mesh_slot").get_editor_property("material_overrides") or []
    )
    if not grate_overrides or grate_overrides[0] is None:
        raise AssertionError("CellFloorGrateMeshSlot.MaterialOverrides[0] must hold the Rodin PBR material!")

    prog = da_pres.get_editor_property("shift_progression_override")
    if prog is None or len(list(prog.get_editor_property("ordered_contracts") or [])) != 6:
        raise AssertionError("DA_Presentation.shift_progression_override does not bind 6 contracts!")

    content_root = unreal.Paths.project_content_dir()
    largest_file = ""
    largest_mb = 0.0
    total_mb = 0.0
    for root, _dirs, files in os.walk(content_root):
        for fname in files:
            if not fname.lower().endswith((".uasset", ".umap", ".uexp", ".ubulk")):
                continue
            fpath = os.path.join(root, fname)
            sz_mb = os.path.getsize(fpath) / (1024.0 * 1024.0)
            total_mb += sz_mb
            if sz_mb > largest_mb:
                largest_mb = sz_mb
                largest_file = os.path.relpath(fpath, content_root)
            if sz_mb > MAX_UASSET_SIZE_MB:
                raise AssertionError(
                    f"Asset '{fpath}' ({sz_mb:.2f} MB) exceeds {MAX_UASSET_SIZE_MB:.1f} MB GitHub non-LFS cap!"
                )

    log_info(
        f"Verified Phase 8 audio + Phase 9.2 materials + DA_ShiftProgression intact | "
        f"Content total={total_mb:.2f} MB, largest='{largest_file}' ({largest_mb:.2f} MB)"
    )
    return largest_file, largest_mb


def verify_unreal_api_bindings() -> None:
    _ = (
        unreal.AssetImportTask,
        unreal.DataAssetFactory,
        unreal.MonolithContractDataAsset,
        unreal.ShiftProgressionDataAsset,
        unreal.TareTithePresentationDataAsset,
        unreal.MeshPresentationSlot,
        unreal.StaticMaterial,
        unreal.MonolithBaseShape.BOX,
        unreal.MonolithBaseShape.OCTAGONAL_PRISM,
        unreal.MonolithBaseShape.ASYMMETRIC_L_BLOCK,
        unreal.MonolithBaseShape.STEPPED_CORE,
        unreal.MonolithBaseShape.CUSTOM_STATIC_MESH,
        unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2,
        unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh,
        unreal.GeometryScript_MeshRepair.weld_mesh_edges,
        unreal.GeometryScript_MeshRepair.remove_small_components,
        unreal.GeometryScript_MeshRepair.compact_mesh,
        unreal.GeometryScript_MeshVoxelProcessing.apply_mesh_solidify,
        unreal.GeometryScript_MeshQueries.get_is_closed_mesh,
        unreal.GeometryScript_MeshQueries.get_num_connected_components,
        unreal.GeometryScript_MeshQueries.get_mesh_bounding_box,
        unreal.GeometryScript_MeshQueries.get_mesh_volume_area_center,
        unreal.GeometryScript_MeshQueries.get_all_vertex_positions,
        unreal.GeometryScript_List.convert_vector_list_to_array,
        unreal.GeometryScript_MeshSimplification.apply_simplify_to_triangle_count,
        unreal.GeometryScript_MeshTransforms.translate_mesh,
        unreal.GeometryScript_MeshTransforms.rotate_mesh,
        unreal.GeometryScript_MeshTransforms.scale_mesh,
        unreal.GeometryScript_Normals.compute_split_normals,
        unreal.GeometryScript_Normals.flip_normals,
        unreal.GeometryScript_UVs.set_num_uv_sets,
        unreal.GeometryScript_UVs.set_mesh_u_vs_from_box_projection,
        unreal.GeometryScript_MeshSpatial.build_bvh_for_mesh,
        unreal.GeometryScript_MeshSpatial.is_point_inside_mesh,
        unreal.GeometryScript_MeshSpatial.find_nearest_point_on_mesh,
    )
    test_xf = unreal.Transform(
        location=unreal.Vector(0.0, 86.0, 0.0),
        rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0),
        scale=unreal.Vector(2.0, 3.0, 4.0),
    )
    tp = test_xf.transform_location(unreal.Vector(10.0, 5.0, 1.0))
    assert (
        abs(float(tp.x) - (-15.0)) < 1.0e-2
        and abs(float(tp.y) - 106.0) < 1.0e-2
        and abs(float(tp.z) - 4.0) < 1.0e-2
    ), f"Unexpected transform_location result: {tp}"


def main() -> None:
    verify_unreal_api_bindings()
    log_info(f"Scanning Rodin drop folder: {RAW_RODIN_DIR}")
    discovered = discover_source_files(RAW_RODIN_DIR)

    da_pres = unreal.EditorAssetLibrary.load_asset(DA_PRESENTATION_PATH)
    if da_pres is None:
        raise RuntimeError(f"Failed to load {DA_PRESENTATION_PATH}")

    wall_slot = da_pres.get_editor_property("cell_wall_panel_mesh_slot")
    if wall_slot.get_editor_property("mesh") is not None:
        wall_slot.set_editor_property("mesh", None)
        da_pres.set_editor_property("cell_wall_panel_mesh_slot", wall_slot)

    if not discovered:
        log_info("No canonical .glb/.fbx files found in C:\\MegaJam2026\\RawAssets\\Rodin.")
        return

    log_info(f"Found {len(discovered)} canonical source file(s): {sorted(discovered.keys())}")
    assets_to_save: List[unreal.Object] = []
    summaries: List[ImportedAssetSummary] = []

    def queue_save(obj: Optional[unreal.Object]) -> None:
        if obj is not None and obj not in assets_to_save:
            assets_to_save.append(obj)

    # 1) Import & fit the 9 Station Props
    for canonical_name in sorted(STATION_PROP_SPECS.keys()):
        if canonical_name not in discovered:
            continue

        src_path = discovered[canonical_name]
        spec = STATION_PROP_SPECS[canonical_name]
        tex_cap = MAX_TEXTURE_SIZE_SECONDARY if spec.is_secondary_prop else MAX_TEXTURE_SIZE_PRIMARY

        log_info(f"Importing station prop {canonical_name} from '{src_path}'...")
        static_mesh, imported_objs = import_mesh_bundle(
            src_path, canonical_name, is_sliceable_monolith=False, max_tex_size=tex_cap
        )
        for obj in imported_objs:
            queue_save(obj)

        vis_transform, fitted_dims, tri_count, yaw_deg = compute_station_visual_transform(static_mesh, spec)
        slot = da_pres.get_editor_property(spec.slot_property)
        slot.set_editor_property("mesh", static_mesh)
        slot.set_editor_property("visual_local_transform", vis_transform)

        # Trap B: Preserve Interchange PBR UMaterialInstance & explicitly populate MaterialOverrides[0] on CellFloorGrateMeshSlot
        imported_pbr_mat = static_mesh.get_material(0)
        if imported_pbr_mat is None or "WorldGridMaterial" in imported_pbr_mat.get_name():
            raise RuntimeError(f"{canonical_name} missing imported Rodin PBR material on Slot 0!")

        if canonical_name == "SM_CellFloorGrate":
            slot.set_editor_property("material_overrides", [imported_pbr_mat])
        else:
            slot.set_editor_property("material_overrides", [])

        da_pres.set_editor_property(spec.slot_property, slot)
        summaries.append(
            ImportedAssetSummary(
                canonical_name=canonical_name,
                asset_path=static_mesh.get_path_name().split(".")[0],
                triangle_count=tri_count,
                fitted_dims_cm=fitted_dims,
                yaw_deg=yaw_deg,
            )
        )

    # 2) Create/update DA_ShiftProgression + DA_Contract_01..06, and import/solidify/calibrate Custom Monoliths
    da_prog, contracts = ensure_shift_progression_and_contracts(da_pres)
    queue_save(da_prog)
    for c_asset in contracts:
        queue_save(c_asset)

    m_slag_outer = da_pres.get_editor_property("default_monolith_outer_material") or unreal.EditorAssetLibrary.load_asset(M_SLAG_OUTER_PATH)
    m_molten_cut_cap = da_pres.get_editor_property("default_molten_cut_cap_material") or unreal.EditorAssetLibrary.load_asset(M_MOLTEN_CUT_CAP_PATH)
    if m_slag_outer is None or m_molten_cut_cap is None:
        raise RuntimeError("Missing M_SlagOuter_Master or M_MoltenCutCap_Master for custom monolith slots!")

    for canonical_name in sorted(MONOLITH_MESH_SPECS.keys()):
        if canonical_name not in discovered:
            continue

        src_path = discovered[canonical_name]
        spec = MONOLITH_MESH_SPECS[canonical_name]
        log_info(f"Importing sliceable monolith {canonical_name} from '{src_path}'...")
        static_mesh, imported_objs = import_mesh_bundle(
            src_path, canonical_name, is_sliceable_monolith=True
        )
        base_veins = list(BASELINE_CONTRACT_SPECS[spec.contract_index]["veins"])
        final_dims, calibrated_density, calibrated_veins, mono_summary = condition_and_verify_custom_monolith(
            static_mesh=static_mesh,
            spec=spec,
            m_slag_outer=m_slag_outer,
            m_molten_cut_cap=m_molten_cut_cap,
            vein_tuples=base_veins,
        )
        for obj in imported_objs:
            queue_save(obj)

        target_contract = contracts[spec.contract_index]
        target_contract.set_editor_property("base_shape", unreal.MonolithBaseShape.CUSTOM_STATIC_MESH)
        target_contract.set_editor_property("custom_monolith_mesh", static_mesh)
        target_contract.set_editor_property("dimensions_cm", final_dims)
        target_contract.set_editor_property("density_grams_per_cm3", calibrated_density)
        target_contract.set_editor_property("outer_shell_material", m_slag_outer)
        target_contract.set_editor_property("molten_cut_cap_material", m_molten_cut_cap)
        target_contract.set_editor_property(
            "hidden_veins",
            [make_vein_struct(s, e, r, w) for (s, e, r, w) in calibrated_veins],
        )

        # Verify spawn overhang torque safety against ClampMaxSafeTorqueNm
        verify_dyn = copy_static_mesh_to_dynamic(static_mesh)
        _, _, com_cm = unreal.GeometryScript_MeshQueries.get_mesh_volume_area_center(verify_dyn)
        lever_m = math.hypot(float(com_cm.x), float(com_cm.y)) * 0.01
        spawn_torque_nm = mono_summary.calibrated_mass_kg * 9.81 * lever_m
        max_safe_nm = float(target_contract.get_editor_property("clamp_max_safe_torque_nm"))
        assert spawn_torque_nm < 0.70 * max_safe_nm, (
            f"{canonical_name} initial overhang torque {spawn_torque_nm:.2f} Nm approaches {max_safe_nm:.1f} Nm!"
        )

        queue_save(target_contract)
        summaries.append(mono_summary)

    queue_save(da_pres)

    saved_count = 0
    for asset in assets_to_save:
        if unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            saved_count += 1
        else:
            log_warn(f"save_loaded_asset returned False for {asset.get_path_name()}")

    largest_file, largest_mb = verify_presentation_integrity_and_disk_budget(da_pres)

    log_info("================ PHASE 9.3 VERIFICATION TABLE ================")
    for item in summaries:
        dx, dy, dz = item.fitted_dims_cm
        if item.is_monolith:
            log_info(
                f"SUMMARY | {item.canonical_name:24s} | tris={item.triangle_count:5d} | "
                f"dims=({dx:5.1f}, {dy:5.1f}, {dz:5.1f}) cm | yaw={item.yaw_deg:4.0f} | "
                f"closed={item.is_closed} (islands={item.num_islands}) | "
                f"vol={item.volume_cm3:8.1f} cm3 | density={item.density_g_cm3:.4f} g/cm3 | "
                f"mass={item.calibrated_mass_kg:.2f} kg | grip={item.grip_verified} | "
                f"min_vein_margin={item.min_vein_margin_cm:.2f} cm"
            )
        else:
            log_info(
                f"SUMMARY | {item.canonical_name:24s} | tris={item.triangle_count:5d} | "
                f"dims=({dx:5.1f}, {dy:5.1f}, {dz:5.1f}) cm | yaw={item.yaw_deg:4.0f} | StationProp PBR"
            )
    log_info(
        f"SUMMARY_DISK | saved={saved_count}/{len(assets_to_save)} | "
        f"largest_uasset='{largest_file}' ({largest_mb:.2f} MB)"
    )
    log_info("==============================================================")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        log_error(f"FATAL: {exc}")
        raise
