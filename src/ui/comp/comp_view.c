/*
 * 文件: src/app_ui/ui/comp/comp_view.c
 * 作用: 该文件属于公共组件模块，负责复用样式、通用控件、局部刷新逻辑或组件级显示封装。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时应重点检查布局尺寸、刷新频率与复用接口是否一致，避免局部样式或坐标调整影响同类页面的对齐和更新节奏。
 */

#include "../core/ui_engine.h"
#include "../core/data.h"
#include "../core/ui_settings.h"
#include "../core/vm/ui_vm_dashboard.h"
#include "../screen/screen.h"
#include "comp_view.h"
#include "comp_style.h"
#include "../fonts/fonts.h"

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

/* ============================================================
 * 速率指示器图片资源（6级动态箭头）
 * ============================================================ */
LV_IMG_DECLARE(sudo_up_level0);
LV_IMG_DECLARE(sudo_up_level1);
LV_IMG_DECLARE(sudo_up_level2);
LV_IMG_DECLARE(sudo_up_level3);
LV_IMG_DECLARE(sudo_up_level4);
LV_IMG_DECLARE(sudo_up_level5);
LV_IMG_DECLARE(sudo_up_level6);
LV_IMG_DECLARE(sudo_down_level0);
LV_IMG_DECLARE(sudo_down_level1);
LV_IMG_DECLARE(sudo_down_level2);
LV_IMG_DECLARE(sudo_down_level3);
LV_IMG_DECLARE(sudo_down_level4);
LV_IMG_DECLARE(sudo_down_level5);
LV_IMG_DECLARE(sudo_down_level6);

#define MAX_WIDGET_RENDER_INSTANCES (LEFT_MAX_WIDGETS + (MAX_CUSTOM_CARDS * MAX_5F_WIDGETS))
#define MAX_VALUE_HANDLES          (MAX_WIDGET_RENDER_INSTANCES * 2U)
#define MAX_ASCENT_ICONS           MAX_WIDGET_RENDER_INSTANCES
#define MAX_NDL_ICONS              (LEFT_MAX_WIDGETS + MAX_CUSTOM_CARDS)
#define MAX_TISSUE_WIDGETS         (MAX_CUSTOM_CARDS * 3U)
#define MAX_SYS_WIDGETS            MAX_WIDGET_RENDER_INSTANCES
#define MAX_COMPASS_WIDGETS        MAX_WIDGET_RENDER_INSTANCES
#define COMP_VALUE_HANDLE_ID_MAX   80U
#define COMPASS_DIAL_TICK_COUNT    12U
#define COMPASS_WIDGET_ANIM_TICK_MS          16U
#define COMPASS_WIDGET_ANIM_DIRECT_DIFF_DEG  1.5f
#define COMPASS_WIDGET_ANIM_SLOW_DIFF_DEG    8.0f
#define COMPASS_WIDGET_ANIM_MID_DIFF_DEG     24.0f
#define COMPASS_WIDGET_ANIM_SLOW_STEP_DEG    3.0f
#define COMPASS_WIDGET_ANIM_MID_STEP_DEG     6.0f
#define COMPASS_WIDGET_ANIM_FAST_STEP_DEG    9.0f
#define COMPASS_WIDGET_ANIM_MIN_STEP_DEG     0.20f
#define COMPASS_WIDGET_ANIM_SNAP_EPS_DEG     0.08f
#define COMPASS_WIDGET_ANIM_REVERSAL_EPS_DEG 0.35f
#define COMPASS_WIDGET_PREDICT_SAMPLE_MIN_MS 12U
#define COMPASS_WIDGET_PREDICT_SAMPLE_MAX_MS 320U
#define COMPASS_WIDGET_PREDICT_HOLD_MS       180U
#define COMPASS_WIDGET_PREDICT_MAX_AHEAD_DEG 1.5f
#define COMPASS_WIDGET_PREDICT_SPEED_ALPHA   0.38f
#define COMPASS_WIDGET_PREDICT_MAX_DPS       260.0f
#define COMPASS_WIDGET_PREDICT_REV_GAIN      0.25f
#define COMPASS_WIDGET_TARGET_BACKSTEP_DEG   8.0f
#define COMPASS_WIDGET_TARGET_RELEASE_DEG    7.0f
#define COMPASS_WIDGET_TARGET_RELEASE_MS     32U
#define COMPASS_WIDGET_TARGET_TREND_MIN_DPS  70.0f
#define COMPASS_WIDGET_STILL_SPEED_MAX_DPS   12.0f
#define COMPASS_WIDGET_LARGE_JUMP_DEG        45.0f
#define COMPASS_WIDGET_LARGE_JUMP_MATCH_DEG  12.0f
#define COMPASS_WIDGET_LARGE_JUMP_MOVE_DEG   3.0f
#define COMPASS_WIDGET_LARGE_JUMP_CONFIRM_MS 220U
#define TISSUE_LEAD_COUNT          16U
#define TISSUE_LEAD_BLINK_MS       450U
#define TISSUE_CHART_PAMB_PERMILLE 400
#define TISSUE_CHART_LIMIT_PERMILLE 900
#define TISSUE_CHART_MAX_PERMILLE 1000
#define TISSUE_CHART_PAD_Y       1       /* 图表上下留白 */
#define TISSUE_CHART_HEADROOM    34U     /* 标题区预留 */
#define TISSUE_CHART_BOTTOM_PAD  2       /* 图表底部留白 */
#define DEPTH_1612_DECIMAL_ANCHOR_TEXT ".0"
#define TISSUE_CHART_COLOR_BG      lv_color_make(0x00, 0x00, 0x00) /* 纯黑背景 */
#define TISSUE_CHART_COLOR_PI      lv_color_make(0x00, 0x33, 0x00) /* PI 虚线 20% */
#define TISSUE_CHART_COLOR_SAFE    lv_color_make(0x00, 0x4C, 0x00) /* 安全区 30% */
#define TISSUE_CHART_COLOR_AMB     lv_color_make(0x00, 0x7F, 0x00) /* 环境线 50% */
#define TISSUE_CHART_COLOR_DECO    lv_color_make(0x00, 0xCC, 0x00) /* 排氮区 80% */
#define TISSUE_CHART_COLOR_DANGER  lv_color_make(0x00, 0xFF, 0x00) /* 高危区 100% */

#define TISSUE_LEAD_COLOR_BG       TISSUE_CHART_COLOR_BG

typedef struct
{
    lv_obj_t *comp;
    lv_obj_t *horiz_bg;
    lv_obj_t *main_val;
    lv_obj_t *title_top;
    lv_obj_t *sub_bot;
    comp_id_t widget_id;
    int8_t last_stop_type;
    bool layout_valid;
} ndl_handle_t;

typedef struct
{
    lv_obj_t *chart;
    lv_obj_t *placeholder;
    comp_id_t widget_id;
    ui_vm_deco_t vm;
    uint32_t render_sig;
    bool render_sig_valid;
} tissue_handle_t;

typedef struct
{
    lv_obj_t *batt_lbl;
    lv_obj_t *temp_lbl;
} sys_handle_t;

typedef struct
{
    lv_obj_t *dial;
    uint16_t render_heading;
    uint16_t render_target;
    bool render_locked;
    bool render_valid;
} compass_handle_t;

typedef struct
{
    comp_id_t id;
    uint8_t pod_index;
    uint8_t part;
    uint8_t custom_card_idx;
    bool left_anchor;
    lv_obj_t *label;
    uint16_t next;
    char last_text[32];
} comp_value_handle_t;

enum
{
    COMP_VALUE_HANDLE_PART_FULL = 0U,
    COMP_VALUE_HANDLE_PART_DEPTH_INT = 1U,
    COMP_VALUE_HANDLE_PART_DEPTH_DEC = 2U,
};

/* ============================================================
 * 速率图标指针阵列（支持多DEPTH 模块同时存在
 * 最多支持屏幕上出现 MAX_ASCENT_ICONS 个深度模
 * (左侧锚点 1 + 5F 自定义网格多
 * ============================================================ */

/* ============================================================
 * NDL_STOP 多形态组件句柄（160x60 极限空间内的"变形金刚"
 * 支持屏幕上多NDL 模块（左侧锚1 + 5F 多个
 * 三种状 NDL常/ Safety停留 / Deco停留
 * ============================================================ */
static lv_obj_t *s_img_ascent_rate[MAX_ASCENT_ICONS] __attribute__((section(".psram_bss")));
static uint8_t  s_ascent_icon_count = 0;
static ndl_handle_t s_ndl_handles[MAX_NDL_ICONS] __attribute__((section(".psram_bss")));
static uint8_t      s_ndl_handle_count = 0;
static ui_vm_ndl_stop_t s_ndl_draw_vm[MAX_NDL_ICONS] __attribute__((section(".psram_bss")));
static tissue_handle_t s_tissue_handles[MAX_TISSUE_WIDGETS] __attribute__((section(".psram_bss")));
static uint8_t s_tissue_handle_count;
static lv_timer_t *s_tissue_blink_timer;
static bool s_tissue_blink_phase = true;

static void tissue_blink_stop(void)
{
    if (s_tissue_blink_timer)
    {
        lv_timer_del(s_tissue_blink_timer);
        s_tissue_blink_timer = NULL;
    }
    s_tissue_blink_phase = true;
}

static uint8_t ui_battery_draw_pct(float pct)
{
    if (pct <= 0.0f)
    {
        return 0U;
    }
    if (pct >= 100.0f)
    {
        return 100U;
    }
    return (uint8_t)pct;
}

static int16_t comp_title_edge_offset_x(lv_align_t align, int16_t offset_x)
{
    switch (align)
    {
    case LV_ALIGN_TOP_LEFT:
    case LV_ALIGN_LEFT_MID:
    case LV_ALIGN_BOTTOM_LEFT:
        return (int16_t)(offset_x - COMP_TITLE_EDGE_NUDGE_PX);

    case LV_ALIGN_TOP_RIGHT:
    case LV_ALIGN_RIGHT_MID:
    case LV_ALIGN_BOTTOM_RIGHT:
        return (int16_t)(offset_x + COMP_TITLE_EDGE_NUDGE_PX);

    default:
        return offset_x;
    }
}

static int16_t comp_value_edge_offset_x(lv_align_t align, int16_t offset_x)
{
    switch (align)
    {
    case LV_ALIGN_TOP_LEFT:
    case LV_ALIGN_LEFT_MID:
    case LV_ALIGN_BOTTOM_LEFT:
    case LV_ALIGN_TOP_MID:
    case LV_ALIGN_CENTER:
    case LV_ALIGN_BOTTOM_MID:
    case LV_ALIGN_TOP_RIGHT:
    case LV_ALIGN_RIGHT_MID:
    case LV_ALIGN_BOTTOM_RIGHT:
        return (int16_t)(offset_x + COMP_VALUE_EDGE_NUDGE_PX);

    default:
        return offset_x;
    }
}

/* =========================================================
 * POD 单模具轮转分配状态机
 *
 * 架构：COMP_POD_0806 (33) 是全局唯一真实存在的气瓶模具
 * APP 下发同一POD_0806 可以出现多次（如左侧锚点POD1+POD2，或 5F 中的多个）
 * MCU 通过渲染计数s_pod_render_count 自动分配身份
 *
 * 渲染时拦COMP_POD_0806，根据计数器判断
 *   - 次遇(count=1, 奇数) 分配POD1
 *   - 次遇(count=2, 偶数) 分配POD2
 *
 * user_data 烙印使用高位掩码区分
 *   - POD1: 1000 + COMP_POD_0806 = 1033
 *   - POD2: 2000 + COMP_POD_0806 = 2033
 * ========================================================= */
static uint8_t s_pod_render_count = 0;  /* POD 渲染计数*/

#define POD_TAG_BASE  1000  /* POD 标签基准偏移 */
#define POD1_TAG      (POD_TAG_BASE + COMP_POD_0806)  /* 1033 */
#define POD2_TAG      (2 * POD_TAG_BASE + COMP_POD_0806)  /* 2033 */

/* =========================================================
 * SYS 模块实例句柄表
 *
 * 不能用单个全局 label 指针缓存 SYS。左侧固定栏和右侧自定义页都可能放
 * COMP_SYS_1606，单指针会被最后一个离屏实例覆盖，导致可见实例不刷新。
 * 句柄表按实例保存，刷新时再用 screen_obj_refresh_visible() 裁剪到当前屏。
 * ========================================================= */
static sys_handle_t s_sys_handles[MAX_SYS_WIDGETS] __attribute__((section(".psram_bss")));
static uint8_t s_sys_handle_count;
static compass_handle_t s_compass_handles[MAX_COMPASS_WIDGETS] __attribute__((section(".psram_bss")));
static uint8_t s_compass_handle_count;
static lv_timer_t *s_compass_anim_timer;
static float s_compass_display_heading_deg;
static float s_compass_display_last_step_deg;
static float s_compass_display_target_deg;
static float s_compass_display_velocity_dps;
static float s_compass_display_pending_target_deg;
static float s_compass_display_pending_delta_deg;
static float s_compass_display_large_jump_target_deg;
static float s_compass_display_large_jump_delta_deg;
static uint32_t s_compass_display_target_tick;
static uint32_t s_compass_display_pending_target_tick;
static uint32_t s_compass_display_large_jump_tick;
static uint8_t s_compass_display_pending_target_count;
static bool s_compass_display_target_valid;
static bool s_compass_display_seeded;
static bool s_compass_display_pending_target_valid;
static bool s_compass_display_large_jump_valid;
static bool s_compass_heading_available;
static bool s_compass_has_valid_heading;
static bool s_compass_unavailable_drawn;
static uint16_t s_compass_display_heading_text = 0xFFFFU;
/* 1612 深度大卡使用“整数+小数”两个 label，句柄池按双句柄上限预留。 */
static comp_value_handle_t s_value_handles[MAX_VALUE_HANDLES] __attribute__((section(".psram_bss")));
static uint16_t s_value_handle_heads[COMP_VALUE_HANDLE_ID_MAX];
static uint16_t s_value_handle_count;
static lv_obj_t *s_depth_unit_labels[MAX_WIDGET_RENDER_INSTANCES] __attribute__((section(".psram_bss")));
static uint16_t s_depth_unit_label_count;

static void compass_widget_anim_timer_ensure(void);
static void compass_widget_refresh_dials(void);
void comp_refresh_heading_text_widgets(void);

static bool ui_obj_is_valid(lv_obj_t **obj_ref)
{
    /* 如果对象已经被 LVGL 销毁，就立刻把缓存指针清空。 */
    if (obj_ref == NULL || *obj_ref == NULL)
    {
        return false;
    }

    if (!lv_obj_is_valid(*obj_ref))
    {
        *obj_ref = NULL;
        return false;
    }

    return true;
}

static void comp_view_label_set_text_if_changed(lv_obj_t *label, const char *text)
{
    const char *old_text;

    if (label == NULL || !lv_obj_is_valid(label) || !lv_obj_check_type(label, &lv_label_class) || text == NULL)
    {
        return;
    }

    old_text = lv_label_get_text(label);
    if (old_text != NULL && strcmp(old_text, text) == 0)
    {
        return;
    }

    lv_label_set_text(label, text);
}

static void comp_depth_unit_label_register(lv_obj_t *label)
{
    if (label == NULL || s_depth_unit_label_count >= (uint16_t)(sizeof(s_depth_unit_labels) / sizeof(s_depth_unit_labels[0])))
    {
        return;
    }

    s_depth_unit_labels[s_depth_unit_label_count++] = label;
}

void comp_refresh_depth_unit_labels(void)
{
    const char *unit = bus_get_depth_unit_label();

    for (uint16_t i = 0U; i < s_depth_unit_label_count; i++)
    {
        if (ui_obj_is_valid(&s_depth_unit_labels[i]))
        {
            comp_view_label_set_text_if_changed(s_depth_unit_labels[i], unit);
        }
    }
}

static bool comp_view_obj_set_hidden_if_changed(lv_obj_t *obj, bool hidden)
{
    bool is_hidden;

    if (obj == NULL)
    {
        return false;
    }

    is_hidden = lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);
    if (is_hidden == hidden)
    {
        return false;
    }

    if (hidden)
    {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
    return true;
}

static void comp_view_label_set_text_fmt_if_changed(lv_obj_t *label, const char *fmt, ...)
{
    char buf[32];
    va_list args;

    if (label == NULL || fmt == NULL)
    {
        return;
    }

    va_start(args, fmt);
    (void)vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    comp_view_label_set_text_if_changed(label, buf);
}

static uint16_t comp_ndl_stop_display_minutes(uint16_t seconds)
{
    return (uint16_t)((seconds + 59U) / 60U);
}

static void comp_ndl_stop_set_time_text(lv_obj_t *label, uint16_t seconds)
{
#if UI_NDL_STOP_TIME_MINUTE_ONLY
    comp_view_label_set_text_fmt_if_changed(label, "%umin", (unsigned)comp_ndl_stop_display_minutes(seconds));
#else
    comp_view_label_set_text_fmt_if_changed(label, "%u:%02u", (unsigned)(seconds / 60U), (unsigned)(seconds % 60U));
#endif
}

static float compass_widget_normalize_heading_float(float deg)
{
    while (deg < 0.0f)
    {
        deg += 360.0f;
    }
    while (deg >= 360.0f)
    {
        deg -= 360.0f;
    }
    return deg;
}

static float compass_widget_shortest_delta_float(float from_deg, float to_deg)
{
    float diff = to_deg - from_deg;

    while (diff > 180.0f)
    {
        diff -= 360.0f;
    }
    while (diff < -180.0f)
    {
        diff += 360.0f;
    }
    return diff;
}

static int compass_widget_round_float_to_int(float value)
{
    return (value >= 0.0f) ? (int)(value + 0.5f) : (int)(value - 0.5f);
}

static uint16_t compass_widget_heading_from_float(float heading)
{
    int value = compass_widget_round_float_to_int(
        compass_widget_normalize_heading_float(heading));

    value %= 360;
    if (value < 0)
    {
        value += 360;
    }
    return (uint16_t)value;
}

static void compass_widget_display_seed(uint16_t heading)
{
    s_compass_display_heading_deg = (float)(heading % 360U);
    s_compass_display_last_step_deg = 0.0f;
    s_compass_display_target_deg = s_compass_display_heading_deg;
    s_compass_display_velocity_dps = 0.0f;
    s_compass_display_pending_target_deg = 0.0f;
    s_compass_display_pending_delta_deg = 0.0f;
    s_compass_display_large_jump_target_deg = 0.0f;
    s_compass_display_large_jump_delta_deg = 0.0f;
    s_compass_display_target_tick = lv_tick_get();
    s_compass_display_pending_target_tick = 0U;
    s_compass_display_large_jump_tick = 0U;
    s_compass_display_pending_target_count = 0U;
    s_compass_display_target_valid = true;
    s_compass_display_seeded = true;
    s_compass_display_pending_target_valid = false;
    s_compass_display_large_jump_valid = false;
    s_compass_display_heading_text = 0xFFFFU;
}

static void compass_widget_display_clear_pending_target(void)
{
    s_compass_display_pending_target_valid = false;
    s_compass_display_pending_target_count = 0U;
    s_compass_display_pending_target_tick = 0U;
    s_compass_display_pending_target_deg = 0.0f;
    s_compass_display_pending_delta_deg = 0.0f;
}

static void compass_widget_display_clear_large_jump(void)
{
    s_compass_display_large_jump_valid = false;
    s_compass_display_large_jump_target_deg = 0.0f;
    s_compass_display_large_jump_delta_deg = 0.0f;
    s_compass_display_large_jump_tick = 0U;
}

/* 通用罗盘组件与主罗盘卡片共用静止单帧大跳确认策略。 */
static bool compass_widget_display_hold_unconfirmed_large_jump(float new_target,
                                                               float delta,
                                                               uint32_t now_tick)
{
    float pending_move;
    bool continues_turn;

    if ((fabsf(s_compass_display_velocity_dps) > COMPASS_WIDGET_STILL_SPEED_MAX_DPS) ||
        (fabsf(delta) < COMPASS_WIDGET_LARGE_JUMP_DEG))
    {
        compass_widget_display_clear_large_jump();
        return false;
    }

    if (!s_compass_display_large_jump_valid)
    {
        s_compass_display_large_jump_target_deg = new_target;
        s_compass_display_large_jump_delta_deg = delta;
        s_compass_display_large_jump_tick = now_tick;
        s_compass_display_large_jump_valid = true;
        return true;
    }

    pending_move = compass_widget_shortest_delta_float(
        s_compass_display_large_jump_target_deg,
        new_target);
    continues_turn =
        ((s_compass_display_large_jump_delta_deg > 0.0f) &&
         (delta > 0.0f) &&
         (pending_move >= COMPASS_WIDGET_LARGE_JUMP_MOVE_DEG)) ||
        ((s_compass_display_large_jump_delta_deg < 0.0f) &&
         (delta < 0.0f) &&
         (pending_move <= -COMPASS_WIDGET_LARGE_JUMP_MOVE_DEG));
    if (continues_turn)
    {
        compass_widget_display_clear_large_jump();
        return false;
    }

    if (fabsf(pending_move) > COMPASS_WIDGET_LARGE_JUMP_MATCH_DEG)
    {
        s_compass_display_large_jump_target_deg = new_target;
        s_compass_display_large_jump_delta_deg = delta;
        s_compass_display_large_jump_tick = now_tick;
        return true;
    }

    if ((now_tick - s_compass_display_large_jump_tick) <
        COMPASS_WIDGET_LARGE_JUMP_CONFIRM_MS)
    {
        return true;
    }

    compass_widget_display_clear_large_jump();
    return false;
}

static void compass_widget_display_decay_velocity(float factor)
{
    s_compass_display_velocity_dps *= factor;
    if (fabsf(s_compass_display_velocity_dps) < 2.0f)
    {
        s_compass_display_velocity_dps = 0.0f;
    }
}

/*
 * 和主罗盘卡片保持同一策略：过滤单帧目标反向毛刺，但第二帧仍反向时
 * 立即释放，避免真实换向被显示层拖慢。
 */
static bool compass_widget_display_hold_transient_backstep(float new_target,
                                                            float delta,
                                                            uint32_t now_tick)
{
    bool has_trend = fabsf(s_compass_display_velocity_dps) >= COMPASS_WIDGET_TARGET_TREND_MIN_DPS;
    bool opposite_to_trend =
        (s_compass_display_velocity_dps > 0.0f && delta < 0.0f) ||
        (s_compass_display_velocity_dps < 0.0f && delta > 0.0f);
    bool large_backstep = fabsf(delta) >= COMPASS_WIDGET_TARGET_BACKSTEP_DEG;
    float pending_move;
    bool same_backstep;
    bool continues_backstep;
    bool close_to_target;
    uint32_t age_ms;

    if (!has_trend || !opposite_to_trend || !large_backstep)
    {
        compass_widget_display_clear_pending_target();
        return false;
    }

    if (!s_compass_display_pending_target_valid ||
        fabsf(compass_widget_shortest_delta_float(s_compass_display_pending_target_deg, new_target)) >
            COMPASS_WIDGET_TARGET_RELEASE_DEG)
    {
        s_compass_display_pending_target_deg = new_target;
        s_compass_display_pending_delta_deg = delta;
        s_compass_display_pending_target_tick = now_tick;
        s_compass_display_pending_target_count = 1U;
        s_compass_display_pending_target_valid = true;
        compass_widget_display_decay_velocity(0.35f);
        s_compass_display_target_tick = now_tick;
        return true;
    }

    pending_move = compass_widget_shortest_delta_float(s_compass_display_pending_target_deg, new_target);
    same_backstep = fabsf(pending_move) <= 0.5f;
    continues_backstep =
        (s_compass_display_pending_delta_deg > 0.0f && pending_move > 0.5f) ||
        (s_compass_display_pending_delta_deg < 0.0f && pending_move < -0.5f);
    if ((same_backstep || continues_backstep) && s_compass_display_pending_target_count < 3U)
    {
        s_compass_display_pending_target_count++;
    }

    s_compass_display_pending_target_deg = new_target;
    close_to_target = fabsf(compass_widget_shortest_delta_float(s_compass_display_target_deg, new_target)) <=
                      COMPASS_WIDGET_TARGET_RELEASE_DEG;
    age_ms = now_tick - s_compass_display_pending_target_tick;
    if (close_to_target ||
        ((same_backstep || continues_backstep) && s_compass_display_pending_target_count >= 2U) ||
        age_ms >= COMPASS_WIDGET_TARGET_RELEASE_MS)
    {
        compass_widget_display_clear_pending_target();
        return false;
    }

    compass_widget_display_decay_velocity(0.35f);
    s_compass_display_target_tick = now_tick;
    return true;
}

static void compass_widget_display_release_stale_pending_target(uint32_t now_tick)
{
    if (!s_compass_display_pending_target_valid)
    {
        return;
    }

    if ((now_tick - s_compass_display_pending_target_tick) < COMPASS_WIDGET_TARGET_RELEASE_MS)
    {
        return;
    }

    /* 反向毛刺只短暂屏蔽；超时后按真实换向处理，避免显示层停住。 */
    s_compass_display_target_deg = s_compass_display_pending_target_deg;
    s_compass_display_target_tick = now_tick;
    s_compass_display_velocity_dps = 0.0f;
    s_compass_display_last_step_deg = 0.0f;
    compass_widget_display_clear_pending_target();
}

static void compass_widget_display_note_target(uint16_t heading)
{
    uint32_t now_tick = lv_tick_get();
    float new_target = (float)(heading % 360U);

    if (!s_compass_display_target_valid)
    {
        s_compass_display_target_deg = new_target;
        s_compass_display_velocity_dps = 0.0f;
        s_compass_display_target_tick = now_tick;
        s_compass_display_target_valid = true;
        return;
    }

    uint32_t dt_ms = now_tick - s_compass_display_target_tick;
    float delta = compass_widget_shortest_delta_float(s_compass_display_target_deg, new_target);
    if (fabsf(delta) <= 0.01f)
    {
        compass_widget_display_clear_pending_target();
        compass_widget_display_clear_large_jump();
        /* 重复 UI 刷新不是新航向样本，保留真实目标时间供样本间补帧。 */
        return;
    }

    if (compass_widget_display_hold_unconfirmed_large_jump(new_target, delta, now_tick))
    {
        return;
    }

    if (compass_widget_display_hold_transient_backstep(new_target, delta, now_tick))
    {
        return;
    }

    if (dt_ms >= COMPASS_WIDGET_PREDICT_SAMPLE_MIN_MS &&
        dt_ms <= COMPASS_WIDGET_PREDICT_SAMPLE_MAX_MS)
    {
        float sample_dps = delta * 1000.0f / (float)dt_ms;
        if (sample_dps > COMPASS_WIDGET_PREDICT_MAX_DPS)
        {
            sample_dps = COMPASS_WIDGET_PREDICT_MAX_DPS;
        }
        else if (sample_dps < -COMPASS_WIDGET_PREDICT_MAX_DPS)
        {
            sample_dps = -COMPASS_WIDGET_PREDICT_MAX_DPS;
        }

        if ((s_compass_display_velocity_dps > 0.0f && sample_dps < 0.0f) ||
            (s_compass_display_velocity_dps < 0.0f && sample_dps > 0.0f))
        {
            s_compass_display_velocity_dps = sample_dps * COMPASS_WIDGET_PREDICT_REV_GAIN;
        }
        else
        {
            s_compass_display_velocity_dps =
                (s_compass_display_velocity_dps * (1.0f - COMPASS_WIDGET_PREDICT_SPEED_ALPHA)) +
                (sample_dps * COMPASS_WIDGET_PREDICT_SPEED_ALPHA);
        }
    }
    else if (dt_ms > COMPASS_WIDGET_PREDICT_SAMPLE_MAX_MS)
    {
        s_compass_display_velocity_dps = 0.0f;
    }

    s_compass_display_target_deg = new_target;
    s_compass_display_target_tick = now_tick;
    compass_widget_display_clear_pending_target();
}

static float compass_widget_display_predict_target(void)
{
    uint32_t now_tick = lv_tick_get();
    uint32_t dt_ms;
    float ahead;
    float decay = 1.0f;

    if (!s_compass_display_target_valid)
    {
        return (float)(bus_get_heading() % 360U);
    }

    compass_widget_display_release_stale_pending_target(now_tick);

    dt_ms = now_tick - s_compass_display_target_tick;
    if (dt_ms >= COMPASS_WIDGET_PREDICT_SAMPLE_MAX_MS)
    {
        s_compass_display_velocity_dps = 0.0f;
        return s_compass_display_target_deg;
    }

    if (dt_ms > COMPASS_WIDGET_PREDICT_HOLD_MS)
    {
        decay = (float)(COMPASS_WIDGET_PREDICT_SAMPLE_MAX_MS - dt_ms) /
                (float)(COMPASS_WIDGET_PREDICT_SAMPLE_MAX_MS - COMPASS_WIDGET_PREDICT_HOLD_MS);
        dt_ms = COMPASS_WIDGET_PREDICT_HOLD_MS;
    }

    ahead = s_compass_display_velocity_dps * (float)dt_ms / 1000.0f;
    if (ahead > COMPASS_WIDGET_PREDICT_MAX_AHEAD_DEG)
    {
        ahead = COMPASS_WIDGET_PREDICT_MAX_AHEAD_DEG;
    }
    else if (ahead < -COMPASS_WIDGET_PREDICT_MAX_AHEAD_DEG)
    {
        ahead = -COMPASS_WIDGET_PREDICT_MAX_AHEAD_DEG;
    }
    ahead *= decay;

    return compass_widget_normalize_heading_float(s_compass_display_target_deg + ahead);
}

static float compass_widget_display_adaptive_step(float diff, float abs_diff)
{
    float cap;
    float alpha;
    float step;

    if (abs_diff <= COMPASS_WIDGET_ANIM_DIRECT_DIFF_DEG)
    {
        return diff;
    }

    if (abs_diff < COMPASS_WIDGET_ANIM_SLOW_DIFF_DEG)
    {
        cap = COMPASS_WIDGET_ANIM_SLOW_STEP_DEG;
        alpha = 0.72f;
    }
    else if (abs_diff < COMPASS_WIDGET_ANIM_MID_DIFF_DEG)
    {
        cap = COMPASS_WIDGET_ANIM_MID_STEP_DEG;
        alpha = 0.62f;
    }
    else
    {
        cap = COMPASS_WIDGET_ANIM_FAST_STEP_DEG;
        alpha = 0.50f;
    }

    step = diff * alpha;
    if (fabsf(step) > cap)
    {
        step = (step > 0.0f) ? cap : -cap;
    }
    if (fabsf(step) < COMPASS_WIDGET_ANIM_MIN_STEP_DEG)
    {
        step = (diff > 0.0f) ? COMPASS_WIDGET_ANIM_MIN_STEP_DEG : -COMPASS_WIDGET_ANIM_MIN_STEP_DEG;
    }
    if (fabsf(step) > abs_diff)
    {
        step = diff;
    }

    return step;
}

static bool compass_widget_display_step_towards_target(void)
{
    float target = compass_widget_display_predict_target();
    float diff;
    float abs_diff;
    float step;
    bool reversed;

    if (!s_compass_display_seeded)
    {
        compass_widget_display_seed(bus_get_heading());
        return true;
    }

    diff = compass_widget_shortest_delta_float(s_compass_display_heading_deg, target);
    abs_diff = fabsf(diff);
    if (abs_diff <= COMPASS_WIDGET_ANIM_SNAP_EPS_DEG)
    {
        s_compass_display_heading_deg = target;
        if (fabsf(s_compass_display_velocity_dps) < 2.0f)
        {
            s_compass_display_last_step_deg = 0.0f;
        }
        return false;
    }

    reversed = (fabsf(s_compass_display_last_step_deg) >= COMPASS_WIDGET_ANIM_REVERSAL_EPS_DEG) &&
               ((s_compass_display_last_step_deg > 0.0f && diff < 0.0f) ||
                (s_compass_display_last_step_deg < 0.0f && diff > 0.0f));

    if (reversed)
    {
        /*
         * 换向时立即丢弃上一方向的显示惯性，避免罗盘数字/指针短暂停住。
         */
        s_compass_display_last_step_deg = 0.0f;
        s_compass_display_velocity_dps = 0.0f;
    }

    /*
     * 通用罗盘组件和主罗盘页保持同一显示策略：目标可突变，屏幕按
     * 自适应速度追赶，避免固定逐度补帧带来的拖尾。
     */
    step = compass_widget_display_adaptive_step(diff, abs_diff);

    s_compass_display_heading_deg =
        compass_widget_normalize_heading_float(s_compass_display_heading_deg + step);
    s_compass_display_last_step_deg = step;
    return true;
}

uint16_t comp_compass_display_heading_deg(void)
{
    if (!s_compass_display_seeded)
    {
        return bus_get_heading() % 360U;
    }

    return compass_widget_heading_from_float(s_compass_display_heading_deg);
}

static bool compass_widget_heading_text_visible(void)
{
    uint16_t idx;

    if ((uint8_t)COMP_HEADING_0806 >= COMP_VALUE_HANDLE_ID_MAX)
    {
        return false;
    }

    idx = s_value_handle_heads[(uint8_t)COMP_HEADING_0806];
    while (idx != UINT16_MAX && idx < s_value_handle_count)
    {
        comp_value_handle_t *h = &s_value_handles[idx];
        if (ui_obj_is_valid(&h->label) && screen_obj_refresh_visible(h->label))
        {
            return true;
        }
        idx = h->next;
    }

    return false;
}

static bool compass_widget_dial_visible(void)
{
    for (uint8_t i = 0U; i < s_compass_handle_count; i++)
    {
        compass_handle_t *h = &s_compass_handles[i];
        if (ui_obj_is_valid(&h->dial) && screen_obj_refresh_visible(h->dial))
        {
            return true;
        }
    }

    return false;
}

void comp_refresh_heading_text_widgets(void)
{
    uint16_t idx;
    uint16_t display_heading;
    char text[8];

    if ((uint8_t)COMP_HEADING_0806 >= COMP_VALUE_HANDLE_ID_MAX)
    {
        return;
    }

    display_heading = comp_compass_display_heading_deg();
    (void)snprintf(text, sizeof(text), "%03u", (unsigned)display_heading);

    idx = s_value_handle_heads[(uint8_t)COMP_HEADING_0806];
    while (idx != UINT16_MAX && idx < s_value_handle_count)
    {
        comp_value_handle_t *h = &s_value_handles[idx];

        if (ui_obj_is_valid(&h->label) &&
            screen_obj_refresh_visible(h->label) &&
            (strncmp(h->last_text, text, sizeof(h->last_text) - 1U) != 0))
        {
            comp_view_label_set_text_if_changed(h->label, text);
            (void)snprintf(h->last_text, sizeof(h->last_text), "%s", text);
        }
        idx = h->next;
    }

    s_compass_display_heading_text = display_heading;
}

static void compass_widget_anim_timer_cb(lv_timer_t *timer)
{
    bool stepped;
    bool had_valid_heading;

    (void)timer;

    if (!compass_widget_dial_visible() && !compass_widget_heading_text_visible())
    {
        return;
    }

    had_valid_heading = s_compass_has_valid_heading;
    s_compass_heading_available = bus_get_heading_available();
    if (s_compass_heading_available && !had_valid_heading)
    {
        s_compass_has_valid_heading = true;
        s_compass_unavailable_drawn = false;
        compass_widget_display_seed(bus_get_heading());
        comp_refresh_heading_text_widgets();
        compass_widget_refresh_dials();
        return;
    }
    if (!s_compass_heading_available)
    {
        /*
         * heading 不可用只表示当前帧不适合刷新目标角。头戴潜水场景下
         * 不能把用户视野里的罗盘清成 "---"，这里冻结最后显示角度，并
         * 只在刚进入冻结态时刷新一次，避免 16ms timer 持续重绘。
         */
        if (!s_compass_display_seeded)
        {
            compass_widget_display_seed(bus_get_heading());
        }
        if (!s_compass_unavailable_drawn)
        {
            compass_widget_display_clear_pending_target();
            compass_widget_display_clear_large_jump();
            s_compass_display_target_valid = false;
            s_compass_display_velocity_dps = 0.0f;
            s_compass_display_last_step_deg = 0.0f;
            comp_refresh_heading_text_widgets();
            compass_widget_refresh_dials();
            s_compass_unavailable_drawn = true;
        }
        return;
    }

    s_compass_unavailable_drawn = false;
    stepped = compass_widget_display_step_towards_target();
    if (!stepped)
    {
        comp_refresh_heading_text_widgets();
        return;
    }

    comp_refresh_heading_text_widgets();
    compass_widget_refresh_dials();
}

static void compass_widget_anim_timer_ensure(void)
{
    if (s_compass_anim_timer == NULL)
    {
        s_compass_anim_timer =
            lv_timer_create(compass_widget_anim_timer_cb, COMPASS_WIDGET_ANIM_TICK_MS, NULL);
    }
}

static int16_t compass_normalize_angle(int16_t angle)
{
    angle %= 360;
    if (angle < 0) angle = (int16_t)(angle + 360);
    return angle;
}

static void compass_point_from_angle(int16_t cx, int16_t cy, int16_t angle_deg, int16_t radius, lv_point_t *out)
{
    int16_t angle;
    int32_t x;
    int32_t y;

    if (out == NULL)
    {
        return;
    }

    angle = compass_normalize_angle(angle_deg);
    x = (int32_t)radius * (int32_t)lv_trigo_sin(angle);
    y = (int32_t)radius * (int32_t)lv_trigo_sin((int16_t)(angle + 90));
    out->x = (lv_coord_t)(cx + (lv_coord_t)(x >> LV_TRIGO_SHIFT));
    out->y = (lv_coord_t)(cy - (lv_coord_t)(y >> LV_TRIGO_SHIFT));
}

static void compass_invalidate_area_include(lv_area_t *area,
                                            bool *area_valid,
                                            const lv_point_t *point,
                                            lv_coord_t pad)
{
    lv_coord_t x1;
    lv_coord_t y1;
    lv_coord_t x2;
    lv_coord_t y2;

    if (area == NULL || area_valid == NULL || point == NULL)
    {
        return;
    }

    x1 = (lv_coord_t)(point->x - pad);
    y1 = (lv_coord_t)(point->y - pad);
    x2 = (lv_coord_t)(point->x + pad);
    y2 = (lv_coord_t)(point->y + pad);
    if (!*area_valid)
    {
        area->x1 = x1;
        area->y1 = y1;
        area->x2 = x2;
        area->y2 = y2;
        *area_valid = true;
        return;
    }

    if (x1 < area->x1) area->x1 = x1;
    if (y1 < area->y1) area->y1 = y1;
    if (x2 > area->x2) area->x2 = x2;
    if (y2 > area->y2) area->y2 = y2;
}

static void compass_invalidate_marker_area(lv_area_t *area,
                                           bool *area_valid,
                                           int16_t cx,
                                           int16_t cy,
                                           int16_t radius,
                                           uint16_t heading,
                                           uint16_t target)
{
    int16_t target_rel = (int16_t)((int)target - (int)heading);
    lv_point_t marker_inner;
    lv_point_t marker_outer;

    while (target_rel > 180) target_rel = (int16_t)(target_rel - 360);
    while (target_rel < -180) target_rel = (int16_t)(target_rel + 360);
    compass_point_from_angle(cx, cy, target_rel, (int16_t)(radius - 10), &marker_inner);
    compass_point_from_angle(cx, cy, target_rel, radius, &marker_outer);
    compass_invalidate_area_include(area, area_valid, &marker_inner, 3);
    compass_invalidate_area_include(area, area_valid, &marker_outer, 3);
}

static void compass_widget_invalidate_dial(compass_handle_t *h,
                                           uint16_t heading,
                                           bool locked,
                                           uint16_t target)
{
    lv_area_t invalid_area;
    bool area_valid = false;
    const lv_area_t *coords;
    int16_t w;
    int16_t height;
    int16_t cx;
    int16_t cy;
    int16_t radius;
    lv_point_t center;
    lv_point_t old_tip;
    lv_point_t new_tip;

    if (h == NULL || h->dial == NULL)
    {
        return;
    }
    if (!h->render_valid)
    {
        lv_obj_invalidate(h->dial);
        goto cache_state;
    }
    if (h->render_heading == heading &&
        h->render_locked == locked &&
        (!locked || h->render_target == target))
    {
        return;
    }

    coords = &h->dial->coords;
    w = (int16_t)lv_area_get_width(coords);
    height = (int16_t)lv_area_get_height(coords);
    cx = (int16_t)(coords->x1 + w / 2);
    cy = (int16_t)(coords->y1 + height / 2);
    radius = (w < height) ? (int16_t)(w / 2 - 5) : (int16_t)(height / 2 - 5);
    if (radius < 16)
    {
        lv_obj_invalidate(h->dial);
        goto cache_state;
    }

    /* 圆环、刻度和方位字母是静态背景。航向补帧只失效旧/新指针与
     * 目标标记覆盖的区域，避免 16ms timer 每次刷新整个罗盘组件。 */
    center.x = (lv_coord_t)cx;
    center.y = (lv_coord_t)cy;
    compass_point_from_angle(cx, cy, (int16_t)h->render_heading,
                             (int16_t)(radius - 12), &old_tip);
    compass_point_from_angle(cx, cy, (int16_t)heading,
                             (int16_t)(radius - 12), &new_tip);
    compass_invalidate_area_include(&invalid_area, &area_valid, &center, 5);
    compass_invalidate_area_include(&invalid_area, &area_valid, &old_tip, 5);
    compass_invalidate_area_include(&invalid_area, &area_valid, &new_tip, 5);
    if (h->render_locked)
    {
        compass_invalidate_marker_area(&invalid_area, &area_valid, cx, cy, radius,
                                       h->render_heading, h->render_target);
    }
    if (locked)
    {
        compass_invalidate_marker_area(&invalid_area, &area_valid, cx, cy, radius,
                                       heading, target);
    }

    if (area_valid)
    {
        if (invalid_area.x1 < coords->x1) invalid_area.x1 = coords->x1;
        if (invalid_area.y1 < coords->y1) invalid_area.y1 = coords->y1;
        if (invalid_area.x2 > coords->x2) invalid_area.x2 = coords->x2;
        if (invalid_area.y2 > coords->y2) invalid_area.y2 = coords->y2;
        lv_obj_invalidate_area(h->dial, &invalid_area);
    }

cache_state:
    h->render_heading = heading;
    h->render_target = target;
    h->render_locked = locked;
    h->render_valid = true;
}

static void compass_widget_refresh_dials(void)
{
    uint16_t heading = comp_compass_display_heading_deg();
    bool locked = bus_is_heading_locked();
    uint16_t target = locked ? bus_get_heading_target() : 0U;

    for (uint8_t i = 0U; i < s_compass_handle_count; i++)
    {
        compass_handle_t *h = &s_compass_handles[i];
        if (ui_obj_is_valid(&h->dial) && screen_obj_refresh_visible(h->dial))
        {
            compass_widget_invalidate_dial(h, heading, locked, target);
        }
    }
}

static void compass_dial_draw_label(lv_draw_ctx_t *draw_ctx, const char *text, int16_t cx, int16_t cy, int16_t angle_deg, int16_t radius)
{
    lv_point_t p;
    lv_area_t label_area;
    lv_draw_label_dsc_t label_dsc;

    compass_point_from_angle(cx, cy, angle_deg, radius, &p);
    label_area.x1 = p.x - 12;
    label_area.y1 = p.y - 8;
    label_area.x2 = p.x + 12;
    label_area.y2 = p.y + 8;

    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.color = GREEN;
    label_dsc.font = get_font(FONT_ID_SMALL);
    label_dsc.align = LV_TEXT_ALIGN_CENTER;
    label_dsc.opa = LV_OPA_COVER;
    lv_draw_label(draw_ctx, &label_dsc, &label_area, text, NULL);
}

static void compass_dial_draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);
    lv_area_t *area = &obj->coords;
    int16_t w = (int16_t)lv_area_get_width(area);
    int16_t h = (int16_t)lv_area_get_height(area);
    int16_t cx = (int16_t)(area->x1 + w / 2);
    int16_t cy = (int16_t)(area->y1 + h / 2);
    int16_t radius = (w < h) ? (int16_t)(w / 2 - 5) : (int16_t)(h / 2 - 5);
    uint16_t heading = comp_compass_display_heading_deg();

    if (radius < 16)
    {
        return;
    }

    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = lv_color_make(0x00, 0x00, 0x00);
    rect_dsc.bg_opa = LV_OPA_COVER;
    lv_draw_rect(draw_ctx, &rect_dsc, area);

    lv_area_t ring_area = {cx - radius, cy - radius, cx + radius, cy + radius};
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_opa = LV_OPA_TRANSP;
    rect_dsc.border_color = GREEN;
    rect_dsc.border_opa = LV_OPA_50;
    rect_dsc.border_width = 1;
    rect_dsc.radius = LV_RADIUS_CIRCLE;
    lv_draw_rect(draw_ctx, &rect_dsc, &ring_area);

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = GREEN;
    line_dsc.opa = LV_OPA_COVER;

    for (uint8_t i = 0U; i < COMPASS_DIAL_TICK_COUNT; i++)
    {
        int16_t angle = (int16_t)(i * 30U);
        bool major = (i % 3U) == 0U;
        lv_point_t p1;
        lv_point_t p2;

        line_dsc.width = major ? 2 : 1;
        line_dsc.opa = major ? LV_OPA_COVER : LV_OPA_50;
        compass_point_from_angle(cx, cy, angle, (int16_t)(radius - (major ? 8 : 5)), &p1);
        compass_point_from_angle(cx, cy, angle, radius, &p2);
        lv_draw_line(draw_ctx, &line_dsc, &p1, &p2);
    }

    compass_dial_draw_label(draw_ctx, "N", cx, cy, 0, (int16_t)(radius - 18));
    compass_dial_draw_label(draw_ctx, "E", cx, cy, 90, (int16_t)(radius - 18));
    compass_dial_draw_label(draw_ctx, "S", cx, cy, 180, (int16_t)(radius - 18));
    compass_dial_draw_label(draw_ctx, "W", cx, cy, 270, (int16_t)(radius - 18));

    line_dsc.color = GREEN;
    line_dsc.width = 3;
    line_dsc.opa = LV_OPA_COVER;
    lv_point_t center = {cx, cy};
    lv_point_t needle_tip;
    compass_point_from_angle(cx, cy, (int16_t)heading, (int16_t)(radius - 12), &needle_tip);
    lv_draw_line(draw_ctx, &line_dsc, &center, &needle_tip);

    if (bus_is_heading_locked())
    {
        int16_t target_rel = (int16_t)((int)bus_get_heading_target() - (int)heading);
        lv_point_t marker_inner;
        lv_point_t marker_outer;

        while (target_rel > 180) target_rel = (int16_t)(target_rel - 360);
        while (target_rel < -180) target_rel = (int16_t)(target_rel + 360);

        line_dsc.width = 2;
        line_dsc.opa = LV_OPA_COVER;
        compass_point_from_angle(cx, cy, target_rel, (int16_t)(radius - 10), &marker_inner);
        compass_point_from_angle(cx, cy, target_rel, radius, &marker_outer);
        lv_draw_line(draw_ctx, &line_dsc, &marker_inner, &marker_outer);
    }

    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = GREEN;
    rect_dsc.bg_opa = LV_OPA_COVER;
    rect_dsc.radius = LV_RADIUS_CIRCLE;
    lv_area_t dot_area = {cx - 3, cy - 3, cx + 3, cy + 3};
    lv_draw_rect(draw_ctx, &rect_dsc, &dot_area);
}

void comp_value_handle_reset(void)
{
    memset(s_value_handles, 0, sizeof(s_value_handles));
    for (uint8_t i = 0U; i < COMP_VALUE_HANDLE_ID_MAX; i++)
    {
        s_value_handle_heads[i] = UINT16_MAX;
    }
    s_value_handle_count = 0U;
}

static void comp_value_handle_register_part(comp_id_t id,
                                            uint8_t pod_index,
                                            uint8_t part,
                                            lv_obj_t *label)
{
    comp_value_handle_t *h;
    uint16_t idx;

    if (label == NULL || id == COMP_EMPTY)
    {
        return;
    }

    if ((uint8_t)id >= COMP_VALUE_HANDLE_ID_MAX)
    {
        return;
    }

    if (s_value_handle_count >= (uint16_t)(sizeof(s_value_handles) / sizeof(s_value_handles[0])))
    {
        return;
    }

    idx = s_value_handle_count++;
    h = &s_value_handles[idx];
    memset(h, 0, sizeof(*h));
    h->id = id;
    h->pod_index = pod_index;
    h->part = part;
    h->label = label;
    h->custom_card_idx = 0xFFU;
    h->next = s_value_handle_heads[(uint8_t)id];
    (void)snprintf(h->last_text, sizeof(h->last_text), "%s", lv_label_get_text(label));

    if (g_left_anchor_obj != NULL && lv_obj_is_valid(g_left_anchor_obj))
    {
        lv_obj_t *p = label;
        while (p != NULL)
        {
            if (p == g_left_anchor_obj)
            {
                h->left_anchor = true;
                break;
            }
            p = lv_obj_get_parent(p);
        }
    }

    if (!h->left_anchor)
    {
        uint8_t max_count = (g_card_custom_obj_count < MAX_CUSTOM_CARDS) ?
                            g_card_custom_obj_count : MAX_CUSTOM_CARDS;
        for (uint8_t i = 0U; i < max_count; i++)
        {
            lv_obj_t *card = g_card_custom_objs[i];
            lv_obj_t *p = label;

            if (card == NULL || !lv_obj_is_valid(card))
            {
                continue;
            }

            while (p != NULL)
            {
                if (p == card)
                {
                    h->custom_card_idx = i;
                    p = NULL;
                    break;
                }
                p = lv_obj_get_parent(p);
            }

            if (h->custom_card_idx != 0xFFU)
            {
                break;
            }
        }
    }

    s_value_handle_heads[(uint8_t)id] = idx;
}

void comp_value_handle_register(comp_id_t id, uint8_t pod_index, lv_obj_t *label)
{
    comp_value_handle_register_part(id, pod_index, COMP_VALUE_HANDLE_PART_FULL, label);
    if (id == COMP_HEADING_0806)
    {
        compass_widget_anim_timer_ensure();
        comp_refresh_heading_text_widgets();
    }
}

void comp_value_handle_register_depth_part(comp_id_t id, bool decimal_part, lv_obj_t *label)
{
    comp_value_handle_register_part(id,
                                    0U,
                                    decimal_part ? COMP_VALUE_HANDLE_PART_DEPTH_DEC :
                                                   COMP_VALUE_HANDLE_PART_DEPTH_INT,
                                    label);
}

static bool comp_value_handle_label_valid(comp_value_handle_t *h)
{
    if (h == NULL || h->label == NULL)
    {
        return false;
    }

    if (!lv_obj_is_valid(h->label))
    {
        h->label = NULL;
        return false;
    }

    return true;
}

static bool comp_value_handle_apply_text(comp_value_handle_t *h, const char *text)
{
    if (text == NULL || !comp_value_handle_label_valid(h))
    {
        return false;
    }

    if (!h->left_anchor &&
        (h->custom_card_idx == 0xFFU ||
         !screen_custom_card_refresh_visible(h->custom_card_idx)))
    {
        return false;
    }

    if (strncmp(h->last_text, text, sizeof(h->last_text) - 1U) == 0)
    {
        return true;
    }

    comp_view_label_set_text_if_changed(h->label, text);
    (void)snprintf(h->last_text, sizeof(h->last_text), "%s", text);
    return true;
}

static bool comp_depth_1612_integer_only(void)
{
    return bus_get_units_mode() == UI_UNITS_IMPERIAL;
}

static int comp_depth_display_int(float display_value, bool integer_only)
{
    if (integer_only)
    {
        return (int)((display_value >= 0.0f) ? (display_value + 0.5f) : (display_value - 0.5f));
    }

    return (int)display_value;
}

static int comp_depth_display_decimal(float display_value)
{
    int di = (int)display_value;
    float decimal_part = fabsf(display_value - (float)di);
    int dd = (int)(decimal_part * 10.0f + 0.5f);

    if (dd > 9)
    {
        dd = 9;
    }

    return dd;
}

static void comp_value_format_text(comp_id_t id, float value, char *buf, size_t buf_size)
{
    if (buf == NULL || buf_size == 0U)
    {
        return;
    }

    if (id == COMP_DEPTH_1606)
    {
        (void)snprintf(buf, buf_size, "%.1f", (double)bus_get_depth_display(value));
    }
    else if (id == COMP_TEMP_0806)
    {
        (void)snprintf(buf, buf_size, "%.1f", (double)value);
    }
    else if (id == COMP_PPO2_0806)
    {
        (void)snprintf(buf, buf_size, "%.2f", (double)value);
    }
    else if (id == COMP_BATTERY_0806)
    {
        (void)snprintf(buf, buf_size, "%.0f%%", (double)value);
    }
    else if (id == COMP_TTS_0806 || id == COMP_NDL_STOP_1606)
    {
        (void)snprintf(buf, buf_size, "%d", (int)value);
    }
    else
    {
        (void)snprintf(buf, buf_size, "%.0f", (double)value);
    }
}

bool comp_value_handle_set_text(comp_id_t id, const char *text)
{
    bool touched = false;
    uint16_t idx;

    if (text == NULL)
    {
        return false;
    }

    if ((uint8_t)id >= COMP_VALUE_HANDLE_ID_MAX)
    {
        return false;
    }

    idx = s_value_handle_heads[(uint8_t)id];
    while (idx != UINT16_MAX && idx < s_value_handle_count)
    {
        comp_value_handle_t *h = &s_value_handles[idx];
        if (h->part == COMP_VALUE_HANDLE_PART_FULL)
        {
            touched = comp_value_handle_apply_text(h, text) || touched;
        }
        idx = h->next;
    }

    return touched;
}

bool comp_value_handle_set_value(comp_id_t id, float value)
{
    char buf[32];
    bool touched = false;
    uint16_t idx;

    if ((uint8_t)id >= COMP_VALUE_HANDLE_ID_MAX)
    {
        return false;
    }

    if (id == COMP_DEPTH_1606 || id == COMP_DEPTH_1612)
    {
        float display_value = bus_get_depth_display(value);
        bool integer_only = (id == COMP_DEPTH_1612) && comp_depth_1612_integer_only();
        int di = comp_depth_display_int(display_value, integer_only);
        int dd = comp_depth_display_decimal(display_value);

        idx = s_value_handle_heads[(uint8_t)id];
        while (idx != UINT16_MAX && idx < s_value_handle_count)
        {
            comp_value_handle_t *h = &s_value_handles[idx];

            if (h->part == COMP_VALUE_HANDLE_PART_DEPTH_INT)
            {
                (void)snprintf(buf, sizeof(buf), "%d", di);
                touched = comp_value_handle_apply_text(h, buf) || touched;
            }
            else if (h->part == COMP_VALUE_HANDLE_PART_DEPTH_DEC)
            {
                if (integer_only)
                {
                    if (comp_value_handle_label_valid(h))
                    {
                        lv_obj_set_style_text_opa(h->label, LV_OPA_TRANSP, 0);
                    }
                    touched = comp_value_handle_apply_text(h, DEPTH_1612_DECIMAL_ANCHOR_TEXT) || touched;
                }
                else
                {
                    (void)snprintf(buf, sizeof(buf), ".%d", dd);
                    if (comp_value_handle_label_valid(h))
                    {
                        lv_obj_set_style_text_opa(h->label, LV_OPA_COVER, 0);
                    }
                    touched = comp_value_handle_apply_text(h, buf) || touched;
                }
            }
            else if (h->part == COMP_VALUE_HANDLE_PART_FULL)
            {
                if (integer_only) (void)snprintf(buf, sizeof(buf), "%d", di);
                else (void)snprintf(buf, sizeof(buf), "%.1f", (double)display_value);
                touched = comp_value_handle_apply_text(h, buf) || touched;
            }

            idx = h->next;
        }

        return touched;
    }

    comp_value_format_text(id, value, buf, sizeof(buf));
    return comp_value_handle_set_text(id, buf);
}

bool comp_value_handle_sync_pod(void)
{
    bool touched = false;
    uint16_t idx;

    idx = s_value_handle_heads[(uint8_t)COMP_POD_0806];
    while (idx != UINT16_MAX && idx < s_value_handle_count)
    {
        comp_value_handle_t *h = &s_value_handles[idx];
        ui_vm_value_text_t value_vm;

        if (h->pod_index != 0U)
        {
            ui_vm_value_text_update(&value_vm, COMP_POD_0806, h->pod_index);
            touched = comp_value_handle_apply_text(h, value_vm.text) || touched;
        }

        idx = h->next;
    }

    return touched;
}

/* =========================================================
 * 获取 POD 标签（根据当前渲染计数器返回值）
 * 返回 POD1_TAG POD2_TAG，用于烙印到 user_data
 *
 * 注意：s_pod_render_count 已在 render_widget_by_id 中先递增
 * 所count=1 时为个POD，count=2 时为个POD
 * ========================================================= */
static uintptr_t get_pod_tag(void)
{
    /* 奇数次渲染视为 POD1，偶数次渲染视为 POD2。 */
    /* 次调count=1，奇 POD1_TAG
     * 次调count=2，偶 POD2_TAG */
    return (s_pod_render_count % 2 == 1) ? POD1_TAG : POD2_TAG;
}

/* =========================================================
 * 获取 POD 编号（返1 2
 * ========================================================= */
static uint8_t get_pod_index(void)
{
    /* 与 POD tag 同步，返回逻辑上的 1 号或 2 号气瓶。 */
    /* 次调count=1，奇 POD1
     * 次调count=2，偶 POD2 */
    return (s_pod_render_count % 2 == 1) ? 1 : 2;
}

void reset_pod_render_sequence(void)
{
    /* POD1/POD2 的身份只应在同一个容器内部轮转。
     * 左侧删除 POD 不应改变右侧自定义卡的 POD 身份，反之亦然。 */
    s_pod_render_count = 0;
}

/* =========================================================
 * 渲染计数器归零（每次网格重建/重绘前必须调用）
 * screen_rebuild_layout() left_anchor_create() 调用
 * ========================================================= */
void reset_widget_render_state(void)
{
    /* 布局重建前必须把所有缓存句柄和轮转计数器清空。 */
    tissue_blink_stop();
    memset(s_img_ascent_rate, 0, sizeof(s_img_ascent_rate));
    s_ascent_icon_count = 0;
    memset(s_ndl_handles, 0, sizeof(s_ndl_handles));
    s_ndl_handle_count = 0;
    memset(s_ndl_draw_vm, 0, sizeof(s_ndl_draw_vm));
    memset(s_tissue_handles, 0, sizeof(s_tissue_handles));
    s_tissue_handle_count = 0;
    memset(s_sys_handles, 0, sizeof(s_sys_handles));
    s_sys_handle_count = 0;
    memset(s_compass_handles, 0, sizeof(s_compass_handles));
    s_compass_handle_count = 0;
    s_compass_unavailable_drawn = false;
    s_compass_heading_available = false;
    s_compass_has_valid_heading = false;
    compass_widget_display_clear_large_jump();
    memset(s_depth_unit_labels, 0, sizeof(s_depth_unit_labels));
    s_depth_unit_label_count = 0;
    comp_value_handle_reset();

    reset_pod_render_sequence();
}

/* =========================================================
 * NDL 底部横向 10 宫格进度条绘制回(0 RAM)
 * 数学推演：容器宽abs_w - 16，两边各8px 边距
 * 10个块 + 9px间隙 = 137px（完美填满）
 * ========================================================= */
static void ndl_horiz_bar_draw_cb(lv_event_t * e)
{
    /* 这个回调负责按当前 VM 进度绘制 NDL/停留进度条。 */
    lv_obj_t * obj = lv_event_get_target(e);
    lv_obj_t * parent = lv_obj_get_parent(obj);
    lv_draw_ctx_t * draw_ctx = lv_event_get_draw_ctx(e);
    lv_area_t * area = &obj->coords;
    const ui_vm_ndl_stop_t *vm = (const ui_vm_ndl_stop_t *)lv_event_get_user_data(e);
    lv_color_t parent_bg = parent ? lv_obj_get_style_bg_color(parent, LV_PART_MAIN) : BLACK;
    bool inverted = lv_color_brightness(parent_bg) > 80U;
    lv_color_t active_color = inverted ? BLACK : GREEN;
    lv_color_t inactive_color = inverted ? lv_color_make(0x00, 0x33, 0x00) : DARK;

    int total_w = lv_area_get_width(area);
    int gap = 3;
    int block_w = (total_w - 9 * gap) / 10;
    if (block_w < 1) block_w = 1;

    float pct = 0.0f;
    if (vm != NULL)
    {
        if (vm->stop_type == STOP_NONE)
        {
            if (vm->ndl_bar_pct <= 100U)
            {
                pct = (float)vm->ndl_bar_pct / 100.0f;
            }
            else
            {
                pct = (float)vm->ndl / 99.0f;
            }
        }
        else if (vm->stop_type == STOP_SAFETY)
        {
            if (vm->stop_time_total_s > 0U)
            {
                pct = (float)vm->stop_time_left_s / (float)vm->stop_time_total_s;
            }
            else
            {
                pct = 1.0f;
            }
        }
        else if (vm->stop_type == STOP_DECO)
        {
            if (vm->in_stop_zone == 0U)
            {
                pct = 1.0f;
            }
            else if (vm->stop_time_total_s > 0U)
            {
                pct = (float)vm->stop_time_left_s / (float)vm->stop_time_total_s;
            }
        }
    }
    if (pct > 1.0f) pct = 1.0f;
    if (pct < 0.0f) pct = 0.0f;

    int active_blocks = (int)(pct * 10.0f);
    float remainder = (pct * 10.0f) - active_blocks;

    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.radius = 0; /* 纯直*/

    for (int i = 0; i < 10; i++)
    {
        int x1 = area->x1 + i * (block_w + gap);
        int x2 = x1 + block_w - 1;
        lv_area_t block_area = {x1, area->y1, x2, area->y2};

        if (i < active_blocks)
        {
            /* 全亮格子 */
            rect_dsc.bg_color = active_color;
            rect_dsc.bg_opa = LV_OPA_COVER;
            lv_draw_rect(draw_ctx, &rect_dsc, &block_area);
        }
        else if (i == active_blocks && remainder > 0.05f)
        {
            /* 半亮格子 (先画暗底，再盖亮 */
            rect_dsc.bg_color = inactive_color;
            rect_dsc.bg_opa = LV_OPA_COVER;
            lv_draw_rect(draw_ctx, &rect_dsc, &block_area);

            int partial_w = (int)(block_w * remainder);
            if (partial_w > 0)
            {
                lv_area_t partial_area = {x1, area->y1, x1 + partial_w - 1, area->y2};
                rect_dsc.bg_color = active_color;
                lv_draw_rect(draw_ctx, &rect_dsc, &partial_area);
            }
        }
        else
        {
            /* 未激活的暗格 */
            rect_dsc.bg_color = inactive_color;
            rect_dsc.bg_opa = LV_OPA_COVER;
            lv_draw_rect(draw_ctx, &rect_dsc, &block_area);
        }
    }
}

static void tissue_draw_rect_area(lv_draw_ctx_t *draw_ctx, lv_draw_rect_dsc_t *rect_dsc, lv_coord_t x1, lv_coord_t y1, lv_coord_t x2, lv_coord_t y2, lv_color_t color, lv_opa_t opa)
{
    if (x2 < x1 || y2 < y1)
    {
        return;
    }

    lv_area_t fill = { x1, y1, x2, y2 };
    rect_dsc->bg_color = color;
    rect_dsc->bg_opa = opa;
    lv_draw_rect(draw_ctx, rect_dsc, &fill);
}

static int tissue_chart_draw_permille(int permille)
{
    if (permille < 0)
    {
        return 0;
    }
    if (permille > TISSUE_CHART_MAX_PERMILLE)
    {
        return TISSUE_CHART_MAX_PERMILLE;
    }
    return permille;
}

static lv_coord_t tissue_chart_x_for_permille(const lv_area_t *area, int permille)
{
    int draw_permille = tissue_chart_draw_permille(permille);
    lv_coord_t w = (lv_coord_t)lv_area_get_width(area);
    if (w <= 1) return area->x1;
    return (lv_coord_t)(area->x1 + (lv_coord_t)((draw_permille * (int)(w - 1)) / TISSUE_CHART_MAX_PERMILLE));
}

static lv_coord_t tissue_chart_row_boundary_y(const lv_area_t *area, uint8_t index)
{
    int plot_span = lv_area_get_height(area) - 1;
    return area->y1 + (lv_coord_t)(((int)index * plot_span) / TISSUE_LEAD_COUNT);
}

static bool tissue_chart_fit_equal_rows(lv_area_t *plot)
{
    int plot_span;
    int row_pitch;
    int fit_span;
    int offset_y;

    if (plot == NULL)
    {
        return false;
    }

    plot_span = lv_area_get_height(plot) - 1;
    row_pitch = plot_span / TISSUE_LEAD_COUNT;
    if (row_pitch <= 1)
    {
        return false;
    }

    fit_span = row_pitch * TISSUE_LEAD_COUNT;
    offset_y = (plot_span - fit_span) / 2;
    plot->y1 = (lv_coord_t)(plot->y1 + offset_y);
    plot->y2 = (lv_coord_t)(plot->y1 + fit_span);
    return true;
}

static uint16_t tissue_chart_raw_permille(const ui_vm_deco_t *vm, uint8_t index)
{
    if (vm == NULL || index >= TISSUE_LEAD_COUNT || vm->tissue_normalized_valid == 0U) return 0U;
    return vm->tissue_bar_permille[index];
}

static uint16_t tissue_chart_gf_permille(const ui_vm_deco_t *vm, uint8_t index)
{
    uint16_t raw_permille = tissue_chart_raw_permille(vm, index);
    if (raw_permille <= TISSUE_CHART_PAMB_PERMILLE) return raw_permille;
    return (uint16_t)(TISSUE_CHART_PAMB_PERMILLE + ((uint16_t)vm->tissue_gf_pct[index] * (TISSUE_CHART_LIMIT_PERMILLE - TISSUE_CHART_PAMB_PERMILLE)) / 100U);
}

static uint16_t tissue_chart_permille_for_widget(const ui_vm_deco_t *vm, comp_id_t widget_id, uint8_t index)
{
    if (widget_id == COMP_TISSUE_GF_4012) return tissue_chart_gf_permille(vm, index);
    return tissue_chart_raw_permille(vm, index);
}

static uint32_t tissue_chart_hash_u32(uint32_t hash, uint32_t value)
{
    hash ^= value;
    return hash * 16777619UL;
}

static uint32_t tissue_chart_render_signature(const tissue_handle_t *h,
                                              const ui_vm_deco_t *vm)
{
    uint32_t hash = 2166136261UL;
    uint32_t plot_span = 0U;
    comp_id_t widget_id;

    if (h == NULL || vm == NULL)
    {
        return 0U;
    }

    widget_id = h->widget_id;
    if (h->chart != NULL && lv_obj_is_valid(h->chart))
    {
        lv_coord_t width = lv_obj_get_width(h->chart);
        if (width > 1)
        {
            plot_span = (uint32_t)(width - 1);
        }
    }

    /*
     * 5F/自定义布局可能同时放多个组织仓小图。刷新时只比较图表真正
     * 使用的字段，避免 CNS/OTU/GF 文本或算法中间浮点变化带着所有
     * TISSUE_GF/TISSUE_RAW 组件一起重绘。
     */
    hash = tissue_chart_hash_u32(hash, (uint32_t)widget_id);
    hash = tissue_chart_hash_u32(hash, vm->tissue_normalized_valid);
    if (vm->tissue_normalized_valid == 0U)
    {
        return hash;
    }

    uint32_t pi_permille = vm->tissue_pi_permille;
    if (pi_permille > TISSUE_CHART_MAX_PERMILLE)
    {
        pi_permille = TISSUE_CHART_MAX_PERMILLE;
    }
    hash = tissue_chart_hash_u32(hash,
                                 (plot_span == 0U) ? pi_permille :
                                 (pi_permille * plot_span) / TISSUE_CHART_MAX_PERMILLE);
    for (uint8_t i = 0U; i < TISSUE_LEAD_COUNT; i++)
    {
        uint32_t permille = tissue_chart_permille_for_widget(vm, widget_id, i);
        if (permille > TISSUE_CHART_MAX_PERMILLE)
        {
            permille = TISSUE_CHART_MAX_PERMILLE;
        }
        hash = tissue_chart_hash_u32(hash,
                                     (plot_span == 0U) ? permille :
                                     (permille * plot_span) / TISSUE_CHART_MAX_PERMILLE);
        hash = tissue_chart_hash_u32(hash,
                                     permille >= TISSUE_CHART_LIMIT_PERMILLE);
    }

    return hash;
}

static bool tissue_handle_danger(const tissue_handle_t *h)
{
    if (h == NULL || (h->widget_id != COMP_TISSUE_GF_4012 && h->widget_id != COMP_TISSUE_RAW_4012))
    {
        return false;
    }
    if (h->vm.tissue_normalized_valid == 0U)
    {
        return false;
    }

    for (uint8_t i = 0U; i < TISSUE_LEAD_COUNT; i++)
    {
        if (tissue_chart_permille_for_widget(&h->vm, h->widget_id, i) >= TISSUE_CHART_LIMIT_PERMILLE)
        {
            return true;
        }
    }
    return false;
}

static bool tissue_any_danger(void)
{
    for (uint8_t i = 0U; i < s_tissue_handle_count; i++)
    {
        tissue_handle_t *h = &s_tissue_handles[i];
        if (ui_obj_is_valid(&h->chart) && screen_obj_refresh_visible(h->chart) && tissue_handle_danger(h))
        {
            return true;
        }
    }
    return false;
}

static void tissue_blink_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    s_tissue_blink_phase = !s_tissue_blink_phase;
    for (uint8_t i = 0U; i < s_tissue_handle_count; i++)
    {
        tissue_handle_t *h = &s_tissue_handles[i];
        if (ui_obj_is_valid(&h->chart) &&
            screen_obj_refresh_visible(h->chart) &&
            tissue_handle_danger(h))
        {
            lv_obj_invalidate(h->chart);
        }
    }
}

static void tissue_blink_sync(void)
{
    if (tissue_any_danger())
    {
        if (!s_tissue_blink_timer)
        {
            s_tissue_blink_phase = true;
            s_tissue_blink_timer = lv_timer_create(tissue_blink_timer_cb, TISSUE_LEAD_BLINK_MS, NULL);
        }
        return;
    }

    if (s_tissue_blink_timer)
    {
        tissue_blink_stop();
        for (uint8_t i = 0U; i < s_tissue_handle_count; i++)
        {
            tissue_handle_t *h = &s_tissue_handles[i];
            if (ui_obj_is_valid(&h->chart))
            {
                lv_obj_invalidate(h->chart);
            }
        }
    }
}

static void tissue_chart_draw_bar_segment(lv_draw_ctx_t *draw_ctx, lv_draw_rect_dsc_t *rect_dsc, const lv_area_t *area, lv_coord_t y1, lv_coord_t y2, int low_permille, int high_permille, lv_color_t color, lv_opa_t opa)
{
    int draw_low = tissue_chart_draw_permille(low_permille);
    int draw_high = tissue_chart_draw_permille(high_permille);
    lv_coord_t x1;
    lv_coord_t x2;
    if (draw_high <= draw_low) return;
    x1 = tissue_chart_x_for_permille(area, draw_low);
    x2 = tissue_chart_x_for_permille(area, draw_high);
    tissue_draw_rect_area(draw_ctx, rect_dsc, x1, y1, x2, y2, color, opa);
}

static void tissue_chart_draw_vertical_line(lv_draw_ctx_t *draw_ctx, const lv_area_t *area, int permille, lv_color_t color, lv_opa_t opa, lv_coord_t width, lv_coord_t dash_width, lv_coord_t dash_gap)
{
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = color;
    line_dsc.opa = opa;
    line_dsc.width = width;
    line_dsc.dash_width = dash_width;
    line_dsc.dash_gap = dash_gap;

    lv_coord_t x = tissue_chart_x_for_permille(area, permille);
    lv_point_t pts[2] = { { x, area->y1 }, { x, area->y2 } };
    lv_draw_line(draw_ctx, &line_dsc, &pts[0], &pts[1]);
}

static void tissue_draw_normalized_chart(lv_draw_ctx_t *draw_ctx, const lv_area_t *area, const tissue_handle_t *h)
{
    lv_area_t plot = {area->x1, (lv_coord_t)(area->y1 + TISSUE_CHART_PAD_Y), area->x2, (lv_coord_t)(area->y2 - TISSUE_CHART_PAD_Y)};
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.radius = 0;

    tissue_draw_rect_area(draw_ctx, &rect_dsc, area->x1, area->y1, area->x2, area->y2, TISSUE_LEAD_COLOR_BG, LV_OPA_COVER);
    if (h == NULL || h->vm.tissue_normalized_valid == 0U)
    {
        return;
    }

    if (!tissue_chart_fit_equal_rows(&plot)) return;
    tissue_draw_rect_area(draw_ctx, &rect_dsc, plot.x1, plot.y1, plot.x2, plot.y1, TISSUE_CHART_COLOR_PI, LV_OPA_COVER);
    tissue_chart_draw_vertical_line(draw_ctx, &plot, h->vm.tissue_pi_permille, TISSUE_CHART_COLOR_PI, LV_OPA_COVER, 1, 3, 3);
    for (uint8_t i = 0U; i < TISSUE_LEAD_COUNT; i++)
    {
        lv_coord_t row_y1 = tissue_chart_row_boundary_y(&plot, i);
        lv_coord_t row_y2 = tissue_chart_row_boundary_y(&plot, (uint8_t)(i + 1U));
        lv_coord_t bar_y1 = (lv_coord_t)(row_y1 + 1);
        lv_coord_t bar_y2 = (lv_coord_t)(row_y2 - 1);
        int value_permille = (int)tissue_chart_permille_for_widget(&h->vm, h->widget_id, i);

        if (bar_y2 < bar_y1) bar_y2 = bar_y1;
        tissue_draw_rect_area(draw_ctx, &rect_dsc, plot.x1, row_y2, plot.x2, row_y2, TISSUE_CHART_COLOR_PI, LV_OPA_COVER);
        tissue_chart_draw_bar_segment(draw_ctx, &rect_dsc, &plot, bar_y1, bar_y2, 0, value_permille < TISSUE_CHART_PAMB_PERMILLE ? value_permille : TISSUE_CHART_PAMB_PERMILLE, TISSUE_CHART_COLOR_SAFE, LV_OPA_COVER);
        if (value_permille > TISSUE_CHART_PAMB_PERMILLE) tissue_chart_draw_bar_segment(draw_ctx, &rect_dsc, &plot, bar_y1, bar_y2, TISSUE_CHART_PAMB_PERMILLE, value_permille < TISSUE_CHART_LIMIT_PERMILLE ? value_permille : TISSUE_CHART_LIMIT_PERMILLE, TISSUE_CHART_COLOR_DECO, LV_OPA_COVER);
        if (value_permille > TISSUE_CHART_LIMIT_PERMILLE && s_tissue_blink_phase) tissue_chart_draw_bar_segment(draw_ctx, &rect_dsc, &plot, bar_y1, bar_y2, TISSUE_CHART_LIMIT_PERMILLE, value_permille, TISSUE_CHART_COLOR_DANGER, LV_OPA_COVER);
    }

    tissue_chart_draw_vertical_line(draw_ctx, &plot, TISSUE_CHART_PAMB_PERMILLE, TISSUE_CHART_COLOR_AMB, LV_OPA_COVER, 2, 0, 0);
    tissue_chart_draw_vertical_line(draw_ctx, &plot, TISSUE_CHART_LIMIT_PERMILLE, TISSUE_CHART_COLOR_DANGER, LV_OPA_COVER, 2, 0, 0);
}

static void tissue_chart_draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);
    lv_area_t *area = &obj->coords;
    const tissue_handle_t *h = (const tissue_handle_t *)lv_event_get_user_data(e);
    const ui_vm_deco_t *vm = (h != NULL) ? &h->vm : NULL;

    if (vm == NULL)
    {
        return;
    }

    tissue_draw_normalized_chart(draw_ctx, area, h);
}

/* =========================================================
 * 创建单个自定义组件（组件工厂 左侧网格 + 5F 共用
 *
 * 关键：每个组件的 lv_obj_set_user_data() 存储了标签烙印
 * 对于 POD，使用高位掩码区分（1033=POD1, 2033=POD2）
 * 告警引擎靠这个烙印实左侧锚点 + 5F 组件同时闪烁"
 *
 * 架构铁律
 *   - 位置参数 (abs_x/y/w/h, span_w/h) 由调用方传入
 *   - 样式参数 (font, offsets) comp_get_style(w_id) 自动查表
 *   - cfg_font_id != 255 时强制覆盖自动字
 *   - 速率图标由工厂自主查字典决定（根elements & ELEM_BAR
 *   - 专属组件（DEPTH/NDL）走早期返回，内部仍style 参数
 *   - 通用组件elements 掩码装配流水线：TITLE VALUE UNIT BAR
 *
 * POD 单模具轮转分配：
 *   - 函数入口检w_id == COMP_POD_0806
 *   - 调用 get_pod_tag() 获得高位掩码标签 (1033/2033)
 *   - 调用 get_pod_index() 获得 POD 编号 (1/2)
 *   - 将标签烙印到容器 user_data
 * ========================================================= */
lv_obj_t *render_widget_by_id(lv_obj_t *parent,
                              comp_id_t w_id,
                              int16_t abs_x, int16_t abs_y,
                              uint16_t abs_w, uint16_t abs_h,
                              uint8_t span_w, uint8_t span_h,
                              font_id_t cfg_font_id)
{
    /* 这是组件工厂的统一入口：同一套 ID 可以同时服务左锚点和 5F 自定义网格。 */
    /* 这个函数承担三层职责：
     * 1. 根据 comp_id_t 找到样式字典
     * 2. 创建 LVGL 对象并完成 user_data 烙印
     * 3. 对复杂组件走专属装配，对普通组件走通用流水线
     * 因此它是整个 UI 组件体系的“总装线”。 */
    /* ===== POD 单模具拦截：提前消耗计数器 ===== */
    bool is_pod_mold = (w_id == COMP_POD_0806);
    uint8_t pod_index = 0;        /* POD number 1 or 2 */
    uintptr_t pod_tag = 0;        /* POD tag 1033 or 2033 */
    if (is_pod_mold)
    {
        /* POD 是单模具复用组件，所以先决定当前这次渲染扮演 POD1 还是 POD2。 */
        /* 这套“轮转分配”依赖布局渲染顺序稳定。
         * 所以前面 screen/layout 重建时必须先 reset_widget_render_state()，
         * 否则 POD1/POD2 身份会错位。 */
        s_pod_render_count++;     /* Increment first, then get current value */
        pod_index = get_pod_index();
        pod_tag = get_pod_tag();
    }

    const comp_style_t *style = comp_get_style(w_id);
    if (!style) return NULL;

    /* 字号选择逻辑
     *   cfg_font_id != 255 强制覆盖（运行时指定
     *   DEPTH 系列 自动适配尺寸（HUGE/MEDIUM/SMALL
     *   其他组件 直接使用字典 font_id */
    font_id_t val_font_id;
    if (cfg_font_id != (font_id_t)255)
    {
        val_font_id = cfg_font_id;  /* 强制覆盖（运行时指定*/
    }
    else if (w_id == COMP_DEPTH_1612 || w_id == COMP_DEPTH_1606)
    {
        /* DEPTH 组件：自动适配尺寸 */
        if (span_w >= 2 && span_h >= 2)
        {
            val_font_id = FONT_ID_HUGE;
        }
        else if (span_w >= 2)
        {
            val_font_id = FONT_ID_MEDIUM;
        }
        else
        {
            val_font_id = FONT_ID_SMALL;
        }
    }
    else
    {
        /* 其他组件：直接使用字font_id */
        val_font_id = style->font_id;
    }

    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, abs_x, abs_y);
    lv_obj_set_size(obj, abs_w, abs_h);
    lv_obj_set_style_bg_color(obj, BLACK, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, DARK, 0);
    lv_obj_set_style_border_width(obj, DEBUG_BORDERS ? 1 : 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 2, 0);

    /* 封杀所有滚动条 */
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);

    /* ===== 靶向告警烙印 =====
     * POD uses high-bit mask tags (1033/2033), others use raw w_id */
    if (is_pod_mold)
    {
        lv_obj_set_user_data(obj, (void *)pod_tag);
    }
    else
    {
        lv_obj_set_user_data(obj, (void *)(uintptr_t)w_id);
    }

    if (w_id == COMP_EMPTY) return obj;

    /* ===== DEPTH 2x2 专属渲染（整小数+单位分离===== */
    bool is_2x2 = (span_w >= 2 && span_h >= 2);
    if (w_id == COMP_DEPTH_1612 && is_2x2)
    {
        /* 样式参数来自 comp_style_t */
        /* DEPTH 大组件单独分支的原因是它的排版过于特殊：
         * 整数、小数、单位、速率图标都不是标准“标题+数值”模型能覆盖的。 */
        const style_depth_t *s = &style->spec.depth;
        ui_vm_depth_t depth_vm;
        bool depth_integer_only = comp_depth_1612_integer_only();
        float depth_display = bus_get_depth_display(bus_get_depth());
        int depth_int = comp_depth_display_int(depth_display, depth_integer_only);

        ui_vm_depth_update(&depth_vm, NULL);

        /* ==========================================
         * 1. 超大号整-> 宽度必须紧密包裹
         * ========================================== */
        lv_obj_t *int_lbl = lv_label_create(obj);
        if (SHOW_PLACEHOLDER_ON_INIT) lv_label_set_text(int_lbl, "--");
        else lv_label_set_text_fmt(int_lbl, "%d", depth_integer_only ? depth_int : (int)depth_vm.int_part);
        // 字体从字典读取（font_id = HUGE 58px
        lv_obj_set_style_text_font(int_lbl, get_font(style->font_id), 0);
        lv_obj_set_style_text_color(int_lbl, GREEN, 0);
        lv_label_set_long_mode(int_lbl, LV_LABEL_LONG_CLIP);

        // 绝杀技：必须设CONTENT！这样无论变"6" 还是 "45"
        // Label 的右边缘都会死死包住个位数，绝不留一丝缝隙！
        lv_obj_set_size(int_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

        // 读取字典中的 RIGHT_MID -45，把右边缘焊死在这堵墙上
        lv_obj_align(int_lbl, (lv_align_t)s->int_align, s->int_offset_x, s->int_offset_y);
        comp_value_handle_register_depth_part(w_id, false, int_lbl);

        /* ==========================================
         * 2. 中号小数 -> 紧贴整数的右边界
         * ========================================== */
        lv_obj_t *dec_lbl = lv_label_create(obj);
        if (depth_integer_only) lv_label_set_text(dec_lbl, DEPTH_1612_DECIMAL_ANCHOR_TEXT);
        else if (SHOW_PLACEHOLDER_ON_INIT) lv_label_set_text(dec_lbl, ".-");
        else lv_label_set_text_fmt(dec_lbl, ".%u", (unsigned)depth_vm.dec_part);
        // 字体从字典读取（title_font_id = MEDIUM 28px，小数比整数小）
        lv_obj_set_style_text_font(dec_lbl, get_font(style->title_font_id), 0);
        lv_obj_set_style_text_color(dec_lbl, GREEN, 0);
        lv_obj_set_style_text_opa(dec_lbl, depth_integer_only ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
        lv_label_set_long_mode(dec_lbl, LV_LABEL_LONG_CLIP);
        lv_obj_set_size(dec_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

        // 因为整数的右边缘(个位被焊死了，小数挂在它右边，自然就永远贴紧个位数！
        lv_obj_align_to(dec_lbl, int_lbl, LV_ALIGN_OUT_RIGHT_BOTTOM, s->dec_offset_x, s->dec_offset_y);
        comp_value_handle_register_depth_part(w_id, true, dec_lbl);

        /* ==========================================
         * 3. 小号单位 (m) -> 紧贴小数正下
         * ========================================== */
        if (style->elements & ELEM_UNIT)
        {
            lv_obj_t *unit_lbl = lv_label_create(obj);
            lv_label_set_text(unit_lbl, bus_get_depth_unit_label());
            comp_depth_unit_label_register(unit_lbl);
            // 单位固定用小号字
            lv_obj_set_style_text_font(unit_lbl, get_font(FONT_ID_SMALL), 0);
            lv_obj_set_style_text_color(unit_lbl, LIGHT, 0);
            lv_label_set_long_mode(unit_lbl, LV_LABEL_LONG_CLIP);
            lv_obj_set_size(unit_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align_to(unit_lbl, dec_lbl, LV_ALIGN_OUT_BOTTOM_MID, s->unit_offset_x, s->unit_offset_y);
        }

        /* 速率图标：工厂自主查字典判断是否需要绘*/
        bool needs_bar_icon = (style->elements & ELEM_BAR) != 0;
        if (needs_bar_icon)
        {
            lv_obj_t *sudu_img = lv_img_create(obj);
            lv_img_set_src(sudu_img, &sudo_up_level0);
            lv_obj_align(sudu_img, (lv_align_t)s->icon_align, s->icon_offset_x, s->icon_offset_y);
            if (s_ascent_icon_count < MAX_ASCENT_ICONS)
                s_img_ascent_rate[s_ascent_icon_count++] = sudu_img;
        }
        return obj;
    }
    else if (w_id == COMP_NDL_STOP_1606 || w_id == COMP_NDL_STOP_1612)
    {
        /* NDL 变形金刚：从 style->spec.ndl_stop 读取所有位置参*/
        /* NDL/SAFE/DECO 三种状态共用同一个物理组件容器，
         * 后续通过 comp_refresh_ndl_stop_vm() 动态切换文字、进度条和布局。 */
        if (s_ndl_handle_count >= MAX_NDL_ICONS) return obj;
        ndl_handle_t *h = &s_ndl_handles[s_ndl_handle_count++];
        ui_vm_ndl_stop_t *draw_vm = &s_ndl_draw_vm[s_ndl_handle_count - 1U];
        h->comp = obj;
        h->widget_id = w_id;
        /* 创建 10 宫格的底层透明画板 */
        h->horiz_bg = lv_obj_create(obj);
        lv_obj_remove_style_all(h->horiz_bg);
        const style_ndl_stop_t *s = &style->spec.ndl_stop;
        lv_coord_t bar_x = 0;
        lv_coord_t bar_y = -4;

        if (w_id == COMP_NDL_STOP_1612)
        {
            bar_x = s->horiz_offset_x;
            bar_y = s->horiz_offset_y;
        }

        /* 🚨 宽度填满减去两边留白：abs_w - 16，两边各8px */
        lv_obj_set_size(h->horiz_bg, abs_w - 16, 10);
        /* 贴近底部，2x2 常态和停留态共用同一条方格基线。 */
        lv_obj_align(h->horiz_bg, LV_ALIGN_BOTTOM_MID, bar_x, bar_y);
        memset(draw_vm, 0, sizeof(*draw_vm));
        lv_obj_add_event_cb(h->horiz_bg, ndl_horiz_bar_draw_cb, LV_EVENT_DRAW_MAIN, draw_vm);
        lv_obj_add_flag(h->horiz_bg, LV_OBJ_FLAG_HIDDEN);

        /* 顶部标题（默认隐藏，停留态时显示*/
        h->title_top = lv_label_create(obj);
        lv_obj_set_style_text_font(h->title_top, get_font(s->deco_title_font_id), 0);
        lv_obj_set_style_text_color(h->title_top, GREEN, 0);
        lv_label_set_text(h->title_top, "");
        lv_obj_add_flag(h->title_top, LV_OBJ_FLAG_HIDDEN);

        /* 主数(22, 3:00) - 使用48px字体 */
        h->main_val = lv_label_create(obj);
        lv_obj_set_style_text_color(h->main_val, GREEN, 0);
        lv_obj_set_style_text_font(h->main_val, get_font(FONT_ID_NDL), 0);
        if (SHOW_PLACEHOLDER_ON_INIT)
            lv_label_set_text(h->main_val, "--");
        else
        {
            ui_vm_ndl_stop_t ndl_vm;
            ui_vm_ndl_stop_update(&ndl_vm, NULL);
            lv_label_set_text_fmt(h->main_val, "%d", ndl_vm.ndl_stop_value);
        }

        /* 底部标题 (NDL 45) */
        h->sub_bot = lv_label_create(obj);
        lv_obj_set_style_text_font(h->sub_bot, get_font(FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(h->sub_bot, GREEN, 0);
        lv_obj_add_flag(h->sub_bot, LV_OBJ_FLAG_HIDDEN);
        return obj;
    }
    else if (w_id == COMP_SYS_1606)
    {
        sys_handle_t *sys_handle = NULL;

        /* ===== SYS 模块：电+ 温度横向排列 ===== */
        /* SYS 组件结构固定且刷新频率高，使用实例句柄表避免遍历对象树。
         * 每个左侧/右侧 SYS 都有自己的 label 句柄，互不覆盖。 */
        if (s_sys_handle_count < MAX_SYS_WIDGETS)
        {
            sys_handle = &s_sys_handles[s_sys_handle_count++];
            memset(sys_handle, 0, sizeof(*sys_handle));
        }

        /* 左侧：电Label */
        lv_obj_t *batt_lbl = lv_label_create(obj);
        lv_obj_set_style_text_font(batt_lbl, get_font(FONT_ID_MEDIUM), 0);
        lv_obj_set_style_text_color(batt_lbl, GREEN, 0);
        lv_label_set_long_mode(batt_lbl, LV_LABEL_LONG_CLIP);
        lv_obj_align(batt_lbl, LV_ALIGN_LEFT_MID, 4, 0);
        if (SHOW_PLACEHOLDER_ON_INIT)
            lv_label_set_text(batt_lbl, "--%");
        else
        {
            lv_label_set_text_fmt(batt_lbl, "%u%%", (unsigned)ui_battery_draw_pct(bus_get_battery_pct()));
        }

        /* 右侧：温Label */
        lv_obj_t *temp_lbl = lv_label_create(obj);
        lv_obj_set_style_text_font(temp_lbl, get_font(FONT_ID_MEDIUM), 0);
        lv_obj_set_style_text_color(temp_lbl, GREEN, 0);
        lv_label_set_long_mode(temp_lbl, LV_LABEL_LONG_CLIP);
        lv_obj_align(temp_lbl, LV_ALIGN_RIGHT_MID, -4, 0);
        if (SHOW_PLACEHOLDER_ON_INIT)
            lv_label_set_text_fmt(temp_lbl, "-- %s", bus_get_temperature_unit_label());
        else
        {
            lv_label_set_text_fmt(temp_lbl, "%.1f %s", (double)bus_get_temperature_display(bus_get_temperature()), bus_get_temperature_unit_label());
        }

        if (sys_handle != NULL)
        {
            sys_handle->batt_lbl = batt_lbl;
            sys_handle->temp_lbl = temp_lbl;
        }

        return obj;
    }

    /* ===== 通用流水线：elements 掩码按需装配零件 =====
     * POD1/POD2/WTIME 及所1x1/2x1 通用组件走此路径
     * ELEM_TITLE ELEM_VALUE ELEM_UNIT ELEM_BAR
     *
     * 样式参数全部来自 comp_get_style(w_id) 查表结果
     * title 文本和数值数据源依赖 w_id switch 分发 */

    /* --- 零件 1：标--- */
    if ((style->elements & ELEM_TITLE) && style->title)
    {
        /* 标题层只负责静态语义标签，真正的动态值统一放到 value 层。
         * 这样 title 字体和 value 字体可以完全独立调。 */
        lv_obj_t *title_lbl = lv_label_create(obj);
        /* POD 单模具：根据 pod_index 动态决定标题文*/
        if (is_pod_mold)
        {
            lv_label_set_text_fmt(title_lbl, "POD %d", pod_index);
        }
        else
        {
            lv_label_set_text(title_lbl, style->title);
        }
        lv_obj_set_style_text_font(title_lbl, get_font(style->title_font_id), 0);
        lv_obj_set_style_text_color(title_lbl, LIGHT, 0);
        lv_obj_set_size(title_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(title_lbl, (lv_align_t)style->title_align,
                     comp_title_edge_offset_x((lv_align_t)style->title_align, style->title_offset_x),
                     style->title_offset_y);
        lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_DOT);
    }

    /* --- 零件 2：主数--- */
    lv_obj_t *val_lbl = NULL;
    if (style->elements & ELEM_VALUE)
    {
        /* 通用 value 初始化也统一走 VM 文本接口。
         * 这样无论是深度、POD、温度还是 PPO2，创建时就能拿到一份符合 UI 规范的初始文本。 */
        val_lbl = lv_label_create(obj);
        lv_obj_set_style_text_font(val_lbl, get_font(val_font_id), 0);
        lv_obj_set_style_text_color(val_lbl, GREEN, 0);
        lv_label_set_long_mode(val_lbl, LV_LABEL_LONG_CLIP);

        if (SHOW_PLACEHOLDER_ON_INIT)
        {
            /* 通用占位*/
            lv_label_set_text(val_lbl, "--");
        }
        else
        {
            char buf[48] = "--";
            switch (w_id)
            {
            case COMP_DEPTH_1612:
            case COMP_NDL_STOP_1606:
            case COMP_NDL_STOP_1612:
            case COMP_TEMP_0806:
            case COMP_BATTERY_0806:
            case COMP_STOP_TIME_1606:
            case COMP_POD_0806:
            default:
            {
                if (w_id == COMP_HEADING_0806 || w_id == COMP_COMPASS_1612)
                {
                    snprintf(buf,
                             sizeof(buf),
                             "%03u",
                             (unsigned)comp_compass_display_heading_deg());
                }
                else
                {
                    ui_vm_value_text_t value_vm;
                    ui_vm_value_text_update(&value_vm, w_id, pod_index);
                    snprintf(buf, sizeof(buf), "%s", value_vm.text);
                }
                break;
            }
            /* 历史旧 ID 已移除，展示文本统一走 ui_vm_value_text_update()。 */
            }
            lv_label_set_text(val_lbl, buf);
        }
        /* 所有使ELEM_VALUE widget 都使spec.basic.value_align */
        lv_obj_align(val_lbl, (lv_align_t)style->spec.basic.value_align,
                     comp_value_edge_offset_x((lv_align_t)style->spec.basic.value_align, style->spec.basic.value_offset_x),
                     style->spec.basic.value_offset_y);
        lv_obj_set_user_data(val_lbl, (void *)(uintptr_t)w_id);
        comp_value_handle_register(w_id, is_pod_mold ? pod_index : 0U, val_lbl);
    }

    /* --- 零件 3：单--- */
    if ((style->elements & ELEM_UNIT) && style->unit && (style->unit[0] != '\0'))
    {
        lv_obj_t *unit_lbl = lv_label_create(obj);
        lv_label_set_text(unit_lbl, strcmp(style->unit, "m") == 0 ? bus_get_depth_unit_label() : style->unit);
        if (strcmp(style->unit, "m") == 0)
        {
            comp_depth_unit_label_register(unit_lbl);
        }
        lv_obj_set_style_text_font(unit_lbl, get_font(FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(unit_lbl, LIGHT, 0);
        lv_label_set_long_mode(unit_lbl, LV_LABEL_LONG_CLIP);
        if ((style->elements & ELEM_VALUE) && (val_lbl != NULL))
        {
            /* 单位贴在组件内部右侧，数值再贴到单位左侧，避免单位被右边框裁掉。 */
            lv_obj_align(unit_lbl, (lv_align_t)style->spec.basic.value_align,
                         comp_value_edge_offset_x((lv_align_t)style->spec.basic.value_align, style->spec.basic.value_offset_x),
                         style->spec.basic.value_offset_y);
            lv_obj_align_to(val_lbl, unit_lbl, LV_ALIGN_OUT_LEFT_MID, -2, 0);
        }
        else
        {
            lv_obj_align(unit_lbl, (lv_align_t)style->title_align,
                         style->title_offset_x, style->title_offset_y);
        }
    }

    /* --- 零件 4：特BAR --- */
    if (style->elements & ELEM_BAR)
    {
        if (w_id == COMP_DEPTH_1612)
        {
            const style_depth_t *s = &style->spec.depth;
            lv_obj_t *sudu_img = lv_img_create(obj);
            lv_img_set_src(sudu_img, &sudo_up_level0);
            lv_obj_align(sudu_img, (lv_align_t)s->icon_align, s->icon_offset_x, s->icon_offset_y);
            if (s_ascent_icon_count < MAX_ASCENT_ICONS)
                s_img_ascent_rate[s_ascent_icon_count++] = sudu_img;
        }
        else if (w_id == COMP_ASCENT_0812 || w_id == COMP_ASCENT_1612)
        {
            /* ASCENT_0812 (1x2)：绘制上升速率方向箭头图标（工厂自主查字典决定*/
            lv_obj_t *sudu_img = lv_img_create(obj);
            lv_img_set_src(sudu_img, &sudo_up_level0);
            lv_obj_align(sudu_img,
                         (lv_align_t)style->spec.basic.icon_align,
                         style->spec.basic.icon_offset_x,
                         style->spec.basic.icon_offset_y);
            if (s_ascent_icon_count < MAX_ASCENT_ICONS)
                s_img_ascent_rate[s_ascent_icon_count++] = sudu_img;
        }
        else if (w_id == COMP_COMPASS_1612)
        {
            if (s_compass_handle_count < MAX_COMPASS_WIDGETS)
            {
                compass_handle_t *h = &s_compass_handles[s_compass_handle_count++];
                memset(h, 0, sizeof(*h));

                h->dial = lv_obj_create(obj);
                lv_obj_remove_style_all(h->dial);
                lv_obj_set_size(h->dial, abs_w - 12U, abs_h - 12U);
                lv_obj_align(h->dial, LV_ALIGN_CENTER, 0, 0);
                lv_obj_add_event_cb(h->dial, compass_dial_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
            }
        }
        else if (w_id == COMP_TISSUE_GF_4012 || w_id == COMP_TISSUE_RAW_4012)
        {
            if (s_tissue_handle_count < MAX_TISSUE_WIDGETS)
            {
                tissue_handle_t *h = &s_tissue_handles[s_tissue_handle_count++];
                memset(h, 0, sizeof(*h));
                h->widget_id = w_id;

                h->chart = lv_obj_create(obj);
                lv_obj_remove_style_all(h->chart);
                lv_obj_set_size(h->chart, abs_w - 10U, abs_h - TISSUE_CHART_HEADROOM - TISSUE_CHART_BOTTOM_PAD);
                lv_obj_align(h->chart, LV_ALIGN_BOTTOM_MID, 0, -TISSUE_CHART_BOTTOM_PAD);
                lv_obj_add_event_cb(h->chart, tissue_chart_draw_cb, LV_EVENT_DRAW_MAIN, h);
                lv_obj_add_flag(h->chart, LV_OBJ_FLAG_HIDDEN);

                h->placeholder = lv_label_create(obj);
                lv_obj_set_style_text_font(h->placeholder, get_font(FONT_ID_MEDIUM), 0);
                lv_obj_set_style_text_color(h->placeholder, GREEN, 0);
                lv_label_set_text(h->placeholder, "--");
                lv_obj_align(h->placeholder, LV_ALIGN_CENTER, 0, 6);
            }
        }
        else if (w_id == COMP_SYS_1606)
        {
            /* SYS 电池+ 外设图标（系统状态栏*/
            lv_obj_t *bat_bg = lv_obj_create(obj);
            lv_obj_remove_style_all(bat_bg);
            lv_obj_set_size(bat_bg, 60, 14);
            lv_obj_align(bat_bg, LV_ALIGN_BOTTOM_LEFT, 4, -4);
            lv_obj_set_style_border_width(bat_bg, 1, 0);
            lv_obj_set_style_border_color(bat_bg, GREEN, 0);
            lv_obj_set_style_radius(bat_bg, 2, 0);

            uint8_t pct = ui_battery_draw_pct(bus_get_battery_pct());
            bool battery_low = pct <= 40U;
            lv_obj_t *bat_fill = lv_obj_create(bat_bg);
            lv_obj_remove_style_all(bat_fill);
            lv_obj_set_size(bat_fill, LV_PCT(battery_low ? pct : 100U), LV_PCT(100));
            lv_obj_align(bat_fill, LV_ALIGN_LEFT_MID, 0, 0);
            lv_obj_set_style_bg_color(bat_fill, battery_low ? LIGHT : GREEN, 0);
            lv_obj_set_style_bg_opa(bat_fill, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(bat_fill, 1, 0);
            (void)bat_fill;
        }
    }

    return obj;
}

void comp_refresh_compass_widgets(void)
{
    bool had_valid_heading;

    compass_widget_anim_timer_ensure();
    had_valid_heading = s_compass_has_valid_heading;
    s_compass_heading_available = bus_get_heading_available();
    if (!s_compass_heading_available)
    {
        if (!s_compass_display_seeded)
        {
            compass_widget_display_seed(bus_get_heading());
        }
        if (!s_compass_unavailable_drawn)
        {
            compass_widget_display_clear_pending_target();
            compass_widget_display_clear_large_jump();
            s_compass_display_target_valid = false;
            s_compass_display_velocity_dps = 0.0f;
            s_compass_display_last_step_deg = 0.0f;
            comp_refresh_heading_text_widgets();
            compass_widget_refresh_dials();
            s_compass_unavailable_drawn = true;
        }
        return;
    }

    if (!had_valid_heading)
    {
        s_compass_has_valid_heading = true;
        s_compass_unavailable_drawn = false;
        compass_widget_display_seed(bus_get_heading());
        comp_refresh_heading_text_widgets();
        compass_widget_refresh_dials();
        return;
    }

    s_compass_unavailable_drawn = false;
    if (!s_compass_display_seeded)
    {
        compass_widget_display_seed(bus_get_heading());
    }
    else
    {
        compass_widget_display_note_target(bus_get_heading());
    }
}

void comp_refresh_tissue_widgets(const ui_vm_deco_t *vm, dirty_mask_t dirty_mask)
{
    if (vm == NULL)
    {
        return;
    }
    if (s_tissue_handle_count == 0U)
    {
        return;
    }
    if ((dirty_mask & DIRTY_TISSUE_TOX) == 0U)
    {
        return;
    }

    for (uint8_t i = 0U; i < s_tissue_handle_count; i++)
    {
        tissue_handle_t *h = &s_tissue_handles[i];
        uint32_t render_sig;
        bool chart_already_shown;
        bool chart_changed;

        if (!ui_obj_is_valid(&h->chart) ||
                !ui_obj_is_valid(&h->placeholder))
        {
            memset(h, 0, sizeof(*h));
            continue;
        }
        if (!screen_obj_refresh_visible(h->chart))
        {
            continue;
        }

        render_sig = tissue_chart_render_signature(h, vm);
        chart_already_shown = lv_obj_has_flag(h->placeholder, LV_OBJ_FLAG_HIDDEN) &&
                              !lv_obj_has_flag(h->chart, LV_OBJ_FLAG_HIDDEN);
        chart_changed = !h->render_sig_valid || (h->render_sig != render_sig);
        if (!chart_changed && chart_already_shown)
        {
            memcpy(&h->vm, vm, sizeof(h->vm));
            continue;
        }

        memcpy(&h->vm, vm, sizeof(h->vm));
        lv_obj_add_flag(h->placeholder, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(h->chart, LV_OBJ_FLAG_HIDDEN);
        h->render_sig = render_sig;
        h->render_sig_valid = true;
        if (chart_changed || !chart_already_shown)
        {
            lv_obj_invalidate(h->chart);
        }
    }

    tissue_blink_sync();
}

void comp_refresh_ndl_stop_vm(const ui_vm_ndl_stop_t *vm, dirty_mask_t dirty_mask)
{
    if (vm == NULL)
    {
        return;
    }
    if (s_ndl_handle_count == 0)
    {
        return;
    }
    if ((dirty_mask & (DIRTY_DECO_STATUS | DIRTY_DIVE_PROFILE)) == 0U)
    {
        return;
    }
    /* NDL 组件不是“文本刷新”那么简单：
     * 它既要更新主值，又要切换 STOP_NONE / SAFETY / DECO 三种版式，
     * 还要重绘底部 10 格进度条，所以必须走专用刷新器。 */

    for (int i = 0; i < s_ndl_handle_count; i++)
    {
        ndl_handle_t *h = &s_ndl_handles[i];
        ui_vm_ndl_stop_t *draw_vm = &s_ndl_draw_vm[i];
        const comp_style_t *style;
        const style_ndl_stop_t *s;
        bool vm_changed;
        bool bar_visibility_changed;
        bool layout_changed;
        bool is_2x2;

        if (!ui_obj_is_valid(&h->comp) ||
            !ui_obj_is_valid(&h->horiz_bg) ||
            !ui_obj_is_valid(&h->main_val) ||
            !ui_obj_is_valid(&h->title_top) ||
            !ui_obj_is_valid(&h->sub_bot))
        {
            memset(h, 0, sizeof(*h));
            memset(draw_vm, 0, sizeof(*draw_vm));
            continue;
        }
        if (!screen_obj_refresh_visible(h->comp))
        {
            continue;
        }

        style = comp_get_style(h->widget_id);
        if (style == NULL)
        {
            style = comp_get_style(COMP_NDL_STOP_1606);
        }
        if (style == NULL)
        {
            continue;
        }
        s = &style->spec.ndl_stop;
        is_2x2 = (h->widget_id == COMP_NDL_STOP_1612);

        vm_changed = (memcmp(draw_vm, vm, sizeof(*draw_vm)) != 0);
        bar_visibility_changed = comp_view_obj_set_hidden_if_changed(h->horiz_bg, false);
        layout_changed = (!h->layout_valid || h->last_stop_type != (int8_t)vm->stop_type);

        if (!vm_changed && !bar_visibility_changed && !layout_changed)
        {
            continue;
        }

        if (vm_changed)
        {
            memcpy(draw_vm, vm, sizeof(*draw_vm));
        }
        if (vm_changed || bar_visibility_changed)
        {
            lv_obj_invalidate(h->horiz_bg);
        }

        if (vm->stop_type == STOP_NONE)
        {
            /* 普通 NDL 态：主值显示剩余免减压时间，底部显示 NDL 标签。 */
            comp_view_obj_set_hidden_if_changed(h->title_top, true);
            comp_view_obj_set_hidden_if_changed(h->sub_bot, false);

            comp_view_label_set_text_if_changed(h->sub_bot, "NDL");
            if (layout_changed)
            {
                lv_obj_set_style_text_font(h->main_val, get_font(style->font_id), 0);
                lv_obj_set_style_text_font(h->sub_bot, get_font(s->norm_sub_font_id), 0);
                if (is_2x2)
                {
                    lv_obj_align(h->sub_bot, (lv_align_t)s->norm_sub_align, s->norm_sub_x, s->norm_sub_y);
                    lv_obj_align(h->main_val, (lv_align_t)s->norm_main_align, s->norm_main_x, s->norm_main_y);
                }
                else
                {
                    lv_obj_align(h->sub_bot, LV_ALIGN_LEFT_MID, 8, -6);
                    lv_obj_align(h->main_val, LV_ALIGN_CENTER, 0, -8);
                }
            }

            comp_view_label_set_text_fmt_if_changed(h->main_val, "%d", vm->ndl);
        }
        else if (vm->stop_type == STOP_SAFETY)
        {
            /* 安全停留态：顶部显示 SAFE 深度，主值改成倒计时，底部显示 IN STOP 或 NDL 参考。 */
            comp_view_obj_set_hidden_if_changed(h->title_top, false);
            comp_view_obj_set_hidden_if_changed(h->sub_bot, false);

            comp_view_label_set_text_fmt_if_changed(h->title_top,
                                                    "SAFE %d%s",
                                                    (int)bus_get_depth_display(vm->stop_depth_m),
                                                    bus_get_depth_unit_label());
            if (layout_changed)
            {
                lv_obj_set_style_text_font(h->title_top, get_font(s->deco_title_font_id), 0);
                if (is_2x2) lv_obj_align(h->title_top, (lv_align_t)s->deco_title_align, s->deco_title_x, s->deco_title_y);
                else lv_obj_align(h->title_top, LV_ALIGN_TOP_LEFT, comp_title_edge_offset_x(LV_ALIGN_TOP_LEFT, 8), 2);
            }

            if (vm->in_stop_zone != 0U)
            {
                comp_view_label_set_text_if_changed(h->sub_bot, "IN STOP");
            }
            else
            {
                comp_view_label_set_text_fmt_if_changed(h->sub_bot, "NDL %d", vm->ndl);
            }
            if (layout_changed)
            {
                lv_obj_set_style_text_font(h->sub_bot, get_font(s->deco_sub_font_id), 0);
                if (is_2x2) lv_obj_align(h->sub_bot, (lv_align_t)s->deco_sub_align, s->deco_sub_x, s->deco_sub_y);
                else lv_obj_align(h->sub_bot, LV_ALIGN_BOTTOM_LEFT, 8, -16);
            }

            if (layout_changed)
            {
                lv_obj_set_style_text_font(h->main_val, get_font(s->deco_main_font_id), 0);
            }
            comp_ndl_stop_set_time_text(h->main_val, vm->stop_time_left_s);
            if (layout_changed)
            {
                if (is_2x2) lv_obj_align(h->main_val, (lv_align_t)s->deco_main_align, s->deco_main_x, s->deco_main_y);
                else lv_obj_align(h->main_val, LV_ALIGN_RIGHT_MID, -4, -6);
            }
        }
        else if (vm->stop_type == STOP_DECO)
        {
            /* 减压停留态：逻辑上比 SAFETY 更强制，所以只保留 DECO 深度和剩余时间。 */
            comp_view_obj_set_hidden_if_changed(h->title_top, false);
            comp_view_obj_set_hidden_if_changed(h->sub_bot, true);

            comp_view_label_set_text_fmt_if_changed(h->title_top,
                                                    "DECO %d%s",
                                                    (int)bus_get_depth_display(vm->stop_depth_m),
                                                    bus_get_depth_unit_label());
            if (layout_changed)
            {
                lv_obj_set_style_text_font(h->title_top, get_font(s->deco_title_font_id), 0);
                lv_obj_set_size(h->title_top, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                lv_obj_align(h->title_top, (lv_align_t)s->deco_title_align, s->deco_title_x, s->deco_title_y);
            }

            lv_obj_set_style_text_font(h->main_val, get_font(s->deco_main_font_id), 0);
            comp_ndl_stop_set_time_text(h->main_val, vm->stop_time_left_s);
            lv_obj_set_size(h->main_val, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align(h->main_val, (lv_align_t)s->deco_main_align, s->deco_main_x, s->deco_main_y);
        }

        if (layout_changed)
        {
            h->last_stop_type = (int8_t)vm->stop_type;
            h->layout_valid = true;
        }
    }
}

void comp_refresh_ndl_stop(dirty_mask_t dirty_mask)
{
    ui_vm_ndl_stop_t vm;
    ui_vm_ndl_stop_update(&vm, NULL);
    comp_refresh_ndl_stop_vm(&vm, dirty_mask);
}

void comp_refresh_sys(dirty_mask_t dirty_mask)
{
    if ((dirty_mask & DIRTY_SYSTEM) == 0U)
    {
        return;
    }

    for (uint8_t i = 0U; i < s_sys_handle_count; i++)
    {
        sys_handle_t *h = &s_sys_handles[i];

        if (!ui_obj_is_valid(&h->batt_lbl) ||
            !ui_obj_is_valid(&h->temp_lbl))
        {
            memset(h, 0, sizeof(*h));
            continue;
        }

        if (screen_obj_refresh_visible(h->batt_lbl))
        {
            comp_view_label_set_text_fmt_if_changed(h->batt_lbl, "%u%%", (unsigned)ui_battery_draw_pct(bus_get_battery_pct()));
        }

        if (screen_obj_refresh_visible(h->temp_lbl))
        {
            comp_view_label_set_text_fmt_if_changed(h->temp_lbl, "%.1f %s", (double)bus_get_temperature_display(bus_get_temperature()), bus_get_temperature_unit_label());
        }
    }
}

void comp_refresh_ascent_icons(const ui_vm_ascent_t *vm)
{
    static int8_t s_last_direction = 0;  /* 0=still, 1=up, -1=down */

    if ((vm == NULL) || (s_ascent_icon_count == 0))
    {
        return;
    }

    bool is_moving = (vm->is_moving != 0U);
    bool current_flash_state = (vm->flash_on != 0U);
    int8_t current_direction = vm->direction;

    const void *target_img_src = &sudo_up_level0;
    /* 速率图标的决策维度有三个：
     * 1. 是否在运动
     * 2. 方向是上升还是下降
     * 3. 当前处于 level0~6 哪一档，并结合 flash_on 做闪烁 */

    if (!is_moving)
    {
        /* 静止时仍保留上一次方向的 level0 图标，避免箭头方向来回丢失。 */
        target_img_src = (s_last_direction > 0) ? &sudo_up_level0 :
                         (s_last_direction < 0) ? &sudo_down_level0 : &sudo_up_level0;
    }
    else if (current_direction > 0)
    {
#if ASCENT_ICON_USE_FINE_LEVELS
        if (vm->rate >= RATE_UP_LEVEL6_THRESHOLD) target_img_src = current_flash_state ? &sudo_up_level6 : &sudo_up_level0;
        else if (vm->rate >= RATE_UP_LEVEL5_THRESHOLD) target_img_src = current_flash_state ? &sudo_up_level5 : &sudo_up_level0;
        else if (vm->rate >= RATE_UP_LEVEL4_THRESHOLD) target_img_src = current_flash_state ? &sudo_up_level4 : &sudo_up_level0;
        else if (vm->rate >= RATE_UP_LEVEL3_THRESHOLD) target_img_src = current_flash_state ? &sudo_up_level3 : &sudo_up_level0;
        else if (vm->rate >= RATE_UP_LEVEL2_THRESHOLD) target_img_src = current_flash_state ? &sudo_up_level2 : &sudo_up_level0;
        else if (vm->rate > RATE_STILL_THRESHOLD) target_img_src = current_flash_state ? &sudo_up_level1 : &sudo_up_level0;
        else target_img_src = &sudo_up_level0;
#else
        if (vm->rate >= RATE_LEGACY_LEVEL2_THRESHOLD) target_img_src = current_flash_state ? &sudo_up_level6 : &sudo_up_level0;
        else if (vm->rate >= RATE_LEGACY_LEVEL1_THRESHOLD) target_img_src = current_flash_state ? &sudo_up_level3 : &sudo_up_level0;
        else target_img_src = &sudo_up_level0;
#endif
    }
    else
    {
        float down_rate_mpm = -vm->rate;
#if ASCENT_ICON_USE_FINE_LEVELS
        if (down_rate_mpm >= RATE_DOWN_LEVEL6_THRESHOLD) target_img_src = current_flash_state ? &sudo_down_level6 : &sudo_down_level0;
        else if (down_rate_mpm >= RATE_DOWN_LEVEL5_THRESHOLD) target_img_src = current_flash_state ? &sudo_down_level5 : &sudo_down_level0;
        else if (down_rate_mpm >= RATE_DOWN_LEVEL4_THRESHOLD) target_img_src = current_flash_state ? &sudo_down_level4 : &sudo_down_level0;
        else if (down_rate_mpm >= RATE_DOWN_LEVEL3_THRESHOLD) target_img_src = current_flash_state ? &sudo_down_level3 : &sudo_down_level0;
        else if (down_rate_mpm >= RATE_DOWN_LEVEL2_THRESHOLD) target_img_src = current_flash_state ? &sudo_down_level2 : &sudo_down_level0;
        else if (down_rate_mpm > RATE_STILL_THRESHOLD) target_img_src = current_flash_state ? &sudo_down_level1 : &sudo_down_level0;
        else target_img_src = &sudo_down_level0;
#else
        if (down_rate_mpm >= RATE_LEGACY_LEVEL2_THRESHOLD) target_img_src = current_flash_state ? &sudo_down_level6 : &sudo_down_level0;
        else if (down_rate_mpm >= RATE_LEGACY_LEVEL1_THRESHOLD) target_img_src = current_flash_state ? &sudo_down_level3 : &sudo_down_level0;
        else target_img_src = &sudo_down_level0;
#endif
    }

    if (current_direction != 0)
    {
        s_last_direction = current_direction;
    }

    for (int i = 0; i < s_ascent_icon_count; i++)
    {
        /* 一次速率变化可能要同步多个 DEPTH/ASCENT 组件实例，所以这里广播到全部缓存图标。 */
        if (ui_obj_is_valid(&s_img_ascent_rate[i]) &&
            screen_obj_refresh_visible(s_img_ascent_rate[i]) &&
            lv_img_get_src(s_img_ascent_rate[i]) != target_img_src)
        {
            lv_img_set_src(s_img_ascent_rate[i], target_img_src);
        }
    }
}
