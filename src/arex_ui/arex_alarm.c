#include "arex_alarm.h"
#include "arex_data.h"
#include "lvgl/lvgl.h"
#include <string.h>

#define AREX_ALARM_INFO_DISPLAY_MS  3000U
#define AREX_ALARM_BANNER_ROTATE_MS 3000U

typedef struct
{
    arex_alarm_id_t id;
    arex_alarm_level_t level;
    const char *text;
    arex_widget_id_t target;
    bool connected;
} arex_alarm_def_t;

typedef struct
{
    bool active;
    bool acked;
    uint32_t first_tick;
    uint32_t last_tick;
    uint32_t seq;
} arex_alarm_state_t;

typedef struct
{
    bool active;
    arex_alarm_level_t level;
    const char *text;
    arex_widget_id_t target;
    uint32_t first_tick;
    uint32_t seq;
} arex_alarm_custom_t;

static const arex_alarm_def_t s_alarm_defs[AREX_ALARM_ID_COUNT] =
{
    { AREX_ALARM_ID_CRIT_ASCENT_RATE,    AREX_ALARM_CRIT, "ASCENT TOO FAST",       WIDGET_DEPTH_1606,    true  },
    { AREX_ALARM_ID_CRIT_PO2_MAX,        AREX_ALARM_CRIT, "PO2 CRITICAL",          WIDGET_PPO2_0806,     true  },
    { AREX_ALARM_ID_CRIT_CEIL_BROKEN,    AREX_ALARM_CRIT, "CEILING BROKEN",        WIDGET_NDL_STOP_1606, true  },
    { AREX_ALARM_ID_CRIT_ALGO_LOCK,      AREX_ALARM_CRIT, "ALGORITHM LOCKED",      WIDGET_EMPTY,         false },
    { AREX_ALARM_ID_CRIT_TANK_EMPTY,     AREX_ALARM_CRIT, "TANK EMPTY",            WIDGET_POD_0806,      true  },
    { AREX_ALARM_ID_CRIT_BATTERY_DEAD,   AREX_ALARM_CRIT, "BATTERY DEAD",          WIDGET_BATTERY_0806,  true  },

    { AREX_ALARM_ID_WARN_PO2_ELEVATED,   AREX_ALARM_WARN, "HIGH PO2",              WIDGET_PPO2_0806,     true  },
    { AREX_ALARM_ID_WARN_NDL_LOW,        AREX_ALARM_WARN, "NDL LOW",               WIDGET_NDL_STOP_1606, true  },
    { AREX_ALARM_ID_WARN_CNS_HIGH,       AREX_ALARM_WARN, "HIGH CNS",              WIDGET_CNS_0806,      true  },
    { AREX_ALARM_ID_WARN_OTU_HIGH,       AREX_ALARM_WARN, "HIGH OTU",              WIDGET_OTU_0806,      true  },
    { AREX_ALARM_ID_WARN_SAFETY_BROKEN,  AREX_ALARM_WARN, "SAFETY BROKEN",         WIDGET_NDL_STOP_1606, true  },
    { AREX_ALARM_ID_WARN_TANK_TURN,      AREX_ALARM_WARN, "TURN PRESSURE",         WIDGET_POD_0806,      true  },
    { AREX_ALARM_ID_WARN_SIDEMOUNT_DIFF, AREX_ALARM_WARN, "TANK PRESSURE DIFF",    WIDGET_POD_0806,      true  },
    { AREX_ALARM_ID_WARN_DEPTH_LIMIT,    AREX_ALARM_WARN, "DEPTH LIMIT",           WIDGET_DEPTH_1606,    true  },
    { AREX_ALARM_ID_WARN_TIME_LIMIT,     AREX_ALARM_WARN, "TIME LIMIT",            WIDGET_DIVE_TIME_1606,true  },
    { AREX_ALARM_ID_WARN_BATTERY_LOW,    AREX_ALARM_WARN, "BATTERY LOW",           WIDGET_BATTERY_0806,  true  },
    { AREX_ALARM_ID_WARN_POD_LOST,       AREX_ALARM_WARN, "POD LOST",              WIDGET_POD_0806,      false },

    { AREX_ALARM_ID_INFO_SAFETY_STOP,    AREX_ALARM_INFO, "SAFETY STOP ACTIVE",    WIDGET_NDL_STOP_1606, true  },
    { AREX_ALARM_ID_INFO_GAS_SWITCH,     AREX_ALARM_INFO, "BETTER GAS AVAILABLE",  WIDGET_GAS_1606,      true  },
    { AREX_ALARM_ID_INFO_STOP_DONE,      AREX_ALARM_INFO, "STOP CLEARED",          WIDGET_NDL_STOP_1606, true  },
    { AREX_ALARM_ID_INFO_COMPASS_CALI,   AREX_ALARM_INFO, "CALIBRATE COMPASS",     WIDGET_HEADING_0806,  false },
};

static arex_alarm_state_t s_alarm_states[AREX_ALARM_ID_COUNT];
static arex_alarm_custom_t s_custom_alarm;
static arex_alarm_display_t s_display;
static uint32_t s_seq;
static int16_t s_display_key = -2;

static void alarm_mark_dirty(void)
{
    g_sensor_data.dirty_mask |= DIRTY_ALARM;
    s_display.revision++;
}

static uint32_t alarm_now(void)
{
    return lv_tick_get();
}

static bool alarm_target_add(arex_widget_id_t *targets, uint8_t *count,
                             uint8_t max_targets, arex_widget_id_t target)
{
    if (target == WIDGET_EMPTY || targets == NULL || count == NULL)
    {
        return false;
    }

    for (uint8_t i = 0; i < *count; i++)
    {
        if (targets[i] == target)
        {
            return false;
        }
    }

    if (*count >= max_targets)
    {
        return false;
    }

    targets[*count] = target;
    (*count)++;
    return true;
}

static arex_alarm_level_t alarm_highest_level(void)
{
    arex_alarm_level_t level = AREX_ALARM_NONE;

    for (uint8_t i = 0; i < AREX_ALARM_ID_COUNT; i++)
    {
        if (!s_alarm_states[i].active)
        {
            continue;
        }
        if (s_alarm_defs[i].level > level)
        {
            level = s_alarm_defs[i].level;
        }
    }

    if (s_custom_alarm.active && s_custom_alarm.level > level)
    {
        level = s_custom_alarm.level;
    }

    return level;
}

static uint8_t alarm_collect_level(arex_alarm_level_t level, int16_t *items, uint8_t max_items)
{
    uint8_t count = 0;

    for (uint8_t i = 0; i < AREX_ALARM_ID_COUNT && count < max_items; i++)
    {
        if (s_alarm_states[i].active &&
                s_alarm_defs[i].level == level)
        {
            items[count++] = (int16_t)i;
        }
    }

    if (s_custom_alarm.active && s_custom_alarm.level == level && count < max_items)
    {
        items[count++] = -1;
    }

    for (uint8_t i = 0; i < count; i++)
    {
        for (uint8_t j = (uint8_t)(i + 1U); j < count; j++)
        {
            uint32_t seq_i = (items[i] >= 0) ? s_alarm_states[items[i]].seq : s_custom_alarm.seq;
            uint32_t seq_j = (items[j] >= 0) ? s_alarm_states[items[j]].seq : s_custom_alarm.seq;
            if (seq_j < seq_i)
            {
                int16_t tmp = items[i];
                items[i] = items[j];
                items[j] = tmp;
            }
        }
    }

    return count;
}

static uint32_t alarm_level_first_tick(arex_alarm_level_t level)
{
    uint32_t first = 0xFFFFFFFFU;

    for (uint8_t i = 0; i < AREX_ALARM_ID_COUNT; i++)
    {
        if (s_alarm_states[i].active &&
                s_alarm_defs[i].level == level &&
                s_alarm_states[i].first_tick < first)
        {
            first = s_alarm_states[i].first_tick;
        }
    }

    if (s_custom_alarm.active &&
            s_custom_alarm.level == level &&
            s_custom_alarm.first_tick < first)
    {
        first = s_custom_alarm.first_tick;
    }

    return (first == 0xFFFFFFFFU) ? alarm_now() : first;
}

static void alarm_update_display(uint32_t now_ms)
{
    arex_alarm_level_t level = alarm_highest_level();
    int16_t old_key = s_display_key;

    if (level == AREX_ALARM_NONE)
    {
        s_display.visible = false;
        s_display.level = AREX_ALARM_NONE;
        s_display.text = NULL;
        s_display.banner_target = WIDGET_EMPTY;
        s_display_key = -2;
    }
    else
    {
        int16_t items[AREX_ALARM_ID_COUNT + 1];
        uint8_t count = alarm_collect_level(level, items, (uint8_t)(AREX_ALARM_ID_COUNT + 1));
        uint8_t pick = 0;
        if (count > 1U)
        {
            uint32_t first = alarm_level_first_tick(level);
            pick = (uint8_t)(((now_ms - first) / AREX_ALARM_BANNER_ROTATE_MS) % count);
        }

        int16_t key = (count > 0U) ? items[pick] : -2;
        s_display.visible = (key != -2);
        s_display.level = level;
        s_display_key = key;

        if (key >= 0)
        {
            s_display.text = s_alarm_defs[key].text;
            s_display.banner_target = s_alarm_defs[key].target;
        }
        else if (key == -1)
        {
            s_display.text = s_custom_alarm.text;
            s_display.banner_target = s_custom_alarm.target;
        }
    }

    if (old_key != s_display_key)
    {
        s_display.revision++;
    }
}

void arex_alarm_init(void)
{
    memset(s_alarm_states, 0, sizeof(s_alarm_states));
    memset(&s_custom_alarm, 0, sizeof(s_custom_alarm));
    memset(&s_display, 0, sizeof(s_display));
    s_display.level = AREX_ALARM_NONE;
    s_display.banner_target = WIDGET_EMPTY;
    s_seq = 0;
    s_display_key = -2;
}

bool arex_alarm_set_active(arex_alarm_id_t id, bool active)
{
    if (id >= AREX_ALARM_ID_COUNT)
    {
        return false;
    }

    arex_alarm_state_t *state = &s_alarm_states[id];
    uint32_t now = alarm_now();

    if (state->active == active)
    {
        if (active)
        {
            state->last_tick = now;
        }
        return false;
    }

    state->active = active;
    state->acked = false;
    state->last_tick = now;

    if (active)
    {
        state->first_tick = now;
        state->seq = ++s_seq;
    }
    else
    {
        state->first_tick = 0;
        state->seq = 0;
    }

    alarm_mark_dirty();
    return true;
}

bool arex_alarm_raise_custom(arex_alarm_level_t level,
                             const char *text,
                             arex_widget_id_t target)
{
    if (level == AREX_ALARM_NONE)
    {
        return false;
    }

    s_custom_alarm.active = true;
    s_custom_alarm.level = level;
    s_custom_alarm.text = text;
    s_custom_alarm.target = target;
    s_custom_alarm.first_tick = alarm_now();
    s_custom_alarm.seq = ++s_seq;
    alarm_mark_dirty();
    return true;
}

void arex_alarm_clear_all(void)
{
    memset(s_alarm_states, 0, sizeof(s_alarm_states));
    memset(&s_custom_alarm, 0, sizeof(s_custom_alarm));
    alarm_mark_dirty();
}

bool arex_alarm_ack_current(void)
{
    if (s_display_key >= 0)
    {
        s_alarm_states[s_display_key].acked = true;
        alarm_mark_dirty();
        return true;
    }

    if (s_display_key == -1)
    {
        alarm_mark_dirty();
        return true;
    }

    return false;
}

void arex_alarm_tick(uint32_t now_ms)
{
    bool changed = false;

    for (uint8_t i = 0; i < AREX_ALARM_ID_COUNT; i++)
    {
        if (s_alarm_states[i].active &&
                s_alarm_defs[i].level == AREX_ALARM_INFO &&
                now_ms - s_alarm_states[i].first_tick >= AREX_ALARM_INFO_DISPLAY_MS)
        {
            s_alarm_states[i].active = false;
            changed = true;
        }
    }

    if (s_custom_alarm.active &&
            s_custom_alarm.level == AREX_ALARM_INFO &&
            now_ms - s_custom_alarm.first_tick >= AREX_ALARM_INFO_DISPLAY_MS)
    {
        s_custom_alarm.active = false;
        changed = true;
    }

    alarm_update_display(now_ms);

    if (changed)
    {
        alarm_mark_dirty();
    }
}

const arex_alarm_display_t *arex_alarm_get_display(void)
{
    return &s_display;
}

uint8_t arex_alarm_get_active_targets(arex_alarm_level_t level,
                                      arex_widget_id_t *targets,
                                      uint8_t max_targets)
{
    uint8_t count = 0;

    if (level == AREX_ALARM_INFO || level == AREX_ALARM_NONE)
    {
        return 0;
    }

    for (uint8_t i = 0; i < AREX_ALARM_ID_COUNT; i++)
    {
        if (s_alarm_states[i].active &&
                s_alarm_defs[i].level == level)
        {
            (void)alarm_target_add(targets, &count, max_targets, s_alarm_defs[i].target);
        }
    }

    if (s_custom_alarm.active && s_custom_alarm.level == level)
    {
        (void)alarm_target_add(targets, &count, max_targets, s_custom_alarm.target);
    }

    return count;
}
