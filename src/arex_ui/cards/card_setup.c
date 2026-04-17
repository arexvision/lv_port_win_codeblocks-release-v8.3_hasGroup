/*
 * card_setup.c  —  5F: DIVE SETUP
 *
 * 架构规则：同 card_info.c，current_y 绝对定位，零 Flex
 */

#include "../arex_ui_engine.h"
#include "../arex_screen.h"
#include "../arex_data.h"
#include "../arex_ui_state.h"
#include "../fonts/arex_fonts.h"
#include "lvgl/lvgl.h"
#include <string.h>

void arex_screen_register_setup_list(lv_obj_t *list);

static const char *s_setup_items[] = {
    "> GAS SWITCH",
    "> CONSERVATISM",
    "> BRIGHTNESS",
    "> COMPASS CAL",
    "> SYSTEM SETUP",
};
static const char *s_setup_badges[] = {
    NULL, "MED", "HIGH", NULL, NULL,
};
#define SETUP_ITEM_COUNT  5
#define ITEM_H            48
#define ITEM_GAP           8
#define TITLE_H           44
#define PAD_X             16

static lv_obj_t *s_list;
static lv_obj_t *s_badge_lbls[SETUP_ITEM_COUNT];

void card_setup_create(lv_obj_t *parent)
{
    arex_screen_make_card_title(parent, "> DIVE SETUP");

    int16_t avail_w = g_layout.rc_w - PAD_X * 2;

    s_list = lv_obj_create(parent);
    lv_obj_set_pos(s_list,  PAD_X, TITLE_H);
    lv_obj_set_size(s_list, avail_w,
                    SETUP_ITEM_COUNT * ITEM_H + (SETUP_ITEM_COUNT - 1) * ITEM_GAP);
    lv_obj_set_style_bg_opa(s_list,      LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list,      0, 0);
    lv_obj_set_style_radius(s_list,       0, 0);
    lv_obj_set_scrollbar_mode(s_list,     LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);

    int16_t item_y = 0;
    for (uint8_t i = 0; i < SETUP_ITEM_COUNT; i++) {
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

        /* child 0: 标题 label（左对齐，绝对坐标 0,0）*/
        lv_obj_t *lbl = lv_label_create(item);
        lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
        lv_obj_set_style_text_font(lbl,  AREX_FONT_TITLE, 0);
        lv_obj_set_style_bg_opa(lbl,     LV_OPA_TRANSP, 0);
        lv_label_set_text(lbl, s_setup_items[i]);
        lv_obj_set_pos(lbl, 0, 0);

        /* child 1: badge label（右对齐，绝对坐标 avail_w-padding-badge_w, 0）
           用 align 简化：LVGL 的 lv_obj_align 相对于 parent，无 Flex 依赖 */
        lv_obj_t *badge = lv_label_create(item);
        lv_obj_set_style_text_color(badge, AREX_LIGHT, 0);
        lv_obj_set_style_text_font(badge,  AREX_FONT_SMALL, 0);
        lv_obj_set_style_bg_opa(badge,     LV_OPA_TRANSP, 0);
        lv_label_set_text(badge, s_setup_badges[i] ? s_setup_badges[i] : "");
        /* 右对齐到 item 右边：使用 lv_obj_align（不依赖 Flex）*/
        lv_obj_align(badge, LV_ALIGN_RIGHT_MID, -2, 0);
        s_badge_lbls[i] = badge;

        item_y += ITEM_H + ITEM_GAP;
    }

    arex_screen_register_setup_list(s_list);
}

void card_setup_update(void)
{
    if (!s_list) return;
    static const char *cons_str[] = { "LOW", "MED", "HIGH" };
    static const char *brt_str[]  = { "LOW", "MED", "HIGH", "MAX" };

    uint8_t cons = g_arex.settings.conservatism;
    uint8_t brt  = g_arex.settings.brightness;
    if (s_badge_lbls[1] && cons < 3)
        lv_label_set_text(s_badge_lbls[1], cons_str[cons]);
    if (s_badge_lbls[2] && brt < 4)
        lv_label_set_text(s_badge_lbls[2], brt_str[brt]);
}
