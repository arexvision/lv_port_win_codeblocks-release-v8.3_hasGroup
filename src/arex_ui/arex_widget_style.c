#include "arex_ui_engine.h"
#include "arex_widget_style.h"

static const arex_widget_style_t g_widget_styles[] =
{
    /* =========================================================
     * 第四步：MCU 本地只读 CSS 样式注册
     *
     * 架构铁律：UI 工程师调整内部像素位移只需在这里改数字，编译即生效
     * 完全不需要改 APP，也不需要改 BLE 协议
     * ========================================================= */
    /* ========== 核心驻留组件 ========== */
    {
        .widget_id = COMP_DEPTH_1612,
        .span_w = 2, .span_h = 2,
        .elements = ELEM_VALUE | ELEM_UNIT | ELEM_BAR,  /* 2x2 大通栏：无 title，靠 spec.depth int/dec/unit 分离 */
        .font_id = AREX_FONT_ID_HUGE,
        .title_font_id = AREX_FONT_ID_MEDIUM,
        .unit = "m",
        .title = "DEPTH",
        .title_offset_x = 8, .title_offset_y = 4, .title_align = LV_TEXT_ALIGN_LEFT,
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
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m",
        .title = "DEPTH",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_TEXT_ALIGN_CENTER,
        .spec.depth = {
            .int_offset_x = 0, .int_offset_y = 4, .int_align = LV_ALIGN_BOTTOM_MID,
            .dec_offset_x = 2,  .dec_offset_y = 3,
            .unit_offset_x = 0, .unit_offset_y = 1,
            .icon_offset_x = -6, .icon_offset_y = 0, .icon_align = LV_ALIGN_RIGHT_MID
        }
    },
    {
        .widget_id = COMP_NDL_STOP_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT | ELEM_BAR,
        .font_id = AREX_FONT_ID_NDL,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "min",
        .title = "NDL",
        .title_offset_x = 0, .title_offset_y = 0, .title_align = LV_TEXT_ALIGN_LEFT,
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
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "DIVE",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_GAS_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "GAS",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_SYS_1606,
        .span_w = 2, .span_h = 1,
        .elements = 0,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "SYS",
        .title_offset_x = 0, .title_offset_y = 0, .title_align = LV_TEXT_ALIGN_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = 0, .value_align = LV_ALIGN_CENTER }
    },
    {
        .widget_id = COMP_TEMP_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "",
        .title = "TEMP",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_TIME_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "TIME",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_TTS_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "",
        .title = "TTS",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_ASCENT_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m/m",
        .title = "RATE",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = COMP_ASCENT_0812,
        .span_w = 1, .span_h = 2,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT | ELEM_BAR,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m/m",
        .title = "RATE",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = COMP_COMPASS_1612,
        .span_w = 2, .span_h = 2,
        .elements = ELEM_VALUE | ELEM_BAR,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "HDG",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_TEXT_ALIGN_CENTER,
        .spec.compass = {
            .tape_offset_x = 0, .tape_offset_y = 20, .tape_align = LV_ALIGN_TOP_MID,
            .val_offset_x = 0, .val_offset_y = -4, .val_align = LV_ALIGN_BOTTOM_MID
        }
    },
    {
        .widget_id = COMP_BATTERY_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "",
        .title = "BATT",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_STOP_DEPTH_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m",
        .title = "STOP",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_STOP_TIME_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "min",
        .title = "STIME",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_PPO2_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "PPO2",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_SURF_GF_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "SURF.GF",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_GF99_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "%",
        .title = "GF99",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_CNS_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "%",
        .title = "CNS",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_OTU_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "OTU",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_GF_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "GF",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_MOD_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m",
        .title = "MOD",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_CEILING_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m",
        .title = "CEIL",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_GAS_MIX_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "O2/He",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_TISSUE_GF_4012,
        .span_w = 4, .span_h = 2,
        .elements = ELEM_TITLE | ELEM_BAR,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_MEDIUM,
        .unit = NULL,
        .title = "TISSUE(GF)",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.tissue = {
            .chart_offset_x = 0, .chart_offset_y = 20, .chart_align = LV_ALIGN_BOTTOM_RIGHT,
            .bar_count = 16, .bar_spacing = 2
        }
    },
    {
        .widget_id = COMP_TISSUE_RAW_4012,
        .span_w = 4, .span_h = 2,
        .elements = ELEM_TITLE | ELEM_BAR,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_MEDIUM,
        .unit = NULL,
        .title = "TISSUE(RAW)",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.tissue = {
            .chart_offset_x = 0, .chart_offset_y = 20, .chart_align = LV_ALIGN_BOTTOM_RIGHT,
            .bar_count = 16, .bar_spacing = 2
        }
    },
    {
        .widget_id = COMP_GAS_DENS_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "g/L",
        .title = "DENS",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_FIO2_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "%",
        .title = "FIO2",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_HEADING_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "HDG",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_POD_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "",
        .title = "POD",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -2, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_DEPTH_MAX_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m",
        .title = "MAX D",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_DEPTH_AVG_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m",
        .title = "AVG D",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_TEMP_MIN_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "C",
        .title = "MIN T",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_TEMP_AVG_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "C",
        .title = "AVG T",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = COMP_EMPTY,
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
        .widget_id = COMP_EMPTY,
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
        .widget_id = COMP_EMPTY,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "min",
        .title = "NDL MIN",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = COMP_EMPTY,
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
        .widget_id = COMP_EMPTY,
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

const arex_widget_style_t* arex_get_widget_style(arex_widget_id_t id)
{
    for (int i = 0; i < STYLE_COUNT; i++)
    {
        if (g_widget_styles[i].widget_id == id)
        {
            return &g_widget_styles[i];
        }
    }
    return NULL;
}

const char *arex_get_widget_name(arex_widget_id_t id)
{
    const arex_widget_style_t *style = arex_get_widget_style(id);
    if (!style)
    {
        return "???";
    }
    return style->title ? style->title : "";
}
