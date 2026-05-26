#ifndef MENU_RUNTIME_H
#define MENU_RUNTIME_H

#include "menu_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* menu_runtime 是菜单的“当前状态机”：
 * - 记录当前打开哪个 menu_id_t
 * - 记录父级菜单栈，用于返回上一级
 * - 把 submenu_model 的动态 label 包装成带稳定 ID 的 menu_row_t
 *
 * 注意：title/rows 是给 view 渲染用的，不给业务层拿来 strcmp。
 */
void menu_runtime_reset(void);
bool menu_runtime_open_info(uint8_t index);
bool menu_runtime_open_setup(uint8_t index);
bool menu_runtime_open_child(menu_id_t child_id, menu_item_id_t source_item);
bool menu_runtime_back(void);
void menu_runtime_refresh(void);

/* 当前菜单查询。
 * current_id 用于业务判断；current_title/current_rows 只用于画界面。
 */
menu_id_t menu_runtime_current_id(void);
const char *menu_runtime_current_title(void);
const menu_row_t *menu_runtime_current_rows(uint8_t *out_count);
const menu_row_t *menu_runtime_row_at(uint8_t index);

/* DIVE PLAN 的交互比较特殊：它不是普通列表，而是一套输入/结果页。
 * runtime 暴露这些状态，让 submenu_view 用对应的绘制方式。
 */
bool menu_runtime_is_dive_plan(void);
bool menu_runtime_is_dive_plan_result(void);
bool menu_runtime_is_nested(void);
uint8_t menu_runtime_default_selection(void);

#ifdef __cplusplus
}
#endif

#endif /* MENU_RUNTIME_H */
