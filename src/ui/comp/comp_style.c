/*
 * 文件: src/app_ui/ui/comp/comp_style.c
 * 作用: 该文件属于公共组件模块，负责复用样式、通用控件、局部刷新逻辑或组件级显示封装。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时应重点检查布局尺寸、刷新频率与复用接口是否一致，避免局部样式或坐标调整影响同类页面的对齐和更新节奏。
 */

#include "../core/ui_engine.h"
#include "comp_style.h"

static const comp_style_t g_widget_styles[] =
{
    /* =========================================================
     * 第四步：MCU 本地只读 CSS 样式注册
     *
     * 架构铁律：UI 工程师调整内部像素位移只需在这里改数字，编译即生效
     * 完全不需要改 APP，也不需要改 BLE 协议
     * ========================================================= */
    /* ========== 核心驻留组件 ========== */
    /* 每个条目都描述一个组件的尺寸、字体、对齐和偏移。 */
    {
        .widget_id = COMP_DEPTH_1612,
        .span_w = 2, .span_h = 2,
        .elements = ELEM_VALUE | ELEM_UNIT | ELEM_BAR,  /* 2x2 大通栏：无 title，靠 spec.depth int/dec/unit 分离 */
        .font_id = FONT_ID_HUGE,
        .title_font_id = FONT_ID_MEDIUM,
        .unit = "m",
        .title = "DEPTH",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_TEXT_ALIGN_LEFT,
        .spec.depth = {
            .int_offset_x = -80, .int_offset_y = 10, .int_align = LV_ALIGN_RIGHT_MID,
            .dec_offset_x = 2,  .dec_offset_y = -30,
            .unit_offset_x = 0, .unit_offset_y = 2,
            .icon_offset_x = -10, .icon_offset_y = 0, .icon_align = LV_ALIGN_RIGHT_MID
        }
    },
    {
        .widget_id = COMP_DEPTH_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "DEPTH",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_TEXT_ALIGN_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_NDL_STOP_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT | ELEM_BAR,
        .font_id = FONT_ID_NDL,
        .title_font_id = FONT_ID_SMALL,
        .unit = "min",
        .title = "NDL",
        .title_offset_x = 10, .title_offset_y = 0, .title_align = LV_TEXT_ALIGN_LEFT,
        .spec.ndl_stop = {
            .vert_offset_x = 10, .vert_offset_y = 0, .vert_align = LV_ALIGN_LEFT_MID,
            .vert_w = 14, .vert_h = 40,
            .horiz_offset_x = 0, .horiz_offset_y = -4, .horiz_w = 140, .horiz_h = 6,
            .norm_main_x = 0, .norm_main_y = 0,  .norm_main_align = LV_ALIGN_LEFT_MID,
            .norm_sub_x  = 0, .norm_sub_y  = -5, .norm_sub_align  = LV_ALIGN_BOTTOM_LEFT,
            .deco_title_x = 0,  .deco_title_y = 4,   .deco_title_align = LV_ALIGN_TOP_LEFT,
            .deco_main_x  = 0, .deco_main_y  = -6,  .deco_main_align  = LV_ALIGN_LEFT_MID,
            .deco_sub_x   = 0,  .deco_sub_y   = -14,.deco_sub_align   = LV_ALIGN_BOTTOM_LEFT
        }
    },
    {
        .widget_id = COMP_DIVE_TIME_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_BAR,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "DIVE",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_GAS_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "GAS",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_SYS_1606,
        .span_w = 2, .span_h = 1,
        .elements = 0,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "SYS",
        .title_offset_x = 10, .title_offset_y = 0, .title_align = LV_TEXT_ALIGN_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = 0, .value_align = LV_ALIGN_CENTER }
    },
    {
        .widget_id = COMP_TEMP_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "TEMP",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_TIME_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "TIME",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_TTS_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "TTS",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_ASCENT_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "RATE",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_ASCENT_0812,
        .span_w = 1, .span_h = 2,
        .elements = ELEM_BAR,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = NULL,
        .title_offset_x = 10, .title_offset_y = 0, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = COMP_COMPASS_1612,
        .span_w = 2, .span_h = 2,
        .elements = ELEM_BAR,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = NULL,
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_TEXT_ALIGN_CENTER,
        .spec.compass = {
            .tape_offset_x = 0, .tape_offset_y = 20, .tape_align = LV_ALIGN_TOP_MID,
            .val_offset_x = 0, .val_offset_y = -4, .val_align = LV_ALIGN_BOTTOM_MID
        }
    },
    {
        .widget_id = COMP_BATTERY_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = "",
        .title = "BATT",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_STOP_DEPTH_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "STOP",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_STOP_TIME_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "STIME",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_PPO2_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "PPO2",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_SURF_GF_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "SURF.GF",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_GF99_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "GF99",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_CNS_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "CNS",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_OTU_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "OTU",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_GF_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "GF",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -5, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_MOD_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "MOD",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_CEILING_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "CEIL",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_GAS_MIX_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "O2/He",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_TISSUE_GF_4012,
        .span_w = 5, .span_h = 2,
        .elements = ELEM_TITLE | ELEM_BAR,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_MEDIUM,
        .unit = NULL,
        .title = "TISSUE(GF)",
        .title_offset_x = 10, .title_offset_y = 0, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.tissue = {
            .chart_offset_x = 0, .chart_offset_y = 10, .chart_align = LV_ALIGN_BOTTOM_RIGHT,
            .bar_count = 16, .bar_spacing = 2
        }
    },
    {
        .widget_id = COMP_TISSUE_RAW_4012,
        .span_w = 5, .span_h = 2,
        .elements = ELEM_TITLE | ELEM_BAR,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_MEDIUM,
        .unit = NULL,
        .title = "TISSUE(RAW)",
        .title_offset_x = 10, .title_offset_y = 0, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.tissue = {
            .chart_offset_x = 0, .chart_offset_y = 20, .chart_align = LV_ALIGN_BOTTOM_RIGHT,
            .bar_count = 16, .bar_spacing = 2
        }
    },
    {
        .widget_id = COMP_GAS_DENS_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "DENS",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_FIO2_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "FIO2",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_HEADING_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "HDG",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_POD_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "POD",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_DEPTH_MAX_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "MAX D",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_DEPTH_AVG_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "AVG D",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_TEMP_MIN_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "MIN T",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_TEMP_AVG_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "AVG T",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_GYRO_2406,
        .span_w = 3, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "GYRO dps",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_BATT_V_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "BAT V",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_BATT_TEMP_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "BAT C",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_PRJ_TEMP_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "OPT C",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_CHARGE_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "PWR",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_PRESSURE_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "P mbar",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -5, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_NOFLY_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "NOFLY",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -5, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_TTS_AT_5MIN_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "@+5",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_TTS_DELTA_5MIN_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "△+5",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_NDL_UP_3M_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "NDL↑3",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_NDL_DOWN_3M_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "NDL↓3",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_NDL_DELTA_3M_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "NDL△3",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_GTR_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "GTR",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_RMV_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "RMV",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_SAC_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "SAC",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_ACCEL_2406,
        .span_w = 3, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "ACC g",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_MAG_2406,
        .span_w = 3, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "MAG uT",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_MLX_2406,
        .span_w = 3, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "MLX uT",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_TMAG_2406,
        .span_w = 3, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "TMAG uT",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_ATTITUDE_2406,
        .span_w = 3, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "ATT deg",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_BLE_RSSI_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "BLE dBm",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_CPU_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "CPU",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_FPS_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "FPS",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_SENSOR_STAT_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_SMALL,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "SENS",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_EMPTY,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "TIME",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = COMP_EMPTY,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "PPO2 MAX",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = COMP_EMPTY,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "NDL MIN",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = COMP_EMPTY,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = FONT_ID_MEDIUM,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = "SAC MAX",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = COMP_EMPTY,
        .span_w = 1, .span_h = 1,
        .elements = 0,
        .font_id = FONT_ID_SMALL,
        .title_font_id = FONT_ID_SMALL,
        .unit = NULL,
        .title = NULL,
        .title_offset_x = 10, .title_offset_y = 0, .title_align = LV_TEXT_ALIGN_CENTER,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = 0, .value_align = LV_ALIGN_CENTER }
    },
};

#define STYLE_COUNT (int)(sizeof(g_widget_styles) / sizeof(g_widget_styles[0]))

const comp_style_t* comp_get_style(comp_id_t id)
{
    /* 通过组件 ID 在线性表中查找样式定义。 */
    for (int i = 0; i < STYLE_COUNT; i++)
    {
        if (g_widget_styles[i].widget_id == id)
        {
            return &g_widget_styles[i];
        }
    }
    return NULL;
}

const char *comp_get_name(comp_id_t id)
{
    const comp_style_t *style = comp_get_style(id);
    if (!style)
    {
        return "";
    }
    return style->title ? style->title : "";
}
