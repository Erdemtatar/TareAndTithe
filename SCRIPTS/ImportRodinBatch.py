import os
from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple

import unreal


RAW_RODIN_DIR = r"C:\MegaJam2026\RawAssets\Rodin"
DEST_MESHES_DIR = "/Game/Presentation/Meshes"
CONTRACTS_DIR = "/Game/Presentation/Contracts"
DA_PRESENTATION_PATH = "/Game/Presentation/DA_Presentation"
DA_PROGRESSION_PATH = "/Game/Presentation/DA_ShiftProgression"

MAX_TEXTURE_SIZE = 2048
MAX_MONOLITH_TRIANGLES = 3200  # Safely under C++ MaxProceduralTriangleBudget = 3500


@dataclass(frozen=True)
class StationPropSpec:
    canonical_name: str
    slot_property: str
    envelope_cm: Tuple[float, float, float]
    target_xy_cm: Tuple[float, float]
    z_align_mode: str  # "min", "center", or "max"
    target_z_cm: float


@dataclass(frozen=True)
class MonolithMeshSpec:
    canonical_name: str
    contract_index: int  # 0-based index in OrderedContracts (3 -> Contract04, 5 -> Contract06)
    envelope_cm: Tuple[float, float, float]


# 9 Canonical Station Props (NEVER populate cell_wall_panel_mesh_slot per VISUAL_PRESENTATION_CONTRACT.md)
STATION_PROP_SPECS: Dict[str, StationPropSpec] = {
    "SM_ClampJaw": StationPropSpec(
        canonical_name="SM_ClampJaw",
        slot_property="clamp_jaw_mesh_slot",
        envelope_cm=(30.0, 30.0, 16.0),
        target_xy_cm=(0.0, 0.0),
        z_align_mode="center",
        target_z_cm=-2.0,
    ),
    "SM_ClampSpringHousing": StationPropSpec(
        canonical_name="SM_ClampSpringHousing",
        slot_property="clamp_spring_housing_mesh_slot",
        envelope_cm=(48.0, 48.0, 40.0),
        target_xy_cm=(0.0, 0.0),
        z_align_mode="min",
        target_z_cm=0.0,
    ),
    "SM_ScaleBase": StationPropSpec(
        canonical_name="SM_ScaleBase",
        slot_property="scale_base_mesh_slot",
        envelope_cm=(82.0, 68.0, 12.0),
        target_xy_cm=(0.0, 0.0),
        z_align_mode="min",
        target_z_cm=0.0,
    ),
    "SM_ScalePan": StationPropSpec(
        canonical_name="SM_ScalePan",
        slot_property="scale_pan_mesh_slot",
        envelope_cm=(76.0, 62.0, 22.0),
        target_xy_cm=(0.0, 0.0),
        z_align_mode="min",
        target_z_cm=-3.0,
    ),
    "SM_DiverterFlapper": StationPropSpec(
        canonical_name="SM_DiverterFlapper",
        slot_property="diverter_flapper_mesh_slot",
        envelope_cm=(84.0, 3.0, 91.0),
        target_xy_cm=(0.0, 0.0),
        z_align_mode="min",
        target_z_cm=0.0,
    ),
    "SM_TareChute": StationPropSpec(
        canonical_name="SM_TareChute",
        slot_property="tare_chute_mesh_slot",
        envelope_cm=(86.0, 84.0, 24.0),
        target_xy_cm=(0.0, 86.0),  # Mandatory +86 cm Y offset on ScaleRoot per VISUAL_PRESENTATION_CONTRACT.md
        z_align_mode="min",
        target_z_cm=0.0,
    ),
    "SM_DirectorBoothFrame": StationPropSpec(
        canonical_name="SM_DirectorBoothFrame",
        slot_property="director_booth_frame_mesh_slot",
        envelope_cm=(24.0, 190.0, 85.0),
        target_xy_cm=(0.0, 0.0),
        z_align_mode="center",
        target_z_cm=0.0,
    ),
    "SM_DirectorShutter": StationPropSpec(
        canonical_name="SM_DirectorShutter",
        slot_property="director_shutter_mesh_slot",
        envelope_cm=(6.0, 165.0, 62.0),
        target_xy_cm=(0.0, 0.0),
        z_align_mode="center",
        target_z_cm=0.0,
    ),
    "SM_CellFloorGrate": StationPropSpec(
        canonical_name="SM_CellFloorGrate",
        slot_property="cell_floor_grate_mesh_slot",
        envelope_cm=(770.0, 760.0, 20.0),
        target_xy_cm=(0.0, 0.0),
        z_align_mode="max",
        target_z_cm=10.0,  # Flush with PitFloorCatchCollider top at Local Z = +10.0 cm
    ),
}

MONOLITH_MESH_SPECS: Dict[str, MonolithMeshSpec] = {
    "SM_Monolith_Contract04": MonolithMeshSpec(
        canonical_name="SM_Monolith_Contract04",
        contract_index=3,
        envelope_cm=(30.0, 28.0, 88.0),
    ),
    "SM_Monolith_Contract06": MonolithMeshSpec(
        canonical_name="SM_Monolith_Contract06",
        contract_index=5,
        envelope_cm=(24.0, 34.0, 84.0),
    ),
}

SUPPORTED_EXTENSIONS = (".glb", ".fbx")


def log_info(msg: str) -> None:
    unreal.log(f"[ImportRodinBatch] {msg}")


def log_warn(msg: str) -> None:
    unreal.log_warning(f"[ImportRodinBatch] {msg}")


def log_error(msg: str) -> None:
    unreal.log_error(f"[ImportRodinBatch] {msg}")


def discover_source_files(raw_dir: str) -> Dict[str, str]:
    os.makedirs(raw_dir, exist_ok=True)
    all_canonical = set(STATION_PROP_SPECS.keys()) | set(MONOLITH_MESH_SPECS.keys())
    discovered: Dict[str, str] = {}

    entries = sorted(os.listdir(raw_dir))

    # 1) Direct files in C:\MegaJam2026\RawAssets\Rodin\<CanonicalName>.(glb|fbx) (.glb prioritized before .fbx)
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

    # 2) Optional per-asset subfolder C:\MegaJam2026\RawAssets\Rodin\<CanonicalName>\*.(glb|fbx)
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


def clamp_texture_2048(tex: unreal.Texture2D) -> bool:
    modified = False
    for prop_name in ("max_texture_size", "maximum_texture_size"):
        try:
            current_max = int(tex.get_editor_property(prop_name))
            if current_max == 0 or current_max > MAX_TEXTURE_SIZE:
                tex.set_editor_property(prop_name, MAX_TEXTURE_SIZE)
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

    return pipeline


def import_mesh_bundle(
    src_file: str,
    canonical_name: str,
    is_sliceable_monolith: bool,
) -> Tuple[unreal.StaticMesh, List[unreal.Object]]:
    if not unreal.EditorAssetLibrary.does_directory_exist(DEST_MESHES_DIR):
        unreal.EditorAssetLibrary.make_directory(DEST_MESHES_DIR)

    target_asset_path = f"{DEST_MESHES_DIR}/{canonical_name}"

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", src_file)
    task.set_editor_property("destination_path", DEST_MESHES_DIR)
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
        # Interchange accepts pipeline overrides via options or override_pipelines depending on engine path
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

    static_meshes = [obj for obj in imported_objects if isinstance(obj, unreal.StaticMesh)]
    if not static_meshes and unreal.EditorAssetLibrary.does_asset_exist(target_asset_path):
        existing = unreal.EditorAssetLibrary.load_asset(target_asset_path)
        if isinstance(existing, unreal.StaticMesh):
            static_meshes.append(existing)

    if not static_meshes:
        raise RuntimeError(f"Import of '{src_file}' produced zero UStaticMesh assets.")

    # Prefer exact canonical match; otherwise rename primary imported StaticMesh to canonical_name
    primary_mesh: Optional[unreal.StaticMesh] = None
    for sm in static_meshes:
        if sm.get_name() == canonical_name:
            primary_mesh = sm
            break

    if primary_mesh is None:
        primary_mesh = static_meshes[0]
        src_pkg_path = primary_mesh.get_path_name().split(".")[0]
        if src_pkg_path != target_asset_path:
            if unreal.EditorAssetLibrary.does_asset_exist(target_asset_path):
                unreal.EditorAssetLibrary.delete_asset(target_asset_path)
            if not unreal.EditorAssetLibrary.rename_asset(src_pkg_path, target_asset_path):
                raise RuntimeError(f"Failed renaming imported mesh {src_pkg_path} -> {target_asset_path}")
            reloaded = unreal.EditorAssetLibrary.load_asset(target_asset_path)
            if not isinstance(reloaded, unreal.StaticMesh):
                raise RuntimeError(f"Failed reloading renamed mesh at {target_asset_path}")
            primary_mesh = reloaded

    if primary_mesh not in imported_objects:
        imported_objects.append(primary_mesh)

    # Clamp all newly imported Texture2D assets to 2048 max resolution (6 GB VRAM & <100 MB Git budget)
    clamped_count = 0
    for obj in imported_objects:
        if isinstance(obj, unreal.Texture2D):
            if clamp_texture_2048(obj):
                clamped_count += 1

    if clamped_count > 0:
        log_info(f"Clamped {clamped_count} Texture2D asset(s) to {MAX_TEXTURE_SIZE}px for {canonical_name}")

    return primary_mesh, imported_objects


def compute_envelope_scale(
    size_xyz: Tuple[float, float, float],
    envelope_xyz: Tuple[float, float, float],
) -> Tuple[float, float, float]:
    sx = envelope_xyz[0] / max(size_xyz[0], 1.0e-3)
    sy = envelope_xyz[1] / max(size_xyz[1], 1.0e-3)
    sz = envelope_xyz[2] / max(size_xyz[2], 1.0e-3)

    # Median ratio preserves primary 2D silhouette aspect ratio while per-axis min() strictly enforces mechanical clearances
    s_base = sorted((sx, sy, sz))[1]
    return (min(s_base, sx), min(s_base, sy), min(s_base, sz))


def compute_station_visual_transform(
    static_mesh: unreal.StaticMesh,
    spec: StationPropSpec,
) -> unreal.Transform:
    bounds = static_mesh.get_bounds()
    origin = bounds.origin
    extent = bounds.box_extent

    size_xyz = (max(2.0 * float(extent.x), 1.0e-3), max(2.0 * float(extent.y), 1.0e-3), max(2.0 * float(extent.z), 1.0e-3))
    scale_x, scale_y, scale_z = compute_envelope_scale(size_xyz, spec.envelope_cm)

    scaled_cx = float(origin.x) * scale_x
    scaled_cy = float(origin.y) * scale_y
    scaled_cz = float(origin.z) * scale_z
    scaled_hz = float(extent.z) * scale_z

    tx = spec.target_xy_cm[0] - scaled_cx
    ty = spec.target_xy_cm[1] - scaled_cy

    if spec.z_align_mode == "min":
        tz = spec.target_z_cm - (scaled_cz - scaled_hz)
    elif spec.z_align_mode == "max":
        tz = spec.target_z_cm - (scaled_cz + scaled_hz)
    else:
        tz = spec.target_z_cm - scaled_cz

    return unreal.Transform(
        location=unreal.Vector(tx, ty, tz),
        rotation=unreal.Rotator(0.0, 0.0, 0.0),
        scale=unreal.Vector(scale_x, scale_y, scale_z),
    )


def condition_sliceable_monolith_mesh(
    static_mesh: unreal.StaticMesh,
    spec: MonolithMeshSpec,
) -> unreal.Vector:
    # Normalize local bounds to (0, 0, 0) center, fit inside contract envelope, repair cracks, and cap triangle count <= 3200
    dyn_mesh = unreal.DynamicMesh()
    copy_from_opts = unreal.GeometryScriptCopyMeshFromAssetOptions()
    copy_from_opts.set_editor_property("apply_build_settings", True)
    copy_from_opts.set_editor_property("request_tangents", False)
    copy_from_opts.set_editor_property("ignore_remove_degenerates", False)

    read_lod = unreal.GeometryScriptMeshReadLOD()
    read_lod.set_editor_property("lod_type", unreal.GeometryScriptLODType.MAX_AVAILABLE)
    read_lod.set_editor_property("lod_index", 0)

    dyn_mesh, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(
        static_mesh, dyn_mesh, copy_from_opts, read_lod, True
    )
    if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError(f"GeometryScript failed reading LOD0 from {static_mesh.get_name()}")

    weld_opts = unreal.GeometryScriptWeldEdgesOptions()
    weld_opts.set_editor_property("tolerance", 0.001)
    dyn_mesh = unreal.GeometryScript_MeshRepair.weld_mesh_edges(dyn_mesh, weld_opts)

    if not unreal.GeometryScript_MeshQueries.get_is_closed_mesh(dyn_mesh):
        fill_opts = unreal.GeometryScriptFillHolesOptions()
        dyn_mesh, filled_holes, failed_holes = unreal.GeometryScript_MeshRepair.fill_all_mesh_holes(
            dyn_mesh, fill_opts
        )
        log_info(
            f"Repaired open boundaries on {spec.canonical_name}: filled={filled_holes}, failed={failed_holes}"
        )

    tri_count = dyn_mesh.get_triangle_count()
    if tri_count > MAX_MONOLITH_TRIANGLES:
        simp_opts = unreal.GeometryScriptSimplifyMeshOptions()
        dyn_mesh = unreal.GeometryScript_MeshSimplification.apply_simplify_to_triangle_count(
            dyn_mesh, MAX_MONOLITH_TRIANGLES, simp_opts
        )
        log_info(
            f"Simplified {spec.canonical_name} from {tri_count} -> {dyn_mesh.get_triangle_count()} triangles"
        )

    bbox = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(dyn_mesh)
    bmin = bbox.min
    bmax = bbox.max
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

    # Uniform fit inside contract envelope so XY overhangs & HiddenVeins remain proportional and centered at (0,0,0)
    uniform_scale = min(
        spec.envelope_cm[0] / size_xyz[0],
        spec.envelope_cm[1] / size_xyz[1],
        spec.envelope_cm[2] / size_xyz[2],
    )

    dyn_mesh = unreal.GeometryScript_MeshTransforms.translate_mesh(
        dyn_mesh, unreal.Vector(-center.x, -center.y, -center.z)
    )
    dyn_mesh = unreal.GeometryScript_MeshTransforms.scale_mesh(
        dyn_mesh,
        unreal.Vector(uniform_scale, uniform_scale, uniform_scale),
        unreal.Vector(0.0, 0.0, 0.0),
    )

    copy_to_opts = unreal.GeometryScriptCopyMeshToAssetOptions()
    copy_to_opts.set_editor_property("enable_recompute_normals", True)
    copy_to_opts.set_editor_property("enable_recompute_tangents", True)
    copy_to_opts.set_editor_property("enable_remove_degenerates", True)

    write_lod = unreal.GeometryScriptMeshWriteLOD()
    write_lod.set_editor_property("write_hi_res_source", False)
    write_lod.set_editor_property("lod_index", 0)

    dyn_mesh, write_outcome = unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(
        dyn_mesh, static_mesh, copy_to_opts, write_lod, True
    )
    if write_outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError(f"GeometryScript failed writing normalized LOD0 to {static_mesh.get_name()}")

    # Enforce bAllowCPUAccess = True for packaged build GeometryScript slicing
    static_mesh.set_editor_property("allow_cpu_access", True)

    final_bounds = static_mesh.get_bounds()
    final_dims = unreal.Vector(
        max(5.0, 2.0 * float(final_bounds.box_extent.x)),
        max(5.0, 2.0 * float(final_bounds.box_extent.y)),
        max(5.0, 2.0 * float(final_bounds.box_extent.z)),
    )
    is_closed = unreal.GeometryScript_MeshQueries.get_is_closed_mesh(dyn_mesh)
    log_info(
        f"Conditioned sliceable monolith {spec.canonical_name}: closed={is_closed}, "
        f"tris={dyn_mesh.get_triangle_count()}, dims=({final_dims.x:.1f}, {final_dims.y:.1f}, {final_dims.z:.1f}) cm, "
        f"allow_cpu_access={static_mesh.get_editor_property('allow_cpu_access')}"
    )
    return final_dims


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
    # Exact parity with UInspectionShiftSubsystem::BuildFallbackGreyboxProgression() in InspectionShiftSubsystem.cpp
    baseline_specs = [
        {
            "name": "DA_Contract_01",
            "code": "ACT I-A // CTR-01: STANDARD SLAG BILLET",
            "shape": unreal.MonolithBaseShape.BOX,
            "dims": (24.0, 24.0, 75.0),
            "density": 0.80,
            "target_kg": 12.0,
            "tol_kg": 1.25,
            "max_tare_kg": 14.0,
            "allow_diverter": False,
            "max_torque_nm": 65.0,
            "grace_s": 4.5,
            "cooldown_s": 3.0,
            "stiff": 180.0,
            "damp": 24.0,
            "veins": [],
        },
        {
            "name": "DA_Contract_02",
            "code": "ACT I-B // CTR-02: ECCENTRIC L-CASTING",
            "shape": unreal.MonolithBaseShape.ASYMMETRIC_L_BLOCK,
            "dims": (24.0, 32.0, 80.0),
            "density": 0.85,
            "target_kg": 14.5,
            "tol_kg": 1.35,
            "max_tare_kg": 16.0,
            "allow_diverter": True,
            "max_torque_nm": 28.5,
            "grace_s": 4.5,
            "cooldown_s": 3.0,
            "stiff": 180.0,
            "damp": 24.0,
            "veins": [],
        },
        {
            "name": "DA_Contract_03",
            "code": "ACT II-A // CTR-03: SINGLE-VEIN OCTAGON CORE",
            "shape": unreal.MonolithBaseShape.OCTAGONAL_PRISM,
            "dims": (30.0, 30.0, 84.0),
            "density": 0.95,
            "target_kg": 15.0,
            "tol_kg": 1.00,
            "max_tare_kg": 18.0,
            "allow_diverter": True,
            "max_torque_nm": 60.0,
            "grace_s": 4.5,
            "cooldown_s": 3.0,
            "stiff": 180.0,
            "damp": 24.0,
            "veins": [((-4.0, -7.0, -14.0), (4.0, 7.0, -6.0), 2.8, 18.0)],
        },
        {
            "name": "DA_Contract_04",
            "code": "ACT II-B // CTR-04: CROSSED ISOTOPE LATTICE",
            "shape": unreal.MonolithBaseShape.BOX,
            "dims": (30.0, 28.0, 88.0),
            "density": 0.90,
            "target_kg": 18.0,
            "tol_kg": 0.95,
            "max_tare_kg": 22.0,
            "allow_diverter": True,
            "max_torque_nm": 65.0,
            "grace_s": 4.5,
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
            "shape": unreal.MonolithBaseShape.ASYMMETRIC_L_BLOCK,
            "dims": (22.0, 28.0, 76.0),
            "density": 1.95,
            "target_kg": 21.0,
            "tol_kg": 0.90,
            "max_tare_kg": 28.0,
            "allow_diverter": True,
            "max_torque_nm": 32.0,
            "grace_s": 4.5,
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
            "shape": unreal.MonolithBaseShape.STEPPED_CORE,
            "dims": (24.0, 34.0, 84.0),
            "density": 1.75,
            "target_kg": 24.0,
            "tol_kg": 0.85,
            "max_tare_kg": 30.0,
            "allow_diverter": True,
            "max_torque_nm": 38.0,
            "grace_s": 4.2,
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

    contracts: List[unreal.Object] = []
    for entry in baseline_specs:
        c_path = f"{CONTRACTS_DIR}/{entry['name']}"
        c_asset, was_created = get_or_create_data_asset(c_path, unreal.MonolithContractDataAsset)
        if was_created:
            c_asset.set_editor_property("contract_code", unreal.Text(entry["code"]))
            c_asset.set_editor_property("base_shape", entry["shape"])
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

    da_prog, prog_created = get_or_create_data_asset(
        DA_PROGRESSION_PATH, unreal.ShiftProgressionDataAsset
    )
    existing_ordered = list(da_prog.get_editor_property("ordered_contracts") or [])
    if prog_created or len(existing_ordered) != 6:
        da_prog.set_editor_property("ordered_contracts", contracts)
        da_prog.set_editor_property("laser_capacitor_charge_seconds", 0.35)
        da_prog.set_editor_property("laser_cooldown_seconds", 0.25)
        da_prog.set_editor_property("min_sliver_volume_cm3", 20.0)
        da_prog.set_editor_property("min_chunk_axis_thickness_cm", 0.30)
        da_prog.set_editor_property("chunk_separation_speed_cm_per_sec", 15.0)
        da_prog.set_editor_property("max_in_flight_watchdog_seconds", 6.0)
        da_prog.set_editor_property("fault_auto_reset_delay_seconds", 1.8)
    else:
        contracts = existing_ordered

    da_pres.set_editor_property("shift_progression_override", da_prog)
    return da_prog, contracts


def verify_unreal_api_bindings() -> None:
    # Validate all UE 5.8 Python classes, enums, structs, and GeometryScript entrypoints up front
    _ = (
        unreal.AssetImportTask,
        unreal.DataAssetFactory,
        unreal.MonolithContractDataAsset,
        unreal.ShiftProgressionDataAsset,
        unreal.TareTithePresentationDataAsset,
        unreal.MeshPresentationSlot,
        unreal.MonolithBaseShape.BOX,
        unreal.MonolithBaseShape.OCTAGONAL_PRISM,
        unreal.MonolithBaseShape.ASYMMETRIC_L_BLOCK,
        unreal.MonolithBaseShape.STEPPED_CORE,
        unreal.MonolithBaseShape.CUSTOM_STATIC_MESH,
        unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2,
        unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh,
        unreal.GeometryScript_MeshRepair.weld_mesh_edges,
        unreal.GeometryScript_MeshRepair.fill_all_mesh_holes,
        unreal.GeometryScript_MeshQueries.get_is_closed_mesh,
        unreal.GeometryScript_MeshQueries.get_mesh_bounding_box,
        unreal.GeometryScript_MeshSimplification.apply_simplify_to_triangle_count,
        unreal.GeometryScript_MeshTransforms.translate_mesh,
        unreal.GeometryScript_MeshTransforms.scale_mesh,
    )
    test_xform = unreal.Transform(
        location=unreal.Vector(0.0, 86.0, 0.0),
        rotation=unreal.Rotator(0.0, 0.0, 0.0),
        scale=unreal.Vector(1.0, 1.0, 1.0),
    )
    assert abs(float(test_xform.translation.y) - 86.0) < 1.0e-3
    assert abs(float(test_xform.scale3d.x) - 1.0) < 1.0e-3
    test_vein = make_vein_struct((-6.0, -9.0, 6.0), (6.0, -2.0, 20.0), 2.5, 18.0)
    assert test_vein is not None
    _ = build_interchange_pipeline("SM_ScaleBase", is_sliceable_monolith=False)

    copy_from_opts = unreal.GeometryScriptCopyMeshFromAssetOptions()
    copy_from_opts.set_editor_property("apply_build_settings", True)
    copy_from_opts.set_editor_property("request_tangents", False)
    copy_from_opts.set_editor_property("ignore_remove_degenerates", False)

    read_lod = unreal.GeometryScriptMeshReadLOD()
    read_lod.set_editor_property("lod_type", unreal.GeometryScriptLODType.MAX_AVAILABLE)
    read_lod.set_editor_property("lod_index", 0)

    weld_opts = unreal.GeometryScriptWeldEdgesOptions()
    weld_opts.set_editor_property("tolerance", 0.001)
    _ = unreal.GeometryScriptFillHolesOptions()
    _ = unreal.GeometryScriptSimplifyMeshOptions()

    copy_to_opts = unreal.GeometryScriptCopyMeshToAssetOptions()
    copy_to_opts.set_editor_property("enable_recompute_normals", True)
    copy_to_opts.set_editor_property("enable_recompute_tangents", True)
    copy_to_opts.set_editor_property("enable_remove_degenerates", True)

    write_lod = unreal.GeometryScriptMeshWriteLOD()
    write_lod.set_editor_property("write_hi_res_source", False)
    write_lod.set_editor_property("lod_index", 0)


def main() -> None:
    verify_unreal_api_bindings()
    log_info(f"Scanning Rodin drop folder: {RAW_RODIN_DIR}")
    discovered = discover_source_files(RAW_RODIN_DIR)

    da_pres = unreal.EditorAssetLibrary.load_asset(DA_PRESENTATION_PATH)
    if da_pres is None:
        raise RuntimeError(f"Failed to load {DA_PRESENTATION_PATH}")

    # Contract invariant check: CellWallPanelMeshSlot.mesh MUST stay None
    wall_slot = da_pres.get_editor_property("cell_wall_panel_mesh_slot")
    if wall_slot.get_editor_property("mesh") is not None:
        log_warn("Clearing disallowed mesh on CellWallPanelMeshSlot per VISUAL_PRESENTATION_CONTRACT.md")
        wall_slot.set_editor_property("mesh", None)
        da_pres.set_editor_property("cell_wall_panel_mesh_slot", wall_slot)
        unreal.EditorAssetLibrary.save_loaded_asset(da_pres, only_if_is_dirty=False)

    if not discovered:
        log_info(
            "No canonical .glb/.fbx files found in C:\\MegaJam2026\\RawAssets\\Rodin yet. "
            "Drop SM_ScaleBase.glb, SM_Monolith_Contract04.glb, etc. and re-run this script anytime."
        )
        return

    log_info(f"Found {len(discovered)} canonical source file(s): {sorted(discovered.keys())}")
    assets_to_save: List[unreal.Object] = []

    def queue_save(obj: Optional[unreal.Object]) -> None:
        if obj is not None and obj not in assets_to_save:
            assets_to_save.append(obj)

    # 1) Import & bind Station Props
    for canonical_name, src_path in sorted(discovered.items()):
        if canonical_name not in STATION_PROP_SPECS:
            continue

        spec = STATION_PROP_SPECS[canonical_name]
        log_info(f"Importing station prop {canonical_name} from '{src_path}'...")
        static_mesh, imported_objs = import_mesh_bundle(
            src_path, canonical_name, is_sliceable_monolith=False
        )
        for obj in imported_objs:
            queue_save(obj)

        vis_transform = compute_station_visual_transform(static_mesh, spec)
        slot = da_pres.get_editor_property(spec.slot_property)
        slot.set_editor_property("mesh", static_mesh)
        slot.set_editor_property("visual_local_transform", vis_transform)

        # Trap #4 guard for CellFloorGrate: prevent default WorldGridMaterial on Slot 0 from bypassing CellFloorMaterial
        if canonical_name == "SM_CellFloorGrate":
            slot0_mat = static_mesh.get_material(0)
            cell_floor_mat = da_pres.get_editor_property("cell_floor_material")
            if cell_floor_mat is not None and (
                slot0_mat is None or "WorldGridMaterial" in slot0_mat.get_name()
            ):
                slot.set_editor_property("material_overrides", [cell_floor_mat])

        da_pres.set_editor_property(spec.slot_property, slot)
        loc = vis_transform.translation
        scl = vis_transform.scale3d
        log_info(
            f"Bound {canonical_name} -> DA_Presentation.{spec.slot_property} | "
            f"Scale=({scl.x:.3f}, {scl.y:.3f}, {scl.z:.3f}), "
            f"Translation=({loc.x:.2f}, {loc.y:.2f}, {loc.z:.2f})"
        )

    # 2) Import, repair/normalize, set allow_cpu_access=True, and bind Custom Sliceable Monoliths
    monolith_hits = [k for k in sorted(discovered.keys()) if k in MONOLITH_MESH_SPECS]
    if monolith_hits:
        da_prog, contracts = ensure_shift_progression_and_contracts(da_pres)
        queue_save(da_prog)
        for c_asset in contracts:
            queue_save(c_asset)

        for canonical_name in monolith_hits:
            src_path = discovered[canonical_name]
            spec = MONOLITH_MESH_SPECS[canonical_name]
            log_info(f"Importing sliceable monolith {canonical_name} from '{src_path}'...")
            static_mesh, imported_objs = import_mesh_bundle(
                src_path, canonical_name, is_sliceable_monolith=True
            )
            final_dims = condition_sliceable_monolith_mesh(static_mesh, spec)
            for obj in imported_objs:
                queue_save(obj)

            target_contract = contracts[spec.contract_index]
            target_contract.set_editor_property(
                "base_shape", unreal.MonolithBaseShape.CUSTOM_STATIC_MESH
            )
            target_contract.set_editor_property("custom_monolith_mesh", static_mesh)
            target_contract.set_editor_property("dimensions_cm", final_dims)
            queue_save(target_contract)
            log_info(
                f"Bound {canonical_name} -> {target_contract.get_name()} "
                f"(BaseShape=CustomStaticMesh, allow_cpu_access=True)"
            )

    # Final invariant enforcement: CellWallPanelMeshSlot MUST remain empty
    if da_pres.get_editor_property("cell_wall_panel_mesh_slot").get_editor_property("mesh") is not None:
        raise AssertionError("CellWallPanelMeshSlot.mesh must NEVER be populated!")

    queue_save(da_pres)

    saved_count = 0
    for asset in assets_to_save:
        if unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            saved_count += 1
        else:
            log_warn(f"save_loaded_asset returned False for {asset.get_path_name()}")

    log_info(f"ImportRodinBatch finished cleanly ({saved_count}/{len(assets_to_save)} asset(s) saved).")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        log_error(f"FATAL: {exc}")
        raise
