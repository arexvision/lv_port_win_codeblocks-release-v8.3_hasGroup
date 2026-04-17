/*
 * card_deco.c  —  2F: TISSUES & DECO
 *
 * 架构规则：
 *   - 16 根 bar 的 X 坐标 = i * (total_w / 16)，底部对齐（绝对坐标）
 *   - 宽高由 g_layout.rc_w / rc_h 决定
 *   - 从 g_sensor 读取组织饱和度数据
 */

#include "../arex_ui_engine.h"
#include "../arex_screen.h"
#include "../fonts/arex_fonts.h"
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <stdbool.h>

#define TITLE_H          44
#define PAD_X            16
#define ROW_H            40    /* 每行 deco-grid 的高度 */
#define ROW_GAP           6
#define BAR_H            80
#define BAR_LBL_H        14
#define BOTTOM_PAD       30
#define SEC_TITLE_H      14
#define SEC_GAP           6

#define TISSUE_DANGER_PCT  90
#define TISSUE_HIGH_MIN    70
#define TISSUE_FLASH_MS   300

static lv_obj_t *s_bars[16];
static lv_obj_t *s_lbl_gf99;
static lv_obj_t *s_lbl_surf_gf;
static lv_obj_t *s_lbl_cns;
static lv_obj_t *s_lbl_otu;
static lv_timer_t *s_flash_timer;
static bool        s_flash_phase;

void card_deco_update(void);

static bool any_danger(void)
{
    for (int i = 0; i < 16; i++)
        if (g_sensor.tissue_pct[i] >= TISSUE_DANGER_PCT) return true;
    return false;
}

static void flash_cb(lv_timer_t *t)
{
    (void)t;
    s_flash_phase = !s_flash_phase;
    for (int i = 0; i < 16; i++) {
        if (g_sensor.tissue_pct[i] >= TISSUE_DANGER_PCT) {
            lv_color_t c = s_flash_phase ? AREX_GREEN : AREX_BLACK;
            lv_obj_set_style_bg_color(s_bars[i], c, LV_PART_INDICATOR);
        }
    }
}

static void flash_ensure(void)
{
    if (any_danger()) {
        if (!s_flash_timer) {
            s_flash_phase = false;
            s_flash_timer = lv_timer_create(flash_cb, TISSUE_FLASH_MS, NULL);
        }
    } else {
        if (s_flash_timer) {
            lv_timer_del(s_flash_timer);
            s_flash_timer = NULL;
        }
    }
}

static lv_color_t bar_color(uint8_t pct)
{
    if (pct >= TISSUE_DANGER_PCT)
        return s_flash_phase ? AREX_GREEN : AREX_BLACK;
    if (pct > TISSUE_HIGH_MIN)
        return AREX_LIGHT;
    return AREX_GREEN;
}

static void surf_gf_style(void)
{
    if (g_sensor.surf_gf > 100) {
        lv_obj_set_style_bg_color(s_lbl_surf_gf, AREX_GREEN, 0);
        lv_obj_set_style_bg_opa(s_lbl_surf_gf,   LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(s_lbl_surf_gf, AREX_BLACK, 0);
        lv_obj_set_style_pad_hor(s_lbl_surf_gf,  4, 0);
    } else {
        lv_obj_set_style_bg_opa(s_lbl_surf_gf,   LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(s_lbl_surf_gf, AREX_GREEN, 0);
        lv_obj_set_style_pad_hor(s_lbl_surf_gf,  0, 0);
    }
}

/* 建一行 deco-grid：左右各一组 cap + val label */
static void make_row(lv_obj_t *parent, int16_t y, int16_t row_w,
                     const char *lc, const char *lv_, lv_obj_t **lv_ref,
                     const char *rc, const char *rv, lv_obj_t **rv_ref,
                     bool sep_line)
{
    int16_t right_x = row_w / 2;

    lv_obj_t *o;
    o = lv_label_create(parent);
    lv_obj_set_style_text_color(o, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(o,  AREX_FONT_SMALL, 0);
    lv_obj_set_style_bg_opa(o,     LV_OPA_TRANSP, 0);
    lv_label_set_text(o, lc);
    lv_obj_set_pos(o, PAD_X, y);

    o = lv_label_create(parent);
    lv_obj_set_style_text_color(o, AREX_GREEN, 0);
    lv_obj_set_style_text_font(o,  AREX_FONT_TITLE, 0);
    lv_obj_set_style_bg_opa(o,     LV_OPA_TRANSP, 0);
    lv_label_set_text(o, lv_);
    lv_obj_set_pos(o, PAD_X, y + 16);
    if (lv_ref) *lv_ref = o;

    o = lv_label_create(parent);
    lv_obj_set_style_text_color(o, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(o,  AREX_FONT_SMALL, 0);
    lv_obj_set_style_bg_opa(o,     LV_OPA_TRANSP, 0);
    lv_label_set_text(o, rc);
    lv_obj_set_pos(o, right_x + PAD_X, y);

    o = lv_label_create(parent);
    lv_obj_set_style_text_color(o, AREX_GREEN, 0);
    lv_obj_set_style_text_font(o,  AREX_FONT_TITLE, 0);
    lv_obj_set_style_bg_opa(o,     LV_OPA_TRANSP, 0);
    lv_label_set_text(o, rv);
    lv_obj_set_pos(o, right_x + PAD_X, y + 16);
    if (rv_ref) *rv_ref = o;

    if (sep_line) {
        lv_obj_t *line = lv_obj_create(parent);
        lv_obj_set_size(line, row_w - PAD_X * 2, 1);
        lv_obj_set_pos(line,  PAD_X, y + ROW_H - 2);
        lv_obj_set_style_bg_color(line, AREX_DARK, 0);
        lv_obj_set_style_bg_opa(line,   LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(line, 0, 0);
        lv_obj_set_style_pad_all(line,   0, 0);
        lv_obj_set_style_radius(line,    0, 0);
    }
}

void card_deco_create(lv_obj_t *parent)
{
    arex_screen_make_card_title(parent, "2F: TISSUES & DECO");

    int16_t card_w = g_layout.rc_w;
    int16_t card_h = g_layout.rc_h;

    /* 三行 deco-grid（从 TITLE_H 开始，每行 ROW_H+ROW_GAP）*/
    int16_t cy = TITLE_H;
    make_row(parent, cy, card_w,
             "ALGORITHM", "ZHL-16C", NULL,
             "GF LOW / HIGH", "30 / 70", NULL, true);
    cy += ROW_H + ROW_GAP;

    make_row(parent, cy, card_w,
             "GF99", "--", &s_lbl_gf99,
             "SurfGF", "--", &s_lbl_surf_gf, true);
    cy += ROW_H + ROW_GAP;

    make_row(parent, cy, card_w,
             "CNS O2", "--%", &s_lbl_cns,
             "OTU", "--", &s_lbl_otu, false);
    cy += ROW_H + ROW_GAP;

    /* TISSUE SATURATION 标题（贴在柱状图上方）*/
    int16_t bars_bottom = card_h - BOTTOM_PAD;
    int16_t bars_top    = bars_bottom - BAR_H;
    int16_t sec_y       = bars_top - SEC_GAP - SEC_TITLE_H - BAR_LBL_H;

    lv_obj_t *sec = lv_label_create(parent);
    lv_obj_set_style_text_color(sec, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(sec,  AREX_FONT_SMALL, 0);
    lv_obj_set_style_bg_opa(sec,     LV_OPA_TRANSP, 0);
    lv_label_set_text(sec, "TISSUE SATURATION");
    lv_obj_set_pos(sec, PAD_X, sec_y);

    /* 16 根 bar：X = i * bar_unit_w，底部对齐于 bars_bottom */
    int16_t tissue_area_w = card_w - PAD_X * 2;
    int16_t bar_unit_w    = tissue_area_w / 16;
    int16_t bar_w         = bar_unit_w - 3; /* 留 3px 间隙 */

    for (int i = 0; i < 16; i++) {
        lv_coord_t bx = PAD_X + i * bar_unit_w;
        lv_coord_t by = bars_top;

        lv_obj_t *bar = lv_bar_create(parent);
        lv_bar_set_range(bar, 0, 110);
        lv_obj_set_size(bar, bar_w, BAR_H);
        lv_obj_set_pos(bar,  bx, by);
        lv_bar_set_start_value(bar, 0, LV_ANIM_OFF);
        lv_bar_set_value(bar, 0, LV_ANIM_OFF);

        lv_obj_set_style_bg_color(bar, AREX_DARK, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar,   LV_OPA_50,  LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar, AREX_GREEN, LV_PART_INDICATOR);
        lv_obj_set_style_radius(bar, 0, 0);
        lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR);
        s_bars[i] = bar;

        /* 隔室编号 label */
        lv_obj_t *lbl = lv_label_create(parent);
        lv_obj_set_style_text_color(lbl, AREX_LIGHT, 0);
        lv_obj_set_style_text_font(lbl,  AREX_FONT_SMALL, 0);
        lv_obj_set_style_bg_opa(lbl,     LV_OPA_TRANSP, 0);
        char nbuf[4];
        snprintf(nbuf, sizeof(nbuf), "%d", i + 1);
        lv_label_set_text(lbl, nbuf);
        lv_obj_set_pos(lbl, bx, bars_bottom + 2);
    }

    /* M 值线（bar 顶部 20% 处）*/
    int16_t mline_y = bars_top + (BAR_H * 20 / 100);
    lv_obj_t *mline = lv_obj_create(parent);
    lv_obj_set_size(mline, tissue_area_w, 2);
    lv_obj_set_pos(mline,  PAD_X, mline_y);
    lv_obj_set_style_bg_color(mline, AREX_GREEN, 0);
    lv_obj_set_style_bg_opa(mline,   LV_OPA_50, 0);
    lv_obj_set_style_border_width(mline, 0, 0);
    lv_obj_set_style_pad_all(mline,  0, 0);
    lv_obj_set_style_radius(mline,   0, 0);

    lv_obj_t *mlbl = lv_label_create(parent);
    lv_obj_set_style_text_color(mlbl, AREX_GREEN, 0);
    lv_obj_set_style_text_font(mlbl,  AREX_FONT_SMALL, 0);
    lv_obj_set_style_bg_opa(mlbl,     LV_OPA_TRANSP, 0);
    lv_label_set_text(mlbl, "M-VALUE");
    lv_obj_set_pos(mlbl, PAD_X + tissue_area_w - 56, mline_y - 14);

    card_deco_update();
}

void card_deco_update(void)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", g_sensor.gf99);
    lv_label_set_text(s_lbl_gf99, buf);

    snprintf(buf, sizeof(buf), "%d%%", g_sensor.surf_gf);
    lv_label_set_text(s_lbl_surf_gf, buf);
    surf_gf_style();

    snprintf(buf, sizeof(buf), "%d%%", g_sensor.cns_pct);
    lv_label_set_text(s_lbl_cns, buf);

    snprintf(buf, sizeof(buf), "%d", g_sensor.otu);
    lv_label_set_text(s_lbl_otu, buf);

    flash_ensure();

    for (int i = 0; i < 16; i++) {
        uint8_t pct = g_sensor.tissue_pct[i];
        lv_bar_set_value(s_bars[i], pct, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(s_bars[i], AREX_DARK, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_bars[i],   LV_OPA_50, LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_bars[i], bar_color(pct), LV_PART_INDICATOR);
    }
}
