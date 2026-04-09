#include "../arex_screen.h"
#include "../arex_data.h"
#include "../arex_ui_state.h"
#include "../../lvgl/lvgl.h"

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
    lv_obj_t *title = lv_label_create(parent);
    lv_obj_set_style_text_color(title, lv_color_make(0x00,0xFF,0x00), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_label_set_text(title, "5F  DIVE SETUP");
    lv_obj_set_pos(title, 16, 12);

    s_list = lv_obj_create(parent);
    lv_obj_set_size(s_list, 420, SETUP_ITEM_COUNT * 60);
    lv_obj_align(s_list, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_OFF);

    for (uint8_t i = 0; i < SETUP_ITEM_COUNT; i++) {
        lv_obj_t *item = lv_obj_create(s_list);
        lv_obj_set_size(item, LV_PCT(100), 52);
        lv_obj_set_style_bg_color(item, lv_color_make(0,0,0), 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(item, lv_color_make(0x00,0x33,0x00), 0);
        lv_obj_set_style_border_width(item, 2, 0);
        lv_obj_set_style_radius(item, 0, 0);
        lv_obj_set_style_pad_all(item, 12, 0);

        lv_obj_t *lbl = lv_label_create(item);
        lv_obj_set_style_text_color(lbl, lv_color_make(0x00,0xFF,0x00), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
        lv_label_set_text(lbl, s_setup_items[i]);
    }

    arex_screen_register_setup_list(s_list);
}

void card_setup_update(void)
{
    /* Static menu */
}
