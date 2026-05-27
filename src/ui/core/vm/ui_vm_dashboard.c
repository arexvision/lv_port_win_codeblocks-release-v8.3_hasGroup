#include "ui_vm_dashboard.h"

#include "../data.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint8_t vm_clamp_u8(uint8_t value, uint8_t max_value)
{
    return (value > max_value) ? max_value : value;
}

static uint8_t vm_clamp_battery(float pct)
{
    if (pct <= 0.0f)
    {
        return 0U;
    }
    if (pct >= 100.0f)
    {
        return 100U;
    }
    return (uint8_t)pct;
}

static void vm_split_decimal_1(float value, int16_t *int_part, uint8_t *dec_part)
{
    int16_t local_int;
    uint8_t local_dec;

    if ((int_part == NULL) || (dec_part == NULL))
    {
        return;
    }

    local_int = (int16_t)value;
    local_dec = (uint8_t)(fabsf(value - (float)local_int) * 10.0f + 0.5f);
    if (local_dec > 9U)
    {
        local_dec = 9U;
    }

    *int_part = local_int;
    *dec_part = local_dec;
}

void ui_vm_compass_update(ui_vm_compass_t *vm,
                          const sensor_data_t *sensor,
                          const sys_config_t *config)
{
    if (vm == NULL)
    {
        return;
    }

    if ((sensor == NULL) || (config == NULL))
    {
        vm->heading = bus_get_heading();
        vm->heading_target = bus_get_heading_target();
        vm->locked = bus_is_heading_locked() ? 1U : 0U;
        vm->right_canvas_w = (uint16_t)(ui_safe_zone_w_get() - LEFT_ANCHOR_W -
                                        ui_panel_gap_px_get());
        vm->reserved = 0U;
        return;
    }

    vm->heading = sensor->heading;
    vm->heading_target = sensor->heading_target;
    vm->locked = sensor->heading_locked ? 1U : 0U;
    vm->right_canvas_w = (uint16_t)(config->safe_zone_w - LEFT_ANCHOR_W -
                                    (config->gap_u * BASE_U));
}

void ui_vm_gas_update(ui_vm_gas_t *vm,
                      const sensor_data_t *sensor,
                      const sys_config_t *config,
                      ui_state_t state,
                      uint8_t gas_cursor)
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));

    if ((sensor == NULL) || (config == NULL))
    {
        vm->gas_count = bus_get_gas_slot_count();
        vm->active_idx = bus_get_gas_active_idx();
        vm->cursor_idx = gas_cursor;
        vm->edit_mode = (state == UI_EDIT_GAS) ? 1U : 0U;
        vm->right_canvas_w = (uint16_t)(ui_safe_zone_w_get() - LEFT_ANCHOR_W -
                                        ui_panel_gap_px_get());
        vm->gap_y = (uint8_t)ui_menu_gap_px_get();

        if (vm->gas_count == 0U)
        {
            (void)snprintf(vm->hint, sizeof(vm->hint), "%s", "[ NO ACTIVE GAS ]");
            return;
        }

        if (vm->cursor_idx >= vm->gas_count)
        {
            vm->cursor_idx = 0U;
        }

        for (uint8_t i = 0U; i < vm->gas_count; i++)
        {
            const char *name = bus_get_gas_slot_name(i);
            float mod_m = bus_get_gas_slot_mod_m(i);
            float ppo2 = bus_get_gas_slot_ppo2(i);

            (void)snprintf(vm->names[i], sizeof(vm->names[i]), "%s", (name != NULL) ? name : "--");
            if (mod_m > 0.0f)
            {
                (void)snprintf(vm->mod_text[i], sizeof(vm->mod_text[i]), "MOD %.0fm", (double)mod_m);
            }
            else
            {
                (void)snprintf(vm->mod_text[i], sizeof(vm->mod_text[i]), "%s", "MOD --");
            }
            (void)snprintf(vm->ppo2_text[i], sizeof(vm->ppo2_text[i]), "PO2 %.2f", (double)ppo2);
            vm->visible[i] = 1U;
            vm->highlighted[i] = ((i == vm->cursor_idx) ||
                                  ((i == vm->active_idx) && (vm->edit_mode == 0U))) ? 1U : 0U;
        }

        (void)snprintf(vm->hint,
                       sizeof(vm->hint),
                       "%s",
                       (state == UI_EDIT_GAS) ? "[ SCROLL TO SELECT / PRESS TO CONFIRM ]"
                                              : "[ PRESS TO SWITCH GAS ]");
        return;
    }

    vm->gas_count = vm_clamp_u8(sensor->gas_slot_count, GAS_COUNT);
    vm->active_idx = vm_clamp_u8(sensor->gas_active_idx, (uint8_t)(GAS_COUNT - 1U));
    vm->cursor_idx = gas_cursor;
    vm->edit_mode = (state == UI_EDIT_GAS) ? 1U : 0U;
    vm->right_canvas_w = (uint16_t)(config->safe_zone_w - LEFT_ANCHOR_W -
                                    ((uint16_t)config->gap_u * BASE_U));
    vm->gap_y = (uint8_t)(config->gap_menu * BASE_U);

    if (vm->gas_count == 0U)
    {
        (void)snprintf(vm->hint, sizeof(vm->hint), "%s", "[ NO ACTIVE GAS ]");
        return;
    }

    if (vm->cursor_idx >= vm->gas_count)
    {
        vm->cursor_idx = 0U;
    }

    for (uint8_t i = 0U; i < vm->gas_count; i++)
    {
        const char *name = sensor->gas_slot_name[i][0] ? sensor->gas_slot_name[i] : GAS_NAMES[i];
        float mod_m = sensor->gas_slot_mod_m[i];
        float ppo2 = sensor->ppo2[i];

        (void)snprintf(vm->names[i], sizeof(vm->names[i]), "%s", name);
        if (mod_m > 0.0f)
        {
            (void)snprintf(vm->mod_text[i], sizeof(vm->mod_text[i]), "MOD %.0fm", (double)mod_m);
        }
        else
        {
            (void)snprintf(vm->mod_text[i], sizeof(vm->mod_text[i]), "%s", "MOD --");
        }
        (void)snprintf(vm->ppo2_text[i], sizeof(vm->ppo2_text[i]), "PO2 %.2f", (double)ppo2);
        vm->visible[i] = 1U;
        vm->highlighted[i] = ((i == vm->cursor_idx) ||
                              ((i == vm->active_idx) && (vm->edit_mode == 0U))) ? 1U : 0U;
    }

    (void)snprintf(vm->hint,
                   sizeof(vm->hint),
                   "%s",
                   (state == UI_EDIT_GAS) ? "[ SCROLL TO SELECT / PRESS TO CONFIRM ]"
                                          : "[ PRESS TO SWITCH GAS ]");
}

void ui_vm_deco_update(ui_vm_deco_t *vm,
                       const sensor_data_t *sensor,
                       const sys_config_t *config)
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));

    if ((sensor == NULL) || (config == NULL))
    {
        vm->gf_low = (bus_get_gf_low() == 0U) ? 40U : bus_get_gf_low();
        vm->gf_high = (bus_get_gf_high() == 0U) ? 70U : bus_get_gf_high();
        (void)snprintf(vm->gf_setting, sizeof(vm->gf_setting), "%u / %u",
                       (unsigned)vm->gf_low,
                       (unsigned)vm->gf_high);
        (void)snprintf(vm->gf99, sizeof(vm->gf99), "%.0f%%", (double)bus_get_gf99());
        (void)snprintf(vm->surf_gf, sizeof(vm->surf_gf), "%.0f%%", (double)bus_get_surf_gf());
        (void)snprintf(vm->cns, sizeof(vm->cns), "%u%%", (unsigned)bus_get_cns_pct());
        (void)snprintf(vm->otu, sizeof(vm->otu), "%u", (unsigned)bus_get_otu());
        vm->chart_active = ((bus_get_depth() > 0.3f) || (bus_get_dive_time_s() > 0U)) ? 1U : 0U;
        vm->surf_gf_alert = (bus_get_surf_gf() > 100.0f) ? 1U : 0U;
        vm->right_canvas_w = (uint16_t)(ui_safe_zone_w_get() - LEFT_ANCHOR_W -
                                        ui_panel_gap_px_get());
        for (uint8_t i = 0U; i < 16U; i++)
        {
            vm->tissue_pct[i] = bus_get_tissue_pct(i);
        }
        return;
    }

    vm->gf_low = (sensor->gf_low == 0U) ? 40U : sensor->gf_low;
    (void)snprintf(vm->gf_setting, sizeof(vm->gf_setting), "%u / %u",
                   (unsigned)vm->gf_low,
                   (unsigned)(sensor->gf_high ? sensor->gf_high : 85U));
    (void)snprintf(vm->gf99, sizeof(vm->gf99), "%.0f%%", (double)sensor->gf99);
    (void)snprintf(vm->surf_gf, sizeof(vm->surf_gf), "%.0f%%", (double)sensor->surf_gf);
    (void)snprintf(vm->cns, sizeof(vm->cns), "%u%%", (unsigned)sensor->cns_pct);
    (void)snprintf(vm->otu, sizeof(vm->otu), "%u", (unsigned)sensor->otu);
    vm->gf_high = (sensor->gf_high == 0U) ? 70U : sensor->gf_high;
    vm->chart_active = ((sensor->depth > 0.3f) || (sensor->dive_time_s > 0U)) ? 1U : 0U;
    vm->surf_gf_alert = (sensor->surf_gf > 100.0f) ? 1U : 0U;
    vm->right_canvas_w = (uint16_t)(config->safe_zone_w - LEFT_ANCHOR_W -
                                    ((uint16_t)config->gap_u * BASE_U));
    (void)memcpy(vm->tissue_pct, sensor->tissue_pct, sizeof(vm->tissue_pct));
}

void ui_vm_sys_update(ui_vm_sys_t *vm, const sensor_data_t *sensor)
{
    if (vm == NULL)
    {
        return;
    }

    if (sensor == NULL)
    {
        vm->battery_pct = vm_clamp_battery(bus_get_battery_pct());
        vm->temp_int = (int16_t)bus_get_temperature();
        vm->temp_dec = (uint8_t)(fabsf(bus_get_temperature() - (float)vm->temp_int) * 10.0f);
        vm->battery_critical = (vm->battery_pct <= 20U) ? 1U : 0U;
        vm->battery_low = (vm->battery_pct <= 40U) ? 1U : 0U;
        return;
    }

    vm->battery_pct = vm_clamp_battery(sensor->battery_pct);
    vm->temp_int = (int16_t)sensor->temperature_c;
    vm->temp_dec = (uint8_t)(fabsf(sensor->temperature_c - (float)vm->temp_int) * 10.0f);
    vm->battery_critical = (vm->battery_pct <= 20U) ? 1U : 0U;
    vm->battery_low = (vm->battery_pct <= 40U) ? 1U : 0U;
}

void ui_vm_depth_update(ui_vm_depth_t *vm, const sensor_data_t *sensor)
{
    float depth;

    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));

    depth = (sensor == NULL) ? bus_get_depth() : sensor->depth;
    vm_split_decimal_1(depth, &vm->int_part, &vm->dec_part);
    (void)snprintf(vm->text,
                   sizeof(vm->text),
                   "%d.%u",
                   (int)vm->int_part,
                   (unsigned)vm->dec_part);
}

void ui_vm_ndl_stop_update(ui_vm_ndl_stop_t *vm, const sensor_data_t *sensor)
{
    if (vm == NULL)
    {
        return;
    }

    if (sensor == NULL)
    {
        vm->stop_type = bus_get_stop_type();
        vm->ndl = bus_get_ndl();
        vm->ndl_stop_value = bus_get_ndl_stop_value();
        vm->stop_depth_m = bus_get_stop_depth_m();
        vm->stop_time_left_s = bus_get_stop_time_left_s();
        vm->stop_time_total_s = bus_get_stop_time_total_s();
        vm->ndl_bar_pct = bus_get_ndl_bar_pct();
        vm->in_stop_zone = bus_get_in_stop_zone() ? 1U : 0U;
        return;
    }

    vm->stop_type = sensor->stop_type;
    vm->ndl = sensor->ndl;
    vm->ndl_stop_value = sensor->ndl_stop_value;
    vm->stop_depth_m = sensor->stop_depth_m;
    vm->stop_time_left_s = sensor->stop_time_left_s;
    vm->stop_time_total_s = sensor->stop_time_total_s;
    vm->ndl_bar_pct = sensor->ndl_bar_pct;
    vm->in_stop_zone = sensor->in_stop_zone ? 1U : 0U;
}

void ui_vm_ascent_update(ui_vm_ascent_t *vm,
                         float rate)
{
    uint32_t tick_ms;

    if (vm == NULL)
    {
        return;
    }

    tick_ms = lv_tick_get();
    vm->rate = rate;
    vm->is_moving = (fabsf(rate) >= RATE_STILL_THRESHOLD) ? 1U : 0U;
    vm->flash_on = ((tick_ms / 500U) % 2U == 0U) ? 1U : 0U;
    vm->direction = 0;
    if (rate > 0.0f)
    {
        vm->direction = 1;
    }
    else if (rate < 0.0f)
    {
        vm->direction = -1;
    }
    else
    {
    }
}

void ui_vm_menu_layout_update(ui_vm_menu_layout_t *vm,
                              const sys_config_t *config)
{
    if (vm == NULL)
    {
        return;
    }

    if (config == NULL)
    {
        vm->right_canvas_w = (uint16_t)(ui_safe_zone_w_get() - LEFT_ANCHOR_W -
                                        ui_panel_gap_px_get());
        vm->item_h_px = ui_menu_item_h_px_get();
        vm->gap_y_px = ui_menu_gap_px_get();
        return;
    }

    vm->right_canvas_w = (uint16_t)(config->safe_zone_w - LEFT_ANCHOR_W -
                                    ((uint16_t)config->gap_u * BASE_U));
    vm->item_h_px = (uint16_t)config->h_menu_item * BASE_U;
    vm->gap_y_px = (uint16_t)config->gap_menu * BASE_U;
}

void ui_vm_value_text_update(ui_vm_value_text_t *vm,
                             comp_id_t w_id,
                             uint8_t pod_index)
{
    if (vm == NULL)
    {
        return;
    }

    (void)snprintf(vm->text, sizeof(vm->text), "%s", "--");

    switch (w_id)
    {
    case COMP_DEPTH_1606:
    case COMP_DEPTH_1612:
    {
        ui_vm_depth_t depth_vm;
        ui_vm_depth_update(&depth_vm, NULL);
        (void)snprintf(vm->text, sizeof(vm->text), "%s", depth_vm.text);
        break;
    }
    case COMP_DIVE_TIME_1606:
        (void)snprintf(vm->text,
                       sizeof(vm->text),
                       "%02u:%02u",
                       (unsigned)(bus_get_dive_time_s() / 60U),
                       (unsigned)(bus_get_dive_time_s() % 60U));
        break;
    case COMP_GAS_1606:
        (void)snprintf(vm->text,
                       sizeof(vm->text),
                       "%s",
                       bus_get_gas_slot_name(bus_get_gas_active_idx()));
        break;
    case COMP_SYS_1606:
    case COMP_TIME_1606:
        (void)snprintf(vm->text,
                       sizeof(vm->text),
                       "%02u:%02u",
                       (unsigned)bus_get_sys_time_h(),
                       (unsigned)bus_get_sys_time_m());
        break;
    case COMP_TTS_0806:
        (void)snprintf(vm->text, sizeof(vm->text), "%d", (int)bus_get_tts());
        break;
    case COMP_ASCENT_0806:
    case COMP_ASCENT_0812:
        (void)snprintf(vm->text, sizeof(vm->text), "%+.1f", (double)bus_get_ascent_rate());
        break;
    case COMP_COMPASS_1612:
    case COMP_HEADING_0806:
        (void)snprintf(vm->text, sizeof(vm->text), "%03u", (unsigned)bus_get_heading());
        break;
    case COMP_STOP_DEPTH_0806:
        (void)snprintf(vm->text, sizeof(vm->text), "%.1f", (double)bus_get_stop_depth_m());
        break;
    case COMP_STOP_TIME_1606:
    {
        ui_vm_ndl_stop_t ndl_vm;
        ui_vm_ndl_stop_update(&ndl_vm, NULL);
        (void)snprintf(vm->text,
                       sizeof(vm->text),
                       "%02u:%02u",
                       (unsigned)(ndl_vm.stop_time_left_s / 60U),
                       (unsigned)(ndl_vm.stop_time_left_s % 60U));
        break;
    }
    case COMP_PPO2_0806:
        (void)snprintf(vm->text,
                       sizeof(vm->text),
                       "%.2f",
                       (double)bus_get_gas_slot_ppo2(bus_get_gas_active_idx()));
        break;
    case COMP_SURF_GF_0806:
        (void)snprintf(vm->text, sizeof(vm->text), "%.2f", (double)bus_get_surf_gf());
        break;
    case COMP_GF99_0806:
        (void)snprintf(vm->text, sizeof(vm->text), "%.0f", (double)bus_get_gf99());
        break;
    case COMP_CNS_0806:
        (void)snprintf(vm->text, sizeof(vm->text), "%u", (unsigned)bus_get_cns_pct());
        break;
    case COMP_OTU_0806:
        (void)snprintf(vm->text, sizeof(vm->text), "%u", (unsigned)bus_get_otu());
        break;
    case COMP_GF_0806:
        (void)snprintf(vm->text,
                       sizeof(vm->text),
                       "%u/%u",
                       (unsigned)bus_get_gf_low(),
                       (unsigned)bus_get_gf_high());
        break;
    case COMP_MOD_0806:
        (void)snprintf(vm->text, sizeof(vm->text), "%.1f", (double)bus_get_mod_m());
        break;
    case COMP_CEILING_0806:
        (void)snprintf(vm->text, sizeof(vm->text), "%.1f", (double)bus_get_ceiling_m());
        break;
    case COMP_GAS_MIX_1606:
        (void)snprintf(vm->text,
                       sizeof(vm->text),
                       "%u/%u",
                       (unsigned)bus_get_gas_mix_o2(),
                       (unsigned)bus_get_gas_mix_he());
        break;
    case COMP_GAS_DENS_0806:
        (void)snprintf(vm->text, sizeof(vm->text), "%.2f", (double)bus_get_gas_density());
        break;
    case COMP_FIO2_0806:
        (void)snprintf(vm->text, sizeof(vm->text), "%.0f%%", (double)bus_get_fio2_pct());
        break;
    case COMP_POD_0806:
        (void)snprintf(vm->text,
                       sizeof(vm->text),
                       "%.0f",
                       (double)bus_get_pod_bar((pod_index > 1U) ? 1U : 0U));
        break;
    case COMP_DEPTH_MAX_0806:
        (void)snprintf(vm->text, sizeof(vm->text), "%.1f", (double)bus_get_max_depth());
        break;
    case COMP_DEPTH_AVG_0806:
        (void)snprintf(vm->text, sizeof(vm->text), "%.1f", (double)bus_get_avg_depth());
        break;
    case COMP_TEMP_0806:
    {
        ui_vm_sys_t sys_vm;
        ui_vm_sys_update(&sys_vm, NULL);
        (void)snprintf(vm->text,
                       sizeof(vm->text),
                       "%d.%u",
                       (int)sys_vm.temp_int,
                       (unsigned)sys_vm.temp_dec);
        break;
    }
    case COMP_BATTERY_0806:
    {
        ui_vm_sys_t sys_vm;
        ui_vm_sys_update(&sys_vm, NULL);
        (void)snprintf(vm->text, sizeof(vm->text), "%u%%", (unsigned)sys_vm.battery_pct);
        break;
    }
    case COMP_TEMP_MIN_0806:
        (void)snprintf(vm->text, sizeof(vm->text), "%.1f", (double)bus_get_min_temp());
        break;
    case COMP_TEMP_AVG_0806:
        (void)snprintf(vm->text, sizeof(vm->text), "%.1f", (double)bus_get_avg_temp());
        break;
    default:
        break;
    }
}
