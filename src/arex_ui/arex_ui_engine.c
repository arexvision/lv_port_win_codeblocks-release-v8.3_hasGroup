#include "arex_ui_engine.h"
#include "arex_data.h"
#include <stdio.h>
#include <string.h>

/* g_sys_config 定义于此，g_sensor_data 定义于 arex_data.c */
arex_sys_config_t g_sys_config;

/* =========================================================
 * 默认配置值
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
    cfg->h_depth  = 8;   /* 80px  */
    cfg->h_ndl    = 6;   /* 60px  */
    cfg->h_pod    = 6;   /* 60px  */
    cfg->h_batt   = 4;   /* 40px  */
    cfg->h_gas    = 6;   /* 60px  */
    cfg->h_time   = 4;   /* 40px  */
    cfg->gap_u    = 1;   /* 10px  */
    cfg->title_h_u = 2;  /* 20px  */

    /* 默认 5F 网格布局 */
    cfg->widget_count = 6;
    cfg->widget_ids[0] = 0;  cfg->widget_w[0] = 2; cfg->widget_h[0] = 2; /* MAX DEPTH 2x2 */
    cfg->widget_ids[1] = 1;  cfg->widget_w[1] = 2; cfg->widget_h[1] = 1; /* NDL 2x1 */
    cfg->widget_ids[2] = 2;  cfg->widget_w[2] = 1; cfg->widget_h[2] = 1; /* TEMP 1x1 */
    cfg->widget_ids[3] = 3;  cfg->widget_w[3] = 2; cfg->widget_h[3] = 2; /* SAC RATE 2x2 */
    cfg->widget_ids[4] = 4;  cfg->widget_w[4] = 2; cfg->widget_h[4] = 1; /* HEADING 2x1 */
    cfg->widget_ids[5] = 5;  cfg->widget_w[5] = 1; cfg->widget_h[5] = 1; /* BATTERY 1x1 */
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
        if (bottom_edge > AREX_PHYSICAL_H - 80)
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

    /* 零高度保护：最小 200px */
    if (top_h < 200) top_h = 200;

    uint16_t bottom_h = (g_sys_config.safe_zone_h > top_h + gap)
                         ? (g_sys_config.safe_zone_h - top_h - gap)
                         : 200;

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
 * 10U 左侧锚点组件 Y 坐标推算
 *
 * Widget 布局:
 *   [0] DEPTH   [1] NDL  [2] TTS
 *   [3] POD1    [4] POD2
 *   [5] BATT    [6] W.TIME
 *   [7] GAS     [8] DIVE TIME
 * ========================================================= */
void arex_calc_anchor_layout(arex_anchor_comp_t comps[ANCHOR_COMP_COUNT], uint16_t *out_total_h)
{
    int16_t  cur_y = 0;
    uint16_t gap   = g_sys_config.gap_u * AREX_BASE_U;
    uint16_t t_h   = g_sys_config.title_h_u * AREX_BASE_U;
    uint16_t half_w = AREX_LEFT_ANCHOR_W / 2;

    /* [0] DEPTH */
    comps[0].y     = cur_y;
    comps[0].h     = g_sys_config.h_depth * AREX_BASE_U;
    comps[0].title_h = t_h;
    comps[0].val_h = comps[0].h - t_h;
    comps[0].w     = AREX_LEFT_ANCHOR_W;
    comps[0].split = 0;
    cur_y += comps[0].h + gap;

    /* [1-2] NDL / TTS 双拼 */
    comps[1].y     = cur_y;
    comps[1].h     = g_sys_config.h_ndl * AREX_BASE_U;
    comps[1].title_h = t_h;
    comps[1].val_h = comps[1].h - t_h;
    comps[1].w     = half_w;
    comps[1].split = 1;
    comps[2].y     = cur_y;
    comps[2].h     = comps[1].h;
    comps[2].title_h = t_h;
    comps[2].val_h = comps[2].h - t_h;
    comps[2].w     = half_w;
    comps[2].split = 2;
    cur_y += comps[1].h + gap;

    /* [3-4] POD1 / POD2 双拼 */
    comps[3].y     = cur_y;
    comps[3].h     = g_sys_config.h_pod * AREX_BASE_U;
    comps[3].title_h = t_h;
    comps[3].val_h = comps[3].h - t_h;
    comps[3].w     = half_w;
    comps[3].split = 1;
    comps[4].y     = cur_y;
    comps[4].h     = comps[3].h;
    comps[4].title_h = t_h;
    comps[4].val_h = comps[4].h - t_h;
    comps[4].w     = half_w;
    comps[4].split = 2;
    cur_y += comps[3].h + gap;

    /* [5-6] BATT / W.TIME 双拼 */
    comps[5].y     = cur_y;
    comps[5].h     = g_sys_config.h_batt * AREX_BASE_U;
    comps[5].title_h = t_h;
    comps[5].val_h = comps[5].h - t_h;
    comps[5].w     = half_w;
    comps[5].split = 1;
    comps[6].y     = cur_y;
    comps[6].h     = comps[5].h;
    comps[6].title_h = t_h;
    comps[6].val_h = comps[6].h - t_h;
    comps[6].w     = half_w;
    comps[6].split = 2;
    cur_y += comps[5].h + gap;

    /* [7] GAS */
    comps[7].y     = cur_y;
    comps[7].h     = g_sys_config.h_gas * AREX_BASE_U;
    comps[7].title_h = t_h;
    comps[7].val_h = comps[7].h - t_h;
    comps[7].w     = AREX_LEFT_ANCHOR_W;
    comps[7].split = 0;
    cur_y += comps[7].h + gap;

    /* [8] DIVE TIME */
    comps[8].y     = cur_y;
    comps[8].h     = g_sys_config.h_time * AREX_BASE_U;
    comps[8].title_h = t_h;
    comps[8].val_h = comps[8].h - t_h;
    comps[8].w     = AREX_LEFT_ANCHOR_W;
    comps[8].split = 0;
    cur_y += comps[8].h;

    *out_total_h = cur_y;
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
    g_sensor_data.ndl  = 0;
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
