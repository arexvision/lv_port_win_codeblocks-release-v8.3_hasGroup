#include "ui_test.h"
#include "ui_test_flags.h"

#if UI_LVGL_MONKEY_TEST_ENABLED

#include "lvgl/lvgl.h"
#include "rtthread.h"

#ifdef RT_USING_FINSH
#include <finsh.h>
#endif

#include "ui/alarm/alarm.h"
#include "ui/core/data.h"
#include "ui/core/ui_dirty.h"
#include "ui/core/ui_engine.h"
#include "ui/core/ui_state.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef enum
{
    MONKEY_STOP_REASON_USER = 0,
    MONKEY_STOP_REASON_PASS,
    MONKEY_STOP_REASON_FAIL
} monkey_stop_reason_t;

typedef enum
{
    MONKEY_MODE_UI_ONLY = 0,
    MONKEY_MODE_FULL
} monkey_mode_t;

typedef struct
{
    bool active;
    bool baseline_locked;
    bool start_requested;
    bool stop_requested;
    bool auto_start_checked;
    monkey_stop_reason_t stop_reason;
    monkey_mode_t mode;
    monkey_mode_t requested_mode;
    const char *fail_reason;

    lv_timer_t *inject_timer;
    lv_timer_t *monitor_timer;
    uint32_t start_ms;
    uint32_t blind_until_ms;
    uint32_t action_count;
    uint32_t blind_action_count;
    uint32_t wide_action_count;
    uint32_t rebuild_request_count;
    uint32_t alarm_inject_count;
    uint32_t state_fail_count;
    uint32_t memory_fail_count;
    uint32_t lv_mem_low_streak;
    uint32_t rt_mem_low_streak;
    uint32_t stack_low_streak;
    bool forced_alarm_active[ALARM_ID_COUNT];

    uint32_t baseline_lv_free;
    uint32_t baseline_rt_free;
    uint32_t baseline_stack_free;
    uint32_t min_lv_free;
    uint32_t min_rt_free;
    uint32_t min_stack_free;
} ui_monkey_state_t;

static ui_monkey_state_t s_monkey;

static const alarm_id_t k_monkey_alarm_ids[] =
{
    ALARM_ID_CRIT_PO2_MAX,
    ALARM_ID_CRIT_CEIL_BROKEN,
    ALARM_ID_CRIT_BATTERY_DEAD,
    ALARM_ID_WARN_DEPTH_LIMIT,
    ALARM_ID_WARN_NDL_LOW,
    ALARM_ID_WARN_CNS_HIGH,
    ALARM_ID_WARN_BATTERY_LOW,
    ALARM_ID_INFO_SAFETY_STOP,
    ALARM_ID_INFO_STOP_DONE,
    ALARM_ID_INFO_COMPASS_CALI,
};

static const char *monkey_mode_name(monkey_mode_t mode)
{
    return (mode == MONKEY_MODE_FULL) ? "full" : "ui_only";
}

static bool monkey_full_mode(void)
{
    return s_monkey.mode == MONKEY_MODE_FULL;
}

static uint32_t monkey_now_ms(void)
{
    return rt_tick_get_millisecond();
}

static uint32_t monkey_elapsed_ms(void)
{
    return monkey_now_ms() - s_monkey.start_ms;
}

static void monkey_mark_blind_window(uint32_t now_ms)
{
    s_monkey.blind_until_ms = now_ms + UI_LVGL_MONKEY_BLIND_WINDOW_MS;
}

static bool monkey_in_blind_window(uint32_t now_ms)
{
    return (lv_anim_count_running() > 0) ||
           ((int32_t)(now_ms - s_monkey.blind_until_ms) < 0);
}

static uint32_t monkey_rt_heap_free(void)
{
#ifdef RT_USING_HEAP
    rt_uint32_t total = 0U;
    rt_uint32_t used = 0U;
    rt_uint32_t max_used = 0U;

    rt_memory_info(&total, &used, &max_used);
    (void)max_used;
    return (total >= used) ? (total - used) : 0U;
#else
    return 0U;
#endif
}

static uint32_t monkey_stack_free_watermark(rt_thread_t thread)
{
    const uint8_t *stack;
    uint32_t free_bytes = 0U;

    if ((thread == RT_NULL) || (thread->stack_addr == RT_NULL) ||
        (thread->stack_size == 0U))
    {
        return 0U;
    }

    stack = (const uint8_t *)thread->stack_addr;

#ifdef ARCH_CPU_STACK_GROWS_UPWARD
    while ((free_bytes < thread->stack_size) &&
           (stack[thread->stack_size - 1U - free_bytes] == '#'))
    {
        ++free_bytes;
    }
#else
    while ((free_bytes < thread->stack_size) && (stack[free_bytes] == '#'))
    {
        ++free_bytes;
    }
#endif

    return free_bytes;
}

static void monkey_note_failure(const char *reason)
{
    if (s_monkey.stop_requested)
    {
        return;
    }

    s_monkey.fail_reason = (reason != RT_NULL) ? reason : "unknown";
    s_monkey.stop_reason = MONKEY_STOP_REASON_FAIL;
    s_monkey.stop_requested = true;
}

static void monkey_check_ui_state(void)
{
    ui_state_t state = ui_state_get_state();
    bool known_state = false;

    switch (state)
    {
    case UI_DASH:
    case UI_INFO:
    case UI_SETUP:
    case UI_EDIT_GAS:
    case UI_MODAL_GAS:
    case UI_MODAL_COMPASS:
    case UI_SUB_MENU:
    case UI_MODAL_ACT:
    case UI_EDIT_VALUE:
    case UI_MODAL_SETUP_CONFIRM:
    case UI_MENU_ENTRY:
    case UI_MODAL_END_DIVE:
    case UI_MODAL_TURN_OFF:
        known_state = true;
        break;
    default:
        break;
    }

    if (!known_state)
    {
        ++s_monkey.state_fail_count;
        monkey_note_failure("unknown_ui_state");
    }
}

static void monkey_publish_fake_data(void)
{
    uint32_t sample = s_monkey.action_count;
    float depth_m = (float)(sample % 90U) * 0.5f;
    float temperature_c = 18.0f + (float)(sample % 120U) * 0.05f;
    uint16_t heading = (uint16_t)((sample * 37U) % 360U);
    float battery_pct = 20.0f + (float)(sample % 80U);

    bus_set_depth_force(depth_m);
    bus_set_temperature(temperature_c);
    bus_set_heading(heading);
    bus_set_battery(battery_pct);
    bus_set_sensor_status((sample & 1U) ? "MONKEY WARN" : "ALL OK");
}

static void monkey_set_fake_alarm(alarm_id_t id, bool active)
{
    if (id >= ALARM_ID_COUNT)
    {
        return;
    }

    if (!active)
    {
        (void)alarm_set_active(id, false);
        s_monkey.forced_alarm_active[id] = false;
        return;
    }

    if (alarm_set_active(id, active))
    {
        s_monkey.forced_alarm_active[id] = true;
    }
    else
    {
        s_monkey.forced_alarm_active[id] = true;
    }
}

static void monkey_clear_full_fake_alarms(void)
{
    for (uint8_t i = 0U; i < ALARM_ID_COUNT; ++i)
    {
        if (s_monkey.forced_alarm_active[i])
        {
            (void)alarm_set_active((alarm_id_t)i, false);
            s_monkey.forced_alarm_active[i] = false;
        }
    }
    (void)alarm_clear_custom();
}

static void monkey_inject_full_fake_alarm(void)
{
    const uint8_t alarm_count = (uint8_t)(sizeof(k_monkey_alarm_ids) / sizeof(k_monkey_alarm_ids[0]));
    const uint32_t sample = s_monkey.action_count;
    const alarm_id_t id = k_monkey_alarm_ids[sample % alarm_count];
    const alarm_level_t custom_level =
        ((sample % 5U) == 0U) ? ALARM_CRIT :
        ((sample % 3U) == 0U) ? ALARM_INFO : ALARM_WARN;
    const char *custom_text =
        (custom_level == ALARM_CRIT) ? "MONKEY CRIT" :
        (custom_level == ALARM_INFO) ? "MONKEY INFO" : "MONKEY WARN";

    monkey_set_fake_alarm(id, true);
    if ((sample % 4U) == 0U)
    {
        const alarm_id_t old_id = k_monkey_alarm_ids[(sample + alarm_count - 2U) % alarm_count];
        monkey_set_fake_alarm(old_id, false);
    }
    (void)alarm_raise_custom(custom_level, custom_text, COMP_EMPTY);
    bus_requeue_dirty(DIRTY_ALARM);
}

static void monkey_request_layout_rebuild(void)
{
    ++s_monkey.rebuild_request_count;
    bus_requeue_dirty(DIRTY_UI_LAYOUT);
    monkey_mark_blind_window(monkey_now_ms());
}

static void monkey_finish_atomic_action(bool blind_window)
{
    ++s_monkey.action_count;
    if (blind_window)
    {
        ++s_monkey.blind_action_count;
    }

    monkey_check_ui_state();
    ui_update_flush_pending_once();
}

static void monkey_back_once(bool blind_window);

static void monkey_rotate_once(int8_t dir, bool blind_window)
{
    ui_handle_rotate(dir);
    monkey_mark_blind_window(monkey_now_ms());
    monkey_finish_atomic_action(blind_window);
}

static bool monkey_ui_only_click_is_safe(void)
{
    ui_state_t state = ui_state_get_state();

    /* Monkey 压测不能确认会结束潜水或关机的弹窗，full 模式也只负责压 UI/业务数据链路。 */
    if ((state == UI_MODAL_END_DIVE) ||
        (state == UI_MODAL_TURN_OFF))
    {
        return false;
    }

    if (monkey_full_mode())
    {
        return true;
    }

    /* UI-only validates page/object lifetime only; skip actions that persist
     * configuration or confirm business-side modals. */
    if ((state == UI_SETUP) ||
        (state == UI_MODAL_GAS) ||
        (state == UI_MODAL_COMPASS) ||
        (state == UI_MODAL_ACT) ||
        (state == UI_MODAL_SETUP_CONFIRM) ||
        (state == UI_MENU_ENTRY))
    {
        return false;
    }

    if ((state == UI_SUB_MENU) && (ui_state_get_sub_parent() == UI_SETUP))
    {
        return false;
    }

    return true;
}

static void monkey_click_once(bool blind_window)
{
    if (!monkey_ui_only_click_is_safe())
    {
        monkey_back_once(blind_window);
        return;
    }

    ui_handle_click();
    monkey_finish_atomic_action(blind_window);
}

static void monkey_back_once(bool blind_window)
{
    ui_handle_back();
    monkey_mark_blind_window(monkey_now_ms());
    monkey_finish_atomic_action(blind_window);
}

static void monkey_rotate_steps(int8_t dir, uint8_t steps, bool blind_window)
{
    for (uint8_t i = 0U; i < steps; ++i)
    {
        monkey_rotate_once(dir, blind_window);
        if (s_monkey.stop_requested)
        {
            break;
        }
    }
}

static void monkey_escape_to_card_home(bool blind_window)
{
    for (uint8_t i = 0U; i < UI_LVGL_MONKEY_ESCAPE_BACK_MAX; ++i)
    {
        monkey_back_once(blind_window);
        if (s_monkey.stop_requested)
        {
            break;
        }
    }
}

static uint8_t monkey_random_steps(uint8_t min_steps, uint8_t max_steps)
{
    uint8_t span;

    if (max_steps <= min_steps)
    {
        return min_steps;
    }

    span = (uint8_t)(max_steps - min_steps + 1U);
    return (uint8_t)(min_steps + (rand() % span));
}

static void monkey_inject_data_and_alarm(bool blind_window)
{
    if (monkey_full_mode())
    {
        monkey_publish_fake_data();
        ++s_monkey.alarm_inject_count;
        monkey_inject_full_fake_alarm();
    }
    else
    {
        monkey_request_layout_rebuild();
    }
    monkey_finish_atomic_action(blind_window);
}

static void monkey_inject_wide_action(bool blind_window)
{
    uint8_t phase = (uint8_t)(s_monkey.wide_action_count % 6U);
    uint8_t steps = monkey_random_steps(4U, UI_LVGL_MONKEY_WIDE_ROTATE_MAX_STEPS);
    int8_t dir = ((rand() & 1) == 0) ? 1 : -1;

    ++s_monkey.wide_action_count;

    /*
     * The wide action deliberately chains several legal UI inputs in one LVGL
     * timer slice. This prevents the pseudo monkey from spending minutes in a
     * tiny local loop, while still using the same public input handlers as real
     * hardware events.
     */
    switch (phase)
    {
    case 0:
        monkey_escape_to_card_home(blind_window);
        monkey_rotate_steps(1, steps, blind_window);
        monkey_click_once(blind_window);
        break;

    case 1:
        monkey_escape_to_card_home(blind_window);
        monkey_rotate_steps(-1, 3U, blind_window);
        monkey_rotate_steps(1, monkey_random_steps(1U, 6U), blind_window);
        monkey_click_once(blind_window);
        break;

    case 2:
        monkey_escape_to_card_home(blind_window);
        monkey_rotate_steps(1, UI_LVGL_MONKEY_WIDE_ROTATE_MAX_STEPS, blind_window);
        monkey_rotate_steps(1, 3U, blind_window);
        monkey_rotate_steps(dir, monkey_random_steps(1U, 7U), blind_window);
        monkey_click_once(blind_window);
        break;

    case 3:
        monkey_rotate_steps(dir, steps, blind_window);
        monkey_click_once(blind_window);
        monkey_rotate_steps((int8_t)-dir, monkey_random_steps(2U, 8U), blind_window);
        monkey_click_once(blind_window);
        break;

    case 4:
        monkey_inject_data_and_alarm(blind_window);
        monkey_request_layout_rebuild();
        monkey_finish_atomic_action(blind_window);
        monkey_rotate_steps(dir, monkey_random_steps(2U, 6U), blind_window);
        break;

    case 5:
    default:
        monkey_back_once(blind_window);
        monkey_click_once(blind_window);
        monkey_rotate_steps(dir, steps, blind_window);
        monkey_back_once(blind_window);
        break;
    }
}

static void monkey_inject_one_action(bool blind_window)
{
    int action = rand() % (blind_window ? 14 : 10);
    int8_t dir = ((rand() & 1) == 0) ? 1 : -1;

    if ((s_monkey.action_count > 0U) &&
        ((s_monkey.action_count % UI_LVGL_MONKEY_WIDE_ACTION_INTERVAL) == 0U))
    {
        monkey_inject_wide_action(blind_window);
        return;
    }

    switch (action)
    {
    case 0:
    case 1:
    case 2:
        monkey_rotate_once(dir, blind_window);
        break;

    case 3:
    case 4:
        monkey_click_once(blind_window);
        break;

    case 5:
    case 6:
        monkey_back_once(blind_window);
        break;

    case 7:
        if (monkey_full_mode())
        {
            ++s_monkey.alarm_inject_count;
            monkey_inject_full_fake_alarm();
        }
        else
        {
            monkey_request_layout_rebuild();
        }
        monkey_finish_atomic_action(blind_window);
        break;

    case 8:
        if (monkey_full_mode())
        {
            (void)alarm_clear_custom();
            bus_requeue_dirty(DIRTY_ALARM);
        }
        else
        {
            monkey_request_layout_rebuild();
        }
        monkey_finish_atomic_action(blind_window);
        break;

    case 9:
        if (monkey_full_mode())
        {
            monkey_publish_fake_data();
        }
        else
        {
            monkey_request_layout_rebuild();
        }
        monkey_finish_atomic_action(blind_window);
        break;

    case 10:
        monkey_request_layout_rebuild();
        monkey_finish_atomic_action(blind_window);
        break;

    case 11:
        monkey_rotate_steps(dir, monkey_random_steps(2U, 5U), blind_window);
        break;

    case 12:
        monkey_click_once(blind_window);
        monkey_back_once(blind_window);
        break;

    case 13:
    default:
        monkey_rotate_once(dir, blind_window);
        monkey_back_once(blind_window);
        break;
    }
}

static void monkey_inject_timer_cb(lv_timer_t *timer)
{
    uint32_t now_ms = monkey_now_ms();
    bool blind_window = monkey_in_blind_window(now_ms);
    uint8_t burst = blind_window ? 3U : 1U;

    (void)timer;

    if (!s_monkey.active)
    {
        return;
    }

    for (uint8_t i = 0U; i < burst; ++i)
    {
        monkey_inject_one_action(blind_window);
        if (s_monkey.stop_requested)
        {
            break;
        }
    }
}

static void monkey_capture_memory(uint32_t *lv_free,
                                  uint32_t *rt_free,
                                  uint32_t *stack_free)
{
    lv_mem_monitor_t lv_mon;

    lv_mem_monitor(&lv_mon);
    if (lv_free != RT_NULL)
    {
        *lv_free = lv_mon.free_size;
    }
    if (rt_free != RT_NULL)
    {
        *rt_free = monkey_rt_heap_free();
    }
    if (stack_free != RT_NULL)
    {
        *stack_free = monkey_stack_free_watermark(rt_thread_self());
    }
}

static void monkey_print_summary(const char *tag)
{
    rt_kprintf("[UI_MONKEY] %s mode=%s elapsed=%lu action=%lu blind_action=%lu wide=%lu rebuild=%lu alarm=%lu "
               "lv_free_min=%lu base=%lu rt_free_min=%lu base=%lu stack_free_min=%lu base=%lu\n",
               tag,
               monkey_mode_name(s_monkey.mode),
               (unsigned long)monkey_elapsed_ms(),
               (unsigned long)s_monkey.action_count,
               (unsigned long)s_monkey.blind_action_count,
               (unsigned long)s_monkey.wide_action_count,
               (unsigned long)s_monkey.rebuild_request_count,
               (unsigned long)s_monkey.alarm_inject_count,
               (unsigned long)s_monkey.min_lv_free,
               (unsigned long)s_monkey.baseline_lv_free,
               (unsigned long)s_monkey.min_rt_free,
               (unsigned long)s_monkey.baseline_rt_free,
               (unsigned long)s_monkey.min_stack_free,
               (unsigned long)s_monkey.baseline_stack_free);
}

static void monkey_monitor_timer_cb(lv_timer_t *timer)
{
    uint32_t lv_free = 0U;
    uint32_t rt_free = 0U;
    uint32_t stack_free = 0U;
    uint32_t elapsed_ms = monkey_elapsed_ms();

    (void)timer;

    if (!s_monkey.active)
    {
        return;
    }

    monkey_capture_memory(&lv_free, &rt_free, &stack_free);

    if (s_monkey.min_lv_free == 0U || lv_free < s_monkey.min_lv_free)
    {
        s_monkey.min_lv_free = lv_free;
    }
    if (s_monkey.min_rt_free == 0U || rt_free < s_monkey.min_rt_free)
    {
        s_monkey.min_rt_free = rt_free;
    }
    if (s_monkey.min_stack_free == 0U || stack_free < s_monkey.min_stack_free)
    {
        s_monkey.min_stack_free = stack_free;
    }

    if (!s_monkey.baseline_locked &&
        elapsed_ms >= UI_LVGL_MONKEY_BASELINE_DELAY_MS)
    {
        s_monkey.baseline_lv_free = lv_free;
        s_monkey.baseline_rt_free = rt_free;
        s_monkey.baseline_stack_free = stack_free;
        s_monkey.min_lv_free = lv_free;
        s_monkey.min_rt_free = rt_free;
        s_monkey.min_stack_free = stack_free;
        s_monkey.baseline_locked = true;
        rt_kprintf("[UI_MONKEY] baseline locked: lv_free=%lu rt_free=%lu stack_free=%lu\n",
                   (unsigned long)lv_free,
                   (unsigned long)rt_free,
                   (unsigned long)stack_free);
        return;
    }

    rt_kprintf("[UI_MONKEY] status mode=%s elapsed=%lu action=%lu blind=%lu wide=%lu anim=%u "
               "lv_free=%lu rt_free=%lu stack_free=%lu state=%u dash=%u\n",
               monkey_mode_name(s_monkey.mode),
               (unsigned long)elapsed_ms,
               (unsigned long)s_monkey.action_count,
               (unsigned long)s_monkey.blind_action_count,
               (unsigned long)s_monkey.wide_action_count,
               (unsigned)lv_anim_count_running(),
               (unsigned long)lv_free,
               (unsigned long)rt_free,
               (unsigned long)stack_free,
               (unsigned)ui_state_get_state(),
               (unsigned)ui_state_get_dash_page());

    if (s_monkey.baseline_locked)
    {
        if (lv_free + UI_LVGL_MONKEY_LV_MEM_TOLERANCE_BYTES <
            s_monkey.baseline_lv_free)
        {
            ++s_monkey.lv_mem_low_streak;
            if (s_monkey.lv_mem_low_streak >= UI_LVGL_MONKEY_MEMORY_FAIL_CONFIRM_SAMPLES)
            {
                ++s_monkey.memory_fail_count;
                monkey_note_failure("lvgl_heap_drop");
            }
        }
        else
        {
            s_monkey.lv_mem_low_streak = 0U;
        }
        if ((s_monkey.baseline_rt_free != 0U) &&
            (rt_free + UI_LVGL_MONKEY_RT_MEM_TOLERANCE_BYTES <
             s_monkey.baseline_rt_free))
        {
            ++s_monkey.rt_mem_low_streak;
            if (s_monkey.rt_mem_low_streak >= UI_LVGL_MONKEY_MEMORY_FAIL_CONFIRM_SAMPLES)
            {
                ++s_monkey.memory_fail_count;
                monkey_note_failure("rt_heap_drop");
            }
        }
        else
        {
            s_monkey.rt_mem_low_streak = 0U;
        }
        if ((stack_free != 0U) &&
            (stack_free < UI_LVGL_MONKEY_STACK_MIN_FREE_BYTES))
        {
            ++s_monkey.stack_low_streak;
            if (s_monkey.stack_low_streak >= UI_LVGL_MONKEY_MEMORY_FAIL_CONFIRM_SAMPLES)
            {
                ++s_monkey.memory_fail_count;
                monkey_note_failure("lvgl_stack_low");
            }
        }
        else
        {
            s_monkey.stack_low_streak = 0U;
        }
    }

    if (elapsed_ms >= UI_LVGL_MONKEY_DURATION_MS)
    {
        s_monkey.stop_reason = MONKEY_STOP_REASON_PASS;
        s_monkey.stop_requested = true;
    }
}

static void monkey_start_in_lvgl_context(void)
{
    uint32_t lv_free = 0U;
    uint32_t rt_free = 0U;
    uint32_t stack_free = 0U;
    bool auto_start_checked = s_monkey.auto_start_checked;
    monkey_mode_t requested_mode = s_monkey.requested_mode;

    if (s_monkey.active)
    {
        return;
    }

    memset(&s_monkey, 0, sizeof(s_monkey));
    s_monkey.auto_start_checked = auto_start_checked;
    s_monkey.mode = requested_mode;
    s_monkey.requested_mode = requested_mode;
    srand(rt_tick_get());
    s_monkey.start_ms = monkey_now_ms();
    monkey_mark_blind_window(s_monkey.start_ms);

    monkey_capture_memory(&lv_free, &rt_free, &stack_free);
    s_monkey.min_lv_free = lv_free;
    s_monkey.min_rt_free = rt_free;
    s_monkey.min_stack_free = stack_free;

    s_monkey.inject_timer = lv_timer_create(monkey_inject_timer_cb,
                                            UI_LVGL_MONKEY_INJECT_PERIOD_MS,
                                            RT_NULL);
    s_monkey.monitor_timer = lv_timer_create(monkey_monitor_timer_cb,
                                             UI_LVGL_MONKEY_MONITOR_PERIOD_MS,
                                             RT_NULL);

    if ((s_monkey.inject_timer == RT_NULL) ||
        (s_monkey.monitor_timer == RT_NULL))
    {
        if (s_monkey.inject_timer != RT_NULL)
        {
            lv_timer_del(s_monkey.inject_timer);
        }
        if (s_monkey.monitor_timer != RT_NULL)
        {
            lv_timer_del(s_monkey.monitor_timer);
        }
        memset(&s_monkey, 0, sizeof(s_monkey));
        rt_kprintf("[UI_MONKEY] start failed: timer create failed\n");
        return;
    }

    s_monkey.active = true;
    rt_kprintf("[UI_MONKEY] started: mode=%s baseline_delay=%lu duration=%lu lv_free=%lu rt_free=%lu stack_free=%lu\n",
               monkey_mode_name(s_monkey.mode),
               (unsigned long)UI_LVGL_MONKEY_BASELINE_DELAY_MS,
               (unsigned long)UI_LVGL_MONKEY_DURATION_MS,
               (unsigned long)lv_free,
               (unsigned long)rt_free,
               (unsigned long)stack_free);
}

static void monkey_stop_in_lvgl_context(void)
{
    monkey_stop_reason_t reason = s_monkey.stop_reason;
    const char *fail_reason = s_monkey.fail_reason;
    bool auto_start_checked = s_monkey.auto_start_checked;

    if (!s_monkey.active)
    {
        s_monkey.stop_requested = false;
        return;
    }

    if (s_monkey.inject_timer != RT_NULL)
    {
        lv_timer_del(s_monkey.inject_timer);
        s_monkey.inject_timer = RT_NULL;
    }
    if (s_monkey.monitor_timer != RT_NULL)
    {
        lv_timer_del(s_monkey.monitor_timer);
        s_monkey.monitor_timer = RT_NULL;
    }

    if (s_monkey.mode == MONKEY_MODE_FULL)
    {
        monkey_clear_full_fake_alarms();
    }

    if (reason == MONKEY_STOP_REASON_PASS)
    {
        monkey_print_summary("PASS");
    }
    else if (reason == MONKEY_STOP_REASON_FAIL)
    {
        rt_kprintf("[UI_MONKEY] FAIL reason=%s state_fail=%lu memory_fail=%lu\n",
                   (fail_reason != RT_NULL) ? fail_reason : "unknown",
                   (unsigned long)s_monkey.state_fail_count,
                   (unsigned long)s_monkey.memory_fail_count);
        monkey_print_summary("FAIL");
    }
    else
    {
        monkey_print_summary("STOP");
    }

    memset(&s_monkey, 0, sizeof(s_monkey));
    s_monkey.auto_start_checked = auto_start_checked;
}

void ui_test_poll_runtime_control(void)
{
    if (!s_monkey.auto_start_checked)
    {
        s_monkey.auto_start_checked = true;
#if UI_LVGL_MONKEY_AUTO_START
        s_monkey.requested_mode = MONKEY_MODE_UI_ONLY;
        s_monkey.start_requested = true;
#endif
    }

    if (s_monkey.start_requested)
    {
        s_monkey.start_requested = false;
        monkey_start_in_lvgl_context();
    }

    if (s_monkey.stop_requested)
    {
        monkey_stop_in_lvgl_context();
    }
}

void ui_monkey_request_start_full(bool full_mode)
{
    s_monkey.requested_mode = full_mode ? MONKEY_MODE_FULL : MONKEY_MODE_UI_ONLY;
    s_monkey.start_requested = true;
}

void ui_monkey_request_start(void)
{
    ui_monkey_request_start_full(false);
}

void ui_monkey_request_stop(void)
{
    s_monkey.stop_reason = MONKEY_STOP_REASON_USER;
    s_monkey.stop_requested = true;
}

#ifdef RT_USING_FINSH
static void cmd_ui_monkey(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("usage: ui_monkey start [ui_only|full] | stop | status\n");
        return;
    }

    if (strcmp(argv[1], "start") == 0)
    {
        bool full_mode = false;
        if (argc >= 3)
        {
            if ((strcmp(argv[2], "full") == 0) || (strcmp(argv[2], "business") == 0))
            {
                full_mode = true;
            }
            else if ((strcmp(argv[2], "ui_only") != 0) && (strcmp(argv[2], "ui") != 0))
            {
                rt_kprintf("usage: ui_monkey start [ui_only|full] | stop | status\n");
                return;
            }
        }
        ui_monkey_request_start_full(full_mode);
        rt_kprintf("[UI_MONKEY] start requested mode=%s\n",
                   monkey_mode_name(full_mode ? MONKEY_MODE_FULL : MONKEY_MODE_UI_ONLY));
        return;
    }

    if (strcmp(argv[1], "stop") == 0)
    {
        ui_monkey_request_stop();
        rt_kprintf("[UI_MONKEY] stop requested\n");
        return;
    }

    if (strcmp(argv[1], "status") == 0)
    {
        rt_kprintf("[UI_MONKEY] active=%u mode=%s start_req=%u req_mode=%s stop_req=%u baseline=%u elapsed=%lu action=%lu blind=%lu wide=%lu\n",
                   s_monkey.active ? 1U : 0U,
                   monkey_mode_name(s_monkey.mode),
                   s_monkey.start_requested ? 1U : 0U,
                   monkey_mode_name(s_monkey.requested_mode),
                   s_monkey.stop_requested ? 1U : 0U,
                   s_monkey.baseline_locked ? 1U : 0U,
                   s_monkey.active ? (unsigned long)monkey_elapsed_ms() : 0UL,
                   (unsigned long)s_monkey.action_count,
                   (unsigned long)s_monkey.blind_action_count,
                   (unsigned long)s_monkey.wide_action_count);
        return;
    }

    rt_kprintf("usage: ui_monkey start [ui_only|full] | stop | status\n");
}
MSH_CMD_EXPORT_ALIAS(cmd_ui_monkey, ui_monkey, LVGL pseudo monkey stress test);
#endif

#endif /* UI_LVGL_MONKEY_TEST_ENABLED */
