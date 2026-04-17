/*
 * arex_ui_engine.c
 *
 * 绝对坐标推算引擎 — AREX AR-HUD 系统核心
 *
 * 架构铁律：
 *   1. 所有 UI 位置 = 纯数学加减乘除推算后调用 lv_obj_set_pos / lv_obj_set_size
 *   2. 严禁 LV_FLEX / LV_GRID
 *   3. sensor_data 更新只刷新 label 文本，永远不触发重新排版
 *   4. safe_zone 是唯一的坐标原点 (0,0)
 */

#include "arex_ui_engine.h"
#include "arex_screen.h"        /* 颜色宏、字体宏 */
#include "arex_card_registry.h"
#include "fonts/arex_fonts.h"
#include <string.h>
#include <stdio.h>

/* =========================================================
 * 全局单例定义
 * ========================================================= */
arex_sys_config_t   g_sys_config;
arex_sensor_data_t  g_sensor;
arex_layout_cache_t g_layout;

/* =========================================================
 * 内部：左锚区块描述表
 * 顺序必须与 g_layout.block_y[6] 索引一一对应
 * ========================================================= */
#define BLOCK_DEPTH  0
#define BLOCK_NDL    1
#define BLOCK_POD    2
#define BLOCK_BATT   3
#define BLOCK_GAS    4
#define BLOCK_TIME   5
#define BLOCK_COUNT  6

/* 从配置中取每块的 U 高度 */
static uint8_t s_block_u[BLOCK_COUNT];

static void s_refresh_block_u(const arex_sys_config_t *cfg)
{
    s_block_u[BLOCK_DEPTH] = cfg->h_depth;
    s_block_u[BLOCK_NDL]   = cfg->h_ndl;
    s_block_u[BLOCK_POD]   = cfg->h_pod;
    s_block_u[BLOCK_BATT]  = cfg->h_batt;
    s_block_u[BLOCK_GAS]   = cfg->h_gas;
    s_block_u[BLOCK_TIME]  = cfg->h_time;
}

/* =========================================================
 * 内部：左锚区内部组件句柄
 * ========================================================= */
static lv_obj_t *s_blk[BLOCK_COUNT];      /* 每块的容器 obj */
static lv_obj_t *s_title[BLOCK_COUNT];    /* 标题 label */
static lv_obj_t *s_value[BLOCK_COUNT];    /* 数值 label（主值） */

/* 双拼块内部：左右子容器 */
static lv_obj_t *s_split_L[BLOCK_COUNT];
static lv_obj_t *s_split_R[BLOCK_COUNT];
static lv_obj_t *s_title_L[BLOCK_COUNT];
static lv_obj_t *s_value_L[BLOCK_COUNT];
static lv_obj_t *s_title_R[BLOCK_COUNT];
static lv_obj_t *s_value_R[BLOCK_COUNT];

/* 右侧卡片容器句柄（每张卡片是 left_anchor 同级的 child of safe_zone） */
static lv_obj_t *s_card_objs[AREX_CARD_COUNT];

/* 卡片指示器点 */
#define DOTS_MAX AREX_CARD_COUNT
static lv_obj_t *s_dots[DOTS_MAX];
static lv_obj_t *s_dots_cont;

/* PO2 三段值（在 BLOCK_GAS 内） */
static lv_obj_t *s_ppo2_val[3];
static lv_obj_t *s_ppo2_sep[2];

/* =========================================================
 * 工具：创建裸 obj，清除默认的 border/bg/pad/scroll
 * ========================================================= */
static lv_obj_t *make_bare_obj(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

/* 工具：创建 label，透明背景 */
static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font,
                             lv_color_t color, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(lbl, 0, 0);
    lv_obj_set_style_pad_all(lbl, 0, 0);
    lv_label_set_text(lbl, text);
    return lbl;
}

/* =========================================================
 * 工具：对齐枚举 → lv_text_align_t
 * ========================================================= */
static lv_text_align_t align_to_lv(uint8_t a)
{
    switch (a) {
        case AREX_ALIGN_CENTER: return LV_TEXT_ALIGN_CENTER;
        case AREX_ALIGN_RIGHT:  return LV_TEXT_ALIGN_RIGHT;
        default:                return LV_TEXT_ALIGN_LEFT;
    }
}

/* =========================================================
 * 1. 默认配置
 * ========================================================= */
void arex_sys_config_default(arex_sys_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));

    cfg->safe_zone_w   = 580;
    cfg->safe_zone_h   = 400;
    cfg->offset_x      = 0;
    cfg->offset_y      = 0;

    cfg->theme_mode    = AREX_THEME_TECH;
    cfg->layout_order  = AREX_ORDER_NORMAL;
    cfg->dots_position = AREX_DOTS_RIGHT;
    cfg->compass_style = AREX_COMPASS_CLASSIC;
    cfg->flash_speed_ds = 3;  /* 0.3s 半周期 */

    cfg->font_sz_huge  = 48;
    cfg->font_sz_med   = 28;
    cfg->font_sz_small = 14;
    cfg->align_title   = AREX_ALIGN_LEFT;
    cfg->align_huge    = AREX_ALIGN_LEFT;
    cfg->align_med     = AREX_ALIGN_LEFT;
    cfg->split_outward = 1;

    cfg->sep_style  = AREX_SEP_DASHED;
    cfg->sep_thick  = 1;
    cfg->sep_alpha  = 51;   /* ~20% */

    cfg->h_depth   = 8;
    cfg->h_ndl     = 6;
    cfg->h_pod     = 6;
    cfg->h_batt    = 4;
    cfg->h_gas     = 6;
    cfg->h_time    = 4;
    cfg->gap_u     = 1;
    cfg->title_h_u = 2;

    for (uint8_t i = 0; i < AREX_CARD_COUNT; i++) cfg->card_order[i] = i;

    /* 5F 默认空 */
    cfg->widget_count = 0;
}

/* =========================================================
 * 2. 计算左锚区总高度（单位 px）
 * ========================================================= */
int16_t arex_layout_calc_anchor_h(const arex_sys_config_t *cfg)
{
    int16_t total = 0;
    const uint8_t us[BLOCK_COUNT] = {
        cfg->h_depth, cfg->h_ndl, cfg->h_pod,
        cfg->h_batt,  cfg->h_gas, cfg->h_time
    };
    for (int i = 0; i < BLOCK_COUNT; i++) {
        total += U2PX(us[i]);
        if (i < BLOCK_COUNT - 1) total += U2PX(cfg->gap_u);
    }
    return total;
}

/* =========================================================
 * 3. 计算右侧容器实际高度（含防坍塌保底）
 * ========================================================= */
int16_t arex_layout_calc_right_h(const arex_sys_config_t *cfg, int16_t anchor_h)
{
    int16_t h;
    if (cfg->theme_mode == AREX_THEME_CLASSIC) {
        h = (int16_t)cfg->safe_zone_h - anchor_h - U2PX(cfg->gap_u);
    } else {
        h = (int16_t)cfg->safe_zone_h;
    }
    if (h < AREX_MIN_RIGHT_H) h = AREX_MIN_RIGHT_H;
    return h;
}

/* =========================================================
 * 4. 推算并写入顶层两区的绝对坐标
 * ========================================================= */
static void layout_calc_regions(const arex_sys_config_t *cfg,
                                 int16_t anchor_h)
{
    int16_t gap_px = U2PX(cfg->gap_u);
    int16_t la_w, la_h, la_x, la_y;
    int16_t rc_w, rc_h, rc_x, rc_y;

    if (cfg->theme_mode == AREX_THEME_TECH) {
        /* Tech 模式：左右并排 */
        la_w = AREX_LEFT_ANCHOR_W;
        la_h = (int16_t)cfg->safe_zone_h;
        rc_w = (int16_t)cfg->safe_zone_w - AREX_LEFT_ANCHOR_W - gap_px;
        rc_h = arex_layout_calc_right_h(cfg, anchor_h);

        if (cfg->layout_order == AREX_ORDER_NORMAL) {
            la_x = 0;
            la_y = 0;
            rc_x = AREX_LEFT_ANCHOR_W + gap_px;
            rc_y = 0;
        } else {
            /* 翻转：左锚在右，卡片在左 */
            rc_x = 0;
            rc_y = 0;
            la_x = rc_w + gap_px;
            la_y = 0;
        }
    } else {
        /* Classic 模式：上下堆叠 */
        la_w = (int16_t)cfg->safe_zone_w;
        la_h = anchor_h;
        rc_w = (int16_t)cfg->safe_zone_w;
        rc_h = arex_layout_calc_right_h(cfg, anchor_h);

        if (cfg->layout_order == AREX_ORDER_NORMAL) {
            la_x = 0; la_y = 0;
            rc_x = 0; rc_y = anchor_h + gap_px;
        } else {
            rc_x = 0; rc_y = 0;
            la_x = 0; la_y = rc_h + gap_px;
        }
    }

    /* 写入缓存 */
    g_layout.la_x = la_x; g_layout.la_y = la_y;
    g_layout.la_w = la_w; g_layout.la_h = la_h;
    g_layout.rc_x = rc_x; g_layout.rc_y = rc_y;
    g_layout.rc_w = rc_w; g_layout.rc_h = rc_h;

    /* 应用到 lv_obj */
    lv_obj_set_pos(g_layout.left_anchor,  la_x, la_y);
    lv_obj_set_size(g_layout.left_anchor, la_w, la_h);
    lv_obj_set_pos(g_layout.right_canvas,  rc_x, rc_y);
    lv_obj_set_size(g_layout.right_canvas, rc_w, rc_h);
}

/* =========================================================
 * 5. 推算左锚区 10U 块的绝对 Y 坐标
 * ========================================================= */
static void layout_calc_blocks(const arex_sys_config_t *cfg)
{
    s_refresh_block_u(cfg);
    int16_t gap_px = U2PX(cfg->gap_u);
    int16_t cy = 0;

    for (int i = 0; i < BLOCK_COUNT; i++) {
        int16_t h = U2PX(s_block_u[i]);
        g_layout.block_y[i] = cy;
        g_layout.block_h[i] = h;
        cy += h + gap_px;
    }
}

/* =========================================================
 * 6. 应用块坐标到左锚区的 lv_obj
 * ========================================================= */
static void layout_apply_blocks(const arex_sys_config_t *cfg)
{
    int16_t w = g_layout.la_w;
    int16_t title_h = U2PX(cfg->title_h_u);

    for (int i = 0; i < BLOCK_COUNT; i++) {
        int16_t bh = g_layout.block_h[i];
        int16_t by = g_layout.block_y[i];

        if (!s_blk[i]) continue;
        lv_obj_set_pos(s_blk[i],  0, by);
        lv_obj_set_size(s_blk[i], w, bh);

        /* 标题分割线颜色 / 透明度（通过 border_bottom 实现） */
        if (s_title[i]) {
            lv_obj_set_pos(s_title[i],  0, 0);
            lv_obj_set_size(s_title[i], w, title_h);
            lv_obj_set_style_text_align(s_title[i], align_to_lv(cfg->align_title), 0);

            /* 分割线：border_bottom */
            if (cfg->sep_style != AREX_SEP_NONE) {
                lv_obj_set_style_border_side(s_title[i], LV_BORDER_SIDE_BOTTOM, 0);
                lv_obj_set_style_border_width(s_title[i], cfg->sep_thick, 0);
                lv_color_t sep_c = AREX_GREEN;
                lv_obj_set_style_border_color(s_title[i], sep_c, 0);
                lv_obj_set_style_border_opa(s_title[i], AREX_ALPHA_TO_OPA(cfg->sep_alpha), 0);
            } else {
                lv_obj_set_style_border_side(s_title[i], LV_BORDER_SIDE_NONE, 0);
            }
        }

        /* 数值区（主值） */
        if (s_value[i]) {
            lv_obj_set_pos(s_value[i],  0, title_h);
            lv_obj_set_size(s_value[i], w, bh - title_h);
            lv_obj_set_style_text_align(s_value[i], align_to_lv(cfg->align_huge), 0);
        }
    }

    /* 双拼块特殊处理：NDL(1), POD(2), BATT(3) */
    static const int split_blocks[] = {BLOCK_NDL, BLOCK_POD, BLOCK_BATT};
    for (int si = 0; si < 3; si++) {
        int i = split_blocks[si];
        if (!s_split_L[i] || !s_split_R[i]) continue;

        int16_t bh = g_layout.block_h[i];
        int16_t half_w = w / 2;

        /* 左子块 */
        lv_obj_set_pos(s_split_L[i],  0,      0);
        lv_obj_set_size(s_split_L[i], half_w, bh);
        /* 右子块 */
        lv_obj_set_pos(s_split_R[i],  half_w, 0);
        lv_obj_set_size(s_split_R[i], half_w, bh);

        /* 标题/数值的对齐：split_outward 模式下强制外展 */
        lv_text_align_t align_l, align_r;
        if (cfg->split_outward) {
            align_l = LV_TEXT_ALIGN_LEFT;
            align_r = LV_TEXT_ALIGN_RIGHT;
        } else {
            align_l = align_to_lv(cfg->align_med);
            align_r = align_to_lv(cfg->align_med);
        }

        if (s_title_L[i]) {
            lv_obj_set_pos(s_title_L[i],  0, 0);
            lv_obj_set_size(s_title_L[i], half_w, title_h);
            lv_obj_set_style_text_align(s_title_L[i], align_l, 0);
        }
        if (s_value_L[i]) {
            lv_obj_set_pos(s_value_L[i],  0, title_h);
            lv_obj_set_size(s_value_L[i], half_w, bh - title_h);
            lv_obj_set_style_text_align(s_value_L[i], align_l, 0);
        }
        if (s_title_R[i]) {
            lv_obj_set_pos(s_title_R[i],  0, 0);
            lv_obj_set_size(s_title_R[i], half_w, title_h);
            lv_obj_set_style_text_align(s_title_R[i], align_r, 0);
        }
        if (s_value_R[i]) {
            lv_obj_set_pos(s_value_R[i],  0, title_h);
            lv_obj_set_size(s_value_R[i], half_w, bh - title_h);
            lv_obj_set_style_text_align(s_value_R[i], align_r, 0);
        }
    }
}

/* =========================================================
 * 7. 推算右侧卡片 Y 坐标（电梯映射）
 * ========================================================= */
static void layout_calc_cards(void)
{
    int16_t card_h = g_layout.rc_h;
    g_layout.card_h = card_h;

    for (int i = 0; i < AREX_CARD_COUNT; i++) {
        g_layout.card_y[i] = i * card_h;
        if (s_card_objs[i]) {
            lv_obj_set_pos(s_card_objs[i],  0, g_layout.card_y[i]);
            lv_obj_set_size(s_card_objs[i], g_layout.rc_w, card_h);
        }
    }
}

/* =========================================================
 * 8. 推算 5F 网格单元尺寸
 * ========================================================= */
static void layout_calc_grid(void)
{
    g_layout.grid_unit_w = g_layout.rc_w / 5;
    g_layout.grid_unit_h = g_layout.rc_h / 6;
}

/* =========================================================
 * 9. 更新卡片指示器位置
 * ========================================================= */
static void layout_apply_dots(const arex_sys_config_t *cfg)
{
    if (!s_dots_cont) return;

    int16_t dot_size = 6;
    int16_t dot_gap  = 8;
    int16_t n = AREX_CARD_COUNT;

    switch (cfg->dots_position) {
        case AREX_DOTS_NONE:
            lv_obj_add_flag(s_dots_cont, LV_OBJ_FLAG_HIDDEN);
            return;

        case AREX_DOTS_LEFT:
            lv_obj_clear_flag(s_dots_cont, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_size(s_dots_cont, dot_size, n * dot_size + (n-1) * dot_gap);
            lv_obj_set_pos(s_dots_cont, 4, (g_layout.rc_h - (n * dot_size + (n-1)*dot_gap)) / 2);
            break;

        case AREX_DOTS_BOTTOM:
            lv_obj_clear_flag(s_dots_cont, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_size(s_dots_cont, n * dot_size + (n-1) * dot_gap, dot_size);
            lv_obj_set_pos(s_dots_cont,
                           (g_layout.rc_w - (n * dot_size + (n-1)*dot_gap)) / 2,
                           g_layout.rc_h - dot_size - 8);
            break;

        default: /* AREX_DOTS_RIGHT */
            lv_obj_clear_flag(s_dots_cont, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_size(s_dots_cont, dot_size, n * dot_size + (n-1) * dot_gap);
            lv_obj_set_pos(s_dots_cont,
                           g_layout.rc_w - dot_size - 8,
                           (g_layout.rc_h - (n * dot_size + (n-1)*dot_gap)) / 2);
            break;
    }

    /* 布局每个点：在容器内按列/行绝对定位 */
    for (int i = 0; i < n; i++) {
        if (!s_dots[i]) continue;
        lv_obj_set_size(s_dots[i], dot_size, dot_size);
        if (cfg->dots_position == AREX_DOTS_BOTTOM) {
            lv_obj_set_pos(s_dots[i], i * (dot_size + dot_gap), 0);
        } else {
            lv_obj_set_pos(s_dots[i], 0, i * (dot_size + dot_gap));
        }
    }
}

/* =========================================================
 * 10. 构建左锚区所有 lv_obj（仅调用一次）
 * ========================================================= */
static void build_left_anchor(void)
{
    lv_obj_t *la = g_layout.left_anchor;
    const arex_sys_config_t *cfg = &g_sys_config;
    int16_t title_h = U2PX(cfg->title_h_u);

    /* --- BLOCK_DEPTH: 单通栏 DEPTH --- */
    s_blk[BLOCK_DEPTH] = make_bare_obj(la);
    s_title[BLOCK_DEPTH] = make_label(s_blk[BLOCK_DEPTH], AREX_FONT_SMALL, AREX_LIGHT, "DEPTH");
    s_value[BLOCK_DEPTH] = make_label(s_blk[BLOCK_DEPTH], AREX_FONT_HUGE,  AREX_GREEN, "45.2");
    lv_obj_set_style_text_letter_space(s_value[BLOCK_DEPTH], -2, 0);

    /* --- BLOCK_NDL: 双拼 NDL / TTS --- */
    s_blk[BLOCK_NDL] = make_bare_obj(la);
    s_split_L[BLOCK_NDL] = make_bare_obj(s_blk[BLOCK_NDL]);
    s_split_R[BLOCK_NDL] = make_bare_obj(s_blk[BLOCK_NDL]);

    s_title_L[BLOCK_NDL] = make_label(s_split_L[BLOCK_NDL], AREX_FONT_SMALL, AREX_LIGHT, "NDL");
    s_value_L[BLOCK_NDL] = make_label(s_split_L[BLOCK_NDL], AREX_FONT_MEDIUM, AREX_GREEN, "0");

    s_title_R[BLOCK_NDL] = make_label(s_split_R[BLOCK_NDL], AREX_FONT_SMALL, AREX_LIGHT, "TTS");
    s_value_R[BLOCK_NDL] = make_label(s_split_R[BLOCK_NDL], AREX_FONT_MEDIUM, AREX_GREEN, "24'");
    /* TTS 反色芯片效果 */
    lv_obj_set_style_bg_color(s_value_R[BLOCK_NDL], AREX_GREEN, 0);
    lv_obj_set_style_bg_opa(s_value_R[BLOCK_NDL],  LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(s_value_R[BLOCK_NDL], AREX_BLACK, 0);
    lv_obj_set_style_pad_hor(s_value_R[BLOCK_NDL],  4, 0);

    /* --- BLOCK_POD: 双拼 POD1 / POD2 --- */
    s_blk[BLOCK_POD] = make_bare_obj(la);
    s_split_L[BLOCK_POD] = make_bare_obj(s_blk[BLOCK_POD]);
    s_split_R[BLOCK_POD] = make_bare_obj(s_blk[BLOCK_POD]);
    s_title_L[BLOCK_POD] = make_label(s_split_L[BLOCK_POD], AREX_FONT_SMALL, AREX_LIGHT, "POD 1");
    s_value_L[BLOCK_POD] = make_label(s_split_L[BLOCK_POD], AREX_FONT_TITLE, AREX_GREEN, "210");
    s_title_R[BLOCK_POD] = make_label(s_split_R[BLOCK_POD], AREX_FONT_SMALL, AREX_LIGHT, "POD 2");
    s_value_R[BLOCK_POD] = make_label(s_split_R[BLOCK_POD], AREX_FONT_TITLE, AREX_LIGHT, "195");

    /* --- BLOCK_BATT: 双拼 BATT / W.TIME --- */
    s_blk[BLOCK_BATT] = make_bare_obj(la);
    s_split_L[BLOCK_BATT] = make_bare_obj(s_blk[BLOCK_BATT]);
    s_split_R[BLOCK_BATT] = make_bare_obj(s_blk[BLOCK_BATT]);
    s_title_L[BLOCK_BATT] = make_label(s_split_L[BLOCK_BATT], AREX_FONT_SMALL, AREX_LIGHT, "BATT");
    s_value_L[BLOCK_BATT] = make_label(s_split_L[BLOCK_BATT], AREX_FONT_TITLE, AREX_GREEN, "85%");
    s_title_R[BLOCK_BATT] = make_label(s_split_R[BLOCK_BATT], AREX_FONT_SMALL, AREX_LIGHT, "W.TIME");
    s_value_R[BLOCK_BATT] = make_label(s_split_R[BLOCK_BATT], AREX_FONT_TITLE, AREX_GREEN, "2.1L");

    /* --- BLOCK_GAS: 单通栏 GAS + PO2 行 --- */
    s_blk[BLOCK_GAS] = make_bare_obj(la);
    s_title[BLOCK_GAS] = make_label(s_blk[BLOCK_GAS], AREX_FONT_SMALL, AREX_LIGHT, "GAS");
    s_value[BLOCK_GAS] = make_label(s_blk[BLOCK_GAS], AREX_FONT_MEDIUM, AREX_GREEN, "TX 18/45");

    /* PO2 三段值（在 GAS 块内，紧贴 value 下方，使用 SMALL 字号） */
    int16_t po2_y = title_h + U2PX(3);   /* 在 value 下方 */
    lv_obj_t *lbl_po2_cap = make_label(s_blk[BLOCK_GAS], AREX_FONT_SMALL, AREX_LIGHT, "PO2");
    lv_obj_set_pos(lbl_po2_cap, 0, po2_y);

    /* 三段值：固定 X 偏移，相对 GAS 块 */
    int16_t ppo2_x[3] = {28, 58, 88};
    const char *ppo2_init[3] = {"1.2", "1.2", "1.3"};
    for (int i = 0; i < 3; i++) {
        s_ppo2_val[i] = make_label(s_blk[BLOCK_GAS], AREX_FONT_SMALL, AREX_GREEN, ppo2_init[i]);
        lv_obj_set_pos(s_ppo2_val[i], ppo2_x[i], po2_y + 16);
        if (i < 2) {
            s_ppo2_sep[i] = make_label(s_blk[BLOCK_GAS], AREX_FONT_SMALL, AREX_GREEN, "|");
            lv_obj_set_pos(s_ppo2_sep[i], ppo2_x[i] + 26, po2_y + 16);
            lv_obj_set_style_text_opa(s_ppo2_sep[i], LV_OPA_30, 0);
        }
    }

    /* --- BLOCK_TIME: 单通栏 DIVE TIME --- */
    s_blk[BLOCK_TIME] = make_bare_obj(la);
    s_title[BLOCK_TIME] = make_label(s_blk[BLOCK_TIME], AREX_FONT_SMALL, AREX_LIGHT, "TIME");
    s_value[BLOCK_TIME] = make_label(s_blk[BLOCK_TIME], AREX_FONT_MEDIUM, AREX_GREEN, "38:14");
}

/* =========================================================
 * 11. 构建右侧卡片区 lv_obj 电梯（仅调用一次）
 * ========================================================= */
static void build_right_canvas(void)
{
    lv_obj_t *rc = g_layout.right_canvas;

    /* 电梯父容器：所有卡片的共同父对象，overflow hidden */
    for (int i = 0; i < AREX_CARD_COUNT; i++) {
        uint8_t card_id = g_sys_config.card_order[i];
        s_card_objs[i] = make_bare_obj(rc);
        lv_obj_set_style_bg_color(s_card_objs[i], AREX_BLACK, 0);
        lv_obj_set_style_bg_opa(s_card_objs[i],  LV_OPA_COVER, 0);
        lv_obj_set_style_clip_corner(s_card_objs[i], true, 0);

        /* 委托给 card_registry 创建卡片内容 */
        arex_card_reg_t *card = arex_card_get_by_id(card_id);
        if (card && card->create_cb) {
            card->tile_obj = s_card_objs[i];
            card->create_cb(s_card_objs[i]);
        }
    }

    /* 卡片指示器容器（绝对定位在 right_canvas 内） */
    s_dots_cont = make_bare_obj(rc);
    for (int i = 0; i < AREX_CARD_COUNT; i++) {
        s_dots[i] = lv_obj_create(s_dots_cont);
        lv_obj_set_style_radius(s_dots[i], 0, 0);
        lv_obj_set_style_bg_color(s_dots[i], AREX_DARK, 0);
        lv_obj_set_style_bg_opa(s_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_dots[i], 0, 0);
        lv_obj_set_style_pad_all(s_dots[i], 0, 0);
        lv_obj_set_style_shadow_width(s_dots[i], 0, 0);
        lv_obj_set_style_shadow_color(s_dots[i], AREX_GREEN, 0);
        lv_obj_set_scrollbar_mode(s_dots[i], LV_SCROLLBAR_MODE_OFF);
        lv_obj_clear_flag(s_dots[i], LV_OBJ_FLAG_SCROLLABLE);
    }
}

/* =========================================================
 * 12. 公开 API：滚动到某张卡片（修改电梯容器 Y 坐标）
 * ========================================================= */
void arex_ui_scroll_to_card(uint8_t idx)
{
    if (idx >= AREX_CARD_COUNT) return;
    /* 通过移动每个 card_obj 实现"电梯"效果：
       card[i] 的显示 Y = card_y[i] - current_scroll_offset
       简化实现：直接修改 right_canvas 的 clip 偏移 */
    int16_t offset_y = -(idx * g_layout.card_h);
    for (int i = 0; i < AREX_CARD_COUNT; i++) {
        if (s_card_objs[i]) {
            lv_obj_set_y(s_card_objs[i], g_layout.card_y[i] + offset_y);
        }
    }
}

/* =========================================================
 * 13. 公开 API：更新滚动点激活状态
 * ========================================================= */
void arex_ui_set_dot_active(uint8_t idx)
{
    for (int i = 0; i < AREX_CARD_COUNT; i++) {
        if (!s_dots[i]) continue;
        if (i == idx) {
            lv_obj_set_style_bg_color(s_dots[i], AREX_GREEN, 0);
            lv_obj_set_style_shadow_width(s_dots[i], 8, 0);
            lv_obj_set_style_shadow_opa(s_dots[i],   LV_OPA_COVER, 0);
        } else {
            lv_obj_set_style_bg_color(s_dots[i], AREX_DARK, 0);
            lv_obj_set_style_shadow_width(s_dots[i], 0, 0);
            lv_obj_set_style_shadow_opa(s_dots[i],   LV_OPA_TRANSP, 0);
        }
    }
}

/* =========================================================
 * 14. arex_ui_apply_config — 核心重排入口
 *     配置变更后调用，重新推算所有坐标并移动 lv_obj
 *     不销毁重建，只 set_pos / set_size
 * ========================================================= */
void arex_ui_apply_config(void)
{
    const arex_sys_config_t *cfg = &g_sys_config;

    /* Step 1: 更新 safe_zone 尺寸与光学偏移 */
    lv_obj_set_size(g_layout.safe_zone, cfg->safe_zone_w, cfg->safe_zone_h);
    lv_obj_align(g_layout.safe_zone, LV_ALIGN_CENTER,
                 cfg->offset_x, cfg->offset_y);

    /* Step 2: 计算左锚区总高度（Classic 模式需要） */
    int16_t anchor_h = arex_layout_calc_anchor_h(cfg);

    /* Step 3: 推算两区绝对坐标并应用 */
    layout_calc_regions(cfg, anchor_h);

    /* Step 4: 推算 10U 块 Y 坐标并应用到左锚区 obj */
    layout_calc_blocks(cfg);
    layout_apply_blocks(cfg);

    /* Step 5: 推算右侧卡片电梯坐标 */
    layout_calc_cards();

    /* Step 6: 推算 5F 网格单元尺寸 */
    layout_calc_grid();

    /* Step 7: 应用卡片指示器位置 */
    layout_apply_dots(cfg);

    /* Step 8: 通知各卡片自行重排内部布局 */
    uint8_t count = arex_card_count();
    for (uint8_t i = 0; i < count; i++) {
        arex_card_reg_t *card = arex_card_get(i);
        if (card && card->update_cb) card->update_cb();
    }
}

/* =========================================================
 * 15. arex_ui_update_data — 数据刷新入口（定时器调用）
 *     只更新 label 文本，绝不排版
 * ========================================================= */
void arex_ui_update_data(void)
{
    const arex_sensor_data_t *d = &g_sensor;
    char buf[32];

    /* DEPTH */
    if (s_value[BLOCK_DEPTH]) {
        snprintf(buf, sizeof(buf), "%.1f", d->depth_m);
        lv_label_set_text(s_value[BLOCK_DEPTH], buf);
    }

    /* NDL / TTS */
    if (s_value_L[BLOCK_NDL]) {
        if (d->ndl_min >= 0)
            snprintf(buf, sizeof(buf), "%d", d->ndl_min);
        else
            lv_snprintf(buf, sizeof(buf), "DCO");
        lv_label_set_text(s_value_L[BLOCK_NDL], buf);
    }
    if (s_value_R[BLOCK_NDL]) {
        snprintf(buf, sizeof(buf), "%d'", d->tts_min);
        lv_label_set_text(s_value_R[BLOCK_NDL], buf);
    }

    /* POD1 / POD2 */
    if (s_value_L[BLOCK_POD]) {
        snprintf(buf, sizeof(buf), "%d", d->pod1_bar);
        lv_label_set_text(s_value_L[BLOCK_POD], buf);
    }
    if (s_value_R[BLOCK_POD]) {
        snprintf(buf, sizeof(buf), "%d", d->pod2_bar);
        lv_label_set_text(s_value_R[BLOCK_POD], buf);
    }

    /* BATT */
    if (s_value_L[BLOCK_BATT]) {
        snprintf(buf, sizeof(buf), "%d%%", d->battery_pct);
        lv_label_set_text(s_value_L[BLOCK_BATT], buf);
    }

    /* GAS 名称（从旧数据总线桥接） */
    /* TIME */
    if (s_value[BLOCK_TIME]) {
        uint16_t m = d->dive_time_sec / 60;
        uint16_t s = d->dive_time_sec % 60;
        snprintf(buf, sizeof(buf), "%02d:%02d", m, s);
        lv_label_set_text(s_value[BLOCK_TIME], buf);
    }

    /* PO2 三段值 */
    for (int i = 0; i < 3; i++) {
        if (s_ppo2_val[i]) {
            snprintf(buf, sizeof(buf), "%.1f", d->ppo2[i]);
            lv_label_set_text(s_ppo2_val[i], buf);
        }
    }
}

/* =========================================================
 * 16. arex_ui_engine_init — 初始化入口
 * ========================================================= */
void arex_ui_engine_init(void)
{
    /* 1. 加载默认配置 */
    arex_sys_config_default(&g_sys_config);
    memset(&g_sensor, 0, sizeof(g_sensor));
    memset(&g_layout, 0, sizeof(g_layout));

    /* 2. 创建物理屏幕对象 */
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, AREX_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);

    /* 3. 创建 safe_zone（ui 根容器，所有子组件挂在这里）
          初始尺寸/偏移由 apply_config 写入，这里先给合理初值 */
    g_layout.safe_zone = make_bare_obj(scr);
    lv_obj_set_style_bg_opa(g_layout.safe_zone, LV_OPA_TRANSP, 0);

    /* 4. 创建左锚区容器（父：safe_zone） */
    g_layout.left_anchor = make_bare_obj(g_layout.safe_zone);
    lv_obj_set_style_bg_color(g_layout.left_anchor, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(g_layout.left_anchor,  LV_OPA_COVER, 0);
    /* 右侧分割线 */
    lv_obj_set_style_border_color(g_layout.left_anchor, AREX_DARK, 0);
    lv_obj_set_style_border_width(g_layout.left_anchor, 2, 0);
    lv_obj_set_style_border_side(g_layout.left_anchor,
                                 LV_BORDER_SIDE_RIGHT, 0);

    /* 5. 创建右侧卡片容器（父：safe_zone），clip 超出部分 */
    g_layout.right_canvas = make_bare_obj(g_layout.safe_zone);
    lv_obj_set_style_bg_color(g_layout.right_canvas, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(g_layout.right_canvas,  LV_OPA_COVER, 0);
    lv_obj_set_style_clip_corner(g_layout.right_canvas, true, 0);
    /* 清除 OVERFLOW_VISIBLE 确保电梯卡片超出部分被裁剪 */
    lv_obj_clear_flag(g_layout.right_canvas, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    /* 6. 首次执行排版（先算出 rc_w/rc_h/block_y 缓存，build 函数需要它）*/
    arex_ui_apply_config();

    /* 7. 构建左锚区内部组件（只建一次，依赖 g_layout.la_w/block_h） */
    build_left_anchor();

    /* 8. 构建右侧卡片电梯（只建一次，依赖 g_layout.rc_w/rc_h/card_h） */
    build_right_canvas();

    /* 9. 再次 apply 以把刚建好的 lv_obj 移到正确位置 */
    arex_ui_apply_config();

    /* 10. 默认显示第 0 张卡片 */
    arex_ui_scroll_to_card(0);
    arex_ui_set_dot_active(0);
}
