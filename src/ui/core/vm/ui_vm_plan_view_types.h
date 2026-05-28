/*
 * 文件: src/app_ui/ui/core/vm/ui_vm_plan_view_types.h
 * 作用: 该文件属于 ViewModel 子模块，负责把底层运行数据整理为界面可直接消费的展示数据结构。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

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
    uint8_t result_summary_page;
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
