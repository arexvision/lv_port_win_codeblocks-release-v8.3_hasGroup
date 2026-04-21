#include "../arex_screen.h"
#include "../arex_data.h"
#include "../arex_ui_engine.h"
#include "lvgl/lvgl.h"
#include "../fonts/arex_fonts.h"
#include <stdio.h>
#include <stdbool.h>

#define BAR_W   23
#define BAR_H   80
#define BAR_GAP 4
#define GRID_X  16
#define TISSUE_AREA_W    (16 * (BAR_W + BAR_GAP) - BAR_GAP)

/* Tile content height — 运行时从配置读取，不依赖硬编码屏高 */
/* BARS_Y / SEC_TITLE_Y 在 card_deco_create() 内作为局部变量计算 */
/* 规范：组织区底部余白约 36px（从480px屏高减去bar区+标签+余白）
   调整 BOTTOM_PAD 使 Section Title Y = 250（规范）*/
#define BOTTOM_PAD       36
#define BAR_LBL_GAP      4
#define COMPARTMENT_LBL_H 14 /* AREX_FONT_SMALL line height */
#define SEC_TITLE_GAP    6   /* HTML .tissue-section-title 与组织区的间距 */
#define SEC_TITLE_H      14 /* AREX_FONT_SMALL(14px)，规范字体18px暂无 */

/* 底部余白、标签间距等保持不变，BARS_Y 在函数内动态计算 */

/* HTML: .t-fill.high for ~75–85%; .t-fill.danger (flashInvert) for top compartment ~95% */
#define TISSUE_DANGER_PCT   90
#define TISSUE_HIGH_MIN     70

/* HTML --flash-speed default 0.3s → 300ms half-period for flashInvert */
#define TISSUE_FLASH_MS     300

static lv_obj_t *s_bars[16];
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
            lv_obj_set_style_bg_color(s_bars[i], c, LV_PART_INDICATOR);
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

/* HTML .t-bar: rgba(0,51,0,0.5) — AREX_DARK is #003300 */
static void bar_set_track_style(lv_obj_t *bar)
{
    lv_obj_set_style_bg_color(bar, AREX_DARK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_50, LV_PART_MAIN);
}

static lv_color_t tissue_fill_color(uint8_t pct)
{
    if (pct >= TISSUE_DANGER_PCT)
        return s_tissue_flash_phase ? AREX_GREEN : AREX_BLACK;
    if (pct > TISSUE_HIGH_MIN)
        return AREX_LIGHT;
    return AREX_GREEN;
}

/* HTML .highlight-invert on SurfGF when over limit (145% demo): green bg, black text */
static void surf_gf_apply_style(void)
{
    if (g_sensor_data.cns_pct > 50) {
        lv_obj_set_style_bg_color(s_lbl_surf_gf, AREX_GREEN, 0);
        lv_obj_set_style_bg_opa(s_lbl_surf_gf, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(s_lbl_surf_gf, AREX_BLACK, 0);
        lv_obj_set_style_pad_hor(s_lbl_surf_gf, 4, 0);
        lv_obj_set_style_pad_ver(s_lbl_surf_gf, 0, 0);
    } else {
        lv_obj_set_style_bg_opa(s_lbl_surf_gf, LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(s_lbl_surf_gf, AREX_GREEN, 0);
        lv_obj_set_style_pad_hor(s_lbl_surf_gf, 0, 0);
        lv_obj_set_style_pad_ver(s_lbl_surf_gf, 0, 0);
    }
}

/* Helper: one deco-grid row (HTML .deco-grid) */
static void make_grid_row(lv_obj_t *parent, lv_coord_t y,
                           const char *left_cap, const char *left_val, lv_obj_t **left_ref,
                           const char *right_cap, const char *right_val, lv_obj_t **right_ref,
                           bool dashed_bottom)
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

    if (dashed_bottom) {
        lv_obj_t *line = lv_obj_create(parent);
        lv_obj_set_size(line, 428, 1);
        lv_obj_set_pos(line, GRID_X, y + 40);
        lv_obj_set_style_bg_color(line, AREX_DARK, 0);
        lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(line, 0, 0);
        lv_obj_set_style_pad_all(line, 0, 0);
        lv_obj_set_style_radius(line, 0, 0);
    }
}

void card_deco_create(lv_obj_t *parent)
{
    /* 运行时推算坐标，跟随 safe_zone_h，不依赖硬编码屏高 */
    lv_coord_t tile_h       = (lv_coord_t)g_sys_config.safe_zone_h;
    lv_coord_t bar_lbl_bot  = tile_h - BOTTOM_PAD;
    lv_coord_t bars_y       = bar_lbl_bot - COMPARTMENT_LBL_H - BAR_LBL_GAP - BAR_H;
    lv_coord_t sec_title_y  = bars_y - SEC_TITLE_GAP - SEC_TITLE_H;

    arex_screen_make_card_title(parent, "2F: TISSUES & DECO");

    /* HTML order: three .deco-grid rows under title
       调整行间距：section title y=290，rows均匀分布 */
    make_grid_row(parent, 60,                  /* 原 50 */
                  "ALGORITHM", "ZHL-16C", NULL,
                  "GF LOW / HIGH", "30 / 70", NULL,
                  true);

    make_grid_row(parent, 107,                 /* 原 97 */
                  "GF99", "--", &s_lbl_gf99,
                  "SurfGF", "--", &s_lbl_surf_gf,
                  true);

    make_grid_row(parent, 154,                /* 原 144 */
                  "CNS O2", "--%", &s_lbl_cns,
                  "OTU", "--", &s_lbl_otu,
                  false);

    /* HTML .tissue-section-title — directly above .tissue-container, margin-top:auto on card */
    lv_obj_t *sec_lbl = lv_label_create(parent);
    lv_obj_set_style_text_color(sec_lbl, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(sec_lbl, AREX_FONT_SMALL, 0);
    lv_label_set_text(sec_lbl, "TISSUE SATURATION (16 COMPARTMENTS)");
    lv_obj_set_pos(sec_lbl, GRID_X, sec_title_y);

    for (int i = 0; i < 16; i++) {
        lv_obj_t *bar = lv_bar_create(parent);
        lv_bar_set_range(bar, 0, 110);
        lv_obj_set_size(bar, BAR_W, BAR_H);
        lv_obj_set_pos(bar, GRID_X + i * (BAR_W + BAR_GAP), bars_y);
        lv_bar_set_start_value(bar, 0, LV_ANIM_OFF);
        lv_bar_set_value(bar, 0, LV_ANIM_OFF);

        bar_set_track_style(bar);
        lv_obj_set_style_bg_color(bar, AREX_GREEN, LV_PART_INDICATOR);
        lv_obj_set_style_radius(bar, 0, 0);
        lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR);

        s_bars[i] = bar;

        lv_obj_t *lbl = lv_label_create(parent);
        lv_obj_set_style_text_color(lbl, AREX_LIGHT, 0);
        lv_obj_set_style_text_font(lbl, AREX_FONT_SMALL, 0);
        char buf[4];
        snprintf(buf, sizeof(buf), "%d", i + 1);
        lv_label_set_text(lbl, buf);
        lv_obj_set_pos(lbl, GRID_X + i * (BAR_W + BAR_GAP), bars_y + BAR_H + BAR_LBL_GAP);
    }

    /* HTML .m-value-line at top 20% of 80px tissue-container */
    lv_obj_t *mline = lv_obj_create(parent);
    lv_obj_set_size(mline, TISSUE_AREA_W, 2);
    lv_obj_set_pos(mline, GRID_X, bars_y + (lv_coord_t)(BAR_H * 0.2f));
    lv_obj_set_style_bg_color(mline, AREX_GREEN, 0);
    lv_obj_set_style_bg_opa(mline, LV_OPA_50, 0);
    lv_obj_set_style_border_width(mline, 0, 0);
    lv_obj_set_style_pad_all(mline, 0, 0);
    lv_obj_set_style_radius(mline, 0, 0);

    lv_obj_t *mlbl = lv_label_create(parent);
    lv_obj_set_style_text_color(mlbl, AREX_GREEN, 0);
    lv_obj_set_style_text_font(mlbl, AREX_FONT_SMALL, 0);
    lv_obj_set_style_bg_opa(mlbl, LV_OPA_TRANSP, 0);
    lv_label_set_text(mlbl, "M-VALUE");
    lv_obj_set_pos(mlbl, GRID_X + TISSUE_AREA_W - 58,
                   bars_y + (lv_coord_t)(BAR_H * 0.2f) - 12);

    card_deco_update();
}

void card_deco_update(void)
{
    char buf[16];

    snprintf(buf, sizeof(buf), "%d%%", g_sensor_data.cns_pct);
    lv_label_set_text(s_lbl_gf99, buf);

    snprintf(buf, sizeof(buf), "%d%%", g_sensor_data.cns_pct);
    lv_label_set_text(s_lbl_surf_gf, buf);
    surf_gf_apply_style();

    snprintf(buf, sizeof(buf), "%d%%", g_sensor_data.cns_pct);
    lv_label_set_text(s_lbl_cns, buf);

    snprintf(buf, sizeof(buf), "%d", g_sensor_data.otu);
    lv_label_set_text(s_lbl_otu, buf);

    tissue_flash_ensure();

    for (int i = 0; i < 16; i++) {
        uint8_t pct = g_sensor_data.tissue_pct[i];
        lv_bar_set_value(s_bars[i], pct, LV_ANIM_ON);
        bar_set_track_style(s_bars[i]);
        lv_obj_set_style_bg_color(s_bars[i], tissue_fill_color(pct), LV_PART_INDICATOR);
    }
}
