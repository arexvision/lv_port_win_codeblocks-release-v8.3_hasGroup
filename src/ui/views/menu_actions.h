#ifndef MENU_ACTIONS_H
#define MENU_ACTIONS_H

#include "menu_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* menu_actions 是菜单的“业务动作层”。
 * submenu_view.c 只把当前 row 传进来，不读取 row->label 做判断。
 * 本函数根据 row->id 决定：
 * - 直接应用设置
 * - 打开子菜单
 * - 弹确认框
 * - 开始内联编辑
 * - 打开 GAS/LIGHT/COMPASS/DIVE PLAN 等特殊流程
 */
bool menu_actions_handle_select(uint8_t row_index,
                                const menu_row_t *row,
                                menu_action_t *out_action);

/* 确认弹窗的 pending 设置保存在 action 层。
 * view 只负责显示/确认/取消，不需要知道具体设置 kind。
 */
void menu_actions_clear_pending(void);
bool menu_actions_confirm_pending(bool *out_close_parent_too,
                                  bool *out_return_dash);

#ifdef __cplusplus
}
#endif

#endif /* MENU_ACTIONS_H */
