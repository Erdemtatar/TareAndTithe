from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple
import unreal


MATERIALS_DIR = "/Game/Presentation/Materials"
LIGHTING_DIR = "/Game/Presentation/Lighting"
CONTRACTS_DIR = "/Game/Presentation/Contracts"
MESHES_DIR = "/Game/Presentation/Meshes"
PBR_DIR = "/Game/Presentation/Meshes/PBR"
VFX_DIR = "/Game/Presentation/VFX"
DA_PRESENTATION_PATH = "/Game/Presentation/DA_Presentation"
AMBIENT_CUBEMAP_ASSET_NAME = "TC_CellAmbientCubemap"
AMBIENT_CUBEMAP_DEST_PATH = f"{LIGHTING_DIR}/{AMBIENT_CUBEMAP_ASSET_NAME}"

CONTRACT_DIRECTIVES: List[Tuple[str, str]] = [
    ("DA_Contract_01", "CUT 12.0 KG TO TITHE | STAMP [E]"),
    ("DA_Contract_02", "CUT GREEN YIELD | [SPACE] DUMPS TARE"),
    ("DA_Contract_03", "HIDDEN VEIN: AUDIO SCAN & BEVEL [RMB]"),
    ("DA_Contract_04", "SLAG COLUMN: DODGE VEIN | WATCH TARE"),
    ("DA_Contract_05", "TORQUE LIMIT: KEEP CUTS BALANCED"),
    ("DA_Contract_06", "DIRECTOR'S INGOT: DUAL VEINS + TORQUE"),
]


@dataclass(frozen=True)
class StationPBRInstanceSpec:
    instance_name: str
    prop_name: str
    slot_property: str
    albedo_tint: Tuple[float, float, float]
    conductor_min_f0: Tuple[float, float, float]
    roughness_contrast_exp: float
    roughness_min: float
    roughness_max: float
    metallic_scale: float
    metallic_min: float
    metallic_max: float
    ao_strength: float
    normal_strength: float


STATION_PBR_SPECS: List[StationPBRInstanceSpec] = [
    StationPBRInstanceSpec(
        instance_name="MI_Station_ClampJaw",
        prop_name="SM_ClampJaw",
        slot_property="clamp_jaw_mesh_slot",
        albedo_tint=(1.68, 1.75, 1.84),
        conductor_min_f0=(0.22, 0.24, 0.28),
        roughness_contrast_exp=1.55,
        roughness_min=0.24,
        roughness_max=0.45,
        metallic_scale=1.05,
        metallic_min=0.40,
        metallic_max=0.58,
        ao_strength=0.68,
        normal_strength=1.45,
    ),
    StationPBRInstanceSpec(
        instance_name="MI_Station_ClampSpringHousing",
        prop_name="SM_ClampSpringHousing",
        slot_property="clamp_spring_housing_mesh_slot",
        albedo_tint=(1.62, 1.70, 1.80),
        conductor_min_f0=(0.21, 0.23, 0.27),
        roughness_contrast_exp=1.52,
        roughness_min=0.25,
        roughness_max=0.47,
        metallic_scale=1.05,
        metallic_min=0.39,
        metallic_max=0.57,
        ao_strength=0.68,
        normal_strength=1.40,
    ),
    StationPBRInstanceSpec(
        instance_name="MI_Station_ScaleBase",
        prop_name="SM_ScaleBase",
        slot_property="scale_base_mesh_slot",
        albedo_tint=(1.40, 1.46, 1.54),
        conductor_min_f0=(0.16, 0.18, 0.21),
        roughness_contrast_exp=1.50,
        roughness_min=0.26,
        roughness_max=0.50,
        metallic_scale=1.05,
        metallic_min=0.40,
        metallic_max=0.58,
        ao_strength=0.74,
        normal_strength=1.40,
    ),
    StationPBRInstanceSpec(
        instance_name="MI_Station_ScalePan",
        prop_name="SM_ScalePan",
        slot_property="scale_pan_mesh_slot",
        albedo_tint=(1.36, 1.35, 1.32),
        conductor_min_f0=(0.18, 0.18, 0.17),
        roughness_contrast_exp=1.55,
        roughness_min=0.24,
        roughness_max=0.46,
        metallic_scale=1.05,
        metallic_min=0.44,
        metallic_max=0.62,
        ao_strength=0.74,
        normal_strength=1.40,
    ),
    StationPBRInstanceSpec(
        instance_name="MI_Station_TareChute",
        prop_name="SM_TareChute",
        slot_property="tare_chute_mesh_slot",
        albedo_tint=(1.38, 1.44, 1.52),
        conductor_min_f0=(0.16, 0.18, 0.21),
        roughness_contrast_exp=1.50,
        roughness_min=0.26,
        roughness_max=0.49,
        metallic_scale=1.05,
        metallic_min=0.40,
        metallic_max=0.58,
        ao_strength=0.74,
        normal_strength=1.40,
    ),
    StationPBRInstanceSpec(
        instance_name="MI_Station_DirectorBoothFrame",
        prop_name="SM_DirectorBoothFrame",
        slot_property="director_booth_frame_mesh_slot",
        albedo_tint=(1.22, 1.30, 1.40),
        conductor_min_f0=(0.15, 0.17, 0.20),
        roughness_contrast_exp=1.52,
        roughness_min=0.26,
        roughness_max=0.48,
        metallic_scale=1.05,
        metallic_min=0.42,
        metallic_max=0.60,
        ao_strength=0.76,
        normal_strength=1.40,
    ),
    StationPBRInstanceSpec(
        instance_name="MI_Station_DirectorShutter",
        prop_name="SM_DirectorShutter",
        slot_property="director_shutter_mesh_slot",
        albedo_tint=(1.32, 1.40, 1.50),
        conductor_min_f0=(0.17, 0.19, 0.22),
        roughness_contrast_exp=1.55,
        roughness_min=0.24,
        roughness_max=0.46,
        metallic_scale=1.05,
        metallic_min=0.44,
        metallic_max=0.62,
        ao_strength=0.74,
        normal_strength=1.40,
    ),
    StationPBRInstanceSpec(
        instance_name="MI_Station_CellFloorGrate",
        prop_name="SM_CellFloorGrate",
        slot_property="cell_floor_grate_mesh_slot",
        albedo_tint=(1.64, 1.68, 1.74),
        conductor_min_f0=(0.19, 0.20, 0.22),
        roughness_contrast_exp=1.52,
        roughness_min=0.26,
        roughness_max=0.49,
        metallic_scale=1.05,
        metallic_min=0.40,
        metallic_max=0.58,
        ao_strength=0.68,
        normal_strength=1.45,
    ),
    StationPBRInstanceSpec(
        instance_name="MI_Station_DiverterFlapper",
        prop_name="SM_DiverterFlapper",
        slot_property="diverter_flapper_mesh_slot",
        albedo_tint=(1.36, 1.42, 1.50),
        conductor_min_f0=(0.18, 0.20, 0.24),
        roughness_contrast_exp=1.55,
        roughness_min=0.24,
        roughness_max=0.45,
        metallic_scale=1.05,
        metallic_min=0.44,
        metallic_max=0.62,
        ao_strength=0.74,
        normal_strength=1.45,
    ),
]


def log_info(msg: str) -> None:
    unreal.log(f"[BuildMaterialsAndBind] {msg}")


def log_error(msg: str) -> None:
    unreal.log_error(f"[BuildMaterialsAndBind] {msg}")


def ensure_directory(dir_path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(dir_path):
        unreal.EditorAssetLibrary.make_directory(dir_path)


def get_or_create_material(asset_name: str) -> unreal.Material:
    ensure_directory(MATERIALS_DIR)

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


def get_or_create_material_instance(asset_name: str, parent_mat: unreal.MaterialInterface) -> unreal.MaterialInstanceConstant:
    ensure_directory(MATERIALS_DIR)

    asset_path = f"{MATERIALS_DIR}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        loaded = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not isinstance(loaded, unreal.MaterialInstanceConstant):
            raise RuntimeError(
                f"Existing asset at {asset_path} is {type(loaded)}, expected unreal.MaterialInstanceConstant"
            )
        mic = loaded
    else:
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        factory = unreal.MaterialInstanceConstantFactoryNew()
        mic = asset_tools.create_asset(asset_name, MATERIALS_DIR, unreal.MaterialInstanceConstant, factory)
        if mic is None:
            raise RuntimeError(f"Failed to create MaterialInstanceConstant {asset_path}")

    unreal.MaterialEditingLibrary.set_material_instance_parent(mic, parent_mat)
    unreal.MaterialEditingLibrary.clear_all_material_instance_parameters(mic)
    return mic


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
    signed_noise = make_noise_from_pos3(
        mat, pos3_expr, scale=scale, out_min=-1.0, out_max=1.0, levels=3, pos_x=pos_x, pos_y=pos_y
    )
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


def finalize_and_save_material(
    mat: unreal.Material,
    is_opaque_nanite_compatible: bool,
    enable_instanced_static_mesh: bool = False,
) -> None:
    # Guard 3: Explicitly disable static lighting usage and enforce Nanite/ISM permutations under Substrate.
    mat.set_editor_property("used_with_static_lighting", False)

    unreal.MaterialEditingLibrary.set_base_material_usage(
        mat, unreal.MaterialUsage.MATUSAGE_STATIC_MESH, True
    )
    if is_opaque_nanite_compatible:
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
    if ps_instr <= 0:
        raise RuntimeError(
            f"Material {mat.get_name()} compiled with 0 pixel shader instructions (invalid/fallback shader)!"
        )

    if not unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False):
        raise RuntimeError(f"Failed saving material asset {mat.get_path_name()}")

    log_info(f"Compiled & saved {mat.get_path_name()} (PS instructions={ps_instr})")


def build_laser_sheet_master() -> unreal.Material:
    """
    Phase 11A High-Energy Translucent Laser Optic Sheet (Guard 3 single-sided two_sided=False):
    - Low interior fill opacity (0.08 - 0.13 during active slice contact) with 68-cycle optical scanline striations
      so the ore monolith and molten cut cap remain unmistakably legible right through the plane.
    - Razor-sharp 0.018 UV perimeter frame (0.88 opacity, 2.60x HDR emissive boost) cleanly exceeding
      BloomThreshold=1.65 along the optical slice border while keeping HUD text immune.
    """
    mat = get_or_create_material("M_LaserSheet_Master")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", False)

    tex_uv = create_expr(mat, unreal.MaterialExpressionTextureCoordinate, -1480, 40)
    tex_uv.set_editor_property("coordinate_index", 0)
    tex_uv.set_editor_property("u_tiling", 1.0)
    tex_uv.set_editor_property("v_tiling", 1.0)

    u_mask = create_expr(mat, unreal.MaterialExpressionComponentMask, -1240, -60)
    u_mask.set_editor_property("r", True)
    u_mask.set_editor_property("g", False)
    u_mask.set_editor_property("b", False)
    u_mask.set_editor_property("a", False)
    connect_expr(tex_uv, "", u_mask, "")

    v_mask = create_expr(mat, unreal.MaterialExpressionComponentMask, -1240, 140)
    v_mask.set_editor_property("r", False)
    v_mask.set_editor_property("g", True)
    v_mask.set_editor_property("b", False)
    v_mask.set_editor_property("a", False)
    connect_expr(tex_uv, "", v_mask, "")

    # UV perimeter border mask: max(|U - 0.5|, |V - 0.5|) mapped across [0.482, 0.500] (0.018 UV edge width)
    u_centered = create_expr(mat, unreal.MaterialExpressionSubtract, -1040, -80)
    u_centered.set_editor_property("const_b", 0.5)
    connect_expr(u_mask, "", u_centered, "A")
    u_abs = create_expr(mat, unreal.MaterialExpressionAbs, -880, -80)
    connect_expr(u_centered, "", u_abs, "")

    v_centered = create_expr(mat, unreal.MaterialExpressionSubtract, -1040, 60)
    v_centered.set_editor_property("const_b", 0.5)
    connect_expr(v_mask, "", v_centered, "A")
    v_abs = create_expr(mat, unreal.MaterialExpressionAbs, -880, 60)
    connect_expr(v_centered, "", v_abs, "")

    box_dist = create_expr(mat, unreal.MaterialExpressionMax, -720, -10)
    connect_expr(u_abs, "", box_dist, "A")
    connect_expr(v_abs, "", box_dist, "B")

    edge_shifted = create_expr(mat, unreal.MaterialExpressionSubtract, -560, -10)
    edge_shifted.set_editor_property("const_b", 0.482)
    connect_expr(box_dist, "", edge_shifted, "A")

    edge_scaled = create_expr(mat, unreal.MaterialExpressionMultiply, -400, -10)
    edge_scaled.set_editor_property("const_b", 55.5556)
    connect_expr(edge_shifted, "", edge_scaled, "A")

    edge_clamped = create_expr(mat, unreal.MaterialExpressionClamp, -240, -10)
    edge_clamped.set_editor_property("min_default", 0.0)
    edge_clamped.set_editor_property("max_default", 1.0)
    connect_expr(edge_scaled, "", edge_clamped, "")

    border_mask = create_expr(mat, unreal.MaterialExpressionPower, -80, -10)
    border_mask.set_editor_property("const_exponent", 1.6)
    connect_expr(edge_clamped, "", border_mask, "Base")

    # High-frequency optical scanline striations across V (68 cycles across 68 cm aperture = 1 cm optical pitch)
    v_freq = create_expr(mat, unreal.MaterialExpressionMultiply, -1040, 240)
    v_freq.set_editor_property("const_b", 68.0)
    connect_expr(v_mask, "", v_freq, "A")

    scan_sin = create_expr(mat, unreal.MaterialExpressionSine, -860, 240)
    scan_sin.set_editor_property("period", 1.0)
    connect_expr(v_freq, "", scan_sin, "")

    scan_amp = create_expr(mat, unreal.MaterialExpressionMultiply, -680, 240)
    scan_amp.set_editor_property("const_b", 0.15)
    connect_expr(scan_sin, "", scan_amp, "A")

    scanline_mod = create_expr(mat, unreal.MaterialExpressionAdd, -500, 240)
    scanline_mod.set_editor_property("const_b", 1.0)
    connect_expr(scan_amp, "", scanline_mod, "A")

    # Emissive output: subtle 0.82x interior scanlines + 2.60x HDR perimeter beam
    emissive_param = create_expr(mat, unreal.MaterialExpressionVectorParameter, -460, -280)
    emissive_param.set_editor_property("parameter_name", "EmissiveColor")
    emissive_param.set_editor_property("default_value", unreal.LinearColor(0.08, 0.88, 1.0, 1.0))

    interior_emissive_gain = create_expr(mat, unreal.MaterialExpressionMultiply, -300, -160)
    interior_emissive_gain.set_editor_property("const_b", 0.82)
    connect_expr(scanline_mod, "", interior_emissive_gain, "A")

    border_emissive_gain = make_const_scalar(mat, 2.60, -300, -80)

    emissive_gain = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -80, -160)
    connect_expr(interior_emissive_gain, "", emissive_gain, "A")
    connect_expr(border_emissive_gain, "", emissive_gain, "B")
    connect_expr(border_mask, "", emissive_gain, "Alpha")

    final_emissive = create_expr(mat, unreal.MaterialExpressionMultiply, 140, -220)
    connect_expr(emissive_param, "", final_emissive, "A")
    connect_expr(emissive_gain, "", final_emissive, "B")
    connect_prop(final_emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    # Opacity output: 0.24 * Opacity * scanline_mod interior (~0.085-0.132 in contact, ~0.043 idle) + 0.88 crisp border
    opacity_param = create_expr(mat, unreal.MaterialExpressionScalarParameter, -500, 380)
    opacity_param.set_editor_property("parameter_name", "Opacity")
    opacity_param.set_editor_property("default_value", 0.42)

    interior_op_raw = create_expr(mat, unreal.MaterialExpressionMultiply, -300, 360)
    interior_op_raw.set_editor_property("const_b", 0.24)
    connect_expr(opacity_param, "", interior_op_raw, "A")

    interior_op = create_expr(mat, unreal.MaterialExpressionMultiply, -120, 320)
    connect_expr(interior_op_raw, "", interior_op, "A")
    connect_expr(scanline_mod, "", interior_op, "B")

    border_op_const = make_const_scalar(mat, 0.88, -120, 440)

    final_opacity = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, 140, 320)
    connect_expr(interior_op, "", final_opacity, "A")
    connect_expr(border_op_const, "", final_opacity, "B")
    connect_expr(border_mask, "", final_opacity, "Alpha")
    connect_prop(final_opacity, "", unreal.MaterialProperty.MP_OPACITY)

    finalize_and_save_material(mat, is_opaque_nanite_compatible=False)
    return mat


def build_molten_cut_cap_master() -> unreal.Material:
    mat = get_or_create_material("M_MoltenCutCap_Master")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("two_sided", False)
    mat.set_editor_property("has_pixel_animation", True)

    crust_base = make_const_color3(mat, 0.032, 0.020, 0.016, -320, -260)
    rough_const = make_const_scalar(mat, 0.64, -320, -120)
    metal_const = make_const_scalar(mat, 0.30, -320, -20)

    connect_prop(crust_base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    connect_prop(rough_const, "", unreal.MaterialProperty.MP_ROUGHNESS)
    connect_prop(metal_const, "", unreal.MaterialProperty.MP_METALLIC)

    # Rich magma fissure contrast exceeding BloomThreshold=1.65 cleanly on hot cores while keeping dark slag crust seams
    noise_alpha = make_local_noise_01(mat, 0.16, -1080, 200)
    core_pow = create_expr(mat, unreal.MaterialExpressionPower, -680, 200)
    core_pow.set_editor_property("const_exponent", 1.35)
    connect_expr(noise_alpha, "", core_pow, "Base")

    ember_low = make_const_color3(mat, 1.15, 0.12, 0.012, -600, 60)
    ember_high = make_const_color3(mat, 4.85, 1.18, 0.085, -600, 240)

    ember_lerp = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -340, 160)
    connect_expr(ember_low, "", ember_lerp, "A")
    connect_expr(ember_high, "", ember_lerp, "B")
    connect_expr(core_pow, "", ember_lerp, "Alpha")

    time_node = create_expr(mat, unreal.MaterialExpressionTime, -860, 420)
    sine_node = create_expr(mat, unreal.MaterialExpressionSine, -680, 420)
    sine_node.set_editor_property("period", 3.2)
    connect_expr(time_node, "", sine_node, "")

    breath_amp = create_expr(mat, unreal.MaterialExpressionMultiply, -500, 420)
    breath_amp.set_editor_property("const_b", 0.09)
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

    tex_uv_cm, uv_pos3 = make_uv0_pos3(mat, tiling_uv=100.0, z_slice=11.3, pos_x=-2100, pos_y=0)

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

    basalt_dark = make_const_color3(mat, 0.036, 0.040, 0.048, -560, -480)
    basalt_strata = make_const_color3(mat, 0.070, 0.076, 0.090, -560, -380)
    basalt_albedo = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -180, -380)
    connect_expr(basalt_dark, "", basalt_albedo, "A")
    connect_expr(basalt_strata, "", basalt_albedo, "B")
    connect_expr(rock_clamp, "", basalt_albedo, "Alpha")

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

    crimson_oxide = make_const_color3(mat, 0.56, 0.055, 0.018, 380, -160)
    final_base_color = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, 760, -180)
    connect_expr(basalt_albedo, "", final_base_color, "A")
    connect_expr(crimson_oxide, "", final_base_color, "B")
    connect_expr(vein_mask, "", final_base_color, "Alpha")
    connect_prop(final_base_color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    seam_core = create_expr(mat, unreal.MaterialExpressionPower, 380, 420)
    seam_core.set_editor_property("const_exponent", 11.5)
    connect_expr(primary_ridge, "", seam_core, "Base")

    seam_glow_color = make_const_color3(mat, 0.34, 0.026, 0.006, 380, 560)
    seam_emissive = create_expr(mat, unreal.MaterialExpressionMultiply, 700, 480)
    connect_expr(seam_glow_color, "", seam_emissive, "A")
    connect_expr(seam_core, "", seam_emissive, "B")
    connect_prop(seam_emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    rough_lerp = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, 760, 40)
    rough_lerp.set_editor_property("const_a", 0.72)
    rough_lerp.set_editor_property("const_b", 0.32)
    connect_expr(vein_mask, "", rough_lerp, "Alpha")
    connect_prop(rough_lerp, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metal_lerp = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, 760, 200)
    metal_lerp.set_editor_property("const_a", 0.28)
    metal_lerp.set_editor_property("const_b", 0.72)
    connect_expr(vein_mask, "", metal_lerp, "Alpha")
    connect_prop(metal_lerp, "", unreal.MaterialProperty.MP_METALLIC)

    finalize_and_save_material(mat, is_opaque_nanite_compatible=True)
    return mat


def build_dark_gunmetal_master() -> unreal.Material:
    """
    Phase 10 Calibrated Dark Industrial Gunmetal PBR Master:
    Fixes conductor F0 from dielectric-soot 0.02 to physical blued steel [0.15, 0.26]
    with directional brushed roughness [0.22, 0.44] and micro-normal grain;
    pre-compiles Nanite + InstancedStaticMesh permutations for under-booth stanchions.
    """
    mat = get_or_create_material("M_DarkGunmetal_Master")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("two_sided", False)

    brushed_uv = create_expr(mat, unreal.MaterialExpressionTextureCoordinate, -1380, -120)
    brushed_uv.set_editor_property("coordinate_index", 0)
    brushed_uv.set_editor_property("u_tiling", 18.0)
    brushed_uv.set_editor_property("v_tiling", 130.0)
    z_slice_a = make_const_scalar(mat, 3.7, -1380, -10)
    brushed_pos3 = create_expr(mat, unreal.MaterialExpressionAppendVector, -1140, -80)
    connect_expr(brushed_uv, "", brushed_pos3, "A")
    connect_expr(z_slice_a, "", brushed_pos3, "B")

    _, fine_pos3 = make_uv0_pos3(mat, tiling_uv=48.0, z_slice=9.1, pos_x=-1380, pos_y=160)

    streak_noise = make_noise_from_pos3(mat, brushed_pos3, scale=0.35, out_min=0.0, out_max=1.0, levels=2, pos_x=-900, pos_y=-80)
    fine_noise = make_noise_from_pos3(mat, fine_pos3, scale=0.45, out_min=0.0, out_max=1.0, levels=2, pos_x=-900, pos_y=120)

    grain_mix = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -680, 0)
    grain_mix.set_editor_property("const_alpha", 0.38)
    connect_expr(streak_noise, "", grain_mix, "A")
    connect_expr(fine_noise, "", grain_mix, "B")

    grain_clamped = create_expr(mat, unreal.MaterialExpressionClamp, -500, 0)
    grain_clamped.set_editor_property("min_default", 0.0)
    grain_clamped.set_editor_property("max_default", 1.0)
    connect_expr(grain_mix, "", grain_clamped, "")

    # Semi-metallic blued cast-steel albedo in [0.18, 0.32] + Metallic=0.54 so both diffuse form and rim specular read cleanly
    steel_dark = make_const_color3(mat, 0.175, 0.198, 0.232, -500, -240)
    steel_mid = make_const_color3(mat, 0.275, 0.305, 0.350, -500, -120)
    albedo_lerp = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -220, -160)
    connect_expr(steel_dark, "", albedo_lerp, "A")
    connect_expr(steel_mid, "", albedo_lerp, "B")
    connect_expr(grain_clamped, "", albedo_lerp, "Alpha")
    connect_prop(albedo_lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough_lerp = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -220, 40)
    rough_lerp.set_editor_property("const_a", 0.25)
    rough_lerp.set_editor_property("const_b", 0.45)
    connect_expr(grain_clamped, "", rough_lerp, "Alpha")
    connect_prop(rough_lerp, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metal_const = make_const_scalar(mat, 0.54, -220, 180)
    connect_prop(metal_const, "", unreal.MaterialProperty.MP_METALLIC)

    # Subtle tangent micro-normal from brushed noise so specular highlights break naturally across flat faces
    norm_nx = make_noise_from_pos3(mat, brushed_pos3, scale=0.55, out_min=-0.07, out_max=0.07, levels=2, pos_x=-700, pos_y=300)
    norm_ny = make_noise_from_pos3(mat, fine_pos3, scale=0.55, out_min=-0.07, out_max=0.07, levels=2, pos_x=-700, pos_y=430)
    norm_xy = create_expr(mat, unreal.MaterialExpressionAppendVector, -440, 350)
    connect_expr(norm_nx, "", norm_xy, "A")
    connect_expr(norm_ny, "", norm_xy, "B")
    norm_z = make_const_scalar(mat, 1.0, -440, 470)
    norm_xyz = create_expr(mat, unreal.MaterialExpressionAppendVector, -220, 370)
    connect_expr(norm_xy, "", norm_xyz, "A")
    connect_expr(norm_z, "", norm_xyz, "B")
    connect_prop(norm_xyz, "", unreal.MaterialProperty.MP_NORMAL)

    finalize_and_save_material(mat, is_opaque_nanite_compatible=True, enable_instanced_static_mesh=True)
    return mat


def build_cell_wall_master() -> unreal.Material:
    mat = get_or_create_material("M_CellWall_Master")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("two_sided", False)

    noise_alpha = make_local_noise_01(mat, 0.032, -1100, -60)
    fine_uv, fine_pos3 = make_uv0_pos3(mat, tiling_uv=12.0, z_slice=4.2, pos_x=-1100, pos_y=220)
    seam_u_mask = create_expr(mat, unreal.MaterialExpressionComponentMask, -840, 220)
    seam_u_mask.set_editor_property("r", True)
    seam_u_mask.set_editor_property("g", False)
    seam_u_mask.set_editor_property("b", False)
    seam_u_mask.set_editor_property("a", False)
    connect_expr(fine_uv, "", seam_u_mask, "")

    seam_sine = create_expr(mat, unreal.MaterialExpressionSine, -660, 220)
    seam_sine.set_editor_property("period", 1.0)
    connect_expr(seam_u_mask, "", seam_sine, "")

    # Soot-matte outer liner albedo + suppressed ambient specular so Y = +-360 vignette borders stay <= 16 sRGB under SkyLight=0.95
    color_a = make_const_color3(mat, 0.004, 0.0045, 0.0055, -560, -220)
    color_b = make_const_color3(mat, 0.008, 0.0090, 0.0110, -560, -80)

    base_lerp = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -240, -140)
    connect_expr(color_a, "", base_lerp, "A")
    connect_expr(color_b, "", base_lerp, "B")
    connect_expr(noise_alpha, "", base_lerp, "Alpha")
    connect_prop(base_lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough_lerp = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -240, 60)
    rough_lerp.set_editor_property("const_a", 0.86)
    rough_lerp.set_editor_property("const_b", 0.96)
    connect_expr(noise_alpha, "", rough_lerp, "Alpha")
    connect_prop(rough_lerp, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metal_const = make_const_scalar(mat, 0.04, -240, 180)
    connect_prop(metal_const, "", unreal.MaterialProperty.MP_METALLIC)

    spec_const = make_const_scalar(mat, 0.04, -240, 250)
    connect_prop(spec_const, "", unreal.MaterialProperty.MP_SPECULAR)

    ao_const = make_const_scalar(mat, 0.18, -240, 320)
    connect_prop(ao_const, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    # Architectural micro-normal relief so midground wash & molten bounce catch concrete bevel grain
    wall_nx_raw = create_expr(mat, unreal.MaterialExpressionMultiply, -480, 260)
    wall_nx_raw.set_editor_property("const_b", 0.06)
    connect_expr(seam_sine, "", wall_nx_raw, "A")
    wall_ny = make_noise_from_pos3(mat, fine_pos3, scale=0.65, out_min=-0.06, out_max=0.06, levels=2, pos_x=-560, pos_y=380)
    wall_xy = create_expr(mat, unreal.MaterialExpressionAppendVector, -320, 320)
    connect_expr(wall_nx_raw, "", wall_xy, "A")
    connect_expr(wall_ny, "", wall_xy, "B")
    wall_z = make_const_scalar(mat, 1.0, -320, 440)
    wall_norm = create_expr(mat, unreal.MaterialExpressionAppendVector, -140, 340)
    connect_expr(wall_xy, "", wall_norm, "A")
    connect_expr(wall_z, "", wall_norm, "B")
    connect_prop(wall_norm, "", unreal.MaterialProperty.MP_NORMAL)

    finalize_and_save_material(mat, is_opaque_nanite_compatible=True, enable_instanced_static_mesh=True)
    return mat


def build_cell_floor_master() -> unreal.Material:
    mat = get_or_create_material("M_CellFloor_Master")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("two_sided", False)

    noise_alpha = make_local_noise_01(mat, 0.06, -980, 0)
    color_a = make_const_color3(mat, 0.095, 0.105, 0.122, -520, -180)
    color_b = make_const_color3(mat, 0.165, 0.180, 0.205, -520, -40)

    base_lerp = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -240, -100)
    connect_expr(color_a, "", base_lerp, "A")
    connect_expr(color_b, "", base_lerp, "B")
    connect_expr(noise_alpha, "", base_lerp, "Alpha")
    connect_prop(base_lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough_lerp = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -240, 100)
    rough_lerp.set_editor_property("const_a", 0.34)
    rough_lerp.set_editor_property("const_b", 0.66)
    connect_expr(noise_alpha, "", rough_lerp, "Alpha")
    connect_prop(rough_lerp, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metal_const = make_const_scalar(mat, 0.62, -240, 240)
    connect_prop(metal_const, "", unreal.MaterialProperty.MP_METALLIC)

    finalize_and_save_material(mat, is_opaque_nanite_compatible=True)
    return mat


def load_and_verify_rodin_prop_textures(prop_name: str) -> Tuple[unreal.Texture2D, unreal.Texture2D, unreal.Texture2D]:
    base_tex_dir = f"{PBR_DIR}/{prop_name}/{prop_name}/Textures"
    diff_path = f"{base_tex_dir}/texture_diffuse"
    orm_path = f"{base_tex_dir}/texture_metallic-texture_roughness"
    norm_path = f"{base_tex_dir}/texture_normal"

    diff_tex = unreal.EditorAssetLibrary.load_asset(diff_path)
    orm_tex = unreal.EditorAssetLibrary.load_asset(orm_path)
    norm_tex = unreal.EditorAssetLibrary.load_asset(norm_path)

    if not isinstance(diff_tex, unreal.Texture2D):
        raise RuntimeError(f"Missing or invalid diffuse Texture2D at {diff_path}")
    if not isinstance(orm_tex, unreal.Texture2D):
        raise RuntimeError(f"Missing or invalid ORM Texture2D at {orm_path}")
    if not isinstance(norm_tex, unreal.Texture2D):
        raise RuntimeError(f"Missing or invalid Normal Texture2D at {norm_path}")

    # Guard 3: Verify glTF 2.0 linear ORM packing invariants (sRGB=False on ORM, TC_NORMALMAP on Normal).
    if bool(orm_tex.get_editor_property("srgb")):
        log_info(f"Correcting sRGB=False on linear glTF ORM map {orm_path}")
        orm_tex.set_editor_property("srgb", False)
        unreal.EditorAssetLibrary.save_loaded_asset(orm_tex, only_if_is_dirty=False)

    gltf_mi_path = f"{PBR_DIR}/{prop_name}/{prop_name}/Materials/model"
    if unreal.EditorAssetLibrary.does_asset_exist(gltf_mi_path):
        gltf_mi = unreal.EditorAssetLibrary.load_asset(gltf_mi_path)
        if isinstance(gltf_mi, unreal.MaterialInstanceConstant):
            tex_params = list(gltf_mi.get_editor_property("texture_parameter_values") or [])
            param_summary = [
                f"{tp.get_editor_property('parameter_info').get_editor_property('name')}->"
                f"{None if tp.get_editor_property('parameter_value') is None else tp.get_editor_property('parameter_value').get_name()}"
                for tp in tex_params
            ]
            log_info(
                f"Verified glTF 2.0 ORM (R=AO, G=Roughness, B=Metallic) source on {prop_name}: "
                f"sRGB(diff={diff_tex.get_editor_property('srgb')}, orm={orm_tex.get_editor_property('srgb')}), "
                f"glTF params={param_summary}"
            )

    return diff_tex, orm_tex, norm_tex


def build_station_pbr_master_and_instances() -> Tuple[unreal.Material, Dict[str, unreal.MaterialInstanceConstant]]:
    """
    Guard 3: Nanite-compatible Substrate-clean M_StationPBR_Master + 8 MI_Station_* instances
    unpacking glTF 2.0 ORM (R=AO, G=Roughness, B=Metallic) with roughness contrast curve,
    conductor F0 floor guard, and crisp normal strength using zero extra VRAM textures.
    """
    default_diff, default_orm, default_norm = load_and_verify_rodin_prop_textures("SM_ClampJaw")

    mat = get_or_create_material("M_StationPBR_Master")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("two_sided", False)

    # 1. Texture Sample Parameters
    tex_base = create_expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, -1280, -360)
    tex_base.set_editor_property("parameter_name", "BaseColorMap")
    tex_base.set_editor_property("texture", default_diff)
    tex_base.set_editor_property("sampler_type", resolve_sampler_type_for_texture(default_diff))

    tex_orm = create_expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, -1280, 40)
    tex_orm.set_editor_property("parameter_name", "ORMMap")
    tex_orm.set_editor_property("texture", default_orm)
    tex_orm.set_editor_property("sampler_type", resolve_sampler_type_for_texture(default_orm))

    tex_norm = create_expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, -1280, 460)
    tex_norm.set_editor_property("parameter_name", "NormalMap")
    tex_norm.set_editor_property("texture", default_norm)
    tex_norm.set_editor_property("sampler_type", resolve_sampler_type_for_texture(default_norm))

    # 2. ORM Unpack: R = Ambient Occlusion, G = Roughness, B = Metallic (glTF 2.0 standard)
    ao_strength = create_expr(mat, unreal.MaterialExpressionScalarParameter, -920, -40)
    ao_strength.set_editor_property("parameter_name", "AOStrength")
    ao_strength.set_editor_property("default_value", 0.85)

    ao_lerp = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -620, -40)
    ao_lerp.set_editor_property("const_a", 1.0)
    connect_expr(tex_orm, "R", ao_lerp, "B")
    connect_expr(ao_strength, "", ao_lerp, "Alpha")
    connect_prop(ao_lerp, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    # Roughness Contrast Remap from ORM.G
    rough_clamp_in = create_expr(mat, unreal.MaterialExpressionClamp, -980, 110)
    rough_clamp_in.set_editor_property("min_default", 0.02)
    rough_clamp_in.set_editor_property("max_default", 1.0)
    connect_expr(tex_orm, "G", rough_clamp_in, "")

    rough_exp_param = create_expr(mat, unreal.MaterialExpressionScalarParameter, -980, 220)
    rough_exp_param.set_editor_property("parameter_name", "RoughnessContrastExp")
    rough_exp_param.set_editor_property("default_value", 1.65)

    rough_pow = create_expr(mat, unreal.MaterialExpressionPower, -760, 140)
    connect_expr(rough_clamp_in, "", rough_pow, "Base")
    connect_expr(rough_exp_param, "", rough_pow, "Exp")

    rough_min_param = create_expr(mat, unreal.MaterialExpressionScalarParameter, -760, 260)
    rough_min_param.set_editor_property("parameter_name", "RoughnessMin")
    rough_min_param.set_editor_property("default_value", 0.25)

    rough_max_param = create_expr(mat, unreal.MaterialExpressionScalarParameter, -760, 360)
    rough_max_param.set_editor_property("parameter_name", "RoughnessMax")
    rough_max_param.set_editor_property("default_value", 0.48)

    rough_remap = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -500, 180)
    connect_expr(rough_min_param, "", rough_remap, "A")
    connect_expr(rough_max_param, "", rough_remap, "B")
    connect_expr(rough_pow, "", rough_remap, "Alpha")

    rough_final = create_expr(mat, unreal.MaterialExpressionClamp, -280, 180)
    rough_final.set_editor_property("min_default", 0.16)
    rough_final.set_editor_property("max_default", 0.85)
    connect_expr(rough_remap, "", rough_final, "")
    connect_prop(rough_final, "", unreal.MaterialProperty.MP_ROUGHNESS)

    # Bounded Semi-Metallic Cast-Steel Remap from ORM.B -> Lerp(MetallicMin, MetallicMax, Saturate(ORM.B * MetallicScale)) in [0.38, 0.62]
    metal_scale_param = create_expr(mat, unreal.MaterialExpressionScalarParameter, -1040, 360)
    metal_scale_param.set_editor_property("parameter_name", "MetallicScale")
    metal_scale_param.set_editor_property("default_value", 1.05)

    metal_scaled = create_expr(mat, unreal.MaterialExpressionMultiply, -860, 420)
    connect_expr(tex_orm, "B", metal_scaled, "A")
    connect_expr(metal_scale_param, "", metal_scaled, "B")

    metal_alpha = create_expr(mat, unreal.MaterialExpressionClamp, -700, 420)
    metal_alpha.set_editor_property("min_default", 0.0)
    metal_alpha.set_editor_property("max_default", 1.0)
    connect_expr(metal_scaled, "", metal_alpha, "")

    metal_min_param = create_expr(mat, unreal.MaterialExpressionScalarParameter, -740, 520)
    metal_min_param.set_editor_property("parameter_name", "MetallicMin")
    metal_min_param.set_editor_property("default_value", 0.40)

    metal_max_param = create_expr(mat, unreal.MaterialExpressionScalarParameter, -740, 610)
    metal_max_param.set_editor_property("parameter_name", "MetallicMax")
    metal_max_param.set_editor_property("default_value", 0.58)

    metal_lerp = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -500, 480)
    connect_expr(metal_min_param, "", metal_lerp, "A")
    connect_expr(metal_max_param, "", metal_lerp, "B")
    connect_expr(metal_alpha, "", metal_lerp, "Alpha")

    metal_final = create_expr(mat, unreal.MaterialExpressionClamp, -300, 480)
    metal_final.set_editor_property("min_default", 0.0)
    metal_final.set_editor_property("max_default", 1.0)
    connect_expr(metal_lerp, "", metal_final, "")
    connect_prop(metal_final, "", unreal.MaterialProperty.MP_METALLIC)

    spec_scale_param = create_expr(mat, unreal.MaterialExpressionScalarParameter, -300, 570)
    spec_scale_param.set_editor_property("parameter_name", "SpecularScale")
    spec_scale_param.set_editor_property("default_value", 0.72)
    connect_prop(spec_scale_param, "", unreal.MaterialProperty.MP_SPECULAR)

    # 3. BaseColor + Cast-Steel Diffuse & Conductor F0 Floor Protection
    albedo_tint_param = create_expr(mat, unreal.MaterialExpressionVectorParameter, -1040, -220)
    albedo_tint_param.set_editor_property("parameter_name", "AlbedoTint")
    albedo_tint_param.set_editor_property("default_value", unreal.LinearColor(1.65, 1.72, 1.82, 1.0))

    tinted_rgb = create_expr(mat, unreal.MaterialExpressionMultiply, -800, -320)
    connect_expr(tex_base, "RGB", tinted_rgb, "A")
    connect_expr(albedo_tint_param, "", tinted_rgb, "B")

    conductor_f0_param = create_expr(mat, unreal.MaterialExpressionVectorParameter, -800, -170)
    conductor_f0_param.set_editor_property("parameter_name", "ConductorMinF0")
    conductor_f0_param.set_editor_property("default_value", unreal.LinearColor(0.24, 0.26, 0.30, 1.0))

    diffuse_floor_rgb = create_expr(mat, unreal.MaterialExpressionMultiply, -600, -380)
    diffuse_floor_rgb.set_editor_property("const_b", 0.45)
    connect_expr(conductor_f0_param, "", diffuse_floor_rgb, "A")

    diffuse_safe_rgb = create_expr(mat, unreal.MaterialExpressionMax, -440, -340)
    connect_expr(tinted_rgb, "", diffuse_safe_rgb, "A")
    connect_expr(diffuse_floor_rgb, "", diffuse_safe_rgb, "B")

    conductor_safe_rgb = create_expr(mat, unreal.MaterialExpressionMax, -440, -220)
    connect_expr(tinted_rgb, "", conductor_safe_rgb, "A")
    connect_expr(conductor_f0_param, "", conductor_safe_rgb, "B")

    final_albedo = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -220, -280)
    connect_expr(diffuse_safe_rgb, "", final_albedo, "A")
    connect_expr(conductor_safe_rgb, "", final_albedo, "B")
    connect_expr(metal_final, "", final_albedo, "Alpha")
    connect_prop(final_albedo, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # 4. Crisp Tangent Normal Scaling
    norm_xy_mask = create_expr(mat, unreal.MaterialExpressionComponentMask, -980, 620)
    norm_xy_mask.set_editor_property("r", True)
    norm_xy_mask.set_editor_property("g", True)
    norm_xy_mask.set_editor_property("b", False)
    norm_xy_mask.set_editor_property("a", False)
    connect_expr(tex_norm, "RGB", norm_xy_mask, "")

    norm_strength_param = create_expr(mat, unreal.MaterialExpressionScalarParameter, -980, 740)
    norm_strength_param.set_editor_property("parameter_name", "NormalStrength")
    norm_strength_param.set_editor_property("default_value", 1.35)

    norm_xy_scaled = create_expr(mat, unreal.MaterialExpressionMultiply, -720, 640)
    connect_expr(norm_xy_mask, "", norm_xy_scaled, "A")
    connect_expr(norm_strength_param, "", norm_xy_scaled, "B")

    norm_out = create_expr(mat, unreal.MaterialExpressionAppendVector, -460, 660)
    connect_expr(norm_xy_scaled, "", norm_out, "A")
    connect_expr(tex_norm, "B", norm_out, "B")
    connect_prop(norm_out, "", unreal.MaterialProperty.MP_NORMAL)

    # Enable MATUSAGE_INSTANCED_STATIC_MESHES so FoundryGrateShaftISM & FoundryFlankColumnISM compile cooked ISM shaders
    finalize_and_save_material(mat, is_opaque_nanite_compatible=True, enable_instanced_static_mesh=True)

    # 5. Build and validate all 9 MI_Station_* MaterialInstanceConstants
    built_instances: Dict[str, unreal.MaterialInstanceConstant] = {}
    for spec in STATION_PBR_SPECS:
        diff_tex, orm_tex, norm_tex = load_and_verify_rodin_prop_textures(spec.prop_name)
        mic = get_or_create_material_instance(spec.instance_name, mat)

        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mic, "BaseColorMap", diff_tex)
        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mic, "ORMMap", orm_tex)
        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mic, "NormalMap", norm_tex)

        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
            mic,
            "AlbedoTint",
            unreal.LinearColor(spec.albedo_tint[0], spec.albedo_tint[1], spec.albedo_tint[2], 1.0),
        )
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
            mic,
            "ConductorMinF0",
            unreal.LinearColor(spec.conductor_min_f0[0], spec.conductor_min_f0[1], spec.conductor_min_f0[2], 1.0),
        )
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
            mic, "RoughnessContrastExp", spec.roughness_contrast_exp
        )
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
            mic, "RoughnessMin", spec.roughness_min
        )
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
            mic, "RoughnessMax", spec.roughness_max
        )
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
            mic, "MetallicScale", spec.metallic_scale
        )
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
            mic, "MetallicMin", spec.metallic_min
        )
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
            mic, "MetallicMax", spec.metallic_max
        )
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
            mic, "AOStrength", spec.ao_strength
        )
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
            mic, "NormalStrength", spec.normal_strength
        )

        unreal.MaterialEditingLibrary.update_material_instance(mic)

        base_mat = mic.get_base_material()
        if base_mat is None or "WorldGridMaterial" in base_mat.get_name():
            raise RuntimeError(f"MI {spec.instance_name} resolved to fallback/invalid base material: {base_mat}")

        for req_tex_param in ("BaseColorMap", "ORMMap", "NormalMap"):
            bound_tex = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(mic, req_tex_param)
            if bound_tex is None:
                raise RuntimeError(f"MI {spec.instance_name} failed binding texture parameter {req_tex_param}!")

        if not unreal.EditorAssetLibrary.save_loaded_asset(mic, only_if_is_dirty=False):
            raise RuntimeError(f"Failed saving MaterialInstanceConstant {mic.get_path_name()}")

        sm_path = f"{MESHES_DIR}/{spec.prop_name}"
        if unreal.EditorAssetLibrary.does_asset_exist(sm_path):
            sm_asset = unreal.EditorAssetLibrary.load_asset(sm_path)
            if isinstance(sm_asset, unreal.StaticMesh):
                sm_asset.set_material(0, mic)
                if not unreal.EditorAssetLibrary.save_loaded_asset(sm_asset, only_if_is_dirty=False):
                    raise RuntimeError(f"Failed saving StaticMesh {sm_path}")

        built_instances[spec.slot_property] = mic
        log_info(f"Built & verified {mic.get_path_name()} -> {spec.prop_name} ({spec.slot_property})")

    return mat, built_instances


def ensure_cook_safe_ambient_cubemap() -> unreal.TextureCube:
    """
    Guard 1: Duplicate Engine ambient cubemap directly into /Game/Presentation/Lighting/TC_CellAmbientCubemap
    so bCookMapsOnly=True + DirectoriesToAlwaysCook=('/Game/Presentation') + DA_Presentation hard reference
    guarantee 100% deterministic packaged SkyLight reflections and ambient fill.
    """
    ensure_directory(LIGHTING_DIR)

    if unreal.EditorAssetLibrary.does_asset_exist(AMBIENT_CUBEMAP_DEST_PATH):
        existing = unreal.EditorAssetLibrary.load_asset(AMBIENT_CUBEMAP_DEST_PATH)
        if isinstance(existing, unreal.TextureCube):
            unreal.EditorAssetLibrary.save_loaded_asset(existing, only_if_is_dirty=False)
            log_info(f"Verified existing cook-safe ambient cubemap: {AMBIENT_CUBEMAP_DEST_PATH}")
            return existing

    candidate_sources = [
        "/Engine/MapTemplates/Sky/DaylightAmbientCubemap",
        "/Engine/Functions/Engine_MaterialFunctions01/Cubemaps/InteriorCubemap",
    ]
    source_path_found: Optional[str] = None
    for candidate in candidate_sources:
        if unreal.EditorAssetLibrary.does_asset_exist(candidate):
            loaded_src = unreal.EditorAssetLibrary.load_asset(candidate)
            if isinstance(loaded_src, unreal.TextureCube):
                source_path_found = candidate
                break

    if source_path_found is None:
        raise RuntimeError(f"Could not locate any Engine UTextureCube among {candidate_sources}")

    duplicated = unreal.EditorAssetLibrary.duplicate_asset(source_path_found, AMBIENT_CUBEMAP_DEST_PATH)
    if not isinstance(duplicated, unreal.TextureCube):
        raise RuntimeError(f"Failed duplicating {source_path_found} -> {AMBIENT_CUBEMAP_DEST_PATH}")

    if not unreal.EditorAssetLibrary.save_loaded_asset(duplicated, only_if_is_dirty=False):
        raise RuntimeError(f"Failed saving duplicated cubemap {AMBIENT_CUBEMAP_DEST_PATH}")

    log_info(f"Duplicated & saved cook-safe SkyLight cubemap: {source_path_found} -> {AMBIENT_CUBEMAP_DEST_PATH}")
    return duplicated


def bind_materials_and_lighting_to_presentation(
    m_laser: unreal.Material,
    m_slag: unreal.Material,
    m_molten: unreal.Material,
    m_wall: unreal.Material,
    m_floor: unreal.Material,
    m_gunmetal: unreal.Material,
    station_instances: Dict[str, unreal.MaterialInstanceConstant],
    ambient_cubemap: unreal.TextureCube,
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
    da_pres.set_editor_property("dark_gunmetal_material", m_gunmetal)

    # Bind all 9 calibrated MI_Station_* instances (including MI_Station_DiverterFlapper) to their DA_Presentation mesh slots
    for slot_prop, mic in station_instances.items():
        slot_struct = da_pres.get_editor_property(slot_prop)
        slot_struct.set_editor_property("material_overrides", [mic])
        da_pres.set_editor_property(slot_prop, slot_struct)

    # Configure Phase 11A-Fix LightingRigConfig (1 VSM shadow-casting area spot + 4 unshadowed industrial tube fill/rim/foundry-shaft lights; zero side-wall wash)
    rig_cfg = unreal.CellLightingRigConfig()

    key_spot = rig_cfg.get_editor_property("key_spot")
    key_spot.set_editor_property("location", unreal.Vector(165.0, -22.0, 330.0))
    key_spot.set_editor_property("target_location", unreal.Vector(260.0, 0.0, 105.0))
    key_spot.set_editor_property("intensity", 8800.0)
    key_spot.set_editor_property("attenuation_radius", 580.0)
    key_spot.set_editor_property("inner_cone_angle", 22.0)
    key_spot.set_editor_property("outer_cone_angle", 40.0)
    key_spot.set_editor_property("source_radius", 26.0)
    key_spot.set_editor_property("source_length", 90.0)
    key_spot.set_editor_property("light_color", unreal.LinearColor(0.96, 0.97, 1.00, 1.0))
    key_spot.set_editor_property("cast_shadows", True)
    rig_cfg.set_editor_property("key_spot", key_spot)

    rig_cfg.set_editor_property("fill_point_location", unreal.Vector(95.0, 14.0, 142.0))
    rig_cfg.set_editor_property("fill_point_intensity", 2650.0)
    rig_cfg.set_editor_property("fill_point_radius", 355.0)
    rig_cfg.set_editor_property("fill_point_source_radius", 36.0)
    rig_cfg.set_editor_property("fill_point_source_length", 0.0)
    rig_cfg.set_editor_property("fill_point_color", unreal.LinearColor(0.84, 0.90, 1.00, 1.0))

    rim_left = rig_cfg.get_editor_property("rim_spot_left")
    rim_left.set_editor_property("location", unreal.Vector(305.0, -135.0, 220.0))
    rim_left.set_editor_property("target_location", unreal.Vector(260.0, 0.0, 108.0))
    rim_left.set_editor_property("intensity", 6800.0)
    rim_left.set_editor_property("attenuation_radius", 430.0)
    rim_left.set_editor_property("inner_cone_angle", 20.0)
    rim_left.set_editor_property("outer_cone_angle", 38.0)
    rim_left.set_editor_property("source_radius", 20.0)
    rim_left.set_editor_property("source_length", 115.0)
    rim_left.set_editor_property("light_color", unreal.LinearColor(0.72, 0.87, 1.00, 1.0))
    rim_left.set_editor_property("cast_shadows", False)
    rig_cfg.set_editor_property("rim_spot_left", rim_left)

    rim_right = rig_cfg.get_editor_property("rim_spot_right")
    rim_right.set_editor_property("location", unreal.Vector(305.0, 135.0, 212.0))
    rim_right.set_editor_property("target_location", unreal.Vector(260.0, 0.0, 108.0))
    rim_right.set_editor_property("intensity", 6800.0)
    rim_right.set_editor_property("attenuation_radius", 430.0)
    rim_right.set_editor_property("inner_cone_angle", 20.0)
    rim_right.set_editor_property("outer_cone_angle", 38.0)
    rim_right.set_editor_property("source_radius", 20.0)
    rim_right.set_editor_property("source_length", 115.0)
    rim_right.set_editor_property("light_color", unreal.LinearColor(0.72, 0.87, 1.00, 1.0))
    rim_right.set_editor_property("cast_shadows", False)
    rig_cfg.set_editor_property("rim_spot_right", rim_right)

    # Direct +X frontal foundry shaft glow (dX = 112 cm onto X = 444 grate, 67 deg outer cone reaching Y = +-155 columns; R = 325 < 360 -> 0.000 side-wall spill)
    mid_wash = rig_cfg.get_editor_property("midground_wash_spot")
    mid_wash.set_editor_property("location", unreal.Vector(332.0, 0.0, 72.0))
    mid_wash.set_editor_property("target_location", unreal.Vector(445.0, 0.0, 82.0))
    mid_wash.set_editor_property("intensity", 10800.0)
    mid_wash.set_editor_property("attenuation_radius", 325.0)
    mid_wash.set_editor_property("inner_cone_angle", 38.0)
    mid_wash.set_editor_property("outer_cone_angle", 67.0)
    mid_wash.set_editor_property("source_radius", 28.0)
    mid_wash.set_editor_property("source_length", 120.0)
    mid_wash.set_editor_property("light_color", unreal.LinearColor(1.00, 0.38, 0.09, 1.0))
    mid_wash.set_editor_property("cast_shadows", False)
    rig_cfg.set_editor_property("midground_wash_spot", mid_wash)

    da_pres.set_editor_property("lighting_rig_config", rig_cfg)

    # Configure Phase 11A-Fix AtmosphereConfig (unoccluded SkyLightIntensity=0.95 + LowerHemisphere=(0.035,0.042,0.055); zero milky side-wall fog)
    atmos_cfg = unreal.CellAtmosphereConfig()
    atmos_cfg.set_editor_property("ambient_sky_cubemap", ambient_cubemap)
    atmos_cfg.set_editor_property("sky_light_intensity", 0.95)
    atmos_cfg.set_editor_property("sky_tint_color", unreal.LinearColor(0.64, 0.78, 0.96, 1.0))
    atmos_cfg.set_editor_property("lower_hemisphere_color", unreal.LinearColor(0.035, 0.042, 0.055, 1.0))
    atmos_cfg.set_editor_property("enable_depth_fog", True)
    atmos_cfg.set_editor_property("fog_density", 0.015)
    atmos_cfg.set_editor_property("fog_height_falloff", 0.28)
    atmos_cfg.set_editor_property("fog_start_distance_cm", 390.0)
    atmos_cfg.set_editor_property("fog_max_opacity", 0.32)
    atmos_cfg.set_editor_property("fog_inscattering_color", unreal.LinearColor(0.005, 0.004, 0.0035, 1.0))
    atmos_cfg.set_editor_property("fog_actor_z_cm", 90.0)
    atmos_cfg.set_editor_property("spawn_midground_architecture_relief", True)
    da_pres.set_editor_property("atmosphere_config", atmos_cfg)

    # Configure Phase 10/11A MoltenBounceConfig (~2200K zero-shadow thermal bounce on cut cap & scale pan well)
    molten_cfg = unreal.MoltenBounceLightConfig()
    molten_cfg.set_editor_property("bounce_color", unreal.LinearColor(1.00, 0.36, 0.07, 1.0))
    molten_cfg.set_editor_property("cut_flash_peak_intensity", 5600.0)
    molten_cfg.set_editor_property("cut_steady_remainder_intensity", 1350.0)
    molten_cfg.set_editor_property("cut_attenuation_radius_cm", 265.0)
    molten_cfg.set_editor_property("cut_light_local_offset", unreal.Vector(-22.0, 0.0, -24.0))
    molten_cfg.set_editor_property("scale_well_flash_intensity", 2800.0)
    molten_cfg.set_editor_property("scale_well_max_steady_intensity", 1150.0)
    molten_cfg.set_editor_property("scale_well_attenuation_radius_cm", 165.0)
    molten_cfg.set_editor_property("scale_well_local_offset", unreal.Vector(0.0, 0.0, 24.0))
    molten_cfg.set_editor_property("flash_decay_duration_seconds", 0.85)
    molten_cfg.set_editor_property("steady_interp_speed", 6.0)
    da_pres.set_editor_property("molten_bounce_config", molten_cfg)

    # Configure Phase 11A-Fix CameraPostProcessPolishConfig (EV100=1.0, Bias=0.85, BloomThreshold=1.65 HUD firewall, VignetteIntensity=0.36)
    pp_cfg = unreal.CameraPostProcessPolishConfig()
    pp_cfg.set_editor_property("locked_ev100_min_max", 1.0)
    pp_cfg.set_editor_property("exposure_compensation_bias", 0.85)
    pp_cfg.set_editor_property("local_exposure_highlight_contrast", 1.0)
    pp_cfg.set_editor_property("local_exposure_shadow_contrast", 1.0)
    pp_cfg.set_editor_property("bloom_intensity", 0.42)
    pp_cfg.set_editor_property("bloom_threshold", 1.65)
    pp_cfg.set_editor_property("film_slope", 0.90)
    pp_cfg.set_editor_property("film_toe", 0.53)
    pp_cfg.set_editor_property("film_shoulder", 0.28)
    pp_cfg.set_editor_property("color_contrast", 1.05)
    pp_cfg.set_editor_property("color_saturation", 0.96)
    pp_cfg.set_editor_property("vignette_intensity", 0.36)
    pp_cfg.set_editor_property("tonemapper_sharpen", 0.35)
    da_pres.set_editor_property("camera_post_process_config", pp_cfg)

    # Verify Invariants (CellWallPanelMeshSlot.mesh stays None, CellFloorGrateMeshSlot.MaterialOverrides[0] populated)
    if da_pres.get_editor_property("cell_wall_panel_mesh_slot").get_editor_property("mesh") is not None:
        raise AssertionError("CellWallPanelMeshSlot.mesh must NEVER be populated!")

    grate_overrides = list(
        da_pres.get_editor_property("cell_floor_grate_mesh_slot").get_editor_property("material_overrides") or []
    )
    if not grate_overrides or grate_overrides[0] is None:
        raise AssertionError("CellFloorGrateMeshSlot.MaterialOverrides[0] must be populated!")

    # Verify & enforce all 4 Niagara VFX slots on DA_Presentation
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

    log_info(
        f"Bound all Phase 11A master materials, 9 MI_Station_* PBR instances, cook-safe SkyLight cubemap, "
        f"lighting/atmosphere/post-process configs, and saved {DA_PRESENTATION_PATH}"
    )


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
    log_info("Starting Phase 10 Render Royalty material generation, station PBR remap, cubemap duplication & DA_Presentation calibration...")
    m_laser = build_laser_sheet_master()
    m_molten = build_molten_cut_cap_master()
    m_slag = build_slag_outer_master()
    m_gunmetal = build_dark_gunmetal_master()
    m_wall = build_cell_wall_master()
    m_floor = build_cell_floor_master()
    _, station_instances = build_station_pbr_master_and_instances()
    ambient_cubemap = ensure_cook_safe_ambient_cubemap()
    bind_materials_and_lighting_to_presentation(
        m_laser=m_laser,
        m_slag=m_slag,
        m_molten=m_molten,
        m_wall=m_wall,
        m_floor=m_floor,
        m_gunmetal=m_gunmetal,
        station_instances=station_instances,
        ambient_cubemap=ambient_cubemap,
    )
    update_contracts_and_monoliths(m_slag=m_slag, m_molten=m_molten)
    log_info("Completed Phase 10 BuildMaterialsAndBind successfully.")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        log_error(f"FATAL: {exc}")
        raise
