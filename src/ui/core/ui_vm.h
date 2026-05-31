/*
 * 文件: src/app_ui/ui/core/ui_vm.h
 * 作用: 该文件属于 UI 核心模块，负责状态机、数据桥接、事件分发、更新调度或 UI 运行时公共定义。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef UI_VM_H
#define UI_VM_H

#include "ui_defs.h"
#include "ui_types.h"
#include "ui_state.h"
#include "vm/ui_vm_dashboard_types.h"
#include "vm/ui_vm_plan_chart_types.h"
#include "vm/ui_vm_plan_view_types.h"
#include "vm/ui_vm_info_types.h"
#include "vm/ui_vm_system_view_types.h"
#include "vm/ui_vm_menu_types.h"
#include "vm/ui_vm_system_view_types.h"

#include <stdbool.h>
#include <stdint.h>

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
void ui_vm_depth_update(ui_vm_depth_t *vm, const sensor_data_t *sensor);
void ui_vm_ndl_stop_update(ui_vm_ndl_stop_t *vm, const sensor_data_t *sensor);
void ui_vm_ascent_update(ui_vm_ascent_t *vm, float rate);
void ui_vm_menu_layout_update(ui_vm_menu_layout_t *vm,
                              const sys_config_t *config);
void ui_vm_setup_menu_update(ui_vm_setup_menu_t *vm);
void ui_vm_modal_gas_update(ui_vm_modal_gas_t *vm, uint8_t gas_cursor);
void ui_vm_info_lines_update(ui_vm_info_lines_t *vm, uint8_t info_group_index);
void ui_vm_gas_switch_menu_update(ui_vm_gas_switch_menu_t *vm);
void ui_vm_dive_setup_menu_update(ui_vm_dive_setup_menu_t *vm,
                                  uint8_t salinity_mode,
                                  uint8_t safety_stop_mode,
                                  uint8_t altitude_level);
void ui_vm_dive_context_update(ui_vm_dive_context_t *vm);
void ui_vm_alerts_menu_update(ui_vm_simple_menu_t *vm,
                              uint16_t depth_alarm_m,
                              uint16_t time_alarm_min,
                              uint8_t ndl_alarm_min);
void ui_vm_display_menu_update(ui_vm_simple_menu_t *vm,
                               uint8_t units_mode,
                               uint8_t log_rate_s,
                               uint8_t bluetooth_enabled);
void ui_vm_datetime_menu_update(ui_vm_simple_menu_t *vm,
                                uint16_t year,
                                uint8_t month,
                                uint8_t day,
                                uint8_t hour,
                                uint8_t minute);
void ui_vm_edit_mod_ppo2_update(ui_vm_edit_spec_t *vm);
void ui_vm_edit_nitrox_o2_update(ui_vm_edit_spec_t *vm, uint8_t value);
void ui_vm_edit_three_gas_o2_update(ui_vm_edit_spec_t *vm, uint8_t item_index, uint8_t value);
void ui_vm_edit_oc_tech_gas_update(ui_vm_edit_spec_t *vm,
                                   uint8_t slot,
                                   uint8_t item_index,
                                   uint8_t o2,
                                   uint8_t he);
void ui_vm_edit_depth_alarm_update(ui_vm_edit_spec_t *vm, uint16_t value);
void ui_vm_edit_time_alarm_update(ui_vm_edit_spec_t *vm, uint16_t value);
void ui_vm_edit_ndl_alarm_update(ui_vm_edit_spec_t *vm, uint16_t value);
void ui_vm_edit_datetime_update(ui_vm_edit_spec_t *vm, uint8_t field, uint16_t value);
void ui_vm_dive_plan_inputs_update(ui_vm_dive_plan_inputs_t *vm);
void ui_vm_dive_plan_view_update(ui_vm_dive_plan_view_t *vm);
void ui_vm_plan_chart_update(ui_vm_plan_chart_t *vm);
void ui_vm_submenu_view_update(ui_vm_submenu_view_t *vm);
void ui_vm_brightness_update(ui_vm_brightness_t *vm);
void ui_vm_left_aux_update(ui_vm_left_aux_t *vm);
void ui_vm_value_text_update(ui_vm_value_text_t *vm,
                             comp_id_t w_id,
                             uint8_t pod_index);

#ifdef __cplusplus
}
#endif

#endif /* UI_VM_H */
