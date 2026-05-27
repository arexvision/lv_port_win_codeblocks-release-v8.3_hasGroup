#ifndef UI_VM_MENU_H
#define UI_VM_MENU_H

#include "ui_vm_menu_types.h"
#include "ui_vm_plan_view.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_vm_setup_menu_update(ui_vm_setup_menu_t *vm);
void ui_vm_modal_gas_update(ui_vm_modal_gas_t *vm, uint8_t gas_cursor);
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
void ui_vm_systems_setup_menu_update(ui_vm_simple_menu_t *vm, uint8_t dive_mode);
void ui_vm_ai_menu_update(ui_vm_simple_menu_t *vm,
                          const uint8_t tank_state[2],
                          uint8_t gtr_enabled);
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
void ui_vm_nitrox_menu_update(ui_vm_simple_menu_t *vm, uint8_t nitrox_o2_pct);
void ui_vm_three_gas_menu_update(ui_vm_simple_menu_t *vm,
                                 const uint8_t three_gas_o2_pct[3],
                                 uint8_t gas_count);
void ui_vm_oc_tech_menu_update(ui_vm_simple_menu_t *vm,
                               const uint8_t o2_pct[5],
                               const uint8_t he_pct[5]);
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
void ui_vm_edit_datetime_update(ui_vm_edit_spec_t *vm, uint8_t field, uint16_t value);
#ifdef __cplusplus
}
#endif

#endif /* UI_VM_MENU_H */
