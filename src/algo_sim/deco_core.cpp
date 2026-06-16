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
#define GAS_DENSITY_O2_G_L 1.428f
#define GAS_DENSITY_N2_G_L 1.251f
#define GAS_DENSITY_HE_G_L 0.179f
#define DECO_SCHEDULE_DEBUG_PRINT_MS 1000U
#define DECO_SCHEDULE_DEBUG_MAX_STOPS 6U
#define DECO_PLAN_CALL_DEBUG 1U               /* 打印每次 step/plan 调用结果 */
#define DECO_HIDE_SWITCH_ONLY_STOPS 1U        /* 隐藏 planner 返回的纯切气预测站 */
#define DECO_CEILING_ACTIVE_M 0.01f           /* ceiling 大于该值即认为有实时减压义务 */
#define DECO_STOP_ZONE_DEEP_MARGIN_M 1.5f     /* 减压站允许比显示站深的范围 */
#define SAFETY_STOP_ZONE_SHALLOW_M 2.4f       /* 安停区浅侧边界 */
#define SAFETY_STOP_ZONE_DEEP_M 6.1f          /* 安停区深侧边界 */
#define TISSUE_UI_PAMB_ANCHOR_PERMILLE 400.0f /* 归一化组织图环境压力锚点 */
#define TISSUE_UI_MVALUE_ANCHOR_PERMILLE 900.0f /* 归一化组织图 M 值锚点 */
#define TISSUE_UI_MAX_PERMILLE 1000.0f        /* 归一化组织图绘制上限 */
#define TISSUE_UI_RECON_EPS 0.0001f           /* M 值反推除零保护 */

static ArexDecoDiveState s_state;
static ArexDecoRuntimeMetrics s_metrics;
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

typedef struct
{
    bool active;
    stop_type_t type;
    float depth_m;
    bool was_in_stop_zone;
    uint16_t total_s;
} stop_progress_t;

static stop_progress_t s_stop_progress;

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

static bool safety_stop_zone_active(float current_depth_m)
{
    return current_depth_m >= SAFETY_STOP_ZONE_SHALLOW_M && current_depth_m <= SAFETY_STOP_ZONE_DEEP_M;
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

static void debug_print_plan_call(const char *tag, int step_status, int plan_status, const ArexDecoSchedule *schedule)
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

    if (!failed && (uint32_t)(now_ms - s_plan_call_debug_last_print_ms) < DECO_SCHEDULE_DEBUG_PRINT_MS)
    {
        return;
    }
    s_plan_call_debug_last_print_ms = now_ms;

    rt_kprintf("[AREX_CALL] %s depth=%.1fm step=%s(%d) plan=%s(%d) ndl=%lus/%umin ceiling=%.2fm stops=%u tts=%lus cv=%u\n",
               tag ? tag : "plan",
               s_state.current_depth_m,
               deco_status_name(step_status),
               step_status,
               deco_status_name((int)plan_status),
               plan_status,
               (unsigned long)ndl_s,
               (unsigned)ndl_min,
               s_metrics.ceiling_depth_m,
               (unsigned)stop_count,
               (unsigned long)tts_s,
               (unsigned)cv);
#else
    (void)tag;
    (void)step_status;
    (void)plan_status;
    (void)schedule;
#endif
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

    rt_kprintf("[AREX_PLAN] depth=%.1fm ceiling=%.2fm cv=%u tts=%lus stops=%u",
               (double)s_state.current_depth_m,
               (double)s_metrics.ceiling_depth_m,
               (unsigned)schedule->ceiling_violated,
               (unsigned long)schedule->tts_seconds,
               (unsigned)schedule->stop_count);
    for (uint8_t i = 0U; i < print_count; i++)
    {
        const ArexDecoStop *stop = &schedule->stops[i];
        rt_kprintf(" | #%u %.2fm dur=%lus hold=%lus sw=%lus gas=%d gf=%.2f",
                   (unsigned)(i + 1U),
                   (double)stop->depth_m,
                   (unsigned long)stop->duration_seconds,
                   (unsigned long)stop->hold_seconds,
                   (unsigned long)stop->switch_penalty_seconds,
                   (int)stop->gas_index,
                   (double)stop->target_gf);
    }
    if (schedule->stop_count > print_count) rt_kprintf(" | ...");
    rt_kprintf("\n");
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

static bool stop_is_switch_only(const ArexDecoStop *stop)
{
    return stop != NULL &&
           stop->duration_seconds > 0U &&
           stop->hold_seconds == 0U &&
           stop->switch_penalty_seconds > 0U;
}

static uint32_t stop_runtime_seconds(const ArexDecoStop *stop)
{
    if (stop == NULL) return 0U;
#if DECO_HIDE_SWITCH_ONLY_STOPS
    if (stop_is_switch_only(stop)) return 0U;
#endif
    return stop->duration_seconds;
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
}

static bool fill_gas_plan_from_ui(const ArexDecoConfig *config, ArexDecoGasPlan *gas_plan)
{
    uint8_t source_count = bus_get_gas_slot_count();
    uint8_t valid_count = 0U;
    int8_t active_idx = 0;

    (void)memset(gas_plan, 0, sizeof(*gas_plan));
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
        valid_count++;
    }

    if (valid_count == 0U)
    {
        if (arex_deco_make_default_gas_plan(config, gas_plan) != AREX_DECO_STATUS_OK) return false;
        gas_plan->active_gas_index = 0;
        return true;
    }

    gas_plan->gas_count = valid_count;
    gas_plan->active_gas_index = (active_idx >= 0 && active_idx < (int8_t)valid_count) ? active_idx : 0;
    return true;
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
    if (s_initialized) return true;
    if (!check_api_version_once()) return false;

    if (arex_deco_make_initial_dive_state(&s_state) != AREX_DECO_STATUS_OK) return false;
    (void)memset(&s_metrics, 0, sizeof(s_metrics));
    apply_current_ui_config();
    (void)arex_deco_reset_tissue_to_surface(&s_state.config, &s_state.gas_plan.gases[s_state.gas_plan.active_gas_index], &s_state.tissue);
    s_initialized = true;
    rt_kprintf("Arex deco core initialized (GF: %u/%u)\n", (unsigned)s_gf_low_pct, (unsigned)s_gf_high_pct);
    return true;
}

static void sync_tissue_data(void)
{
    ArexDecoTissueGradientMetrics gradients;
    int16_t tissue_raw[AREX_DECO_COMPARTMENT_COUNT];
    uint8_t tissue_gf[AREX_DECO_COMPARTMENT_COUNT];
    uint16_t tissue_bar_permille[AREX_DECO_COMPARTMENT_COUNT];
    float tissue_n2_bar[AREX_DECO_COMPARTMENT_COUNT];
    float tissue_m_value_bar[AREX_DECO_COMPARTMENT_COUNT];

    if (arex_deco_calculate_tissue_gradients(&s_state, &gradients) != AREX_DECO_STATUS_OK) return;

    float ambient_pressure_bar = pressure_bar_at_depth(&s_state.config, s_state.current_depth_m);
    int8_t active_idx = s_state.gas_plan.active_gas_index;
    if (active_idx < 0 || active_idx >= (int8_t)s_state.gas_plan.gas_count) active_idx = 0;
    float n2_fraction = (active_idx >= 0 && active_idx < (int8_t)s_state.gas_plan.gas_count) ? s_state.gas_plan.gases[active_idx].nitrogen_fraction : 0.0f;
    float dry_pressure_bar = ambient_pressure_bar - s_state.config.water_vapor_pressure_bar;
    if (!isfinite(dry_pressure_bar) || dry_pressure_bar < 0.0f) dry_pressure_bar = 0.0f;
    float inspired_n2_bar = dry_pressure_bar * n2_fraction;
    uint16_t pi_permille = (ambient_pressure_bar > TISSUE_UI_RECON_EPS) ? round_u16_permille((inspired_n2_bar / ambient_pressure_bar) * TISSUE_UI_PAMB_ANCHOR_PERMILLE) : 0U;

    for (uint8_t i = 0U; i < AREX_DECO_COMPARTMENT_COUNT; i++)
    {
        tissue_raw[i] = round_i16_pct(gradients.absolute_gf_percent[i]);
        tissue_gf[i] = round_u8_pct(gradients.relative_gf_percent[i]);
        if (tissue_gf[i] > 200U) tissue_gf[i] = 200U;

        tissue_n2_bar[i] = s_state.tissue.nitrogen_bar[i];
        float tissue_total_inert_bar = s_state.tissue.nitrogen_bar[i] + s_state.tissue.helium_bar[i];
        float absolute_gf = gradients.absolute_gf_percent[i];
        tissue_m_value_bar[i] = ambient_pressure_bar;
        if (isfinite(absolute_gf) && fabsf(absolute_gf) > TISSUE_UI_RECON_EPS) tissue_m_value_bar[i] = ambient_pressure_bar + ((tissue_total_inert_bar - ambient_pressure_bar) * 100.0f / absolute_gf);
        if (!isfinite(tissue_m_value_bar[i]) || tissue_m_value_bar[i] < ambient_pressure_bar) tissue_m_value_bar[i] = ambient_pressure_bar;

        if (ambient_pressure_bar <= TISSUE_UI_RECON_EPS)
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

    bus_set_tissue_loads(tissue_raw, tissue_gf, target_gf_percent_from_core(gradients.current_target_gf));
    bus_set_tissue_normalized_payload(tissue_bar_permille, pi_permille, ambient_pressure_bar, inspired_n2_bar, tissue_n2_bar, tissue_m_value_bar);
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

    float n2_fraction = active_gas->nitrogen_fraction;
    float gas_density = (active_gas->oxygen_fraction * GAS_DENSITY_O2_G_L + n2_fraction * GAS_DENSITY_N2_G_L + active_gas->helium_fraction * GAS_DENSITY_HE_G_L) * pressure_bar / s_state.config.surface_pressure_bar;
    bus_set_gas_density(gas_density);

    for (uint8_t i = 0U; i < s_state.gas_plan.gas_count && i < GAS_COUNT; i++)
    {
        bus_set_ppo2(i, s_state.gas_plan.gases[i].oxygen_fraction * pressure_bar);
    }
}

static void sync_gas_recommendation(const ArexDecoGasRecommendation *gas_rec)
{
    int8_t recommended_idx = -1;

    if (bus_get_dive_time_s() > 0U &&
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

static const ArexDecoStop *first_runtime_stop(const ArexDecoSchedule *schedule)
{
    if (schedule == NULL) return NULL;
    for (uint8_t i = 0U; i < schedule->stop_count; i++)
    {
        const ArexDecoStop *stop = &schedule->stops[i];
        if (stop->depth_m > 0.0f && stop_runtime_seconds(stop) > 0U) return stop;
    }
    return NULL;
}

static void sync_stop_data(const ArexDecoSchedule *schedule)
{
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

    const ArexDecoStop *runtime_stop = first_runtime_stop(schedule);

    if (runtime_stop != NULL && s_metrics.ceiling_depth_m > DECO_CEILING_ACTIVE_M)
    {
        uint32_t runtime_s = stop_runtime_seconds(runtime_stop);
        stop_type = STOP_DECO;
        stop_depth_m = runtime_stop->depth_m;
        stop_left_s = (runtime_s > 65535U) ? 65535U : (uint16_t)runtime_s;
        in_stop_zone = deco_stop_zone_active(s_state.current_depth_m, stop_depth_m);
        ndl_min = 0;
    }
    else if (runtime_stop != NULL)
    {
        uint32_t runtime_s = stop_runtime_seconds(runtime_stop);
        stop_depth_m = runtime_stop->depth_m;
        stop_left_s = (runtime_s > 65535U) ? 65535U : (uint16_t)runtime_s;
        stop_type = STOP_SAFETY;
        in_stop_zone = safety_stop_zone_active(s_state.current_depth_m);
    }
    stop_total_s = sync_stop_progress_total(stop_type, stop_depth_m, stop_left_s, in_stop_zone);
    bus_update_deco(ndl_min, stop_type, stop_depth_m, stop_total_s, stop_left_s, in_stop_zone);
    if (stop_type == STOP_NONE)
    {
        uint8_t bar = (ndl_min <= 0) ? 0U : (uint8_t)((ndl_min > 99 ? 99 : ndl_min) * 100 / 99);
        bus_set_ndl_bar_pct(bar);
    }
}

static void sync_core_data(const ArexDecoSchedule *schedule)
{
    uint32_t nofly_seconds = 0U;

    sync_tissue_data();
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
    sync_stop_data(schedule);
    sync_deco_plan_data(schedule);
}

static void sync_core_data_without_plan(void)
{
    uint32_t nofly_seconds = 0U;

    sync_tissue_data();
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
}

static void refresh_current_outputs(void)
{
    ArexDecoSchedule schedule;
    ArexDecoGasRecommendation gas_rec;

    (void)memset(&schedule, 0, sizeof(schedule));
    (void)memset(&gas_rec, 0, sizeof(gas_rec));
    ArexDecoStatus plan_status = arex_deco_plan(&s_state, &schedule, NULL);
    ArexDecoStatus gas_status = arex_deco_recommend_gas(&s_state, &gas_rec);
    debug_print_plan_call("refresh", -1, plan_status, &schedule);
    if (plan_status == AREX_DECO_STATUS_OK) debug_print_schedule(&schedule);
    sync_gas_recommendation((gas_status == AREX_DECO_STATUS_OK) ? &gas_rec : NULL);
    if (plan_status == AREX_DECO_STATUS_OK) sync_core_data(&schedule);
    else sync_core_data_without_plan();
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
            }
        }
    }
    clear_gas_switch_cmd();
}

void deco_core_init(void)
{
    (void)ensure_initialized();
}

void deco_core_reset(void)
{
    s_initialized = false;
    reset_stop_progress();
    (void)ensure_initialized();
}

void deco_core_set_final_stop_depth(uint8_t depth_m)
{
    s_final_deco_stop_depth_m = (depth_m == 6U) ? 6U : 3U;
    if (ensure_initialized())
    {
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
        apply_current_ui_config();
        refresh_current_outputs();
    }
    rt_kprintf("[DIVE_SETUP] Safety stop mode: %s\n", ui_safety_stop_label(mode));
}

void deco_core_apply_gases_from_ui(void)
{
    if (ensure_initialized())
    {
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
    ArexDecoGasPlan original_gas_plan;
    ArexDecoGasRecommendation gas_rec;
    ArexDecoSchedule schedule;
    ArexDecoStepInput input;

    if (!ensure_initialized()) return false;
    if (seconds == 0U) return false;
    if (s_state.current_depth_m > 0.30f) return false;

    original_gas_plan = s_state.gas_plan;
    step_state = s_state;
    if (arex_deco_make_default_gas_plan(&step_state.config, &step_state.gas_plan) != AREX_DECO_STATUS_OK) return false;
    step_state.gas_plan.active_gas_index = 0;
    step_state.current_depth_m = 0.0f;

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

    (void)memset(&schedule, 0, sizeof(schedule));
    (void)memset(&gas_rec, 0, sizeof(gas_rec));
    ArexDecoStatus plan_status = arex_deco_plan(&s_state, &schedule, NULL);
    ArexDecoStatus gas_status = arex_deco_recommend_gas(&s_state, &gas_rec);
    debug_print_plan_call("rtc_offline", AREX_DECO_STATUS_OK, plan_status, &schedule);
    if (plan_status == AREX_DECO_STATUS_OK) debug_print_schedule(&schedule);
    sync_gas_recommendation((gas_status == AREX_DECO_STATUS_OK) ? &gas_rec : NULL);
    if (plan_status == AREX_DECO_STATUS_OK) sync_core_data(&schedule);
    else sync_core_data_without_plan();
    rt_kprintf("[RTC_OFFLINE] surface sleep %lus with AIR\n", (unsigned long)seconds);
    return true;
}

void deco_core_tick(float depth_m, float temperature_c, uint32_t delta_time_s)
{
    (void)temperature_c;
    if (!ensure_initialized()) return;
    if (delta_time_s == 0U) delta_time_s = 1U;
    if (depth_m < 0.0f) depth_m = 0.0f;

    handle_pending_gas_switch(depth_m);

    ArexDecoStepInput input;
    ArexDecoDiveState next_state;
    (void)memset(&input, 0, sizeof(input));
    input.api_version = arex_deco_get_api_version();
    input.start_depth_m = s_state.current_depth_m;
    input.end_depth_m = depth_m;
    input.duration_seconds = delta_time_s;
    input.gas_index = s_state.gas_plan.active_gas_index;

    ArexDecoStatus step_status = arex_deco_step(&s_state, &input, &next_state, &s_metrics);
    if (step_status != AREX_DECO_STATUS_OK)
    {
        debug_print_plan_call("tick", (int)step_status, -1, NULL);
        rt_kprintf("[DECO] step failed, resetting core\n");
        deco_core_reset();
        return;
    }

    s_state = next_state;

    ArexDecoSchedule schedule;
    ArexDecoGasRecommendation gas_rec;
    (void)memset(&schedule, 0, sizeof(schedule));
    (void)memset(&gas_rec, 0, sizeof(gas_rec));
    ArexDecoStatus plan_status = arex_deco_plan(&s_state, &schedule, NULL);
    ArexDecoStatus gas_status = arex_deco_recommend_gas(&s_state, &gas_rec);
    debug_print_plan_call("tick", (int)step_status, plan_status, &schedule);
    if (plan_status == AREX_DECO_STATUS_OK)
    {
        debug_print_schedule(&schedule);
    }
    sync_gas_recommendation((gas_status == AREX_DECO_STATUS_OK) ? &gas_rec : NULL);
    if (plan_status == AREX_DECO_STATUS_OK) sync_core_data(&schedule);
    else sync_core_data_without_plan();
}

static uint16_t gas_qty_l(float depth_m, uint32_t seconds, float rmv_lpm, const ArexDecoConfig *config)
{
    float minutes = (float)seconds / 60.0f;
    float pressure = pressure_bar_at_depth(config, depth_m) / config->surface_pressure_bar;
    return round_u16_float(rmv_lpm * pressure * minutes);
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
    const ArexDecoGas *bottom_gas = &plan_state.gas_plan.gases[plan_state.gas_plan.active_gas_index];
    uint16_t bottom_gas_l = gas_qty_l(depth_m, input.duration_seconds, rmv_lpm, &plan_state.config);
    (void)append_plan_row(out_snapshot, DIVE_PLAN_ROW_BOTTOM, (int16_t)(depth_m + 0.5f), bottom_time_min, round_up_minutes(run_s), bottom_gas, bottom_gas_l);

    for (uint8_t i = 0U; i < schedule.stop_count && out_snapshot->entry_count < PLAN_MAX_ROWS; i++)
    {
        const ArexDecoStop *stop = &schedule.stops[i];
        uint32_t runtime_s = stop_runtime_seconds(stop);
        int8_t gas_idx = stop->gas_index;
        if (gas_idx < 0 || gas_idx >= (int8_t)plan_state.gas_plan.gas_count) gas_idx = plan_state.gas_plan.active_gas_index;
        const ArexDecoGas *gas = &plan_state.gas_plan.gases[gas_idx];
        uint16_t stop_gas_l = gas_qty_l(stop->depth_m, stop->duration_seconds, rmv_lpm, &plan_state.config);
        run_s += stop->duration_seconds;
        total_deco_l = (uint16_t)(total_deco_l + stop_gas_l);
        if (runtime_s == 0U) continue;
        (void)append_plan_row(out_snapshot, DIVE_PLAN_ROW_DECO_STOP, (int16_t)(stop->depth_m + 0.5f), round_up_minutes(runtime_s), round_up_minutes(run_s), gas, stop_gas_l);
    }

    uint32_t total_runtime_s = descent_s + (uint32_t)bottom_time_min * 60U + schedule.tts_seconds;
    uint32_t ascent_s = schedule.tts_seconds;
    if (schedule.stop_count > 0U)
    {
        uint32_t stop_s = 0U;
        for (uint8_t i = 0U; i < schedule.stop_count; i++) stop_s += schedule.stops[i].duration_seconds;
        ascent_s = (schedule.tts_seconds > stop_s) ? (schedule.tts_seconds - stop_s) : 0U;
    }
    uint16_t ascent_gas_l = gas_qty_l(depth_m * 0.5f, ascent_s, rmv_lpm, &plan_state.config);
    (void)append_plan_row(out_snapshot, DIVE_PLAN_ROW_ASCENT, 0, round_up_minutes(ascent_s), round_up_minutes(total_runtime_s), bottom_gas, ascent_gas_l);

    out_snapshot->valid = 1U;
    out_snapshot->page = 0U;
    out_snapshot->total_runtime_min = round_up_minutes(total_runtime_s);
    out_snapshot->total_deco_min = 0U;
    for (uint8_t i = 0U; i < schedule.stop_count; i++) out_snapshot->total_deco_min = (uint16_t)(out_snapshot->total_deco_min + round_up_minutes(schedule.stops[i].duration_seconds));
    out_snapshot->total_gas_l = (uint16_t)(bottom_gas_l + total_deco_l + ascent_gas_l);
    out_snapshot->cns_pct = round_u16_float(schedule.end_of_dive_exposure.cns_percent);
    out_snapshot->otu = round_u16_float(schedule.end_of_dive_exposure.otu);
    out_snapshot->total_pages = (uint8_t)((out_snapshot->entry_count + 7U) / 8U);
    if (out_snapshot->total_pages == 0U) out_snapshot->total_pages = 1U;
    out_snapshot->total_pages++;
    return true;
}
