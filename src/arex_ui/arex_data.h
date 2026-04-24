#ifndef AREX_DATA_H
#define AREX_DATA_H

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

/* --- 专用标记接口 --- */
void arex_bus_set_chart_refresh(void);   /* 仅打 DIRTY_CHART，不改数据 */
void arex_bus_clear_all_dirty(void);     /* 消费任务调用，清洗脏标记 */

/* --- 历史轨迹推流（已在 card_plan.c 中实现，此处声明导出） --- */
void arex_dive_log_append(float current_time_s, float current_depth_m);

#endif /* AREX_DATA_H */
