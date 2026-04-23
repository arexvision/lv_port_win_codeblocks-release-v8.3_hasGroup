#include "../arex_screen.h"
#include "../arex_ui_engine.h"
#include "../arex_ui_state.h"
#include "lvgl/lvgl.h"
#include "../fonts/arex_fonts.h"
#include <stdio.h>
#include <string.h>

void arex_screen_register_setup_list(lv_obj_t *list);

static const char *s_setup_items[] = {
    "> GAS SWITCH",
    "> CONSERVATISM",
    "> BRIGHTNESS",
    "> COMPASS CAL",
    "> SYSTEM SETUP",
};

/* Initial badge values — index matches s_setup_items.
   NULL means no badge for that row. */
static const char *s_setup_badges[] = {
    NULL,       /* GAS SWITCH — no badge */
    "MED",      /* CONSERVATISM default (GF 30/70) */
    "HIGH",     /* BRIGHTNESS default */
    NULL,       /* COMPASS CAL — no badge */
    NULL,       /* SYSTEM SETUP — no badge */
};

#define SETUP_ITEM_COUNT 5

static lv_obj_t *s_list;

/* badge labels are child 1 of each item (child 0 = title label) */
static lv_obj_t *s_badge_lbls[SETUP_ITEM_COUNT];

void card_setup_create(lv_obj_t *parent)
{
    arex_screen_make_card_title(parent, "> DIVE SETUP");

    /* 从配置总线推算物理尺寸 */
    int right_canvas_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                         - (g_sys_config.gap_u * AREX_BASE_U);
    int item_h = 48;
    int item_w = right_canvas_w - 15;
    int gap_y  = 8;

    s_list = lv_obj_create(parent);
    lv_obj_set_size(s_list, right_canvas_w,
                    SETUP_ITEM_COUNT * item_h + (SETUP_ITEM_COUNT - 1) * gap_y);
    lv_obj_set_pos(s_list, 0, 50);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_clear_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);

    int current_y = 0;
    for (uint8_t i = 0; i < SETUP_ITEM_COUNT; i++) {
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

        /* 标题 label (child 0) */
        lv_obj_t *lbl = lv_label_create(item);
        lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
        lv_obj_set_style_text_font(lbl, arex_get_font(AREX_FONT_ID_TITLE), 0);
        lv_obj_set_size(lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 12, 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_label_set_text(lbl, s_setup_items[i]);

        /* Badge label (child 1) */
        lv_obj_t *badge = lv_label_create(item);
        lv_obj_set_style_text_color(badge, AREX_LIGHT, 0);
        lv_obj_set_style_text_font(badge, arex_get_font(AREX_FONT_ID_SMALL), 0);
        lv_obj_set_size(badge, 80, 28);
        lv_obj_align(badge, LV_ALIGN_RIGHT_MID, -12, 0);
        lv_obj_set_style_text_align(badge, LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_long_mode(badge, LV_LABEL_LONG_DOT);
        lv_label_set_text(badge, s_setup_badges[i] ? s_setup_badges[i] : "");
        s_badge_lbls[i] = badge;

        current_y += item_h + gap_y;
    }

    arex_screen_register_setup_list(s_list);
}

void card_setup_update(void)
{
    if (!s_list) return;

    static const char *cons_str[] = { "LOW", "MED", "HIGH" };
    static const char *brt_str[]  = { "LOW", "MED", "HIGH", "MAX" };

    /* dirty check：只在值真正变化时才调用 set_text，避免触发无效重绘 */
    static uint8_t last_cons = 0xFF;
    static uint8_t last_brt  = 0xFF;

    uint8_t cons = g_sys_config.conservatism;
    uint8_t brt  = g_sys_config.brightness;

    if (s_badge_lbls[1] && cons < 3 && cons != last_cons) {
        lv_label_set_text(s_badge_lbls[1], cons_str[cons]);
        last_cons = cons;
    }
    if (s_badge_lbls[2] && brt < 4 && brt != last_brt) {
        lv_label_set_text(s_badge_lbls[2], brt_str[brt]);
        last_brt = brt;
    }
}
