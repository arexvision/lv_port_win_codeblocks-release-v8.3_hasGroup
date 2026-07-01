#include "deco_core.h"

#include "arex_deco/arex_deco.h"
#include "rtthread.h"

extern "C" {
#include "../ui/alarm/alarm.h"
#include "../ui/core/data.h"
#include "../ui/core/ui_settings.h"
#include "../ui/core/ui_state.h"
}

#include <math.h>
#include <stdio.h>
#include <string.h>

#define PLAN_MAX_ROWS DIVE_PLAN_RESULT_MAX_ROWS
#define PLAN_DESCENT_RATE_MPM 18.0f
#define DECO_SCHEDULE_DEBUG_PRINT_MS 1000U
#define DECO_SCHEDULE_DEBUG_MAX_STOPS 6U
#define DECO_UI_SCHEDULE_DEBUG_MAX_STOPS MAX_DECO_STOPS /* 打印 UI 实际可承载的过滤后站点 */
#define DECO_PLAN_CALL_DEBUG 1U               /* 打印每次 step/plan 调用结果 */
#define DECO_GAS_SWITCH_PENALTY_SECONDS 60U   /* 传给 core 的切气惩罚时间 */
#define DECO_CEILING_ACTIVE_M 0.01f           /* ceiling 大于该值即认为有实时减压义务 */
#define DECO_STOP_ZONE_DEEP_MARGIN_M 1.5f     /* 减压站允许比显示站深的范围 */
#define DECO_GAS_DENSITY_COMPRESSIBILITY_Z 1.0f /* 真实气体压缩因子，当前按理想气体 */
#define DECO_TEMPERATURE_KELVIN_OFFSET 273.15f /* 摄氏度转 Kelvin */
#define DECO_FORECAST_TTS_HOLD_SECONDS 300U   /* TTS hold 预测 5 分钟 */
#define DECO_FORECAST_TTS_INTERVAL_S 5U       /* TTS forecast 低频刷新间隔 */
#define DECO_NDL_EXCURSION_DELTA_M 3.0f       /* NDL 上/下 3m 试探 */
#define DECO_NDL_DYNAMIC_RATE_THRESHOLD_MPM UI_ASCENT_RATE_STILL_DEADBAND_MPM /* 动态 NDL3 方向阈值 */
#define TISSUE_UI_PAMB_ANCHOR_PERMILLE 400.0f /* 归一化组织图环境压力锚点 */
#define TISSUE_UI_MVALUE_ANCHOR_PERMILLE 900.0f /* 归一化组织图 M 值锚点 */
#define TISSUE_UI_MAX_PERMILLE 1000.0f        /* 归一化组织图绘制上限 */
#define TISSUE_UI_PRESSURE_EPS 0.0001f        /* 组织压力归一化除零保护 */

static ArexDecoDiveState s_state;
static ArexDecoRuntimeMetrics s_metrics;
static ArexDecoRuntimeStopSelectorState s_runtime_stop_selector_state;
static bool s_initialized;
static bool s_api_version_checked;
static bool s_api_version_ok;
static uint8_t s_gf_low_pct = 40U;
static uint8_t s_gf_high_pct = 85U;
static uint8_t s_final_deco_stop_depth_m = 3U;
static uint8_t s_salinity_mode;
static uint8_t s_safety_stop_mode = UI_SAFETY_STOP_DEFAULT;
static uint32_t s_schedule_debug_last_print_ms;
static uint32_t s_plan_call_debug_last_print_ms;
static uint32_t s_runtime_stop_debug_last_print_ms;
static float s_temperature_c;
static bool s_temperature_valid;
static bool s_surface_confirmed = true;
static uint32_t s_tts_forecast_elapsed_s = DECO_FORECAST_TTS_INTERVAL_S;
static uint32_t s_runtime_ignored_gas_mask;
static uint8_t s_algo_gas_source_slots[AREX_DECO_MAX_GAS_COUNT];

static uint8_t algo_gas_source_slot(uint8_t gas_idx)
{
    if (gas_idx >= AREX_DECO_MAX_GAS_COUNT) return 0xFFU;
    return s_algo_gas_source_slots[gas_idx];
}

static const char *algo_gas_source_name(uint8_t gas_idx)
{
    uint8_t source_slot = algo_gas_source_slot(gas_idx);
    if (source_slot >= GAS_COUNT) return "--";
    return bus_get_gas_slot_name(source_slot);
}

static void format_algo_gas_ref(int8_t gas_idx, char *buf, size_t buf_size)
{
    uint8_t source_slot;
    const char *source_name;

    if (buf == NULL || buf_size == 0U) return;
    if (gas_idx < 0 || gas_idx >= (int8_t)AREX_DECO_MAX_GAS_COUNT)
    {
        (void)snprintf(buf, buf_size, "none");
        return;
    }

    source_slot = algo_gas_source_slot((uint8_t)gas_idx);
    source_name = algo_gas_source_name((uint8_t)gas_idx);
    if (source_slot < GAS_COUNT)
    {
        (void)snprintf(buf, buf_size, "%d(slot%u %s)", (int)gas_idx, (unsigned)source_slot, source_name);
    }
    else
    {
        (void)snprintf(buf, buf_size, "%d(--)", (int)gas_idx);
    }
}

typedef struct
{
    bool active;
    stop_type_t type;
    float depth_m;
    bool was_in_stop_zone;
    uint16_t total_s;
} stop_progress_t;

static stop_progress_t s_stop_progress;

static void reset_runtime_stop_selector(void)
{
    (void)memset(&s_runtime_stop_selector_state, 0, sizeof(s_runtime_stop_selector_state));
}

static bool check_api_version_once(void)
{
    if (s_api_version_checked)
    {
        return s_api_version_ok;
    }

    s_api_version_checked = true;
    const ArexDecoVersion version = arex_deco_get_api_version();
    if (version.major != AREX_DECO_API_VERSION_MAJOR ||
        version.minor != AREX_DECO_API_VERSION_MINOR ||
        version.patch != AREX_DECO_API_VERSION_PATCH)
    {
        rt_kprintf("[AREX_SIM] API version mismatch: lib=%u.%u.%u header=%u.%u.%u\n",
                   (unsigned)version.major,
                   (unsigned)version.minor,
                   (unsigned)version.patch,
                   (unsigned)AREX_DECO_API_VERSION_MAJOR,
                   (unsigned)AREX_DECO_API_VERSION_MINOR,
                   (unsigned)AREX_DECO_API_VERSION_PATCH);
        s_api_version_ok = false;
        return false;
    }

    s_api_version_ok = true;
    return true;
}

static uint16_t round_up_minutes(uint32_t seconds)
{
    if (seconds == 0U) return 0U;
    if (seconds >= 3932100U) return 65535U;
    return (uint16_t)((seconds + 59U) / 60U);
}

static int16_t round_i16_minutes_from_seconds(int32_t seconds)
{
    int32_t minutes;
    if (seconds == 0) return 0;
    if (seconds > 0) minutes = (seconds + 59) / 60;
    else minutes = -(((-seconds) + 59) / 60);
    if (minutes > 32767) return 32767;
    if (minutes < -32768) return -32768;
    return (int16_t)minutes;
}

static int16_t display_delta_minutes(uint16_t future_min, uint16_t current_min)
{
    int32_t delta = (int32_t)future_min - (int32_t)current_min;
    if (delta > 32767) return 32767;
    if (delta < -32768) return -32768;
    return (int16_t)delta;
}

static int16_t ndl_minutes_from_seconds(int32_t seconds)
{
    if (seconds <= 0) return 0;
    return round_i16_minutes_from_seconds(seconds);
}

static uint16_t round_u16_float(float value)
{
    if (value <= 0.0f) return 0U;
    if (value >= 65535.0f) return 65535U;
    return (uint16_t)(value + 0.5f);
}

static uint8_t round_u8_pct(float value)
{
    if (value <= 0.0f) return 0U;
    if (value >= 255.0f) return 255U;
    return (uint8_t)(value + 0.5f);
}

static int16_t round_i16_pct(float value)
{
    if (!isfinite(value)) return 0;
    if (value <= -32768.0f) return -32768;
    if (value >= 32767.0f) return 32767;
    return (int16_t)((value >= 0.0f) ? (value + 0.5f) : (value - 0.5f));
}

static uint16_t round_u16_permille(float value)
{
    if (!isfinite(value) || value <= 0.0f) return 0U;
    if (value >= TISSUE_UI_MAX_PERMILLE) return (uint16_t)TISSUE_UI_MAX_PERMILLE;
    return (uint16_t)(value + 0.5f);
}

static float target_gf_percent_from_core(float current_target_gf)
{
    if (!isfinite(current_target_gf) || current_target_gf <= 0.0f) return (float)s_gf_high_pct;
    if (current_target_gf <= 1.0f) return current_target_gf * 100.0f;
    return current_target_gf;
}

static void reset_stop_progress(void)
{
    (void)memset(&s_stop_progress, 0, sizeof(s_stop_progress));
}

static uint16_t sync_stop_progress_total(stop_type_t type, float depth_m, uint16_t left_s, bool in_stop_zone)
{
    bool stop_changed;

    if (type == STOP_NONE || left_s == 0U)
    {
        reset_stop_progress();
        return 0U;
    }

    stop_changed = !s_stop_progress.active ||
                   s_stop_progress.type != type ||
                   fabsf(s_stop_progress.depth_m - depth_m) > 0.05f;
    if (stop_changed)
    {
        s_stop_progress.active = true;
        s_stop_progress.type = type;
        s_stop_progress.depth_m = depth_m;
        s_stop_progress.was_in_stop_zone = false;
        s_stop_progress.total_s = left_s;
    }

    if (in_stop_zone && !s_stop_progress.was_in_stop_zone)
    {
        s_stop_progress.total_s = left_s;
    }
    else if (in_stop_zone && left_s > s_stop_progress.total_s)
    {
        s_stop_progress.total_s = left_s;
    }
    else if (!in_stop_zone)
    {
        s_stop_progress.total_s = left_s;
    }

    s_stop_progress.was_in_stop_zone = in_stop_zone;
    return s_stop_progress.total_s;
}

static bool deco_stop_zone_active(float current_depth_m, float stop_depth_m)
{
    return current_depth_m <= (stop_depth_m + DECO_STOP_ZONE_DEEP_MARGIN_M);
}

static const char *deco_status_name(int status)
{
    switch (status)
    {
    case AREX_DECO_STATUS_OK:
        return "OK";
    case AREX_DECO_STATUS_INVALID_ARGUMENT:
        return "INVALID_ARGUMENT";
    case AREX_DECO_STATUS_UNSUPPORTED_VERSION:
        return "UNSUPPORTED_VERSION";
    case AREX_DECO_STATUS_INSUFFICIENT_CAPACITY:
        return "INSUFFICIENT_CAPACITY";
    case AREX_DECO_STATUS_INVALID_STATE:
        return "INVALID_STATE";
    case AREX_DECO_STATUS_NOT_IMPLEMENTED:
        return "NOT_IMPLEMENTED";
    default:
        return "NA";
    }
}

static void debug_print_plan_call(const char *tag, int step_status, int plan_status, const ArexDecoSchedule *schedule, int8_t step_gas_idx, int8_t plan_active_idx)
{
#if DECO_PLAN_CALL_DEBUG
    uint32_t now_ms = rt_tick_get();
    bool failed = (step_status != -1 && step_status != AREX_DECO_STATUS_OK) ||
                  (plan_status != -1 && plan_status != AREX_DECO_STATUS_OK);
    uint32_t ndl_s = (s_metrics.ndl_seconds > 0) ? (uint32_t)s_metrics.ndl_seconds : 0U;
    uint16_t ndl_min = (ndl_s > 0U) ? round_up_minutes(ndl_s) : 0U;
    uint8_t stop_count = (plan_status == AREX_DECO_STATUS_OK && schedule != NULL) ? schedule->stop_count : 0U;
    uint8_t cv = (plan_status == AREX_DECO_STATUS_OK && schedule != NULL) ? schedule->ceiling_violated : 0U;
    uint32_t tts_s = (plan_status == AREX_DECO_STATUS_OK && schedule != NULL) ? schedule->tts_seconds : 0U;
    char step_gas_text[32];
    char plan_active_text[32];

    if (!failed && (uint32_t)(now_ms - s_plan_call_debug_last_print_ms) < DECO_SCHEDULE_DEBUG_PRINT_MS)
    {
        return;
    }
    s_plan_call_debug_last_print_ms = now_ms;
    format_algo_gas_ref(step_gas_idx, step_gas_text, sizeof(step_gas_text));
    format_algo_gas_ref(plan_active_idx, plan_active_text, sizeof(plan_active_text));

    rt_kprintf("[AREX_CALL] %s depth=%.1fm step=%s(%d) step_gas=%s plan=%s(%d) plan_active=%s ndl=%lus/%umin ceiling=%.2fm gf99=%.1f%% surfgf=%.1f%% stops=%u tts=%lus cv=%u\n",
               tag ? tag : "plan",
               s_state.current_depth_m,
               deco_status_name(step_status),
               step_status,
               step_gas_text,
               deco_status_name((int)plan_status),
               plan_status,
               plan_active_text,
               (unsigned long)ndl_s,
               (unsigned)ndl_min,
               s_metrics.ceiling_depth_m,
               s_metrics.gf99_percent,
               s_metrics.surface_gf_percent,
               (unsigned)stop_count,
               (unsigned long)tts_s,
               (unsigned)cv);
#else
    (void)tag;
    (void)step_status;
    (void)plan_status;
    (void)schedule;
    (void)step_gas_idx;
    (void)plan_active_idx;
#endif
}

static uint32_t stop_runtime_seconds(const ArexDecoStop *stop);
static bool stop_display_suppressed(const ArexDecoStop *stop);

static void debug_print_ui_schedule(const ArexDecoSchedule *schedule)
{
    uint8_t display_count = 0U;
    uint8_t hidden_suppressed = 0U;
    uint8_t hidden_zero = 0U;

    if (schedule == NULL) return;

    rt_kprintf("[AREX_UI_PLAN] tts_raw=%lus raw_stops=%u",
               (unsigned long)schedule->tts_seconds,
               (unsigned)schedule->stop_count);

    for (uint8_t i = 0U; i < schedule->stop_count && display_count < DECO_UI_SCHEDULE_DEBUG_MAX_STOPS; i++)
    {
        const ArexDecoStop *stop = &schedule->stops[i];
        uint32_t runtime_s = stop_runtime_seconds(stop);
        if (stop_display_suppressed(stop))
        {
            hidden_suppressed++;
            continue;
        }
        if (stop->depth_m <= 0.0f || runtime_s == 0U)
        {
            hidden_zero++;
            continue;
        }
        char gas_text[32];
        format_algo_gas_ref(stop->gas_index, gas_text, sizeof(gas_text));
        rt_kprintf(" | #%u raw#%u %.2fm %lus/%umin gas=%s gf=%.2f kind=%u flags=0x%02x",
                   (unsigned)(display_count + 1U),
                   (unsigned)(i + 1U),
                   (double)stop->depth_m,
                   (unsigned long)runtime_s,
                   (unsigned)round_up_minutes(runtime_s),
                   gas_text,
                   (double)stop->target_gf,
                   (unsigned)stop->kind,
                   (unsigned)stop->flags);
        display_count++;
    }

    rt_kprintf(" | display=%u hidden_suppressed=%u hidden_zero=%u\n",
               (unsigned)display_count,
               (unsigned)hidden_suppressed,
               (unsigned)hidden_zero);
}

static void debug_print_schedule(const ArexDecoSchedule *schedule)
{
    uint32_t now_ms = rt_tick_get();
    uint8_t print_count;

    if (schedule == NULL)
    {
        return;
    }
    if ((uint32_t)(now_ms - s_schedule_debug_last_print_ms) < DECO_SCHEDULE_DEBUG_PRINT_MS)
    {
        return;
    }

    s_schedule_debug_last_print_ms = now_ms;
    print_count = schedule->stop_count;
    if (print_count > DECO_SCHEDULE_DEBUG_MAX_STOPS) print_count = DECO_SCHEDULE_DEBUG_MAX_STOPS;

    rt_kprintf("[AREX_PLAN] depth=%.1fm ceiling=%.2fm gf99=%.1f%% surfgf=%.1f%% cv=%u tts=%lus stops=%u",
               (double)s_state.current_depth_m,
               (double)s_metrics.ceiling_depth_m,
               (double)s_metrics.gf99_percent,
               (double)s_metrics.surface_gf_percent,
               (unsigned)schedule->ceiling_violated,
               (unsigned long)schedule->tts_seconds,
               (unsigned)schedule->stop_count);
    for (uint8_t i = 0U; i < print_count; i++)
    {
        const ArexDecoStop *stop = &schedule->stops[i];
        int8_t gas_idx = stop->gas_index;
        uint8_t source_slot = (gas_idx >= 0) ? algo_gas_source_slot((uint8_t)gas_idx) : 0xFFU;
        const char *source_name = (gas_idx >= 0) ? algo_gas_source_name((uint8_t)gas_idx) : "--";
        rt_kprintf(" | #%u %.2fm dur=%lus hold=%lus sw=%lus gas=%d(slot%u %s) gf=%.2f kind=%u flags=0x%02x",
                   (unsigned)(i + 1U),
                   (double)stop->depth_m,
                   (unsigned long)stop->duration_seconds,
                   (unsigned long)stop->hold_seconds,
                   (unsigned long)stop->switch_penalty_seconds,
                   (int)gas_idx,
                   (unsigned)source_slot,
                   source_name,
                   (double)stop->target_gf,
                   (unsigned)stop->kind,
                   (unsigned)stop->flags);
    }
    if (schedule->stop_count > print_count) rt_kprintf(" | ...");
    rt_kprintf("\n");
    debug_print_ui_schedule(schedule);
}

static float pressure_bar_at_depth(const ArexDecoConfig *config, float depth_m)
{
    if (depth_m < 0.0f) depth_m = 0.0f;
    return config->surface_pressure_bar + depth_m / config->water_meters_per_bar;
}

static float core_mod_depth_for_gas(const ArexDecoConfig *config, const ArexDecoGas *gas)
{
    float mod_m = 0.0f;
    if (config == NULL || gas == NULL) return 0.0f;
    if (arex_deco_calculate_gas_mod(config, gas, &mod_m) == AREX_DECO_STATUS_OK) return mod_m;
    return gas->max_depth_m;
}

static uint32_t stop_runtime_seconds(const ArexDecoStop *stop)
{
    if (stop == NULL) return 0U;
    return stop->hold_seconds;
}

static bool stop_display_suppressed(const ArexDecoStop *stop)
{
    if (stop == NULL) return false;
    return (stop->flags & AREX_DECO_STOP_FLAG_DISPLAY_SUPPRESSED) != 0U;
}

static bool select_runtime_stop(const ArexDecoSchedule *schedule, ArexDecoRuntimeStop *runtime_stop)
{
    ArexDecoRuntimeStopSelectorInput input;
    ArexDecoRuntimeStopSelectorState next_state;
    ArexDecoStatus status;
    uint32_t now_ms;

    if (runtime_stop == NULL) return false;
    (void)memset(runtime_stop, 0, sizeof(*runtime_stop));
    if (schedule == NULL) return false;

    (void)memset(&input, 0, sizeof(input));
    (void)memset(&next_state, 0, sizeof(next_state));
    input.api_version = arex_deco_get_api_version();
    input.current_depth_m = s_state.current_depth_m;
    input.elapsed_seconds = s_state.elapsed_seconds;

    status = arex_deco_select_runtime_stop(schedule, &s_runtime_stop_selector_state, &input, &next_state, runtime_stop);
    if (status != AREX_DECO_STATUS_OK)
    {
        rt_kprintf("[AREX_RUNTIME_STOP] selector failed: %s(%d)\n", deco_status_name((int)status), (int)status);
        reset_runtime_stop_selector();
        (void)memset(runtime_stop, 0, sizeof(*runtime_stop));
        return false;
    }

    s_runtime_stop_selector_state = next_state;
    now_ms = rt_tick_get();
    if ((uint32_t)(now_ms - s_runtime_stop_debug_last_print_ms) >= DECO_SCHEDULE_DEBUG_PRINT_MS)
    {
        char gas_text[32];
        unsigned raw_display_index = runtime_stop->available ? (unsigned)(runtime_stop->source_raw_index + 1U) : 0U;
        s_runtime_stop_debug_last_print_ms = now_ms;
        format_algo_gas_ref(runtime_stop->gas_index, gas_text, sizeof(gas_text));
        rt_kprintf("[AREX_RUNTIME_STOP] avail=%u raw#%u depth=%.2fm rem=%lus total=%lus gas=%s reason=%u short=%u active=%u cand=%u cand_seen=%lus\n",
                   (unsigned)runtime_stop->available,
                   raw_display_index,
                   (double)runtime_stop->depth_m,
                   (unsigned long)runtime_stop->remaining_seconds,
                   (unsigned long)runtime_stop->total_seconds,
                   gas_text,
                   (unsigned)runtime_stop->reason,
                   (unsigned)runtime_stop->is_short,
                   (unsigned)s_runtime_stop_selector_state.active,
                   (unsigned)s_runtime_stop_selector_state.candidate_active,
                   (unsigned long)s_runtime_stop_selector_state.candidate_seen_seconds);
    }
    return runtime_stop->available != 0U;
}

static void format_gas_name(const ArexDecoGas *gas, char *name_buf, size_t name_buf_size)
{
    uint8_t o2_pct = round_u8_pct(gas->oxygen_fraction * 100.0f);
    uint8_t he_pct = round_u8_pct(gas->helium_fraction * 100.0f);

    if (he_pct > 0U) {
        (void)snprintf(name_buf, name_buf_size, "TX %u/%u", (unsigned)o2_pct, (unsigned)he_pct);
    } else if (o2_pct == 21U) {
        (void)snprintf(name_buf, name_buf_size, "AIR");
    } else if (o2_pct == 100U) {
        (void)snprintf(name_buf, name_buf_size, "O2 100%%");
    } else {
        (void)snprintf(name_buf, name_buf_size, "NX %u", (unsigned)o2_pct);
    }
}

static void debug_print_gas_plan_map(const ArexDecoGasPlan *gas_plan, const uint8_t *source_slots)
{
    if (gas_plan == NULL) return;

    rt_kprintf("[DECO_GAS_MAP] count=%u active=%d ignored=0x%lx",
               (unsigned)gas_plan->gas_count,
               (int)gas_plan->active_gas_index,
               (unsigned long)s_runtime_ignored_gas_mask);
    for (uint8_t i = 0U; i < gas_plan->gas_count && i < AREX_DECO_MAX_GAS_COUNT; i++)
    {
        const ArexDecoGas *gas = &gas_plan->gases[i];
        uint8_t source_slot = (source_slots != NULL) ? source_slots[i] : 0xFFU;
        char name[16];
        format_gas_name(gas, name, sizeof(name));
        if (source_slot != 0xFFU)
        {
            const char *slot_name = bus_get_gas_slot_name(source_slot);
            rt_kprintf(" | idx%u<-slot%u %s %u/%u mod=%.1fm maxpo2=%.2f en=%u",
                       (unsigned)i,
                       (unsigned)source_slot,
                       (slot_name != NULL && slot_name[0] != '\0') ? slot_name : name,
                       (unsigned)round_u8_pct(gas->oxygen_fraction * 100.0f),
                       (unsigned)round_u8_pct(gas->helium_fraction * 100.0f),
                       (double)gas->max_depth_m,
                       (double)gas->max_ppo2_bar,
                       (unsigned)gas->enabled);
        }
        else
        {
            rt_kprintf(" | idx%u<-default %s %u/%u mod=%.1fm maxpo2=%.2f en=%u",
                       (unsigned)i,
                       name,
                       (unsigned)round_u8_pct(gas->oxygen_fraction * 100.0f),
                       (unsigned)round_u8_pct(gas->helium_fraction * 100.0f),
                       (double)gas->max_depth_m,
                       (double)gas->max_ppo2_bar,
                       (unsigned)gas->enabled);
        }
    }
    rt_kprintf("\n");
}

static uint32_t safety_stop_seconds_from_mode(uint8_t mode, uint8_t *enabled)
{
    *enabled = (mode == UI_SAFETY_STOP_OFF) ? 0U : 1U;
    switch (mode)
    {
    case UI_SAFETY_STOP_OFF:
        return 0U;
    case UI_SAFETY_STOP_4MIN:
        return 240U;
    case UI_SAFETY_STOP_5MIN:
        return 300U;
    case UI_SAFETY_STOP_3MIN:
    case UI_SAFETY_STOP_ADAPT:
    case UI_SAFETY_STOP_CNTUP:
    default:
        return 180U;
    }
}

static ArexDecoWaterType water_type_from_salinity(uint8_t mode)
{
    if (mode == 0U) return AREX_DECO_WATER_FRESH;
    return AREX_DECO_WATER_SALT;
}

static void fill_config_from_ui(ArexDecoConfig *config)
{
    uint8_t safety_enabled;

    (void)arex_deco_make_default_config(config);
    config->gf_low = (float)s_gf_low_pct / 100.0f;
    config->gf_high = (float)s_gf_high_pct / 100.0f;
    config->last_stop_m = (float)s_final_deco_stop_depth_m;
    config->deco_step_m = 3.0f;
    config->water_type = water_type_from_salinity(s_salinity_mode);
    config->water_meters_per_bar = (config->water_type == AREX_DECO_WATER_FRESH) ? AREX_DECO_DEFAULT_FRESH_WATER_METERS_PER_BAR : AREX_DECO_DEFAULT_SALT_WATER_METERS_PER_BAR;
    config->safety_stop_seconds = safety_stop_seconds_from_mode(s_safety_stop_mode, &safety_enabled);
    config->safety_stop_enabled = safety_enabled;
    config->gas_switch_penalty_seconds = DECO_GAS_SWITCH_PENALTY_SECONDS;
}

static bool fill_gas_plan_from_ui(const ArexDecoConfig *config, ArexDecoGasPlan *gas_plan)
{
    uint8_t source_count = bus_get_gas_slot_count();
    uint8_t valid_count = 0U;
    int8_t active_idx = 0;
    uint8_t source_slots[AREX_DECO_MAX_GAS_COUNT];

    (void)memset(gas_plan, 0, sizeof(*gas_plan));
    (void)memset(source_slots, 0xFF, sizeof(source_slots));
    (void)memset(s_algo_gas_source_slots, 0xFF, sizeof(s_algo_gas_source_slots));
    gas_plan->api_version = arex_deco_get_api_version();
    if (source_count > GAS_COUNT) source_count = GAS_COUNT;

    for (uint8_t i = 0U; i < source_count && valid_count < AREX_DECO_MAX_GAS_COUNT; i++)
    {
        uint8_t o2 = bus_get_gas_slot_o2_pct(i);
        uint8_t he = bus_get_gas_slot_he_pct(i);
        if (o2 == 0U || o2 > 100U || he > 100U || (uint16_t)o2 + (uint16_t)he > 100U)
        {
            continue;
        }

        ArexDecoGas *gas = &gas_plan->gases[valid_count];
        float slot_mod_m = bus_get_gas_slot_mod_m(i);
        float ppo2 = bus_get_gas_slot_max_ppo2(i);
        if (ppo2 <= 0.0f) ppo2 = bus_get_mod_ppo2();
        gas->oxygen_fraction = (float)o2 / 100.0f;
        gas->helium_fraction = (float)he / 100.0f;
        gas->nitrogen_fraction = 1.0f - gas->oxygen_fraction - gas->helium_fraction;
        gas->min_depth_m = 0.0f;
        gas->max_ppo2_bar = ppo2;
        gas->max_depth_m = (slot_mod_m > 0.0f) ? slot_mod_m : core_mod_depth_for_gas(config, gas);
        gas->enabled = 1U;
        gas->role = (valid_count == 0U) ? AREX_DECO_GAS_ROLE_BOTTOM : AREX_DECO_GAS_ROLE_DECO;

        if (i == bus_get_gas_active_idx())
        {
            active_idx = (int8_t)valid_count;
        }
        source_slots[valid_count] = i;
        s_algo_gas_source_slots[valid_count] = i;
        valid_count++;
    }

    if (valid_count == 0U)
    {
        if (arex_deco_make_default_gas_plan(config, gas_plan) != AREX_DECO_STATUS_OK) return false;
        gas_plan->active_gas_index = 0;
        s_algo_gas_source_slots[0] = 0xFFU;
        debug_print_gas_plan_map(gas_plan, NULL);
        return true;
    }

    gas_plan->gas_count = valid_count;
    gas_plan->active_gas_index = (active_idx >= 0 && active_idx < (int8_t)valid_count) ? active_idx : 0;
    debug_print_gas_plan_map(gas_plan, source_slots);
    return true;
}

static uint32_t gas_index_mask(uint8_t gas_idx)
{
    return (gas_idx < 32U) ? (1UL << gas_idx) : 0U;
}

static void runtime_ignore_gas(uint8_t gas_idx)
{
    uint32_t mask = gas_index_mask(gas_idx);
    if (mask == 0U || gas_idx >= s_state.gas_plan.gas_count) return;
    if (gas_idx == (uint8_t)s_state.gas_plan.active_gas_index) return;
    if ((s_runtime_ignored_gas_mask & mask) == 0U)
    {
        s_runtime_ignored_gas_mask |= mask;
        bus_set_recommended_gas_idx(-1);
        rt_kprintf("[DECO_GAS] ignore gas=%u for runtime planner\n", (unsigned)gas_idx);
    }
}

static void runtime_restore_gas(uint8_t gas_idx)
{
    uint32_t mask = gas_index_mask(gas_idx);
    if (mask == 0U) return;
    if ((s_runtime_ignored_gas_mask & mask) != 0U)
    {
        s_runtime_ignored_gas_mask &= ~mask;
        rt_kprintf("[DECO_GAS] restore gas=%u for runtime planner\n", (unsigned)gas_idx);
    }
}

static void runtime_clear_ignored_gases(void)
{
    if (s_runtime_ignored_gas_mask != 0U)
    {
        s_runtime_ignored_gas_mask = 0U;
        rt_kprintf("[DECO_GAS] clear runtime ignored gases\n");
    }
}

static void apply_runtime_ignored_gases(ArexDecoDiveState *state)
{
    if (state == NULL || s_runtime_ignored_gas_mask == 0U) return;
    for (uint8_t i = 0U; i < state->gas_plan.gas_count && i < AREX_DECO_MAX_GAS_COUNT; i++)
    {
        if ((s_runtime_ignored_gas_mask & gas_index_mask(i)) != 0U &&
            (int8_t)i != state->gas_plan.active_gas_index)
        {
            state->gas_plan.gases[i].enabled = 0U;
        }
    }
}

static void apply_current_ui_config(void)
{
    ArexDecoConfig config;
    ArexDecoGasPlan gas_plan;

    fill_config_from_ui(&config);
    if (!fill_gas_plan_from_ui(&config, &gas_plan))
    {
        return;
    }
    s_state.config = config;
    s_state.gas_plan = gas_plan;
}

static bool ensure_initialized(void)
{
    ArexDecoGasPlan surface_gas_plan;
    const ArexDecoGas *surface_gas;

    if (s_initialized) return true;
    if (!check_api_version_once()) return false;

    if (arex_deco_make_initial_dive_state(&s_state) != AREX_DECO_STATUS_OK) return false;
    (void)memset(&s_metrics, 0, sizeof(s_metrics));
    apply_current_ui_config();
    surface_gas = &s_state.gas_plan.gases[s_state.gas_plan.active_gas_index];
    if (s_surface_confirmed && arex_deco_make_default_gas_plan(&s_state.config, &surface_gas_plan) == AREX_DECO_STATUS_OK)
    {
        surface_gas = &surface_gas_plan.gases[0];
    }
    (void)arex_deco_reset_tissue_to_surface(&s_state.config, surface_gas, &s_state.tissue);
    s_initialized = true;
    bus_set_tts_at_5min(0U);
    bus_set_tts_delta_5min(0);
    bus_set_ndl_up_3m(0);
    bus_set_ndl_down_3m(0);
    bus_clear_ndl_delta_3m();
    rt_kprintf("Arex deco core initialized (GF: %u/%u)\n", (unsigned)s_gf_low_pct, (unsigned)s_gf_high_pct);
    return true;
}

static void sync_tissue_data(const ArexDecoDiveState *output_state)
{
    ArexDecoTissuePressureMetrics pressures;
    int16_t tissue_raw[AREX_DECO_COMPARTMENT_COUNT];
    uint8_t tissue_gf[AREX_DECO_COMPARTMENT_COUNT];
    uint16_t tissue_bar_permille[AREX_DECO_COMPARTMENT_COUNT];
    float tissue_n2_bar[AREX_DECO_COMPARTMENT_COUNT];
    float tissue_he_bar[AREX_DECO_COMPARTMENT_COUNT];
    float tissue_m_value_bar[AREX_DECO_COMPARTMENT_COUNT];
    float tissue_m_gf_bar[AREX_DECO_COMPARTMENT_COUNT];

    if (output_state == NULL) output_state = &s_state;
    if (arex_deco_calculate_tissue_pressures(output_state, &pressures) != AREX_DECO_STATUS_OK) return;

    float ambient_pressure_bar = pressures.ambient_pressure_bar;
    float inspired_inert_bar = pressures.inspired_n2_bar + pressures.inspired_he_bar;
    uint16_t pi_permille = (ambient_pressure_bar > TISSUE_UI_PRESSURE_EPS) ? round_u16_permille((inspired_inert_bar / ambient_pressure_bar) * TISSUE_UI_PAMB_ANCHOR_PERMILLE) : 0U;

    for (uint8_t i = 0U; i < AREX_DECO_COMPARTMENT_COUNT; i++)
    {
        tissue_n2_bar[i] = pressures.tissue_n2_bar[i];
        tissue_he_bar[i] = pressures.tissue_he_bar[i];
        tissue_m_value_bar[i] = pressures.tissue_m_value_bar[i];
        tissue_m_gf_bar[i] = pressures.tissue_m_gf_bar[i];

        float tissue_total_inert_bar = pressures.tissue_n2_bar[i] + pressures.tissue_he_bar[i];
        float raw_denom = pressures.tissue_m_value_bar[i] - ambient_pressure_bar;
        float gf_denom = pressures.tissue_m_gf_bar[i] - ambient_pressure_bar;
        float absolute_gf = (isfinite(raw_denom) && fabsf(raw_denom) > TISSUE_UI_PRESSURE_EPS) ? ((tissue_total_inert_bar - ambient_pressure_bar) * 100.0f / raw_denom) : 0.0f;
        float relative_gf = (tissue_total_inert_bar > ambient_pressure_bar && isfinite(gf_denom) && gf_denom > TISSUE_UI_PRESSURE_EPS) ? ((tissue_total_inert_bar - ambient_pressure_bar) * 100.0f / gf_denom) : 0.0f;

        tissue_raw[i] = round_i16_pct(absolute_gf);
        tissue_gf[i] = round_u8_pct(relative_gf);
        if (tissue_gf[i] > 200U) tissue_gf[i] = 200U;

        if (ambient_pressure_bar <= TISSUE_UI_PRESSURE_EPS)
        {
            tissue_bar_permille[i] = 0U;
        }
        else if (tissue_total_inert_bar <= ambient_pressure_bar)
        {
            tissue_bar_permille[i] = round_u16_permille((tissue_total_inert_bar / ambient_pressure_bar) * TISSUE_UI_PAMB_ANCHOR_PERMILLE);
        }
        else
        {
            float over_limit_ratio = isfinite(absolute_gf) ? (absolute_gf / 100.0f) : 0.0f;
            tissue_bar_permille[i] = round_u16_permille(TISSUE_UI_PAMB_ANCHOR_PERMILLE + over_limit_ratio * (TISSUE_UI_MVALUE_ANCHOR_PERMILLE - TISSUE_UI_PAMB_ANCHOR_PERMILLE));
        }
    }

    bus_set_tissue_loads(tissue_raw, tissue_gf, target_gf_percent_from_core(pressures.current_gf_target));
    bus_set_tissue_normalized_payload(tissue_bar_permille,
                                      pi_permille,
                                      ambient_pressure_bar,
                                      pressures.inspired_n2_bar,
                                      pressures.inspired_he_bar,
                                      tissue_n2_bar,
                                      tissue_he_bar,
                                      tissue_m_value_bar,
                                      tissue_m_gf_bar);
}

static void sync_gas_data(void)
{
    float pressure_bar = pressure_bar_at_depth(&s_state.config, s_state.current_depth_m);
    int8_t active_idx = s_state.gas_plan.active_gas_index;
    if (active_idx < 0 || active_idx >= (int8_t)s_state.gas_plan.gas_count) active_idx = 0;

    ArexDecoGas *active_gas = &s_state.gas_plan.gases[active_idx];
    char gas_name[16];
    format_gas_name(active_gas, gas_name, sizeof(gas_name));
    bus_set_gas((uint8_t)active_idx, gas_name);
    bus_set_gas_mix(round_u8_pct(active_gas->oxygen_fraction * 100.0f), round_u8_pct(active_gas->helium_fraction * 100.0f));
    bus_set_fio2(active_gas->oxygen_fraction * 100.0f);
    bus_set_mod(core_mod_depth_for_gas(&s_state.config, active_gas));

    float gas_density = 0.0f;
    float temperature_kelvin = s_temperature_c + DECO_TEMPERATURE_KELVIN_OFFSET;
    if (s_temperature_valid &&
        isfinite(temperature_kelvin) &&
        temperature_kelvin > 0.0f &&
        arex_deco_calculate_gas_density(&s_state.config, active_gas, s_state.current_depth_m, temperature_kelvin, DECO_GAS_DENSITY_COMPRESSIBILITY_Z, &gas_density) == AREX_DECO_STATUS_OK &&
        isfinite(gas_density))
    {
        bus_set_gas_density(gas_density);
    }
    else
    {
        bus_set_gas_density(0.0f);
    }

    for (uint8_t i = 0U; i < s_state.gas_plan.gas_count && i < GAS_COUNT; i++)
    {
        bus_set_ppo2(i, s_state.gas_plan.gases[i].oxygen_fraction * pressure_bar);
    }
}

static void sync_gas_recommendation(const ArexDecoGasRecommendation *gas_rec, const ArexDecoRuntimeStop *runtime_stop)
{
    int8_t recommended_idx = -1;
    dive_lifecycle_phase_t phase = bus_get_dive_lifecycle_phase();
    bool lifecycle_allows_prompt = phase == DIVE_LIFECYCLE_ACTIVE || phase == DIVE_LIFECYCLE_SURFACING_PENDING;
    bool deco_context = runtime_stop != NULL && runtime_stop->available != 0U && s_metrics.ceiling_depth_m > DECO_CEILING_ACTIVE_M;

    if (lifecycle_allows_prompt &&
        deco_context &&
        gas_rec != NULL &&
        gas_rec->available &&
        gas_rec->recommended_gas_index >= 0 &&
        gas_rec->recommended_gas_index < (int8_t)s_state.gas_plan.gas_count &&
        gas_rec->recommended_gas_index != s_state.gas_plan.active_gas_index)
    {
        recommended_idx = gas_rec->recommended_gas_index;
    }

    bus_set_recommended_gas_idx(recommended_idx);
    (void)alarm_set_active(ALARM_ID_INFO_GAS_SWITCH, recommended_idx >= 0);
}

static void sync_deco_plan_data(const ArexDecoSchedule *schedule)
{
    deco_stop_t stops[MAX_DECO_STOPS];
    uint8_t count = 0U;

    if (schedule == NULL || schedule->stop_count == 0U)
    {
        bus_set_deco_plan(NULL, 0U);
        return;
    }

    for (uint8_t i = 0U; i < schedule->stop_count && count < MAX_DECO_STOPS; i++)
    {
        uint32_t runtime_s = stop_runtime_seconds(&schedule->stops[i]);
        if (stop_display_suppressed(&schedule->stops[i]))
        {
            continue;
        }
        if (schedule->stops[i].depth_m <= 0.0f || runtime_s == 0U)
        {
            continue;
        }
        stops[count].depth_m = schedule->stops[i].depth_m;
        stops[count].stay_min = (float)runtime_s / 60.0f;
        count++;
    }
    bus_set_deco_plan((count > 0U) ? stops : NULL, count);
}

static void sync_ndl_low_alarm(int16_t ndl_min, stop_type_t stop_type)
{
    uint16_t threshold_min = bus_get_ndl_alarm_min();
    bool ndl_context = stop_type == STOP_NONE || stop_type == STOP_SAFETY;
    bool active = threshold_min > 0U &&
                  ndl_context &&
                  ndl_min > 0 &&
                  ndl_min <= (int16_t)threshold_min;
    (void)alarm_set_active(ALARM_ID_WARN_NDL_LOW, active);
}

static void sync_stop_data(const ArexDecoRuntimeStop *runtime_stop)
{
    ArexDecoSafetyStopStatus safety_stop;
    int16_t ndl_min = 0;
    stop_type_t stop_type = STOP_NONE;
    float stop_depth_m = 0.0f;
    uint16_t stop_total_s = 0U;
    uint16_t stop_left_s = 0U;
    bool in_stop_zone = false;

    if (s_metrics.ndl_seconds > 0)
    {
        uint16_t ndl_calc = round_up_minutes((uint32_t)s_metrics.ndl_seconds);
        ndl_min = (ndl_calc > 99U) ? 99 : (int16_t)ndl_calc;
    }

    (void)memset(&safety_stop, 0, sizeof(safety_stop));
    if (runtime_stop != NULL && runtime_stop->available != 0U && s_metrics.ceiling_depth_m > DECO_CEILING_ACTIVE_M)
    {
        stop_type = STOP_DECO;
        stop_depth_m = runtime_stop->depth_m;
        stop_left_s = (runtime_stop->remaining_seconds > 65535U) ? 65535U : (uint16_t)runtime_stop->remaining_seconds;
        in_stop_zone = deco_stop_zone_active(s_state.current_depth_m, stop_depth_m);
        ndl_min = 0;
        stop_total_s = sync_stop_progress_total(stop_type, stop_depth_m, stop_left_s, in_stop_zone);
    }
    else if (arex_deco_safety_stop(&s_state, &safety_stop) == AREX_DECO_STATUS_OK &&
             safety_stop.required != 0U &&
             safety_stop.completed == 0U &&
             safety_stop.missed == 0U &&
             safety_stop.phase != AREX_DECO_SAFETY_STOP_PHASE_SUPPRESSED_BY_DECO)
    {
        stop_type = STOP_SAFETY;
        stop_depth_m = safety_stop.target_depth_m;
        stop_total_s = (safety_stop.required_seconds > 65535U) ? 65535U : (uint16_t)safety_stop.required_seconds;
        stop_left_s = (safety_stop.remaining_seconds > 65535U) ? 65535U : (uint16_t)safety_stop.remaining_seconds;
        in_stop_zone = safety_stop.counting != 0U;
    }
    else
    {
        reset_stop_progress();
    }
    sync_ndl_low_alarm(ndl_min, stop_type);
    bus_update_deco(ndl_min, stop_type, stop_depth_m, stop_total_s, stop_left_s, in_stop_zone);
    if (stop_type == STOP_NONE)
    {
        uint8_t bar = (ndl_min <= 0) ? 0U : (uint8_t)((ndl_min > 99 ? 99 : ndl_min) * 100 / 99);
        bus_set_ndl_bar_pct(bar);
    }
}

static void sync_forecast_data(void)
{
    ArexDecoNdlExcursionForecast ndl_forecast;

    (void)memset(&ndl_forecast, 0, sizeof(ndl_forecast));
    if (arex_deco_forecast_ndl_excursion(&s_state, DECO_NDL_EXCURSION_DELTA_M, &ndl_forecast) == AREX_DECO_STATUS_OK)
    {
        int16_t ndl_up_min = ndl_minutes_from_seconds(ndl_forecast.ndl_up_seconds);
        int16_t ndl_down_min = ndl_minutes_from_seconds(ndl_forecast.ndl_down_seconds);
        float rate_mpm = bus_get_ascent_rate();
        bus_set_ndl_up_3m(ndl_up_min);
        bus_set_ndl_down_3m(ndl_down_min);
        if (rate_mpm > DECO_NDL_DYNAMIC_RATE_THRESHOLD_MPM) bus_set_ndl_delta_3m(ndl_up_min);
        else if (rate_mpm < -DECO_NDL_DYNAMIC_RATE_THRESHOLD_MPM) bus_set_ndl_delta_3m(ndl_down_min);
        else bus_clear_ndl_delta_3m();
    }
    else
    {
        bus_clear_ndl_delta_3m();
    }

    if (s_tts_forecast_elapsed_s >= DECO_FORECAST_TTS_INTERVAL_S)
    {
        ArexDecoTtsForecast tts_forecast;
        (void)memset(&tts_forecast, 0, sizeof(tts_forecast));
        s_tts_forecast_elapsed_s = 0U;
        if (arex_deco_forecast_tts_hold(&s_state, DECO_FORECAST_TTS_HOLD_SECONDS, &tts_forecast) == AREX_DECO_STATUS_OK)
        {
            uint16_t current_min = round_up_minutes(tts_forecast.current_tts_seconds);
            uint16_t at_min = round_up_minutes(tts_forecast.tts_at_hold_seconds);
            bus_set_tts_at_5min(at_min);
            bus_set_tts_delta_5min(display_delta_minutes(at_min, current_min));
        }
    }
}

static void sync_core_data(const ArexDecoSchedule *schedule, const ArexDecoRuntimeStop *runtime_stop, const ArexDecoDiveState *output_state)
{
    uint32_t nofly_seconds = 0U;

    sync_tissue_data(output_state);
    bus_set_cns(round_u8_pct(s_state.oxygen_exposure.cns_percent));
    bus_set_otu(round_u16_float(s_state.oxygen_exposure.otu));
    bus_set_gf99(s_metrics.gf99_percent);
    bus_set_surf_gf(s_metrics.surface_gf_percent);
    bus_set_ceiling(s_metrics.ceiling_depth_m);
    (void)alarm_set_active(ALARM_ID_CRIT_CEIL_BROKEN, schedule != NULL && schedule->ceiling_violated != 0U);
    bus_set_tts(schedule != NULL ? round_up_minutes(schedule->tts_seconds) : 0U);
    if (arex_deco_nofly(&s_state, &nofly_seconds) == AREX_DECO_STATUS_OK)
    {
        bus_set_nofly_time(round_up_minutes(nofly_seconds));
    }
    sync_gas_data();
    sync_forecast_data();
    sync_stop_data(runtime_stop);
    sync_deco_plan_data(schedule);
}

static void sync_core_data_without_plan(const ArexDecoDiveState *output_state)
{
    uint32_t nofly_seconds = 0U;

    sync_tissue_data(output_state);
    bus_set_cns(round_u8_pct(s_state.oxygen_exposure.cns_percent));
    bus_set_otu(round_u16_float(s_state.oxygen_exposure.otu));
    bus_set_gf99(s_metrics.gf99_percent);
    bus_set_surf_gf(s_metrics.surface_gf_percent);
    bus_set_ceiling(s_metrics.ceiling_depth_m);
    if (arex_deco_nofly(&s_state, &nofly_seconds) == AREX_DECO_STATUS_OK)
    {
        bus_set_nofly_time(round_up_minutes(nofly_seconds));
    }
    sync_gas_data();
    sync_forecast_data();
}

static bool make_surface_air_state(const ArexDecoDiveState *source, ArexDecoDiveState *surface_state)
{
    if (source == NULL || surface_state == NULL) return false;
    *surface_state = *source;
    if (arex_deco_make_default_gas_plan(&surface_state->config, &surface_state->gas_plan) != AREX_DECO_STATUS_OK) return false;
    surface_state->gas_plan.active_gas_index = 0;
    surface_state->current_depth_m = 0.0f;
    return true;
}

static void refresh_current_outputs(void)
{
    ArexDecoSchedule schedule;
    ArexDecoGasRecommendation gas_rec;
    ArexDecoRuntimeStop runtime_stop;
    ArexDecoDiveState surface_state;
    ArexDecoDiveState planner_state;
    ArexDecoDiveState gas_rec_state;
    const ArexDecoDiveState *plan_state = &s_state;

    (void)memset(&schedule, 0, sizeof(schedule));
    (void)memset(&gas_rec, 0, sizeof(gas_rec));
    (void)memset(&runtime_stop, 0, sizeof(runtime_stop));
    if (s_surface_confirmed && make_surface_air_state(&s_state, &surface_state)) plan_state = &surface_state;
    planner_state = *plan_state;
    apply_runtime_ignored_gases(&planner_state);
    gas_rec_state = s_state;
    apply_runtime_ignored_gases(&gas_rec_state);
    ArexDecoStatus plan_status = arex_deco_plan(&planner_state, &schedule, NULL);
    ArexDecoStatus gas_status = s_surface_confirmed ? AREX_DECO_STATUS_INVALID_STATE : arex_deco_recommend_gas(&gas_rec_state, &gas_rec);
    debug_print_plan_call("refresh", -1, plan_status, &schedule, -1, planner_state.gas_plan.active_gas_index);
    if (plan_status == AREX_DECO_STATUS_OK) debug_print_schedule(&schedule);
    if (plan_status == AREX_DECO_STATUS_OK) (void)select_runtime_stop(&schedule, &runtime_stop);
    sync_gas_recommendation((gas_status == AREX_DECO_STATUS_OK) ? &gas_rec : NULL, &runtime_stop);
    if (plan_status == AREX_DECO_STATUS_OK) sync_core_data(&schedule, &runtime_stop, plan_state);
    else sync_core_data_without_plan(plan_state);
}

static void handle_pending_gas_switch(float depth_m)
{
    uint8_t target_gas_idx = 0U;
    ArexDecoStepInput input;
    ArexDecoDiveState next_state;

    (void)depth_m;
    if (!has_pending_gas_switch(&target_gas_idx)) return;

    if (target_gas_idx < s_state.gas_plan.gas_count)
    {
        runtime_restore_gas(target_gas_idx);
        ArexDecoGas *gas = &s_state.gas_plan.gases[target_gas_idx];
        if (gas->enabled != 0U && s_state.current_depth_m <= gas->max_depth_m + 0.1f)
        {
            (void)memset(&input, 0, sizeof(input));
            input.api_version = arex_deco_get_api_version();
            input.start_depth_m = s_state.current_depth_m;
            input.end_depth_m = s_state.current_depth_m;
            input.duration_seconds = 0U;
            input.gas_index = (int8_t)target_gas_idx;
            if (arex_deco_step(&s_state, &input, &next_state, &s_metrics) == AREX_DECO_STATUS_OK)
            {
                s_state = next_state;
                reset_runtime_stop_selector();
            }
        }
    }
    clear_gas_switch_cmd();
}

static void handle_pending_gas_ignore(void)
{
    uint8_t target_gas_idx = 0U;
    if (!has_pending_gas_ignore(&target_gas_idx)) return;
    runtime_ignore_gas(target_gas_idx);
    reset_runtime_stop_selector();
    clear_gas_ignore_cmd();
}

void deco_core_init(void)
{
    (void)ensure_initialized();
}

void deco_core_reset(void)
{
    s_initialized = false;
    s_runtime_ignored_gas_mask = 0U;
    clear_gas_ignore_cmd();
    reset_stop_progress();
    reset_runtime_stop_selector();
    s_temperature_valid = false;
    s_tts_forecast_elapsed_s = DECO_FORECAST_TTS_INTERVAL_S;
    s_runtime_stop_debug_last_print_ms = 0U;
    (void)ensure_initialized();
}

void deco_core_set_surface_confirmed(bool confirmed)
{
    s_surface_confirmed = confirmed;
    reset_runtime_stop_selector();
    reset_stop_progress();
    if (confirmed)
    {
        runtime_clear_ignored_gases();
        clear_gas_ignore_cmd();
    }
    if (confirmed && s_initialized) s_state.current_depth_m = 0.0f;
}

void deco_core_set_final_stop_depth(uint8_t depth_m)
{
    s_final_deco_stop_depth_m = (depth_m == 6U) ? 6U : 3U;
    if (ensure_initialized())
    {
        reset_runtime_stop_selector();
        apply_current_ui_config();
        refresh_current_outputs();
    }
    rt_kprintf("[DIVE_SETUP] Last deco stop: %um\n", (unsigned)s_final_deco_stop_depth_m);
}

void deco_core_set_gf(uint8_t gf_low_pct, uint8_t gf_high_pct)
{
    if (gf_low_pct > 100U) gf_low_pct = 100U;
    if (gf_high_pct > 100U) gf_high_pct = 100U;
    s_gf_low_pct = gf_low_pct;
    s_gf_high_pct = gf_high_pct;
    if (ensure_initialized())
    {
        reset_runtime_stop_selector();
        apply_current_ui_config();
        refresh_current_outputs();
    }
    rt_kprintf("[DIVE_SETUP] GF: %u/%u\n", (unsigned)s_gf_low_pct, (unsigned)s_gf_high_pct);
}

void deco_core_set_salinity_mode(uint8_t mode)
{
    s_salinity_mode = mode;
    if (ensure_initialized())
    {
        reset_runtime_stop_selector();
        apply_current_ui_config();
        refresh_current_outputs();
    }
    rt_kprintf("[DIVE_SETUP] Salinity mode: %u\n", (unsigned)mode);
}

void deco_core_set_safety_stop_mode(uint8_t mode)
{
    s_safety_stop_mode = mode;
    if (ensure_initialized())
    {
        reset_runtime_stop_selector();
        apply_current_ui_config();
        refresh_current_outputs();
    }
    rt_kprintf("[DIVE_SETUP] Safety stop mode: %s\n", ui_safety_stop_label(mode));
}

void deco_core_apply_gases_from_ui(void)
{
    if (ensure_initialized())
    {
        runtime_clear_ignored_gases();
        clear_gas_ignore_cmd();
        reset_runtime_stop_selector();
        apply_current_ui_config();
        refresh_current_outputs();
    }
}

float deco_core_calculate_gas_mod(uint8_t o2_pct, uint8_t he_pct, float max_ppo2)
{
    ArexDecoConfig config;
    ArexDecoGas gas;
    float mod_m = 0.0f;

    if (o2_pct == 0U || o2_pct > 100U || he_pct > 100U || (uint16_t)o2_pct + (uint16_t)he_pct > 100U) return 0.0f;
    if (!check_api_version_once()) return 0.0f;
    fill_config_from_ui(&config);
    (void)memset(&gas, 0, sizeof(gas));
    gas.oxygen_fraction = (float)o2_pct / 100.0f;
    gas.helium_fraction = (float)he_pct / 100.0f;
    gas.nitrogen_fraction = 1.0f - gas.oxygen_fraction - gas.helium_fraction;
    gas.max_ppo2_bar = max_ppo2;
    gas.enabled = 1U;
    if (arex_deco_calculate_gas_mod(&config, &gas, &mod_m) != AREX_DECO_STATUS_OK) return 0.0f;
    return mod_m;
}

bool deco_core_rtc_offline(uint32_t seconds)
{
    ArexDecoDiveState step_state;
    ArexDecoDiveState next_state;
    ArexDecoDiveState plan_state;
    ArexDecoGasPlan original_gas_plan;
    ArexDecoSchedule schedule;
    ArexDecoRuntimeStop runtime_stop;
    ArexDecoStepInput input;

    if (!ensure_initialized()) return false;
    if (seconds == 0U) return false;
    if (!s_surface_confirmed) return false;

    original_gas_plan = s_state.gas_plan;
    step_state = s_state;
    if (!make_surface_air_state(&s_state, &step_state)) return false;

    (void)memset(&input, 0, sizeof(input));
    input.api_version = arex_deco_get_api_version();
    input.start_depth_m = 0.0f;
    input.end_depth_m = 0.0f;
    input.duration_seconds = seconds;
    input.gas_index = 0;

    if (arex_deco_step(&step_state, &input, &next_state, &s_metrics) != AREX_DECO_STATUS_OK) return false;

    next_state.gas_plan = original_gas_plan;
    next_state.current_depth_m = 0.0f;
    s_state = next_state;

    if (!make_surface_air_state(&s_state, &plan_state)) return false;
    (void)memset(&schedule, 0, sizeof(schedule));
    (void)memset(&runtime_stop, 0, sizeof(runtime_stop));
    ArexDecoStatus plan_status = arex_deco_plan(&plan_state, &schedule, NULL);
    debug_print_plan_call("rtc_offline", AREX_DECO_STATUS_OK, plan_status, &schedule, input.gas_index, plan_state.gas_plan.active_gas_index);
    if (plan_status == AREX_DECO_STATUS_OK) debug_print_schedule(&schedule);
    if (plan_status == AREX_DECO_STATUS_OK) (void)select_runtime_stop(&schedule, &runtime_stop);
    sync_gas_recommendation(NULL, &runtime_stop);
    if (plan_status == AREX_DECO_STATUS_OK) sync_core_data(&schedule, &runtime_stop, &plan_state);
    else sync_core_data_without_plan(&plan_state);
    rt_kprintf("[RTC_OFFLINE] surface sleep %lus with AIR\n", (unsigned long)seconds);
    return true;
}

void deco_core_tick(float depth_m, float temperature_c, uint32_t delta_time_s)
{
    ArexDecoDiveState step_state;
    s_temperature_c = temperature_c;
    s_temperature_valid = isfinite(temperature_c) && temperature_c > -DECO_TEMPERATURE_KELVIN_OFFSET;
    if (!ensure_initialized()) return;
    if (delta_time_s == 0U) delta_time_s = 1U;
    if (depth_m < 0.0f) depth_m = 0.0f;

    handle_pending_gas_switch(depth_m);
    handle_pending_gas_ignore();

    ArexDecoStepInput input;
    ArexDecoDiveState next_state;
    (void)memset(&input, 0, sizeof(input));
    input.api_version = arex_deco_get_api_version();
    input.duration_seconds = delta_time_s;
    if (s_surface_confirmed)
    {
        if (!make_surface_air_state(&s_state, &step_state)) return;
        input.start_depth_m = 0.0f;
        input.end_depth_m = 0.0f;
        input.gas_index = 0;
    }
    else
    {
        step_state = s_state;
        input.start_depth_m = s_state.current_depth_m;
        input.end_depth_m = depth_m;
        input.gas_index = s_state.gas_plan.active_gas_index;
    }

    ArexDecoStatus step_status = arex_deco_step(&step_state, &input, &next_state, &s_metrics);
    if (step_status != AREX_DECO_STATUS_OK)
    {
        debug_print_plan_call("tick", (int)step_status, -1, NULL, input.gas_index, -1);
        rt_kprintf("[DECO] step failed, resetting core\n");
        deco_core_reset();
        return;
    }

    if (s_surface_confirmed)
    {
        next_state.gas_plan = s_state.gas_plan;
        next_state.current_depth_m = 0.0f;
    }
    s_state = next_state;
    if (s_tts_forecast_elapsed_s < DECO_FORECAST_TTS_INTERVAL_S)
    {
        uint32_t next_elapsed = s_tts_forecast_elapsed_s + delta_time_s;
        s_tts_forecast_elapsed_s = (next_elapsed > DECO_FORECAST_TTS_INTERVAL_S) ? DECO_FORECAST_TTS_INTERVAL_S : next_elapsed;
    }

    ArexDecoSchedule schedule;
    ArexDecoGasRecommendation gas_rec;
    ArexDecoRuntimeStop runtime_stop;
    ArexDecoDiveState plan_state;
    ArexDecoDiveState planner_state;
    ArexDecoDiveState gas_rec_state;
    const ArexDecoDiveState *plan_source = &s_state;
    (void)memset(&schedule, 0, sizeof(schedule));
    (void)memset(&gas_rec, 0, sizeof(gas_rec));
    (void)memset(&runtime_stop, 0, sizeof(runtime_stop));
    if (s_surface_confirmed && make_surface_air_state(&s_state, &plan_state)) plan_source = &plan_state;
    planner_state = *plan_source;
    apply_runtime_ignored_gases(&planner_state);
    gas_rec_state = s_state;
    apply_runtime_ignored_gases(&gas_rec_state);
    ArexDecoStatus plan_status = arex_deco_plan(&planner_state, &schedule, NULL);
    ArexDecoStatus gas_status = s_surface_confirmed ? AREX_DECO_STATUS_INVALID_STATE : arex_deco_recommend_gas(&gas_rec_state, &gas_rec);
    debug_print_plan_call("tick", (int)step_status, plan_status, &schedule, input.gas_index, planner_state.gas_plan.active_gas_index);
    if (plan_status == AREX_DECO_STATUS_OK)
    {
        debug_print_schedule(&schedule);
        (void)select_runtime_stop(&schedule, &runtime_stop);
    }
    sync_gas_recommendation((gas_status == AREX_DECO_STATUS_OK) ? &gas_rec : NULL, &runtime_stop);
    if (plan_status == AREX_DECO_STATUS_OK) sync_core_data(&schedule, &runtime_stop, plan_source);
    else sync_core_data_without_plan(plan_source);
}

static uint16_t gas_qty_l(float depth_m, uint32_t seconds, float rmv_lpm, const ArexDecoConfig *config)
{
    float minutes = (float)seconds / 60.0f;
    float pressure = pressure_bar_at_depth(config, depth_m) / config->surface_pressure_bar;
    return round_u16_float(rmv_lpm * pressure * minutes);
}

static uint32_t plan_stop_duration_sum_s(const ArexDecoSchedule *schedule)
{
    uint32_t stop_s = 0U;
    if (schedule == NULL) return 0U;
    for (uint8_t i = 0U; i < schedule->stop_count; i++) stop_s += schedule->stops[i].duration_seconds;
    return stop_s;
}

static float plan_ascent_display_distance_m(float start_depth_m, const ArexDecoSchedule *schedule)
{
    float distance_m = 0.0f;
    float from_depth_m = start_depth_m;
    if (schedule == NULL) return (start_depth_m > 0.0f) ? start_depth_m : 0.0f;

    for (uint8_t i = 0U; i < schedule->stop_count; i++)
    {
        const ArexDecoStop *stop = &schedule->stops[i];
        if (stop_display_suppressed(stop)) continue;
        if (stop_runtime_seconds(stop) == 0U) continue;
        if (stop->depth_m < from_depth_m)
        {
            distance_m += from_depth_m - stop->depth_m;
            from_depth_m = stop->depth_m;
        }
    }
    if (from_depth_m > 0.0f) distance_m += from_depth_m;
    return distance_m;
}

static uint32_t plan_ascent_segment_s(float from_depth_m,
                                      float to_depth_m,
                                      uint32_t total_ascent_s,
                                      float total_distance_m,
                                      uint32_t *used_ascent_s)
{
    uint32_t used_s = (used_ascent_s != NULL) ? *used_ascent_s : 0U;
    uint32_t remain_s = (total_ascent_s > used_s) ? (total_ascent_s - used_s) : 0U;
    uint32_t segment_s;
    float distance_m = from_depth_m - to_depth_m;

    if (remain_s == 0U || total_distance_m <= 0.0f || distance_m <= 0.0f) return 0U;

    segment_s = (uint32_t)(((float)total_ascent_s * distance_m / total_distance_m) + 0.5f);
    if (segment_s > remain_s) segment_s = remain_s;
    if (used_ascent_s != NULL) *used_ascent_s = used_s + segment_s;
    return segment_s;
}

static uint8_t append_plan_row(dive_plan_result_snapshot_t *snapshot, dive_plan_row_type_t type,
                               int16_t depth_m, uint16_t time_min, uint16_t run_min,
                               const ArexDecoGas *gas, uint16_t gas_l)
{
    if (snapshot->entry_count >= PLAN_MAX_ROWS)
    {
        return 0U;
    }
    dive_plan_row_t *row = &snapshot->rows[snapshot->entry_count++];
    row->type = type;
    row->depth_m = depth_m;
    row->time_min = time_min;
    row->run_min = run_min;
    row->o2_pct = round_u8_pct(gas->oxygen_fraction * 100.0f);
    row->he_pct = round_u8_pct(gas->helium_fraction * 100.0f);
    row->gas_l = gas_l;
    return 1U;
}

bool deco_core_plan_calculate(float depth_m, uint16_t bottom_time_min, float rmv_lpm, dive_plan_result_snapshot_t *out_snapshot)
{
    ArexDecoDiveState plan_state;
    ArexDecoDiveState next_state;
    ArexDecoRuntimeMetrics metrics;
    ArexDecoSchedule schedule;

    if (out_snapshot == NULL) return false;
    (void)memset(out_snapshot, 0, sizeof(*out_snapshot));
    if (!ensure_initialized()) return false;
    if (depth_m < 3.0f) depth_m = 3.0f;
    if (depth_m > 120.0f) depth_m = 120.0f;
    if (bottom_time_min < 1U) bottom_time_min = 1U;
    if (bottom_time_min > 300U) bottom_time_min = 300U;
    if (rmv_lpm < 5.0f) rmv_lpm = 5.0f;
    if (rmv_lpm > 50.0f) rmv_lpm = 50.0f;

    if (arex_deco_make_initial_dive_state(&plan_state) != AREX_DECO_STATUS_OK) return false;
    plan_state.config = s_state.config;
    plan_state.gas_plan = s_state.gas_plan;
    (void)arex_deco_reset_tissue_to_surface(&plan_state.config, &plan_state.gas_plan.gases[plan_state.gas_plan.active_gas_index], &plan_state.tissue);

    uint32_t descent_s = (uint32_t)((depth_m / PLAN_DESCENT_RATE_MPM) * 60.0f + 0.5f);
    ArexDecoStepInput input;
    (void)memset(&input, 0, sizeof(input));
    input.api_version = arex_deco_get_api_version();
    input.start_depth_m = 0.0f;
    input.end_depth_m = depth_m;
    input.duration_seconds = descent_s;
    input.gas_index = plan_state.gas_plan.active_gas_index;
    if (arex_deco_step(&plan_state, &input, &next_state, &metrics) != AREX_DECO_STATUS_OK) return false;
    plan_state = next_state;

    input.start_depth_m = depth_m;
    input.end_depth_m = depth_m;
    input.duration_seconds = (uint32_t)bottom_time_min * 60U;
    if (arex_deco_step(&plan_state, &input, &next_state, &metrics) != AREX_DECO_STATUS_OK) return false;
    plan_state = next_state;

    (void)memset(&schedule, 0, sizeof(schedule));
    if (arex_deco_plan(&plan_state, &schedule, NULL) != AREX_DECO_STATUS_OK) return false;

    uint32_t run_s = descent_s + (uint32_t)bottom_time_min * 60U;
    uint16_t total_deco_l = 0U;
    uint16_t total_ascent_l = 0U;
    const ArexDecoGas *bottom_gas = &plan_state.gas_plan.gases[plan_state.gas_plan.active_gas_index];
    const ArexDecoGas *ascent_gas = bottom_gas;
    uint16_t bottom_gas_l = gas_qty_l(depth_m, input.duration_seconds, rmv_lpm, &plan_state.config);
    uint32_t total_stop_s = plan_stop_duration_sum_s(&schedule);
    uint32_t total_ascent_s = (schedule.tts_seconds > total_stop_s) ? (schedule.tts_seconds - total_stop_s) : 0U;
    uint32_t used_ascent_s = 0U;
    float ascent_distance_m = plan_ascent_display_distance_m(depth_m, &schedule);
    float ascent_from_depth_m = depth_m;
    bool first_ascent_handled = false;
    (void)append_plan_row(out_snapshot, DIVE_PLAN_ROW_BOTTOM, (int16_t)(depth_m + 0.5f), bottom_time_min, round_up_minutes(run_s), bottom_gas, bottom_gas_l);

    for (uint8_t i = 0U; i < schedule.stop_count && out_snapshot->entry_count < PLAN_MAX_ROWS; i++)
    {
        const ArexDecoStop *stop = &schedule.stops[i];
        uint32_t runtime_s = stop_runtime_seconds(stop);
        bool display_stop = !stop_display_suppressed(stop) && runtime_s > 0U;
        int8_t gas_idx = stop->gas_index;
        if (gas_idx < 0 || gas_idx >= (int8_t)plan_state.gas_plan.gas_count) gas_idx = plan_state.gas_plan.active_gas_index;
        const ArexDecoGas *gas = &plan_state.gas_plan.gases[gas_idx];
        uint16_t stop_gas_l = gas_qty_l(stop->depth_m, stop->duration_seconds, rmv_lpm, &plan_state.config);

        if (display_stop && stop->depth_m < ascent_from_depth_m)
        {
            uint32_t segment_s = plan_ascent_segment_s(ascent_from_depth_m, stop->depth_m, total_ascent_s, ascent_distance_m, &used_ascent_s);
            uint16_t ascent_l = gas_qty_l((ascent_from_depth_m + stop->depth_m) * 0.5f, segment_s, rmv_lpm, &plan_state.config);
            run_s += segment_s;
            total_ascent_l = (uint16_t)(total_ascent_l + ascent_l);
            if (!first_ascent_handled && segment_s > 0U && out_snapshot->entry_count + 1U < PLAN_MAX_ROWS)
            {
                (void)append_plan_row(out_snapshot, DIVE_PLAN_ROW_ASCENT, (int16_t)(stop->depth_m + 0.5f), round_up_minutes(segment_s), round_up_minutes(run_s), ascent_gas, ascent_l);
            }
            first_ascent_handled = true;
            ascent_from_depth_m = stop->depth_m;
        }

        run_s += stop->duration_seconds;
        total_deco_l = (uint16_t)(total_deco_l + stop_gas_l);
        if (!display_stop) continue;
        (void)append_plan_row(out_snapshot, DIVE_PLAN_ROW_DECO_STOP, (int16_t)(stop->depth_m + 0.5f), round_up_minutes(runtime_s), round_up_minutes(run_s), gas, stop_gas_l);
        ascent_gas = gas;
    }

    uint32_t total_runtime_s = descent_s + (uint32_t)bottom_time_min * 60U + schedule.tts_seconds;
    if (total_ascent_s > used_ascent_s)
    {
        uint32_t surface_ascent_s = total_ascent_s - used_ascent_s;
        total_ascent_l = (uint16_t)(total_ascent_l + gas_qty_l(ascent_from_depth_m * 0.5f, surface_ascent_s, rmv_lpm, &plan_state.config));
    }

    out_snapshot->valid = 1U;
    out_snapshot->page = 0U;
    out_snapshot->total_runtime_min = round_up_minutes(total_runtime_s);
    out_snapshot->total_deco_min = 0U;
    for (uint8_t i = 0U; i < schedule.stop_count; i++) out_snapshot->total_deco_min = (uint16_t)(out_snapshot->total_deco_min + round_up_minutes(schedule.stops[i].duration_seconds));
    out_snapshot->total_gas_l = (uint16_t)(bottom_gas_l + total_deco_l + total_ascent_l);
    out_snapshot->cns_pct = round_u16_float(schedule.end_of_dive_exposure.cns_percent);
    out_snapshot->otu = round_u16_float(schedule.end_of_dive_exposure.otu);
    out_snapshot->total_pages = (uint8_t)((out_snapshot->entry_count + 7U) / 8U);
    if (out_snapshot->total_pages == 0U) out_snapshot->total_pages = 1U;
    out_snapshot->total_pages++;
    return true;
}
