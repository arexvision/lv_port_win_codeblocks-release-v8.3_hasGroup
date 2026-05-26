#include "../screen/screen.h"
#include "../core/ui_engine.h"
#include "../core/ui_state.h"
#include "../screen/layout_view.h"
#include "../views/menu_defs.h"
#include "../views/submenu_model.h"
#include "lvgl/lvgl.h"
#include "../fonts/fonts.h"
#include <stdio.h>
#include <string.h>

void screen_register_setup_list(lv_obj_t *list);
void menu_setup_update(void);

/* DIVE MENU 顶层菜单页。
 * 它占用右侧 tileview 中的一个固定页面，但职责是设置入口和 badge 展示。
 */
#define SETUP_ITEM_COUNT SUBMENU_SETUP_COUNT

const menu_list_cfg_t menu_setup_cfg =
{
    .items = g_menu_setup_items,
    .count = SETUP_ITEM_COUNT,
};

static lv_obj_t *s_list;

/* badge 句柄数组：由动态菜单工厂输出的 item handles 填充。
 * child 0=title label，child 1=badge label。
 */
static lv_obj_t *s_setup_item_objs[SETUP_ITEM_COUNT];
static lv_obj_t *s_setup_badge_lbls[SETUP_ITEM_COUNT];

void menu_setup_create(lv_obj_t *parent)
{
    uint8_t setup_count = 0;
    const menu_item_cfg_t *setup_items = menu_defs_setup_items(&setup_count);

    render_card_title(parent, "DIVE MENU");

    int right_canvas_w = g_sys_config.safe_zone_w - LEFT_ANCHOR_W
                         - ((int)g_sys_config.gap_u * BASE_U);

    uint16_t item_h_px = (uint16_t)g_sys_config.h_menu_item * BASE_U;
    uint16_t gap_y_px  = (uint16_t)g_sys_config.gap_menu * BASE_U;
    uint16_t list_h = setup_count * item_h_px
                      + (setup_count - 1) * gap_y_px;

    s_list = lv_obj_create(parent);
    lv_obj_remove_style_all(s_list);
    lv_obj_set_size(s_list, right_canvas_w, list_h);
    lv_obj_set_pos(s_list, 0, CARD_TITLE_H);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_clear_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);

    /* 顶层菜单也走同一套菜单行渲染，减少重复维护。 */
    render_dynamic_menu(s_list, setup_items, setup_count, 0, s_setup_item_objs);

    for (uint8_t i = 0; i < setup_count; i++)
    {
        s_setup_badge_lbls[i] = lv_obj_get_child(s_setup_item_objs[i], 1);
    }

    /* 首次创建后，立刻按当前系统配置刷新 badge。 */
    menu_setup_update();
    screen_register_setup_list(s_list);
}

void menu_setup_update(void)
{
    if (!s_list) return;

    static const char *cal_str[] = { "AUTO", "LEARN", "OK" };
    static compass_cal_ui_state_t last_cal_state = COMPASS_CAL_IDLE;

    uint8_t cons = g_sys_config.conservatism;
    uint8_t brt  = g_sys_config.brightness;
    compass_cal_ui_state_t cal_state = get_compass_calibration_ui_state();

    if (s_setup_badge_lbls[1])
    {
        lv_label_set_text(s_setup_badge_lbls[1], submenu_conservatism_badge(cons));
    }
    if (s_setup_badge_lbls[2])
    {
        lv_label_set_text(s_setup_badge_lbls[2], submenu_brightness_badge(brt));
    }
    if (s_setup_badge_lbls[3])
    {
        uint8_t idx = 0;
        if (cal_state == COMPASS_CAL_RUNNING) idx = 1;
        else if (cal_state == COMPASS_CAL_READY) idx = 2;
        lv_label_set_text(s_setup_badge_lbls[3], cal_str[idx]);
    }
    if (cal_state != last_cal_state)
    {
        last_cal_state = cal_state;
        screen_refresh_compass_cal_submenu_if_open();
    }
}
