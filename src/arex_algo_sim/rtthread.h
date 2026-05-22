#ifndef AREX_ALGO_SIM_RTTHREAD_H
#define AREX_ALGO_SIM_RTTHREAD_H

#include "lvgl/lvgl.h"

#include <stdint.h>
#include <stdio.h>

typedef uint32_t rt_tick_t;
typedef int32_t rt_int32_t;
typedef uint32_t rt_uint32_t;

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

static inline rt_tick_t rt_tick_from_millisecond(rt_int32_t ms)
{
    if (ms <= 0) {
        return 0U;
    }
    return (rt_tick_t)ms;
}

static inline void rt_thread_delay(rt_tick_t tick)
{
    (void)tick;
}

static inline void rt_thread_mdelay(rt_int32_t ms)
{
    (void)ms;
}

#define rt_kprintf printf

#endif /* AREX_ALGO_SIM_RTTHREAD_H */
