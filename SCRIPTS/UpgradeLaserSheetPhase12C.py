import sys

sys.dont_write_bytecode = True

import unreal


MATERIALS_DIR = "/Game/Presentation/Materials"
LASER_SHEET_PATH = f"{MATERIALS_DIR}/M_LaserSheet_Master"


def log_info(msg: str) -> None:
    unreal.log(f"[UpgradeLaserSheetPhase12C] {msg}")


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


def make_scalar_param(mat: unreal.Material, name: str, default_val: float, pos_x: int, pos_y: int):
    node = create_expr(mat, unreal.MaterialExpressionScalarParameter, pos_x, pos_y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", float(default_val))
    return node


def make_vector_param(
    mat: unreal.Material, name: str, r: float, g: float, b: float, a: float, pos_x: int, pos_y: int
):
    node = create_expr(mat, unreal.MaterialExpressionVectorParameter, pos_x, pos_y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", unreal.LinearColor(float(r), float(g), float(b), float(a)))
    return node


def make_const_scalar(mat: unreal.Material, val: float, pos_x: int, pos_y: int):
    node = create_expr(mat, unreal.MaterialExpressionConstant, pos_x, pos_y)
    node.set_editor_property("r", float(val))
    return node


def make_mask_channel(mat: unreal.Material, src_expr, r: bool, g: bool, b: bool, pos_x: int, pos_y: int):
    mask = create_expr(mat, unreal.MaterialExpressionComponentMask, pos_x, pos_y)
    mask.set_editor_property("r", r)
    mask.set_editor_property("g", g)
    mask.set_editor_property("b", b)
    mask.set_editor_property("a", False)
    connect_expr(src_expr, "", mask, "")
    return mask


def make_clamp01(mat: unreal.Material, src_expr, pos_x: int, pos_y: int):
    cl = create_expr(mat, unreal.MaterialExpressionClamp, pos_x, pos_y)
    cl.set_editor_property("min_default", 0.0)
    cl.set_editor_property("max_default", 1.0)
    connect_expr(src_expr, "", cl, "")
    return cl


def populate_phase12c_laser_sheet_graph(mat: unreal.Material) -> None:
    """
    Phase 12C High-Energy OBB-Masked Translucent Laser Optic Sheet:
    - Preserves 0.018 UV perimeter frame across all 5 gameplay state palettes (Cyan, Emerald, Yellow, Amber, Crimson).
    - Replaces static 68-cycle V gizmo stripes with phase-continuous dual-wave GPU Time scrolling plasma scanlines
      at 0.08..0.12 active interior opacity (mean background lift <= +18 sRGB).
    - Analytically reconstructs P_scene from native WorldPosition, CameraPositionWS, SceneDepth, and PixelDepth
      and masks a dual-zone #FFF6D6 incandescent core + molten gold-orange thermal shoulder (shifting to white-crimson
      arc on DirectVeinHitScalar=1) strictly to the tilt-rigid 3D cuttable monolith OBB (0.0 glow on SM_DiverterFlapper
      and SM_ClampJaw).
    """
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", False)
    mat.set_editor_property("used_with_static_lighting", False)
    mat.set_editor_property("has_pixel_animation", True)

    # =========================================================================
    # Stage 1: UV0 & 0.018 UV Perimeter Border Mask
    # =========================================================================
    tex_uv = create_expr(mat, unreal.MaterialExpressionTextureCoordinate, -2400, -400)
    tex_uv.set_editor_property("coordinate_index", 0)
    tex_uv.set_editor_property("u_tiling", 1.0)
    tex_uv.set_editor_property("v_tiling", 1.0)

    u_mask = make_mask_channel(mat, tex_uv, True, False, False, -2180, -480)
    v_mask = make_mask_channel(mat, tex_uv, False, True, False, -2180, -320)

    u_centered = create_expr(mat, unreal.MaterialExpressionSubtract, -1980, -480)
    u_centered.set_editor_property("const_b", 0.5)
    connect_expr(u_mask, "", u_centered, "A")
    u_abs = create_expr(mat, unreal.MaterialExpressionAbs, -1820, -480)
    connect_expr(u_centered, "", u_abs, "")

    v_centered = create_expr(mat, unreal.MaterialExpressionSubtract, -1980, -320)
    v_centered.set_editor_property("const_b", 0.5)
    connect_expr(v_mask, "", v_centered, "A")
    v_abs = create_expr(mat, unreal.MaterialExpressionAbs, -1820, -320)
    connect_expr(v_centered, "", v_abs, "")

    box_dist = create_expr(mat, unreal.MaterialExpressionMax, -1660, -400)
    connect_expr(u_abs, "", box_dist, "A")
    connect_expr(v_abs, "", box_dist, "B")

    edge_shifted = create_expr(mat, unreal.MaterialExpressionSubtract, -1500, -400)
    edge_shifted.set_editor_property("const_b", 0.482)
    connect_expr(box_dist, "", edge_shifted, "A")

    edge_scaled = create_expr(mat, unreal.MaterialExpressionMultiply, -1340, -400)
    edge_scaled.set_editor_property("const_b", 55.5556)
    connect_expr(edge_shifted, "", edge_scaled, "A")

    edge_clamped = make_clamp01(mat, edge_scaled, -1180, -400)

    border_mask = create_expr(mat, unreal.MaterialExpressionPower, -1020, -400)
    border_mask.set_editor_property("const_exponent", 1.6)
    connect_expr(edge_clamped, "", border_mask, "Base")

    inv_border_mask = create_expr(mat, unreal.MaterialExpressionOneMinus, -840, -400)
    connect_expr(border_mask, "", inv_border_mask, "")

    # =========================================================================
    # Stage 2: Phase-Continuous Dual-Wave Scrolling Plasma Scanlines
    # =========================================================================
    time_node = create_expr(mat, unreal.MaterialExpressionTime, -2400, 0)
    laser_charge_param = make_scalar_param(mat, "LaserCharge", 0.0, -2400, 140)
    vein_prox_param = make_scalar_param(mat, "VeinProximity", 0.0, -2400, 260)
    charge_01 = make_clamp01(mat, laser_charge_param, -2180, 140)
    prox_01 = make_clamp01(mat, vein_prox_param, -2180, 260)

    # Primary plasma raster wave: U*14 + V*24 - Time*0.85
    u_w1 = create_expr(mat, unreal.MaterialExpressionMultiply, -2140, -140)
    u_w1.set_editor_property("const_b", 14.0)
    connect_expr(u_mask, "", u_w1, "A")

    v_w1 = create_expr(mat, unreal.MaterialExpressionMultiply, -2140, -40)
    v_w1.set_editor_property("const_b", 24.0)
    connect_expr(v_mask, "", v_w1, "A")

    uv_w1 = create_expr(mat, unreal.MaterialExpressionAdd, -1960, -100)
    connect_expr(u_w1, "", uv_w1, "A")
    connect_expr(v_w1, "", uv_w1, "B")

    t_w1 = create_expr(mat, unreal.MaterialExpressionMultiply, -2140, 40)
    t_w1.set_editor_property("const_b", 0.85)
    connect_expr(time_node, "", t_w1, "A")

    phase_w1 = create_expr(mat, unreal.MaterialExpressionSubtract, -1780, -60)
    connect_expr(uv_w1, "", phase_w1, "A")
    connect_expr(t_w1, "", phase_w1, "B")

    sin_w1 = create_expr(mat, unreal.MaterialExpressionSine, -1620, -60)
    sin_w1.set_editor_property("period", 1.0)
    connect_expr(phase_w1, "", sin_w1, "")

    half_w1 = create_expr(mat, unreal.MaterialExpressionMultiply, -1460, -60)
    half_w1.set_editor_property("const_b", 0.5)
    connect_expr(sin_w1, "", half_w1, "A")

    wave_primary_01 = create_expr(mat, unreal.MaterialExpressionAdd, -1300, -60)
    wave_primary_01.set_editor_property("const_b", 0.5)
    connect_expr(half_w1, "", wave_primary_01, "A")

    # Secondary capacitor surge harmonic wave: U*22 - V*32 - Time*4.20
    u_w2 = create_expr(mat, unreal.MaterialExpressionMultiply, -2140, 360)
    u_w2.set_editor_property("const_b", 22.0)
    connect_expr(u_mask, "", u_w2, "A")

    v_w2 = create_expr(mat, unreal.MaterialExpressionMultiply, -2140, 460)
    v_w2.set_editor_property("const_b", 32.0)
    connect_expr(v_mask, "", v_w2, "A")

    uv_w2 = create_expr(mat, unreal.MaterialExpressionSubtract, -1960, 400)
    connect_expr(u_w2, "", uv_w2, "A")
    connect_expr(v_w2, "", uv_w2, "B")

    t_w2 = create_expr(mat, unreal.MaterialExpressionMultiply, -2140, 560)
    t_w2.set_editor_property("const_b", 4.20)
    connect_expr(time_node, "", t_w2, "A")

    phase_w2 = create_expr(mat, unreal.MaterialExpressionSubtract, -1780, 440)
    connect_expr(uv_w2, "", phase_w2, "A")
    connect_expr(t_w2, "", phase_w2, "B")

    sin_w2 = create_expr(mat, unreal.MaterialExpressionSine, -1620, 440)
    sin_w2.set_editor_property("period", 1.0)
    connect_expr(phase_w2, "", sin_w2, "")

    half_w2 = create_expr(mat, unreal.MaterialExpressionMultiply, -1460, 440)
    half_w2.set_editor_property("const_b", 0.5)
    connect_expr(sin_w2, "", half_w2, "A")

    wave_surge_01 = create_expr(mat, unreal.MaterialExpressionAdd, -1300, 440)
    wave_surge_01.set_editor_property("const_b", 0.5)
    connect_expr(half_w2, "", wave_surge_01, "A")

    charge_wave_alpha = create_expr(mat, unreal.MaterialExpressionMultiply, -1300, 200)
    charge_wave_alpha.set_editor_property("const_b", 0.65)
    connect_expr(charge_01, "", charge_wave_alpha, "A")

    wave_blend = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -1120, 140)
    connect_expr(wave_primary_01, "", wave_blend, "A")
    connect_expr(wave_surge_01, "", wave_blend, "B")
    connect_expr(charge_wave_alpha, "", wave_blend, "Alpha")

    wave_safe = create_expr(mat, unreal.MaterialExpressionClamp, -960, 140)
    wave_safe.set_editor_property("min_default", 0.001)
    wave_safe.set_editor_property("max_default", 1.0)
    connect_expr(wave_blend, "", wave_safe, "")

    scan_filament = create_expr(mat, unreal.MaterialExpressionPower, -800, 140)
    scan_filament.set_editor_property("const_exponent", 1.45)
    connect_expr(wave_safe, "", scan_filament, "Base")

    # Base Emissive & Interior Opacity (0.08..0.12 in active contact)
    emissive_param = make_vector_param(mat, "EmissiveColor", 0.08, 0.88, 1.0, 1.0, -960, -680)
    trough_gain_param = make_scalar_param(mat, "InteriorTroughGain", 0.08, -960, -540)
    crest_gain_param = make_scalar_param(mat, "InteriorCrestGain", 0.46, -960, -460)
    border_gain_param = make_scalar_param(mat, "BorderEmissiveGain", 2.60, -760, -540)

    prox_tiny_boost = create_expr(mat, unreal.MaterialExpressionMultiply, -960, -340)
    prox_tiny_boost.set_editor_property("const_b", 0.04)
    connect_expr(prox_01, "", prox_tiny_boost, "A")

    charge_crest_boost = create_expr(mat, unreal.MaterialExpressionMultiply, -960, -260)
    charge_crest_boost.set_editor_property("const_b", 0.45)
    connect_expr(charge_01, "", charge_crest_boost, "A")

    crest_plus_prox = create_expr(mat, unreal.MaterialExpressionAdd, -760, -380)
    connect_expr(crest_gain_param, "", crest_plus_prox, "A")
    connect_expr(prox_tiny_boost, "", crest_plus_prox, "B")

    effective_crest_gain = create_expr(mat, unreal.MaterialExpressionAdd, -600, -340)
    connect_expr(crest_plus_prox, "", effective_crest_gain, "A")
    connect_expr(charge_crest_boost, "", effective_crest_gain, "B")

    interior_emissive_gain = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -420, -340)
    connect_expr(trough_gain_param, "", interior_emissive_gain, "A")
    connect_expr(effective_crest_gain, "", interior_emissive_gain, "B")
    connect_expr(scan_filament, "", interior_emissive_gain, "Alpha")

    base_emissive_gain = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -220, -460)
    connect_expr(interior_emissive_gain, "", base_emissive_gain, "A")
    connect_expr(border_gain_param, "", base_emissive_gain, "B")
    connect_expr(border_mask, "", base_emissive_gain, "Alpha")

    base_sheet_emissive = create_expr(mat, unreal.MaterialExpressionMultiply, 0, -540)
    connect_expr(emissive_param, "", base_sheet_emissive, "A")
    connect_expr(base_emissive_gain, "", base_sheet_emissive, "B")

    opacity_param = make_scalar_param(mat, "Opacity", 0.42, -960, 280)
    interior_op_scale = make_scalar_param(mat, "InteriorOpacityScale", 0.22, -960, 380)

    op_base_mul = create_expr(mat, unreal.MaterialExpressionMultiply, -760, 320)
    connect_expr(opacity_param, "", op_base_mul, "A")
    connect_expr(interior_op_scale, "", op_base_mul, "B")

    scan_op_mod = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -760, 200)
    scan_op_mod.set_editor_property("const_a", 0.86)
    scan_op_mod.set_editor_property("const_b", 1.16)
    connect_expr(scan_filament, "", scan_op_mod, "Alpha")

    interior_opacity = create_expr(mat, unreal.MaterialExpressionMultiply, -560, 260)
    connect_expr(op_base_mul, "", interior_opacity, "A")
    connect_expr(scan_op_mod, "", interior_opacity, "B")

    border_op_const = make_const_scalar(mat, 0.88, -560, 380)
    base_sheet_opacity = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, -340, 300)
    connect_expr(interior_opacity, "", base_sheet_opacity, "A")
    connect_expr(border_op_const, "", base_sheet_opacity, "B")
    connect_expr(border_mask, "", base_sheet_opacity, "Alpha")

    # =========================================================================
    # Stage 3: Correction A Analytical P_scene Reconstruction (Native Nodes)
    # =========================================================================
    p_sheet = create_expr(mat, unreal.MaterialExpressionWorldPosition, -2400, 780)
    cam_pos = create_expr(mat, unreal.MaterialExpressionCameraPositionWS, -2400, 900)
    scene_depth = create_expr(mat, unreal.MaterialExpressionSceneDepth, -2400, 1020)
    pixel_depth = create_expr(mat, unreal.MaterialExpressionPixelDepth, -2400, 1140)

    cam_to_sheet = create_expr(mat, unreal.MaterialExpressionSubtract, -2140, 820)
    connect_expr(p_sheet, "", cam_to_sheet, "A")
    connect_expr(cam_pos, "", cam_to_sheet, "B")

    depth_diff_raw = create_expr(mat, unreal.MaterialExpressionSubtract, -2140, 1020)
    connect_expr(scene_depth, "", depth_diff_raw, "A")
    connect_expr(pixel_depth, "", depth_diff_raw, "B")

    depth_diff = create_expr(mat, unreal.MaterialExpressionMax, -1960, 1020)
    depth_diff.set_editor_property("const_b", 0.0)
    connect_expr(depth_diff_raw, "", depth_diff, "A")

    safe_pixel_depth = create_expr(mat, unreal.MaterialExpressionMax, -1960, 1140)
    safe_pixel_depth.set_editor_property("const_b", 1.0)
    connect_expr(pixel_depth, "", safe_pixel_depth, "A")

    depth_ratio = create_expr(mat, unreal.MaterialExpressionDivide, -1780, 1060)
    connect_expr(depth_diff, "", depth_ratio, "A")
    connect_expr(safe_pixel_depth, "", depth_ratio, "B")

    delta_ray_ws = create_expr(mat, unreal.MaterialExpressionMultiply, -1580, 940)
    connect_expr(cam_to_sheet, "", delta_ray_ws, "A")
    connect_expr(depth_ratio, "", delta_ray_ws, "B")

    p_scene = create_expr(mat, unreal.MaterialExpressionAdd, -1380, 820)
    connect_expr(p_sheet, "", p_scene, "A")
    connect_expr(delta_ray_ws, "", p_scene, "B")

    ray_dist_sq_raw = create_expr(mat, unreal.MaterialExpressionDotProduct, -1380, 980)
    connect_expr(delta_ray_ws, "", ray_dist_sq_raw, "A")
    connect_expr(delta_ray_ws, "", ray_dist_sq_raw, "B")

    ray_dist_sq = create_expr(mat, unreal.MaterialExpressionMax, -1220, 980)
    ray_dist_sq.set_editor_property("const_b", 0.0)
    connect_expr(ray_dist_sq_raw, "", ray_dist_sq, "A")

    ray_dist_cm = create_expr(mat, unreal.MaterialExpressionSquareRoot, -1060, 980)
    connect_expr(ray_dist_sq, "", ray_dist_cm, "")

    # =========================================================================
    # Stage 4: Tilt-Rigid 3D OBB Cuttable Monolith Spatial Mask (P_scene & P_sheet)
    # =========================================================================
    mask_center_ws = make_vector_param(mat, "MonolithCutMaskCenterWS", 260.0, 0.0, 175.0, 1.0, -2400, 1340)
    mask_extent_ws = make_vector_param(mat, "MonolithCutMaskExtentWS", 16.0, 16.0, 45.0, 1.0, -2400, 1500)
    mask_axis_x = make_vector_param(mat, "MonolithCutMaskAxisXWS", 1.0, 0.0, 0.0, 0.0, -2400, 1660)
    mask_axis_y = make_vector_param(mat, "MonolithCutMaskAxisYWS", 0.0, 1.0, 0.0, 0.0, -2400, 1820)
    mask_axis_z = make_vector_param(mat, "MonolithCutMaskAxisZWS", 0.0, 0.0, 1.0, 0.0, -2400, 1980)
    sheet_pad_ws = make_vector_param(mat, "SheetMaskPadWS", 22.0, 6.0, 2.0, 0.0, -2400, 2140)
    cut_active_param = make_scalar_param(mat, "CutMaskActiveScalar", 0.0, -1060, 1280)
    scene_feather_param = make_scalar_param(mat, "SceneMaskFeatherCm", 0.65, -1380, 1620)

    ext_x = make_mask_channel(mat, mask_extent_ws, True, False, False, -2140, 1460)
    ext_y = make_mask_channel(mat, mask_extent_ws, False, True, False, -2140, 1540)
    ext_z = make_mask_channel(mat, mask_extent_ws, False, False, True, -2140, 1620)
    pad_x = make_mask_channel(mat, sheet_pad_ws, True, False, False, -2140, 2100)
    pad_y = make_mask_channel(mat, sheet_pad_ws, False, True, False, -2140, 2180)

    # P_scene OBB test
    delta_scene = create_expr(mat, unreal.MaterialExpressionSubtract, -1180, 1360)
    connect_expr(p_scene, "", delta_scene, "A")
    connect_expr(mask_center_ws, "", delta_scene, "B")

    sc_dot_x = create_expr(mat, unreal.MaterialExpressionDotProduct, -980, 1360)
    connect_expr(delta_scene, "", sc_dot_x, "A")
    connect_expr(mask_axis_x, "", sc_dot_x, "B")
    sc_abs_x = create_expr(mat, unreal.MaterialExpressionAbs, -840, 1360)
    connect_expr(sc_dot_x, "", sc_abs_x, "")
    sc_dist_x = create_expr(mat, unreal.MaterialExpressionSubtract, -700, 1360)
    connect_expr(sc_abs_x, "", sc_dist_x, "A")
    connect_expr(ext_x, "", sc_dist_x, "B")

    sc_dot_y = create_expr(mat, unreal.MaterialExpressionDotProduct, -980, 1480)
    connect_expr(delta_scene, "", sc_dot_y, "A")
    connect_expr(mask_axis_y, "", sc_dot_y, "B")
    sc_abs_y = create_expr(mat, unreal.MaterialExpressionAbs, -840, 1480)
    connect_expr(sc_dot_y, "", sc_abs_y, "")
    sc_dist_y = create_expr(mat, unreal.MaterialExpressionSubtract, -700, 1480)
    connect_expr(sc_abs_y, "", sc_dist_y, "A")
    connect_expr(ext_y, "", sc_dist_y, "B")

    sc_dot_z = create_expr(mat, unreal.MaterialExpressionDotProduct, -980, 1600)
    connect_expr(delta_scene, "", sc_dot_z, "A")
    connect_expr(mask_axis_z, "", sc_dot_z, "B")
    sc_abs_z = create_expr(mat, unreal.MaterialExpressionAbs, -840, 1600)
    connect_expr(sc_dot_z, "", sc_abs_z, "")
    sc_dist_z = create_expr(mat, unreal.MaterialExpressionSubtract, -700, 1600)
    connect_expr(sc_abs_z, "", sc_dist_z, "A")
    connect_expr(ext_z, "", sc_dist_z, "B")

    sc_max_xy = create_expr(mat, unreal.MaterialExpressionMax, -540, 1420)
    connect_expr(sc_dist_x, "", sc_max_xy, "A")
    connect_expr(sc_dist_y, "", sc_max_xy, "B")

    sc_box_dist = create_expr(mat, unreal.MaterialExpressionMax, -380, 1500)
    connect_expr(sc_max_xy, "", sc_box_dist, "A")
    connect_expr(sc_dist_z, "", sc_box_dist, "B")

    safe_feather = create_expr(mat, unreal.MaterialExpressionMax, -540, 1620)
    safe_feather.set_editor_property("const_b", 0.10)
    connect_expr(scene_feather_param, "", safe_feather, "A")

    sc_norm = create_expr(mat, unreal.MaterialExpressionDivide, -220, 1540)
    connect_expr(sc_box_dist, "", sc_norm, "A")
    connect_expr(safe_feather, "", sc_norm, "B")

    mask_scene = create_expr(mat, unreal.MaterialExpressionOneMinus, -60, 1540)
    connect_expr(make_clamp01(mat, sc_norm, -140, 1540), "", mask_scene, "")

    # P_sheet OBB test (XY padded + Upper Jaw Ceiling + Lower Monolith Bound per Correction A)
    delta_sheet = create_expr(mat, unreal.MaterialExpressionSubtract, -1180, 1860)
    connect_expr(p_sheet, "", delta_sheet, "A")
    connect_expr(mask_center_ws, "", delta_sheet, "B")

    sh_ext_x = create_expr(mat, unreal.MaterialExpressionAdd, -1020, 1760)
    connect_expr(ext_x, "", sh_ext_x, "A")
    connect_expr(pad_x, "", sh_ext_x, "B")

    sh_ext_y = create_expr(mat, unreal.MaterialExpressionAdd, -1020, 1840)
    connect_expr(ext_y, "", sh_ext_y, "A")
    connect_expr(pad_y, "", sh_ext_y, "B")

    sh_dot_x = create_expr(mat, unreal.MaterialExpressionDotProduct, -980, 1940)
    connect_expr(delta_sheet, "", sh_dot_x, "A")
    connect_expr(mask_axis_x, "", sh_dot_x, "B")
    sh_abs_x = create_expr(mat, unreal.MaterialExpressionAbs, -840, 1940)
    connect_expr(sh_dot_x, "", sh_abs_x, "")
    sh_dist_x = create_expr(mat, unreal.MaterialExpressionSubtract, -700, 1940)
    connect_expr(sh_abs_x, "", sh_dist_x, "A")
    connect_expr(sh_ext_x, "", sh_dist_x, "B")

    sh_dot_y = create_expr(mat, unreal.MaterialExpressionDotProduct, -980, 2060)
    connect_expr(delta_sheet, "", sh_dot_y, "A")
    connect_expr(mask_axis_y, "", sh_dot_y, "B")
    sh_abs_y = create_expr(mat, unreal.MaterialExpressionAbs, -840, 2060)
    connect_expr(sh_dot_y, "", sh_abs_y, "")
    sh_dist_y = create_expr(mat, unreal.MaterialExpressionSubtract, -700, 2060)
    connect_expr(sh_abs_y, "", sh_dist_y, "A")
    connect_expr(sh_ext_y, "", sh_dist_y, "B")

    sh_max_xy = create_expr(mat, unreal.MaterialExpressionMax, -540, 2000)
    connect_expr(sh_dist_x, "", sh_max_xy, "A")
    connect_expr(sh_dist_y, "", sh_max_xy, "B")

    sh_xy_norm = create_expr(mat, unreal.MaterialExpressionDivide, -380, 2000)
    sh_xy_norm.set_editor_property("const_b", 1.50)
    connect_expr(sh_max_xy, "", sh_xy_norm, "A")

    mask_sheet_xy = create_expr(mat, unreal.MaterialExpressionOneMinus, -100, 2000)
    connect_expr(make_clamp01(mat, sh_xy_norm, -240, 2000), "", mask_sheet_xy, "")

    sh_signed_z = create_expr(mat, unreal.MaterialExpressionDotProduct, -980, 2200)
    connect_expr(delta_sheet, "", sh_signed_z, "A")
    connect_expr(mask_axis_z, "", sh_signed_z, "B")

    # Upper jaw ceiling: LocalSheet.Z <= ExtentWS.Z + 0.5
    sh_upper_excess = create_expr(mat, unreal.MaterialExpressionSubtract, -760, 2180)
    connect_expr(sh_signed_z, "", sh_upper_excess, "A")
    connect_expr(ext_z, "", sh_upper_excess, "B")

    sh_upper_norm = create_expr(mat, unreal.MaterialExpressionDivide, -600, 2180)
    sh_upper_norm.set_editor_property("const_b", 0.50)
    connect_expr(sh_upper_excess, "", sh_upper_norm, "A")

    mask_sheet_upper_z = create_expr(mat, unreal.MaterialExpressionOneMinus, -300, 2180)
    connect_expr(make_clamp01(mat, sh_upper_norm, -450, 2180), "", mask_sheet_upper_z, "")

    # Lower monolith bound: LocalSheet.Z >= -ExtentWS.Z - 2.0
    neg_ext_z = create_expr(mat, unreal.MaterialExpressionMultiply, -920, 2320)
    neg_ext_z.set_editor_property("const_b", -1.0)
    connect_expr(ext_z, "", neg_ext_z, "A")

    lower_z_start = create_expr(mat, unreal.MaterialExpressionSubtract, -760, 2320)
    lower_z_start.set_editor_property("const_b", 1.20)
    connect_expr(neg_ext_z, "", lower_z_start, "A")

    sh_lower_excess = create_expr(mat, unreal.MaterialExpressionSubtract, -600, 2320)
    connect_expr(lower_z_start, "", sh_lower_excess, "A")
    connect_expr(sh_signed_z, "", sh_lower_excess, "B")

    sh_lower_norm = create_expr(mat, unreal.MaterialExpressionDivide, -450, 2320)
    sh_lower_norm.set_editor_property("const_b", 0.80)
    connect_expr(sh_lower_excess, "", sh_lower_norm, "A")

    mask_sheet_lower_z = create_expr(mat, unreal.MaterialExpressionOneMinus, -160, 2320)
    connect_expr(make_clamp01(mat, sh_lower_norm, -300, 2320), "", mask_sheet_lower_z, "")

    mask_z_both = create_expr(mat, unreal.MaterialExpressionMultiply, 40, 2240)
    connect_expr(mask_sheet_upper_z, "", mask_z_both, "A")
    connect_expr(mask_sheet_lower_z, "", mask_z_both, "B")

    mask_sheet_all = create_expr(mat, unreal.MaterialExpressionMultiply, 180, 2080)
    connect_expr(mask_sheet_xy, "", mask_sheet_all, "A")
    connect_expr(mask_z_both, "", mask_sheet_all, "B")

    mask_scene_sheet = create_expr(mat, unreal.MaterialExpressionMultiply, 320, 1780)
    connect_expr(mask_scene, "", mask_scene_sheet, "A")
    connect_expr(mask_sheet_all, "", mask_scene_sheet, "B")

    active_clamped = make_clamp01(mat, cut_active_param, 180, 1400)
    mask_with_active = create_expr(mat, unreal.MaterialExpressionMultiply, 460, 1620)
    connect_expr(mask_scene_sheet, "", mask_with_active, "A")
    connect_expr(active_clamped, "", mask_with_active, "B")

    spatial_cut_mask = create_expr(mat, unreal.MaterialExpressionMultiply, 620, 1480)
    connect_expr(mask_with_active, "", spatial_cut_mask, "A")
    connect_expr(inv_border_mask, "", spatial_cut_mask, "B")

    # =========================================================================
    # Stage 5: Exact Plane-Orthogonal Dual-Zone Incandescent Contact Seam
    # PlaneDistCm = |(P_scene - P_sheet) . N_sheet| = |Dot(DeltaRayWS, VertexNormalWS)|
    # Exact 3D perpendicular distance from monolith surface to laser plane across
    # all grazing angles (1..90 deg) and beveled roll/pitch orientations!
    # =========================================================================
    vert_normal = create_expr(mat, unreal.MaterialExpressionVertexNormalWS, -1480, 620)

    plane_dist_signed = create_expr(mat, unreal.MaterialExpressionDotProduct, -1260, 660)
    connect_expr(delta_ray_ws, "", plane_dist_signed, "A")
    connect_expr(vert_normal, "", plane_dist_signed, "B")

    plane_dist_cm = create_expr(mat, unreal.MaterialExpressionAbs, -1080, 660)
    connect_expr(plane_dist_signed, "", plane_dist_cm, "")

    min_grazing_param = make_scalar_param(mat, "MinGrazingScale", 0.055, -1260, 520)
    safe_min_grazing = create_expr(mat, unreal.MaterialExpressionMax, -1080, 520)
    safe_min_grazing.set_editor_property("const_b", 0.02)
    connect_expr(min_grazing_param, "", safe_min_grazing, "A")

    min_ray_bound_cm = create_expr(mat, unreal.MaterialExpressionMultiply, -880, 560)
    connect_expr(ray_dist_cm, "", min_ray_bound_cm, "A")
    connect_expr(safe_min_grazing, "", min_ray_bound_cm, "B")

    dist_grazing = create_expr(mat, unreal.MaterialExpressionMax, -640, 820)
    connect_expr(plane_dist_cm, "", dist_grazing, "A")
    connect_expr(min_ray_bound_cm, "", dist_grazing, "B")

    charge_w_mul = create_expr(mat, unreal.MaterialExpressionMultiply, -780, 720)
    charge_w_mul.set_editor_property("const_b", 0.30)
    connect_expr(charge_01, "", charge_w_mul, "A")
    charge_w_scale = create_expr(mat, unreal.MaterialExpressionAdd, -600, 720)
    charge_w_scale.set_editor_property("const_b", 1.0)
    connect_expr(charge_w_mul, "", charge_w_scale, "A")

    effective_dist_cm = create_expr(mat, unreal.MaterialExpressionDivide, -420, 800)
    connect_expr(dist_grazing, "", effective_dist_cm, "A")
    connect_expr(charge_w_scale, "", effective_dist_cm, "B")

    core_width_param = make_scalar_param(mat, "CoreSeamWidthCm", 1.05, -600, 940)
    shoulder_width_param = make_scalar_param(mat, "ShoulderSeamWidthCm", 2.45, -600, 1060)

    safe_core_w = create_expr(mat, unreal.MaterialExpressionMax, -420, 940)
    safe_core_w.set_editor_property("const_b", 0.20)
    connect_expr(core_width_param, "", safe_core_w, "A")

    safe_shoulder_w = create_expr(mat, unreal.MaterialExpressionMax, -420, 1060)
    safe_shoulder_w.set_editor_property("const_b", 0.50)
    connect_expr(shoulder_width_param, "", safe_shoulder_w, "A")

    core_norm = create_expr(mat, unreal.MaterialExpressionDivide, -240, 880)
    connect_expr(effective_dist_cm, "", core_norm, "A")
    connect_expr(safe_core_w, "", core_norm, "B")
    core_falloff = create_expr(mat, unreal.MaterialExpressionOneMinus, 40, 880)
    connect_expr(make_clamp01(mat, core_norm, -100, 880), "", core_falloff, "")
    core_pow = create_expr(mat, unreal.MaterialExpressionPower, 200, 880)
    core_pow.set_editor_property("const_exponent", 1.35)
    connect_expr(core_falloff, "", core_pow, "Base")
    core_mask = create_expr(mat, unreal.MaterialExpressionMultiply, 780, 920)
    connect_expr(core_pow, "", core_mask, "A")
    connect_expr(spatial_cut_mask, "", core_mask, "B")

    sh_norm = create_expr(mat, unreal.MaterialExpressionDivide, -240, 1040)
    connect_expr(effective_dist_cm, "", sh_norm, "A")
    connect_expr(safe_shoulder_w, "", sh_norm, "B")
    sh_falloff = create_expr(mat, unreal.MaterialExpressionOneMinus, 40, 1040)
    connect_expr(make_clamp01(mat, sh_norm, -100, 1040), "", sh_falloff, "")
    sh_pow = create_expr(mat, unreal.MaterialExpressionPower, 200, 1040)
    sh_pow.set_editor_property("const_exponent", 1.85)
    connect_expr(sh_falloff, "", sh_pow, "Base")
    shoulder_mask = create_expr(mat, unreal.MaterialExpressionMultiply, 780, 1080)
    connect_expr(sh_pow, "", shoulder_mask, "A")
    connect_expr(spatial_cut_mask, "", shoulder_mask, "B")

    # Seam Palette (Warm Incandescent #FFF6D6 + Molten Gold-Orange vs White-Crimson Vein Intersect)
    seam_core_col = make_vector_param(mat, "SeamCoreColor", 6.20, 5.10, 2.90, 1.0, 420, 460)
    seam_sh_col = make_vector_param(mat, "SeamShoulderColor", 3.60, 1.02, 0.10, 1.0, 420, 620)
    vein_core_col = make_vector_param(mat, "VeinHitCoreColor", 6.80, 1.80, 1.60, 1.0, 420, 760)
    vein_sh_col = make_vector_param(mat, "VeinHitShoulderColor", 4.60, 0.12, 0.06, 1.0, 420, 900)
    vein_hit_param = make_scalar_param(mat, "DirectVeinHitScalar", 0.0, 420, 360)
    vein_hit_01 = make_clamp01(mat, vein_hit_param, 600, 360)

    active_core_col = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, 780, 520)
    connect_expr(seam_core_col, "", active_core_col, "A")
    connect_expr(vein_core_col, "", active_core_col, "B")
    connect_expr(vein_hit_01, "", active_core_col, "Alpha")

    active_sh_col = create_expr(mat, unreal.MaterialExpressionLinearInterpolate, 780, 700)
    connect_expr(seam_sh_col, "", active_sh_col, "A")
    connect_expr(vein_sh_col, "", active_sh_col, "B")
    connect_expr(vein_hit_01, "", active_sh_col, "Alpha")

    core_contrib = create_expr(mat, unreal.MaterialExpressionMultiply, 980, 620)
    connect_expr(active_core_col, "", core_contrib, "A")
    connect_expr(core_mask, "", core_contrib, "B")

    sh_contrib = create_expr(mat, unreal.MaterialExpressionMultiply, 980, 780)
    connect_expr(active_sh_col, "", sh_contrib, "A")
    connect_expr(shoulder_mask, "", sh_contrib, "B")

    seam_raw_emissive = create_expr(mat, unreal.MaterialExpressionAdd, 1160, 700)
    connect_expr(core_contrib, "", seam_raw_emissive, "A")
    connect_expr(sh_contrib, "", seam_raw_emissive, "B")

    charge_seam_mul = create_expr(mat, unreal.MaterialExpressionMultiply, 980, 440)
    charge_seam_mul.set_editor_property("const_b", 1.35)
    connect_expr(charge_01, "", charge_seam_mul, "A")
    charge_seam_boost = create_expr(mat, unreal.MaterialExpressionAdd, 1160, 440)
    charge_seam_boost.set_editor_property("const_b", 1.0)
    connect_expr(charge_seam_mul, "", charge_seam_boost, "A")

    seam_emissive = create_expr(mat, unreal.MaterialExpressionMultiply, 1340, 620)
    connect_expr(seam_raw_emissive, "", seam_emissive, "A")
    connect_expr(charge_seam_boost, "", seam_emissive, "B")

    final_emissive = create_expr(mat, unreal.MaterialExpressionAdd, 1540, -100)
    connect_expr(base_sheet_emissive, "", final_emissive, "A")
    connect_expr(seam_emissive, "", final_emissive, "B")
    connect_prop(final_emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    core_op_part = create_expr(mat, unreal.MaterialExpressionMultiply, 980, 1020)
    core_op_part.set_editor_property("const_b", 0.42)
    connect_expr(core_mask, "", core_op_part, "A")

    sh_op_part = create_expr(mat, unreal.MaterialExpressionMultiply, 980, 1140)
    sh_op_part.set_editor_property("const_b", 0.52)
    connect_expr(shoulder_mask, "", sh_op_part, "A")

    seam_op_sum = create_expr(mat, unreal.MaterialExpressionAdd, 1160, 1080)
    connect_expr(core_op_part, "", seam_op_sum, "A")
    connect_expr(sh_op_part, "", seam_op_sum, "B")

    seam_opacity = create_expr(mat, unreal.MaterialExpressionClamp, 1340, 1080)
    seam_opacity.set_editor_property("min_default", 0.0)
    seam_opacity.set_editor_property("max_default", 0.94)
    connect_expr(seam_op_sum, "", seam_opacity, "")

    final_opacity = create_expr(mat, unreal.MaterialExpressionMax, 1540, 320)
    connect_expr(base_sheet_opacity, "", final_opacity, "A")
    connect_expr(seam_opacity, "", final_opacity, "B")
    connect_prop(final_opacity, "", unreal.MaterialProperty.MP_OPACITY)


def main() -> None:
    if not unreal.EditorAssetLibrary.does_asset_exist(LASER_SHEET_PATH):
        raise RuntimeError(f"Missing target material asset: {LASER_SHEET_PATH}")

    loaded = unreal.EditorAssetLibrary.load_asset(LASER_SHEET_PATH)
    if not isinstance(loaded, unreal.Material):
        raise RuntimeError(f"Asset {LASER_SHEET_PATH} is {type(loaded)}, expected unreal.Material")

    mat: unreal.Material = loaded
    unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
    populate_phase12c_laser_sheet_graph(mat)

    unreal.MaterialEditingLibrary.set_base_material_usage(
        mat, unreal.MaterialUsage.MATUSAGE_STATIC_MESH, True
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
        raise RuntimeError(f"Failed saving {LASER_SHEET_PATH}")

    log_info(
        f"PHASE12C_SHADER_OK path={LASER_SHEET_PATH} ps_instructions={ps_instr} vs_instructions={vs_instr}"
    )


if __name__ == "__main__":
    main()
