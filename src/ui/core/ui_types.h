/*
 * 文件: src/app_ui/ui/core/ui_types.h
 * 作用: 该文件属于 UI 核心模块，负责状态机、数据桥接、事件分发、更新调度或 UI 运行时公共定义。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef UI_TYPES_H
#define UI_TYPES_H

#include "ui_defs.h"
#include "ui_dirty.h"
#include "../screen/page_registry_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LEFT_MAX_WIDGETS 14
#define MAX_5F_WIDGETS   30
#define MAX_CUSTOM_CARDS  MAX_DYNAMIC_SLOTS
#define UI_CUSTOM_CARD_TITLE_MAX_LEN 16U
#define UI_CUSTOM_CARD_TITLE_BUF_SIZE (UI_CUSTOM_CARD_TITLE_MAX_LEN + 1U)

typedef struct
{
    uint8_t widget_id;
    uint8_t x;
    uint8_t y;
} grid_widget_t;

typedef struct
{
    uint8_t            widget_count;
    char title[UI_CUSTOM_CARD_TITLE_BUF_SIZE];
    grid_widget_t widgets[MAX_5F_WIDGETS];
} custom_card_cfg_t;

#pragma pack(push, 1)
typedef struct
{
    uint16_t safe_zone_w;
    uint16_t safe_zone_h;
    int16_t  offset_x;
    int16_t  offset_y;
    uint8_t  theme_mode;
    uint8_t  layout_order;
    uint8_t  dots_position;
    uint8_t  compass_style;
    uint8_t  flash_speed;
    bool     mask_enabled;
    bool     split_outward;
    uint8_t  sep_style;
    uint8_t  sep_thick;
    uint8_t  sep_alpha;
    uint8_t  h_depth;
    uint8_t  h_ndl;
    uint8_t  h_pod;
    uint8_t  h_batt;
    uint8_t  h_gas;
    uint8_t  h_time;
    uint8_t  gap_u;
    uint8_t  panel_gap_u;
    uint8_t  title_h_u;
    uint8_t  h_menu_item;
    uint8_t  gap_menu;
    uint8_t  h_tissues_chart;
    uint8_t            left_widget_count;
    grid_widget_t left_widgets[LEFT_MAX_WIDGETS];
    uint8_t                custom_card_count;
    custom_card_cfg_t custom_cards[MAX_CUSTOM_CARDS];
    uint8_t                custom_card_slot[PAGE_COUNT];
    uint8_t card_order[PAGE_COUNT];
    float   mod_ppo2;
    uint8_t conservatism;
    uint8_t salinity_mode;
    uint8_t last_deco_stop_m;
    uint8_t brightness;
    uint8_t log_rate_s;
    uint8_t time_24h_enabled;
    uint8_t safety_stop_mode;
    uint8_t altitude_level;
    uint16_t depth_alarm_m;
    uint16_t time_alarm_min;
    uint16_t ndl_alarm_min;
} sys_config_t;
#pragma pack(pop)

typedef enum
{
    STOP_NONE = 0,
    STOP_SAFETY,
    STOP_DECO
} stop_type_t;

typedef struct
{
    float   depth;
    int16_t ndl;
    int16_t ndl_stop_value;
    uint8_t ndl_bar_pct;
    stop_type_t stop_type;
    float            stop_depth_m;
    uint16_t         stop_time_total_s;
    uint16_t         stop_time_left_s;
    bool             in_stop_zone;
    uint16_t tts;
    uint32_t dive_time_s;
    uint32_t surface_time_s;
    char    gas_name[16];
    uint8_t gas_active_idx;
    int8_t  gas_recommended_idx;
    float   temperature_c;
    float   bat_temperature_c;
    float   prj_temperature_c;
    float   battery_voltage_v;
    uint8_t charge_state;
    float   ambient_pressure_mbar;
    uint16_t nofly_time_min;
    float   gyro_x_dps;
    float   gyro_y_dps;
    float   gyro_z_dps;
    float   accel_x_g;
    float   accel_y_g;
    float   accel_z_g;
    float   mag_x_ut;
    float   mag_y_ut;
    float   mag_z_ut;
    float   mlx_x_ut;
    float   mlx_y_ut;
    float   mlx_z_ut;
    float   tmag_x_ut;
    float   tmag_y_ut;
    float   tmag_z_ut;
    float   tmag_ut;
    int16_t pitch_deg;
    int16_t roll_deg;
    uint16_t attitude_heading_deg;
    int16_t ble_rssi_dbm;
    uint8_t cpu_load_pct;
    uint16_t fps;
    char    sensor_status[16];
    float   battery_pct;
    uint8_t sys_time_h;
    uint8_t sys_time_m;
    uint8_t sys_time_s;
    float   ascent_rate;
    uint16_t heading;
    bool    heading_locked;
    uint16_t heading_target;
    float   ppo2[GAS_COUNT];
    char    gas_slot_name[GAS_COUNT][16];
    uint8_t gas_slot_o2_pct[GAS_COUNT];
    uint8_t gas_slot_he_pct[GAS_COUNT];
    float   gas_slot_mod_m[GAS_COUNT];
    float   gas_slot_max_ppo2[GAS_COUNT];
    uint8_t gas_slot_count;
    int16_t next_stop_m;
    uint8_t next_stop_min;
    float   pod1_bar;
    float   pod2_bar;
    uint8_t cylinder_count;
    float   sac_rate;
    float   max_depth;
    float   avg_depth;
    float   min_temp;
    float   max_temp;
    float   avg_temp;
    float   surf_gf;
    float   gf99;
    uint8_t gf_low;
    uint8_t gf_high;
    float   mod_m;
    float   ceiling_m;
    float   gas_density;
    float   fio2_pct;
    uint8_t gas_o2_pct;
    uint8_t gas_he_pct;
    uint8_t  tissue_raw_pct[16];
    uint8_t  tissue_gf_pct[16];
    uint8_t  cns_pct;
    uint16_t otu;
    bool    deco_violation;
    bool    strobe_on;
    bool    flashlight_on;
    dirty_mask_t dirty_mask;
} sensor_data_t;

extern sys_config_t  g_sys_config;
extern sensor_data_t g_sensor_data;

typedef struct
{
    float time_s;
    float depth_m;
} dive_pt_t;

#define MAX_LOGBOOK_ENTRIES 16U
#define LOGBOOK_TANK_COUNT  4U

typedef struct
{
    uint16_t log_no;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t start_h;
    uint8_t start_m;
    uint8_t end_h;
    uint8_t end_m;
} logbook_meta_t;

typedef struct
{
    bool valid;
    logbook_meta_t meta;
    uint32_t dive_time_s;
    uint32_t surface_interval_s;
    float max_depth_m;
    float avg_depth_m;
    float surface_mbar;
    uint8_t start_cns_pct;
    uint8_t end_cns_pct;
    float avg_sac_l_min;
    char mode[12];
    char deco_model[16];
    char tank_start[LOGBOOK_TANK_COUNT][8];
    char tank_end[LOGBOOK_TANK_COUNT][8];
} logbook_entry_t;

typedef struct
{
    float depth_m;
    float stay_min;
} deco_stop_t;

#define MAX_DIVE_LOG   200
#ifndef MAX_DECO_STOPS
#define MAX_DECO_STOPS 10
#endif

#ifdef __cplusplus
}
#endif

#endif /* UI_TYPES_H */
