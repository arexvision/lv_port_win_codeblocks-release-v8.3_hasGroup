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
    #define rt_hw_interrupt_disable()   ((rt_base_t)0)
    #define rt_hw_interrupt_enable(lvl) ((void)(lvl))
    #define rt_enter_critical()       ((void)0)
    #define rt_exit_critical(lvl)     ((void)(lvl))
#else
    #include <rtthread.h>
    #include <rthw.h>
#endif

/* =========================================================
 * AREX Data Bus — 硬件写入接口层
 *
 * 铁律：硬件工程师 / 模拟层 / BLE 任务 只能调用以下 arex_bus_set_* 函数。
 * 禁止直接写入 g_sensor_data / g_sys_config，禁止包含任何 LVGL 代码！
 *
 * 数据流：
 *   硬件传感器 ──arex_bus_set_*()──▶ g_sensor_data (dirty_mask)
 *   BLE 任务   ──arex_bus_set_ui_layout()──▶ g_sys_config + DIRTY_UI_LAYOUT
 * ========================================================= */
#include "arex_ui_engine.h"

#ifdef __cplusplus
extern "C" {
#endif


/* =========================================================
 * BLE 通讯帧结构体（无业务逻辑，纯数据载体）
 *
 * 协议版本 0x01，总大小 184 字节，可单帧传输。
 * 由 BLE 任务负责接收并校验 CRC，校验通过后调用 arex_bus_set_ui_layout()。
 *
 * BLE 帧字节数计算：
 *   version(1) + card_order[7](7) + left_count(1)
 *   + left_widgets[12] × 6B(72)
 *   + custom_5f_count(1) + custom_5f_widgets[20] × 5B(100)
 *   + crc16(2)
 *   = 184 字节
 * ========================================================= */
#pragma pack(push, 1)
/* 左侧 2x6 组件描述 (6 Bytes) */
typedef struct {
    uint8_t id;         /* arex_widget_id_t (0~13) */
    uint8_t x;          /* 列索引 0~1 */
    uint8_t y;          /* 行索引 0~5 */
    uint8_t w;          /* 列跨度 1~2 */
    uint8_t h;          /* 行跨度 1~2 */
    uint8_t font_id;     /* arex_font_id_t (0~3) */
} ble_sync_left_widget_t;

/* 5F 自定义组件描述 (5 Bytes) */
typedef struct {
    uint8_t id;         /* arex_widget_id_t (0~13) */
    uint8_t r;          /* 起始行 0~5 */
    uint8_t c;          /* 起始列 0~4 */
    uint8_t w;          /* 列跨度 1~2 */
    uint8_t h;          /* 行跨度 1~2 */
} ble_sync_5f_widget_t;

/* BLE UI 布局同步帧 */
typedef struct {
    uint8_t  version;                     /* 协议版本，0x01 */
    uint8_t  card_order[7];              /* 卡片滑动顺序数组 */
    uint8_t  left_count;                 /* 左侧组件数量 */
    ble_sync_left_widget_t left_widgets[12];       /* 左侧 2x6 组件 */
    uint8_t  custom_5f_count;            /* 5F 网格组件数量 */
    ble_sync_5f_widget_t custom_5f_widgets[20];   /* 5F 组件 */
    uint16_t crc16;                      /* CRC-16/XMODEM 校验和 */
} arex_ble_ui_sync_payload_t;
#pragma pack(pop)

#define AREX_BLE_CFG_VERSION  0x01
#define AREX_BLE_FRAME_SIZE    sizeof(arex_ble_ui_sync_payload_t)


/* =========================================================
 * Data Bus Setter 接口
 * ========================================================= */

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

/* --- 临界区保护的数组写入接口 --- */
/* 16 组织舱饱和度数组（>32bit，必须包临界区防止数据撕裂） */
void arex_bus_set_tissues(const uint8_t tissue_pct[16]);
/* 完整减压站序列（>32bit，必须包临界区） */
void arex_bus_set_deco_plan(const arex_deco_stop_t *stops, uint8_t count);

/* --- System Data 接口 --- */
void arex_bus_set_temperature(float temp_c);
void arex_bus_set_device_status(bool strobe_on, bool flashlight_on, uint8_t cylinder_count);
void arex_bus_toggle_layout_order(void);

/* --- 历史轨迹推流（已在 card_plan.c 中实现，此处声明导出） --- */
void arex_dive_log_append(float current_time_s, float current_depth_m);

/* --- BLE 布局同步接口（由 BLE 任务调用） --- */
void arex_bus_set_ui_layout(const arex_ble_ui_sync_payload_t *payload);

/* 清除所有脏标记 */
void arex_bus_clear_all_dirty(void);

#ifdef __cplusplus
}
#endif


#endif /* AREX_DATA_H */
