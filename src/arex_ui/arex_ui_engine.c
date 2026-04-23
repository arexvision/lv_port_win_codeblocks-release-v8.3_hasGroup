#include "arex_ui_engine.h"
#include "arex_data.h"
#include <stdio.h>
#include <string.h>

/* g_sys_config 定义于此，g_sensor_data 定义于 arex_data.c */
arex_sys_config_t g_sys_config;

/* =========================================================
 * 默认配置值 (全字段初始化 — APP 同步就绪)
 * ========================================================= */
void arex_sys_config_defaults(arex_sys_config_t *cfg)
{
    memset(cfg, 0, sizeof(arex_sys_config_t));

    /* 安全区 */
    cfg->safe_zone_w  = 580;
    cfg->safe_zone_h  = 400;
    cfg->offset_x     = 0;
    cfg->offset_y     = 0;

    /* 架构 */
    cfg->theme_mode    = AREX_THEME_TECH;
    cfg->layout_order  = AREX_ORDER_NORMAL;
    cfg->dots_position = AREX_DOTS_RIGHT;
    cfg->compass_style = AREX_COMPASS_CLASSIC;
    cfg->flash_speed   = 1;
    cfg->mask_enabled  = false;

    /* 字体 */
    cfg->font_sz_huge  = 58;
    cfg->font_sz_med   = 28;
    cfg->font_sz_small = 14;
    cfg->align_title   = AREX_ALIGN_LEFT;
    cfg->align_huge    = AREX_ALIGN_LEFT;
    cfg->align_med     = AREX_ALIGN_LEFT;
    cfg->split_outward = true;

    /* 分割线 */
    cfg->sep_style  = AREX_SEP_DASHED;
    cfg->sep_thick  = 1;
    cfg->sep_alpha  = 51;   /* 20% of 255 */

    /* 10U 高度分配 (单位 U，1U = 10px) */
    cfg->h_depth  = 8;   /* DEPTH 大通栏: 8U=80px */
    cfg->h_ndl    = 6;   /* NDL/TTS 双拼: 6U=60px */
    cfg->h_pod    = 6;   /* POD 1/2 双拼: 6U=60px */
    cfg->h_batt   = 5;   /* BATT/W.TIME 双拼: 5U=50px */
    cfg->h_gas    = 6;   /* GAS 中通栏: 6U=60px */
    cfg->h_time   = 5;   /* DIVE TIME 底部: 5U=50px */
    cfg->gap_u         = 1;   /* 模块间距: 1U=10px */
    cfg->title_h_u     = 2;   /* 标题高度: 2U=20px */
    cfg->h_menu_item   = 5;   /* 菜单项高度: 5U=50px */
    cfg->gap_menu      = 1;   /* 菜单项间距: 1U=10px */
    cfg->h_tissues_chart = 9; /* 组织柱图高度: 9U=90px */

    /* =====================================================
     * 左侧锚点行配置 (APP 同步就绪 — 自由双拼)
     *
     * 每行定义: {左模块, 右模块, h_u, title_h_u, val_font, val_align, sep_style, sep_thick}
     * right_module = AREX_MODULE_EMPTY → 左侧独占全宽(160px)
     * right_module != EMPTY → 双拼布局(各80px)
     *
     * 字号: 0=SMALL 1=MEDIUM 2=TITLE 3=HUGE
     * 对齐: 0=LEFT 1=CENTER 2=RIGHT
     *
     * ===================================================== */
    static const arex_left_row_cfg_t def_layout[AREX_MAX_LEFT_ROWS] = {
        /* row 0: DEPTH 单栏全宽 */
        { AREX_MODULE_DEPTH, AREX_MODULE_EMPTY, 8, 2, 3, 0, AREX_SEP_DASHED, 0 },
        /* row 1: NDL + TTS 双拼 */
        { AREX_MODULE_NDL,  AREX_MODULE_TTS,  6, 2, 1, 0, AREX_SEP_DASHED, 0 },
        /* row 2: POD1 + POD2 双拼 */
        { AREX_MODULE_POD1, AREX_MODULE_POD2, 6, 2, 2, 0, AREX_SEP_DASHED, 0 },
        /* row 3: BATT + WTM 双拼 */
        { AREX_MODULE_BATT, AREX_MODULE_WTM,  5, 2, 0, 0, AREX_SEP_DASHED, 0 },
        /* row 4: GAS 单栏全宽 */
        { AREX_MODULE_GAS,  AREX_MODULE_EMPTY, 6, 2, 1, 0, AREX_SEP_DASHED, 0 },
        /* row 5: TIME 单栏全宽 */
        { AREX_MODULE_TIME, AREX_MODULE_EMPTY, 5, 2, 0, 0, AREX_SEP_DASHED, 0 },
        /* row 6-7: EMPTY */
        { AREX_MODULE_EMPTY, AREX_MODULE_EMPTY, 0, 0, 0, 0, AREX_SEP_NONE,   0 },
        { AREX_MODULE_EMPTY, AREX_MODULE_EMPTY, 0, 0, 0, 0, AREX_SEP_NONE,   0 },
    };
    for (uint8_t i = 0; i < AREX_MAX_LEFT_ROWS; i++) {
        cfg->left_layout[i] = def_layout[i];
    }

    /* 默认 5F 网格布局 */
    cfg->widget_count = 6;
    cfg->widget_ids[0] = 0;  cfg->widget_w[0] = 2; cfg->widget_h[0] = 2;
    cfg->widget_ids[1] = 1;  cfg->widget_w[1] = 2; cfg->widget_h[1] = 1;
    cfg->widget_ids[2] = 2;  cfg->widget_w[2] = 1; cfg->widget_h[2] = 1;
    cfg->widget_ids[3] = 3;  cfg->widget_w[3] = 2; cfg->widget_h[3] = 2;
    cfg->widget_ids[4] = 4;  cfg->widget_w[4] = 2; cfg->widget_h[4] = 1;
    cfg->widget_ids[5] = 5;  cfg->widget_w[5] = 1; cfg->widget_h[5] = 1;
}

/* =========================================================
 * 安全区边界检测
 * ========================================================= */
bool arex_safe_zone_in_danger(void)
{
    int16_t max_offset_x = (int16_t)((AREX_PHYSICAL_W - g_sys_config.safe_zone_w) / 2);
    int16_t max_offset_y = (int16_t)((AREX_PHYSICAL_H - g_sys_config.safe_zone_h) / 2);

    if (g_sys_config.offset_x < -max_offset_x || g_sys_config.offset_x > max_offset_x)
        return true;
    if (g_sys_config.offset_y < -max_offset_y || g_sys_config.offset_y > max_offset_y)
        return true;

    /* 面镜盲区掩膜检测 */
    if (g_sys_config.mask_enabled) {
        int16_t bottom_edge = (int16_t)(AREX_PHYSICAL_H / 2 + g_sys_config.safe_zone_h / 2 + g_sys_config.offset_y);
        if (bottom_edge > AREX_PHYSICAL_H - AREX_MASK_EDGE_GUARD)
            return true;
    }

    return false;
}

/* =========================================================
 * 辅助：计算 Safe Zone 内部可用区域
 * ========================================================= */
void arex_calc_layout_rect(int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h,
                           int16_t anchor_offset_x, int16_t anchor_offset_y)
{
    /* 以屏幕中心为原点，应用安全区偏移 */
    int16_t center_x = (int16_t)(AREX_PHYSICAL_W / 2) + anchor_offset_x;
    int16_t center_y = (int16_t)(AREX_PHYSICAL_H / 2) + anchor_offset_y;

    *out_x = center_x - (int16_t)(g_sys_config.safe_zone_w / 2);
    *out_y = center_y - (int16_t)(g_sys_config.safe_zone_h / 2);
    *out_w = g_sys_config.safe_zone_w;
    *out_h = g_sys_config.safe_zone_h;
}

/* =========================================================
 * Tech 模式绝对坐标推算
 *
 * 左锚点: (0, 0), 宽=160px, 高=全 safe_zone_h
 * 右卡片: (160+gap, 0), 宽=safe_zone_w-160-gap, 高=全 safe_zone_h
 * 翻转时: 交换左右 X
 * ========================================================= */
void arex_calc_tech_layout(int16_t *out_lx, int16_t *out_ly,
                           uint16_t *out_lw, uint16_t *out_lh,
                           int16_t *out_rx, int16_t *out_ry,
                           uint16_t *out_rw, uint16_t *out_rh)
{
    uint16_t gap = g_sys_config.gap_u * AREX_BASE_U;

    if (g_sys_config.layout_order == AREX_ORDER_NORMAL) {
        *out_lx = 0;
        *out_rx = (int16_t)(AREX_LEFT_ANCHOR_W + gap);
    } else {
        *out_lx = (int16_t)(g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W - gap);
        *out_rx = 0;
    }

    *out_ly = 0;
    *out_ry = 0;

    *out_lw = AREX_LEFT_ANCHOR_W;
    *out_lh = g_sys_config.safe_zone_h;

    *out_rw = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W - gap;
    *out_rh = g_sys_config.safe_zone_h;
}

/* =========================================================
 * Classic 模式绝对坐标推算
 *
 * 上区: (0, 0), 宽=safe_zone_w, 高=按 10U 累加计算
 * 下区: (0, top_h+gap), 宽=safe_zone_w, 高=safe_zone_h-top_h-gap
 * 翻转时: 交换上下 Y
 * ========================================================= */
void arex_calc_classic_layout(int16_t *out_top_x, int16_t *out_top_y,
                              uint16_t *out_top_w, uint16_t *out_top_h,
                              int16_t *out_bot_x, int16_t *out_bot_y,
                              uint16_t *out_bot_w, uint16_t *out_bot_h)
{
    uint16_t gap = g_sys_config.gap_u * AREX_BASE_U;

    /* 计算上区总高度 = 各模块高度累加 */
    uint16_t top_h = 0;
    top_h += g_sys_config.h_depth * AREX_BASE_U;               /* DEPTH */
    top_h += g_sys_config.h_ndl * AREX_BASE_U + gap;            /* NDL/TTS 双拼 */
    top_h += g_sys_config.h_pod * AREX_BASE_U + gap;            /* POD 双拼 */
    top_h += g_sys_config.h_batt * AREX_BASE_U + gap;           /* BATT 双拼 */
    top_h += g_sys_config.h_gas * AREX_BASE_U + gap;            /* GAS */
    top_h += g_sys_config.h_time * AREX_BASE_U;                 /* DIVE TIME */

    /* 零高度保护：最小 AREX_MIN_CLASSIC_TOP_H px */
    if (top_h < AREX_MIN_CLASSIC_TOP_H) top_h = AREX_MIN_CLASSIC_TOP_H;

    uint16_t bottom_h = (g_sys_config.safe_zone_h > top_h + gap)
                         ? (g_sys_config.safe_zone_h - top_h - gap)
                         : AREX_MIN_CLASSIC_TOP_H;

    if (g_sys_config.layout_order == AREX_ORDER_NORMAL) {
        *out_top_x = 0;
        *out_bot_x = 0;
        *out_top_y = 0;
        *out_bot_y = (int16_t)(top_h + gap);
    } else {
        *out_top_x = 0;
        *out_bot_x = 0;
        *out_top_y = (int16_t)(bottom_h + gap);
        *out_bot_y = 0;
    }

    *out_top_w = g_sys_config.safe_zone_w;
    *out_top_h = top_h;
    *out_bot_w = g_sys_config.safe_zone_w;
    *out_bot_h = bottom_h;
}

/* =========================================================
 * 10U 左侧锚点组件 Y 坐标推算 (自由双拼版本)
 *
 * 算法：
 *   遍历 g_sys_config.left_layout[] 每一行
 *   ├─ left_module == EMPTY：跳过该行（不留空洞）
 *   ├─ right_module == EMPTY：单栏布局 → 1 个 comps 入口，w=160px
 *   └─ right_module != EMPTY：双拼布局 → 2 个 comps 入口，w=80px，左块 split=1，右块 split=2
 *
 * 输出 comps[] 有效入口数量由填充的 count 参数返回（最多 ANCHOR_COMP_COUNT）
 *
 * 绝对禁止在引擎代码里写任何 "如果是 NDL 就配 TTS" 的硬编码！
 * 所有组合关系由 left_layout[] 数组决定。
 * ========================================================= */

/* 模块枚举 → 默认高度(U) */
static uint8_t module_default_hu(arex_left_module_t mod)
{
    switch (mod) {
        case AREX_MODULE_DEPTH:  return 8;  /* DEPTH 默认 8U */
        case AREX_MODULE_NDL:
        case AREX_MODULE_TTS:
        case AREX_MODULE_POD1:
        case AREX_MODULE_POD2: return 6;  /* 双拼默认 6U */
        case AREX_MODULE_BATT:
        case AREX_MODULE_WTM:  return 5;  /* 双拼默认 5U */
        case AREX_MODULE_GAS:   return 6;  /* GAS 默认 6U */
        case AREX_MODULE_TIME:  return 5;  /* TIME 默认 5U */
        default: return 0;
    }
}

void arex_calc_anchor_layout(arex_anchor_comp_t comps[ANCHOR_COMP_COUNT],
                             uint16_t *out_total_h, uint8_t *out_count)
{
    uint16_t gap       = g_sys_config.gap_u * AREX_BASE_U;
    uint16_t half_w    = AREX_LEFT_ANCHOR_W / 2;   /* 80px */
    int16_t  cur_y    = 0;
    uint8_t  out_idx  = 0;

    memset(comps, 0, sizeof(arex_anchor_comp_t) * ANCHOR_COMP_COUNT);

    /* 遍历每一行配置 */
    for (uint8_t row = 0; row < AREX_MAX_LEFT_ROWS && out_idx < ANCHOR_COMP_COUNT; row++) {
        arex_left_module_t left_mod  = (arex_left_module_t)g_sys_config.left_layout[row].left_module;
        arex_left_module_t right_mod = (arex_left_module_t)g_sys_config.left_layout[row].right_module;

        if (left_mod == AREX_MODULE_EMPTY) continue;

        /* 高度：优先用 row 配置的 h_u，否则查模块默认值 */
        uint8_t h_u = g_sys_config.left_layout[row].h_u;
        if (h_u == 0) h_u = module_default_hu(left_mod);
        if (h_u == 0) h_u = 6; /* 保底 6U */

        uint16_t h_px  = h_u * AREX_BASE_U;
        uint16_t t_h_u = (g_sys_config.left_layout[row].title_h_u > 0)
                          ? g_sys_config.left_layout[row].title_h_u
                          : g_sys_config.title_h_u;
        uint16_t t_h   = t_h_u * AREX_BASE_U;
        uint16_t v_h   = (h_px >= t_h) ? (h_px - t_h) : 0;

        /* 获取该行样式（优先 row 配置，否则用全局默认值） */
        uint8_t val_font   = g_sys_config.left_layout[row].val_font;
        uint8_t val_align  = g_sys_config.left_layout[row].val_align;

        /* 单栏：left_module 独占整行宽 160px */
        if (right_mod == AREX_MODULE_EMPTY) {
            comps[out_idx].module      = left_mod;
            comps[out_idx].y          = cur_y;
            comps[out_idx].h          = h_px;
            comps[out_idx].title_h    = t_h;
            comps[out_idx].val_h      = v_h;
            comps[out_idx].w          = AREX_LEFT_ANCHOR_W;  /* 160px */
            comps[out_idx].split      = 0;
            comps[out_idx].val_font   = val_font;
            comps[out_idx].val_align  = val_align;
            out_idx++;
        }
        /* 双拼：左右各占 80px */
        else {
            /* 左块 */
            comps[out_idx].module     = left_mod;
            comps[out_idx].y         = cur_y;
            comps[out_idx].h         = h_px;
            comps[out_idx].title_h   = t_h;
            comps[out_idx].val_h     = v_h;
            comps[out_idx].w         = half_w;   /* 80px */
            comps[out_idx].split     = 1;        /* 双拼左 */
            comps[out_idx].val_font  = val_font;
            comps[out_idx].val_align = AREX_ALIGN_LEFT;
            out_idx++;

            /* 右块 */
            if (out_idx < ANCHOR_COMP_COUNT) {
                /* right_module 的字体从 left_layout 的下一个同名模块配置中找，
                 * 这里简化处理：复用当前行的 val_font，右块靠右对齐 */
                comps[out_idx].module     = right_mod;
                comps[out_idx].y         = cur_y;
                comps[out_idx].h         = h_px;
                comps[out_idx].title_h   = t_h;
                comps[out_idx].val_h      = v_h;
                comps[out_idx].w         = half_w;   /* 80px */
                comps[out_idx].split      = 2;        /* 双拼右 */
                comps[out_idx].val_font   = val_font;
                comps[out_idx].val_align = AREX_ALIGN_RIGHT;
                out_idx++;
            }
        }

        cur_y += h_px + gap;
    }

    *out_total_h = cur_y;
    if (out_count) *out_count = out_idx;
}

/* =========================================================
 * 5x6 网格布局推算
 *
 * 计算每个 widget 单元格的绝对位置。
 * 收到 row(0~5), col(0~4), w_span(1~2), h_span(1~2)
 * 直接算出 X = col * unit_w, Y = row * unit_h
 * ========================================================= */
void arex_calc_widget_cell(uint16_t parent_w, uint16_t parent_h,
                          uint8_t row, uint8_t col,
                          uint8_t w_span, uint8_t h_span,
                          int16_t *out_x, int16_t *out_y,
                          uint16_t *out_w, uint16_t *out_h)
{
    uint16_t unit_w = parent_w / AREX_WIDGET_COLS;  /* e.g. 92px if parent=460 */
    uint16_t unit_h = parent_h / AREX_WIDGET_ROWS;   /* e.g. 80px if parent=480 */

    *out_x = (int16_t)(col * unit_w);
    *out_y = (int16_t)(row * unit_h);
    *out_w = w_span * unit_w;
    *out_h = h_span * unit_h;

    /* 边界修正，防止越界 */
    if (*out_x + *out_w > parent_w) *out_w = parent_w - *out_x;
    if (*out_y + *out_h > parent_h) *out_h = parent_h - *out_y;
}

/* =========================================================
 * 16 柱组织图 X 坐标推算
 *
 * 底部对齐，16 等分柱状图。
 * 每根柱宽 = total_w / 16，X = i * col_w
 * ========================================================= */
void arex_calc_tissue_bars(uint16_t total_w, uint16_t bar_max_h,
                           int16_t out_x[16], uint16_t out_w[16])
{
    uint16_t col_w = total_w / 16;
    for (uint8_t i = 0; i < 16; i++) {
        out_x[i] = (int16_t)(i * col_w);
        out_w[i] = col_w;
    }
    (void)bar_max_h; /* 柱高由调用方按百分比算出，此处只返回 X 和 W */
}

/* =========================================================
 * LVGL 样式辅助
 * ========================================================= */

/* 将 AREX_ALIGN_* 转换为 LVGL 对齐方式 */
lv_text_align_t arex_align_to_lv(uint8_t align)
{
    if (align == AREX_ALIGN_LEFT)   return LV_TEXT_ALIGN_LEFT;
    if (align == AREX_ALIGN_CENTER) return LV_TEXT_ALIGN_CENTER;
    return LV_TEXT_ALIGN_RIGHT;
}

/* 将对齐转换为 LVGL ALIGN 常量 */
lv_align_t arex_align_to_lv_align(uint8_t align)
{
    if (align == AREX_ALIGN_LEFT)   return LV_ALIGN_LEFT_MID;
    if (align == AREX_ALIGN_CENTER) return LV_ALIGN_CENTER;
    return LV_ALIGN_RIGHT_MID;
}

/* 根据字号获取字体指针 */
const lv_font_t *arex_get_font(uint8_t size_cat)
{
    /* 静态声明字号，依赖 arex_fonts.h 中的字体 */
    extern const lv_font_t lv_font_courier_48;
    extern const lv_font_t lv_font_courier_28;
    extern const lv_font_t lv_font_courier_20;
    extern const lv_font_t lv_font_courier_14;

    switch (size_cat) {
        case 0: return &lv_font_courier_48; /* huge */
        case 1: return &lv_font_courier_28; /* med  */
        case 2: return &lv_font_courier_20; /* small */
        default: return &lv_font_courier_14;
    }
}

/* =========================================================
 * JSON 配置解析 (用于 App 蓝牙同步 / SETUP 导入)
 * ========================================================= */
/*
 * 当接收到 JSON 配置时，按以下流程处理：
 *
 * 1. 解析 JSON 到临时结构体
 * 2. 调用 memcpy(&g_sys_config, &tmp, sizeof(...)) 覆盖
 * 3. 调用 arex_ui_apply_config() 重排 UI
 *
 * JSON 字段示例 (与 HTML configIds 对应):
 * {
 *   "theme_mode": "tech",
 *   "safe_zone_w": 580,
 *   "h_depth": 8,
 *   "h_ndl": 6,
 *   "widget_ids": [0, 1, 2, 3, 4, 5],
 *   "widget_w":  [2, 2, 1, 2, 2, 1],
 *   "widget_h":  [2, 1, 1, 2, 1, 1],
 *   ...
 * }
 */

/* =========================================================
 * 初始化入口 (由 UI_main 调用)
 * ========================================================= */
void arex_ui_init(void)
{
    arex_sys_config_defaults(&g_sys_config);
    memset(&g_sensor_data, 0, sizeof(g_sensor_data));

    /* 初始化默认潜水数据 */
    g_sensor_data.depth = 45.2f;
    g_sensor_data.ndl  = 5;
    g_sensor_data.tts  = 24;
    g_sensor_data.pod1_bar = 210.0f;
    g_sensor_data.pod2_bar = 195.0f;
    g_sensor_data.battery_pct = 85.0f;
    g_sensor_data.heading = 265;
    g_sensor_data.dive_time_s = 2294; /* 38:14 */
    g_sensor_data.gas_active_idx = 2;
    strcpy(g_sensor_data.gas_name, "TX 18/45");
    g_sensor_data.ppo2[0] = 1.2f;
    g_sensor_data.ppo2[1] = 1.2f;
    g_sensor_data.ppo2[2] = 1.3f;
    g_sensor_data.cns_pct = 15;
    g_sensor_data.otu = 22;
    g_sensor_data.next_stop_m = 21;
    g_sensor_data.next_stop_min = 3;

    /* 模拟组织饱和度数据 */
    for (uint8_t i = 0; i < 16; i++) {
        g_sensor_data.tissue_pct[i] = (i < 8) ? (95 - i * 10) : (i - 7);
    }
}

/* =========================================================
 * 应用配置变更 (配置界面修改后调用此函数)
 * 1. 检测 safe zone 边界
 * 2. 重建 left anchor 排版
 * 3. 重建 right card 布局
 * ========================================================= */
void arex_ui_apply_config(void)
{
    /* 安全区边界校验 */
    if (arex_safe_zone_in_danger()) {
        /* TODO: 触发危险警告 UI */
    }

    /* TODO: 触发 arex_screen_rebuild_layout() 重建排版
     * 这将在重构 arex_screen.c 时实现
     */
}

/* =========================================================
 * 定时数据更新 (由 lv_timer 以 1Hz/2Hz 调用)
 * 仅更新 lv_label 文字，绝不触发排版重构
 * ========================================================= */
void arex_ui_update_data(void)
{
    /* 由调用方在 arex_screen.c 中实现具体的 lv_label_set_text 调用
     * 此函数作为空钩子存在，供未来扩展
     */
}
