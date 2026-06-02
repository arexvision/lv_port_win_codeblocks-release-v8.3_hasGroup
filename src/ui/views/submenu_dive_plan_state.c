/*
 * 文件: src/app_ui/ui/views/submenu_dive_plan_state.c
 * 作用: 该文件属于视图表现模块，负责菜单、子菜单、模态框或特定视图状态的展示与交互联动。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "submenu_dive_plan_state.h"

#include "../core/callbacks.h"
#include "../core/data.h"
#include "../core/vm/ui_vm_menu.h"
#include "../core/vm/ui_vm_plan_view.h"

#ifdef PC_SIMULATOR
#include "../../algo_sim/buhlmann_debug.h"
#else
#include "rtthread.h"
#ifdef RT_USING_PM
#include "drivers/pm.h"
#endif
#endif

#include <stdio.h>
#include <string.h>

#define PLAN_ROWS_PER_PAGE 8U
#ifndef PC_SIMULATOR
#define PLAN_ASYNC_THREAD_STACK_SIZE 20480U
#define PLAN_ASYNC_THREAD_PRIORITY_ACTIVE 13U
#define PLAN_ASYNC_THREAD_PRIORITY_IDLE   15U
#define PLAN_ASYNC_THREAD_TICK       10U
#endif

typedef struct
{
    dive_plan_page_t page;
    bool defaults_loaded;
    float depth_m;
    uint16_t time_min;
    float rmv_lpm;
    uint8_t result_page;
    dive_plan_result_snapshot_t result;
} submenu_dive_plan_state_t;

static submenu_dive_plan_state_t s_state =
{
    DIVE_PLAN_PAGE_DEPTH,
    false,
    30.0f,
    20U,
    14.0f,
    0U,
    { 0 }
};

#ifndef PC_SIMULATOR
typedef struct
{
    uint16_t depth_dm;
    uint16_t time_min;
    uint16_t rmv_dlpm;
    uint32_t config_signature;
} dive_plan_input_signature_t;

typedef struct
{
    uint32_t id;
    float depth_m;
    uint16_t time_min;
    float rmv_lpm;
    dive_plan_input_signature_t signature;
} dive_plan_async_request_t;

static volatile bool s_plan_async_running;
static volatile bool s_plan_async_done;
static volatile bool s_plan_async_success;
static volatile uint32_t s_plan_async_generation;
static volatile uint32_t s_plan_async_done_id;
static dive_plan_async_request_t s_plan_async_request;
static dive_plan_result_snapshot_t s_plan_async_result;
static rt_thread_t s_plan_async_thread;
static rt_sem_t s_plan_async_sem;
static rt_mutex_t s_plan_async_mutex;
static bool s_plan_cache_valid;
static dive_plan_input_signature_t s_plan_cache_signature;
static dive_plan_result_snapshot_t s_plan_cache_result;
#endif

static uint16_t plan_round_u16(float value)
{
    /* 浮点输入先按常规四舍五入成整数。 */
    if (value <= 0.0f)
    {
        return 0U;
    }
    if (value >= 65535.0f)
    {
        return 65535U;
    }
    return (uint16_t)(value + 0.5f);
}

static float limit_plan_input_float(float value, float min_value, float max_value)
{
    /* 潜水计划的输入值需要限制在合理范围内。 */
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

#ifndef PC_SIMULATOR
static uint16_t plan_round_tenths_u16(float value)
{
    return plan_round_u16(value * 10.0f);
}

static dive_plan_input_signature_t plan_make_input_signature(void)
{
    dive_plan_input_signature_t signature = { 0 };

    signature.depth_dm = plan_round_tenths_u16(s_state.depth_m);
    signature.time_min = s_state.time_min;
    signature.rmv_dlpm = plan_round_tenths_u16(s_state.rmv_lpm);
    signature.config_signature = ui_get_dive_plan_config_signature();
    return signature;
}

static bool plan_input_signature_cacheable(const dive_plan_input_signature_t *signature)
{
    return (signature != NULL) && (signature->config_signature != 0U);
}

static bool plan_input_signature_equal(const dive_plan_input_signature_t *a,
                                       const dive_plan_input_signature_t *b)
{
    return (a != NULL) && (b != NULL) &&
           (a->depth_dm == b->depth_dm) &&
           (a->time_min == b->time_min) &&
           (a->rmv_dlpm == b->rmv_dlpm) &&
           (a->config_signature == b->config_signature);
}

static bool plan_apply_cached_result_if_current(void)
{
    dive_plan_input_signature_t signature = plan_make_input_signature();

    if (!s_plan_cache_valid ||
        !plan_input_signature_cacheable(&signature) ||
        !plan_input_signature_equal(&s_plan_cache_signature, &signature))
    {
        return false;
    }

    submenu_dive_plan_set_result_snapshot(&s_plan_cache_result);
    s_state.page = DIVE_PLAN_PAGE_RESULT;
    rt_kprintf("[dplan] cache hit depth=%.0fm time=%umin rmv=%.0f cfg=0x%08x\n",
               s_state.depth_m,
               (unsigned)s_state.time_min,
               s_state.rmv_lpm,
               (unsigned)signature.config_signature);
    return true;
}

static void plan_store_cached_result(const dive_plan_input_signature_t *signature,
                                     const dive_plan_result_snapshot_t *snapshot)
{
    if (!plan_input_signature_cacheable(signature) || snapshot == NULL || snapshot->valid == 0U)
    {
        return;
    }

    s_plan_cache_signature = *signature;
    s_plan_cache_result = *snapshot;
    s_plan_cache_valid = true;
}

static bool plan_async_lock(void)
{
    return (s_plan_async_mutex != RT_NULL) &&
           (rt_mutex_take(s_plan_async_mutex, rt_tick_from_millisecond(50U)) == RT_EOK);
}

static bool plan_async_lock_wait(void)
{
    return (s_plan_async_mutex != RT_NULL) &&
           (rt_mutex_take(s_plan_async_mutex, RT_WAITING_FOREVER) == RT_EOK);
}

static void plan_async_unlock(void)
{
    if (s_plan_async_mutex != RT_NULL)
    {
        rt_mutex_release(s_plan_async_mutex);
    }
}

static bool plan_async_is_running_locked(void)
{
    return s_plan_async_running;
}
#endif

static uint8_t plan_gf_low(void)
{
    /* 当前低 GF 直接从 VM 读取，保证和全局配置一致。 */
    ui_vm_dive_context_t vm;

    ui_vm_dive_context_update(&vm);
    return vm.gf_low;
}

static uint8_t plan_gf_high(void)
{
    /* 当前高 GF 直接从 VM 读取。 */
    ui_vm_dive_context_t vm;

    ui_vm_dive_context_update(&vm);
    return vm.gf_high;
}

static uint8_t plan_last_deco_depth(void)
{
    /* 最后减压停留深度同样来自 VM 上下文。 */
    ui_vm_dive_context_t vm;

    ui_vm_dive_context_update(&vm);
    return vm.last_stop_depth_m;
}

static void plan_ensure_defaults(void)
{
    /* 第一次进入计划页时先加载默认输入值。 */
    ui_vm_dive_plan_inputs_t vm;

    if (s_state.defaults_loaded)
    {
        return;
    }

    ui_vm_dive_plan_inputs_update(&vm);
    s_state.depth_m = vm.depth_m;
    s_state.time_min = vm.time_min;
    s_state.rmv_lpm = vm.rmv_lpm;
    s_state.defaults_loaded = true;
}

static uint8_t plan_result_total_pages(void)
{
    /* 结果页总页数根据结果有效性和条目数动态决定。 */
    if (s_state.result.valid == 0U)
    {
        return 1U;
    }
    if (s_state.result.total_pages == 0U)
    {
        uint8_t row_pages = (uint8_t)((s_state.result.entry_count + PLAN_ROWS_PER_PAGE - 1U) / PLAN_ROWS_PER_PAGE);
        return (uint8_t)(row_pages + 1U);
    }
    return s_state.result.total_pages;
}

#ifdef PC_SIMULATOR
static dive_plan_row_type_t plan_row_type_from_algo(buhlmann_debug_plan_row_type_t type)
{
    switch (type)
    {
    case BUHLMANN_DEBUG_PLAN_ROW_DECO_STOP:
        return DIVE_PLAN_ROW_DECO_STOP;
    case BUHLMANN_DEBUG_PLAN_ROW_ASCENT:
        return DIVE_PLAN_ROW_ASCENT;
    case BUHLMANN_DEBUG_PLAN_ROW_BOTTOM:
    default:
        return DIVE_PLAN_ROW_BOTTOM;
    }
}

bool dive_plan_backend_calculate(float depth_m,
                                 uint16_t bottom_time_min,
                                 float rmv_lpm,
                                 dive_plan_result_snapshot_t *out_snapshot)
{
    buhlmann_debug_plan_result_t algo_result;
    uint8_t row_count;

    if (out_snapshot == NULL)
    {
        return false;
    }

    (void)memset(out_snapshot, 0, sizeof(*out_snapshot));
    if (!buhlmann_debug_plan_calculate(depth_m,
                                       bottom_time_min,
                                       rmv_lpm,
                                       &algo_result))
    {
        return false;
    }

    row_count = algo_result.entry_count;
    if (row_count > DIVE_PLAN_RESULT_MAX_ROWS)
    {
        row_count = DIVE_PLAN_RESULT_MAX_ROWS;
    }

    out_snapshot->valid = 1U;
    out_snapshot->page = 0U;
    out_snapshot->entry_count = row_count;
    out_snapshot->total_pages = (uint8_t)((row_count + PLAN_ROWS_PER_PAGE - 1U) / PLAN_ROWS_PER_PAGE);
    if (out_snapshot->total_pages == 0U)
    {
        out_snapshot->total_pages = 1U;
    }
    out_snapshot->total_pages++;
    out_snapshot->total_runtime_min = algo_result.total_runtime_min;
    out_snapshot->total_deco_min = algo_result.total_deco_min;
    out_snapshot->total_gas_l = algo_result.total_gas_l;
    out_snapshot->cns_pct = algo_result.cns_pct;
    out_snapshot->otu = algo_result.otu;

    for (uint8_t i = 0U; i < row_count; i++)
    {
        out_snapshot->rows[i].type = plan_row_type_from_algo(algo_result.entries[i].type);
        out_snapshot->rows[i].depth_m = algo_result.entries[i].depth_m;
        out_snapshot->rows[i].time_min = algo_result.entries[i].time_min;
        out_snapshot->rows[i].run_min = algo_result.entries[i].run_min;
        out_snapshot->rows[i].o2_pct = algo_result.entries[i].o2_pct;
        out_snapshot->rows[i].he_pct = algo_result.entries[i].he_pct;
        out_snapshot->rows[i].gas_l = algo_result.entries[i].gas_l;
    }
    return true;
}
#else
__attribute__((weak)) bool dive_plan_backend_calculate(float depth_m,
                                                       uint16_t bottom_time_min,
                                                       float rmv_lpm,
                                                       dive_plan_result_snapshot_t *out_snapshot)
{
    (void)depth_m;
    (void)bottom_time_min;
    (void)rmv_lpm;
    (void)out_snapshot;
    return false;
}
#endif

static void plan_execute(void)
{
    dive_plan_result_snapshot_t snapshot;

    if (!dive_plan_backend_calculate(s_state.depth_m, s_state.time_min, s_state.rmv_lpm, &snapshot))
    {
        submenu_dive_plan_set_result_snapshot(NULL);
        s_state.page = DIVE_PLAN_PAGE_ERROR;
        return;
    }

    submenu_dive_plan_set_result_snapshot(&snapshot);
    s_state.page = DIVE_PLAN_PAGE_RESULT;
}

#ifndef PC_SIMULATOR
static uint32_t plan_async_now_ms(void)
{
    return rt_tick_get_millisecond();
}

static void plan_async_pm_hold(void)
{
#ifdef RT_USING_PM
    /*
     * DivePlan 是 CPU 密集型后台计算，不能通过提高线程优先级来提速：
     * LCD/LVGL/DMA 消费链路必须始终优先于它。这里仅阻止计算期间进入
     * idle 低功耗，让大计算稳定跑完，同时不破坏 RTOS 调度优先级。
     */
    rt_pm_request(PM_SLEEP_MODE_IDLE);
#endif
}

static void plan_async_pm_release(void)
{
#ifdef RT_USING_PM
    rt_pm_release(PM_SLEEP_MODE_IDLE);
#endif
}

static void plan_async_worker(void *parameter)
{
    (void)parameter;

    rt_thread_t self = rt_thread_self();
    rt_uint8_t active_priority = PLAN_ASYNC_THREAD_PRIORITY_ACTIVE;
    rt_uint8_t idle_priority = PLAN_ASYNC_THREAD_PRIORITY_IDLE;

    while (1)
    {
        if (s_plan_async_sem == RT_NULL ||
            rt_sem_take(s_plan_async_sem, RT_WAITING_FOREVER) != RT_EOK)
        {
            continue;
        }

        dive_plan_async_request_t request;
        dive_plan_result_snapshot_t snapshot;
        uint32_t start_ms = plan_async_now_ms();
        bool success;

        if (!plan_async_lock_wait())
        {
            continue;
        }
        request = s_plan_async_request;
        plan_async_unlock();

        if (self != RT_NULL)
        {
            (void)rt_thread_control(self, RT_THREAD_CTRL_CHANGE_PRIORITY, &active_priority);
        }

        plan_async_pm_hold();
        success = dive_plan_backend_calculate(request.depth_m,
                                             request.time_min,
                                             request.rmv_lpm,
                                             &snapshot);
        plan_async_pm_release();

        if (self != RT_NULL)
        {
            (void)rt_thread_control(self, RT_THREAD_CTRL_CHANGE_PRIORITY, &idle_priority);
        }

        if (plan_async_lock_wait())
        {
            uint32_t current_generation = s_plan_async_generation;

            if (success)
            {
                s_plan_async_result = snapshot;
                plan_store_cached_result(&request.signature, &snapshot);
            }
            s_plan_async_success = success;
            s_plan_async_done_id = request.id;
            s_plan_async_done = true;
            s_plan_async_running = false;
            plan_async_unlock();

            rt_kprintf("[dplan] id=%u depth=%.0fm time=%umin rmv=%.0f success=%u obsolete=%u elapsed=%ums prio=%u cfg=0x%08x\n",
                       (unsigned)request.id,
                       request.depth_m,
                       (unsigned)request.time_min,
                       request.rmv_lpm,
                       success ? 1U : 0U,
                       (request.id == current_generation) ? 0U : 1U,
                       (unsigned)(plan_async_now_ms() - start_ms),
                       (unsigned)active_priority,
                       (unsigned)request.signature.config_signature);
        }
        else
        {
            s_plan_async_running = false;

            rt_kprintf("[dplan] id=%u depth=%.0fm time=%umin rmv=%.0f success=%u obsolete=1 elapsed=%ums prio=%u cfg=0x%08x lock_fail=1\n",
                       (unsigned)request.id,
                       request.depth_m,
                       (unsigned)request.time_min,
                       request.rmv_lpm,
                       success ? 1U : 0U,
                       (unsigned)(plan_async_now_ms() - start_ms),
                       (unsigned)active_priority,
                       (unsigned)request.signature.config_signature);
        }
    }
}

static bool plan_async_ensure_worker(void)
{
    if (s_plan_async_mutex == RT_NULL)
    {
        s_plan_async_mutex = rt_mutex_create("dplanM", RT_IPC_FLAG_FIFO);
        if (s_plan_async_mutex == RT_NULL)
        {
            return false;
        }
    }

    if (s_plan_async_sem == RT_NULL)
    {
        s_plan_async_sem = rt_sem_create("dplanS", 0U, RT_IPC_FLAG_FIFO);
        if (s_plan_async_sem == RT_NULL)
        {
            return false;
        }
    }

    if (s_plan_async_thread != RT_NULL)
    {
        return true;
    }

    s_plan_async_thread = rt_thread_create("dplan",
                                           plan_async_worker,
                                           RT_NULL,
                                           PLAN_ASYNC_THREAD_STACK_SIZE,
                                           PLAN_ASYNC_THREAD_PRIORITY_IDLE,
                                           PLAN_ASYNC_THREAD_TICK);
    if (s_plan_async_thread == RT_NULL)
    {
        return false;
    }

    rt_thread_startup(s_plan_async_thread);
    return true;
}

static bool plan_start_async(void)
{
    if (s_plan_async_running)
    {
        return false;
    }
    if (!plan_async_ensure_worker())
    {
        return false;
    }

    if (!plan_async_lock())
    {
        return false;
    }

    if (s_plan_async_running)
    {
        plan_async_unlock();
        return false;
    }

    s_plan_async_generation++;
    if (s_plan_async_generation == 0U)
    {
        s_plan_async_generation = 1U;
    }

    s_plan_async_request.id = s_plan_async_generation;
    s_plan_async_request.depth_m = s_state.depth_m;
    s_plan_async_request.time_min = s_state.time_min;
    s_plan_async_request.rmv_lpm = s_state.rmv_lpm;
    s_plan_async_request.signature = plan_make_input_signature();
    s_plan_async_done = false;
    s_plan_async_success = false;
    s_plan_async_done_id = 0U;
    s_plan_async_running = true;
    plan_async_unlock();

    if (rt_sem_release(s_plan_async_sem) != RT_EOK)
    {
        if (plan_async_lock())
        {
            s_plan_async_running = false;
            plan_async_unlock();
        }
        return false;
    }
    return true;
}
#endif

void submenu_dive_plan_set_result_snapshot(const dive_plan_result_snapshot_t *snapshot)
{
    /* 结果快照由算法/上层计算后统一写回这里。 */
    if (snapshot == NULL)
    {
        (void)memset(&s_state.result, 0, sizeof(s_state.result));
        s_state.result_page = 0U;
        return;
    }

    s_state.result = *snapshot;
    if (s_state.result.page >= plan_result_total_pages())
    {
        s_state.result.page = 0U;
    }
    s_state.result_page = s_state.result.page;
}

void submenu_dive_plan_get_snapshot(submenu_dive_plan_snapshot_t *out_snapshot)
{
    /* 向 view 层导出当前计划页的完整快照。 */
    ui_vm_dive_plan_inputs_t input_vm;

    if (out_snapshot == NULL)
    {
        return;
    }

    (void)memset(out_snapshot, 0, sizeof(*out_snapshot));

    plan_ensure_defaults();
    ui_vm_dive_plan_inputs_update(&input_vm);

    out_snapshot->page = s_state.page;
    out_snapshot->depth_m = s_state.depth_m;
    out_snapshot->time_min = s_state.time_min;
    out_snapshot->rmv_lpm = s_state.rmv_lpm;
    out_snapshot->gf_low = plan_gf_low();
    out_snapshot->gf_high = plan_gf_high();
    out_snapshot->last_stop_depth_m = plan_last_deco_depth();
    out_snapshot->header_gas_o2 = input_vm.header_gas_o2;
    (void)snprintf(out_snapshot->gas_summary,
                   sizeof(out_snapshot->gas_summary),
                   "%s",
                   input_vm.gas_summary);

    out_snapshot->result_page_index = s_state.result_page;
    out_snapshot->result_total_pages = plan_result_total_pages();
    out_snapshot->result_entry_count = (s_state.result.valid != 0U) ? s_state.result.entry_count : 0U;
    out_snapshot->result_summary_page =
        (s_state.result.valid != 0U &&
         s_state.result_page * PLAN_ROWS_PER_PAGE >= s_state.result.entry_count) ? 1U : 0U;
    out_snapshot->total_runtime_min = (s_state.result.valid != 0U) ? s_state.result.total_runtime_min : 0U;
    out_snapshot->total_deco_min = (s_state.result.valid != 0U) ? s_state.result.total_deco_min : 0U;
    out_snapshot->total_gas_l = (s_state.result.valid != 0U) ? s_state.result.total_gas_l : 0U;
    out_snapshot->cns_pct = (s_state.result.valid != 0U) ? s_state.result.cns_pct : 0U;
    out_snapshot->otu = (s_state.result.valid != 0U) ? s_state.result.otu : 0U;

    if (s_state.result.valid != 0U)
    {
        uint8_t row_count = s_state.result.entry_count;

        if (row_count > DIVE_PLAN_RESULT_MAX_ROWS)
        {
            row_count = DIVE_PLAN_RESULT_MAX_ROWS;
        }
        for (uint8_t i = 0U; i < row_count; i++)
        {
            out_snapshot->rows[i] = s_state.result.rows[i];
        }
    }
}

void submenu_dive_plan_set_page(dive_plan_page_t page)
{
    /* 切换潜水计划页内部页码。 */
    s_state.page = page;
}

dive_plan_page_t submenu_dive_plan_get_page(void)
{
    return s_state.page;
}

uint8_t submenu_dive_plan_get_result_page_index(void)
{
    return s_state.result_page;
}

uint8_t submenu_dive_plan_get_result_total_pages(void)
{
    return plan_result_total_pages();
}

void submenu_dive_plan_set_depth_m(float value)
{
    /* 深度输入会被归一化并带入 READY 页。 */
    plan_ensure_defaults();
    s_state.depth_m = (float)plan_round_u16(value);
    s_state.depth_m = limit_plan_input_float(s_state.depth_m, 3.0f, 120.0f);
    s_state.page = DIVE_PLAN_PAGE_READY;
}

void submenu_dive_plan_set_time_min(float value)
{
    /* 时间输入同样做四舍五入和上下限约束。 */
    uint16_t rounded_value;

    plan_ensure_defaults();
    rounded_value = plan_round_u16(value);
    if (rounded_value < 1U)
    {
        rounded_value = 1U;
    }
    if (rounded_value > 300U)
    {
        rounded_value = 300U;
    }
    s_state.time_min = rounded_value;
    s_state.page = DIVE_PLAN_PAGE_READY;
}

void submenu_dive_plan_set_rmv_lpm(float value)
{
    /* RMV 输入会被限制在合理人体范围。 */
    plan_ensure_defaults();
    s_state.rmv_lpm = (float)plan_round_u16(value);
    s_state.rmv_lpm = limit_plan_input_float(s_state.rmv_lpm, 5.0f, 50.0f);
    s_state.page = DIVE_PLAN_PAGE_READY;
}

bool submenu_dive_plan_handle_rotate(int8_t dir)
{
    /* 旋钮在计划页里用于调整当前输入值。 */
    plan_ensure_defaults();
    switch (s_state.page)
    {
    case DIVE_PLAN_PAGE_DEPTH:
        s_state.depth_m += (float)dir;
        if (s_state.depth_m < 3.0f) s_state.depth_m = 3.0f;
        if (s_state.depth_m > 120.0f) s_state.depth_m = 120.0f;
        return true;
    case DIVE_PLAN_PAGE_TIME:
    {
        int next = (int)s_state.time_min + (int)dir;
        if (next < 1) next = 1;
        if (next > 300) next = 300;
        s_state.time_min = (uint16_t)next;
        return true;
    }
    case DIVE_PLAN_PAGE_RMV:
        s_state.rmv_lpm += (float)dir;
        if (s_state.rmv_lpm < 5.0f) s_state.rmv_lpm = 5.0f;
        if (s_state.rmv_lpm > 50.0f) s_state.rmv_lpm = 50.0f;
        return true;
    default:
        return false;
    }
}

bool submenu_dive_plan_is_result_page(void)
{
    return s_state.page == DIVE_PLAN_PAGE_RESULT;
}

bool submenu_dive_plan_is_calculating(void)
{
#ifdef PC_SIMULATOR
    return s_state.page == DIVE_PLAN_PAGE_CALCULATING;
#else
    bool running = false;

    if (s_plan_async_mutex != RT_NULL && plan_async_lock())
    {
        running = s_plan_async_running;
        plan_async_unlock();
    }
    return (s_state.page == DIVE_PLAN_PAGE_CALCULATING) || running;
#endif
}

bool submenu_dive_plan_poll_async(void)
{
#ifdef PC_SIMULATOR
    return false;
#else
    bool success = false;
    uint32_t done_id = 0U;
    uint32_t generation = 0U;
    dive_plan_result_snapshot_t result;

    if (!plan_async_lock())
    {
        return false;
    }

    if (!s_plan_async_done)
    {
        plan_async_unlock();
        return false;
    }

    success = s_plan_async_success;
    done_id = s_plan_async_done_id;
    generation = s_plan_async_generation;
    if (success)
    {
        result = s_plan_async_result;
    }
    s_plan_async_done = false;
    plan_async_unlock();

    if (done_id != generation)
    {
        return false;
    }

    if (success)
    {
        submenu_dive_plan_set_result_snapshot(&result);
        s_state.page = DIVE_PLAN_PAGE_RESULT;
    }
    else
    {
        submenu_dive_plan_set_result_snapshot(NULL);
        s_state.page = DIVE_PLAN_PAGE_ERROR;
    }
    return true;
#endif
}

bool submenu_dive_plan_handle_action(menu_item_id_t item_id,
                                     bool *out_close_submenu,
                                     uint8_t *out_keep_index)
{
    /* 点击动作用于推进计划页的内部导航和结果页切换。 */
    if (out_close_submenu != NULL)
    {
        *out_close_submenu = false;
    }
    if (out_keep_index != NULL)
    {
        *out_keep_index = 0U;
    }
    if (item_id == MENU_ITEM_DIVE_PLAN_EXIT)
    {
        if (out_close_submenu != NULL)
        {
            *out_close_submenu = true;
        }
        return true;
    }

    if (item_id == MENU_ITEM_DIVE_PLAN_NEXT)
    {
        switch (s_state.page)
        {
        case DIVE_PLAN_PAGE_DEPTH:
            s_state.page = DIVE_PLAN_PAGE_TIME;
            break;
        case DIVE_PLAN_PAGE_TIME:
            s_state.page = DIVE_PLAN_PAGE_RMV;
            break;
        case DIVE_PLAN_PAGE_RMV:
            s_state.page = DIVE_PLAN_PAGE_READY;
            break;
        case DIVE_PLAN_PAGE_RESULT:
        case DIVE_PLAN_PAGE_ERROR:
            s_state.page = DIVE_PLAN_PAGE_READY;
            break;
        default:
            break;
        }
        if (out_keep_index != NULL)
        {
            *out_keep_index = 1U;
        }
        return true;
    }

    if (item_id == MENU_ITEM_DIVE_PLAN_MORE)
    {
        uint8_t total_pages = plan_result_total_pages();
        if (s_state.result_page + 1U < total_pages)
        {
            s_state.result_page++;
        }
        if (out_keep_index != NULL)
        {
            *out_keep_index = 1U;
        }
        return true;
    }

    if (item_id == MENU_ITEM_DIVE_PLAN_PLAN)
    {
        plan_ensure_defaults();
        s_state.result_page = 0U;
#ifdef PC_SIMULATOR
        plan_execute();
#else
        if (plan_apply_cached_result_if_current())
        {
            if (out_keep_index != NULL)
            {
                *out_keep_index = 1U;
            }
            return true;
        }
        if (s_plan_async_mutex != RT_NULL && plan_async_lock())
        {
            bool running = plan_async_is_running_locked();
            plan_async_unlock();
            if (running)
            {
                s_state.page = DIVE_PLAN_PAGE_CALCULATING;
                if (out_keep_index != NULL)
                {
                    *out_keep_index = 1U;
                }
                return true;
            }
        }
        else if (s_plan_async_running)
        {
            /*
             * 这里是 worker 尚未初始化或锁暂不可得时的保守退化：
             * 若看到运行标志，宁可保持 calculating，也不要重复提交大计算。
             */
            s_state.page = DIVE_PLAN_PAGE_CALCULATING;
            if (out_keep_index != NULL)
            {
                *out_keep_index = 1U;
            }
            return true;
        }
        submenu_dive_plan_set_result_snapshot(NULL);
        s_state.page = DIVE_PLAN_PAGE_CALCULATING;
        if (!plan_start_async())
        {
            s_state.page = DIVE_PLAN_PAGE_ERROR;
        }
#endif
        if (out_keep_index != NULL)
        {
            *out_keep_index = 1U;
        }
        return true;
    }

    return false;
}

void submenu_dive_plan_reset(void)
{
#ifndef PC_SIMULATOR
    if (s_plan_async_mutex != RT_NULL && plan_async_lock())
    {
        s_plan_async_generation++;
        s_plan_async_done = false;
        plan_async_unlock();
    }
    else
    {
        /* worker 正在持锁收口结果时，不无锁改共享标志，避免撕裂完成态。 */
    }
#endif
    s_state.page = DIVE_PLAN_PAGE_DEPTH;
    s_state.defaults_loaded = false;
    s_state.depth_m = 30.0f;
    s_state.time_min = 20U;
    s_state.rmv_lpm = 14.0f;
    s_state.result_page = 0U;
    (void)memset(&s_state.result, 0, sizeof(s_state.result));
}
