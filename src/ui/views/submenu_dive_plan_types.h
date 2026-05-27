#ifndef SUBMENU_DIVE_PLAN_TYPES_H
#define SUBMENU_DIVE_PLAN_TYPES_H

#include <stdint.h>

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
    dive_plan_row_t rows[16];
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
    uint16_t total_runtime_min;
    uint16_t total_deco_min;
    uint16_t total_gas_l;
    uint16_t cns_pct;
    uint16_t otu;
    dive_plan_row_t rows[16];
} submenu_dive_plan_snapshot_t;

#ifdef __cplusplus
}
#endif

#endif /* SUBMENU_DIVE_PLAN_TYPES_H */
