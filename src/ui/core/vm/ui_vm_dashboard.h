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
