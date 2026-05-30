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

#define SHOW_PLACEHOLDER_ON_INIT  1
#define BASE_U             10
#define MIN_CLASSIC_TOP_H  200
#define MASK_EDGE_GUARD    80
#define PHYSICAL_W    640
#define PHYSICAL_H    480
#define LEFT_ANCHOR_W  160
#define CARD_TITLE_H  60
#define DECO_REFRESH_MS  1000

#define GREEN   lv_color_make(0x00, 0xFF, 0x00)
#define LIGHT   lv_color_make(0x55, 0xFF, 0x55)
#define DARK    lv_color_make(0x00, 0x33, 0x00)
#define BLACK   lv_color_make(0x00, 0x00, 0x00)
#define BG      lv_color_make(0x05, 0x05, 0x05)

#define DEBUG_BORDERS   0
#define CARD_DEBUG_BORDERS  1
#define INNER_BORDER_W  2
#define GAS_BORDER_W    2
#define GRID_BORDER_W   0

#define MAX_WIDGETS    30
#define COMP_GRID_COLS    5
#define COMP_GRID_ROWS    6
#define MAX_LEFT_ROWS    8

#define ANCHOR_SEP_THICK  3
#define ANCHOR_SEP_STYLE  SEP_SOLID

#define RATE_LEVEL1_THRESHOLD  3.0f
#define RATE_LEVEL2_THRESHOLD  9.0f
#define RATE_STILL_THRESHOLD   UI_ASCENT_RATE_STILL_DEADBAND_MPM

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
    COMP_GYRO_1606        = 38
} comp_id_t;

typedef enum
{
    ALARM_NONE  = 0,
    ALARM_INFO  = 1,
    ALARM_WARN  = 2,
    ALARM_CRIT  = 3,
} alarm_level_t;

#define ALARM_SHOW_PREFIX  0

extern const char *GAS_NAMES[GAS_COUNT];
extern const uint8_t GAS_MOD_M[GAS_COUNT];

#ifdef __cplusplus
}
#endif

#endif /* UI_DEFS_H */
