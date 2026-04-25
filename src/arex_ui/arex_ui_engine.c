#include "arex_ui_engine.h"
#include "arex_card_registry.h"
#include "arex_screen.h"
#include "fonts/arex_fonts.h"
#include <stdio.h>
#include <string.h>

/* 气体名称表 (供全局引用) */
const char *AREX_GAS_NAMES[AREX_GAS_COUNT] = {
    "AIR",
    "NX 32",
    "TX 18/45",
    "O2 100%"
};

/* 气体 MOD 表 (单位: 米) */
const uint8_t AREX_GAS_MOD_M[AREX_GAS_COUNT] = {
    56,  /* AIR */
    34,  /* NX 32 */
    68,  /* TX 18/45 */
    6    /* O2 100% */
};

/* =========================================================
 * 全局单例定义
 * ========================================================= */
arex_sys_config_t  g_sys_config;
arex_sensor_data_t g_sensor_data;

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

    cfg->split_outward = true;

    /* 分割线 */
    cfg->sep_style  = AREX_SEP_DASHED;
    cfg->sep_thick  = 2;
    cfg->sep_alpha  = 51;   /* 20% of 255 */

    /* 10U 高度分配 (单位 U，1U = 10px) */
    cfg->h_depth  = 8;   /* DEPTH 大通栏: 8U=80px */
    cfg->h_ndl    = 6;   /* NDL/TTS 双拼: 6U=60px */
    cfg->h_pod    = 6;   /* POD 1/2 双拼: 6U=60px */
    cfg->h_batt   = 5;   /* BATT/W.TIME 双拼: 5U=50px */
    cfg->h_gas    = 6;   /* GAS 中通栏: 6U=60px */
    cfg->h_time   = 5;   /* DIVE TIME 底部: 5U=50px */
    cfg->gap_u         = 0;   /* 模块间距: 0U=0px（由 sep_thick 负责分割线粗细） */
    cfg->panel_gap_u   = 1;   /* 面板间距: 1U=10px */
    cfg->title_h_u     = 2;   /* 标题高度: 2U=20px */
    cfg->h_menu_item   = 5;   /* 菜单项高度: 5U=50px */
    cfg->gap_menu      = 1;   /* 菜单项间距: 1U=10px */
    cfg->h_tissues_chart = 9; /* 组织柱图高度: 9U=90px */

    /* =====================================================
     * 左侧锚点行配置 (APP 同步就绪 — 自由双拼)
     *
     * 每行定义: {左模块, 右模块, h_u, title_h_u, title_font, val_font, val_align, sep_style, sep_thick}
     * right_module = AREX_MODULE_EMPTY → 左侧独占全宽(160px)
     * right_module != EMPTY → 双拼布局(各80px)
     *
     * 字号 arex_font_id_t: 0=SMALL(14px) 1=TITLE(20px) 2=MEDIUM(28px) 3=HUGE(48px)
     * 对齐 arex_align_t: 0=LEFT 1=CENTER 2=RIGHT
     *
     * ===================================================== */
    static const arex_left_row_cfg_t def_layout[AREX_MAX_LEFT_ROWS] = {
        /* row 0: DEPTH 单栏全宽 */
        { AREX_MODULE_DEPTH, AREX_MODULE_EMPTY, 9, 2, AREX_FONT_ID_SMALL,  AREX_FONT_ID_HUGE,   AREX_ALIGN_LEFT, AREX_SEP_DASHED, 2 },
        /* row 1: NDL + TTS 双拼 */
        { AREX_MODULE_NDL,  AREX_MODULE_TTS,  6, 2, AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_LEFT, AREX_SEP_DASHED, 2 },
        /* row 2: POD1 + POD2 双拼 */
        { AREX_MODULE_POD1, AREX_MODULE_POD2, 6, 2, AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM,  AREX_ALIGN_LEFT, AREX_SEP_DASHED, 2 },
        /* row 3: BATT + WTM 双拼 */
        { AREX_MODULE_TIME, AREX_MODULE_EMPTY,  6, 2, AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM,  AREX_ALIGN_LEFT, AREX_SEP_DASHED, 2 },
        /* row 4: GAS 单栏全宽 */
        { AREX_MODULE_GAS,  AREX_MODULE_EMPTY, 6, 2, AREX_FONT_ID_SMALL,  AREX_FONT_ID_TITLE, AREX_ALIGN_LEFT, AREX_SEP_DASHED, 2 },
        /* row 5: TIME 单栏全宽 */
        { AREX_MODULE_BATT, AREX_MODULE_WTM, 6, 2, AREX_FONT_ID_SMALL,  AREX_FONT_ID_TITLE,  AREX_ALIGN_LEFT, AREX_SEP_DASHED, 2 },
        /* row 6-7: EMPTY */
        { AREX_MODULE_EMPTY, AREX_MODULE_EMPTY, 0, 0, 0, 0, AREX_ALIGN_LEFT, AREX_SEP_NONE,   0 },
        { AREX_MODULE_EMPTY, AREX_MODULE_EMPTY, 0, 0, 0, 0, AREX_ALIGN_LEFT, AREX_SEP_NONE,   0 },
    };
    for (uint8_t i = 0; i < AREX_MAX_LEFT_ROWS; i++) {
        cfg->left_layout[i] = def_layout[i];
    }

    /* 默认 5F 网格布局
     * 行 r(0~5) × 列 c(0~4)
     * 跨度 w(1~2列) × h(1~2行)
     *
     *  5列布局示意（5列=10格，6行）：
     *  col:  0  1  2  3  4
     *  row0: [DEPTH 2x2 大块    ] [TEMP  ]
     *  row2: [SAC 2x1           ] [WARN ]
     *  row3: [POD1 2x2 大块       ]
     *  row5: [POD2 2x1] [NDL 1x1]
     *
     *  widget_id → arex_widget_id_t:
     *    0=EMPTY 1=DEPTH 2=TEMP 3=HEADING 4=SAC_RATE 5=BATTERY
     *    6=NDL 7=TTS 8=PPO2 9=CNS 10=POD1 11=POD2 12=WTIME
     */
    cfg->widget_count = 12;
    /* 5x6 网格完全铺满布局：
     * Row 0: DEPTH(2x2) | TEMP(1x1) | HEADING(2x1)
     * Row 1: (DEPTH右)  | ---     | (HEADING右)
     * Row 2: SAC(2x1)  | BATTERY(2x1) | PPO2(1x1)
     * Row 3: NDL(2x1)  | TTS(2x1)    | CNS(1x1)
     * Row 4: POD1(2x1) | POD2(2x1)    | (WTIME左)
     * Row 5: (POD1右)  | (POD2右)     | WTIME(2x2)
     */
    /*  id              r  c  w  h */
    cfg->widget_ids[0] = AREX_WIDGET_DEPTH;     cfg->widget_r[0] = 0; cfg->widget_c[0] = 0; cfg->widget_w[0] = 2; cfg->widget_h[0] = 2;
    cfg->widget_ids[1] = AREX_WIDGET_TEMP;      cfg->widget_r[1] = 0; cfg->widget_c[1] = 2; cfg->widget_w[1] = 1; cfg->widget_h[1] = 1;
    cfg->widget_ids[2] = AREX_WIDGET_HEADING;   cfg->widget_r[2] = 0; cfg->widget_c[2] = 3; cfg->widget_w[2] = 2; cfg->widget_h[2] = 1;
    cfg->widget_ids[3] = AREX_WIDGET_SAC_RATE;  cfg->widget_r[3] = 2; cfg->widget_c[3] = 0; cfg->widget_w[3] = 2; cfg->widget_h[3] = 1;
    cfg->widget_ids[4] = AREX_WIDGET_BATTERY;   cfg->widget_r[4] = 2; cfg->widget_c[4] = 2; cfg->widget_w[4] = 2; cfg->widget_h[4] = 1;
    cfg->widget_ids[5] = AREX_WIDGET_PPO2;       cfg->widget_r[5] = 2; cfg->widget_c[5] = 4; cfg->widget_w[5] = 1; cfg->widget_h[5] = 1;
    cfg->widget_ids[6] = AREX_WIDGET_NDL;       cfg->widget_r[6] = 3; cfg->widget_c[6] = 0; cfg->widget_w[6] = 2; cfg->widget_h[6] = 1;
    cfg->widget_ids[7] = AREX_WIDGET_TTS;        cfg->widget_r[7] = 3; cfg->widget_c[7] = 2; cfg->widget_w[7] = 2; cfg->widget_h[7] = 1;
    cfg->widget_ids[8] = AREX_WIDGET_CNS;        cfg->widget_r[8] = 3; cfg->widget_c[8] = 4; cfg->widget_w[8] = 1; cfg->widget_h[8] = 1;
    cfg->widget_ids[9] = AREX_WIDGET_POD1;      cfg->widget_r[9] = 4; cfg->widget_c[9] = 0; cfg->widget_w[9] = 2; cfg->widget_h[9] = 1;
    cfg->widget_ids[10] = AREX_WIDGET_POD2;      cfg->widget_r[10] = 4; cfg->widget_c[10] = 2; cfg->widget_w[10] = 2; cfg->widget_h[10] = 1;
    cfg->widget_ids[11] = AREX_WIDGET_WTIME;     cfg->widget_r[11] = 4; cfg->widget_c[11] = 4; cfg->widget_w[11] = 1; cfg->widget_h[11] = 1;
    cfg->widget_ids[11] = AREX_WIDGET_WTIME;     cfg->widget_r[11] = 5; cfg->widget_c[11] = 0; cfg->widget_w[11] = 1; cfg->widget_h[11] = 1;

    /* 卡片顺序（INFO/SETUP 固定，中间 5 个可重排）
     * card_order[pos] = card_id
     * 固定: CARD_POS_INFO=0, CARD_POS_SETUP=6
     * 可重排: CARD_POS_1 ~ CARD_POS_5 */
    cfg->card_order[CARD_POS_INFO]  = CARD_ID_INFO;         /* 不可修改 */
    cfg->card_order[CARD_POS_1]     = CARD_ID_CUSTOM_GRID;  /* 5F 自定义网格 — 放在最前 */
    cfg->card_order[CARD_POS_2]     = CARD_ID_DECO;
    cfg->card_order[CARD_POS_3]     = CARD_ID_COMPASS;
    cfg->card_order[CARD_POS_4]     = CARD_ID_GAS;
    cfg->card_order[CARD_POS_5]     = CARD_ID_PLAN;
    cfg->card_order[CARD_POS_SETUP] = CARD_ID_SETUP;        /* 不可修改 */

    /* 用户设置默认值 */
    cfg->mod_ppo2       = 1.4f;
    cfg->conservatism   = 1;    /* MED */
    cfg->brightness     = 2;    /* HIGH */
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
        uint8_t title_font  = g_sys_config.left_layout[row].title_font;
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
            comps[out_idx].title_font = title_font;
            comps[out_idx].val_font   = val_font;
            comps[out_idx].title_align = AREX_ALIGN_LEFT;
            comps[out_idx].val_align  = val_align;
            comps[out_idx].sep_style = g_sys_config.left_layout[row].sep_style;
            comps[out_idx].sep_thick  = g_sys_config.left_layout[row].sep_thick;
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
            comps[out_idx].title_font = title_font;
            comps[out_idx].val_font  = val_font;
            comps[out_idx].title_align = AREX_ALIGN_LEFT;
            comps[out_idx].val_align = AREX_ALIGN_LEFT;
            comps[out_idx].sep_style = g_sys_config.left_layout[row].sep_style;
            comps[out_idx].sep_thick  = g_sys_config.left_layout[row].sep_thick;
            out_idx++;

            /* 右块 */
            if (out_idx < ANCHOR_COMP_COUNT) {
                comps[out_idx].module     = right_mod;
                comps[out_idx].y         = cur_y;
                comps[out_idx].h         = h_px;
                comps[out_idx].title_h   = t_h;
                comps[out_idx].val_h      = v_h;
                comps[out_idx].w         = half_w;   /* 80px */
                comps[out_idx].split      = 2;        /* 双拼右 */
                comps[out_idx].title_font = title_font;
                comps[out_idx].val_font   = val_font;
                comps[out_idx].title_align = AREX_ALIGN_RIGHT;
                comps[out_idx].val_align = AREX_ALIGN_RIGHT;
                comps[out_idx].sep_style = g_sys_config.left_layout[row].sep_style;
                comps[out_idx].sep_thick  = g_sys_config.left_layout[row].sep_thick;
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

/* =========================================================
 * 字体映射器 (Font Mapper)
 *
 * 全系统唯一允许将字体 ID 转换为真实 lvgl 字体指针的地方。
 * 所有配置结构体中保存的 title_font / val_font 均应为 arex_font_id_t 值。
 *
 * ID 映射表：
 *   AREX_FONT_ID_SMALL  (0) → lv_font_courier_14  14px  标签/单位/Badge
 *   AREX_FONT_ID_TITLE  (1) → lv_font_courier_20  20px  菜单项/卡片标题
 *   AREX_FONT_ID_MEDIUM (2) → lv_font_courier_28  28px  数据值
 *   AREX_FONT_ID_HUGE   (3) → lv_font_courier_48  48px  深度大数字
 * ========================================================= */
const lv_font_t *arex_get_font(uint8_t font_id)
{
    switch (font_id) {
        case AREX_FONT_ID_SMALL:  return AREX_FONT_SMALL;   /* 14px */
        case AREX_FONT_ID_TITLE:  return AREX_FONT_TITLE;   /* 20px */
        case AREX_FONT_ID_MEDIUM: return AREX_FONT_MEDIUM;  /* 28px */
        case AREX_FONT_ID_HUGE:   return AREX_FONT_HUGE;   /* 48px */
        default:                   return AREX_FONT_SMALL;   /* 兜底：永不为 NULL */
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
    g_sensor_data.depth = 0.0f;
    g_sensor_data.ndl  = 5;
    g_sensor_data.tts  = 24;
    g_sensor_data.pod1_bar = 0.0f;
    g_sensor_data.pod2_bar = 0.0f;
    g_sensor_data.battery_pct = 85.0f;
    g_sensor_data.heading = 265;
    g_sensor_data.dive_time_s = 0; /* 38:14 */
    g_sensor_data.surface_time_s = 0; /* WTM: 10:45 */
    g_sensor_data.gas_active_idx = 2;
    strcpy(g_sensor_data.gas_name, "AIR");
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
 * 卡片顺序查询 (统一入口 — 替代旧的 g_arex_card_order)
 * ========================================================= */
uint8_t g_sys_card_order(uint8_t pos)
{
    if (pos >= AREX_CARD_COUNT) return 0;
    return g_sys_config.card_order[pos];
}

/* =========================================================
 * 通用动态菜单工厂
 * 所有尺寸从 g_sys_config 推算，不含硬编码像素值。
 * ========================================================= */
void arex_render_dynamic_menu(lv_obj_t *parent_card,
                              const arex_menu_item_cfg_t *items,
                              uint8_t item_count,
                              int start_y,
                              lv_obj_t **out_item_handles)
{
    if (!parent_card || !items || item_count == 0) return;

    int right_canvas_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                       - ((int)g_sys_config.gap_u * AREX_BASE_U);
    int item_w = right_canvas_w - 15;  /* 右侧 15px 呼吸距 */

    int current_y = start_y;
    for (uint8_t i = 0; i < item_count; i++) {
        const arex_menu_item_cfg_t *item_cfg = &items[i];
        /* height_u 默认 0 → 查 h_menu_item (单位 U) */
        int item_h = (int)(item_cfg->height_u > 0 ? item_cfg->height_u : g_sys_config.h_menu_item)
                   * AREX_BASE_U;
        /* gap_y 从 gap_menu (单位 U) 推算 */
        int gap_y = (int)g_sys_config.gap_menu * AREX_BASE_U;

        lv_obj_t *item = lv_obj_create(parent_card);
        lv_obj_set_pos(item, 0, current_y);
        lv_obj_set_size(item, item_w, item_h);
        lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(item, AREX_DARK, 0);
        lv_obj_set_style_border_width(item, item_cfg->border_width, LV_PART_MAIN);
        lv_obj_set_style_radius(item, 0, 0);
        lv_obj_set_style_pad_all(item, 0, LV_PART_MAIN);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

        /* 标题 label */
        if (item_cfg->title_text) {
            lv_obj_t *title_lbl = lv_label_create(item);
            lv_label_set_text(title_lbl, item_cfg->title_text);
            lv_obj_set_style_text_font(title_lbl, arex_get_font(item_cfg->title_font_id), 0);
            lv_obj_set_style_text_color(title_lbl, AREX_GREEN, 0);
            lv_obj_set_size(title_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 12, 0);
            lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_DOT);
        }

        /* 右侧徽章 label */
        if (item_cfg->value_badge) {
            lv_obj_t *badge_lbl = lv_label_create(item);
            lv_label_set_text(badge_lbl, item_cfg->value_badge);
            lv_obj_set_style_text_font(badge_lbl, arex_get_font(item_cfg->value_font_id), 0);
            lv_obj_set_style_text_color(badge_lbl, AREX_LIGHT, 0);
            lv_obj_set_size(badge_lbl, 80, 28);
            lv_obj_align(badge_lbl, LV_ALIGN_RIGHT_MID, -12, 0);
            lv_obj_set_style_text_align(badge_lbl, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_long_mode(badge_lbl, LV_LABEL_LONG_DOT);
        }

        if (out_item_handles) {
            out_item_handles[i] = item;
        }

        current_y += item_h + gap_y;
    }
}

/* =========================================================
 * 5F 自定义网格组件外部容器（由 arex_screen.c 注入）
 * ========================================================= */
lv_obj_t *g_left_anchor_obj = NULL;
lv_obj_t *g_card_custom_obj = NULL;

/* 告警状态 */
static arex_alarm_level_t s_alarm_level = AREX_ALARM_NONE;
static char s_alarm_text[64] = {0};
static lv_timer_t *s_alarm_blink_timer = NULL;
static bool s_alarm_blink_on = false;
static lv_obj_t *s_alarm_banner = NULL;

/* 告警横幅静态样式（避免每次创建重复初始化） */
static lv_style_t s_banner_style_warn;
static lv_style_t s_banner_style_crit;

/* =========================================================
 * 5F 网格坐标推算（纯数学绝对映射，无 lv_grid）
 *
 * 核心公式：
 *   cell_w = parent_w / 5
 *   cell_h = parent_h / 6
 *   abs_x  = col * cell_w + gap
 *   abs_y  = row * cell_h + gap
 *   abs_w  = span_w * cell_w - gap*2
 *   abs_h  = span_h * cell_h - gap*2
 * ========================================================= */
#define WIDGET_GAP  0   /* 网格缝隙 px */

/* =========================================================
 * 5F 网格坐标推算（锁定 80x60 基准 + 40px 标题避让）
 *
 * parent_w/parent_h: 父容器总尺寸（用于边界修正）
 * row/col: 网格行列索引(0~5 / 0~4)
 * span_w/span_h: 跨越的列数/行数
 * out_*: 输出绝对坐标
 *
 * 排版矩阵严格锁定 80x60 基准（完美整数）：
 *   cell_w = 80px  (400 / 5)
 *   cell_h = 60px  ((400-40) / 6)
 * Y 坐标增加 AREX_CARD_TITLE_H=40px 偏移，确保第一行落在标题区下方。
 * 宽高减 4px (2px 缝隙 x2) 制造四周 2px 物理留白。
 * ========================================================= */
void arex_calc_widget_grid(uint16_t parent_w, uint16_t parent_h,
                           uint8_t row, uint8_t col,
                           uint8_t span_w, uint8_t span_h,
                           int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h)
{
    /* 锁定 80x60 基准网格 */
    uint16_t cell_w = 80;   /* 5列 → 400/5 = 80px */
    uint16_t cell_h = 60;   /* 6行 → (400-40)/6 = 60px */

    /* X: 列偏移 + 缝隙(2px) */
    *out_x = (int16_t)(col * cell_w + WIDGET_GAP);
    /* Y: 标题区下方 + 行偏移 + 缝隙(2px) */
    *out_y = (int16_t)(AREX_CARD_TITLE_H + row * cell_h + WIDGET_GAP);
    /* 宽高: 跨距×基准 - 4px 缝隙(四周各 2px) */
    *out_w = (uint16_t)(span_w * cell_w - WIDGET_GAP * 2);
    *out_h = (uint16_t)(span_h * cell_h - WIDGET_GAP * 2);

    /* 边界修正（以容器总尺寸为边界） */
    if (*out_x + *out_w > (int16_t)parent_w)
        *out_w = (uint16_t)((int16_t)parent_w - *out_x);
    if (*out_y + *out_h > (int16_t)parent_h)
        *out_h = (uint16_t)((int16_t)parent_h - *out_y);
}

/* =========================================================
 * 字号自适应引擎
 * 根据组件跨越的列数×行数，选用最合适的字体 ID。
 *
 * 规则：
 *   span_w >= 2 && span_h >= 2 → 2x2 大块 → AREX_FONT_ID_HUGE (48px)
 *   span_w >= 2 || span_h >= 2 → 长条 → AREX_FONT_ID_MEDIUM (28px)
 *   span_w == 1 && span_h == 1 → 1x1 小块 → AREX_FONT_ID_SMALL (14px)
 * ========================================================= */
static arex_font_id_t span_to_font(uint8_t span_w, uint8_t span_h)
{
    if (span_w >= 2 && span_h >= 2) return AREX_FONT_ID_HUGE;
    if (span_w >= 2 || span_h >= 2) return AREX_FONT_ID_MEDIUM;
    return AREX_FONT_ID_SMALL;
}

/* =========================================================
 * 组件元数据字典（按 arex_widget_id_t 索引）
 * 渲染引擎只查此表，不做任何"如果是 DEPTH" 的硬编码判断。
 * ========================================================= */
typedef struct {
    const char *title;     /* 显示标题（英文） */
    const char *unit;     /* 单位字符串 */
    arex_font_id_t title_font;
    arex_font_id_t val_font;
    arex_align_t   val_align;
} widget_meta_t;

static const widget_meta_t s_widget_meta[AREX_WIDGET_COUNT] = {
    /* EMPTY */     { NULL,          NULL,   AREX_FONT_ID_SMALL,  AREX_FONT_ID_SMALL,  AREX_ALIGN_CENTER },
    /* DEPTH  */    { "DEPTH",      "m",    AREX_FONT_ID_SMALL,  AREX_FONT_ID_HUGE,   AREX_ALIGN_CENTER },
    /* TEMP   */    { "TEMP",       "C",    AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_CENTER },
    /* HEADING */   { "HEADING",    "",     AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_CENTER },
    /* SAC    */    { "SAC",        "l/m",  AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_CENTER },
    /* BATT   */    { "BATTERY",    "%",    AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_CENTER },
    /* NDL    */    { "NDL",        "min",  AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_CENTER },
    /* TTS    */    { "TTS",        "min",  AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_CENTER },
    /* PPO2   */    { "PPO2",      "",     AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_CENTER },
    /* CNS    */    { "CNS",        "%",    AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_CENTER },
    /* POD1   */    { "POD1",       "bar",  AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_CENTER },
    /* POD2   */    { "POD2",       "bar",  AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_CENTER },
    /* WTIME  */    { "W.TIME",     "",     AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_CENTER },
};

/* =========================================================
 * 获取 widget 显示名称
 * ========================================================= */
const char *arex_get_widget_name(arex_widget_id_t id)
{
    if (id >= AREX_WIDGET_COUNT) return "???";
    return s_widget_meta[id].title ? s_widget_meta[id].title : "";
}

/* =========================================================
 * 创建单个自定义组件（组件工厂）
 *
 * 关键：每个组件的 lv_obj_set_user_data() 存储了 arex_widget_id_t，
 * 告警引擎靠这个烙印实现"左侧锚点 + 5F 组件同时闪烁"。
 *
 * 字号由 span 自动决定，大块→Huge，中块→Medium，小块→Small。
 * ========================================================= */
static lv_obj_t *create_custom_widget(lv_obj_t *parent,
                                      arex_widget_id_t w_id,
                                      int16_t abs_x, int16_t abs_y,
                                      uint16_t abs_w, uint16_t abs_h,
                                      uint8_t span_w, uint8_t span_h)
{
    if (w_id >= AREX_WIDGET_COUNT) return NULL;

    const widget_meta_t *meta = &s_widget_meta[w_id];

    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, abs_x, abs_y);
    lv_obj_set_size(obj, abs_w, abs_h);
    lv_obj_set_style_bg_color(obj, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, AREX_DARK, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 2, 0);

    /* ========== 靶向告警烙印 ========== */
    /* 全系统唯一的身份烙印：存储 widget_id。
     * 告警引擎搜索时会 lv_obj_get_user_data() 比对 target_id。
     * 同时用于 arex_widget_set_value() 定位句柄。 */
    lv_obj_set_user_data(obj, (void *)(uintptr_t)w_id);

    /* ========== 标题 label ========== */
    if (meta->title) {
        lv_obj_t *title_lbl = lv_label_create(obj);
        lv_label_set_text(title_lbl, meta->title);
        lv_obj_set_style_text_font(title_lbl, arex_get_font(meta->title_font), 0);
        lv_obj_set_style_text_color(title_lbl, AREX_GREEN, 0);
        lv_obj_set_size(title_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(title_lbl, LV_ALIGN_TOP_MID, 0, 2);
        lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_DOT);
    }

    /* ========== 数值 label（存储句柄供 update 循环更新文字）========== */
    lv_obj_t *val_lbl = lv_label_create(obj);
    lv_label_set_text(val_lbl, "--");
    /* 字号由 span 自动决定 */
    lv_obj_set_style_text_font(val_lbl, arex_get_font(span_to_font(span_w, span_h)), 0);
    lv_obj_set_style_text_color(val_lbl, AREX_GREEN, 0);
    lv_obj_set_size(val_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(val_lbl, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_long_mode(val_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_user_data(val_lbl, (void *)(uintptr_t)w_id);

    /* ========== 单位 label ========== */
    if (meta->unit && meta->unit[0]) {
        lv_obj_t *unit_lbl = lv_label_create(obj);
        lv_label_set_text(unit_lbl, meta->unit);
        lv_obj_set_style_text_font(unit_lbl, arex_get_font(AREX_FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(unit_lbl, AREX_LIGHT, 0);
        lv_obj_set_size(unit_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(unit_lbl, LV_ALIGN_BOTTOM_MID, 0, -2);
        lv_label_set_long_mode(unit_lbl, LV_LABEL_LONG_DOT);
    }

    (void)meta; /* meta 仅用于 title/unit/font，布局已在 caller 中算好 */
    return obj;
}

/* =========================================================
 * 全局 widget 句柄表（按 arex_widget_id_t 索引，供 update 循环查找）
 * 注意：一个 widget_id 可能有多个物理实例（左侧锚点1个 + 5F N个），
 * 所以这是链表表头，实际使用时遍历子节点查找。
 * ========================================================= */
#define MAX_WIDGET_HANDLES 16
static lv_obj_t *s_widget_handles[AREX_WIDGET_COUNT]; /* 仅记录 5F 区域的句柄 */
static uint8_t   s_widget_handle_count = 0;

/* 按 widget_id 在容器中查找第一个匹配的子节点 */
static lv_obj_t *find_widget_in_container(lv_obj_t *container, arex_widget_id_t w_id)
{
    if (!container) return NULL;
    int16_t child_cnt = lv_obj_get_child_cnt(container);
    for (int16_t i = 0; i < child_cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(container, i);
        if (child && (arex_widget_id_t)(uintptr_t)lv_obj_get_user_data(child) == w_id) {
            return child;
        }
    }
    return NULL;
}

/* =========================================================
 * 5F 网格总线渲染器
 *
 * 1. 从 g_sys_config.widget_* 读取所有组件配置
 * 2. 逐一枚遍历，用纯数学行×列映射算出绝对坐标
 * 3. 调用组件工厂渲染，注入 user_data 烙印
 * 4. 注册外部容器到告警引擎
 * ========================================================= */
void arex_render_5f_custom_grid(lv_obj_t *card_custom, lv_obj_t *left_anchor)
{
    /* 注入外部容器 */
    g_card_custom_obj = card_custom;
    g_left_anchor_obj = left_anchor;

    if (!card_custom) return;

    /* 获取容器总尺寸（标题区偏移由 arex_calc_widget_grid 内部处理） */
    uint16_t parent_w = lv_obj_get_content_width(card_custom);
    uint16_t parent_h = lv_obj_get_content_height(card_custom);

    /* 清除旧 widget 句柄表 */
    memset(s_widget_handles, 0, sizeof(s_widget_handles));
    s_widget_handle_count = 0;

    /* 清除容器中所有旧组件（rebuild 时） */
    lv_obj_clean(card_custom);

    /* ---- 创建卡片标题（与 arex_screen_make_card_title 风格一致） ---- */
    lv_obj_t *lbl = lv_label_create(card_custom);
    lv_obj_set_style_text_color(lbl, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(lbl, arex_get_font(AREX_FONT_ID_TITLE), 0);
    lv_obj_set_pos(lbl, 16, 8);
    lv_obj_set_size(lbl, parent_w - 32, AREX_CARD_TITLE_H - 10);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(lbl, "5F: CUSTOM WIDGETS");

    lv_obj_t *line = lv_obj_create(card_custom);
    lv_obj_set_size(line, parent_w - 32, 2);
    lv_obj_set_pos(line, 16, AREX_CARD_TITLE_H - 2);
    lv_obj_set_style_bg_color(line, AREX_DARK, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    lv_obj_set_style_radius(line, 0, 0);

    /* 遍历所有组件 */
    uint8_t count = g_sys_config.widget_count;
    if (count > AREX_MAX_WIDGETS) count = AREX_MAX_WIDGETS;

    for (uint8_t i = 0; i < count; i++) {
        arex_widget_id_t w_id   = (arex_widget_id_t)g_sys_config.widget_ids[i];
        uint8_t r       = g_sys_config.widget_r[i];
        uint8_t c       = g_sys_config.widget_c[i];
        uint8_t span_w  = g_sys_config.widget_w[i];
        uint8_t span_h  = g_sys_config.widget_h[i];

        if (w_id == AREX_WIDGET_EMPTY) continue;
        if (r >= AREX_WIDGET_ROWS || c >= AREX_WIDGET_COLS) continue;
        if (span_w == 0) span_w = 1;
        if (span_h == 0) span_h = 1;

        /* 纯数学绝对坐标映射（含 AREX_CARD_TITLE_H=40px 标题避让偏移） */
        int16_t abs_x, abs_y;
        uint16_t abs_w, abs_h;
        arex_calc_widget_grid(parent_w, parent_h,
                              r, c, span_w, span_h,
                              &abs_x, &abs_y, &abs_w, &abs_h);

        /* 调用组件工厂 */
        lv_obj_t *w = create_custom_widget(card_custom, w_id,
                                            abs_x, abs_y, abs_w, abs_h,
                                            span_w, span_h);

        /* 记录句柄（用于 update 循环） */
        if (w && s_widget_handle_count < MAX_WIDGET_HANDLES) {
            s_widget_handles[s_widget_handle_count++] = w;
        }
    }
}

/* =========================================================
 * 按 widget_id 设置数值（由外部 update 循环调用）
 *
 * 算法：在 g_card_custom_obj 和 g_left_anchor_obj 两个容器中
 * 遍历所有子节点，用 user_data 烙印匹配 target_id，
 * 找到后定位其中的数值 label 并更新文字。
 *
 * 绝不触发任何重绘或排版重构！只更新 lv_label 文字。
 * ========================================================= */
void arex_widget_set_value(arex_widget_id_t id, float value)
{
    /* 搜索 5F 卡片区域 */
    lv_obj_t *container = g_card_custom_obj;
    if (!container) container = g_left_anchor_obj;
    if (!container) return;

    int16_t child_cnt = lv_obj_get_child_cnt(container);
    for (int16_t i = 0; i < child_cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(container, i);
        if (!child) continue;

        /* user_data 烙印匹配 */
        if ((arex_widget_id_t)(uintptr_t)lv_obj_get_user_data(child) == id) {
            /* 在该 widget 的子节点中找数值 label */
            int16_t sub_cnt = lv_obj_get_child_cnt(child);
            for (int16_t j = 0; j < sub_cnt; j++) {
                lv_obj_t *sub = lv_obj_get_child(child, j);
                if (!sub) continue;
                /* 数值 label 的 user_data 也存储了 widget_id */
                if ((arex_widget_id_t)(uintptr_t)lv_obj_get_user_data(sub) == id) {
                    /* 只对 lv_label 类型更新文字 */
                    if (lv_obj_check_type(sub, &lv_label_class)) {
                        char buf[32];
                        /* 根据数据类型选择格式化 */
                        if (id == AREX_WIDGET_DEPTH || id == AREX_WIDGET_TEMP) {
                            snprintf(buf, sizeof(buf), "%.1f", (double)value);
                        } else if (id == AREX_WIDGET_PPO2) {
                            snprintf(buf, sizeof(buf), "%.2f", (double)value);
                        } else if (id == AREX_WIDGET_POD1 || id == AREX_WIDGET_POD2) {
                            snprintf(buf, sizeof(buf), "%.0f", (double)value);
                        } else {
                            snprintf(buf, sizeof(buf), "%.0f", (double)value);
                        }
                        lv_label_set_text(sub, buf);
                    }
                    break; /* 找到即停 */
                }
            }
        }
    }
}

/* =========================================================
 * 告警闪烁定时器回调
 * ========================================================= */
static void alarm_blink_cb(lv_timer_t *timer)
{
    (void)timer;
    s_alarm_blink_on = !s_alarm_blink_on;

    /* 遍历所有注册的 widget 句柄应用/移除闪烁样式 */
    for (uint8_t i = 0; i < s_widget_handle_count; i++) {
        lv_obj_t *w = s_widget_handles[i];
        if (!w) continue;

        if (s_alarm_blink_on) {
            lv_obj_set_style_bg_color(w, AREX_LIGHT, 0);
            lv_obj_set_style_text_color(w, AREX_BLACK, 0);
        } else {
            lv_obj_set_style_bg_color(w, AREX_BLACK, 0);
            lv_obj_set_style_text_color(w, AREX_GREEN, 0);
        }
    }
}

/* =========================================================
 * 靶向告警触发引擎
 *
 * 1. 弹出顶部的纯英文告警横幅（永不显示图案）
 * 2. 若 target_id != EMPTY，遍历所有子节点，
 *    找到打了烙印的组件并加入闪烁队列
 * 3. 启动/停止 blink 定时器
 *
 * 告警消失时调用 arex_clear_all_alarm_styles() 清除。
 * ========================================================= */
void arex_trigger_alarm(arex_alarm_level_t level,
                        const char *eng_text,
                        arex_widget_id_t target_id)
{
    /* 停止旧的 blink 定时器 */
    if (s_alarm_blink_timer) {
        lv_timer_pause(s_alarm_blink_timer);
        s_alarm_blink_on = false;
    }

    /* 清除旧告警样式 */
    arex_clear_all_alarm_styles();

    /* 弹出告警横幅 */
    arex_show_alarm_banner(level, eng_text);

    if (target_id == AREX_WIDGET_EMPTY) {
        /* 仅横幅告警，不做靶向 */
        return;
    }

    /* 初始化告警样式到所有匹配的 widget（注册到闪烁队列） */
    for (uint8_t i = 0; i < s_widget_handle_count; i++) {
        lv_obj_t *w = s_widget_handles[i];
        if (!w) continue;

        if ((arex_widget_id_t)(uintptr_t)lv_obj_get_user_data(w) == target_id) {
            /* 加入闪烁队列（已在 s_widget_handles 中） */
            /* 立即应用一次 */
            lv_obj_set_style_bg_color(w, AREX_LIGHT, 0);
        }
    }

    /* 启动/重设 blink 定时器 */
    if (level == AREX_ALARM_CRIT) {
        /* CRITICAL: 2Hz 快速闪烁 */
        if (!s_alarm_blink_timer) {
            s_alarm_blink_timer = lv_timer_create(alarm_blink_cb, 500, NULL);
        } else {
            lv_timer_reset(s_alarm_blink_timer);
            lv_timer_set_period(s_alarm_blink_timer, 500);
        }
        lv_timer_resume(s_alarm_blink_timer);
    } else if (level == AREX_ALARM_WARN) {
        /* WARN: 1Hz 慢闪烁 */
        if (!s_alarm_blink_timer) {
            s_alarm_blink_timer = lv_timer_create(alarm_blink_cb, 1000, NULL);
        } else {
            lv_timer_reset(s_alarm_blink_timer);
            lv_timer_set_period(s_alarm_blink_timer, 1000);
        }
        lv_timer_resume(s_alarm_blink_timer);
    }
    /* INFO 级别不闪烁，仅横幅 */

    s_alarm_level = level;
}

/* =========================================================
 * 清除所有组件的告警样式，恢复正常显示
 * ========================================================= */
void arex_clear_all_alarm_styles(void)
{
    /* 停止 blink 定时器 */
    if (s_alarm_blink_timer) {
        lv_timer_pause(s_alarm_blink_timer);
    }

    /* 恢复所有 widget 句柄的正常样式 */
    for (uint8_t i = 0; i < s_widget_handle_count; i++) {
        lv_obj_t *w = s_widget_handles[i];
        if (!w) continue;
        lv_obj_set_style_bg_color(w, AREX_BLACK, 0);
        lv_obj_set_style_text_color(w, AREX_GREEN, 0);
    }

    /* 隐藏横幅 */
    arex_hide_alarm_banner();

    s_alarm_level = AREX_ALARM_NONE;
}

/* =========================================================
 * 告警横幅显示/隐藏
 * 默认横幅显示在屏幕顶部（独立图层），永不显示图案，纯英文文字。
 * ========================================================= */
void arex_show_alarm_banner(arex_alarm_level_t level, const char *eng_text)
{
    arex_hide_alarm_banner();

    if (!eng_text || !eng_text[0]) return;

    /* 创建横幅（顶层 screen，避免被其他对象遮挡） */
    lv_obj_t *screen = lv_scr_act();
    s_alarm_banner = lv_obj_create(screen);
    lv_obj_set_size(s_alarm_banner, AREX_PHYSICAL_W, 36);
    lv_obj_set_pos(s_alarm_banner, 0, 0);
    lv_obj_set_style_border_width(s_alarm_banner, 0, 0);
    lv_obj_set_style_radius(s_alarm_banner, 0, 0);
    lv_obj_set_style_pad_all(s_alarm_banner, 0, 0);

    if (level == AREX_ALARM_CRIT) {
        lv_obj_set_style_bg_color(s_alarm_banner, AREX_DARK, 0);
    } else if (level == AREX_ALARM_WARN) {
        lv_obj_set_style_bg_color(s_alarm_banner, AREX_DARK, 0);
    } else {
        lv_obj_set_style_bg_color(s_alarm_banner, AREX_BLACK, 0);
    }
    lv_obj_set_style_bg_opa(s_alarm_banner, LV_OPA_COVER, 0);

    /* 告警文字 label */
    lv_obj_t *lbl = lv_label_create(s_alarm_banner);
    lv_label_set_text(lbl, eng_text);
    lv_obj_set_style_text_font(lbl, arex_get_font(AREX_FONT_ID_TITLE), 0);
    lv_obj_set_style_text_color(lbl, AREX_LIGHT, 0);
    lv_obj_set_size(lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
}

void arex_hide_alarm_banner(void)
{
    if (s_alarm_banner) {
        lv_obj_del(s_alarm_banner);
        s_alarm_banner = NULL;
    }
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

/* =========================================================
 * 11. Data Bus UI 消费任务 — 全系统唯一允许执行 lv_label_set_text 的地方
 *
 * 架构铁律：
 *   - 硬件工程师：只能调用 arex_bus_set_*() 系列函数（仅写数据+打脏标记）
 *   - UI 工程师  ：只能修改 arex_ui_update_task() 消费者
 *   - 两者通过 g_sensor_data.dirty_mask 完全解耦
 *
 * 由 lv_timer 驱动，建议 50ms 周期（20 FPS 足够覆盖所有传感器变化）
 * ========================================================= */
void arex_ui_update_task(lv_timer_t *timer)
{
    (void)timer;

    uint32_t mask = g_sensor_data.dirty_mask;
    if (mask == DIRTY_NONE) return;

    /* 深度 + NDL + TTS + 减压停留 —— 左侧面板全量刷新 + 3B Deco 卡片刷新 */
    if (mask & (DIRTY_DEPTH | DIRTY_NDL | DIRTY_TTS | DIRTY_DECO)) {
        arex_screen_refresh_left_panel();
        card_deco_update();
    }

    /* 气瓶压力 —— 左侧面板 POD 刷新 */
    if (mask & DIRTY_POD) {
        arex_screen_refresh_left_panel();
    }

    /* 电池 —— 左侧面板刷新 */
    if (mask & DIRTY_BATT) {
        arex_screen_refresh_left_panel();
    }

    /* 罗盘航向 */
    if (mask & DIRTY_HEADING) {
        arex_screen_refresh_compass_target();
    }

    /* 潜水时间 + W.TIME —— 左侧面板 + 4F 曲线图 */
    if (mask & DIRTY_TIME) {
        arex_screen_refresh_left_panel();
    }

    /* PO2 值 —— 左侧面板 */
    if (mask & DIRTY_PPO2) {
        arex_screen_refresh_left_panel();
    }

    /* 气体切换 */
    if (mask & DIRTY_GAS) {
        arex_screen_refresh_gas_menu();
        arex_screen_refresh_left_panel();
    }

    /* 4F 曲线图刷新 */
    if (mask & DIRTY_CHART) {
        card_plan_update();
    }

    /* 洗净所有脏标记 */
    arex_bus_clear_all_dirty();
}
