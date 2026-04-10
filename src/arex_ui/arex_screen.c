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
    lv_obj_set_pos(s_lbl_tts, 90, 96);

    /* NEXT STOP */
    lv_obj_t *lbl_ns_cap = make_label(s_left_panel, &s_style_label_small, "NEXT STOP");
    lv_obj_set_pos(lbl_ns_cap, 0, 132);
    s_lbl_next_stop = make_label(s_left_panel, &s_style_label_med, "21m 3'");
    lv_obj_set_style_text_color(s_lbl_next_stop, AREX_LIGHT, 0);
    lv_obj_set_pos(s_lbl_next_stop, 0, 148);

    /* POD 1 / POD 2 */
    lv_obj_t *lbl_p1_cap = make_label(s_left_panel, &s_style_label_small, "POD 1");
    lv_obj_set_pos(lbl_p1_cap, 0, 192);
    s_lbl_pod1 = make_label(s_left_panel, &s_style_label_med, "210 BAR");
    lv_obj_set_pos(s_lbl_pod1, 0, 208);

    lv_obj_t *lbl_p2_cap = make_label(s_left_panel, &s_style_label_small, "POD 2");
    lv_obj_set_pos(lbl_p2_cap, 90, 192);
    s_lbl_pod2 = make_label(s_left_panel, &s_style_label_med, "195 BAR");
    lv_obj_set_style_text_color(s_lbl_pod2, AREX_LIGHT, 0);
    lv_obj_set_pos(s_lbl_pod2, 90, 208);

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

    /* TIME */
    lv_obj_t *lbl_time_cap = make_label(s_left_panel, &s_style_label_small, "TIME");
    lv_obj_set_pos(lbl_time_cap, 0, 440);
    s_lbl_time = make_label(s_left_panel, &s_style_label_med, "38:14");
    lv_obj_set_pos(s_lbl_time, 0, 456);
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

/* =========================================
   Wall indicators
   ========================================= */
static void wall_create(void)
{
    /* Top wall */
    s_wall_top = lv_obj_create(s_right_cont);
    lv_obj_set_size(s_wall_top, AREX_RIGHT_W, 60);
    lv_obj_set_pos(s_wall_top, 0, 0);
    lv_obj_set_style_bg_color(s_wall_top, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(s_wall_top, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_wall_top, AREX_DARK, 0);
    lv_obj_set_style_border_width(s_wall_top, 2, 0);
    lv_obj_set_style_text_color(s_wall_top, AREX_GREEN, 0);
    lv_obj_add_flag(s_wall_top, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_pad_all(s_wall_top, 8, 0);

    lv_obj_t *lbl = lv_label_create(s_wall_top);
    lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
    lv_obj_center(lbl);
    lv_label_set_text(lbl, "");
    lv_obj_set_user_data(s_wall_top, lbl);

    /* Bottom wall */
    s_wall_bottom = lv_obj_create(s_right_cont);
    lv_obj_set_size(s_wall_bottom, AREX_RIGHT_W, 60);
    lv_obj_set_pos(s_wall_bottom, 0, AREX_SCREEN_H - 60);
    lv_obj_set_style_bg_color(s_wall_bottom, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(s_wall_bottom, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_wall_bottom, AREX_DARK, 0);
    lv_obj_set_style_border_width(s_wall_bottom, 2, 0);
    lv_obj_set_style_text_color(s_wall_bottom, AREX_GREEN, 0);
    lv_obj_add_flag(s_wall_bottom, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_pad_all(s_wall_bottom, 8, 0);

    lv_obj_t *lbl2 = lv_label_create(s_wall_bottom);
    lv_obj_set_style_text_color(lbl2, AREX_GREEN, 0);
    lv_obj_center(lbl2);
    lv_label_set_text(lbl2, "");
    lv_obj_set_user_data(s_wall_bottom, lbl2);
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
    lv_obj_set_style_pad_all(s_submenu_layer, 20, 0);
    lv_obj_clear_flag(s_submenu_layer, LV_OBJ_FLAG_SCROLLABLE);

    s_submenu_title = lv_label_create(s_submenu_layer);
    lv_obj_set_style_text_color(s_submenu_title, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(s_submenu_title, AREX_FONT_TITLE, 0);
    lv_obj_set_pos(s_submenu_title, 0, 0);
    lv_label_set_text(s_submenu_title, "> SUB MENU");

    s_submenu_list = lv_obj_create(s_submenu_layer);
    lv_obj_set_size(s_submenu_list, AREX_RIGHT_W - 40, AREX_SCREEN_H - 60);
    lv_obj_set_pos(s_submenu_list, 0, 40);
    lv_obj_set_style_bg_opa(s_submenu_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_submenu_list, 0, 0);
    lv_obj_set_style_pad_all(s_submenu_list, 0, 0);
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
    modal_create();
    submenu_layer_create();

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

    snprintf(buf, sizeof(buf), "%d BAR", g_arex.dive.pod1_bar);
    lv_label_set_text(s_lbl_pod1, buf);

    snprintf(buf, sizeof(buf), "%d BAR", g_arex.dive.pod2_bar);
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
static const char *charge_blocks[] = { "", "[#] [ ] [ ]", "[#] [#] [ ]", "[#] [#] [#]" };

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
    char buf[64];
    snprintf(buf, sizeof(buf), "%s\n%s", text, charge_blocks[charge]);

    lv_obj_t *wall = (side == WALL_TOP) ? s_wall_top : s_wall_bottom;
    lv_obj_t *lbl  = (lv_obj_t *)lv_obj_get_user_data(wall);
    lv_label_set_text(lbl, buf);
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
        lv_obj_set_size(item, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(item, AREX_DARK, 0);
        lv_obj_set_style_border_width(item, 2, 0);
        lv_obj_set_style_radius(item, 0, 0);
        lv_obj_set_style_pad_all(item, 10, 0);

        lv_obj_t *lbl = lv_label_create(item);
        lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
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

/* INFO sub-menu content table */
static const char *s_info_sub[][6] = {
    { "MAX DEPTH: 45m", "DIVE TIME: 55m", "SURFACE INT: 2h 10m", "< BACK", NULL },
    { "VIEW PROFILE",   "RECALCULATE",    "< BACK",              NULL },
    { "VIEW BAR GRAPH", "GF: 30/70",      "CNS: 15%",  "OTU: 22", "< BACK", NULL },
    { "GAS 1: TX18/45", "GAS 2: O2 100%", "ALGO: ZHL-16C",       "< BACK", NULL },
    { "POD 1: 210 BAR", "POD 2: 195 BAR", "BATTERY: 85%",  "TEMP: 24C", "< BACK", NULL },
};
static const char *s_info_titles[] = {
    "> LAST DIVE", "> DIVE PLAN", "> TISSUE & TOX", "> GAS & CALC", "> SENSOR & DEVICE"
};

void arex_screen_open_info_submenu(uint8_t item_idx)
{
    if (item_idx >= 5) return;
    uint8_t count = 0;
    while (count < 6 && s_info_sub[item_idx][count]) count++;
    submenu_populate(s_info_titles[item_idx], s_info_sub[item_idx], count);
    g_ui.sub_item_count = count;
    g_ui.sub_menu_idx   = 0;
    g_ui.sub_parent     = UI_INFO;
    g_ui.state          = UI_SUB_MENU;
    submenu_slide_in();
}

/* SETUP sub-menu content table */
static const char *s_setup_sub[][6] = {
    { "SELECT AIR", "SELECT NX 32", "SELECT TX 18/45", "SELECT O2 100%", "< BACK", NULL },
    { "LOW (GF 40/85)", "MED (GF 30/70)", "HIGH (GF 20/65)", "< BACK", NULL },
    { "LOW", "MED", "HIGH", "MAX", "< BACK", NULL },
    { "START CALIBRATION", "< BACK", NULL },
    { "MODE SETUP >", "DIVE SETUP >", "AI SETUP >", "ALERTS SETUP >", "< BACK", NULL },
};
static const char *s_setup_titles[] = {
    "> GAS SWITCH", "> CONSERVATISM", "> BRIGHTNESS", "> COMPASS CAL", "> SYSTEM SETUP"
};

void arex_screen_open_setup_submenu(uint8_t item_idx)
{
    if (item_idx >= 5) return;
    uint8_t count = 0;
    while (count < 6 && s_setup_sub[item_idx][count]) count++;
    submenu_populate(s_setup_titles[item_idx], s_setup_sub[item_idx], count);
    g_ui.sub_item_count = count;
    g_ui.sub_menu_idx   = 0;
    g_ui.sub_parent     = UI_SETUP;
    g_ui.state          = UI_SUB_MENU;
    submenu_slide_in();
}

void arex_screen_handle_submenu_select(uint8_t item_idx)
{
    if (item_idx >= g_ui.sub_item_count) return;
    lv_obj_t *item = lv_obj_get_child(s_submenu_list, item_idx);
    lv_obj_t *lbl  = item ? lv_obj_get_child(item, 0) : NULL;
    if (!lbl) return;
    const char *text = lv_label_get_text(lbl);
    if (!text) return;

    if (strcmp(text, "< BACK") == 0) {
        arex_screen_close_submenu();
        return;
    }
    /* Deeper menus, value edits, and actions handled in arex_ui_state.c
       via the public handle_submenu_select path — just forward the text */
    /* (Extend here as cards are implemented) */
}

void arex_screen_close_submenu(void)
{
    if (g_ui.sub_history_depth > 0) {
        /* TODO: pop history when nested menus added */
    }
    submenu_slide_out();
    g_ui.state = g_ui.sub_parent;
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
   ========================================= */
void arex_screen_refresh_edit_value(void)
{
    if (!g_ui.edit_ctx.active) return;
    lv_obj_t *item = lv_obj_get_child(s_submenu_list, g_ui.edit_ctx.item_index);
    if (!item) return;
    lv_obj_t *lbl = lv_obj_get_child(item, 0);
    if (!lbl) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "MOD PO2: %.1f  ▲▼", g_ui.edit_ctx.value);
    lv_label_set_text(lbl, buf);
    lv_obj_set_style_bg_color(item, AREX_GREEN, 0);
    lv_obj_set_style_text_color(lbl, AREX_BLACK, 0);
}

void arex_screen_commit_edit_value(void)
{
    lv_obj_t *item = lv_obj_get_child(s_submenu_list, g_ui.edit_ctx.item_index);
    if (!item) return;
    lv_obj_t *lbl = lv_obj_get_child(item, 0);
    if (!lbl) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "MOD PO2: %.1f", g_arex.settings.mod_ppo2);
    lv_label_set_text(lbl, buf);
    lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
    lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
    arex_screen_set_submenu_selection(g_ui.sub_menu_idx);
}

void arex_screen_cancel_edit_value(void)
{
    arex_screen_commit_edit_value(); /* restore from g_arex (already reverted by state machine) */
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
