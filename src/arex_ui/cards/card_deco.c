#include "../arex_screen.h"
#include "../arex_data.h"
#include "lvgl/lvgl.h"
#include "../fonts/arex_fonts.h"
#include <stdio.h>

#define BAR_W   23
#define BAR_H   80
#define BAR_GAP 4
#define GRID_X  16

/* Bar area starts at y=230 (after 3 grid rows + tissue label) */
#define BARS_Y  230

static lv_obj_t *s_bars[16];
static lv_obj_t *s_lbl_gf99;
static lv_obj_t *s_lbl_surf_gf;
static lv_obj_t *s_lbl_cns;
static lv_obj_t *s_lbl_otu;

/* Flash timer handle */
static lv_timer_t *s_flash_timer;
static bool        s_flash_state;

void card_deco_update(void);

static void flash_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (g_arex.deco.surf_gf <= 100) {
        lv_timer_del(s_flash_timer);
        s_flash_timer = NULL;
        lv_obj_set_style_text_color(s_lbl_surf_gf, lv_color_make(0x00,0xFF,0x00), 0);
        return;
    }
    s_flash_state = !s_flash_state;
    lv_color_t col = s_flash_state ? lv_color_make(0xFF,0x00,0x00) : lv_color_make(0,0,0);
    lv_obj_set_style_text_color(s_lbl_surf_gf, col, 0);
}

static lv_color_t tissue_color(uint8_t pct)
{
    if (pct > 100) return lv_color_make(0xFF, 0x00, 0x00);
    if (pct > 80)  return lv_color_make(0xFF, 0xAA, 0x00);
    return lv_color_make(0x00, 0xFF, 0x00);
}

/* Helper: make one deco-grid row at the given y position.
   Each row has a left label+value and a right label+value.
   Returns nothing; stores value labels in the provided pointers (may be NULL). */
static void make_grid_row(lv_obj_t *parent, lv_coord_t y,
                           const char *left_cap, const char *left_val, lv_obj_t **left_ref,
                           const char *right_cap, const char *right_val, lv_obj_t **right_ref,
                           bool dashed_bottom)
{
    /* Left cap */
    lv_obj_t *lc = lv_label_create(parent);
    lv_obj_set_style_text_color(lc, lv_color_make(0x55,0xFF,0x55), 0);
    lv_obj_set_style_text_font(lc, AREX_FONT_SMALL, 0);
    lv_label_set_text(lc, left_cap);
    lv_obj_set_pos(lc, GRID_X, y);

    /* Left val */
    lv_obj_t *lv_ = lv_label_create(parent);
    lv_obj_set_style_text_color(lv_, lv_color_make(0x00,0xFF,0x00), 0);
    lv_obj_set_style_text_font(lv_, AREX_FONT_TITLE, 0);
    lv_label_set_text(lv_, left_val);
    lv_obj_set_pos(lv_, GRID_X, y + 16);
    if (left_ref) *left_ref = lv_;

    /* Right cap */
    lv_obj_t *rc = lv_label_create(parent);
    lv_obj_set_style_text_color(rc, lv_color_make(0x55,0xFF,0x55), 0);
    lv_obj_set_style_text_font(rc, AREX_FONT_SMALL, 0);
    lv_label_set_text(rc, right_cap);
    lv_obj_set_pos(rc, 240, y);

    /* Right val */
    lv_obj_t *rv = lv_label_create(parent);
    lv_obj_set_style_text_color(rv, lv_color_make(0x00,0xFF,0x00), 0);
    lv_obj_set_style_text_font(rv, AREX_FONT_TITLE, 0);
    lv_label_set_text(rv, right_val);
    lv_obj_set_pos(rv, 240, y + 16);
    if (right_ref) *right_ref = rv;

    if (dashed_bottom) {
        lv_obj_t *line = lv_obj_create(parent);
        lv_obj_set_size(line, 428, 1);
        lv_obj_set_pos(line, GRID_X, y + 40);
        lv_obj_set_style_bg_color(line, lv_color_make(0x00,0x33,0x00), 0);
        lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(line, 0, 0);
        lv_obj_set_style_pad_all(line, 0, 0);
        lv_obj_set_style_radius(line, 0, 0);
    }
}

void card_deco_create(lv_obj_t *parent)
{
    arex_screen_make_card_title(parent, "2F: TISSUES & DECO");

    /* Row 1: ALGORITHM + GF LOW/HIGH  (y=50) */
    make_grid_row(parent, 50,
                  "ALGORITHM", "ZHL-16C", NULL,
                  "GF LOW/HIGH", "30 / 70", NULL,
                  true);

    /* Row 2: GF99 + SURF GF  (y=97) */
    make_grid_row(parent, 97,
                  "GF99", "--", &s_lbl_gf99,
                  "SURF GF", "--", &s_lbl_surf_gf,
                  true);

    /* Row 3: CNS O2 + OTU  (y=144) */
    make_grid_row(parent, 144,
                  "CNS O2", "--%", &s_lbl_cns,
                  "OTU", "--", &s_lbl_otu,
                  false);

    /* "TISSUE SATURATION" section label (y=200) */
    lv_obj_t *sec_lbl = lv_label_create(parent);
    lv_obj_set_style_text_color(sec_lbl, lv_color_make(0x55,0xFF,0x55), 0);
    lv_obj_set_style_text_font(sec_lbl, AREX_FONT_SMALL, 0);
    lv_label_set_text(sec_lbl, "TISSUE SATURATION (16 COMPARTMENTS)");
    lv_obj_set_pos(sec_lbl, GRID_X, 200);

    /* 16 tissue bars (y=218) */
    for (int i = 0; i < 16; i++) {
        lv_obj_t *bar = lv_bar_create(parent);
        lv_bar_set_range(bar, 0, 110);
        lv_obj_set_size(bar, BAR_W, BAR_H);
        lv_obj_set_pos(bar, GRID_X + i * (BAR_W + BAR_GAP), BARS_Y);
        lv_bar_set_start_value(bar, 0, LV_ANIM_OFF);
        lv_bar_set_value(bar, 0, LV_ANIM_OFF);

        lv_obj_set_style_bg_color(bar, lv_color_make(0x00,0x22,0x00), 0);
        lv_obj_set_style_bg_color(bar, lv_color_make(0x00,0xFF,0x00), LV_PART_INDICATOR);
        lv_obj_set_style_radius(bar, 0, 0);
        lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR);

        s_bars[i] = bar;

        /* Compartment number label below bar */
        lv_obj_t *lbl = lv_label_create(parent);
        lv_obj_set_style_text_color(lbl, lv_color_make(0x55,0xFF,0x55), 0);
        lv_obj_set_style_text_font(lbl, AREX_FONT_SMALL, 0);
        char buf[4];
        snprintf(buf, sizeof(buf), "%d", i + 1);
        lv_label_set_text(lbl, buf);
        lv_obj_set_pos(lbl, GRID_X + i * (BAR_W + BAR_GAP), BARS_Y + BAR_H + 4);
    }

    /* M-value line at 80% height from bottom (top 20%) */
    lv_obj_t *mline = lv_obj_create(parent);
    lv_obj_set_size(mline, 16 * (BAR_W + BAR_GAP) - BAR_GAP, 2);
    lv_obj_set_pos(mline, GRID_X, BARS_Y + (int)(BAR_H * 0.2f));
    lv_obj_set_style_bg_color(mline, lv_color_make(0x00,0xFF,0x00), 0);
    lv_obj_set_style_bg_opa(mline, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(mline, 0, 0);
    lv_obj_set_style_pad_all(mline, 0, 0);
    lv_obj_set_style_radius(mline, 0, 0);

    card_deco_update();
}

void card_deco_update(void)
{
    char buf[16];

    snprintf(buf, sizeof(buf), "%d%%", g_arex.deco.gf99);
    lv_label_set_text(s_lbl_gf99, buf);

    snprintf(buf, sizeof(buf), "%d%%", g_arex.deco.surf_gf);
    lv_label_set_text(s_lbl_surf_gf, buf);

    snprintf(buf, sizeof(buf), "%d%%", g_arex.deco.cns_pct);
    lv_label_set_text(s_lbl_cns, buf);

    snprintf(buf, sizeof(buf), "%d", g_arex.deco.otu);
    lv_label_set_text(s_lbl_otu, buf);

    /* Flash timer for surf_gf > 100 */
    if (g_arex.deco.surf_gf > 100 && !s_flash_timer) {
        s_flash_timer = lv_timer_create(flash_timer_cb, 500, NULL);
    }

    for (int i = 0; i < 16; i++) {
        uint8_t pct = g_arex.deco.tissue_pct[i];
        lv_bar_set_value(s_bars[i], pct, LV_ANIM_ON);
        lv_obj_set_style_bg_color(s_bars[i], tissue_color(pct), LV_PART_INDICATOR);
    }
}
