import sys

sys.dont_write_bytecode = True

from typing import Tuple
import unreal


MATERIALS_DIR = "/Game/Presentation/Materials"
PBR_DIR = "/Game/Presentation/Meshes/PBR"
STATION_PBR_MASTER_PATH = f"{MATERIALS_DIR}/M_StationPBR_Master"
MOLTEN_CUT_CAP_MASTER_PATH = f"{MATERIALS_DIR}/M_MoltenCutCap_Master"


def log_info(msg: str) -> None:
    unreal.log(f"[UpgradeMaterialsPhase12D] {msg}")


def resolve_enum(enum_cls, *candidate_names: str):
    for name in candidate_names:
        val = getattr(enum_cls, name, None)
        if val is not None:
            return val
    avail = [k for k in dir(enum_cls) if not k.startswith("_")]
    raise RuntimeError(f"Could not resolve any of {candidate_names} on {enum_cls} (avail={avail})")


def create_expr(mat: unreal.Material, expr_cls, pos_x: int = 0, pos_y: int = 0):
    expr = unreal.MaterialEditingLibrary.create_material_expression(mat, expr_cls, pos_x, pos_y)
    if expr is None:
        raise RuntimeError(f"Failed to create expression {expr_cls} in {mat.get_name()}")
    return expr


def connect_expr(from_expr, from_pin: str, to_expr, to_pin: str) -> None:
    ok = unreal.MaterialEditingLibrary.connect_material_expressions(from_expr, from_pin, to_expr, to_pin)
    if not ok:
        out_names = list(unreal.MaterialEditingLibrary.get_material_expression_output_names(from_expr) or [])
        in_names = list(unreal.MaterialEditingLibrary.get_material_expression_input_names(to_expr) or [])
        if to_pin == "" and len(in_names) >= 1:
            ok = unreal.MaterialEditingLibrary.connect_material_expressions(
                from_expr, from_pin, to_expr, str(in_names[0])
            )
        if not ok and from_pin == "" and len(out_names) >= 1:
            cand_to = str(in_names[0]) if (to_pin == "" and len(in_names) >= 1) else to_pin
            ok = unreal.MaterialEditingLibrary.connect_material_expressions(
                from_expr, str(out_names[0]), to_expr, cand_to
            )
        if not ok:
            raise RuntimeError(
                f"Failed connecting {from_expr.get_class().get_name()}[{from_pin!r}] (avail={out_names}) -> "
                f"{to_expr.get_class().get_name()}[{to_pin!r}] (avail={in_names})"
            )


def connect_prop(from_expr, from_pin: str, mat_prop: unreal.MaterialProperty) -> None:
    ok = unreal.MaterialEditingLibrary.connect_material_property(from_expr, from_pin, mat_prop)
    if not ok:
        out_names = list(unreal.MaterialEditingLibrary.get_material_expression_output_names(from_expr) or [])
        if from_pin == "" and len(out_names) >= 1:
            ok = unreal.MaterialEditingLibrary.connect_material_property(from_expr, str(out_names[0]), mat_prop)
        if not ok:
            raise RuntimeError(
                f"Failed connecting {from_expr.get_class().get_name()}[{from_pin!r}] (avail={out_names}) -> {mat_prop}"
            )


def make_const_scalar(mat: unreal.Material, val: float, pos_x: int = 0, pos_y: int = 0):
    node = create_expr(mat, unreal.MaterialExpressionConstant, pos_x, pos_y)
    node.set_editor_property("r", float(val))
    return node


def make_const_color3(mat: unreal.Material, r: float, g: float, b: float, pos_x: int = 0, pos_y: int = 0):
    node = create_expr(mat, unreal.MaterialExpressionConstant3Vector, pos_x, pos_y)
    node.set_editor_property("constant", unreal.LinearColor(float(r), float(g), float(b), 1.0))
    return node


def make_scalar_param(mat: unreal.Material, name: str, default_val: float, pos_x: int = 0, pos_y: int = 0):
    node = create_expr(mat, unreal.MaterialExpressionScalarParameter, pos_x, pos_y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", float(default_val))
    return node


def make_vector_param(
    mat: unreal.Material,
    name: str,
    r: float,
    g: float,
    b: float,
    a: float,
    pos_x: int = 0,
    pos_y: int = 0,
):
    node = create_expr(mat, unreal.MaterialExpressionVectorParameter, pos_x, pos_y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", unreal.LinearColor(float(r), float(g), float(b), float(a)))
    return node


def make_clamp01(mat: unreal.Material, src_expr, src_pin: str = "", pos_x: int = 0, pos_y: int = 0):
    cl = create_expr(mat, unreal.MaterialExpressionClamp, pos_x, pos_y)
    cl.set_editor_property("min_default", 0.0)
    cl.set_editor_property("max_default", 1.0)
    connect_expr(src_expr, src_pin, cl, "")
    return cl


def make_mask_channel(mat: unreal.Material, src_expr, r: bool, g: bool, b: bool, pos_x: int = 0, pos_y: int = 0):
    mask = create_expr(mat, unreal.MaterialExpressionComponentMask, pos_x, pos_y)
    mask.set_editor_property("r", r)
    mask.set_editor_property("g", g)
    mask.set_editor_property("b", b)
    mask.set_editor_property("a", False)
    connect_expr(src_expr, "", mask, "")
    return mask


def make_safe_unit_vector3(mat: unreal.Material, vec_expr, pos_x: int = 0, pos_y: int = 0):
    dot_sq = create_expr(mat, unreal.MaterialExpressionDotProduct, pos_x, pos_y)
    connect_expr(vec_expr, "", dot_sq, "A")
    connect_expr(vec_expr, "", dot_sq, "B")

    safe_sq = create_expr(mat, unreal.MaterialExpressionMax, pos_x + 140, pos_y)
    safe_sq.set_editor_property("const_b", 1.0e-6)
    connect_expr(dot_sq, "", safe_sq, "A")

    vec_len = create_expr(mat, unreal.MaterialExpressionSquareRoot, pos_x + 280, pos_y)
    connect_expr(safe_sq, "", vec_len, "")

    unit_vec = create_expr(mat, unreal.MaterialExpressionDivide, pos_x + 420, pos_y)
    connect_expr(vec_expr, "", unit_vec, "A")
    connect_expr(vec_len, "", unit_vec, "B")
    return unit_vec


def make_gradient_noise(
    mat: unreal.Material,
    pos3_expr,
    scale: float,
    levels: int = 2,
    out_min: float = 0.0,
    out_max: float = 1.0,
    pos_x: int = 0,
    pos_y: int = 0,
):
    noise_fn = resolve_enum(unreal.NoiseFunction, "NOISEFUNCTION_GRADIENT_ALU", "GRADIENT_ALU")
    noise = create_expr(mat, unreal.MaterialExpressionNoise, pos_x, pos_y)
    noise.set_editor_property("scale", float(scale))
    noise.set_editor_property("noise_function", noise_fn)
    noise.set_editor_property("quality", 1)
    noise.set_editor_property("levels", int(levels))
    noise.set_editor_property("turbulence", False)
    noise.set_editor_property("output_min", float(out_min))
    noise.set_editor_property("output_max", float(out_max))
    connect_expr(pos3_expr, "", noise, "")
    return noise


def resolve_sampler_type_for_texture(tex: unreal.Texture2D) -> unreal.MaterialSamplerType:
    comp = tex.get_editor_property("compression_settings")
    srgb = bool(tex.get_editor_property("srgb"))
    if comp == unreal.TextureCompressionSettings.TC_NORMALMAP:
        return unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    if comp == unreal.TextureCompressionSettings.TC_MASKS:
        return unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
    if srgb:
        return unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
    return unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR


def load_clamp_jaw_default_textures() -> Tuple[unreal.Texture2D, unreal.Texture2D, unreal.Texture2D]:
    base_tex_dir = f"{PBR_DIR}/SM_ClampJaw/SM_ClampJaw/Textures"
    diff_tex = unreal.EditorAssetLibrary.load_asset(f"{base_tex_dir}/texture_diffuse")
    orm_tex = unreal.EditorAssetLibrary.load_asset(f"{base_tex_dir}/texture_metallic-texture_roughness")
    norm_tex = unreal.EditorAssetLibrary.load_asset(f"{base_tex_dir}/texture_normal")
    if not isinstance(diff_tex, unreal.Texture2D):
        raise RuntimeError("Missing SM_ClampJaw diffuse texture")
    if not isinstance(orm_tex, unreal.Texture2D):
        raise RuntimeError("Missing SM_ClampJaw ORM texture")
    if not isinstance(norm_tex, unreal.Texture2D):
        raise RuntimeError("Missing SM_ClampJaw normal texture")
    return diff_tex, orm_tex, norm_tex


def populate_phase12d_station_pbr_graph(
    mat: unreal.Material,
    default_diff: unreal.Texture2D,
    default_orm: unreal.Texture2D,
    default_norm: unreal.Texture2D,
) -> None:
    """
    Phase 12D Nanite & ISM-safe M_StationPBR_Master:
    - Preserves all 10 existing scalar/vector/texture parameters for 100% compatibility with all 9 MI_Station_* assets.
    - Safeguard 2: Computes rigid object/ISM-instance-locked centimeter coordinates without non-uniform stretching
      by projecting (WorldPosition - TransformPosition(0, Instance->World)) onto the 3 normalized Instance->World basis axes.
    - Adds zero-decal edge chipping, cavity oil/soot & upward slag spatter driven by 2 lean Gradient ALU Noise nodes (Levels=2),
      gated by ProceduralWearWeight (default 1.0).
    - Adds RearGridMacroSootWeight (default 0.0; 1.0 on FoundryRearGridMID) to suppress outer/corner 2x2 grate luminance.
    - Adds EmissiveTint (default (0,0,0,1)) for cook-safe ISM cold optical lens glints on ADirectorBoothActor.
    """
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("two_sided", False)
    mat.set_editor_property("used_with_static_lighting", False)

    pos_src_inst = resolve_enum(
        unreal.MaterialPositionTransformSource, "TRANSFORMPOSSOURCE_INSTANCE", "INSTANCE"
    )
    pos_dst_world = resolve_enum(
        unreal.MaterialPositionTransformSource, "TRANSFORMPOSSOURCE_WORLD", "WORLD"
    )
    vec_src_inst = resolve_enum(
        unreal.MaterialVectorCoordTransformSource, "TRANSFORMSOURCE_INSTANCE", "INSTANCE"
    )
    vec_dst_world = resolve_enum(
        unreal.MaterialVectorCoordTransform, "TRANSFORM_WORLD", "WORLD"
    )

    # =========================================================================
    # 1. Rigid Isotropic Object / ISM-Instance Centimeter Coordinates (P_iso_cm)
    # =========================================================================
    world_pos = create_expr(mat, unreal.MaterialExpressionWorldPosition, -2600, -900)
    zero_local = make_const_color3(mat, 0.0, 0.0, 0.0, -2600, -760)

    inst_origin_ws = create_expr(mat, unreal.MaterialExpressionTransformPosition, -2360, -760)
    inst_origin_ws.set_editor_property("transform_source_type", pos_src_inst)
    inst_origin_ws.set_editor_property("transform_type", pos_dst_world)
    connect_expr(zero_local, "", inst_origin_ws, "")

    delta_ws = create_expr(mat, unreal.MaterialExpressionSubtract, -2100, -840)
    connect_expr(world_pos, "", delta_ws, "A")
    connect_expr(inst_origin_ws, "", delta_ws, "B")

    def make_instance_unit_axis_ws(ux: float, uy: float, uz: float, py: int):
        raw_axis = make_const_color3(mat, ux, uy, uz, -2600, py)
        xf_axis = create_expr(mat, unreal.MaterialExpressionTransform, -2380, py)
        xf_axis.set_editor_property("transform_source_type", vec_src_inst)
        xf_axis.set_editor_property("transform_type", vec_dst_world)
        connect_expr(raw_axis, "", xf_axis, "")
        return make_safe_unit_vector3(mat, xf_axis, -2160, py)

    axis_x_ws = make_instance_unit_axis_ws(1.0, 0.0, 0.0, -600)
    axis_y_ws = make_instance_unit_axis_ws(0.0, 1.0, 0.0, -460)
    axis_z_ws = make_instance_unit_axis_ws(0.0, 0.0, 1.0, -320)

    iso_x = create_expr(mat, unreal.MaterialExpressionDotProduct, -1560, -740)
    connect_expr(delta_ws, "", iso_x, "A")
    connect_expr(axis_x_ws, "", iso_x, "B")

    iso_y = create_expr(mat, unreal.MaterialExpressionDotProduct, -1560, -620)
    connect_expr(delta_ws, "", iso_y, "A")
    connect_expr(axis_y_ws, "", iso_y, "B")

    iso_z = create_expr(mat, unreal.MaterialExpressionDotProduct, -1560, -500)
    connect_expr(delta_ws, "", iso_z, "A")
    connect_expr(axis_z_ws, "", iso_z, "B")

    iso_xy = create_expr(mat, unreal.MaterialExpressionAppendVector, -1380, -680)
    connect_expr(iso_x, "", iso_xy, "A")
    connect_expr(iso_y, "", iso_xy, "B")

    p_iso_cm = create_expr(mat, unreal.MaterialExpressionAppendVector, -1200, -600)
    connect_expr(iso_xy, "", p_iso_cm, "A")
    connect_expr(iso_z, "", p_iso_cm, "B")

    # =========================================================================
    # 2. Base Rodin PBR Texture Sample Parameters
    # =========================================================================
    tex_base = create_expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, -1480, -240)
    tex_base.set_editor_property("parameter_name", "BaseColorMap")
    tex_base.set_editor_property("texture", default_diff)
    tex_base.set_editor_property("sampler_type", resolve_sampler_type_for_texture(default_diff))

    tex_orm = create_expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, -1480, 120)
    tex_orm.set_editor_property("parameter_name", "ORMMap")
    tex_orm.set_editor_property("texture", default_orm)
    tex_orm.set_editor_property("sampler_type", resolve_sampler_type_for_texture(default_orm))

    tex_norm = create_expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, -1480, 520)
    tex_norm.set_editor_property("parameter_name", "NormalMap")
    tex_norm.set_editor_property("texture", default_norm)
    tex_norm.set_editor_property("sampler_type", resolve_sampler_type_for_texture(default_norm))

    # =========================================================================
    # 3. Procedural Edge Chipping, Cavity Soot & Upward Slag Spatter (2 Noise Nodes)
    # =========================================================================
    wear_weight_param = make_scalar_param(mat, "ProceduralWearWeight", 1.0, -1200, -920)
    wear_weight = make_clamp01(mat, wear_weight_param, "", -1020, -920)

    noise_edge_raw = make_gradient_noise(mat, p_iso_cm, scale=0.14, levels=2, out_min=0.0, out_max=1.0, pos_x=-1000, pos_y=-740)
    noise_edge = make_clamp01(mat, noise_edge_raw, "", -820, -740)

    grime_offset = make_const_color3(mat, 37.0, 83.0, 19.0, -1200, -460)
    p_iso_grime = create_expr(mat, unreal.MaterialExpressionAdd, -1040, -520)
    connect_expr(p_iso_cm, "", p_iso_grime, "A")
    connect_expr(grime_offset, "", p_iso_grime, "B")

    noise_grime_raw = make_gradient_noise(mat, p_iso_grime, scale=0.038, levels=2, out_min=0.0, out_max=1.0, pos_x=-860, pos_y=-520)
    noise_grime = make_clamp01(mat, noise_grime_raw, "", -680, -520)

    # Edge chipping mask from tangent normal bevel slope (1.0 - NormalMap.B) * open AO * noise_edge
    one_minus_norm_b = create_expr(mat, unreal.MaterialExpressionOneMinus, -1180, 660)
    connect_expr(tex_norm, "B", one_minus_norm_b, "")

    bevel_scaled = create_expr(mat, unreal.MaterialExpressionMultiply, -1020, 660)
    bevel_scaled.set_editor_property("const_b", 7.5)
    connect_expr(one_minus_norm_b, "", bevel_scaled, "A")
    bevel_mask = make_clamp01(mat, bevel_scaled, "", -860, 660)

    ao_open_sub = create_expr(mat, unreal.MaterialExpressionSubtract, -1180, -20)
    ao_open_sub.set_editor_property("const_b", 0.55)
    connect_expr(tex_orm, "R", ao_open_sub, "A")
    ao_open_mul = create_expr(mat, unreal.MaterialExpressionMultiply, -1020, -20)
    ao_open_mul.set_editor_property("const_b", 2.22)
    connect_expr(ao_open_sub, "", ao_open_mul, "A")
    ao_open_mask = make_clamp01(mat, ao_open_mul, "", -860, -20)

    edge_noise_sub = create_expr(mat, unreal.MaterialExpressionSubtract, -660, -740)
    edge_noise_sub.set_editor_property("const_b", 0.36)
    connect_expr(noise_edge, "", edge_noise_sub, "A")
    edge_noise_mul = create_expr(mat, unreal.MaterialExpressionMultiply, -500, -740)
    edge_noise_mul.set_editor_property("const_b", 2.20)
    connect_expr(edge_noise_sub, "", edge_noise_mul, "A")
    edge_noise_mask = make_clamp01(mat, edge_noise_mul, "", -340, -740)

    chip_ab = create_expr(mat, unreal.MaterialExpressionMultiply, -660, -640)
    connect_expr(bevel_mask, "", chip_ab, "A")
    connect_expr(ao_open_mask, "", chip_ab, "B")

    chip_abc = create_expr(mat, unreal.MaterialExpressionMultiply, -180, -680)
    connect_expr(chip_ab, "", chip_abc, "A")
    connect_expr(edge_noise_mask, "", chip_abc, "B")

    chip_boost = create_expr(mat, unreal.MaterialExpressionMultiply, -20, -680)
    chip_boost.set_editor_property("const_b", 1.65)
    connect_expr(chip_abc, "", chip_boost, "A")
    chip_clamped = make_clamp01(mat, chip_boost, "", 140, -680)

    chip_mask = create_expr(mat, unreal.MaterialExpressionMultiply, 300, -680)
    connect_expr(chip_clamped, "", chip_mask, "A")
    connect_expr(wear_weight, "", chip_mask, "B")

    # Cavity soot + local upward-facing slag spatter mask
    inv_ao = create_expr(mat, unreal.MaterialExpressionOneMinus, -1180, 60)
    connect_expr(tex_orm, "R", inv_ao, "")
    cav_scaled = create_expr(mat, unreal.MaterialExpressionMultiply, -1020, 60)
    cav_scaled.set_editor_property("const_b", 1.45)
    connect_expr(inv_ao, "", cav_scaled, "A")
    cav_base = make_clamp01(mat, cav_scaled, "", -860, 60)

    grime_mod = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -500, -520)
    grime_mod.set_editor_property("const_a", 0.45)
    grime_mod.set_editor_property("const_b", 1.00)
    connect_expr(noise_grime, "", grime_mod, "Alpha")

    cavity_soot = create_expr(mat, unreal.MaterialExpressionMultiply, -320, -480)
    connect_expr(cav_base, "", cavity_soot, "A")
    connect_expr(grime_mod, "", cavity_soot, "B")

    vtx_norm_ws = create_expr(mat, unreal.MaterialExpressionVertexNormalWS, -1560, -320)
    up_dot_raw = create_expr(mat, unreal.MaterialExpressionDotProduct, -1340, -320)
    connect_expr(vtx_norm_ws, "", up_dot_raw, "A")
    connect_expr(axis_z_ws, "", up_dot_raw, "B")
    local_up_mask = make_clamp01(mat, up_dot_raw, "", -1160, -320)

    spatter_prod = create_expr(mat, unreal.MaterialExpressionMultiply, -660, -420)
    connect_expr(noise_edge, "", spatter_prod, "A")
    connect_expr(noise_grime, "", spatter_prod, "B")
    spatter_sub = create_expr(mat, unreal.MaterialExpressionSubtract, -500, -420)
    spatter_sub.set_editor_property("const_b", 0.34)
    connect_expr(spatter_prod, "", spatter_sub, "A")
    spatter_mul = create_expr(mat, unreal.MaterialExpressionMultiply, -340, -420)
    spatter_mul.set_editor_property("const_b", 3.20)
    connect_expr(spatter_sub, "", spatter_mul, "A")
    spatter_dots = make_clamp01(mat, spatter_mul, "", -180, -420)

    up_spatter_raw = create_expr(mat, unreal.MaterialExpressionMultiply, -20, -380)
    connect_expr(spatter_dots, "", up_spatter_raw, "A")
    connect_expr(local_up_mask, "", up_spatter_raw, "B")
    up_spatter = create_expr(mat, unreal.MaterialExpressionMultiply, 140, -380)
    up_spatter.set_editor_property("const_b", 0.55)
    connect_expr(up_spatter_raw, "", up_spatter, "A")

    cav_weighted = create_expr(mat, unreal.MaterialExpressionMultiply, -140, -500)
    cav_weighted.set_editor_property("const_b", 0.68)
    connect_expr(cavity_soot, "", cav_weighted, "A")

    grime_sum = create_expr(mat, unreal.MaterialExpressionAdd, 280, -460)
    connect_expr(cav_weighted, "", grime_sum, "A")
    connect_expr(up_spatter, "", grime_sum, "B")
    grime_clamped = make_clamp01(mat, grime_sum, "", 420, -460)

    grime_mask = create_expr(mat, unreal.MaterialExpressionMultiply, 560, -460)
    connect_expr(grime_clamped, "", grime_mask, "A")
    connect_expr(wear_weight, "", grime_mask, "B")

    # =========================================================================
    # 4. Rear Foundry Grid Macro Soot Vignette (RearGridMacroSootWeight, default 0.0)
    # =========================================================================
    rear_soot_param = make_scalar_param(mat, "RearGridMacroSootWeight", 0.0, -1200, -1080)
    rear_soot_weight = make_clamp01(mat, rear_soot_param, "", -1020, -1080)

    wy = make_mask_channel(mat, world_pos, False, True, False, -2320, -1080)
    wz = make_mask_channel(mat, world_pos, False, False, True, -2320, -980)

    wy_norm = create_expr(mat, unreal.MaterialExpressionDivide, -2140, -1080)
    wy_norm.set_editor_property("const_b", 135.0)
    connect_expr(wy, "", wy_norm, "A")
    wy_sq = create_expr(mat, unreal.MaterialExpressionMultiply, -1980, -1080)
    connect_expr(wy_norm, "", wy_sq, "A")
    connect_expr(wy_norm, "", wy_sq, "B")

    wz_centered = create_expr(mat, unreal.MaterialExpressionSubtract, -2140, -980)
    wz_centered.set_editor_property("const_b", 85.0)
    connect_expr(wz, "", wz_centered, "A")
    wz_norm = create_expr(mat, unreal.MaterialExpressionDivide, -1980, -980)
    wz_norm.set_editor_property("const_b", 130.0)
    connect_expr(wz_centered, "", wz_norm, "A")
    wz_sq = create_expr(mat, unreal.MaterialExpressionMultiply, -1820, -980)
    connect_expr(wz_norm, "", wz_sq, "A")
    connect_expr(wz_norm, "", wz_sq, "B")

    rad_sq = create_expr(mat, unreal.MaterialExpressionAdd, -1660, -1040)
    connect_expr(wy_sq, "", rad_sq, "A")
    connect_expr(wz_sq, "", rad_sq, "B")

    rad_sub = create_expr(mat, unreal.MaterialExpressionSubtract, -1500, -1040)
    rad_sub.set_editor_property("const_b", 0.18)
    connect_expr(rad_sq, "", rad_sub, "A")
    rad_mul = create_expr(mat, unreal.MaterialExpressionMultiply, -1340, -1040)
    rad_mul.set_editor_property("const_b", 0.62)
    connect_expr(rad_sub, "", rad_mul, "A")
    edge_vig = make_clamp01(mat, rad_mul, "", -1180, -1040)

    pit_z_ref = make_const_scalar(mat, 35.0, -2140, -880)
    pit_z_sub = create_expr(mat, unreal.MaterialExpressionSubtract, -1980, -880)
    connect_expr(pit_z_ref, "", pit_z_sub, "A")
    connect_expr(wz, "", pit_z_sub, "B")
    pit_z_div = create_expr(mat, unreal.MaterialExpressionDivide, -1820, -880)
    pit_z_div.set_editor_property("const_b", 110.0)
    connect_expr(pit_z_sub, "", pit_z_div, "A")
    pit_soot_01 = make_clamp01(mat, pit_z_div, "", -1660, -880)
    pit_soot = create_expr(mat, unreal.MaterialExpressionMultiply, -1500, -880)
    pit_soot.set_editor_property("const_b", 0.45)
    connect_expr(pit_soot_01, "", pit_soot, "A")

    vig_scaled = create_expr(mat, unreal.MaterialExpressionMultiply, -1000, -1020)
    vig_scaled.set_editor_property("const_b", 0.65)
    connect_expr(edge_vig, "", vig_scaled, "A")

    macro_soot_sum = create_expr(mat, unreal.MaterialExpressionAdd, -840, -980)
    connect_expr(vig_scaled, "", macro_soot_sum, "A")
    connect_expr(pit_soot, "", macro_soot_sum, "B")

    macro_noise_mod = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -660, -980)
    macro_noise_mod.set_editor_property("const_a", 0.72)
    macro_noise_mod.set_editor_property("const_b", 1.00)
    connect_expr(noise_grime, "", macro_noise_mod, "Alpha")

    macro_soot_modulated = create_expr(mat, unreal.MaterialExpressionMultiply, -480, -980)
    connect_expr(macro_soot_sum, "", macro_soot_modulated, "A")
    connect_expr(macro_noise_mod, "", macro_soot_modulated, "B")
    macro_soot_01 = make_clamp01(mat, macro_soot_modulated, "", -320, -980)

    macro_soot_mask = create_expr(mat, unreal.MaterialExpressionMultiply, -160, -980)
    connect_expr(macro_soot_01, "", macro_soot_mask, "A")
    connect_expr(rear_soot_weight, "", macro_soot_mask, "B")

    # =========================================================================
    # 5. ORM Unpack + Wear Modulation (AO, Roughness, Metallic, Specular)
    # =========================================================================
    ao_strength = make_scalar_param(mat, "AOStrength", 0.85, -920, 120)
    ao_lerp = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -620, 120)
    ao_lerp.set_editor_property("const_a", 1.0)
    connect_expr(tex_orm, "R", ao_lerp, "B")
    connect_expr(ao_strength, "", ao_lerp, "Alpha")

    ao_soot_darken = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, 740, 80)
    ao_soot_darken.set_editor_property("const_a", 1.0)
    ao_soot_darken.set_editor_property("const_b", 0.72)
    connect_expr(grime_mask, "", ao_soot_darken, "Alpha")

    ao_final = create_expr(mat, unreal.MaterialExpressionMultiply, 940, 80)
    connect_expr(ao_lerp, "", ao_final, "A")
    connect_expr(ao_soot_darken, "", ao_final, "B")
    connect_prop(ao_final, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    # Roughness
    rough_clamp_in = create_expr(mat, unreal.MaterialExpressionClamp, -980, 220)
    rough_clamp_in.set_editor_property("min_default", 0.02)
    rough_clamp_in.set_editor_property("max_default", 1.0)
    connect_expr(tex_orm, "G", rough_clamp_in, "")

    rough_exp_param = make_scalar_param(mat, "RoughnessContrastExp", 1.65, -980, 310)
    rough_pow = create_expr(mat, unreal.MaterialExpressionPower, -760, 240)
    connect_expr(rough_clamp_in, "", rough_pow, "Base")
    connect_expr(rough_exp_param, "", rough_pow, "Exp")

    rough_min_param = make_scalar_param(mat, "RoughnessMin", 0.25, -760, 340)
    rough_max_param = make_scalar_param(mat, "RoughnessMax", 0.48, -760, 420)

    rough_remap = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -500, 260)
    connect_expr(rough_min_param, "", rough_remap, "A")
    connect_expr(rough_max_param, "", rough_remap, "B")
    connect_expr(rough_pow, "", rough_remap, "Alpha")

    grime_rough_alpha = create_expr(mat, unreal.MaterialExpressionMultiply, 660, 220)
    grime_rough_alpha.set_editor_property("const_b", 0.52)
    connect_expr(grime_mask, "", grime_rough_alpha, "A")

    rough_after_grime = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, 820, 200)
    rough_after_grime.set_editor_property("const_b", 0.78)
    connect_expr(rough_remap, "", rough_after_grime, "A")
    connect_expr(grime_rough_alpha, "", rough_after_grime, "Alpha")

    chip_rough_alpha = create_expr(mat, unreal.MaterialExpressionMultiply, 660, 300)
    chip_rough_alpha.set_editor_property("const_b", 0.55)
    connect_expr(chip_mask, "", chip_rough_alpha, "A")

    rough_after_chip = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, 980, 220)
    rough_after_chip.set_editor_property("const_b", 0.18)
    connect_expr(rough_after_grime, "", rough_after_chip, "A")
    connect_expr(chip_rough_alpha, "", rough_after_chip, "Alpha")

    macro_rough_alpha = create_expr(mat, unreal.MaterialExpressionMultiply, 980, 320)
    macro_rough_alpha.set_editor_property("const_b", 0.45)
    connect_expr(macro_soot_mask, "", macro_rough_alpha, "A")

    rough_after_macro = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, 1140, 240)
    rough_after_macro.set_editor_property("const_b", 0.74)
    connect_expr(rough_after_chip, "", rough_after_macro, "A")
    connect_expr(macro_rough_alpha, "", rough_after_macro, "Alpha")

    rough_final = create_expr(mat, unreal.MaterialExpressionClamp, 1300, 240)
    rough_final.set_editor_property("min_default", 0.16)
    rough_final.set_editor_property("max_default", 0.90)
    connect_expr(rough_after_macro, "", rough_final, "")
    connect_prop(rough_final, "", unreal.MaterialProperty.MP_ROUGHNESS)

    # Metallic
    metal_scale_param = make_scalar_param(mat, "MetallicScale", 1.05, -1040, 480)
    metal_scaled = create_expr(mat, unreal.MaterialExpressionMultiply, -860, 480)
    connect_expr(tex_orm, "B", metal_scaled, "A")
    connect_expr(metal_scale_param, "", metal_scaled, "B")
    metal_alpha = make_clamp01(mat, metal_scaled, "", -700, 480)

    metal_min_param = make_scalar_param(mat, "MetallicMin", 0.40, -740, 560)
    metal_max_param = make_scalar_param(mat, "MetallicMax", 0.58, -740, 640)

    metal_lerp = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -500, 520)
    connect_expr(metal_min_param, "", metal_lerp, "A")
    connect_expr(metal_max_param, "", metal_lerp, "B")
    connect_expr(metal_alpha, "", metal_lerp, "Alpha")

    grime_metal_alpha = create_expr(mat, unreal.MaterialExpressionMultiply, 660, 440)
    grime_metal_alpha.set_editor_property("const_b", 0.38)
    connect_expr(grime_mask, "", grime_metal_alpha, "A")

    metal_after_grime = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, 840, 460)
    metal_after_grime.set_editor_property("const_b", 0.24)
    connect_expr(metal_lerp, "", metal_after_grime, "A")
    connect_expr(grime_metal_alpha, "", metal_after_grime, "Alpha")

    chip_metal_alpha = create_expr(mat, unreal.MaterialExpressionMultiply, 840, 560)
    chip_metal_alpha.set_editor_property("const_b", 0.55)
    connect_expr(chip_mask, "", chip_metal_alpha, "A")

    metal_after_chip = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, 1020, 480)
    metal_after_chip.set_editor_property("const_b", 0.82)
    connect_expr(metal_after_grime, "", metal_after_chip, "A")
    connect_expr(chip_metal_alpha, "", metal_after_chip, "Alpha")

    metal_final = make_clamp01(mat, metal_after_chip, "", 1200, 480)
    connect_prop(metal_final, "", unreal.MaterialProperty.MP_METALLIC)

    # Specular
    spec_scale_param = make_scalar_param(mat, "SpecularScale", 0.72, 840, 660)
    spec_macro_atten = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, 1020, 660)
    spec_macro_atten.set_editor_property("const_a", 1.0)
    spec_macro_atten.set_editor_property("const_b", 0.45)
    connect_expr(macro_soot_mask, "", spec_macro_atten, "Alpha")

    spec_final = create_expr(mat, unreal.MaterialExpressionMultiply, 1200, 660)
    connect_expr(spec_scale_param, "", spec_final, "A")
    connect_expr(spec_macro_atten, "", spec_final, "B")
    connect_prop(spec_final, "", unreal.MaterialProperty.MP_SPECULAR)

    # =========================================================================
    # 6. BaseColor + Conductor F0 Floor + Edge Chipping / Soot Grime / Macro Soot
    # =========================================================================
    albedo_tint_param = make_vector_param(mat, "AlbedoTint", 1.65, 1.72, 1.82, 1.0, -1040, -240)
    tinted_rgb = create_expr(mat, unreal.MaterialExpressionMultiply, -800, -280)
    connect_expr(tex_base, "RGB", tinted_rgb, "A")
    connect_expr(albedo_tint_param, "", tinted_rgb, "B")

    conductor_f0_param = make_vector_param(mat, "ConductorMinF0", 0.24, 0.26, 0.30, 1.0, -800, -160)
    diffuse_floor_rgb = create_expr(mat, unreal.MaterialExpressionMultiply, -600, -320)
    diffuse_floor_rgb.set_editor_property("const_b", 0.45)
    connect_expr(conductor_f0_param, "", diffuse_floor_rgb, "A")

    diffuse_safe_rgb = create_expr(mat, unreal.MaterialExpressionMax, -440, -280)
    connect_expr(tinted_rgb, "", diffuse_safe_rgb, "A")
    connect_expr(diffuse_floor_rgb, "", diffuse_safe_rgb, "B")

    conductor_safe_rgb = create_expr(mat, unreal.MaterialExpressionMax, -440, -180)
    connect_expr(tinted_rgb, "", conductor_safe_rgb, "A")
    connect_expr(conductor_f0_param, "", conductor_safe_rgb, "B")

    base_steel_albedo = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -220, -240)
    connect_expr(diffuse_safe_rgb, "", base_steel_albedo, "A")
    connect_expr(conductor_safe_rgb, "", base_steel_albedo, "B")
    connect_expr(metal_final, "", base_steel_albedo, "Alpha")

    slag_soot_color = make_const_color3(mat, 0.030, 0.026, 0.024, 620, -280)
    grime_color_alpha = create_expr(mat, unreal.MaterialExpressionMultiply, 620, -180)
    grime_color_alpha.set_editor_property("const_b", 0.46)
    connect_expr(grime_mask, "", grime_color_alpha, "A")

    albedo_after_grime = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, 820, -240)
    connect_expr(base_steel_albedo, "", albedo_after_grime, "A")
    connect_expr(slag_soot_color, "", albedo_after_grime, "B")
    connect_expr(grime_color_alpha, "", albedo_after_grime, "Alpha")

    chip_steel_color = make_const_color3(mat, 0.37, 0.40, 0.44, 820, -380)
    chip_color_alpha = create_expr(mat, unreal.MaterialExpressionMultiply, 820, -120)
    chip_color_alpha.set_editor_property("const_b", 0.48)
    connect_expr(chip_mask, "", chip_color_alpha, "A")

    albedo_after_chip = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, 1020, -240)
    connect_expr(albedo_after_grime, "", albedo_after_chip, "A")
    connect_expr(chip_steel_color, "", albedo_after_chip, "B")
    connect_expr(chip_color_alpha, "", albedo_after_chip, "Alpha")

    macro_soot_atten = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, 1020, -100)
    macro_soot_atten.set_editor_property("const_a", 1.0)
    macro_soot_atten.set_editor_property("const_b", 0.42)
    connect_expr(macro_soot_mask, "", macro_soot_atten, "Alpha")

    final_albedo = create_expr(mat, unreal.MaterialExpressionMultiply, 1240, -220)
    connect_expr(albedo_after_chip, "", final_albedo, "A")
    connect_expr(macro_soot_atten, "", final_albedo, "B")
    connect_prop(final_albedo, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # =========================================================================
    # 7. Crisp Tangent Normal Scaling + Cook-Safe EmissiveTint (default 0,0,0)
    # =========================================================================
    norm_xy_mask = create_expr(mat, unreal.MaterialExpressionComponentMask, -980, 760)
    norm_xy_mask.set_editor_property("r", True)
    norm_xy_mask.set_editor_property("g", True)
    norm_xy_mask.set_editor_property("b", False)
    norm_xy_mask.set_editor_property("a", False)
    connect_expr(tex_norm, "RGB", norm_xy_mask, "")

    norm_strength_param = make_scalar_param(mat, "NormalStrength", 1.35, -980, 860)
    norm_xy_scaled = create_expr(mat, unreal.MaterialExpressionMultiply, -720, 780)
    connect_expr(norm_xy_mask, "", norm_xy_scaled, "A")
    connect_expr(norm_strength_param, "", norm_xy_scaled, "B")

    norm_out = create_expr(mat, unreal.MaterialExpressionAppendVector, -460, 800)
    connect_expr(norm_xy_scaled, "", norm_out, "A")
    connect_expr(tex_norm, "B", norm_out, "B")
    connect_prop(norm_out, "", unreal.MaterialProperty.MP_NORMAL)

    emissive_tint_param = make_vector_param(mat, "EmissiveTint", 0.0, 0.0, 0.0, 1.0, 1020, 800)
    connect_prop(emissive_tint_param, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def populate_phase12d_molten_cut_cap_graph(mat: unreal.Material) -> None:
    """
    Phase 12D Object-Locked & GPU Time Thermal-Cooling M_MoltenCutCap_Master:
    - Safeguard 2: Uses TransformPosition(WorldPosition, World -> Local) on Scale3D=(1,1,1) RemainderMeshComponent
      and ASeveredChunkActor for exact 1:1 centimeter magma fissures with zero texture swimming on tumbling chunks.
    - Pure GPU Time thermal dissipation curve (0.00 ms CPU tick) via CutSpawnTimeSeconds (-100.0 default),
      CoolingDurationSeconds (3.85 default), and CoolingCurveEnabled (1.0 default):
      cools from incandescent white-gold plasma (>1.65 BloomThreshold) through rich molten orange into dark solidified
      slag crust with faint deep cherry-ember veins (<1.65 BloomThreshold).
    """
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("two_sided", False)
    mat.set_editor_property("used_with_static_lighting", False)
    mat.set_editor_property("has_pixel_animation", True)

    pos_src_world = resolve_enum(
        unreal.MaterialPositionTransformSource, "TRANSFORMPOSSOURCE_WORLD", "WORLD"
    )
    pos_dst_local = resolve_enum(
        unreal.MaterialPositionTransformSource, "TRANSFORMPOSSOURCE_LOCAL", "LOCAL"
    )

    # 1. Rigid Object-Locked 3D Centimeter Position (zero swimming on tumbling Chaos chunks)
    world_pos = create_expr(mat, unreal.MaterialExpressionWorldPosition, -1800, -180)
    local_pos_cm = create_expr(mat, unreal.MaterialExpressionTransformPosition, -1560, -180)
    local_pos_cm.set_editor_property("transform_source_type", pos_src_world)
    local_pos_cm.set_editor_property("transform_type", pos_dst_local)
    connect_expr(world_pos, "", local_pos_cm, "")

    fine_offset = make_const_color3(mat, 19.0, 47.0, 11.0, -1560, -40)
    local_pos_fine = create_expr(mat, unreal.MaterialExpressionAdd, -1360, -80)
    connect_expr(local_pos_cm, "", local_pos_fine, "A")
    connect_expr(fine_offset, "", local_pos_fine, "B")

    noise_coarse = make_gradient_noise(
        mat, local_pos_cm, scale=0.14, levels=3, out_min=0.0, out_max=1.0, pos_x=-1180, pos_y=-220
    )
    noise_fine = make_gradient_noise(
        mat, local_pos_fine, scale=0.34, levels=2, out_min=0.0, out_max=1.0, pos_x=-1180, pos_y=-60
    )

    magma_mix = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -960, -140)
    magma_mix.set_editor_property("const_alpha", 0.35)
    connect_expr(noise_coarse, "", magma_mix, "A")
    connect_expr(noise_fine, "", magma_mix, "B")

    magma_safe = create_expr(mat, unreal.MaterialExpressionClamp, -800, -140)
    magma_safe.set_editor_property("min_default", 0.001)
    magma_safe.set_editor_property("max_default", 1.0)
    connect_expr(magma_mix, "", magma_safe, "")

    fissure_mask = create_expr(mat, unreal.MaterialExpressionPower, -620, -140)
    fissure_mask.set_editor_property("const_exponent", 1.45)
    connect_expr(magma_safe, "", fissure_mask, "Base")

    # 2. GPU Time Blackbody Thermal Cooling Parameterization
    time_node = create_expr(mat, unreal.MaterialExpressionTime, -1800, 260)
    spawn_time_param = make_scalar_param(mat, "CutSpawnTimeSeconds", -100.0, -1800, 380)
    cool_dur_param = make_scalar_param(mat, "CoolingDurationSeconds", 3.85, -1800, 500)
    cool_enabled_param = make_scalar_param(mat, "CoolingCurveEnabled", 1.0, -1560, 600)
    cool_enabled_01 = make_clamp01(mat, cool_enabled_param, "", -1360, 600)

    elapsed_raw = create_expr(mat, unreal.MaterialExpressionSubtract, -1560, 320)
    connect_expr(time_node, "", elapsed_raw, "A")
    connect_expr(spawn_time_param, "", elapsed_raw, "B")

    elapsed_pos = create_expr(mat, unreal.MaterialExpressionMax, -1380, 320)
    elapsed_pos.set_editor_property("const_b", 0.0)
    connect_expr(elapsed_raw, "", elapsed_pos, "A")

    safe_cool_dur = create_expr(mat, unreal.MaterialExpressionMax, -1560, 500)
    safe_cool_dur.set_editor_property("const_b", 0.25)
    connect_expr(cool_dur_param, "", safe_cool_dur, "A")

    tau_unclamped = create_expr(mat, unreal.MaterialExpressionDivide, -1200, 380)
    connect_expr(elapsed_pos, "", tau_unclamped, "A")
    connect_expr(safe_cool_dur, "", tau_unclamped, "B")

    tau_01 = make_clamp01(mat, tau_unclamped, "", -1040, 380)
    tau = create_expr(mat, unreal.MaterialExpressionMultiply, -880, 440)
    connect_expr(tau_01, "", tau, "A")
    connect_expr(cool_enabled_01, "", tau, "B")

    one_minus_tau = create_expr(mat, unreal.MaterialExpressionOneMinus, -720, 440)
    connect_expr(tau, "", one_minus_tau, "")
    heat_safe = create_expr(mat, unreal.MaterialExpressionClamp, -580, 440)
    heat_safe.set_editor_property("min_default", 0.0001)
    heat_safe.set_editor_property("max_default", 1.0)
    connect_expr(one_minus_tau, "", heat_safe, "")

    # Fast incandescent plasma flash envelope vs slower deep-fissure orange-crimson ember decay
    heat_flash = create_expr(mat, unreal.MaterialExpressionPower, -400, 360)
    heat_flash.set_editor_property("const_exponent", 2.20)
    connect_expr(heat_safe, "", heat_flash, "Base")

    heat_ember = create_expr(mat, unreal.MaterialExpressionPower, -400, 500)
    heat_ember.set_editor_property("const_exponent", 0.85)
    connect_expr(heat_safe, "", heat_ember, "Base")

    # 3. Solidifying Slag BaseColor, Roughness & Metallic
    cooled_crust_dark = make_const_color3(mat, 0.018, 0.016, 0.017, -620, -480)
    cooled_crust_vein = make_const_color3(mat, 0.038, 0.022, 0.018, -620, -360)
    cooled_albedo = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -360, -420)
    connect_expr(cooled_crust_dark, "", cooled_albedo, "A")
    connect_expr(cooled_crust_vein, "", cooled_albedo, "B")
    connect_expr(fissure_mask, "", cooled_albedo, "Alpha")

    molten_albedo = make_const_color3(mat, 0.055, 0.028, 0.018, -360, -300)
    final_base_color = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -120, -360)
    connect_expr(cooled_albedo, "", final_base_color, "A")
    connect_expr(molten_albedo, "", final_base_color, "B")
    connect_expr(heat_ember, "", final_base_color, "Alpha")
    connect_prop(final_base_color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    cooled_rough = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -360, -180)
    cooled_rough.set_editor_property("const_a", 0.78)
    cooled_rough.set_editor_property("const_b", 0.50)
    connect_expr(fissure_mask, "", cooled_rough, "Alpha")

    final_rough = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -120, -180)
    final_rough.set_editor_property("const_b", 0.28)
    connect_expr(cooled_rough, "", final_rough, "A")
    connect_expr(heat_flash, "", final_rough, "Alpha")
    connect_prop(final_rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metal_const = make_const_scalar(mat, 0.30, -120, -60)
    connect_prop(metal_const, "", unreal.MaterialProperty.MP_METALLIC)

    # 4. 3-Stage Blackbody Emissive Curve (Fresh White-Gold -> Molten Orange -> Cooled Dark Cherry-Ember Veins)
    hot_crust_col = make_const_color3(mat, 1.35, 0.20, 0.018, -620, 40)
    hot_fissure_col = make_const_color3(mat, 5.60, 2.35, 0.42, -620, 140)
    fresh_hot_emissive = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -360, 80)
    connect_expr(hot_crust_col, "", fresh_hot_emissive, "A")
    connect_expr(hot_fissure_col, "", fresh_hot_emissive, "B")
    connect_expr(fissure_mask, "", fresh_hot_emissive, "Alpha")

    mid_crust_col = make_const_color3(mat, 0.14, 0.014, 0.002, -620, 240)
    mid_fissure_col = make_const_color3(mat, 2.85, 0.48, 0.032, -620, 320)
    mid_ember_emissive = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -360, 240)
    connect_expr(mid_crust_col, "", mid_ember_emissive, "A")
    connect_expr(mid_fissure_col, "", mid_ember_emissive, "B")
    connect_expr(fissure_mask, "", mid_ember_emissive, "Alpha")

    cooled_crust_em = make_const_color3(mat, 0.008, 0.001, 0.000, -360, 620)
    cooled_vein_em = make_const_color3(mat, 0.42, 0.032, 0.004, -360, 720)
    cooled_residual_emissive = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -120, 660)
    connect_expr(cooled_crust_em, "", cooled_residual_emissive, "A")
    connect_expr(cooled_vein_em, "", cooled_residual_emissive, "B")
    connect_expr(fissure_mask, "", cooled_residual_emissive, "Alpha")

    cooled_to_mid = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -80, 360)
    connect_expr(cooled_residual_emissive, "", cooled_to_mid, "A")
    connect_expr(mid_ember_emissive, "", cooled_to_mid, "B")
    connect_expr(heat_ember, "", cooled_to_mid, "Alpha")

    thermal_emissive = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, 140, 220)
    connect_expr(cooled_to_mid, "", thermal_emissive, "A")
    connect_expr(fresh_hot_emissive, "", thermal_emissive, "B")
    connect_expr(heat_flash, "", thermal_emissive, "Alpha")

    # Subtle 3.2s magma breathing modulation
    sine_node = create_expr(mat, unreal.MaterialExpressionSine, -1560, 140)
    sine_node.set_editor_property("period", 3.2)
    connect_expr(time_node, "", sine_node, "")

    breath_amp = create_expr(mat, unreal.MaterialExpressionMultiply, -1380, 140)
    breath_amp.set_editor_property("const_b", 0.08)
    connect_expr(sine_node, "", breath_amp, "A")

    breath_factor = create_expr(mat, unreal.MaterialExpressionAdd, -1200, 140)
    breath_factor.set_editor_property("const_b", 1.00)
    connect_expr(breath_amp, "", breath_factor, "A")

    emissive_out = create_expr(mat, unreal.MaterialExpressionMultiply, 340, 220)
    connect_expr(thermal_emissive, "", emissive_out, "A")
    connect_expr(breath_factor, "", emissive_out, "B")
    connect_prop(emissive_out, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def compile_and_save_master_material(
    asset_path: str,
    enable_instanced_static_mesh: bool,
) -> Tuple[int, int]:
    if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        raise RuntimeError(f"Missing target material asset: {asset_path}")

    loaded = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not isinstance(loaded, unreal.Material):
        raise RuntimeError(f"Asset {asset_path} is {type(loaded)}, expected unreal.Material")

    mat: unreal.Material = loaded
    unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)

    if asset_path == STATION_PBR_MASTER_PATH:
        diff_tex, orm_tex, norm_tex = load_clamp_jaw_default_textures()
        populate_phase12d_station_pbr_graph(mat, diff_tex, orm_tex, norm_tex)
    elif asset_path == MOLTEN_CUT_CAP_MASTER_PATH:
        populate_phase12d_molten_cut_cap_graph(mat)
    else:
        raise RuntimeError(f"Unsupported master material path: {asset_path}")

    mat.set_editor_property("used_with_static_lighting", False)
    unreal.MaterialEditingLibrary.set_base_material_usage(
        mat, unreal.MaterialUsage.MATUSAGE_STATIC_MESH, True
    )
    unreal.MaterialEditingLibrary.set_base_material_usage(
        mat, unreal.MaterialUsage.MATUSAGE_NANITE, True
    )
    if enable_instanced_static_mesh:
        unreal.MaterialEditingLibrary.set_base_material_usage(
            mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, True
        )

    unreal.MaterialEditingLibrary.layout_material_expressions(mat)
    compile_errors = unreal.MaterialEditingLibrary.recompile_material(mat)
    if compile_errors:
        raise RuntimeError(f"Shader compilation errors in {mat.get_name()}: {list(compile_errors)}")

    stats = unreal.MaterialEditingLibrary.get_statistics(mat)
    ps_instr = int(getattr(stats, "num_pixel_shader_instructions", 0))
    vs_instr = int(getattr(stats, "num_vertex_shader_instructions", 0))
    if ps_instr <= 0:
        raise RuntimeError(f"{mat.get_name()} compiled with 0 pixel shader instructions!")

    if not unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False):
        raise RuntimeError(f"Failed saving {asset_path}")

    log_info(
        f"PHASE12D_SHADER_OK path={asset_path} ps_instructions={ps_instr} vs_instructions={vs_instr}"
    )
    return ps_instr, vs_instr


def main() -> None:
    compile_and_save_master_material(STATION_PBR_MASTER_PATH, enable_instanced_static_mesh=True)
    compile_and_save_master_material(MOLTEN_CUT_CAP_MASTER_PATH, enable_instanced_static_mesh=False)


if __name__ == "__main__":
    main()
