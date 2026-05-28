/*
 * 文件: src/app_ui/ui/menus/menu_info.c
 * 作用: 该文件属于菜单定义模块，负责信息菜单或设置菜单的条目组织、入口描述与页面装配。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时建议先理解该文件在 app_ui 流程中的位置，再修改注释所描述的职责边界，避免把数据准备、状态切换和视图渲染职责混杂到一起。
 */

#include "../screen/screen.h"
#include "../core/data.h"
#include "../core/ui_engine.h"
#include "../core/ui_vm.h"
#include "../core/vm/ui_vm_menu_types.h"
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
static ui_vm_menu_layout_t s_menu_layout_vm;

void menu_info_create(lv_obj_t *parent)
{
    uint8_t info_count = 0;
    const menu_item_cfg_t *info_items = menu_defs_info_items(&info_count);

    render_card_title(parent, "INFO MENU");

    ui_vm_menu_layout_update(&s_menu_layout_vm, NULL);
    int right_canvas_w = (int)s_menu_layout_vm.right_canvas_w;

    /* 列表总高度从 h_menu_item 和 gap_menu 推算。 */
    uint16_t item_h_px = s_menu_layout_vm.item_h_px;
    uint16_t gap_y_px  = s_menu_layout_vm.gap_y_px;
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
