/*
 * 文件: src/app_ui/ui/core/vm/ui_vm_plan_chart.c
 * 作用: 该文件属于 ViewModel 子模块，负责把底层运行数据整理为界面可直接消费的展示数据结构。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "ui_vm_plan_chart.h"

#include "../data.h"
#include "../ui_defs.h"

#include <math.h>
#include <string.h>

static float vm_plan_max_log_depth(uint8_t count, const dive_pt_t *points, float current_depth_m)
{
    float max_depth = current_depth_m;

    for (uint8_t i = 0U; i < count; i++)
    {
        if (points[i].depth_m > max_depth)
        {
            max_depth = points[i].depth_m;
        }
    }

    return max_depth;
}

static float vm_plan_predicted_total_time(float current_time_s,
                                          float current_depth_m,
                                          uint8_t stop_count,
                                          const deco_stop_t *stops)
{
    float predicted_t_sec = current_time_s;
    float sim_d = current_depth_m;

    for (uint8_t i = 0U; i < stop_count; i++)
    {
        float asc_t = (sim_d > stops[i].depth_m) ? (sim_d - stops[i].depth_m) * 6.0f : 0.0f;
        predicted_t_sec += asc_t;
        predicted_t_sec += stops[i].stay_min * 60.0f;
        sim_d = stops[i].depth_m;
    }

    if (sim_d > 0.0f)
    {
        predicted_t_sec += sim_d * 6.0f;
    }

    return predicted_t_sec;
}

static float vm_plan_depth_axis(float max_log_d)
{
    float axis = 60.0f;

    if (max_log_d >= axis * 0.9f)
    {
        axis = ceilf((max_log_d + 15.0f) / 20.0f) * 20.0f;
    }

    return axis;
}

static float vm_plan_time_axis(float current_time_s, float predicted_t_sec)
{
    float target_max_t_sec = fmaxf(current_time_s, predicted_t_sec) * 1.05f;
    float axis = 20.0f;

    if (target_max_t_sec < 20.0f)
    {
        target_max_t_sec = 20.0f;
    }

    if (target_max_t_sec > (float)PLAN_TRACK_HOUR_MODE_THRESHOLD_S)
    {
        axis = ceilf(target_max_t_sec / 3600.0f) * 3600.0f;
    }
    else if (target_max_t_sec > 60.0f)
    {
        axis = ceilf(target_max_t_sec / 60.0f) * 60.0f;
    }
    else
    {
        axis = ceilf(target_max_t_sec / 10.0f) * 10.0f;
    }

    return axis;
}

static uint16_t vm_plan_time_step(float max_t_axis_sec)
{
    uint16_t x_step = 10U;

    if (max_t_axis_sec > 28800.0f)
    {
        x_step = 14400U;
    }
    else if (max_t_axis_sec > 14400.0f)
    {
        x_step = 7200U;
    }
    else if (max_t_axis_sec > (float)PLAN_TRACK_HOUR_MODE_THRESHOLD_S)
    {
        x_step = 3600U;
    }
    else if (max_t_axis_sec > 3600.0f)
    {
        x_step = PLAN_TRACK_LONG_MINUTE_STEP_S;
    }
    else if (max_t_axis_sec > 1200.0f)
    {
        x_step = 600U;
    }
    else if (max_t_axis_sec > 600.0f)
    {
        x_step = 300U;
    }
    else if (max_t_axis_sec > 300.0f)
    {
        x_step = 120U;
    }
    else if (max_t_axis_sec > 60.0f)
    {
        x_step = 60U;
    }
    else
    {
    }

    return x_step;
}

void ui_vm_plan_chart_update(ui_vm_plan_chart_t *vm)
{
    uint8_t dive_log_count;
    uint8_t deco_stop_count;

    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));

    vm->current_time_s = (float)bus_get_dive_time_s();
    vm->current_depth_m = bus_get_depth();

    dive_log_count = bus_get_dive_log_count();
    deco_stop_count = bus_get_deco_stop_count();

    vm->dive_log_count = dive_log_count;
    vm->deco_stop_count = deco_stop_count;
    vm->draw_enabled = ((vm->current_depth_m > 0.5f) || (dive_log_count > 0U)) ? 1U : 0U;

    for (uint8_t i = 0U; i < dive_log_count; i++)
    {
        (void)bus_get_dive_log_point(i, &vm->dive_log[i]);
    }

    if (bus_get_stop_type() == STOP_SAFETY && bus_get_stop_depth_m() > 0.0f)
    {
        uint16_t safety_s = bus_get_in_stop_zone() ? bus_get_stop_time_left_s() : bus_get_stop_time_total_s();
        if (safety_s == 0U)
        {
            safety_s = bus_get_stop_time_total_s();
        }
        if (safety_s > 0U)
        {
            vm->deco_stop_count = 1U;
            vm->deco_stops[0].depth_m = bus_get_stop_depth_m();
            vm->deco_stops[0].stay_min = (float)safety_s / 60.0f;
            deco_stop_count = 1U;
        }
        else
        {
            vm->deco_stop_count = 0U;
            deco_stop_count = 0U;
        }
    }
    else
    {
        for (uint8_t i = 0U; i < deco_stop_count; i++)
        {
            (void)bus_get_deco_stop(i, &vm->deco_stops[i]);
        }
    }

    vm->predicted_total_time_s = vm_plan_predicted_total_time(vm->current_time_s,
                                                              vm->current_depth_m,
                                                              deco_stop_count,
                                                              vm->deco_stops);
    vm->max_depth_axis_m = vm_plan_depth_axis(vm_plan_max_log_depth(dive_log_count,
                                                                    vm->dive_log,
                                                                    vm->current_depth_m));
    vm->max_time_axis_s = vm_plan_time_axis(vm->current_time_s, vm->predicted_total_time_s);
    vm->x_step_s = vm_plan_time_step(vm->max_time_axis_s);
}
