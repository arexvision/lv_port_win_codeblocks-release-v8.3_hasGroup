#include "arex_data.h"
#include "arex_alarm.h"
#include <math.h>
#include <string.h>
#include "stdio.h"
/* card_plan.c 中的减压站序列全局数组（由减压引擎写入，UI 消费） */
extern arex_deco_stop_t g_deco_stops[MAX_DECO_STOPS];
extern uint16_t         g_deco_stop_count;

#ifdef PC_SIMULATOR
#include "lvgl.h"
#endif

/* =========================================================
 * Data Bus Setter 实现 — 硬件/模拟层专用
 * 铁律：仅更新数值 + 打脏标记，绝不碰 LVGL！
 * ========================================================= */

/* =========================================================
 * 模拟/业务告警阈值集中配置
 *
 * 说明：
 * - 当前第一版不接 NVDS/APP 持久化，这里就是默认阈值入口。
 * - 后续如果设置菜单要支持用户自定义阈值，优先把这些宏改为
 *   g_sys_config 或用户配置字段，而不是在 setter 中分散硬编码。
 * - UI 速率图标和上升过快告警使用两套窗口：图标追求灵敏，
 *   告警追求稳定，避免模拟数据轻微抖动导致误报。
 * ========================================================= */
#define AREX_DEPTH_UI_RATE_WINDOW_SIZE     3U       /* 速度图标采样窗口：越小越灵敏 */
#define AREX_DEPTH_ALARM_RATE_WINDOW_SIZE  4U       /* 上升过快告警采样窗口：越大越稳 */
#define AREX_DEPTH_DISPLAY_DEBOUNCE_M      0.05f    /* 深度显示防抖：小于 0.05m 不刷新数字 */
#define AREX_ASCENT_RATE_UI_EPSILON        0.2f     /* 速度图标变化阈值：低于该值认为无明显变化 */
#define AREX_ASCENT_RATE_UI_SMOOTH_ALPHA   0.75f    /* 速度图标平滑系数：越大响应越快 */
#define AREX_ASCENT_ALARM_THRESHOLD_MPM    10.0f    /* CRIT.ASCENT_RATE：上升速度 >= 10m/min 触发 */
#define AREX_ASCENT_ALARM_RELEASE_MPM      8.0f     /* CRIT.ASCENT_RATE：回落到 8m/min 以下解除 */
#define AREX_ASCENT_ALARM_HOLD_SAMPLES     2U       /* CRIT.ASCENT_RATE：连续满足的采样次数 */

#define AREX_ALARM_DEPTH_LIMIT_M           40.0f        /* WARN.DEPTH_LIMIT：深度 >= 40m */
#define AREX_ALARM_TIME_LIMIT_S            (60U * 60U)  /* WARN.TIME_LIMIT：潜水时间 >= 60min */
#define AREX_ALARM_NDL_LOW_MIN             5            /* WARN.NDL_LOW：NDL < 5min */
#define AREX_ALARM_TURN_PRESSURE_BAR       100.0f       /* WARN.TANK_TURN：任一有效 POD < 100bar */
#define AREX_ALARM_TANK_EMPTY_BAR          50.0f        /* CRIT.TANK_EMPTY：任一有效 POD < 50bar */
#define AREX_ALARM_SIDEMOUNT_DIFF_BAR      50.0f        /* WARN.SIDEMOUNT_DIFF：双瓶压差 >= 50bar */
#define AREX_ALARM_BATTERY_LOW_PCT         20.0f        /* WARN.BATTERY_LOW：电量 < 20% */
#define AREX_ALARM_BATTERY_DEAD_PCT        5.0f         /* CRIT.BATTERY_DEAD：电量 < 5% */
#define AREX_ALARM_PO2_CRIT_BAR            1.6f         /* CRIT.PO2_MAX：PPO2 > 1.6bar */
#define AREX_ALARM_CEILING_MARGIN_M        0.6f         /* CRIT.CEIL_BROKEN：浅于 ceiling 0.6m */
#define AREX_ALARM_SAFETY_BROKEN_M         2.4f         /* WARN.SAFETY_BROKEN：安全停留时浅于 2.4m */
#define AREX_ALARM_CNS_HIGH_PCT            80U          /* WARN.CNS_HIGH：CNS >= 80% */
#define AREX_ALARM_OTU_HIGH                250U         /* WARN.OTU_HIGH：OTU >= 250 */

#define AREX_ALARM_GAS_SWITCH_ASCENT_MPM   0.5f         /* INFO.GAS_SWITCH：只在上升趋势中提示更优气体 */

typedef struct
{
    uint32_t timestamp_ms;
    float    depth_m;
} arex_depth_sample_t;

static arex_depth_sample_t _ui_depth_rate_window[AREX_DEPTH_UI_RATE_WINDOW_SIZE];
static arex_depth_sample_t _alarm_depth_rate_window[AREX_DEPTH_ALARM_RATE_WINDOW_SIZE];
static uint8_t             _ui_depth_rate_count = 0;
static uint8_t             _ui_depth_rate_head = 0;
static uint8_t             _alarm_depth_rate_count = 0;
static uint8_t             _alarm_depth_rate_head = 0;
static uint8_t             _ascent_alarm_over_limit_count = 0;
static float               _ui_rate_filtered_mpm = 0.0f;

/* 深度统计累计值 */
static float    _depth_sum = 0.0f;       /* 深度累计和 */
static uint32_t _depth_sample_count = 0;  /* 深度采样次数 */

/* 温度统计累计值 */
static float    _temp_sum = 0.0f;        /* 温度累计和 */
static uint32_t _temp_sample_count = 0;  /* 温度采样次数 */

static uint32_t arex_depth_now_ms(void)
{
#ifdef PC_SIMULATOR
    return lv_tick_get();
#else
    return rt_tick_get_millisecond();
#endif
}

static void arex_depth_rate_reset(void)
{
    memset(_ui_depth_rate_window, 0, sizeof(_ui_depth_rate_window));
    memset(_alarm_depth_rate_window, 0, sizeof(_alarm_depth_rate_window));
    _ui_depth_rate_count = 0;
    _ui_depth_rate_head = 0;
    _alarm_depth_rate_count = 0;
    _alarm_depth_rate_head = 0;
    _ascent_alarm_over_limit_count = 0;
    _ui_rate_filtered_mpm = 0.0f;
}

static void arex_depth_rate_push_sample(arex_depth_sample_t *window,
                                        uint8_t window_size,
                                        uint8_t *head,
                                        uint8_t *count,
                                        uint32_t now_ms,
                                        float depth_m)
{
    window[*head].timestamp_ms = now_ms;
    window[*head].depth_m = depth_m;
    *head = (uint8_t)((*head + 1U) % window_size);

    if (*count < window_size)
    {
        (*count)++;
    }
}

static float arex_depth_rate_estimate_mpm(const arex_depth_sample_t *window,
        uint8_t window_size,
        uint8_t count,
        uint8_t head)
{
    if (count < 3U)
    {
        return 0.0f;
    }

    float sum_t = 0.0f;
    float sum_d = 0.0f;
    float sum_tt = 0.0f;
    float sum_td = 0.0f;
    uint8_t oldest = (uint8_t)((head + window_size - count) % window_size);
    uint32_t base_ms = window[oldest].timestamp_ms;

    for (uint8_t i = 0; i < count; i++)
    {
        uint8_t idx = (uint8_t)((oldest + i) % window_size);
        float t_min = ((float)(window[idx].timestamp_ms - base_ms)) / 60000.0f;
        float depth_m = window[idx].depth_m;

        sum_t += t_min;
        sum_d += depth_m;
        sum_tt += t_min * t_min;
        sum_td += t_min * depth_m;
    }

    float n = (float)count;
    float denominator = n * sum_tt - sum_t * sum_t;
    if (fabsf(denominator) < 1e-6f)
    {
        return 0.0f;
    }

    return (n * sum_td - sum_t * sum_d) / denominator;
}

static float arex_depth_rate_estimate_latest_mpm(const arex_depth_sample_t *window,
        uint8_t window_size,
        uint8_t count,
        uint8_t head)
{
    if (count < 2U)
    {
        return 0.0f;
    }

    uint8_t newest = (uint8_t)((head + window_size - 1U) % window_size);
    uint8_t prev = (uint8_t)((head + window_size - 2U) % window_size);
    uint32_t dt_ms = window[newest].timestamp_ms - window[prev].timestamp_ms;
    if (dt_ms == 0U)
    {
        return 0.0f;
    }

    return (window[newest].depth_m - window[prev].depth_m) * 60000.0f / (float)dt_ms;
}

static float arex_alarm_active_ppo2(void)
{
    uint8_t active_idx = g_sensor_data.gas_active_idx;
    if (active_idx >= AREX_GAS_COUNT)
    {
        return 0.0f;
    }
    return g_sensor_data.ppo2[active_idx];
}

static void arex_alarm_eval_ppo2(void)
{
    float active_ppo2 = arex_alarm_active_ppo2();
    /* WARN.PO2_ELEVATED：优先使用系统设置的 PPO2 上限；未配置时按 1.4bar 默认安全线。 */
    float elevated_limit = (g_sys_config.mod_ppo2 > 0.1f) ? g_sys_config.mod_ppo2 : 1.4f;
    bool critical = (active_ppo2 > AREX_ALARM_PO2_CRIT_BAR);
    bool elevated = (!critical && active_ppo2 > elevated_limit);

    arex_alarm_set_active(AREX_ALARM_ID_CRIT_PO2_MAX, critical);
    arex_alarm_set_active(AREX_ALARM_ID_WARN_PO2_ELEVATED, elevated);
}

static void arex_alarm_eval_battery(void)
{
    bool dead = (g_sensor_data.battery_pct < AREX_ALARM_BATTERY_DEAD_PCT);
    bool low = (!dead && g_sensor_data.battery_pct < AREX_ALARM_BATTERY_LOW_PCT);

    arex_alarm_set_active(AREX_ALARM_ID_CRIT_BATTERY_DEAD, dead);
    arex_alarm_set_active(AREX_ALARM_ID_WARN_BATTERY_LOW, low);
}

static void arex_alarm_eval_pod(void)
{
    bool pod1_valid = (g_sensor_data.pod1_bar > 0.0f);
    bool pod2_valid = (g_sensor_data.pod2_bar > 0.0f);
    bool tank_empty = false;
    bool tank_turn = false;
    bool sidemount_diff = false;

    if (pod1_valid)
    {
        tank_empty = tank_empty || (g_sensor_data.pod1_bar < AREX_ALARM_TANK_EMPTY_BAR);
        tank_turn = tank_turn || (g_sensor_data.pod1_bar < AREX_ALARM_TURN_PRESSURE_BAR);
    }
    if (pod2_valid)
    {
        tank_empty = tank_empty || (g_sensor_data.pod2_bar < AREX_ALARM_TANK_EMPTY_BAR);
        tank_turn = tank_turn || (g_sensor_data.pod2_bar < AREX_ALARM_TURN_PRESSURE_BAR);
    }

    tank_turn = tank_turn && !tank_empty;
    if (pod1_valid && pod2_valid)
    {
        sidemount_diff = (fabsf(g_sensor_data.pod1_bar - g_sensor_data.pod2_bar) >=
                          AREX_ALARM_SIDEMOUNT_DIFF_BAR);
    }

    arex_alarm_set_active(AREX_ALARM_ID_CRIT_TANK_EMPTY, tank_empty);
    arex_alarm_set_active(AREX_ALARM_ID_WARN_TANK_TURN, tank_turn);
    arex_alarm_set_active(AREX_ALARM_ID_WARN_SIDEMOUNT_DIFF, sidemount_diff);
}

static void arex_alarm_eval_depth_limit(void)
{
    arex_alarm_set_active(AREX_ALARM_ID_WARN_DEPTH_LIMIT,
                          g_sensor_data.depth >= AREX_ALARM_DEPTH_LIMIT_M);
}

static void arex_alarm_eval_time_limit(void)
{
    arex_alarm_set_active(AREX_ALARM_ID_WARN_TIME_LIMIT,
                          g_sensor_data.dive_time_s >= AREX_ALARM_TIME_LIMIT_S);
}

static void arex_alarm_eval_ndl(void)
{
    arex_alarm_set_active(AREX_ALARM_ID_WARN_NDL_LOW,
                          g_sensor_data.stop_type == AREX_STOP_NONE &&
                          g_sensor_data.ndl >= 0 &&
                          g_sensor_data.ndl < AREX_ALARM_NDL_LOW_MIN);
}

static void arex_alarm_eval_oxygen_toxicity(void)
{
    arex_alarm_set_active(AREX_ALARM_ID_WARN_CNS_HIGH,
                          g_sensor_data.cns_pct >= AREX_ALARM_CNS_HIGH_PCT);
    arex_alarm_set_active(AREX_ALARM_ID_WARN_OTU_HIGH,
                          g_sensor_data.otu >= AREX_ALARM_OTU_HIGH);
}

static void arex_alarm_eval_deco_limits(void)
{
    bool ceiling_broken = (g_sensor_data.stop_type == AREX_STOP_DECO &&
                           g_sensor_data.ceiling_m > 0.0f &&
                           g_sensor_data.depth < (g_sensor_data.ceiling_m - AREX_ALARM_CEILING_MARGIN_M));
    bool safety_broken = (g_sensor_data.stop_type == AREX_STOP_SAFETY &&
                          g_sensor_data.depth < AREX_ALARM_SAFETY_BROKEN_M);

    arex_alarm_set_active(AREX_ALARM_ID_CRIT_CEIL_BROKEN, ceiling_broken);
    arex_alarm_set_active(AREX_ALARM_ID_WARN_SAFETY_BROKEN, safety_broken);
}

static void arex_alarm_eval_gas_switch(void)
{
    static bool s_gas_switch_condition = false;
    bool available = false;
    uint8_t active_idx = g_sensor_data.gas_active_idx;
    uint8_t active_o2 = (active_idx < AREX_GAS_COUNT) ?
                        g_sensor_data.gas_slot_o2_pct[active_idx] : 0U;

    uint8_t gas_count = g_sensor_data.gas_slot_count;
    if (gas_count == 0U || gas_count > AREX_GAS_COUNT)
    {
        gas_count = AREX_GAS_COUNT;
    }

    if (g_sensor_data.depth > 0.1f &&
            g_sensor_data.ascent_rate > AREX_ALARM_GAS_SWITCH_ASCENT_MPM &&
            active_idx < gas_count)
    {
        for (uint8_t i = 0; i < gas_count; i++)
        {
            if (i == active_idx)
            {
                continue;
            }
            if (g_sensor_data.gas_slot_o2_pct[i] > active_o2 &&
                    g_sensor_data.gas_slot_mod_m[i] + 0.05f >= g_sensor_data.depth)
            {
                available = true;
                break;
            }
        }
    }

    if (available != s_gas_switch_condition)
    {
        arex_alarm_set_active(AREX_ALARM_ID_INFO_GAS_SWITCH, available);
    }
    s_gas_switch_condition = available;
}

static void arex_alarm_eval_safety_stop_info(void)
{
    static bool s_safety_stop_condition = false;
    bool active = (g_sensor_data.stop_type == AREX_STOP_SAFETY);

    if (active != s_safety_stop_condition)
    {
        arex_alarm_set_active(AREX_ALARM_ID_INFO_SAFETY_STOP, active);
    }
    s_safety_stop_condition = active;
}

void arex_data_init(void)
{
    memset(&g_sensor_data, 0, sizeof(g_sensor_data));
    arex_depth_rate_reset();
    arex_alarm_init();
    _depth_sum = 0.0f;
    _depth_sample_count = 0;
    _temp_sum = 0.0f;
    _temp_sample_count = 0;

    g_sensor_data.gas_active_idx = 0;
    g_sensor_data.gas_slot_count = 1;
    strncpy(g_sensor_data.gas_name, "AIR", sizeof(g_sensor_data.gas_name) - 1);

    strncpy(g_sensor_data.gas_slot_name[0], "AIR", sizeof(g_sensor_data.gas_slot_name[0]) - 1);
    g_sensor_data.gas_slot_o2_pct[0] = 21;
    g_sensor_data.gas_slot_he_pct[0] = 0;
    g_sensor_data.gas_slot_mod_m[0] = 56.0f;

    strncpy(g_sensor_data.gas_slot_name[1], "NX 32", sizeof(g_sensor_data.gas_slot_name[1]) - 1);
    g_sensor_data.gas_slot_o2_pct[1] = 32;
    g_sensor_data.gas_slot_he_pct[1] = 0;
    g_sensor_data.gas_slot_mod_m[1] = 34.0f;

    strncpy(g_sensor_data.gas_slot_name[2], "O2 100%", sizeof(g_sensor_data.gas_slot_name[2]) - 1);
    g_sensor_data.gas_slot_o2_pct[2] = 100;
    g_sensor_data.gas_slot_he_pct[2] = 0;
    g_sensor_data.gas_slot_mod_m[2] = 6.0f;

    /* 减压站预测数据初始化（仅初始化节数，数据本身由减压引擎填充） */
    g_deco_stop_count = 0;
}

void arex_bus_raise_alarm(arex_alarm_level_t level,
                          const char *text,
                          arex_widget_id_t target)
{
    (void)arex_alarm_raise_custom(level, text, target);
}

void arex_bus_set_depth(float depth_m)
{
    uint32_t now_ms = arex_depth_now_ms();
    float prev_ui_rate = g_sensor_data.ascent_rate;
    bool prev_is_moving = fabsf(prev_ui_rate) >= AREX_RATE_STILL_THRESHOLD;
    float ui_depth_rate_mpm;
    float alarm_depth_rate_mpm;

    arex_depth_rate_push_sample(_ui_depth_rate_window,
                                AREX_DEPTH_UI_RATE_WINDOW_SIZE,
                                &_ui_depth_rate_head,
                                &_ui_depth_rate_count,
                                now_ms,
                                depth_m);
    arex_depth_rate_push_sample(_alarm_depth_rate_window,
                                AREX_DEPTH_ALARM_RATE_WINDOW_SIZE,
                                &_alarm_depth_rate_head,
                                &_alarm_depth_rate_count,
                                now_ms,
                                depth_m);

    ui_depth_rate_mpm = arex_depth_rate_estimate_latest_mpm(_ui_depth_rate_window,
                        AREX_DEPTH_UI_RATE_WINDOW_SIZE,
                        _ui_depth_rate_count,
                        _ui_depth_rate_head);
    alarm_depth_rate_mpm = arex_depth_rate_estimate_mpm(_alarm_depth_rate_window,
                           AREX_DEPTH_ALARM_RATE_WINDOW_SIZE,
                           _alarm_depth_rate_count,
                           _alarm_depth_rate_head);

    /* UI 通道更关注连续观感：短窗口 + 轻量平滑 */
    {
        float ui_rate_target_mpm = -ui_depth_rate_mpm;  /* 正=上升，负=下潜 */
        float ui_rate_mpm;
        bool current_is_moving;

        _ui_rate_filtered_mpm += AREX_ASCENT_RATE_UI_SMOOTH_ALPHA *
                                 (ui_rate_target_mpm - _ui_rate_filtered_mpm);
        ui_rate_mpm = _ui_rate_filtered_mpm;

        if (fabsf(ui_rate_mpm) < AREX_ASCENT_RATE_UI_EPSILON)
        {
            ui_rate_mpm = 0.0f;
            _ui_rate_filtered_mpm = 0.0f;
        }

        g_sensor_data.ascent_rate = ui_rate_mpm;
        current_is_moving = fabsf(ui_rate_mpm) >= AREX_RATE_STILL_THRESHOLD;

        if ((fabsf(ui_rate_mpm - prev_ui_rate) >= AREX_ASCENT_RATE_UI_EPSILON) ||
                (current_is_moving != prev_is_moving))
        {
            g_sensor_data.dirty_mask |= DIRTY_DEPTH;
        }
    }

    /* 告警通道更关注误报控制：长窗口 + 迟滞 + 持续判定 */
    if (alarm_depth_rate_mpm <= -AREX_ASCENT_ALARM_THRESHOLD_MPM)
    {
        if (_ascent_alarm_over_limit_count < 0xFFU)
        {
            _ascent_alarm_over_limit_count++;
        }
    }
    else if (alarm_depth_rate_mpm > -AREX_ASCENT_ALARM_RELEASE_MPM)
    {
        _ascent_alarm_over_limit_count = 0;
    }

    if (_ascent_alarm_over_limit_count >= AREX_ASCENT_ALARM_HOLD_SAMPLES)
    {
        arex_alarm_set_active(AREX_ALARM_ID_CRIT_ASCENT_RATE, true);
    }
    else if (alarm_depth_rate_mpm > -AREX_ASCENT_ALARM_RELEASE_MPM)
    {
        arex_alarm_set_active(AREX_ALARM_ID_CRIT_ASCENT_RATE, false);
    }

    /* 深度数值显示继续保留轻量防抖，避免数字末位来回跳 */
    if (fabsf(g_sensor_data.depth - depth_m) > AREX_DEPTH_DISPLAY_DEBOUNCE_M)
    {
        g_sensor_data.depth = depth_m;
        g_sensor_data.dirty_mask |= DIRTY_DEPTH;
        /* 轨迹+减压站图表需要刷新 */
        g_sensor_data.dirty_mask |= DIRTY_TRAJECTORY;

        /* 统计计算：最大深度 + 平均深度 */
        if (depth_m > g_sensor_data.max_depth)
        {
            g_sensor_data.max_depth = depth_m;
        }
        _depth_sum += depth_m;
        _depth_sample_count++;
        g_sensor_data.avg_depth = (_depth_sample_count > 0) ? (_depth_sum / _depth_sample_count) : 0.0f;

        arex_alarm_eval_depth_limit();
        arex_alarm_eval_deco_limits();
        arex_alarm_eval_gas_switch();
    }
}

void arex_bus_set_ndl(int16_t ndl_min)
{
    if (g_sensor_data.ndl != ndl_min)
    {
        g_sensor_data.ndl = ndl_min;
        g_sensor_data.dirty_mask |= DIRTY_NDL;
        arex_alarm_eval_ndl();
    }
}

void arex_bus_set_tts(uint16_t tts_min)
{
    if (g_sensor_data.tts != tts_min)
    {
        g_sensor_data.tts = tts_min;
        g_sensor_data.dirty_mask |= DIRTY_TTS;
    }
}

void arex_bus_set_pod(uint8_t pod_idx, float bar)
{
    if (pod_idx == 0 && g_sensor_data.pod1_bar != bar)
    {
        g_sensor_data.pod1_bar = bar;
        g_sensor_data.dirty_mask |= DIRTY_POD;
    }
    else if (pod_idx == 1 && g_sensor_data.pod2_bar != bar)
    {
        g_sensor_data.pod2_bar = bar;
        g_sensor_data.dirty_mask |= DIRTY_POD;
    }
    arex_alarm_eval_pod();
}

void arex_bus_set_battery(float pct)
{
    static bool s_battery_initialized = false;

    if (pct < 0.0f)
    {
        pct = 0.0f;
    }
    else if (pct > 100.0f)
    {
        pct = 100.0f;
    }

    if (!s_battery_initialized || fabsf(g_sensor_data.battery_pct - pct) > 0.1f)
    {
        s_battery_initialized = true;
        g_sensor_data.battery_pct = pct;
        g_sensor_data.dirty_mask |= DIRTY_BATT;
        arex_alarm_eval_battery();
    }
}

void arex_bus_set_heading(uint16_t heading_deg)
{
    if (g_sensor_data.heading != heading_deg)
    {
        g_sensor_data.heading = heading_deg;
        g_sensor_data.dirty_mask |= DIRTY_HEADING;
    }
}

void arex_bus_set_dive_time(uint32_t dive_s)
{
    if (g_sensor_data.dive_time_s != dive_s)
    {
        g_sensor_data.dive_time_s = dive_s;
        g_sensor_data.dirty_mask |= DIRTY_DIVE_TIME;
        arex_alarm_eval_time_limit();
        /* 潜水时间推进，图表的 NOW 点会随之右移 */
        g_sensor_data.dirty_mask |= DIRTY_TRAJECTORY;
    }
}

void arex_bus_set_surface_time(uint32_t surface_s)
{
    if (g_sensor_data.surface_time_s != surface_s)
    {
        g_sensor_data.surface_time_s = surface_s;
        g_sensor_data.dirty_mask |= DIRTY_DIVE_TIME;
    }
}

void arex_bus_set_ppo2(uint8_t sensor_idx, float ppo2_val)
{
    if (sensor_idx < AREX_GAS_COUNT && g_sensor_data.ppo2[sensor_idx] != ppo2_val)
    {
        g_sensor_data.ppo2[sensor_idx] = ppo2_val;
        g_sensor_data.dirty_mask |= DIRTY_PPO2;
        arex_alarm_eval_ppo2();
    }
}

void arex_bus_set_gas(uint8_t gas_idx, const char *gas_name)
{
    uint8_t gas_count = g_sensor_data.gas_slot_count;
    if (gas_count == 0U || gas_count > AREX_GAS_COUNT)
    {
        gas_count = AREX_GAS_COUNT;
    }
    if (gas_idx >= gas_count)
    {
        gas_idx = 0;
    }

    if (g_sensor_data.gas_active_idx != gas_idx)
    {
        g_sensor_data.gas_active_idx = gas_idx;
    }
    if (gas_name != NULL && strncmp(g_sensor_data.gas_name, gas_name, 15) != 0)
    {
        strncpy(g_sensor_data.gas_name, gas_name, 15);
        g_sensor_data.gas_name[15] = '\0';
    }
    g_sensor_data.dirty_mask |= DIRTY_GAS;
    arex_alarm_eval_ppo2();
    arex_alarm_eval_gas_switch();
}

void arex_bus_set_gas_slot_count(uint8_t count)
{
    if (count == 0U)
    {
        count = 1U;
    }
    if (count > AREX_GAS_COUNT)
    {
        count = AREX_GAS_COUNT;
    }

    if (g_sensor_data.gas_slot_count != count)
    {
        g_sensor_data.gas_slot_count = count;
        if (g_sensor_data.gas_active_idx >= count)
        {
            g_sensor_data.gas_active_idx = 0;
            snprintf(g_sensor_data.gas_name,
                     sizeof(g_sensor_data.gas_name),
                     "%s",
                     g_sensor_data.gas_slot_name[0][0] ? g_sensor_data.gas_slot_name[0] : "AIR");
        }
        g_sensor_data.dirty_mask |= DIRTY_GAS | DIRTY_PPO2;
        arex_alarm_eval_ppo2();
        arex_alarm_eval_gas_switch();
    }
}

void arex_bus_set_gas_slot(uint8_t gas_idx, const char *gas_name,
                           uint8_t o2_pct, uint8_t he_pct, float mod_m)
{
    if (gas_idx >= AREX_GAS_COUNT)
    {
        return;
    }

    bool changed = false;

    if (gas_name != NULL && strncmp(g_sensor_data.gas_slot_name[gas_idx], gas_name, 15) != 0)
    {
        strncpy(g_sensor_data.gas_slot_name[gas_idx], gas_name, 15);
        g_sensor_data.gas_slot_name[gas_idx][15] = '\0';
        changed = true;
    }
    if (g_sensor_data.gas_slot_o2_pct[gas_idx] != o2_pct)
    {
        g_sensor_data.gas_slot_o2_pct[gas_idx] = o2_pct;
        changed = true;
    }
    if (g_sensor_data.gas_slot_he_pct[gas_idx] != he_pct)
    {
        g_sensor_data.gas_slot_he_pct[gas_idx] = he_pct;
        changed = true;
    }
    if (fabsf(g_sensor_data.gas_slot_mod_m[gas_idx] - mod_m) > 0.05f)
    {
        g_sensor_data.gas_slot_mod_m[gas_idx] = mod_m;
        changed = true;
    }

    if (changed)
    {
        g_sensor_data.dirty_mask |= DIRTY_GAS;
        arex_alarm_eval_gas_switch();
    }
}

void arex_bus_set_deco(int16_t stop_m, uint8_t stop_min)
{
    if (g_sensor_data.next_stop_m != stop_m || g_sensor_data.next_stop_min != stop_min)
    {
        g_sensor_data.next_stop_m = stop_m;
        g_sensor_data.next_stop_min = stop_min;
        g_sensor_data.dirty_mask |= DIRTY_TRAJECTORY;
    }
}

void arex_bus_set_cns(uint8_t cns_pct)
{
    if (g_sensor_data.cns_pct != cns_pct)
    {
        g_sensor_data.cns_pct = cns_pct;
        g_sensor_data.dirty_mask |= DIRTY_CNS;
        arex_alarm_eval_oxygen_toxicity();
    }
}

void arex_bus_set_otu(uint16_t otu_val)
{
    if (g_sensor_data.otu != otu_val)
    {
        g_sensor_data.otu = otu_val;
        g_sensor_data.dirty_mask |= DIRTY_OTU;
        arex_alarm_eval_oxygen_toxicity();
    }
}

void arex_bus_set_gf99(float gf99)
{
    if (fabsf(g_sensor_data.gf99 - gf99) > 0.1f)
    {
        g_sensor_data.gf99 = gf99;
        g_sensor_data.dirty_mask |= DIRTY_CNS;
    }
}

void arex_bus_set_surf_gf(float surf_gf)
{
    if (fabsf(g_sensor_data.surf_gf - surf_gf) > 0.1f)
    {
        g_sensor_data.surf_gf = surf_gf;
        g_sensor_data.dirty_mask |= DIRTY_CNS;
    }
}

/* =========================================================
 * 临界区保护的数组写入 — 防止多线程数据撕裂
 *
 * 铁律：> 32bit 的数据块拷贝必须包在关中断临界区里。
 *   - PC 仿真器: rt_hw_interrupt_disable/enable 替换为空操作
 *   - 真机 RT-Thread: 触发底层 cpsr 关中断，耗时 < 0.1us
 * ========================================================= */

/* 16 组织舱饱和度数组写入（16 字节，必须包临界区） */
void arex_bus_set_tissues(const uint8_t tissue_pct[16])
{
    rt_base_t level = rt_hw_interrupt_disable();
    memcpy(g_sensor_data.tissue_pct, tissue_pct, 16);
    g_sensor_data.dirty_mask |= DIRTY_TISSUES;
    rt_hw_interrupt_enable(level);
}

/* 完整减压站序列写入（可变长度，必须包临界区） */
void arex_bus_set_deco_plan(const arex_deco_stop_t *stops, uint8_t count)
{
    if (count > MAX_DECO_STOPS)
    {
        count = MAX_DECO_STOPS;
    }
    rt_base_t level = rt_hw_interrupt_disable();
    g_deco_stop_count = count;
    if (count > 0 && stops != NULL)
    {
        memcpy(g_deco_stops, stops, count * sizeof(arex_deco_stop_t));
    }
    g_sensor_data.dirty_mask |= DIRTY_TRAJECTORY;
    rt_hw_interrupt_enable(level);
}

uint32_t arex_bus_take_dirty(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    uint32_t mask = g_sensor_data.dirty_mask;
    g_sensor_data.dirty_mask = DIRTY_NONE;
    rt_hw_interrupt_enable(level);
    return mask;
}

void arex_bus_requeue_dirty(uint32_t mask)
{
    if (mask == DIRTY_NONE)
    {
        return;
    }

    rt_base_t level = rt_hw_interrupt_disable();
    g_sensor_data.dirty_mask |= mask;
    rt_hw_interrupt_enable(level);
}

void arex_bus_clear_all_dirty(void)
{
    g_sensor_data.dirty_mask = DIRTY_NONE;
}

void arex_bus_set_temperature(float temp_c)
{
    if (fabsf(g_sensor_data.temperature_c - temp_c) > 0.1f)
    {
        g_sensor_data.temperature_c = temp_c;
        g_sensor_data.dirty_mask |= DIRTY_TEMP;

        /* 统计计算：最低温度 + 平均温度 */
        if (_temp_sample_count == 0 || temp_c < g_sensor_data.min_temp)
        {
            g_sensor_data.min_temp = temp_c;
        }
        _temp_sum += temp_c;
        _temp_sample_count++;
        g_sensor_data.avg_temp = (_temp_sample_count > 0) ? (_temp_sum / _temp_sample_count) : 0.0f;
    }
}

void arex_bus_set_ui_layout(const arex_ble_ui_sync_payload_t *payload)
{
    printf("[BUS] arex_bus_set_ui_layout called, version=0x%02X\r\n", payload ? payload->version : 0);

    if (payload == NULL || payload->version != AREX_BLE_CFG_VERSION)
    {
        printf("[BUS] REJECTED: payload=%p, version=0x%02X\r\n", payload, payload ? payload->version : 0);
        return;
    }

    /* 临界区保护，防止 UI 任务在中途读到撕裂的数据 */
#ifdef PC_SIMULATOR
    volatile rt_base_t level = 0;
#else
    rt_base_t level = rt_hw_interrupt_disable();
#endif

    /* 1. 兼容旧协议：旧 payload 只有 8 个 card_order 槽位，不能按新运行时数组长度整块 memcpy */
    for (size_t i = 0; i < sizeof(g_sys_config.card_order); i++)
    {
        g_sys_config.card_order[i] = CARD_ID_UNUSED;
    }
    g_sys_config.card_order[CARD_POS_INFO] = CARD_ID_INFO;
    g_sys_config.card_order[CARD_POS_SETUP] = CARD_ID_SETUP;
    {
        uint8_t dynamic_pos = CARD_POS_DYNAMIC_FIRST;

        for (int i = 0; i < 8 && dynamic_pos < CARD_POS_SETUP; i++)
        {
            uint8_t card_id = payload->card_order[i];

            if (card_id == CARD_ID_UNUSED)
            {
                continue;
            }

            g_sys_config.card_order[dynamic_pos++] = card_id;
        }
    }

    /* 2. 映射左侧 2x7 锚点配置到 g_sys_config */
    memset(g_sys_config.left_widgets, 0, sizeof(g_sys_config.left_widgets));
    g_sys_config.left_widget_count = (payload->left_count > AREX_LEFT_MAX_WIDGETS)
                                     ? AREX_LEFT_MAX_WIDGETS
                                     : payload->left_count;
    for (int i = 0; i < g_sys_config.left_widget_count; i++)
    {
        g_sys_config.left_widgets[i].widget_id = (arex_widget_id_t)payload->left_widgets[i].widget_id;
        g_sys_config.left_widgets[i].x         = payload->left_widgets[i].x;
        g_sys_config.left_widgets[i].y         = payload->left_widgets[i].y;
    }

    /* 3. 兼容旧协议：单张 5F 配置映射到 custom_cards[0] */
    memset(g_sys_config.custom_cards, 0, sizeof(g_sys_config.custom_cards));
    memset(g_sys_config.custom_card_slot, 0xFF, sizeof(g_sys_config.custom_card_slot));
    g_sys_config.custom_card_count = 0;

    if (payload->custom_5f_count > 0)
    {
        g_sys_config.custom_card_count = 1;
        /* 在 card_order 中查找 CUSTOM_GRID 卡片的位置，设置正确的 slot 映射 */
        for (uint8_t pos = CARD_POS_DYNAMIC_FIRST; pos < CARD_POS_SETUP; pos++)
        {
            if (g_sys_config.card_order[pos] == CARD_ID_CUSTOM_GRID)
            {
                g_sys_config.custom_card_slot[pos] = 0;
                break;
            }
        }
        g_sys_config.custom_cards[0].widget_count = (payload->custom_5f_count > AREX_5F_MAX_WIDGETS)
            ? AREX_5F_MAX_WIDGETS
            : payload->custom_5f_count;
    }
    for (int i = 0; i < g_sys_config.custom_cards[0].widget_count; i++)
    {
        g_sys_config.custom_cards[0].widgets[i].widget_id = (arex_widget_id_t)payload->custom_5f_widgets[i].widget_id;
        g_sys_config.custom_cards[0].widgets[i].x = payload->custom_5f_widgets[i].c;  /* 列 -> x */
        g_sys_config.custom_cards[0].widgets[i].y = payload->custom_5f_widgets[i].r;  /* 行 -> y */
    }

    /* 4. 打上终极脏标记，通知 UI 推倒重建 */
    g_sensor_data.dirty_mask |= DIRTY_UI_LAYOUT;
    printf("[BUS] DIRTY_UI_LAYOUT set, dirty_mask=0x%08X\r\n", g_sensor_data.dirty_mask);

#ifdef PC_SIMULATOR
    (void)level;
#else
    rt_hw_interrupt_enable(level);
#endif
}

// void arex_bus_set_device_status(bool strobe_on, bool flashlight_on, uint8_t cylinder_count)
// {
//     if (g_sensor_data.strobe_on != strobe_on ||
//         g_sensor_data.flashlight_on != flashlight_on ||
//         g_sensor_data.cylinder_count != cylinder_count) {
//         g_sensor_data.strobe_on = strobe_on;
//         g_sensor_data.flashlight_on = flashlight_on;
//         g_sensor_data.cylinder_count = cylinder_count;
//         g_sensor_data.dirty_mask |= DIRTY_UI_LAYOUT;
//     }
// }

void arex_bus_toggle_layout_order(void)
{
    g_sys_config.layout_order = (g_sys_config.layout_order == AREX_ORDER_NORMAL)
                                ? AREX_ORDER_REVERSE
                                : AREX_ORDER_NORMAL;
    g_sensor_data.dirty_mask |= DIRTY_UI_LAYOUT;
}

void arex_bus_toggle_theme(void)
{
    g_sys_config.theme_mode = (g_sys_config.theme_mode == AREX_THEME_TECH)
                              ? AREX_THEME_CLASSIC
                              : AREX_THEME_TECH;
    g_sensor_data.dirty_mask |= DIRTY_UI_LAYOUT;
}

void arex_bus_toggle_dots_position(void)
{
    static const uint8_t seq[] = { AREX_DOTS_RIGHT, AREX_DOTS_LEFT, AREX_DOTS_BOTTOM, AREX_DOTS_NONE };
    static uint8_t idx = 0;
    idx = (idx + 1) % (sizeof(seq) / sizeof(seq[0]));
    g_sys_config.dots_position = seq[idx];
    g_sensor_data.dirty_mask |= DIRTY_UI_LAYOUT;
}

void arex_bus_toggle_compass_style(void)
{
    static const uint8_t seq[] = { AREX_COMPASS_CLASSIC, AREX_COMPASS_AERO, AREX_COMPASS_SUB };
    static uint8_t idx = 0;
    idx = (idx + 1) % (sizeof(seq) / sizeof(seq[0]));
    g_sys_config.compass_style = seq[idx];
    g_sensor_data.dirty_mask |= DIRTY_UI_LAYOUT;
}

void arex_bus_toggle_sep_style(void)
{
    static const uint8_t seq[] = { AREX_SEP_NONE, AREX_SEP_SOLID, AREX_SEP_DASHED, AREX_SEP_DOTTED };
    static uint8_t idx = 0;
    idx = (idx + 1) % (sizeof(seq) / sizeof(seq[0]));
    g_sys_config.sep_style = seq[idx];
    g_sensor_data.dirty_mask |= DIRTY_UI_LAYOUT;
}

void arex_bus_toggle_flash_speed(void)
{
    g_sys_config.flash_speed = (g_sys_config.flash_speed + 1) % 3;
    g_sensor_data.dirty_mask |= DIRTY_UI_LAYOUT;
}

void arex_bus_toggle_mask(void)
{
    g_sys_config.mask_enabled = !g_sys_config.mask_enabled;
    g_sensor_data.dirty_mask |= DIRTY_UI_LAYOUT;
}

void arex_bus_toggle_split_outward(void)
{
    g_sys_config.split_outward = !g_sys_config.split_outward;
    g_sensor_data.dirty_mask |= DIRTY_UI_LAYOUT;
}

void arex_bus_set_ui_offset(int16_t offset_x, int16_t offset_y)
{
    if (g_sys_config.offset_x == offset_x && g_sys_config.offset_y == offset_y)
    {
        return;
    }

    g_sys_config.offset_x = offset_x;
    g_sys_config.offset_y = offset_y;
    g_sensor_data.dirty_mask |= DIRTY_UI_LAYOUT;
}

/* =========================================================
 * 减压状态综合更新接口（原子操作）
 *
 * 算法层每个周期调用一次，一次性更新所有减压相关数据：
 *   - NDL 免减压时间
 *   - 停留状态机类型
 *   - 停留深度/时间参数
 *   - 停留区域标志
 *
 * 相比分离调用，本接口的优势：
 *   1. 原子更新，避免 UI 任务读到中间状态
 *   2. 一次临界区保护，减少中断关闭时间
 *   3. 合并 DIRTY_NDL | DIRTY_NDL_STOP 脏标记
 * ========================================================= */
void arex_bus_update_deco(int16_t ndl_min, arex_stop_type_t stop_type,
                          float depth_m, uint16_t total_time_s,
                          uint16_t time_s, bool in_stop_zone)
{
    /* 计算合并脏标记：停留相关才打 DIRTY_NDL_STOP */
    uint32_t new_dirty = DIRTY_NDL;  /* NDL 始终需要检查 */

    /* 计算是否需要更新 */
    arex_stop_type_t prev_stop_type = g_sensor_data.stop_type;
    uint16_t prev_stop_time_left_s = g_sensor_data.stop_time_left_s;
    bool ndl_changed  = (g_sensor_data.ndl != ndl_min);
    bool stop_changed = (g_sensor_data.stop_type != stop_type ||
                         g_sensor_data.stop_depth_m != depth_m ||
                         g_sensor_data.stop_time_total_s != total_time_s ||
                         g_sensor_data.stop_time_left_s != time_s ||
                         g_sensor_data.in_stop_zone != in_stop_zone);

    if (!ndl_changed && !stop_changed)
    {
        return;  /* 无变化，快速返回 */
    }

    /* 临界区保护：一次性更新所有字段 */
    rt_base_t level = rt_hw_interrupt_disable();

    if (ndl_changed)
    {
        g_sensor_data.ndl = ndl_min;
    }

    if (stop_changed)
    {
        g_sensor_data.stop_type = stop_type;
        g_sensor_data.stop_depth_m = depth_m;
        g_sensor_data.stop_time_total_s = total_time_s;
        g_sensor_data.stop_time_left_s = time_s;
        g_sensor_data.in_stop_zone = in_stop_zone;
        new_dirty |= DIRTY_NDL_STOP;  /* 停留数据变化才打此标记 */
    }

    g_sensor_data.dirty_mask |= new_dirty;

    rt_hw_interrupt_enable(level);

    arex_alarm_eval_ndl();
    arex_alarm_eval_deco_limits();
    arex_alarm_eval_safety_stop_info();
    if (prev_stop_type != AREX_STOP_NONE && prev_stop_time_left_s > 0U && time_s == 0U)
    {
        arex_alarm_set_active(AREX_ALARM_ID_INFO_STOP_DONE, true);
    }
}

/* GF Low/High 设定值同步接口（由 buhlmann_task 调用） */
void arex_bus_set_gf_setting(uint8_t gf_low, uint8_t gf_high)
{
    if (g_sensor_data.gf_low != gf_low || g_sensor_data.gf_high != gf_high)
    {
        g_sensor_data.gf_low = gf_low;
        g_sensor_data.gf_high = gf_high;
        g_sensor_data.dirty_mask |= DIRTY_GF_SETTING;
    }
}

/* MOD（最大操作深度）同步接口 */
void arex_bus_set_mod(float mod_m)
{
    if (g_sensor_data.mod_m != mod_m)
    {
        g_sensor_data.mod_m = mod_m;
        g_sensor_data.dirty_mask |= DIRTY_MOD;
    }
}

/* Ceiling（减压上限）同步接口 */
void arex_bus_set_ceiling(float ceiling_m)
{
    if (g_sensor_data.ceiling_m != ceiling_m)
    {
        g_sensor_data.ceiling_m = ceiling_m;
        g_sensor_data.dirty_mask |= DIRTY_CEILING;
        arex_alarm_eval_deco_limits();
    }
}

/* 气体混合比（O2/He）同步接口 */
void arex_bus_set_gas_mix(uint8_t o2_pct, uint8_t he_pct)
{
    if (g_sensor_data.gas_o2_pct != o2_pct || g_sensor_data.gas_he_pct != he_pct)
    {
        g_sensor_data.gas_o2_pct = o2_pct;
        g_sensor_data.gas_he_pct = he_pct;
        g_sensor_data.dirty_mask |= DIRTY_GAS_MIX;
    }
}

/* 气体密度同步接口 */
void arex_bus_set_gas_density(float density)
{
    if (g_sensor_data.gas_density != density)
    {
        g_sensor_data.gas_density = density;
        g_sensor_data.dirty_mask |= DIRTY_GAS_DENS;
    }
}

/* FiO2（实际吸入氧浓度）同步接口 */
void arex_bus_set_fio2(float fio2_pct)
{
    if (g_sensor_data.fio2_pct != fio2_pct)
    {
        g_sensor_data.fio2_pct = fio2_pct;
        g_sensor_data.dirty_mask |= DIRTY_FIO2;
    }
}

/* =========================================================
 * 配置持久化接口
 * 由具体平台（PC 模拟器 / 真机）提供 weak 实现覆盖
 * ========================================================= */
#ifdef PC_SIMULATOR
#else
__attribute__((weak))    //真机需打开，用于覆盖此默认实现
#endif
bool arex_config_load(arex_sys_config_t *cfg)
{
    /*
     * ========== 真机实现模板（删除 #ifdef PC_SIMULATOR 后替换此处） ==========
     * 说明：
     *   - cfg 指向 g_sys_config 全局变量
     *   - 成功返回 true（表示加载了有效配置，UI 不要用默认值）
     *   - 失败返回 false（表示无配置或配置损坏，UI 用默认值）
     *
     * 伪代码结构：
     *
     * #define CFG_MAGIC   0xAREX5F5A
     * #define CFG_ADDR    (Flash分区起始地址 + 偏移量)
     *
     * arex_config_block_t blk;
     * fal_partition_read(PART_NAME, CFG_ADDR, &blk, sizeof(blk));
     *
     * // 验证魔法数
     * if (blk.magic != CFG_MAGIC) {
     *     return false;
     * }
     *
     * // 复制主配置（包含 left_widgets 和 custom_5f_widgets）
     * memcpy(cfg, &tmp, sizeof(arex_sys_config_t));
     *
     * return true;
     */
    (void)cfg;
    return false;
}

#ifdef PC_SIMULATOR
#else
__attribute__((weak))    //真机需打开，用于覆盖此默认实现
#endif
bool arex_config_save(const arex_sys_config_t *cfg)
{
    /*
     * ========== 真机实现模板（删除 #ifdef PC_SIMULATOR 后替换此处） ==========
     * 说明：
     *   - cfg 指向当前配置（通常即 g_sys_config）
     *   - 成功返回 true，失败返回 false
     *
     * 伪代码结构：
     *
     * #define CFG_ADDR    (Flash分区起始地址 + 偏移量)
     *
     * // 整块写入（包含 left_widgets 和 custom_5f_widgets）
     * fal_partition_erase(PART_NAME, CFG_ADDR, sizeof(arex_sys_config_t));
     * if (fal_partition_write(PART_NAME, CFG_ADDR, cfg, sizeof(arex_sys_config_t)) != sizeof(arex_sys_config_t)) {
     *     return false;
     * }
     *
     * return true;
     */
    (void)cfg;
    return false;
}
