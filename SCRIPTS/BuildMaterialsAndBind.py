from typing import Dict, List, Tuple
import unreal


MATERIALS_DIR = "/Game/Presentation/Materials"
CONTRACTS_DIR = "/Game/Presentation/Contracts"
MESHES_DIR = "/Game/Presentation/Meshes"
VFX_DIR = "/Game/Presentation/VFX"
DA_PRESENTATION_PATH = "/Game/Presentation/DA_Presentation"

CONTRACT_DIRECTIVES: List[Tuple[str, str]] = [
    ("DA_Contract_01", "CUT 12.0 KG TO TITHE | STAMP [E]"),
    ("DA_Contract_02", "CUT GREEN YIELD | [SPACE] DUMPS TARE"),
    ("DA_Contract_03", "HIDDEN VEIN: AUDIO SCAN & BEVEL [RMB]"),
    ("DA_Contract_04", "SLAG COLUMN: DODGE VEIN | WATCH TARE"),
    ("DA_Contract_05", "TORQUE LIMIT: KEEP CUTS BALANCED"),
    ("DA_Contract_06", "DIRECTOR'S INGOT: DUAL VEINS + TORQUE"),
]


def log_info(msg: str) -> None:
    unreal.log(f"[BuildMaterialsAndBind] {msg}")


def log_error(msg: str) -> None:
    unreal.log_error(f"[BuildMaterialsAndBind] {msg}")


def get_or_create_material(asset_name: str) -> unreal.Material:
    if not unreal.EditorAssetLibrary.does_directory_exist(MATERIALS_DIR):
        unreal.EditorAssetLibrary.make_directory(MATERIALS_DIR)

    asset_path = f"{MATERIALS_DIR}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        loaded = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not isinstance(loaded, unreal.Material):
            raise RuntimeError(f"Existing asset at {asset_path} is {type(loaded)}, expected unreal.Material")
        mat = loaded
    else:
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        factory = unreal.MaterialFactoryNew()
        mat = asset_tools.create_asset(asset_name, MATERIALS_DIR, unreal.Material, factory)
        if mat is None:
            raise RuntimeError(f"Failed to create material asset {asset_path}")

    unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
    return mat


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
        # Fallback if single-input expression names its pin "Input" vs ""
        if to_pin == "" and len(in_names) == 1:
            ok = unreal.MaterialEditingLibrary.connect_material_expressions(from_expr, from_pin, to_expr, str(in_names[0]))
        if not ok:
            raise RuntimeError(
                f"Failed connecting {from_expr.get_class().get_name()}[{from_pin!r}] (avail={out_names}) -> "
                f"{to_expr.get_class().get_name()}[{to_pin!r}] (avail={in_names})"
            )


def connect_prop(from_expr, from_pin: str, mat_prop: unreal.MaterialProperty) -> None:
    ok = unreal.MaterialEditingLibrary.connect_material_property(from_expr, from_pin, mat_prop)
    if not ok:
        out_names = unreal.MaterialEditingLibrary.get_material_expression_output_names(from_expr)
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


def make_local_noise_01(mat: unreal.Material, scale: float, pos_x: int = -900, pos_y: int = 0):
    local_pos = create_expr(mat, unreal.MaterialExpressionLocalPosition, pos_x, pos_y)
    noise = create_expr(mat, unreal.MaterialExpressionNoise, pos_x + 240, pos_y)
    noise.set_editor_property("scale", float(scale))
    noise.set_editor_property("noise_function", unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU)
    noise.set_editor_property("quality", 1)
    noise.set_editor_property("levels", 3)
    noise.set_editor_property("turbulence", False)
    noise.set_editor_property("output_min", 0.0)
    noise.set_editor_property("output_max", 1.0)
    connect_expr(local_pos, "XYZ", noise, "")

    clamp_node = create_expr(mat, unreal.MaterialExpressionClamp, pos_x + 480, pos_y)
    clamp_node.set_editor_property("min_default", 0.0)
    clamp_node.set_editor_property("max_default", 1.0)
    connect_expr(noise, "", clamp_node, "")
    return clamp_node


def make_uv0_pos3(
    mat: unreal.Material,
    tiling_uv: float = 100.0,
    z_slice: float = 0.0,
    pos_x: int = -1600,
    pos_y: int = 0,
):
    tex_coord = create_expr(mat, unreal.MaterialExpressionTextureCoordinate, pos_x, pos_y)
    tex_coord.set_editor_property("coordinate_index", 0)
    tex_coord.set_editor_property("u_tiling", float(tiling_uv))
    tex_coord.set_editor_property("v_tiling", float(tiling_uv))

    z_const = make_const_scalar(mat, z_slice, pos_x, pos_y + 110)
    uv3 = create_expr(mat, unreal.MaterialExpressionAppendVector, pos_x + 220, pos_y + 30)
    connect_expr(tex_coord, "", uv3, "A")
    connect_expr(z_const, "", uv3, "B")
    return tex_coord, uv3


def make_noise_from_pos3(
    mat: unreal.Material,
    pos3_expr,
    scale: float,
    out_min: float,
    out_max: float,
    levels: int = 3,
    pos_x: int = -1100,
    pos_y: int = 0,
):
    noise = create_expr(mat, unreal.MaterialExpressionNoise, pos_x, pos_y)
    noise.set_editor_property("scale", float(scale))
    noise.set_editor_property("noise_function", unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU)
    noise.set_editor_property("quality", 1)
    noise.set_editor_property("levels", int(levels))
    noise.set_editor_property("turbulence", False)
    noise.set_editor_property("output_min", float(out_min))
    noise.set_editor_property("output_max", float(out_max))
    connect_expr(pos3_expr, "", noise, "")
    return noise


def make_ridged_vein_mask(
    mat: unreal.Material,
    pos3_expr,
    scale: float,
    sharpness_exp: float,
    pos_x: int = -960,
    pos_y: int = 0,
):
    # Signed gradient noise [-1, +1] -> 1 - clamp(|n|, 0, 1) -> sharp ridge seam along zero-crossings
    signed_noise = make_noise_from_pos3(mat, pos3_expr, scale=scale, out_min=-1.0, out_max=1.0, levels=3, pos_x=pos_x, pos_y=pos_y)
    abs_node = create_expr(mat, unreal.MaterialExpressionAbs, pos_x + 210, pos_y)
    connect_expr(signed_noise, "", abs_node, "")

    clamp_node = create_expr(mat, unreal.MaterialExpressionClamp, pos_x + 360, pos_y)
    clamp_node.set_editor_property("min_default", 0.0)
    clamp_node.set_editor_property("max_default", 1.0)
    connect_expr(abs_node, "", clamp_node, "")

    ridge_node = create_expr(mat, unreal.MaterialExpressionOneMinus, pos_x + 520, pos_y)
    connect_expr(clamp_node, "", ridge_node, "")

    pow_node = create_expr(mat, unreal.MaterialExpressionPower, pos_x + 680, pos_y)
    pow_node.set_editor_property("const_exponent", float(sharpness_exp))
    connect_expr(ridge_node, "", pow_node, "Base")
    return ridge_node, pow_node


def finalize_and_save_material(mat: unreal.Material, is_opaque_nanite_compatible: bool) -> None:
    unreal.MaterialEditingLibrary.set_base_material_usage(
        mat, unreal.MaterialUsage.MATUSAGE_STATIC_MESH, True
    )
    if is_opaque_nanite_compatible:
        unreal.MaterialEditingLibrary.set_base_material_usage(
            mat, unreal.MaterialUsage.MATUSAGE_NANITE, True
        )

    unreal.MaterialEditingLibrary.layout_material_expressions(mat)
    compile_errors = unreal.MaterialEditingLibrary.recompile_material(mat)
    if compile_errors:
        raise RuntimeError(f"Shader compilation errors in {mat.get_name()}: {list(compile_errors)}")

    if not unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False):
        raise RuntimeError(f"Failed saving material asset {mat.get_path_name()}")

    log_info(f"Compiled & saved {mat.get_path_name()}")


def build_laser_sheet_master() -> unreal.Material:
    mat = get_or_create_material("M_LaserSheet_Master")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)

    emissive_param = create_expr(mat, unreal.MaterialExpressionVectorParameter, -420, -120)
    emissive_param.set_editor_property("parameter_name", "EmissiveColor")
    emissive_param.set_editor_property("default_value", unreal.LinearColor(0.08, 0.88, 1.0, 1.0))
    connect_prop(emissive_param, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    opacity_param = create_expr(mat, unreal.MaterialExpressionScalarParameter, -420, 140)
    opacity_param.set_editor_property("parameter_name", "Opacity")
    opacity_param.set_editor_property("default_value", 0.28)
    connect_prop(opacity_param, "", unreal.MaterialProperty.MP_OPACITY)

    finalize_and_save_material(mat, is_opaque_nanite_compatible=False)
    return mat


def build_molten_cut_cap_master() -> unreal.Material:
    mat = get_or_create_material("M_MoltenCutCap_Master")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("two_sided", False)
    mat.set_editor_property("has_pixel_animation", True)

    crust_base = make_const_color3(mat, 0.025, 0.018, 0.016, -320, -260)
    rough_const = make_const_scalar(mat, 0.68, -320, -120)
    metal_const = make_const_scalar(mat, 0.25, -320, -20)

    connect_prop(crust_base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    connect_prop(rough_const, "", unreal.MaterialProperty.MP_ROUGHNESS)
    connect_prop(metal_const, "", unreal.MaterialProperty.MP_METALLIC)

    noise_alpha = make_local_noise_01(mat, 0.16, -1080, 200)
    ember_low = make_const_color3(mat, 0.85, 0.07, 0.01, -600, 80)
    ember_high = make_const_color3(mat, 3.4, 0.72, 0.04, -600, 220)

    ember_lerp = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -340, 160)
    connect_expr(ember_low, "", ember_lerp, "A")
    connect_expr(ember_high, "", ember_lerp, "B")
    connect_expr(noise_alpha, "", ember_lerp, "Alpha")

    # Subtle breathing pulse in [0.90, 1.10]: 1.0 + 0.10 * sin(Time / Period * 2pi)
    time_node = create_expr(mat, unreal.MaterialExpressionTime, -860, 420)
    sine_node = create_expr(mat, unreal.MaterialExpressionSine, -680, 420)
    sine_node.set_editor_property("period", 3.2)
    connect_expr(time_node, "", sine_node, "")

    breath_amp = create_expr(mat, unreal.MaterialExpressionMultiply, -500, 420)
    breath_amp.set_editor_property("const_b", 0.10)
    connect_expr(sine_node, "", breath_amp, "A")

    breath_factor = create_expr(mat, unreal.MaterialExpressionAdd, -340, 420)
    breath_factor.set_editor_property("const_b", 1.00)
    connect_expr(breath_amp, "", breath_factor, "A")

    emissive_out = create_expr(mat, unreal.MaterialExpressionMultiply, -140, 260)
    connect_expr(ember_lerp, "", emissive_out, "A")
    connect_expr(breath_factor, "", emissive_out, "B")

    connect_prop(emissive_out, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    finalize_and_save_material(mat, is_opaque_nanite_compatible=True)
    return mat


def build_slag_outer_master() -> unreal.Material:
    """
    Phase 9.4 Seamless UV0 Crimson-Oxide Ore Shader:
    Uses ONLY baked mesh TexCoord[0] (UV0) — NEVER LocalPosition — so severed chunks
    re-centering their pivot on cut NEVER jump, slide, or pop procedural veins.
    """
    mat = get_or_create_material("M_SlagOuter_Master")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("two_sided", False)

    # 1) Baked UV0 scaled into 100-cm box-projected space (1 unit = 1 cm on monolith faces)
    tex_uv_cm, uv_pos3 = make_uv0_pos3(mat, tiling_uv=100.0, z_slice=11.3, pos_x=-2100, pos_y=0)

    # 2) Organic domain warp in UV0 space so crimson-iron veins meander naturally across -X/+X/+-Y faces
    warp_offset_b = make_const_color3(mat, 37.0, 91.0, 19.0, -1840, 220)
    uv_pos3_b = create_expr(mat, unreal.MaterialExpressionAdd, -1640, 180)
    connect_expr(uv_pos3, "", uv_pos3_b, "A")
    connect_expr(warp_offset_b, "", uv_pos3_b, "B")

    warp_u = make_noise_from_pos3(mat, uv_pos3, scale=0.055, out_min=-14.0, out_max=14.0, levels=3, pos_x=-1440, pos_y=80)
    warp_v = make_noise_from_pos3(mat, uv_pos3_b, scale=0.062, out_min=-14.0, out_max=14.0, levels=3, pos_x=-1440, pos_y=240)
    warp_uv2 = create_expr(mat, unreal.MaterialExpressionAppendVector, -1200, 150)
    connect_expr(warp_u, "", warp_uv2, "A")
    connect_expr(warp_v, "", warp_uv2, "B")

    zero_z = make_const_scalar(mat, 0.0, -1200, 280)
    warp_vec3 = create_expr(mat, unreal.MaterialExpressionAppendVector, -1020, 180)
    connect_expr(warp_uv2, "", warp_vec3, "A")
    connect_expr(zero_z, "", warp_vec3, "B")

    warped_pos3 = create_expr(mat, unreal.MaterialExpressionAdd, -840, 80)
    connect_expr(uv_pos3, "", warped_pos3, "A")
    connect_expr(warp_vec3, "", warped_pos3, "B")

    # 3) Horizontal sedimentary strata along V (G channel of UV0) + fine basalt mineral grain
    v_coord = create_expr(mat, unreal.MaterialExpressionComponentMask, -1640, -340)
    v_coord.set_editor_property("r", False)
    v_coord.set_editor_property("g", True)
    v_coord.set_editor_property("b", False)
    v_coord.set_editor_property("a", False)
    connect_expr(tex_uv_cm, "", v_coord, "")

    strata_warp = create_expr(mat, unreal.MaterialExpressionMultiply, -1420, -240)
    strata_warp.set_editor_property("const_b", 0.32)
    connect_expr(warp_u, "", strata_warp, "A")

    v_warped = create_expr(mat, unreal.MaterialExpressionAdd, -1220, -320)
    connect_expr(v_coord, "", v_warped, "A")
    connect_expr(strata_warp, "", v_warped, "B")

    strata_sine = create_expr(mat, unreal.MaterialExpressionSine, -1040, -320)
    strata_sine.set_editor_property("period", 14.5)
    connect_expr(v_warped, "", strata_sine, "")

    strata_half = create_expr(mat, unreal.MaterialExpressionMultiply, -860, -320)
    strata_half.set_editor_property("const_b", 0.5)
    connect_expr(strata_sine, "", strata_half, "A")

    strata_01 = create_expr(mat, unreal.MaterialExpressionAdd, -700, -320)
    strata_01.set_editor_property("const_b", 0.5)
    connect_expr(strata_half, "", strata_01, "A")

    fine_grain = make_noise_from_pos3(mat, uv_pos3, scale=0.24, out_min=0.0, out_max=1.0, levels=2, pos_x=-1040, pos_y=-160)
    rock_blend = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -520, -260)
    rock_blend.set_editor_property("const_alpha", 0.42)
    connect_expr(strata_01, "", rock_blend, "A")
    connect_expr(fine_grain, "", rock_blend, "B")

    rock_clamp = create_expr(mat, unreal.MaterialExpressionClamp, -360, -260)
    rock_clamp.set_editor_property("min_default", 0.0)
    rock_clamp.set_editor_property("max_default", 1.0)
    connect_expr(rock_blend, "", rock_clamp, "")

    basalt_dark = make_const_color3(mat, 0.030, 0.033, 0.040, -560, -480)
    basalt_strata = make_const_color3(mat, 0.048, 0.052, 0.062, -560, -380)
    basalt_albedo = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -180, -380)
    connect_expr(basalt_dark, "", basalt_albedo, "A")
    connect_expr(basalt_strata, "", basalt_albedo, "B")
    connect_expr(rock_clamp, "", basalt_albedo, "Alpha")

    # 4) Primary + tributary sharp oxidized crimson-iron ore veins
    primary_ridge, primary_vein = make_ridged_vein_mask(
        mat, warped_pos3, scale=0.078, sharpness_exp=6.5, pos_x=-660, pos_y=40
    )

    trib_offset = make_const_color3(mat, 53.0, 29.0, 7.0, -840, 280)
    trib_pos3 = create_expr(mat, unreal.MaterialExpressionAdd, -680, 260)
    connect_expr(warped_pos3, "", trib_pos3, "A")
    connect_expr(trib_offset, "", trib_pos3, "B")

    _, trib_vein_raw = make_ridged_vein_mask(
        mat, trib_pos3, scale=0.145, sharpness_exp=9.0, pos_x=-520, pos_y=260
    )
    trib_vein = create_expr(mat, unreal.MaterialExpressionMultiply, 260, 260)
    trib_vein.set_editor_property("const_b", 0.68)
    connect_expr(trib_vein_raw, "", trib_vein, "A")

    combined_vein = create_expr(mat, unreal.MaterialExpressionMax, 420, 120)
    connect_expr(primary_vein, "", combined_vein, "A")
    connect_expr(trib_vein, "", combined_vein, "B")

    vein_mask = create_expr(mat, unreal.MaterialExpressionClamp, 580, 120)
    vein_mask.set_editor_property("min_default", 0.0)
    vein_mask.set_editor_property("max_default", 1.0)
    connect_expr(combined_vein, "", vein_mask, "")

    crimson_oxide = make_const_color3(mat, 0.46, 0.045, 0.015, 380, -160)
    final_base_color = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, 760, -180)
    connect_expr(basalt_albedo, "", final_base_color, "A")
    connect_expr(crimson_oxide, "", final_base_color, "B")
    connect_expr(vein_mask, "", final_base_color, "Alpha")
    connect_prop(final_base_color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # 5) Subtle deep-red mineral seam glow (0.22, 0.018, 0.004) strictly along inner vein fissure core
    seam_core = create_expr(mat, unreal.MaterialExpressionPower, 380, 420)
    seam_core.set_editor_property("const_exponent", 11.5)
    connect_expr(primary_ridge, "", seam_core, "Base")

    seam_glow_color = make_const_color3(mat, 0.22, 0.018, 0.004, 380, 560)
    seam_emissive = create_expr(mat, unreal.MaterialExpressionMultiply, 700, 480)
    connect_expr(seam_glow_color, "", seam_emissive, "A")
    connect_expr(seam_core, "", seam_emissive, "B")
    connect_prop(seam_emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    # 6) Basalt vs metallic-oxide roughness & metallic contrast
    rough_lerp = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, 760, 40)
    rough_lerp.set_editor_property("const_a", 0.82)
    rough_lerp.set_editor_property("const_b", 0.40)
    connect_expr(vein_mask, "", rough_lerp, "Alpha")
    connect_prop(rough_lerp, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metal_lerp = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, 760, 200)
    metal_lerp.set_editor_property("const_a", 0.22)
    metal_lerp.set_editor_property("const_b", 0.64)
    connect_expr(vein_mask, "", metal_lerp, "Alpha")
    connect_prop(metal_lerp, "", unreal.MaterialProperty.MP_METALLIC)

    finalize_and_save_material(mat, is_opaque_nanite_compatible=True)
    return mat


def build_dark_gunmetal_master() -> unreal.Material:
    """
    Dark industrial gunmetal PBR material (Nanite-compatible, UV0-driven) for
    SM_DiverterFlapper and chute splash guards so they never render pale flat grey.
    """
    mat = get_or_create_material("M_DarkGunmetal_Master")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("two_sided", False)

    _, uv_pos3 = make_uv0_pos3(mat, tiling_uv=40.0, z_slice=3.7, pos_x=-1100, pos_y=0)
    grain = make_noise_from_pos3(mat, uv_pos3, scale=0.35, out_min=0.0, out_max=1.0, levels=2, pos_x=-760, pos_y=0)
    grain_clamped = create_expr(mat, unreal.MaterialExpressionClamp, -540, 0)
    grain_clamped.set_editor_property("min_default", 0.0)
    grain_clamped.set_editor_property("max_default", 1.0)
    connect_expr(grain, "", grain_clamped, "")

    steel_dark = make_const_color3(mat, 0.018, 0.021, 0.026, -540, -220)
    steel_mid = make_const_color3(mat, 0.036, 0.041, 0.049, -540, -100)
    albedo_lerp = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -240, -140)
    connect_expr(steel_dark, "", albedo_lerp, "A")
    connect_expr(steel_mid, "", albedo_lerp, "B")
    connect_expr(grain_clamped, "", albedo_lerp, "Alpha")
    connect_prop(albedo_lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough_lerp = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -240, 60)
    rough_lerp.set_editor_property("const_a", 0.38)
    rough_lerp.set_editor_property("const_b", 0.54)
    connect_expr(grain_clamped, "", rough_lerp, "Alpha")
    connect_prop(rough_lerp, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metal_const = make_const_scalar(mat, 0.85, -240, 200)
    connect_prop(metal_const, "", unreal.MaterialProperty.MP_METALLIC)

    finalize_and_save_material(mat, is_opaque_nanite_compatible=True)
    return mat


def build_cell_wall_master() -> unreal.Material:
    mat = get_or_create_material("M_CellWall_Master")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("two_sided", False)

    noise_alpha = make_local_noise_01(mat, 0.035, -980, 0)
    color_a = make_const_color3(mat, 0.070, 0.076, 0.088, -520, -180)
    color_b = make_const_color3(mat, 0.115, 0.122, 0.138, -520, -40)

    base_lerp = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -240, -100)
    connect_expr(color_a, "", base_lerp, "A")
    connect_expr(color_b, "", base_lerp, "B")
    connect_expr(noise_alpha, "", base_lerp, "Alpha")
    connect_prop(base_lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough_const = make_const_scalar(mat, 0.90, -240, 120)
    metal_const = make_const_scalar(mat, 0.08, -240, 240)
    connect_prop(rough_const, "", unreal.MaterialProperty.MP_ROUGHNESS)
    connect_prop(metal_const, "", unreal.MaterialProperty.MP_METALLIC)

    finalize_and_save_material(mat, is_opaque_nanite_compatible=True)
    return mat


def build_cell_floor_master() -> unreal.Material:
    mat = get_or_create_material("M_CellFloor_Master")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("two_sided", False)

    noise_alpha = make_local_noise_01(mat, 0.06, -980, 0)
    color_a = make_const_color3(mat, 0.042, 0.046, 0.054, -520, -180)
    color_b = make_const_color3(mat, 0.078, 0.084, 0.096, -520, -40)

    base_lerp = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -240, -100)
    connect_expr(color_a, "", base_lerp, "A")
    connect_expr(color_b, "", base_lerp, "B")
    connect_expr(noise_alpha, "", base_lerp, "Alpha")
    connect_prop(base_lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough_const = make_const_scalar(mat, 0.76, -240, 120)
    metal_const = make_const_scalar(mat, 0.32, -240, 240)
    connect_prop(rough_const, "", unreal.MaterialProperty.MP_ROUGHNESS)
    connect_prop(metal_const, "", unreal.MaterialProperty.MP_METALLIC)

    finalize_and_save_material(mat, is_opaque_nanite_compatible=True)
    return mat


def bind_materials_to_presentation(
    m_laser: unreal.Material,
    m_slag: unreal.Material,
    m_molten: unreal.Material,
    m_wall: unreal.Material,
    m_floor: unreal.Material,
    m_gunmetal: unreal.Material,
) -> None:
    da_pres = unreal.EditorAssetLibrary.load_asset(DA_PRESENTATION_PATH)
    if da_pres is None:
        raise RuntimeError(f"Failed to load {DA_PRESENTATION_PATH}")

    loops_ms = da_pres.get_editor_property("continuous_loops_meta_sound")
    ambient_bed = da_pres.get_editor_property("ambient_cell_bed_sound")
    spatial_impulses = da_pres.get_editor_property("spatial_impulse_sounds")

    if loops_ms is None:
        raise AssertionError("DA_Presentation.continuous_loops_meta_sound is None!")
    if ambient_bed is None:
        raise AssertionError("DA_Presentation.ambient_cell_bed_sound is None!")
    if spatial_impulses is None or len(spatial_impulses) != 7:
        raise AssertionError(
            f"DA_Presentation.spatial_impulse_sounds expected 7 entries, got "
            f"{0 if spatial_impulses is None else len(spatial_impulses)}"
        )
    for impulse_id, sound_asset in spatial_impulses.items():
        if sound_asset is None:
            raise AssertionError(f"DA_Presentation.spatial_impulse_sounds[{impulse_id}] is None!")

    log_info(
        f"Verified Phase 8 audio bindings intact on DA_Presentation "
        f"(loops={loops_ms.get_name()}, ambient={ambient_bed.get_name()}, impulses={len(spatial_impulses)})"
    )

    da_pres.set_editor_property("laser_sheet_material", m_laser)
    da_pres.set_editor_property("default_monolith_outer_material", m_slag)
    da_pres.set_editor_property("default_molten_cut_cap_material", m_molten)
    da_pres.set_editor_property("cell_wall_material", m_wall)
    da_pres.set_editor_property("cell_floor_material", m_floor)

    flapper_slot = da_pres.get_editor_property("diverter_flapper_mesh_slot")
    flapper_slot.set_editor_property("material_overrides", [m_gunmetal])
    da_pres.set_editor_property("diverter_flapper_mesh_slot", flapper_slot)

    flapper_mesh_path = f"{MESHES_DIR}/SM_DiverterFlapper"
    if unreal.EditorAssetLibrary.does_asset_exist(flapper_mesh_path):
        flapper_sm = unreal.EditorAssetLibrary.load_asset(flapper_mesh_path)
        if isinstance(flapper_sm, unreal.StaticMesh):
            flapper_sm.set_material(0, m_gunmetal)
            unreal.EditorAssetLibrary.save_loaded_asset(flapper_sm, only_if_is_dirty=False)
            log_info(f"Bound M_DarkGunmetal_Master to {flapper_mesh_path} Slot 0")

    # Verify & enforce all 4 Niagara VFX slots on DA_Presentation (SliverVaporizedVFX -> NS_LaserSliceSparks)
    required_vfx_bindings: List[Tuple[str, str]] = [
        ("laser_slice_sparks_vfx", f"{VFX_DIR}/NS_LaserSliceSparks"),
        ("sliver_vaporized_vfx", f"{VFX_DIR}/NS_LaserSliceSparks"),
        ("vein_detonation_vfx", f"{VFX_DIR}/NS_VeinDetonation"),
        ("scale_heavy_impact_vfx", f"{VFX_DIR}/NS_ScaleHeavyImpact"),
    ]
    bound_vfx_summary: List[str] = []
    for vfx_prop, default_vfx_path in required_vfx_bindings:
        current_vfx = da_pres.get_editor_property(vfx_prop)
        if current_vfx is None:
            loaded_vfx = unreal.EditorAssetLibrary.load_asset(default_vfx_path)
            if loaded_vfx is None:
                raise AssertionError(
                    f"Missing required NiagaraSystem asset {default_vfx_path} for DA_Presentation.{vfx_prop}!"
                )
            da_pres.set_editor_property(vfx_prop, loaded_vfx)
            current_vfx = da_pres.get_editor_property(vfx_prop)
            log_info(f"Bound DA_Presentation.{vfx_prop} -> {default_vfx_path}")
        if current_vfx is None:
            raise AssertionError(f"DA_Presentation.{vfx_prop} is None after binding!")
        bound_vfx_summary.append(f"{vfx_prop}={current_vfx.get_name()}")

    log_info(f"Verified all 4 Niagara VFX slots on DA_Presentation ({', '.join(bound_vfx_summary)})")

    if not unreal.EditorAssetLibrary.save_loaded_asset(da_pres, only_if_is_dirty=False):
        raise RuntimeError(f"Failed saving {DA_PRESENTATION_PATH}")

    log_info(f"Bound all master materials, VFX slots + flapper dark gunmetal override and saved {DA_PRESENTATION_PATH}")


def update_contracts_and_monoliths(
    m_slag: unreal.Material,
    m_molten: unreal.Material,
) -> None:
    for contract_name, directive_str in CONTRACT_DIRECTIVES:
        contract_path = f"{CONTRACTS_DIR}/{contract_name}"
        if not unreal.EditorAssetLibrary.does_asset_exist(contract_path):
            raise RuntimeError(f"Missing contract DataAsset at {contract_path}")

        contract_asset = unreal.EditorAssetLibrary.load_asset(contract_path)
        if contract_asset is None:
            raise RuntimeError(f"Failed to load contract DataAsset at {contract_path}")

        contract_asset.set_editor_property("outer_shell_material", m_slag)
        contract_asset.set_editor_property("molten_cut_cap_material", m_molten)
        contract_asset.set_editor_property("contract_directive_text", directive_str)

        if not unreal.EditorAssetLibrary.save_loaded_asset(contract_asset, only_if_is_dirty=False):
            raise RuntimeError(f"Failed saving contract DataAsset {contract_path}")

        base_shape = contract_asset.get_editor_property("base_shape")
        custom_mesh = contract_asset.get_editor_property("custom_monolith_mesh")
        log_info(
            f"Updated & saved {contract_path}: shape={base_shape.name}, "
            f"custom_mesh={None if custom_mesh is None else custom_mesh.get_name()}, "
            f"directive={directive_str!r}"
        )

    for custom_mesh_name in ("SM_Monolith_Contract04", "SM_Monolith_Contract06"):
        mesh_path = f"{MESHES_DIR}/{custom_mesh_name}"
        if unreal.EditorAssetLibrary.does_asset_exist(mesh_path):
            sm = unreal.EditorAssetLibrary.load_asset(mesh_path)
            if isinstance(sm, unreal.StaticMesh):
                static_mats = [
                    unreal.StaticMaterial(m_slag, "OuterShell"),
                    unreal.StaticMaterial(m_molten, "MoltenCutCap"),
                ]
                sm.set_editor_property("static_materials", static_mats)
                if not unreal.EditorAssetLibrary.save_loaded_asset(sm, only_if_is_dirty=False):
                    raise RuntimeError(f"Failed saving {mesh_path}")
                log_info(f"Verified Slot 0 (M_SlagOuter_Master) & Slot 1 (M_MoltenCutCap_Master) on {mesh_path}")


def main() -> None:
    log_info("Starting Phase 9.5 master material generation, VFX assertions, contract directives & DA_Presentation binding...")
    m_laser = build_laser_sheet_master()
    m_molten = build_molten_cut_cap_master()
    m_slag = build_slag_outer_master()
    m_gunmetal = build_dark_gunmetal_master()
    m_wall = build_cell_wall_master()
    m_floor = build_cell_floor_master()
    bind_materials_to_presentation(
        m_laser=m_laser,
        m_slag=m_slag,
        m_molten=m_molten,
        m_wall=m_wall,
        m_floor=m_floor,
        m_gunmetal=m_gunmetal,
    )
    update_contracts_and_monoliths(m_slag=m_slag, m_molten=m_molten)
    log_info("Completed Phase 9.5 BuildMaterialsAndBind successfully.")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        log_error(f"FATAL: {exc}")
        raise
