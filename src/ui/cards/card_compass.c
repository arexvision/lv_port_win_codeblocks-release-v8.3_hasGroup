/*
 * 文件: src/app_ui/ui/cards/card_compass.c
 * 作用: 该文件属于仪表卡片模块，负责某一类卡片页面的创建、布局、刷新或与页面注册表之间的装配。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时应重点检查布局尺寸、刷新频率与复用接口是否一致，避免局部样式或坐标调整影响同类页面的对齐和更新节奏。
 */

#include "../screen/screen.h"
#include "../core/data.h"
#include "../core/ui_engine.h"
#include "../core/ui_state.h"
#include "../core/vm/ui_vm_dashboard.h"
#include "../screen/layout_view.h"
#include "../../config/build/ui_debug_flags.h"
#include "card_compass.h"
#include "lvgl/lvgl.h"
#include "../fonts/fonts.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

extern void rt_kprintf(const char *fmt, ...);

#if UI_COMPASS_DIAG_TRACE_ENABLED
void ble_sensor_debug_note_ui_force_refresh(uint16_t heading);
void ble_sensor_debug_note_ui_dirty(uint16_t heading);
#endif

/* ============================================================
 * 1F: NAV COMPASS — 零内存数学绘制引擎
 *
 * 采用 LV_EVENT_DRAW_MAIN 回调实现纯数学渲染，内存占用为 0。
 * 完美复刻 HTML 原型中顺滑的"战术卷尺（Tactical Tape）"效果。
 *
 * 规范参数：
 *   卷尺高度：60px
 *   像素/度比例：3.0（刻度更密集紧凑）
 *   每 3 度一根密集短线，每 15 度一根中线，每 45 度显示纯方位字母
 *   上下极简边框（OPA_30），中心瞄准基线 3px 绿色
 * ============================================================ */

/* ============================================================
 * 罗盘卷尺绘制参数
 * ============================================================ */
#define COMPASS_TAPE_H      60      /* 卷尺高度 60px */
#define PX_PER_DEGREE       3.0f   /* 像素/度比例 */
#define TAPE_TOP_OFFSET     10      /* 卷尺距标题区顶部的偏移 */
#define COMPASS_UI_ANIM_TICK_MS          16U
#define COMPASS_UI_ANIM_DIRECT_DIFF_DEG  1.5f
#define COMPASS_UI_ANIM_SLOW_DIFF_DEG    8.0f
#define COMPASS_UI_ANIM_MID_DIFF_DEG     24.0f
#define COMPASS_UI_ANIM_SLOW_STEP_DEG    3.0f
#define COMPASS_UI_ANIM_MID_STEP_DEG     6.0f
#define COMPASS_UI_ANIM_FAST_STEP_DEG    9.0f
#define COMPASS_UI_ANIM_MIN_STEP_DEG     0.20f
#define COMPASS_UI_ANIM_SNAP_EPS_DEG     0.08f
#define COMPASS_UI_ANIM_REVERSAL_EPS_DEG 0.35f
#define COMPASS_UI_PREDICT_SAMPLE_MIN_MS 12U
#define COMPASS_UI_PREDICT_SAMPLE_MAX_MS 320U
#define COMPASS_UI_PREDICT_HOLD_MS       120U
#define COMPASS_UI_PREDICT_MAX_AHEAD_DEG 2.5f
#define COMPASS_UI_PREDICT_SPEED_ALPHA   0.38f
#define COMPASS_UI_PREDICT_MAX_DPS       260.0f
#define COMPASS_UI_PREDICT_SAME_DECAY    0.55f
#define COMPASS_UI_PREDICT_REV_GAIN      0.25f
#define COMPASS_UI_TARGET_BACKSTEP_DEG   8.0f
#define COMPASS_UI_TARGET_RELEASE_DEG    7.0f
#define COMPASS_UI_TARGET_RELEASE_MS     96U
#define COMPASS_UI_TARGET_TREND_MIN_DPS  70.0f

static ui_vm_compass_t s_compass_vm_cache;
static lv_timer_t *s_compass_anim_timer = NULL;
static float s_compass_display_heading_deg = 0.0f;
static float s_compass_display_last_step_deg = 0.0f;
static float s_compass_display_target_deg = 0.0f;
static float s_compass_display_velocity_dps = 0.0f;
static float s_compass_display_pending_target_deg = 0.0f;
static float s_compass_display_pending_delta_deg = 0.0f;
static uint32_t s_compass_display_target_tick = 0U;
static uint32_t s_compass_display_pending_target_tick = 0U;
static uint8_t s_compass_display_pending_target_count = 0U;
static bool s_compass_display_target_valid = false;
static bool s_compass_display_seeded = false;
static bool s_compass_display_pending_target_valid = false;
static uint16_t s_compass_display_heading_text = 0xFFFFU;

static float compass_normalize_heading_float(float deg)
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

static float compass_shortest_delta_float(float from_deg, float to_deg)
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

static int compass_round_float_to_int(float value)
{
    return (value >= 0.0f) ? (int)(value + 0.5f) : (int)(value - 0.5f);
}

static uint16_t compass_heading_from_float(float heading)
{
    int value = compass_round_float_to_int(compass_normalize_heading_float(heading));

    value %= 360;
    if (value < 0)
    {
        value += 360;
    }
    return (uint16_t)value;
}

static float compass_display_heading_snapshot(void);

uint16_t card_compass_display_heading_deg(void)
{
    return compass_heading_from_float(compass_display_heading_snapshot());
}

static float compass_display_heading_snapshot(void)
{
    if (!s_compass_display_seeded)
    {
        return (float)(s_compass_vm_cache.heading % 360U);
    }

    return s_compass_display_heading_deg;
}

static void compass_display_seed(uint16_t heading)
{
    s_compass_display_heading_deg = (float)(heading % 360U);
    s_compass_display_last_step_deg = 0.0f;
    s_compass_display_target_deg = s_compass_display_heading_deg;
    s_compass_display_velocity_dps = 0.0f;
    s_compass_display_pending_target_deg = 0.0f;
    s_compass_display_pending_delta_deg = 0.0f;
    s_compass_display_target_tick = lv_tick_get();
    s_compass_display_pending_target_tick = 0U;
    s_compass_display_pending_target_count = 0U;
    s_compass_display_target_valid = true;
    s_compass_display_seeded = true;
    s_compass_display_pending_target_valid = false;
    s_compass_display_heading_text = 0xFFFFU;
}

static void compass_display_clear_pending_target(void)
{
    s_compass_display_pending_target_valid = false;
    s_compass_display_pending_target_count = 0U;
    s_compass_display_pending_target_tick = 0U;
    s_compass_display_pending_target_deg = 0.0f;
    s_compass_display_pending_delta_deg = 0.0f;
}

static void compass_display_decay_velocity(float factor)
{
    s_compass_display_velocity_dps *= factor;
    if (fabsf(s_compass_display_velocity_dps) < 2.0f)
    {
        s_compass_display_velocity_dps = 0.0f;
    }
}

/*
 * 只处理显示层的瞬时反向毛刺：融合层偶发把目标角回退一帧时，先短暂
 * 暂存，不立刻让屏幕倒退；如果下一帧仍继续反向，则认为用户真实换向。
 */
static bool compass_display_hold_transient_backstep(float new_target,
                                                    float delta,
                                                    uint32_t now_tick)
{
    bool has_trend = fabsf(s_compass_display_velocity_dps) >= COMPASS_UI_TARGET_TREND_MIN_DPS;
    bool opposite_to_trend =
        (s_compass_display_velocity_dps > 0.0f && delta < 0.0f) ||
        (s_compass_display_velocity_dps < 0.0f && delta > 0.0f);
    bool large_backstep = fabsf(delta) >= COMPASS_UI_TARGET_BACKSTEP_DEG;
    float pending_move;
    bool same_backstep;
    bool continues_backstep;
    bool close_to_target;
    uint32_t age_ms;

    if (!has_trend || !opposite_to_trend || !large_backstep)
    {
        compass_display_clear_pending_target();
        return false;
    }

    if (!s_compass_display_pending_target_valid ||
        fabsf(compass_shortest_delta_float(s_compass_display_pending_target_deg, new_target)) >
            COMPASS_UI_TARGET_RELEASE_DEG)
    {
        s_compass_display_pending_target_deg = new_target;
        s_compass_display_pending_delta_deg = delta;
        s_compass_display_pending_target_tick = now_tick;
        s_compass_display_pending_target_count = 1U;
        s_compass_display_pending_target_valid = true;
        compass_display_decay_velocity(0.35f);
        s_compass_display_target_tick = now_tick;
        return true;
    }

    pending_move = compass_shortest_delta_float(s_compass_display_pending_target_deg, new_target);
    same_backstep = fabsf(pending_move) <= 0.5f;
    continues_backstep =
        (s_compass_display_pending_delta_deg > 0.0f && pending_move > 0.5f) ||
        (s_compass_display_pending_delta_deg < 0.0f && pending_move < -0.5f);
    if ((same_backstep || continues_backstep) && s_compass_display_pending_target_count < 3U)
    {
        s_compass_display_pending_target_count++;
    }

    s_compass_display_pending_target_deg = new_target;
    close_to_target = fabsf(compass_shortest_delta_float(s_compass_display_target_deg, new_target)) <=
                      COMPASS_UI_TARGET_RELEASE_DEG;
    age_ms = now_tick - s_compass_display_pending_target_tick;
    if (close_to_target ||
        ((same_backstep || continues_backstep) && s_compass_display_pending_target_count >= 2U) ||
        age_ms >= COMPASS_UI_TARGET_RELEASE_MS)
    {
        compass_display_clear_pending_target();
        return false;
    }

    compass_display_decay_velocity(0.35f);
    s_compass_display_target_tick = now_tick;
    return true;
}

static void compass_display_note_target(uint16_t heading)
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
    float delta = compass_shortest_delta_float(s_compass_display_target_deg, new_target);
    if (fabsf(delta) <= 0.01f)
    {
        compass_display_clear_pending_target();
        compass_display_decay_velocity(COMPASS_UI_PREDICT_SAME_DECAY);
        s_compass_display_target_tick = now_tick;
        return;
    }

    if (compass_display_hold_transient_backstep(new_target, delta, now_tick))
    {
        return;
    }

    if (dt_ms >= COMPASS_UI_PREDICT_SAMPLE_MIN_MS &&
        dt_ms <= COMPASS_UI_PREDICT_SAMPLE_MAX_MS)
    {
        float sample_dps = delta * 1000.0f / (float)dt_ms;
        if (sample_dps > COMPASS_UI_PREDICT_MAX_DPS)
        {
            sample_dps = COMPASS_UI_PREDICT_MAX_DPS;
        }
        else if (sample_dps < -COMPASS_UI_PREDICT_MAX_DPS)
        {
            sample_dps = -COMPASS_UI_PREDICT_MAX_DPS;
        }

        if ((s_compass_display_velocity_dps > 0.0f && sample_dps < 0.0f) ||
            (s_compass_display_velocity_dps < 0.0f && sample_dps > 0.0f))
        {
            s_compass_display_velocity_dps = sample_dps * COMPASS_UI_PREDICT_REV_GAIN;
        }
        else
        {
            s_compass_display_velocity_dps =
                (s_compass_display_velocity_dps * (1.0f - COMPASS_UI_PREDICT_SPEED_ALPHA)) +
                (sample_dps * COMPASS_UI_PREDICT_SPEED_ALPHA);
        }
    }
    else if (dt_ms > COMPASS_UI_PREDICT_SAMPLE_MAX_MS)
    {
        s_compass_display_velocity_dps = 0.0f;
    }

    s_compass_display_target_deg = new_target;
    s_compass_display_target_tick = now_tick;
    compass_display_clear_pending_target();
}

static float compass_display_predict_target(void)
{
    uint32_t now_tick = lv_tick_get();
    uint32_t dt_ms;
    float ahead;

    if (!s_compass_display_target_valid)
    {
        return (float)(s_compass_vm_cache.heading % 360U);
    }

    dt_ms = now_tick - s_compass_display_target_tick;
    if (dt_ms > COMPASS_UI_PREDICT_HOLD_MS)
    {
        return s_compass_display_target_deg;
    }

    ahead = s_compass_display_velocity_dps * (float)dt_ms / 1000.0f;
    if (ahead > COMPASS_UI_PREDICT_MAX_AHEAD_DEG)
    {
        ahead = COMPASS_UI_PREDICT_MAX_AHEAD_DEG;
    }
    else if (ahead < -COMPASS_UI_PREDICT_MAX_AHEAD_DEG)
    {
        ahead = -COMPASS_UI_PREDICT_MAX_AHEAD_DEG;
    }

    return compass_normalize_heading_float(s_compass_display_target_deg + ahead);
}

static float compass_display_adaptive_step(float diff, float abs_diff)
{
    float cap;
    float alpha;
    float step;

    if (abs_diff <= COMPASS_UI_ANIM_DIRECT_DIFF_DEG)
    {
        return diff;
    }

    if (abs_diff < COMPASS_UI_ANIM_SLOW_DIFF_DEG)
    {
        cap = COMPASS_UI_ANIM_SLOW_STEP_DEG;
        alpha = 0.72f;
    }
    else if (abs_diff < COMPASS_UI_ANIM_MID_DIFF_DEG)
    {
        cap = COMPASS_UI_ANIM_MID_STEP_DEG;
        alpha = 0.62f;
    }
    else
    {
        cap = COMPASS_UI_ANIM_FAST_STEP_DEG;
        alpha = 0.50f;
    }

    step = diff * alpha;
    if (fabsf(step) > cap)
    {
        step = (step > 0.0f) ? cap : -cap;
    }
    if (fabsf(step) < COMPASS_UI_ANIM_MIN_STEP_DEG)
    {
        step = (diff > 0.0f) ? COMPASS_UI_ANIM_MIN_STEP_DEG : -COMPASS_UI_ANIM_MIN_STEP_DEG;
    }
    if (fabsf(step) > abs_diff)
    {
        step = diff;
    }

    return step;
}

static bool compass_display_step_towards_target(void)
{
    float target = compass_display_predict_target();
    float diff;
    float abs_diff;
    float step;
    bool reversed;

    if (!s_compass_display_seeded)
    {
        compass_display_seed(s_compass_vm_cache.heading);
        return true;
    }

    diff = compass_shortest_delta_float(s_compass_display_heading_deg, target);
    abs_diff = fabsf(diff);
    if (abs_diff <= COMPASS_UI_ANIM_SNAP_EPS_DEG)
    {
        if (fabsf(compass_shortest_delta_float(s_compass_display_heading_deg, target)) <= COMPASS_UI_ANIM_SNAP_EPS_DEG)
        {
            s_compass_display_heading_deg = target;
        }
        if (fabsf(s_compass_display_velocity_dps) < 2.0f)
        {
            s_compass_display_last_step_deg = 0.0f;
        }
        return false;
    }

    reversed = (fabsf(s_compass_display_last_step_deg) >= COMPASS_UI_ANIM_REVERSAL_EPS_DEG) &&
               ((s_compass_display_last_step_deg > 0.0f && diff < 0.0f) ||
                (s_compass_display_last_step_deg < 0.0f && diff > 0.0f));

    if (reversed)
    {
        /*
         * 用户突然反向转头时，显示层必须立即释放上一方向的惯性。
         * 只影响 UI 补帧，不反写算法 heading。
         */
        s_compass_display_last_step_deg = 0.0f;
        s_compass_display_velocity_dps = 0.0f;
    }

    /*
     * 手机指南针式跟随：小差值直接贴近，大差值按自适应速度追赶。
     * 这样目标突变时不会一帧跳到目标，也不会固定 1 度/帧造成明显拖尾。
     */
    step = compass_display_adaptive_step(diff, abs_diff);

    s_compass_display_heading_deg =
        compass_normalize_heading_float(s_compass_display_heading_deg + step);
    s_compass_display_last_step_deg = step;
    return true;
}

/* ============================================================
 * 罗盘卷尺底层数学绘制引擎 (0 RAM 开销)
 *
 * 该回调在 LVGL 重绘时自动调用，仅渲染可见窗口内的刻度。
 * 360° 和 0° 交界处完美循环。
 *
 * 视觉规范（HTML 原型复刻）：
 *   - 每 3 度画一根密集短线
 *   - 每 15 度画一根中长线
 *   - 每 45 度画最长的线并显示 N/NE/E/SE/S/SW/W/NW
 *   - 上下两根若隐若现的边框线（OPA_30）
 *   - 正中央 3px 宽绿色瞄准基线
 *   - 目标锁定时底部显示绿色游标块
 * ============================================================ */
static void compass_tape_draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);
    lv_area_t *area = &obj->coords;

    int box_w = lv_area_get_width(area);
    int center_x = area->x1 + (box_w / 2);  /* 屏幕视口正中心 */

    float heading = compass_display_heading_snapshot();  /* UI 补帧后的显示航向 */

    /* ---- 1. 初始化画笔 ---- */
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);

    lv_draw_label_dsc_t lbl_dsc;
    lv_draw_label_dsc_init(&lbl_dsc);
    lbl_dsc.color = GREEN;
    lbl_dsc.font = get_font(FONT_ID_SMALL);  /* 14px 小字 */
    lbl_dsc.align = LV_TEXT_ALIGN_CENTER;

    /* ---- 2. 绘制上下轨道线 (极淡的透明度) ---- */
    line_dsc.color = GREEN;
    line_dsc.width = 1;
    line_dsc.opa = 76;  /* 约 30% 透明度 */
    lv_point_t top_line[] = {{area->x1, area->y1}, {area->x2, area->y1}};
    lv_point_t bot_line[] = {{area->x1, area->y2}, {area->x2, area->y2}};
    lv_draw_line(draw_ctx, &line_dsc, &top_line[0], &top_line[1]);
    lv_draw_line(draw_ctx, &line_dsc, &bot_line[0], &bot_line[1]);

    /* ---- 3. 动态计算视口左右边缘对应的"度数"范围 ---- */
    int fov_deg = (int)(box_w / PX_PER_DEGREE);
    int start_deg = (int)heading - (fov_deg / 2) - 10;
    int end_deg = (int)heading + (fov_deg / 2) + 10;

    /* ---- 4. 绘制精密刻度 ---- */
    line_dsc.opa = LV_OPA_COVER;  /* 刻度线全亮 */

    for (int i = start_deg; i <= end_deg; i++)
    {
        /* 每 3 度一根密集短线 */
        if (i % 3 != 0) continue;

        /* 数学推演：该度数在屏幕上的 X 坐标 */
        int dx = (int)((i - heading) * PX_PER_DEGREE);
        int x = center_x + dx;

        /* 越界裁剪保护 */
        if (x < area->x1 || x > area->x2) continue;

        /* 梯级刻度设计：短线 5px，中线 9px，长线 14px */
        int tick_h = 5;
        line_dsc.width = 1;

        /* 逢 15 度画中长线 */
        if (i % 15 == 0)
        {
            tick_h = 9;
            line_dsc.width = 2;
        }

        /* 逢 45 度画最长的线，并绘制 N, NE 等纯正方位字母 */
        if (i % 45 == 0)
        {
            tick_h = 14;

            /* 处理 360 度循环取模 (比如 -45度 变成 315度) */
            int display_deg = ((i % 360) + 360) % 360;
            char buf[8] = "";

            if (display_deg == 0) strcpy(buf, "N");
            else if (display_deg == 45) strcpy(buf, "NE");
            else if (display_deg == 90) strcpy(buf, "E");
            else if (display_deg == 135) strcpy(buf, "SE");
            else if (display_deg == 180) strcpy(buf, "S");
            else if (display_deg == 225) strcpy(buf, "SW");
            else if (display_deg == 270) strcpy(buf, "W");
            else if (display_deg == 315) strcpy(buf, "NW");

            /* 文本下压，避免与刻度线粘连 */
            lv_area_t txt_area = {x - 20, area->y1 + 18, x + 20, area->y1 + 38};
            lv_draw_label(draw_ctx, &lbl_dsc, &txt_area, buf, NULL);
        }

        /* 刻度线必须统一从顶部 (y1) 往下垂直画 */
        lv_point_t pts[] = {{x, area->y1}, {x, area->y1 + tick_h}};
        lv_draw_line(draw_ctx, &line_dsc, &pts[0], &pts[1]);
    }

    /* ---- 5. 贯穿整个卷尺的中心准星 (高亮加粗) ---- */
    line_dsc.color = GREEN;
    line_dsc.width = 3;
    line_dsc.opa = LV_OPA_COVER;
    lv_point_t center_pts[] = {{center_x, area->y1}, {center_x, area->y2}};
    lv_draw_line(draw_ctx, &line_dsc, &center_pts[0], &center_pts[1]);

    /* ---- 6. 目标锁定游标 (如果用户锁定了航向) ---- */
    if (s_compass_vm_cache.locked != 0U)
    {
        float target_dx = (float)(s_compass_vm_cache.heading_target - heading);

        /* 处理捷径逻辑 (如当前350，目标10，应向右走20度而不是向左走340度) */
        if (target_dx > 180.0f) target_dx -= 360.0f;
        if (target_dx < -180.0f) target_dx += 360.0f;

        int tx = center_x + (int)(target_dx * PX_PER_DEGREE);

        if (tx >= area->x1 && tx <= area->x2)
        {
            /* 在目标方位画一个向上的小实心绿块 */
            lv_draw_rect_dsc_t rect_dsc;
            lv_draw_rect_dsc_init(&rect_dsc);
            rect_dsc.bg_color = GREEN;
            rect_dsc.bg_opa = LV_OPA_COVER;
            lv_area_t t_area = {tx - 4, area->y2 - 8, tx + 4, area->y2};
            lv_draw_rect(draw_ctx, &rect_dsc, &t_area);
        }
    }
}

/* ============================================================
 * 静态句柄声明（供外部 ui_engine.c 引用）
 * ============================================================ */
static lv_obj_t *s_compass_tape_obj = NULL;   /* 卷尺绘制对象 */
static lv_obj_t *s_heading_val_lbl = NULL;    /* 巨型航向文本 */
static lv_obj_t *s_heading_hint_lbl = NULL;   /* 顶部操作提示 */

static bool compass_obj_is_valid(lv_obj_t **obj_ref)
{
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

static void compass_label_set_text_if_changed(lv_obj_t *label, const char *text)
{
    const char *old_text;

    if (label == NULL || text == NULL)
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

static void compass_update_heading_label_from_display(bool force_update)
{
    uint16_t display_heading;
    char heading_text[8];

    if (!compass_obj_is_valid(&s_heading_val_lbl))
    {
        return;
    }

    display_heading = compass_heading_from_float(compass_display_heading_snapshot());
    if (!force_update && s_compass_display_heading_text == display_heading)
    {
        return;
    }

    s_compass_display_heading_text = display_heading;
    (void)snprintf(heading_text, sizeof(heading_text), "%03u", (unsigned)display_heading);
    compass_label_set_text_if_changed(s_heading_val_lbl, heading_text);
}

static void compass_update_hint_label(void)
{
    if (!compass_obj_is_valid(&s_heading_hint_lbl))
    {
        return;
    }

    if (s_compass_vm_cache.locked != 0U)
    {
        char hint_text[32];
        (void)snprintf(hint_text, sizeof(hint_text),
                       "[ TARGET LOCKED: %03d ]",
                       s_compass_vm_cache.heading_target);
        compass_label_set_text_if_changed(s_heading_hint_lbl, hint_text);
    }
    else
    {
        compass_label_set_text_if_changed(s_heading_hint_lbl, "[ ENTER ] mark heading");
    }
}

static void compass_anim_timer_cb(lv_timer_t *timer)
{
    bool stepped;

    (void)timer;

    if (!screen_page_id_refresh_visible(PAGE_ID_COMPASS))
    {
        return;
    }

    if (!compass_obj_is_valid(&s_heading_val_lbl) &&
        !compass_obj_is_valid(&s_compass_tape_obj))
    {
        return;
    }

    stepped = compass_display_step_towards_target();
    if (!stepped)
    {
        compass_update_heading_label_from_display(false);
        return;
    }

    compass_update_heading_label_from_display(false);
    if (compass_obj_is_valid(&s_compass_tape_obj))
    {
        lv_obj_invalidate(s_compass_tape_obj);
    }
}

static void compass_anim_timer_ensure(void)
{
    if (s_compass_anim_timer == NULL)
    {
        s_compass_anim_timer =
            lv_timer_create(compass_anim_timer_cb, COMPASS_UI_ANIM_TICK_MS, NULL);
    }
}

/* ============================================================
 * 罗盘卡片工厂渲染函数
 *
 * 天地反转布局（完美对齐 HTML 原型）：
 *   1. 顶部操作提示区
 *   2. 居中巨型度数（视觉中心）
 *   3. 底部钉死卷尺
 * ============================================================ */
void render_compass_custom(lv_obj_t *parent_card)
{
    /* 1. 统一标题 */
    render_card_title(parent_card, "NAV COMPASS");

    /* 计算右侧区域宽度 */
    int right_canvas_w = (int)s_compass_vm_cache.right_canvas_w;

    /* ========================================================
     * 1. 顶部操作提示区 (Target Locked / Enter mark)
     * 挂载在大标题下方
     * ======================================================== */
    s_heading_hint_lbl = lv_label_create(parent_card);
    lv_obj_set_style_text_font(s_heading_hint_lbl, get_font(FONT_ID_SMALL), 0);
    lv_obj_set_style_text_color(s_heading_hint_lbl, LIGHT, 0);
    lv_obj_align(s_heading_hint_lbl, LV_ALIGN_TOP_MID, 0, CARD_TITLE_H + 20);

    if (s_compass_vm_cache.locked != 0U)
    {
        lv_label_set_text_fmt(s_heading_hint_lbl, "[ TARGET LOCKED: %03d ]", s_compass_vm_cache.heading_target);
    }
    else
    {
        lv_label_set_text(s_heading_hint_lbl, "[ ENTER ] mark heading");
    }

    /* ========================================================
     * 2. 居中巨型当前航向文本 (绝对视觉中心)
     * ======================================================== */
    s_heading_val_lbl = lv_label_create(parent_card);
    lv_obj_set_style_text_font(s_heading_val_lbl, get_font(FONT_ID_HUGE), 0);  /* 48px */
    lv_obj_set_style_text_color(s_heading_val_lbl, GREEN, 0);
    /* 核心修复：绝对居中，并稍微往上抬一点点 */
    lv_obj_align(s_heading_val_lbl, LV_ALIGN_CENTER, 0, -10);
    lv_label_set_text_fmt(s_heading_val_lbl, "%03d", s_compass_vm_cache.heading);

    /* 度数空心小圆圈 */
    lv_obj_t *degree_circle = lv_obj_create(parent_card);
    lv_obj_remove_style_all(degree_circle);
    lv_obj_set_size(degree_circle, 10, 10);
    lv_obj_set_style_border_width(degree_circle, 2, 0);
    lv_obj_set_style_border_color(degree_circle, GREEN, 0);
    lv_obj_set_style_radius(degree_circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_align_to(degree_circle, s_heading_val_lbl, LV_ALIGN_OUT_RIGHT_TOP, 4, 8);

    /* ========================================================
     * 3. 零内存罗盘卷尺 (Tape) -> 钉死在最底部！
     * ======================================================== */
    s_compass_tape_obj = lv_obj_create(parent_card);
    lv_obj_remove_style_all(s_compass_tape_obj);

    /* 宽度留边，高度 60px */
    lv_obj_set_size(s_compass_tape_obj, right_canvas_w - 20, COMPASS_TAPE_H);
    /* 核心修复：钉在卡片最底部，往上偏移 20px 防贴边 */
    lv_obj_align(s_compass_tape_obj, LV_ALIGN_BOTTOM_MID, 0, -20);

    /* 挂载数学绘制引擎 */
    lv_obj_add_event_cb(s_compass_tape_obj, compass_tape_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
}

void card_compass_create(lv_obj_t *parent)
{
    ui_vm_compass_update(&s_compass_vm_cache, NULL, NULL);
    compass_display_seed(s_compass_vm_cache.heading);
    render_compass_custom(parent);
    compass_anim_timer_ensure();
}

void card_compass_refresh_heading_vm(const ui_vm_compass_t *vm, bool force_refresh)
{
    if (vm != NULL)
    {
        s_compass_vm_cache = *vm;
    }
#if UI_COMPASS_DIAG_TRACE_ENABLED
    static uint32_t s_last_compass_ui_log_tick = 0;
    static uint16_t s_last_compass_ui_heading = 0xFFFFU;

    if (force_refresh)
    {
        ble_sensor_debug_note_ui_force_refresh(s_compass_vm_cache.heading);
    }
    else
    {
        uint32_t now_tick = lv_tick_get();
        bool heading_changed = (s_last_compass_ui_heading != s_compass_vm_cache.heading);
        bool heartbeat_due =
            (s_last_compass_ui_log_tick == 0U) ||
            ((now_tick - s_last_compass_ui_log_tick) >= 2000U);

        if (heading_changed || heartbeat_due)
        {
            s_last_compass_ui_log_tick = now_tick;
            s_last_compass_ui_heading = s_compass_vm_cache.heading;
            ble_sensor_debug_note_ui_dirty(s_last_compass_ui_heading);
#if UI_COMPASS_DIAG_SYSTEM_TRACE_ENABLED
            rt_kprintf("[COMPASS_UI] dirty heading=%u label=%d tape=%d card=%u dash=%u\r\n",
                       s_last_compass_ui_heading,
                       s_heading_val_lbl ? 1 : 0,
                       s_compass_tape_obj ? 1 : 0,
                       0,
                       0);
#endif
        }
    }
#else
    (void)force_refresh;
#endif

    if (!screen_page_id_refresh_visible(PAGE_ID_COMPASS))
    {
        return;
    }

    if (force_refresh || !s_compass_display_seeded)
    {
        /*
         * 页面切入/对象重建时直接对齐目标角。否则旧页面残留显示角会缓慢追赶，
         * 用户会误判为罗盘本身漂移。
         */
        compass_display_seed(s_compass_vm_cache.heading);
    }
    else
    {
        compass_display_note_target(s_compass_vm_cache.heading);
    }

    compass_anim_timer_ensure();
    compass_update_heading_label_from_display(force_refresh);
    compass_update_hint_label();
}

void card_compass_refresh_heading(bool force_refresh)
{
    card_compass_refresh_heading_vm(&s_compass_vm_cache, force_refresh);
}

void card_compass_update(void)
{
    ui_vm_compass_update(&s_compass_vm_cache, NULL, NULL);
    card_compass_refresh_heading_vm(&s_compass_vm_cache, false);
}
