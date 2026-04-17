/*
 * card_info.c  —  0F: INFO MENU
 *
 * 架构规则：
 *   - 严禁 lv_obj_set_flex_flow / lv_obj_set_flex_align
 *   - 列表项位置用 current_y 累加绝对定位
 *   - 只从 g_layout 读取容器尺寸，不硬编码宽高
 *   - create_cb 只建控件；update_cb 只刷文本（info 标题静态，无需刷新）
 */

#include "../arex_ui_engine.h"
#include "../arex_screen.h"
#include "../arex_ui_state.h"
#include "../fonts/arex_fonts.h"
#include "lvgl/lvgl.h"
#include <string.h>

void arex_screen_register_info_list(lv_obj_t *list);

static const char *s_info_items[] = {
    "> LAST DIVE",
    "> DIVE PLAN",
    "> TISSUE & TOX",
    "> GAS & CALC",
    "> SENSOR & DEVICE",
};
#define INFO_ITEM_COUNT  5
#define ITEM_H           48
#define ITEM_GAP          8
#define TITLE_H          44   /* 卡片标题行高 */
#define PAD_X            16

static lv_obj_t *s_list;

void card_info_create(lv_obj_t *parent)
{
    arex_screen_make_card_title(parent, "> INFO MENU");

    int16_t avail_w = g_layout.rc_w - PAD_X * 2;
    int16_t cy = TITLE_H;

    /* 列表容器（透明，仅作裁剪边界）*/
    s_list = lv_obj_create(parent);
    lv_obj_set_pos(s_list,  PAD_X, cy);
    lv_obj_set_size(s_list, avail_w,
                    INFO_ITEM_COUNT * ITEM_H + (INFO_ITEM_COUNT - 1) * ITEM_GAP);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_set_style_radius(s_list, 0, 0);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);

    /* current_y 累加：每条 item 绝对定位 */
    int16_t item_y = 0;
    for (uint8_t i = 0; i < INFO_ITEM_COUNT; i++) {
        lv_obj_t *item = lv_obj_create(s_list);
        lv_obj_set_pos(item,  0, item_y);
        lv_obj_set_size(item, avail_w, ITEM_H);
        lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
        lv_obj_set_style_bg_opa(item,   LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(item, AREX_DARK, 0);
        lv_obj_set_style_border_width(item, 2, 0);
        lv_obj_set_style_radius(item,  0, 0);
        lv_obj_set_style_pad_ver(item, 12, 0);
        lv_obj_set_style_pad_hor(item, 15, 0);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl = lv_label_create(item);
        lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
        lv_obj_set_style_text_font(lbl,  AREX_FONT_TITLE, 0);
        lv_obj_set_style_bg_opa(lbl,     LV_OPA_TRANSP, 0);
        lv_label_set_text(lbl, s_info_items[i]);
        lv_obj_set_pos(lbl, 0, 0);

        item_y += ITEM_H + ITEM_GAP;
    }

    arex_screen_register_info_list(s_list);
}

void card_info_update(void)
{
    /* 标题静态，无需刷新 */
}
