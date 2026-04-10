#include "../arex_screen.h"
#include "../arex_data.h"
#include "../arex_ui_state.h"
#include "lvgl/lvgl.h"
#include "../fonts/arex_fonts.h"
#include <stdio.h>
#include <string.h>

void arex_screen_register_info_list(lv_obj_t *list);

static const char *s_info_items[] = {
    "> LAST DIVE",
    "> DIVE PLAN",
    "> TISSUE & TOX",
    "> GAS & CALC",
    "> SENSOR & DEVICE",
};
#define INFO_ITEM_COUNT 5

static lv_obj_t *s_list;

void card_info_create(lv_obj_t *parent)
{
    arex_screen_make_card_title(parent, "> INFO MENU");

    /* List container */
    s_list = lv_obj_create(parent);
    lv_obj_set_size(s_list, 428, INFO_ITEM_COUNT * 48 + (INFO_ITEM_COUNT - 1) * 8);
    lv_obj_set_pos(s_list, 16, 50);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_set_style_pad_row(s_list, 8, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_OFF);

    for (uint8_t i = 0; i < INFO_ITEM_COUNT; i++) {
        lv_obj_t *item = lv_obj_create(s_list);
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
        lv_label_set_text(lbl, s_info_items[i]);
    }

    arex_screen_register_info_list(s_list);
}

void card_info_update(void)
{
    /* INFO sub-menu strings are built dynamically in arex_screen.c
       from g_arex values each time the user opens a sub-menu.
       This update callback is intentionally minimal — the static
       item titles never change. */
}
