/*
 * 文件: src/app_ui/ui/alarm/alarm.c
 * 作用: 该文件属于闹钟界面模块，负责闹钟数据、视图构建、交互刷新或与上层 UI 状态之间的衔接。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时建议先理解该文件在 app_ui 流程中的位置，再修改注释所描述的职责边界，避免把数据准备、状态切换和视图渲染职责混杂到一起。
 */

#include "alarm.h"
#include "../core/data.h"
#include "../core/ui_state.h"
#include "lvgl/lvgl.h"
#include <string.h>

#define ALARM_INFO_DISPLAY_MS  5000U
#define ALARM_BANNER_ROTATE_MS 3000U

typedef enum
{
    ALARM_MODE_CONDITION = 0,
    ALARM_MODE_AUTO_TIMEOUT,
    ALARM_MODE_CONFIRM_ACTION,
} alarm_mode_t;

typedef struct
{
    alarm_id_t id;
    alarm_level_t level;
    const char *text;
    comp_id_t target;
    bool connected;
    alarm_mode_t mode;
} alarm_def_t;

typedef struct
{
    bool active;
    bool banner_acked;
    bool target_acked;
    uint32_t first_tick;
    uint32_t last_tick;
    uint32_t seq;
} alarm_state_t;

typedef struct
{
    bool active;
    alarm_level_t level;
    const char *text;
    comp_id_t target;
    uint32_t first_tick;
    uint32_t seq;
    bool banner_acked;
    bool target_acked;
    alarm_mode_t mode;
} alarm_custom_t;

static const alarm_def_t s_alarm_defs[ALARM_ID_COUNT] =
{
    { ALARM_ID_CRIT_ASCENT_RATE,    ALARM_CRIT, "ASCENT TOO FAST",       COMP_ASCENT_0806,   true,  ALARM_MODE_CONDITION },
    { ALARM_ID_CRIT_PO2_MAX,        ALARM_CRIT, "PO2 CRITICAL",          COMP_PPO2_0806,     true,  ALARM_MODE_CONDITION },
    { ALARM_ID_CRIT_PO2_MIN,        ALARM_CRIT, "PO2 TOO LOW",           COMP_PPO2_0806,     true,  ALARM_MODE_CONDITION },
    { ALARM_ID_CRIT_CEIL_BROKEN,    ALARM_CRIT, "CEILING BROKEN",        COMP_NDL_STOP_1606, true,  ALARM_MODE_CONDITION },
    { ALARM_ID_CRIT_ALGO_LOCK,      ALARM_CRIT, "ALGORITHM LOCKED",      COMP_EMPTY,         false, ALARM_MODE_CONDITION },
    { ALARM_ID_CRIT_TANK_EMPTY,     ALARM_CRIT, "TANK EMPTY",            COMP_POD_0806,      true,  ALARM_MODE_CONDITION },
    { ALARM_ID_CRIT_BATTERY_DEAD,   ALARM_CRIT, "BATTERY DEAD",          COMP_BATTERY_0806,  true,  ALARM_MODE_CONDITION },

    { ALARM_ID_WARN_PO2_ELEVATED,   ALARM_WARN, "HIGH PO2",              COMP_PPO2_0806,     true,  ALARM_MODE_CONDITION },
    { ALARM_ID_WARN_NDL_LOW,        ALARM_WARN, "NDL LOW",               COMP_NDL_STOP_1606, true,  ALARM_MODE_CONDITION },
    { ALARM_ID_WARN_CNS_HIGH,       ALARM_WARN, "HIGH CNS",              COMP_CNS_0806,      true,  ALARM_MODE_CONDITION },
    { ALARM_ID_WARN_OTU_HIGH,       ALARM_WARN, "HIGH OTU",              COMP_OTU_0806,      true,  ALARM_MODE_CONDITION },
    { ALARM_ID_WARN_SAFETY_BROKEN,  ALARM_WARN, "SAFETY BROKEN",         COMP_NDL_STOP_1606, true,  ALARM_MODE_CONDITION },
    { ALARM_ID_WARN_TANK_TURN,      ALARM_WARN, "TURN PRESSURE",         COMP_POD_0806,      true,  ALARM_MODE_CONDITION },
    { ALARM_ID_WARN_SIDEMOUNT_DIFF, ALARM_WARN, "TANK PRESSURE DIFF",    COMP_POD_0806,      true,  ALARM_MODE_CONDITION },
    { ALARM_ID_WARN_DEPTH_LIMIT,    ALARM_WARN, "DEPTH LIMIT",           COMP_DEPTH_1606,    true,  ALARM_MODE_CONDITION },
    { ALARM_ID_WARN_TIME_LIMIT,     ALARM_WARN, "TIME LIMIT",            COMP_DIVE_TIME_1606,true,  ALARM_MODE_CONDITION },
    { ALARM_ID_WARN_BATTERY_LOW,    ALARM_WARN, "BATTERY LOW",           COMP_BATTERY_0806,  true,  ALARM_MODE_CONDITION },
    { ALARM_ID_WARN_POD_LOST,       ALARM_WARN, "POD LOST",              COMP_POD_0806,      false, ALARM_MODE_CONDITION },

    { ALARM_ID_INFO_SAFETY_STOP,    ALARM_INFO, "SAFETY STOP ACTIVE",    COMP_NDL_STOP_1606, true,  ALARM_MODE_AUTO_TIMEOUT },
    { ALARM_ID_INFO_GAS_SWITCH,     ALARM_INFO, "BETTER GAS AVAILABLE",  COMP_GAS_1606,      true,  ALARM_MODE_CONFIRM_ACTION },
    { ALARM_ID_INFO_STOP_DONE,      ALARM_INFO, "STOP DONE",             COMP_NDL_STOP_1606, true,  ALARM_MODE_AUTO_TIMEOUT },
    { ALARM_ID_INFO_COMPASS_CALI,   ALARM_INFO, "CALIBRATE COMPASS",     COMP_HEADING_0806,  false, ALARM_MODE_CONDITION },
};

static alarm_state_t s_alarm_states[ALARM_ID_COUNT];
static alarm_custom_t s_custom_alarm;
static alarm_display_t s_display;
static comp_id_t s_visible_targets[ALARM_VISIBLE_TARGET_MAX];
static uint8_t s_visible_target_count;
static uint32_t s_seq;
static int16_t s_display_key = -2;
static int8_t s_gas_switch_prompt_idx = -1;
static float s_gas_switch_prompt_depth_m;
static float s_gas_switch_prompt_mod_m;
static bool s_gas_switch_depth_hidden;

static void alarm_mark_dirty(void)
{
    bus_requeue_dirty(DIRTY_ALARM);
    s_display.revision++;
}

static uint32_t alarm_now(void)
{
    return lv_tick_get();
}

static bool alarm_target_equivalent(comp_id_t visible, comp_id_t target)
{
    if (visible == target)
    {
        return true;
    }

#if ALARM_TARGET_MATCH_DEPTH_1612
    return (target == COMP_DEPTH_1606 && visible == COMP_DEPTH_1612) ||
           (target == COMP_DEPTH_1612 && visible == COMP_DEPTH_1606);
#else
    return false;
#endif
}

static bool alarm_target_is_visible(comp_id_t target)
{
    if (target == COMP_EMPTY)
    {
        return false;
    }
    for (uint8_t i = 0U; i < s_visible_target_count; i++)
    {
        if (alarm_target_equivalent(s_visible_targets[i], target))
        {
            return true;
        }
    }
    return false;
}

static void alarm_state_reset(alarm_state_t *state)
{
    if (state == NULL) return;
    state->active = false;
    state->banner_acked = false;
    state->target_acked = false;
    state->first_tick = 0U;
    state->last_tick = 0U;
    state->seq = 0U;
}

static void alarm_gas_switch_reset(void)
{
    s_gas_switch_prompt_idx = -1;
    s_gas_switch_prompt_depth_m = 0.0f;
    s_gas_switch_prompt_mod_m = 0.0f;
    s_gas_switch_depth_hidden = false;
}

static bool alarm_gas_switch_accept_active(void)
{
    int8_t recommended_idx = bus_get_recommended_gas_idx();
    if (recommended_idx < 0)
    {
        alarm_gas_switch_reset();
        return false;
    }

    if (recommended_idx != s_gas_switch_prompt_idx)
    {
        s_gas_switch_prompt_idx = recommended_idx;
        s_gas_switch_prompt_depth_m = bus_get_depth();
        s_gas_switch_prompt_mod_m = bus_get_gas_slot_mod_m((uint8_t)recommended_idx);
        s_gas_switch_depth_hidden = false;
    }
    else if (s_gas_switch_depth_hidden)
    {
        float hide_base_m = (s_gas_switch_prompt_mod_m > 0.0f) ? s_gas_switch_prompt_mod_m : s_gas_switch_prompt_depth_m;
        if (bus_get_depth() > hide_base_m - ALARM_GAS_SWITCH_PROMPT_EXIT_DELTA_M)
        {
            s_gas_switch_depth_hidden = false;
        }
    }

    return !s_gas_switch_depth_hidden;
}

static bool alarm_is_active(alarm_id_t id)
{
    return id < ALARM_ID_COUNT && s_alarm_states[id].active;
}

static bool alarm_banner_candidate(uint8_t id)
{
    if (!alarm_is_active(id) || s_alarm_states[id].banner_acked)
    {
        return false;
    }
    if (s_alarm_defs[id].level == ALARM_WARN && alarm_target_is_visible(s_alarm_defs[id].target))
    {
        return false;
    }
    return true;
}

static bool alarm_custom_banner_candidate(void)
{
    if (!s_custom_alarm.active || s_custom_alarm.banner_acked)
    {
        return false;
    }
    if (s_custom_alarm.level == ALARM_WARN && alarm_target_is_visible(s_custom_alarm.target))
    {
        return false;
    }
    return true;
}

static alarm_level_t alarm_highest_banner_level(void)
{
    alarm_level_t level = ALARM_NONE;

    for (uint8_t i = 0U; i < ALARM_ID_COUNT; i++)
    {
        if (alarm_banner_candidate(i) && s_alarm_defs[i].level > level)
        {
            level = s_alarm_defs[i].level;
        }
    }

    if (alarm_custom_banner_candidate() && s_custom_alarm.level > level)
    {
        level = s_custom_alarm.level;
    }

    return level;
}

static uint8_t alarm_collect_banner_level(alarm_level_t level, int16_t *items, uint8_t max_items)
{
    uint8_t count = 0U;

    for (uint8_t i = 0U; i < ALARM_ID_COUNT && count < max_items; i++)
    {
        if (alarm_banner_candidate(i) && s_alarm_defs[i].level == level)
        {
            items[count++] = (int16_t)i;
        }
    }

    if (alarm_custom_banner_candidate() && s_custom_alarm.level == level && count < max_items)
    {
        items[count++] = -1;
    }

    for (uint8_t i = 0U; i < count; i++)
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

static uint32_t alarm_banner_level_first_tick(alarm_level_t level)
{
    uint32_t first = UINT32_MAX;

    for (uint8_t i = 0U; i < ALARM_ID_COUNT; i++)
    {
        if (alarm_banner_candidate(i) && s_alarm_defs[i].level == level && s_alarm_states[i].first_tick < first)
        {
            first = s_alarm_states[i].first_tick;
        }
    }

    if (alarm_custom_banner_candidate() && s_custom_alarm.level == level && s_custom_alarm.first_tick < first)
    {
        first = s_custom_alarm.first_tick;
    }

    return (first == UINT32_MAX) ? alarm_now() : first;
}

static void alarm_update_display(uint32_t now_ms)
{
    alarm_level_t level = alarm_highest_banner_level();
    int16_t old_key = s_display_key;

    if (level == ALARM_NONE)
    {
        s_display.visible = false;
        s_display.level = ALARM_NONE;
        s_display.text = NULL;
        s_display.banner_target = COMP_EMPTY;
        s_display_key = -2;
    }
    else
    {
        int16_t items[ALARM_ID_COUNT + 1];
        uint8_t count = alarm_collect_banner_level(level, items, (uint8_t)(ALARM_ID_COUNT + 1));
        uint8_t pick = 0U;
        if (count > 1U)
        {
            uint32_t first = alarm_banner_level_first_tick(level);
            pick = (uint8_t)(((now_ms - first) / ALARM_BANNER_ROTATE_MS) % count);
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

static int16_t alarm_first_confirmable_key(void)
{
    alarm_level_t best_level = ALARM_NONE;
    int16_t best_key = -2;
    uint32_t best_seq = UINT32_MAX;

    if (s_display_key != -2)
    {
        return s_display_key;
    }

    for (uint8_t i = 0U; i < ALARM_ID_COUNT; i++)
    {
        if (!s_alarm_states[i].active)
        {
            continue;
        }
        if (s_alarm_defs[i].level >= ALARM_CRIT && s_alarm_states[i].banner_acked)
        {
            continue;
        }
        if (s_alarm_defs[i].level < ALARM_CRIT &&
            s_alarm_states[i].banner_acked &&
            s_alarm_states[i].target_acked)
        {
            continue;
        }
        if (s_alarm_defs[i].level > best_level || (s_alarm_defs[i].level == best_level && s_alarm_states[i].seq < best_seq))
        {
            best_level = s_alarm_defs[i].level;
            best_key = (int16_t)i;
            best_seq = s_alarm_states[i].seq;
        }
    }

    if (s_custom_alarm.active &&
        !((s_custom_alarm.level >= ALARM_CRIT && s_custom_alarm.banner_acked) ||
          (s_custom_alarm.level < ALARM_CRIT && s_custom_alarm.banner_acked && s_custom_alarm.target_acked)) &&
        (s_custom_alarm.level > best_level || (s_custom_alarm.level == best_level && s_custom_alarm.seq < best_seq)))
    {
        best_key = -1;
    }

    return best_key;
}

static bool alarm_confirm_key(int16_t key)
{
    if (key >= 0)
    {
        alarm_state_t *state = &s_alarm_states[key];
        if (!state->active)
        {
            return false;
        }

        if ((alarm_id_t)key == ALARM_ID_INFO_GAS_SWITCH)
        {
            int8_t gas_idx = bus_get_recommended_gas_idx();
            if (gas_idx >= 0 && gas_idx < (int8_t)bus_get_gas_slot_count())
            {
                request_gas_switch((uint8_t)gas_idx);
            }
            bus_set_recommended_gas_idx(-1);
            alarm_state_reset(state);
            s_gas_switch_prompt_idx = gas_idx;
            s_gas_switch_depth_hidden = true;
            alarm_mark_dirty();
            return true;
        }

        state->banner_acked = true;
        if (s_alarm_defs[key].level < ALARM_CRIT)
        {
            state->target_acked = true;
        }
        if (s_alarm_defs[key].level == ALARM_INFO)
        {
            alarm_state_reset(state);
        }
        alarm_mark_dirty();
        return true;
    }

    if (key == -1 && s_custom_alarm.active)
    {
        s_custom_alarm.banner_acked = true;
        if (s_custom_alarm.level < ALARM_CRIT)
        {
            s_custom_alarm.target_acked = true;
        }
        if (s_custom_alarm.level == ALARM_INFO)
        {
            memset(&s_custom_alarm, 0, sizeof(s_custom_alarm));
        }
        alarm_mark_dirty();
        return true;
    }

    return false;
}

void alarm_init(void)
{
    memset(s_alarm_states, 0, sizeof(s_alarm_states));
    memset(&s_custom_alarm, 0, sizeof(s_custom_alarm));
    memset(&s_display, 0, sizeof(s_display));
    memset(s_visible_targets, 0, sizeof(s_visible_targets));
    s_display.level = ALARM_NONE;
    s_display.banner_target = COMP_EMPTY;
    s_visible_target_count = 0U;
    s_seq = 0U;
    s_display_key = -2;
    alarm_gas_switch_reset();
}

bool alarm_set_active(alarm_id_t id, bool active)
{
    if (id >= ALARM_ID_COUNT)
    {
        return false;
    }

    alarm_state_t *state = &s_alarm_states[id];
    uint32_t now = alarm_now();
    bool accepted_active = active;

    if (id == ALARM_ID_INFO_GAS_SWITCH)
    {
        accepted_active = active && alarm_gas_switch_accept_active();
        if (!active)
        {
            alarm_gas_switch_reset();
        }
    }

    if (state->active == accepted_active)
    {
        if (accepted_active)
        {
            state->last_tick = now;
        }
        return false;
    }

    if (!accepted_active)
    {
        alarm_state_reset(state);
        alarm_mark_dirty();
        return true;
    }

    state->active = true;
    state->banner_acked = false;
    state->target_acked = false;
    state->first_tick = now;
    state->last_tick = now;
    state->seq = ++s_seq;
    alarm_mark_dirty();
    return true;
}

bool alarm_raise_custom(alarm_level_t level, const char *text, comp_id_t target)
{
    if (level == ALARM_NONE)
    {
        return false;
    }

    s_custom_alarm.active = true;
    s_custom_alarm.level = level;
    s_custom_alarm.text = text;
    s_custom_alarm.target = target;
    s_custom_alarm.first_tick = alarm_now();
    s_custom_alarm.seq = ++s_seq;
    s_custom_alarm.banner_acked = false;
    s_custom_alarm.target_acked = false;
    s_custom_alarm.mode = (level == ALARM_INFO) ? ALARM_MODE_AUTO_TIMEOUT : ALARM_MODE_CONDITION;
    alarm_mark_dirty();
    return true;
}

bool alarm_clear_custom(void)
{
    if (!s_custom_alarm.active)
    {
        return false;
    }

    memset(&s_custom_alarm, 0, sizeof(s_custom_alarm));
    alarm_mark_dirty();
    return true;
}

void alarm_clear_all(void)
{
    memset(s_alarm_states, 0, sizeof(s_alarm_states));
    memset(&s_custom_alarm, 0, sizeof(s_custom_alarm));
    alarm_gas_switch_reset();
    alarm_mark_dirty();
}

bool alarm_set_acknowledged(alarm_id_t id, bool acknowledged)
{
    if (id >= ALARM_ID_COUNT || !s_alarm_states[id].active)
    {
        return false;
    }

    if (s_alarm_states[id].banner_acked == acknowledged && s_alarm_states[id].target_acked == acknowledged)
    {
        return false;
    }

    s_alarm_states[id].banner_acked = acknowledged;
    s_alarm_states[id].target_acked = acknowledged;
    alarm_mark_dirty();
    return true;
}

bool alarm_current_requires_ack(void)
{
    return alarm_first_confirmable_key() != -2;
}

bool alarm_confirm_current(void)
{
    return alarm_confirm_key(alarm_first_confirmable_key());
}

bool alarm_ack_current(void)
{
    return alarm_confirm_current();
}

bool alarm_display_is(alarm_id_t id)
{
    return id < ALARM_ID_COUNT && s_display_key == (int16_t)id;
}

void alarm_tick(uint32_t now_ms)
{
    bool changed = false;

    for (uint8_t i = 0U; i < ALARM_ID_COUNT; i++)
    {
        if (s_alarm_states[i].active && s_alarm_defs[i].mode == ALARM_MODE_AUTO_TIMEOUT && now_ms - s_alarm_states[i].first_tick >= ALARM_INFO_DISPLAY_MS)
        {
            alarm_state_reset(&s_alarm_states[i]);
            changed = true;
        }
    }

    float gas_switch_hide_base_m = (s_gas_switch_prompt_mod_m > 0.0f) ? s_gas_switch_prompt_mod_m : s_gas_switch_prompt_depth_m;
    if (s_alarm_states[ALARM_ID_INFO_GAS_SWITCH].active &&
        s_gas_switch_prompt_idx >= 0 &&
        bus_get_depth() <= (gas_switch_hide_base_m - ALARM_GAS_SWITCH_PROMPT_EXIT_DELTA_M))
    {
        request_gas_ignore((uint8_t)s_gas_switch_prompt_idx);
        alarm_state_reset(&s_alarm_states[ALARM_ID_INFO_GAS_SWITCH]);
        s_gas_switch_depth_hidden = true;
        changed = true;
    }

    if (s_custom_alarm.active &&
        s_custom_alarm.mode == ALARM_MODE_AUTO_TIMEOUT &&
        now_ms - s_custom_alarm.first_tick >= ALARM_INFO_DISPLAY_MS)
    {
        memset(&s_custom_alarm, 0, sizeof(s_custom_alarm));
        changed = true;
    }

    alarm_update_display(now_ms);

    if (changed)
    {
        alarm_mark_dirty();
    }
}

const alarm_display_t *alarm_get_display(void)
{
    return &s_display;
}

void alarm_set_visible_targets(const comp_id_t *targets, uint8_t count)
{
    if (count > ALARM_VISIBLE_TARGET_MAX)
    {
        count = ALARM_VISIBLE_TARGET_MAX;
    }

    s_visible_target_count = count;
    if (count > 0U && targets != NULL)
    {
        memcpy(s_visible_targets, targets, count * sizeof(s_visible_targets[0]));
    }
}

static bool alarm_effect_add(alarm_target_effect_entry_t *entries,
                             uint8_t *count,
                             uint8_t max_entries,
                             comp_id_t target,
                             alarm_level_t level,
                             alarm_target_effect_t effect)
{
    if (entries == NULL || count == NULL || target == COMP_EMPTY || effect == ALARM_TARGET_EFFECT_NONE)
    {
        return false;
    }

    for (uint8_t i = 0U; i < *count; i++)
    {
        if (entries[i].target == target)
        {
            if (level > entries[i].level || (level == entries[i].level && effect > entries[i].effect))
            {
                entries[i].level = level;
                entries[i].effect = effect;
            }
            return true;
        }
    }

    if (*count >= max_entries)
    {
        return false;
    }

    entries[*count].target = target;
    entries[*count].level = level;
    entries[*count].effect = effect;
    (*count)++;
    return true;
}

uint8_t alarm_get_target_effects(alarm_target_effect_entry_t *entries, uint8_t max_entries)
{
    uint8_t count = 0U;

    for (uint8_t i = 0U; i < ALARM_ID_COUNT; i++)
    {
        alarm_target_effect_t effect = ALARM_TARGET_EFFECT_NONE;
        if (!s_alarm_states[i].active || s_alarm_defs[i].level == ALARM_INFO)
        {
            continue;
        }

        if (s_alarm_defs[i].level >= ALARM_CRIT)
        {
            effect = ALARM_TARGET_EFFECT_CRIT_FLASH;
        }
        else if (s_alarm_defs[i].level == ALARM_WARN)
        {
            effect = ALARM_TARGET_EFFECT_WARN_BREATHE;
        }

        (void)alarm_effect_add(entries, &count, max_entries, s_alarm_defs[i].target, s_alarm_defs[i].level, effect);
    }

    if (s_custom_alarm.active && s_custom_alarm.level != ALARM_INFO)
    {
        alarm_target_effect_t effect = (s_custom_alarm.level >= ALARM_CRIT)
                                       ? ALARM_TARGET_EFFECT_CRIT_FLASH
                                       : ALARM_TARGET_EFFECT_WARN_BREATHE;
        (void)alarm_effect_add(entries, &count, max_entries, s_custom_alarm.target, s_custom_alarm.level, effect);
    }

    return count;
}
