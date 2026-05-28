/*
 * 文件: src/app_ui/ui/views/submenu_dive_plan_types.h
 * 作用: 该文件属于视图表现模块，负责菜单、子菜单、模态框或特定视图状态的展示与交互联动。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef SUBMENU_DIVE_PLAN_TYPES_H
#define SUBMENU_DIVE_PLAN_TYPES_H

#include <stdint.h>

#define DIVE_PLAN_RESULT_MAX_ROWS 32U

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    DIVE_PLAN_PAGE_DEPTH = 0,
    DIVE_PLAN_PAGE_TIME,
    DIVE_PLAN_PAGE_RMV,
    DIVE_PLAN_PAGE_READY,
    DIVE_PLAN_PAGE_RESULT,
    DIVE_PLAN_PAGE_ERROR,
} dive_plan_page_t;

typedef enum
{
    DIVE_PLAN_ROW_BOTTOM = 0,
    DIVE_PLAN_ROW_ASCENT,
    DIVE_PLAN_ROW_DECO_STOP,
} dive_plan_row_type_t;

typedef struct
{
    dive_plan_row_type_t type;
    int16_t depth_m;
    uint16_t time_min;
    uint16_t run_min;
    uint8_t o2_pct;
    uint8_t he_pct;
    uint16_t gas_l;
} dive_plan_row_t;

typedef struct
{
    uint8_t valid;
    uint8_t page;
    uint8_t total_pages;
    uint8_t entry_count;
    uint16_t total_runtime_min;
    uint16_t total_deco_min;
    uint16_t total_gas_l;
    uint16_t cns_pct;
    uint16_t otu;
    dive_plan_row_t rows[DIVE_PLAN_RESULT_MAX_ROWS];
} dive_plan_result_snapshot_t;

typedef struct
{
    dive_plan_page_t page;
    float depth_m;
    uint16_t time_min;
    float rmv_lpm;
    uint8_t gf_low;
    uint8_t gf_high;
    uint8_t last_stop_depth_m;
    uint8_t header_gas_o2;
    char gas_summary[32];
    uint8_t result_page_index;
    uint8_t result_total_pages;
    uint8_t result_entry_count;
    uint8_t result_summary_page;
    uint16_t total_runtime_min;
    uint16_t total_deco_min;
    uint16_t total_gas_l;
    uint16_t cns_pct;
    uint16_t otu;
    dive_plan_row_t rows[DIVE_PLAN_RESULT_MAX_ROWS];
} submenu_dive_plan_snapshot_t;

#ifdef __cplusplus
}
#endif

#endif /* SUBMENU_DIVE_PLAN_TYPES_H */
