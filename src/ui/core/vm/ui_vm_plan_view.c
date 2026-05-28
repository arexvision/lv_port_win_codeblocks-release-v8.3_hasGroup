/*
 * 文件: src/app_ui/ui/core/vm/ui_vm_plan_view.c
 * 作用: 该文件属于 ViewModel 子模块，负责把底层运行数据整理为界面可直接消费的展示数据结构。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "ui_vm_plan_view.h"

#include "../data.h"
#include "../../views/submenu_dive_plan_state.h"

#include <stdio.h>
#include <string.h>

static void vm_format_gas_summary_text(char *out, size_t out_size)
{
    uint8_t gas_count;
    size_t used = 0U;
    uint8_t valid_count = 0U;
    int written;

    if ((out == NULL) || (out_size == 0U))
    {
        return;
    }

    gas_count = bus_get_gas_slot_count();
    if (gas_count > 3U)
    {
        gas_count = 3U;
    }
    if (gas_count == 0U)
    {
        (void)snprintf(out, out_size, "%s", "GAS: AIR");
        return;
    }

    written = snprintf(out, out_size, "%s", "GAS:");
    if (written > 0)
    {
        used = (size_t)written;
    }

    for (uint8_t i = 0U; (i < gas_count) && (valid_count < 3U) && (used + 1U < out_size); i++)
    {
        uint8_t o2 = bus_get_gas_slot_o2_pct(i);
        uint8_t he = bus_get_gas_slot_he_pct(i);

        if ((o2 == 0U) || (o2 > 100U) || (he > 100U) || (((uint16_t)o2 + (uint16_t)he) > 100U))
        {
            continue;
        }

        written = snprintf(out + used,
                           out_size - used,
                           " %u/%02u",
                           (unsigned)o2,
                           (unsigned)he);
        if (written <= 0)
        {
            break;
        }
        used += (size_t)written;
        valid_count++;
    }

    if (valid_count == 0U)
    {
        (void)snprintf(out, out_size, "%s", "GAS: AIR");
    }
}

static const char *vm_plan_row_time_text(const dive_plan_row_t *row, char *buf, size_t buf_size)
{
    if (row == NULL)
    {
        return "";
    }

    if (row->type == DIVE_PLAN_ROW_BOTTOM)
    {
        return "bot";
    }
    if (row->type == DIVE_PLAN_ROW_ASCENT)
    {
        return "asc";
    }

    (void)snprintf(buf, buf_size, "%u", (unsigned)row->time_min);
    return buf;
}

void ui_vm_dive_plan_inputs_update(ui_vm_dive_plan_inputs_t *vm)
{
    uint8_t gas_count;

    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));

    vm->depth_m = (bus_get_max_depth() >= 3.0f) ? bus_get_max_depth() : 30.0f;
    vm->time_min = (bus_get_dive_time_s() > 0U)
                   ? (uint16_t)((bus_get_dive_time_s() + 59U) / 60U)
                   : 20U;
    if (vm->time_min < 1U)
    {
        vm->time_min = 1U;
    }
    vm->rmv_lpm = 14.0f;

    gas_count = bus_get_gas_slot_count();
    for (uint8_t i = 1U; i < gas_count; i++)
    {
        uint8_t o2 = bus_get_gas_slot_o2_pct(i);
        uint8_t he = bus_get_gas_slot_he_pct(i);
        if ((o2 > 0U) && (o2 <= 100U) && (he <= 100U) && (((uint16_t)o2 + (uint16_t)he) <= 100U))
        {
            vm->header_gas_o2 = o2;
            break;
        }
    }

    vm_format_gas_summary_text(vm->gas_summary, sizeof(vm->gas_summary));
}

void ui_vm_dive_plan_view_update(ui_vm_dive_plan_view_t *vm)
{
    submenu_dive_plan_snapshot_t snapshot;
    uint16_t input_min = 3U;
    uint16_t input_max = 120U;
    uint16_t input_value = 30U;
    uint8_t page;

    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));
    submenu_dive_plan_get_snapshot(&snapshot);
    page = (uint8_t)snapshot.page;

    vm->page = page;
    vm->depth_m = snapshot.depth_m;
    vm->time_min = snapshot.time_min;
    vm->rmv_lpm = snapshot.rmv_lpm;
    vm->header_gas_o2 = snapshot.header_gas_o2;
    vm->gf_low = snapshot.gf_low;
    vm->gf_high = snapshot.gf_high;
    vm->last_stop_depth_m = snapshot.last_stop_depth_m;
    vm->result_page_index = snapshot.result_page_index;
    vm->result_total_pages = snapshot.result_total_pages;
    vm->result_entry_count = snapshot.result_entry_count;

    (void)snprintf(vm->depth_value, sizeof(vm->depth_value), "%u", (unsigned)(snapshot.depth_m + 0.5f));
    (void)snprintf(vm->time_value, sizeof(vm->time_value), "%u", (unsigned)snapshot.time_min);
    (void)snprintf(vm->rmv_value, sizeof(vm->rmv_value), "%u", (unsigned)(snapshot.rmv_lpm + 0.5f));
    (void)snprintf(vm->ready_gf_text,
                   sizeof(vm->ready_gf_text),
                   "GF:              %u/%u",
                   (unsigned)vm->gf_low,
                   (unsigned)vm->gf_high);
    (void)snprintf(vm->ready_last_stop_text,
                   sizeof(vm->ready_last_stop_text),
                   "Last Stop:       %um",
                   (unsigned)vm->last_stop_depth_m);
    (void)snprintf(vm->ready_start_cns_text,
                   sizeof(vm->ready_start_cns_text),
                   "%s",
                   "Start CNS:       0%");
    (void)snprintf(vm->gas_summary, sizeof(vm->gas_summary), "%s", snapshot.gas_summary);
    (void)snprintf(vm->result_runtime_text,
                   sizeof(vm->result_runtime_text),
                   "Runtime: %umin",
                   (unsigned)snapshot.total_runtime_min);
    (void)snprintf(vm->result_deco_text,
                   sizeof(vm->result_deco_text),
                   "Total Deco: %umin",
                   (unsigned)snapshot.total_deco_min);
    (void)snprintf(vm->result_gas_text,
                   sizeof(vm->result_gas_text),
                   "Gas: %uL",
                   (unsigned)snapshot.total_gas_l);
    (void)snprintf(vm->result_cns_text,
                   sizeof(vm->result_cns_text),
                   "CNS: %u%%",
                   (unsigned)snapshot.cns_pct);
    (void)snprintf(vm->result_otu_text,
                   sizeof(vm->result_otu_text),
                   "OTU: %u",
                   (unsigned)snapshot.otu);
    (void)snprintf(vm->error_title, sizeof(vm->error_title), "%s", "Plan Failed");
    (void)snprintf(vm->error_hint,
                   sizeof(vm->error_hint),
                   "%s",
                   "Check depth, time, RMV and gas setup");
    (void)snprintf(vm->result_page_text,
                   sizeof(vm->result_page_text),
                   "Page %u/%u",
                   (unsigned)(vm->result_page_index + 1U),
                   (unsigned)vm->result_total_pages);

    if (page == (uint8_t)DIVE_PLAN_PAGE_TIME)
    {
        input_min = 1U;
        input_max = 300U;
        input_value = snapshot.time_min;
        (void)snprintf(vm->input_prompt, sizeof(vm->input_prompt), "%s", "Enter Bottom Time");
        (void)snprintf(vm->input_unit, sizeof(vm->input_unit), "%s", "in minutes");
    }
    else if (page == (uint8_t)DIVE_PLAN_PAGE_RMV)
    {
        input_min = 5U;
        input_max = 50U;
        input_value = (uint16_t)(snapshot.rmv_lpm + 0.5f);
        (void)snprintf(vm->input_prompt, sizeof(vm->input_prompt), "%s", "Enter RMV");
        (void)snprintf(vm->input_unit, sizeof(vm->input_unit), "%s", "in Liters/min");
    }
    else
    {
        input_value = (uint16_t)(snapshot.depth_m + 0.5f);
        (void)snprintf(vm->input_prompt, sizeof(vm->input_prompt), "%s", "Enter Bottom Depth");
        (void)snprintf(vm->input_unit, sizeof(vm->input_unit), "%s", "in meters");
    }

    (void)snprintf(vm->input_min_text, sizeof(vm->input_min_text), "MIN: %u", (unsigned)input_min);
    (void)snprintf(vm->input_max_text, sizeof(vm->input_max_text), "MAX: %u", (unsigned)input_max);
    (void)input_value;

    for (uint8_t i = 0U; i < 8U; i++)
    {
        const uint8_t row_index = (uint8_t)(vm->result_page_index * 8U + i);
        dive_plan_row_t row;
        char time_text[8];

        if (row_index >= snapshot.result_entry_count)
        {
            break;
        }
        row = snapshot.rows[row_index];

        vm->rows[i].valid = 1U;
        (void)snprintf(vm->rows[i].depth_text, sizeof(vm->rows[i].depth_text), "%dm", (int)row.depth_m);
        (void)snprintf(vm->rows[i].time_text,
                       sizeof(vm->rows[i].time_text),
                       "%s",
                       vm_plan_row_time_text(&row, time_text, sizeof(time_text)));
        (void)snprintf(vm->rows[i].run_text, sizeof(vm->rows[i].run_text), "%u", (unsigned)row.run_min);
        (void)snprintf(vm->rows[i].gas_text,
                       sizeof(vm->rows[i].gas_text),
                       "%02u/%02u",
                       (unsigned)row.o2_pct,
                       (unsigned)row.he_pct);
        (void)snprintf(vm->rows[i].qty_text, sizeof(vm->rows[i].qty_text), "%u", (unsigned)row.gas_l);
    }
}
