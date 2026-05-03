#ifndef AREX_UI_ENGINE_H
#define AREX_UI_ENGINE_H

#include "lvgl/lvgl.h"
#include <stdint.h>
#include <stdbool.h>
#include "arex_card_registry.h"

#ifdef __cplusplus
extern "C" {
#endif


/* =========================================================
 * 系统核心宏定义
 * ========================================================= */
/* 上电时所有数据区显示 "--" 占位符，等 sim_tick_cb 首次触发后才显示真实数据。
 * 设为 0 恢复原有行为（初始值直接显示）。 */
#define AREX_SHOW_PLACEHOLDER_ON_INIT  1/*只是用来模拟器使用的，实际项目也可以用来防抖，上电会显示--，传感器数据来了变为数据*/
#define AREX_BASE_U             10   /* 物理基准单位 1U = 10px */
#define AREX_MIN_CLASSIC_TOP_H  200  /* Classic 模式下最小上区高度 px */
#define AREX_MASK_EDGE_GUARD    80   /* 面镜盲区掩膜底部警戒阈值 px */
#define AREX_PHYSICAL_W    640  /* 硬件屏幕极限宽 */
#define AREX_PHYSICAL_H    480  /* 硬件屏幕极限高 */
#define AREX_LEFT_ANCHOR_W  160  /* 左侧锚点固定宽度 */
/* 右侧卡片全局标题区高度分配（统一控制标题文字+分割线占用高度） */
#define AREX_CARD_TITLE_H  60  /* 标题区高度：文字(Y=8) + 分隔线(Y=48) + 下方留白，视觉底边焊死 Y=48 */

/* 减压数据刷新节流宏：只允许每 N ms 刷新一次，避免高频深度变化时 UI 负载过高 */
#define AREX_DECO_REFRESH_MS   1000   /* 减压跟踪刷新间隔（ms），设为 0 则关闭节流（每次都刷新） */

/* 告警横幅配置：1=显示 "CRITICAL:" / "WARNING:" 前缀，0=只显示告警文字 */
#define AREX_ALARM_SHOW_PREFIX  0

/* =========================================================
 * 颜色宏 (统一集中管理)
 * ========================================================= */
#define AREX_GREEN   lv_color_make(0x00, 0xFF, 0x00)
#define AREX_LIGHT   lv_color_make(0x55, 0xFF, 0x55)
#define AREX_DARK    lv_color_make(0x00, 0x33, 0x00)
#define AREX_BLACK   lv_color_make(0x00, 0x00, 0x00)
#define AREX_BG      lv_color_make(0x05, 0x05, 0x05)

/* =========================================================
 * UI 调试与排版开关
 * 0 = 量产模式 (隐藏所有布局外框，极其干净)
 * 1 = 调试模式 (显示所有暗绿色外框，用于排版对齐)
 * ========================================================= */
#define AREX_DEBUG_BORDERS   1  /* 固定区(左侧锚点/SafeZone)排版框 */
#define AREX_CARD_DEBUG_BORDERS  1  /* 卡片区域选项排版框 */
#define AREX_INNER_BORDER_W  2  /* 内部菜单项的暗绿色边框粗细 (0=隐藏, 1或2=显示) */
#define AREX_GAS_BORDER_W    2  /* GAS SWITCH 卡片的边框粗细 */
#define AREX_GRID_BORDER_W   0  /* 5F 自定义网格组件的边框粗细 */

/* 5列x6行网格，最多装30个组件 */
#define AREX_MAX_WIDGETS    30
#define AREX_WIDGET_COLS    5
#define AREX_WIDGET_ROWS    6

/* 10U 左侧锚点最大行数（布局数组长度，2x7=14格，最坏14个单格组件） */
#define AREX_MAX_LEFT_ROWS    8

/* 左侧锚点模块间分割线宏 */
#define AREX_ANCHOR_SEP_THICK  3   /* 模块间分割线粗细 px */
#define AREX_ANCHOR_SEP_STYLE  AREX_SEP_SOLID  /* 分割线样式: SOLID/DASHED/DOTTED */

/* 上升速率图标相关常量（供全局引用） */
#define MAX_ASCENT_ICONS  4         /* 最多支持屏幕上出现 MAX_ASCENT_ICONS 个深度模块 */

/* 速率阈值宏（可调整以修改 level1 触发灵敏度） */
/* Level1 触发阈值：速率绝对值 >= 此值时显示 level1 */
#define AREX_RATE_LEVEL1_THRESHOLD  3.0f   /* m/min */
/* Level2 触发阈值：速率绝对值 >= 此值时显示 level2 */
#define AREX_RATE_LEVEL2_THRESHOLD  9.0f   /* m/min */
/* 静止判定阈值：速率绝对值 < 此值时视为静止（不闪烁） */
#define AREX_RATE_STILL_THRESHOLD   3.0f   /* m/min */
extern lv_obj_t *s_img_ascent_rate[MAX_ASCENT_ICONS];
extern uint8_t  s_ascent_icon_count;

/* =========================================================
 * 1b. 气体表常量 (供全局引用)
 * ========================================================= */
#define AREX_GAS_COUNT  4

extern const char  *AREX_GAS_NAMES[AREX_GAS_COUNT];
extern const uint8_t AREX_GAS_MOD_M[AREX_GAS_COUNT];

/* 卡片数量常量（与 CARD_ID_* 枚举一致，引用 arex_card_registry.h 中的枚举尾项） */
#define AREX_CARD_COUNT  CARD_ID_COUNT

/* 卡片顺序配置读取接口（供 arex_card_registry.c / arex_ui_state.c 使用） */
extern uint8_t g_sys_card_order(uint8_t pos);

/* =========================================================
 * 2. 枚举字典 (配置项映射)
 * ========================================================= */

/* ---- 字体 ID 枚举 (APP 同步核心) ----
 *
 * 全系统唯一字体 ID 字典。APP 通过下发数字 ID 来切换字体，
 * 渲染引擎通过 arex_get_font(id) 映射为真实 lvgl 字体指针。
 * 禁止在配置结构体中保存 lv_font_t*，只允许保存此枚举值！
 */
typedef enum {
    AREX_FONT_ID_SMALL = 0,  /* 14px  标签/单位/Badge */
    AREX_FONT_ID_TITLE,       /* 20px  菜单项/卡片标题 */
    AREX_FONT_ID_MEDIUM,      /* 28px  数据值 */
    AREX_FONT_ID_HUGE,       /* 58px  深度大数字 */
} arex_font_id_t;

typedef enum {
    AREX_THEME_TECH = 0,   /* Left Grid + Right Cards（当前使用） */
    AREX_THEME_CLASSIC      /* 上下流式布局（预留，渲染代码未实现） */
} arex_theme_t;

typedef enum {
    AREX_ORDER_NORMAL = 0,  /* 标准 (左/又) */
    AREX_ORDER_REVERSE      /* 翻转 (上/下) */
} arex_order_t;

typedef enum {
    AREX_DOTS_RIGHT = 0,
    AREX_DOTS_LEFT,
    AREX_DOTS_BOTTOM,
    AREX_DOTS_NONE
} arex_dots_pos_t;

typedef enum {
    AREX_COMPASS_CLASSIC = 0, /* 战术横带 (Tape) */
    AREX_COMPASS_AERO,        /* 战斗机平显 (HUD) */
    AREX_COMPASS_SUB          /* 潜艇声呐 (Sonar) */
} arex_compass_style_t;

typedef enum {
    AREX_ALIGN_LEFT   = 0,
    AREX_ALIGN_CENTER,
    AREX_ALIGN_RIGHT
} arex_align_t;

typedef enum {
    AREX_SEP_NONE    = 0,
    AREX_SEP_SOLID,
    AREX_SEP_DASHED,
    AREX_SEP_DOTTED
} arex_sep_style_t;

/* =========================================================
 * AREX 全量组件 ID 字典 (扁平分组枚举，APP/MCU 严格对齐)
 *
 * 架构铁律：
 *   - 组件 ID 的数值直接编码了物理尺寸信息（如 WIDGET_DEPTH_1612 = 2列×2行）
 *   - POD_0806 (33) 是全局唯一真实存在的气瓶模具 ID
 *   - APP 下发时，同一个 POD_0806 可以出现多次
 *   - MCU 通过渲染计数器（s_pod_render_count）自动分配 POD1/POD2
 *
 * 命名规则: WIDGET_<TYPE>_<W><H>
 *   TYPE: DEPTH, NDL_STOP, DIVE_TIME, GAS, TEMP, ...
 *   W: 列跨度 (1~5)
 *   H: 行跨度 (1~6)
 * ========================================================= */
/* =========================================================
 * AREX 全局组件 ID 字典 (Strictly mapped from Protobuf)
 * 警告：此枚举必须与 APP 端的 WidgetType 保持 100% 对齐！
 * ========================================================= */
typedef enum {
    WIDGET_TYPE_UNSPECIFIED = 0,
    WIDGET_EMPTY            = 0,  /* C语言内部别名：空槽位占位符 */

    /* --- 核心驻留区 (强制固定) --- */
    WIDGET_NDL_STOP_1606    = 1,
    WIDGET_DEPTH_1612       = 2,
    WIDGET_DEPTH_1606       = 3,
    WIDGET_DIVE_TIME_1606   = 4,
    WIDGET_GAS_1606         = 5,
    WIDGET_SYS_1606         = 6,

    /* --- 基础组件 (Basic) --- */
    WIDGET_TEMP_0806        = 10,
    WIDGET_TIME_1606        = 11,
    WIDGET_TTS_0806         = 12,
    WIDGET_ASCENT_0806      = 13,
    WIDGET_ASCENT_0812      = 14,
    WIDGET_COMPASS_1612     = 15,
    WIDGET_BATTERY_0806     = 16,
    WIDGET_STOP_DEPTH_0806  = 17,
    WIDGET_STOP_TIME_1606   = 18,
    WIDGET_PPO2_0806        = 19,

    /* --- 技术潜水 (Tech Dive) --- */
    WIDGET_SURF_GF_0806     = 20,
    WIDGET_GF99_0806        = 21,
    WIDGET_CNS_0806         = 22,
    WIDGET_OTU_0806         = 23,
    WIDGET_GF_0806          = 24,
    WIDGET_MOD_0806         = 25,
    WIDGET_CEILING_0806     = 26,
    WIDGET_GAS_MIX_1606     = 27,
    WIDGET_TISSUE_GF_4012   = 28,
    WIDGET_TISSUE_RAW_4012  = 29,
    WIDGET_GAS_DENS_0806    = 30,
    WIDGET_FIO2_0806        = 31,

    /* --- 传感器 (Sensors) --- */
    WIDGET_HEADING_0806     = 32,
    WIDGET_POD_0806         = 33,
    WIDGET_DEPTH_MAX_0806   = 34,
    WIDGET_DEPTH_AVG_0806   = 35,
    WIDGET_TEMP_MIN_0806    = 36,
    WIDGET_TEMP_AVG_0806    = 37
    /* 🚨 架构师警告：APP 端的 Protobuf 中移除了以下 ID：
     * WIDGET_TEMP_MAX_0806 (原38)
     * WIDGET_SAC_RATE_0806 (原39, 耗气率)
     * WIDGET_WTIME_0806 (原40, 水面休息时间)
     * WIDGET_PPO2_SAFE_0806 等边界安全组件 (原50+)
     * 请务必确认产品经理确实删除了这些模块！
     */
} arex_widget_id_t;

/* =========================================================
 * 2d. 告警级别枚举
 * ========================================================= */
typedef enum {
    AREX_ALARM_NONE   = 0,
    AREX_ALARM_INFO   = 1,   /* INFO: 提醒 */
    AREX_ALARM_WARN   = 2,   /* WARN: 警告（1Hz 闪烁）*/
    AREX_ALARM_CRIT   = 3,   /* CRITICAL: 危险（2Hz 快速闪烁）*/
} arex_alarm_level_t;

/* =========================================================
 * 3. NVDS 核心配置结构体 (字节对齐，用于持久化)
 * ========================================================= */

/* 统一的左右网格组件类型 */
#define AREX_LEFT_MAX_WIDGETS 12
#define AREX_5F_MAX_WIDGETS   30

typedef struct {
    arex_widget_id_t widget_id;
    uint8_t x;   /* 列索引 */
    uint8_t y;   /* 行索引 */
} arex_grid_widget_t;

#pragma pack(push, 1)
typedef struct {
    /* --- 光学与安全区 (Safe Zone) --- */
    uint16_t safe_zone_w;    /* 默认 580 */
    uint16_t safe_zone_h;    /* 默认 420 */
    int16_t  offset_x;      /* 瞳距校准 (IPD) */
    int16_t  offset_y;      /* 浮力盲区校准 */

    /* --- 全局架构与行为 --- */
    uint8_t  theme_mode;    /* arex_theme_t */
    uint8_t  layout_order;   /* arex_order_t */
    uint8_t  dots_position;   /* arex_dots_pos_t */
    uint8_t  compass_style;  /* arex_compass_style_t */
    uint8_t  flash_speed;    /* 动画闪烁速度 (0=慢, 1=中, 2=快) */
    bool     mask_enabled;   /* 面镜盲区掩膜开关 */

    /* --- 样式与对齐 --- */
    bool     split_outward;  /* 双拼模块向外展开 */

    /* --- 分割线系统 --- */
    uint8_t  sep_style;       /* arex_sep_style_t */
    uint8_t  sep_thick;       /* 线条粗细 px */
    uint8_t  sep_alpha;       /* 透明度 0~255 */

    /* --- 10U 网格高度分配 (1U = 10px) --- */
    uint8_t  h_depth;        /* DEPTH 大通栏 (默认 8U) */
    uint8_t  h_ndl;          /* NDL/TTS 双拼 (默认 6U) */
    uint8_t  h_pod;          /* POD 1/2 双拼 (默认 6U) */
    uint8_t  h_batt;         /* BATT/W.TIME 双拼 (默认 4U) */
    uint8_t  h_gas;          /* GAS 中通栏 (默认 6U) */
    uint8_t  h_time;         /* DIVE TIME 底部 (默认 4U) */
    uint8_t  gap_u;          /* 模块间距 (默认 1U) */
    uint8_t  panel_gap_u;     /* 左侧锚点与右侧面板间距 (默认 1U) */
    uint8_t  title_h_u;      /* 标题高度 (默认 2U) */
    uint8_t  h_menu_item;    /* 菜单项高度 (默认 4.8U→取整5U=48px) */
    uint8_t  gap_menu;       /* 菜单项间距 (默认 0.8U=8px) */
    uint8_t  h_tissues_chart;/* 组织柱图高度 (默认 9U=90px) */

    /* --- 左侧 2x7 锚点配置 --- */
    uint8_t            left_widget_count;
    arex_grid_widget_t left_widgets[AREX_LEFT_MAX_WIDGETS];

    /* --- 右侧 5F 自定义网格配置 --- */
    uint8_t            custom_5f_count;
    arex_grid_widget_t custom_5f_widgets[AREX_5F_MAX_WIDGETS];

    /* --- 卡片顺序 (APP 同步就绪)
     * card_order[pos] = card_id
     * INFO 固定在 tile 0，SETUP 固定在 tile 7。
     * CARD_POS_1 ~ CARD_POS_6 这 6 个位置可由 APP 重排。
     */
    uint8_t card_order[AREX_CARD_COUNT];

    /* --- 用户设置 (运行时可修改) --- */
    float   mod_ppo2;           /* 默认 1.4f */
    uint8_t conservatism;       /* 默认 1 (MED) */
    uint8_t brightness;         /* 默认 2 (HIGH) */

} arex_sys_config_t;
#pragma pack(pop)

/* =========================================================
 * 4. 实时数据总线 (RAM Only - 高频刷新)
 * ========================================================= */

/* 停留状态枚举 */
typedef enum {
    AREX_STOP_NONE = 0,    /* 0: 常态，无停留 */
    AREX_STOP_SAFETY,      /* 1: 安全停留 */
    AREX_STOP_DECO         /* 2: 强制减压停留 */
} arex_stop_type_t;

typedef struct {
    /* =========================================================
     * 核心数据 (Core)
     * ========================================================= */
    float   depth;              /* 当前深度 m */
    int16_t ndl;               /* 免减压时间 min */
    int16_t ndl_stop_value;    /* NDL_STOP: 停留时间/剩余 NDL 动态值 */

    /* --- 动态停留状态机 --- */
    arex_stop_type_t stop_type;        /* 当前所处的停留模式 */
    float            stop_depth_m;     /* 目标停留深度 (如 3.0m 或 6.0m) */
    uint16_t         stop_time_total_s;/* 该减压站的总时间 (用于计算横向进度条) */
    uint16_t         stop_time_left_s; /* 剩余倒计时 (秒) */
    bool             in_stop_zone;     /* 是否在目标深度 ±1.5m 范围内？(决定是否读秒) */
    uint16_t tts;              /* 回到水面时间 min */
    uint32_t dive_time_s;       /* 潜水总时 s */
    uint32_t surface_time_s;    /* WTM 水面休息时间 s */
    char    gas_name[16];       /* 当前气体名称 */
    uint8_t gas_active_idx;     /* 当前气体索引 0-3 */

    /* =========================================================
     * 基础数据 (Basic)
     * ========================================================= */
    float   temperature_c;      /* 当前温度 */
    float   battery_pct;         /* 电池百分比 */
    uint8_t sys_time_h;         /* 当前时 (0-23) */
    uint8_t sys_time_m;         /* 当前分 (0-59) */
    float   ascent_rate;        /* 上升速率 m/min (正=上升，负=下潜) */
    uint16_t heading;           /* 当前航向 0~359 */
    bool    heading_locked;     /* 航向是否锁定 */
    uint16_t heading_target;    /* 锁定目标航向 */
    float   ppo2[3];           /* 三段 PPO2 */
    int16_t next_stop_m;       /* 下一减压站深度 m */
    uint8_t next_stop_min;     /* 下一减压站停留时间 min */

    /* =========================================================
     * 传感器数据 (Sensors)
     * ========================================================= */
    float   pod1_bar;          /* 气瓶1压力 bar */
    float   pod2_bar;          /* 气瓶2压力 bar */
    uint8_t cylinder_count;    /* 气瓶连接数量 */
    float   sac_rate;          /* SAC 呼吸速率 L/min */
    float   max_depth;         /* 本次最大深度 m */
    float   avg_depth;         /* 平均深度 m */
    float   min_temp;          /* 最低温度 */
    float   max_temp;          /* 最高温度 */
    float   avg_temp;          /* 平均温度 */

    /* =========================================================
     * 技术潜水数据 (Tech Dive)
     * ========================================================= */
    float   surf_gf;           /* Surf.GF 预测值 */
    float   gf99;              /* GF99 实时值 % */
    uint8_t gf_low;            /* GF Low 设定值 (如 30) */
    uint8_t gf_high;           /* GF High 设定值 (如 70) */
    float   mod_m;             /* 最大操作深度 m */
    float   ceiling_m;         /* 实时减压上限 Ceiling m */
    float   gas_density;       /* 气体密度 g/L */
    float   fio2_pct;          /* 实际吸入氧浓度 % */
    uint8_t gas_o2_pct;       /* O2 浓度 % */
    uint8_t gas_he_pct;       /* He 浓度 % */

    /* =========================================================
     * 氧中毒监控 (Oxygen Toxicity)
     * ========================================================= */
    uint8_t  tissue_pct[16];  /* 16 组织舱饱和度数组 */
    uint8_t  cns_pct;         /* CNS 氧中毒百分比 0-100 */
    uint16_t otu;             /* OTU 氧中毒剂量单位 */

    /* =========================================================
     * 设备状态 (Device Status)
     * ========================================================= */
    bool    deco_violation;    /* 减压违规标志 */
    bool    strobe_on;         /* 频闪灯开关状态 */
    bool    flashlight_on;      /* 手电筒开关状态 */

    /* =========================================================
     * Data Bus 脏标记位域 (UI 消费任务专用)
     * ========================================================= */
    uint32_t dirty_mask;

} arex_sensor_data_t;

/* =========================================================
 * Data Bus 脏标记位掩码枚举
 * ========================================================= */
typedef enum {
    DIRTY_NONE       = 0,
    /* 核心数据 */
    DIRTY_DEPTH      = (1U << 0),   /* 深度数据 */
    DIRTY_NDL        = (1U << 1),   /* 免减压时间 */
    DIRTY_NDL_STOP   = (1U << 2),   /* NDL_STOP 动态值 + 停留状态机 */
    DIRTY_TTS        = (1U << 3),   /* 回到水面时间 */
    DIRTY_DIVE_TIME  = (1U << 4),   /* 潜水总时 */
    DIRTY_GAS        = (1U << 5),   /* 气体切换 */

    /* 基础数据 */
    DIRTY_TEMP       = (1U << 6),   /* 温度数据 */
    DIRTY_BATT       = (1U << 7),   /* 电池电量 */
    DIRTY_TIME_DAY   = (1U << 8),   /* 当前钟表时间 */
    DIRTY_ASCENT     = (1U << 9),   /* 上升速率 */
    DIRTY_HEADING    = (1U << 10),  /* 罗盘航向 */
    DIRTY_PPO2       = (1U << 11),  /* PO2 值 */
    DIRTY_STOP_DEPTH = (1U << 12),  /* 停止深度 */
    DIRTY_STOP_TIME  = (1U << 13),  /* 停止时间 */

    /* 传感器数据 */
    DIRTY_POD        = (1U << 14),  /* 气瓶压力 */
    DIRTY_SAC        = (1U << 15),  /* SAC 呼吸速率 */
    DIRTY_DEPTH_STATS = (1U << 16), /* 深度统计 (max/avg) */
    DIRTY_TEMP_STATS = (1U << 17), /* 温度统计 (min/max/avg) */

    /* 技术潜水数据 */
    DIRTY_SURF_GF    = (1U << 18),  /* Surf.GF */
    DIRTY_GF99       = (1U << 19),  /* GF99 */
    DIRTY_GF_SETTING = (1U << 20),  /* GF 设定值 */
    DIRTY_MOD        = (1U << 21),  /* 最大操作深度 */
    DIRTY_CEILING    = (1U << 22),  /* 减压上限 */
    DIRTY_GAS_MIX    = (1U << 23),  /* 气体混合比 */
    DIRTY_GAS_DENS   = (1U << 24),  /* 气体密度 */
    DIRTY_FIO2       = (1U << 25),  /* 吸入氧浓度 */

    /* 氧中毒 */
    DIRTY_TISSUES    = (1U << 26),  /* 组织舱饱和度 */
    DIRTY_CNS        = (1U << 27),  /* CNS 百分比 */
    DIRTY_OTU        = (1U << 28),  /* OTU */

    /* 设备状态 */
    DIRTY_ALARM      = (1U << 29),  /* 告警状态 */
    DIRTY_DEVICES    = (1U << 30),  /* 外设状态 */
    DIRTY_UI_LAYOUT  = (1U << 31),  /* UI 布局重建 */
    DIRTY_SETUP      = (1U << 22),  /* 用户设置变更（conservatism 等） */

} arex_dirty_bit_t;

/* =========================================================
 * 5. 全局单例
 * ========================================================= */
extern arex_sys_config_t  g_sys_config;
extern arex_sensor_data_t g_sensor_data;

/* =========================================================
 * NDL_STOP 多形态组件句柄（160x60 极限空间内的"变形金刚"）
 * 支持屏幕上多个 NDL 模块（左侧锚点 1 个 + 5F 多个）
 * 三种状态: NDL常态 / Safety停留 / Deco停留
 * ========================================================= */
#define MAX_NDL_ICONS 4
typedef struct {
    lv_obj_t *comp;
    lv_obj_t *vert_bg;
    lv_obj_t *vert_fill;
    lv_obj_t *horiz_bg;
    lv_obj_t *horiz_fill;
    lv_obj_t *main_val;
    lv_obj_t *title_top;
    lv_obj_t *sub_bot;
} ndl_handle_t;

/* 速率图标指针阵列（最多支持 MAX_ASCENT_ICONS 个 DEPTH 模块） */
#define MAX_ASCENT_ICONS 4
extern lv_obj_t *s_img_ascent_rate[MAX_ASCENT_ICONS];
extern uint8_t  s_ascent_icon_count;

/* NDL_STOP 多形态组件句柄数组 */
extern ndl_handle_t s_ndl_handles[MAX_NDL_ICONS];
extern uint8_t      s_ndl_handle_count;

/* =========================================================
 * 6b. 潜水轨迹与减压停留（供 card_plan.c 和 UI_main.c 共享）
 * ========================================================= */
typedef struct { float time_s; float depth_m; } arex_dive_pt_t;
typedef struct { float depth_m; float stay_min; }  arex_deco_stop_t;

#define MAX_DIVE_LOG   100
#define MAX_DECO_STOPS 10

extern arex_dive_pt_t   g_dive_log[MAX_DIVE_LOG];
extern uint16_t         g_dive_log_count;
extern arex_deco_stop_t g_deco_stops[MAX_DECO_STOPS];
extern uint16_t         g_deco_stop_count;

/* =========================================================
 * 7. API 接口
 * ========================================================= */
void arex_ui_init(void);
void arex_ui_apply_config(void);
void arex_ui_update_data(void);

/* 配置默认值加载 */
void arex_sys_config_defaults(arex_sys_config_t *cfg);

/* 安全区边界检测 */
bool arex_safe_zone_in_danger(void);

/* =========================================================
 * 8. 绝对坐标推算引擎 (供 arex_screen.c 调用)
 * ========================================================= */

/* Tech 模式布局 */
void arex_calc_tech_layout(int16_t *out_lx, int16_t *out_ly,
                           uint16_t *out_lw, uint16_t *out_lh,
                           int16_t *out_rx, int16_t *out_ry,
                           uint16_t *out_rw, uint16_t *out_rh);

/* Classic 模式布局 */
void arex_calc_classic_layout(int16_t *out_top_x, int16_t *out_top_y,
                              uint16_t *out_top_w, uint16_t *out_top_h,
                              int16_t *out_bot_x, int16_t *out_bot_y,
                              uint16_t *out_bot_w, uint16_t *out_bot_h);

/* 5x6 网格坐标推算 */
void arex_calc_widget_cell(uint16_t parent_w, uint16_t parent_h,
                          uint8_t row, uint8_t col,
                          uint8_t w_span, uint8_t h_span,
                          int16_t *out_x, int16_t *out_y,
                          uint16_t *out_w, uint16_t *out_h);

/* 16 柱组织图 X 坐标推算 */
void arex_calc_tissue_bars(uint16_t total_w, uint16_t bar_max_h,
                           int16_t out_x[16], uint16_t out_w[16]);

/* LVGL 辅助 */
lv_text_align_t arex_align_to_lv(uint8_t align);
lv_align_t arex_align_to_lv_align(uint8_t align);

/* 通用卡片标题渲染器：标题区固定 AREX_CARD_TITLE_H(40px)，
 * 文字 Y=5，分割线 Y=AREX_CARD_TITLE_H-2。下方内容区以 AREX_CARD_TITLE_H 为 Y=0 起点。 */
void arex_render_card_title(lv_obj_t *parent_card, const char *title_text);

/* 字体映射器：唯一允许将字体 ID 转换为真实 lvgl 字体指针的地方 */
const lv_font_t *arex_get_font(uint8_t font_id);

/* =========================================================
 * 9. 布局矩形计算 (供 rebuild 调用)
 * ========================================================= */
void arex_calc_layout_rect(int16_t *out_x, int16_t *out_y,
                          uint16_t *out_w, uint16_t *out_h,
                          int16_t anchor_offset_x, int16_t anchor_offset_y);

/* =========================================================
 * 9b. 右侧卡片动态菜单配置 (APP 同步核心)
 *
 * 每个菜单选项的描述结构体。APP 下发 JSON 即可改变菜单外观，
 * 渲染引擎通过 arex_render_dynamic_menu() 统一遍历，不做硬编码判断。
 * ========================================================= */
typedef struct {
    const char *title_text;      /* 左侧主文本 (可为空) */
    const char *value_badge;     /* 右侧数值/状态徽章 (可为空) */
    uint8_t     title_font_id;   /* 标题字体 ID: arex_font_id_t */
    uint8_t     value_font_id;   /* 徽章字体 ID: arex_font_id_t */
    uint8_t     border_width;    /* 边框粗细 px，0=无边框 */
    uint8_t     height_u;        /* 该选项高度 (单位 U，默认 0=用 h_menu_item) */
} arex_menu_item_cfg_t;

/* 菜单列表包装体 — 作为 arex_card_t.config_data 传入注册表 */
typedef struct {
    const arex_menu_item_cfg_t *items;
    uint8_t                     count;
} arex_menu_list_cfg_t;

/* 通用动态菜单工厂声明 */
void arex_render_dynamic_menu(lv_obj_t *parent_card,
                              const arex_menu_item_cfg_t *items,
                              uint8_t item_count,
                              int start_y,
                              lv_obj_t **out_item_handles);

/* =========================================================
 * 10. 5F 自定义网格渲染引擎 (靶向告警同步核心)
 * ========================================================= */

/* 5F 网格总线渲染器：遍历 g_sys_config.widget_* 数组，
 * 用纯数学行×列→绝对坐标映射渲染所有组件。
 * left_anchor_obj 传入用于告警引擎跨区搜索烙印对象。 */
void arex_render_5f_custom_grid(lv_obj_t *card_custom,
                                 lv_obj_t *left_anchor_obj);

/* 按 widget_id 设置数值（由 update 循环调用，绝不触发重绘） */
void arex_widget_set_value(arex_widget_id_t id, float value);

/* 5F 自定义网格重建（由 arex_screen_rebuild_layout 调用） */
void arex_5f_grid_rebuild(void);

/* 按 widget_id 设置字符串（用于 GAS 等非数值组件） */
void arex_widget_set_text(arex_widget_id_t id, const char *text);

/* 全局组件数据路由分发器：根据 widget_id 自动从 g_sensor_data 取值并刷新界面 */
void arex_widget_sync_data(arex_widget_id_t w_id);

/* 靶向告警触发：全屏搜索所有打了 user_data 烙印的组件并同步闪烁。
 * target_id = AREX_WIDGET_EMPTY 时仅弹出横幅，不做靶向同步。 */
void arex_trigger_alarm(arex_alarm_level_t level,
                        const char *eng_text,
                        arex_widget_id_t target_id);

/* 清除所有组件的告警样式（告警消失时调用） */
void arex_clear_all_alarm_styles(void);

/* 根据 widget_id 获取显示名称（供调试/横幅使用） */
const char *arex_get_widget_name(arex_widget_id_t id);

/* 告警横幅显示（由 arex_trigger_alarm 调用） */
void arex_show_alarm_banner(arex_alarm_level_t level, const char *eng_text);

/* =========================================================
 * 2c. 左侧 2x7 绝对网格组件配置 (APP 同步核心)
 *
 * 架构铁律：APP 只下发 [widget_id, x, y] 三字段（3字节）。
 * MCU 根据 widget_id 从样式表自动查表获取 span_w/span_h。
 * ========================================================= */
#define AREX_LEFT_COLS   2
#define AREX_LEFT_ROWS   7
#define AREX_LEFT_CELL_W 80
#define AREX_LEFT_CELL_H 60
#define AREX_LEFT_GRID_W (AREX_LEFT_COLS * AREX_LEFT_CELL_W)  /* 160px */
#define AREX_LEFT_GRID_H (AREX_LEFT_ROWS * AREX_LEFT_CELL_H)  /* 420px */

/* APP 下发数据结构（只含位置，无样式）已迁移到 arex_grid_widget_t */

/* 组件布局类型 */
typedef enum {
    AREX_LAYOUT_CENTER      = 0,  /* 通用居中：标题上 + 数值中 + 单位下 */
    AREX_LAYOUT_DIAGONAL   = 1,  /* 对角线：标题左上 + 数值右下 */
    AREX_LAYOUT_TOP_BOTTOM = 2,  /* 上下：标题顶部 + 数值底部 */
    AREX_LAYOUT_LEFT_RIGHT = 3,  /* 左右：数值左侧 + 单位/标题右侧 */
    AREX_LAYOUT_NDL_BAR    = 4,  /* NDL专属：进度条 + 数值 + 标题 */
    AREX_LAYOUT_DEPTH_SPLIT = 5, /* DEPTH专属：整数 + 小数 + 单位 + 箭头 */
} arex_layout_type_t;

/* =========================================================
 * 第二步：极简 BLE 通信布局结构体 (APP → MCU)
 *
 * 架构铁律：APP 只下发"意图和网格坐标"，MCU 包揽所有"内部像素级排版"。
 * BLE 协议中每个网格组件仅占 3 字节，彻底压榨协议体积。
 * ========================================================= */
#define AREX_MAX_WIDGETS 30

#pragma pack(push, 1)
typedef struct {
    arex_widget_id_t widget_id;  /* 组件类型 ID（必须与枚举严格对齐） */
    uint8_t x;                    /* 列索引 0~4 */
    uint8_t y;                    /* 行索引 0~5 */
} arex_widget_pos_t;
#pragma pack(pop)

/* =========================================================
 * 第三步：MCU 本地样式字典结构体 (Union 内存优化版)
 *
 * 架构铁律：APP 省略掉的 w 和 h 由 MCU 本地样式表提供。
 * 各种奇形怪状模块的专属 offset 用 Union 强制共享内存，防止结构体膨胀。
 * ========================================================= */

/* DEPTH 专属样式参数
 * 用于 DEPTH_1612/1606 等深度组件，实现整数+小数+单位+箭头图标分离排版 */
typedef struct {
    int8_t  int_offset_x;    /* 整数部分 X 偏移 */
    int8_t  int_offset_y;    /* 整数部分 Y 偏移 */
    uint8_t int_align;       /* 整数部分对齐方式 */
    int8_t  dec_offset_x;    /* 小数部分 X 偏移（相对整数） */
    int8_t  dec_offset_y;    /* 小数部分 Y 偏移（相对整数） */
    int8_t  unit_offset_x;   /* 单位 X 偏移（相对小数） */
    int8_t  unit_offset_y;   /* 单位 Y 偏移（相对小数） */
    int8_t  icon_offset_x;  /* 箭头图标 X 偏移 */
    int8_t  icon_offset_y;  /* 箭头图标 Y 偏移 */
    uint8_t icon_align;      /* 箭头图标对齐方式 */
} arex_style_depth_t;

/* NDL 专属样式参数
 * 用于 NDL_STOP_1606 等停留组件，实现进度条+数值分离排版 */
typedef struct {
    int8_t  bar_offset_x;   /* 进度条 X 偏移 */
    int8_t  bar_offset_y;   /* 进度条 Y 偏移 */
    uint8_t bar_align;      /* 进度条对齐方式 */
    int8_t  bar_w;          /* 进度条宽度 */
    int8_t  bar_h;          /* 进度条高度 */
    uint8_t bar_fill_dir;   /* 填充方向：0=从下往上, 1=从上往下 */
} arex_style_ndl_t;

/* NDL_STOP 多形态专属样式参数
 * 用于 NDL_STOP_1606，支持三种状态：NDL常态/Safety停留/Deco停留 */
typedef struct {
    /* 垂直进度条（NDL常态显示） */
    int8_t  vert_offset_x;  /* 垂直条 X 偏移 */
    int8_t  vert_offset_y;  /* 垂直条 Y 偏移 */
    uint8_t vert_align;      /* 垂直条对齐方式 */
    int8_t  vert_w;          /* 垂直条宽度 */
    int8_t  vert_h;          /* 垂直条高度 */
    /* 横向进度条（停留态显示） */
    int8_t  horiz_offset_x;  /* 横向条 X 偏移 */
    int8_t  horiz_offset_y;  /* 横向条 Y 偏移 */
    int8_t  horiz_w;         /* 横向条宽度 */
    int8_t  horiz_h;         /* 横向条高度 */

    /* =======================================
     * 常态 (Normal) 排版参数
     * ======================================= */
    int8_t  norm_main_x;  int8_t norm_main_y;  uint8_t norm_main_align; /* NDL 巨大数字 */
    int8_t  norm_sub_x;   int8_t norm_sub_y;   uint8_t norm_sub_align;  /* 底部 NDL 文本 */

    /* =======================================
     * 停留态 (Deco/Safety) 排版参数
     * ======================================= */
    int8_t  deco_title_x;  int8_t deco_title_y;  uint8_t deco_title_align; /* 顶部 SAFETY/DECO */
    int8_t  deco_main_x;   int8_t deco_main_y;   uint8_t deco_main_align;  /* 停留倒计时 MM:SS */
    int8_t  deco_sub_x;    int8_t deco_sub_y;    uint8_t deco_sub_align;   /* Safety 悬浮的 NDL 文本 */
} arex_style_ndl_stop_t;

/* TISSUE 组织图专属样式参数
 * 用于 TISSUE_GF_4012/TISSUE_RAW_4012，实现16柱组织图排版 */
typedef struct {
    int8_t  chart_offset_x; /* 柱状图 X 偏移 */
    int8_t  chart_offset_y; /* 柱状图 Y 偏移 */
    uint8_t chart_align;     /* 柱状图对齐方式 */
    int8_t  bar_count;      /* 柱状图数量（固定16） */
    int8_t  bar_spacing;    /* 柱子间距 */
} arex_style_tissue_t;

/* COMPASS 罗盘专属样式参数
 * 用于 COMPASS_1612，实现卷尺+数值分离排版 */
typedef struct {
    int8_t  tape_offset_x;  /* 卷尺 X 偏移 */
    int8_t  tape_offset_y;  /* 卷尺 Y 偏移 */
    uint8_t tape_align;      /* 卷尺对齐方式 */
    int8_t  val_offset_x;   /* 航向数值 X 偏移 */
    int8_t  val_offset_y;   /* 航向数值 Y 偏移 */
    uint8_t val_align;      /* 航向数值对齐方式 */
} arex_style_compass_t;

/* 通用基础样式（无特殊参数的组件使用）
 * 用于 TEMP/TIME/TTS/BATT/POD 等1x1或2x1通用组件 */
typedef struct {
    int8_t  value_offset_x; /* 数值 X 偏移 */
    int8_t  value_offset_y; /* 数值 Y 偏移 */
    uint8_t value_align;     /* 数值对齐方式 */
} arex_style_basic_t;

/* =========================================================
 * 第三步：8-bit UI 元素开关掩码 (Element Bitmask)
 *
 * 每 bit 对应一个 UI 零件的有无，字典中按 PRD 约束"按需勾选"。
 * 渲染工厂完全依据此掩码决定流水线装配哪些零件，彻底消灭 if-else 膨胀。
 * ========================================================= */
/* ELEM_TITLE : 标题行（如 "DEPTH"、"TEMP"） */
#define ELEM_TITLE (1 << 0)
/* ELEM_VALUE  : 主数值（整数/小数/字符串） */
#define ELEM_VALUE (1 << 1)
/* ELEM_UNIT   : 单位字符串（"m"、"min"、"C" 等，NULL 单位自动跳过） */
#define ELEM_UNIT  (1 << 2)
/* ELEM_BAR    : 特殊进度条/图标（DEPTH 速率箭头、NDL 停留柱、SYS 电池条等） */
#define ELEM_BAR   (1 << 3)
/* ELEM_EXTRA  : 附加异构元素（POD1/POD2 专属 ID 标签等） */
#define ELEM_EXTRA (1 << 4)

/* ELEM_SYS_BAR: 底部 SYS 区域标记（WIDGET_SYS_1606 组件化渲染） */
#define ELEM_SYS_BAR (1 << 5)

/* MCU 本地样式字典（Union 共享内存，大小永远等于最大成员） */
#define AREX_MAX_STYLE_SPEC_SIZE 32
typedef struct {
    arex_widget_id_t widget_id;   /* 绑定的组件 ID */
    uint8_t span_w;              /* 跨越列数 */
    uint8_t span_h;              /* 跨越行数 */

    /* 元素开关掩码，决定流水线要装配哪些零件 */
    uint8_t elements;            /* ELEM_TITLE | ELEM_VALUE | ELEM_UNIT | ELEM_BAR | ELEM_EXTRA */

    arex_font_id_t font_id;      /* 主数值字号 */
    arex_font_id_t title_font_id; /* 标题字号 */
    const char *unit;            /* 单位字符串（如 "m"、"min"、NULL） */
    int8_t  title_offset_x;
    int8_t  title_offset_y;
    uint8_t title_align;

    /* 专属样式 Union（强制共享内存，防止膨胀） */
    union {
        arex_style_depth_t        depth;                            /* DEPTH专属排版参数 */
        arex_style_ndl_t          ndl;                              /* NDL 专属参数（进度条/停留态） */
        arex_style_ndl_stop_t     ndl_stop;                         /* NDL_STOP专属排版参数 */
        arex_style_tissue_t       tissue;                           /* TISSUE专属排版参数 */
        arex_style_compass_t      compass;                          /* COMPASS专属排版参数 */
        arex_style_basic_t        basic;                            /* 通用组件排版参数 */
        uint8_t                   dummy[AREX_MAX_STYLE_SPEC_SIZE];  /* 强制对齐 */
    } spec;

    /* 组件显示名称 */
    const char *title;
} arex_widget_style_t;

/* 辅助查表函数声明 */
const arex_widget_style_t* arex_get_widget_style(arex_widget_id_t id);

/* =========================================================
 * POD 渲染状态机（POD1/POD2 共用同一枚举值，靠计数器区分）
 *
 * 策略：静态渲染计数器，同一渲染批次内按顺序分配 POD1→POD2。
 * 每次网格重建/重绘前必须调用 arex_reset_widget_render_state() 归零。
 * ========================================================= */
void arex_reset_widget_render_state(void);
#define ALIGN_TL LV_ALIGN_TOP_LEFT
#define ALIGN_TM LV_ALIGN_TOP_MID
#define ALIGN_TR LV_ALIGN_TOP_RIGHT
#define ALIGN_LM LV_ALIGN_LEFT_MID
#define ALIGN_CT LV_ALIGN_CENTER
#define ALIGN_RM LV_ALIGN_RIGHT_MID
#define ALIGN_BL LV_ALIGN_BOTTOM_LEFT
#define ALIGN_BM LV_ALIGN_BOTTOM_MID
#define ALIGN_BR LV_ALIGN_BOTTOM_RIGHT

/* 5F 网格组件配置已迁移到 g_sys_config.custom_5f_widgets[] */

/* 外部告警状态容器（由 arex_screen.c 在创建锚点和卡片时注入） */
extern lv_obj_t *g_left_anchor_obj;
extern lv_obj_t *g_card_custom_obj;

/* 5F 网格坐标推算：支持 title_zone_h 避让偏移，确保网格落在标题区下方 */
void arex_calc_widget_grid(uint16_t parent_w, uint16_t parent_h,
                           uint8_t row, uint8_t col,
                           uint8_t span_w, uint8_t span_h,
                           int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h);

/* =========================================================
 * 12. 左侧 2x7 绝对网格渲染引擎
 *
 * 严格将 160x420 区域划分为 2列(80px) x 7行(60px) 的绝对网格矩阵，
 * 彻底废弃 current_y 累加排版，改用 x*y*w*h 纯数学坐标推演。
 * 内部调用 render_widget_by_id 工厂函数，兼容 arex_widget_id_t 体系。
 * 样式由 arex_get_widget_style(widget_id) 自动查表获取，无需手动配置。
 * ========================================================= */

/* 左侧网格总线渲染器：遍历 g_left_widgets[] 数组，
 * 用纯数学 cell_w * cell_h 推算绝对坐标并渲染所有组件。
 * left_anchor 传入用于告警引擎跨区搜索烙印对象。 */
void arex_render_left_anchor_grid(lv_obj_t *left_anchor);

/* 通用组件工厂（左侧网格 + 5F 共用）：
 * 接收绝对物理坐标，生成标准化组件容器。
 * 渲染时自主查字典判断是否需要绘制速率图标（根据 elements & ELEM_BAR 决定）。
 * cfg_font_id 可覆盖默认字号计算（设为 255 则自动计算）。
 * 返回组件容器对象句柄。 */
lv_obj_t *render_widget_by_id(lv_obj_t *parent,
                               arex_widget_id_t w_id,
                               int16_t abs_x, int16_t abs_y,
                               uint16_t abs_w, uint16_t abs_h,
                               uint8_t span_w, uint8_t span_h,
                               arex_font_id_t cfg_font_id);

/* =========================================================
 * 第五步：新简化工厂函数（APP下发位置 + MCU本地查样式表）
 *
 * 架构：APP 只下发 [widget_id, x, y]，MCU 根据 widget_id
 * 自动从样式注册表获取 w/h/offset，渲染时组合两者。
 *
 * @param parent   父容器
 * @param pos      APP下发的极简坐标（仅含ID/X/Y，3字节）
 * @param cell_w   网格单元宽度（默认80px或根据区域不同）
 * @param cell_h   网格单元高度（默认60px或根据区域不同）
 * @param title_h  标题区高度偏移（0则无偏移）
 * @return         组件容器对象句柄
 * ========================================================= */
lv_obj_t* arex_render_widget(lv_obj_t *parent,
                              const arex_widget_pos_t *pos,
                              uint16_t cell_w, uint16_t cell_h,
                              uint16_t title_h);

/* UI 消费任务 — 全系统唯一允许执行 lv_label_set_text 的地方（50ms 定时器驱动） */
void arex_ui_update_task(lv_timer_t *timer);

#ifdef __cplusplus
}
#endif

#endif /* AREX_UI_ENGINE_H */
