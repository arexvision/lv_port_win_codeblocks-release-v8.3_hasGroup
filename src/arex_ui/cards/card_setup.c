#include "../arex_screen.h"
#include "../arex_data.h"
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

    s_list = lv_obj_create(parent);
    lv_obj_set_size(s_list, LV_PCT(100), SETUP_ITEM_COUNT * 48 + (SETUP_ITEM_COUNT - 1) * 8);
    lv_obj_set_pos(s_list, 0, 50);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_set_style_pad_row(s_list, 8, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_OFF);

    for (uint8_t i = 0; i < SETUP_ITEM_COUNT; i++) {
        lv_obj_t *item = lv_obj_create(s_list);
        lv_obj_set_size(item, LV_PCT(100), 48);
        lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(item, AREX_DARK, 0);
        lv_obj_set_style_border_width(item, 2, 0);
        lv_obj_set_style_radius(item, 0, 0);
        lv_obj_set_style_pad_all(item, 0, 0);          /* 零边距，防止撑高 */
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_clip_corner(item, true, 0);   /* 强制裁剪溢出内容 */

        /* 标题 label (child 0) — 左侧 12px 呼吸空间，占左侧主体宽度 */
        lv_obj_t *lbl = lv_label_create(item);
        lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
        lv_obj_set_style_text_font(lbl, AREX_FONT_TITLE, 0);
        lv_obj_set_size(lbl, 280, 48);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 12, 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_LEFT, 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_label_set_text(lbl, s_setup_items[i]);

        /* Badge label (child 1) — 右侧 12px 呼吸空间 */
        lv_obj_t *badge = lv_label_create(item);
        lv_obj_set_style_text_color(badge, AREX_LIGHT, 0);
        lv_obj_set_style_text_font(badge, AREX_FONT_SMALL, 0);
        lv_obj_set_size(badge, 80, 28);
        lv_obj_align(badge, LV_ALIGN_RIGHT_MID, -12, 0);
        lv_obj_set_style_text_align(badge, LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_long_mode(badge, LV_LABEL_LONG_DOT);
        lv_label_set_text(badge, s_setup_badges[i] ? s_setup_badges[i] : "");
        s_badge_lbls[i] = badge;
    }

    arex_screen_register_setup_list(s_list);
}

void card_setup_update(void)
{
    /* Sync badge text with live settings */
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
