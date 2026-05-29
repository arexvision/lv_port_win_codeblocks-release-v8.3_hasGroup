/*
 * 文件: src/app_ui/ui/core/vm/ui_vm_info.c
 * 作用: 该文件属于 ViewModel 子模块，负责把底层运行数据整理为界面可直接消费的展示数据结构。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "ui_vm_info.h"

#include "../data.h"

#include <stdio.h>
#include <string.h>

static void vm_format_duration(char *out, size_t out_size, uint32_t total_s)
{
    uint32_t h;
    uint32_t m;
    uint32_t s;

    if ((out == NULL) || (out_size == 0U))
    {
        return;
    }

    h = total_s / 3600U;
    m = (total_s % 3600U) / 60U;
    s = total_s % 60U;

    if (h > 0U)
    {
        (void)snprintf(out, out_size, "%02u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);
    }
    else
    {
        (void)snprintf(out, out_size, "%02u:%02u", (unsigned)m, (unsigned)s);
    }
}

static void vm_format_pressure(char *out, size_t out_size, const char *label, float bar)
{
    if ((out == NULL) || (out_size == 0U))
    {
        return;
    }

    if (bar <= 0.1f)
    {
        (void)snprintf(out, out_size, "%s: -- BAR", (label != NULL) ? label : "--");
    }
    else
    {
        (void)snprintf(out, out_size, "%s: %.0f BAR", (label != NULL) ? label : "--", (double)bar);
    }
}

static uint8_t vm_max_tissue_gf_pct(void)
{
    uint8_t max_pct = 0U;

    for (uint8_t i = 0U; i < 16U; i++)
    {
        uint8_t tissue_pct = bus_get_tissue_gf_pct(i);
        if (tissue_pct > max_pct)
        {
            max_pct = tissue_pct;
        }
    }

    return max_pct;
}

void ui_vm_info_page_update(ui_vm_info_page_t *vm, uint8_t page_index)
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));

    switch (page_index)
    {
    case 0U:
    {
        char dive_time[12];
        char surface_time[12];

        vm_format_duration(dive_time, sizeof(dive_time), bus_get_dive_time_s());
        vm_format_duration(surface_time, sizeof(surface_time), bus_get_surface_time_s());
        (void)snprintf(vm->lines[0], sizeof(vm->lines[0]), "MAX DEPTH: %.1fm", (double)bus_get_max_depth());
        (void)snprintf(vm->lines[1], sizeof(vm->lines[1]), "AVG DEPTH: %.1fm", (double)bus_get_avg_depth());
        (void)snprintf(vm->lines[2], sizeof(vm->lines[2]), "DIVE TIME: %s", dive_time);
        (void)snprintf(vm->lines[3], sizeof(vm->lines[3]), "SURFACE: %s", surface_time);
        vm->count = 4U;
        break;
    }
    case 2U:
    {
        uint8_t gf_low = bus_get_gf_low();
        uint8_t gf_high = bus_get_gf_high();

        (void)snprintf(vm->lines[0],
                       sizeof(vm->lines[0]),
                       "GF: %u/%u",
                       (unsigned)(gf_low != 0U ? gf_low : 30U),
                       (unsigned)(gf_high != 0U ? gf_high : 70U));
        (void)snprintf(vm->lines[1], sizeof(vm->lines[1]), "GF99: %.0f%%", (double)bus_get_gf99());
        (void)snprintf(vm->lines[2], sizeof(vm->lines[2]), "SURF GF: %.0f%%", (double)bus_get_surf_gf());
        (void)snprintf(vm->lines[3], sizeof(vm->lines[3]), "TISSUE: %u%%", (unsigned)vm_max_tissue_gf_pct());
        (void)snprintf(vm->lines[4], sizeof(vm->lines[4]), "CNS: %u%%", (unsigned)bus_get_cns_pct());
        (void)snprintf(vm->lines[5], sizeof(vm->lines[5]), "OTU: %u", (unsigned)bus_get_otu());
        vm->count = 6U;
        break;
    }
    case 3U:
    {
        uint8_t active_idx = bus_get_gas_active_idx();
        uint8_t gas_count = bus_get_gas_slot_count();

        if ((gas_count == 0U) || (active_idx >= gas_count))
        {
            active_idx = 0U;
        }

        (void)snprintf(vm->lines[0],
                       sizeof(vm->lines[0]),
                       "ACTIVE: G%u %s",
                       (unsigned)(active_idx + 1U),
                       (bus_get_gas_slot_name(active_idx) != NULL) ? bus_get_gas_slot_name(active_idx) : "--");
        (void)snprintf(vm->lines[1],
                       sizeof(vm->lines[1]),
                       "MIX: O2 %u%% HE %u%%",
                       (unsigned)bus_get_gas_mix_o2(),
                       (unsigned)bus_get_gas_mix_he());
        (void)snprintf(vm->lines[2],
                       sizeof(vm->lines[2]),
                       "MOD: %.0fm",
                       (double)(((active_idx < GAS_COUNT) && (bus_get_gas_slot_mod_m(active_idx) > 0.0f))
                                    ? bus_get_gas_slot_mod_m(active_idx)
                                    : bus_get_mod_m()));
        (void)snprintf(vm->lines[3],
                       sizeof(vm->lines[3]),
                       "PPO2: %.2f",
                       (double)((active_idx < GAS_COUNT) ? bus_get_gas_slot_ppo2(active_idx) : 0.0f));
        (void)snprintf(vm->lines[4], sizeof(vm->lines[4]), "DENS: %.1fg/L", (double)bus_get_gas_density());
        vm->count = 5U;
        break;
    }
    case 4U:
    {
        float battery_pct = bus_get_battery_pct();

        if (battery_pct < 0.0f)
        {
            battery_pct = 0.0f;
        }
        else if (battery_pct > 100.0f)
        {
            battery_pct = 100.0f;
        }
        else
        {
        }

        vm_format_pressure(vm->lines[0], sizeof(vm->lines[0]), "POD 1", bus_get_pod1_bar());
        vm_format_pressure(vm->lines[1], sizeof(vm->lines[1]), "POD 2", bus_get_pod2_bar());
        (void)snprintf(vm->lines[2], sizeof(vm->lines[2]), "BATTERY: %.0f%%", (double)battery_pct);
        (void)snprintf(vm->lines[3], sizeof(vm->lines[3]), "TEMP: %.1fC", (double)bus_get_temperature());

        if (bus_get_bat_temperature_valid() && bus_get_prj_temperature_valid())
        {
            (void)snprintf(vm->lines[4],
                           sizeof(vm->lines[4]),
                           "BAT/PRJ: %.1f/%.1fC",
                           (double)bus_get_bat_temperature(),
                           (double)bus_get_prj_temperature());
        }
        else if (bus_get_bat_temperature_valid())
        {
            (void)snprintf(vm->lines[4],
                           sizeof(vm->lines[4]),
                           "BAT TEMP: %.1fC",
                           (double)bus_get_bat_temperature());
        }
        else if (bus_get_prj_temperature_valid())
        {
            (void)snprintf(vm->lines[4],
                           sizeof(vm->lines[4]),
                           "PRJ TEMP: %.1fC",
                           (double)bus_get_prj_temperature());
        }
        else
        {
            (void)snprintf(vm->lines[4], sizeof(vm->lines[4]), "%s", "BAT/PRJ: --/--C");
        }
        vm->count = 5U;
        break;
    }
    default:
        break;
    }
}
