#ifndef UI_VM_SYSTEM_VIEW_TYPES_H
#define UI_VM_SYSTEM_VIEW_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint8_t light_power_on;
} ui_vm_submenu_view_t;

typedef struct
{
    uint8_t brightness_level;
} ui_vm_brightness_t;

typedef struct
{
    char battery_temp_text[16];
    char project_temp_text[16];
} ui_vm_left_aux_t;

#ifdef __cplusplus
}
#endif

#endif /* UI_VM_SYSTEM_VIEW_TYPES_H */
