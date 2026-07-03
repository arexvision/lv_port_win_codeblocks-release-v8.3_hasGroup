/*
 * 文件: src/app_ui/ui/core/ui_state.c
 * 作用: 该文件属于 UI 核心模块，负责状态机、数据桥接、事件分发、更新调度或 UI 运行时公共定义。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "ui_state.h"
#include "callbacks.h"
#include "data.h"
#include "ui_engine.h"
#include "../../config/build/ui_build_flags.h"
#include "../../config/build/ui_debug_flags.h"
#include "../alarm/alarm.h"
#include "../screen/page_registry.h"
#include "../screen/screen.h"
#include "../views/menu_defs.h"

#include "lvgl/lvgl.h"
#include <string.h>

#ifndef UI_MENU_ROTATE_LEGACY_WALL_RETURN
#define UI_MENU_ROTATE_LEGACY_WALL_RETURN 0  /* 旧菜单边界蓄力返回逻辑，默认关闭 */
#endif

/* =========================================
   Global UI context
   ========================================= */
static ui_ctx_t s_ui;

/* =========================================
   气体切换命令队列（单向数据流：UI → Algorithm）
   ========================================= */
/* 这些命令变量不直接驱动 UI，而是作为 UI 层向算法层提交动作请求的“缓冲区”。
 * 这样做的好处是可以把“界面触发”和“算法执行”解耦，避免在点击/旋转回调里直接做耗时计算。 */
static gas_switch_cmd_t g_gas_switch_cmd = {false, 0};
static gas_ignore_cmd_t g_gas_ignore_cmd = {false, 0};
/* 罗盘校准命令：UI 只负责发起/撤销请求，真正的校准流程由底层状态机推进。 */
static compass_cal_cmd_t g_compass_cal_cmd = {false, COMPASS_CAL_CMD_NONE};
/* 校准界面的状态镜像，供 UI 决定是否显示校准中、等待确认或空闲提示。 */
static compass_cal_ui_state_t g_compass_cal_ui_state = COMPASS_CAL_IDLE;
/* 罗盘航向锁定相关状态：pending 表示待触发，active 表示已经进入锁定。 */
static bool g_heading_lock_pending = false;
static bool g_heading_lock_active = false;
static uint8_t s_pending_dash_page = 0xFFU;
static uint32_t s_pending_dash_due_ms = 0U;
static uint32_t s_last_dash_commit_ms = 0U;
static uint32_t s_last_click_ms = 0U;

#if UI_CLICK_PROFILE_ENABLED
extern void rt_kprintf(const char *fmt, ...);

typedef struct
{
    uint32_t count;
    uint32_t debounced;
    uint32_t total_ms;
    uint32_t max_ms;
    uint32_t flush_ms_total;
    uint32_t flush_ms_max;
    uint32_t action_ms_total;
    uint32_t action_ms_max;
    uint32_t slow_count;
    uint8_t max_state_before;
    uint8_t max_state_after;
    uint8_t max_page_id;
    uint32_t last_print_ms;
} ui_click_profile_t;

static ui_click_profile_t s_click_profile;

static void ui_click_profile_maybe_print(uint32_t now_ms)
{
    if (s_click_profile.last_print_ms == 0U)
    {
        s_click_profile.last_print_ms = now_ms;
        return;
    }

    if ((now_ms - s_click_profile.last_print_ms) < UI_CLICK_PROFILE_INTERVAL_MS)
    {
        return;
    }

    const uint32_t count = (s_click_profile.count == 0U) ? 1U : s_click_profile.count;
    rt_kprintf("[UI_CLICK] count=%lu debounced=%lu total_avg/max=%lu/%lu "
               "flush_avg/max=%lu/%lu action_avg/max=%lu/%lu slow=%lu "
               "max_state:%u->%u max_page=%u\n",
               (unsigned long)s_click_profile.count,
               (unsigned long)s_click_profile.debounced,
               (unsigned long)(s_click_profile.total_ms / count),
               (unsigned long)s_click_profile.max_ms,
               (unsigned long)(s_click_profile.flush_ms_total / count),
               (unsigned long)s_click_profile.flush_ms_max,
               (unsigned long)(s_click_profile.action_ms_total / count),
               (unsigned long)s_click_profile.action_ms_max,
               (unsigned long)s_click_profile.slow_count,
               (unsigned)s_click_profile.max_state_before,
               (unsigned)s_click_profile.max_state_after,
               (unsigned)s_click_profile.max_page_id);

    memset(&s_click_profile, 0, sizeof(s_click_profile));
    s_click_profile.last_print_ms = now_ms;
}

static void ui_click_profile_note(bool debounced,
                                  uint8_t state_before,
                                  uint8_t state_after,
                                  uint8_t page_id,
                                  uint32_t total_ms,
                                  uint32_t flush_ms,
                                  uint32_t action_ms)
{
    const uint32_t now_ms = lv_tick_get();

    if (debounced)
    {
        s_click_profile.debounced++;
        ui_click_profile_maybe_print(now_ms);
        return;
    }

    s_click_profile.count++;
    s_click_profile.total_ms += total_ms;
    s_click_profile.flush_ms_total += flush_ms;
    s_click_profile.action_ms_total += action_ms;

    if (total_ms > s_click_profile.max_ms)
    {
        s_click_profile.max_ms = total_ms;
        s_click_profile.max_state_before = state_before;
        s_click_profile.max_state_after = state_after;
        s_click_profile.max_page_id = page_id;
    }
    if (flush_ms > s_click_profile.flush_ms_max) s_click_profile.flush_ms_max = flush_ms;
    if (action_ms > s_click_profile.action_ms_max) s_click_profile.action_ms_max = action_ms;
    if (total_ms >= UI_CLICK_SLOW_MS) s_click_profile.slow_count++;

    ui_click_profile_maybe_print(now_ms);
}
#endif

/* =========================================
   Init
   ========================================= */
void ui_state_init(void)
{
    /* 先整体清零，确保历史残留状态不会影响新一轮 UI 生命周期。 */
    memset(&s_ui, 0, sizeof(s_ui));
    /* 默认停留在仪表盘页，符合界面启动后的主流程。 */
    s_ui.state         = UI_DASH;
    /* 仪表盘默认显示从第一个动态页开始的位置。 */
    s_ui.dash_page     = PAGE_POS_DYNAMIC_FIRST;
    /* 菜单索引从 0 开始，便于后续统一做边界修正。 */
    s_ui.menu_info_idx = 0;
    s_ui.menu_entry_idx = 0;
    /* 边界充能计数归零，避免上一次进入菜单的“蓄力”残留。 */
    s_ui.wall_charge   = 0;
    s_pending_dash_page = 0xFFU;
    s_pending_dash_due_ms = 0U;
    s_last_dash_commit_ms = 0U;
    s_last_click_ms = 0U;
}

/* =========================================
   Internal: notify registered pages
   ========================================= */
void ui_refresh_all(void)
{
    /* 遍历所有注册页面，逐个触发更新回调。
     * 这里不关心具体页面类型，只依赖 page_registry 提供的统一接口。 */
    for (uint8_t i = 0; i < page_count(); i++)
    {
        page_t *c = page_get(i);
        /* 页面对象存在且具备刷新回调时才执行，避免空指针或空实现。 */
        if (c && c->update_cb) c->update_cb();
    }
}

/* =========================================
   Internal: tileview navigation
   ========================================= */
void ui_go_to_page(uint8_t tile_pos)
{
    s_pending_dash_page = 0xFFU;
    /* 同步 UI 状态机中的当前页索引。 */
    s_ui.dash_page = tile_pos;
    /* 真正的页面切换动作交给 screen 层完成。 */
    screen_scroll_to_page(tile_pos);
    s_last_dash_commit_ms = lv_tick_get();
}

static void ui_schedule_dash_page(uint8_t tile_pos)
{
#if UI_DASH_ROTATE_COALESCE_ENABLED
    const uint32_t now_ms = lv_tick_get();

    /* 首次旋转必须立即落页，保证用户得到确定的视觉反馈。
     * 只有在一个合并窗口内继续旋转，才延后到最终目标页，避免中间页反复
     * set_tile/set_text/invalidate。 */
    if (s_pending_dash_page == 0xFFU &&
        (s_last_dash_commit_ms == 0U ||
         (now_ms - s_last_dash_commit_ms) >= UI_DASH_ROTATE_COALESCE_WINDOW_MS))
    {
        ui_go_to_page(tile_pos);
        return;
    }

    s_ui.dash_page = tile_pos;
    s_pending_dash_page = tile_pos;
    s_pending_dash_due_ms = now_ms + UI_DASH_ROTATE_COALESCE_WINDOW_MS;
#else
    ui_go_to_page(tile_pos);
#endif
}

static void ui_flush_pending_dash_page(void)
{
#if UI_DASH_ROTATE_COALESCE_ENABLED
    uint8_t tile_pos = s_pending_dash_page;

    if (tile_pos == 0xFFU)
    {
        return;
    }

    s_pending_dash_page = 0xFFU;
    if (tile_pos != ui_state_get_dash_page())
    {
        return;
    }

    s_ui.dash_page = tile_pos;
    screen_scroll_to_page(tile_pos);
    s_last_dash_commit_ms = lv_tick_get();
#endif
}

void ui_state_poll_deferred_navigation(void)
{
#if UI_DASH_ROTATE_COALESCE_ENABLED
    if (s_pending_dash_page == 0xFFU)
    {
        return;
    }

    if ((int32_t)(lv_tick_get() - s_pending_dash_due_ms) < 0)
    {
        return;
    }

    ui_flush_pending_dash_page();
#endif
}

static uint8_t ui_wrap_index(uint8_t current, int8_t dir, uint8_t count)
{
    int16_t next;
    if (count == 0U) return 0U;
    next = (int16_t)current + (int16_t)dir;
    while (next < 0) next += count;
    while (next >= (int16_t)count) next -= count;
    return (uint8_t)next;
}

static void ui_return_to_card_home(void)
{
    s_ui.wall_charge = 0;
    screen_hide_walls_snap();
    menu_entry_clear_selection();
    s_ui.state = UI_DASH;
    s_ui.dash_page = PAGE_POS_DYNAMIC_FIRST;
    ui_go_to_page(PAGE_POS_DYNAMIC_FIRST);
}

static void ui_enter_info_menu_page(void)
{
#if ENABLE_INFO_MENU
    s_ui.wall_charge = 0;
    s_ui.state = UI_INFO;
    s_ui.menu_info_idx = 0;
    menu_entry_clear_selection();
    ui_go_to_page(PAGE_POS_INFO);
    screen_set_info_selection(0);
#endif
}

static void ui_enter_setup_menu_page(void)
{
    s_ui.wall_charge = 0;
    s_ui.state = UI_SETUP;
    s_ui.menu_setup_idx = 0;
    menu_entry_clear_selection();
    menu_defs_set_setup_root(MENU_SETUP_ROOT_DIVE);
    menu_setup_update();
    ui_go_to_page(page_setup_display_pos());
    screen_set_setup_selection(0);
}

static void ui_enter_device_control_page(void)
{
    s_ui.wall_charge = 0;
    s_ui.state = UI_SETUP;
    s_ui.menu_setup_idx = 0;
    menu_entry_clear_selection();
    menu_defs_set_setup_root(MENU_SETUP_ROOT_DEVICE);
    menu_setup_update();
    ui_go_to_page(page_setup_display_pos());
    screen_set_setup_selection(0);
}

/* =========================================
   Rotate handler (+1 = down, -1 = up)
   ========================================= */
void ui_handle_rotate(int8_t dir)
{
    if (dir != 0)
    {
        screen_scroll_dots_notify_interaction();
    }

    if (s_ui.state != UI_DASH)
    {
        ui_flush_pending_dash_page();
    }

    /* 旋钮方向统一由状态机解释，不同 UI 状态下含义不同。 */
    /* 这是真正的 UI 状态机入口之一。
     * 同一个物理输入，在 DASH/菜单/编辑态下会被翻译成完全不同的语义：
     * - DASH: 翻页或边界蓄力
     * - MENU: 上下移动光标
     * - EDIT: 改数值
     * - 特殊页面: 交给专用处理器 */
    switch (s_ui.state)
    {

    /* --- DASH: scroll between pages with wall-charge at edges --- */
    case UI_DASH:
    {
        /* 仪表盘楼层循环：动态卡片 + MENU 入口页。 */
        uint8_t dash_min = PAGE_POS_DYNAMIC_FIRST;
        uint8_t dash_max = page_menu_display_pos();
        int8_t next = (int8_t)s_ui.dash_page + dir;
        s_ui.wall_charge = 0;
        screen_hide_walls();
        menu_entry_clear_selection();
        if (next < (int8_t)dash_min) next = (int8_t)dash_max;
        if (next > (int8_t)dash_max) next = (int8_t)dash_min;
        ui_schedule_dash_page((uint8_t)next);
        break;
    }

    case UI_MENU_ENTRY:
    {
        uint8_t count = menu_entry_item_count();
        if (count == 0U) break;
        s_ui.wall_charge = 0;
        screen_hide_walls();
        s_ui.menu_entry_idx = (uint8_t)(((int8_t)s_ui.menu_entry_idx + dir + (int8_t)count) % (int8_t)count);
        menu_entry_set_selection(s_ui.menu_entry_idx);
        break;
    }

    /* --- EDIT_GAS --- */
    case UI_EDIT_GAS:
    {
        /* 气体编辑页面的游标数量来自气体槽总数，空表时不做任何处理。 */
        uint8_t gas_count = bus_get_gas_slot_count();
        if (gas_count == 0U)
        {
            break;
        }
        int8_t next = ((int8_t)s_ui.gas_cursor + dir + gas_count) % gas_count;
        s_ui.gas_cursor = (uint8_t)next;
        screen_refresh_gas_menu();
        break;
    }

    /* --- INFO menu --- */
    case UI_INFO:
    {
        uint8_t len = screen_info_item_count();
        if (len == 0U) break;
#if UI_MENU_ROTATE_LEGACY_WALL_RETURN
        if (dir == 1 && s_ui.menu_info_idx == len - 1)
        {
            s_ui.wall_charge++;
            screen_show_wall(WALL_BOTTOM, s_ui.wall_charge, "<<< RETURN TO DASH <<<");
            if (s_ui.wall_charge >= 3)
            {
                s_ui.wall_charge = 0;
                screen_hide_walls_snap();
                s_ui.state = UI_DASH;
                s_ui.dash_page = PAGE_POS_DYNAMIC_FIRST;
                ui_go_to_page(PAGE_POS_DYNAMIC_FIRST);
            }
        }
        else
        {
            s_ui.wall_charge = 0;
            screen_hide_walls();
            int8_t next = (int8_t)s_ui.menu_info_idx + dir;
            if (next < 0) next = 0;
            if (next >= (int8_t)len) next = len - 1;
            s_ui.menu_info_idx = (uint8_t)next;
            screen_set_info_selection(s_ui.menu_info_idx);
        }
#else
        /* 顶层菜单旋转只循环移动光标，返回统一由 BACK/返回键负责。 */
        s_ui.wall_charge = 0;
        screen_hide_walls();
        s_ui.menu_info_idx = ui_wrap_index(s_ui.menu_info_idx, dir, len);
        screen_set_info_selection(s_ui.menu_info_idx);
#endif
        break;
    }

    /* --- SETUP menu --- */
    case UI_SETUP:
    {
        uint8_t len = screen_setup_item_count();
        if (len == 0U) break;
#if UI_MENU_ROTATE_LEGACY_WALL_RETURN
        if (dir == -1 && s_ui.menu_setup_idx == 0)
        {
            s_ui.wall_charge++;
            screen_show_wall(WALL_TOP, s_ui.wall_charge, ">>> RETURN TO DASH >>>");
            if (s_ui.wall_charge >= 3)
            {
                s_ui.wall_charge = 0;
                screen_hide_walls_snap();
                s_ui.state = UI_DASH;
                s_ui.dash_page = page_setup_display_pos() - 1;
                ui_go_to_page(page_setup_display_pos() - 1);
            }
        }
        else
        {
            s_ui.wall_charge = 0;
            screen_hide_walls();
            int8_t next = (int8_t)s_ui.menu_setup_idx + dir;
            if (next < 0) next = 0;
            if (next >= (int8_t)len) next = len - 1;
            s_ui.menu_setup_idx = (uint8_t)next;
            screen_set_setup_selection(s_ui.menu_setup_idx);
        }
#else
        /* 顶层设置菜单同样循环移动光标，不再用墙提示返回主屏。 */
        s_ui.wall_charge = 0;
        screen_hide_walls();
        s_ui.menu_setup_idx = ui_wrap_index(s_ui.menu_setup_idx, dir, len);
        screen_set_setup_selection(s_ui.menu_setup_idx);
#endif
        break;
    }

    /* --- SUB_MENU --- */
    case UI_SUB_MENU:
    {
        /* 子菜单内部可能把旋钮事件转交给更深层的潜水计划编辑页。 */
        /* 先给 DIVE PLAN 这类“伪菜单真页面”的特殊逻辑一次拦截机会，
         * 只有它不消费事件时，才退回普通列表选中逻辑。 */
        if (screen_handle_dive_plan_rotate(dir))
        {
            break;
        }
        if (screen_handle_logbook_rotate(dir))
        {
            break;
        }
        if (s_ui.sub_item_count == 0U)
        {
            break;
        }
#if UI_MENU_ROTATE_LEGACY_WALL_RETURN
        int8_t next = (int8_t)s_ui.sub_menu_idx + dir;
        if (next < 0) next = 0;
        if (next >= (int8_t)s_ui.sub_item_count) next = s_ui.sub_item_count - 1;
        s_ui.sub_menu_idx = (uint8_t)next;
#else
        s_ui.wall_charge = 0;
        screen_hide_walls();
        s_ui.sub_menu_idx = ui_wrap_index(s_ui.sub_menu_idx, dir, s_ui.sub_item_count);
#endif
        screen_set_submenu_selection(s_ui.sub_menu_idx);
        break;
    }

    /* --- EDIT_VALUE --- */
    case UI_EDIT_VALUE:
    {
        /* 数值编辑态只允许在 min/max 范围内按 step 递增或递减。 */
        if (!s_ui.edit_ctx.active) break;
        float next = s_ui.edit_ctx.value - dir * s_ui.edit_ctx.step;
        if (next < s_ui.edit_ctx.min) next = s_ui.edit_ctx.min;
        if (next > s_ui.edit_ctx.max) next = s_ui.edit_ctx.max;
        int steps = (int)((next - s_ui.edit_ctx.min) / s_ui.edit_ctx.step + 0.5f);
        s_ui.edit_ctx.value = s_ui.edit_ctx.min + steps * s_ui.edit_ctx.step;
        screen_refresh_edit_value();
        break;
    }

    default:
        break;
    }
}

/* =========================================
   Click handler
   ========================================= */
void ui_handle_click(void)
{
#if UI_CLICK_DEBOUNCE_ENABLED || UI_CLICK_PROFILE_ENABLED
    const uint32_t click_start_ms = lv_tick_get();
#endif
#if UI_CLICK_PROFILE_ENABLED
    uint32_t mark_ms = click_start_ms;
    uint32_t flush_ms = 0U;
    uint8_t state_before = (uint8_t)s_ui.state;
    uint8_t page_id_before = page_id_at(s_ui.dash_page);
#endif

#if UI_CLICK_DEBOUNCE_ENABLED
    if (s_last_click_ms != 0U &&
        (click_start_ms - s_last_click_ms) < UI_CLICK_DEBOUNCE_WINDOW_MS)
    {
#if UI_CLICK_PROFILE_ENABLED
        ui_click_profile_note(true, state_before, (uint8_t)s_ui.state, page_id_before,
                              0U, 0U, 0U);
#endif
        return;
    }
    s_last_click_ms = click_start_ms;
#endif

    ui_flush_pending_dash_page();
#if UI_CLICK_PROFILE_ENABLED
    flush_ms = lv_tick_get() - mark_ms;
    mark_ms = lv_tick_get();
#endif

    {
        extern bool alarm_mark_clear_requested(void);
        if (alarm_mark_clear_requested())
        {
            s_ui.alarm_pending_click = false;
#if UI_CLICK_PROFILE_ENABLED
            ui_click_profile_note(false, state_before, (uint8_t)s_ui.state, page_id_before,
                                  lv_tick_get() - click_start_ms, flush_ms,
                                  lv_tick_get() - mark_ms);
#endif
            return;
        }
    }

    /* 告警锁：触发后必须先 click/rotate 一次才可清除 */
    if (s_ui.alarm_pending_click)
    {
        /* 先清除告警挂起标志，再通知告警模块允许释放锁定。 */
        extern bool alarm_mark_clear_requested(void);
        s_ui.alarm_pending_click = false;
        alarm_mark_clear_requested();
    }

    switch (s_ui.state)
    {
    /* click 更偏向“确认/进入/执行”：
     * rotate 决定你看哪里，click 决定你对当前焦点做什么。 */

    case UI_DASH:
    {
        /* page_id 从 card_order[] 映射 */
        uint8_t page_id = page_id_at(s_ui.dash_page);

        if (page_id == PAGE_ID_COMPASS)
        {
            /* 罗盘页点击用于切换航向锁定状态，首次点击会锁定当前航向。 */
            if (!bus_is_heading_locked())
            {
                g_heading_lock_pending = true;
                bus_lock_heading_to_current();
                g_heading_lock_active = true;
                screen_refresh_compass_target();
            }
            else
            {
                s_ui.state = UI_MODAL_COMPASS;
                screen_show_modal_compass();
            }
        }
        else if (page_id == PAGE_ID_GAS)
        {
            /* GAS 页点击不是立刻切气，而是先进入 EDIT_GAS 选择目标气体，
             * 再通过 modal 二次确认，避免潜水中误切气。 */
            s_ui.state = UI_EDIT_GAS;
            s_ui.gas_cursor = bus_get_gas_active_idx();
            screen_refresh_gas_menu();
        }
        else if (page_id == PAGE_ID_MENU)
        {
            s_ui.state = UI_MENU_ENTRY;
            s_ui.menu_entry_idx = 0;
            menu_entry_set_selection(0);
        }
        break;
    }

    case UI_MENU_ENTRY:
#if ENABLE_INFO_MENU
        if (menu_entry_selection_is_info(s_ui.menu_entry_idx))
        {
            ui_enter_info_menu_page();
        }
        else
#endif
        if (menu_entry_selection_is_end_dive(s_ui.menu_entry_idx))
        {
            if (bus_get_dive_lifecycle_phase() == DIVE_LIFECYCLE_SURFACING_PENDING)
            {
                s_ui.state = UI_MODAL_END_DIVE;
                screen_show_modal_setup_confirm("END DIVE\nCONFIRM SURFACE");
            }
            else
            {
                menu_entry_update();
            }
        }
        else if (menu_entry_selection_is_device(s_ui.menu_entry_idx))
        {
            ui_enter_device_control_page();
        }
        else
        {
            ui_enter_setup_menu_page();
        }
        break;

    case UI_EDIT_GAS:
        s_ui.state = UI_MODAL_GAS;
        s_ui.gas_modal_from_submenu = false;  // HOTFIX: Route GAS modal exit based on context.
        screen_show_modal_gas();
        break;

    case UI_MODAL_GAS:
    {
        uint8_t ci = s_ui.gas_cursor;
        uint8_t gas_count = bus_get_gas_slot_count();
        if (gas_count == 0U)
        {
            screen_pulse_modal();
            break;
        }
        if (ci >= gas_count)
        {
            ci = 0;
        }
        float mod_m = bus_get_gas_slot_mod_m(ci);
        if (mod_m <= 0.0f)
        {
            mod_m = (float)GAS_MOD_M[ci];
        }
        if (bus_get_depth() <= mod_m)
        {
            /* UI 层只发“切到哪一路气体”的请求，不直接改算法态。
             * 真正的数据切换由算法/任务线程执行后，再反向更新 Data Bus。 */
            request_gas_switch(ci);
            screen_hide_modal();
            /* 注意：gas_name 和 gas_active_idx 由算法适配层更新 */
            screen_refresh_gas_menu();
            screen_refresh_left_panel();
            // HOTFIX: Route GAS modal exit based on context.
            if (s_ui.gas_modal_from_submenu)
            {
                s_ui.gas_modal_from_submenu = false;
                screen_close_submenu();
            }
            else
            {
                s_ui.state = UI_DASH;
            }
        }
        else
        {
            screen_pulse_modal();
        }
        break;
    }

    case UI_MODAL_COMPASS:
        bus_clear_heading_lock();
        g_heading_lock_active = false;
        screen_refresh_compass_target();
        screen_hide_modal();
        s_ui.state = UI_DASH;
        break;

    case UI_MODAL_END_DIVE:
        if (bus_get_dive_lifecycle_phase() == DIVE_LIFECYCLE_SURFACING_PENDING)
        {
            ui_on_end_dive_confirm();
        }
        screen_hide_modal();
        s_ui.state = UI_DASH;
        s_ui.menu_entry_idx = 0U;
        menu_entry_clear_selection();
        screen_refresh_setup_menu();
        screen_refresh_left_panel();
        break;

    case UI_MODAL_TURN_OFF:
        ui_on_turn_off();
        screen_hide_modal();
        s_ui.state = UI_DASH;
        screen_refresh_setup_menu();
        screen_refresh_left_panel();
        break;

    case UI_MODAL_SETUP_CONFIRM:
        screen_confirm_submenu_setting();
        break;

    case UI_EDIT_VALUE:
        s_ui.edit_ctx.active = false;
        s_ui.state = UI_SUB_MENU;
        screen_commit_edit_value();
        break;

    case UI_INFO:
#if ENABLE_INFO_MENU
        screen_open_info_submenu(s_ui.menu_info_idx);
#else
        s_ui.state = UI_DASH;
        s_ui.dash_page = PAGE_POS_DYNAMIC_FIRST;
        ui_go_to_page(PAGE_POS_DYNAMIC_FIRST);
#endif
        break;

    case UI_SETUP:
    {
        menu_id_t setup_menu = menu_defs_setup_menu_for_index(s_ui.menu_setup_idx);
        if (setup_menu == MENU_SETUP_BLUETOOTH)
        {
            ui_persisted_settings_snapshot_t snapshot;
            (void)memset(&snapshot, 0, sizeof(snapshot));
            (void)ui_get_persisted_settings_snapshot(&snapshot);
            ui_on_bluetooth_set(snapshot.bluetooth_enabled == 0U);
            screen_refresh_setup_menu();
            break;
        }
        if (setup_menu == MENU_SETUP_TURN_OFF)
        {
            s_ui.state = UI_MODAL_TURN_OFF;
            screen_show_modal_setup_confirm("TURN OFF\nCONFIRM SLEEP");
            break;
        }
        screen_open_setup_submenu(s_ui.menu_setup_idx);
        break;
    }

    case UI_SUB_MENU:
        screen_handle_submenu_select(s_ui.sub_menu_idx);
        break;

    default:
        break;
    }

#if UI_CLICK_PROFILE_ENABLED
    ui_click_profile_note(false, state_before, (uint8_t)s_ui.state, page_id_before,
                          lv_tick_get() - click_start_ms, flush_ms,
                          lv_tick_get() - mark_ms);
#endif
}

/* =========================================
   Back / ESC handler
   ========================================= */
void ui_handle_back(void)
{
    ui_flush_pending_dash_page();

    {
        extern bool alarm_mark_clear_requested(void);
        if (alarm_mark_clear_requested())
        {
            s_ui.alarm_pending_click = false;
            return;
        }
    }

    /* back 只负责“退出当前层级”，不做业务提交。
     * 对编辑态来说是回滚，对 modal 来说是关闭，对子菜单来说是返回上层。 */
    switch (s_ui.state)
    {
    case UI_DASH:
        ui_return_to_card_home();
        break;

    case UI_MENU_ENTRY:
        s_ui.state = UI_DASH;
        s_ui.menu_entry_idx = 0U;
        menu_entry_clear_selection();
        break;

    case UI_EDIT_GAS:
        s_ui.state = UI_DASH;
        screen_refresh_gas_menu();
        break;

    case UI_MODAL_GAS:
        screen_hide_modal();
        // HOTFIX: Route GAS modal exit based on context.
        if (s_ui.gas_modal_from_submenu)
        {
            s_ui.gas_modal_from_submenu = false;
            screen_close_submenu();
        }
        else
        {
            s_ui.state = UI_EDIT_GAS;
        }
        break;

    case UI_MODAL_COMPASS:
    case UI_MODAL_ACT:
        screen_hide_modal();
        if (s_ui.sub_item_count > 0)
        {
            s_ui.state = UI_SUB_MENU;
        }
        else
        {
            s_ui.state = UI_DASH;
        }
        break;

    case UI_MODAL_SETUP_CONFIRM:
        screen_cancel_submenu_setting();
        break;

    case UI_MODAL_END_DIVE:
    case UI_MODAL_TURN_OFF:
        screen_hide_modal();
        if (s_ui.state == UI_MODAL_END_DIVE)
        {
            s_ui.state = UI_MENU_ENTRY;
            menu_entry_set_selection(s_ui.menu_entry_idx);
        }
        else
        {
            s_ui.state = UI_SETUP;
            screen_set_setup_selection(s_ui.menu_setup_idx);
        }
        break;

    case UI_EDIT_VALUE:
        s_ui.edit_ctx.value = s_ui.edit_ctx.original;
        s_ui.edit_ctx.active = false;
        s_ui.state = UI_SUB_MENU;
        screen_cancel_edit_value();
        break;

    case UI_SUB_MENU:
        if (screen_handle_logbook_back())
        {
            break;
        }
        screen_close_submenu();
        break;

    case UI_INFO:
        ui_return_to_card_home();
        break;

    case UI_SETUP:
        ui_return_to_card_home();
        break;

    default:
        break;
    }
}

/* =========================================
   气体切换命令队列接口实现
   ========================================= */
void request_gas_switch(uint8_t gas_idx)
{
    g_gas_switch_cmd.pending = true;
    g_gas_switch_cmd.gas_idx = gas_idx;
}

bool has_pending_gas_switch(uint8_t *out_gas_idx)
{
    if (g_gas_switch_cmd.pending && out_gas_idx != NULL)
    {
        *out_gas_idx = g_gas_switch_cmd.gas_idx;
        return true;
    }
    return false;
}

void clear_gas_switch_cmd(void)
{
    g_gas_switch_cmd.pending = false;
}

void request_gas_ignore(uint8_t gas_idx)
{
    g_gas_ignore_cmd.pending = true;
    g_gas_ignore_cmd.gas_idx = gas_idx;
}

bool has_pending_gas_ignore(uint8_t *out_gas_idx)
{
    if (g_gas_ignore_cmd.pending && out_gas_idx != NULL)
    {
        *out_gas_idx = g_gas_ignore_cmd.gas_idx;
        return true;
    }
    return false;
}

void clear_gas_ignore_cmd(void)
{
    g_gas_ignore_cmd.pending = false;
}

void request_compass_calibration_start(void)
{
    g_compass_cal_cmd.pending = true;
    g_compass_cal_cmd.action = COMPASS_CAL_CMD_START;
}

void request_compass_calibration_reset(void)
{
    g_compass_cal_cmd.pending = true;
    g_compass_cal_cmd.action = COMPASS_CAL_CMD_RESET;
}

bool has_pending_compass_calibration(compass_cal_cmd_action_t *out_action)
{
    if (g_compass_cal_cmd.pending && out_action != NULL)
    {
        *out_action = g_compass_cal_cmd.action;
        return true;
    }
    return false;
}

void clear_compass_calibration_cmd(void)
{
    g_compass_cal_cmd.pending = false;
    g_compass_cal_cmd.action = COMPASS_CAL_CMD_NONE;
}

void set_compass_calibration_ui_state(compass_cal_ui_state_t state)
{
    g_compass_cal_ui_state = state;
}

compass_cal_ui_state_t get_compass_calibration_ui_state(void)
{
    return g_compass_cal_ui_state;
}

ui_state_t ui_state_get_state(void)
{
    return s_ui.state;
}

void ui_state_set_state(ui_state_t state)
{
    s_ui.state = state;
}

uint8_t ui_state_get_dash_page(void)
{
    return s_ui.dash_page;
}

void ui_state_set_dash_page(uint8_t page)
{
    s_ui.dash_page = page;
}

uint8_t ui_state_get_menu_info_idx(void)
{
    return s_ui.menu_info_idx;
}

void ui_state_set_menu_info_idx(uint8_t idx)
{
    s_ui.menu_info_idx = idx;
}

uint8_t ui_state_get_menu_setup_idx(void)
{
    return s_ui.menu_setup_idx;
}

void ui_state_set_menu_setup_idx(uint8_t idx)
{
    s_ui.menu_setup_idx = idx;
}

uint8_t ui_state_get_gas_cursor(void)
{
    return s_ui.gas_cursor;
}

void ui_state_set_gas_cursor(uint8_t cursor)
{
    s_ui.gas_cursor = cursor;
}

uint8_t ui_state_get_sub_menu_idx(void)
{
    return s_ui.sub_menu_idx;
}

void ui_state_set_sub_menu_idx(uint8_t idx)
{
    s_ui.sub_menu_idx = idx;
}

uint8_t ui_state_get_sub_item_count(void)
{
    return s_ui.sub_item_count;
}

void ui_state_set_sub_item_count(uint8_t count)
{
    s_ui.sub_item_count = count;
}

ui_state_t ui_state_get_sub_parent(void)
{
    return s_ui.sub_parent;
}

void ui_state_set_sub_parent(ui_state_t state)
{
    s_ui.sub_parent = state;
}

uint8_t ui_state_get_sub_history_depth(void)
{
    return s_ui.sub_history_depth;
}

void ui_state_set_sub_history_depth(uint8_t depth)
{
    s_ui.sub_history_depth = depth;
}

bool ui_state_get_gas_modal_from_submenu(void)
{
    return s_ui.gas_modal_from_submenu;
}

void ui_state_set_gas_modal_from_submenu(bool enabled)
{
    s_ui.gas_modal_from_submenu = enabled;
}

bool ui_state_get_alarm_pending_click(void)
{
    return s_ui.alarm_pending_click;
}

void ui_state_set_alarm_pending_click(bool pending)
{
    s_ui.alarm_pending_click = pending;
}

bool ui_state_get_edit_active(void)
{
    return s_ui.edit_ctx.active;
}

void ui_state_set_edit_active(bool active)
{
    s_ui.edit_ctx.active = active;
}

uint8_t ui_state_get_edit_item_index(void)
{
    return s_ui.edit_ctx.item_index;
}

void ui_state_set_edit_item_index(uint8_t index)
{
    s_ui.edit_ctx.item_index = index;
}

bool ui_state_get_edit_value_active(void)
{
    return s_ui.edit_ctx.active;
}

float ui_state_get_edit_value(void)
{
    return s_ui.edit_ctx.value;
}

void ui_state_set_edit_value(float value)
{
    s_ui.edit_ctx.value = value;
}

float ui_state_get_edit_original(void)
{
    return s_ui.edit_ctx.original;
}

void ui_state_set_edit_original(float value)
{
    s_ui.edit_ctx.original = value;
}

float ui_state_get_edit_min(void)
{
    return s_ui.edit_ctx.min;
}

void ui_state_set_edit_min(float value)
{
    s_ui.edit_ctx.min = value;
}

float ui_state_get_edit_max(void)
{
    return s_ui.edit_ctx.max;
}

void ui_state_set_edit_max(float value)
{
    s_ui.edit_ctx.max = value;
}

float ui_state_get_edit_step(void)
{
    return s_ui.edit_ctx.step;
}

void ui_state_set_edit_step(float value)
{
    s_ui.edit_ctx.step = value;
}

submenu_setting_kind_t ui_state_get_edit_setting_kind(void)
{
    return s_ui.edit_ctx.setting_kind;
}

void ui_state_set_edit_setting_kind(submenu_setting_kind_t kind)
{
    s_ui.edit_ctx.setting_kind = kind;
}

uint8_t ui_state_get_edit_setting_arg(void)
{
    return s_ui.edit_ctx.setting_arg;
}

void ui_state_set_edit_setting_arg(uint8_t arg)
{
    s_ui.edit_ctx.setting_arg = arg;
}

uint8_t ui_state_get_edit_decimals(void)
{
    return s_ui.edit_ctx.decimals;
}

void ui_state_set_edit_decimals(uint8_t decimals)
{
    s_ui.edit_ctx.decimals = decimals;
}

const char *ui_state_get_edit_label(void)
{
    return s_ui.edit_ctx.label;
}

void ui_state_set_edit_label(const char *label)
{
    if (label == NULL)
    {
        s_ui.edit_ctx.label[0] = '\0';
    }
    else
    {
        lv_snprintf(s_ui.edit_ctx.label, sizeof(s_ui.edit_ctx.label), "%s", label);
    }
}

bool ui_state_get_heading_lock_pending(void)
{
    return g_heading_lock_pending;
}

void ui_state_set_heading_lock_pending(bool pending)
{
    g_heading_lock_pending = pending;
}

bool ui_state_get_heading_lock_active(void)
{
    return g_heading_lock_active;
}

void ui_state_set_heading_lock_active(bool active)
{
    g_heading_lock_active = active;
}
