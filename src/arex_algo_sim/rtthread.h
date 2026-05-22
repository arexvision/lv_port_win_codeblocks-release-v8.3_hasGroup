#ifndef AREX_ALGO_SIM_RTTHREAD_H
#define AREX_ALGO_SIM_RTTHREAD_H

#include "lvgl/lvgl.h"

#include <stdint.h>
#include <stdio.h>

typedef uint32_t rt_tick_t;

#ifndef RT_TICK_PER_SECOND
#define RT_TICK_PER_SECOND 1000U
#endif

static inline rt_tick_t rt_tick_get(void)
{
    return (rt_tick_t)lv_tick_get();
}

static inline uint32_t rt_tick_get_millisecond(void)
{
    return (uint32_t)lv_tick_get();
}

#define rt_kprintf printf

#endif /* AREX_ALGO_SIM_RTTHREAD_H */
