#include "../arex_screen.h"
#include "../arex_data.h"
#include "../arex_ui_engine.h"
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

    /* 从配置总线推算物理尺寸 */
    int right_canvas_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                         - (g_sys_config.gap_u * AREX_BASE_U);
    int item_h = 48;   /* h_menu_item=5U，但规范锁死48px */
    int item_w = right_canvas_w - 15;
    int gap_y  = 8;    /* gap_menu=1U，规范锁死8px */

    /* 透明容器：不使用 Flex，改用绝对定位循环 */
    s_list = lv_obj_create(parent);
    lv_obj_set_size(s_list, right_canvas_w,
                    INFO_ITEM_COUNT * item_h + (INFO_ITEM_COUNT - 1) * gap_y);
    lv_obj_set_pos(s_list, 0, 50);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_clear_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);

    int current_y = 0;
    for (uint8_t i = 0; i < INFO_ITEM_COUNT; i++) {
        lv_obj_t *item = lv_obj_create(s_list);
        lv_obj_set_pos(item, 0, current_y);
        lv_obj_set_size(item, item_w, item_h);
        lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(item, AREX_DARK, 0);
        lv_obj_set_style_border_width(item, 2, LV_PART_MAIN);
        lv_obj_set_style_radius(item, 0, 0);
        lv_obj_set_style_pad_all(item, 0, LV_PART_MAIN);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl = lv_label_create(item);
        lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
        lv_obj_set_style_text_font(lbl, arex_get_font(AREX_FONT_ID_TITLE), 0);
        lv_obj_set_size(lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 12, 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_label_set_text(lbl, s_info_items[i]);

        current_y += item_h + gap_y;
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
