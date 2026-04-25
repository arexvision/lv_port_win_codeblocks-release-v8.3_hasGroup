#include "../arex_screen.h"
#include "../arex_data.h"
#include "../arex_ui_engine.h"
#include "../arex_ui_state.h"
#include "lvgl/lvgl.h"
#include "../fonts/arex_fonts.h"
#include <stdio.h>
#include <string.h>

void arex_screen_register_info_list(lv_obj_t *list);

/* =========================================================
 * INFO MENU 配置数据 (APP 同步就绪)
 * height_u=0 表示使用 g_sys_config.h_menu_item 默认值
 * ========================================================= */
static const arex_menu_item_cfg_t s_info_items[] = {
    /*  title_text,         badge,  title_font,       val_font,       border, height_u */
    { "> LAST DIVE",      NULL,   AREX_FONT_ID_TITLE, AREX_FONT_ID_SMALL, 2, 0 },
    { "> DIVE PLAN",      NULL,   AREX_FONT_ID_TITLE, AREX_FONT_ID_SMALL, 2, 0 },
    { "> TISSUE & TOX",  NULL,   AREX_FONT_ID_TITLE, AREX_FONT_ID_SMALL, 2, 0 },
    { "> GAS & CALC",     NULL,   AREX_FONT_ID_TITLE, AREX_FONT_ID_SMALL, 2, 0 },
    { "> SENSOR & DEVICE", NULL,  AREX_FONT_ID_TITLE, AREX_FONT_ID_SMALL, 2, 0 },
};
#define INFO_ITEM_COUNT (sizeof(s_info_items) / sizeof(s_info_items[0]))

const arex_menu_list_cfg_t info_menu_cfg = {
    .items = s_info_items,
    .count = INFO_ITEM_COUNT,
};

static lv_obj_t *s_list;

void card_info_create(lv_obj_t *parent)
{
    arex_screen_make_card_title(parent, "> INFO MENU");

    int right_canvas_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                       - ((int)g_sys_config.gap_u * AREX_BASE_U);

    /* 列表总高度从 h_menu_item 和 gap_menu 推算 */
    uint16_t item_h_px = (uint16_t)g_sys_config.h_menu_item * AREX_BASE_U;
    uint16_t gap_y_px  = (uint16_t)g_sys_config.gap_menu * AREX_BASE_U;
    uint16_t list_h = INFO_ITEM_COUNT * item_h_px
                    + (INFO_ITEM_COUNT - 1) * gap_y_px;

    s_list = lv_obj_create(parent);
    lv_obj_set_size(s_list, right_canvas_w, list_h);
    lv_obj_set_pos(s_list, 0, AREX_CARD_TITLE_H);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_clear_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);

    /* 通用动态菜单工厂统一渲染 */
    arex_render_dynamic_menu(s_list, s_info_items, INFO_ITEM_COUNT, 0, NULL);

    arex_screen_register_info_list(s_list);
}

void card_info_update(void)
{
    /* INFO sub-menu strings are built dynamically in arex_screen.c
       from g_sensor_data values each time the user opens a sub-menu.
       This update callback is intentionally minimal — the static
       item titles never change. */
}
