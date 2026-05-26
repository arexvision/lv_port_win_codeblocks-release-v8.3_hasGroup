#include "../screen/screen.h"
#include "../core/ui_engine.h"
#include "../core/ui_state.h"
#include "../screen/layout_view.h"
#include "lvgl/lvgl.h"
#include "../fonts/fonts.h"
#include <stdio.h>
#include <string.h>

void arex_screen_register_setup_list(lv_obj_t *list);

/* =========================================================
 * DIVE SETUP 配置数据 (APP 同步就绪)
 * ========================================================= */
static const arex_menu_item_cfg_t s_setup_items[] =
{
    /*  title_text,          badge,       title_font,       val_font,       border, height_u */
    { "GAS SWITCH",    NULL,         FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
    { "CONSERVATISM",  "MED",        FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
    { "BRIGHTNESS",    "ECO",        FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
    { "COMPASS CAL",   "IDLE",       FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
    { "LIGHT CONTROL", NULL,         FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
    { "SYSTEM SETUP",  NULL,         FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
};
#define SETUP_ITEM_COUNT (sizeof(s_setup_items) / sizeof(s_setup_items[0]))

const arex_menu_list_cfg_t setup_menu_cfg =
{
    .items = s_setup_items,
    .count = SETUP_ITEM_COUNT,
};

static lv_obj_t *s_list;

/* badge 句柄数组: 由工厂输出的 item handles 填充 */
static lv_obj_t *s_setup_item_objs[SETUP_ITEM_COUNT];
static lv_obj_t *s_setup_badge_lbls[SETUP_ITEM_COUNT];

void card_setup_create(lv_obj_t *parent)
{
    arex_render_card_title(parent, "DIVE MENU");

    int right_canvas_w = g_sys_config.safe_zone_w - LEFT_ANCHOR_W
                         - ((int)g_sys_config.gap_u * BASE_U);

    uint16_t item_h_px = (uint16_t)g_sys_config.h_menu_item * BASE_U;
    uint16_t gap_y_px  = (uint16_t)g_sys_config.gap_menu * BASE_U;
    uint16_t list_h = SETUP_ITEM_COUNT * item_h_px
                      + (SETUP_ITEM_COUNT - 1) * gap_y_px;

    s_list = lv_obj_create(parent);
    lv_obj_remove_style_all(s_list);
    lv_obj_set_size(s_list, right_canvas_w, list_h);
    lv_obj_set_pos(s_list, 0, CARD_TITLE_H);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_clear_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);

    /* 通用动态菜单工厂统一渲染 */
    arex_render_dynamic_menu(s_list, s_setup_items, SETUP_ITEM_COUNT, 0, s_setup_item_objs);

    /* 填充 badge 句柄数组: child 0=title label, child 1=badge label */
    for (uint8_t i = 0; i < SETUP_ITEM_COUNT; i++)
    {
        s_setup_badge_lbls[i] = lv_obj_get_child(s_setup_item_objs[i], 1);
    }

    /* 首次创建后立即按当前系统配置刷新 badge，避免默认文案与实际亮度档位短暂不一致。 */
    card_setup_update();
    arex_screen_register_setup_list(s_list);
}

void card_setup_update(void)
{
    if (!s_list) return;

    static const char *cons_str[] = { "LOW", "MED", "HIGH", "CUSTOM" };
    static const char *brt_str[]  = { "ECO", "MED", "HIGH", "MAX", "SUN" };
    static const char *cal_str[]   = { "AUTO", "LEARN", "OK" };
    static arex_compass_cal_ui_state_t last_cal_state = AREX_COMPASS_CAL_IDLE;

    uint8_t cons = g_sys_config.conservatism;
    uint8_t brt  = g_sys_config.brightness;
    arex_compass_cal_ui_state_t cal_state = arex_get_compass_calibration_ui_state();

    if (s_setup_badge_lbls[1] && cons < 4)
    {
        lv_label_set_text(s_setup_badge_lbls[1], cons_str[cons]);
    }
    if (s_setup_badge_lbls[2] && brt < 5)
    {
        lv_label_set_text(s_setup_badge_lbls[2], brt_str[brt]);
    }
    if (s_setup_badge_lbls[3])
    {
        uint8_t idx = 0;
        if (cal_state == AREX_COMPASS_CAL_RUNNING) idx = 1;
        else if (cal_state == AREX_COMPASS_CAL_READY) idx = 2;
        lv_label_set_text(s_setup_badge_lbls[3], cal_str[idx]);
    }
    if (cal_state != last_cal_state)
    {
        last_cal_state = cal_state;
        arex_screen_refresh_compass_cal_submenu_if_open();
    }
}
