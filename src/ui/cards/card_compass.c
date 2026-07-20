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
#define COMPASS_UI_PREDICT_HOLD_MS       180U
#define COMPASS_UI_PREDICT_MAX_AHEAD_DEG 1.5f
#define COMPASS_UI_PREDICT_SPEED_ALPHA   0.38f
#define COMPASS_UI_PREDICT_MAX_DPS       260.0f
#define COMPASS_UI_PREDICT_REV_GAIN      0.25f
#define COMPASS_UI_TARGET_BACKSTEP_DEG   8.0f
#define COMPASS_UI_TARGET_RELEASE_DEG    7.0f
#define COMPASS_UI_TARGET_RELEASE_MS     32U
#define COMPASS_UI_TARGET_TREND_MIN_DPS  70.0f
#define COMPASS_UI_STILL_DEADBAND_DEG    1.0f
#define COMPASS_UI_STILL_SPEED_MAX_DPS   12.0f
#define COMPASS_UI_LARGE_JUMP_DEG        45.0f
#define COMPASS_UI_LARGE_JUMP_MATCH_DEG  12.0f
#define COMPASS_UI_LARGE_JUMP_MOVE_DEG   3.0f
#define COMPASS_UI_LARGE_JUMP_CONFIRM_MS 220U
#define COMPASS_UI_PITCH_SHOW_DEG         8.0f
#define COMPASS_UI_PITCH_HIDE_DEG         5.0f
#define COMPASS_UI_PITCH_FILTER_ALPHA     0.20f
#define COMPASS_UI_PITCH_NEUTRAL_AY_MAX_G 0.25f
#define COMPASS_UI_PITCH_NEUTRAL_SAMPLES  12U
#define COMPASS_UI_CAL_RING_SIZE           132
#define COMPASS_UI_CAL_RING_RADIUS         56.0f
#define COMPASS_UI_CAL_TICK_RADIUS         64.0f
#define COMPASS_UI_CAL_BALL_SIZE           14
#define COMPASS_UI_CAL_RING_PHASE_MAX_PCT  60U
#define COMPASS_UI_CAL_BALL_FULL_TILT_G    0.45f
#define COMPASS_UI_CAL_BALL_FILTER_ALPHA   0.25f
#define COMPASS_UI_CAL_PROGRESS_ALPHA_PER_TICK 0.22f
#define COMPASS_UI_CAL_PROGRESS_MIN_RATE_PPS   5.0f
#define COMPASS_UI_CAL_PROGRESS_MAX_RATE_PPS  78.125f
#define COMPASS_UI_CAL_PROGRESS_MAX_DT_MS    100U
#define COMPASS_UI_CAL_PROGRESS_SNAP_PCT    0.05f
#define COMPASS_UI_CAL_PROGRESS_BAR_SCALE   10
#define COMPASS_UI_CAL_MOTION_START_DPS     6.0f
#define COMPASS_UI_CAL_MOTION_STOP_DPS      3.0f
#define COMPASS_UI_CAL_MOTION_STOP_HOLD_MS  320U
#define COMPASS_UI_CAL_MOTION_FRAME_MS      240U
#define COMPASS_UI_CAL_MOTION_Y             78
#define COMPASS_UI_CAL_PROGRESS_Y          104
#define COMPASS_UI_HEADING_CENTER_Y        (-10)
#define COMPASS_UI_CAL_CENTER_Y             28
#define COMPASS_UI_PITCH_CENTER_Y           48
#define COMPASS_UI_CAL_PITCH_CENTER_Y      112
#define COMPASS_UI_ARROW_UP               "\xE2\x86\x91"
#define COMPASS_UI_ARROW_DOWN             "\xE2\x86\x93"

static ui_vm_compass_t s_compass_vm_cache;
static lv_timer_t *s_compass_anim_timer = NULL;
static float s_compass_display_heading_deg = 0.0f;
static float s_compass_display_last_step_deg = 0.0f;
static float s_compass_display_target_deg = 0.0f;
static float s_compass_display_velocity_dps = 0.0f;
static float s_compass_display_pending_target_deg = 0.0f;
static float s_compass_display_pending_delta_deg = 0.0f;
static float s_compass_display_large_jump_target_deg = 0.0f;
static float s_compass_display_large_jump_delta_deg = 0.0f;
static uint32_t s_compass_display_target_tick = 0U;
static uint32_t s_compass_display_pending_target_tick = 0U;
static uint32_t s_compass_display_large_jump_tick = 0U;
static uint8_t s_compass_display_pending_target_count = 0U;
static bool s_compass_display_target_valid = false;
static bool s_compass_display_seeded = false;
static bool s_compass_display_pending_target_valid = false;
static bool s_compass_display_large_jump_valid = false;
static bool s_compass_heading_available = false;
static bool s_compass_has_valid_heading = false;
static bool s_compass_unavailable_drawn = false;
static uint16_t s_compass_display_heading_text = 0xFFFFU;
static float s_compass_pitch_display_deg = 0.0f;
static bool s_compass_pitch_seeded = false;
static bool s_compass_pitch_visible = false;
static float s_compass_pitch_neutral_deg = 0.0f;
static uint8_t s_compass_pitch_neutral_samples = 0U;
static bool s_compass_pitch_neutral_valid = false;

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

bool card_compass_display_heading_available(void)
{
    return s_compass_heading_available;
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

static void compass_display_clear_pending_target(void)
{
    s_compass_display_pending_target_valid = false;
    s_compass_display_pending_target_count = 0U;
    s_compass_display_pending_target_tick = 0U;
    s_compass_display_pending_target_deg = 0.0f;
    s_compass_display_pending_delta_deg = 0.0f;
}

static void compass_display_clear_large_jump(void)
{
    s_compass_display_large_jump_valid = false;
    s_compass_display_large_jump_target_deg = 0.0f;
    s_compass_display_large_jump_delta_deg = 0.0f;
    s_compass_display_large_jump_tick = 0U;
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
 * 静止时整数角在边界两侧反复变化会让用户看到 123/124 来回闪。显示层保留
 * 1 度 Schmitt 滞回；真实转头累计到 2 度即放行，动画仍会补出中间数字。
 */
static bool compass_display_hold_still_jitter(float new_target,
                                              float delta,
                                              uint32_t now_tick)
{
    (void)new_target;
    (void)now_tick;

    if ((fabsf(s_compass_display_velocity_dps) > COMPASS_UI_STILL_SPEED_MAX_DPS) ||
        (fabsf(delta) > COMPASS_UI_STILL_DEADBAND_DEG))
    {
        return false;
    }

    compass_display_decay_velocity(0.35f);
    return true;
}

/*
 * 静止时不允许单个融合样本把屏幕从正确方向直接拉走 45 度以上。持续错误
 * 最多等待一个完整磁采样周期；真实快速转头若下一样本继续同方向推进则立即
 * 放行，因此不会进入逐度追赶，也不会给正常连续旋转增加固定延迟。
 */
static bool compass_display_hold_unconfirmed_large_jump(float new_target,
                                                        float delta,
                                                        uint32_t now_tick)
{
    float pending_move;
    bool continues_turn;

    if ((fabsf(s_compass_display_velocity_dps) > COMPASS_UI_STILL_SPEED_MAX_DPS) ||
        (fabsf(delta) < COMPASS_UI_LARGE_JUMP_DEG))
    {
        compass_display_clear_large_jump();
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

    pending_move = compass_shortest_delta_float(s_compass_display_large_jump_target_deg,
                                                 new_target);
    continues_turn =
        ((s_compass_display_large_jump_delta_deg > 0.0f) &&
         (delta > 0.0f) &&
         (pending_move >= COMPASS_UI_LARGE_JUMP_MOVE_DEG)) ||
        ((s_compass_display_large_jump_delta_deg < 0.0f) &&
         (delta < 0.0f) &&
         (pending_move <= -COMPASS_UI_LARGE_JUMP_MOVE_DEG));
    if (continues_turn)
    {
        compass_display_clear_large_jump();
        return false;
    }

    if (fabsf(pending_move) > COMPASS_UI_LARGE_JUMP_MATCH_DEG)
    {
        s_compass_display_large_jump_target_deg = new_target;
        s_compass_display_large_jump_delta_deg = delta;
        s_compass_display_large_jump_tick = now_tick;
        return true;
    }

    if ((now_tick - s_compass_display_large_jump_tick) <
        COMPASS_UI_LARGE_JUMP_CONFIRM_MS)
    {
        return true;
    }

    compass_display_clear_large_jump();
    return false;
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

static void compass_display_release_stale_pending_target(uint32_t now_tick)
{
    if (!s_compass_display_pending_target_valid)
    {
        return;
    }

    if ((now_tick - s_compass_display_pending_target_tick) < COMPASS_UI_TARGET_RELEASE_MS)
    {
        return;
    }

    /*
     * UI 只过滤瞬时反向毛刺。若传感器下一帧尚未到达，也要在两个
     * 16ms tick 内释放目标，避免用户真实换向时数字看起来停住。
     */
    s_compass_display_target_deg = s_compass_display_pending_target_deg;
    s_compass_display_target_tick = now_tick;
    s_compass_display_velocity_dps = 0.0f;
    s_compass_display_last_step_deg = 0.0f;
    compass_display_clear_pending_target();
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
        compass_display_clear_large_jump();
        /*
         * card_compass_update() 比 BMM150 新航向快得多，同一目标会在两个磁样本
         * 之间被重复送入。重复帧不能伪装成新传感器样本并重置预测时钟，否则
         * 速度会每 16ms 被衰减，UI 只能阶梯式追随约 5Hz 的目标。预测函数会在
         * HOLD_MS 后自动停止前瞻，不会因这里保留时间戳而无限滑行。
         */
        return;
    }

    if (compass_display_hold_unconfirmed_large_jump(new_target, delta, now_tick))
    {
        return;
    }

    if (compass_display_hold_still_jitter(new_target, delta, now_tick))
    {
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
    float decay = 1.0f;

    if (!s_compass_display_target_valid)
    {
        return (float)(s_compass_vm_cache.heading % 360U);
    }

    compass_display_release_stale_pending_target(now_tick);

    dt_ms = now_tick - s_compass_display_target_tick;
    if (dt_ms >= COMPASS_UI_PREDICT_SAMPLE_MAX_MS)
    {
        s_compass_display_velocity_dps = 0.0f;
        return s_compass_display_target_deg;
    }

    if (dt_ms > COMPASS_UI_PREDICT_HOLD_MS)
    {
        /*
         * 新磁样本超出常见周期后逐步撤销前瞻，不能在固定超时点突然回到
         * 原目标。这样静止时最多产生 1.5 度、且连续衰减的收尾，不会形成
         * “快到 90 又回退”的反向台阶。
         */
        decay = (float)(COMPASS_UI_PREDICT_SAMPLE_MAX_MS - dt_ms) /
                (float)(COMPASS_UI_PREDICT_SAMPLE_MAX_MS - COMPASS_UI_PREDICT_HOLD_MS);
        dt_ms = COMPASS_UI_PREDICT_HOLD_MS;
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
    ahead *= decay;

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
static lv_obj_t *s_heading_degree_circle = NULL;
static lv_obj_t *s_heading_pitch_lbl = NULL;  /* 头戴抬头/低头姿态反馈 */
static lv_obj_t *s_compass_cal_ring = NULL;
static lv_obj_t *s_compass_cal_ball = NULL;
static lv_obj_t *s_compass_cal_sector_ticks[16] = {NULL};
static lv_obj_t *s_compass_cal_motion_lbl = NULL;
static lv_obj_t *s_compass_cal_progress_bar = NULL;
static float s_compass_cal_ball_x = 0.0f;
static float s_compass_cal_ball_y = 0.0f;
static bool s_compass_cal_ball_seeded = false;
static float s_compass_cal_display_progress = 0.0f;
static uint8_t s_compass_cal_target_progress = 0U;
static bool s_compass_cal_progress_seeded = false;
static bool s_compass_cal_progress_active = false;
static uint32_t s_compass_cal_progress_tick = 0U;
static bool s_compass_cal_motion_active = false;
static uint32_t s_compass_cal_motion_below_tick = 0U;
static uint32_t s_compass_cal_motion_frame_tick = 0U;
static uint8_t s_compass_cal_motion_frame = 0U;
static bool s_compass_hint_prominent = false;
static compass_cal_ui_state_t s_compass_last_cal_state = COMPASS_CAL_IDLE;
static uint32_t s_compass_cal_session_id = 0U;
static uint32_t s_compass_cal_complete_until_tick = 0U;
static bool s_compass_page_was_visible = false;
static compass_cal_ui_snapshot_t s_compass_cal_snapshot = {
    0U,
    COMPASS_CAL_IDLE,
    0U,
    COMPASS_CAL_HINT_LEVEL_SWEEP,
    0U,
    0U,
};

static void compass_refresh_calibration_snapshot(void)
{
    compass_cal_ui_snapshot_t snapshot;

    if (get_compass_calibration_snapshot(&snapshot))
    {
        if (snapshot.session_id != s_compass_cal_session_id)
        {
            /*
             * 新会话必须一次性清除上次校准的平滑、运动和完成提示状态。
             * 页面隐藏期间开始校准时，返回页面后会直接接续该会话的真实进度。
             */
            s_compass_cal_session_id = snapshot.session_id;
            s_compass_cal_display_progress = 0.0f;
            s_compass_cal_target_progress = 0U;
            s_compass_cal_progress_seeded = false;
            s_compass_cal_progress_active = false;
            s_compass_cal_progress_tick = 0U;
            s_compass_cal_motion_active = false;
            s_compass_cal_motion_below_tick = 0U;
            s_compass_cal_motion_frame_tick = 0U;
            s_compass_cal_motion_frame = 0U;
            s_compass_cal_ball_seeded = false;
            s_compass_last_cal_state = COMPASS_CAL_IDLE;
            s_compass_cal_complete_until_tick = 0U;
        }
        s_compass_cal_snapshot = snapshot;
    }
}

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

static void compass_set_hint_prominent(bool prominent)
{
    if (!compass_obj_is_valid(&s_heading_hint_lbl) ||
        (s_compass_hint_prominent == prominent))
    {
        return;
    }

    s_compass_hint_prominent = prominent;
    lv_obj_set_style_text_font(s_heading_hint_lbl,
                               get_font(prominent ? FONT_ID_BIG_TITLE :
                                                      FONT_ID_SMALL),
                               0);
    lv_obj_set_style_text_letter_space(s_heading_hint_lbl, 0, 0);
    lv_obj_set_style_text_line_space(s_heading_hint_lbl, prominent ? 2 : 0, 0);
    lv_obj_align(s_heading_hint_lbl,
                 LV_ALIGN_TOP_MID,
                 0,
                 prominent ? (CARD_TITLE_H + 4) : (CARD_TITLE_H + 20));
}

static bool compass_calibration_progress_visible(compass_cal_ui_state_t state)
{
    return (state == COMPASS_CAL_RUNNING) ||
           (state == COMPASS_CAL_VERIFYING) ||
           (state == COMPASS_CAL_SAVING);
}

static uint8_t compass_calibration_display_progress_pct(void)
{
    int progress = compass_round_float_to_int(s_compass_cal_display_progress);

    if (progress < 0)
    {
        progress = 0;
    }
    else if (progress > 100)
    {
        progress = 100;
    }
    return (uint8_t)progress;
}

static void compass_update_calibration_display_progress(void)
{
    const compass_cal_ui_state_t state = s_compass_cal_snapshot.state;
    const bool active = compass_calibration_progress_visible(state);
    const uint8_t published_progress = s_compass_cal_snapshot.progress_pct;
    const uint32_t now_tick = lv_tick_get();
    uint32_t dt_ms;
    float diff;
    float step;
    float min_step;
    float max_step;

    if (!active)
    {
        s_compass_cal_progress_active = false;
        s_compass_cal_progress_seeded = false;
        s_compass_cal_target_progress = 0U;
        s_compass_cal_progress_tick = 0U;
        s_compass_cal_display_progress =
            (state == COMPASS_CAL_READY) ? 100.0f : 0.0f;
        return;
    }

    if (!s_compass_cal_progress_seeded || !s_compass_cal_progress_active)
    {
        /* 页面中途进入校准时直接接上真实进度，不能从 0% 伪播放。 */
        s_compass_cal_target_progress = published_progress;
        s_compass_cal_display_progress = (float)published_progress;
        s_compass_cal_progress_seeded = true;
        s_compass_cal_progress_active = true;
        s_compass_cal_progress_tick = now_tick;
        return;
    }

    /* 发布链切换阶段时可能短暂读到旧值，展示层只允许单调前进。 */
    if (published_progress > s_compass_cal_target_progress)
    {
        s_compass_cal_target_progress = published_progress;
    }

    diff = (float)s_compass_cal_target_progress - s_compass_cal_display_progress;
    if (diff <= COMPASS_UI_CAL_PROGRESS_SNAP_PCT)
    {
        s_compass_cal_display_progress = (float)s_compass_cal_target_progress;
        s_compass_cal_progress_tick = now_tick;
        return;
    }

    dt_ms = (s_compass_cal_progress_tick == 0U) ?
                COMPASS_UI_ANIM_TICK_MS :
                (now_tick - s_compass_cal_progress_tick);
    s_compass_cal_progress_tick = now_tick;
    if (dt_ms == 0U)
    {
        return;
    }
    if (dt_ms > COMPASS_UI_CAL_PROGRESS_MAX_DT_MS)
    {
        dt_ms = COMPASS_UI_CAL_PROGRESS_MAX_DT_MS;
    }

    /*
     * 进度速度按真实经过时间计算。LVGL偶发晚调度时不会变慢，连续补帧时也
     * 不会突然加速；目标仍只来自算法已接受的真实进度，不生成墙钟假进度。
     */
    step = diff * COMPASS_UI_CAL_PROGRESS_ALPHA_PER_TICK *
           ((float)dt_ms / (float)COMPASS_UI_ANIM_TICK_MS);
    min_step = COMPASS_UI_CAL_PROGRESS_MIN_RATE_PPS *
               ((float)dt_ms / 1000.0f);
    max_step = COMPASS_UI_CAL_PROGRESS_MAX_RATE_PPS *
               ((float)dt_ms / 1000.0f);
    if (step < min_step)
    {
        step = min_step;
    }
    else if (step > max_step)
    {
        step = max_step;
    }
    if (step > diff)
    {
        step = diff;
    }
    s_compass_cal_display_progress += step;
}

static bool compass_update_calibration_motion(uint32_t now_tick)
{
    const float gyro_x = bus_get_gyro_x_dps();
    const float gyro_y = bus_get_gyro_y_dps();
    const float gyro_z = bus_get_gyro_z_dps();
    float speed_dps = sqrtf((gyro_x * gyro_x) +
                            (gyro_y * gyro_y) +
                            (gyro_z * gyro_z));

    if (!isfinite(speed_dps))
    {
        speed_dps = 0.0f;
    }

    if (!s_compass_cal_motion_active)
    {
        if (speed_dps >= COMPASS_UI_CAL_MOTION_START_DPS)
        {
            s_compass_cal_motion_active = true;
            s_compass_cal_motion_below_tick = 0U;
            s_compass_cal_motion_frame_tick = now_tick;
        }
        return s_compass_cal_motion_active;
    }

    if (speed_dps > COMPASS_UI_CAL_MOTION_STOP_DPS)
    {
        s_compass_cal_motion_below_tick = 0U;
    }
    else if (s_compass_cal_motion_below_tick == 0U)
    {
        s_compass_cal_motion_below_tick = now_tick;
    }
    else if ((now_tick - s_compass_cal_motion_below_tick) >=
             COMPASS_UI_CAL_MOTION_STOP_HOLD_MS)
    {
        s_compass_cal_motion_active = false;
        s_compass_cal_motion_below_tick = 0U;
        s_compass_cal_motion_frame = 0U;
        s_compass_cal_motion_frame_tick = now_tick;
    }

    return s_compass_cal_motion_active;
}

static void compass_reset_calibration_motion(void)
{
    s_compass_cal_motion_active = false;
    s_compass_cal_motion_below_tick = 0U;
    s_compass_cal_motion_frame_tick = 0U;
    s_compass_cal_motion_frame = 0U;
}

static void compass_update_calibration_sector_ticks(uint16_t coverage_mask,
                                                    bool use_coverage_mask,
                                                    uint8_t covered_count)
{
    for (uint8_t i = 0U; i < 16U; ++i)
    {
        if (compass_obj_is_valid(&s_compass_cal_sector_ticks[i]))
        {
            const float angle = (-90.0f + ((float)i * 22.5f)) *
                                (3.14159265358979323846f / 180.0f);
            const bool covered = use_coverage_mask ?
                ((coverage_mask & (uint16_t)(1U << i)) != 0U) :
                (i < covered_count);

            lv_obj_align(s_compass_cal_sector_ticks[i],
                         LV_ALIGN_CENTER,
                         (lv_coord_t)(cosf(angle) * COMPASS_UI_CAL_TICK_RADIUS),
                         (lv_coord_t)(sinf(angle) * COMPASS_UI_CAL_TICK_RADIUS) +
                             COMPASS_UI_CAL_CENTER_Y);
            lv_obj_set_style_bg_opa(s_compass_cal_sector_ticks[i],
                                    covered ? LV_OPA_COVER : LV_OPA_TRANSP,
                                    0);
            lv_obj_set_style_border_width(s_compass_cal_sector_ticks[i],
                                          covered ? 0 : 1,
                                          0);
            lv_obj_set_style_border_color(s_compass_cal_sector_ticks[i],
                                          GREEN,
                                          0);
            lv_obj_set_style_border_opa(s_compass_cal_sector_ticks[i],
                                        LV_OPA_COVER,
                                        0);
            lv_obj_clear_flag(s_compass_cal_sector_ticks[i],
                              LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static float compass_head_pitch_deg(void)
{
    const float acc_x = bus_get_accel_x_g();
    const float acc_y = bus_get_accel_y_g();
    float raw_pitch_deg;

    /*
     * EVT2 头戴安装下，水平时重力主要落在 BMI270 +X，低头转向 +Y，
     * 抬头转向 -Y。使用重力向量而不是 AHRS Euler pitch，避免 75~90 度
     * 附近的欧拉角奇异和方向翻转。
     */
    if (!isfinite(acc_x) || !isfinite(acc_y) ||
        ((fabsf(acc_x) + fabsf(acc_y)) < 0.20f))
    {
        return 0.0f;
    }
    raw_pitch_deg =
        atan2f(acc_y, fabsf(acc_x)) * (180.0f / 3.14159265358979323846f);

    /*
     * 头戴结构的中性 Ay 不是理论零点。只在进入页面后的稳定近水平窗口
     * 建一次本机中性基准；若用户一开始就是明显低头/抬头，则不采纳该姿态
     * 作为基准，仍显示原始相对方向，提示用户回到水平。
     */
    if ((!s_compass_pitch_neutral_valid) &&
        (fabsf(acc_y) <= COMPASS_UI_PITCH_NEUTRAL_AY_MAX_G)) {
        if (s_compass_pitch_neutral_samples == 0U) {
            s_compass_pitch_neutral_deg = raw_pitch_deg;
        } else {
            s_compass_pitch_neutral_deg +=
                (raw_pitch_deg - s_compass_pitch_neutral_deg) /
                (float)(s_compass_pitch_neutral_samples + 1U);
        }
        if (s_compass_pitch_neutral_samples < 0xFFU) {
            s_compass_pitch_neutral_samples++;
        }
        if (s_compass_pitch_neutral_samples >=
            COMPASS_UI_PITCH_NEUTRAL_SAMPLES) {
            s_compass_pitch_neutral_valid = true;
        }
    }

    return s_compass_pitch_neutral_valid ?
               (raw_pitch_deg - s_compass_pitch_neutral_deg) : raw_pitch_deg;
}

static void compass_update_pitch_indicator(void)
{
    float pitch_deg;
    float abs_pitch_deg;
    char pitch_text[24];

    if (!compass_obj_is_valid(&s_heading_pitch_lbl))
    {
        return;
    }

    if ((s_compass_cal_snapshot.state == COMPASS_CAL_RUNNING) &&
        (s_compass_cal_snapshot.hint == COMPASS_CAL_HINT_ROLL_CIRCLE))
    {
        lv_obj_add_flag(s_heading_pitch_lbl, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_align(s_heading_pitch_lbl,
                 LV_ALIGN_CENTER,
                 0,
                 (s_compass_cal_snapshot.state == COMPASS_CAL_RUNNING) ?
                     COMPASS_UI_CAL_PITCH_CENTER_Y :
                     COMPASS_UI_PITCH_CENTER_Y);

    pitch_deg = compass_head_pitch_deg();
    if (!s_compass_pitch_seeded)
    {
        s_compass_pitch_display_deg = pitch_deg;
        s_compass_pitch_seeded = true;
    }
    else
    {
        s_compass_pitch_display_deg +=
            (pitch_deg - s_compass_pitch_display_deg) * COMPASS_UI_PITCH_FILTER_ALPHA;
    }
    abs_pitch_deg = fabsf(s_compass_pitch_display_deg);

    if (s_compass_pitch_visible)
    {
        s_compass_pitch_visible = abs_pitch_deg > COMPASS_UI_PITCH_HIDE_DEG;
    }
    else
    {
        s_compass_pitch_visible = abs_pitch_deg >= COMPASS_UI_PITCH_SHOW_DEG;
    }

    if (!s_compass_pitch_visible)
    {
        lv_obj_add_flag(s_heading_pitch_lbl, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    (void)snprintf(pitch_text,
                   sizeof(pitch_text),
                   (s_compass_pitch_display_deg < 0.0f) ?
                       "HEAD " COMPASS_UI_ARROW_UP " %02u" :
                       "HEAD " COMPASS_UI_ARROW_DOWN " %02u",
                   (unsigned)compass_round_float_to_int(abs_pitch_deg));
    compass_label_set_text_if_changed(s_heading_pitch_lbl, pitch_text);
    lv_obj_clear_flag(s_heading_pitch_lbl, LV_OBJ_FLAG_HIDDEN);
}

static void compass_update_calibration_ring(void)
{
    const bool ring_valid = compass_obj_is_valid(&s_compass_cal_ring);
    const bool ball_valid = compass_obj_is_valid(&s_compass_cal_ball);
    const bool motion_valid = compass_obj_is_valid(&s_compass_cal_motion_lbl);
    const bool progress_valid = compass_obj_is_valid(&s_compass_cal_progress_bar);
    const compass_cal_ui_state_t cal_state = s_compass_cal_snapshot.state;
    const compass_cal_hint_t cal_hint = s_compass_cal_snapshot.hint;
    const bool roll_mode =
        (cal_state == COMPASS_CAL_RUNNING) &&
        (cal_hint == COMPASS_CAL_HINT_ROLL_CIRCLE);
    const bool level_mode =
        (cal_state == COMPASS_CAL_RUNNING) &&
        (cal_hint != COMPASS_CAL_HINT_ROLL_CIRCLE) &&
        (cal_hint != COMPASS_CAL_HINT_FACTORY_SERVICE) &&
        (cal_hint != COMPASS_CAL_HINT_MAG_ENVIRONMENT) &&
        (cal_hint != COMPASS_CAL_HINT_SENSOR_DATA);
    const bool ring_visible = roll_mode || level_mode;

    if (!ring_valid || !ball_valid)
    {
        return;
    }

    if (!ring_visible)
    {
        lv_obj_add_flag(s_compass_cal_ring, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_compass_cal_ball, LV_OBJ_FLAG_HIDDEN);
        if (motion_valid)
        {
            lv_obj_add_flag(s_compass_cal_motion_lbl, LV_OBJ_FLAG_HIDDEN);
        }
        if (progress_valid)
        {
            lv_obj_add_flag(s_compass_cal_progress_bar, LV_OBJ_FLAG_HIDDEN);
        }
        s_compass_cal_ball_seeded = false;
        compass_reset_calibration_motion();
        for (uint8_t i = 0U; i < 16U; ++i)
        {
            if (compass_obj_is_valid(&s_compass_cal_sector_ticks[i]))
            {
                lv_obj_add_flag(s_compass_cal_sector_ticks[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
        if (compass_obj_is_valid(&s_compass_tape_obj))
        {
            if ((cal_state == COMPASS_CAL_RUNNING) ||
                (cal_state == COMPASS_CAL_VERIFYING) ||
                (cal_state == COMPASS_CAL_SAVING) ||
                (cal_state == COMPASS_CAL_ERROR) ||
                (cal_state == COMPASS_CAL_SAVE_ERROR))
            {
                lv_obj_add_flag(s_compass_tape_obj, LV_OBJ_FLAG_HIDDEN);
            }
            else
            {
                lv_obj_clear_flag(s_compass_tape_obj, LV_OBJ_FLAG_HIDDEN);
            }
        }
        return;
    }

    if (roll_mode)
    {
        const uint8_t total_progress = compass_calibration_display_progress_pct();
        const uint8_t ring_progress =
            (total_progress >= COMPASS_UI_CAL_RING_PHASE_MAX_PCT) ? 100U :
            (uint8_t)(((uint32_t)total_progress * 100U) /
                      COMPASS_UI_CAL_RING_PHASE_MAX_PCT);
        const uint16_t arc_end =
            (ring_progress == 0U) ? 1U :
            (uint16_t)(((uint32_t)ring_progress * 360U) / 100U);
        const uint8_t covered_ticks =
            (uint8_t)(((uint32_t)ring_progress * 16U + 99U) / 100U);
        const float acc_y = bus_get_accel_y_g();
        const float acc_z = bus_get_accel_z_g();
        float tilt_g = sqrtf((acc_y * acc_y) + (acc_z * acc_z));
        float ball_radius;
        float ball_angle;
        float target_ball_x;
        float target_ball_y;

        if (!isfinite(tilt_g))
        {
            tilt_g = 0.0f;
        }
        ball_radius = (tilt_g / COMPASS_UI_CAL_BALL_FULL_TILT_G) *
                      COMPASS_UI_CAL_RING_RADIUS;
        if (ball_radius > COMPASS_UI_CAL_RING_RADIUS)
        {
            ball_radius = COMPASS_UI_CAL_RING_RADIUS;
        }
        ball_angle = atan2f(acc_z, acc_y);
        target_ball_x = cosf(ball_angle) * ball_radius;
        target_ball_y = sinf(ball_angle) * ball_radius;
        if (!s_compass_cal_ball_seeded)
        {
            s_compass_cal_ball_x = target_ball_x;
            s_compass_cal_ball_y = target_ball_y;
            s_compass_cal_ball_seeded = true;
        }
        else
        {
            s_compass_cal_ball_x +=
                (target_ball_x - s_compass_cal_ball_x) *
                COMPASS_UI_CAL_BALL_FILTER_ALPHA;
            s_compass_cal_ball_y +=
                (target_ball_y - s_compass_cal_ball_y) *
                COMPASS_UI_CAL_BALL_FILTER_ALPHA;
        }

        lv_arc_set_angles(s_compass_cal_ring, 0U, arc_end);
        lv_obj_add_flag(s_compass_cal_ring, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(s_compass_cal_ball,
                     LV_ALIGN_CENTER,
                     compass_round_float_to_int(s_compass_cal_ball_x),
                     compass_round_float_to_int(s_compass_cal_ball_y) +
                         COMPASS_UI_CAL_CENTER_Y);
        lv_obj_clear_flag(s_compass_cal_ball, LV_OBJ_FLAG_HIDDEN);
        /* 三维阶段尚未发布空间方向 mask，仅把圆点作为中性进度刻度。 */
        compass_update_calibration_sector_ticks(0U, false, covered_ticks);
        compass_reset_calibration_motion();
        if (motion_valid)
        {
            lv_obj_add_flag(s_compass_cal_motion_lbl, LV_OBJ_FLAG_HIDDEN);
        }
        if (progress_valid)
        {
            lv_obj_add_flag(s_compass_cal_progress_bar, LV_OBJ_FLAG_HIDDEN);
        }
    }
    else
    {
        static const char *const motion_frames[] = {
            "<       >",
            "<<     >>",
            "<<<   >>>",
        };
        int32_t progress_bar_value = compass_round_float_to_int(
            s_compass_cal_display_progress *
            (float)COMPASS_UI_CAL_PROGRESS_BAR_SCALE);
        const uint16_t coverage_mask = s_compass_cal_snapshot.coverage_mask;
        const uint32_t now_tick = lv_tick_get();
        const bool moving = compass_update_calibration_motion(now_tick);

        if (progress_bar_value < 0)
        {
            progress_bar_value = 0;
        }
        else if (progress_bar_value >
                 (80 * COMPASS_UI_CAL_PROGRESS_BAR_SCALE))
        {
            progress_bar_value = 80 * COMPASS_UI_CAL_PROGRESS_BAR_SCALE;
        }

        /*
         * 无工厂三轴模型时只采近水平 Y/Z 闭环。禁止显示自动绕圈滚球，
         * 否则用户会跟随亮点倾斜面镜，姿态门控会拒绝这些样本并长期卡住。
         * 单色光机不依赖明暗点数区分状态；使用大号展开动效和有边框进度条，
         * 即使所有像素都只能显示绿色，用户仍能理解“保持水平并转动面镜”。
         */
        lv_obj_add_flag(s_compass_cal_ring, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_compass_cal_ball, LV_OBJ_FLAG_HIDDEN);
        s_compass_cal_ball_seeded = false;
        /* 水平阶段优先显示算法真正接收的方向，而不是按百分比顺序点亮。 */
        compass_update_calibration_sector_ticks(coverage_mask, true, 0U);
        if (moving &&
            ((now_tick - s_compass_cal_motion_frame_tick) >=
             COMPASS_UI_CAL_MOTION_FRAME_MS))
        {
            s_compass_cal_motion_frame =
                (uint8_t)((s_compass_cal_motion_frame + 1U) % 3U);
            s_compass_cal_motion_frame_tick = now_tick;
        }
        if (motion_valid && !s_compass_pitch_visible)
        {
            compass_label_set_text_if_changed(
                s_compass_cal_motion_lbl,
                motion_frames[s_compass_cal_motion_frame]);
            lv_obj_clear_flag(s_compass_cal_motion_lbl, LV_OBJ_FLAG_HIDDEN);
        }
        else if (motion_valid)
        {
            lv_obj_add_flag(s_compass_cal_motion_lbl, LV_OBJ_FLAG_HIDDEN);
        }
        if (progress_valid && !s_compass_pitch_visible)
        {
            lv_bar_set_value(s_compass_cal_progress_bar,
                             progress_bar_value,
                             LV_ANIM_OFF);
            lv_obj_clear_flag(s_compass_cal_progress_bar, LV_OBJ_FLAG_HIDDEN);
        }
        else if (progress_valid)
        {
            lv_obj_add_flag(s_compass_cal_progress_bar, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (compass_obj_is_valid(&s_compass_tape_obj))
    {
        lv_obj_add_flag(s_compass_tape_obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static void compass_update_heading_label_from_display(bool force_update)
{
    compass_cal_ui_state_t cal_state;
    uint16_t display_heading;
    char heading_text[8];

    if (!compass_obj_is_valid(&s_heading_val_lbl))
    {
        return;
    }

    cal_state = s_compass_cal_snapshot.state;
    lv_obj_align(s_heading_val_lbl,
                 LV_ALIGN_CENTER,
                 0,
                 ((cal_state == COMPASS_CAL_RUNNING) ||
                  (cal_state == COMPASS_CAL_VERIFYING) ||
                  (cal_state == COMPASS_CAL_SAVING) ||
                  (cal_state == COMPASS_CAL_SAVE_ERROR)) ?
                     COMPASS_UI_CAL_CENTER_Y :
                     COMPASS_UI_HEADING_CENTER_Y);
    if (cal_state == COMPASS_CAL_ERROR)
    {
        compass_label_set_text_if_changed(s_heading_val_lbl, "---");
        s_compass_display_heading_text = 0xFFFFU;
        if (compass_obj_is_valid(&s_heading_degree_circle))
        {
            lv_obj_add_flag(s_heading_degree_circle, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }
    if (cal_state == COMPASS_CAL_SAVE_ERROR)
    {
        compass_label_set_text_if_changed(s_heading_val_lbl, "ERR");
        s_compass_display_heading_text = 0xFFFFU;
        if (compass_obj_is_valid(&s_heading_degree_circle))
        {
            lv_obj_add_flag(s_heading_degree_circle, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }
    if ((cal_state == COMPASS_CAL_RUNNING) ||
        (cal_state == COMPASS_CAL_VERIFYING) ||
        (cal_state == COMPASS_CAL_SAVING))
    {
        /* 中央大字显示实时百分比，不再长期显示静态 CAL，也避免裸数字“2”被误解。 */
        (void)snprintf(heading_text,
                       sizeof(heading_text),
                       "%02u%%",
                       (unsigned)compass_calibration_display_progress_pct());
        compass_label_set_text_if_changed(s_heading_val_lbl, heading_text);
        s_compass_display_heading_text = 0xFFFFU;
        if (compass_obj_is_valid(&s_heading_degree_circle))
        {
            lv_obj_add_flag(s_heading_degree_circle, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }
    if (!s_compass_heading_available && !s_compass_has_valid_heading)
    {
        compass_label_set_text_if_changed(s_heading_val_lbl, "---");
        s_compass_display_heading_text = 0xFFFFU;
        if (compass_obj_is_valid(&s_heading_degree_circle))
        {
            lv_obj_add_flag(s_heading_degree_circle, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    if (compass_obj_is_valid(&s_heading_degree_circle))
    {
        lv_obj_clear_flag(s_heading_degree_circle, LV_OBJ_FLAG_HIDDEN);
    }
    display_heading = compass_heading_from_float(compass_display_heading_snapshot());
    if (!force_update && s_compass_display_heading_text == display_heading)
    {
        return;
    }

    s_compass_display_heading_text = display_heading;
    (void)snprintf(heading_text, sizeof(heading_text), "%03u", (unsigned)display_heading);
    compass_label_set_text_if_changed(s_heading_val_lbl, heading_text);
    if (compass_obj_is_valid(&s_heading_degree_circle))
    {
        lv_obj_align_to(s_heading_degree_circle,
                        s_heading_val_lbl,
                        LV_ALIGN_OUT_RIGHT_TOP,
                        4,
                        8);
    }
}

static void compass_update_hint_label(void)
{
    compass_cal_ui_state_t cal_state;
    compass_cal_ui_state_t previous_state;
    uint32_t now_tick;

    if (!compass_obj_is_valid(&s_heading_hint_lbl))
    {
        return;
    }

    now_tick = lv_tick_get();
    cal_state = s_compass_cal_snapshot.state;
    previous_state = s_compass_last_cal_state;
    if (cal_state != previous_state)
    {
        s_compass_last_cal_state = cal_state;
        s_compass_unavailable_drawn = false;
        if ((cal_state == COMPASS_CAL_READY) &&
            ((previous_state == COMPASS_CAL_RUNNING) ||
             (previous_state == COMPASS_CAL_VERIFYING) ||
             (previous_state == COMPASS_CAL_SAVING)))
        {
            s_compass_cal_complete_until_tick = now_tick + 2000U;
        }
    }

    if ((cal_state == COMPASS_CAL_READY) &&
        ((int32_t)(s_compass_cal_complete_until_tick - now_tick) > 0))
    {
        compass_set_hint_prominent(true);
        compass_label_set_text_if_changed(s_heading_hint_lbl,
                                          "CAL COMPLETE\nSAVED");
        return;
    }

    if (cal_state == COMPASS_CAL_RUNNING)
    {
        char hint_text[64];
        const compass_cal_hint_t cal_hint = s_compass_cal_snapshot.hint;
        const uint8_t coverage_bins = s_compass_cal_snapshot.coverage_bins;
        const uint8_t remaining_bins =
            (coverage_bins < 16U) ? (uint8_t)(16U - coverage_bins) : 0U;
        compass_set_hint_prominent(true);

        if (cal_hint == COMPASS_CAL_HINT_FACTORY_SERVICE)
        {
            (void)snprintf(hint_text,
                           sizeof(hint_text),
                           "FACTORY CAL\nSERVICE REQUIRED");
        }
        else if (cal_hint == COMPASS_CAL_HINT_ROLL_CIRCLE)
        {
            (void)snprintf(hint_text,
                           sizeof(hint_text),
                           "TILT MASK SLOWLY\nMOVE DOT AROUND");
        }
        else if (cal_hint == COMPASS_CAL_HINT_MAG_ENVIRONMENT)
        {
            (void)snprintf(hint_text,
                           sizeof(hint_text),
                           "MOVE AWAY FROM\nMETAL");
        }
        else if (cal_hint == COMPASS_CAL_HINT_SENSOR_DATA)
        {
            (void)snprintf(hint_text,
                           sizeof(hint_text),
                           "NO MAG DATA\nRESTART MASK");
        }
        else if (cal_hint == COMPASS_CAL_HINT_MODEL_QUALITY)
        {
            (void)snprintf(hint_text,
                           sizeof(hint_text),
                           "KEEP MASK LEVEL\nREPEAT MORE SLOWLY");
        }
        else if (s_compass_pitch_visible)
        {
            (void)snprintf(hint_text,
                           sizeof(hint_text),
                           "LEVEL THE MASK\nTHEN TURN");
        }
        else if (cal_hint == COMPASS_CAL_HINT_MORE_COVERAGE)
        {
            (void)snprintf(hint_text,
                           sizeof(hint_text),
                           "ROTATE MASK SLOWLY\n%u DIRECTIONS LEFT",
                           (unsigned)remaining_bins);
        }
        else if (cal_hint == COMPASS_CAL_HINT_RETURN_LEVEL)
        {
            (void)snprintf(hint_text,
                           sizeof(hint_text),
                           "LEVEL MASK\nTURN SLOWLY");
        }
        else
        {
            (void)snprintf(hint_text,
                           sizeof(hint_text),
                           "KEEP MASK LEVEL\nROTATE SLOWLY");
        }
        compass_label_set_text_if_changed(s_heading_hint_lbl, hint_text);
        return;
    }

    if (cal_state == COMPASS_CAL_SAVING)
    {
        static const char spinner[] = "|/-\\";
        char hint_text[48];
        const char frame = spinner[(now_tick / 160U) & 0x03U];
        compass_set_hint_prominent(true);
        (void)snprintf(hint_text,
                       sizeof(hint_text),
                       "SAVING %c\nKEEP POWER ON",
                       frame);
        compass_label_set_text_if_changed(s_heading_hint_lbl, hint_text);
        return;
    }

    if (cal_state == COMPASS_CAL_VERIFYING)
    {
        static const char spinner[] = "|/-\\";
        char hint_text[48];
        const char frame = spinner[(now_tick / 160U) & 0x03U];
        compass_set_hint_prominent(true);
        (void)snprintf(hint_text,
                       sizeof(hint_text),
                       "HOLD MASK STILL\nVERIFYING %c",
                       frame);
        compass_label_set_text_if_changed(s_heading_hint_lbl, hint_text);
        return;
    }

    if (cal_state == COMPASS_CAL_ERROR)
    {
        compass_set_hint_prominent(true);
        compass_label_set_text_if_changed(s_heading_hint_lbl,
                                          "COMPASS ERROR\nRESTART DEVICE");
        return;
    }

    if (cal_state == COMPASS_CAL_SAVE_ERROR)
    {
        compass_set_hint_prominent(true);
        compass_label_set_text_if_changed(s_heading_hint_lbl,
                                          "SAVE FAILED\nSTART CAL AGAIN");
        return;
    }

    /* 离开校准态后必须恢复普通提示字号，页面状态切换不能遗留 40px 字体。 */
    compass_set_hint_prominent(false);
    if (!s_compass_heading_available)
    {
        compass_label_set_text_if_changed(
            s_heading_hint_lbl,
            s_compass_has_valid_heading ?
                "HEADING HOLD\nWAIT FOR RECOVERY" : "ACQUIRING HEADING");
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
    bool became_visible;

    (void)timer;

    if (!screen_page_id_refresh_visible(PAGE_ID_COMPASS))
    {
        s_compass_page_was_visible = false;
        return;
    }
    became_visible = !s_compass_page_was_visible;
    s_compass_page_was_visible = true;

    if (!compass_obj_is_valid(&s_heading_val_lbl) &&
        !compass_obj_is_valid(&s_compass_tape_obj))
    {
        return;
    }

    /* 一次读取完整快照，保证本帧百分比、提示、状态和覆盖点来自同一拍。 */
    compass_refresh_calibration_snapshot();
    if (became_visible &&
        compass_calibration_progress_visible(s_compass_cal_snapshot.state))
    {
        /* 页面隐藏期间算法可能已推进很多，重入首帧直接接真实进度。 */
        s_compass_cal_target_progress = s_compass_cal_snapshot.progress_pct;
        s_compass_cal_display_progress =
            (float)s_compass_cal_snapshot.progress_pct;
        s_compass_cal_progress_seeded = true;
        s_compass_cal_progress_active = true;
        s_compass_cal_progress_tick = lv_tick_get();
    }
    /* 校准展示独立于 heading 可用性，首次校准期间也必须持续刷新百分比。 */
    compass_update_calibration_display_progress();
    compass_update_pitch_indicator();
    compass_update_calibration_ring();
    compass_update_hint_label();

    if (!s_compass_heading_available)
    {
        if (compass_calibration_progress_visible(s_compass_cal_snapshot.state))
        {
            compass_update_heading_label_from_display(false);
            s_compass_unavailable_drawn = false;
        }
        else if (!s_compass_unavailable_drawn)
        {
            compass_update_heading_label_from_display(false);
            s_compass_unavailable_drawn = true;
        }
        return;
    }

    s_compass_unavailable_drawn = false;
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
    compass_refresh_calibration_snapshot();

    /* 1. 统一标题 */
    render_card_title(parent_card, "NAV COMPASS");

    /* 计算右侧区域宽度 */
    int right_canvas_w = (int)s_compass_vm_cache.right_canvas_w;

    /* ========================================================
     * 1. 顶部操作提示区 (Target Locked / Enter mark)
     * 挂载在大标题下方
     * ======================================================== */
    s_heading_hint_lbl = lv_label_create(parent_card);
    s_compass_hint_prominent = false;
    lv_obj_set_style_text_font(s_heading_hint_lbl, get_font(FONT_ID_SMALL), 0);
    lv_obj_set_style_text_color(s_heading_hint_lbl, LIGHT, 0);
    lv_obj_align(s_heading_hint_lbl, LV_ALIGN_TOP_MID, 0, CARD_TITLE_H + 20);

    lv_obj_set_width(s_heading_hint_lbl, right_canvas_w - 12);
    lv_obj_set_style_text_align(s_heading_hint_lbl, LV_TEXT_ALIGN_CENTER, 0);
    compass_update_hint_label();

    /* ========================================================
     * 2. 居中巨型当前航向文本 (绝对视觉中心)
     * ======================================================== */
    s_heading_val_lbl = lv_label_create(parent_card);
    lv_obj_set_style_text_font(s_heading_val_lbl, get_font(FONT_ID_HUGE), 0);  /* 48px */
    lv_obj_set_style_text_color(s_heading_val_lbl, GREEN, 0);
    /* 核心修复：绝对居中，并稍微往上抬一点点 */
    lv_obj_align(s_heading_val_lbl,
                 LV_ALIGN_CENTER,
                 0,
                 COMPASS_UI_HEADING_CENTER_Y);
    lv_label_set_text(s_heading_val_lbl, "000");

    /* 度数空心小圆圈 */
    s_heading_degree_circle = lv_obj_create(parent_card);
    lv_obj_remove_style_all(s_heading_degree_circle);
    lv_obj_set_size(s_heading_degree_circle, 10, 10);
    lv_obj_set_style_border_width(s_heading_degree_circle, 2, 0);
    lv_obj_set_style_border_color(s_heading_degree_circle, GREEN, 0);
    lv_obj_set_style_radius(s_heading_degree_circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_align_to(s_heading_degree_circle, s_heading_val_lbl, LV_ALIGN_OUT_RIGHT_TOP, 4, 8);
    compass_update_heading_label_from_display(true);

    /* 苹果式圆环滚球校准：圆弧表示算法已接受覆盖，小球由实时重力方向驱动。 */
    s_compass_cal_ring = lv_arc_create(parent_card);
    lv_obj_set_size(s_compass_cal_ring,
                    COMPASS_UI_CAL_RING_SIZE,
                    COMPASS_UI_CAL_RING_SIZE);
    lv_obj_align(s_compass_cal_ring,
                 LV_ALIGN_CENTER,
                 0,
                 COMPASS_UI_CAL_CENTER_Y);
    lv_arc_set_bg_angles(s_compass_cal_ring, 0, 360);
    lv_arc_set_angles(s_compass_cal_ring, 0, 1);
    lv_obj_remove_style(s_compass_cal_ring, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(s_compass_cal_ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(s_compass_cal_ring, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_compass_cal_ring, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_compass_cal_ring, LIGHT, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(s_compass_cal_ring, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_compass_cal_ring, GREEN, LV_PART_INDICATOR);

    s_compass_cal_ball = lv_obj_create(parent_card);
    lv_obj_remove_style_all(s_compass_cal_ball);
    lv_obj_set_size(s_compass_cal_ball,
                    COMPASS_UI_CAL_BALL_SIZE,
                    COMPASS_UI_CAL_BALL_SIZE);
    lv_obj_set_style_radius(s_compass_cal_ball, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_compass_cal_ball, GREEN, 0);
    lv_obj_set_style_bg_opa(s_compass_cal_ball, LV_OPA_COVER, 0);
    s_compass_cal_ball_seeded = false;
    lv_obj_add_flag(s_compass_cal_ring, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_compass_cal_ball, LV_OBJ_FLAG_HIDDEN);

    s_compass_cal_motion_lbl = lv_label_create(parent_card);
    lv_obj_set_style_text_font(s_compass_cal_motion_lbl,
                               get_font(FONT_ID_BIG_TITLE),
                               0);
    lv_obj_set_style_text_color(s_compass_cal_motion_lbl, GREEN, 0);
    lv_obj_set_style_text_align(s_compass_cal_motion_lbl,
                                LV_TEXT_ALIGN_CENTER,
                                0);
    lv_obj_align(s_compass_cal_motion_lbl,
                 LV_ALIGN_CENTER,
                 0,
                 COMPASS_UI_CAL_MOTION_Y);
    lv_label_set_text(s_compass_cal_motion_lbl, "<       >");
    lv_obj_add_flag(s_compass_cal_motion_lbl, LV_OBJ_FLAG_HIDDEN);

    s_compass_cal_progress_bar = lv_bar_create(parent_card);
    lv_obj_set_size(s_compass_cal_progress_bar, 160, 10);
    lv_obj_align(s_compass_cal_progress_bar,
                 LV_ALIGN_CENTER,
                 0,
                 COMPASS_UI_CAL_PROGRESS_Y);
    /* 0.1% 分辨率由本地展示进度驱动，LVGL 自身动画保持关闭。 */
    lv_bar_set_range(s_compass_cal_progress_bar,
                     0,
                     80 * COMPASS_UI_CAL_PROGRESS_BAR_SCALE);
    lv_bar_set_value(s_compass_cal_progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_radius(s_compass_cal_progress_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_compass_cal_progress_bar, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_compass_cal_progress_bar,
                            LV_OPA_TRANSP,
                            LV_PART_MAIN);
    lv_obj_set_style_border_width(s_compass_cal_progress_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_compass_cal_progress_bar,
                                  GREEN,
                                  LV_PART_MAIN);
    lv_obj_set_style_border_opa(s_compass_cal_progress_bar,
                                LV_OPA_COVER,
                                LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_compass_cal_progress_bar,
                              GREEN,
                              LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_compass_cal_progress_bar,
                            LV_OPA_COVER,
                            LV_PART_INDICATOR);
    lv_obj_add_flag(s_compass_cal_progress_bar, LV_OBJ_FLAG_HIDDEN);

    for (uint8_t i = 0U; i < 16U; ++i)
    {
        const float angle = (-90.0f + ((float)i * 22.5f)) *
                            (3.14159265358979323846f / 180.0f);
        const lv_coord_t tick_x =
            (lv_coord_t)(cosf(angle) * COMPASS_UI_CAL_TICK_RADIUS);
        const lv_coord_t tick_y =
            (lv_coord_t)(sinf(angle) * COMPASS_UI_CAL_TICK_RADIUS) +
            COMPASS_UI_CAL_CENTER_Y;
        s_compass_cal_sector_ticks[i] = lv_obj_create(parent_card);
        lv_obj_remove_style_all(s_compass_cal_sector_ticks[i]);
        lv_obj_set_size(s_compass_cal_sector_ticks[i], 6, 6);
        lv_obj_set_style_radius(s_compass_cal_sector_ticks[i],
                                LV_RADIUS_CIRCLE,
                                0);
        lv_obj_set_style_bg_color(s_compass_cal_sector_ticks[i], GREEN, 0);
        lv_obj_set_style_bg_opa(s_compass_cal_sector_ticks[i], LV_OPA_20, 0);
        lv_obj_align(s_compass_cal_sector_ticks[i],
                     LV_ALIGN_CENTER,
                     tick_x,
                     tick_y);
        lv_obj_add_flag(s_compass_cal_sector_ticks[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* 俯仰反馈独立于 heading：角度被安全 hold 时，用户仍能确认头部动作已识别。 */
    s_heading_pitch_lbl = lv_label_create(parent_card);
    lv_obj_set_style_text_font(s_heading_pitch_lbl, get_font(FONT_ID_SMALL), 0);
    lv_obj_set_style_text_color(s_heading_pitch_lbl, GREEN, 0);
    lv_obj_set_style_text_align(s_heading_pitch_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_heading_pitch_lbl,
                 LV_ALIGN_CENTER,
                 0,
                 COMPASS_UI_PITCH_CENTER_Y);
    lv_obj_add_flag(s_heading_pitch_lbl, LV_OBJ_FLAG_HIDDEN);
    s_compass_pitch_seeded = false;
    s_compass_pitch_visible = false;
    s_compass_pitch_neutral_deg = 0.0f;
    s_compass_pitch_neutral_samples = 0U;
    s_compass_pitch_neutral_valid = false;
    compass_update_pitch_indicator();

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
    s_compass_heading_available = (s_compass_vm_cache.heading_available != 0U);
    s_compass_has_valid_heading = s_compass_heading_available;
    s_compass_unavailable_drawn = false;
    s_compass_page_was_visible = false;
    compass_display_seed(s_compass_vm_cache.heading);
    render_compass_custom(parent);
    compass_anim_timer_ensure();
}

void card_compass_refresh_heading_vm(const ui_vm_compass_t *vm, bool force_refresh)
{
    bool had_valid_heading;

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

    had_valid_heading = s_compass_has_valid_heading;
    s_compass_heading_available = (s_compass_vm_cache.heading_available != 0U);
    if (s_compass_heading_available)
    {
        s_compass_has_valid_heading = true;
    }
    if (!s_compass_heading_available)
    {
        if (!s_compass_unavailable_drawn || force_refresh)
        {
            /*
             * 头戴罗盘在极端俯仰/短时无效时，UI 不能清成 "---"。
             * 此处只冻结显示层目标，继续显示最后一次可展示角度；算法层
             * available=false 仍保留给日志和 BLE 诊断。静态冻结画面只刷新一次，
             * 避免 33ms fast path 重复 invalidate 罗盘卷尺。
             */
            if (!s_compass_display_seeded)
            {
                compass_display_seed(s_compass_vm_cache.heading);
            }
            compass_display_clear_pending_target();
            compass_display_clear_large_jump();
            s_compass_display_target_valid = false;
            s_compass_display_velocity_dps = 0.0f;
            s_compass_display_last_step_deg = 0.0f;
            compass_update_heading_label_from_display(true);
            if (compass_obj_is_valid(&s_compass_tape_obj))
            {
                lv_obj_invalidate(s_compass_tape_obj);
            }
            compass_update_hint_label();
            s_compass_unavailable_drawn = true;
        }
        return;
    }

    s_compass_unavailable_drawn = false;
    if (force_refresh || !s_compass_display_seeded || !had_valid_heading)
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
