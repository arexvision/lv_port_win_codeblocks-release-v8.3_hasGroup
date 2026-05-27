#include "submenu_dive_plan_state.h"

#include "../core/data.h"
#include "../core/vm/ui_vm_menu.h"
#include "../core/vm/ui_vm_plan_view.h"

#include <stdio.h>
#include <string.h>

#ifdef PC_SIMULATOR
#include "../../algo_sim/buhlmann_debug.h"
#endif

#define PLAN_ROWS_PER_PAGE 8U

typedef struct
{
    dive_plan_page_t page;
    bool defaults_loaded;
    float depth_m;
    uint16_t time_min;
    float rmv_lpm;
    uint8_t result_page;
    dive_plan_result_snapshot_t result;
} submenu_dive_plan_state_t;

static submenu_dive_plan_state_t s_state =
{
    DIVE_PLAN_PAGE_DEPTH,
    false,
    30.0f,
    20U,
    14.0f,
    0U,
    { 0 }
};

static uint16_t plan_round_u16(float value)
{
    if (value <= 0.0f)
    {
        return 0U;
    }
    if (value >= 65535.0f)
    {
        return 65535U;
    }
    return (uint16_t)(value + 0.5f);
}

static float clamp_float(float value, float min_value, float max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

static uint8_t plan_gf_low(void)
{
    ui_vm_dive_context_t vm;

    ui_vm_dive_context_update(&vm);
    return vm.gf_low;
}

static uint8_t plan_gf_high(void)
{
    ui_vm_dive_context_t vm;

    ui_vm_dive_context_update(&vm);
    return vm.gf_high;
}

static uint8_t plan_last_deco_depth(void)
{
    ui_vm_dive_context_t vm;

    ui_vm_dive_context_update(&vm);
    return vm.last_stop_depth_m;
}

static void plan_ensure_defaults(void)
{
    ui_vm_dive_plan_inputs_t vm;

    if (s_state.defaults_loaded)
    {
        return;
    }

    ui_vm_dive_plan_inputs_update(&vm);
    s_state.depth_m = vm.depth_m;
    s_state.time_min = vm.time_min;
    s_state.rmv_lpm = vm.rmv_lpm;
    s_state.defaults_loaded = true;
}

static uint8_t plan_result_total_pages(void)
{
    if (s_state.result.valid == 0U)
    {
        return 1U;
    }
    if (s_state.result.total_pages == 0U)
    {
        return (uint8_t)((s_state.result.entry_count + PLAN_ROWS_PER_PAGE - 1U) / PLAN_ROWS_PER_PAGE);
    }
    return s_state.result.total_pages;
}

#ifdef PC_SIMULATOR
static dive_plan_row_type_t plan_row_type_from_algo(buhlmann_debug_plan_row_type_t type)
{
    switch (type)
    {
    case BUHLMANN_DEBUG_PLAN_ROW_ASCENT:
        return DIVE_PLAN_ROW_ASCENT;
    case BUHLMANN_DEBUG_PLAN_ROW_DECO_STOP:
        return DIVE_PLAN_ROW_DECO_STOP;
    case BUHLMANN_DEBUG_PLAN_ROW_BOTTOM:
    default:
        return DIVE_PLAN_ROW_BOTTOM;
    }
}

static bool plan_calculate_result(void)
{
    buhlmann_debug_plan_result_t algo_result;
    dive_plan_result_snapshot_t snapshot;

    if (!buhlmann_debug_plan_calculate(s_state.depth_m,
                                       s_state.time_min,
                                       s_state.rmv_lpm,
                                       &algo_result))
    {
        submenu_dive_plan_set_result_snapshot(NULL);
        return false;
    }

    (void)memset(&snapshot, 0, sizeof(snapshot));
    snapshot.valid = 1U;
    snapshot.page = 0U;
    snapshot.entry_count = (algo_result.entry_count > 16U) ? 16U : algo_result.entry_count;
    snapshot.total_pages = (uint8_t)((snapshot.entry_count + PLAN_ROWS_PER_PAGE - 1U) / PLAN_ROWS_PER_PAGE);
    if (snapshot.total_pages == 0U)
    {
        snapshot.total_pages = 1U;
    }
    snapshot.total_runtime_min = algo_result.total_runtime_min;
    snapshot.total_deco_min = algo_result.total_deco_min;
    snapshot.total_gas_l = algo_result.total_gas_l;
    snapshot.cns_pct = algo_result.cns_pct;
    snapshot.otu = algo_result.otu;

    for (uint8_t i = 0U; i < snapshot.entry_count; i++)
    {
        snapshot.rows[i].type = plan_row_type_from_algo(algo_result.entries[i].type);
        snapshot.rows[i].depth_m = algo_result.entries[i].depth_m;
        snapshot.rows[i].time_min = algo_result.entries[i].time_min;
        snapshot.rows[i].run_min = algo_result.entries[i].run_min;
        snapshot.rows[i].o2_pct = algo_result.entries[i].o2_pct;
        snapshot.rows[i].he_pct = algo_result.entries[i].he_pct;
        snapshot.rows[i].gas_l = algo_result.entries[i].gas_l;
    }

    submenu_dive_plan_set_result_snapshot(&snapshot);
    return true;
}
#endif

void submenu_dive_plan_set_result_snapshot(const dive_plan_result_snapshot_t *snapshot)
{
    if (snapshot == NULL)
    {
        (void)memset(&s_state.result, 0, sizeof(s_state.result));
        s_state.result_page = 0U;
        return;
    }

    s_state.result = *snapshot;
    if (s_state.result.page >= plan_result_total_pages())
    {
        s_state.result.page = 0U;
    }
    s_state.result_page = s_state.result.page;
}

void submenu_dive_plan_get_snapshot(submenu_dive_plan_snapshot_t *out_snapshot)
{
    ui_vm_dive_plan_inputs_t input_vm;

    if (out_snapshot == NULL)
    {
        return;
    }

    (void)memset(out_snapshot, 0, sizeof(*out_snapshot));

    plan_ensure_defaults();
    ui_vm_dive_plan_inputs_update(&input_vm);

    out_snapshot->page = s_state.page;
    out_snapshot->depth_m = s_state.depth_m;
    out_snapshot->time_min = s_state.time_min;
    out_snapshot->rmv_lpm = s_state.rmv_lpm;
    out_snapshot->gf_low = plan_gf_low();
    out_snapshot->gf_high = plan_gf_high();
    out_snapshot->last_stop_depth_m = plan_last_deco_depth();
    out_snapshot->header_gas_o2 = input_vm.header_gas_o2;
    (void)snprintf(out_snapshot->gas_summary,
                   sizeof(out_snapshot->gas_summary),
                   "%s",
                   input_vm.gas_summary);

    out_snapshot->result_page_index = s_state.result_page;
    out_snapshot->result_total_pages = plan_result_total_pages();
    out_snapshot->result_entry_count = (s_state.result.valid != 0U) ? s_state.result.entry_count : 0U;
    out_snapshot->total_runtime_min = (s_state.result.valid != 0U) ? s_state.result.total_runtime_min : 0U;
    out_snapshot->total_deco_min = (s_state.result.valid != 0U) ? s_state.result.total_deco_min : 0U;
    out_snapshot->total_gas_l = (s_state.result.valid != 0U) ? s_state.result.total_gas_l : 0U;
    out_snapshot->cns_pct = (s_state.result.valid != 0U) ? s_state.result.cns_pct : 0U;
    out_snapshot->otu = (s_state.result.valid != 0U) ? s_state.result.otu : 0U;

    if (s_state.result.valid != 0U)
    {
        uint8_t row_count = s_state.result.entry_count;

        if (row_count > 16U)
        {
            row_count = 16U;
        }
        for (uint8_t i = 0U; i < row_count; i++)
        {
            out_snapshot->rows[i] = s_state.result.rows[i];
        }
    }
}

void submenu_dive_plan_set_page(dive_plan_page_t page)
{
    s_state.page = page;
}

dive_plan_page_t submenu_dive_plan_get_page(void)
{
    return s_state.page;
}

uint8_t submenu_dive_plan_get_result_page_index(void)
{
    return s_state.result_page;
}

uint8_t submenu_dive_plan_get_result_total_pages(void)
{
    return plan_result_total_pages();
}

void submenu_dive_plan_set_depth_m(float value)
{
    plan_ensure_defaults();
    s_state.depth_m = (float)plan_round_u16(value);
    s_state.depth_m = clamp_float(s_state.depth_m, 3.0f, 120.0f);
    s_state.page = DIVE_PLAN_PAGE_READY;
}

void submenu_dive_plan_set_time_min(float value)
{
    uint16_t rounded_value;

    plan_ensure_defaults();
    rounded_value = plan_round_u16(value);
    if (rounded_value < 1U)
    {
        rounded_value = 1U;
    }
    if (rounded_value > 300U)
    {
        rounded_value = 300U;
    }
    s_state.time_min = rounded_value;
    s_state.page = DIVE_PLAN_PAGE_READY;
}

void submenu_dive_plan_set_rmv_lpm(float value)
{
    plan_ensure_defaults();
    s_state.rmv_lpm = (float)plan_round_u16(value);
    s_state.rmv_lpm = clamp_float(s_state.rmv_lpm, 5.0f, 50.0f);
    s_state.page = DIVE_PLAN_PAGE_READY;
}

bool submenu_dive_plan_handle_rotate(int8_t dir)
{
    plan_ensure_defaults();
    switch (s_state.page)
    {
    case DIVE_PLAN_PAGE_DEPTH:
        s_state.depth_m += (float)dir;
        if (s_state.depth_m < 3.0f) s_state.depth_m = 3.0f;
        if (s_state.depth_m > 120.0f) s_state.depth_m = 120.0f;
        return true;
    case DIVE_PLAN_PAGE_TIME:
    {
        int next = (int)s_state.time_min + (int)dir;
        if (next < 1) next = 1;
        if (next > 300) next = 300;
        s_state.time_min = (uint16_t)next;
        return true;
    }
    case DIVE_PLAN_PAGE_RMV:
        s_state.rmv_lpm += (float)dir;
        if (s_state.rmv_lpm < 5.0f) s_state.rmv_lpm = 5.0f;
        if (s_state.rmv_lpm > 50.0f) s_state.rmv_lpm = 50.0f;
        return true;
    default:
        return false;
    }
}

bool submenu_dive_plan_is_result_page(void)
{
    return s_state.page == DIVE_PLAN_PAGE_RESULT;
}

bool submenu_dive_plan_handle_action(menu_item_id_t item_id,
                                     bool *out_close_submenu,
                                     uint8_t *out_keep_index)
{
    if (out_close_submenu != NULL)
    {
        *out_close_submenu = false;
    }
    if (out_keep_index != NULL)
    {
        *out_keep_index = 0U;
    }
    if (item_id == MENU_ITEM_DIVE_PLAN_EXIT)
    {
        if (out_close_submenu != NULL)
        {
            *out_close_submenu = true;
        }
        return true;
    }

    if (item_id == MENU_ITEM_DIVE_PLAN_NEXT)
    {
        switch (s_state.page)
        {
        case DIVE_PLAN_PAGE_DEPTH:
            s_state.page = DIVE_PLAN_PAGE_TIME;
            break;
        case DIVE_PLAN_PAGE_TIME:
            s_state.page = DIVE_PLAN_PAGE_RMV;
            break;
        case DIVE_PLAN_PAGE_RMV:
            s_state.page = DIVE_PLAN_PAGE_READY;
            break;
        case DIVE_PLAN_PAGE_RESULT:
        case DIVE_PLAN_PAGE_ERROR:
            s_state.page = DIVE_PLAN_PAGE_READY;
            break;
        default:
            break;
        }
        if (out_keep_index != NULL)
        {
            *out_keep_index = 1U;
        }
        return true;
    }

    if (item_id == MENU_ITEM_DIVE_PLAN_MORE)
    {
        uint8_t total_pages = plan_result_total_pages();
        if (s_state.result_page + 1U < total_pages)
        {
            s_state.result_page++;
        }
        if (out_keep_index != NULL)
        {
            *out_keep_index = 1U;
        }
        return true;
    }

    if (item_id == MENU_ITEM_DIVE_PLAN_PLAN)
    {
        plan_ensure_defaults();
        s_state.result_page = 0U;
#ifdef PC_SIMULATOR
        s_state.page = plan_calculate_result() ? DIVE_PLAN_PAGE_RESULT : DIVE_PLAN_PAGE_ERROR;
#else
        s_state.page = DIVE_PLAN_PAGE_ERROR;
#endif
        if (out_keep_index != NULL)
        {
            *out_keep_index = 1U;
        }
        return true;
    }

    return false;
}

void submenu_dive_plan_reset(void)
{
    s_state.page = DIVE_PLAN_PAGE_DEPTH;
    s_state.defaults_loaded = false;
    s_state.depth_m = 30.0f;
    s_state.time_min = 20U;
    s_state.rmv_lpm = 14.0f;
    s_state.result_page = 0U;
    (void)memset(&s_state.result, 0, sizeof(s_state.result));
}
