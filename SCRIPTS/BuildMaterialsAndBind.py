import sys
import unreal


MATERIALS_DIR = "/Game/Presentation/Materials"
DA_PRESENTATION_PATH = "/Game/Presentation/DA_Presentation"


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
        out_names = unreal.MaterialEditingLibrary.get_material_expression_output_names(from_expr)
        in_names = unreal.MaterialEditingLibrary.get_material_expression_input_names(to_expr)
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
    mat = get_or_create_material("M_SlagOuter_Master")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("two_sided", False)

    noise_alpha = make_local_noise_01(mat, 0.11, -980, 0)
    color_a = make_const_color3(mat, 0.032, 0.036, 0.044, -520, -180)
    color_b = make_const_color3(mat, 0.095, 0.105, 0.122, -520, -40)

    base_lerp = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -240, -100)
    connect_expr(color_a, "", base_lerp, "A")
    connect_expr(color_b, "", base_lerp, "B")
    connect_expr(noise_alpha, "", base_lerp, "Alpha")
    connect_prop(base_lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough_lerp = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -240, 120)
    rough_lerp.set_editor_property("const_a", 0.64)
    rough_lerp.set_editor_property("const_b", 0.88)
    connect_expr(noise_alpha, "", rough_lerp, "Alpha")
    connect_prop(rough_lerp, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metal_const = make_const_scalar(mat, 0.38, -240, 260)
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

    if not unreal.EditorAssetLibrary.save_loaded_asset(da_pres, only_if_is_dirty=False):
        raise RuntimeError(f"Failed saving {DA_PRESENTATION_PATH}")

    log_info(f"Bound all 5 master materials and saved {DA_PRESENTATION_PATH}")


def main() -> None:
    log_info("Starting headless master material generation and DA_Presentation binding...")
    m_laser = build_laser_sheet_master()
    m_molten = build_molten_cut_cap_master()
    m_slag = build_slag_outer_master()
    m_wall = build_cell_wall_master()
    m_floor = build_cell_floor_master()
    bind_materials_to_presentation(
        m_laser=m_laser,
        m_slag=m_slag,
        m_molten=m_molten,
        m_wall=m_wall,
        m_floor=m_floor,
    )
    log_info("Completed BuildMaterialsAndBind successfully.")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        log_error(f"FATAL: {exc}")
        raise
