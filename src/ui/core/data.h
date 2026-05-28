/*
 * 文件: src/app_ui/ui/core/data.h
 * 作用: 该文件属于 UI 核心模块，负责状态机、数据桥接、事件分发、更新调度或 UI 运行时公共定义。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef DATA_H
#define DATA_H

#include "ui_main.h"
/* =========================================================
 * 平台兼容层 — 必须在所有 include 之前
 *
 * 真机 (RT-Thread): 使用 rt_hw_interrupt_disable/enable 临界区
 * PC 仿真器:        替换为空操作，防止编译报错
 * ========================================================= */
#ifdef PC_SIMULATOR
typedef int rt_base_t;
#define rt_hw_interrupt_disable()   ((rt_base_t)0)
#define rt_hw_interrupt_enable(lvl) ((void)(lvl))
#define rt_enter_critical()       ((void)0)
#define rt_exit_critical(lvl)     ((void)(lvl))
#else
#include <rtthread.h>
#include <rthw.h>
#endif

#include "ui_types.h"

/* =========================================================
 * Data Bus — 硬件写入接口层
 *
 * 铁律：硬件工程师 / 模拟层 / BLE 任务 只能调用以下 bus_set_* 函数。
 * 禁止直接写入 g_sensor_data / g_sys_config，禁止包含任何 LVGL 代码！
 *
 * 数据流：
 *   硬件传感器 ──bus_set_*()──▶ g_sensor_data (dirty_mask)
 *   BLE 任务   ──bus_set_ui_layout()──▶ g_sys_config + DIRTY_UI_LAYOUT
 * ========================================================= */
#ifdef __cplusplus
extern "C" {
#endif


/* =========================================================
 * BLE 通讯帧结构体（无业务逻辑，纯数据载体）
 *
 * 协议版本 0x01，总大小 184 字节，可单帧传输。
 * 由 BLE 任务负责接收并校验 CRC，校验通过后调用 bus_set_ui_layout()。
 *
 * BLE 帧字节数计算：
 *   version(1) + card_order[8](8) + left_count(1)
 *   + left_widgets[14] × 3B(42)     ← 2x7 网格，span_w/h 由 MCU 查样式表
 *   + custom_5f_count(1) + custom_5f_widgets[30] × 3B(90)
 *   + crc16(2)
 *   = 145 字节                      ← 优化后
 * ========================================================= */
#pragma pack(push, 1)
/* 左侧 2x7 组件描述 (3 Bytes) */
typedef struct
{
    uint8_t widget_id;    /* comp_id_t (0~52) */
    uint8_t x;           /* 列索引 0~1 */
    uint8_t y;           /* 行索引 0~6 */
} ble_sync_left_widget_t;

/* 5F 自定义组件描述 (3 Bytes) - span_w/h 由 MCU 从样式表自动推导 */
typedef struct
{
    uint8_t widget_id;   /* comp_id_t (0~52) */
    uint8_t r;           /* 起始行 0~5 */
    uint8_t c;           /* 起始列 0~4 */
} ble_sync_5f_widget_t;

/* BLE UI 布局同步帧 */
typedef struct
{
    uint8_t  version;                      /* 协议版本，0x01 */
    uint8_t  card_order[8];               /* 卡片滑动顺序数组 */
    uint8_t  left_count;                  /* 左侧组件数量 */
    ble_sync_left_widget_t left_widgets[14];        /* 左侧 2x7 组件 */
    uint8_t  custom_5f_count;             /* 5F 网格组件数量 */
    ble_sync_5f_widget_t custom_5f_widgets[30];    /* 5F 组件 */
    uint16_t crc16;                       /* CRC-16/XMODEM 校验和 */
} ble_ui_sync_payload_t;
#pragma pack(pop)

#define BLE_CFG_VERSION  0x01
#define BLE_FRAME_SIZE    sizeof(ble_ui_sync_payload_t)


/* =========================================================
 * Data Bus Setter 接口
 * ========================================================= */

void data_init(void);

/* --- 传感器数据写入接口 --- */
void bus_set_depth(float depth_m);               /* 防抖阈值 0.05m */
void bus_set_ascent_rate(float rate_mpm);        /* 正=上升，负=下潜，0=停止 */
void bus_set_tts(uint16_t tts_min);
void bus_set_pod(uint8_t pod_idx, float bar);   /* pod_idx: 0=pod1, 1=pod2 */
void bus_set_battery(float pct);
void bus_set_heading(uint16_t heading_deg);
void bus_set_dive_time(uint32_t dive_s);
void bus_set_surface_time(uint32_t surface_s);
void bus_set_ppo2(uint8_t sensor_idx, float ppo2_val); /* sensor_idx: 0~4 */
void bus_set_gas(uint8_t gas_idx, const char *gas_name);
void bus_set_gas_slot_count(uint8_t count);
void bus_set_gas_slot(uint8_t gas_idx, const char *gas_name,
                           uint8_t o2_pct, uint8_t he_pct, float mod_m);
void bus_set_deco(int16_t stop_m, uint8_t stop_min);
void bus_set_cns(uint8_t cns_pct);
void bus_set_otu(uint16_t otu_val);
void bus_set_gf99(float gf99);
void bus_set_surf_gf(float surf_gf);
void bus_set_temperature(float temp_c);
void bus_set_bat_temperature(float temp_c);
void bus_set_prj_temperature(float temp_c);
/* --- 临界区保护的数组写入接口 --- */
/* 16 组织舱饱和度数组（>32bit，必须包临界区防止数据撕裂） */
void bus_set_tissues(const uint8_t tissue_pct[16]);
/* 完整减压站序列（>32bit，必须包临界区） */
void bus_set_deco_plan(const deco_stop_t *stops, uint8_t count);

/* --- 模拟器布局切换接口 --- */
void bus_toggle_layout_order(void);
void bus_toggle_theme(void);
void bus_toggle_dots_position(void);
void bus_toggle_compass_style(void);
void bus_toggle_sep_style(void);
void bus_toggle_flash_speed(void);
void bus_toggle_mask(void);
void bus_toggle_split_outward(void);
void bus_set_ui_offset(int16_t offset_x, int16_t offset_y);

/* =========================================================
 * 减压状态综合更新接口（原子操作）
 *
 * 将 NDL + 停留状态合并为一次原子调用，避免数据不一致。
 * 算法层每个周期只需调用一次，减少总线通信开销。
 *
 * @param ndl_min        免减压时间（分钟，0=进入减压区）
 * @param stop_type      停留类型（NONE/Safety/Deco）
 * @param depth_m        停留深度（米，0=不在停留）
 * @param time_s         停留剩余时间（秒），UI 直接显示倒计时
 * ========================================================= */
void bus_update_deco(int16_t ndl_min, stop_type_t stop_type,
                          float depth_m, uint16_t total_time_s,
                          uint16_t time_s, bool in_stop_zone);
void bus_set_ndl_bar_pct(uint8_t pct);

/* --- NDL 独立接口（快速轮询，仅更新 NDL 数值） --- */
/* 注意：优先使用 bus_update_deco() 一次性更新所有减压数据 */
void bus_set_ndl(int16_t ndl_min);

/* --- 用户设置接口 --- */
void bus_set_gf_setting(uint8_t gf_low, uint8_t gf_high);
void bus_set_conservatism(uint8_t level);
void bus_set_mod_ppo2(float ppo2);
void bus_set_salinity_mode(uint8_t mode);
void bus_set_last_deco_stop(uint8_t depth_m);
void bus_set_brightness(uint8_t level);
void bus_set_log_rate(uint8_t seconds);

/* --- 技术潜水参数接口 --- */
void bus_set_mod(float mod_m);
void bus_set_ceiling(float ceiling_m);
void bus_set_gas_mix(uint8_t o2_pct, uint8_t he_pct);
void bus_set_gas_density(float density);
void bus_set_fio2(float fio2_pct);

/* --- UI 只读快照/意图接口 --- */
uint16_t ui_safe_zone_w_get(void);
uint16_t ui_safe_zone_h_get(void);
int16_t ui_offset_x_get(void);
int16_t ui_offset_y_get(void);
bool ui_mask_enabled_get(void);
uint16_t ui_block_gap_px_get(void);
uint16_t ui_panel_gap_px_get(void);
uint16_t ui_menu_gap_px_get(void);
uint16_t ui_menu_item_h_px_get(void);
uint16_t ui_tissues_chart_h_px_get(void);
order_t ui_layout_order_get(void);
uint8_t ui_dots_position_get(void);
uint8_t ui_depth_h_u_get(void);
uint8_t ui_ndl_h_u_get(void);
uint8_t ui_pod_h_u_get(void);
uint8_t ui_batt_h_u_get(void);
uint8_t ui_gas_h_u_get(void);
uint8_t ui_time_h_u_get(void);
uint8_t ui_left_widget_count_get(void);
const grid_widget_t *ui_left_widget_get(uint8_t index);
uint8_t ui_custom_card_count_get(void);
uint8_t ui_custom_card_widget_count_get(uint8_t custom_card_idx);
const grid_widget_t *ui_custom_card_widget_get(uint8_t custom_card_idx, uint8_t widget_idx);
uint8_t ui_custom_card_slot_get(uint8_t storage_pos);
float bus_get_depth(void);
float bus_get_stop_depth_m(void);
stop_type_t bus_get_stop_type(void);
uint8_t bus_get_ndl_bar_pct(void);
uint16_t bus_get_stop_time_total_s(void);
uint16_t bus_get_stop_time_left_s(void);
bool bus_get_in_stop_zone(void);
int16_t bus_get_ndl(void);
int16_t bus_get_ndl_stop_value(void);
float bus_get_max_depth(void);
float bus_get_avg_depth(void);
uint32_t bus_get_dive_time_s(void);
uint32_t bus_get_surface_time_s(void);
float bus_get_battery_pct(void);
float bus_get_pod1_bar(void);
float bus_get_pod2_bar(void);
float bus_get_temperature(void);
float bus_get_min_temp(void);
float bus_get_avg_temp(void);
float bus_get_max_temp(void);
bool bus_get_bat_temperature_valid(void);
bool bus_get_prj_temperature_valid(void);
float bus_get_bat_temperature(void);
float bus_get_prj_temperature(void);
float bus_get_ascent_rate(void);
uint16_t bus_get_sys_time_h(void);
uint16_t bus_get_sys_time_m(void);
uint8_t bus_get_gas_slot_count(void);
uint8_t bus_get_gas_active_idx(void);
const char *bus_get_gas_slot_name(uint8_t gas_idx);
uint8_t bus_get_gas_slot_o2_pct(uint8_t gas_idx);
uint8_t bus_get_gas_slot_he_pct(uint8_t gas_idx);
float bus_get_gas_slot_mod_m(uint8_t gas_idx);
float bus_get_gas_slot_ppo2(uint8_t gas_idx);
uint8_t bus_get_gas_mix_o2(void);
uint8_t bus_get_gas_mix_he(void);
float bus_get_gas_density(void);
float bus_get_mod_m(void);
float bus_get_ceiling_m(void);
float bus_get_mod_ppo2(void);
float bus_get_fio2_pct(void);
uint8_t bus_get_gf_low(void);
uint8_t bus_get_gf_high(void);
float bus_get_gf99(void);
float bus_get_surf_gf(void);
uint8_t bus_get_cns_pct(void);
uint16_t bus_get_otu(void);
uint8_t bus_get_tissue_pct(uint8_t index);
uint8_t bus_get_pod_count(void);
float bus_get_pod_bar(uint8_t pod_idx);
float bus_get_tts(void);
float bus_get_sac_rate(void);
uint8_t bus_get_last_deco_stop(void);
uint8_t bus_get_salinity_mode(void);
uint8_t bus_get_conservatism(void);
uint8_t bus_get_brightness(void);
uint8_t bus_get_log_rate(void);
bool bus_is_heading_locked(void);
uint16_t bus_get_heading(void);
uint16_t bus_get_heading_target(void);
void bus_lock_heading_to_current(void);
void bus_clear_heading_lock(void);

/* --- 历史轨迹 / 减压停留原始数据只读接口 --- */
uint8_t bus_get_dive_log_count(void);
bool bus_get_dive_log_point(uint8_t index, dive_pt_t *out_point);
uint8_t bus_get_deco_stop_count(void);
bool bus_get_deco_stop(uint8_t index, deco_stop_t *out_stop);

/* --- 历史轨迹推流 --- */
void dive_log_append(float current_time_s, float current_depth_m);
void dive_log_append_sampled(float current_time_s, float current_depth_m);
void dive_log_reset(void);

/* --- BLE 布局同步接口（由 BLE 任务调用） --- */
void bus_set_ui_layout(const ble_ui_sync_payload_t *payload);

/* 原子取走当前脏标记：UI 消费本帧 mask，新写入的 dirty 留给下一帧 */
uint32_t bus_take_dirty(void);
void bus_requeue_dirty(uint32_t mask);

/* 清除所有脏标记 */
void bus_clear_all_dirty(void);

// /* 重置潜水统计值（开始新潜水时调用） */
// void bus_reset_stats(void);

/* --- 配置持久化（weak 实现由具体平台覆盖） --- */
bool config_load(sys_config_t *cfg);
bool config_save(const sys_config_t *cfg);

#ifdef __cplusplus
}
#endif


#endif /* DATA_H */
