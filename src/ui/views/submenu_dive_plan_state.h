/*
 * 文件: src/app_ui/ui/views/submenu_dive_plan_state.h
 * 作用: 该文件属于视图表现模块，负责菜单、子菜单、模态框或特定视图状态的展示与交互联动。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef SUBMENU_DIVE_PLAN_STATE_H
#define SUBMENU_DIVE_PLAN_STATE_H

#include "submenu_dive_plan_types.h"
#include "menu_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

void submenu_dive_plan_get_snapshot(submenu_dive_plan_snapshot_t *out_snapshot);
/* 子菜单潜水计划状态机重置为初始值。 */
void submenu_dive_plan_reset(void);
/* 外部算法/结果页可以把计算结果快照塞回状态机。 */
void submenu_dive_plan_set_result_snapshot(const dive_plan_result_snapshot_t *snapshot);
bool dive_plan_backend_calculate(float depth_m,
                                 uint16_t bottom_time_min,
                                 float rmv_lpm,
                                 dive_plan_result_snapshot_t *out_snapshot);
/* 旋钮输入会在潜水计划页内部消耗掉，不再继续往外层菜单冒泡。 */
bool submenu_dive_plan_handle_rotate(int8_t dir);
/* 处理潜水计划页上的点击动作。 */
bool submenu_dive_plan_is_result_page(void);
bool submenu_dive_plan_handle_action(menu_item_id_t item_id,
                                     bool *out_close_submenu,
                                     uint8_t *out_keep_index);
/* UI 定时器轮询后台计划计算结果；返回 true 表示页面需要刷新。 */
bool submenu_dive_plan_poll_async(void);

void submenu_dive_plan_set_page(dive_plan_page_t page);
dive_plan_page_t submenu_dive_plan_get_page(void);
uint8_t submenu_dive_plan_get_result_page_index(void);
uint8_t submenu_dive_plan_get_result_total_pages(void);
void submenu_dive_plan_set_depth_m(float value);
void submenu_dive_plan_set_time_min(float value);
void submenu_dive_plan_set_rmv_lpm(float value);

#ifdef __cplusplus
}
#endif

#endif /* SUBMENU_DIVE_PLAN_STATE_H */
