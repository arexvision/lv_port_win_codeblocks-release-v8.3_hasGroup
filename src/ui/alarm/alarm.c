/*
 * 文件: src/app_ui/ui/alarm/alarm.c
 * 作用: 该文件属于闹钟界面模块，负责闹钟数据、视图构建、交互刷新或与上层 UI 状态之间的衔接。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时建议先理解该文件在 app_ui 流程中的位置，再修改注释所描述的职责边界，避免把数据准备、状态切换和视图渲染职责混杂到一起。
 */

#include "alarm.h"
#include "../core/data.h"
#include "lvgl/lvgl.h"
#include <string.h>

#define ALARM_INFO_DISPLAY_MS  3000U
#define ALARM_BANNER_ROTATE_MS 3000U

typedef enum
{
    ALARM_CLEAR_CONDITION_ONLY = 0, /* 条件解除前强制驻留，ACK 不隐藏 */
    ALARM_CLEAR_ACK_HIDE,           /* ACK 后隐藏，直到条件先解除再重新触发 */
    ALARM_CLEAR_AUTO_TIMEOUT        /* 通知类：自动超时隐藏 */
} alarm_clear_policy_t;

typedef struct
{
    alarm_id_t id;
    alarm_level_t level;
    const char *text;
    comp_id_t target;
    bool connected;
    alarm_clear_policy_t clear_policy;
} alarm_def_t;

typedef struct
{
    bool active;
    bool acked;
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
    bool acked;
    alarm_clear_policy_t clear_policy;
} alarm_custom_t;

static const alarm_def_t s_alarm_defs[ALARM_ID_COUNT] =
{
    { ALARM_ID_CRIT_ASCENT_RATE,    ALARM_CRIT, "ASCENT TOO FAST",       COMP_DEPTH_1606,    true,  ALARM_CLEAR_CONDITION_ONLY },
    { ALARM_ID_CRIT_PO2_MAX,        ALARM_CRIT, "PO2 CRITICAL",          COMP_PPO2_0806,     true,  ALARM_CLEAR_CONDITION_ONLY },
    { ALARM_ID_CRIT_CEIL_BROKEN,    ALARM_CRIT, "CEILING BROKEN",        COMP_NDL_STOP_1606, true,  ALARM_CLEAR_CONDITION_ONLY },
    { ALARM_ID_CRIT_ALGO_LOCK,      ALARM_CRIT, "ALGORITHM LOCKED",      COMP_EMPTY,         false, ALARM_CLEAR_CONDITION_ONLY },
    { ALARM_ID_CRIT_TANK_EMPTY,     ALARM_CRIT, "TANK EMPTY",            COMP_POD_0806,      true,  ALARM_CLEAR_CONDITION_ONLY },
    { ALARM_ID_CRIT_BATTERY_DEAD,   ALARM_CRIT, "BATTERY DEAD",          COMP_BATTERY_0806,  true,  ALARM_CLEAR_CONDITION_ONLY },

    { ALARM_ID_WARN_PO2_ELEVATED,   ALARM_WARN, "HIGH PO2",              COMP_PPO2_0806,     true,  ALARM_CLEAR_ACK_HIDE },
    { ALARM_ID_WARN_NDL_LOW,        ALARM_WARN, "NDL LOW",               COMP_NDL_STOP_1606, true,  ALARM_CLEAR_ACK_HIDE },
    { ALARM_ID_WARN_CNS_HIGH,       ALARM_WARN, "HIGH CNS",              COMP_CNS_0806,      true,  ALARM_CLEAR_ACK_HIDE },
    { ALARM_ID_WARN_OTU_HIGH,       ALARM_WARN, "HIGH OTU",              COMP_OTU_0806,      true,  ALARM_CLEAR_ACK_HIDE },
    { ALARM_ID_WARN_SAFETY_BROKEN,  ALARM_WARN, "SAFETY BROKEN",         COMP_NDL_STOP_1606, true,  ALARM_CLEAR_ACK_HIDE },
    { ALARM_ID_WARN_TANK_TURN,      ALARM_WARN, "TURN PRESSURE",         COMP_POD_0806,      true,  ALARM_CLEAR_ACK_HIDE },
    { ALARM_ID_WARN_SIDEMOUNT_DIFF, ALARM_WARN, "TANK PRESSURE DIFF",    COMP_POD_0806,      true,  ALARM_CLEAR_ACK_HIDE },
    { ALARM_ID_WARN_DEPTH_LIMIT,    ALARM_WARN, "DEPTH LIMIT",           COMP_DEPTH_1606,    true,  ALARM_CLEAR_ACK_HIDE },
    { ALARM_ID_WARN_TIME_LIMIT,     ALARM_WARN, "TIME LIMIT",            COMP_DIVE_TIME_1606,true,  ALARM_CLEAR_ACK_HIDE },
    { ALARM_ID_WARN_BATTERY_LOW,    ALARM_WARN, "BATTERY LOW",           COMP_BATTERY_0806,  true,  ALARM_CLEAR_ACK_HIDE },
    { ALARM_ID_WARN_POD_LOST,       ALARM_WARN, "POD LOST",              COMP_POD_0806,      false, ALARM_CLEAR_ACK_HIDE },

    /* L1 prompts are event-style notifications unless a future owner needs a modal flow. */
    { ALARM_ID_INFO_SAFETY_STOP,    ALARM_INFO, "SAFETY STOP ACTIVE",    COMP_NDL_STOP_1606, true,  ALARM_CLEAR_AUTO_TIMEOUT },
    { ALARM_ID_INFO_GAS_SWITCH,     ALARM_INFO, "BETTER GAS AVAILABLE",  COMP_GAS_1606,      true,  ALARM_CLEAR_CONDITION_ONLY },
    { ALARM_ID_INFO_STOP_DONE,      ALARM_INFO, "STOP CLEARED",          COMP_NDL_STOP_1606, true,  ALARM_CLEAR_AUTO_TIMEOUT },
    { ALARM_ID_INFO_COMPASS_CALI,   ALARM_INFO, "CALIBRATE COMPASS",     COMP_HEADING_0806,  false, ALARM_CLEAR_CONDITION_ONLY },
};

static alarm_state_t s_alarm_states[ALARM_ID_COUNT];
static alarm_custom_t s_custom_alarm;
static alarm_display_t s_display;
static uint32_t s_seq;
static int16_t s_display_key = -2;

static void alarm_mark_dirty(void)
{
    /* 告警显示内容发生变化后，通过脏标记通知 UI 层刷新告警区域。 */
    bus_requeue_dirty(DIRTY_ALARM);
    s_display.revision++;
}

static uint32_t alarm_now(void)
{
    /* 统一使用 LVGL tick 作为告警时间基准，便于和 UI 动画节奏保持一致。 */
    return lv_tick_get();
}

static bool alarm_is_ack_hidden(uint8_t id)
{
    /* ACK_HIDE 策略表示用户确认后先隐藏，但条件未解除前仍保留内部状态。 */
    return s_alarm_states[id].acked &&
           s_alarm_defs[id].clear_policy == ALARM_CLEAR_ACK_HIDE;
}

static bool alarm_is_displayable(uint8_t id)
{
    /* 活跃且未被 ACK 隐藏的告警，才允许进入展示候选集。 */
    return s_alarm_states[id].active && !alarm_is_ack_hidden(id);
}

static bool alarm_custom_is_displayable(void)
{
    /* 自定义告警走和标准告警相同的显示资格判断逻辑。 */
    return s_custom_alarm.active &&
           !(s_custom_alarm.acked &&
             s_custom_alarm.clear_policy == ALARM_CLEAR_ACK_HIDE);
}

static bool alarm_target_add(comp_id_t *targets, uint8_t *count,
                             uint8_t max_targets, comp_id_t target)
{
    /* 这个工具函数用于收集需要被高亮的组件列表，同时做去重和容量保护。 */
    if (target == COMP_EMPTY || targets == NULL || count == NULL)
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

static alarm_level_t alarm_highest_level(void)
{
    /* 当前展示优先级完全由“最高告警等级”决定。 */
    alarm_level_t level = ALARM_NONE;

    for (uint8_t i = 0; i < ALARM_ID_COUNT; i++)
    {
        if (!alarm_is_displayable(i))
        {
            continue;
        }
        if (s_alarm_defs[i].level > level)
        {
            level = s_alarm_defs[i].level;
        }
    }

    if (alarm_custom_is_displayable() && s_custom_alarm.level > level)
    {
        level = s_custom_alarm.level;
    }

    return level;
}

static uint8_t alarm_collect_level(alarm_level_t level, int16_t *items, uint8_t max_items)
{
    /* 先收集同等级候选，再按 seq 排序，保证轮播顺序和触发顺序一致。 */
    uint8_t count = 0;

    for (uint8_t i = 0; i < ALARM_ID_COUNT && count < max_items; i++)
    {
        if (alarm_is_displayable(i) &&
                s_alarm_defs[i].level == level)
        {
            items[count++] = (int16_t)i;
        }
    }

    if (alarm_custom_is_displayable() && s_custom_alarm.level == level && count < max_items)
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

static uint32_t alarm_level_first_tick(alarm_level_t level)
{
    /* 同等级多告警轮播时，first_tick 用来决定轮播起点和时间基准。 */
    uint32_t first = UINT32_MAX;

    for (uint8_t i = 0; i < ALARM_ID_COUNT; i++)
    {
        if (alarm_is_displayable(i) &&
                s_alarm_defs[i].level == level &&
                s_alarm_states[i].first_tick < first)
        {
            first = s_alarm_states[i].first_tick;
        }
    }

    if (alarm_custom_is_displayable() &&
            s_custom_alarm.level == level &&
            s_custom_alarm.first_tick < first)
    {
        first = s_custom_alarm.first_tick;
    }

    return (first == UINT32_MAX) ? alarm_now() : first;
}

static void alarm_update_display(uint32_t now_ms)
{
    /* 这个函数负责把“内部告警状态集合”压缩成“当前前台显示的一条 banner”。 */
    alarm_level_t level = alarm_highest_level();
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
        /* 同等级多条告警时按照固定时间窗口轮播展示。 */
        int16_t items[ALARM_ID_COUNT + 1];
        uint8_t count = alarm_collect_level(level, items, (uint8_t)(ALARM_ID_COUNT + 1));
        uint8_t pick = 0;
        if (count > 1U)
        {
            uint32_t first = alarm_level_first_tick(level);
            pick = (uint8_t)(((now_ms - first) / ALARM_BANNER_ROTATE_MS) % count);
        }

        int16_t key = (count > 0U) ? items[pick] : -2;
        s_display.visible = (key != -2);
        s_display.level = level;
        s_display_key = key;

        if (key >= 0)
        {
            /* 标准告警从静态定义表取文本和目标组件。 */
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

void alarm_init(void)
{
    memset(s_alarm_states, 0, sizeof(s_alarm_states));
    memset(&s_custom_alarm, 0, sizeof(s_custom_alarm));
    memset(&s_display, 0, sizeof(s_display));
    s_display.level = ALARM_NONE;
    s_display.banner_target = COMP_EMPTY;
    s_seq = 0;
    s_display_key = -2;
}

bool alarm_set_active(alarm_id_t id, bool active)
{
    if (id >= ALARM_ID_COUNT)
    {
        return false;
    }

    alarm_state_t *state = &s_alarm_states[id];
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

bool alarm_raise_custom(alarm_level_t level,
                             const char *text,
                             comp_id_t target)
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
    s_custom_alarm.acked = false;
    s_custom_alarm.clear_policy = (level == ALARM_INFO) ?
                                  ALARM_CLEAR_AUTO_TIMEOUT :
                                  ALARM_CLEAR_ACK_HIDE;
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
    alarm_mark_dirty();
}

bool alarm_ack_current(void)
{
    if (s_display_key >= 0)
    {
        if (s_alarm_defs[s_display_key].clear_policy != ALARM_CLEAR_ACK_HIDE)
        {
            return false;
        }
        s_alarm_states[s_display_key].acked = true;
        alarm_mark_dirty();
        return true;
    }

    if (s_display_key == -1)
    {
        if (s_custom_alarm.clear_policy != ALARM_CLEAR_ACK_HIDE)
        {
            return false;
        }
        s_custom_alarm.acked = true;
        alarm_mark_dirty();
        return true;
    }

    return false;
}

void alarm_tick(uint32_t now_ms)
{
    bool changed = false;

    for (uint8_t i = 0; i < ALARM_ID_COUNT; i++)
    {
        if (s_alarm_states[i].active &&
                s_alarm_defs[i].clear_policy == ALARM_CLEAR_AUTO_TIMEOUT &&
                now_ms - s_alarm_states[i].first_tick >= ALARM_INFO_DISPLAY_MS)
        {
            s_alarm_states[i].active = false;
            changed = true;
        }
    }

    if (s_custom_alarm.active &&
            s_custom_alarm.clear_policy == ALARM_CLEAR_AUTO_TIMEOUT &&
            now_ms - s_custom_alarm.first_tick >= ALARM_INFO_DISPLAY_MS)
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

const alarm_display_t *alarm_get_display(void)
{
    return &s_display;
}

uint8_t alarm_get_active_targets(alarm_level_t level,
                                      comp_id_t *targets,
                                      uint8_t max_targets)
{
    uint8_t count = 0;

    if (level == ALARM_INFO || level == ALARM_NONE)
    {
        return 0;
    }

    for (uint8_t i = 0; i < ALARM_ID_COUNT; i++)
    {
        if (alarm_is_displayable(i) &&
                s_alarm_defs[i].level == level)
        {
            (void)alarm_target_add(targets, &count, max_targets, s_alarm_defs[i].target);
        }
    }

    if (alarm_custom_is_displayable() && s_custom_alarm.level == level)
    {
        (void)alarm_target_add(targets, &count, max_targets, s_custom_alarm.target);
    }

    return count;
}
