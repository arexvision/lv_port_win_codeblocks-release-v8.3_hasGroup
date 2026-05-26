#include "../screen/screen.h"
#include "../core/data.h"
#include "../core/ui_engine.h"
#include "../core/ui_state.h"
#include "../screen/layout_view.h"
#include "../views/menu_defs.h"
#include "lvgl/lvgl.h"
#include "../fonts/fonts.h"
#include <stdio.h>
#include <string.h>

void screen_register_info_list(lv_obj_t *list);

/* INFO 顶层菜单页。
 * 它占用右侧 tileview 中的一个固定页面，但职责只是菜单入口，不是业务卡片。
 */
#define INFO_ITEM_COUNT SUBMENU_INFO_COUNT

const menu_list_cfg_t menu_info_cfg =
{
    .items = g_menu_info_items,
    .count = INFO_ITEM_COUNT,
};

static lv_obj_t *s_list;

void menu_info_create(lv_obj_t *parent)
{
    uint8_t info_count = 0;
    const menu_item_cfg_t *info_items = menu_defs_info_items(&info_count);

    render_card_title(parent, "INFO MENU");

    int right_canvas_w = g_sys_config.safe_zone_w - LEFT_ANCHOR_W
                         - ((int)g_sys_config.gap_u * BASE_U);

    /* 列表总高度从 h_menu_item 和 gap_menu 推算。 */
    uint16_t item_h_px = (uint16_t)g_sys_config.h_menu_item * BASE_U;
    uint16_t gap_y_px  = (uint16_t)g_sys_config.gap_menu * BASE_U;
    uint16_t list_h = info_count * item_h_px
                      + (info_count - 1) * gap_y_px;

    s_list = lv_obj_create(parent);
    lv_obj_remove_style_all(s_list);
    lv_obj_set_size(s_list, right_canvas_w, list_h);
    lv_obj_set_pos(s_list, 0, CARD_TITLE_H);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_clear_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);

    /* 顶层菜单也走同一套菜单行渲染，减少重复维护。 */
    render_dynamic_menu(s_list, info_items, info_count, 0, NULL);

    screen_register_info_list(s_list);
}

void menu_info_update(void)
{
    /* INFO 子菜单的动态数值在打开子菜单时生成。
     * 顶层入口文案本身不会随传感器数据变化，所以这里保持为空。
     */
}
