/*
 * 文件: src/ui/views/submenu_view.c
 * 作用: 该文件属于视图表现模块，负责菜单、子菜单、模态框或特定视图状态的展示与交互联动。
 * 说明: 本文件位于 UI 视图层，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "submenu_view.h"

#include "../../config/build/ui_debug_flags.h"
#include "../core/callbacks.h"
#include "../core/data.h"
#include "../ports/logbook_io_port.h"
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
static lv_obj_t *s_submenu_hint = NULL;
static lv_obj_t *s_submenu_title_line = NULL;
static lv_obj_t *s_submenu_list = NULL;
static lv_obj_t *s_light_status_lbl = NULL;
static uint16_t s_submenu_width = 0;
static uint16_t s_submenu_height = 0;
static ui_vm_dive_plan_view_t s_dive_plan_last_vm __attribute__((section(".psram_bss")));
static bool s_dive_plan_last_vm_valid = false;
static bool s_submenu_selection_scroll_silent = false;
static lv_coord_t s_submenu_target_scroll_y = 0;
static bool s_light_color_preview_active = false;
static uint32_t s_light_color_preview_original_rgb = 0xFFFFFFUL;
static uint16_t s_light_color_preview_hue = 0U;
static lv_obj_t *s_light_color_title_lbl = NULL;
static lv_obj_t *s_light_color_progress_lbl = NULL;
static lv_obj_t *s_light_color_swatch = NULL;
static lv_obj_t *s_light_color_bar_fill = NULL;
static lv_obj_t *s_light_color_cursor = NULL;
static uint8_t s_submenu_selected_idx = 0xFFU;

enum
{
    LIGHT_COLOR_HUE_MAX = 360U,
    LIGHT_COLOR_HUE_STEP_DEG = 6U,
    LIGHT_COLOR_BAR_PAD_X = 5U,
    LIGHT_COLOR_BAR_PAD_Y = 7U,
    LIGHT_COLOR_BAR_FILL_H = 10U,
    LIGHT_COLOR_BAR_TICK_COUNT = 10U,
    LIGHT_COLOR_ROTARY_ROW_INDEX = 0U,
};

static bool normalize_rotate_steps(int8_t steps, int8_t *out_dir, uint8_t *out_count)
{
    int16_t signed_steps = (int16_t)steps;
    int8_t dir = (signed_steps > 0) ? 1 : ((signed_steps < 0) ? -1 : 0);
    uint8_t count = (uint8_t)((signed_steps < 0) ? -signed_steps : signed_steps);

    if (dir == 0 || count == 0U)
    {
        return false;
    }
    if (out_dir)
    {
        *out_dir = dir;
    }
    if (out_count)
    {
        *out_count = count;
    }
    return true;
}

static uint32_t light_rgb_make(uint8_t r, uint8_t g, uint8_t b)
{
    return (((uint32_t)r) << 16) | (((uint32_t)g) << 8) | (uint32_t)b;
}

static uint32_t light_color_rgb_from_hue(uint16_t hue)
{
    uint16_t h = (uint16_t)(hue % LIGHT_COLOR_HUE_MAX);
    uint8_t sector = (uint8_t)(h / 60U);
    uint8_t offset = (uint8_t)(h % 60U);
    uint8_t rising = (uint8_t)(((uint16_t)offset * 255U) / 60U);
    uint8_t falling = (uint8_t)(255U - rising);

    switch (sector)
    {
    case 0U: return light_rgb_make(255U, rising, 0U);
    case 1U: return light_rgb_make(falling, 255U, 0U);
    case 2U: return light_rgb_make(0U, 255U, rising);
    case 3U: return light_rgb_make(0U, falling, 255U);
    case 4U: return light_rgb_make(rising, 0U, 255U);
    default: return light_rgb_make(255U, 0U, falling);
    }
}

static uint16_t light_color_hue_from_rgb(uint32_t rgb)
{
    uint8_t r = (uint8_t)((rgb >> 16) & 0xFFU);
    uint8_t g = (uint8_t)((rgb >> 8) & 0xFFU);
    uint8_t b = (uint8_t)(rgb & 0xFFU);
    uint8_t max = r;
    uint8_t min = r;
    int32_t hue;
    uint8_t delta;

    if (g > max) max = g;
    if (b > max) max = b;
    if (g < min) min = g;
    if (b < min) min = b;

    delta = (uint8_t)(max - min);
    if (delta == 0U)
    {
        return 0U;
    }

    if (max == r)
    {
        hue = (60L * ((int32_t)g - (int32_t)b)) / (int32_t)delta;
    }
    else if (max == g)
    {
        hue = 120L + (60L * ((int32_t)b - (int32_t)r)) / (int32_t)delta;
    }
    else
    {
        hue = 240L + (60L * ((int32_t)r - (int32_t)g)) / (int32_t)delta;
    }

    while (hue < 0L) hue += LIGHT_COLOR_HUE_MAX;
    while (hue >= (int32_t)LIGHT_COLOR_HUE_MAX) hue -= LIGHT_COLOR_HUE_MAX;
    return (uint16_t)hue;
}

static const char *light_color_name_from_hue(uint16_t hue)
{
    uint16_t h = (uint16_t)(hue % LIGHT_COLOR_HUE_MAX);

    if (h < 15U || h >= 345U) return "RED";
    if (h < 45U) return "ORANGE";
    if (h < 75U) return "YELLOW";
    if (h < 105U) return "LIME";
    if (h < 150U) return "GREEN";
    if (h < 195U) return "CYAN";
    if (h < 255U) return "BLUE";
    if (h < 285U) return "PURPLE";
    if (h < 315U) return "MAGENTA";
    return "PINK";
}

static uint8_t light_color_step_index_from_hue(uint16_t hue)
{
    uint16_t h = (uint16_t)(hue % LIGHT_COLOR_HUE_MAX);
    uint8_t step = (uint8_t)((h + (LIGHT_COLOR_HUE_STEP_DEG / 2U)) / LIGHT_COLOR_HUE_STEP_DEG);
    const uint8_t step_count = (uint8_t)(LIGHT_COLOR_HUE_MAX / LIGHT_COLOR_HUE_STEP_DEG);

    if (step >= step_count)
    {
        step = 0U;
    }
    return (uint8_t)(step + 1U);
}

static lv_color_t light_color_progress_green(uint8_t step, uint8_t step_count)
{
    uint16_t level;

    if (step_count == 0U)
    {
        return lv_color_make(0x00, 0xFF, 0x00);
    }

    level = (uint16_t)(0x40U + (((uint16_t)step * 0xBFU) / step_count));
    if (level > 0xFFU)
    {
        level = 0xFFU;
    }
    return lv_color_make(0x00, (uint8_t)level, 0x00);
}

static uint16_t light_color_bar_outer_w(uint16_t w)
{
    return (w > 96U) ? (uint16_t)(w - 96U) : (uint16_t)(w - 24U);
}

static uint16_t light_color_bar_inner_w(uint16_t outer_w)
{
    return (outer_w > (LIGHT_COLOR_BAR_PAD_X * 2U)) ? (uint16_t)(outer_w - (LIGHT_COLOR_BAR_PAD_X * 2U)) : 1U;
}

static bool submenu_current_menu_is_readonly_info(void)
{
    return menu_defs_is_readonly_menu(menu_runtime_current_id());
}

static dirty_mask_t submenu_info_refresh_mask_for_current_menu(void)
{
    /* INFO 子菜单以前对 DIRTY_INFO_REFRESH_MASK 全量响应，停在 LAST DIVE 时
     * 罗盘/传感器高频 dirty 也会触发 lv_obj_clean + 重建列表。按当前页订阅
     * 真正使用的数据域，可以把旋转/后台刷新期间的无关重建降下来。 */
    switch (menu_runtime_current_id())
    {
    case MENU_INFO_LAST_DIVE:
        /*
         * LAST DIVE 展示的是已落盘/缓存的上一潜摘要，不应跟随当前潜水的
         * depth/dive_time/ascent 等 DIRTY_DIVE_PROFILE 高频刷新。否则用户
         * 在 LAST DIVE 页面快速旋转或模拟潜水时，会把只读列表反复重建进
         * lv_task_handler()。水面态保留 DIRTY_DIVE_PROFILE，是为了让 SURFACE
         * 时间继续正常刷新；潜水/出水确认窗口只保留系统/日志触发。
         */
        return (bus_get_dive_lifecycle_phase() == DIVE_LIFECYCLE_SURFACE_CONFIRMED)
               ? (DIRTY_DIVE_PROFILE | DIRTY_LOGBOOK | DIRTY_SYSTEM)
               : (DIRTY_LOGBOOK | DIRTY_SYSTEM);
    case MENU_INFO_DIVE_PLAN:
        return DIRTY_PLAN | DIRTY_DIVE_PROFILE | DIRTY_DIVE_CONFIG | DIRTY_GAS_SUPPLY;
    case MENU_INFO_TISSUE_TOX:
        return DIRTY_TISSUE_TOX | DIRTY_DIVE_CONFIG;
    case MENU_INFO_GAS_CALC:
        return DIRTY_GAS_SUPPLY | DIRTY_DIVE_CONFIG;
    case MENU_INFO_SENSOR_DEVICE:
        return DIRTY_SYSTEM | DIRTY_GAS_SUPPLY;
    case MENU_INFO_DIVE_LOG:
        return DIRTY_LOGBOOK;
    default:
        return DIRTY_INFO_REFRESH_MASK;
    }
}

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
    logbook_record_key_t record_key;
    logbook_meta_t meta;
    uint8_t recovered;
    uint8_t abnormal_end;
} logbook_picker_item_t;

enum
{
    LOGBOOK_PICKER_VISIBLE_ROWS = 5U,
    LOGBOOK_SUMMARY_RETRY_MAX = 5U,
    LOGBOOK_SUMMARY_RETRY_BASE_MS = 100U,
    LOGBOOK_LOADER_FOREGROUND_PERIOD_MS = 8U,
    LOGBOOK_LOADER_BACKGROUND_PERIOD_MS = 32U,
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
static lv_timer_t *s_logbook_retry_timer = NULL;
static uint16_t s_logbook_load_page = 0U;
static uint8_t s_logbook_load_offset = 0U;
static uint8_t s_logbook_load_retry_count = 0U;
static bool s_logbook_load_background = false;
static bool s_logbook_load_page_dirty = false;
static uint16_t s_logbook_detail_backend_index = 0U;
static uint16_t s_logbook_detail_display_index = 0U;
static logbook_record_key_t s_logbook_detail_record_key = LOGBOOK_INVALID_RECORD_KEY;
#ifndef PC_SIMULATOR
static uint32_t s_logbook_detail_request_id = 0U;
#endif
static bool s_logbook_detail_loading = false;
static bool s_logbook_detail_load_failed = false;
static bool s_logbook_picker_refresh_pending = false;
static bool s_logbook_picker_view_ready = false;
static bool s_logbook_loading = false;
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
static lv_obj_t *s_logbook_detail_roots[3];
static lv_obj_t *s_logbook_detail_parent = NULL;
static uint16_t s_logbook_detail_view_w = 0U;
static uint16_t s_logbook_detail_view_h = 0U;
static uint32_t s_logbook_detail_view_gen = 1U;
static uint32_t s_logbook_detail_cache_gen = 0U;
static bool s_logbook_detail_direct_content_active = false;
/* 只表示列表已确认至少存在一条日志；详情失败不得修改该状态。 */
static bool s_logbook_has_logs = false;
static bool s_logbook_points_loaded = false;

enum {
    LOGBOOK_EMPTY_CONFIRM_UNKNOWN = 0U,
    LOGBOOK_EMPTY_CONFIRM_PENDING = 1U,
    LOGBOOK_EMPTY_CONFIRM_DONE = 2U,
    LOGBOOK_EMPTY_CONFIRM_RETRY_MAX = 4U,
};
static uint8_t s_logbook_empty_confirm_state = LOGBOOK_EMPTY_CONFIRM_UNKNOWN;
static uint8_t s_logbook_empty_confirm_retry_count = 0U;

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
static uint16_t logbook_display_to_backend_index(uint16_t display_index);
static bool logbook_picker_load_summary(uint16_t index,
                                        logbook_entry_t *out_entry,
                                        logbook_record_key_t *out_record_key);
static void logbook_picker_item_from_entry(logbook_picker_item_t *dst,
                                           const logbook_entry_t *src,
                                           logbook_record_key_t record_key);
static void logbook_entry_from_picker_item(logbook_entry_t *dst, const logbook_picker_item_t *src);
static uint8_t logbook_picker_focus_to_index(uint8_t focus, uint8_t visible_count);
static bool logbook_picker_has_back(void);
static bool logbook_picker_has_next(void);
static uint8_t logbook_picker_focus_back(void);
static uint8_t logbook_picker_focus_next(void);
static uint8_t logbook_picker_focus_count(void);
static void logbook_picker_focus_default(void);
static void logbook_picker_sync_focus(void);
static void submenu_populate_current(void);
static void logbook_load_current(void);
static void logbook_picker_stop_loader(void);
static bool logbook_picker_page_has_missing(uint16_t page_index);
static void logbook_picker_start_loader(uint16_t page_index, bool background);
static void logbook_detail_stop_loader(void);
static void logbook_detail_start_loader(uint16_t index);
static void logbook_retry_stop(void);
static void logbook_retry_start(void);
static void logbook_detail_views_invalidate(void);
static void logbook_detail_views_reset(void);
static void logbook_picker_view_reset(void);
static void logbook_picker_view_forget(void);
static void logbook_picker_load_timer_cb(lv_timer_t *timer);
static void logbook_detail_load_timer_cb(lv_timer_t *timer);
static void logbook_retry_timer_cb(lv_timer_t *timer);
static void light_color_preview_clear_refs(void);
static void light_color_preview_cancel_if_active(void);

static void logbook_points_release(void)
{
    if (s_logbook_points == NULL)
    {
        return;
    }

#ifdef PC_SIMULATOR
    logbook_backend_release_samples(s_logbook_points);
#else
    logbook_io_release_points(s_logbook_points);
#endif
    s_logbook_points = NULL;
    s_logbook_point_count = 0U;
    s_logbook_points_loaded = false;
    logbook_detail_views_invalidate();
}

static logbook_record_key_t logbook_record_key_for_display_index(uint16_t index)
{
    if (s_logbook_snapshot_entries == NULL || index >= s_logbook_snapshot_count)
    {
        return LOGBOOK_INVALID_RECORD_KEY;
    }
    return s_logbook_snapshot_entries[index].record_key;
}

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
    s_logbook_detail_backend_index = 0U;
    s_logbook_detail_display_index = 0U;
    s_logbook_detail_record_key = LOGBOOK_INVALID_RECORD_KEY;
    logbook_detail_views_reset();
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

static uint16_t logbook_display_to_backend_index(uint16_t display_index)
{
    if ((s_logbook_snapshot_count == 0U) || (display_index >= s_logbook_snapshot_count))
    {
        return display_index;
    }
    return (uint16_t)(s_logbook_snapshot_count - 1U - display_index);
}

static bool logbook_picker_load_summary(uint16_t index,
                                        logbook_entry_t *out_entry,
                                        logbook_record_key_t *out_record_key)
{
    if (out_entry == NULL || out_record_key == NULL)
    {
        return false;
    }

#ifdef PC_SIMULATOR
    *out_record_key = LOGBOOK_INVALID_RECORD_KEY;
    return logbook_backend_get_summary(logbook_display_to_backend_index(index), out_entry);
#else
    return logbook_io_get_summary(logbook_display_to_backend_index(index),
                                  out_entry,
                                  out_record_key);
#endif
}

static void logbook_picker_item_from_entry(logbook_picker_item_t *dst,
                                           const logbook_entry_t *src,
                                           logbook_record_key_t record_key)
{
    if ((dst == NULL) || (src == NULL))
    {
        return;
    }

    dst->valid = src->valid;
    dst->record_key = record_key;
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
    s_logbook_load_retry_count = 0U;
    s_logbook_load_background = false;
    s_logbook_load_page_dirty = false;
}

static void logbook_detail_finish_loader(bool load_failed)
{
    bool state_changed = (s_logbook_detail_loading != false) ||
                         (s_logbook_detail_load_failed != load_failed);

    if (s_logbook_detail_timer != NULL)
    {
        lv_timer_del(s_logbook_detail_timer);
        s_logbook_detail_timer = NULL;
    }
#ifndef PC_SIMULATOR
    if (s_logbook_detail_request_id != 0U)
    {
        logbook_io_cancel_detail(s_logbook_detail_request_id);
        s_logbook_detail_request_id = 0U;
    }
#endif
    s_logbook_detail_loading = false;
    s_logbook_detail_load_failed = load_failed;
    if (state_changed)
    {
        logbook_detail_views_invalidate();
    }
}
static void logbook_detail_stop_loader(void)
{
    logbook_detail_finish_loader(false);
}

static void logbook_retry_stop(void)
{
    if (s_logbook_retry_timer != NULL)
    {
        lv_timer_del(s_logbook_retry_timer);
        s_logbook_retry_timer = NULL;
    }
}

static void logbook_retry_start(void)
{
    if (!s_logbook_loading)
    {
        logbook_retry_stop();
        return;
    }

    if (s_logbook_retry_timer == NULL)
    {
        s_logbook_retry_timer = lv_timer_create(logbook_retry_timer_cb, 500U, NULL);
    }
}

static void logbook_detail_start_loader(uint16_t index)
{
    logbook_detail_stop_loader();
    s_logbook_detail_backend_index = logbook_display_to_backend_index(index);
    s_logbook_detail_display_index = index;
    s_logbook_detail_record_key = logbook_record_key_for_display_index(index);
    s_logbook_detail_loading = true;
    s_logbook_detail_load_failed = false;
    logbook_detail_views_invalidate();

#ifndef PC_SIMULATOR
    if (s_logbook_detail_record_key == LOGBOOK_INVALID_RECORD_KEY ||
        !logbook_io_request_detail(s_logbook_detail_record_key,
                                   &s_logbook_detail_request_id))
    {
        logbook_detail_finish_loader(true);
        return;
    }
#endif

    s_logbook_detail_timer = lv_timer_create(logbook_detail_load_timer_cb, 10U, NULL);
    if (s_logbook_detail_timer == NULL)
    {
        logbook_detail_finish_loader(true);
    }
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

    if (logbook_picker_page_has_missing(page_index))
    {
        /* count 已可信但单条 summary 仍可能遇到 dlogfs 短暂繁忙；当前页优先有限重试。 */
        logbook_picker_start_loader(page_index, false);
    }
    else if ((page_index + 1U) < s_logbook_page_count)
    {
        logbook_picker_start_loader((uint16_t)(page_index + 1U), true);
    }
}

static void logbook_picker_restore_focus(uint16_t index)
{
    if (s_logbook_snapshot_count == 0U || s_logbook_page_count == 0U)
    {
        s_logbook_page_index = 0U;
        s_logbook_picker_focus = 0U;
        s_logbook_focus = 0U;
        return;
    }

    if (index >= s_logbook_snapshot_count)
    {
        index = (uint16_t)(s_logbook_snapshot_count - 1U);
    }

    s_logbook_page_index = (uint16_t)(index / LOGBOOK_PICKER_VISIBLE_ROWS);
    if (s_logbook_page_index >= s_logbook_page_count)
    {
        s_logbook_page_index = (uint16_t)(s_logbook_page_count - 1U);
    }

    logbook_picker_prepare_page(s_logbook_page_index);
    s_logbook_picker_focus = (uint8_t)(index - s_logbook_page_index * LOGBOOK_PICKER_VISIBLE_ROWS);
    if (s_logbook_picker_focus >= s_logbook_picker_visible && s_logbook_picker_visible > 0U)
    {
        s_logbook_picker_focus = (uint8_t)(s_logbook_picker_visible - 1U);
    }
    logbook_picker_sync_focus();
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

static void logbook_picker_start_loader(uint16_t page_index, bool background)
{
    if (page_index >= s_logbook_page_count)
    {
        logbook_picker_stop_loader();
        return;
    }

    if (background && !logbook_picker_page_has_missing(page_index))
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
    s_logbook_load_retry_count = 0U;
    s_logbook_load_background = background;
    s_logbook_load_page_dirty = false;
    if (s_logbook_load_timer == NULL)
    {
        s_logbook_load_timer = lv_timer_create(logbook_picker_load_timer_cb,
                                               background ? LOGBOOK_LOADER_BACKGROUND_PERIOD_MS :
                                                            LOGBOOK_LOADER_FOREGROUND_PERIOD_MS,
                                               NULL);
    }
    else
    {
        lv_timer_set_period(s_logbook_load_timer,
                            background ? LOGBOOK_LOADER_BACKGROUND_PERIOD_MS :
                                         LOGBOOK_LOADER_FOREGROUND_PERIOD_MS);
        lv_timer_ready(s_logbook_load_timer);
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
        logbook_record_key_t record_key = LOGBOOK_INVALID_RECORD_KEY;
        if (logbook_picker_load_summary(index, &entry, &record_key))
        {
            logbook_picker_item_from_entry(&s_logbook_snapshot_entries[index], &entry, record_key);
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

static void logbook_delete_cached_obj(lv_obj_t **obj_ref)
{
    if ((obj_ref == NULL) || (*obj_ref == NULL))
    {
        return;
    }

    /* picker 页会缓存多个 LVGL 对象用于快速翻页。页面切换时父容器可能已被清空，
     * 删除前必须确认对象仍挂在 LVGL 树上，避免 stale 指针再次进入 lv_obj_del()。 */
    if (lv_obj_is_valid(*obj_ref))
    {
        lv_obj_del(*obj_ref);
    }
    *obj_ref = NULL;
}

static int8_t logbook_detail_page_to_root_index(logbook_page_t page)
{
    switch (page)
    {
    case LOGBOOK_PAGE_SUMMARY:
        return 0;
    case LOGBOOK_PAGE_DETAIL_1:
        return 1;
    case LOGBOOK_PAGE_DETAIL_2:
        return 2;
    default:
        return -1;
    }
}

static void logbook_detail_views_forget(void)
{
    for (uint8_t i = 0U; i < 3U; ++i)
    {
        s_logbook_detail_roots[i] = NULL;
    }
    s_logbook_detail_parent = NULL;
    s_logbook_detail_view_w = 0U;
    s_logbook_detail_view_h = 0U;
    s_logbook_detail_cache_gen = 0U;
    s_logbook_detail_direct_content_active = false;
}

static void logbook_detail_views_reset(void)
{
    if (s_logbook_detail_direct_content_active)
    {
        if (s_logbook_detail_parent != NULL && lv_obj_is_valid(s_logbook_detail_parent))
        {
            lv_obj_clean(s_logbook_detail_parent);
        }
        logbook_detail_views_forget();
        return;
    }

    for (uint8_t i = 0U; i < 3U; ++i)
    {
        logbook_delete_cached_obj(&s_logbook_detail_roots[i]);
    }
    logbook_detail_views_forget();
}

static void logbook_detail_views_invalidate(void)
{
    s_logbook_detail_view_gen++;
    if (s_logbook_detail_view_gen == 0U)
    {
        s_logbook_detail_view_gen = 1U;
    }
}

static bool logbook_detail_views_parent_valid(lv_obj_t *parent)
{
    if (parent == NULL || !lv_obj_is_valid(parent))
    {
        return false;
    }

    for (uint8_t i = 0U; i < 3U; ++i)
    {
        lv_obj_t *root = s_logbook_detail_roots[i];
        if (root == NULL)
        {
            continue;
        }
        if (!lv_obj_is_valid(root) || (lv_obj_get_parent(root) != parent))
        {
            return false;
        }
    }
    return true;
}

static lv_obj_t *logbook_detail_views_create_root(lv_obj_t *parent, uint16_t w, uint16_t h)
{
    lv_obj_t *root = lv_obj_create(parent);
    if (root == NULL)
    {
        return NULL;
    }

    lv_obj_remove_style_all(root);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_size(root, w, h);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    return root;
}

static void logbook_picker_view_forget(void)
{
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

static void logbook_picker_view_reset(void)
{
    for (uint8_t i = 0U; i < LOGBOOK_PICKER_VISIBLE_ROWS; ++i)
    {
        logbook_delete_cached_obj(&s_logbook_picker_rows[i]);
    }
    logbook_delete_cached_obj(&s_logbook_picker_title);
    logbook_delete_cached_obj(&s_logbook_picker_count);
    logbook_delete_cached_obj(&s_logbook_picker_page);
    logbook_delete_cached_obj(&s_logbook_picker_back);
    logbook_delete_cached_obj(&s_logbook_picker_next);
    logbook_picker_view_forget();
}

static void logbook_picker_load_timer_cb(lv_timer_t *timer)
{
    if ((s_logbook_snapshot_entries == NULL) || !menu_runtime_is_logbook() || (s_logbook_page != LOGBOOK_PAGE_PICK))
    {
        logbook_picker_stop_loader();
        return;
    }

    if (!s_logbook_load_background &&
        s_logbook_load_retry_count > 0U &&
        s_logbook_load_offset == 0U)
    {
        /* 退避只发生在两轮扫描之间；进入新一轮后仍快速跳过已经成功的行。 */
        lv_timer_set_period(timer, LOGBOOK_LOADER_FOREGROUND_PERIOD_MS);
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
        if (s_logbook_load_page_dirty && !s_logbook_load_background &&
            (s_logbook_load_page == s_logbook_page_index))
        {
            s_logbook_load_page_dirty = false;
            submenu_populate_current();
        }

        if (!s_logbook_load_background &&
            logbook_picker_page_has_missing(s_logbook_load_page) &&
            s_logbook_load_retry_count < LOGBOOK_SUMMARY_RETRY_MAX)
        {
            s_logbook_load_retry_count++;
            s_logbook_load_offset = 0U;
            lv_timer_set_period(s_logbook_load_timer,
                                LOGBOOK_SUMMARY_RETRY_BASE_MS * s_logbook_load_retry_count);
            return;
        }

        if (!s_logbook_load_background)
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
        logbook_record_key_t record_key = LOGBOOK_INVALID_RECORD_KEY;
        if (logbook_picker_load_summary(index, &entry, &record_key))
        {
            logbook_picker_item_from_entry(&s_logbook_snapshot_entries[index], &entry, record_key);
            if (!s_logbook_load_background && (s_logbook_load_page == s_logbook_page_index))
            {
                s_logbook_load_page_dirty = true;
            }
        }
    }
    s_logbook_load_offset++;
}

static void logbook_detail_load_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!s_logbook_detail_loading ||
        !menu_runtime_is_logbook() ||
        s_logbook_page == LOGBOOK_PAGE_PICK)
    {
        logbook_detail_stop_loader();
        return;
    }

#ifdef PC_SIMULATOR
    logbook_entry_t detail_entry;
    bool profile_loaded = s_logbook_points_loaded;

    memset(&detail_entry, 0, sizeof(detail_entry));
    if (logbook_backend_get_detail(s_logbook_detail_backend_index, &detail_entry) && detail_entry.valid)
    {
        s_logbook_entry = detail_entry;
        if (!profile_loaded)
        {
            const bool acquired = logbook_backend_acquire_samples(s_logbook_detail_backend_index,
                                                                  &s_logbook_points,
                                                                  &s_logbook_point_count);
            profile_loaded = acquired && s_logbook_points != NULL && s_logbook_point_count > 0U;
            if (!profile_loaded && s_logbook_points != NULL)
            {
                logbook_backend_release_samples(s_logbook_points);
                s_logbook_points = NULL;
                s_logbook_point_count = 0U;
            }
            s_logbook_points_loaded = profile_loaded;
        }
        logbook_detail_finish_loader(!profile_loaded);
    }
    else
    {
        /* PC 内存后端的一条详情失败也不能反向宣告整个日志库为空。 */
        logbook_detail_finish_loader(true);
    }
#else
    logbook_io_result_t result;

    memset(&result, 0, sizeof(result));
    if (!logbook_io_take_detail(s_logbook_detail_request_id, &result))
    {
        return;
    }

    if (result.record_key != s_logbook_detail_record_key)
    {
        if (result.points != NULL)
        {
            logbook_io_release_points(result.points);
        }
        logbook_detail_stop_loader();
        return;
    }

    if ((result.status == LOGBOOK_IO_READY ||
         result.status == LOGBOOK_IO_PROFILE_UNAVAILABLE) &&
        result.entry.valid)
    {
        s_logbook_entry = result.entry;
        if (result.points != NULL && result.point_count > 0U)
        {
            logbook_points_release();
            s_logbook_points = result.points;
            s_logbook_point_count = result.point_count;
            s_logbook_points_loaded = true;
            result.points = NULL;
        }
        logbook_detail_finish_loader(result.status == LOGBOOK_IO_PROFILE_UNAVAILABLE &&
                                     !s_logbook_points_loaded);
    }
    else
    {
        /* 列表 summary 已证明记录存在；详情失败只影响当前记录，绝不能显示 NO LOGS。 */
        logbook_detail_finish_loader(true);
    }

    if (result.points != NULL)
    {
        logbook_io_release_points(result.points);
    }
#endif

    if (menu_runtime_is_logbook() &&
        s_logbook_page != LOGBOOK_PAGE_PICK &&
        s_logbook_index == s_logbook_detail_display_index)
    {
        submenu_populate_current();
    }
}
static void logbook_retry_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!menu_runtime_is_logbook() || (s_logbook_page != LOGBOOK_PAGE_PICK))
    {
        logbook_retry_stop();
        return;
    }

    if (!s_logbook_loading)
    {
        logbook_retry_stop();
        return;
    }

    logbook_load_current();
    submenu_populate_current();
    if (!s_logbook_loading)
    {
        logbook_retry_stop();
    }
}

static bool logbook_picker_snapshot_refresh(void)
{
    logbook_picker_cache_invalidate();
    s_logbook_loading = !logbook_backend_is_ready();
    if (s_logbook_loading)
    {
        s_logbook_empty_confirm_state = LOGBOOK_EMPTY_CONFIRM_UNKNOWN;
        s_logbook_empty_confirm_retry_count = 0U;
        return false;
    }

    uint16_t count = 0U;
    if (!logbook_backend_count_ex(&count))
    {
        /* 后端 ready 只代表 SD/FAT 可访问，不代表索引已经可信。
         * 旧存档首次升级、空索引重建或短暂目录枚举失败时，count 查询失败
         * 必须保持 LOADING 并重试，不能落成 NO LOGS。 */
        s_logbook_loading = true;
        s_logbook_empty_confirm_state = LOGBOOK_EMPTY_CONFIRM_UNKNOWN;
        s_logbook_empty_confirm_retry_count = 0U;
        return false;
    }

    if (count == 0U)
    {
        if (s_logbook_empty_confirm_state != LOGBOOK_EMPTY_CONFIRM_DONE &&
            s_logbook_empty_confirm_retry_count < LOGBOOK_EMPTY_CONFIRM_RETRY_MAX)
        {
            /* FAT 首次枚举、旧索引重建刚完成或后端空缓存复核期间，0 条是最容易
             * 误伤用户的可见结论。用有界重试覆盖 1~2s 的挂载/目录恢复抖动，
             * 超出窗口后才显示 NO LOGS，避免现场永久 LOADING。 */
            s_logbook_empty_confirm_state = LOGBOOK_EMPTY_CONFIRM_PENDING;
            s_logbook_empty_confirm_retry_count++;
            s_logbook_loading = true;
            return false;
        }
        s_logbook_empty_confirm_state = LOGBOOK_EMPTY_CONFIRM_DONE;
        s_logbook_loading = false;
        return false;
    }

    s_logbook_empty_confirm_state = LOGBOOK_EMPTY_CONFIRM_UNKNOWN;
    s_logbook_empty_confirm_retry_count = 0U;
    if (!logbook_picker_cache_reserve(count))
    {
        s_logbook_loading = true;
        return false;
    }
    s_logbook_loading = false;
    s_logbook_picker_visible = (count > LOGBOOK_PICKER_VISIBLE_ROWS) ? LOGBOOK_PICKER_VISIBLE_ROWS : (uint8_t)count;
    logbook_picker_prepare_page(s_logbook_page_index);
    logbook_picker_focus_default();
    return (s_logbook_snapshot_count > 0U);
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

    s_logbook_has_logs = false;
    logbook_points_release();
    s_logbook_has_logs = logbook_picker_snapshot_refresh();
    if (s_logbook_loading)
    {
        logbook_retry_start();
    }
    else
    {
        logbook_retry_stop();
    }
    if (s_logbook_has_logs)
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
    s_logbook_picker_refresh_pending = false;
    s_logbook_page_index = 0U;
    s_logbook_picker_focus = 0U;
    s_logbook_focus = 0U;
    s_logbook_index = 0U;
    logbook_picker_stop_loader();
    logbook_detail_stop_loader();
    logbook_retry_stop();
    logbook_picker_cache_invalidate();
    s_logbook_empty_confirm_state = LOGBOOK_EMPTY_CONFIRM_UNKNOWN;
    s_logbook_empty_confirm_retry_count = 0U;
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

static void logbook_label_set_text_if_changed(lv_obj_t *label, const char *text)
{
    const char *next = (text != NULL) ? text : "";
    const char *old = NULL;

    if (label == NULL)
    {
        return;
    }

    old = lv_label_get_text(label);
    if ((old == NULL) || (strcmp(old, next) != 0))
    {
        lv_label_set_text(label, next);
    }
}

static void logbook_set_hidden_if_changed(lv_obj_t *obj, bool hidden)
{
    if (obj == NULL)
    {
        return;
    }

    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) == hidden)
    {
        return;
    }
    if (hidden)
    {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static void logbook_apply_row_style(lv_obj_t *row, lv_obj_t *left_lbl, lv_obj_t *right_lbl,
                                    bool focused, uint16_t h)
{
    font_id_t row_font = focused ? FONT_ID_TITLE : FONT_ID_SMALL;
    lv_color_t border_color = focused ? GREEN : DARK;
    lv_coord_t border_width = focused ? 2 : 1;
    const lv_font_t *font = get_font(row_font);
    lv_color_t color = focused ? LIGHT : GREEN;
    (void)h;

    if (row != NULL)
    {
        lv_obj_set_style_bg_color(row, BLACK, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        if (lv_obj_get_style_border_color(row, LV_PART_MAIN).full != border_color.full)
        {
            lv_obj_set_style_border_color(row, border_color, 0);
        }
        if (lv_obj_get_style_border_width(row, LV_PART_MAIN) != border_width)
        {
            lv_obj_set_style_border_width(row, border_width, 0);
        }
    }
    if (left_lbl != NULL)
    {
        if (lv_obj_get_style_text_font(left_lbl, LV_PART_MAIN) != font)
        {
            lv_obj_set_style_text_font(left_lbl, font, 0);
        }
        if (lv_obj_get_style_text_color(left_lbl, LV_PART_MAIN).full != color.full)
        {
            lv_obj_set_style_text_color(left_lbl, color, 0);
        }
    }
    if (right_lbl != NULL)
    {
        if (lv_obj_get_style_text_font(right_lbl, LV_PART_MAIN) != font)
        {
            lv_obj_set_style_text_font(right_lbl, font, 0);
        }
        if (lv_obj_get_style_text_color(right_lbl, LV_PART_MAIN).full != color.full)
        {
            lv_obj_set_style_text_color(right_lbl, color, 0);
        }
    }
}

static void logbook_apply_nav_button_style(lv_obj_t *btn, lv_obj_t *lbl, bool focused)
{
    lv_color_t border_color = focused ? GREEN : DARK;
    lv_coord_t border_width = focused ? INNER_BORDER_W + 2 : INNER_BORDER_W;
    const lv_font_t *font = get_font(focused ? FONT_ID_MEDIUM : FONT_ID_TITLE);
    lv_color_t color = focused ? LIGHT : GREEN;

    if (btn != NULL)
    {
        lv_obj_set_style_bg_color(btn, BLACK, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        if (lv_obj_get_style_border_color(btn, LV_PART_MAIN).full != border_color.full)
        {
            lv_obj_set_style_border_color(btn, border_color, 0);
        }
        if (lv_obj_get_style_border_width(btn, LV_PART_MAIN) != border_width)
        {
            lv_obj_set_style_border_width(btn, border_width, 0);
        }
        lv_obj_set_style_radius(btn, 0, 0);
    }
    if (lbl != NULL)
    {
        if (lv_obj_get_style_text_font(lbl, LV_PART_MAIN) != font)
        {
            lv_obj_set_style_text_font(lbl, font, 0);
        }
        if (lv_obj_get_style_text_color(lbl, LV_PART_MAIN).full != color.full)
        {
            lv_obj_set_style_text_color(lbl, color, 0);
        }
        lv_obj_set_size(lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
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

    if (s_logbook_points_loaded && s_logbook_points != NULL && s_logbook_point_count > 0U)
    {
        uint32_t profile_start_ms = logbook_profile_begin();
        (void)depth_chart_draw_profile(draw_ctx, s_logbook_points, s_logbook_point_count, area->x1, area->y1, (lv_coord_t)(chart_w - 1), (lv_coord_t)(chart_h - 1), max_t, max_d, LIGHT, 2U, LV_OPA_COVER, NULL);
        logbook_profile_end("chart_draw_profile", profile_start_ms);
    }
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
    if (!s_logbook_points_loaded || s_logbook_points == NULL || s_logbook_point_count == 0U)
    {
        logbook_label(parent, s_logbook_detail_load_failed ? "PROFILE UNAVAILABLE" : "PROFILE...", FONT_ID_SMALL, DARK, 50, (int16_t)(48 + chart_h / 2U - 10U), (uint16_t)(w - 70U), 20U, LV_TEXT_ALIGN_CENTER);
    }
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
    const uint16_t button_w = 92U;
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

    if (s_logbook_picker_view_ready &&
        ((s_logbook_picker_title == NULL) ||
         !lv_obj_is_valid(s_logbook_picker_title) ||
         (lv_obj_get_parent(s_logbook_picker_title) != parent)))
    {
        logbook_picker_view_forget();
    }

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

    if (s_logbook_picker_title) logbook_label_set_text_if_changed(s_logbook_picker_title, "DIVE LOG");
    if (s_logbook_picker_count)
    {
        (void)snprintf(right, sizeof(right), "%u LOGS", (unsigned)s_logbook_snapshot_count);
        logbook_label_set_text_if_changed(s_logbook_picker_count, right);
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
        logbook_label_set_text_if_changed(s_logbook_picker_page, page_text);
    }

    for (uint8_t row = 0U; row < LOGBOOK_PICKER_VISIBLE_ROWS; row++)
    {
        char date[20];
        if (row >= visible_count)
        {
            logbook_set_hidden_if_changed(s_logbook_picker_rows[row], true);
            continue;
        }
        logbook_set_hidden_if_changed(s_logbook_picker_rows[row], false);
        uint16_t page_offset = row;
        uint16_t entry_index = (uint16_t)(s_logbook_page_index * LOGBOOK_PICKER_VISIBLE_ROWS + page_offset);
        if ((entry_index >= s_logbook_snapshot_count) || !s_logbook_snapshot_entries[entry_index].valid)
        {
            logbook_label_set_text_if_changed(s_logbook_picker_left[row], "...");
            logbook_label_set_text_if_changed(s_logbook_picker_right[row], "");
            logbook_apply_row_style(s_logbook_picker_rows[row], s_logbook_picker_left[row], s_logbook_picker_right[row], (s_logbook_picker_focus == row), row_h);
            continue;
        }
        logbook_format_date(date, sizeof(date), &s_logbook_snapshot_entries[entry_index].meta);
        logbook_format_picker_id(left, sizeof(left), &s_logbook_snapshot_entries[entry_index]);
        (void)snprintf(right, sizeof(right), "%s", date);
        logbook_label_set_text_if_changed(s_logbook_picker_left[row], left);
        logbook_label_set_text_if_changed(s_logbook_picker_right[row], right);
        logbook_apply_row_style(s_logbook_picker_rows[row], s_logbook_picker_left[row], s_logbook_picker_right[row], (s_logbook_picker_focus == row), row_h);
    }
    for (uint8_t row = visible_count; row < LOGBOOK_PICKER_VISIBLE_ROWS; ++row)
    {
        logbook_set_hidden_if_changed(s_logbook_picker_rows[row], true);
        logbook_label_set_text_if_changed(s_logbook_picker_left[row], "");
        logbook_label_set_text_if_changed(s_logbook_picker_right[row], "");
    }
    if (s_logbook_picker_back)
    {
        logbook_set_hidden_if_changed(s_logbook_picker_back, !has_back);
    }
    if (s_logbook_picker_next)
    {
        logbook_set_hidden_if_changed(s_logbook_picker_next, !has_next);
    }
    if (has_back) logbook_apply_nav_button_style(s_logbook_picker_back, s_logbook_picker_back_lbl, (s_logbook_picker_focus == logbook_picker_focus_back()));
    if (has_next) logbook_apply_nav_button_style(s_logbook_picker_next, s_logbook_picker_next_lbl, (s_logbook_picker_focus == logbook_picker_focus_next()));

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

static bool logbook_detail_views_show(lv_obj_t *parent, uint16_t w, uint16_t h, logbook_page_t page)
{
    const int8_t active = logbook_detail_page_to_root_index(page);
    if (active < 0)
    {
        return false;
    }

    if (!logbook_detail_views_parent_valid(parent) ||
        s_logbook_detail_parent != parent ||
        s_logbook_detail_view_w != w ||
        s_logbook_detail_view_h != h ||
        s_logbook_detail_cache_gen != s_logbook_detail_view_gen)
    {
        /* 详情页快速旋转时只切换 root 的隐藏状态；只有数据/尺寸/父容器变化才重建。
         * 这能避开 LAST DIVE 内页每一步旋转都 clean + recreate 整棵对象树的 CPU 峰值。 */
        logbook_detail_views_reset();
        s_logbook_detail_parent = parent;
        s_logbook_detail_view_w = w;
        s_logbook_detail_view_h = h;
        s_logbook_detail_cache_gen = s_logbook_detail_view_gen;
    }

    if (s_logbook_detail_roots[(uint8_t)active] == NULL)
    {
        lv_obj_t *root = logbook_detail_views_create_root(parent, w, h);
        if (root == NULL)
        {
            return false;
        }

        s_logbook_detail_roots[(uint8_t)active] = root;
        switch (page)
        {
        case LOGBOOK_PAGE_DETAIL_1:
            logbook_draw_detail_1(root, w, h);
            break;
        case LOGBOOK_PAGE_DETAIL_2:
            logbook_draw_detail_2(root, w, h);
            break;
        case LOGBOOK_PAGE_SUMMARY:
        default:
            logbook_draw_summary(root, w, h);
            break;
        }
    }

    for (uint8_t i = 0U; i < 3U; ++i)
    {
        logbook_set_hidden_if_changed(s_logbook_detail_roots[i], i != (uint8_t)active);
    }
    return true;
}

static void submenu_populate_logbook(void)
{
    uint16_t w = s_submenu_width;
    uint16_t h = s_submenu_height;
    uint32_t profile_start_ms = logbook_profile_begin();
    bool picker_page = (s_logbook_page == LOGBOOK_PAGE_PICK);

    if (picker_page)
    {
        logbook_detail_views_reset();
    }
    else if (s_logbook_picker_view_ready)
    {
        logbook_picker_view_reset();
    }

    if (picker_page && !s_logbook_picker_view_ready)
    {
        lv_obj_clean(s_submenu_list);
    }

    if (!s_logbook_has_logs)
    {
        /* 空/失败状态不能叠加在上一轮 picker rows 或详情 root 上。 */
        logbook_detail_views_reset();
        if (s_logbook_picker_view_ready)
        {
            logbook_picker_view_reset();
        }
        lv_obj_clean(s_submenu_list);
        s_logbook_detail_parent = s_submenu_list;
        s_logbook_detail_direct_content_active = true;
        /* NO LOGS 只允许表示 picker 已确认 count==0。详情失败属于单条记录加载状态，
         * 即使所有重试耗尽也只能显示 LOAD FAILED，不能反向宣告整个日志库为空。 */
        const char *empty_text = (s_logbook_page == LOGBOOK_PAGE_PICK) ?
                                 (s_logbook_loading ? "LOADING" : "NO LOGS") :
                                 (s_logbook_detail_loading ? "LOADING" : "LOAD FAILED");
        logbook_label(s_submenu_list,
                      empty_text,
                      FONT_ID_MEDIUM,
                      LIGHT,
                      0,
                      logbook_compact_layout() ? 116 : 160,
                      w,
                      48,
                      LV_TEXT_ALIGN_CENTER);
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
        break;
    case LOGBOOK_PAGE_DETAIL_1:
        if (!logbook_detail_views_show(s_submenu_list, w, h, s_logbook_page))
        {
            logbook_detail_views_reset();
            lv_obj_clean(s_submenu_list);
            logbook_draw_detail_1(s_submenu_list, w, h);
            s_logbook_detail_parent = s_submenu_list;
            s_logbook_detail_direct_content_active = true;
        }
        ui_state_set_sub_item_count(0U);
        break;
    case LOGBOOK_PAGE_DETAIL_2:
        if (!logbook_detail_views_show(s_submenu_list, w, h, s_logbook_page))
        {
            logbook_detail_views_reset();
            lv_obj_clean(s_submenu_list);
            logbook_draw_detail_2(s_submenu_list, w, h);
            s_logbook_detail_parent = s_submenu_list;
            s_logbook_detail_direct_content_active = true;
        }
        ui_state_set_sub_item_count(0U);
        ui_state_set_sub_menu_idx(s_logbook_focus);
        break;
    case LOGBOOK_PAGE_SUMMARY:
    default:
        if (!logbook_detail_views_show(s_submenu_list, w, h, s_logbook_page))
        {
            logbook_detail_views_reset();
            lv_obj_clean(s_submenu_list);
            logbook_draw_summary(s_submenu_list, w, h);
            s_logbook_detail_parent = s_submenu_list;
            s_logbook_detail_direct_content_active = true;
        }
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

static bool submenu_row_is_light_status(const menu_row_t *row)
{
    return row != NULL &&
           (row->type == MENU_ROW_LIGHT_POWER ||
            row->type == MENU_ROW_LIGHT_COLOR ||
            row->type == MENU_ROW_LIGHT_LEVEL ||
            row->type == MENU_ROW_LIGHT_MODE);
}

static const char *submenu_light_status_text(const menu_row_t *row)
{
    if (row == NULL)
    {
        return "";
    }
    switch (row->type)
    {
    case MENU_ROW_LIGHT_POWER: return submenu_light_power_on() ? "ON" : "OFF";
    case MENU_ROW_LIGHT_COLOR: return bus_get_light_color_label();
    case MENU_ROW_LIGHT_LEVEL: return bus_get_light_level_label();
    case MENU_ROW_LIGHT_MODE:  return submenu_light_mode_text();
    default:                   return "";
    }
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
    light_color_preview_cancel_if_active();
    logbook_picker_stop_loader();
    logbook_detail_stop_loader();
    logbook_retry_stop();
    logbook_points_release();
    logbook_picker_cache_invalidate();
    logbook_picker_view_forget();
    s_submenu_layer = NULL;
    s_submenu_title = NULL;
    s_submenu_hint = NULL;
    s_submenu_title_line = NULL;
    s_submenu_list = NULL;
    s_light_status_lbl = NULL;
    s_light_color_title_lbl = NULL;
    s_light_color_progress_lbl = NULL;
    s_light_color_swatch = NULL;
    s_light_color_cursor = NULL;
    s_submenu_width = 0;
    s_submenu_height = 0;
    s_submenu_target_scroll_y = 0;
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
    if (!MENU_LIST_SCROLL_ANIM_ENABLED || s_submenu_selection_scroll_silent)
    {
        return LV_ANIM_OFF;
    }
    if (submenu_current_menu_is_readonly_info())
    {
        /* 只读 INFO 页的旋转目标是“立即跟手”。这里关闭列表自动滚动动画，
         * 避免快速旋转时多段 scroll 动画排队，表现成选中态延迟和 handler 拉长。 */
        return LV_ANIM_OFF;
    }
    return LV_ANIM_ON;
}

static void submenu_list_restore_scroll(lv_coord_t scroll_y)
{
    if (!s_submenu_list)
    {
        return;
    }

    if (scroll_y < 0)
    {
        scroll_y = 0;
    }

    if (s_submenu_target_scroll_y == scroll_y &&
        lv_obj_get_scroll_y(s_submenu_list) == scroll_y)
    {
        return;
    }

    lv_anim_del(s_submenu_list, (lv_anim_exec_xcb_t)0);
    lv_obj_scroll_to_y(s_submenu_list, scroll_y, LV_ANIM_OFF);
    s_submenu_target_scroll_y = lv_obj_get_scroll_y(s_submenu_list);
}

static void submenu_list_scroll_item_to_view(lv_obj_t *item)
{
    lv_coord_t visible_h;
    lv_coord_t item_y;
    lv_coord_t item_h;
    lv_coord_t scroll_y;
    lv_coord_t target_y;
    lv_coord_t margin = MENU_LIST_EDGE_PAD_PX;

    visible_h = lv_obj_get_height(s_submenu_list);
    item_y = lv_obj_get_y(item);
    item_h = lv_obj_get_height(item);
    scroll_y = s_submenu_target_scroll_y;
    target_y = scroll_y;
    if (visible_h <= item_h + margin * 2) margin = 0;

    if (item_y - margin < scroll_y) target_y = item_y - margin;
    else if (item_y + item_h + margin > scroll_y + visible_h) target_y = item_y + item_h + margin - visible_h;
    if (target_y < 0) target_y = 0;

    if (target_y == s_submenu_target_scroll_y &&
        lv_obj_get_scroll_y(s_submenu_list) == target_y)
    {
        return;
    }

    s_submenu_target_scroll_y = target_y;
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
    lv_label_set_text(s_submenu_title, "SUB MENU");
    lv_obj_set_style_text_color(s_submenu_title, LIGHT, 0);
    lv_obj_set_style_text_font(s_submenu_title, get_font(FONT_ID_TITLE), 0);

    s_submenu_hint = lv_label_create(s_submenu_layer);
    lv_obj_remove_style_all(s_submenu_hint);
    lv_obj_set_pos(s_submenu_hint, 150, 10);
    lv_obj_set_size(s_submenu_hint, s_submenu_width - 166, 26);
    lv_label_set_long_mode(s_submenu_hint, LV_LABEL_LONG_CLIP);
    lv_label_set_text(s_submenu_hint, "[ENTER SAVE] [BACK CANCEL]");
    lv_obj_set_style_text_color(s_submenu_hint, GREEN, 0);
    lv_obj_set_style_text_font(s_submenu_hint, get_font(FONT_ID_SMALL), 0);
    lv_obj_add_flag(s_submenu_hint, LV_OBJ_FLAG_HIDDEN);

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

bool screen_submenu_visible(void)
{
    if (s_submenu_layer == NULL || !lv_obj_is_valid(s_submenu_layer))
    {
        return false;
    }

    if (lv_anim_get(s_submenu_layer, (lv_anim_exec_xcb_t)lv_obj_set_x) != NULL)
    {
        return true;
    }

    return lv_obj_get_x(s_submenu_layer) < (lv_coord_t)submenu_right_width();
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
    lv_obj_set_style_bg_color(underline, GREEN, 0);
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
    plan_make_label(spin, buf, FONT_ID_SMALL, BLACK, 0, 2, 100, 20, LV_TEXT_ALIGN_CENTER);
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

static void light_color_preview_clear_refs(void)
{
    s_light_color_title_lbl = NULL;
    s_light_color_progress_lbl = NULL;
    s_light_color_swatch = NULL;
    s_light_color_bar_fill = NULL;
    s_light_color_cursor = NULL;
}

static void light_color_preview_refresh(void)
{
    uint16_t w = submenu_right_width();
    uint16_t bar_w = light_color_bar_outer_w(w);
    uint16_t inner_w = light_color_bar_inner_w(bar_w);
    uint16_t cursor_x;
    uint16_t fill_w;
    char text[32];
    const uint8_t step = light_color_step_index_from_hue(s_light_color_preview_hue);
    const uint8_t step_count = (uint8_t)(LIGHT_COLOR_HUE_MAX / LIGHT_COLOR_HUE_STEP_DEG);

    if (s_light_color_title_lbl != NULL)
    {
        lv_label_set_text(s_light_color_title_lbl, light_color_name_from_hue(s_light_color_preview_hue));
    }
    if (s_light_color_progress_lbl != NULL)
    {
        lv_snprintf(text, sizeof(text), "%02u/%02u", (unsigned int)step, (unsigned int)step_count);
        lv_label_set_text(s_light_color_progress_lbl, text);
    }
    if (s_light_color_swatch != NULL)
    {
        lv_obj_set_style_bg_color(s_light_color_swatch, light_color_progress_green(step, step_count), 0);
    }
    if (s_light_color_bar_fill != NULL)
    {
        fill_w = (uint16_t)((((uint32_t)step * (uint32_t)inner_w) + (step_count / 2U)) / step_count);
        if (fill_w < 2U)
        {
            fill_w = 2U;
        }
        if (fill_w > inner_w)
        {
            fill_w = inner_w;
        }
        lv_obj_set_width(s_light_color_bar_fill, (lv_coord_t)fill_w);
        lv_obj_set_style_bg_color(s_light_color_bar_fill, lv_color_make(0x00, 0x6E, 0x00), 0);
        lv_obj_set_style_bg_grad_color(s_light_color_bar_fill, light_color_progress_green(step, step_count), 0);
    }
    if (s_light_color_cursor != NULL)
    {
        cursor_x = (uint16_t)((((uint32_t)step * (uint32_t)inner_w) + (step_count / 2U)) / step_count);
        if (cursor_x > inner_w)
        {
            cursor_x = inner_w;
        }
        lv_obj_set_x(s_light_color_cursor, (lv_coord_t)(48 + LIGHT_COLOR_BAR_PAD_X + cursor_x - 2));
    }
}

static void light_color_preview_draw_page(void)
{
    const uint16_t w = submenu_right_width();
    const uint16_t bar_x = 48U;
    const uint16_t bar_y = 198U;
    const uint16_t bar_w = light_color_bar_outer_w(w);
    const uint16_t inner_w = light_color_bar_inner_w(bar_w);
    const uint16_t bar_h = 28U;
    lv_obj_t *bar_bg;
    lv_obj_t *bar_track;

    lv_obj_clean(s_submenu_list);
    light_color_preview_clear_refs();
    submenu_list_set_page_geometry();
    lv_obj_clear_flag(s_submenu_title, LV_OBJ_FLAG_HIDDEN);
    if (s_submenu_title_line)
    {
        lv_obj_clear_flag(s_submenu_title_line, LV_OBJ_FLAG_HIDDEN);
    }
    lv_label_set_text(s_submenu_title, "LIGHT COLOR");

    s_light_color_title_lbl = logbook_label(s_submenu_list,
                                            "",
                                            FONT_ID_TITLE,
                                            GREEN,
                                            0,
                                            54,
                                            w,
                                            34U,
                                            LV_TEXT_ALIGN_CENTER);

    s_light_color_swatch = lv_obj_create(s_submenu_list);
    lv_obj_remove_style_all(s_light_color_swatch);
    lv_obj_set_size(s_light_color_swatch, 54, 54);
    lv_obj_set_pos(s_light_color_swatch, (lv_coord_t)((w > 54U) ? ((w - 54U) / 2U) : 0U), 92);
    lv_obj_set_style_bg_opa(s_light_color_swatch, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_light_color_swatch, GREEN, 0);
    lv_obj_set_style_border_width(s_light_color_swatch, INNER_BORDER_W, 0);
    lv_obj_clear_flag(s_light_color_swatch, LV_OBJ_FLAG_SCROLLABLE);

    s_light_color_progress_lbl = logbook_label(s_submenu_list,
                                               "",
                                               FONT_ID_SMALL,
                                               LIGHT,
                                               0,
                                               154,
                                               w,
                                               24U,
                                               LV_TEXT_ALIGN_CENTER);

    bar_bg = lv_obj_create(s_submenu_list);
    lv_obj_remove_style_all(bar_bg);
    lv_obj_set_pos(bar_bg, bar_x, bar_y);
    lv_obj_set_size(bar_bg, bar_w, bar_h);
    lv_obj_set_style_bg_color(bar_bg, BLACK, 0);
    lv_obj_set_style_bg_opa(bar_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(bar_bg, GREEN, 0);
    lv_obj_set_style_border_width(bar_bg, 2, 0);
    lv_obj_clear_flag(bar_bg, LV_OBJ_FLAG_SCROLLABLE);

    bar_track = lv_obj_create(bar_bg);
    lv_obj_remove_style_all(bar_track);
    lv_obj_set_pos(bar_track, LIGHT_COLOR_BAR_PAD_X, LIGHT_COLOR_BAR_PAD_Y);
    lv_obj_set_size(bar_track, inner_w, LIGHT_COLOR_BAR_FILL_H);
    lv_obj_set_style_bg_color(bar_track, lv_color_make(0x00, 0x24, 0x00), 0);
    lv_obj_set_style_bg_opa(bar_track, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(bar_track, DARK, 0);
    lv_obj_set_style_border_width(bar_track, 1, 0);
    lv_obj_clear_flag(bar_track, LV_OBJ_FLAG_SCROLLABLE);

    s_light_color_bar_fill = lv_obj_create(bar_track);
    lv_obj_remove_style_all(s_light_color_bar_fill);
    lv_obj_set_pos(s_light_color_bar_fill, 0, 0);
    lv_obj_set_size(s_light_color_bar_fill, 2, LIGHT_COLOR_BAR_FILL_H);
    lv_obj_set_style_bg_opa(s_light_color_bar_fill, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_dir(s_light_color_bar_fill, LV_GRAD_DIR_HOR, 0);
    lv_obj_clear_flag(s_light_color_bar_fill, LV_OBJ_FLAG_SCROLLABLE);

    for (uint8_t i = 0U; i <= LIGHT_COLOR_BAR_TICK_COUNT; i++)
    {
        lv_obj_t *tick = lv_obj_create(bar_bg);
        lv_coord_t tick_x = (lv_coord_t)(LIGHT_COLOR_BAR_PAD_X + (((uint32_t)i * inner_w) / LIGHT_COLOR_BAR_TICK_COUNT));
        lv_obj_remove_style_all(tick);
        lv_obj_set_pos(tick, (lv_coord_t)(tick_x - 1), (lv_coord_t)(bar_h - 7U));
        lv_obj_set_size(tick, (i == 0U || i == LIGHT_COLOR_BAR_TICK_COUNT) ? 2 : 1, 5);
        lv_obj_set_style_bg_color(tick, (i == 0U || i == LIGHT_COLOR_BAR_TICK_COUNT) ? GREEN : DARK, 0);
        lv_obj_set_style_bg_opa(tick, LV_OPA_COVER, 0);
        lv_obj_clear_flag(tick, LV_OBJ_FLAG_SCROLLABLE);
    }

    s_light_color_cursor = lv_obj_create(s_submenu_list);
    lv_obj_remove_style_all(s_light_color_cursor);
    lv_obj_set_size(s_light_color_cursor, 4, (lv_coord_t)(bar_h + 6U));
    lv_obj_set_pos(s_light_color_cursor, (lv_coord_t)(bar_x + LIGHT_COLOR_BAR_PAD_X), (lv_coord_t)(bar_y - 3U));
    lv_obj_set_style_bg_color(s_light_color_cursor, LIGHT, 0);
    lv_obj_set_style_bg_opa(s_light_color_cursor, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_light_color_cursor, GREEN, 0);
    lv_obj_set_style_border_width(s_light_color_cursor, 1, 0);
    lv_obj_clear_flag(s_light_color_cursor, LV_OBJ_FLAG_SCROLLABLE);

    light_color_preview_refresh();
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
    if (s_submenu_hint) lv_obj_add_flag(s_submenu_hint, LV_OBJ_FLAG_HIDDEN);
    if (!s_light_color_preview_active)
    {
        light_color_preview_clear_refs();
    }
    s_submenu_selected_idx = 0xFFU;
    if (!menu_runtime_is_logbook())
    {
        /* 非 Logbook 分支下面会直接 clean 共享容器。先销毁缓存 root，
         * 避免下次进入时持有已被 LVGL 释放并可能复用的对象地址。 */
        logbook_detail_views_reset();
    }

    if ((menu_runtime_current_id() == MENU_INFO_LAST_DIVE) && !bus_is_last_dive_ready())
    {
        const uint16_t w = submenu_right_width();
        const uint16_t h = s_submenu_height;
        const int16_t title_y = (int16_t)((h > 120U) ? ((h / 2U) - 46U) : 30U);
        const int16_t loading_y = (int16_t)(title_y + 44);

        if (s_logbook_picker_view_ready)
        {
            logbook_picker_view_reset();
        }
        lv_label_set_text(s_submenu_title, "LAST DIVE");
        lv_obj_add_flag(s_submenu_title, LV_OBJ_FLAG_HIDDEN);
        if (s_submenu_title_line)
        {
            lv_obj_add_flag(s_submenu_title_line, LV_OBJ_FLAG_HIDDEN);
        }
        submenu_list_set_page_geometry();
        lv_obj_clean(s_submenu_list);
        s_light_status_lbl = NULL;
        logbook_label(s_submenu_list, "LAST DIVE", FONT_ID_TITLE, GREEN, 0, title_y, w, 36U, LV_TEXT_ALIGN_CENTER);
        logbook_label(s_submenu_list, "LOADING...", FONT_ID_MEDIUM, LIGHT, 0, loading_y, w, 40U, LV_TEXT_ALIGN_CENTER);
        ui_state_set_sub_item_count(0U);
        ui_state_set_sub_menu_idx(0U);
        (void)rows;
        (void)count;
        (void)title;
        return;
    }

    bool is_dive_plan = menu_runtime_is_dive_plan();
    if (is_dive_plan)
    {
        if (s_logbook_picker_view_ready)
        {
            logbook_picker_view_reset();
        }
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
        lv_snprintf(nested_title, sizeof(nested_title), "%s", title ? title : "");
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
        if (submenu_row_is_light_status(&rows[i]))
        {
            lv_obj_t *lbl_light = lv_label_create(item);
            lv_obj_set_style_text_color(lbl_light, GREEN, 0);
            lv_obj_set_style_text_font(lbl_light, get_font(FONT_ID_TITLE), 0);
            lv_obj_set_size(lbl_light, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align(lbl_light, LV_ALIGN_LEFT_MID, 12, 0);
            lv_obj_set_style_text_align(lbl_light, LV_TEXT_ALIGN_LEFT, 0);
            lv_label_set_text(lbl_light, rows[i].label ? rows[i].label : "");

            lv_obj_t *lbl_status = lv_label_create(item);
            lv_obj_set_style_text_color(lbl_status, submenu_light_status_color(&rows[i]), 0);
            lv_obj_set_style_text_font(lbl_status, get_font(FONT_ID_TITLE), 0);
            lv_obj_set_size(lbl_status, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align(lbl_status, LV_ALIGN_RIGHT_MID, -12, 0);
            lv_obj_set_style_text_align(lbl_status, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(lbl_status, submenu_light_status_text(&rows[i]));

            if (rows[i].type == MENU_ROW_LIGHT_POWER)
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

static void submenu_label_set_text_if_changed(lv_obj_t *label, const char *text)
{
    const char *next = (text != NULL) ? text : "";
    const char *old = NULL;

    if (label == NULL)
    {
        return;
    }

    old = lv_label_get_text(label);
    if ((old == NULL) || (strcmp(old, next) != 0))
    {
        lv_label_set_text(label, next);
    }
}

static bool refresh_readonly_info_submenu_labels(const menu_row_t *rows, uint8_t count)
{
    if (!submenu_current_menu_is_readonly_info() || !s_submenu_list || rows == NULL)
    {
        return false;
    }
    if ((menu_runtime_current_id() == MENU_INFO_LAST_DIVE) && !bus_is_last_dive_ready())
    {
        /* LAST DIVE 的 LOADING 视图不是普通行对象，等后端 ready 后回退全量重建。 */
        return false;
    }
    if (lv_obj_get_child_cnt(s_submenu_list) != count)
    {
        return false;
    }

    for (uint8_t i = 0U; i < count; i++)
    {
        if (submenu_row_is_light_status(&rows[i]))
        {
            return false;
        }
        lv_obj_t *item = lv_obj_get_child(s_submenu_list, i);
        lv_obj_t *lbl = item ? lv_obj_get_child(item, 0) : NULL;
        if (lbl == NULL)
        {
            return false;
        }
        submenu_label_set_text_if_changed(lbl, rows[i].label);
    }

    return true;
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
    bool readonly_info = submenu_current_menu_is_readonly_info();
    uint32_t cnt = lv_obj_get_child_cnt(s_submenu_list);
    if (cnt == 0U)
    {
        return;
    }
    if (idx >= cnt)
    {
        idx = (uint8_t)(cnt - 1U);
    }

    if (s_submenu_selected_idx == idx)
    {
        lv_obj_t *selected_item = lv_obj_get_child(s_submenu_list, idx);
        if (selected_item)
        {
            submenu_list_scroll_item_to_view(selected_item);
        }
        return;
    }

    if (s_submenu_selected_idx < cnt)
    {
        uint8_t old_idx = s_submenu_selected_idx;
        lv_obj_t *item = lv_obj_get_child(s_submenu_list, old_idx);
        lv_obj_t *lbl  = item ? lv_obj_get_child(item, 0) : NULL;
        /* 正在编辑的 item 由 begin_edit_value 单独管理，不参与选中态刷新 */
        if (item && !(ui_state_get_edit_active() && old_idx == ui_state_get_edit_item_index()))
        {
            if (!readonly_info)
            {
                lv_obj_clear_state(item, LV_STATE_FOCUSED | LV_STATE_EDITED | LV_STATE_CHECKED);  // HOTFIX: Clear LVGL states to fix bold residue.
                if (lbl) lv_obj_clear_state(lbl, LV_STATE_ANY);  // HOTFIX: Clear LVGL states to fix bold residue.
                lv_obj_set_style_bg_color(item, BLACK, 0);
                lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
                lv_obj_set_style_border_width(item, INNER_BORDER_W, 0);
            }
            lv_obj_set_style_border_color(item, DARK, 0);
            if (lbl)
            {
                lv_obj_set_style_text_color(lbl, GREEN, 0);
                if (!readonly_info)
                {
                    lv_obj_set_style_text_font(lbl, get_font(compact_plan ? FONT_ID_SMALL : FONT_ID_TITLE), 0);
                }
            }
            /* LIGHT CONTROL 特殊处理：第二列状态恢复为当前状态色 */
            lv_obj_t *lbl2 = lv_obj_get_child(item, 1);
            if (lbl2)
            {
                const menu_row_t *row = menu_runtime_row_at(old_idx);
                lv_obj_set_style_text_color(lbl2, submenu_light_status_color(row), 0);
                if (!readonly_info)
                {
                    lv_obj_set_style_text_font(lbl2, get_font(compact_plan ? FONT_ID_SMALL : FONT_ID_TITLE), 0);
                }
            }
        }
    }
    else
    {
        for (uint32_t i = 0; i < cnt; i++)
        {
            lv_obj_t *item = lv_obj_get_child(s_submenu_list, i);
            lv_obj_t *lbl  = item ? lv_obj_get_child(item, 0) : NULL;
            if (item == NULL) continue;
            if (ui_state_get_edit_active() && ((uint8_t)i == ui_state_get_edit_item_index())) continue;
            if (!readonly_info)
            {
                lv_obj_clear_state(item, LV_STATE_FOCUSED | LV_STATE_EDITED | LV_STATE_CHECKED);  // HOTFIX: Clear LVGL states to fix bold residue.
                if (lbl) lv_obj_clear_state(lbl, LV_STATE_ANY);  // HOTFIX: Clear LVGL states to fix bold residue.
                lv_obj_set_style_bg_color(item, BLACK, 0);
                lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
                lv_obj_set_style_border_width(item, INNER_BORDER_W, 0);
            }
            lv_obj_set_style_border_color(item, DARK, 0);
            if (lbl)
            {
                lv_obj_set_style_text_color(lbl, GREEN, 0);
                if (!readonly_info)
                {
                    lv_obj_set_style_text_font(lbl, get_font(compact_plan ? FONT_ID_SMALL : FONT_ID_TITLE), 0);
                }
            }
            /* LIGHT CONTROL 特殊处理：第二列状态恢复为当前状态色 */
            lv_obj_t *lbl2 = lv_obj_get_child(item, 1);
            if (lbl2)
            {
                const menu_row_t *row = menu_runtime_row_at((uint8_t)i);
                lv_obj_set_style_text_color(lbl2, submenu_light_status_color(row), 0);
                if (!readonly_info)
                {
                    lv_obj_set_style_text_font(lbl2, get_font(compact_plan ? FONT_ID_SMALL : FONT_ID_TITLE), 0);
                }
            }
        }
    }

    {
        lv_obj_t *item = lv_obj_get_child(s_submenu_list, idx);
        lv_obj_t *lbl  = item ? lv_obj_get_child(item, 0) : NULL;
        if (item && !(ui_state_get_edit_active() && idx == ui_state_get_edit_item_index()))
        {
            if (!readonly_info)
            {
                lv_obj_add_state(item, LV_STATE_FOCUSED);  // HOTFIX: Clear LVGL states to fix bold residue.
                lv_obj_set_style_bg_color(item, BLACK, 0);
                lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
                lv_obj_set_style_border_width(item, INNER_BORDER_W + 2, 0);
            }
            lv_obj_set_style_border_color(item, GREEN, 0);
            if (lbl)
            {
                lv_obj_set_style_text_color(lbl, LIGHT, 0);
                if (!readonly_info)
                {
                    lv_obj_set_style_text_font(lbl, get_font(compact_plan ? FONT_ID_SMALL : FONT_ID_MEDIUM), 0);
                }
            }
            /* LIGHT CONTROL second column uses the same selected emphasis. */
            lv_obj_t *lbl2 = lv_obj_get_child(item, 1);
            if (lbl2)
            {
                lv_obj_set_style_text_color(lbl2, LIGHT, 0);
                if (!readonly_info)
                {
                    lv_obj_set_style_text_font(lbl2, get_font(compact_plan ? FONT_ID_SMALL : FONT_ID_MEDIUM), 0);
                }
            }
        }
    }
    s_submenu_selected_idx = idx;

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

    s_submenu_target_scroll_y = 0;
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
    const menu_row_t *rows = NULL;
    bool prev_silent = s_submenu_selection_scroll_silent;
    lv_coord_t keep_scroll_y = s_submenu_target_scroll_y;
    menu_runtime_refresh();
    rows = menu_runtime_current_rows(&count);

    s_submenu_selection_scroll_silent = true;
    if (refresh_readonly_info_submenu_labels(rows, count))
    {
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
        return;
    }

    submenu_populate_current();
    submenu_list_restore_scroll(keep_scroll_y);
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

void screen_refresh_info_submenu_if_open_for_dirty(dirty_mask_t mask)
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

    if ((mask == DIRTY_NONE) ||
        ((mask & submenu_info_refresh_mask_for_current_menu()) == DIRTY_NONE))
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

void screen_refresh_info_submenu_if_open(void)
{
    dirty_mask_t mask = screen_current_info_submenu_refresh_mask();
    if (mask == DIRTY_NONE)
    {
        return;
    }
    screen_refresh_info_submenu_if_open_for_dirty(mask);
}

dirty_mask_t screen_current_info_submenu_refresh_mask(void)
{
    if ((ui_state_get_state() != UI_SUB_MENU) ||
        (ui_state_get_sub_parent() != UI_INFO) ||
        (ui_state_get_sub_history_depth() != 0U))
    {
        return DIRTY_NONE;
    }

    return submenu_info_refresh_mask_for_current_menu();
}

bool screen_refresh_logbook_if_open(void)
{
    if (!s_submenu_title || !s_submenu_list)
    {
        return false;
    }

    if ((ui_state_get_state() != UI_SUB_MENU) ||
        (ui_state_get_sub_parent() != UI_INFO) ||
        (ui_state_get_sub_history_depth() != 0U))
    {
        return false;
    }

    if (menu_runtime_current_id() == MENU_INFO_LAST_DIVE)
    {
        /* LAST DIVE 只需要刷新摘要 VM；不要走 DIVE LOG picker 的
         * logbook_load_current()，否则日志 dirty 会把只读页拖进列表快照重建。 */
        refresh_info_submenu_page(ui_state_get_sub_menu_idx());
        return true;
    }

    if (!menu_runtime_is_logbook())
    {
        return false;
    }

    s_logbook_empty_confirm_state = LOGBOOK_EMPTY_CONFIRM_UNKNOWN;
    s_logbook_empty_confirm_retry_count = 0U;

    if (menu_runtime_is_logbook() && s_logbook_page != LOGBOOK_PAGE_PICK)
    {
        /* 后台 meta/index 更新不能打断已经打开的详情。记录延迟刷新，等返回 picker 后
         * 再重查索引，避免释放当前 profile 或把短暂存储忙误显示成 NO LOGS。 */
        s_logbook_picker_refresh_pending = true;
        submenu_populate_current();
        return true;
    }

    logbook_load_current();
    submenu_populate_current();
    return true;
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
    return screen_handle_dive_plan_rotate_steps(dir);
}

bool screen_handle_dive_plan_rotate_steps(int8_t steps)
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
    int8_t dir = 0;
    uint8_t step_count = 0U;
    if (!normalize_rotate_steps(steps, &dir, &step_count))
    {
        return false;
    }

    bool consumed = false;
    for (uint8_t i = 0U; i < step_count; i++)
    {
        if (!submenu_dive_plan_handle_rotate(dir))
        {
            break;
        }
        consumed = true;
    }
    if (!consumed)
    {
        return false;
    }
    refresh_info_submenu_page(1U);
    return true;
}

bool screen_handle_logbook_rotate(int8_t dir)
{
    return screen_handle_logbook_rotate_steps(dir);
}

bool screen_handle_logbook_rotate_steps(int8_t steps)
{
    if ((ui_state_get_state() != UI_SUB_MENU) ||
        (ui_state_get_sub_parent() != UI_INFO) ||
        !submenu_is_logbook_visible())
    {
        return false;
    }

    int8_t dir = 0;
    uint8_t step_count = 0U;
    if (!normalize_rotate_steps(steps, &dir, &step_count))
    {
        return false;
    }

    if (!s_logbook_has_logs)
    {
        s_logbook_focus = 0U;
        submenu_populate_current();
        return true;
    }

    if (s_logbook_page == LOGBOOK_PAGE_PICK)
    {
        uint8_t focus_count = logbook_picker_focus_count();
        uint8_t old_focus = s_logbook_picker_focus;
        uint16_t old_logbook_focus = s_logbook_focus;
        int16_t next = (int16_t)s_logbook_picker_focus + ((int16_t)dir * (int16_t)step_count);
        if (focus_count == 0U)
        {
            if (s_logbook_picker_focus != 0U || s_logbook_focus != 0U)
            {
                s_logbook_picker_focus = 0U;
                s_logbook_focus = 0U;
                submenu_populate_current();
            }
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
        if (s_logbook_picker_focus == old_focus &&
            s_logbook_focus == old_logbook_focus)
        {
            return true;
        }
        submenu_populate_current();
        return true;
    }

    logbook_page_t old_page = s_logbook_page;
    int16_t next_page = (int16_t)s_logbook_page + ((int16_t)dir * (int16_t)step_count);
    if (next_page < (int16_t)LOGBOOK_PAGE_SUMMARY)
    {
        next_page = (int16_t)LOGBOOK_PAGE_SUMMARY;
    }
    if (next_page > (int16_t)LOGBOOK_PAGE_DETAIL_2)
    {
        next_page = (int16_t)LOGBOOK_PAGE_DETAIL_2;
    }
    s_logbook_page = (logbook_page_t)next_page;
    s_logbook_focus = 0U;
    if (s_logbook_page == old_page)
    {
        return true;
    }
    submenu_populate_current();
    return true;
}

static void submenu_show_light_color_preview_hint(void)
{
    if (s_submenu_hint)
    {
        lv_label_set_text(s_submenu_hint, "[ENTER SAVE] [BACK CANCEL]");
        lv_obj_clear_flag(s_submenu_hint, LV_OBJ_FLAG_HIDDEN);
    }
}

static void light_color_preview_cancel_if_active(void)
{
    if (!s_light_color_preview_active)
    {
        return;
    }

    /* ROTARY COLOR 旋转只做实时预览；只有 ENTER 才允许持久化。
     * 若预览态被 BACK 以外的关闭路径打断，也按取消处理，恢复进入页面前的颜色。 */
    bus_preview_light_rgb(s_light_color_preview_original_rgb);
    s_light_color_preview_active = false;
    light_color_preview_clear_refs();
}

void screen_begin_light_color_preview(uint8_t item_idx)
{
    (void)item_idx;

    if (menu_runtime_current_id() != MENU_LIGHT_COLOR)
    {
        return;
    }
    s_light_color_preview_original_rgb = bus_get_light_rgb();
    s_light_color_preview_hue = light_color_hue_from_rgb(s_light_color_preview_original_rgb);
    s_light_color_preview_active = true;
    ui_state_set_sub_menu_idx(LIGHT_COLOR_ROTARY_ROW_INDEX);
    light_color_preview_draw_page();
    submenu_show_light_color_preview_hint();
    ui_state_set_state(UI_EDIT_LIGHT_COLOR);
}

bool screen_handle_light_color_preview_rotate(int8_t dir)
{
    return screen_handle_light_color_preview_rotate_steps(dir);
}

bool screen_handle_light_color_preview_rotate_steps(int8_t steps)
{
    if (!s_light_color_preview_active || menu_runtime_current_id() != MENU_LIGHT_COLOR)
    {
        return false;
    }

    int8_t dir = 0;
    uint8_t step_count = 0U;
    if (!normalize_rotate_steps(steps, &dir, &step_count))
    {
        return false;
    }

    for (uint8_t i = 0U; i < step_count; i++)
    {
        if (dir > 0)
        {
            s_light_color_preview_hue =
                (uint16_t)((s_light_color_preview_hue + LIGHT_COLOR_HUE_STEP_DEG) % LIGHT_COLOR_HUE_MAX);
        }
        else if (s_light_color_preview_hue < LIGHT_COLOR_HUE_STEP_DEG)
        {
            s_light_color_preview_hue =
                (uint16_t)(LIGHT_COLOR_HUE_MAX - (LIGHT_COLOR_HUE_STEP_DEG - s_light_color_preview_hue));
        }
        else
        {
            s_light_color_preview_hue = (uint16_t)(s_light_color_preview_hue - LIGHT_COLOR_HUE_STEP_DEG);
        }
    }
    bus_preview_light_rgb(light_color_rgb_from_hue(s_light_color_preview_hue));
    light_color_preview_refresh();
    submenu_show_light_color_preview_hint();
    return true;
}

void screen_commit_light_color_preview(void)
{
    if (s_light_color_preview_active)
    {
        bus_set_light_rgb(bus_get_light_rgb());
    }
    s_light_color_preview_active = false;
    light_color_preview_clear_refs();
    if (s_submenu_hint) lv_obj_add_flag(s_submenu_hint, LV_OBJ_FLAG_HIDDEN);
    screen_close_submenu();
}

void screen_cancel_light_color_preview(void)
{
    light_color_preview_cancel_if_active();
    ui_state_set_sub_menu_idx(LIGHT_COLOR_ROTARY_ROW_INDEX);
    ui_state_set_state(UI_SUB_MENU);
    if (s_submenu_hint) lv_obj_add_flag(s_submenu_hint, LV_OBJ_FLAG_HIDDEN);
    refresh_current_submenu_page(LIGHT_COLOR_ROTARY_ROW_INDEX);
}

bool screen_handle_logbook_back(void)
{
    if ((ui_state_get_state() != UI_SUB_MENU) ||
        (ui_state_get_sub_parent() != UI_INFO) ||
        !submenu_is_logbook_visible())
    {
        return false;
    }

    if (!s_logbook_has_logs && s_logbook_page == LOGBOOK_PAGE_PICK)
    {
        screen_close_submenu();
        return true;
    }

    if (s_logbook_page != LOGBOOK_PAGE_PICK)
    {
        logbook_detail_stop_loader();
        logbook_points_release();
        s_logbook_page = LOGBOOK_PAGE_PICK;
        if (s_logbook_picker_refresh_pending || !s_logbook_has_logs)
        {
            s_logbook_picker_refresh_pending = false;
            logbook_load_current();
        }
        logbook_picker_restore_focus(s_logbook_index);
        submenu_populate_current();
        return true;
    }

    screen_close_submenu();
    return true;
}

static void screen_handle_logbook_select(void)
{
    if (!s_logbook_has_logs)
    {
        if (s_logbook_loading)
        {
            return;
        }
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
        logbook_picker_stop_loader();
        logbook_detail_stop_loader();
        logbook_points_release();
        if (!s_logbook_snapshot_entries[s_logbook_index].valid)
        {
            logbook_entry_t recovered_entry;
            logbook_record_key_t recovered_key = LOGBOOK_INVALID_RECORD_KEY;
            if (logbook_picker_load_summary(s_logbook_index, &recovered_entry, &recovered_key))
            {
                logbook_picker_item_from_entry(&s_logbook_snapshot_entries[s_logbook_index],
                                               &recovered_entry,
                                               recovered_key);
            }
        }
        if (!s_logbook_snapshot_entries[s_logbook_index].valid)
        {
            logbook_picker_start_loader(s_logbook_page_index, false);
            submenu_populate_current();
            break;
        }
        logbook_entry_from_picker_item(&s_logbook_entry, &s_logbook_snapshot_entries[s_logbook_index]);
        s_logbook_focus = s_logbook_index;
        s_logbook_page = LOGBOOK_PAGE_SUMMARY;
        s_logbook_focus = 0U;
        /* 先展示 picker summary 和 PROFILE...；真机详情/profile 由唯一 worker 补齐。 */
        logbook_detail_start_loader(s_logbook_index);
        submenu_populate_current();
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

    s_submenu_target_scroll_y = 0;
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
    s_submenu_target_scroll_y = 0;
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
    lv_coord_t keep_scroll_y = s_submenu_target_scroll_y;
    menu_runtime_refresh();
    (void)menu_runtime_current_rows(&count);

    s_submenu_selection_scroll_silent = true;
    submenu_populate_current();
    submenu_list_restore_scroll(keep_scroll_y);
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

void screen_refresh_current_submenu(void)
{
    refresh_current_submenu_page(ui_state_get_sub_menu_idx());
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
    case MENU_ACTION_CLOSE_PARENT_TOO:
        screen_close_submenu();
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
    case MENU_ACTION_BEGIN_LIGHT_COLOR_PREVIEW:
        screen_begin_light_color_preview(item_idx);
        break;
    case MENU_ACTION_SHOW_GAS_MODAL:
        screen_show_modal_gas();
        ui_state_set_state(UI_MODAL_GAS);
        break;
    case MENU_ACTION_SHOW_TEXT_MODAL:
        screen_show_modal_act(action.modal_text);
        break;
    case MENU_ACTION_SHOW_BACK_NOTICE:
        screen_show_modal_back_notice("DIVING", action.modal_text);
        ui_state_set_state(UI_MODAL_DIVE_LOCKED);
        break;
    default:
        break;
    }
}

void screen_close_submenu(void)
{
    light_color_preview_cancel_if_active();
    /* 任何返回路径后续都可能 clean/reuse s_submenu_list。必须先删除详情页
     * root，不能把已释放对象地址留到下次进入 Logbook 再判断。 */
    logbook_detail_views_reset();

    if (!s_submenu_layer || !s_submenu_title || !s_submenu_list)
    {
        ui_state_set_sub_history_depth(0U);
        ui_state_set_sub_item_count(0U);
        ui_state_set_sub_menu_idx(0U);
        ui_state_set_edit_active(false);
        ui_state_set_gas_modal_from_submenu(false);
        logbook_picker_stop_loader();
        logbook_detail_stop_loader();
        logbook_retry_stop();
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
    logbook_picker_stop_loader();
    logbook_detail_stop_loader();
    logbook_retry_stop();
    logbook_points_release();
    ui_state_set_sub_history_depth(0U);
    ui_state_set_sub_item_count(0U);
    ui_state_set_sub_menu_idx(0U);
    ui_state_set_state(ui_state_get_sub_parent());
}

void screen_confirm_submenu_setting(void)
{
    bool close_extra_mode_layer = false;
    bool return_dash_after_apply = false;
    bool keep_current_row = false;
    if (!menu_actions_confirm_pending(&close_extra_mode_layer, &return_dash_after_apply, &keep_current_row))
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
        logbook_picker_stop_loader();
        logbook_detail_stop_loader();
        logbook_retry_stop();
        logbook_detail_views_reset();
        logbook_points_release();
        menu_runtime_reset();
        menu_actions_clear_pending();
        submenu_slide_out();
        ui_state_set_state(UI_DASH);
        return;
    }
    if (keep_current_row)
    {
        refresh_current_submenu_page(ui_state_get_sub_menu_idx());
        ui_state_set_state(UI_SUB_MENU);
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
