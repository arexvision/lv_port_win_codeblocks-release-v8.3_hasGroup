#include "ui_vm_system_view.h"

#include "../data.h"
#include "../callbacks.h"

#include <stdio.h>
#include <string.h>

void ui_vm_submenu_view_update(ui_vm_submenu_view_t *vm)
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));
    vm->light_power_on = bus_get_light_power() ? 1U : 0U;
}

void ui_vm_brightness_update(ui_vm_brightness_t *vm)
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));
    vm->brightness_level = bus_get_brightness();
}

void ui_vm_left_aux_update(ui_vm_left_aux_t *vm)
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));

    if (bus_get_bat_temperature_valid())
    {
        (void)snprintf(vm->battery_temp_text,
                       sizeof(vm->battery_temp_text),
                       "%.1fC",
                       (double)bus_get_bat_temperature());
    }
    else
    {
        (void)snprintf(vm->battery_temp_text, sizeof(vm->battery_temp_text), "%s", "--");
    }

    if (bus_get_prj_temperature_valid())
    {
        (void)snprintf(vm->project_temp_text,
                       sizeof(vm->project_temp_text),
                       "%.1fC",
                       (double)bus_get_prj_temperature());
    }
    else
    {
        (void)snprintf(vm->project_temp_text, sizeof(vm->project_temp_text), "%s", "--");
    }
}
