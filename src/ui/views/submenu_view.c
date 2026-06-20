/*
 * 文件: src/app_ui/ui/views/submenu_view.c
 * 作用: 该文件属于视图表现模块，负责菜单、子菜单、模态框或特定视图状态的展示与交互联动。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "submenu_view.h"

#include "../../config/build/ui_debug_flags.h"
#include "../core/callbacks.h"
#include "../core/data.h"
#include "../screen/screen.h"
#include "menu_actions.h"
#include "submenu_model.h"
#include "menu_runtime.h"
#include "../core/vm/ui_vm_plan_view.h"
#include "../core/vm/ui_vm_system_view.h"
#include "../core/vm/ui_vm_menu_types.h"
#include "../core/ui_state.h"
#include "../comp/depth_chart_renderer.h"
#include "../fonts/fonts.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static lv_obj_t *s_submenu_layer = NULL;
static lv_obj_t *s_submenu_title = NULL;
static lv_obj_t *s_submenu_title_line = NULL;
static lv_obj_t *s_submenu_list = NULL;
static lv_obj_t *s_light_status_lbl = NULL;
static uint16_t s_submenu_width = 0;
static uint16_t s_submenu_height = 0;
static ui_vm_dive_plan_view_t s_dive_plan_last_vm;
static bool s_dive_plan_last_vm_valid = false;
static bool s_submenu_selection_scroll_silent = false;

typedef enum
{
    LOGBOOK_PAGE_PICK = 0,
    LOGBOOK_PAGE_SUMMARY,
    LOGBOOK_PAGE_DETAIL_1,
    LOGBOOK_PAGE_DETAIL_2,
} logbook_page_t;

typedef struct
{
    bool valid;
    logbook_meta_t meta;
    uint8_t recovered;
    uint8_t abnormal_end;
} logbook_picker_item_t;

enum
{
    LOGBOOK_PICKER_VISIBLE_ROWS = 5U,
};

static logbook_page_t s_logbook_page = LOGBOOK_PAGE_PICK;
static uint16_t s_logbook_page_index = 0U;
static uint16_t s_logbook_page_count = 0U;
static uint16_t s_logbook_snapshot_count = 0U;
static uint8_t s_logbook_picker_visible = 0U;
static uint8_t s_logbook_picker_focus = 0U;
static uint16_t s_logbook_index = 0U;
static uint16_t s_logbook_focus = 0U;
static logbook_entry_t s_logbook_entry;
static logbook_picker_item_t *s_logbook_snapshot_entries = NULL;
static const dive_pt_t *s_logbook_points = NULL;
static uint16_t s_logbook_point_count;
static lv_timer_t *s_logbook_load_timer = NULL;
static lv_timer_t *s_logbook_detail_timer = NULL;
static uint16_t s_logbook_load_page = 0U;
static uint8_t s_logbook_load_offset = 0U;
static bool s_logbook_load_prefetch = false;
static bool s_logbook_load_page_dirty = false;
static uint16_t s_logbook_detail_index = 0U;
static bool s_logbook_detail_loading = false;
static bool s_logbook_picker_view_ready = false;
static lv_obj_t *s_logbook_picker_title = NULL;
static lv_obj_t *s_logbook_picker_count = NULL;
static lv_obj_t *s_logbook_picker_page = NULL;
static lv_obj_t *s_logbook_picker_rows[LOGBOOK_PICKER_VISIBLE_ROWS];
static lv_obj_t *s_logbook_picker_left[LOGBOOK_PICKER_VISIBLE_ROWS];
static lv_obj_t *s_logbook_picker_right[LOGBOOK_PICKER_VISIBLE_ROWS];
static lv_obj_t *s_logbook_picker_back = NULL;
static lv_obj_t *s_logbook_picker_back_lbl = NULL;
static lv_obj_t *s_logbook_picker_next = NULL;
static lv_obj_t *s_logbook_picker_next_lbl = NULL;
static bool s_logbook_valid = false;
static bool s_logbook_points_loaded = false;
static const dive_pt_t *s_logbook_prefetch_points = NULL;
static uint16_t s_logbook_prefetch_index = 0xFFFFU;
static uint16_t s_logbook_prefetch_point_count = 0U;
static bool s_logbook_prefetch_ready = false;

#ifndef PC_SIMULATOR
enum
{
    LOGBOOK_DETAIL_THREAD_STACK_SIZE = 4096U,
    LOGBOOK_DETAIL_THREAD_PRIORITY = 15U,
    LOGBOOK_DETAIL_THREAD_TICK = 10U,
    LOGBOOK_PREFETCH_THREAD_STACK_SIZE = 3072U,
    LOGBOOK_PREFETCH_THREAD_PRIORITY = 16U,
    LOGBOOK_PREFETCH_THREAD_TICK = 10U,
};

typedef struct
{
    uint32_t id;
    uint16_t index;
    bool load_points;
} logbook_detail_request_t;

typedef struct
{
    uint32_t id;
    uint16_t index;
    bool success;
    bool has_points;
    logbook_entry_t entry;
    const dive_pt_t *points;
    uint16_t point_count;
} logbook_detail_result_t;

typedef struct
{
    uint32_t id;
    uint16_t index;
} logbook_prefetch_request_t;

static volatile bool s_logbook_detail_async_running = false;
static volatile bool s_logbook_detail_async_done = false;
static volatile uint32_t s_logbook_detail_async_generation = 0U;
static logbook_detail_request_t s_logbook_detail_async_request;
static logbook_detail_result_t s_logbook_detail_async_result;
static rt_thread_t s_logbook_detail_thread = RT_NULL;
static rt_sem_t s_logbook_detail_sem = RT_NULL;
static rt_mutex_t s_logbook_detail_mutex = RT_NULL;
static volatile bool s_logbook_prefetch_running = false;
static volatile uint32_t s_logbook_prefetch_generation = 0U;
static logbook_prefetch_request_t s_logbook_prefetch_request;
static rt_thread_t s_logbook_prefetch_thread = RT_NULL;
static rt_sem_t s_logbook_prefetch_sem = RT_NULL;
static rt_mutex_t s_logbook_prefetch_mutex = RT_NULL;
#endif

#if UI_LOGBOOK_PROFILE_ENABLED
static uint32_t logbook_profile_begin(void)
{
    return rt_tick_get_millisecond();
}

static void logbook_profile_end(const char *name, uint32_t start_ms)
{
    rt_kprintf("[UI_LOGBOOK_PROFILE] %s cost=%lums\r\n",
               name ? name : "unknown",
               (unsigned long)(rt_tick_get_millisecond() - start_ms));
}
#else
static uint32_t logbook_profile_begin(void)
{
    return 0U;
}

static void logbook_profile_end(const char *name, uint32_t start_ms)
{
    (void)name;
    (void)start_ms;
}
#endif

static void screen_handle_logbook_select(void);
static void refresh_current_submenu_page(uint8_t keep_idx);
static void logbook_picker_cache_invalidate(void);
static bool logbook_picker_snapshot_refresh(void);
static void logbook_picker_bind_current_page(void);
static void logbook_picker_load_page_sync(uint16_t page_index);
static void logbook_picker_item_from_entry(logbook_picker_item_t *dst, const logbook_entry_t *src);
static void logbook_entry_from_picker_item(logbook_entry_t *dst, const logbook_picker_item_t *src);
static void logbook_entry_make_pending(logbook_entry_t *dst, uint16_t index);
static uint8_t logbook_picker_focus_to_index(uint8_t focus, uint8_t visible_count);
static bool logbook_picker_has_back(void);
static bool logbook_picker_has_next(void);
static uint8_t logbook_picker_focus_back(void);
static uint8_t logbook_picker_focus_next(void);
static uint8_t logbook_picker_focus_count(void);
static void logbook_picker_focus_default(void);
static void submenu_populate_current(void);
static void logbook_picker_stop_loader(void);
static void logbook_picker_start_loader(uint16_t page_index, bool prefetch);
static void logbook_detail_stop_loader(void);
static void logbook_detail_start_loader(uint16_t index);
static void logbook_prefetch_cancel(void);
static void logbook_prefetch_start(uint16_t index);
static bool logbook_prefetch_take(uint16_t index);
static bool logbook_prefetch_take_wait(uint16_t index, uint32_t timeout_ms);
static void logbook_picker_view_reset(void);
static void logbook_picker_load_timer_cb(lv_timer_t *timer);
static void logbook_detail_load_timer_cb(lv_timer_t *timer);
static bool logbook_points_load(uint16_t index);

static void logbook_points_release(void)
{
    if (s_logbook_points == NULL)
    {
        return;
    }

    logbook_backend_release_samples(s_logbook_points);
    s_logbook_points = NULL;
    s_logbook_point_count = 0U;
    s_logbook_points_loaded = false;
}

static void logbook_prefetch_release_ready(void)
{
    if (s_logbook_prefetch_ready && s_logbook_prefetch_points != NULL)
    {
        logbook_backend_release_samples(s_logbook_prefetch_points);
    }
    s_logbook_prefetch_points = NULL;
    s_logbook_prefetch_point_count = 0U;
    s_logbook_prefetch_index = 0xFFFFU;
    s_logbook_prefetch_ready = false;
}

#ifndef PC_SIMULATOR
static bool logbook_detail_async_lock(void)
{
    return (s_logbook_detail_mutex != RT_NULL) &&
           (rt_mutex_take(s_logbook_detail_mutex, rt_tick_from_millisecond(50U)) == RT_EOK);
}

static bool logbook_detail_async_lock_wait(void)
{
    return (s_logbook_detail_mutex != RT_NULL) &&
           (rt_mutex_take(s_logbook_detail_mutex, RT_WAITING_FOREVER) == RT_EOK);
}

static void logbook_detail_async_unlock(void)
{
    if (s_logbook_detail_mutex != RT_NULL)
    {
        rt_mutex_release(s_logbook_detail_mutex);
    }
}

static void logbook_detail_async_worker(void *parameter)
{
    (void)parameter;

    while (1)
    {
        if (s_logbook_detail_sem == RT_NULL ||
            rt_sem_take(s_logbook_detail_sem, RT_WAITING_FOREVER) != RT_EOK)
        {
            continue;
        }

        logbook_detail_request_t request;
        logbook_entry_t entry;
        const dive_pt_t *points = NULL;
        uint16_t point_count = 0U;
        bool success = false;
        bool has_points = false;

        if (!logbook_detail_async_lock_wait())
        {
            continue;
        }
        request = s_logbook_detail_async_request;
        logbook_detail_async_unlock();

        success = logbook_backend_get_detail(request.index, &entry);
        if (success && entry.valid && request.load_points)
        {
            has_points = logbook_backend_acquire_samples(request.index, &points, &point_count);
            if (!has_points)
            {
                points = NULL;
                point_count = 0U;
            }
        }

        if (!logbook_detail_async_lock_wait())
        {
            if (has_points)
            {
                logbook_backend_release_samples(points);
            }
            continue;
        }

        if (request.id == s_logbook_detail_async_request.id &&
            request.index == s_logbook_detail_async_request.index)
        {
            s_logbook_detail_async_result.id = request.id;
            s_logbook_detail_async_result.index = request.index;
            s_logbook_detail_async_result.success = success;
            s_logbook_detail_async_result.has_points = has_points;
            s_logbook_detail_async_result.entry = entry;
            s_logbook_detail_async_result.points = points;
            s_logbook_detail_async_result.point_count = point_count;
            s_logbook_detail_async_done = true;
            s_logbook_detail_async_running = false;
        }
        else if (has_points)
        {
            logbook_backend_release_samples(points);
        }
        logbook_detail_async_unlock();
    }
}

static bool logbook_detail_async_ensure_worker(void)
{
    if (s_logbook_detail_mutex == RT_NULL)
    {
        s_logbook_detail_mutex = rt_mutex_create("ldlogM", RT_IPC_FLAG_FIFO);
        if (s_logbook_detail_mutex == RT_NULL)
        {
            return false;
        }
    }

    if (s_logbook_detail_sem == RT_NULL)
    {
        s_logbook_detail_sem = rt_sem_create("ldlogS", 0U, RT_IPC_FLAG_FIFO);
        if (s_logbook_detail_sem == RT_NULL)
        {
            return false;
        }
    }

    if (s_logbook_detail_thread != RT_NULL)
    {
        return true;
    }

    s_logbook_detail_thread = rt_thread_create("ldlog",
                                               logbook_detail_async_worker,
                                               RT_NULL,
                                               LOGBOOK_DETAIL_THREAD_STACK_SIZE,
                                               LOGBOOK_DETAIL_THREAD_PRIORITY,
                                               LOGBOOK_DETAIL_THREAD_TICK);
    if (s_logbook_detail_thread == RT_NULL)
    {
        return false;
    }

    rt_thread_startup(s_logbook_detail_thread);
    return true;
}

static bool logbook_detail_async_start(uint16_t index)
{
    if (!logbook_detail_async_ensure_worker())
    {
        return false;
    }
    if (!logbook_detail_async_lock())
    {
        return false;
    }
    if (s_logbook_detail_async_running)
    {
        logbook_detail_async_unlock();
        return false;
    }

    s_logbook_detail_async_generation++;
    if (s_logbook_detail_async_generation == 0U)
    {
        s_logbook_detail_async_generation = 1U;
    }

    s_logbook_detail_async_request.id = s_logbook_detail_async_generation;
    s_logbook_detail_async_request.index = index;
    s_logbook_detail_async_request.load_points = !s_logbook_points_loaded;
    s_logbook_detail_async_result.id = s_logbook_detail_async_generation;
    s_logbook_detail_async_result.index = index;
    s_logbook_detail_async_result.success = false;
    s_logbook_detail_async_result.has_points = false;
    s_logbook_detail_async_result.points = NULL;
    s_logbook_detail_async_result.point_count = 0U;
    s_logbook_detail_async_done = false;
    s_logbook_detail_async_running = true;
    logbook_detail_async_unlock();

    if (rt_sem_release(s_logbook_detail_sem) != RT_EOK)
    {
        if (logbook_detail_async_lock())
        {
            s_logbook_detail_async_running = false;
            logbook_detail_async_unlock();
        }
        return false;
    }
    return true;
}

static bool logbook_detail_async_fetch(logbook_detail_result_t *result)
{
    bool ready = false;

    if (result == NULL)
    {
        return false;
    }

    if (!logbook_detail_async_lock())
    {
        return false;
    }

    if (s_logbook_detail_async_done)
    {
        *result = s_logbook_detail_async_result;
        s_logbook_detail_async_done = false;
        ready = true;
    }
    logbook_detail_async_unlock();
    return ready;
}

static void logbook_detail_async_cancel(void)
{
    if (!logbook_detail_async_lock_wait())
    {
        return;
    }

    s_logbook_detail_async_generation++;
    if (s_logbook_detail_async_generation == 0U)
    {
        s_logbook_detail_async_generation = 1U;
    }
    if (s_logbook_detail_async_done && s_logbook_detail_async_result.has_points &&
        s_logbook_detail_async_result.points != NULL)
    {
        logbook_backend_release_samples(s_logbook_detail_async_result.points);
    }
    s_logbook_detail_async_done = false;
    s_logbook_detail_async_running = false;
    logbook_detail_async_unlock();
}
#endif

#ifndef PC_SIMULATOR
static bool logbook_prefetch_lock(void)
{
    return (s_logbook_prefetch_mutex != RT_NULL) &&
           (rt_mutex_take(s_logbook_prefetch_mutex, rt_tick_from_millisecond(20U)) == RT_EOK);
}

static bool logbook_prefetch_lock_wait(void)
{
    return (s_logbook_prefetch_mutex != RT_NULL) &&
           (rt_mutex_take(s_logbook_prefetch_mutex, RT_WAITING_FOREVER) == RT_EOK);
}

static void logbook_prefetch_unlock(void)
{
    if (s_logbook_prefetch_mutex != RT_NULL)
    {
        rt_mutex_release(s_logbook_prefetch_mutex);
    }
}

static void logbook_prefetch_worker(void *parameter)
{
    (void)parameter;

    while (1)
    {
        if (s_logbook_prefetch_sem == RT_NULL ||
            rt_sem_take(s_logbook_prefetch_sem, RT_WAITING_FOREVER) != RT_EOK)
        {
            continue;
        }

        logbook_prefetch_request_t request;
        const dive_pt_t *points = NULL;
        uint16_t point_count = 0U;
        bool ok;
        uint32_t profile_start_ms;

        if (!logbook_prefetch_lock_wait())
        {
            continue;
        }
        request = s_logbook_prefetch_request;
        logbook_prefetch_unlock();

        profile_start_ms = logbook_profile_begin();
        ok = logbook_backend_acquire_samples(request.index, &points, &point_count);
        logbook_profile_end("prefetch_points", profile_start_ms);
        if (!ok)
        {
            points = NULL;
            point_count = 0U;
        }

        if (!logbook_prefetch_lock_wait())
        {
            if (points != NULL)
            {
                logbook_backend_release_samples(points);
            }
            continue;
        }

        if (request.id == s_logbook_prefetch_request.id &&
            request.index == s_logbook_prefetch_request.index)
        {
            logbook_prefetch_release_ready();
            s_logbook_prefetch_index = request.index;
            s_logbook_prefetch_points = points;
            s_logbook_prefetch_point_count = point_count;
            s_logbook_prefetch_ready = ok && (points != NULL) && (point_count > 0U);
            s_logbook_prefetch_running = false;
        }
        else if (points != NULL)
        {
            logbook_backend_release_samples(points);
        }
        logbook_prefetch_unlock();
    }
}

static bool logbook_prefetch_ensure_worker(void)
{
    if (s_logbook_prefetch_mutex == RT_NULL)
    {
        s_logbook_prefetch_mutex = rt_mutex_create("lpfM", RT_IPC_FLAG_FIFO);
        if (s_logbook_prefetch_mutex == RT_NULL)
        {
            return false;
        }
    }

    if (s_logbook_prefetch_sem == RT_NULL)
    {
        s_logbook_prefetch_sem = rt_sem_create("lpfS", 0U, RT_IPC_FLAG_FIFO);
        if (s_logbook_prefetch_sem == RT_NULL)
        {
            return false;
        }
    }

    if (s_logbook_prefetch_thread != RT_NULL)
    {
        return true;
    }

    s_logbook_prefetch_thread = rt_thread_create("lpf",
                                                 logbook_prefetch_worker,
                                                 RT_NULL,
                                                 LOGBOOK_PREFETCH_THREAD_STACK_SIZE,
                                                 LOGBOOK_PREFETCH_THREAD_PRIORITY,
                                                 LOGBOOK_PREFETCH_THREAD_TICK);
    if (s_logbook_prefetch_thread == RT_NULL)
    {
        return false;
    }

    rt_thread_startup(s_logbook_prefetch_thread);
    return true;
}

static void logbook_prefetch_cancel(void)
{
    if (!logbook_prefetch_lock())
    {
        return;
    }

    s_logbook_prefetch_generation++;
    if (s_logbook_prefetch_generation == 0U)
    {
        s_logbook_prefetch_generation = 1U;
    }
    s_logbook_prefetch_running = false;
    logbook_prefetch_release_ready();
    logbook_prefetch_unlock();
}

static void logbook_prefetch_start(uint16_t index)
{
    if (index >= s_logbook_snapshot_count)
    {
        return;
    }
    if (!logbook_prefetch_ensure_worker() || !logbook_prefetch_lock())
    {
        return;
    }
    if (s_logbook_prefetch_ready && s_logbook_prefetch_index == index)
    {
        logbook_prefetch_unlock();
        return;
    }
    if (s_logbook_prefetch_running && s_logbook_prefetch_request.index == index)
    {
        logbook_prefetch_unlock();
        return;
    }

    s_logbook_prefetch_generation++;
    if (s_logbook_prefetch_generation == 0U)
    {
        s_logbook_prefetch_generation = 1U;
    }
    logbook_prefetch_release_ready();
    s_logbook_prefetch_request.id = s_logbook_prefetch_generation;
    s_logbook_prefetch_request.index = index;
    s_logbook_prefetch_running = true;
    logbook_prefetch_unlock();

    if (rt_sem_release(s_logbook_prefetch_sem) != RT_EOK && logbook_prefetch_lock())
    {
        s_logbook_prefetch_running = false;
        logbook_prefetch_unlock();
    }
}

static bool logbook_prefetch_take(uint16_t index)
{
    bool taken = false;

    if (!logbook_prefetch_lock())
    {
        return false;
    }

    if (s_logbook_prefetch_ready &&
        s_logbook_prefetch_index == index &&
        s_logbook_prefetch_points != NULL)
    {
        s_logbook_points = s_logbook_prefetch_points;
        s_logbook_point_count = s_logbook_prefetch_point_count;
        s_logbook_points_loaded = true;
        s_logbook_prefetch_points = NULL;
        s_logbook_prefetch_point_count = 0U;
        s_logbook_prefetch_index = 0xFFFFU;
        s_logbook_prefetch_ready = false;
        taken = true;
    }
    logbook_prefetch_unlock();
    return taken;
}

static bool logbook_prefetch_take_wait(uint16_t index, uint32_t timeout_ms)
{
    const uint32_t start_ms = rt_tick_get_millisecond();

    do
    {
        bool running = false;

        if (logbook_prefetch_take(index))
        {
            return true;
        }

        if (!logbook_prefetch_lock())
        {
            return false;
        }
        running = s_logbook_prefetch_running;
        logbook_prefetch_unlock();

        if (!running)
        {
            return false;
        }

        rt_thread_mdelay(10);
    } while ((uint32_t)(rt_tick_get_millisecond() - start_ms) < timeout_ms);

    return logbook_prefetch_take(index);
}
#else
static void logbook_prefetch_cancel(void)
{
    logbook_prefetch_release_ready();
}

static void logbook_prefetch_start(uint16_t index)
{
    (void)index;
}

static bool logbook_prefetch_take(uint16_t index)
{
    (void)index;
    return false;
}

static bool logbook_prefetch_take_wait(uint16_t index, uint32_t timeout_ms)
{
    (void)index;
    (void)timeout_ms;
    return false;
}
#endif

static void logbook_picker_cache_invalidate(void)
{
    if (s_logbook_snapshot_entries != NULL)
    {
#ifdef PC_SIMULATOR
        free(s_logbook_snapshot_entries);
#else
        rt_free(s_logbook_snapshot_entries);
#endif
        s_logbook_snapshot_entries = NULL;
    }
    s_logbook_snapshot_count = 0U;
    s_logbook_page_count = 0U;
    s_logbook_picker_visible = 0U;
    s_logbook_picker_focus = 0U;
    s_logbook_index = 0U;
    s_logbook_focus = 0U;
    logbook_prefetch_cancel();
}

static bool logbook_picker_cache_reserve(uint16_t count)
{
    if (count == 0U)
    {
        return true;
    }
    if (s_logbook_snapshot_entries != NULL && s_logbook_snapshot_count == count)
    {
        return true;
    }

    if (s_logbook_snapshot_entries != NULL)
    {
#ifdef PC_SIMULATOR
        free(s_logbook_snapshot_entries);
#else
        rt_free(s_logbook_snapshot_entries);
#endif
        s_logbook_snapshot_entries = NULL;
    }

#ifdef PC_SIMULATOR
    s_logbook_snapshot_entries = (logbook_picker_item_t *)calloc((size_t)count, sizeof(logbook_picker_item_t));
#else
    s_logbook_snapshot_entries = (logbook_picker_item_t *)rt_malloc((size_t)count * sizeof(logbook_picker_item_t));
    if (s_logbook_snapshot_entries != NULL)
    {
        memset(s_logbook_snapshot_entries, 0, (size_t)count * sizeof(logbook_picker_item_t));
    }
#endif
    if (s_logbook_snapshot_entries == NULL)
    {
        s_logbook_snapshot_count = 0U;
        s_logbook_page_count = 0U;
        return false;
    }

    s_logbook_snapshot_count = count;
    s_logbook_page_count = (uint16_t)((count + LOGBOOK_PICKER_VISIBLE_ROWS - 1U) / LOGBOOK_PICKER_VISIBLE_ROWS);
    return true;
}

static void logbook_picker_item_from_entry(logbook_picker_item_t *dst, const logbook_entry_t *src)
{
    if ((dst == NULL) || (src == NULL))
    {
        return;
    }

    dst->valid = src->valid;
    dst->meta = src->meta;
    dst->recovered = src->recovered;
    dst->abnormal_end = src->abnormal_end;
}

static void logbook_entry_from_picker_item(logbook_entry_t *dst, const logbook_picker_item_t *src)
{
    if ((dst == NULL) || (src == NULL))
    {
        return;
    }

    (void)memset(dst, 0, sizeof(*dst));
    dst->valid = src->valid;
    dst->meta = src->meta;
    dst->recovered = src->recovered;
    dst->abnormal_end = src->abnormal_end;
}

static void logbook_entry_make_pending(logbook_entry_t *dst, uint16_t index)
{
    if (dst == NULL)
    {
        return;
    }

    (void)memset(dst, 0, sizeof(*dst));
    dst->valid = true;
    dst->meta.log_no = (uint16_t)(index + 1U);
    (void)snprintf(dst->mode, sizeof(dst->mode), "--");
    (void)snprintf(dst->deco_model, sizeof(dst->deco_model), "--");
}

static void logbook_picker_bind_current_page(void)
{
    if ((s_logbook_page_count == 0U) || (s_logbook_snapshot_count == 0U))
    {
        s_logbook_picker_visible = 0U;
        s_logbook_picker_focus = 0U;
        return;
    }

    if (s_logbook_page_index >= s_logbook_page_count)
    {
        s_logbook_page_index = (uint16_t)(s_logbook_page_count - 1U);
    }

    const uint16_t start = (uint16_t)(s_logbook_page_index * LOGBOOK_PICKER_VISIBLE_ROWS);
    if (start >= s_logbook_snapshot_count)
    {
        s_logbook_picker_visible = 0U;
        s_logbook_picker_focus = 0U;
        return;
    }

    uint16_t remain = (uint16_t)(s_logbook_snapshot_count - start);
    s_logbook_picker_visible = (remain > LOGBOOK_PICKER_VISIBLE_ROWS) ? LOGBOOK_PICKER_VISIBLE_ROWS : (uint8_t)remain;
    if ((s_logbook_picker_visible > 0U) &&
        (s_logbook_picker_focus >= logbook_picker_focus_count()))
    {
        s_logbook_picker_focus = (uint8_t)(logbook_picker_focus_count() - 1U);
    }
}

static uint8_t logbook_picker_focus_to_index(uint8_t focus, uint8_t visible_count)
{
    if (focus < visible_count)
    {
        return focus;
    }
    return 0U;
}

static bool logbook_picker_has_back(void)
{
    return s_logbook_page_index > 0U;
}

static bool logbook_picker_has_next(void)
{
    return (s_logbook_page_index + 1U) < s_logbook_page_count;
}

static uint8_t logbook_picker_focus_back(void)
{
    if (!logbook_picker_has_back())
    {
        return 0xFFU;
    }
    return s_logbook_picker_visible;
}

static uint8_t logbook_picker_focus_next(void)
{
    if (!logbook_picker_has_next())
    {
        return 0xFFU;
    }
    return (uint8_t)(s_logbook_picker_visible + (logbook_picker_has_back() ? 1U : 0U));
}

static uint8_t logbook_picker_focus_count(void)
{
    uint8_t count = s_logbook_picker_visible;
    if (count == 0U)
    {
        return 0U;
    }
    if (logbook_picker_has_back())
    {
        count++;
    }
    if (logbook_picker_has_next())
    {
        count++;
    }
    return count;
}

static void logbook_picker_focus_default(void)
{
    if (s_logbook_picker_visible == 0U)
    {
        s_logbook_picker_focus = 0U;
        return;
    }
    s_logbook_picker_focus = 0U;
}

static void logbook_picker_stop_loader(void)
{
    if (s_logbook_load_timer != NULL)
    {
        lv_timer_del(s_logbook_load_timer);
        s_logbook_load_timer = NULL;
    }
    s_logbook_load_page = 0U;
    s_logbook_load_offset = 0U;
    s_logbook_load_prefetch = false;
    s_logbook_load_page_dirty = false;
}

static void logbook_detail_stop_loader(void)
{
    if (s_logbook_detail_timer != NULL)
    {
        lv_timer_del(s_logbook_detail_timer);
        s_logbook_detail_timer = NULL;
    }
    s_logbook_detail_loading = false;
#ifndef PC_SIMULATOR
    logbook_detail_async_cancel();
#endif
}

static void logbook_detail_start_loader(uint16_t index)
{
    logbook_detail_stop_loader();
    s_logbook_detail_index = index;
    s_logbook_detail_loading = true;
#ifndef PC_SIMULATOR
    if (!logbook_detail_async_start(index))
    {
        s_logbook_detail_loading = false;
        return;
    }
#endif
    s_logbook_detail_timer = lv_timer_create(logbook_detail_load_timer_cb, 10U, NULL);
}

static void logbook_picker_prepare_page(uint16_t page_index)
{
    if (page_index >= s_logbook_page_count)
    {
        return;
    }

    logbook_picker_stop_loader();
    logbook_picker_load_page_sync(page_index);
    logbook_picker_bind_current_page();

    if ((page_index + 1U) < s_logbook_page_count)
    {
        logbook_picker_start_loader((uint16_t)(page_index + 1U), true);
    }
}

static bool logbook_picker_page_has_missing(uint16_t page_index)
{
    if ((s_logbook_snapshot_entries == NULL) || (page_index >= s_logbook_page_count))
    {
        return false;
    }

    const uint16_t start = (uint16_t)(page_index * LOGBOOK_PICKER_VISIBLE_ROWS);
    uint16_t page_remain = (uint16_t)(s_logbook_snapshot_count - start);
    if (page_remain > LOGBOOK_PICKER_VISIBLE_ROWS)
    {
        page_remain = LOGBOOK_PICKER_VISIBLE_ROWS;
    }

    for (uint16_t i = 0U; i < page_remain; ++i)
    {
        if (!s_logbook_snapshot_entries[start + i].valid)
        {
            return true;
        }
    }
    return false;
}

static void logbook_picker_start_loader(uint16_t page_index, bool prefetch)
{
    if (page_index >= s_logbook_page_count)
    {
        logbook_picker_stop_loader();
        return;
    }

    if (prefetch && !logbook_picker_page_has_missing(page_index))
    {
        if ((page_index + 1U) < s_logbook_page_count)
        {
            page_index++;
        }
        else
        {
            logbook_picker_stop_loader();
            return;
        }
    }

    s_logbook_load_page = page_index;
    s_logbook_load_offset = 0U;
    s_logbook_load_prefetch = prefetch;
    s_logbook_load_page_dirty = false;
    if (s_logbook_load_timer == NULL)
    {
        s_logbook_load_timer = lv_timer_create(logbook_picker_load_timer_cb, 1U, NULL);
    }
    else
    {
        lv_timer_ready(s_logbook_load_timer);
    }
}

static void logbook_picker_release_objects(void)
{
    if (s_logbook_picker_view_ready)
    {
        logbook_picker_view_reset();
    }
}

static void logbook_picker_load_page_sync(uint16_t page_index)
{
    if ((s_logbook_snapshot_entries == NULL) || (page_index >= s_logbook_page_count))
    {
        return;
    }

    const uint16_t start = (uint16_t)(page_index * LOGBOOK_PICKER_VISIBLE_ROWS);
    if (start >= s_logbook_snapshot_count)
    {
        return;
    }

    uint16_t page_remain = (uint16_t)(s_logbook_snapshot_count - start);
    if (page_remain > LOGBOOK_PICKER_VISIBLE_ROWS)
    {
        page_remain = LOGBOOK_PICKER_VISIBLE_ROWS;
    }

    /* 当前可见页必须一次性补齐后再绘制，避免进入 DiveLog 时 5 行列表逐条露出。
     * 后续页仍走 timer 预取，首屏只同步读取最多 5 条 summary，卡顿面可控。 */
    for (uint16_t i = 0U; i < page_remain; ++i)
    {
        const uint16_t index = (uint16_t)(start + i);
        if (s_logbook_snapshot_entries[index].valid)
        {
            continue;
        }

        logbook_entry_t entry;
        if (logbook_backend_get_summary(index, &entry))
        {
            logbook_picker_item_from_entry(&s_logbook_snapshot_entries[index], &entry);
        }
    }
}

static void logbook_picker_sync_focus(void)
{
    if (s_logbook_picker_visible == 0U)
    {
        s_logbook_picker_focus = 0U;
        s_logbook_focus = 0U;
        return;
    }

    if (s_logbook_picker_focus >= logbook_picker_focus_count())
    {
        s_logbook_picker_focus = (uint8_t)(logbook_picker_focus_count() - 1U);
    }
    if (s_logbook_picker_focus < s_logbook_picker_visible)
    {
        s_logbook_focus = (uint16_t)(s_logbook_page_index * LOGBOOK_PICKER_VISIBLE_ROWS +
                                     logbook_picker_focus_to_index(s_logbook_picker_focus, s_logbook_picker_visible));
    }
}

static void logbook_picker_view_reset(void)
{
    for (uint8_t i = 0U; i < LOGBOOK_PICKER_VISIBLE_ROWS; ++i)
    {
        if (s_logbook_picker_rows[i] != NULL)
        {
            lv_obj_del(s_logbook_picker_rows[i]);
            s_logbook_picker_rows[i] = NULL;
        }
    }
    if (s_logbook_picker_title != NULL)
    {
        lv_obj_del(s_logbook_picker_title);
    }
    if (s_logbook_picker_count != NULL)
    {
        lv_obj_del(s_logbook_picker_count);
    }
    if (s_logbook_picker_page != NULL)
    {
        lv_obj_del(s_logbook_picker_page);
    }
    if (s_logbook_picker_back != NULL)
    {
        lv_obj_del(s_logbook_picker_back);
    }
    if (s_logbook_picker_next != NULL)
    {
        lv_obj_del(s_logbook_picker_next);
    }
    s_logbook_picker_view_ready = false;
    s_logbook_picker_title = NULL;
    s_logbook_picker_count = NULL;
    s_logbook_picker_page = NULL;
    s_logbook_picker_back = NULL;
    s_logbook_picker_back_lbl = NULL;
    s_logbook_picker_next = NULL;
    s_logbook_picker_next_lbl = NULL;
    for (uint8_t i = 0U; i < LOGBOOK_PICKER_VISIBLE_ROWS; ++i)
    {
        s_logbook_picker_rows[i] = NULL;
        s_logbook_picker_left[i] = NULL;
        s_logbook_picker_right[i] = NULL;
    }
}

static void logbook_picker_load_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if ((s_logbook_snapshot_entries == NULL) || !menu_runtime_is_logbook() || (s_logbook_page != LOGBOOK_PAGE_PICK))
    {
        logbook_picker_stop_loader();
        return;
    }

    const uint16_t start = (uint16_t)(s_logbook_load_page * LOGBOOK_PICKER_VISIBLE_ROWS);
    if (start >= s_logbook_snapshot_count)
    {
        logbook_picker_stop_loader();
        return;
    }

    uint16_t page_remain = (uint16_t)(s_logbook_snapshot_count - start);
    if (page_remain > LOGBOOK_PICKER_VISIBLE_ROWS)
    {
        page_remain = LOGBOOK_PICKER_VISIBLE_ROWS;
    }

    if (s_logbook_load_offset >= page_remain)
    {
        if (s_logbook_load_page_dirty && !s_logbook_load_prefetch &&
            (s_logbook_load_page == s_logbook_page_index))
        {
            s_logbook_load_page_dirty = false;
            submenu_populate_current();
        }

        if (!s_logbook_load_prefetch)
        {
            if ((s_logbook_load_page + 1U) < s_logbook_page_count)
            {
                logbook_picker_start_loader((uint16_t)(s_logbook_load_page + 1U), true);
                return;
            }
            logbook_picker_stop_loader();
            return;
        }
        logbook_picker_stop_loader();
        return;
    }

    const uint16_t index = (uint16_t)(start + s_logbook_load_offset);
    if (!s_logbook_snapshot_entries[index].valid)
    {
        logbook_entry_t entry;
        if (logbook_backend_get_summary(index, &entry))
        {
            logbook_picker_item_from_entry(&s_logbook_snapshot_entries[index], &entry);
            if (!s_logbook_load_prefetch && (s_logbook_load_page == s_logbook_page_index))
            {
                s_logbook_load_page_dirty = true;
                submenu_populate_current();
                s_logbook_load_page_dirty = false;
            }
        }
    }
    s_logbook_load_offset++;
}

static void logbook_detail_load_timer_cb(lv_timer_t *timer)
{
    (void)timer;
#ifndef PC_SIMULATOR
    logbook_detail_result_t result;
#endif

    if (!s_logbook_detail_loading ||
        !menu_runtime_is_logbook() ||
        s_logbook_page == LOGBOOK_PAGE_PICK)
    {
        logbook_detail_stop_loader();
        return;
    }

#ifndef PC_SIMULATOR
    if (!logbook_detail_async_fetch(&result))
    {
        return;
    }

    if (result.index != s_logbook_detail_index)
    {
        if (result.has_points && result.points != NULL)
        {
            logbook_backend_release_samples(result.points);
        }
        logbook_detail_stop_loader();
        return;
    }

    s_logbook_valid = result.success && result.entry.valid;
    if (s_logbook_valid)
    {
        s_logbook_entry = result.entry;
        if (result.has_points && result.points != NULL)
        {
            logbook_points_release();
            s_logbook_points = result.points;
            s_logbook_point_count = result.point_count;
            s_logbook_points_loaded = true;
            result.points = NULL;
        }
    }
    if (result.has_points && result.points != NULL)
    {
        logbook_backend_release_samples(result.points);
    }
    logbook_detail_stop_loader();
    if (menu_runtime_is_logbook() &&
        s_logbook_page != LOGBOOK_PAGE_PICK &&
        s_logbook_index == s_logbook_detail_index)
    {
        submenu_populate_current();
    }
#else
    s_logbook_valid = logbook_backend_get_detail(s_logbook_detail_index, &s_logbook_entry);
    if (s_logbook_valid && !s_logbook_points_loaded)
    {
        (void)logbook_points_load(s_logbook_detail_index);
    }
    logbook_detail_stop_loader();
    if (menu_runtime_is_logbook() &&
        s_logbook_page != LOGBOOK_PAGE_PICK &&
        s_logbook_index == s_logbook_detail_index)
    {
        submenu_populate_current();
    }
#endif
}

static bool logbook_picker_snapshot_refresh(void)
{
    uint16_t count = logbook_backend_count();

    logbook_picker_cache_invalidate();
    if (count == 0U)
    {
        return false;
    }

    if (!logbook_picker_cache_reserve(count))
    {
        return false;
    }
    s_logbook_picker_visible = (count > LOGBOOK_PICKER_VISIBLE_ROWS) ? LOGBOOK_PICKER_VISIBLE_ROWS : (uint8_t)count;
    logbook_picker_prepare_page(s_logbook_page_index);
    logbook_picker_focus_default();
    return (s_logbook_snapshot_count > 0U);
}

static bool logbook_points_load(uint16_t index)
{
    uint32_t profile_start_ms = logbook_profile_begin();

    logbook_points_release();
    s_logbook_points_loaded = logbook_backend_acquire_samples(index, &s_logbook_points, &s_logbook_point_count);
    logbook_profile_end("points_load", profile_start_ms);
    return s_logbook_points_loaded;
}

static void submenu_dive_plan_render_cache_reset(void)
{
    /* 潜水计划页会被频繁刷新，必须把上一次已渲染内容的缓存一起清掉。 */
    (void)memset(&s_dive_plan_last_vm, 0, sizeof(s_dive_plan_last_vm));
    s_dive_plan_last_vm_valid = false;
}

static bool submenu_is_dive_plan_visible(void)
{
    /* 潜水计划页虽然复用子菜单层，但绘制方式和普通列表完全不同。 */
    return menu_runtime_is_dive_plan();
}

static bool submenu_is_logbook_visible(void)
{
    return menu_runtime_is_logbook();
}

static void logbook_load_current(void)
{
    uint32_t profile_start_ms = logbook_profile_begin();

    s_logbook_valid = false;
    logbook_points_release();
    logbook_prefetch_cancel();
    s_logbook_valid = logbook_picker_snapshot_refresh();
    if (s_logbook_valid)
    {
        uint8_t offset = logbook_picker_focus_to_index(s_logbook_picker_focus, s_logbook_picker_visible);
        s_logbook_index = (uint16_t)(s_logbook_page_index * LOGBOOK_PICKER_VISIBLE_ROWS + offset);
        if (s_logbook_index < s_logbook_snapshot_count)
        {
            logbook_entry_from_picker_item(&s_logbook_entry, &s_logbook_snapshot_entries[s_logbook_index]);
        }
    }
    logbook_profile_end("load_current_summary", profile_start_ms);
}

static void logbook_reset_state(void)
{
    s_logbook_page = LOGBOOK_PAGE_PICK;
    s_logbook_page_index = 0U;
    s_logbook_picker_focus = 0U;
    s_logbook_focus = 0U;
    s_logbook_index = 0U;
    logbook_picker_stop_loader();
    logbook_detail_stop_loader();
    logbook_prefetch_cancel();
    logbook_picker_cache_invalidate();
    logbook_load_current();
}

static void logbook_format_duration(char *buf, size_t buf_size, uint32_t total_s)
{
    uint32_t h = total_s / 3600U;
    uint32_t m = (total_s % 3600U) / 60U;

    if (h > 0U) (void)snprintf(buf, buf_size, "%uh %02um", (unsigned)h, (unsigned)m);
    else (void)snprintf(buf, buf_size, "%umin", (unsigned)m);
}

static void logbook_format_date(char *buf, size_t buf_size, const logbook_meta_t *meta)
{
    static const char *months[] = {"---", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    uint8_t month = (meta->month <= 12U) ? meta->month : 0U;
    (void)snprintf(buf, buf_size, "%02u-%s-%04u", (unsigned)meta->day, months[month], (unsigned)meta->year);
}

static void logbook_format_id(char *buf, size_t buf_size, const logbook_entry_t *entry)
{
    if (buf == NULL || buf_size == 0U || entry == NULL)
    {
        return;
    }

    (void)snprintf(buf,
                   buf_size,
                   "%s#%u",
                   (entry->recovered || entry->abnormal_end) ? "REC" : "DIVE",
                   (unsigned)entry->meta.log_no);
}

static void logbook_format_picker_id(char *buf, size_t buf_size, const logbook_picker_item_t *entry)
{
    if (buf == NULL || buf_size == 0U || entry == NULL)
    {
        return;
    }

    (void)snprintf(buf,
                   buf_size,
                   "%s#%u",
                   (entry->recovered || entry->abnormal_end) ? "REC" : "DIVE",
                   (unsigned)entry->meta.log_no);
}

static void logbook_format_title(char *buf, size_t buf_size, const logbook_entry_t *entry, const char *date)
{
    size_t used;
    logbook_format_id(buf, buf_size, entry);
    used = strlen(buf);
    if (used < buf_size)
    {
        (void)snprintf(buf + used, buf_size - used, "     %s", date ? date : "");
    }
}

static uint16_t logbook_panel_width(uint16_t w)
{
    if (w > 430U)
    {
        return 390U;
    }
    return (w > 36U) ? (uint16_t)(w - 36U) : w;
}

static bool logbook_compact_layout(void)
{
    return !ui_layout_is_vertical_split() && s_submenu_height <= 320U;
}

static lv_obj_t *logbook_label(lv_obj_t *parent, const char *text, font_id_t font_id, lv_color_t color,
                               int16_t x, int16_t y, uint16_t w, uint16_t h, lv_text_align_t align)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl, get_font(font_id), 0);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, 0);
    lv_obj_set_pos(lbl, x, y);
    lv_obj_set_size(lbl, w, h);
    lv_obj_set_style_text_align(lbl, align, 0);
    lv_label_set_text(lbl, text ? text : "");
    return lbl;
}

static void logbook_apply_row_style(lv_obj_t *row, lv_obj_t *left_lbl, lv_obj_t *right_lbl,
                                    bool focused, uint16_t h)
{
    font_id_t row_font = focused ? FONT_ID_TITLE : FONT_ID_SMALL;
    (void)h;

    if (row != NULL)
    {
        lv_obj_set_style_bg_color(row, BLACK, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, focused ? GREEN : DARK, 0);
        lv_obj_set_style_border_width(row, focused ? 2 : 1, 0);
    }
    if (left_lbl != NULL)
    {
        lv_obj_set_style_text_font(left_lbl, get_font(row_font), 0);
        lv_obj_set_style_text_color(left_lbl, focused ? LIGHT : GREEN, 0);
    }
    if (right_lbl != NULL)
    {
        lv_obj_set_style_text_font(right_lbl, get_font(row_font), 0);
        lv_obj_set_style_text_color(right_lbl, focused ? LIGHT : GREEN, 0);
    }
}

static void logbook_draw_row(lv_obj_t *parent, const char *left, const char *right, uint8_t focus_id,
                             int16_t x, int16_t y, uint16_t w, uint16_t h)
{
    bool focused = (s_logbook_page == LOGBOOK_PAGE_PICK) ?
                   (s_logbook_picker_focus == focus_id) :
                   (s_logbook_focus == focus_id);
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_pos(row, x, y);
    lv_obj_set_size(row, w, h);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *left_lbl = logbook_label(row, left, FONT_ID_TITLE, GREEN, 12, 4, (uint16_t)(w / 2U), (uint16_t)(h - 8U), LV_TEXT_ALIGN_LEFT);
    lv_obj_t *right_lbl = NULL;
    if (right != NULL)
    {
        right_lbl = logbook_label(row, right, FONT_ID_TITLE, GREEN, (int16_t)(w / 2U), 4, (uint16_t)(w / 2U - 12U), (uint16_t)(h - 8U), LV_TEXT_ALIGN_RIGHT);
    }
    logbook_apply_row_style(row, left_lbl, right_lbl, focused, h);
}

static void logbook_chart_draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);
    lv_area_t *area = &obj->coords;
    int chart_w = lv_area_get_width(area);
    int chart_h = lv_area_get_height(area);
    float max_d = (s_logbook_entry.max_depth_m > 1.0f) ? s_logbook_entry.max_depth_m : 1.0f;
    float max_t = (s_logbook_entry.dive_time_s > 1U) ? (float)s_logbook_entry.dive_time_s : 1.0f;

    lv_draw_line_dsc_t grid;
    lv_draw_line_dsc_init(&grid);
    grid.color = DARK;
    grid.width = 1;
    grid.opa = 180;

    for (uint8_t i = 0U; i <= 4U; i++)
    {
        lv_coord_t y = (lv_coord_t)(area->y1 + ((chart_h - 1) * i) / 4);
        lv_point_t p[2] = {{area->x1, y}, {area->x2, y}};
        lv_draw_line(draw_ctx, &grid, &p[0], &p[1]);
    }
    for (uint8_t i = 0U; i <= 4U; i++)
    {
        lv_coord_t x = (lv_coord_t)(area->x1 + ((chart_w - 1) * i) / 4);
        lv_point_t p[2] = {{x, area->y1}, {x, area->y2}};
        lv_draw_line(draw_ctx, &grid, &p[0], &p[1]);
    }

    (void)depth_chart_draw_profile(draw_ctx, s_logbook_points, s_logbook_point_count, area->x1, area->y1, (lv_coord_t)(chart_w - 1), (lv_coord_t)(chart_h - 1), max_t, max_d, LIGHT, 2U, LV_OPA_COVER, NULL);
}

static void logbook_draw_summary(lv_obj_t *parent, uint16_t w, uint16_t h)
{
    char date[20];
    char buf[48];
    char dive_time[16];
    bool compact = logbook_compact_layout();
    uint16_t chart_h = compact ? 120U : 160U;
    int16_t axis_y = compact ? 156 : 190;
    int16_t stats_y1 = compact ? 188 : 230;
    int16_t stats_y2 = compact ? 222 : 272;
    font_id_t stats_font = compact ? FONT_ID_TITLE : FONT_ID_MEDIUM;
    uint16_t stats_h = compact ? 30U : 42U;

    logbook_format_date(date, sizeof(date), &s_logbook_entry.meta);
    logbook_format_duration(dive_time, sizeof(dive_time), s_logbook_entry.dive_time_s);
    logbook_format_title(buf, sizeof(buf), &s_logbook_entry, date);
    logbook_label(parent, buf, FONT_ID_TITLE, LIGHT, 8, 10, (uint16_t)(w - 16U), 30, LV_TEXT_ALIGN_LEFT);

    (void)snprintf(buf, sizeof(buf), "0%s", bus_get_depth_unit_label());
    logbook_label(parent, buf, FONT_ID_TITLE, LIGHT, 8, 48, 42, 24, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *chart = lv_obj_create(parent);
    lv_obj_remove_style_all(chart);
    lv_obj_set_pos(chart, 50, 48);
    lv_obj_set_size(chart, (uint16_t)(w - 70U), chart_h);
    lv_obj_add_event_cb(chart, logbook_chart_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    (void)snprintf(buf, sizeof(buf), "%.0f%s", (double)bus_get_depth_display(s_logbook_entry.max_depth_m), bus_get_depth_unit_label());
    logbook_label(parent, buf, FONT_ID_TITLE, LIGHT, 8, axis_y, 56, 24, LV_TEXT_ALIGN_LEFT);
    logbook_label(parent, dive_time, FONT_ID_TITLE, LIGHT, (int16_t)(w - 110), axis_y, 100, 24, LV_TEXT_ALIGN_RIGHT);

    (void)snprintf(buf, sizeof(buf), "MAX %.1f%s", (double)bus_get_depth_display(s_logbook_entry.max_depth_m), bus_get_depth_unit_label());
    logbook_label(parent, buf, stats_font, LIGHT, 20, stats_y1, 180, stats_h, LV_TEXT_ALIGN_LEFT);
    (void)snprintf(buf, sizeof(buf), "AVG %.1f%s", (double)bus_get_depth_display(s_logbook_entry.avg_depth_m), bus_get_depth_unit_label());
    logbook_label(parent, buf, stats_font, LIGHT, 20, stats_y2, 180, stats_h, LV_TEXT_ALIGN_LEFT);
    (void)snprintf(buf, sizeof(buf), "START %02u:%02u", (unsigned)s_logbook_entry.meta.start_h, (unsigned)s_logbook_entry.meta.start_m);
    logbook_label(parent, buf, stats_font, LIGHT, 230, stats_y1, 200, stats_h, LV_TEXT_ALIGN_LEFT);
    (void)snprintf(buf, sizeof(buf), "END %02u:%02u", (unsigned)s_logbook_entry.meta.end_h, (unsigned)s_logbook_entry.meta.end_m);
    logbook_label(parent, buf, stats_font, LIGHT, 230, stats_y2, 200, stats_h, LV_TEXT_ALIGN_LEFT);

    (void)h;
}

static void logbook_draw_picker(lv_obj_t *parent, uint16_t w, uint16_t h)
{
    bool compact = logbook_compact_layout();
    uint16_t panel_w = logbook_panel_width(w);
    int16_t title_y = compact ? 8 : 16;
    int16_t count_y = compact ? 14 : 22;
    int16_t row_start_y = compact ? 54 : 62;
    int16_t row_gap = compact ? 36 : 42;
    uint16_t row_h = compact ? 32U : 36U;
    const uint16_t button_w = compact ? 92U : 96U;
    const uint16_t button_h = 34U;
    const int16_t footer_margin = compact ? 16 : 18;
    const int16_t button_y = (int16_t)((h > (button_h + 22U)) ? (h - button_h - 10U) : (h - button_h));
    const int16_t page_y = (int16_t)(button_y - 22);
    const int16_t back_x = footer_margin;
    const int16_t next_x = (int16_t)(footer_margin + panel_w - button_w);
    const int16_t page_x = footer_margin;
    const uint16_t page_w = panel_w;
    char left[32];
    char right[24];
    char page_text[24];
    uint8_t visible_count = s_logbook_picker_visible;
    uint16_t current_page = (s_logbook_page_count == 0U) ? 0U : (uint16_t)(s_logbook_page_index + 1U);
    bool has_back = logbook_picker_has_back();
    bool has_next = logbook_picker_has_next();

    if (!s_logbook_picker_view_ready)
    {
        s_logbook_picker_title = logbook_label(parent, "DIVE LOG", FONT_ID_TITLE, GREEN, 18, title_y, panel_w, 32, LV_TEXT_ALIGN_LEFT);
        s_logbook_picker_count = logbook_label(parent, "", FONT_ID_SMALL, GREEN, (int16_t)(18 + panel_w - 110), count_y, 100, 24, LV_TEXT_ALIGN_RIGHT);
        for (uint8_t i = 0U; i < LOGBOOK_PICKER_VISIBLE_ROWS; ++i)
        {
            int16_t row_y = (int16_t)(row_start_y + i * row_gap);
            s_logbook_picker_rows[i] = lv_obj_create(parent);
            lv_obj_remove_style_all(s_logbook_picker_rows[i]);
            lv_obj_set_pos(s_logbook_picker_rows[i], 18, row_y);
            lv_obj_set_size(s_logbook_picker_rows[i], panel_w, row_h);
            lv_obj_clear_flag(s_logbook_picker_rows[i], LV_OBJ_FLAG_SCROLLABLE);
            s_logbook_picker_left[i] = logbook_label(s_logbook_picker_rows[i], "", FONT_ID_TITLE, GREEN, 12, 4, (uint16_t)((panel_w * 7U) / 20U), (uint16_t)(row_h - 8U), LV_TEXT_ALIGN_LEFT);
            s_logbook_picker_right[i] = logbook_label(s_logbook_picker_rows[i], "", FONT_ID_TITLE, GREEN, (int16_t)((panel_w * 7U) / 20U), 4, (uint16_t)((panel_w * 13U) / 20U - 12U), (uint16_t)(row_h - 8U), LV_TEXT_ALIGN_RIGHT);
        }
        s_logbook_picker_back = lv_obj_create(parent);
        lv_obj_remove_style_all(s_logbook_picker_back);
        lv_obj_set_pos(s_logbook_picker_back, back_x, button_y);
        lv_obj_set_size(s_logbook_picker_back, button_w, button_h);
        lv_obj_clear_flag(s_logbook_picker_back, LV_OBJ_FLAG_SCROLLABLE);
        s_logbook_picker_back_lbl = logbook_label(s_logbook_picker_back, "BACK", FONT_ID_TITLE, GREEN, 0, 0, button_w, button_h, LV_TEXT_ALIGN_CENTER);
        s_logbook_picker_next = lv_obj_create(parent);
        lv_obj_remove_style_all(s_logbook_picker_next);
        lv_obj_set_pos(s_logbook_picker_next, next_x, button_y);
        lv_obj_set_size(s_logbook_picker_next, button_w, button_h);
        lv_obj_clear_flag(s_logbook_picker_next, LV_OBJ_FLAG_SCROLLABLE);
        s_logbook_picker_next_lbl = logbook_label(s_logbook_picker_next, "NEXT", FONT_ID_TITLE, GREEN, 0, 0, button_w, button_h, LV_TEXT_ALIGN_CENTER);
        s_logbook_picker_page = logbook_label(parent, "", FONT_ID_SMALL, LIGHT, page_x, page_y, page_w, 20U, LV_TEXT_ALIGN_CENTER);
        s_logbook_picker_view_ready = true;
    }
    else
    {
        lv_obj_set_pos(s_logbook_picker_title, 18, title_y);
        lv_obj_set_pos(s_logbook_picker_count, (int16_t)(18 + panel_w - 110), count_y);
        for (uint8_t i = 0U; i < LOGBOOK_PICKER_VISIBLE_ROWS; ++i)
        {
            int16_t row_y = (int16_t)(row_start_y + i * row_gap);
            if (s_logbook_picker_rows[i] != NULL)
            {
                lv_obj_set_pos(s_logbook_picker_rows[i], 18, row_y);
                lv_obj_set_size(s_logbook_picker_rows[i], panel_w, row_h);
            }
            if (s_logbook_picker_left[i] != NULL)
            {
                lv_obj_set_pos(s_logbook_picker_left[i], 12, 4);
                lv_obj_set_size(s_logbook_picker_left[i], (uint16_t)((panel_w * 7U) / 20U), (uint16_t)(row_h - 8U));
            }
            if (s_logbook_picker_right[i] != NULL)
            {
                lv_obj_set_pos(s_logbook_picker_right[i], (int16_t)((panel_w * 7U) / 20U), 4);
                lv_obj_set_size(s_logbook_picker_right[i], (uint16_t)((panel_w * 13U) / 20U - 12U), (uint16_t)(row_h - 8U));
            }
        }
        if (s_logbook_picker_back != NULL)
        {
            lv_obj_set_pos(s_logbook_picker_back, back_x, button_y);
            lv_obj_set_size(s_logbook_picker_back, button_w, button_h);
        }
        if (s_logbook_picker_next != NULL)
        {
            lv_obj_set_pos(s_logbook_picker_next, next_x, button_y);
            lv_obj_set_size(s_logbook_picker_next, button_w, button_h);
        }
        if (s_logbook_picker_page != NULL)
        {
            lv_obj_set_pos(s_logbook_picker_page, page_x, page_y);
            lv_obj_set_size(s_logbook_picker_page, page_w, 20U);
        }
    }

    if (s_logbook_picker_title) lv_label_set_text(s_logbook_picker_title, "DIVE LOG");
    if (s_logbook_picker_count)
    {
        (void)snprintf(right, sizeof(right), "%u LOGS", (unsigned)s_logbook_snapshot_count);
        lv_label_set_text(s_logbook_picker_count, right);
    }
    if (s_logbook_page_count > 0U)
    {
        (void)snprintf(page_text, sizeof(page_text), "Page %u/%u", (unsigned)current_page, (unsigned)s_logbook_page_count);
    }
    else
    {
        (void)snprintf(page_text, sizeof(page_text), "Page 0/0");
    }
    if (s_logbook_picker_page && page_w > 0U)
    {
        lv_obj_set_style_text_align(s_logbook_picker_page, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(s_logbook_picker_page, page_text);
    }

    for (uint8_t row = 0U; row < LOGBOOK_PICKER_VISIBLE_ROWS; row++)
    {
        char date[20];
        if (row >= visible_count)
        {
            if (s_logbook_picker_rows[row]) lv_obj_add_flag(s_logbook_picker_rows[row], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        if (s_logbook_picker_rows[row]) lv_obj_clear_flag(s_logbook_picker_rows[row], LV_OBJ_FLAG_HIDDEN);
        uint16_t page_offset = row;
        uint16_t entry_index = (uint16_t)(s_logbook_page_index * LOGBOOK_PICKER_VISIBLE_ROWS + page_offset);
        if ((entry_index >= s_logbook_snapshot_count) || !s_logbook_snapshot_entries[entry_index].valid)
        {
            if (s_logbook_picker_left[row]) lv_label_set_text(s_logbook_picker_left[row], "...");
            if (s_logbook_picker_right[row]) lv_label_set_text(s_logbook_picker_right[row], "");
            logbook_apply_row_style(s_logbook_picker_rows[row], s_logbook_picker_left[row], s_logbook_picker_right[row], (s_logbook_picker_focus == row), row_h);
            continue;
        }
        logbook_format_date(date, sizeof(date), &s_logbook_snapshot_entries[entry_index].meta);
        logbook_format_picker_id(left, sizeof(left), &s_logbook_snapshot_entries[entry_index]);
        (void)snprintf(right, sizeof(right), "%s", date);
        if (s_logbook_picker_left[row]) lv_label_set_text(s_logbook_picker_left[row], left);
        if (s_logbook_picker_right[row]) lv_label_set_text(s_logbook_picker_right[row], right);
        logbook_apply_row_style(s_logbook_picker_rows[row], s_logbook_picker_left[row], s_logbook_picker_right[row], (s_logbook_picker_focus == row), row_h);
    }
    for (uint8_t row = visible_count; row < LOGBOOK_PICKER_VISIBLE_ROWS; ++row)
    {
        if (s_logbook_picker_rows[row]) lv_obj_add_flag(s_logbook_picker_rows[row], LV_OBJ_FLAG_HIDDEN);
        if (s_logbook_picker_left[row]) lv_label_set_text(s_logbook_picker_left[row], "");
        if (s_logbook_picker_right[row]) lv_label_set_text(s_logbook_picker_right[row], "");
    }
    if (s_logbook_picker_back)
    {
        if (has_back) lv_obj_clear_flag(s_logbook_picker_back, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_logbook_picker_back, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_logbook_picker_next)
    {
        if (has_next) lv_obj_clear_flag(s_logbook_picker_next, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_logbook_picker_next, LV_OBJ_FLAG_HIDDEN);
    }
    if (has_back) logbook_apply_row_style(s_logbook_picker_back, s_logbook_picker_back_lbl, NULL, (s_logbook_picker_focus == logbook_picker_focus_back()), button_h);
    if (has_next) logbook_apply_row_style(s_logbook_picker_next, s_logbook_picker_next_lbl, NULL, (s_logbook_picker_focus == logbook_picker_focus_next()), button_h);

    (void)h;
}

static void logbook_draw_detail_1(lv_obj_t *parent, uint16_t w, uint16_t h)
{
    char date[20];
    char buf[56];
    char surf[16];
    bool compact = logbook_compact_layout();
    int16_t row_start_y = compact ? 50 : 62;
    int16_t row_gap = compact ? 35 : 40;
    uint16_t row_h = compact ? 32U : 34U;

    logbook_format_date(date, sizeof(date), &s_logbook_entry.meta);
    logbook_format_title(buf, sizeof(buf), &s_logbook_entry, date);
    logbook_label(parent, buf, FONT_ID_TITLE, LIGHT, 8, 10, (uint16_t)(w - 16U), 30, LV_TEXT_ALIGN_LEFT);
    logbook_format_duration(surf, sizeof(surf), s_logbook_entry.surface_interval_s);

    const char *labels[] = {"Mode", "Status", "Surface Int", "Surface mbar", "Deco Model", "End CNS"};
    const char *values[6];
    char surface_mbar[16];
    char status[16];
    char start_cns[12];
    char end_cns[12];
    (void)snprintf(surface_mbar, sizeof(surface_mbar), "%.0f", (double)s_logbook_entry.surface_mbar);
    (void)snprintf(status, sizeof(status), "%s",
                   (s_logbook_entry.recovered || s_logbook_entry.abnormal_end) ? "RECOVERED" : "COMPLETE");
    (void)snprintf(start_cns, sizeof(start_cns), "%03u%%", (unsigned)s_logbook_entry.start_cns_pct);
    (void)snprintf(end_cns, sizeof(end_cns), "%03u%%", (unsigned)s_logbook_entry.end_cns_pct);
    values[0] = s_logbook_entry.mode;
    values[1] = status;
    values[2] = surf;
    values[3] = surface_mbar;
    values[4] = s_logbook_entry.deco_model;
    values[5] = end_cns;

    for (uint8_t i = 0U; i < 6U; i++)
    {
        int16_t y = (int16_t)(row_start_y + i * row_gap);
        logbook_draw_row(parent, labels[i], values[i], 255U, 18, y, logbook_panel_width(w), row_h);
    }

    (void)h;
    (void)start_cns;
}

static void logbook_draw_detail_2(lv_obj_t *parent, uint16_t w, uint16_t h)
{
    char date[20];
    char buf[56];
    bool compact = logbook_compact_layout();
    font_id_t table_font = compact ? FONT_ID_SMALL : FONT_ID_MEDIUM;
    int16_t header_y = compact ? 48 : 58;
    int16_t row_start_y = compact ? 86 : 104;
    int16_t row_gap = compact ? 34 : 42;
    uint16_t row_h = compact ? 28U : 36U;
    int16_t avg_y = compact ? 230 : 290;
    uint16_t avg_h = compact ? 28U : 40U;

    logbook_format_date(date, sizeof(date), &s_logbook_entry.meta);
    logbook_format_title(buf, sizeof(buf), &s_logbook_entry, date);
    logbook_label(parent, buf, FONT_ID_TITLE, LIGHT, 8, 10, (uint16_t)(w - 16U), 30, LV_TEXT_ALIGN_LEFT);
    logbook_label(parent, "Start     End", table_font, LIGHT, 150, header_y, 250, row_h, LV_TEXT_ALIGN_LEFT);

    static const char *names[] = {"D1", "T2", "T3", "T4"};
    for (uint8_t i = 0U; i < LOGBOOK_TANK_COUNT; i++)
    {
        int16_t y = (int16_t)(row_start_y + i * row_gap);
        logbook_label(parent, names[i], table_font, LIGHT, 60, y, 70, row_h, LV_TEXT_ALIGN_LEFT);
        logbook_label(parent, s_logbook_entry.tank_start[i], table_font, LIGHT, 155, y, 100, row_h, LV_TEXT_ALIGN_LEFT);
        logbook_label(parent, s_logbook_entry.tank_end[i], table_font, LIGHT, 310, y, 100, row_h, LV_TEXT_ALIGN_LEFT);
    }

    (void)snprintf(buf, sizeof(buf), "Avg SAC D1 %.1f", (double)s_logbook_entry.avg_sac_l_min);
    logbook_label(parent, buf, table_font, LIGHT, 60, avg_y, 300, avg_h, LV_TEXT_ALIGN_LEFT);
    (void)h;
}

static void submenu_populate_logbook(void)
{
    uint16_t w = s_submenu_width;
    uint16_t h = s_submenu_height;
    uint32_t profile_start_ms = logbook_profile_begin();
    bool picker_page = (s_logbook_page == LOGBOOK_PAGE_PICK);

    if (!picker_page && s_logbook_picker_view_ready)
    {
        logbook_picker_view_reset();
    }

    if (!picker_page || !s_logbook_picker_view_ready)
    {
        lv_obj_clean(s_submenu_list);
    }

    if (!s_logbook_valid)
    {
        logbook_label(s_submenu_list, "NO LOGS", FONT_ID_MEDIUM, LIGHT, 0, logbook_compact_layout() ? 116 : 160, w, 48, LV_TEXT_ALIGN_CENTER);
        (void)h;
        ui_state_set_sub_item_count(0U);
        ui_state_set_sub_menu_idx(0U);
        logbook_profile_end("populate_empty", profile_start_ms);
        return;
    }

    switch (s_logbook_page)
    {
    case LOGBOOK_PAGE_PICK:
        logbook_draw_picker(s_submenu_list, w, h);
        ui_state_set_sub_item_count(logbook_picker_focus_count());
        ui_state_set_sub_menu_idx(s_logbook_picker_focus);
        if (s_logbook_picker_focus < s_logbook_picker_visible)
        {
            const uint16_t prefetch_index =
                (uint16_t)(s_logbook_page_index * LOGBOOK_PICKER_VISIBLE_ROWS +
                           logbook_picker_focus_to_index(s_logbook_picker_focus, s_logbook_picker_visible));
            logbook_prefetch_start(prefetch_index);
        }
        break;
    case LOGBOOK_PAGE_DETAIL_1:
        logbook_draw_detail_1(s_submenu_list, w, h);
        ui_state_set_sub_item_count(0U);
        break;
    case LOGBOOK_PAGE_DETAIL_2:
        logbook_draw_detail_2(s_submenu_list, w, h);
        ui_state_set_sub_item_count(0U);
        ui_state_set_sub_menu_idx(s_logbook_focus);
        break;
    case LOGBOOK_PAGE_SUMMARY:
    default:
        logbook_draw_summary(s_submenu_list, w, h);
        ui_state_set_sub_item_count(0U);
        ui_state_set_sub_menu_idx(s_logbook_focus);
        break;
    }
    logbook_profile_end("populate", profile_start_ms);
}

static uint16_t submenu_right_width(void)
{
    return (s_submenu_width > 0U)
           ? s_submenu_width
           : ui_content_w_get();
}

static bool submenu_light_power_on(void)
{
    ui_vm_submenu_view_t vm;

    ui_vm_submenu_view_update(&vm);
    return (vm.light_power_on != 0U);
}

static light_mode_t submenu_light_mode(void)
{
    ui_vm_submenu_view_t vm;

    ui_vm_submenu_view_update(&vm);
    return (vm.light_mode == (uint8_t)LIGHT_MODE_BREATH)
           ? LIGHT_MODE_BREATH
           : LIGHT_MODE_ALWAYS;
}

static const char *submenu_light_mode_text(void)
{
    return submenu_light_mode() == LIGHT_MODE_BREATH ? "BREATH" : "ALWAYS";
}

static lv_color_t submenu_light_status_color(const menu_row_t *row)
{
    if (row != NULL && row->type == MENU_ROW_LIGHT_POWER)
    {
        return submenu_light_power_on() ? GREEN : LIGHT;
    }
    return GREEN;
}

void submenu_view_reset(void)
{
    /* 布局重建后，旧的子菜单对象和菜单运行时都需要一起清空。 */
    logbook_points_release();
    logbook_picker_stop_loader();
    logbook_detail_stop_loader();
    logbook_picker_cache_invalidate();
    s_submenu_layer = NULL;
    s_submenu_title = NULL;
    s_submenu_title_line = NULL;
    s_submenu_list = NULL;
    s_light_status_lbl = NULL;
    s_submenu_width = 0;
    s_submenu_height = 0;
    submenu_dive_plan_render_cache_reset();
    menu_runtime_reset();
    menu_actions_clear_pending();
}

lv_obj_t *submenu_view_get_list(void)
{
    return s_submenu_list;
}

static void submenu_list_set_normal_geometry(void)
{
    uint16_t list_y = (CARD_TITLE_H > MENU_LIST_TOP_NUDGE_PX) ? (uint16_t)(CARD_TITLE_H - MENU_LIST_TOP_NUDGE_PX) : CARD_TITLE_H;
    uint16_t list_h = (s_submenu_height > list_y) ? (uint16_t)(s_submenu_height - list_y) : s_submenu_height;

    lv_obj_set_size(s_submenu_list, s_submenu_width - 15, list_h);
    lv_obj_set_pos(s_submenu_list, 0, list_y);
    lv_obj_set_style_pad_all(s_submenu_list, 0, 0);
    lv_obj_set_style_pad_bottom(s_submenu_list, MENU_LIST_EDGE_PAD_PX, 0);
    lv_obj_add_flag(s_submenu_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_submenu_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_submenu_list, LV_SCROLLBAR_MODE_OFF);
}

static void submenu_list_set_page_geometry(void)
{
    lv_obj_set_pos(s_submenu_list, 0, 0);
    lv_obj_set_size(s_submenu_list, s_submenu_width, s_submenu_height);
    lv_obj_set_style_pad_all(s_submenu_list, 0, 0);
    lv_obj_clear_flag(s_submenu_list, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_anim_enable_t submenu_selection_scroll_anim(void)
{
    return (MENU_LIST_SCROLL_ANIM_ENABLED && !s_submenu_selection_scroll_silent) ? LV_ANIM_ON : LV_ANIM_OFF;
}

static void submenu_list_scroll_item_to_view(lv_obj_t *item)
{
    lv_coord_t visible_h;
    lv_coord_t item_y;
    lv_coord_t item_h;
    lv_coord_t scroll_y;
    lv_coord_t target_y;
    lv_coord_t margin = MENU_LIST_EDGE_PAD_PX;

    lv_obj_update_layout(s_submenu_list);
    visible_h = lv_obj_get_height(s_submenu_list);
    item_y = lv_obj_get_y(item);
    item_h = lv_obj_get_height(item);
    scroll_y = lv_obj_get_scroll_y(s_submenu_list);
    target_y = scroll_y;
    if (visible_h <= item_h + margin * 2) margin = 0;

    if (item_y - margin < scroll_y) target_y = item_y - margin;
    else if (item_y + item_h + margin > scroll_y + visible_h) target_y = item_y + item_h + margin - visible_h;
    if (target_y < 0) target_y = 0;

    lv_obj_scroll_to_y(s_submenu_list, target_y, submenu_selection_scroll_anim());
}

void submenu_view_create(lv_obj_t *parent, uint16_t width, uint16_t height)
{
    /* 子菜单层初始放在屏幕右侧外面，通过 slide_in 动画滑入可视区域。 */
    s_submenu_width = width;
    s_submenu_height = height;

    s_submenu_layer = lv_obj_create(parent);
    lv_obj_set_size(s_submenu_layer, s_submenu_width, s_submenu_height);
    lv_obj_set_pos(s_submenu_layer, s_submenu_width, 0);
    lv_obj_set_style_bg_color(s_submenu_layer, BLACK, 0);
    lv_obj_set_style_bg_opa(s_submenu_layer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_submenu_layer, 0, 0);
    lv_obj_set_style_pad_all(s_submenu_layer, 0, 0);
    lv_obj_clear_flag(s_submenu_layer, LV_OBJ_FLAG_SCROLLABLE);

    s_submenu_title = lv_label_create(s_submenu_layer);
    lv_obj_remove_style_all(s_submenu_title);
    lv_obj_set_pos(s_submenu_title, 16, 8);
    lv_obj_set_size(s_submenu_title, s_submenu_width - 32, 40);
    lv_label_set_long_mode(s_submenu_title, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_submenu_title, "> SUB MENU");
    lv_obj_set_style_text_color(s_submenu_title, LIGHT, 0);
    lv_obj_set_style_text_font(s_submenu_title, get_font(FONT_ID_TITLE), 0);

    s_submenu_title_line = lv_obj_create(s_submenu_layer);
    lv_obj_remove_style_all(s_submenu_title_line);
    lv_obj_set_size(s_submenu_title_line, s_submenu_width - 32, 2);
    lv_obj_set_pos(s_submenu_title_line, 16, 48);
    lv_obj_set_style_bg_color(s_submenu_title_line, DARK, 0);
    lv_obj_set_style_bg_opa(s_submenu_title_line, LV_OPA_COVER, 0);

    s_submenu_list = lv_obj_create(s_submenu_layer);
    lv_obj_set_style_bg_opa(s_submenu_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_submenu_list, 0, 0);
    submenu_list_set_normal_geometry();
}

/* =========================================================
 * Sub-menu layer
 * ========================================================= */
static void submenu_slide_in(void)
{
    /* 打开子菜单时从右侧滑入。 */
    if (!s_submenu_layer) return;
    uint16_t slide_w = submenu_right_width();

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_submenu_layer);
    lv_anim_set_values(&a, slide_w, 0);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_time(&a, 250);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void submenu_slide_out(void)
{
    /* 关闭子菜单时滑回右侧屏幕外。 */
    if (!s_submenu_layer) return;
    uint16_t slide_w = submenu_right_width();

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_submenu_layer);
    lv_anim_set_values(&a, 0, slide_w);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_time(&a, 250);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_start(&a);
}

static lv_obj_t *plan_make_label(lv_obj_t *parent,
                                 const char *text,
                                 uint8_t font_id,
                                 lv_color_t color,
                                 int x,
                                 int y,
                                 int w,
                                 int h,
                                 lv_text_align_t align)
{
    /* 潜水计划页的文本控件都通过这个轻量工厂统一创建。 */
    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_remove_style_all(lbl);
    lv_obj_set_pos(lbl, x, y);
    lv_obj_set_size(lbl, w, h);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_style_text_font(lbl, get_font(font_id), 0);
    lv_obj_set_style_text_align(lbl, align, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(lbl, text ? text : "");
    return lbl;
}

static lv_obj_t *plan_make_button(lv_obj_t *parent, const char *text, int x, int y, bool focused)
{
    /* 潜水计划页底部按钮的统一样式工厂。 */
    int btn_w = 92;
    int btn_h = 34;
    font_id_t font_id = focused ? FONT_ID_MEDIUM : FONT_ID_TITLE;
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, btn_w, btn_h);
    lv_obj_set_style_bg_color(btn, BLACK, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, focused ? GREEN : DARK, 0);
    lv_obj_set_style_border_width(btn, focused ? INNER_BORDER_W + 2 : INNER_BORDER_W, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = plan_make_label(btn,
                                    text,
                                    font_id,
                                    focused ? LIGHT : GREEN,
                                    0,
                                    0,
                                    LV_SIZE_CONTENT,
                                    LV_SIZE_CONTENT,
                                    LV_TEXT_ALIGN_CENTER);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    return btn;
}

static bool plan_compact_layout(void)
{
    return !ui_layout_is_vertical_split() && s_submenu_height <= 320U;
}

static uint8_t plan_result_rows_per_page_for_layout(void)
{
    return plan_compact_layout() ? 6U : 8U;
}

static bool plan_action_buttons_visible(uint8_t page)
{
    return page != (uint8_t)DIVE_PLAN_PAGE_DEPTH &&
           page != (uint8_t)DIVE_PLAN_PAGE_TIME &&
           page != (uint8_t)DIVE_PLAN_PAGE_RMV;
}

static void plan_draw_header(lv_obj_t *parent, int w)
{
    /* 绘制潜水计划页顶部摘要区，展示输入参数和当前气体。 */
    ui_vm_dive_plan_view_t vm;

    ui_vm_dive_plan_view_update(&vm);

    lv_obj_t *oc = lv_obj_create(parent);
    lv_obj_remove_style_all(oc);
    lv_obj_set_pos(oc, 12, 8);
    lv_obj_set_size(oc, 30, 42);
    lv_obj_set_style_bg_color(oc, BLACK, 0);
    lv_obj_set_style_bg_opa(oc, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(oc, GREEN, 0);
    lv_obj_set_style_border_width(oc, 2, 0);
    plan_make_label(oc, "OC", FONT_ID_SMALL, LIGHT, 0, 9, 30, 22, LV_TEXT_ALIGN_CENTER);

    plan_make_label(parent, "DEPTH", FONT_ID_SMALL, GREEN, 70, 12, 70, 18, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, "TIME", FONT_ID_SMALL, GREEN, 155, 12, 70, 18, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, "RMV", FONT_ID_SMALL, GREEN, 240, 12, 70, 18, LV_TEXT_ALIGN_CENTER);

    plan_make_label(parent,
                    (vm.page == (uint8_t)DIVE_PLAN_PAGE_DEPTH) ? "---" : vm.depth_value,
                    FONT_ID_SMALL,
                    LIGHT,
                    70,
                    34,
                    70,
                    18,
                    LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent,
                    (vm.page <= (uint8_t)DIVE_PLAN_PAGE_TIME) ? "--" : vm.time_value,
                    FONT_ID_SMALL,
                    LIGHT,
                    155,
                    34,
                    70,
                    18,
                    LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent,
                    (vm.page <= (uint8_t)DIVE_PLAN_PAGE_RMV) ? "--" : vm.rmv_value,
                    FONT_ID_SMALL,
                    LIGHT,
                    240,
                    34,
                    70,
                    18,
                    LV_TEXT_ALIGN_CENTER);

    if (vm.header_gas_o2 != 0U)
    {
        {
            char buf[8];
            lv_snprintf(buf, sizeof(buf), "%u", (unsigned)vm.header_gas_o2);
            plan_make_label(parent, buf, FONT_ID_SMALL, LIGHT, w - 74, 12, 54, 18, LV_TEXT_ALIGN_CENTER);
        }
    }
    else
    {
        plan_make_label(parent, "---", FONT_ID_SMALL, LIGHT, w - 74, 12, 54, 18, LV_TEXT_ALIGN_CENTER);
    }
}

static void plan_draw_bottom_line(lv_obj_t *parent, int w)
{
    /* 底部分隔线用于把输入/结果区和动作按钮区分开。 */
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, 0, (int)s_submenu_height - 42);
    lv_obj_set_size(line, w, 1);
    lv_obj_set_style_bg_color(line, GREEN, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
}

static void plan_draw_input(lv_obj_t *parent, int w)
{
    /* 输入页根据当前 page 类型切换显示深度、时间或 RMV。 */
    ui_vm_dive_plan_view_t vm;
    char buf[48];
    bool compact = plan_compact_layout();
    int value_y = compact ? 74 : 98;
    int underline_y = compact ? 116 : 132;
    int prompt_y = compact ? 136 : 166;
    int unit_y = compact ? 160 : 190;
    int min_y = compact ? 188 : 224;
    int max_y = compact ? 210 : 246;
    int spin_y = compact ? 232 : 276;

    ui_vm_dive_plan_view_update(&vm);

    if (vm.page == (uint8_t)DIVE_PLAN_PAGE_TIME)
    {
        plan_make_label(parent, vm.time_value, FONT_ID_MEDIUM, LIGHT, 0, value_y, w, 42, LV_TEXT_ALIGN_CENTER);
    }
    else if (vm.page == (uint8_t)DIVE_PLAN_PAGE_RMV)
    {
        plan_make_label(parent, vm.rmv_value, FONT_ID_MEDIUM, LIGHT, 0, value_y, w, 42, LV_TEXT_ALIGN_CENTER);
    }
    else
    {
        plan_make_label(parent, vm.depth_value, FONT_ID_MEDIUM, LIGHT, 0, value_y, w, 42, LV_TEXT_ALIGN_CENTER);
    }

    lv_obj_t *underline = lv_obj_create(parent);
    lv_obj_remove_style_all(underline);
    lv_obj_set_pos(underline, (w - 38) / 2, underline_y);
    lv_obj_set_size(underline, 38, 4);
    lv_obj_set_style_bg_color(underline, lv_color_make(255, 255, 0), 0);
    lv_obj_set_style_bg_opa(underline, LV_OPA_COVER, 0);

    plan_make_label(parent, vm.input_prompt, FONT_ID_SMALL, LIGHT, 0, prompt_y, w, 22, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, vm.input_unit, FONT_ID_SMALL, LIGHT, 0, unit_y, w, 22, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, vm.input_min_text, FONT_ID_SMALL, LIGHT, 0, min_y, w, 18, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, vm.input_max_text, FONT_ID_SMALL, LIGHT, 0, max_y, w, 18, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *spin = lv_obj_create(parent);
    lv_obj_remove_style_all(spin);
    lv_obj_set_pos(spin, (w - 100) / 2, spin_y);
    lv_obj_set_size(spin, 100, 25);
    lv_obj_set_style_bg_color(spin, LIGHT, 0);
    lv_obj_set_style_bg_opa(spin, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(spin, DARK, 0);
    lv_obj_set_style_border_width(spin, 1, 0);
    if (vm.page == (uint8_t)DIVE_PLAN_PAGE_TIME)
    {
        lv_snprintf(buf, sizeof(buf), "%s", vm.time_value);
    }
    else if (vm.page == (uint8_t)DIVE_PLAN_PAGE_RMV)
    {
        lv_snprintf(buf, sizeof(buf), "%s", vm.rmv_value);
    }
    else
    {
        lv_snprintf(buf, sizeof(buf), "%s", vm.depth_value);
    }
    plan_make_label(spin, buf, FONT_ID_SMALL, BLACK, 6, 2, 78, 20, LV_TEXT_ALIGN_RIGHT);
    plan_make_label(spin, "^", FONT_ID_SMALL, DARK, 84, 0, 14, 12, LV_TEXT_ALIGN_CENTER);
    plan_make_label(spin, "v", FONT_ID_SMALL, DARK, 84, 12, 14, 12, LV_TEXT_ALIGN_CENTER);
}

static void plan_draw_ready(lv_obj_t *parent, int w)
{
    /* READY 页用于展示“可以开始计算”的摘要状态。 */
    ui_vm_dive_plan_view_t vm;
    bool compact = plan_compact_layout();
    int title_y = compact ? 76 : 106;
    int line_x = compact ? 126 : 96;
    int line_w = compact ? w - 252 : w - 192;
    int gf_y = compact ? 130 : 168;
    int stop_y = compact ? 160 : 200;
    int cns_y = compact ? 190 : 232;
    int gas_y = compact ? 220 : 264;

    ui_vm_dive_plan_view_update(&vm);

    plan_make_label(parent, "Ready to Plan Dive", FONT_ID_SMALL, LIGHT, 0, title_y, w, 24, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, vm.ready_gf_text, FONT_ID_SMALL, LIGHT, line_x, gf_y, line_w, 22, LV_TEXT_ALIGN_LEFT);
    plan_make_label(parent, vm.ready_last_stop_text, FONT_ID_SMALL, LIGHT, line_x, stop_y, line_w, 22, LV_TEXT_ALIGN_LEFT);
    plan_make_label(parent, vm.ready_start_cns_text, FONT_ID_SMALL, LIGHT, line_x, cns_y, line_w, 22, LV_TEXT_ALIGN_LEFT);
    plan_make_label(parent, vm.gas_summary, FONT_ID_SMALL, LIGHT, line_x, gas_y, line_w, 22, LV_TEXT_ALIGN_LEFT);
}

static void plan_draw_calculating(lv_obj_t *parent, int w)
{
    bool compact = plan_compact_layout();
    plan_make_label(parent, "Calculating Plan", FONT_ID_MEDIUM, LIGHT, 0, compact ? 104 : 126, w, 40, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, "Please wait...", FONT_ID_SMALL, LIGHT, 0, compact ? 164 : 188, w, 24, LV_TEXT_ALIGN_CENTER);
}

static void plan_draw_result_summary(lv_obj_t *parent, int w, const ui_vm_dive_plan_view_t *vm)
{
    bool compact = plan_compact_layout();
    int title_y = compact ? 72 : 76;
    int line_x = compact ? 126 : 92;
    int line_w = compact ? w - 252 : w - 184;
    int row_y = compact ? 108 : 126;
    int row_gap = compact ? 27 : 38;

    plan_make_label(parent, "SUMMARY", FONT_ID_SMALL, GREEN, 0, title_y, w, 22, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, vm->result_runtime_text, FONT_ID_SMALL, LIGHT, line_x, row_y + row_gap * 0, line_w, 22, LV_TEXT_ALIGN_LEFT);
    plan_make_label(parent, vm->result_deco_text, FONT_ID_SMALL, LIGHT, line_x, row_y + row_gap * 1, line_w, 22, LV_TEXT_ALIGN_LEFT);
    plan_make_label(parent, vm->result_gas_text, FONT_ID_SMALL, LIGHT, line_x, row_y + row_gap * 2, line_w, 22, LV_TEXT_ALIGN_LEFT);
    plan_make_label(parent, vm->result_cns_text, FONT_ID_SMALL, LIGHT, line_x, row_y + row_gap * 3, line_w, 22, LV_TEXT_ALIGN_LEFT);
    plan_make_label(parent, vm->result_otu_text, FONT_ID_SMALL, LIGHT, line_x, row_y + row_gap * 4, line_w, 22, LV_TEXT_ALIGN_LEFT);
}

static void plan_draw_result(lv_obj_t *parent, int w)
{
    ui_vm_dive_plan_view_t vm;
    int col_x[5];
    int col_w[5];
    bool compact = plan_compact_layout();
    int table_right = w - 44;
    int gap = 8;
    int head_y = compact ? 58 : 68;
    int line_y = compact ? 80 : 88;
    int row_y = compact ? 92 : 100;
    int row_gap = compact ? 22 : 26;
    int page_y = compact ? (int)s_submenu_height - 60 : (int)s_submenu_height - 72;

    ui_vm_dive_plan_view_update(&vm);

    if (table_right > w - 16) table_right = w - 16;
    if (table_right < 360) table_right = w - 24;
    col_w[4] = 64;
    col_w[3] = 74;
    col_w[2] = 58;
    col_w[1] = 58;
    col_w[0] = 58;
    col_x[4] = table_right - col_w[4];
    col_x[3] = col_x[4] - gap - col_w[3];
    col_x[2] = col_x[3] - gap - col_w[2];
    col_x[1] = col_x[2] - gap - col_w[1];
    col_x[0] = col_x[1] - gap - col_w[0];
    if (col_x[0] < 16)
    {
        col_x[0] = 16;
    }

    if (vm.result_summary_page != 0U)
    {
        plan_draw_result_summary(parent, w, &vm);
        plan_make_label(parent, vm.result_page_text, FONT_ID_SMALL, GREEN, 0, page_y, w, 18, LV_TEXT_ALIGN_CENTER);
        return;
    }

    plan_make_label(parent, "Stp", FONT_ID_SMALL, GREEN, col_x[0], head_y, col_w[0], 18, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, "Tme", FONT_ID_SMALL, GREEN, col_x[1], head_y, col_w[1], 18, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, "Run", FONT_ID_SMALL, GREEN, col_x[2], head_y, col_w[2], 18, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, "Gas", FONT_ID_SMALL, GREEN, col_x[3], head_y, col_w[3], 18, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, "Qty", FONT_ID_SMALL, GREEN, col_x[4], head_y, col_w[4], 18, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, 16, line_y);
    lv_obj_set_size(line, w - 32, 1);
    lv_obj_set_style_bg_color(line, GREEN, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);

    for (uint8_t i = 0U; i < vm.result_rows_per_page && i < 8U; i++)
    {
        if (vm.rows[i].valid == 0U)
        {
            continue;
        }
        plan_make_label(parent, vm.rows[i].depth_text, FONT_ID_SMALL, LIGHT, col_x[0], row_y, col_w[0], 18, LV_TEXT_ALIGN_CENTER);
        plan_make_label(parent, vm.rows[i].time_text, FONT_ID_SMALL, LIGHT, col_x[1], row_y, col_w[1], 18, LV_TEXT_ALIGN_CENTER);
        plan_make_label(parent, vm.rows[i].run_text, FONT_ID_SMALL, LIGHT, col_x[2], row_y, col_w[2], 18, LV_TEXT_ALIGN_CENTER);
        plan_make_label(parent, vm.rows[i].gas_text, FONT_ID_SMALL, LIGHT, col_x[3], row_y, col_w[3], 18, LV_TEXT_ALIGN_CENTER);
        plan_make_label(parent, vm.rows[i].qty_text, FONT_ID_SMALL, LIGHT, col_x[4], row_y, col_w[4], 18, LV_TEXT_ALIGN_RIGHT);
        row_y += row_gap;
    }

    plan_make_label(parent, vm.result_page_text, FONT_ID_SMALL, GREEN, 0, page_y, w, 18, LV_TEXT_ALIGN_CENTER);
}

static void plan_draw_error(lv_obj_t *parent, int w)
{
    ui_vm_dive_plan_view_t vm;

    ui_vm_dive_plan_view_update(&vm);

    plan_make_label(parent, vm.error_title, FONT_ID_MEDIUM, LIGHT, 0, 118, w, 40, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, vm.error_hint, FONT_ID_SMALL, LIGHT, 0, 176, w, 24, LV_TEXT_ALIGN_CENTER);
}

static void submenu_populate_dive_plan(const menu_row_t *rows, uint8_t count)
{
    ui_vm_dive_plan_view_t vm;

    if (!s_submenu_list) return;

    submenu_dive_plan_set_result_rows_per_page(plan_result_rows_per_page_for_layout());
    ui_vm_dive_plan_view_update(&vm);

    if (s_dive_plan_last_vm_valid && (memcmp(&s_dive_plan_last_vm, &vm, sizeof(vm)) == 0))
    {
        /* 页面内容没变化时直接复用现有对象，避免把 LCD 消息队列刷爆。 */
        return;
    }

    int w = (int)submenu_right_width();
    uint8_t focus_idx = (count > 1U) ? 1U : 0U;

    s_dive_plan_last_vm = vm;
    s_dive_plan_last_vm_valid = true;
    ui_state_set_sub_item_count(count);
    lv_obj_clean(s_submenu_list);
    s_light_status_lbl = NULL;

    bool show_actions = plan_action_buttons_visible(vm.page);

    if (show_actions && count > 0U)
    {
        bool focused = (focus_idx == 0U);
        (void)plan_make_button(s_submenu_list, rows[0].label, 12, (int)s_submenu_height - 38, focused);
    }
    if (show_actions && count > 1U)
    {
        bool focused = (focus_idx == 1U);
        (void)plan_make_button(s_submenu_list, rows[1].label, w - 104, (int)s_submenu_height - 38, focused);
    }

    plan_draw_header(s_submenu_list, w);
    switch ((dive_plan_page_t)vm.page)
    {
    case DIVE_PLAN_PAGE_READY:
        plan_draw_ready(s_submenu_list, w);
        break;
    case DIVE_PLAN_PAGE_CALCULATING:
        plan_draw_calculating(s_submenu_list, w);
        break;
    case DIVE_PLAN_PAGE_RESULT:
        plan_draw_result(s_submenu_list, w);
        break;
    case DIVE_PLAN_PAGE_ERROR:
        plan_draw_error(s_submenu_list, w);
        break;
    case DIVE_PLAN_PAGE_DEPTH:
    case DIVE_PLAN_PAGE_TIME:
    case DIVE_PLAN_PAGE_RMV:
    default:
        plan_draw_input(s_submenu_list, w);
        break;
    }
    if (show_actions && count > 0U)
    {
        plan_draw_bottom_line(s_submenu_list, w);
    }
    screen_set_submenu_selection(focus_idx);
}

static void submenu_populate(const char *title, const menu_row_t *rows, uint8_t count)
{
    if (!s_submenu_title || !s_submenu_list) return;

    bool is_dive_plan = menu_runtime_is_dive_plan();
    if (is_dive_plan)
    {
        /* DIVE PLAN 不是普通“纵向菜单列表”，而是一个借用子菜单层承载的独立页面。
         * 因此标题栏、列表尺寸和选中态逻辑都要切到专用分支。 */
        lv_label_set_text(s_submenu_title, "DIVE PLAN");
        lv_obj_add_flag(s_submenu_title, LV_OBJ_FLAG_HIDDEN);
        if (s_submenu_title_line)
        {
            lv_obj_add_flag(s_submenu_title_line, LV_OBJ_FLAG_HIDDEN);
        }
        submenu_list_set_page_geometry();
        submenu_populate_dive_plan(rows, count);
        return;
    }

    if (menu_runtime_is_logbook())
    {
        lv_label_set_text(s_submenu_title, "DIVE LOG");
        lv_obj_add_flag(s_submenu_title, LV_OBJ_FLAG_HIDDEN);
        if (s_submenu_title_line)
        {
            lv_obj_add_flag(s_submenu_title_line, LV_OBJ_FLAG_HIDDEN);
        }
        submenu_list_set_page_geometry();
        submenu_populate_logbook();
        return;
    }

    if (s_logbook_picker_view_ready)
    {
        logbook_picker_view_reset();
    }

    lv_obj_clear_flag(s_submenu_title, LV_OBJ_FLAG_HIDDEN);
    if (s_submenu_title_line)
    {
        lv_obj_clear_flag(s_submenu_title_line, LV_OBJ_FLAG_HIDDEN);
    }
    submenu_list_set_normal_geometry();

    if (menu_runtime_is_nested())
    {
        char nested_title[48];
        lv_snprintf(nested_title, sizeof(nested_title), "> %s", title ? title : "");
        lv_label_set_text(s_submenu_title, nested_title);
    }
    else
    {
        lv_label_set_text(s_submenu_title, title);
    }
    lv_obj_clean(s_submenu_list);
    s_light_status_lbl = NULL;  /* 重置 LIGHT 状态标签 */

    /* right_w 从缓存读取，fallback = safe_zone_w - left_anchor_w - panel_gap */
    ui_vm_menu_layout_t menu_layout_vm;
    ui_vm_menu_layout_update(&menu_layout_vm, NULL);
    uint16_t right_w = submenu_right_width();
    uint16_t sub_w = right_w;
    int item_h = (int)menu_layout_vm.item_h_px;
    int item_w = (int)sub_w - 15;
    int gap_y  = (int)menu_layout_vm.gap_y_px;
    int current_y = MENU_LIST_EDGE_PAD_PX;
    bool compact_plan = menu_runtime_is_dive_plan_result();
    if (compact_plan)
    {
        item_h = 24;
        gap_y = 2;
    }

    for (uint8_t i = 0; i < count; i++)
    {
        lv_obj_t *item = lv_obj_create(s_submenu_list);
        lv_obj_set_pos(item, 0, current_y);
        lv_obj_set_size(item, item_w, item_h);
        lv_obj_set_style_bg_color(item, BLACK, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(item, DARK, 0);
        lv_obj_set_style_border_width(item, INNER_BORDER_W, LV_PART_MAIN);
        lv_obj_set_style_radius(item, 0, 0);
        lv_obj_set_style_pad_all(item, 0, LV_PART_MAIN);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

        /* LIGHT CONTROL 特殊布局: 左侧项目名，右侧当前状态 */
        if (rows[i].type == MENU_ROW_LIGHT_POWER || rows[i].type == MENU_ROW_LIGHT_MODE)
        {
            const bool is_power = (rows[i].type == MENU_ROW_LIGHT_POWER);

            lv_obj_t *lbl_light = lv_label_create(item);
            lv_obj_set_style_text_color(lbl_light, GREEN, 0);
            lv_obj_set_style_text_font(lbl_light, get_font(FONT_ID_TITLE), 0);
            lv_obj_set_size(lbl_light, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align(lbl_light, LV_ALIGN_LEFT_MID, 12, 0);
            lv_obj_set_style_text_align(lbl_light, LV_TEXT_ALIGN_LEFT, 0);
            lv_label_set_text(lbl_light, is_power ? "LIGHT" : "MODE");

            lv_obj_t *lbl_status = lv_label_create(item);
            lv_obj_set_style_text_color(lbl_status, submenu_light_status_color(&rows[i]), 0);
            lv_obj_set_style_text_font(lbl_status, get_font(FONT_ID_TITLE), 0);
            lv_obj_set_size(lbl_status, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align(lbl_status, LV_ALIGN_RIGHT_MID, -12, 0);
            lv_obj_set_style_text_align(lbl_status, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(lbl_status, is_power
                              ? (submenu_light_power_on() ? "ON" : "OFF")
                              : submenu_light_mode_text());

            if (is_power)
            {
                s_light_status_lbl = lbl_status;
            }
            current_y += item_h + gap_y;
            continue;
        }

        /* 普通菜单项 */
        /* 普通项只渲染标题文字，不在 view 层处理点击逻辑。
         * 选中、进入子菜单、编辑数值等动作统一交给 menu_actions / ui_state。 */
        lv_obj_t *lbl = lv_label_create(item);
        lv_obj_set_style_text_color(lbl, GREEN, 0);
        lv_obj_set_style_text_font(lbl, get_font(compact_plan ? FONT_ID_SMALL : FONT_ID_TITLE), 0);
        lv_obj_set_size(lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 12, 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_LEFT, 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_label_set_text(lbl, rows[i].label ? rows[i].label : "");

        current_y += item_h + gap_y;
    }
    screen_set_submenu_selection(0);
}

static void submenu_populate_current(void)
{
    uint8_t count = 0;
    const menu_row_t *rows = menu_runtime_current_rows(&count);
    submenu_populate(menu_runtime_current_title(), rows, count);
    if (!menu_runtime_is_logbook())
    {
        ui_state_set_sub_item_count(count);
    }
}

void screen_set_submenu_selection(uint8_t idx)
{
    if (!s_submenu_list) return;
    if (submenu_is_dive_plan_visible())
    {
        uint32_t cnt = lv_obj_get_child_cnt(s_submenu_list);
        uint32_t action_count = ui_state_get_sub_item_count();
        if (!plan_action_buttons_visible((uint8_t)submenu_dive_plan_get_page()))
        {
            return;
        }
        if (action_count == 0U)
        {
            return;
        }
        if (action_count > cnt)
        {
            action_count = cnt;
        }
        for (uint32_t i = 0; i < action_count; i++)
        {
            lv_obj_t *item = lv_obj_get_child(s_submenu_list, i);
            lv_obj_t *lbl = item ? lv_obj_get_child(item, 0) : NULL;
            if (!item) continue;
            if (i == idx)
            {
                lv_obj_set_style_border_color(item, GREEN, 0);
                lv_obj_set_style_border_width(item, INNER_BORDER_W + 2, 0);
                if (lbl)
                {
                    lv_obj_set_style_text_color(lbl, LIGHT, 0);
                    lv_obj_set_style_text_font(lbl, get_font(FONT_ID_MEDIUM), 0);
                    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
                }
            }
            else
            {
                lv_obj_set_style_border_color(item, DARK, 0);
                lv_obj_set_style_border_width(item, INNER_BORDER_W, 0);
                if (lbl)
                {
                    lv_obj_set_style_text_color(lbl, GREEN, 0);
                    lv_obj_set_style_text_font(lbl, get_font(FONT_ID_TITLE), 0);
                    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
                }
            }
        }
        return;
    }
    if (submenu_is_logbook_visible())
    {
        return;
    }

    bool compact_plan = menu_runtime_is_dive_plan_result();
    uint32_t cnt = lv_obj_get_child_cnt(s_submenu_list);
    if (cnt == 0U)
    {
        return;
    }
    if (idx >= cnt)
    {
        idx = (uint8_t)(cnt - 1U);
    }
    for (uint32_t i = 0; i < cnt; i++)
    {
        lv_obj_t *item = lv_obj_get_child(s_submenu_list, i);
        lv_obj_t *lbl  = lv_obj_get_child(item, 0);
        /* 正在编辑的 item 由 begin_edit_value 单独管理，不参与选中态刷新 */
        if (ui_state_get_edit_active() && ((uint8_t)i == ui_state_get_edit_item_index())) continue;
        if (i == idx)
        {
            lv_obj_add_state(item, LV_STATE_FOCUSED);  // HOTFIX: Clear LVGL states to fix bold residue.
            lv_obj_set_style_bg_color(item, BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(item, GREEN, 0);
            lv_obj_set_style_border_width(item, INNER_BORDER_W + 2, 0);
            if (lbl)
            {
                lv_obj_set_style_text_color(lbl, LIGHT, 0);
                lv_obj_set_style_text_font(lbl, get_font(compact_plan ? FONT_ID_SMALL : FONT_ID_MEDIUM), 0);
            }
            /* LIGHT CONTROL second column uses the same selected emphasis. */
            lv_obj_t *lbl2 = lv_obj_get_child(item, 1);
            if (lbl2)
            {
                lv_obj_set_style_text_color(lbl2, LIGHT, 0);
                lv_obj_set_style_text_font(lbl2, get_font(compact_plan ? FONT_ID_SMALL : FONT_ID_MEDIUM), 0);
            }
        }
        else
        {
            lv_obj_clear_state(item, LV_STATE_FOCUSED | LV_STATE_EDITED | LV_STATE_CHECKED);  // HOTFIX: Clear LVGL states to fix bold residue.
            if (lbl) lv_obj_clear_state(lbl, LV_STATE_ANY);  // HOTFIX: Clear LVGL states to fix bold residue.
            lv_obj_set_style_bg_color(item, BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(item, DARK, 0);
            lv_obj_set_style_border_width(item, INNER_BORDER_W, 0);
            if (lbl)
            {
                lv_obj_set_style_text_color(lbl, GREEN, 0);
                lv_obj_set_style_text_font(lbl, get_font(compact_plan ? FONT_ID_SMALL : FONT_ID_TITLE), 0);
            }
            /* LIGHT CONTROL 特殊处理：第二列状态恢复为当前状态色 */
            lv_obj_t *lbl2 = lv_obj_get_child(item, 1);
            if (lbl2)
            {
                const menu_row_t *row = menu_runtime_row_at((uint8_t)i);
                lv_obj_set_style_text_color(lbl2, submenu_light_status_color(row), 0);
                lv_obj_set_style_text_font(lbl2, get_font(compact_plan ? FONT_ID_SMALL : FONT_ID_TITLE), 0);
            }
        }
    }
    lv_obj_t *selected_item = lv_obj_get_child(s_submenu_list, idx);
    if (selected_item)
    {
        submenu_list_scroll_item_to_view(selected_item);
    }
}

/* INFO sub-menu */
void screen_open_info_submenu(uint8_t item_idx)
{
    uint8_t count = 0;
    if (!menu_runtime_open_info(item_idx))
    {
        return;
    }
    if (menu_runtime_is_dive_plan())
    {
        submenu_dive_plan_render_cache_reset();
        submenu_dive_plan_reset();
        menu_runtime_refresh();
    }
    if (menu_runtime_is_logbook())
    {
        logbook_reset_state();
        menu_runtime_refresh();
    }

    submenu_populate_current();
    if (!menu_runtime_is_logbook())
    {
        (void)menu_runtime_current_rows(&count);
        ui_state_set_sub_item_count(count);
        ui_state_set_sub_menu_idx(menu_runtime_default_selection());
    }
    ui_state_set_sub_parent(UI_INFO);
    ui_state_set_state(UI_SUB_MENU);
    ui_state_set_sub_history_depth(0U);
    screen_set_submenu_selection(ui_state_get_sub_menu_idx());
    submenu_slide_in();
}

static void refresh_info_submenu_page(uint8_t keep_idx)
{
    uint8_t count = 0;
    bool prev_silent = s_submenu_selection_scroll_silent;
    menu_runtime_refresh();
    (void)menu_runtime_current_rows(&count);

    s_submenu_selection_scroll_silent = true;
    submenu_populate_current();
    ui_state_set_sub_item_count(count);
    if (count == 0U)
    {
        keep_idx = 0U;
    }
    else if (keep_idx >= count)
    {
        keep_idx = (uint8_t)(count - 1U);
    }
    ui_state_set_sub_menu_idx(keep_idx);
    screen_set_submenu_selection(keep_idx);
    s_submenu_selection_scroll_silent = prev_silent;
}

void screen_refresh_info_submenu_if_open(void)
{
    if (!s_submenu_title || !s_submenu_list)
    {
        return;
    }
    if ((ui_state_get_state() != UI_SUB_MENU) ||
        (ui_state_get_sub_parent() != UI_INFO) ||
        (ui_state_get_sub_history_depth() != 0U))
    {
        return;
    }

    if (menu_runtime_is_dive_plan() && submenu_dive_plan_is_calculating())
    {
        return;
    }
    if (menu_runtime_is_logbook())
    {
        /* Logbook 页面由进入、旋转、选中等显式交互驱动刷新。
         * 避免通用 INFO 刷新节拍反复重建列表并触发 SD 摘要读取。 */
        return;
    }

    refresh_info_submenu_page(ui_state_get_sub_menu_idx());
}

void screen_refresh_settings_submenu_if_open(void)
{
    if (!s_submenu_title || !s_submenu_list)
    {
        return;
    }
    if ((ui_state_get_state() != UI_SUB_MENU) ||
        (ui_state_get_sub_parent() != UI_SETUP))
    {
        return;
    }

    /* APP 或其它业务入口只更新 bus/user settings；UI 在自己的刷新节拍内
     * 重新同步菜单模型缓存并重绘当前页，避免 app_ui 反向依赖上层业务。 */
    submenu_sync_persisted_settings();
    refresh_current_submenu_page(ui_state_get_sub_menu_idx());
}

bool screen_handle_dive_plan_rotate(int8_t dir)
{
    if ((ui_state_get_state() != UI_SUB_MENU) ||
        (ui_state_get_sub_parent() != UI_INFO) ||
        !submenu_is_dive_plan_visible())
    {
        return false;
    }
    if (submenu_dive_plan_is_calculating())
    {
        return false;
    }
    if (!submenu_dive_plan_handle_rotate(dir))
    {
        return false;
    }
    refresh_info_submenu_page(1U);
    return true;
}

bool screen_handle_logbook_rotate(int8_t dir)
{
    if ((ui_state_get_state() != UI_SUB_MENU) ||
        (ui_state_get_sub_parent() != UI_INFO) ||
        !submenu_is_logbook_visible())
    {
        return false;
    }

    if (!s_logbook_valid)
    {
        s_logbook_focus = 0U;
        submenu_populate_current();
        return true;
    }

    if (s_logbook_page == LOGBOOK_PAGE_PICK)
    {
        uint8_t focus_count = logbook_picker_focus_count();
        int16_t next = (int16_t)s_logbook_picker_focus + dir;
        if (focus_count == 0U)
        {
            s_logbook_picker_focus = 0U;
            submenu_populate_current();
            return true;
        }
        if (next < 0) next = 0;
        if (next >= (int16_t)focus_count) next = (int16_t)(focus_count - 1U);
        s_logbook_picker_focus = (uint8_t)next;
        if (s_logbook_picker_focus < s_logbook_picker_visible)
        {
            s_logbook_focus = (uint16_t)(s_logbook_page_index * LOGBOOK_PICKER_VISIBLE_ROWS +
                                         logbook_picker_focus_to_index(s_logbook_picker_focus, s_logbook_picker_visible));
        }
        submenu_populate_current();
        return true;
    }

    if (dir > 0)
    {
        if (s_logbook_page == LOGBOOK_PAGE_SUMMARY) s_logbook_page = LOGBOOK_PAGE_DETAIL_1;
        else if (s_logbook_page == LOGBOOK_PAGE_DETAIL_1) s_logbook_page = LOGBOOK_PAGE_DETAIL_2;
    }
    else if (dir < 0)
    {
        if (s_logbook_page == LOGBOOK_PAGE_DETAIL_1) s_logbook_page = LOGBOOK_PAGE_SUMMARY;
        else if (s_logbook_page == LOGBOOK_PAGE_DETAIL_2) s_logbook_page = LOGBOOK_PAGE_DETAIL_1;
    }
    s_logbook_focus = 0U;
    submenu_populate_current();
    return true;
}

bool screen_handle_logbook_back(void)
{
    if ((ui_state_get_state() != UI_SUB_MENU) ||
        (ui_state_get_sub_parent() != UI_INFO) ||
        !submenu_is_logbook_visible())
    {
        return false;
    }

    screen_close_submenu();
    return true;
}

static void screen_handle_logbook_select(void)
{
    if (!s_logbook_valid)
    {
        screen_close_submenu();
        return;
    }

    switch (s_logbook_page)
    {
    case LOGBOOK_PAGE_PICK:
        if (s_logbook_picker_focus == logbook_picker_focus_back())
        {
            if (s_logbook_page_index > 0U)
            {
                s_logbook_page_index--;
                logbook_picker_prepare_page(s_logbook_page_index);
                s_logbook_picker_focus = logbook_picker_has_back() ? logbook_picker_focus_back() : logbook_picker_focus_next();
                submenu_populate_current();
            }
            break;
        }
        if (s_logbook_picker_focus == logbook_picker_focus_next())
        {
            if ((s_logbook_page_index + 1U) < s_logbook_page_count)
            {
                s_logbook_page_index++;
                logbook_picker_prepare_page(s_logbook_page_index);
                s_logbook_picker_focus = logbook_picker_has_next() ? logbook_picker_focus_next() : logbook_picker_focus_back();
                submenu_populate_current();
            }
            break;
        }
        if (s_logbook_picker_focus >= s_logbook_picker_visible)
        {
            break;
        }
        s_logbook_index = (uint16_t)(s_logbook_page_index * LOGBOOK_PICKER_VISIBLE_ROWS +
                                     logbook_picker_focus_to_index(s_logbook_picker_focus, s_logbook_picker_visible));
        if (s_logbook_index >= s_logbook_snapshot_count)
        {
            break;
        }
        logbook_points_release();
        logbook_detail_stop_loader();
        if (s_logbook_snapshot_entries[s_logbook_index].valid)
        {
            logbook_entry_from_picker_item(&s_logbook_entry, &s_logbook_snapshot_entries[s_logbook_index]);
        }
        else
        {
            /* 首次点击未缓存条目时，先显示占位详情页，避免同步读 summary/detail 卡住 UI。 */
            logbook_entry_make_pending(&s_logbook_entry, s_logbook_index);
        }
        s_logbook_valid = s_logbook_entry.valid;
        s_logbook_focus = s_logbook_index;
        s_logbook_page = LOGBOOK_PAGE_SUMMARY;
        s_logbook_focus = 0U;
        /* Summary 首屏只依赖 picker summary 和 profile 点。profile 若仍放在 detail
         * 异步线程后面，会等 get_detail 诊断读取完成后才出现路径图，用户会看到图和数据延迟补出。 */
        if (s_logbook_valid)
        {
            if (!logbook_prefetch_take_wait(s_logbook_index, 240U))
            {
                (void)logbook_points_load(s_logbook_index);
            }
        }
        submenu_populate_current();
        logbook_detail_start_loader(s_logbook_index);
        break;
    case LOGBOOK_PAGE_SUMMARY:
    case LOGBOOK_PAGE_DETAIL_1:
    case LOGBOOK_PAGE_DETAIL_2:
        break;
    default:
        s_logbook_page = LOGBOOK_PAGE_PICK;
        s_logbook_focus = 0U;
        submenu_populate_current();
        break;
    }
}

static bool refresh_compass_cal_submenu(void)
{
    uint8_t count = 0;
    bool prev_silent = s_submenu_selection_scroll_silent;
    if (!s_submenu_list || !s_submenu_title)
    {
        return false;
    }

    if (menu_runtime_current_id() != MENU_SETUP_COMPASS_CAL)
    {
        return false;
    }

    menu_runtime_refresh();
    (void)menu_runtime_current_rows(&count);
    if (count == 0)
    {
        return false;
    }
    s_submenu_selection_scroll_silent = true;
    submenu_populate_current();
    ui_state_set_sub_item_count(count);
    if (ui_state_get_sub_menu_idx() >= count)
    {
        ui_state_set_sub_menu_idx((uint8_t)(count - 1U));
    }
    screen_set_submenu_selection(ui_state_get_sub_menu_idx());
    s_submenu_selection_scroll_silent = prev_silent;
    return true;
}

void screen_open_setup_submenu(uint8_t item_idx)
{
    uint8_t count = 0;
    if (!menu_runtime_open_setup(item_idx))
    {
        return;
    }

    submenu_populate_current();
    (void)menu_runtime_current_rows(&count);
    ui_state_set_sub_item_count(count);
    ui_state_set_sub_menu_idx(0U);
    ui_state_set_sub_parent(UI_SETUP);
    ui_state_set_state(UI_SUB_MENU);
    ui_state_set_sub_history_depth(0U);
    submenu_slide_in();
}

void screen_refresh_compass_cal_submenu_if_open(void)
{
    (void)refresh_compass_cal_submenu();
}

void screen_open_nested_submenu(const char *title, const char **items, uint8_t count)
{
    (void)title;
    (void)items;
    (void)count;
    submenu_populate_current();
    (void)menu_runtime_current_rows(&count);
    ui_state_set_sub_item_count(count);
    ui_state_set_sub_menu_idx(0U);
    ui_state_set_state(UI_SUB_MENU);
}

static void refresh_current_submenu_page(uint8_t keep_idx)
{
    uint8_t count = 0;
    bool prev_silent = s_submenu_selection_scroll_silent;
    menu_runtime_refresh();
    (void)menu_runtime_current_rows(&count);

    s_submenu_selection_scroll_silent = true;
    submenu_populate_current();
    ui_state_set_sub_item_count(count);
    if (count == 0U)
    {
        keep_idx = 0U;
    }
    else if (keep_idx >= count)
    {
        keep_idx = (uint8_t)(count - 1);
    }
    ui_state_set_sub_menu_idx(keep_idx);
    screen_set_submenu_selection(keep_idx);
    s_submenu_selection_scroll_silent = prev_silent;
}

void screen_handle_submenu_select(uint8_t item_idx)
{
    menu_action_t action;
    const menu_row_t *row;
    if (!s_submenu_list || !s_submenu_title) return;
    if (menu_runtime_is_logbook())
    {
        screen_handle_logbook_select();
        return;
    }
    if (menu_runtime_is_dive_plan() && ui_state_get_sub_item_count() == 0U)
    {
        bool close_submenu = false;
        uint8_t keep_idx = 0U;
        if (submenu_dive_plan_handle_action(MENU_ITEM_DIVE_PLAN_NEXT, &close_submenu, &keep_idx))
        {
            if (close_submenu) screen_close_submenu();
            else refresh_current_submenu_page(keep_idx);
        }
        return;
    }
    if (item_idx >= ui_state_get_sub_item_count()) return;
    row = menu_runtime_row_at(item_idx);
    if (!menu_actions_handle_select(item_idx, row, &action))
    {
        return;
    }
    /* 这里是子菜单层的“动作分发口”：
     * menu_runtime 决定当前有哪些行；
     * menu_actions 决定选中某一行后应该产生什么动作；
     * submenu_view 只负责把动作翻译成界面行为或状态切换。 */

    switch (action.type)
    {
    case MENU_ACTION_BACK:
    case MENU_ACTION_CLOSE:
        screen_close_submenu();
        break;
    case MENU_ACTION_OPEN_CHILD:
        if (menu_runtime_open_child(action.child_menu, row->id))
        {
            submenu_populate_current();
            /* 子菜单再次进入子级后，UI 状态侧也必须同步真实层级深度。
             * 否则布局重建时会把“当前在子菜单里”误判成顶层菜单，出现旋钮输入还在、
             * 但可见菜单上下文已经丢失的假死现象。 */
            ui_state_set_sub_history_depth(menu_runtime_stack_depth());
            ui_state_set_sub_menu_idx(0U);
            screen_set_submenu_selection(ui_state_get_sub_menu_idx());
        }
        break;
    case MENU_ACTION_REFRESH:
        refresh_current_submenu_page(action.keep_index);
        break;
    case MENU_ACTION_SHOW_CONFIRM:
        screen_show_modal_setup_confirm(action.modal_text);
        ui_state_set_state(UI_MODAL_SETUP_CONFIRM);
        break;
    case MENU_ACTION_BEGIN_EDIT:
        screen_begin_edit_value(item_idx, &action.edit_spec);
        break;
    case MENU_ACTION_SHOW_GAS_MODAL:
        screen_show_modal_gas();
        ui_state_set_state(UI_MODAL_GAS);
        break;
    case MENU_ACTION_SHOW_TEXT_MODAL:
        screen_show_modal_act(action.modal_text);
        break;
    default:
        break;
    }
}

void screen_close_submenu(void)
{
    if (!s_submenu_layer || !s_submenu_title || !s_submenu_list)
    {
        ui_state_set_sub_history_depth(0U);
        ui_state_set_sub_item_count(0U);
        ui_state_set_sub_menu_idx(0U);
        ui_state_set_edit_active(false);
        ui_state_set_gas_modal_from_submenu(false);
        logbook_points_release();
        ui_state_set_state(ui_state_get_sub_parent());
        return;
    }

    if (menu_runtime_back())
    {
        uint8_t count = 0;
        (void)menu_runtime_current_rows(&count);
        if (count > 0)
        {
            submenu_populate_current();
            ui_state_set_sub_item_count(count);
            if (ui_state_get_sub_menu_idx() >= count)
            {
                ui_state_set_sub_menu_idx((uint8_t)(count - 1U));
            }
            screen_set_submenu_selection(ui_state_get_sub_menu_idx());
        }
        ui_state_set_sub_history_depth(menu_runtime_stack_depth());
        ui_state_set_state(UI_SUB_MENU);
        return;
    }
    submenu_slide_out();
    menu_runtime_reset();
    menu_actions_clear_pending();
    logbook_points_release();
    logbook_detail_stop_loader();
    ui_state_set_sub_history_depth(0U);
    ui_state_set_sub_item_count(0U);
    ui_state_set_sub_menu_idx(0U);
    ui_state_set_state(ui_state_get_sub_parent());
}

void screen_confirm_submenu_setting(void)
{
    bool close_extra_mode_layer = false;
    bool return_dash_after_apply = false;
    if (!menu_actions_confirm_pending(&close_extra_mode_layer, &return_dash_after_apply))
    {
        screen_hide_modal();
        ui_state_set_state(UI_SUB_MENU);
        return;
    }
    screen_hide_modal();
    if (return_dash_after_apply)
    {
        ui_state_set_sub_history_depth(0U);
        ui_state_set_edit_active(false);
        logbook_points_release();
        logbook_detail_stop_loader();
        menu_runtime_reset();
        menu_actions_clear_pending();
        submenu_slide_out();
        ui_state_set_state(UI_DASH);
        return;
    }
    screen_close_submenu();
    if (close_extra_mode_layer)
    {
        screen_close_submenu();
    }
}

void screen_cancel_submenu_setting(void)
{
    menu_actions_clear_pending();
    screen_hide_modal();
    ui_state_set_state(UI_SUB_MENU);
}
