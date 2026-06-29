/*
 * UI/runtime setting constants shared by UI bus, simulator, and real-device bridges.
 * Keep user-visible option tables here to avoid simulator/firmware drift.
 */

#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UI_CONSERVATISM_DEFAULT_LEVEL  1U
#define UI_CONSERVATISM_PROFILE_COUNT  4U

#define UI_LOG_RATE_DEFAULT_S      2U
#define UI_LOG_RATE_OPTION_COUNT   4U

#define UI_SAFETY_STOP_OFF     0U
#define UI_SAFETY_STOP_3MIN    1U
#define UI_SAFETY_STOP_4MIN    2U
#define UI_SAFETY_STOP_5MIN    3U
#define UI_SAFETY_STOP_ADAPT   4U
#define UI_SAFETY_STOP_CNTUP   5U
#define UI_SAFETY_STOP_COUNT   6U
#define UI_SAFETY_STOP_DEFAULT UI_SAFETY_STOP_3MIN

#define UI_SURFACE_CONFIRM_MIN_MIN      1U   /* 潜水结束判定最短分钟数 */
#define UI_SURFACE_CONFIRM_MAX_MIN      10U  /* 潜水结束判定最长分钟数 */
#define UI_SURFACE_CONFIRM_DEFAULT_MIN  5U   /* 潜水结束判定默认分钟数 */
#define UI_DIVE_START_DEPTH_MIN_M       0.3f /* 入水判定最浅深度 */
#define UI_DIVE_START_DEPTH_MAX_M       2.0f /* 入水判定最深深度 */
#define UI_DIVE_START_DEPTH_STEP_M      0.1f /* 入水判定步进深度 */
#define UI_DIVE_START_DEPTH_DEFAULT_M   1.6f /* 入水判定默认深度 */

#define UI_TEMP_UNIT_C         0U
#define UI_TEMP_UNIT_F         1U
#define UI_TEMP_UNIT_DEFAULT   UI_TEMP_UNIT_C

#define UI_UNITS_METRIC        0U
#define UI_UNITS_IMPERIAL      1U
#define UI_DEFAULT_USE_FT      0U
#if UI_DEFAULT_USE_FT
#define UI_UNITS_DEFAULT       UI_UNITS_IMPERIAL
#else
#define UI_UNITS_DEFAULT       UI_UNITS_METRIC
#endif
#define UI_METERS_TO_FEET      3.28084f

#define UI_ASCENT_RATE_DISPLAY_EPSILON_MPM  0.2f
#define UI_ASCENT_RATE_SAMPLE_PERIOD_MS     2000UL
#define UI_ASCENT_RATE_STALE_TIMEOUT_MS     3000UL
#define UI_ASCENT_RATE_DEPTH_DEADBAND_M     0.10f
#define UI_ASCENT_RATE_STILL_DEADBAND_MPM   3.0f

static inline bool ui_gf_from_conservatism_level(uint8_t level,
                                                 uint8_t *gf_low,
                                                 uint8_t *gf_high)
{
    static const uint8_t gf_table[UI_CONSERVATISM_PROFILE_COUNT][2] =
    {
        { 40U, 95U },
        { 40U, 85U },
        { 30U, 70U },
        { 50U, 70U },
    };

    if ((gf_low == NULL) || (gf_high == NULL))
    {
        return false;
    }

    if (level >= UI_CONSERVATISM_PROFILE_COUNT)
    {
        level = UI_CONSERVATISM_DEFAULT_LEVEL;
    }

    *gf_low = gf_table[level][0];
    *gf_high = gf_table[level][1];
    return true;
}

static inline uint8_t ui_conservatism_from_gf(uint8_t gf_low, uint8_t gf_high)
{
    if (gf_low == 40U && gf_high == 95U) return 0U;
    if (gf_low == 40U && gf_high == 85U) return 1U;
    if (gf_low == 30U && gf_high == 70U) return 2U;
    return 3U;
}

static inline uint8_t ui_log_rate_option(uint8_t index)
{
    static const uint8_t rate_table[UI_LOG_RATE_OPTION_COUNT] =
    {
        2U, 5U, 10U, 30U
    };

    if (index >= UI_LOG_RATE_OPTION_COUNT)
    {
        return UI_LOG_RATE_DEFAULT_S;
    }
    return rate_table[index];
}

static inline bool ui_log_rate_is_valid(uint8_t seconds)
{
    for (uint8_t i = 0U; i < UI_LOG_RATE_OPTION_COUNT; i++)
    {
        if (seconds == ui_log_rate_option(i))
        {
            return true;
        }
    }
    return false;
}

static inline uint8_t ui_next_log_rate(uint8_t current_seconds)
{
    for (uint8_t i = 0U; i < UI_LOG_RATE_OPTION_COUNT; i++)
    {
        if (current_seconds == ui_log_rate_option(i))
        {
            return ui_log_rate_option((uint8_t)((i + 1U) % UI_LOG_RATE_OPTION_COUNT));
        }
    }
    return ui_log_rate_option(0U);
}

static inline const char *ui_safety_stop_label(uint8_t mode)
{
    switch (mode)
    {
    case UI_SAFETY_STOP_OFF:
        return "OFF";
    case UI_SAFETY_STOP_3MIN:
        return "3MIN";
    case UI_SAFETY_STOP_4MIN:
        return "4MIN";
    case UI_SAFETY_STOP_5MIN:
        return "5MIN";
    case UI_SAFETY_STOP_ADAPT:
        return "ADAPT";
    case UI_SAFETY_STOP_CNTUP:
        return "CNTUP";
    default:
        return "--";
    }
}

static inline const char *ui_temp_unit_label(uint8_t unit)
{
    return (unit == UI_TEMP_UNIT_F) ? "F" : "C";
}

static inline float ui_temp_display_from_c(float temp_c, uint8_t unit)
{
    return (unit == UI_TEMP_UNIT_F) ? (temp_c * 9.0f / 5.0f + 32.0f) : temp_c;
}

static inline const char *ui_depth_unit_label(uint8_t units_mode)
{
    return (units_mode == UI_UNITS_IMPERIAL) ? "ft" : "m";
}

static inline const char *ui_depth_units_label(uint8_t units_mode)
{
    return (units_mode == UI_UNITS_IMPERIAL) ? "feet" : "meters";
}

static inline float ui_depth_display_from_m(float depth_m, uint8_t units_mode)
{
    return (units_mode == UI_UNITS_IMPERIAL) ? (depth_m * UI_METERS_TO_FEET) : depth_m;
}

#ifdef __cplusplus
}
#endif

#endif /* UI_SETTINGS_H */
