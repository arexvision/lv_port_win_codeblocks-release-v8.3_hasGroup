#include "../arex_screen.h"
#include "../arex_ui_engine.h"
#include "../arex_ui_state.h"
#include "lvgl/lvgl.h"
#include "../fonts/arex_fonts.h"
#include <stdio.h>
#include <string.h>

void arex_screen_register_setup_list(lv_obj_t *list);

/* =========================================================
 * DIVE SETUP 配置数据 (APP 同步就绪)
 * ========================================================= */
static const arex_menu_item_cfg_t s_setup_items[] = {
    /*  title_text,          badge,       title_font,       val_font,       border, height_u */
    { "> GAS SWITCH",    NULL,         AREX_FONT_ID_TITLE, AREX_FONT_ID_SMALL, 2, 0 },
    { "> CONSERVATISM",  "MED",        AREX_FONT_ID_TITLE, AREX_FONT_ID_SMALL, 2, 0 },
    { "> BRIGHTNESS",    "HIGH",       AREX_FONT_ID_TITLE, AREX_FONT_ID_SMALL, 2, 0 },
    { "> COMPASS CAL",   NULL,         AREX_FONT_ID_TITLE, AREX_FONT_ID_SMALL, 2, 0 },
    { "> SYSTEM SETUP",  NULL,         AREX_FONT_ID_TITLE, AREX_FONT_ID_SMALL, 2, 0 },
};
#define SETUP_ITEM_COUNT (sizeof(s_setup_items) / sizeof(s_setup_items[0]))

const arex_menu_list_cfg_t setup_menu_cfg = {
    .items = s_setup_items,
    .count = SETUP_ITEM_COUNT,
};

static lv_obj_t *s_list;

/* badge 句柄数组: 由工厂输出的 item handles 填充 */
static lv_obj_t *s_setup_item_objs[SETUP_ITEM_COUNT];
static lv_obj_t *s_setup_badge_lbls[SETUP_ITEM_COUNT];

void card_setup_create(lv_obj_t *parent)
{
    arex_screen_make_card_title(parent, "> DIVE SETUP");

    int right_canvas_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                       - ((int)g_sys_config.gap_u * AREX_BASE_U);

    uint16_t item_h_px = (uint16_t)g_sys_config.h_menu_item * AREX_BASE_U;
    uint16_t gap_y_px  = (uint16_t)g_sys_config.gap_menu * AREX_BASE_U;
    uint16_t list_h = SETUP_ITEM_COUNT * item_h_px
                    + (SETUP_ITEM_COUNT - 1) * gap_y_px;

    s_list = lv_obj_create(parent);
    lv_obj_set_size(s_list, right_canvas_w, list_h);
    lv_obj_set_pos(s_list, 0, 50);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_clear_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);

    /* 通用动态菜单工厂统一渲染 */
    arex_render_dynamic_menu(s_list, s_setup_items, SETUP_ITEM_COUNT, 0, s_setup_item_objs);

    /* 填充 badge 句柄数组: child 0=title label, child 1=badge label */
    for (uint8_t i = 0; i < SETUP_ITEM_COUNT; i++) {
        s_setup_badge_lbls[i] = lv_obj_get_child(s_setup_item_objs[i], 1);
    }

    arex_screen_register_setup_list(s_list);
}

void card_setup_update(void)
{
    if (!s_list) return;

    static const char *cons_str[] = { "LOW", "MED", "HIGH" };
    static const char *brt_str[]  = { "LOW", "MED", "HIGH", "MAX" };

    /* dirty check: 只在值真正变化时才调用 set_text */
    static uint8_t last_cons = 0xFF;
    static uint8_t last_brt  = 0xFF;

    uint8_t cons = g_sys_config.conservatism;
    uint8_t brt  = g_sys_config.brightness;

    if (s_setup_badge_lbls[1] && cons < 3 && cons != last_cons) {
        lv_label_set_text(s_setup_badge_lbls[1], cons_str[cons]);
        last_cons = cons;
    }
    if (s_setup_badge_lbls[2] && brt < 4 && brt != last_brt) {
        lv_label_set_text(s_setup_badge_lbls[2], brt_str[brt]);
        last_brt = brt;
    }
}
