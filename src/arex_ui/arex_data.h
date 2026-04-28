#ifndef AREX_DATA_H
#define AREX_DATA_H

/* =========================================================
 * 平台兼容层 — 必须在所有 include 之前
 *
 * 真机 (RT-Thread): 使用 rt_hw_interrupt_disable/enable 临界区
 * PC 仿真器:        替换为空操作，防止编译报错
 * ========================================================= */
#define PC_SIMULATOR  //移植硬件后需要注释
#ifdef PC_SIMULATOR
    typedef int rt_base_t;
    #define rt_hw_interrupt_disable()   ((rt_base_t)0)  //假代码
    #define rt_hw_interrupt_enable(lvl) ((void)(lvl))   //假代码
#else
    #include <rtthread.h>
#endif

/* =========================================================
 * AREX Data Bus — 硬件写入接口层
 *
 * 铁律：硬件工程师 / 模拟层 只能调用以下 arex_bus_set_* 函数。
 * 禁止直接写入 g_sensor_data，禁止包含任何 LVGL 代码！
 *
 * 数据流：
 *   硬件传感器 ──arex_bus_set_*()──▶ g_sensor_data (dirty_mask)
 *                                              │
 *                              arex_ui_update_task() (50ms)
 *                                              │
 *                                    按脏标记按需刷新 UI
 * ========================================================= */
#include "arex_ui_engine.h"

/* --- 传感器数据写入接口 --- */
void arex_bus_set_depth(float depth_m);               /* 防抖阈值 0.05m */
void arex_bus_set_ndl(int16_t ndl_min);
void arex_bus_set_tts(uint16_t tts_min);
void arex_bus_set_pod(uint8_t pod_idx, float bar);   /* pod_idx: 0=pod1, 1=pod2 */
void arex_bus_set_battery(float pct);
void arex_bus_set_heading(uint16_t heading_deg);
void arex_bus_set_dive_time(uint32_t dive_s);
void arex_bus_set_surface_time(uint32_t surface_s);
void arex_bus_set_ppo2(uint8_t sensor_idx, float ppo2_val); /* sensor_idx: 0~2 */
void arex_bus_set_gas(uint8_t gas_idx, const char *gas_name);
void arex_bus_set_deco(int16_t stop_m, uint8_t stop_min);
void arex_bus_set_cns(uint8_t cns_pct);
void arex_bus_set_otu(uint16_t otu_val);

/* 清除所有脏标记 */
void arex_bus_clear_all_dirty(void);

/* --- 临界区保护的数组写入接口 --- */
/* 16 组织舱饱和度数组（>32bit，必须包临界区防止数据撕裂） */
void arex_bus_set_tissues(const uint8_t tissue_pct[16]);
/* 完整减压站序列（>32bit，必须包临界区） */
void arex_bus_set_deco_plan(const arex_deco_stop_t *stops, uint8_t count);

/* --- System Data 接口 --- */
void arex_bus_set_temperature(float temp_c);
void arex_bus_set_device_status(bool strobe_on, bool flashlight_on, uint8_t cylinder_count);

/* --- 历史轨迹推流（已在 card_plan.c 中实现，此处声明导出） --- */
void arex_dive_log_append(float current_time_s, float current_depth_m);

#endif /* AREX_DATA_H */
