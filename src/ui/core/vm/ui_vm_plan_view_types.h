#ifndef UI_VM_PLAN_VIEW_TYPES_H
#define UI_VM_PLAN_VIEW_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    float depth_m;
    uint16_t time_min;
    float rmv_lpm;
    uint8_t header_gas_o2;
    char gas_summary[32];
} ui_vm_dive_plan_inputs_t;

typedef struct
{
    uint8_t valid;
    char depth_text[8];
    char time_text[8];
    char run_text[8];
    char gas_text[12];
    char qty_text[8];
} ui_vm_dive_plan_row_t;

typedef struct
{
    uint8_t page;
    uint8_t header_gas_o2;
    uint8_t gf_low;
    uint8_t gf_high;
    uint8_t last_stop_depth_m;
    uint8_t result_page_index;
    uint8_t result_total_pages;
    uint8_t result_entry_count;
    float depth_m;
    float rmv_lpm;
    uint16_t time_min;
    char depth_value[8];
    char time_value[8];
    char rmv_value[8];
    char input_prompt[24];
    char input_unit[24];
    char input_min_text[16];
    char input_max_text[16];
    char ready_gf_text[24];
    char ready_last_stop_text[24];
    char ready_start_cns_text[24];
    char gas_summary[32];
    char result_runtime_text[24];
    char result_deco_text[24];
    char result_gas_text[24];
    char result_cns_text[24];
    char result_otu_text[24];
    char error_title[24];
    char error_hint[40];
    char result_page_text[16];
    ui_vm_dive_plan_row_t rows[8];
} ui_vm_dive_plan_view_t;

#ifdef __cplusplus
}
#endif

#endif /* UI_VM_PLAN_VIEW_TYPES_H */
