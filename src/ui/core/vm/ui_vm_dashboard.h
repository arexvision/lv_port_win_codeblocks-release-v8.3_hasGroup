/*
 * 文件: src/app_ui/ui/core/vm/ui_vm_dashboard.h
 * 作用: 该文件属于 ViewModel 子模块，负责把底层运行数据整理为界面可直接消费的展示数据结构。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef UI_VM_DASHBOARD_H
#define UI_VM_DASHBOARD_H

#include "ui_vm_dashboard_types.h"
#include "ui_vm_menu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_vm_compass_update(ui_vm_compass_t *vm,
                          const sensor_data_t *sensor,
                          const sys_config_t *config);
void ui_vm_gas_update(ui_vm_gas_t *vm,
                      const sensor_data_t *sensor,
                      const sys_config_t *config,
                      ui_state_t state,
                      uint8_t gas_cursor);
void ui_vm_deco_update(ui_vm_deco_t *vm,
                       const sensor_data_t *sensor,
                       const sys_config_t *config);
void ui_vm_sys_update(ui_vm_sys_t *vm, const sensor_data_t *sensor);
void ui_vm_depth_update(ui_vm_depth_t *vm, const sensor_data_t *sensor);
void ui_vm_ndl_stop_update(ui_vm_ndl_stop_t *vm, const sensor_data_t *sensor);
void ui_vm_ascent_update(ui_vm_ascent_t *vm, float rate);
void ui_vm_menu_layout_update(ui_vm_menu_layout_t *vm,
                              const sys_config_t *config);
void ui_vm_value_text_update(ui_vm_value_text_t *vm,
                             comp_id_t w_id,
                             uint8_t pod_index);

#ifdef __cplusplus
}
#endif

#endif /* UI_VM_DASHBOARD_H */
