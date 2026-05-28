/*
 * 文件: src/app_ui/ui/views/menu_actions.h
 * 作用: 该文件属于视图表现模块，负责菜单、子菜单、模态框或特定视图状态的展示与交互联动。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef MENU_ACTIONS_H
#define MENU_ACTIONS_H

#include "menu_defs.h"
#include "submenu_dive_plan_state.h"

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
