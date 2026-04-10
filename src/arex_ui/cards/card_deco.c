#include "../arex_screen.h"
#include "../arex_data.h"
#include "lvgl/lvgl.h"
#include "../fonts/arex_fonts.h"
#include <stdio.h>

#define BAR_W   20
#define BAR_H   80
#define BAR_GAP 5
#define GRID_X  10
#define GRID_Y  60

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

void card_deco_create(lv_obj_t *parent)
{
    arex_screen_make_card_title(parent, "2F: TISSUES & DECO");

    /* Top row: GF99 / SURF GF / CNS / OTU */
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, 420, 48);
    lv_obj_set_pos(row, 20, 36);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);

    struct { lv_obj_t **ref; const char *cap; } cols[] = {
        { &s_lbl_gf99,    "GF99" },
        { &s_lbl_surf_gf, "SURF GF" },
        { &s_lbl_cns,     "CNS" },
        { &s_lbl_otu,     "OTU" },
    };
    for (int i = 0; i < 4; i++) {
        lv_obj_t *col = lv_obj_create(row);
        lv_obj_set_size(col, 96, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(col, 0, 0);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);

        lv_obj_t *cap = lv_label_create(col);
        lv_obj_set_style_text_color(cap, lv_color_make(0x55,0xFF,0x55), 0);
        lv_obj_set_style_text_font(cap, AREX_FONT_SMALL, 0);
        lv_label_set_text(cap, cols[i].cap);

        lv_obj_t *val = lv_label_create(col);
        lv_obj_set_style_text_color(val, lv_color_make(0x00,0xFF,0x00), 0);
        lv_obj_set_style_text_font(val, AREX_FONT_TITLE, 0);
        lv_label_set_text(val, "--");
        *cols[i].ref = val;
    }

    /* 16 tissue bars */
    for (int i = 0; i < 16; i++) {
        lv_obj_t *bar = lv_bar_create(parent);
        lv_bar_set_range(bar, 0, 110);
        lv_obj_set_size(bar, BAR_W, BAR_H);
        lv_obj_set_pos(bar, GRID_X + i * (BAR_W + BAR_GAP), GRID_Y + 56);
        lv_bar_set_start_value(bar, 0, LV_ANIM_OFF);
        lv_bar_set_value(bar, 0, LV_ANIM_OFF);

        lv_obj_set_style_bg_color(bar, lv_color_make(0x00,0x22,0x00), 0);
        lv_obj_set_style_bg_color(bar, lv_color_make(0x00,0xFF,0x00), LV_PART_INDICATOR);
        lv_obj_set_style_radius(bar, 0, 0);
        lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR);

        s_bars[i] = bar;

        /* compartment label */
        lv_obj_t *lbl = lv_label_create(parent);
        lv_obj_set_style_text_color(lbl, lv_color_make(0x55,0xFF,0x55), 0);
        lv_obj_set_style_text_font(lbl, AREX_FONT_SMALL, 0);
        char buf[4];
        snprintf(buf, sizeof(buf), "%d", i + 1);
        lv_label_set_text(lbl, buf);
        lv_obj_set_pos(lbl, GRID_X + i * (BAR_W + BAR_GAP), GRID_Y + 56 + BAR_H + 4);
    }

    /* M-value line at 100% */
    lv_obj_t *mline = lv_obj_create(parent);
    lv_obj_set_size(mline, 16 * (BAR_W + BAR_GAP) - BAR_GAP, 2);
    lv_obj_set_pos(mline, GRID_X, GRID_Y + 56 + BAR_H * (1 - 100/110.0f));
    lv_obj_set_style_bg_color(mline, lv_color_make(0xFF,0x00,0x00), 0);
    lv_obj_set_style_bg_opa(mline, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(mline, 0, 0);

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
