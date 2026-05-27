#ifndef UI_VM_SYSTEM_VIEW_H
#define UI_VM_SYSTEM_VIEW_H

#include "ui_vm_system_view_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_vm_submenu_view_update(ui_vm_submenu_view_t *vm);
void ui_vm_brightness_update(ui_vm_brightness_t *vm);
void ui_vm_left_aux_update(ui_vm_left_aux_t *vm);

#ifdef __cplusplus
}
#endif

#endif /* UI_VM_SYSTEM_VIEW_H */
