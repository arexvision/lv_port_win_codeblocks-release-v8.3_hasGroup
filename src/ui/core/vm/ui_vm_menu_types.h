#ifndef UI_VM_MENU_TYPES_H
#define UI_VM_MENU_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#include "../ui_defs.h"
#include "../ui_types.h"
#include "ui_vm_info_types.h"
#include "ui_vm_system_view_types.h"
#include "../ui_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint16_t right_canvas_w;
    uint16_t item_h_px;
    uint16_t gap_y_px;
} ui_vm_menu_layout_t;

typedef struct
{
    char conservatism_badge[16];
    char brightness_badge[16];
    uint8_t compass_cal_badge_idx;
    compass_cal_ui_state_t compass_cal_state;
} ui_vm_setup_menu_t;

typedef struct
{
    char title[24];
    char body[48];
    char hint[32];
    uint8_t valid;
    uint8_t danger;
} ui_vm_modal_gas_t;

typedef struct
{
    uint8_t count;
    char items[GAS_COUNT][20];
} ui_vm_gas_switch_menu_t;

typedef struct
{
    uint8_t count;
    char items[6][32];
} ui_vm_dive_setup_menu_t;

typedef struct
{
    uint8_t salinity_mode;
    uint8_t gf_low;
    uint8_t gf_high;
    uint8_t last_stop_depth_m;
} ui_vm_dive_context_t;

typedef struct
{
    uint8_t count;
    char items[8][40];
} ui_vm_simple_menu_t;

typedef struct
{
    float value;
    float min;
    float max;
    float step;
    uint8_t decimals;
    char label[20];
} ui_vm_edit_spec_t;

#ifdef __cplusplus
}
#endif

#endif /* UI_VM_MENU_TYPES_H */
