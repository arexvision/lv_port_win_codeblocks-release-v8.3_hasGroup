#include "arex_screen.h"
#include "arex_data.h"
#include "arex_ui_state.h"
#include "arex_card_registry.h"
#include "fonts/arex_fonts.h"
#include <stdio.h>
#include <string.h>

/* =========================================
   Internal widget handles
   ========================================= */
static lv_obj_t *s_scr;
static lv_obj_t *s_left_panel;
static lv_obj_t *s_right_cont;    /* clip container */
static lv_obj_t *s_tileview;

/* Left panel labels */
static lv_obj_t *s_lbl_depth;
static lv_obj_t *s_lbl_ndl;
static lv_obj_t *s_lbl_tts;
static lv_obj_t *s_lbl_next_stop;
static lv_obj_t *s_lbl_pod1;
static lv_obj_t *s_lbl_pod2;
static lv_obj_t *s_lbl_gas_name;
static lv_obj_t *s_lbl_ppo2;
static lv_obj_t *s_lbl_time;

/* Wall indicators */
static lv_obj_t *s_wall_top;
static lv_obj_t *s_wall_bottom;
static lv_obj_t *s_wall_text_top,    *s_wall_blocks_top;
static lv_obj_t *s_wall_text_bottom, *s_wall_blocks_bottom;

/* Scroll dots */
static lv_obj_t *s_scroll_dots[6];

/* Modal overlay */
static lv_obj_t *s_modal;
static lv_obj_t *s_modal_box;

/* Sub-menu layer */
static lv_obj_t *s_submenu_layer;
static lv_obj_t *s_submenu_title;
static lv_obj_t *s_submenu_list;

/* INFO / SETUP list objects (inside their tiles) */
static lv_obj_t *s_info_list;
static lv_obj_t *s_setup_list;

/* =========================================
   Styles (static, init once)
   ========================================= */
static lv_style_t s_style_screen;
static lv_style_t s_style_panel;
static lv_style_t s_style_label_huge;
static lv_style_t s_style_label_med;
static lv_style_t s_style_label_small;
static lv_style_t s_style_menu_item;
static lv_style_t s_style_menu_item_active;
static bool       s_styles_inited = false;

static void styles_init(void)
{
    if (s_styles_inited) return;
    s_styles_inited = true;

    lv_style_init(&s_style_screen);
    lv_style_set_bg_color(&s_style_screen, AREX_BG);
    lv_style_set_bg_opa(&s_style_screen, LV_OPA_COVER);
    lv_style_set_pad_all(&s_style_screen, 0);
    lv_style_set_border_width(&s_style_screen, 0);

    lv_style_init(&s_style_panel);
    lv_style_set_bg_color(&s_style_panel, AREX_BLACK);
    lv_style_set_bg_opa(&s_style_panel, LV_OPA_COVER);
    lv_style_set_pad_all(&s_style_panel, 10);
    lv_style_set_border_color(&s_style_panel, AREX_DARK);
    lv_style_set_border_width(&s_style_panel, 2);
    lv_style_set_border_side(&s_style_panel, LV_BORDER_SIDE_RIGHT);

    lv_style_init(&s_style_label_huge);
    lv_style_set_text_color(&s_style_label_huge, AREX_GREEN);
    lv_style_set_text_font(&s_style_label_huge, AREX_FONT_HUGE);

    lv_style_init(&s_style_label_med);
    lv_style_set_text_color(&s_style_label_med, AREX_GREEN);
    lv_style_set_text_font(&s_style_label_med, AREX_FONT_MEDIUM);

    lv_style_init(&s_style_label_small);
    lv_style_set_text_color(&s_style_label_small, AREX_LIGHT);
    lv_style_set_text_font(&s_style_label_small, AREX_FONT_SMALL);

    lv_style_init(&s_style_menu_item);
    lv_style_set_bg_color(&s_style_menu_item, AREX_BLACK);
    lv_style_set_bg_opa(&s_style_menu_item, LV_OPA_COVER);
    lv_style_set_text_color(&s_style_menu_item, AREX_GREEN);
    lv_style_set_border_color(&s_style_menu_item, AREX_DARK);
    lv_style_set_border_width(&s_style_menu_item, 2);
    lv_style_set_pad_all(&s_style_menu_item, 12);
    lv_style_set_radius(&s_style_menu_item, 0);

    lv_style_init(&s_style_menu_item_active);
    lv_style_set_bg_color(&s_style_menu_item_active, AREX_GREEN);
    lv_style_set_bg_opa(&s_style_menu_item_active, LV_OPA_COVER);
    lv_style_set_text_color(&s_style_menu_item_active, AREX_BLACK);
    lv_style_set_border_color(&s_style_menu_item_active, AREX_GREEN);
}

/* =========================================
   Helper: make a label
   ========================================= */
static lv_obj_t *make_label(lv_obj_t *parent, lv_style_t *style, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_add_style(lbl, style, 0);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, 0);
    return lbl;
}

/* =========================================
   Left panel
   ========================================= */
static void left_panel_create(void)
{
    s_left_panel = lv_obj_create(s_scr);
    lv_obj_set_size(s_left_panel, AREX_LEFT_W, AREX_SCREEN_H);
    lv_obj_set_pos(s_left_panel, 0, 0);
    lv_obj_add_style(s_left_panel, &s_style_panel, 0);
    lv_obj_set_scrollbar_mode(s_left_panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(s_left_panel, LV_OBJ_FLAG_SCROLLABLE);

    /* DEPTH label */
    lv_obj_t *lbl_depth_caption = make_label(s_left_panel, &s_style_label_small, "DEPTH");
    lv_obj_set_pos(lbl_depth_caption, 0, 0);

    s_lbl_depth = make_label(s_left_panel, &s_style_label_huge, "45.2");
    lv_obj_set_pos(s_lbl_depth, 0, 16);

    /* NDL / TTS row */
    lv_obj_t *lbl_ndl_cap = make_label(s_left_panel, &s_style_label_small, "NDL");
    lv_obj_set_pos(lbl_ndl_cap, 0, 80);
    s_lbl_ndl = make_label(s_left_panel, &s_style_label_med, "0");
    lv_obj_set_pos(s_lbl_ndl, 0, 96);

    lv_obj_t *lbl_tts_cap = make_label(s_left_panel, &s_style_label_small, "TTS");
    lv_obj_set_pos(lbl_tts_cap, 90, 80);
    s_lbl_tts = make_label(s_left_panel, &s_style_label_med, "24'");
    lv_obj_set_style_bg_color(s_lbl_tts, AREX_GREEN, 0);
    lv_obj_set_style_bg_opa(s_lbl_tts, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(s_lbl_tts, AREX_BLACK, 0);
    lv_obj_set_style_pad_ver(s_lbl_tts, 0, 0);
    lv_obj_set_style_pad_hor(s_lbl_tts, 4, 0);
    lv_obj_set_pos(s_lbl_tts, 90, 96);

    /* NEXT STOP */
    lv_obj_t *lbl_ns_cap = make_label(s_left_panel, &s_style_label_small, "NEXT STOP");
    lv_obj_set_pos(lbl_ns_cap, 0, 132);
    s_lbl_next_stop = make_label(s_left_panel, &s_style_label_med, "21m 3'");
    lv_obj_set_style_text_color(s_lbl_next_stop, AREX_LIGHT, 0);
    lv_obj_set_pos(s_lbl_next_stop, 0, 148);

    /* POD 1 / POD 2 — HTML: font calc(28*0.75)=21px, space-between layout
       Panel inner width = 180 - 2*10(pad) = 160px.
       POD1 left-aligned, POD2 right-aligned (align right edge at x=158) */
    lv_obj_t *lbl_p1_cap = make_label(s_left_panel, &s_style_label_small, "POD 1");
    lv_obj_set_pos(lbl_p1_cap, 0, 192);
    s_lbl_pod1 = make_label(s_left_panel, &s_style_label_med, "210");
    lv_obj_set_style_text_font(s_lbl_pod1, AREX_FONT_TITLE, 0);
    lv_obj_set_pos(s_lbl_pod1, 0, 208);

    lv_obj_t *lbl_p2_cap = make_label(s_left_panel, &s_style_label_small, "POD 2");
    lv_obj_set_pos(lbl_p2_cap, 84, 192);
    s_lbl_pod2 = make_label(s_left_panel, &s_style_label_med, "195");
    lv_obj_set_style_text_font(s_lbl_pod2, AREX_FONT_TITLE, 0);
    lv_obj_set_style_text_color(s_lbl_pod2, AREX_LIGHT, 0);
    lv_obj_set_pos(s_lbl_pod2, 84, 208);

    /* GAS */
    lv_obj_t *lbl_gas_cap = make_label(s_left_panel, &s_style_label_small, "GAS");
    lv_obj_set_pos(lbl_gas_cap, 0, 252);
    s_lbl_gas_name = make_label(s_left_panel, &s_style_label_med, "TX 18/45");
    lv_obj_set_pos(s_lbl_gas_name, 0, 268);

    /* PO2 */
    lv_obj_t *lbl_po2_cap = make_label(s_left_panel, &s_style_label_small, "PO2");
    lv_obj_set_pos(lbl_po2_cap, 0, 304);
    s_lbl_ppo2 = make_label(s_left_panel, &s_style_label_small, "1.2 | 1.2 | 1.3");
    lv_obj_set_pos(s_lbl_ppo2, 0, 320);

    /* TIME — HTML: margin-top:auto, sticks to bottom of panel */
    lv_obj_t *lbl_time_cap = make_label(s_left_panel, &s_style_label_small, "TIME");
    lv_obj_set_pos(lbl_time_cap, 0, 430);
    s_lbl_time = make_label(s_left_panel, &s_style_label_med, "38:14");
    lv_obj_set_pos(s_lbl_time, 0, 446);
}

/* =========================================
   Right panel: clip container + tileview
   ========================================= */
static void right_panel_create(void)
{
    s_right_cont = lv_obj_create(s_scr);
    lv_obj_set_size(s_right_cont, AREX_RIGHT_W, AREX_SCREEN_H);
    lv_obj_set_pos(s_right_cont, AREX_LEFT_W, 0);
    lv_obj_set_style_bg_color(s_right_cont, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(s_right_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_right_cont, 0, 0);
    lv_obj_set_style_border_width(s_right_cont, 0, 0);
    lv_obj_clear_flag(s_right_cont, LV_OBJ_FLAG_SCROLLABLE);

    /* Tileview — vertical, one tile per card */
    s_tileview = lv_tileview_create(s_right_cont);
    lv_obj_set_size(s_tileview, AREX_RIGHT_W, AREX_SCREEN_H);
    lv_obj_set_pos(s_tileview, 0, 0);
    lv_obj_set_style_bg_color(s_tileview, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(s_tileview, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_tileview, 0, 0);
    lv_obj_set_scrollbar_mode(s_tileview, LV_SCROLLBAR_MODE_OFF);
    /* Disable touch/mouse scrolling — input goes through arex_input only */
    lv_obj_clear_flag(s_tileview, LV_OBJ_FLAG_SCROLLABLE);

    /* Create tiles in card_order */
    uint8_t count = arex_card_count();
    for (uint8_t i = 0; i < count; i++) {
        arex_card_reg_t *card = arex_card_get(i);
        if (!card) continue;

        lv_obj_t *tile = lv_tileview_add_tile(s_tileview, 0, i, LV_DIR_TOP | LV_DIR_BOTTOM);
        lv_obj_set_style_bg_color(tile, AREX_BLACK, 0);
        lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(tile, 0, 0);
        card->tile_obj = tile;

        if (card->create_cb) card->create_cb(tile);
    }

    /* Scroll dots */
    lv_obj_t *dot_cont = lv_obj_create(s_right_cont);
    lv_obj_set_size(dot_cont, 10, count * 14);
    lv_obj_align(dot_cont, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_set_style_bg_opa(dot_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dot_cont, 0, 0);
    lv_obj_set_style_pad_all(dot_cont, 0, 0);
    lv_obj_set_flex_flow(dot_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(dot_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (uint8_t i = 0; i < count && i < 6; i++) {
        s_scroll_dots[i] = lv_obj_create(dot_cont);
        lv_obj_set_size(s_scroll_dots[i], 6, 6);
        lv_obj_set_style_radius(s_scroll_dots[i], 0, 0);
        lv_obj_set_style_bg_color(s_scroll_dots[i], AREX_DARK, 0);
        lv_obj_set_style_bg_opa(s_scroll_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_scroll_dots[i], 0, 0);
        lv_obj_set_style_pad_all(s_scroll_dots[i], 0, 0);
    }
    arex_screen_update_scroll_dots(0, true);
}

/* Wall indicator: each wall has two child labels —
     child 0 = text line ("<<< RETURN TO DASH <<<")
     child 1 = charge blocks ("[ ■ ]   [ ■ ]   [   ]") */

static lv_obj_t *make_wall(lv_obj_t *parent, lv_coord_t y)
{
    lv_obj_t *w = lv_obj_create(parent);
    lv_obj_set_size(w, AREX_RIGHT_W, 90);
    lv_obj_set_pos(w, 0, y);
    lv_obj_set_style_bg_color(w, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(w, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(w, AREX_DARK, 0);
    lv_obj_set_style_border_width(w, 2, 0);
    lv_obj_set_style_border_side(w, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_all(w, 0, 0);
    lv_obj_add_flag(w, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(w, LV_OBJ_FLAG_SCROLLABLE);

    /* Line 1: text */
    lv_obj_t *txt = lv_label_create(w);
    lv_obj_set_style_text_color(txt, AREX_GREEN, 0);
    lv_obj_set_style_text_font(txt, AREX_FONT_TITLE, 0);
    lv_obj_set_width(txt, AREX_RIGHT_W);
    lv_obj_set_style_text_align(txt, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(txt, 0, 12);
    lv_label_set_text(txt, "");

    /* Line 2: charge blocks — larger font, centered */
    lv_obj_t *blk = lv_label_create(w);
    lv_obj_set_style_text_color(blk, AREX_GREEN, 0);
    lv_obj_set_style_text_font(blk, AREX_FONT_MEDIUM, 0);
    lv_obj_set_width(blk, AREX_RIGHT_W);
    lv_obj_set_style_text_align(blk, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(blk, 0, 50);
    lv_label_set_text(blk, "");

    return w;
}

static void wall_create(void)
{
    s_wall_top    = make_wall(s_right_cont, 0);
    s_wall_bottom = make_wall(s_right_cont, AREX_SCREEN_H - 90);
    s_wall_text_top      = lv_obj_get_child(s_wall_top, 0);
    s_wall_blocks_top    = lv_obj_get_child(s_wall_top, 1);
    s_wall_text_bottom   = lv_obj_get_child(s_wall_bottom, 0);
    s_wall_blocks_bottom = lv_obj_get_child(s_wall_bottom, 1);
}

/* =========================================
   Modal overlay
   ========================================= */
static void modal_create(void)
{
    s_modal = lv_obj_create(s_right_cont);
    lv_obj_set_size(s_modal, AREX_RIGHT_W, AREX_SCREEN_H);
    lv_obj_set_pos(s_modal, 0, 0);
    lv_obj_set_style_bg_color(s_modal, lv_color_make(0,0,0), 0);
    lv_obj_set_style_bg_opa(s_modal, LV_OPA_90, 0);
    lv_obj_set_style_border_width(s_modal, 0, 0);
    lv_obj_add_flag(s_modal, LV_OBJ_FLAG_HIDDEN);

    s_modal_box = lv_obj_create(s_modal);
    lv_obj_set_size(s_modal_box, 380, 260);
    lv_obj_center(s_modal_box);
    lv_obj_set_style_bg_color(s_modal_box, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(s_modal_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_modal_box, AREX_GREEN, 0);
    lv_obj_set_style_border_width(s_modal_box, 3, 0);
    lv_obj_set_style_radius(s_modal_box, 0, 0);
    lv_obj_set_style_pad_all(s_modal_box, 24, 0);
}

/* =========================================
   Sub-menu layer (slides in from right)
   ========================================= */
static void submenu_layer_create(void)
{
    s_submenu_layer = lv_obj_create(s_right_cont);
    lv_obj_set_size(s_submenu_layer, AREX_RIGHT_W, AREX_SCREEN_H);
    lv_obj_set_pos(s_submenu_layer, AREX_RIGHT_W, 0); /* hidden off-screen right */
    lv_obj_set_style_bg_color(s_submenu_layer, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(s_submenu_layer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_submenu_layer, 0, 0);
    lv_obj_set_style_pad_all(s_submenu_layer, 0, 0);
    lv_obj_clear_flag(s_submenu_layer, LV_OBJ_FLAG_SCROLLABLE);

    /* Title uses same helper as card tiles for visual consistency */
    s_submenu_title = lv_label_create(s_submenu_layer);
    lv_obj_set_style_text_color(s_submenu_title, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(s_submenu_title, AREX_FONT_TITLE, 0);
    lv_obj_set_pos(s_submenu_title, 16, 12);
    lv_label_set_text(s_submenu_title, "> SUB MENU");

    /* Title border line, identical to arex_screen_make_card_title */
    lv_obj_t *title_line = lv_obj_create(s_submenu_layer);
    lv_obj_set_size(title_line, AREX_RIGHT_W - 32, 2);
    lv_obj_set_pos(title_line, 16, 38);
    lv_obj_set_style_bg_color(title_line, AREX_DARK, 0);
    lv_obj_set_style_bg_opa(title_line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(title_line, 0, 0);
    lv_obj_set_style_pad_all(title_line, 0, 0);
    lv_obj_set_style_radius(title_line, 0, 0);

    /* List container — identical geometry to card_info/card_setup lists */
    s_submenu_list = lv_obj_create(s_submenu_layer);
    lv_obj_set_size(s_submenu_list, 428, AREX_SCREEN_H - 50);
    lv_obj_set_pos(s_submenu_list, 16, 50);
    lv_obj_set_style_bg_opa(s_submenu_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_submenu_list, 0, 0);
    lv_obj_set_style_pad_all(s_submenu_list, 0, 0);
    lv_obj_set_style_pad_row(s_submenu_list, 8, 0);
    lv_obj_set_flex_flow(s_submenu_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_submenu_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollbar_mode(s_submenu_list, LV_SCROLLBAR_MODE_OFF);
}

/* =========================================
   arex_screen_create — public entry
   ========================================= */
void arex_screen_create(void)
{
    styles_init();

    s_scr = lv_obj_create(NULL);
    lv_obj_add_style(s_scr, &s_style_screen, 0);

    left_panel_create();
    right_panel_create();
    wall_create();
    submenu_layer_create();
    modal_create();          /* modal last → highest z-order, always on top */

    lv_scr_load(s_scr);
}

/* =========================================
   Tileview navigation
   ========================================= */
void arex_screen_scroll_to_card(uint8_t idx)
{
    arex_card_reg_t *card = arex_card_get(idx);
    if (!card || !card->tile_obj) return;
    lv_obj_set_tile(s_tileview, card->tile_obj, LV_ANIM_ON);
    arex_screen_update_scroll_dots(idx, true);
}

/* =========================================
   Left panel refresh
   ========================================= */
void arex_screen_refresh_left_panel(void)
{
    char buf[32];

    snprintf(buf, sizeof(buf), "%.1f", g_arex.dive.depth);
    lv_label_set_text(s_lbl_depth, buf);

    snprintf(buf, sizeof(buf), "%d", g_arex.dive.ndl);
    lv_label_set_text(s_lbl_ndl, buf);

    snprintf(buf, sizeof(buf), "%d'", g_arex.dive.tts);
    lv_label_set_text(s_lbl_tts, buf);

    snprintf(buf, sizeof(buf), "%dm %d'", g_arex.dive.next_stop_m, g_arex.dive.next_stop_min);
    lv_label_set_text(s_lbl_next_stop, buf);

    snprintf(buf, sizeof(buf), "%d", g_arex.dive.pod1_bar);
    lv_label_set_text(s_lbl_pod1, buf);

    snprintf(buf, sizeof(buf), "%d", g_arex.dive.pod2_bar);
    lv_label_set_text(s_lbl_pod2, buf);

    lv_label_set_text(s_lbl_gas_name, AREX_GAS_TABLE[g_arex.gas.active_idx].name);

    snprintf(buf, sizeof(buf), "%.1f|%.1f|%.1f",
             g_arex.gas.ppo2[0], g_arex.gas.ppo2[1], g_arex.gas.ppo2[2]);
    lv_label_set_text(s_lbl_ppo2, buf);

    uint32_t s = g_arex.dive.dive_time_s;
    snprintf(buf, sizeof(buf), "%02d:%02d", s / 60, s % 60);
    lv_label_set_text(s_lbl_time, buf);
}

/* =========================================
   Wall indicators
   ========================================= */
/* Charge block strings — matches HTML .charge-blocks with [ ■ ] slots.
   U+25A0 (■) is included in lv_font_courier_28 glyph range. */
static const char *charge_blocks[] = {
    "[   ]   [   ]   [   ]",
    "[ \xE2\x96\xA0 ]   [   ]   [   ]",
    "[ \xE2\x96\xA0 ]   [ \xE2\x96\xA0 ]   [   ]",
    "[ \xE2\x96\xA0 ]   [ \xE2\x96\xA0 ]   [ \xE2\x96\xA0 ]",
};

/* Slide tileview to offset_y with ease-out, hold there until wall clears.
   Mirrors HTML: elevator-track transition 0.35s cubic-bezier(0.2,0.8,0.2,1)
   with updateElevator(wallCharge * 20) — no spring-back, just smooth push. */
static void wall_nudge_tileview(lv_coord_t offset_y)
{
    lv_anim_del(s_tileview, (lv_anim_exec_xcb_t)lv_obj_set_y);

    lv_coord_t cur_y = lv_obj_get_y(s_tileview);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_tileview);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&a, cur_y, offset_y);
    lv_anim_set_time(&a, 350);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

void arex_screen_show_wall(wall_side_t side, uint8_t charge, const char *text)
{
    if (charge > 3) charge = 3;

    lv_obj_t *wall    = (side == WALL_TOP) ? s_wall_top    : s_wall_bottom;
    lv_obj_t *txt_lbl = (side == WALL_TOP) ? s_wall_text_top    : s_wall_text_bottom;
    lv_obj_t *blk_lbl = (side == WALL_TOP) ? s_wall_blocks_top  : s_wall_blocks_bottom;

    lv_label_set_text(txt_lbl, text);
    lv_label_set_text(blk_lbl, charge_blocks[charge]);
    lv_obj_clear_flag(wall, LV_OBJ_FLAG_HIDDEN);

    /* Rubber-band nudge: top-wall pushes down, bottom-wall pushes up */
    lv_coord_t nudge = (lv_coord_t)(charge * 20);
    wall_nudge_tileview(side == WALL_TOP ? nudge : -nudge);
}

void arex_screen_hide_walls(void)
{
    lv_obj_add_flag(s_wall_top,    LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_wall_bottom, LV_OBJ_FLAG_HIDDEN);
    /* Smooth return to y=0 — same 350ms ease-out as the nudge,
       mirrors HTML: updateElevator() after wallCharge reset */
    lv_coord_t cur_y = lv_obj_get_y(s_tileview);
    if (cur_y == 0) return;
    lv_anim_del(s_tileview, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_tileview);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&a, cur_y, 0);
    lv_anim_set_time(&a, 350);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

void arex_screen_hide_walls_snap(void)
{
    lv_obj_add_flag(s_wall_top,    LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_wall_bottom, LV_OBJ_FLAG_HIDDEN);
    /* Instant snap — called when threshold crossed and view is changing */
    lv_anim_del(s_tileview, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_obj_set_y(s_tileview, 0);
}

/* =========================================
   Scroll dots
   ========================================= */
void arex_screen_update_scroll_dots(uint8_t active_idx, bool visible)
{
    uint8_t count = arex_card_count();
    for (uint8_t i = 0; i < count && i < 6; i++) {
        if (!s_scroll_dots[i]) continue;
        if (!visible) { lv_obj_add_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_clear_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
        lv_color_t col = (i == active_idx) ? AREX_GREEN : AREX_DARK;
        lv_obj_set_style_bg_color(s_scroll_dots[i], col, 0);
    }
}

/* =========================================
   Info / Setup list selection
   ========================================= */
void arex_screen_set_info_selection(uint8_t idx)
{
    if (!s_info_list) return;
    uint32_t cnt = lv_obj_get_child_cnt(s_info_list);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *item = lv_obj_get_child(s_info_list, i);
        lv_obj_t *lbl  = lv_obj_get_child(item, 0);
        if (i == idx) {
            lv_obj_set_style_bg_color(item, AREX_GREEN, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, AREX_BLACK, 0);
        } else {
            lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
        }
    }
}

uint8_t arex_screen_info_item_count(void)
{
    if (!s_info_list) return 0;
    return (uint8_t)lv_obj_get_child_cnt(s_info_list);
}

void arex_screen_set_setup_selection(uint8_t idx)
{
    if (!s_setup_list) return;
    uint32_t cnt = lv_obj_get_child_cnt(s_setup_list);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *item = lv_obj_get_child(s_setup_list, i);
        lv_obj_t *lbl  = lv_obj_get_child(item, 0);
        if (i == idx) {
            lv_obj_set_style_bg_color(item, AREX_GREEN, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, AREX_BLACK, 0);
        } else {
            lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
        }
    }
}

uint8_t arex_screen_setup_item_count(void)
{
    if (!s_setup_list) return 0;
    return (uint8_t)lv_obj_get_child_cnt(s_setup_list);
}

/* Called by card_info.c and card_setup.c to register their list objects */
void arex_screen_register_info_list(lv_obj_t *list)  { s_info_list  = list; }
void arex_screen_register_setup_list(lv_obj_t *list) { s_setup_list = list; }

/* =========================================
   Sub-menu layer
   ========================================= */
static void submenu_slide_in(void)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_submenu_layer);
    lv_anim_set_values(&a, AREX_RIGHT_W, 0);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_time(&a, 250);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void submenu_slide_out(void)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_submenu_layer);
    lv_anim_set_values(&a, 0, AREX_RIGHT_W);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_time(&a, 250);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_start(&a);
}

static void submenu_populate(const char *title, const char **items, uint8_t count)
{
    lv_label_set_text(s_submenu_title, title);
    lv_obj_clean(s_submenu_list);

    for (uint8_t i = 0; i < count; i++) {
        lv_obj_t *item = lv_obj_create(s_submenu_list);
        lv_obj_set_size(item, LV_PCT(100), 48);
        lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(item, AREX_DARK, 0);
        lv_obj_set_style_border_width(item, 2, 0);
        lv_obj_set_style_radius(item, 0, 0);
        lv_obj_set_style_pad_ver(item, 12, 0);
        lv_obj_set_style_pad_hor(item, 15, 0);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_clip_corner(item, false, 0);

        lv_obj_t *lbl = lv_label_create(item);
        lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
        lv_obj_set_style_text_font(lbl, AREX_FONT_TITLE, 0);
        lv_label_set_text(lbl, items[i]);
    }
    arex_screen_set_submenu_selection(0);
}

void arex_screen_set_submenu_selection(uint8_t idx)
{
    uint32_t cnt = lv_obj_get_child_cnt(s_submenu_list);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *item = lv_obj_get_child(s_submenu_list, i);
        lv_obj_t *lbl  = lv_obj_get_child(item, 0);
        if (i == idx) {
            lv_obj_set_style_bg_color(item, AREX_GREEN, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, AREX_BLACK, 0);
        } else {
            lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
        }
    }
}

/* INFO sub-menu — dynamically built strings */
static const char *s_info_titles[] = {
    "> LAST DIVE", "> DIVE PLAN", "> TISSUE & TOX", "> GAS & CALC", "> SENSOR & DEVICE"
};

/* String buffers for dynamic INFO sub-menu items */
static char s_info_str[5][5][32]; /* [submenu][item][chars] */
static const char *s_info_dyn[5][6]; /* pointer table, NULL-terminated */

static void build_info_submenu(uint8_t idx)
{
    uint8_t n = 0;
    switch (idx) {
        case 0: /* LAST DIVE */
            snprintf(s_info_str[0][0], 32, "MAX DEPTH: %dm",
                     (int)g_arex.dive.depth);
            snprintf(s_info_str[0][1], 32, "DIVE TIME: %dm",
                     (int)(g_arex.dive.dive_time_s / 60));
            snprintf(s_info_str[0][2], 32, "SURFACE INT: 2h 10m"); /* static demo */
            s_info_dyn[0][n++] = s_info_str[0][0];
            s_info_dyn[0][n++] = s_info_str[0][1];
            s_info_dyn[0][n++] = s_info_str[0][2];
            s_info_dyn[0][n++] = "< BACK";
            break;
        case 1: /* DIVE PLAN */
            s_info_dyn[1][n++] = "VIEW PROFILE";
            s_info_dyn[1][n++] = "RECALCULATE";
            s_info_dyn[1][n++] = "< BACK";
            break;
        case 2: /* TISSUE & TOX */
            snprintf(s_info_str[2][0], 32, "GF: %d/%d",
                     30, 70); /* TODO: from g_arex when GF settings added */
            snprintf(s_info_str[2][1], 32, "CNS: %d%%", g_arex.deco.cns_pct);
            snprintf(s_info_str[2][2], 32, "OTU: %d",   g_arex.deco.otu);
            s_info_dyn[2][n++] = "VIEW BAR GRAPH";
            s_info_dyn[2][n++] = s_info_str[2][0];
            s_info_dyn[2][n++] = s_info_str[2][1];
            s_info_dyn[2][n++] = s_info_str[2][2];
            s_info_dyn[2][n++] = "< BACK";
            break;
        case 3: /* GAS & CALC */
            snprintf(s_info_str[3][0], 32, "GAS 1: %s",
                     AREX_GAS_TABLE[g_arex.gas.active_idx].name);
            snprintf(s_info_str[3][1], 32, "ALGO: ZHL-16C");
            s_info_dyn[3][n++] = s_info_str[3][0];
            s_info_dyn[3][n++] = s_info_str[3][1];
            s_info_dyn[3][n++] = "< BACK";
            break;
        case 4: /* SENSOR & DEVICE */
            snprintf(s_info_str[4][0], 32, "POD 1: %d BAR", g_arex.dive.pod1_bar);
            snprintf(s_info_str[4][1], 32, "POD 2: %d BAR", g_arex.dive.pod2_bar);
            snprintf(s_info_str[4][2], 32, "BATTERY: 85%%"); /* static demo */
            snprintf(s_info_str[4][3], 32, "TEMP: 24C");     /* static demo */
            s_info_dyn[4][n++] = s_info_str[4][0];
            s_info_dyn[4][n++] = s_info_str[4][1];
            s_info_dyn[4][n++] = s_info_str[4][2];
            s_info_dyn[4][n++] = s_info_str[4][3];
            s_info_dyn[4][n++] = "< BACK";
            break;
        default:
            s_info_dyn[idx][n++] = "< BACK";
            break;
    }
    s_info_dyn[idx][n] = NULL;
}

void arex_screen_open_info_submenu(uint8_t item_idx)
{
    if (item_idx >= 5) return;
    build_info_submenu(item_idx);
    uint8_t count = 0;
    while (count < 6 && s_info_dyn[item_idx][count]) count++;
    submenu_populate(s_info_titles[item_idx], s_info_dyn[item_idx], count);
    g_ui.sub_item_count = count;
    g_ui.sub_menu_idx   = 0;
    g_ui.sub_parent     = UI_INFO;
    g_ui.state          = UI_SUB_MENU;
    submenu_slide_in();
}

/* SETUP sub-menu content table — max 7 items per row (last must be NULL) */
static const char *s_setup_sub[][7] = {
    { "SELECT AIR", "SELECT NX 32", "SELECT TX 18/45", "SELECT O2 100%", "< BACK", NULL },
    { "LOW (GF 40/85)", "MED (GF 30/70)", "HIGH (GF 20/65)", "< BACK", NULL },
    { "LOW", "MED", "HIGH", "MAX", "< BACK", NULL },
    { "START CALIBRATION", "< BACK", NULL },
    { "MODE SETUP >", "DIVE SETUP >", "AI SETUP >", "ALERTS SETUP >", "DISPLAY / SYS >", "< BACK", NULL },
};
static const char *s_setup_titles[] = {
    "> GAS SWITCH", "> CONSERVATISM", "> BRIGHTNESS", "> COMPASS CAL", "> SYSTEM SETUP"
};

void arex_screen_open_setup_submenu(uint8_t item_idx)
{
    if (item_idx >= 5) return;
    uint8_t count = 0;
    while (count < 7 && s_setup_sub[item_idx][count]) count++;
    submenu_populate(s_setup_titles[item_idx], s_setup_sub[item_idx], count);
    g_ui.sub_item_count = count;
    g_ui.sub_menu_idx   = 0;
    g_ui.sub_parent     = UI_SETUP;
    g_ui.state          = UI_SUB_MENU;
    submenu_slide_in();
}

/* =========================================
   Nested sub-menu content tables (3rd level)
   ========================================= */
static const char *s_nested_mode_setup[]   = { "AIR", "NITROX", "3 GAS NX", "GAUGE", "< BACK", NULL };
static const char *s_nested_ai_setup[]     = { "PAIR T1", "PAIR T2", "GTR MODE: ON", "< BACK", NULL };
static const char *s_nested_alerts_setup[] = { "DEPTH ALARM: 40m", "TIME ALARM: 60m", "LOW NDL: 5m", "TEST VIBRATION", "< BACK", NULL };
static const char *s_nested_display_sys[]  = { "UNITS: METRIC", "DATE & CLOCK", "LOG RATE: 10s", "BLUETOOTH: OFF", "RESET DEFAULTS", "< BACK", NULL };

/* DIVE SETUP nested items — MOD PO2 string built dynamically */
static char s_modppo2_str[20];
static const char *s_nested_dive_setup[6]; /* filled before use */

static void build_nested_dive_setup(void)
{
    snprintf(s_modppo2_str, sizeof(s_modppo2_str), "MOD PO2: %.1f", g_arex.settings.mod_ppo2);
    s_nested_dive_setup[0] = "SALINITY: FRESH";
    s_nested_dive_setup[1] = s_modppo2_str;
    s_nested_dive_setup[2] = "SAFETY STOP: 3 MIN";
    s_nested_dive_setup[3] = "ALTITUDE: AUTO";
    s_nested_dive_setup[4] = "< BACK";
    s_nested_dive_setup[5] = NULL;
}

/* Helper: get a nested items table and count by title */
static const char **nested_items_for(const char *title, uint8_t *out_count)
{
    const char **tbl = NULL;
    if      (strcmp(title, "MODE SETUP")    == 0) tbl = s_nested_mode_setup;
    else if (strcmp(title, "DIVE SETUP")    == 0) { build_nested_dive_setup(); tbl = s_nested_dive_setup; }
    else if (strcmp(title, "AI SETUP")      == 0) tbl = s_nested_ai_setup;
    else if (strcmp(title, "ALERTS SETUP")  == 0) tbl = s_nested_alerts_setup;
    else if (strcmp(title, "DISPLAY / SYS") == 0) tbl = s_nested_display_sys;

    if (tbl && out_count) {
        *out_count = 0;
        while (*out_count < 8 && tbl[*out_count]) (*out_count)++;
    }
    return tbl;
}

/* Push current sub-menu state onto the history stack */
static void submenu_history_push(void)
{
    if (g_ui.sub_history_depth >= AREX_SUB_HISTORY_MAX) return;
    arex_sub_history_t *h = &g_ui.sub_history[g_ui.sub_history_depth];
    /* Store the current title shown in the submenu layer */
    const char *cur_title = lv_label_get_text(s_submenu_title);
    lv_snprintf(h->title, sizeof(h->title), "%s", cur_title ? cur_title : "");
    h->idx = g_ui.sub_menu_idx;
    g_ui.sub_history_depth++;
}

void arex_screen_open_nested_submenu(const char *title, const char **items, uint8_t count)
{
    submenu_history_push();
    char full_title[40];
    lv_snprintf(full_title, sizeof(full_title), "> %s", title);
    submenu_populate(full_title, items, count);
    g_ui.sub_item_count = count;
    g_ui.sub_menu_idx   = 0;
    /* sub_parent stays the same — nested menus return to SETUP ultimately */
    g_ui.state = UI_SUB_MENU;
    /* No slide animation for nested — already visible, just swap content */
}

void arex_screen_handle_submenu_select(uint8_t item_idx)
{
    if (item_idx >= g_ui.sub_item_count) return;
    lv_obj_t *item = lv_obj_get_child(s_submenu_list, item_idx);
    lv_obj_t *lbl  = item ? lv_obj_get_child(item, 0) : NULL;
    if (!lbl) return;
    const char *text = lv_label_get_text(lbl);
    if (!text) return;

    /* --- BACK button --- */
    if (strcmp(text, "< BACK") == 0) {
        arex_screen_close_submenu();
        return;
    }

    /* Get the current sub-menu title (strip leading "> ") */
    const char *raw_title = lv_label_get_text(s_submenu_title);
    char cur_title[40] = {0};
    if (raw_title) {
        const char *p = raw_title;
        if (p[0] == '>' && p[1] == ' ') p += 2;
        lv_snprintf(cur_title, sizeof(cur_title), "%s", p);
    }

    /* --- Items with ">" suffix → enter nested sub-menu --- */
    if (text[strlen(text) - 1] == '>') {
        char nested_name[40] = {0};
        /* Strip trailing " >" */
        size_t len = strlen(text);
        size_t copy_len = (len >= 2) ? len - 2 : 0;
        if (copy_len >= sizeof(nested_name)) copy_len = sizeof(nested_name) - 1;
        memcpy(nested_name, text, copy_len);
        /* Trim trailing space */
        while (copy_len > 0 && nested_name[copy_len - 1] == ' ') {
            nested_name[--copy_len] = '\0';
        }
        uint8_t ncnt = 0;
        const char **nitems = nested_items_for(nested_name, &ncnt);
        if (nitems && ncnt > 0) {
            arex_screen_open_nested_submenu(nested_name, nitems, ncnt);
        }
        return;
    }

    /* -------------------------------------------------------
       GAS SWITCH: "SELECT XXX" → switch active gas
    ------------------------------------------------------- */
    if (strcmp(cur_title, "GAS SWITCH") == 0) {
        const char *gas_name = text;
        /* Strip "SELECT " prefix */
        if (strncmp(text, "SELECT ", 7) == 0) gas_name = text + 7;
        for (uint8_t i = 0; i < AREX_GAS_COUNT; i++) {
            if (strcmp(AREX_GAS_TABLE[i].name, gas_name) == 0) {
                g_arex.gas.active_idx = i;
                arex_screen_refresh_gas_menu();
                arex_screen_refresh_left_panel();
                arex_screen_close_submenu();
                return;
            }
        }
        return;
    }

    /* -------------------------------------------------------
       CONSERVATISM: LOW / MED / HIGH
    ------------------------------------------------------- */
    if (strcmp(cur_title, "CONSERVATISM") == 0) {
        uint8_t val = 0xFF;
        if      (strncmp(text, "LOW",  3) == 0) val = 0;
        else if (strncmp(text, "MED",  3) == 0) val = 1;
        else if (strncmp(text, "HIGH", 4) == 0) val = 2;
        if (val != 0xFF) {
            g_arex.settings.conservatism = val;
            const char *badge_str = (val == 0) ? "LOW" : (val == 1) ? "MED" : "HIGH";
            arex_screen_update_setup_badge(1 /* CONSERVATISM row */, badge_str);
            arex_screen_close_submenu();
        }
        return;
    }

    /* -------------------------------------------------------
       BRIGHTNESS: LOW / MED / HIGH / MAX
    ------------------------------------------------------- */
    if (strcmp(cur_title, "BRIGHTNESS") == 0) {
        uint8_t val = 0xFF;
        if      (strcmp(text, "LOW")  == 0) val = 0;
        else if (strcmp(text, "MED")  == 0) val = 1;
        else if (strcmp(text, "HIGH") == 0) val = 2;
        else if (strcmp(text, "MAX")  == 0) val = 3;
        if (val != 0xFF) {
            g_arex.settings.brightness = val;
            arex_screen_update_setup_badge(2 /* BRIGHTNESS row */, text);
            arex_screen_close_submenu();
        }
        return;
    }

    /* -------------------------------------------------------
       DIVE SETUP (nested): MOD PO2 inline edit
    ------------------------------------------------------- */
    if (strcmp(cur_title, "DIVE SETUP") == 0) {
        if (strncmp(text, "MOD PO2:", 8) == 0) {
            arex_screen_begin_edit_value(item_idx,
                                         g_arex.settings.mod_ppo2,
                                         1.0f, 1.6f, 0.1f);
            return;
        }
        /* Other DIVE SETUP items → generic action modal */
        arex_screen_show_modal_act(text);
        return;
    }

    /* -------------------------------------------------------
       Everything else → generic action modal (1s auto-close)
    ------------------------------------------------------- */
    arex_screen_show_modal_act(text);
}

void arex_screen_close_submenu(void)
{
    if (g_ui.sub_history_depth > 0) {
        /* Pop history: restore previous sub-menu level */
        g_ui.sub_history_depth--;
        arex_sub_history_t *h = &g_ui.sub_history[g_ui.sub_history_depth];

        /* Determine which items table to restore */
        const char *prev_title = h->title;
        if (prev_title[0] == '>' && prev_title[1] == ' ') prev_title += 2;

        /* Try setup sub-tables first */
        bool found = false;
        for (uint8_t i = 0; i < 5 && !found; i++) {
            const char *setup_title_stripped = s_setup_titles[i];
            if (setup_title_stripped[0] == '>' && setup_title_stripped[1] == ' ')
                setup_title_stripped += 2;
            if (strcmp(prev_title, setup_title_stripped) == 0) {
                uint8_t cnt = 0;
                while (cnt < 6 && s_setup_sub[i][cnt]) cnt++;
                submenu_populate(s_setup_titles[i], s_setup_sub[i], cnt);
                g_ui.sub_item_count = cnt;
                g_ui.sub_menu_idx   = h->idx;
                arex_screen_set_submenu_selection(h->idx);
                found = true;
            }
        }
        /* Try nested tables */
        if (!found) {
            uint8_t ncnt = 0;
            const char **nitems = nested_items_for(prev_title, &ncnt);
            if (nitems && ncnt > 0) {
                char full_title[40];
                lv_snprintf(full_title, sizeof(full_title), "> %s", prev_title);
                submenu_populate(full_title, nitems, ncnt);
                g_ui.sub_item_count = ncnt;
                g_ui.sub_menu_idx   = h->idx;
                arex_screen_set_submenu_selection(h->idx);
                found = true;
            }
        }
        /* Stay in UI_SUB_MENU regardless */
        g_ui.state = UI_SUB_MENU;
        return;
    }
    submenu_slide_out();
    g_ui.state = g_ui.sub_parent;
}

/* =========================================
   Setup badge update
   Updates the right-side badge label (child 1) of a SETUP menu item.
   item_idx is 0-based position in the setup list.
   ========================================= */
void arex_screen_update_setup_badge(uint8_t item_idx, const char *value)
{
    if (!s_setup_list) return;
    lv_obj_t *item = lv_obj_get_child(s_setup_list, item_idx);
    if (!item) return;
    /* child 0 = title label, child 1 = badge label */
    lv_obj_t *badge = lv_obj_get_child(item, 1);
    if (!badge) return;
    lv_label_set_text(badge, value ? value : "");
}

/* Forward declaration — modal_set_content is defined below in the Modal section */
static void modal_set_content(const char *title, const char *body, const char *hint);

/* =========================================
   Modal action (generic, 1-second auto-close)
   ========================================= */
static void modal_act_timer_cb(lv_timer_t *t)
{
    (void)t;
    arex_screen_hide_modal();
    if (g_ui.state == UI_MODAL_ACT) {
        g_ui.state = UI_SUB_MENU;
    }
    lv_timer_del(t);
}

void arex_screen_show_modal_act(const char *action_text)
{
    modal_set_content("ACTION", action_text ? action_text : "",
                      "[ ESC TO BACK ]");
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
    g_ui.state = UI_MODAL_ACT;
    lv_timer_create(modal_act_timer_cb, 1000, NULL);
}

/* =========================================
   Modal
   ========================================= */
static void modal_set_content(const char *title, const char *body, const char *hint)
{
    lv_obj_clean(s_modal_box);

    lv_obj_t *t = lv_label_create(s_modal_box);
    lv_obj_set_style_text_color(t, AREX_GREEN, 0);
    lv_obj_set_style_text_font(t, AREX_FONT_TITLE, 0);
    lv_label_set_text(t, title);
    lv_obj_set_pos(t, 0, 0);

    lv_obj_t *b = lv_label_create(s_modal_box);
    lv_obj_set_style_text_color(b, AREX_GREEN, 0);
    lv_obj_set_style_text_font(b, AREX_FONT_MEDIUM, 0);
    lv_label_set_text(b, body);
    lv_obj_set_pos(b, 0, 40);

    lv_obj_t *h = lv_label_create(s_modal_box);
    lv_obj_set_style_text_color(h, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(h, AREX_FONT_SMALL, 0);
    lv_label_set_text(h, hint);
    lv_obj_set_pos(h, 0, 100);
}

void arex_screen_show_modal_gas(void)
{
    uint8_t ci = g_ui.gas_cursor;
    char body[32];
    snprintf(body, sizeof(body), "%s\nMOD: %dm",
             AREX_GAS_TABLE[ci].name, AREX_GAS_TABLE[ci].mod_m);

    const char *hint = (g_arex.dive.depth > AREX_GAS_TABLE[ci].mod_m)
        ? "[ FATAL: OVER MOD ]"
        : "[ ENTER CONFIRM ]  [ ESC CANCEL ]";

    modal_set_content("CONFIRM GAS", body, hint);
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}

void arex_screen_show_modal_compass(void)
{
    modal_set_content("CLEAR TARGET?", "REMOVE HEADING MARKER?",
                      "[ ENTER CONFIRM ]  [ ESC CANCEL ]");
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}

void arex_screen_pulse_modal(void)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_modal_box);
    lv_anim_set_values(&a, 0, 6);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_time(&a, 80);
    lv_anim_set_playback_time(&a, 80);
    lv_anim_set_repeat_count(&a, 2);
    lv_anim_start(&a);
}

void arex_screen_hide_modal(void)
{
    lv_obj_add_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}

/* =========================================
   Compass target text (inside compass card)
   — forwarded to card_compass.c
   ========================================= */
void arex_screen_refresh_compass_target(void)
{
    arex_card_reg_t *c = arex_card_get_by_id(CARD_ID_COMPASS);
    if (c && c->update_cb) c->update_cb();
}

/* =========================================
   Gas menu (inside gas card)
   ========================================= */
void arex_screen_refresh_gas_menu(void)
{
    arex_card_reg_t *c = arex_card_get_by_id(CARD_ID_GAS);
    if (c && c->update_cb) c->update_cb();
}

/* =========================================
   Inline value edit (forwarded to sub-menu list)

   HTML .menu-item.editing:  border-color = AREX_GREEN, black bg
   HTML .mod-po2-edit:       green bg, black text (the value block)
   HTML .mod-po2-arrows:     AREX_LIGHT color, right side

   Row layout (fixed coords inside item, pad_hor=15):
     x=0  "MOD PO2: "  prefix label (child 0, existing)
     x≈110  green-bg value badge (child 1, new obj)
     x=right-4  "▲▼" arrows (child 2, new label)
   ========================================= */
void arex_screen_refresh_edit_value(void)
{
    if (!g_ui.edit_ctx.active) return;
    lv_obj_t *item = lv_obj_get_child(s_submenu_list, g_ui.edit_ctx.item_index);
    if (!item) return;

    /* child 1 = badge container, its child 0 = value label */
    lv_obj_t *badge = lv_obj_get_child(item, 1);
    if (!badge) return;
    lv_obj_t *val_lbl = lv_obj_get_child(badge, 0);
    if (!val_lbl) return;
    char buf[12];
    snprintf(buf, sizeof(buf), " %.1f ", g_ui.edit_ctx.value);
    lv_label_set_text(val_lbl, buf);
}

void arex_screen_begin_edit_value(uint8_t item_idx, float value,
                                   float min, float max, float step)
{
    g_ui.edit_ctx.value      = value;
    g_ui.edit_ctx.original   = value;
    g_ui.edit_ctx.min        = min;
    g_ui.edit_ctx.max        = max;
    g_ui.edit_ctx.step       = step;
    g_ui.edit_ctx.item_index = item_idx;
    g_ui.edit_ctx.active     = true;
    g_ui.state = UI_EDIT_VALUE;

    lv_obj_t *item = lv_obj_get_child(s_submenu_list, item_idx);
    if (!item) return;

    /* Highlight border to indicate editing state */
    lv_obj_set_style_border_color(item, AREX_GREEN, 0);

    /* child 0: shorten prefix to "MOD PO2:" */
    lv_obj_t *prefix_lbl = lv_obj_get_child(item, 0);
    if (prefix_lbl) {
        lv_label_set_text(prefix_lbl, "MOD PO2:");
        lv_obj_set_pos(prefix_lbl, 0, 0);   /* stays at default left */
    }

    /* child 1: value badge — absolute position inside item content area
       item pad_hor=15, pad_ver=12; inner height = 48-24=24px
       place badge at x=108 (after "MOD PO2: " ~9 chars * ~12px) */
    lv_obj_t *badge = lv_obj_create(item);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(badge, AREX_GREEN, 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_set_style_pad_all(badge, 0, 0);
    lv_obj_set_style_radius(badge, 0, 0);
    lv_obj_set_size(badge, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    /* Use absolute pos relative to item's content area (inside padding) */
    lv_obj_set_pos(badge, 108, 0);

    lv_obj_t *val_lbl = lv_label_create(badge);
    lv_obj_set_style_text_color(val_lbl, AREX_BLACK, 0);
    lv_obj_set_style_text_font(val_lbl, AREX_FONT_TITLE, 0);
    lv_obj_set_style_bg_opa(val_lbl, LV_OPA_TRANSP, 0);
    char buf[12];
    snprintf(buf, sizeof(buf), " %.1f ", value);
    lv_label_set_text(val_lbl, buf);

    /* child 2: arrow label — absolute position, right side
       item inner width = item_width - 2*pad_hor. Place at fixed x offset from right.
       Since item width = LV_PCT(100) ≈ 428px, inner = 428-30 = 398.
       Arrow "▲▼" ~2 chars, place at x = 398 - ~30 = 368 */
    lv_obj_t *arrow_lbl = lv_label_create(item);
    lv_obj_set_style_text_color(arrow_lbl, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(arrow_lbl, AREX_FONT_TITLE, 0);
    lv_obj_set_style_bg_opa(arrow_lbl, LV_OPA_TRANSP, 0);
    lv_label_set_text(arrow_lbl, "▲▼");
    lv_obj_set_pos(arrow_lbl, 360, 0);
}

static void edit_value_cleanup(lv_obj_t *item)
{
    if (!item) return;
    /* Restore border */
    lv_obj_set_style_border_color(item, AREX_DARK, 0);
    /* Delete in reverse: child 2 (arrows), child 1 (badge) */
    uint32_t cnt = lv_obj_get_child_cnt(item);
    if (cnt > 2) lv_obj_del(lv_obj_get_child(item, 2));
    cnt = lv_obj_get_child_cnt(item);
    if (cnt > 1) lv_obj_del(lv_obj_get_child(item, 1));
}

void arex_screen_commit_edit_value(void)
{
    lv_obj_t *item = lv_obj_get_child(s_submenu_list, g_ui.edit_ctx.item_index);
    if (!item) return;

    edit_value_cleanup(item);

    /* Restore prefix label to full text with committed value */
    lv_obj_t *lbl = lv_obj_get_child(item, 0);
    if (lbl) {
        char buf[32];
        snprintf(buf, sizeof(buf), "MOD PO2: %.1f", g_arex.settings.mod_ppo2);
        lv_label_set_text(lbl, buf);
        lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
    }
    arex_screen_set_submenu_selection(g_ui.sub_menu_idx);
}

void arex_screen_cancel_edit_value(void)
{
    arex_screen_commit_edit_value(); /* g_arex already reverted by state machine */
}

/* =========================================
   Card title helper
   Mirrors HTML .card-title:
     font: AREX_FONT_TITLE (20px Courier Bold)
     color: AREX_LIGHT
     border-bottom: 2px solid AREX_DARK
     padding-bottom: 8px  → line placed at y=38
   ========================================= */
lv_obj_t *arex_screen_make_card_title(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_style_text_color(lbl, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(lbl, AREX_FONT_TITLE, 0);
    lv_label_set_text(lbl, text);
    lv_obj_set_pos(lbl, 16, 12);

    /* Bottom border line */
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_set_size(line, AREX_RIGHT_W - 32, 2);
    lv_obj_set_pos(line, 16, 38);
    lv_obj_set_style_bg_color(line, AREX_DARK, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    lv_obj_set_style_radius(line, 0, 0);

    return lbl;
}
