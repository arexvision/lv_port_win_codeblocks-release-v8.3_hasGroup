#include "arex_ui_engine.h"
#include "arex_card_registry.h"
#include "arex_screen.h"
#include "fonts/arex_fonts.h"
#include "arex_data.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ============================================================
 * 速率指示器图片资源（6级动态箭头）
 * ============================================================ */
LV_IMG_DECLARE(sudo_up_level0);
LV_IMG_DECLARE(sudo_up_level1);
LV_IMG_DECLARE(sudo_up_level2);
LV_IMG_DECLARE(sudo_down_level0);
LV_IMG_DECLARE(sudo_down_level1);
LV_IMG_DECLARE(sudo_down_level2);

/* ============================================================
 * 速率图标指针阵列（支持多个 DEPTH 模块同时存在）
 * 最多支持屏幕上出现 MAX_ASCENT_ICONS 个深度模块
 * (左侧锚点 1 个 + 5F 自定义网格多个)
 * ============================================================ */

/* ============================================================
 * NDL_STOP 多形态组件句柄（160x60 极限空间内的"变形金刚"）
 * 支持屏幕上多个 NDL 模块（左侧锚点 1 个 + 5F 多个）
 * 三种状态: NDL常态 / Safety停留 / Deco停留
 * ============================================================ */
lv_obj_t *s_img_ascent_rate[MAX_ASCENT_ICONS];
uint8_t  s_ascent_icon_count = 0;
ndl_handle_t s_ndl_handles[MAX_NDL_ICONS];
uint8_t      s_ndl_handle_count = 0;

/* ============================================================
 * 罗盘卡片静态句柄（由 card_compass.c 持有）
 * 用于 arex_ui_update_task 中的零内存引擎刷新
 * ============================================================ */
extern lv_obj_t *s_compass_tape_obj;
extern lv_obj_t *s_heading_val_lbl;
extern lv_obj_t *s_heading_hint_lbl;

/* 减压跟踪节流时间戳（由 arex_ui_update_task 使用） */
static uint32_t _deco_last_refresh_ms = 0;

/* 气体名称表 (供全局引用) */
const char *AREX_GAS_NAMES[AREX_GAS_COUNT] = {
    "AIR",
    "NX 32",
    "TX 18/45",
    "O2 100%"
};

/* 气体 MOD 表 (单位: 米) */
const uint8_t AREX_GAS_MOD_M[AREX_GAS_COUNT] = {
    56,  /* AIR */
    34,  /* NX 32 */
    68,  /* TX 18/45 */
    6    /* O2 100% */
};

/* =========================================================
 * 全局单例定义
 * ========================================================= */
arex_sys_config_t  g_sys_config;
arex_sensor_data_t g_sensor_data;  //注意这个是全局变量，所有UI层都要用它。因为赋值是原子操作，可以放心大胆用（不需要加锁）

/*当你写下 g_sensor_data.depth = 15.5f; 时，编译器会在底层把它翻译成：
找到 g_sensor_data 的基地址 (0x20000000)。
加上 depth 的偏移量 (+0)。
直接生成一条单步汇编指令（如 STR），把 15.5 的二进制数据，像狙击枪一样，精准地打入 0x20000000 开始的 4 个字节里！
它根本不会碰 heading，也不会碰 battery。 这就是一次纯粹的、针对单个 32 位地址的“单指令写入”。因此，它是绝对原子的。
*/

static const arex_widget_style_t g_widget_styles[] = {
/* =========================================================
 * 第四步：MCU 本地只读 CSS 样式注册表
 *
 * 架构铁律：UI 工程师调整内部像素位移只需在这里改数字，编译即生效。
 * 完全不需要改 APP，也不需要改 BLE 协议。
 * ========================================================= */
    /* ========== 核心驻留组件 ========== */
    {
        .widget_id = WIDGET_DEPTH_1612,
        .span_w = 2, .span_h = 2,
        .elements = ELEM_VALUE | ELEM_UNIT | ELEM_BAR,  /* 2x2 大通栏：无 title，靠 spec.depth 做 int/dec/unit 分离 */
        .font_id = AREX_FONT_ID_HUGE,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m",
        .title = "DEPTH",
        .title_offset_x = 8, .title_offset_y = 4, .title_align = LV_TEXT_ALIGN_LEFT,
        .spec.depth = {
            .int_offset_x = 10, .int_offset_y = 30, .int_align = LV_TEXT_ALIGN_LEFT,
            .dec_offset_x = 2,  .dec_offset_y = 5,
            .unit_offset_x = 0, .unit_offset_y = 2,
            .icon_offset_x = -10, .icon_offset_y = 0, .icon_align = LV_ALIGN_RIGHT_MID
        }
    },
    {
        .widget_id = WIDGET_DEPTH_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m",
        .title = "DEPTH",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_TEXT_ALIGN_LEFT,
        .spec.depth = {
            .int_offset_x = 8, .int_offset_y = 0, .int_align = LV_ALIGN_LEFT_MID,
            .dec_offset_x = 2,  .dec_offset_y = 3,
            .unit_offset_x = 0, .unit_offset_y = 1,
            .icon_offset_x = -6, .icon_offset_y = 0, .icon_align = LV_ALIGN_RIGHT_MID
        }
    },
    {
        .widget_id = WIDGET_NDL_STOP_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT | ELEM_BAR,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "min",
        .title = "NDL",
        .title_offset_x = 0, .title_offset_y = 0, .title_align = LV_TEXT_ALIGN_LEFT,
        .spec.ndl_stop = {
            /* 基础 Bar 设置 */
            .vert_offset_x = 10, .vert_offset_y = 0, .vert_align = LV_ALIGN_LEFT_MID,
            .vert_w = 14, .vert_h = 40,
            .horiz_offset_x = 0, .horiz_offset_y = -4, .horiz_w = 140, .horiz_h = 6,
            /* 常态 (Normal) 排版 */
            .norm_main_x = -45, .norm_main_y = 0,  .norm_main_align = LV_ALIGN_RIGHT_MID,
            .norm_sub_x  = -10, .norm_sub_y  = -5, .norm_sub_align  = LV_ALIGN_BOTTOM_RIGHT,
            /* 停留态 (Stop) 排版 */
            .deco_title_x = 10,  .deco_title_y = 4,   .deco_title_align = LV_ALIGN_TOP_LEFT,
            .deco_main_x  = -10, .deco_main_y  = -5, .deco_main_align  = LV_ALIGN_RIGHT_MID,
            .deco_sub_x   = 10,  .deco_sub_y   = -14,.deco_sub_align   = LV_ALIGN_BOTTOM_LEFT
        }
    },
    {
        .widget_id = WIDGET_DIVE_TIME_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_BAR,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "DIVE",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_TEXT_ALIGN_CENTER,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_GAS_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "GAS",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_SYS_1606,
        .span_w = 2, .span_h = 1,
        .elements = 0,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "SYS",
        .title_offset_x = 0, .title_offset_y = 0, .title_align = LV_TEXT_ALIGN_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = 0, .value_align = LV_ALIGN_CENTER }
    },
    /* ========== 基础组件 ========== */
    {
        .widget_id = WIDGET_TEMP_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "C",
        .title = "TEMP",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_TIME_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "TIME",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_TTS_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "min",
        .title = "TTS",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_ASCENT_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m/m",
        .title = "RATE",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_ASCENT_0812,
        .span_w = 1, .span_h = 2,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT | ELEM_BAR,  /* 1x2 带 sudu 速率图标 */
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m/m",
        .title = "RATE",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_COMPASS_1612,
        .span_w = 2, .span_h = 2,
        .elements = ELEM_VALUE | ELEM_BAR,  /* 罗盘：无 title，靠 spec.compass 做 tape/val 分离 */
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "HDG",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_TEXT_ALIGN_CENTER,
        .spec.compass = { .tape_offset_x = 0, .tape_offset_y = 20, .tape_align = LV_ALIGN_TOP_MID,
                          .val_offset_x = 0, .val_offset_y = -4, .val_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_BATTERY_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "%",
        .title = "BATT",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_STOP_DEPTH_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m",
        .title = "STOP",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_STOP_TIME_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "min",
        .title = "STIME",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_PPO2_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "PPO2",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    /* ========== 技术潜水组件 ========== */
    {
        .widget_id = WIDGET_SURF_GF_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "SURF.GF",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_GF99_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "%",
        .title = "GF99",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_CNS_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "%",
        .title = "CNS",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_OTU_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "OTU",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_GF_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "GF",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_MOD_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m",
        .title = "MOD",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_CEILING_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m",
        .title = "CEIL",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_GAS_MIX_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "O2/He",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_TISSUE_GF_4012,
        .span_w = 4, .span_h = 2,
        .elements = ELEM_TITLE | ELEM_BAR,  /* 4x2 大图：title(Med) + tissue 柱状图，chart 由 spec.tissue 驱动 */
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_MEDIUM,
        .unit = NULL,
        .title = "TISSUE(GF)",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.tissue = { .chart_offset_x = 0, .chart_offset_y = 20, .chart_align = LV_ALIGN_TOP_MID,
                         .bar_count = 16, .bar_spacing = 2 }
    },
    {
        .widget_id = WIDGET_TISSUE_RAW_4012,
        .span_w = 4, .span_h = 2,
        .elements = ELEM_TITLE | ELEM_BAR,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_MEDIUM,
        .unit = NULL,
        .title = "TISSUE(RAW)",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.tissue = { .chart_offset_x = 0, .chart_offset_y = 20, .chart_align = LV_ALIGN_TOP_MID,
                         .bar_count = 16, .bar_spacing = 2 }
    },
    {
        .widget_id = WIDGET_GAS_DENS_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "g/L",
        .title = "DENS",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_FIO2_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "%",
        .title = "FIO2",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    /* ========== 传感器组件 ========== */
    {
        .widget_id = WIDGET_HEADING_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "HDG",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_POD_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,  /* ELEM_EXTRA → POD1/POD2 专属 ID 标签 */
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "",
        .title = "POD",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -2, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_DEPTH_MAX_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m",
        .title = "MAX D",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_DEPTH_AVG_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m",
        .title = "AVG D",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_TEMP_MIN_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "C",
        .title = "MIN T",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_TEMP_AVG_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "C",
        .title = "AVG T",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_EMPTY,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "TIME",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_EMPTY,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "PPO2 MAX",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_EMPTY,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "min",
        .title = "NDL MIN",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_EMPTY,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "l/m",
        .title = "SAC MAX",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_EMPTY,
        .span_w = 1, .span_h = 1,
        .elements = 0,
        .font_id = AREX_FONT_ID_SMALL,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = NULL,
        .title_offset_x = 0, .title_offset_y = 0, .title_align = LV_TEXT_ALIGN_CENTER,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = 0, .value_align = LV_ALIGN_CENTER }
    },
};

#define STYLE_COUNT (int)(sizeof(g_widget_styles) / sizeof(g_widget_styles[0]))

/* 查表函数 */
const arex_widget_style_t* arex_get_widget_style(arex_widget_id_t id)
{
    for (int i = 0; i < STYLE_COUNT; i++) {
        if (g_widget_styles[i].widget_id == id)
            return &g_widget_styles[i];
    }
    return NULL;
}

/* =========================================================
 * POD 单模具轮转分配状态机
 *
 * 架构：WIDGET_POD_0806 (33) 是全局唯一真实存在的气瓶模具。
 * APP 下发同一个 POD_0806 可以出现多次（如左侧锚点的 POD1+POD2，或 5F 中的多个）。
 * MCU 通过渲染计数器 s_pod_render_count 自动分配身份。
 *
 * 渲染时拦截 WIDGET_POD_0806，根据计数器判断：
 *   - 第1次遇到 (count=1, 奇数) → 分配为 POD1
 *   - 第2次遇到 (count=2, 偶数) → 分配为 POD2
 *
 * user_data 烙印使用高位掩码区分：
 *   - POD1: 1000 + WIDGET_POD_0806 = 1033
 *   - POD2: 2000 + WIDGET_POD_0806 = 2033
 * ========================================================= */
static uint8_t s_pod_render_count = 0;  /* POD 渲染计数器 */

#define POD_TAG_BASE  1000  /* POD 标签基准偏移 */
#define POD1_TAG      (POD_TAG_BASE + WIDGET_POD_0806)  /* 1033 */
#define POD2_TAG      (2 * POD_TAG_BASE + WIDGET_POD_0806)  /* 2033 */

/* =========================================================
 * SYS 模块全局静态指针（O(1) 直接访问，零遍历）
 * ========================================================= */
static lv_obj_t *s_sys_batt_lbl = NULL;      /* 电量百分比 */
static lv_obj_t *s_sys_temp_lbl = NULL;      /* 温度 */
static lv_obj_t *s_sys_strobe_img = NULL;    /* 留转灯图标 */
static lv_obj_t *s_sys_flash_img = NULL;     /* 手电筒图标 */
static lv_obj_t *s_sys_cyl_lbl = NULL;      /* 气瓶数量文本 "x0" */

/* =========================================================
 * 获取 POD 标签（根据当前渲染计数器返回值）
 * 返回 POD1_TAG 或 POD2_TAG，用于烙印到 user_data
 *
 * 注意：s_pod_render_count 已在 render_widget_by_id 中先递增。
 * 所以 count=1 时为第1个POD，count=2 时为第2个POD。
 * ========================================================= */
static uintptr_t arex_get_pod_tag(void)
{
    /* 第1次调用(count=1，奇数) → POD1_TAG
     * 第2次调用(count=2，偶数) → POD2_TAG */
    return (s_pod_render_count % 2 == 1) ? POD1_TAG : POD2_TAG;
}

/* =========================================================
 * 获取 POD 编号（返回 1 或 2）
 * ========================================================= */
static uint8_t arex_get_pod_index(void)
{
    /* 第1次调用(count=1，奇数) → POD1
     * 第2次调用(count=2，偶数) → POD2 */
    return (s_pod_render_count % 2 == 1) ? 1 : 2;
}

/* =========================================================
 * 渲染计数器归零（每次网格重建/重绘前必须调用）
 * 由 arex_screen_rebuild_layout() 或 left_anchor_create() 调用
 * ========================================================= */
void arex_reset_widget_render_state(void)
{
    s_pod_render_count = 0;

    /* 归零底部 SystemData 静态句柄，防止 lv_timer 访问死内存 */
    s_sys_batt_lbl     = NULL;
    s_sys_temp_lbl     = NULL;
    s_sys_strobe_img   = NULL;
    s_sys_flash_img    = NULL;
    s_sys_cyl_lbl      = NULL;
}

/* 左侧 2x7 绝对网格配置数组
 *
 * 160x420 区域 = 2列(80px) x 7行(60px)
 * 与 5F 卡片共用 arex_widget_id_t 枚举体系。
 *
 * Grid Layout:
 *   Row 0: NDL      | (占用 2x1 = 160x60)
 *   Row 1: DEPTH    | (占用 2x2 = 160x120, 带 sudu 速率图标)
 *   Row 2: (DEPTH 第二行)
 *   Row 3: POD1     | POD2    (各占用 1x1 = 80x60)
 *   Row 4: TIME     | (占用 2x1 = 160x60)
 *   Row 5: GAS      | (占用 2x1 = 160x60)
 *   Row 6: SYS      | (占用 2x1 = 160x60，SystemData 可配置)
 * ========================================================= */
arex_left_widget_t g_left_widgets[AREX_LEFT_MAX_WIDGETS] = {0};
uint8_t g_left_widget_count = 0;

/* 5F 自定义网格配置数组（与左侧锚点结构一致）
 *
 * 5x6 网格区域，由 APP 下发配置
 * 每个组件只含 widget_id + r/c 三字段，span_w/h 由 MCU 样式表自动查表
 * ========================================================= */
arex_5f_widget_t g_5f_widgets[AREX_5F_MAX_WIDGETS] = {0};
uint8_t g_5f_widget_count = 0;


/* 从 KV 持久化存储加载配置（weak 实现由具体平台覆盖） */
/* =========================================================
 * 默认配置值
 *
 * 当前实现的布局: Left Grid + Right Cards
 *   左侧: 160x420 固定 2列(x80) x 7行(y60) 网格
 *   右侧: tileview 滑动卡片 (INFO / 5F / DECO / COMPASS / GAS / PLAN / SETUP)
 *   安全区: 580x420 由 left_anchor(160) + right_cards(420) 组成
 *
 * 字段分组:
 *   [A] 活跃字段 — 当前渲染代码实际读取
 *   [R] 预留字段 — 已定义但渲染代码未使用，为未来 Classic 上下布局预留
 * ========================================================= */
void arex_sys_config_defaults(arex_sys_config_t *cfg)
{
    memset(cfg, 0, sizeof(arex_sys_config_t));

    /* ========== [A] 安全区 ========== */
    cfg->safe_zone_w  = 580;
    cfg->safe_zone_h  = 420;
    cfg->offset_x     = 0;            /* x=0 表示水平居中（左右各留白 3U） */
    cfg->offset_y     = -10;          /* y=-10 向上偏移（上面留白 2U，下面留白 4U） */

    /* ========== [A] 架构 ========== */
    cfg->layout_order  = AREX_ORDER_NORMAL;  /* 0=标准(左锚右卡)，1=翻转(右锚左卡) */
    cfg->dots_position = AREX_DOTS_LEFT;    /* tileview 指示点位置 */
    cfg->compass_style = AREX_COMPASS_CLASSIC;
    cfg->mask_enabled  = false;

    /* ========== [R] 主题模式预留（当前固定为 Left Grid + Right Cards） ==========
     * 可选扩展为 Classic 上下流式布局，届时渲染代码需读取以下字段：
     *   - theme_mode        → AREX_THEME_CLASSIC
     *   - h_depth / h_ndl / h_pod / h_batt / h_gas / h_time   → 上下分区高度
     *   - sep_style / sep_thick                                → 分割线样式
     *   - split_outward / flash_speed                          → 动画参数
     *   - title_h_u / h_menu_item / gap_menu                  → 菜单排版
     *   - h_tissues_chart                                     → 组织图高度
     */
    cfg->theme_mode    = AREX_THEME_TECH;    /* 当前固定 TECH（Left Grid + Right Cards） */
    cfg->sep_style     = AREX_SEP_DASHED;    /* [R] 分割线样式（待用） */
    cfg->sep_thick     = 2;                  /* [R] 线条粗细 px（待用） */
    cfg->split_outward = true;               /* [R] 双拼模块展开方向（待用） */
    cfg->flash_speed   = 1;                  /* [R] 动画闪烁速度（待用） */

    /* ========== [A] 分割线透明度 ========== */
    cfg->sep_alpha  = 51;   /* 20% of 255 — SystemData 顶部分割线透明度 */

    /* ========== [R] Classic 上下布局 10U 高度分配 (当前未使用) ==========
     * 1U = 10px，总计 10U = 100px（预留将来改为上下分区流式布局）
     * DEPTH 大通栏 → NDL/TTS 双拼 → POD 双拼 → BATT 双拼 → GAS → DIVE TIME
     */
    cfg->h_depth         = 8;   /* DEPTH 大通栏: 8U=80px */
    cfg->h_ndl           = 6;   /* NDL/TTS 双拼: 6U=60px */
    cfg->h_pod           = 6;   /* POD 1/2 双拼: 6U=60px */
    cfg->h_batt          = 5;   /* BATT/W.TIME 双拼: 5U=50px */
    cfg->h_gas           = 6;   /* GAS 中通栏: 6U=60px */
    cfg->h_time          = 5;   /* DIVE TIME 底部: 5U=50px */
    cfg->title_h_u       = 2;   /* [R] 标题高度（待用） */
    cfg->h_menu_item     = 5;   /* [R] 菜单项高度（待用） */
    cfg->gap_menu        = 1;   /* [R] 菜单项间距（待用） */
    cfg->h_tissues_chart = 9;   /* [R] 组织柱图高度（待用） */

    /* ========== [A] 面板间距 ========== */
    cfg->gap_u       = 0;   /* 左侧锚点与右侧面板间距: 0U=0px（由 sep_thick 负责分割线粗细） */
    cfg->panel_gap_u = 1;   /* tileview 容器间距: 1U=10px */

    /* ========== [A] 5F 自定义网格 (5列 x 6行) ==========
     *
     *  5列布局示意（5列=10格，6行）：
     *  col:  0  1  2  3  4
     *  row0: [DEPTH 2x2 大块    ] [TEMP  ] [HEADING 2 x1 ]
     *  row2: [空槽    ]          [BATT   ] [PPO2 1x1 ]
     *  row3: [NDL 2x1           ] [TTS 2x1 ] [CNS  1x1 ]
     *  row4: [POD1              ] [POD2    ] [空槽   ]
     *  row5: [空槽               ] [空槽    ] [空槽   ]
     *
     *  简洁位置配置：widget_id + r/c 三字段，span_w/h 由 MCU 样式表自动推导
     */
    g_5f_widget_count = 12;
    g_5f_widgets[0]  = (arex_5f_widget_t){ WIDGET_DEPTH_1612,     0, 0 };
    g_5f_widgets[1]  = (arex_5f_widget_t){ WIDGET_TEMP_0806,     0, 2 };
    g_5f_widgets[2]  = (arex_5f_widget_t){ WIDGET_HEADING_0806,  0, 3 };
    g_5f_widgets[3]  = (arex_5f_widget_t){ WIDGET_EMPTY,         2, 0 };  /* SAC 已移除 */
    g_5f_widgets[4]  = (arex_5f_widget_t){ WIDGET_BATTERY_0806,  2, 2 };
    g_5f_widgets[5]  = (arex_5f_widget_t){ WIDGET_PPO2_0806,     2, 4 };
    g_5f_widgets[6]  = (arex_5f_widget_t){ WIDGET_NDL_STOP_1606, 3, 0 };
    g_5f_widgets[7]  = (arex_5f_widget_t){ WIDGET_TTS_0806,      3, 2 };
    g_5f_widgets[8]  = (arex_5f_widget_t){ WIDGET_CNS_0806,      3, 4 };
    g_5f_widgets[9]  = (arex_5f_widget_t){ WIDGET_POD_0806,      4, 0 };
    g_5f_widgets[10] = (arex_5f_widget_t){ WIDGET_POD_0806,      4, 2 };
    g_5f_widgets[11] = (arex_5f_widget_t){ WIDGET_EMPTY,         4, 4 };  /* 保留空槽 */
    g_5f_widgets[12] = (arex_5f_widget_t){ WIDGET_EMPTY,         5, 0 };  /* 保留空槽 */

    /* ========== [A] 左侧 2x7 固定网格 (160x420) ==========
     * 160x420 区域 = 2列(80px) x 7行(60px)，由 arex_render_left_anchor_grid() 渲染
     *
     *  Grid Layout:
     *    Row 0: NDL      | (2x1 → 160x60)
     *    Row 1-2: DEPTH  | (2x2 → 160x120，带 sudu 速率图标)
     *    Row 3: POD1     | POD2    (各 1x1 → 80x60)
     *    Row 4: TIME     | (2x1 → 160x60)
     *    Row 5: GAS      | (2x1 → 160x60)
     *    Row 6: SYS      | (2x1 → 160x60，SystemData 可配置)
     */
    g_left_widget_count = 7;

    /* 简洁位置配置：APP 下发 widget_id + x/y，span_w/h 由 MCU 样式表自动推导 */
    g_left_widgets[0] = (arex_left_widget_t){ WIDGET_NDL_STOP_1606,   0, 0 };
    g_left_widgets[1] = (arex_left_widget_t){ WIDGET_DEPTH_1612,      0, 1 };
    g_left_widgets[2] = (arex_left_widget_t){ WIDGET_DIVE_TIME_1606,  0, 3 };  /* 潜水时间 */
    g_left_widgets[3] = (arex_left_widget_t){ WIDGET_GAS_1606,        0, 4 };
    g_left_widgets[4] = (arex_left_widget_t){ WIDGET_POD_0806,        0, 5 };
    g_left_widgets[5] = (arex_left_widget_t){ WIDGET_POD_0806,        1, 5 };
    g_left_widgets[6] = (arex_left_widget_t){ WIDGET_SYS_1606,        0, 6 };

    /* 动态计算实际 widget 数量（以最后一个非零 widget 为准） */
    g_left_widget_count = 0;
    for (int i = 0; i < AREX_LEFT_MAX_WIDGETS; i++) {
        if (g_left_widgets[i].widget_id != 0) {
            g_left_widget_count = i + 1;
        }
    }

    /* ========== [A] 右侧卡片顺序 (tileview 滑动顺序) ==========
     * card_order[pos] = card_id
     * INFO(0) / SETUP(7) 固定，中间 6 张可由 APP 重排
     */
    cfg->card_order[CARD_POS_INFO]  = CARD_ID_INFO;
    cfg->card_order[CARD_POS_1]     = CARD_ID_COMPASS;
    cfg->card_order[CARD_POS_2]     = CARD_ID_DECO;
    cfg->card_order[CARD_POS_3]     = CARD_ID_PLAN;
    cfg->card_order[CARD_POS_4]     = CARD_ID_GAS;
    cfg->card_order[CARD_POS_5]     = CARD_ID_CUSTOM_GRID;
    cfg->card_order[CARD_POS_6]     = CARD_ID_BLANK;      /* 空白卡片 */
    cfg->card_order[CARD_POS_SETUP] = CARD_ID_SETUP;

    /* ========== [A] 用户设置默认值 ========== */
    cfg->mod_ppo2       = 1.4f;
    cfg->conservatism   = 1;    /* MED */
    cfg->brightness     = 2;    /* HIGH */
}

/* =========================================================
 * 安全区边界检测
 * ========================================================= */
bool arex_safe_zone_in_danger(void)
{
    int16_t max_offset_x = (int16_t)((AREX_PHYSICAL_W - g_sys_config.safe_zone_w) / 2);
    int16_t max_offset_y = (int16_t)((AREX_PHYSICAL_H - g_sys_config.safe_zone_h) / 2);

    if (g_sys_config.offset_x < -max_offset_x || g_sys_config.offset_x > max_offset_x)
        return true;
    if (g_sys_config.offset_y < -max_offset_y || g_sys_config.offset_y > max_offset_y)
        return true;

    /* 面镜盲区掩膜检测 */
    if (g_sys_config.mask_enabled) {
        int16_t bottom_edge = (int16_t)(AREX_PHYSICAL_H / 2 + g_sys_config.safe_zone_h / 2 + g_sys_config.offset_y);
        if (bottom_edge > AREX_PHYSICAL_H - AREX_MASK_EDGE_GUARD)
            return true;
    }

    return false;
}

/* =========================================================
 * 辅助：计算 Safe Zone 内部可用区域
 * ========================================================= */
void arex_calc_layout_rect(int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h,
                           int16_t anchor_offset_x, int16_t anchor_offset_y)
{
    /* 以屏幕中心为原点，应用安全区偏移 */
    int16_t center_x = (int16_t)(AREX_PHYSICAL_W / 2) + anchor_offset_x;
    int16_t center_y = (int16_t)(AREX_PHYSICAL_H / 2) + anchor_offset_y;

    *out_x = center_x - (int16_t)(g_sys_config.safe_zone_w / 2);
    *out_y = center_y - (int16_t)(g_sys_config.safe_zone_h / 2);
    *out_w = g_sys_config.safe_zone_w;
    *out_h = g_sys_config.safe_zone_h;
}

/* =========================================================
 * Tech 模式绝对坐标推算
 *
 * 左锚点: (0, 0), 宽=160px, 高=全 safe_zone_h
 * 右卡片: (160+gap, 0), 宽=safe_zone_w-160-gap, 高=全 safe_zone_h
 * 翻转时: 交换左右 X
 * ========================================================= */
void arex_calc_tech_layout(int16_t *out_lx, int16_t *out_ly,
                           uint16_t *out_lw, uint16_t *out_lh,
                           int16_t *out_rx, int16_t *out_ry,
                           uint16_t *out_rw, uint16_t *out_rh)
{
    uint16_t gap = g_sys_config.gap_u * AREX_BASE_U;

    if (g_sys_config.layout_order == AREX_ORDER_NORMAL) {
        *out_lx = 0;
        *out_rx = (int16_t)(AREX_LEFT_ANCHOR_W + gap);
    } else {
        *out_lx = (int16_t)(g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W - gap);
        *out_rx = 0;
    }

    *out_ly = 0;
    *out_ry = 0;

    *out_lw = AREX_LEFT_ANCHOR_W;
    *out_lh = g_sys_config.safe_zone_h;

    *out_rw = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W - gap;
    *out_rh = g_sys_config.safe_zone_h;
}

/* =========================================================
 * Classic 模式绝对坐标推算
 *
 * 上区: (0, 0), 宽=safe_zone_w, 高=按 10U 累加计算
 * 下区: (0, top_h+gap), 宽=safe_zone_w, 高=safe_zone_h-top_h-gap
 * 翻转时: 交换上下 Y
 * ========================================================= */
void arex_calc_classic_layout(int16_t *out_top_x, int16_t *out_top_y,
                              uint16_t *out_top_w, uint16_t *out_top_h,
                              int16_t *out_bot_x, int16_t *out_bot_y,
                              uint16_t *out_bot_w, uint16_t *out_bot_h)
{
    uint16_t gap = g_sys_config.gap_u * AREX_BASE_U;

    /* 计算上区总高度 = 各模块高度累加 */
    uint16_t top_h = 0;
    top_h += g_sys_config.h_depth * AREX_BASE_U;               /* DEPTH */
    top_h += g_sys_config.h_ndl * AREX_BASE_U + gap;            /* NDL/TTS 双拼 */
    top_h += g_sys_config.h_pod * AREX_BASE_U + gap;            /* POD 双拼 */
    top_h += g_sys_config.h_batt * AREX_BASE_U + gap;           /* BATT 双拼 */
    top_h += g_sys_config.h_gas * AREX_BASE_U + gap;            /* GAS */
    top_h += g_sys_config.h_time * AREX_BASE_U;                 /* DIVE TIME */

    /* 零高度保护：最小 AREX_MIN_CLASSIC_TOP_H px */
    if (top_h < AREX_MIN_CLASSIC_TOP_H) top_h = AREX_MIN_CLASSIC_TOP_H;

    uint16_t bottom_h = (g_sys_config.safe_zone_h > top_h + gap)
                         ? (g_sys_config.safe_zone_h - top_h - gap)
                         : AREX_MIN_CLASSIC_TOP_H;

    if (g_sys_config.layout_order == AREX_ORDER_NORMAL) {
        *out_top_x = 0;
        *out_bot_x = 0;
        *out_top_y = 0;
        *out_bot_y = (int16_t)(top_h + gap);
    } else {
        *out_top_x = 0;
        *out_bot_x = 0;
        *out_top_y = (int16_t)(bottom_h + gap);
        *out_bot_y = 0;
    }

    *out_top_w = g_sys_config.safe_zone_w;
    *out_top_h = top_h;
    *out_bot_w = g_sys_config.safe_zone_w;
    *out_bot_h = bottom_h;
}


/* =========================================================
 * 5x6 网格布局推算
 *
 * 计算每个 widget 单元格的绝对位置。
 * 收到 row(0~5), col(0~4), w_span(1~2), h_span(1~2)
 * 直接算出 X = col * unit_w, Y = row * unit_h
 * ========================================================= */
void arex_calc_widget_cell(uint16_t parent_w, uint16_t parent_h,
                          uint8_t row, uint8_t col,
                          uint8_t w_span, uint8_t h_span,
                          int16_t *out_x, int16_t *out_y,
                          uint16_t *out_w, uint16_t *out_h)
{
    uint16_t unit_w = parent_w / AREX_WIDGET_COLS;  /* e.g. 92px if parent=460 */
    uint16_t unit_h = parent_h / AREX_WIDGET_ROWS;   /* e.g. 80px if parent=480 */

    *out_x = (int16_t)(col * unit_w);
    *out_y = (int16_t)(row * unit_h);
    *out_w = w_span * unit_w;
    *out_h = h_span * unit_h;

    /* 边界修正，防止越界 */
    if (*out_x + *out_w > parent_w) *out_w = parent_w - *out_x;
    if (*out_y + *out_h > parent_h) *out_h = parent_h - *out_y;
}

/* =========================================================
 * 16 柱组织图 X 坐标推算
 *
 * 底部对齐，16 等分柱状图。
 * 每根柱宽 = total_w / 16，X = i * col_w
 * ========================================================= */
void arex_calc_tissue_bars(uint16_t total_w, uint16_t bar_max_h,
                           int16_t out_x[16], uint16_t out_w[16])
{
    uint16_t col_w = total_w / 16;
    for (uint8_t i = 0; i < 16; i++) {
        out_x[i] = (int16_t)(i * col_w);
        out_w[i] = col_w;
    }
    (void)bar_max_h; /* 柱高由调用方按百分比算出，此处只返回 X 和 W */
}

/* =========================================================
 * LVGL 样式辅助
 * ========================================================= */

/* 将 AREX_ALIGN_* 转换为 LVGL 对齐方式 */
lv_text_align_t arex_align_to_lv(uint8_t align)
{
    if (align == AREX_ALIGN_LEFT)   return LV_TEXT_ALIGN_LEFT;
    if (align == AREX_ALIGN_CENTER) return LV_TEXT_ALIGN_CENTER;
    return LV_TEXT_ALIGN_RIGHT;
}

/* 将对齐转换为 LVGL ALIGN 常量 */
lv_align_t arex_align_to_lv_align(uint8_t align)
{
    if (align == AREX_ALIGN_LEFT)   return LV_ALIGN_LEFT_MID;
    if (align == AREX_ALIGN_CENTER) return LV_ALIGN_CENTER;
    return LV_ALIGN_RIGHT_MID;
}

/* =========================================================
 * 字体映射器 (Font Mapper)
 *
 * 全系统唯一允许将字体 ID 转换为真实 lvgl 字体指针的地方。
 * 所有配置结构体中保存的 title_font / val_font 均应为 arex_font_id_t 值。
 *
 * ID 映射表：
 *   AREX_FONT_ID_SMALL  (0) → lv_font_courier_14  14px  标签/单位/Badge
 *   AREX_FONT_ID_TITLE  (1) → lv_font_courier_20  20px  菜单项/卡片标题
 *   AREX_FONT_ID_MEDIUM (2) → lv_font_courier_28  28px  数据值
 *   AREX_FONT_ID_HUGE   (3) → lv_font_courier_58  58px  深度大数字(与HTML规范一致)
 * ========================================================= */
const lv_font_t *arex_get_font(uint8_t font_id)
{
    switch (font_id) {
        case AREX_FONT_ID_SMALL:  return AREX_FONT_SMALL;   /* 14px */
        case AREX_FONT_ID_TITLE:  return AREX_FONT_TITLE;   /* 20px */
        case AREX_FONT_ID_MEDIUM: return AREX_FONT_MEDIUM;  /* 28px */
        case AREX_FONT_ID_HUGE:   return AREX_FONT_HUGE;   /* 48px */
        default:                   return AREX_FONT_SMALL;   /* 兜底：永不为 NULL */
    }
}

/* =========================================================
 * JSON 配置解析 (用于 App 蓝牙同步 / SETUP 导入)
 * ========================================================= */
/*
 * 当接收到 JSON 配置时，按以下流程处理：
 *
 * 1. 解析 JSON 到临时结构体
 * 2. 调用 memcpy(&g_sys_config, &tmp, sizeof(...)) 覆盖
 * 3. 调用 arex_ui_apply_config() 重排 UI
 *
 * JSON 字段示例 (与 HTML configIds 对应):
 * {
 *   "theme_mode": "tech",
 *   "safe_zone_w": 580,
 *   "h_depth": 8,
 *   "h_ndl": 6,
 *   "widget_ids": [0, 1, 2, 3, 4, 5],
 *   "widget_w":  [2, 2, 1, 2, 2, 1],
 *   "widget_h":  [2, 1, 1, 2, 1, 1],
 *   ...
 * }
 */

/* =========================================================
 * 初始化入口 (由 UI_main 调用)
 *
 * 启动流程：
 *   1. 从 KV 持久化读取配置 → 成功则直接使用
 *   2. KV 无数据/读取失败 → 填入默认值
 *   3. 传感器数据清零（由 sim_tick_cb / 外部 API 实时写入）
 * ========================================================= */
void arex_ui_init(void)
{
    /* 1. 加载持久化配置，失败则用默认值保底 */
    if (!arex_config_load(&g_sys_config)) {
        arex_sys_config_defaults(&g_sys_config);
    }

    strcpy(g_sensor_data.gas_name, "AIR");
    /* 2. 传感器数据清零 */
    memset(&g_sensor_data, 0, sizeof(g_sensor_data));
}

/* =========================================================
 * 应用配置变更 (配置界面修改后调用此函数)
 * 1. 检测 safe zone 边界
 * 2. 重建 left anchor 排版
 * 3. 重建 right card 布局
 * ========================================================= */
void arex_ui_apply_config(void)
{
    /* 安全区边界校验 */
    if (arex_safe_zone_in_danger()) {
        /* TODO: 触发危险警告 UI */
    }

    /* TODO: 触发 arex_screen_rebuild_layout() 重建排版
     * 这将在重构 arex_screen.c 时实现
     */
}

/* =========================================================
 * 卡片顺序查询 (统一入口 — 替代旧的 g_arex_card_order)
 * ========================================================= */
uint8_t g_sys_card_order(uint8_t pos)
{
    if (pos >= AREX_CARD_COUNT) return 0;
    return g_sys_config.card_order[pos];
}

/* =========================================================
 * 通用动态菜单工厂
 * 所有尺寸从 g_sys_config 推算，不含硬编码像素值。
 * ========================================================= */
void arex_render_dynamic_menu(lv_obj_t *parent_card,
                              const arex_menu_item_cfg_t *items,
                              uint8_t item_count,
                              int start_y,
                              lv_obj_t **out_item_handles)
{
    if (!parent_card || !items || item_count == 0) return;

    int right_canvas_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                       - ((int)g_sys_config.gap_u * AREX_BASE_U);
    int item_w = right_canvas_w - 15;  /* 右侧 15px 呼吸距 */

    int current_y = start_y;
    for (uint8_t i = 0; i < item_count; i++) {
        const arex_menu_item_cfg_t *item_cfg = &items[i];
        /* height_u 默认 0 → 查 h_menu_item (单位 U) */
        int item_h = (int)(item_cfg->height_u > 0 ? item_cfg->height_u : g_sys_config.h_menu_item)
                   * AREX_BASE_U;
        /* gap_y 从 gap_menu (单位 U) 推算 */
        int gap_y = (int)g_sys_config.gap_menu * AREX_BASE_U;

        lv_obj_t *item = lv_obj_create(parent_card);
        lv_obj_remove_style_all(item);
        lv_obj_set_pos(item, 0, current_y);
        lv_obj_set_size(item, item_w, item_h);
        lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(item, AREX_DARK, 0);
        lv_obj_set_style_border_width(item, AREX_CARD_DEBUG_BORDERS ? item_cfg->border_width : 0, LV_PART_MAIN);
        lv_obj_set_style_radius(item, 0, 0);
        lv_obj_set_style_pad_all(item, 0, LV_PART_MAIN);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

        /* 标题 label */
        if (item_cfg->title_text) {
            lv_obj_t *title_lbl = lv_label_create(item);
            lv_label_set_text(title_lbl, item_cfg->title_text);
            lv_obj_set_style_text_font(title_lbl, arex_get_font(item_cfg->title_font_id), 0);
            lv_obj_set_style_text_color(title_lbl, AREX_GREEN, 0);
            lv_obj_set_size(title_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 12, 0);
            lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_DOT);
        }

        /* 右侧徽章 label */
        if (item_cfg->value_badge) {
            lv_obj_t *badge_lbl = lv_label_create(item);
            lv_label_set_text(badge_lbl, item_cfg->value_badge);
            lv_obj_set_style_text_font(badge_lbl, arex_get_font(item_cfg->value_font_id), 0);
            lv_obj_set_style_text_color(badge_lbl, AREX_LIGHT, 0);
            lv_obj_set_size(badge_lbl, 80, 28);
            lv_obj_align(badge_lbl, LV_ALIGN_RIGHT_MID, -12, 0);
            lv_obj_set_style_text_align(badge_lbl, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_long_mode(badge_lbl, LV_LABEL_LONG_DOT);
        }

        if (out_item_handles) {
            out_item_handles[i] = item;
        }

        current_y += item_h + gap_y;
    }
}

/* =========================================================
 * 通用卡片标题渲染器
 * 标题文字(Y=8)与分割线(Y=48)为视觉组合，绝对焊死在卡片顶部。
 * AREX_CARD_TITLE_H 仅作为下方"内容区(菜单/图表)的起始 Y 坐标偏移"。
 *
 * parent_card: 父容器（tile 对象）
 * title_text:  标题文字
 *
 * 标题布局（焊死，绝对不跟随 AREX_CARD_TITLE_H）：
 *   文字:   Y=8,  高度 40px，AREX_LIGHT 色
 *   分割线: Y=48, h=2px, AREX_DARK 色
 * ========================================================= */
void arex_render_card_title(lv_obj_t *parent_card, const char *title_text)
{
    uint16_t right_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                     - ((int)g_sys_config.gap_u * AREX_BASE_U);

    /* 1. 标题文字：扒光默认样式 + 强制小字号/次级颜色 */
    lv_obj_t *lbl = lv_label_create(parent_card);
    lv_obj_remove_style_all(lbl);
    lv_obj_set_pos(lbl, 16, 8);
    lv_obj_set_size(lbl, right_w - 32, 40);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(lbl, title_text);
    lv_obj_set_style_text_font(lbl, arex_get_font(AREX_FONT_ID_TITLE), 0);
    lv_obj_set_style_text_color(lbl, AREX_LIGHT, 0);

    /* 2. 分割线：绝对固定在文字下方（焊死 Y=48） */
    lv_obj_t *line = lv_obj_create(parent_card);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, right_w - 32, 2);
    lv_obj_set_pos(line, 16, 48);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(line, AREX_DARK, 0);
}

/* =========================================================
 * 5F 自定义网格组件外部容器（由 arex_screen.c 注入）
 * ========================================================= */
lv_obj_t *g_left_anchor_obj = NULL;
lv_obj_t *g_card_custom_obj = NULL;

/* 告警状态 */
static arex_alarm_level_t s_alarm_level = AREX_ALARM_NONE;
static char s_alarm_text[64] = {0};
static lv_timer_t *s_alarm_blink_timer = NULL;
static bool s_alarm_blink_on = false;
static lv_obj_t *s_alarm_banner = NULL;

/* 告警横幅静态样式（避免每次创建重复初始化） */
static lv_style_t s_banner_style_warn;
static lv_style_t s_banner_style_crit;

/* =========================================================
 * 5F 网格坐标推算（纯数学绝对映射，无 lv_grid）
 *
 * 核心公式：
 *   cell_w = parent_w / 5
 *   cell_h = parent_h / 6
 *   abs_x  = col * cell_w + gap
 *   abs_y  = row * cell_h + gap
 *   abs_w  = span_w * cell_w - gap*2
 *   abs_h  = span_h * cell_h - gap*2
 * ========================================================= */
#define WIDGET_GAP  0   /* 网格缝隙 px */

/* =========================================================
 * 5F 网格坐标推算（锁定 5 列 + 标题避让，动态 cell_h 自适应）
 *
 * parent_w/parent_h: 父容器总尺寸（用于动态推算）
 * row/col: 网格行列索引(0~5 / 0~4)
 * span_w/span_h: 跨越的列数/行数
 * out_*: 输出绝对坐标
 *
 * 排版矩阵严格锁定 5 列：
 *   cell_w = parent_w / 5
 *   cell_h = (parent_h - AREX_CARD_TITLE_H) / 6
 * Y 坐标增加 AREX_CARD_TITLE_H=60px 偏移，确保第一行落在标题区下方。
 * 宽高减 4px (2px 缝隙 x2) 制造四周 2px 物理留白。
 * 如果标题高度改为其他值，cell_h 会自动重新计算，内容区完美自适应。
 * ========================================================= */
void arex_calc_widget_grid(uint16_t parent_w, uint16_t parent_h,
                           uint8_t row, uint8_t col,
                           uint8_t span_w, uint8_t span_h,
                           int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h)
{
    /* 锁定 5 列基准，动态计算 cell_h */
    uint16_t cell_w = parent_w / 5;
    uint16_t cell_h = (parent_h > AREX_CARD_TITLE_H)
                      ? ((parent_h - AREX_CARD_TITLE_H) / AREX_WIDGET_ROWS)
                      : 60;  /* 保底 fallback */

    /* X: 列偏移 + 缝隙(2px) */
    *out_x = (int16_t)(col * cell_w + WIDGET_GAP);
    /* Y: 标题区下方 + 行偏移 + 缝隙(2px) */
    *out_y = (int16_t)(AREX_CARD_TITLE_H + row * cell_h + WIDGET_GAP);
    /* 宽高: 跨距×基准 - 4px 缝隙(四周各 2px) */
    *out_w = (uint16_t)(span_w * cell_w - WIDGET_GAP * 2);
    *out_h = (uint16_t)(span_h * cell_h - WIDGET_GAP * 2);

    /* 边界修正（以容器总尺寸为边界） */
    if (*out_x + *out_w > (int16_t)parent_w)
        *out_w = (uint16_t)((int16_t)parent_w - *out_x);
    if (*out_y + *out_h > (int16_t)parent_h)
        *out_h = (uint16_t)((int16_t)parent_h - *out_y);
}

/* 字号自适应引擎（已内联到 render_widget_by_id，保留函数体供未来扩展） */

/* =========================================================
 * 获取 widget 显示名称（从 g_widget_styles[] 读取 title 字段）
 * ========================================================= */
const char *arex_get_widget_name(arex_widget_id_t id)
{
    const arex_widget_style_t *style = arex_get_widget_style(id);
    if (!style) return "???";
    return style->title ? style->title : "";
}

/* =========================================================
 * 创建单个自定义组件（组件工厂 — 左侧网格 + 5F 共用）
 *
 * 关键：每个组件的 lv_obj_set_user_data() 存储了标签烙印。
 * 对于 POD，使用高位掩码区分（1033=POD1, 2033=POD2）。
 * 告警引擎靠这个烙印实现"左侧锚点 + 5F 组件同时闪烁"。
 *
 * 架构铁律：
 *   - 位置参数 (abs_x/y/w/h, span_w/h) 由调用方传入
 *   - 样式参数 (font, offsets) 由 arex_get_widget_style(w_id) 自动查表
 *   - cfg_font_id != 255 时强制覆盖自动字号
 *   - 速率图标由工厂自主查字典决定（根据 elements & ELEM_BAR）
 *   - 专属组件（DEPTH/NDL）走早期返回，内部仍读 style 参数
 *   - 通用组件按 elements 掩码装配流水线：TITLE → VALUE → UNIT → BAR
 *
 * POD 单模具轮转分配：
 *   - 函数入口检测 w_id == WIDGET_POD_0806
 *   - 调用 arex_get_pod_tag() 获得高位掩码标签 (1033/2033)
 *   - 调用 arex_get_pod_index() 获得 POD 编号 (1/2)
 *   - 将标签烙印到容器 user_data
 * ========================================================= */
lv_obj_t *render_widget_by_id(lv_obj_t *parent,
                              arex_widget_id_t w_id,
                              int16_t abs_x, int16_t abs_y,
                              uint16_t abs_w, uint16_t abs_h,
                              uint8_t span_w, uint8_t span_h,
                              arex_font_id_t cfg_font_id)
{
    /* ===== POD 单模具拦截：提前消耗计数器 ===== */
    bool is_pod_mold = (w_id == WIDGET_POD_0806);
    uint8_t pod_index = 0;        /* POD number 1 or 2 */
    uintptr_t pod_tag = 0;        /* POD tag 1033 or 2033 */
    if (is_pod_mold) {
        s_pod_render_count++;     /* Increment first, then get current value */
        pod_index = arex_get_pod_index();
        pod_tag = arex_get_pod_tag();
    }

    const arex_widget_style_t *style = arex_get_widget_style(w_id);
    if (!style) return NULL;

    /* 字号自适应（可被 cfg_font_id 覆盖）：
     *   2x2 大块 → AREX_FONT_ID_HUGE (48px)
     *   2x1 长条 → AREX_FONT_ID_MEDIUM (28px)
     *   1x1 小块 → AREX_FONT_ID_SMALL (14px) */
    arex_font_id_t val_font_id;
    if (cfg_font_id != (arex_font_id_t)255) {
        val_font_id = cfg_font_id;  /* 强制覆盖 */
    } else if (span_w >= 2 && span_h >= 2) {
        val_font_id = AREX_FONT_ID_HUGE;
    } else if (span_w >= 2) {
        val_font_id = AREX_FONT_ID_MEDIUM;
    } else {
        val_font_id = AREX_FONT_ID_SMALL;
    }

    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, abs_x, abs_y);
    lv_obj_set_size(obj, abs_w, abs_h);
    lv_obj_set_style_bg_color(obj, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, AREX_DARK, 0);
    lv_obj_set_style_border_width(obj, AREX_DEBUG_BORDERS ? 1 : 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 2, 0);

    /* 封杀所有滚动条 */
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);

    /* ===== 靶向告警烙印 =====
     * POD uses high-bit mask tags (1033/2033), others use raw w_id */
    if (is_pod_mold) {
        lv_obj_set_user_data(obj, (void *)pod_tag);
    } else {
        lv_obj_set_user_data(obj, (void *)(uintptr_t)w_id);
    }

    /* ===== DEPTH 2x2 专属渲染（整数+小数+单位分离） ===== */
    bool is_2x2 = (span_w >= 2 && span_h >= 2);
    if (w_id == WIDGET_DEPTH_1612 && is_2x2) {
        /* 样式参数来自 arex_widget_style_t */
        const arex_style_depth_t *s = &style->spec.depth;
        lv_obj_t *int_lbl = lv_label_create(obj);
        if (AREX_SHOW_PLACEHOLDER_ON_INIT) lv_label_set_text(int_lbl, "--");
        else lv_label_set_text_fmt(int_lbl, "%d", (int)g_sensor_data.depth);
        lv_obj_set_style_text_font(int_lbl, arex_get_font(AREX_FONT_ID_HUGE), 0);
        lv_obj_set_style_text_color(int_lbl, AREX_GREEN, 0);
        lv_obj_align(int_lbl, (lv_align_t)s->int_align, s->int_offset_x, s->int_offset_y);

        lv_obj_t *dec_lbl = lv_label_create(obj);
        if (AREX_SHOW_PLACEHOLDER_ON_INIT) lv_label_set_text(dec_lbl, ".-");
        else lv_label_set_text_fmt(dec_lbl, ".%d", (int)((g_sensor_data.depth - (int)g_sensor_data.depth) * 10 + 0.5f));
        lv_obj_set_style_text_font(dec_lbl, arex_get_font(AREX_FONT_ID_MEDIUM), 0);
        lv_obj_set_style_text_color(dec_lbl, AREX_GREEN, 0);
        lv_obj_align_to(dec_lbl, int_lbl, LV_ALIGN_OUT_RIGHT_TOP, s->dec_offset_x, s->dec_offset_y);

        lv_obj_t *unit_lbl = lv_label_create(obj);
        lv_label_set_text(unit_lbl, style->unit ? style->unit : "");
        lv_obj_set_style_text_font(unit_lbl, arex_get_font(AREX_FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(unit_lbl, AREX_LIGHT, 0);
        lv_obj_align_to(unit_lbl, dec_lbl, LV_ALIGN_OUT_BOTTOM_MID, s->unit_offset_x, s->unit_offset_y);

        /* 速率图标：工厂自主查字典判断是否需要绘制 */
        bool needs_bar_icon = (style->elements & ELEM_BAR) != 0;
        if (needs_bar_icon) {
            lv_obj_t *sudu_img = lv_img_create(obj);
            lv_img_set_src(sudu_img, &sudo_up_level0);
            lv_obj_align(sudu_img, (lv_align_t)s->icon_align, s->icon_offset_x, s->icon_offset_y);
            if (s_ascent_icon_count < MAX_ASCENT_ICONS)
                s_img_ascent_rate[s_ascent_icon_count++] = sudu_img;
        }
        return obj;
    } else if (w_id == WIDGET_NDL_STOP_1606) {
        /* NDL 变形金刚：从 style->spec.ndl_stop 读取所有位置参数 */
        if (s_ndl_handle_count >= MAX_NDL_ICONS) return obj;
        ndl_handle_t *h = &s_ndl_handles[s_ndl_handle_count++];
        h->comp = obj;

        const arex_style_ndl_stop_t *s = &style->spec.ndl_stop;
        /* 垂直进度条 */
        h->vert_bg = lv_obj_create(obj);
        lv_obj_remove_style_all(h->vert_bg);
        lv_obj_set_size(h->vert_bg, s->vert_w, s->vert_h);
        lv_obj_align(h->vert_bg, (lv_align_t)s->vert_align, s->vert_offset_x, s->vert_offset_y);
        lv_obj_set_style_border_width(h->vert_bg, 2, 0);
        lv_obj_set_style_border_color(h->vert_bg, AREX_GREEN, 0);
        lv_obj_set_style_radius(h->vert_bg, 4, 0);

        h->vert_fill = lv_obj_create(h->vert_bg);
        lv_obj_remove_style_all(h->vert_fill);
        lv_obj_set_size(h->vert_fill, LV_PCT(100), LV_PCT(60));
        lv_obj_align(h->vert_fill, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(h->vert_fill, AREX_GREEN, 0);
        lv_obj_set_style_bg_opa(h->vert_fill, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(h->vert_fill, 2, 0);

        /* 横向进度条 */
        h->horiz_bg = lv_obj_create(obj);
        lv_obj_remove_style_all(h->horiz_bg);
        lv_obj_set_size(h->horiz_bg, s->horiz_w, s->horiz_h);
        lv_obj_align(h->horiz_bg, LV_ALIGN_BOTTOM_MID, s->horiz_offset_x, s->horiz_offset_y);
        lv_obj_set_style_border_width(h->horiz_bg, 1, 0);
        lv_obj_set_style_border_color(h->horiz_bg, AREX_GREEN, 0);
        lv_obj_add_flag(h->horiz_bg, LV_OBJ_FLAG_HIDDEN);

        h->horiz_fill = lv_obj_create(h->horiz_bg);
        lv_obj_remove_style_all(h->horiz_fill);
        lv_obj_set_size(h->horiz_fill, LV_PCT(0), LV_PCT(100));
        lv_obj_align(h->horiz_fill, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_bg_color(h->horiz_fill, AREX_GREEN, 0);
        lv_obj_set_style_bg_opa(h->horiz_fill, LV_OPA_COVER, 0);

        /* 主干数值（常态初始位置） */
        h->main_val = lv_label_create(obj);
        lv_obj_set_style_text_color(h->main_val, AREX_GREEN, 0);
        lv_obj_set_style_text_font(h->main_val, arex_get_font(AREX_FONT_ID_HUGE), 0);
        lv_obj_align(h->main_val, (lv_align_t)s->norm_main_align, s->norm_main_x, s->norm_main_y);
        if (AREX_SHOW_PLACEHOLDER_ON_INIT)
            lv_label_set_text(h->main_val, "--");
        else
            lv_label_set_text_fmt(h->main_val, "%d", g_sensor_data.ndl);

        /* 顶部标题（默认隐藏，停留态时显示） */
        h->title_top = lv_label_create(obj);
        lv_obj_set_style_text_font(h->title_top, arex_get_font(AREX_FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(h->title_top, AREX_LIGHT, 0);
        lv_label_set_text(h->title_top, "");
        lv_obj_add_flag(h->title_top, LV_OBJ_FLAG_HIDDEN);

        /* 底部副标题（常态显示 NDL） */
        h->sub_bot = lv_label_create(obj);
        lv_obj_set_style_text_font(h->sub_bot, arex_get_font(AREX_FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(h->sub_bot, AREX_GREEN, 0);
        lv_obj_align(h->sub_bot, (lv_align_t)s->norm_sub_align, s->norm_sub_x, s->norm_sub_y);
        lv_label_set_text(h->sub_bot, "NDL");
        return obj;
    } else if (w_id == WIDGET_SYS_1606) {
        /* ===== SYS 模块：电池 + 温度 + 设备状态图标（O(1) 静态指针捕获） ===== */
        LV_IMG_DECLARE(liuzhuandeng);
        LV_IMG_DECLARE(Shoudiantong);
        LV_IMG_DECLARE(qiping);

        /* 左半部分：电量 Label —— 捕获指针 */
        s_sys_batt_lbl = lv_label_create(obj);
        lv_obj_set_style_text_font(s_sys_batt_lbl, arex_get_font(AREX_FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(s_sys_batt_lbl, AREX_GREEN, 0);
        lv_obj_align(s_sys_batt_lbl, LV_ALIGN_TOP_LEFT, 10, 2);
        if (AREX_SHOW_PLACEHOLDER_ON_INIT)
            lv_label_set_text(s_sys_batt_lbl, "--%");
        else
            lv_label_set_text_fmt(s_sys_batt_lbl, "%d%%", (int)g_sensor_data.battery_pct);

        /* 左半部分：温度 Label —— 捕获指针 */
        s_sys_temp_lbl = lv_label_create(obj);
        lv_obj_set_style_text_font(s_sys_temp_lbl, arex_get_font(AREX_FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(s_sys_temp_lbl, AREX_GREEN, 0);
        lv_obj_align(s_sys_temp_lbl, LV_ALIGN_BOTTOM_LEFT, 10, -2);
        if (AREX_SHOW_PLACEHOLDER_ON_INIT)
            lv_label_set_text(s_sys_temp_lbl, "-- C");
        else {
            int t_int = (int)g_sensor_data.temperature_c;
            int t_dec = (int)(fabsf(g_sensor_data.temperature_c - t_int) * 10);
            lv_label_set_text_fmt(s_sys_temp_lbl, "%d.%d C", t_int, t_dec);
        }

        /* 右半部分：设备状态图标 —— 捕获指针 */
        /* 1. 留转灯图标 */
        s_sys_strobe_img = lv_img_create(obj);
        lv_img_set_src(s_sys_strobe_img, &liuzhuandeng);
        lv_obj_align(s_sys_strobe_img, LV_ALIGN_TOP_RIGHT, -30, 0);
        lv_obj_set_style_img_opa(s_sys_strobe_img, g_sensor_data.strobe_on ? LV_OPA_COVER : LV_OPA_40, 0);

        /* 2. 手电筒图标 */
        s_sys_flash_img = lv_img_create(obj);
        lv_img_set_src(s_sys_flash_img, &Shoudiantong);
        lv_obj_align(s_sys_flash_img, LV_ALIGN_TOP_RIGHT, -10, 5);
        lv_obj_set_style_img_opa(s_sys_flash_img, g_sensor_data.flashlight_on ? LV_OPA_COVER : LV_OPA_40, 0);

        /* 3. 气瓶图标 */
        lv_obj_t *img_cyl = lv_img_create(obj);
        lv_img_set_src(img_cyl, &qiping);
        lv_obj_align(img_cyl, LV_ALIGN_BOTTOM_RIGHT, -40, 0);

        /* 4. 气瓶数量文本 "x0" —— 捕获指针 */
        s_sys_cyl_lbl = lv_label_create(obj);
        lv_obj_set_style_text_font(s_sys_cyl_lbl, arex_get_font(AREX_FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(s_sys_cyl_lbl, AREX_GREEN, 0);
        lv_obj_align(s_sys_cyl_lbl, LV_ALIGN_BOTTOM_RIGHT, -12, 0);
        if (AREX_SHOW_PLACEHOLDER_ON_INIT)
            lv_label_set_text(s_sys_cyl_lbl, "x0");
        else
            lv_label_set_text_fmt(s_sys_cyl_lbl, "x%d", g_sensor_data.cylinder_count);

        (void)img_cyl;
        return obj;
    }

    /* ===== 通用流水线：按 elements 掩码按需装配零件 =====
     * POD1/POD2/WTIME 及所有 1x1/2x1 通用组件走此路径
     * ELEM_TITLE → ELEM_VALUE → ELEM_UNIT → ELEM_BAR
     *
     * 样式参数全部来自 arex_get_widget_style(w_id) 查表结果
     * 仅 title 文本和数值数据源依赖 w_id 做 switch 分发 */

    /* --- 零件 1：标题 --- */
    if ((style->elements & ELEM_TITLE) && style->title) {
        lv_obj_t *title_lbl = lv_label_create(obj);
        /* POD 单模具：根据 pod_index 动态决定标题文字 */
        if (is_pod_mold) {
            lv_label_set_text_fmt(title_lbl, "POD %d", pod_index);
        } else {
            lv_label_set_text(title_lbl, style->title);
        }
        lv_obj_set_style_text_font(title_lbl, arex_get_font(style->title_font_id), 0);
        lv_obj_set_style_text_color(title_lbl, AREX_LIGHT, 0);
        lv_obj_set_size(title_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(title_lbl, (lv_align_t)style->title_align,
                     style->title_offset_x, style->title_offset_y);
        lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_DOT);
    }

    /* --- 零件 2：主数值 --- */
    if (style->elements & ELEM_VALUE) {
    lv_obj_t *val_lbl = lv_label_create(obj);
        lv_obj_set_style_text_font(val_lbl, arex_get_font(val_font_id), 0);
        lv_obj_set_style_text_color(val_lbl, AREX_GREEN, 0);

    if (AREX_SHOW_PLACEHOLDER_ON_INIT) {
            /* 通用占位符 */
        lv_label_set_text(val_lbl, "--");
    } else {
        char buf[48] = "--";
        switch (w_id) {
                case WIDGET_DEPTH_1612:  snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.depth); break;
                case WIDGET_NDL_STOP_1606: snprintf(buf, sizeof(buf), "%d", g_sensor_data.ndl_stop_value); break;
                case WIDGET_DIVE_TIME_1606: snprintf(buf, sizeof(buf), "%02d:%02d", g_sensor_data.dive_time_s/60, g_sensor_data.dive_time_s%60); break;
                case WIDGET_GAS_1606:      snprintf(buf, sizeof(buf), "%s", g_sensor_data.gas_name); break;
                case WIDGET_SYS_1606:    snprintf(buf, sizeof(buf), "%02d:%02d", g_sensor_data.sys_time_h, g_sensor_data.sys_time_m); break;
                case WIDGET_TEMP_0806:    snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.temperature_c); break;
                case WIDGET_TIME_1606:    snprintf(buf, sizeof(buf), "%02d:%02d", g_sensor_data.sys_time_h, g_sensor_data.sys_time_m); break;
                case WIDGET_TTS_0806:      snprintf(buf, sizeof(buf), "%d", g_sensor_data.tts); break;
                case WIDGET_ASCENT_0806:
                case WIDGET_ASCENT_0812:  snprintf(buf, sizeof(buf), "%+.1f", (double)g_sensor_data.ascent_rate); break;
                case WIDGET_COMPASS_1612: snprintf(buf, sizeof(buf), "%03d", g_sensor_data.heading); break;
                case WIDGET_BATTERY_0806: snprintf(buf, sizeof(buf), "%.0f", (double)g_sensor_data.battery_pct); break;
                case WIDGET_STOP_DEPTH_0806: snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.stop_depth_m); break;
                case WIDGET_STOP_TIME_1606: snprintf(buf, sizeof(buf), "%d", g_sensor_data.stop_time_left_s); break;
                case WIDGET_PPO2_0806:     snprintf(buf, sizeof(buf), "%.2f", (double)g_sensor_data.ppo2[0]); break;
                case WIDGET_SURF_GF_0806:  snprintf(buf, sizeof(buf), "%.2f", (double)g_sensor_data.surf_gf); break;
                case WIDGET_GF99_0806:     snprintf(buf, sizeof(buf), "%.0f", (double)g_sensor_data.gf99); break;
                case WIDGET_CNS_0806:      snprintf(buf, sizeof(buf), "%d", g_sensor_data.cns_pct); break;
                case WIDGET_OTU_0806:      snprintf(buf, sizeof(buf), "%d", g_sensor_data.otu); break;
                case WIDGET_GF_0806:        snprintf(buf, sizeof(buf), "%d/%d", g_sensor_data.gf_low, g_sensor_data.gf_high); break;
                case WIDGET_MOD_0806:       snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.mod_m); break;
                case WIDGET_CEILING_0806:  snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.ceiling_m); break;
                case WIDGET_GAS_MIX_1606:  snprintf(buf, sizeof(buf), "%d/%d", g_sensor_data.gas_o2_pct, g_sensor_data.gas_he_pct); break;
                case WIDGET_GAS_DENS_0806:  snprintf(buf, sizeof(buf), "%.2f", (double)g_sensor_data.gas_density); break;
                case WIDGET_FIO2_0806:      snprintf(buf, sizeof(buf), "%.0f%%", (double)g_sensor_data.fio2_pct); break;
                case WIDGET_HEADING_0806:   snprintf(buf, sizeof(buf), "%03d", g_sensor_data.heading); break;
                /* ===== POD 单模具：数据源根据 pod_index 动态分配 ===== */
                case WIDGET_POD_0806:
                    if (is_pod_mold) {
                        if (pod_index == 1) {
                            snprintf(buf, sizeof(buf), "%.0f", (double)g_sensor_data.pod1_bar);
                        } else {
                            snprintf(buf, sizeof(buf), "%.0f", (double)g_sensor_data.pod2_bar);
                        }
                    } else {
                        snprintf(buf, sizeof(buf), "--");
                    }
                    break;
                case WIDGET_DEPTH_MAX_0806: snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.max_depth); break;
                case WIDGET_DEPTH_AVG_0806: snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.avg_depth); break;
                case WIDGET_TEMP_MIN_0806: snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.min_temp); break;
                case WIDGET_TEMP_AVG_0806: snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.avg_temp); break;
                /* 🚨 以下已废弃，Protobuf 已移除对应 ID
                case WIDGET_WTIME_0806: {
                    uint32_t t = g_sensor_data.surface_time_s;
                    snprintf(buf, sizeof(buf), "%02d:%02d", t / 60, t % 60);
                break;
                }
                case WIDGET_TEMP_MAX_0806: snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.max_temp); break;
                case WIDGET_SAC_RATE_0806:  snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.sac_rate); break;
                case WIDGET_PPO2_SAFE_0806: snprintf(buf, sizeof(buf), "%.2f", 1.4); break;
                case WIDGET_NDL_SAFE_0806:  snprintf(buf, sizeof(buf), "%d", 5); break;
                case WIDGET_SAC_SAFE_0806:  snprintf(buf, sizeof(buf), "%.1f", 25.0); break;
                */
                default:                          snprintf(buf, sizeof(buf), "--"); break;
        }
        lv_label_set_text(val_lbl, buf);
    }

        /* 所有使用 ELEM_VALUE 的 widget 都使用 spec.basic.value_align */
        if (style->elements & ELEM_VALUE) {
            lv_obj_align(val_lbl, (lv_align_t)style->spec.basic.value_align,
                         style->spec.basic.value_offset_x, style->spec.basic.value_offset_y);
        }
    lv_obj_set_user_data(val_lbl, (void *)(uintptr_t)w_id);
    }

    /* --- 零件 3：单位 --- */
    if ((style->elements & ELEM_UNIT) && style->unit) {
        lv_obj_t *unit_lbl = lv_label_create(obj);
        lv_label_set_text(unit_lbl, style->unit);
        lv_obj_set_style_text_font(unit_lbl, arex_get_font(AREX_FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(unit_lbl, AREX_LIGHT, 0);
        /* 单位位于数值下方 */
        if (style->elements & ELEM_VALUE) {
            /* 挂在数值 label 下方 */
            lv_obj_align_to(unit_lbl, obj, LV_ALIGN_BOTTOM_MID, 0, -2);
        } else {
            lv_obj_align(unit_lbl, (lv_align_t)style->title_align,
                         style->title_offset_x, style->title_offset_y);
        }
    }

    /* --- 零件 4：特殊 BAR --- */
    if (style->elements & ELEM_BAR) {
        if (w_id == WIDGET_DEPTH_1612) {
            /* DEPTH 2x2 的速率图标（早期分支已处理，此分支仅作兜底） */
            /* DEPTH_1612 的 icon 在早期分支里，这里不需要再处理 */
        } else if (w_id == WIDGET_ASCENT_0812) {
            /* ASCENT_0812 (1x2)：绘制上升速率方向箭头图标（工厂自主查字典决定） */
            lv_obj_t *sudu_img = lv_img_create(obj);
            lv_img_set_src(sudu_img, &sudo_up_level0);
            lv_obj_align(sudu_img, LV_ALIGN_CENTER, 0, 0);
            if (s_ascent_icon_count < MAX_ASCENT_ICONS)
                s_img_ascent_rate[s_ascent_icon_count++] = sudu_img;
        } else if (w_id == WIDGET_COMPASS_1612) {
            /* COMPASS_1612 (2x2)：卷尺 tape 在早期分支里，ELEM_BAR 标记由 spec.compass 驱动 */
        } else if (w_id == WIDGET_TISSUE_GF_4012 || w_id == WIDGET_TISSUE_RAW_4012) {
            /* TISSUE (4x2)：16 柱组织图，ELEM_BAR 标记由 spec.tissue 驱动 */
        } else if (w_id == WIDGET_SYS_1606) {
            /* SYS 电池条 + 外设图标（系统状态栏） */
            lv_obj_t *bat_bg = lv_obj_create(obj);
            lv_obj_remove_style_all(bat_bg);
            lv_obj_set_size(bat_bg, 60, 14);
            lv_obj_align(bat_bg, LV_ALIGN_BOTTOM_LEFT, 4, -4);
            lv_obj_set_style_border_width(bat_bg, 1, 0);
            lv_obj_set_style_border_color(bat_bg, AREX_GREEN, 0);
            lv_obj_set_style_radius(bat_bg, 2, 0);

            uint8_t pct = g_sensor_data.battery_pct;
            lv_obj_t *bat_fill = lv_obj_create(bat_bg);
            lv_obj_remove_style_all(bat_fill);
            lv_obj_set_size(bat_fill, LV_PCT(pct > 20 ? 100 : pct), LV_PCT(100));
            lv_obj_align(bat_fill, LV_ALIGN_LEFT_MID, 0, 0);
            lv_obj_set_style_bg_color(bat_fill, pct > 20 ? AREX_GREEN : AREX_LIGHT, 0);
            lv_obj_set_style_bg_opa(bat_fill, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(bat_fill, 1, 0);
            (void)bat_fill;
        }
    }

    return obj;
}

/* =========================================================
 * 全局 widget 句柄表（按 arex_widget_id_t 索引，供 update 循环查找）
 * 注意：一个 widget_id 可能有多个物理实例（左侧锚点1个 + 5F N个），
 * 所以这是链表表头，实际使用时遍历子节点查找。
 * ========================================================= */
#define MAX_WIDGET_HANDLES 16
#define MAX_WIDGETS  41
static lv_obj_t *s_widget_handles[MAX_WIDGETS]; /* 仅记录 5F 区域的句柄 */
static uint8_t   s_widget_handle_count = 0;

/* 按 widget_id 在容器中查找第一个匹配的子节点 */
static lv_obj_t *find_widget_in_container(lv_obj_t *container, arex_widget_id_t w_id)
{
    if (!container) return NULL;
    int16_t child_cnt = lv_obj_get_child_cnt(container);
    for (int16_t i = 0; i < child_cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(container, i);
        if (child && (arex_widget_id_t)(uintptr_t)lv_obj_get_user_data(child) == w_id) {
            return child;
        }
    }
    return NULL;
}

/* =========================================================
 * 5F 网格总线渲染器
 *
 * 1. 从 g_sys_config.widget_* 读取所有组件配置
 * 2. 逐一枚遍历，用纯数学行×列映射算出绝对坐标
 * 3. 调用组件工厂渲染，注入 user_data 烙印
 * 4. 注册外部容器到告警引擎
 * ========================================================= */
void arex_render_5f_custom_grid(lv_obj_t *card_custom, lv_obj_t *left_anchor)
{
    /* 注入外部容器 */
    g_card_custom_obj = card_custom;
    g_left_anchor_obj = left_anchor;

    if (!card_custom) return;

    /* 获取容器总尺寸（标题区偏移由 arex_calc_widget_grid 内部处理） */
    uint16_t parent_w = lv_obj_get_content_width(card_custom);
    uint16_t parent_h = lv_obj_get_content_height(card_custom);

    /* 清除旧 widget 句柄表 */
    memset(s_widget_handles, 0, sizeof(s_widget_handles));
    s_widget_handle_count = 0;

    /* 清除容器中所有旧组件（rebuild 时） */
    lv_obj_clean(card_custom);

    /* ---- 创建卡片标题（使用通用引擎函数） ---- */
    arex_render_card_title(card_custom, "5F: CUSTOM WIDGETS");

    /* 遍历所有组件（使用新的 g_5f_widgets[] 结构体数组） */
    uint8_t count = g_5f_widget_count;
    if (count > AREX_5F_MAX_WIDGETS) count = AREX_5F_MAX_WIDGETS;

    for (uint8_t i = 0; i < count; i++) {
        arex_widget_id_t w_id   = g_5f_widgets[i].widget_id;
        uint8_t r = g_5f_widgets[i].r;
        uint8_t c = g_5f_widgets[i].c;

        /* 从样式表查 span_w/span_h（MCU 本地自动推导） */
        const arex_widget_style_t *style = arex_get_widget_style(w_id);
        uint8_t span_w = (style != NULL) ? style->span_w : 1;
        uint8_t span_h = (style != NULL) ? style->span_h : 1;

        if (w_id == WIDGET_EMPTY) continue;
        if (r >= AREX_WIDGET_ROWS || c >= AREX_WIDGET_COLS) continue;

        /* 纯数学绝对坐标映射（含 AREX_CARD_TITLE_H=40px 标题避让偏移） */
        int16_t abs_x, abs_y;
        uint16_t abs_w, abs_h;
        arex_calc_widget_grid(parent_w, parent_h,
                              r, c, span_w, span_h,
                              &abs_x, &abs_y, &abs_w, &abs_h);

        /* 调用组件工厂（工厂自主查字典决定是否绘制速率图标） */
        lv_obj_t *w = render_widget_by_id(card_custom, w_id,
                                          abs_x, abs_y, abs_w, abs_h,
                                          span_w, span_h, (arex_font_id_t)255);

        /* 记录句柄（用于 update 循环） */
        if (w && s_widget_handle_count < MAX_WIDGET_HANDLES) {
            s_widget_handles[s_widget_handle_count++] = w;
        }
    }
}

/* =========================================================
 * arex_5f_grid_rebuild — 重建 5F 自定义网格
 *
 * 由 arex_screen_rebuild_layout() 调用，当 BLE 下发新的 5F 布局时触发。
 * 直接操作 g_card_custom_obj 容器，清除并重建所有网格组件。
 * ========================================================= */
void arex_5f_grid_rebuild(void)
{
    if (!g_card_custom_obj) {
        printf("[5F] ERROR: g_card_custom_obj is NULL!\r\n");
        return;
    }

    printf("[5F] Rebuilding: widget_count=%u, container_size=%dx%d\r\n",
           g_5f_widget_count,
           lv_obj_get_content_width(g_card_custom_obj),
           lv_obj_get_content_height(g_card_custom_obj));

    /* 获取容器尺寸 */
    uint16_t parent_w = lv_obj_get_content_width(g_card_custom_obj);
    uint16_t parent_h = lv_obj_get_content_height(g_card_custom_obj);

    /* 清除旧 widget 句柄表 */
    memset(s_widget_handles, 0, sizeof(s_widget_handles));
    s_widget_handle_count = 0;

    /* 清除容器中所有旧组件 */
    lv_obj_clean(g_card_custom_obj);

    /* 创建卡片标题 */
    arex_render_card_title(g_card_custom_obj, "5F: CUSTOM WIDGETS");

    /* 遍历所有组件 */
    uint8_t count = g_5f_widget_count;
    if (count > AREX_5F_MAX_WIDGETS) count = AREX_5F_MAX_WIDGETS;

    for (uint8_t i = 0; i < count; i++) {
        arex_widget_id_t w_id   = g_5f_widgets[i].widget_id;
        uint8_t r = g_5f_widgets[i].r;
        uint8_t c = g_5f_widgets[i].c;

        if (w_id == WIDGET_EMPTY) continue;

        /* 从样式表查 span_w/span_h */
        const arex_widget_style_t *style = arex_get_widget_style(w_id);
        uint8_t span_w = (style != NULL) ? style->span_w : 1;
        uint8_t span_h = (style != NULL) ? style->span_h : 1;

        if (r >= AREX_WIDGET_ROWS || c >= AREX_WIDGET_COLS) continue;

        /* 计算绝对坐标 */
        int16_t abs_x, abs_y;
        uint16_t abs_w, abs_h;
        arex_calc_widget_grid(parent_w, parent_h, r, c, span_w, span_h,
                              &abs_x, &abs_y, &abs_w, &abs_h);

        /* 渲染组件 */
        lv_obj_t *w = render_widget_by_id(g_card_custom_obj, w_id,
                                          abs_x, abs_y, abs_w, abs_h,
                                          span_w, span_h, (arex_font_id_t)255);
        if (w && s_widget_handle_count < MAX_WIDGET_HANDLES) {
            s_widget_handles[s_widget_handle_count++] = w;
        }
    }

    printf("[5F] Rebuilt with %u widgets\r\n", count);
}

/* =========================================================
 * 按 widget_id 设置数值（由外部 update 循环调用）
 *
 * 架构：
 *   - 遍历 g_card_custom_obj 和 g_left_anchor_obj 两个容器
 *   - 用 user_data 烙印匹配 target_id
 *   - POD 使用高位掩码标签 (1033=POD1, 2033=POD2)
 *
 * 算法：
 *   - DEPTH: 用 child[0]/child[1] 下标访问
 *   - POD: 遍历查找标签 1033/2033
 *   - 其他: 遍历子节点查找 user_data 匹配
 *
 * 绝不触发任何重绘或排版重构！只更新 lv_label 文字。
 * ========================================================= */
void arex_widget_set_value(arex_widget_id_t id, float value)
{
    /* 遍历两个容器（5F 卡片 + 左侧锚点） */
    lv_obj_t *containers[2] = { g_card_custom_obj, g_left_anchor_obj };

    for (uint8_t c = 0; c < 2; c++) {
        lv_obj_t *container = containers[c];
        if (!container) continue;

        int16_t child_cnt = lv_obj_get_child_cnt(container);
        for (int16_t i = 0; i < child_cnt; i++) {
            lv_obj_t *child = lv_obj_get_child(container, i);
            if (!child) continue;

            uintptr_t child_tag = (uintptr_t)lv_obj_get_user_data(child);

            /* DEPTH 专属：整数/小数用 child[0]/child[1] 下标访问 */
            if (id == WIDGET_DEPTH_1612 && child_tag == (uintptr_t)id) {
                int di = (int)value;
                int dd = (int)((value - di) * 10 + 0.5f);
                lv_obj_t *part0 = lv_obj_get_child(child, 0);
                lv_obj_t *part1 = lv_obj_get_child(child, 1);
                if (part0 && lv_obj_check_type(part0, &lv_label_class)) {
                    lv_label_set_text_fmt(part0, "%d", di);
                }
                if (part1 && lv_obj_check_type(part1, &lv_label_class)) {
                    lv_label_set_text_fmt(part1, ".%d", dd);
                }
                break;
            }

            /* ===== POD 单模具：数据源根据 pod_index 动态分配 =====
             * 注意：由于关闭了 ELEM_EXTRA，POD 不再有独立的 ID 标签子元素。
             * 数值 label 通过通用路径创建，其 user_data = WIDGET_POD_0806。
             * 因此可以简化逻辑：直接通过 child_tag == WIDGET_POD_0806 匹配即可。
             * POD1/POD2 的区分由渲染时的 pod_index 决定，更新时无需区分。 */
            if (child_tag == (uintptr_t)WIDGET_POD_0806) {
                int16_t sub_cnt = lv_obj_get_child_cnt(child);
                for (int16_t j = 0; j < sub_cnt; j++) {
                    lv_obj_t *sub = lv_obj_get_child(child, j);
                    if (!sub) continue;
                    if ((uintptr_t)lv_obj_get_user_data(sub) == (uintptr_t)WIDGET_POD_0806) {
                        if (lv_obj_check_type(sub, &lv_label_class)) {
                            char buf[32];
                            snprintf(buf, sizeof(buf), "%.0f", (double)value);
                            lv_label_set_text(sub, buf);
                        }
                        break;
                    }
                }
                continue;
            }

            /* ===== 通用 widget：用 user_data == id 匹配 ===== */
            if (child_tag == (uintptr_t)id) {
                int16_t sub_cnt = lv_obj_get_child_cnt(child);
                for (int16_t j = 0; j < sub_cnt; j++) {
                    lv_obj_t *sub = lv_obj_get_child(child, j);
                    if (!sub) continue;
                    if ((uintptr_t)lv_obj_get_user_data(sub) == (uintptr_t)id) {
                        if (lv_obj_check_type(sub, &lv_label_class)) {
                            char buf[32];
                            if (id == WIDGET_TEMP_0806) {
                                snprintf(buf, sizeof(buf), "%.1f", (double)value);
                            } else if (id == WIDGET_PPO2_0806) {
                                snprintf(buf, sizeof(buf), "%.2f", (double)value);
                            } else if (id == WIDGET_BATTERY_0806) {
                                snprintf(buf, sizeof(buf), "%.0f%%", (double)value);
                            } else if (id == WIDGET_TTS_0806 || id == WIDGET_NDL_STOP_1606) {
                                snprintf(buf, sizeof(buf), "%d", (int)value);
                            } else {
                                snprintf(buf, sizeof(buf), "%.0f", (double)value);
                            }
                            lv_label_set_text(sub, buf);
                        }
                        break;
                    }
                }
            }
        }
    }
}

/* =========================================================
 * 按 widget_id 设置字符串（用于 GAS 等非数值组件）
 * ========================================================= */
void arex_widget_set_text(arex_widget_id_t id, const char *text)
{
    if (!text) return;

    /* 遍历两个容器（5F 卡片 + 左侧锚点） */
    lv_obj_t *containers[2] = { g_card_custom_obj, g_left_anchor_obj };

    for (uint8_t c = 0; c < 2; c++) {
        lv_obj_t *container = containers[c];
        if (!container) continue;

        int16_t child_cnt = lv_obj_get_child_cnt(container);
        for (int16_t i = 0; i < child_cnt; i++) {
            lv_obj_t *child = lv_obj_get_child(container, i);
            if (!child) continue;

            if ((arex_widget_id_t)(uintptr_t)lv_obj_get_user_data(child) == id) {
                int16_t sub_cnt = lv_obj_get_child_cnt(child);
                for (int16_t j = 0; j < sub_cnt; j++) {
                    lv_obj_t *sub = lv_obj_get_child(child, j);
                    if (!sub) continue;
                    if ((arex_widget_id_t)(uintptr_t)lv_obj_get_user_data(sub) == id) {
                        if (lv_obj_check_type(sub, &lv_label_class)) {
                            lv_label_set_text(sub, text);
                        }
                        break;
                    }
                }
            }
        }
    }
}

/* =========================================================
 * 告警闪烁定时器回调
 * ========================================================= */
static void alarm_blink_cb(lv_timer_t *timer)
{
    (void)timer;
    s_alarm_blink_on = !s_alarm_blink_on;

    /* 遍历所有注册的 widget 句柄应用/移除闪烁样式 */
    for (uint8_t i = 0; i < s_widget_handle_count; i++) {
        lv_obj_t *w = s_widget_handles[i];
        if (!w) continue;

        if (s_alarm_blink_on) {
            lv_obj_set_style_bg_color(w, AREX_LIGHT, 0);
            lv_obj_set_style_text_color(w, AREX_BLACK, 0);
        } else {
            lv_obj_set_style_bg_color(w, AREX_BLACK, 0);
            lv_obj_set_style_text_color(w, AREX_GREEN, 0);
        }
    }
}

/* =========================================================
 * 靶向告警触发引擎
 *
 * 1. 弹出顶部的纯英文告警横幅（永不显示图案）
 * 2. 若 target_id != EMPTY，遍历所有子节点，
 *    找到打了烙印的组件并加入闪烁队列
 * 3. 启动/停止 blink 定时器
 *
 * 告警消失时调用 arex_clear_all_alarm_styles() 清除。
 * ========================================================= */
void arex_trigger_alarm(arex_alarm_level_t level,
                        const char *eng_text,
                        arex_widget_id_t target_id)
{
    /* 停止旧的 blink 定时器 */
    if (s_alarm_blink_timer) {
        lv_timer_pause(s_alarm_blink_timer);
        s_alarm_blink_on = false;
    }

    /* 清除旧告警样式 */
    arex_clear_all_alarm_styles();

    /* 弹出告警横幅 */
    arex_show_alarm_banner(level, eng_text);

    if (target_id == WIDGET_EMPTY) {
        /* 仅横幅告警，不做靶向 */
        return;
    }

    /* 初始化告警样式到所有匹配的 widget（注册到闪烁队列） */
    for (uint8_t i = 0; i < s_widget_handle_count; i++) {
        lv_obj_t *w = s_widget_handles[i];
        if (!w) continue;

        if ((arex_widget_id_t)(uintptr_t)lv_obj_get_user_data(w) == target_id) {
            /* 加入闪烁队列（已在 s_widget_handles 中） */
            /* 立即应用一次 */
            lv_obj_set_style_bg_color(w, AREX_LIGHT, 0);
        }
    }

    /* 启动/重设 blink 定时器 */
    if (level == AREX_ALARM_CRIT) {
        /* CRITICAL: 2Hz 快速闪烁 */
        if (!s_alarm_blink_timer) {
            s_alarm_blink_timer = lv_timer_create(alarm_blink_cb, 500, NULL);
        } else {
            lv_timer_reset(s_alarm_blink_timer);
            lv_timer_set_period(s_alarm_blink_timer, 500);
        }
        lv_timer_resume(s_alarm_blink_timer);
    } else if (level == AREX_ALARM_WARN) {
        /* WARN: 1Hz 慢闪烁 */
        if (!s_alarm_blink_timer) {
            s_alarm_blink_timer = lv_timer_create(alarm_blink_cb, 1000, NULL);
        } else {
            lv_timer_reset(s_alarm_blink_timer);
            lv_timer_set_period(s_alarm_blink_timer, 1000);
        }
        lv_timer_resume(s_alarm_blink_timer);
    }
    /* INFO 级别不闪烁，仅横幅 */

    s_alarm_level = level;
}

/* =========================================================
 * 清除所有组件的告警样式，恢复正常显示
 * ========================================================= */
void arex_clear_all_alarm_styles(void)
{
    /* 停止 blink 定时器 */
    if (s_alarm_blink_timer) {
        lv_timer_pause(s_alarm_blink_timer);
    }

    /* 恢复所有 widget 句柄的正常样式 */
    for (uint8_t i = 0; i < s_widget_handle_count; i++) {
        lv_obj_t *w = s_widget_handles[i];
        if (!w) continue;
        lv_obj_set_style_bg_color(w, AREX_BLACK, 0);
        lv_obj_set_style_text_color(w, AREX_GREEN, 0);
    }

    /* 隐藏横幅 */
    arex_hide_alarm_banner();

    s_alarm_level = AREX_ALARM_NONE;
}

/* =========================================================
 * 告警横幅显示/隐藏
 * 默认横幅显示在屏幕顶部（独立图层），永不显示图案，纯英文文字。
 * ========================================================= */
void arex_show_alarm_banner(arex_alarm_level_t level, const char *eng_text)
{
    arex_hide_alarm_banner();

    if (!eng_text || !eng_text[0]) return;

    /* 创建横幅（顶层 screen，避免被其他对象遮挡） */
    lv_obj_t *screen = lv_scr_act();
    s_alarm_banner = lv_obj_create(screen);
    lv_obj_set_size(s_alarm_banner, AREX_PHYSICAL_W, 36);
    lv_obj_set_pos(s_alarm_banner, 0, 0);
    lv_obj_set_style_border_width(s_alarm_banner, 0, 0);
    lv_obj_set_style_radius(s_alarm_banner, 0, 0);
    lv_obj_set_style_pad_all(s_alarm_banner, 0, 0);

    if (level == AREX_ALARM_CRIT) {
        lv_obj_set_style_bg_color(s_alarm_banner, AREX_DARK, 0);
    } else if (level == AREX_ALARM_WARN) {
        lv_obj_set_style_bg_color(s_alarm_banner, AREX_DARK, 0);
    } else {
        lv_obj_set_style_bg_color(s_alarm_banner, AREX_BLACK, 0);
    }
    lv_obj_set_style_bg_opa(s_alarm_banner, LV_OPA_COVER, 0);

    /* 告警文字 label */
    lv_obj_t *lbl = lv_label_create(s_alarm_banner);
    lv_label_set_text(lbl, eng_text);
    lv_obj_set_style_text_font(lbl, arex_get_font(AREX_FONT_ID_TITLE), 0);
    lv_obj_set_style_text_color(lbl, AREX_LIGHT, 0);
    lv_obj_set_size(lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
}

void arex_hide_alarm_banner(void)
{
    if (s_alarm_banner) {
        lv_obj_del(s_alarm_banner);
        s_alarm_banner = NULL;
    }
}

/* =========================================================
 * 定时数据更新 (由 lv_timer 以 1Hz/2Hz 调用)
 * 仅更新 lv_label 文字，绝不触发排版重构
 * ========================================================= */
void arex_ui_update_data(void)
{
    /* 由调用方在 arex_screen.c 中实现具体的 lv_label_set_text 调用
     * 此函数作为空钩子存在，供未来扩展
     */
}

/* =========================================================
 * 11. Data Bus UI 消费任务 — 全系统唯一允许执行 lv_label_set_text 的地方
 *
 * 架构铁律：
 *   - 硬件工程师：只能调用 arex_bus_set_*() 系列函数（仅写数据+打脏标记）
 *   - UI 工程师  ：只能修改 arex_ui_update_task() 消费者
 *   - 两者通过 g_sensor_data.dirty_mask 完全解耦
 *
 * 由 lv_timer 驱动，建议 50ms 周期（20 FPS 足够覆盖所有传感器变化）
 * ========================================================= */
void arex_ui_update_task(lv_timer_t *timer)
{
    (void)timer;

    /* ============================================================
     * 🚨 核心修复：独立于数据的"时间心跳引擎"必须放在最前面！
     *
     * 即使没有任何脏标记，只要处于运动状态(|rate|>=3.0 m/min)，
     * 我们就强行注入 DIRTY_DEPTH 脏标记，唤醒 UI 引擎去画闪烁动画！
     * ============================================================ */
    {
        static bool last_flash_state = false;
        bool current_flash_state = (lv_tick_get() / 500) % 2 == 0;

        if (current_flash_state != last_flash_state) {
            last_flash_state = current_flash_state;

            float rate = g_sensor_data.ascent_rate;
            if (fabsf(rate) >= AREX_RATE_STILL_THRESHOLD) {
                g_sensor_data.dirty_mask |= DIRTY_DEPTH;
            }
        }
    }

    uint32_t mask = g_sensor_data.dirty_mask;
    if (mask == DIRTY_NONE) return;

    /* 最高优先级：UI 布局重建（BLE 配置同步触发）。
     * 重建耗时较长，锁住 LVGL invalidation 防止闪烁，本帧直接退出。 */
    if (mask & DIRTY_UI_LAYOUT) {
        lv_disp_t *disp = lv_disp_get_default();
        if (disp) lv_disp_enable_invalidation(disp, false);
        arex_screen_rebuild_layout();
        if (disp) lv_disp_enable_invalidation(disp, true);
        g_sensor_data.dirty_mask &= ~DIRTY_UI_LAYOUT;
        return;
    }

    /* 深度 + NDL + TTS + 组织舱 —— 左侧面板全量刷新 + 2F Deco 卡片刷新 */
    if (mask & (DIRTY_DEPTH | DIRTY_NDL | DIRTY_TTS | DIRTY_TISSUES)) {
        arex_screen_refresh_left_panel();
        card_deco_update();

        /* ============================================================
         * 速率图标闪烁引擎（与开头心跳同步执行）
         *
         * 当 DIRTY_DEPTH 被心跳唤醒后，统一在此处执行图标更新
         * ============================================================ */
        if (s_ascent_icon_count > 0) {
            static int8_t s_last_direction = 0;  /* 0=静止, 1=上升, -1=下降 */
            float rate = g_sensor_data.ascent_rate;
            bool is_moving = (fabsf(rate) >= AREX_RATE_STILL_THRESHOLD);
            bool current_flash_state = (lv_tick_get() / 500) % 2 == 0;
            const void *target_img_src = &sudo_up_level0;

            /* 判断当前运动方向: 1=上升(positive), -1=下降(negative), 0=静止 */
            int8_t current_direction = 0;
            if (rate > 0.0f) {
                current_direction = 1;
            } else if (rate < 0.0f) {
                current_direction = -1;
            }

            /* A. 静止悬停 或 方向切换过渡期 或 闪烁灭相位 → 强制显示 level0
             * 方向根据最后的运动方向决定（静止时保持最后的方向图标） */
            bool direction_changed = (current_direction != 0 && s_last_direction != 0 && current_direction != s_last_direction);
            if (!is_moving || direction_changed || current_flash_state == false) {
                int8_t effective_dir = is_moving ? current_direction : s_last_direction;
                target_img_src = (effective_dir > 0) ? &sudo_up_level0 : &sudo_down_level0;
            }
            /* B. 移动中 且 非方向切换期 且 闪烁亮相位 → 显示真实等级 */
            else {
                if (rate >= AREX_RATE_LEVEL2_THRESHOLD) {
                    target_img_src = &sudo_up_level2;
                } else if (rate >= AREX_RATE_LEVEL1_THRESHOLD) {
                    target_img_src = &sudo_up_level1;
                } else if (rate > -AREX_RATE_LEVEL1_THRESHOLD) {
                    target_img_src = &sudo_down_level0;
                } else if (rate > -AREX_RATE_LEVEL2_THRESHOLD) {
                    target_img_src = &sudo_down_level1;
            } else {
                    target_img_src = &sudo_down_level2;
                }
            }

            /* 更新方向状态（仅在非静止时更新，静止时保持最后方向） */
            if (current_direction != 0) {
                s_last_direction = current_direction;
            }

            /* C. 循环遍历数组，让所有速率图标同步刷新 */
            for (int i = 0; i < s_ascent_icon_count; i++) {
                if (s_img_ascent_rate[i] != NULL) {
                    lv_img_set_src(s_img_ascent_rate[i], target_img_src);
                }
            }
        }
    }

    /* ============================================================
     * NDL_STOP 多形态状态机：NDL常态 / Safety停留 / Deco停留
     * 根据 g_sensor_data.stop_type 瞬间切换所有子组件的显隐、位置和字号
     * 遍历数组，同步刷新所有 NDL 实例（左侧锚点 + 5F 多个）
     * ============================================================ */
    if (s_ndl_handle_count > 0 && (mask & (DIRTY_NDL_STOP | DIRTY_DEPTH | DIRTY_NDL))) {
        /* 实时查表获取样式字典 */
        const arex_widget_style_t *style = arex_get_widget_style(WIDGET_NDL_STOP_1606);
        if (!style) return;

        const arex_style_ndl_stop_t *s = &style->spec.ndl_stop;

        for (int i = 0; i < s_ndl_handle_count; i++) {
            ndl_handle_t *h = &s_ndl_handles[i];

            /* ========== 状态 1: 常态 NDL 模式 ========== */
            if (g_sensor_data.stop_type == AREX_STOP_NONE) {
                lv_obj_clear_flag(h->vert_bg, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(h->horiz_bg, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(h->title_top, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(h->sub_bot, LV_OBJ_FLAG_HIDDEN);

                lv_obj_set_style_text_font(h->main_val, arex_get_font(AREX_FONT_ID_HUGE), 0);
                lv_label_set_text_fmt(h->main_val, "%d", g_sensor_data.ndl);
                lv_label_set_text(h->sub_bot, "NDL");

                /* 应用常态字典，消灭魔法数字 */
                lv_obj_align(h->main_val, (lv_align_t)s->norm_main_align, s->norm_main_x, s->norm_main_y);
                lv_obj_align(h->sub_bot, (lv_align_t)s->norm_sub_align, s->norm_sub_x, s->norm_sub_y);

                int fill_h = (g_sensor_data.ndl * s->vert_h) / 99;
                if (fill_h > s->vert_h) fill_h = s->vert_h;
                if (fill_h < 1) fill_h = 1;
                lv_obj_set_size(h->vert_fill, LV_PCT(100), fill_h);
            }
            /* ========== 状态 2/3: 停留模式 (Safety / Deco) ========== */
            else {
                lv_obj_add_flag(h->vert_bg, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(h->horiz_bg, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(h->title_top, LV_OBJ_FLAG_HIDDEN);

                /* 缩小字号，为 MM:SS 腾出空间 */
                lv_obj_set_style_text_font(h->main_val, arex_get_font(style->font_id), 0);

                /* 应用停留态字典 */
                lv_obj_align(h->main_val, (lv_align_t)s->deco_main_align, s->deco_main_x, s->deco_main_y);
                lv_obj_align(h->title_top, (lv_align_t)s->deco_title_align, s->deco_title_x, s->deco_title_y);

                /* 核心防重叠修复：主干只显示纯粹的时间！不带深度！ */
                if (g_sensor_data.in_stop_zone) {
                    int m = g_sensor_data.stop_time_left_s / 60;
                    int sec = g_sensor_data.stop_time_left_s % 60;
                    lv_label_set_text_fmt(h->main_val, "%d:%02d", m, sec);
                } else {
                    int min = (g_sensor_data.stop_time_left_s + 59) / 60;
                    lv_label_set_text_fmt(h->main_val, "%d'", min);
                }

                /* 标题文本精简分配 */
                if (g_sensor_data.stop_type == AREX_STOP_SAFETY) {
                    lv_label_set_text_fmt(h->title_top, "SAFE %.0fm", g_sensor_data.stop_depth_m);
                    lv_obj_clear_flag(h->sub_bot, LV_OBJ_FLAG_HIDDEN);
                    lv_label_set_text_fmt(h->sub_bot, "NDL %d", g_sensor_data.ndl);
                    lv_obj_align(h->sub_bot, (lv_align_t)s->deco_sub_align, s->deco_sub_x, s->deco_sub_y);
                } else {
                    lv_label_set_text_fmt(h->title_top, "DECO %.0fm", g_sensor_data.stop_depth_m);
                    lv_obj_add_flag(h->sub_bot, LV_OBJ_FLAG_HIDDEN);
                }

                int fill_w = (g_sensor_data.stop_time_total_s > 0)
                             ? ((g_sensor_data.stop_time_total_s - g_sensor_data.stop_time_left_s) * s->horiz_w) / g_sensor_data.stop_time_total_s
                             : 0;
                if (fill_w > s->horiz_w) fill_w = s->horiz_w;
                if (fill_w < 1) fill_w = 1;
                lv_obj_set_size(h->horiz_fill, fill_w, s->horiz_h);
            }
        }
    }

    /* 气瓶压力 —— 左侧面板 POD 刷新 */
    if (mask & DIRTY_POD) {
        arex_screen_refresh_left_panel();
    }

    /* 电池刷新 —— 数据驱动网格自动更新 */
    if (mask & DIRTY_BATT) {
        arex_screen_refresh_left_panel();
        arex_widget_set_value(WIDGET_BATTERY_0806, g_sensor_data.battery_pct);
    }

    /* 罗盘航向 — 零内存数学引擎，触发 invalidate + 更新标签 */
    if (mask & DIRTY_HEADING) {
        /* 更新卷尺下方的巨型文字 */
        if (s_heading_val_lbl) {
            lv_label_set_text_fmt(s_heading_val_lbl, "%03d", g_sensor_data.heading);
        }
        /* 触发卷尺画板的底层数学重绘（极其轻量） */
        if (s_compass_tape_obj) {
            lv_obj_invalidate(s_compass_tape_obj);
        }
        /* 如果有锁定，更新提示文本 */
        if (s_heading_hint_lbl) {
            if (g_sensor_data.heading_locked) {
                lv_label_set_text_fmt(s_heading_hint_lbl, "[ TARGET LOCKED: %03d° ]", g_sensor_data.heading_target);
            } else {
                lv_label_set_text(s_heading_hint_lbl, "[ ENTER ] mark heading");
            }
        }
    }

    /* 潜水时间 + W.TIME —— 左侧面板 + 4F 曲线图 */
    if (mask & DIRTY_DIVE_TIME) {
        arex_screen_refresh_left_panel();
    }

    /* PO2 值 —— 左侧面板 */
    if (mask & DIRTY_PPO2) {
        arex_screen_refresh_left_panel();
    }

    /* 气体切换 */
    if (mask & DIRTY_GAS) {
        arex_screen_refresh_gas_menu();
        arex_screen_refresh_left_panel();
    }

    /* 组织舱 + 减压站序列刷新（轨迹追加 + 减压站重绘，节流保护） */
    if (mask & DIRTY_TISSUES) {
        uint32_t now = lv_tick_get();
#if AREX_DECO_REFRESH_MS > 0 //相当于刷新的间隔（这个决定了刷新的点，但是和真正的采样频率有区别（一般要对应上））
        if (now - _deco_last_refresh_ms >= AREX_DECO_REFRESH_MS) {
            _deco_last_refresh_ms = now;
            card_plan_update();
        }
#else
        (void)_deco_last_refresh_ms;
        card_plan_update();
#endif
    }

    /* CNS 氧中毒 —— 2F Deco 卡片 */
    if (mask & DIRTY_CNS) {
        card_deco_update();
    }

    /* OTU 氧中毒 —— 2F Deco 卡片 */
    if (mask & DIRTY_OTU) {
        card_deco_update();
    }

    /* 温度刷新 —— 数据驱动网格自动更新 */
    if (mask & DIRTY_TEMP) {
        arex_widget_set_value(WIDGET_TEMP_0806, g_sensor_data.temperature_c);
    }

    /* ============================================================
     * O(1) SYS_1606 全模块极速点对点刷新
     * 直接操作静态指针，绝不遍历 UI 树！
     * ============================================================ */
    if (mask & (DIRTY_BATT | DIRTY_TEMP | DIRTY_DEVICES)) {
        /* 1. 电量百分比 */
        if (mask & DIRTY_BATT) {
            if (s_sys_batt_lbl) {
                lv_label_set_text_fmt(s_sys_batt_lbl, "%d%%", (int)g_sensor_data.battery_pct);
            }
        }
        /* 2. 温度 */
        if (mask & DIRTY_TEMP) {
            if (s_sys_temp_lbl) {
                /* 整数拼接法绕过 %f 限制，完美显示 26.5 C */
                int t_int = (int)g_sensor_data.temperature_c;
                int t_dec = (int)(fabsf(g_sensor_data.temperature_c - t_int) * 10);
                lv_label_set_text_fmt(s_sys_temp_lbl, "%d.%d C", t_int, t_dec);
            }
        }
        /* 3. 设备状态图标 */
        if (mask & DIRTY_DEVICES) {
            if (s_sys_strobe_img) {
                lv_obj_set_style_img_opa(s_sys_strobe_img, g_sensor_data.strobe_on ? LV_OPA_COVER : LV_OPA_40, 0);
            }
            if (s_sys_flash_img) {
                lv_obj_set_style_img_opa(s_sys_flash_img, g_sensor_data.flashlight_on ? LV_OPA_COVER : LV_OPA_40, 0);
            }
            if (s_sys_cyl_lbl) {
                lv_label_set_text_fmt(s_sys_cyl_lbl, "x%d", g_sensor_data.cylinder_count);
            }
        }
    }

    /* 洗净所有脏标记 */
    arex_bus_clear_all_dirty();
}

/* =========================================================
 * 12. 左侧 2x6 绝对网格渲染引擎
 *
 * 严格将 160x360 区域划分为 2列(80px) x 6行(60px) 的绝对网格矩阵，
 * 彻底废弃 current_y 累加排版，改用 x*y*w*h 纯数学坐标推演。
 * SystemData 底部 60px 由 WIDGET_SYS_1606 组件化渲染。
 * ========================================================= */

/* 左侧网格总线渲染器：遍历 g_left_widgets[] 数组，
 * 用纯数学 cell_w * cell_h 推算绝对坐标并渲染所有组件。
 * left_anchor 传入用于告警引擎跨区搜索烙印对象。 */
void arex_render_left_anchor_grid(lv_obj_t *left_anchor)
{
    if (!left_anchor) return;

    /* 注入外部容器（供告警引擎跨区搜索烙印对象） */
    g_left_anchor_obj = left_anchor;

    /* 注意：不单独清空 s_img_ascent_rate[] / s_ndl_handles[]！
     * 它们已经在 arex_screen_rebuild_layout() 入口统一清空了。
     * 这里只需要追加左侧锚点的 widget 指针即可（追加模式）。 */

    /* 基准网格单元：2列 x 6行，每格 80x60 */
    const uint16_t cell_w = AREX_LEFT_CELL_W;   /* 80px */
    const uint16_t cell_h = AREX_LEFT_CELL_H;   /* 60px */

    /* 遍历并渲染基于网格的组件 */
    for (uint8_t i = 0; i < g_left_widget_count && i < AREX_LEFT_MAX_WIDGETS; i++) {
        arex_left_widget_t *cfg = &g_left_widgets[i];
        if (cfg->widget_id == WIDGET_EMPTY) continue;

        /* 从样式表查表获取跨度信息 */
        const arex_widget_style_t *style = arex_get_widget_style(cfg->widget_id);
        uint8_t span_w = (style != NULL) ? style->span_w : 1;
        uint8_t span_h = (style != NULL) ? style->span_h : 1;

        /* 绝对物理坐标推演：col * cell_w, row * cell_h */
        int16_t  abs_x = (int16_t)(cfg->x * cell_w);
        int16_t  abs_y = (int16_t)(cfg->y * cell_h);
        uint16_t abs_w = span_w * cell_w;
        uint16_t abs_h = span_h * cell_h;

        /* 调用底层工厂：速率图标由工厂自主查字典决定 */
        render_widget_by_id(left_anchor, cfg->widget_id,
                            abs_x, abs_y, abs_w, abs_h,
                            span_w, span_h, (arex_font_id_t)255);
    }
}

/* =========================================================
 * 第五步：新简化工厂函数（APP下发位置 + MCU本地查样式表）
 *
 * 架构铁律：APP 只下发 [widget_id, x, y]，MCU 根据 widget_id
 * 自动从样式注册表获取 w/h/offset，渲染时组合两者。
 * ========================================================= */
lv_obj_t* arex_render_widget(lv_obj_t *parent,
                            const arex_widget_pos_t *pos,
                            uint16_t cell_w, uint16_t cell_h,
                            uint16_t title_h)
{
    if (!parent || !pos) return NULL;
    if (pos->widget_id == WIDGET_EMPTY) return NULL;

    /* 1. 查本地样式表 */
    const arex_widget_style_t *style = arex_get_widget_style(pos->widget_id);
    if (!style) {
        /* 容错：未知ID，尝试用通用方式渲染 */
        lv_obj_t *comp = lv_obj_create(parent);
        lv_obj_remove_style_all(comp);
        int16_t ax = (int16_t)(pos->x * cell_w);
        int16_t ay = (int16_t)(pos->y * cell_h) + title_h;
        lv_obj_set_pos(comp, ax, ay);
        lv_obj_set_size(comp, cell_w, cell_h);
        return comp;
    }

    /* 2. 推算绝对物理坐标 */
    int16_t  abs_x = (int16_t)(pos->x * cell_w);
    int16_t  abs_y = (int16_t)(pos->y * cell_h) + title_h;
    uint16_t abs_w = (uint16_t)(style->span_w * cell_w);
    uint16_t abs_h = (uint16_t)(style->span_h * cell_h);

    /* 3. 直接调用底层工厂（速率图标由工厂自主查字典决定） */
    return render_widget_by_id(parent, pos->widget_id,
                               abs_x, abs_y, abs_w, abs_h,
                               style->span_w, style->span_h,
                               (arex_font_id_t)255);
}
