"""
Phase 12-FIX1 Surgical Contract Asset Updater & BVH/Torque Verifier.

Updates and saves ONLY /Game/Presentation/Contracts/DA_Contract_02..06:
  - DA_Contract_01: UNTOUCHED (0 bytes modified; verified read-only as symmetric Box tutorial).
  - DA_Contract_02: CantileveredJHook procedural casting (0 veins, ~52.1% initial clamp strain).
  - DA_Contract_03: OffsetCChannel procedural casting (1 hidden vein in lower flange, ~53.1% initial clamp strain).
  - DA_Contract_04: Preserves SM_Monolith_Contract04 + veins; sets ClampGripHorizontalOffsetCm = (0.0, +1.85) (~50.9% strain).
  - DA_Contract_05: CounterweightedZBracket bismuth casting (2 hidden veins, ~56.3% initial clamp strain).
  - DA_Contract_06: Preserves SM_Monolith_Contract06 + Veins 0..1; sets ClampGripHorizontalOffsetCm = (0.0, -2.50) (~60.0% strain)
                    and lowers Vein 2 to Z in [+13.0, +20.5] cm (R = 2.20 cm, > +5.0 cm clamp jaw clearance).

Does NOT touch L_MainGameplay.umap, DA_Presentation.uasset, DA_ShiftProgression.uasset, or Content/Presentation/Meshes/*.
"""

import math
import sys
from typing import Dict, List, Sequence, Tuple

import unreal


CONTRACTS_DIR = "/Game/Presentation/Contracts"
GRAVITY_M_PER_S2 = 9.81
AUTO_FIT_JAW_VERTICAL_DEPTH_RATIO = 0.175
TOP_GRIP_SAMPLE_DEPTH_CM = 9.0


def log_info(msg: str) -> None:
    unreal.log(f"[UpdateContractsPhase12Fix1] {msg}")


def log_error(msg: str) -> None:
    unreal.log_error(f"[UpdateContractsPhase12Fix1] {msg}")


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


def get_profile_yz_for_shape(
    shape: unreal.MonolithBaseShape,
    dims_xyz: Tuple[float, float, float],
) -> List[Tuple[float, float]]:
    _, sy, sz = dims_xyz
    if shape == unreal.MonolithBaseShape.CANTILEVERED_J_HOOK:
        return [
            (-0.32 * sy, -0.50 * sz),
            (+0.30 * sy, -0.50 * sz),
            (+0.50 * sy, -0.36 * sz),
            (+0.50 * sy, -0.04 * sz),
            (+0.14 * sy, +0.16 * sz),
            (+0.14 * sy, +0.50 * sz),
            (-0.50 * sy, +0.50 * sz),
            (-0.50 * sy, -0.38 * sz),
        ]
    if shape == unreal.MonolithBaseShape.OFFSET_C_CHANNEL:
        return [
            (-0.50 * sy, -0.50 * sz),
            (+0.36 * sy, -0.50 * sz),
            (+0.50 * sy, -0.40 * sz),
            (+0.50 * sy, +0.50 * sz),
            (-0.17 * sy, +0.50 * sz),
            (-0.17 * sy, +0.30 * sz),
            (-0.48 * sy, +0.20 * sz),
            (-0.48 * sy, +0.05 * sz),
            (+0.04 * sy, -0.04 * sz),
            (+0.04 * sy, -0.19 * sz),
            (-0.50 * sy, -0.25 * sz),
        ]
    if shape == unreal.MonolithBaseShape.COUNTERWEIGHTED_Z_BRACKET:
        return [
            (-0.50 * sy, -0.50 * sz),
            (+0.20 * sy, -0.50 * sz),
            (+0.20 * sy, -0.08 * sz),
            (+0.50 * sy, +0.04 * sz),
            (+0.50 * sy, +0.26 * sz),
            (+0.135 * sy, +0.34 * sz),
            (+0.135 * sy, +0.50 * sz),
            (-0.385 * sy, +0.50 * sz),
            (-0.385 * sy, +0.34 * sz),
            (-0.24 * sy, +0.26 * sz),
            (-0.24 * sy, +0.04 * sz),
            (-0.50 * sy, -0.08 * sz),
        ]
    raise ValueError(f"Unsupported extruded profile shape: {shape}")


def compute_polygon_area_and_centroid(pts: Sequence[Tuple[float, float]]) -> Tuple[float, float, float]:
    n = len(pts)
    area2 = 0.0
    cy6 = 0.0
    cz6 = 0.0
    for i in range(n):
        y0, z0 = pts[i]
        y1, z1 = pts[(i + 1) % n]
        cross = y0 * z1 - y1 * z0
        area2 += cross
        cy6 += (y0 + y1) * cross
        cz6 += (z0 + z1) * cross
    area = 0.5 * area2
    if abs(area) <= 1.0e-8:
        raise AssertionError("Degenerate zero-area polygon profile")
    return area, cy6 / (3.0 * area2), cz6 / (3.0 * area2)


def build_transient_contract_dynamic_mesh(contract: unreal.MonolithContractDataAsset) -> unreal.DynamicMesh:
    dyn_mesh = unreal.DynamicMesh()
    shape = contract.get_editor_property("base_shape")
    custom_sm = contract.get_editor_property("custom_monolith_mesh")
    dims_v = contract.get_editor_property("dimensions_cm")
    dims = (float(dims_v.x), float(dims_v.y), float(dims_v.z))

    if shape == unreal.MonolithBaseShape.CUSTOM_STATIC_MESH and custom_sm is not None:
        asset_opts = unreal.GeometryScriptCopyMeshFromAssetOptions()
        asset_opts.set_editor_property("apply_build_settings", True)
        asset_opts.set_editor_property("request_tangents", False)
        asset_opts.set_editor_property("ignore_remove_degenerates", False)
        lod = unreal.GeometryScriptMeshReadLOD()
        lod.set_editor_property("lod_type", unreal.GeometryScriptLODType.MAX_AVAILABLE)
        lod.set_editor_property("lod_index", 0)
        dyn_mesh, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(
            custom_sm, dyn_mesh, asset_opts, lod
        )
        if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
            raise RuntimeError(f"Failed copying StaticMesh {custom_sm.get_name()} to DynamicMesh")
        if not unreal.GeometryScript_MeshQueries.get_is_closed_mesh(dyn_mesh):
            weld_opts = unreal.GeometryScriptWeldEdgesOptions()
            weld_opts.set_editor_property("tolerance", 0.001)
            dyn_mesh = unreal.GeometryScript_MeshRepair.weld_mesh_edges(dyn_mesh, weld_opts)
        return dyn_mesh

    prim_opts = unreal.GeometryScriptPrimitiveOptions()
    if shape == unreal.MonolithBaseShape.BOX:
        dyn_mesh = unreal.GeometryScript_Primitives.append_box(
            dyn_mesh,
            prim_opts,
            unreal.Transform(),
            dims[0],
            dims[1],
            dims[2],
            0,
            0,
            0,
            unreal.GeometryScriptPrimitiveOriginMode.CENTER,
        )
        return dyn_mesh

    profile_yz = get_profile_yz_for_shape(shape, dims)
    area_yz, ana_cy, ana_cz = compute_polygon_area_and_centroid(profile_yz)
    if area_yz <= 0.0:
        raise AssertionError(f"ProfileYZ for {shape} must be strictly CCW (signed area={area_yz:.3f})")

    # Build in local (X_extrude, Y_extrude, Z_extrude) where polygon is (Y, Z) extruded along Z_extrude in [0, DimX],
    # then map (u, v, w) -> (w - 0.5*DimX, u, v). Find the exact Rotator among 90-degree basis rotations that matches
    # the analytical centroid and bounds to < 0.02 cm.
    poly_2d = [unreal.Vector2D(y, z) for (y, z) in profile_yz]
    found_mesh = None
    expected_vol = dims[0] * area_yz

    for roll_deg in (-90.0, 0.0, 90.0, 180.0):
        for pitch_deg in (-90.0, 0.0, 90.0, 180.0):
            for yaw_deg in (-90.0, 0.0, 90.0, 180.0):
                cand = unreal.DynamicMesh()
                cand = unreal.GeometryScript_Primitives.append_simple_extrude_polygon(
                    cand,
                    prim_opts,
                    unreal.Transform(),
                    poly_2d,
                    dims[0],
                    0,
                    True,
                )
                cand = unreal.GeometryScript_MeshTransforms.translate_mesh(
                    cand, unreal.Vector(0.0, 0.0, -0.5 * dims[0])
                )
                cand = unreal.GeometryScript_MeshTransforms.rotate_mesh(
                    cand,
                    unreal.Rotator(roll=roll_deg, pitch=pitch_deg, yaw=yaw_deg),
                    unreal.Vector(0.0, 0.0, 0.0),
                )
                _, c_vol, c_com = unreal.GeometryScript_MeshQueries.get_mesh_volume_area_center(cand)
                if float(c_vol) < 0.0:
                    cand = unreal.GeometryScript_Normals.flip_normals(cand)
                    _, c_vol, c_com = unreal.GeometryScript_MeshQueries.get_mesh_volume_area_center(cand)

                c_bb = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(cand)
                dx = float(c_bb.max.x) - float(c_bb.min.x)
                dy = float(c_bb.max.y) - float(c_bb.min.y)
                dz = float(c_bb.max.z) - float(c_bb.min.z)
                if (
                    abs(dx - dims[0]) < 0.05
                    and abs(dy - dims[1]) < 0.05
                    and abs(dz - dims[2]) < 0.05
                    and abs(float(c_com.x)) < 0.05
                    and abs(float(c_com.y) - ana_cy) < 0.05
                    and abs(float(c_com.z) - ana_cz) < 0.05
                    and abs(float(c_vol) - expected_vol) < 1.0
                ):
                    found_mesh = cand
                    break
            if found_mesh is not None:
                break
        if found_mesh is not None:
            break

    if found_mesh is None:
        raise RuntimeError(f"Failed orienting transient extruded profile for {shape}")
    return found_mesh


def compute_top_grip_center_cm(dyn_mesh: unreal.DynamicMesh) -> Tuple[float, float, float]:
    bbox = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(dyn_mesh)
    min_z = float(bbox.min.z)
    max_z = float(bbox.max.z)
    full_h = max(1.0, max_z - min_z)
    sample_depth = max(2.0, min(TOP_GRIP_SAMPLE_DEPTH_CM, 0.45 * full_h))
    z_cut = max_z - sample_depth

    # Slice off everything below z_cut on a duplicate mesh so the remaining upper band's bounding box
    # equals the exact vertex + edge-plane crossing bounds computed in C++ ComputeTopGripLocalBounds3d!
    band_mesh = unreal.DynamicMesh()
    unreal.GeometryScript_MeshDecomposition.copy_mesh_to_mesh(dyn_mesh, band_mesh)
    cut_opts = unreal.GeometryScriptMeshPlaneCutOptions()
    cut_opts.set_editor_property("fill_holes", False)
    cut_opts.set_editor_property("fill_spans", False)
    cut_opts.set_editor_property("flip_cut_side", True)
    plane_xf = unreal.Transform(
        location=unreal.Vector(0.0, 0.0, z_cut),
        rotation=unreal.Rotator(0.0, 0.0, 0.0),
        scale=unreal.Vector(1.0, 1.0, 1.0),
    )
    band_mesh = unreal.GeometryScript_MeshBooleans.apply_mesh_plane_cut(band_mesh, plane_xf, cut_opts)
    if band_mesh.get_triangle_count() == 0:
        cand2 = unreal.DynamicMesh()
        unreal.GeometryScript_MeshDecomposition.copy_mesh_to_mesh(dyn_mesh, cand2)
        cut_opts.set_editor_property("flip_cut_side", False)
        band_mesh = unreal.GeometryScript_MeshBooleans.apply_mesh_plane_cut(cand2, plane_xf, cut_opts)

    band_bb = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(band_mesh)
    return (
        0.5 * (float(band_bb.min.x) + float(band_bb.max.x)),
        0.5 * (float(band_bb.min.y) + float(band_bb.max.y)),
        0.5 * (float(band_bb.min.z) + float(band_bb.max.z)),
    )


def verify_no_floating_islands_on_horizontal_cuts(
    dyn_mesh: unreal.DynamicMesh,
    contract_name: str,
) -> None:
    bbox = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(dyn_mesh)
    min_z = float(bbox.min.z)
    max_z = float(bbox.max.z)
    height = max_z - min_z

    cut_specs = [
        (frac, unreal.Rotator(0.0, 0.0, 0.0))
        for frac in (0.15, 0.29, 0.43, 0.57, 0.64, 0.73, 0.81)
    ] + [
        (0.47, unreal.Rotator(12.0, 0.0, 0.0)),
        (0.47, unreal.Rotator(-12.0, 0.0, 0.0)),
        (0.47, unreal.Rotator(0.0, 12.0, 0.0)),
        (0.47, unreal.Rotator(0.0, -12.0, 0.0)),
    ]

    for frac, rot in cut_specs:
        z_cut = min_z + frac * height
        is_horizontal = abs(float(rot.roll)) < 1.0e-3 and abs(float(rot.pitch)) < 1.0e-3
        for flip_side in (False, True):
            half_mesh = unreal.DynamicMesh()
            unreal.GeometryScript_MeshDecomposition.copy_mesh_to_mesh(dyn_mesh, half_mesh)
            cut_opts = unreal.GeometryScriptMeshPlaneCutOptions()
            cut_opts.set_editor_property("fill_holes", True)
            cut_opts.set_editor_property("fill_spans", True)
            cut_opts.set_editor_property("flip_cut_side", flip_side)
            plane_xf = unreal.Transform(
                location=unreal.Vector(0.0, 0.0, z_cut),
                rotation=rot,
                scale=unreal.Vector(1.0, 1.0, 1.0),
            )
            half_mesh = unreal.GeometryScript_MeshBooleans.apply_mesh_plane_cut(half_mesh, plane_xf, cut_opts)
            closed = unreal.GeometryScript_MeshQueries.get_is_closed_mesh(half_mesh)
            islands = int(unreal.GeometryScript_MeshQueries.get_num_connected_components(half_mesh))
            _, vol_cm3, _ = unreal.GeometryScript_MeshQueries.get_mesh_volume_area_center(half_mesh)
            if (not closed) or float(vol_cm3) <= 10.0 or (is_horizontal and islands != 1):
                raise AssertionError(
                    f"{contract_name} cut at Z={z_cut:.2f} rot=({rot.roll:.1f},{rot.pitch:.1f}) (flip={flip_side}) "
                    f"failed watertight/island check: closed={closed}, islands={islands}, vol={vol_cm3:.1f} cm3"
                )


def query_signed_solid_distance_cm(
    dyn_mesh: unreal.DynamicMesh,
    bvh: unreal.GeometryScriptDynamicMeshBVH,
    pt: unreal.Vector,
) -> float:
    spatial_opts = unreal.GeometryScriptSpatialQueryOptions()
    spatial_opts.set_editor_property("winding_iso_threshold", 0.5)
    _, is_inside, _ = unreal.GeometryScript_MeshSpatial.is_point_inside_mesh(dyn_mesh, bvh, pt, spatial_opts)
    _, nearest_pt, _ = unreal.GeometryScript_MeshSpatial.find_nearest_point_on_mesh(dyn_mesh, bvh, pt, spatial_opts)
    pos = nearest_pt.position
    surf_dist = math.sqrt(
        (float(pt.x) - float(pos.x)) ** 2
        + (float(pt.y) - float(pos.y)) ** 2
        + (float(pt.z) - float(pos.z)) ** 2
    )
    return surf_dist if is_inside else -surf_dist


def verify_contract_geometry_veins_and_strain(
    contract_idx: int,
    contract: unreal.MonolithContractDataAsset,
) -> Dict[str, float]:
    c_name = contract.get_name()
    dyn_mesh = build_transient_contract_dynamic_mesh(contract)
    is_closed = unreal.GeometryScript_MeshQueries.get_is_closed_mesh(dyn_mesh)
    num_islands = int(unreal.GeometryScript_MeshQueries.get_num_connected_components(dyn_mesh))
    _, vol_cm3, com_cm = unreal.GeometryScript_MeshQueries.get_mesh_volume_area_center(dyn_mesh)

    if (not is_closed) or num_islands != 1 or float(vol_cm3) <= 100.0:
        raise AssertionError(
            f"{c_name} invalid manifold mesh: closed={is_closed}, islands={num_islands}, vol={vol_cm3:.1f}"
        )

    shape = contract.get_editor_property("base_shape")
    if shape in (
        unreal.MonolithBaseShape.CANTILEVERED_J_HOOK,
        unreal.MonolithBaseShape.OFFSET_C_CHANNEL,
        unreal.MonolithBaseShape.COUNTERWEIGHTED_Z_BRACKET,
    ):
        verify_no_floating_islands_on_horizontal_cuts(dyn_mesh, c_name)

    bbox = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(dyn_mesh)
    min_z = float(bbox.min.z)
    max_z = float(bbox.max.z)
    height_z = max_z - min_z
    jaw_depth_z = max(4.0, height_z * AUTO_FIT_JAW_VERTICAL_DEPTH_RATIO)
    jaw_pocket_min_z = max_z - jaw_depth_z

    _, bvh = unreal.GeometryScript_MeshSpatial.build_bvh_for_mesh(dyn_mesh)
    veins = list(contract.get_editor_property("hidden_veins") or [])
    min_vein_margin_cm = 999.0
    min_jaw_clearance_cm = 999.0

    for v_idx, vein in enumerate(veins):
        s_v = vein.get_editor_property("local_start_cm")
        e_v = vein.get_editor_property("local_end_cm")
        rad_cm = float(vein.get_editor_property("radius_cm"))
        vein_top_z = max(float(s_v.z), float(e_v.z)) + rad_cm
        jaw_clear = jaw_pocket_min_z - vein_top_z
        min_jaw_clearance_cm = min(min_jaw_clearance_cm, jaw_clear)
        if jaw_clear < 1.50:
            raise AssertionError(
                f"{c_name} Vein[{v_idx}] top Z={vein_top_z:.2f} cm intrudes into clamp jaw pocket "
                f"(JawMinZ={jaw_pocket_min_z:.2f} cm, clearance={jaw_clear:.2f} cm < 1.50 cm)!"
            )

        for step in range(17):
            alpha = step / 16.0
            px = float(s_v.x) + alpha * (float(e_v.x) - float(s_v.x))
            py = float(s_v.y) + alpha * (float(e_v.y) - float(s_v.y))
            pz = float(s_v.z) + alpha * (float(e_v.z) - float(s_v.z))
            sdist = query_signed_solid_distance_cm(dyn_mesh, bvh, unreal.Vector(px, py, pz))
            margin = sdist - rad_cm
            min_vein_margin_cm = min(min_vein_margin_cm, margin)
            if margin < 1.80:
                raise AssertionError(
                    f"{c_name} Vein[{v_idx}] sample {step}/16 at ({px:.2f},{py:.2f},{pz:.2f}) breaches exterior "
                    f"shell margin: signed_dist={sdist:.2f} cm, radius={rad_cm:.2f} cm, margin={margin:.2f} cm < 1.80 cm"
                )

    density = float(contract.get_editor_property("density_grams_per_cm3"))
    mass_kg = float(vol_cm3) * density * 0.001
    grip_center = compute_top_grip_center_cm(dyn_mesh)
    grip_offset = contract.get_editor_property("clamp_grip_horizontal_offset_cm")
    eff_pivot_x = grip_center[0] + float(grip_offset.x)
    eff_pivot_y = grip_center[1] + float(grip_offset.y)
    mount_com_x = float(com_cm.x) - eff_pivot_x
    mount_com_y = float(com_cm.y) - eff_pivot_y

    lever_m = math.hypot(mount_com_x, mount_com_y) * 0.01
    static_torque_nm = mass_kg * GRAVITY_M_PER_S2 * lever_m
    max_safe_nm = float(contract.get_editor_property("clamp_max_safe_torque_nm"))
    strain_ratio = static_torque_nm / max(1.0, max_safe_nm)

    if contract_idx == 0:
        if strain_ratio > 0.02:
            raise AssertionError(f"{c_name} onboarding symmetric billet must have ~0.0 strain, got {strain_ratio:.4f}")
    else:
        if not (0.45 <= strain_ratio <= 0.75):
            raise AssertionError(
                f"{c_name} initial clamp strain {strain_ratio:.3f} ({static_torque_nm:.2f}/{max_safe_nm:.1f} Nm) "
                f"outside required [0.45, 0.75] window! (com=({com_cm.x:.2f},{com_cm.y:.2f}), "
                f"grip_center=({grip_center[0]:.2f},{grip_center[1]:.2f}), offset=({grip_offset.x:.2f},{grip_offset.y:.2f}))"
            )

    log_info(
        f"VERIFIED {c_name} | shape={shape.name} | mass={mass_kg:.2f} kg | "
        f"mount_com=({mount_com_x:+.2f}, {mount_com_y:+.2f}) cm | torque={static_torque_nm:.2f}/{max_safe_nm:.1f} Nm "
        f"(strain={strain_ratio * 100.0:.1f}%) | veins={len(veins)} "
        f"(min_margin={min_vein_margin_cm:.2f} cm, min_jaw_clear={min_jaw_clearance_cm:.2f} cm)"
    )
    return {
        "mass_kg": mass_kg,
        "torque_nm": static_torque_nm,
        "strain_ratio": strain_ratio,
    }


def main() -> None:
    contracts: List[unreal.MonolithContractDataAsset] = []
    for i in range(1, 7):
        c_path = f"{CONTRACTS_DIR}/DA_Contract_0{i}"
        loaded = unreal.EditorAssetLibrary.load_asset(c_path)
        if loaded is None:
            raise RuntimeError(f"Missing contract asset: {c_path}")
        contracts.append(loaded)

    m_slag_outer = (
        contracts[3].get_editor_property("outer_shell_material")
        or unreal.EditorAssetLibrary.load_asset("/Game/Presentation/Materials/M_SlagOuter_Master")
    )
    m_molten_cap = (
        contracts[3].get_editor_property("molten_cut_cap_material")
        or unreal.EditorAssetLibrary.load_asset("/Game/Presentation/Materials/M_MoltenCutCap_Master")
    )

    # DA_Contract_01: DO NOT MODIFY OR SAVE (0 bytes touched)

    # DA_Contract_02: CantileveredJHook procedural casting
    c02 = contracts[1]
    c02.set_editor_property("contract_code", unreal.Text("ACT I-B // CTR-02: CANTILEVERED J-HOOK"))
    c02.set_editor_property("contract_directive_text", "CUT GREEN YIELD | [SPACE] DUMPS TARE")
    c02.set_editor_property("base_shape", unreal.MonolithBaseShape.CANTILEVERED_J_HOOK)
    c02.set_editor_property("custom_monolith_mesh", None)
    c02.set_editor_property("dimensions_cm", unreal.Vector(24.0, 34.0, 80.0))
    c02.set_editor_property("clamp_grip_horizontal_offset_cm", unreal.Vector2D(0.0, 0.0))
    c02.set_editor_property("density_grams_per_cm3", 0.85)
    c02.set_editor_property("target_tithe_mass_kg", 14.5)
    c02.set_editor_property("quota_tolerance_kg", 1.35)
    c02.set_editor_property("max_allowed_tare_waste_kg", 16.0)
    c02.set_editor_property("allow_diverter_toggle", True)
    c02.set_editor_property("clamp_max_safe_torque_nm", 29.0)
    c02.set_editor_property("clamp_fatigue_grace_seconds", 6.5)
    c02.set_editor_property("clamp_fatigue_cooldown_seconds", 3.0)
    c02.set_editor_property("clamp_spring_stiffness", 180.0)
    c02.set_editor_property("clamp_spring_damping", 24.0)
    c02.set_editor_property("hidden_veins", [])
    if c02.get_editor_property("outer_shell_material") is None and m_slag_outer is not None:
        c02.set_editor_property("outer_shell_material", m_slag_outer)
    if c02.get_editor_property("molten_cut_cap_material") is None and m_molten_cap is not None:
        c02.set_editor_property("molten_cut_cap_material", m_molten_cap)

    # DA_Contract_03: OffsetCChannel procedural casting
    c03 = contracts[2]
    c03.set_editor_property("contract_code", unreal.Text("ACT II-A // CTR-03: OFFSET C-CHANNEL CORE"))
    c03.set_editor_property("contract_directive_text", "HIDDEN VEIN: AUDIO SCAN & BEVEL [RMB]")
    c03.set_editor_property("base_shape", unreal.MonolithBaseShape.OFFSET_C_CHANNEL)
    c03.set_editor_property("custom_monolith_mesh", None)
    c03.set_editor_property("dimensions_cm", unreal.Vector(28.0, 34.0, 84.0))
    c03.set_editor_property("clamp_grip_horizontal_offset_cm", unreal.Vector2D(0.0, 0.0))
    c03.set_editor_property("density_grams_per_cm3", 0.92)
    c03.set_editor_property("target_tithe_mass_kg", 15.0)
    c03.set_editor_property("quota_tolerance_kg", 1.00)
    c03.set_editor_property("max_allowed_tare_waste_kg", 18.0)
    c03.set_editor_property("allow_diverter_toggle", True)
    c03.set_editor_property("clamp_max_safe_torque_nm", 30.0)
    c03.set_editor_property("clamp_fatigue_grace_seconds", 6.5)
    c03.set_editor_property("clamp_fatigue_cooldown_seconds", 3.0)
    c03.set_editor_property("clamp_spring_stiffness", 180.0)
    c03.set_editor_property("clamp_spring_damping", 24.0)
    c03.set_editor_property(
        "hidden_veins",
        [make_vein_struct((-5.5, -8.5, -33.5), (5.5, 6.5, -28.0), 2.4, 18.0)],
    )
    if c03.get_editor_property("outer_shell_material") is None and m_slag_outer is not None:
        c03.set_editor_property("outer_shell_material", m_slag_outer)
    if c03.get_editor_property("molten_cut_cap_material") is None and m_molten_cap is not None:
        c03.set_editor_property("molten_cut_cap_material", m_molten_cap)

    # DA_Contract_04: Keep SM_Monolith_Contract04 & existing veins untouched; apply eccentric +1.85 cm Y clamp offset
    c04 = contracts[3]
    if c04.get_editor_property("base_shape") != unreal.MonolithBaseShape.CUSTOM_STATIC_MESH:
        raise AssertionError("DA_Contract_04 expected CustomStaticMesh!")
    c04.set_editor_property("clamp_grip_horizontal_offset_cm", unreal.Vector2D(0.0, 1.85))

    # DA_Contract_05: CounterweightedZBracket bismuth casting
    c05 = contracts[4]
    c05.set_editor_property("contract_code", unreal.Text("ACT III-A // CTR-05: BISMUTH Z-COUNTERWEIGHT"))
    c05.set_editor_property("contract_directive_text", "TORQUE LIMIT: KEEP CUTS BALANCED")
    c05.set_editor_property("base_shape", unreal.MonolithBaseShape.COUNTERWEIGHTED_Z_BRACKET)
    c05.set_editor_property("custom_monolith_mesh", None)
    c05.set_editor_property("dimensions_cm", unreal.Vector(24.0, 34.0, 78.0))
    c05.set_editor_property("clamp_grip_horizontal_offset_cm", unreal.Vector2D(0.0, 0.0))
    c05.set_editor_property("density_grams_per_cm3", 1.82)
    c05.set_editor_property("target_tithe_mass_kg", 21.0)
    c05.set_editor_property("quota_tolerance_kg", 0.90)
    c05.set_editor_property("max_allowed_tare_waste_kg", 28.0)
    c05.set_editor_property("allow_diverter_toggle", True)
    c05.set_editor_property("clamp_max_safe_torque_nm", 32.0)
    c05.set_editor_property("clamp_fatigue_grace_seconds", 6.5)
    c05.set_editor_property("clamp_fatigue_cooldown_seconds", 3.0)
    c05.set_editor_property("clamp_spring_stiffness", 210.0)
    c05.set_editor_property("clamp_spring_damping", 28.0)
    c05.set_editor_property(
        "hidden_veins",
        [
            make_vein_struct((-4.5, -9.5, -31.0), (4.5, -2.5, -16.0), 2.3, 17.0),
            make_vein_struct((-4.5, 2.5, 5.5), (4.5, 9.5, 16.5), 2.3, 17.0),
        ],
    )
    if c05.get_editor_property("outer_shell_material") is None and m_slag_outer is not None:
        c05.set_editor_property("outer_shell_material", m_slag_outer)
    if c05.get_editor_property("molten_cut_cap_material") is None and m_molten_cap is not None:
        c05.set_editor_property("molten_cut_cap_material", m_molten_cap)

    # DA_Contract_06: Keep SM_Monolith_Contract06 & Veins 0..1 untouched; apply -2.50 cm Y clamp offset & lower Vein 2
    c06 = contracts[5]
    if c06.get_editor_property("base_shape") != unreal.MonolithBaseShape.CUSTOM_STATIC_MESH:
        raise AssertionError("DA_Contract_06 expected CustomStaticMesh!")
    c06.set_editor_property("clamp_grip_horizontal_offset_cm", unreal.Vector2D(0.0, -2.50))
    c06_veins = list(c06.get_editor_property("hidden_veins") or [])
    if len(c06_veins) != 3:
        raise AssertionError(f"DA_Contract_06 expected 3 hidden veins, found {len(c06_veins)}")
    c06_veins[2] = make_vein_struct((-4.5, -4.5, 13.0), (4.5, 4.5, 20.5), 2.20, 16.0)
    c06.set_editor_property("hidden_veins", c06_veins)

    # Verify zero duplicate procedural shapes across DA_Contract_01..06
    seen_proc_shapes = set()
    for c_asset in contracts:
        sh = c_asset.get_editor_property("base_shape")
        if sh != unreal.MonolithBaseShape.CUSTOM_STATIC_MESH:
            if sh in seen_proc_shapes:
                raise AssertionError(f"Duplicate procedural EMonolithBaseShape detected: {sh}")
            seen_proc_shapes.add(sh)

    # Run manifold + horizontal-cut single-island + BVH vein margin + jaw clearance + initial torque strain checks
    for idx, c_asset in enumerate(contracts):
        verify_contract_geometry_veins_and_strain(idx, c_asset)

    # Save ONLY DA_Contract_02..06
    for c_asset in contracts[1:]:
        if not unreal.EditorAssetLibrary.save_loaded_asset(c_asset, only_if_is_dirty=False):
            raise RuntimeError(f"Failed saving {c_asset.get_path_name()}")
        log_info(f"Saved {c_asset.get_path_name()}")

    log_info("SUCCESS: Updated & verified DA_Contract_02..06 (DA_Contract_01, maps, and meshes untouched).")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        log_error(f"FATAL: {exc}")
        sys.exit(1)
