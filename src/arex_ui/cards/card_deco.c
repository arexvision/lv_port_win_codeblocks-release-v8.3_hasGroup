#include "../arex_screen.h"
#include "../arex_data.h"
#include "../arex_ui_engine.h"
#include "lvgl/lvgl.h"
#include "../fonts/arex_fonts.h"
#include <stdio.h>
#include <stdbool.h>

#define GRID_X  16

#define TISSUE_DANGER_PCT   90
#define TISSUE_HIGH_MIN     70
#define TISSUE_FLASH_MS     300

/* 运行时从配置推算，card_deco_create 内局部变量 */
static lv_obj_t *s_bars[16];       /* 每根柱子的 fill 块 */
static int        s_chart_h;        /* 组织图高度（容器总高） */
static int        s_bar_max_h;      /* 柱子最大高度（= chart_h - NUM_LBL_H，给编号留空间） */
static int        s_bar_col_w;      /* 每列宽度（含间隙） */

static lv_obj_t *s_lbl_gf99;
static lv_obj_t *s_lbl_surf_gf;
static lv_obj_t *s_lbl_cns;
static lv_obj_t *s_lbl_otu;

static lv_timer_t *s_tissue_flash_timer;
static bool        s_tissue_flash_phase;

void card_deco_update(void);

static bool any_tissue_danger(void)
{
    for (int i = 0; i < 16; i++) {
        if (g_sensor_data.tissue_pct[i] >= TISSUE_DANGER_PCT) return true;
    }
    return false;
}

static void tissue_danger_flash_cb(lv_timer_t *t)
{
    (void)t;
    s_tissue_flash_phase = !s_tissue_flash_phase;
    for (int i = 0; i < 16; i++) {
        if (g_sensor_data.tissue_pct[i] >= TISSUE_DANGER_PCT) {
            lv_color_t c = s_tissue_flash_phase ? AREX_GREEN : AREX_BLACK;
            lv_obj_set_style_bg_color(s_bars[i], c, 0);
        }
    }
}

static void tissue_flash_ensure(void)
{
    if (any_tissue_danger()) {
        if (!s_tissue_flash_timer) {
            s_tissue_flash_phase = false;
            s_tissue_flash_timer = lv_timer_create(tissue_danger_flash_cb, TISSUE_FLASH_MS, NULL);
        }
    } else {
        if (s_tissue_flash_timer) {
            lv_timer_del(s_tissue_flash_timer);
            s_tissue_flash_timer = NULL;
        }
    }
}

static lv_color_t tissue_fill_color(uint8_t pct)
{
    if (pct >= TISSUE_DANGER_PCT)
        return s_tissue_flash_phase ? AREX_GREEN : AREX_BLACK;
    if (pct > TISSUE_HIGH_MIN)
        return AREX_LIGHT;
    return AREX_GREEN;
}

static void surf_gf_apply_style(void)
{
    if (g_sensor_data.cns_pct > 50) {
        lv_obj_set_style_bg_color(s_lbl_surf_gf, AREX_GREEN, 0);
        lv_obj_set_style_bg_opa(s_lbl_surf_gf, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(s_lbl_surf_gf, AREX_BLACK, 0);
        lv_obj_set_style_pad_hor(s_lbl_surf_gf, 4, 0);
    } else {
        lv_obj_set_style_bg_opa(s_lbl_surf_gf, LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(s_lbl_surf_gf, AREX_GREEN, 0);
        lv_obj_set_style_pad_hor(s_lbl_surf_gf, 0, 0);
    }
}

/* 一行数据网格：caption + value，左右各一组 */
static void make_grid_row(lv_obj_t *parent, lv_coord_t y,
                           const char *left_cap, const char *left_val, lv_obj_t **left_ref,
                           const char *right_cap, const char *right_val, lv_obj_t **right_ref)
{
    lv_obj_t *lc = lv_label_create(parent);
    lv_obj_set_style_text_color(lc, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(lc, AREX_FONT_SMALL, 0);
    lv_label_set_text(lc, left_cap);
    lv_obj_set_pos(lc, GRID_X, y);

    lv_obj_t *lv_ = lv_label_create(parent);
    lv_obj_set_style_text_color(lv_, AREX_GREEN, 0);
    lv_obj_set_style_text_font(lv_, AREX_FONT_TITLE, 0);
    lv_label_set_text(lv_, left_val);
    lv_obj_set_pos(lv_, GRID_X, y + 16);
    if (left_ref) *left_ref = lv_;

    lv_obj_t *rc = lv_label_create(parent);
    lv_obj_set_style_text_color(rc, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(rc, AREX_FONT_SMALL, 0);
    lv_label_set_text(rc, right_cap);
    lv_obj_set_pos(rc, 240, y);

    lv_obj_t *rv = lv_label_create(parent);
    lv_obj_set_style_text_color(rv, AREX_GREEN, 0);
    lv_obj_set_style_text_font(rv, AREX_FONT_TITLE, 0);
    lv_label_set_text(rv, right_val);
    lv_obj_set_pos(rv, 240, y + 16);
    if (right_ref) *right_ref = rv;
}

void card_deco_create(lv_obj_t *parent)
{
    /* 从配置总线推算右侧宽度和图表高度 */
    int right_canvas_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                         - (g_sys_config.gap_u * AREX_BASE_U);
    int chart_h = g_sys_config.h_tissues_chart * AREX_BASE_U; /* 默认 9U = 90px */
    int chart_w = right_canvas_w - 15;

    s_chart_h   = chart_h;
    s_bar_col_w = chart_w / 16;

    arex_screen_make_card_title(parent, "2F: TISSUES & DECO");

    /* 三行数据网格，紧贴标题下方，行高约 40px（14px caption + 20px value + 6px gap） */
    make_grid_row(parent, 55,
                  "ALGORITHM", "ZHL-16C", NULL,
                  "GF LOW / HIGH", "30 / 70", NULL);

    make_grid_row(parent, 100,
                  "GF99", "--", &s_lbl_gf99,
                  "SurfGF", "--", &s_lbl_surf_gf);

    make_grid_row(parent, 145,
                  "CNS O2", "--%", &s_lbl_cns,
                  "OTU", "--", &s_lbl_otu);

    /* 组织图容器：高度 = 柱子区 + 编号行，底部锁定，底部留 8px */
#define NUM_LBL_H  16   /* 编号行高度，给底部数字预留 */
    int total_chart_h = chart_h + NUM_LBL_H;
    /* 柱子最大高度 = chart_h - NUM_LBL_H，给底部编号留出 16px 空间 */
    int bar_max_h = chart_h - NUM_LBL_H;
    s_bar_max_h = bar_max_h;

    lv_obj_t *chart_cont = lv_obj_create(parent);
    lv_obj_set_size(chart_cont, chart_w, total_chart_h);
    lv_obj_align(chart_cont, LV_ALIGN_BOTTOM_LEFT, GRID_X, -8);
    lv_obj_set_style_pad_all(chart_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(chart_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(chart_cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(chart_cont, LV_OBJ_FLAG_SCROLLABLE);

    /* Section 标题，紧贴图表容器上方 8px */
    lv_obj_t *sec_lbl = lv_label_create(parent);
    lv_obj_set_style_text_color(sec_lbl, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(sec_lbl, AREX_FONT_SMALL, 0);
    lv_label_set_text(sec_lbl, "TISSUE SATURATION (16 COMPARTMENTS)");
    lv_obj_align_to(sec_lbl, chart_cont, LV_ALIGN_OUT_TOP_LEFT, 0, -6);

    /* M-VALUE 基准线（bar_max_h 的 top 20% 处） */
    lv_coord_t mline_y = (lv_coord_t)(bar_max_h * 0.2f);
    lv_obj_t *mline = lv_obj_create(chart_cont);
    lv_obj_set_size(mline, chart_w, 2);
    lv_obj_set_pos(mline, 0, mline_y);
    lv_obj_set_style_bg_color(mline, AREX_GREEN, 0);
    lv_obj_set_style_bg_opa(mline, LV_OPA_50, 0);
    lv_obj_set_style_border_width(mline, 0, 0);
    lv_obj_set_style_pad_all(mline, 0, 0);
    lv_obj_set_style_radius(mline, 0, 0);

    /* M-VALUE 标签：黑底遮住后面柱子，像橡皮擦 */
    lv_obj_t *mlbl = lv_label_create(chart_cont);
    lv_obj_set_style_text_color(mlbl, AREX_GREEN, 0);
    lv_obj_set_style_text_font(mlbl, AREX_FONT_SMALL, 0);
    lv_obj_set_style_bg_color(mlbl, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(mlbl, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(mlbl, 3, 0);
    lv_label_set_text(mlbl, "M-VAL");
    lv_obj_set_pos(mlbl, chart_w - 52, mline_y - 12);

    /* 16 根柱子：嵌套降高 — 柱子TOP对齐（留底部给编号），填充底部对齐 */
    int col_w = s_bar_col_w;
    for (int i = 0; i < 16; i++) {
        /* 1. 柱子背景框：TOP_MID 悬空对齐，下方自动空出 16px 给编号 */
        lv_obj_t *bar_bg = lv_obj_create(chart_cont);
        lv_obj_set_pos(bar_bg, i * col_w, 0);
        lv_obj_set_size(bar_bg, col_w - 4, bar_max_h);
        lv_obj_align(bar_bg, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(bar_bg, AREX_DARK, 0);
        lv_obj_set_style_bg_opa(bar_bg, LV_OPA_50, 0);
        lv_obj_set_style_border_width(bar_bg, 0, 0);
        lv_obj_set_style_pad_all(bar_bg, 0, 0);
        lv_obj_set_style_radius(bar_bg, 0, 0);
        lv_obj_clear_flag(bar_bg, LV_OBJ_FLAG_SCROLLABLE);

        /* 2. 绿色填充：宽度撑满柱宽，底部对齐（bar_bg 内贴底生长） */
        lv_obj_t *bar_fill = lv_obj_create(bar_bg);
        lv_obj_set_size(bar_fill, LV_PCT(100), 0);
        lv_obj_align(bar_fill, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(bar_fill, AREX_GREEN, 0);
        lv_obj_set_style_bg_opa(bar_fill, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(bar_fill, 0, 0);
        lv_obj_set_style_pad_all(bar_fill, 0, 0);
        lv_obj_set_style_radius(bar_fill, 0, 0);
        lv_obj_clear_flag(bar_fill, LV_OBJ_FLAG_SCROLLABLE);

        s_bars[i] = bar_fill;

        /* 3. 底部编号（1~16）：真正贴容器底部的元素 */
        lv_obj_t *lbl = lv_label_create(chart_cont);
        lv_obj_set_style_text_color(lbl, AREX_LIGHT, 0);
        lv_obj_set_style_text_font(lbl, AREX_FONT_SMALL, 0);
        char buf[4];
        lv_snprintf(buf, sizeof(buf), "%d", i + 1);
        lv_label_set_text(lbl, buf);
        lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, i * col_w + (col_w - 4) / 2 - 3, -16);
    }

    card_deco_update();
}

void card_deco_update(void)
{
    char buf[16];

    lv_snprintf(buf, sizeof(buf), "%d%%", g_sensor_data.cns_pct);
    lv_label_set_text(s_lbl_gf99, buf);

    lv_snprintf(buf, sizeof(buf), "%d%%", g_sensor_data.cns_pct);
    lv_label_set_text(s_lbl_surf_gf, buf);
    surf_gf_apply_style();

    lv_snprintf(buf, sizeof(buf), "%d%%", g_sensor_data.cns_pct);
    lv_label_set_text(s_lbl_cns, buf);

    lv_snprintf(buf, sizeof(buf), "%d", g_sensor_data.otu);
    lv_label_set_text(s_lbl_otu, buf);

    tissue_flash_ensure();

    for (int i = 0; i < 16; i++) {
        uint8_t pct = g_sensor_data.tissue_pct[i];
        /* 从 0~110 映射到 s_bar_max_h（给底部编号留空间的实际柱子高度） */
        int fill_h = (int)((pct > 110 ? 110 : pct) * s_bar_max_h / 110);
        lv_obj_set_size(s_bars[i], LV_PCT(100), fill_h);
        lv_obj_align(s_bars[i], LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(s_bars[i], tissue_fill_color(pct), 0);
    }
}

