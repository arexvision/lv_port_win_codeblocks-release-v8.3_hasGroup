#include "../arex_screen.h"
#include "../arex_data.h"
#include "../arex_ui_state.h"
#include "lvgl/lvgl.h"
#include "../fonts/arex_fonts.h"

void arex_screen_register_setup_list(lv_obj_t *list);

static const char *s_setup_items[] = {
    "> GAS SWITCH",
    "> CONSERVATISM",
    "> BRIGHTNESS",
    "> COMPASS CAL",
    "> SYSTEM SETUP",
};
#define SETUP_ITEM_COUNT 5

static lv_obj_t *s_list;

void card_setup_create(lv_obj_t *parent)
{
    arex_screen_make_card_title(parent, "> DIVE SETUP");

    s_list = lv_obj_create(parent);
    lv_obj_set_size(s_list, 428, SETUP_ITEM_COUNT * 48 + (SETUP_ITEM_COUNT - 1) * 8);
    lv_obj_set_pos(s_list, 16, 50);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_set_style_pad_row(s_list, 8, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_OFF);

    for (uint8_t i = 0; i < SETUP_ITEM_COUNT; i++) {
        lv_obj_t *item = lv_obj_create(s_list);
        lv_obj_set_size(item, LV_PCT(100), 48);
        lv_obj_set_style_bg_color(item, lv_color_make(0,0,0), 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(item, lv_color_make(0x00,0x33,0x00), 0);
        lv_obj_set_style_border_width(item, 2, 0);
        lv_obj_set_style_radius(item, 0, 0);
        lv_obj_set_style_pad_ver(item, 12, 0);
        lv_obj_set_style_pad_hor(item, 15, 0);

        lv_obj_t *lbl = lv_label_create(item);
        lv_obj_set_style_text_color(lbl, lv_color_make(0x00,0xFF,0x00), 0);
        lv_obj_set_style_text_font(lbl, AREX_FONT_TITLE, 0);
        lv_label_set_text(lbl, s_setup_items[i]);
    }

    arex_screen_register_setup_list(s_list);
}

void card_setup_update(void)
{
    /* Static menu */
}
