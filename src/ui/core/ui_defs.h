/*
 * 文件: src/app_ui/ui/core/ui_defs.h
 * 作用: 该文件属于 UI 核心模块，负责状态机、数据桥接、事件分发、更新调度或 UI 运行时公共定义。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef UI_DEFS_H
#define UI_DEFS_H

#include "lvgl/lvgl.h"
#include "ui_settings.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化显示开关：1=组件刚创建时显示占位值，0=等真实数据刷新后再显示。 */
#define SHOW_PLACEHOLDER_ON_INIT  1

/* 基础布局单位：1U=10px。safe zone、gap、菜单高度等配置会按这个单位换算。 */
#define BASE_U  10

/* 横向固定栏模式的最小顶部区域高度保护，低于这个值时布局容易挤压。 */
#define MIN_CLASSIC_TOP_H  200

/* 安全区越界保护距离，用于判断 mask/offset 是否把内容推得太靠近屏幕边缘。 */
#define MASK_EDGE_GUARD  80

/* 物理屏幕尺寸：PC 仿真器当前是 640x480，嵌入式移植时也按这个显示坐标系设计。 */
#define PHYSICAL_W  640
#define PHYSICAL_H  480

/* 固定栏基准尺寸：侧边固定栏宽 160px；上/下固定栏高 120px。 */
#define LEFT_ANCHOR_W  160
#define TOP_ANCHOR_H   120

/* 卡片/菜单标题区高度。菜单列表默认从标题区下方开始，再由 MENU_LIST_TOP_NUDGE_PX 微调。 */
#define CARD_TITLE_H  60

/* DECO 卡片的低频刷新周期，单位 ms；避免组织/减压区域每帧都重算重绘。 */
#define DECO_REFRESH_MS  1000

/* UI 主色板：GREEN=主荧光绿，LIGHT=高亮文字，DARK=暗边框，BLACK=底色，BG=全局背景。 */
#define GREEN  lv_color_make(0x00, 0xFF, 0x00)
#define LIGHT  lv_color_make(0x55, 0xFF, 0x55)
#define DARK   lv_color_make(0x00, 0x33, 0x00)
#define BLACK  lv_color_make(0x00, 0x00, 0x00)
#define BG     lv_color_make(0x05, 0x05, 0x05)

/* 全局布局调试边框：1=打开页面/容器辅助边框，0=关闭。 */
#define DEBUG_BORDERS  0

/* 卡片内控件调试边框：1=显示菜单行/组件等内部边框，方便对齐；正式视觉可按需关闭。 */
#define CARD_DEBUG_BORDERS  1

/* 通用内部边框宽度，主要用于菜单项、卡片内框等绿色线条。 */
#define INNER_BORDER_W  2

/* 菜单列表上移量：数值越大，INFO/DIVE MENU 的选项越靠近标题。 */
#define MENU_LIST_TOP_NUDGE_PX  8

/* 菜单滚动容器边缘缓冲：防止第一项/最后一项选中边框被 LVGL 裁掉。 */
#define MENU_LIST_EDGE_PAD_PX  2

/* 组件标题贴边微调：左对齐标题向左挪、右对齐标题向右挪，用来抵消容器 padding 和字形留白。 */
#define COMP_TITLE_EDGE_NUDGE_PX  3

/* 组件数值贴边微调：左/右对齐的大数值向边缘贴近，正值表示贴边更强。 */
#define COMP_VALUE_EDGE_NUDGE_PX  3

/* GAS 卡片专用边框宽度。 */
#define GAS_BORDER_W  2

/* 自定义网格辅助边框宽度：0=不显示网格边框，调试布局时可临时调大。 */
#define GRID_BORDER_W  0

/* 单帧 BLE/TCP 布局最多携带的自定义组件数量。 */
#define MAX_WIDGETS  30

/* 侧边固定栏网格：左/右布局使用 2列 x 7行。 */
#define FIXED_SIDE_COLS  2
#define FIXED_SIDE_ROWS  7

/* 横向固定栏网格：上/下布局使用 7列 x 2行。 */
#define FIXED_TOP_COLS  7
#define FIXED_TOP_ROWS  2

/* 侧边布局下的自定义卡网格：5列 x 6行。 */
#define CUSTOM_SIDE_COLS  5
#define CUSTOM_SIDE_ROWS  6

/* 上/下布局下的自定义卡网格：7列 x 4行。 */
#define CUSTOM_TOP_COLS  7
#define CUSTOM_TOP_ROWS  4

/* 固定栏最大行数兼容值，主要给旧逻辑/数组容量保护用，不代表当前实际网格一定有 8 行。 */
#define MAX_LEFT_ROWS  8

/* 固定栏和内容区之间分隔条的厚度。 */
#define ANCHOR_SEP_THICK  3

/* 固定栏和内容区之间分隔条样式，当前使用实线。 */
#define ANCHOR_SEP_STYLE  SEP_SOLID

/* 上升速率一级警告阈值，单位 m/min。 */
#define RATE_LEVEL1_THRESHOLD  3.0f

/* 上升速率二级警告阈值，单位 m/min。 */
#define RATE_LEVEL2_THRESHOLD  9.0f

/* 上升速率静止判定阈值，沿用 ui_settings.h 里的可配置死区。 */
#define RATE_STILL_THRESHOLD  UI_ASCENT_RATE_STILL_DEADBAND_MPM

/* 气体槽数量上限：gas slot、PPO2、MOD 等数组按这个容量组织。 */
#define GAS_COUNT  5

typedef enum
{
    FONT_ID_SMALL = 0,
    FONT_ID_TITLE,
    FONT_ID_MEDIUM,
    FONT_ID_LARGE,
    FONT_ID_HUGE,
    FONT_ID_NDL,
} font_id_t;

typedef enum
{
    THEME_TECH = 0,
    THEME_CLASSIC
} theme_t;

typedef enum
{
    ORDER_NORMAL = 0,
    ORDER_REVERSE
} order_t;

typedef enum
{
    CONSERVATISM_LOW = 0,
    CONSERVATISM_MED,
    CONSERVATISM_HIGH,
    CONSERVATISM_CUSTOM,
    CONSERVATISM_COUNT
} conservatism_level_t;

typedef enum
{
    DOTS_RIGHT = 0,
    DOTS_LEFT,
    DOTS_BOTTOM,
    DOTS_NONE
} dots_pos_t;

typedef enum
{
    BRIGHTNESS_LOW = 0,
    BRIGHTNESS_MED,
    BRIGHTNESS_HIGH,
    BRIGHTNESS_MAX,
    BRIGHTNESS_COUNT
} brightness_level_t;

typedef enum
{
    COMPASS_CLASSIC = 0,
    COMPASS_AERO,
    COMPASS_SUB
} compass_style_t;

typedef enum
{
    ALIGN_LEFT   = 0,
    ALIGN_CENTER,
    ALIGN_RIGHT
} align_t;

typedef enum
{
    SEP_NONE    = 0,
    SEP_SOLID,
    SEP_DASHED,
    SEP_DOTTED
} sep_style_t;

typedef enum
{
    COMP_TYPE_UNSPECIFIED = 0,
    COMP_EMPTY            = 0,
    COMP_NDL_STOP_1606    = 1,
    COMP_DEPTH_1612       = 2,
    COMP_DEPTH_1606       = 3,
    COMP_DIVE_TIME_1606   = 4,
    COMP_GAS_1606         = 5,
    COMP_SYS_1606         = 6,
    COMP_TEMP_0806        = 10,
    COMP_TIME_1606        = 11,
    COMP_TTS_0806         = 12,
    COMP_ASCENT_0806      = 13,
    COMP_ASCENT_0812      = 14,
    COMP_COMPASS_1612     = 15,
    COMP_BATTERY_0806     = 16,
    COMP_STOP_DEPTH_0806  = 17,
    COMP_STOP_TIME_1606   = 18,
    COMP_PPO2_0806        = 19,
    COMP_SURF_GF_0806     = 20,
    COMP_GF99_0806        = 21,
    COMP_CNS_0806         = 22,
    COMP_OTU_0806         = 23,
    COMP_GF_0806          = 24,
    COMP_MOD_0806         = 25,
    COMP_CEILING_0806     = 26,
    COMP_GAS_MIX_1606     = 27,
    COMP_TISSUE_GF_4012   = 28,
    COMP_TISSUE_RAW_4012  = 29,
    COMP_GAS_DENS_0806    = 30,
    COMP_FIO2_0806        = 31,
    COMP_HEADING_0806     = 32,
    COMP_POD_0806         = 33,
    COMP_DEPTH_MAX_0806   = 34,
    COMP_DEPTH_AVG_0806   = 35,
    COMP_TEMP_MIN_0806    = 36,
    COMP_TEMP_AVG_0806    = 37,

    //临时的测试组件，后续会根据实际需要调整增删
    COMP_GYRO_2406        = 38,  /* 陀螺仪角速度 */
    COMP_BATT_V_0806      = 39,  /* 电池电压 */
    COMP_BATT_TEMP_0806   = 40,  /* 电池温度 */
    COMP_PRJ_TEMP_0806    = 41,  /* 主板/项目温度 */
    COMP_CHARGE_0806      = 42,  /* 充电状态 */
    COMP_PRESSURE_0806    = 43,  /* 环境压力 */
    COMP_NOFLY_0806       = 44,  /* 禁飞时间 */
    COMP_ACCEL_2406       = 45,  /* 加速度计 */
    COMP_MAG_2406         = 46,  /* 磁力计xyz数据 */
    COMP_TMAG_2406        = 47,  /* 总磁场xyz强度 */
    COMP_ATTITUDE_2406    = 48,  /* 姿态角数据 */
    COMP_BLE_RSSI_0806    = 49,  /* BLE 信号强度 */
    COMP_CPU_0806         = 50,  /* CPU 占用/负载 */
    COMP_FPS_0806         = 51,  /* UI 帧率 */
    COMP_SENSOR_STAT_1606 = 52,  /* 传感器状态 */
    COMP_MLX_2406         = 53   /* MLX磁场强度 */
} comp_id_t;

typedef enum
{
    ALARM_NONE  = 0,
    ALARM_INFO  = 1,
    ALARM_WARN  = 2,
    ALARM_CRIT  = 3,
} alarm_level_t;

/* 报警文本是否显示级别前缀：1=显示 WARN/CRIT 等前缀，0=只显示报警正文。 */
#define ALARM_SHOW_PREFIX  0

extern const char *GAS_NAMES[GAS_COUNT];
extern const uint8_t GAS_MOD_M[GAS_COUNT];

#ifdef __cplusplus
}
#endif

#endif /* UI_DEFS_H */
