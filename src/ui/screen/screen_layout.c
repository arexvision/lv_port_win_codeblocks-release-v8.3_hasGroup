/*
 * 文件: src/app_ui/ui/screen/screen_layout.c
 * 作用: 该文件属于屏幕或页面编排模块，负责整屏布局、分页切换、覆盖层、编辑态显示或页面注册管理。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "screen_layout.h"
#include "../comp/comp_update.h"
#include "../comp/comp_view.h"
#include "../core/data.h"
#include "../core/ui_state.h"
#include "../core/ui_vm.h"
#include "../core/ui_engine.h"
#include "page_registry.h"
#include "../views/modal_view.h"
#include "../views/submenu_view.h"
#include "../cards/card_compass.h"
#include "layout_view.h"
#include "page_registry.h"
#include <stdio.h>
#include <string.h>

/* APP 0x45 下发布局后的产品策略：
 * 不再尝试恢复“旧的动态页位置”，而是统一在布局重建完成后回到卡片第一页，
 * 避免自定义卡片插入/删除导致 display_pos 漂移，把 UI 状态机带到错误页。 */
static bool s_enter_card_home_after_layout_rebuild = false;

void screen_request_enter_card_home_after_layout_rebuild(void)
{
    /*
     * 物理意义：
     * APP 0x45 下发布局属于结构级替换，旧的 UI_SETUP / UI_SUB_MENU / 编辑态上下文
     * 已经不再可信。这里先把纯状态仓库收敛到 DASH 首页，真正的 LVGL 对象选中态等
     * 到 tileview 重建完成后再由强制首页逻辑重新落地。
     */
    s_enter_card_home_after_layout_rebuild = true;
    ui_state_set_sub_history_depth(0U);
    ui_state_set_sub_item_count(0U);
    ui_state_set_sub_menu_idx(0U);
    ui_state_set_sub_parent(UI_DASH);
    ui_state_set_menu_info_idx(0U);
    ui_state_set_menu_setup_idx(0U);
    ui_state_set_dash_page(PAGE_POS_DYNAMIC_FIRST);
    ui_state_set_state(UI_DASH);
}

typedef struct
{
    bool valid;
    uint8_t display_pos;
    uint8_t storage_pos;
    uint8_t page_id;
    uint8_t custom_slot;
} screen_page_restore_snapshot_t;

static screen_page_restore_snapshot_t screen_capture_page_restore_snapshot(uint8_t display_pos)
{
    screen_page_restore_snapshot_t snapshot;

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.valid = false;
    snapshot.display_pos = display_pos;
    snapshot.storage_pos = 0xFFU;
    snapshot.page_id = PAGE_ID_UNUSED;
    snapshot.custom_slot = 0xFFU;

    if (display_pos >= page_count())
    {
        return snapshot;
    }

    snapshot.storage_pos = page_storage_pos(display_pos);
    snapshot.page_id = page_id_at(display_pos);
    if (snapshot.page_id == PAGE_ID_UNUSED)
    {
        return snapshot;
    }

    if ((snapshot.page_id == PAGE_ID_CUSTOM_GRID) && (snapshot.storage_pos != 0xFFU))
    {
        snapshot.custom_slot = ui_custom_card_slot_get(snapshot.storage_pos);
    }

    snapshot.valid = true;
    return snapshot;
}

static bool screen_find_display_pos_by_snapshot(const screen_page_restore_snapshot_t *snapshot,
                                                uint8_t *out_display_pos)
{
    if ((snapshot == NULL) || (out_display_pos == NULL) || !snapshot->valid)
    {
        return false;
    }

    if (snapshot->page_id == PAGE_ID_INFO)
    {
        *out_display_pos = PAGE_POS_INFO;
        return true;
    }

    if (snapshot->page_id == PAGE_ID_SETUP)
    {
        *out_display_pos = page_setup_display_pos();
        return true;
    }

    for (uint8_t pos = PAGE_POS_DYNAMIC_FIRST; pos < page_count(); ++pos)
    {
        uint8_t page_id = page_id_at(pos);

        if (page_id != snapshot->page_id)
        {
            continue;
        }

        if (page_id == PAGE_ID_CUSTOM_GRID)
        {
            uint8_t storage_pos = page_storage_pos(pos);
            uint8_t custom_slot = (storage_pos != 0xFFU) ? ui_custom_card_slot_get(storage_pos) : 0xFFU;

            if ((snapshot->custom_slot != 0xFFU) && (custom_slot == snapshot->custom_slot))
            {
                *out_display_pos = pos;
                return true;
            }

            if ((snapshot->storage_pos != 0xFFU) && (storage_pos == snapshot->storage_pos))
            {
                *out_display_pos = pos;
                return true;
            }

            continue;
        }

        *out_display_pos = pos;
        return true;
    }

    return false;
}

static uint8_t screen_restore_dash_page_or_default(const screen_page_restore_snapshot_t *snapshot)
{
    uint8_t restored_display_pos = PAGE_POS_DYNAMIC_FIRST;

    if (screen_find_display_pos_by_snapshot(snapshot, &restored_display_pos))
    {
        return restored_display_pos;
    }

    if (restored_display_pos >= page_setup_display_pos())
    {
        return PAGE_POS_DYNAMIC_FIRST;
    }

    return restored_display_pos;
}

static uint8_t screen_clamp_info_index(uint8_t idx)
{
    uint8_t count = screen_info_item_count();

    if (count == 0U)
    {
        return 0U;
    }

    return (idx < count) ? idx : (uint8_t)(count - 1U);
}

static uint8_t screen_clamp_setup_index(uint8_t idx)
{
    uint8_t count = screen_setup_item_count();

    if (count == 0U)
    {
        return 0U;
    }

    return (idx < count) ? idx : (uint8_t)(count - 1U);
}

static bool screen_restore_info_state(uint8_t menu_idx)
{
    if (screen_info_item_count() == 0U)
    {
        return false;
    }

    menu_idx = screen_clamp_info_index(menu_idx);
    ui_state_set_sub_history_depth(0U);
    ui_state_set_sub_item_count(0U);
    ui_state_set_sub_menu_idx(0U);
    ui_state_set_sub_parent(UI_INFO);
    ui_state_set_state(UI_INFO);
    ui_state_set_menu_info_idx(menu_idx);
    ui_state_set_dash_page(PAGE_POS_INFO);
    ui_go_to_page(PAGE_POS_INFO);
    screen_set_info_selection(menu_idx);
    return true;
}

static bool screen_restore_setup_state(uint8_t menu_idx)
{
    uint8_t setup_pos = page_setup_display_pos();

    if (screen_setup_item_count() == 0U)
    {
        return false;
    }

    menu_idx = screen_clamp_setup_index(menu_idx);
    ui_state_set_sub_history_depth(0U);
    ui_state_set_sub_item_count(0U);
    ui_state_set_sub_menu_idx(0U);
    ui_state_set_sub_parent(UI_SETUP);
    ui_state_set_state(UI_SETUP);
    ui_state_set_menu_setup_idx(menu_idx);
    ui_state_set_dash_page(setup_pos);
    ui_go_to_page(setup_pos);
    screen_set_setup_selection(menu_idx);
    return true;
}

static void screen_force_card_home_after_layout_rebuild(void)
{
    /*
     * 物理意义：
     * 这是 APP 0x45 / 自定义布局下发后的强制首页策略。
     * 这里不再依赖旧的页面恢复快照，也不允许恢复到 INFO / SETUP。
     * 强制首页只做一件事：跳到卡片第一页，并让状态机停在 DASH。
     */
    ui_state_set_sub_history_depth(0U);
    ui_state_set_sub_item_count(0U);
    ui_state_set_sub_menu_idx(0U);
    ui_state_set_sub_parent(UI_DASH);
    ui_state_set_menu_info_idx(0U);
    ui_state_set_menu_setup_idx(0U);
    ui_state_set_dash_page(PAGE_POS_DYNAMIC_FIRST);
    ui_state_set_state(UI_DASH);

    if (s_tile_objs[PAGE_POS_DYNAMIC_FIRST])
    {
        ui_go_to_page(PAGE_POS_DYNAMIC_FIRST);
    }
}

static bool screen_restore_submenu_state(ui_state_t sub_parent,
                                         uint8_t menu_idx,
                                         uint8_t sub_idx)
{
    uint8_t item_count = 0U;

    if (sub_parent == UI_INFO)
    {
        if (!screen_restore_info_state(menu_idx))
        {
            return false;
        }

        screen_open_info_submenu(ui_state_get_menu_info_idx());
    }
    else if (sub_parent == UI_SETUP)
    {
        if (!screen_restore_setup_state(menu_idx))
        {
            return false;
        }

        screen_open_setup_submenu(ui_state_get_menu_setup_idx());
    }
    else
    {
        return false;
    }

    item_count = ui_state_get_sub_item_count();
    if (item_count == 0U)
    {
        return false;
    }

    if (sub_idx >= item_count)
    {
        sub_idx = (uint8_t)(item_count - 1U);
    }

    ui_state_set_sub_history_depth(0U);
    ui_state_set_sub_menu_idx(sub_idx);
    screen_set_submenu_selection(sub_idx);
    return true;
}

static bool screen_restore_menu_parent_state(ui_state_t parent_state, uint8_t menu_idx)
{
    if (parent_state == UI_INFO)
    {
        return screen_restore_info_state(menu_idx);
    }

    if (parent_state == UI_SETUP)
    {
        return screen_restore_setup_state(menu_idx);
    }

    return false;
}

static void screen_log_restore_snapshot(const char *phase,
                                        ui_state_t saved_state,
                                        const screen_page_restore_snapshot_t *snapshot,
                                        uint8_t restored_display_pos,
                                        ui_state_t restored_state)
{
    
}

void clear_widget_arrays(void)
{
    /* 清空组件渲染状态缓存，确保布局重建后不会复用旧的对象记录。 */
    reset_widget_render_state();
}

void left_anchor_rebuild(uint8_t comp_count)
{
    /* 左锚点重建只关心容器尺寸和子组件重新渲染，不直接使用 comp_count。 */
    (void)comp_count;
    if (!s_left_anchor || !s_safe_zone) return;
    lv_obj_set_size(s_left_anchor, ui_anchor_w_get(), ui_anchor_h_get());
    lv_obj_clean(s_left_anchor);
    render_left_anchor_grid(s_left_anchor);
}

void left_anchor_create(void)
{
    /* 左锚点是整屏布局的固定参考物，负责承载 2x7 左侧信息网格。 */
    uint16_t panel_gap = ui_panel_gap_px_get();
    uint16_t content_w = ui_content_w_get();
    uint16_t content_h = ui_content_h_get();

    s_left_anchor = lv_obj_create(s_safe_zone);
    lv_obj_set_size(s_left_anchor, ui_anchor_w_get(), ui_anchor_h_get());
    if (ui_layout_is_vertical_split())
    {
        if (ui_layout_order_get() == ORDER_NORMAL)
        {
            /* 正常布局时左锚点固定贴左。 */
            lv_obj_set_pos(s_left_anchor, 0, 0);
        }
        else
        {
            /* 翻转布局时左锚点移动到右侧。 */
            lv_obj_set_pos(s_left_anchor, (lv_coord_t)(content_w + panel_gap), 0);
        }
    }
    else
    {
        lv_obj_set_pos(s_left_anchor,
                       0,
                       (ui_layout_order_get() == ORDER_NORMAL)
                       ? 0
                       : (lv_coord_t)content_h);
    }
    lv_obj_add_style(s_left_anchor, &s_style_anchor_bg, 0);
    lv_obj_set_style_pad_all(s_left_anchor, 0, 0);
    lv_obj_set_scrollbar_mode(s_left_anchor, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(s_left_anchor, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clean(s_left_anchor);
    clear_widget_arrays();
    render_left_anchor_grid(s_left_anchor);
}

void safe_zone_reposition(void)
{
    /* 安全区尺寸、偏移或布局顺序变化后，统一在这里重算整屏对象位置。 */
    if (!s_safe_zone) return;
    lv_obj_set_size(s_safe_zone, ui_safe_zone_w_get(), ui_safe_zone_h_get());
    lv_obj_align(s_safe_zone, LV_ALIGN_CENTER, ui_offset_x_get(), ui_offset_y_get());

    uint16_t panel_gap = ui_panel_gap_px_get();
    uint16_t content_w = ui_content_w_get();
    uint16_t content_h = ui_content_h_get();

    if (ui_layout_is_vertical_split())
    {
        if (ui_layout_order_get() == ORDER_NORMAL)
        {
            /* 正常模式：左锚点在左，右内容区在右。 */
            lv_obj_set_pos(s_left_anchor, 0, 0);
            lv_obj_set_pos(s_right_cont, (lv_coord_t)(ui_anchor_w_get() + panel_gap), 0);
        }
        else
        {
            /* 翻转模式：右内容区在左，左锚点在右。 */
            lv_obj_set_pos(s_right_cont, 0, 0);
            lv_obj_set_pos(s_left_anchor, (lv_coord_t)content_w + panel_gap, 0);
        }
    }
    else
    {
        if (ui_layout_order_get() == ORDER_NORMAL)
        {
            lv_obj_set_pos(s_left_anchor, 0, 0);
            lv_obj_set_pos(s_right_cont, 0, (lv_coord_t)ui_anchor_h_get());
        }
        else
        {
            lv_obj_set_pos(s_right_cont, 0, 0);
            lv_obj_set_pos(s_left_anchor, 0, (lv_coord_t)content_h);
        }
    }

    if (s_left_anchor)
    {
        lv_obj_move_foreground(s_left_anchor);
        lv_obj_invalidate(s_left_anchor);
    }

    lv_obj_set_size(s_left_anchor, ui_anchor_w_get(), ui_anchor_h_get());
    lv_obj_set_size(s_right_cont, content_w, content_h);
    if (s_tileview)
    {
        lv_obj_set_size(s_tileview, content_w, content_h);
    }
    s_cached_right_w = content_w;

    if (s_dot_cont)
    {
        /* 指示点的位置依赖布局顺序、点位模式和当前可见动态页数量。 */
        lv_coord_t dot_x, dot_y;
        uint16_t dot_cont_h = page_visible_dash_count() * 14;
        lv_obj_set_size(s_dot_cont, 10, (lv_coord_t)dot_cont_h);
        if (ui_dots_position_get() == DOTS_LEFT)
        {
            lv_coord_t gap_center_x;
            lv_coord_t gap_center_y = (lv_coord_t)(ui_safe_zone_h_get() / 2U);
            if (ui_layout_is_vertical_split())
            {
                gap_center_x = (ui_layout_order_get() == ORDER_NORMAL)
                               ? (lv_coord_t)(ui_anchor_w_get() + panel_gap / 2)
                               : (lv_coord_t)(content_w + panel_gap / 2);
            }
            else
            {
                gap_center_x = (lv_coord_t)(content_w - 18U);
                gap_center_y = (lv_coord_t)((ui_layout_order_get() == ORDER_NORMAL)
                               ? (ui_anchor_h_get() + content_h / 2U)
                               : (content_h / 2U));
            }
            dot_x = gap_center_x - 5;
            dot_y = gap_center_y - (lv_coord_t)(dot_cont_h / 2);
        }
        else if (ui_dots_position_get() == DOTS_RIGHT)
        {
            lv_coord_t content_x = 0;
            lv_coord_t content_y = 0;
            if (ui_layout_is_vertical_split() && ui_layout_order_get() == ORDER_NORMAL)
            {
                content_x = (lv_coord_t)(ui_anchor_w_get() + panel_gap);
            }
            if (!ui_layout_is_vertical_split() && ui_layout_order_get() == ORDER_NORMAL)
            {
                content_y = (lv_coord_t)ui_anchor_h_get();
            }
            dot_x = content_x + (lv_coord_t)content_w - 18;
            dot_y = content_y + (lv_coord_t)(content_h / 2U - (int)dot_cont_h / 2);
        }
        else if (ui_dots_position_get() == DOTS_BOTTOM)
        {
            dot_x = (lv_coord_t)(ui_safe_zone_w_get() / 2U - 5);
            dot_y = (lv_coord_t)(ui_safe_zone_h_get() - 18U);
        }
        else
        {
            dot_x = -1000;
            dot_y = -1000;
        }
        lv_obj_set_pos(s_dot_cont, dot_x, dot_y);
        for (uint8_t i = 0; i < DASH_PAGE_COUNT; i++)
        {
            if (s_scroll_dots[i])
            {
                lv_obj_set_pos(s_scroll_dots[i], 2, (lv_coord_t)(i * 14));
                if (i >= page_visible_dash_count())
                {
                    lv_obj_add_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
                }
                else
                {
                    lv_obj_clear_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
                }
            }
        }
        uint8_t active_idx = 0;
        if (ui_state_get_dash_page() >= PAGE_POS_DYNAMIC_FIRST && ui_state_get_dash_page() < page_setup_display_pos())
        {
            for (uint8_t pos = PAGE_POS_DYNAMIC_FIRST; pos < ui_state_get_dash_page(); pos++)
            {
                uint8_t page_id = page_id_at(pos);
                if (page_id != PAGE_ID_UNUSED)
                {
                    active_idx++;
                }
            }
        }
        screen_update_scroll_dots(active_idx, ui_state_get_state() == UI_DASH || ui_state_get_state() == UI_EDIT_GAS);
    }
}

void right_panel_create(void)
{
    /* 右侧面板承载 tileview、页面卡片和滚动指示点。 */
    uint16_t panel_gap = ui_panel_gap_px_get();
    uint16_t content_w = ui_content_w_get();
    uint16_t content_h = ui_content_h_get();
    s_cached_right_w = content_w;

    s_right_cont = lv_obj_create(s_safe_zone);
    lv_obj_set_size(s_right_cont, content_w, content_h);
    if (ui_layout_is_vertical_split())
    {
        if (ui_layout_order_get() == ORDER_NORMAL)
        {
            lv_obj_set_pos(s_right_cont, (lv_coord_t)(ui_anchor_w_get() + panel_gap), 0);
        }
        else
        {
            lv_obj_set_pos(s_right_cont, 0, 0);
        }
    }
    else
    {
        lv_obj_set_pos(s_right_cont,
                       0,
                       (ui_layout_order_get() == ORDER_NORMAL)
                       ? (lv_coord_t)ui_anchor_h_get()
                       : 0);
    }
    lv_obj_set_style_bg_color(s_right_cont, BLACK, 0);
    lv_obj_set_style_bg_opa(s_right_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_right_cont, 0, 0);
    lv_obj_set_style_border_width(s_right_cont, 0, 0);
    lv_obj_clear_flag(s_right_cont, LV_OBJ_FLAG_SCROLLABLE);

    s_tileview = lv_tileview_create(s_right_cont);
    lv_obj_set_size(s_tileview, content_w, content_h);
    lv_obj_set_pos(s_tileview, 0, 0);
    lv_obj_set_style_bg_color(s_tileview, BLACK, 0);
    lv_obj_set_style_bg_opa(s_tileview, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_tileview, 0, 0);
    lv_obj_set_scrollbar_mode(s_tileview, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(s_tileview, LV_OBJ_FLAG_SCROLLABLE);

    memset(s_tile_objs, 0, sizeof(s_tile_objs));
    uint8_t count = page_count();
    for (uint8_t i = 0; i < count; i++)
    {
        /* 按 page_registry 提供的显示顺序逐页创建 tile。 */
        page_t *page = page_get(i);
        if (!page) continue;
        lv_obj_t *tile = lv_tileview_add_tile(s_tileview, 0, i, LV_DIR_TOP | LV_DIR_BOTTOM);
        lv_obj_set_style_bg_color(tile, BLACK, 0);
        lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(tile, 0, 0);
        lv_obj_set_scrollbar_mode(tile, LV_SCROLLBAR_MODE_OFF);
        lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
        s_tile_objs[i] = tile;
        page->tile_obj = tile;
        switch (page->engine_type)
        {
        case PAGE_ENGINE_GRID:
        {
            /* GRID 类型页面通过自定义网格渲染器生成内容。 */
            uint8_t storage_pos = page_storage_pos(i);
            uint8_t custom_page_idx = ui_custom_card_slot_get(storage_pos);
            if (custom_page_idx < MAX_CUSTOM_CARDS)
            {
                render_5f_custom_grid(tile, g_left_anchor_obj, custom_page_idx);
            }
            break;
        }
        case PAGE_ENGINE_MENU:
        case PAGE_ENGINE_CUSTOM:
        default:
            /* 其余页面直接调用各自的 create 回调。 */
            if (page->create_cb) page->create_cb(tile);
            break;
        }
    }

    s_dot_cont = lv_obj_create(s_safe_zone);
    uint16_t dot_cont_h = page_visible_dash_count() * 14;
    lv_obj_set_size(s_dot_cont, 10, dot_cont_h);
    lv_obj_set_style_bg_opa(s_dot_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_dot_cont, 0, 0);
    lv_obj_set_style_pad_all(s_dot_cont, 0, 0);
    lv_obj_set_scrollbar_mode(s_dot_cont, LV_SCROLLBAR_MODE_OFF);

    if (ui_dots_position_get() == DOTS_LEFT)
    {
        lv_coord_t gap_center_x;
        lv_coord_t gap_center_y = (lv_coord_t)(ui_safe_zone_h_get() / 2U);
        gap_center_x = (ui_layout_order_get() == ORDER_NORMAL)
                       ? (lv_coord_t)(ui_anchor_w_get() + panel_gap / 2)
                       : (lv_coord_t)(content_w + panel_gap / 2);
        if (!ui_layout_is_vertical_split())
        {
            gap_center_x = (lv_coord_t)(content_w - 18U);
            gap_center_y = (lv_coord_t)((ui_layout_order_get() == ORDER_NORMAL)
                           ? (ui_anchor_h_get() + content_h / 2U)
                           : (content_h / 2U));
        }
        lv_obj_set_pos(s_dot_cont, gap_center_x - 5, gap_center_y - dot_cont_h / 2);
    }
    else if (ui_dots_position_get() == DOTS_RIGHT)
    {
        lv_coord_t content_x = (ui_layout_is_vertical_split() && ui_layout_order_get() == ORDER_NORMAL)
                               ? (lv_coord_t)(ui_anchor_w_get() + panel_gap)
                               : 0;
        lv_coord_t content_y = (!ui_layout_is_vertical_split() && ui_layout_order_get() == ORDER_NORMAL)
                               ? (lv_coord_t)ui_anchor_h_get()
                               : 0;
        lv_coord_t dots_x = content_x + content_w - 18;
        lv_coord_t dots_y = content_y + (lv_coord_t)(content_h / 2U - dot_cont_h / 2);
        lv_obj_set_pos(s_dot_cont, dots_x, dots_y);
    }
    else if (ui_dots_position_get() == DOTS_BOTTOM)
    {
        lv_coord_t dots_x = (lv_coord_t)(ui_safe_zone_w_get() / 2U - 5);
        lv_coord_t dots_y = (lv_coord_t)(ui_safe_zone_h_get() - 18U);
        lv_obj_set_pos(s_dot_cont, dots_x, dots_y);
    }

    for (uint8_t i = 0; i < DASH_PAGE_COUNT; i++)
    {
        s_scroll_dots[i] = lv_obj_create(s_dot_cont);
        lv_obj_set_size(s_scroll_dots[i], 6, 6);
        lv_obj_set_pos(s_scroll_dots[i], 2, (lv_coord_t)(i * 14));
        lv_obj_set_style_radius(s_scroll_dots[i], 0, 0);
        lv_obj_set_style_bg_color(s_scroll_dots[i], DARK, 0);
        lv_obj_set_style_bg_opa(s_scroll_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_scroll_dots[i], 0, 0);
        lv_obj_set_style_pad_all(s_scroll_dots[i], 0, 0);
        lv_obj_set_style_shadow_width(s_scroll_dots[i], 0, 0);
        lv_obj_clear_flag(s_scroll_dots[i], LV_OBJ_FLAG_SCROLLABLE);
        if (ui_dots_position_get() == DOTS_NONE)
            lv_obj_add_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
        if (i >= page_visible_dash_count())
            lv_obj_add_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
    }

    screen_update_scroll_dots(0, ui_state_get_state() == UI_DASH || ui_state_get_state() == UI_EDIT_GAS);
}

lv_obj_t *make_wall(lv_obj_t *parent, lv_coord_t y)
{
    lv_obj_t *w = lv_obj_create(parent);
    lv_coord_t wall_h = (lv_coord_t)ui_tissues_chart_h_px_get();
    lv_coord_t wall_w = (s_cached_right_w > 0) ? s_cached_right_w : (lv_coord_t)ui_content_w_get();
    lv_obj_set_size(w, wall_w, wall_h);
    lv_obj_set_pos(w, 0, y);
    lv_obj_set_style_bg_color(w, BLACK, 0);
    lv_obj_set_style_bg_opa(w, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(w, DARK, 0);
    lv_obj_set_style_border_width(w, INNER_BORDER_W, 0);
    lv_obj_set_style_border_side(w, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_all(w, 0, 0);
    lv_obj_add_flag(w, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(w, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *txt = lv_label_create(w);
    lv_obj_set_style_text_color(txt, GREEN, 0);
    lv_obj_set_style_text_font(txt, get_font(FONT_ID_TITLE), 0);
    lv_obj_set_width(txt, wall_w);
    lv_obj_set_style_text_align(txt, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(txt, 0, 12);
    lv_label_set_text(txt, "");

    lv_obj_t *blk = lv_label_create(w);
    lv_obj_set_style_text_color(blk, GREEN, 0);
    lv_obj_set_style_text_font(blk, &lv_font_courier_28, 0);
    lv_obj_set_width(blk, wall_w);
    lv_obj_set_style_text_align(blk, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(blk, 0, 50);
    lv_label_set_text(blk, "");
    return w;
}

void wall_create(void)
{
    lv_coord_t wall_h = (lv_coord_t)ui_tissues_chart_h_px_get();
    lv_coord_t content_h = (lv_coord_t)ui_content_h_get();
    lv_coord_t wall_y_bottom = (content_h > wall_h)
                               ? (lv_coord_t)(content_h - wall_h)
                               : content_h;
    s_wall_top    = make_wall(s_right_cont, 0);
    s_wall_bottom = make_wall(s_right_cont, wall_y_bottom);
    s_wall_text_top      = lv_obj_get_child(s_wall_top, 0);
    s_wall_blocks_top    = lv_obj_get_child(s_wall_top, 1);
    s_wall_text_bottom   = lv_obj_get_child(s_wall_bottom, 0);
    s_wall_blocks_bottom = lv_obj_get_child(s_wall_bottom, 1);
}

void screen_rebuild_layout(void)
{
    /* 这个入口只重建“空间关系”和“组件挂载关系”，不重建 tileview 页面集合。
     * 适用于安全区尺寸、左右翻转、dots 位置、左侧组件布局等几何类变化。 */
    lv_disp_t *disp = lv_disp_get_default();
    if (disp) lv_disp_enable_invalidation(disp, true);
    screen_mark_tiles_layout_dirty();
    clear_widget_arrays();
    if (s_left_anchor) lv_obj_clean(s_left_anchor);
    if (s_left_anchor) left_anchor_rebuild(0);
    safe_zone_reposition();
    grid_5f_rebuild_all();
    /* 重建后的新对象还没有数据，必须主动补一次全量同步。 */
    screen_refresh_all_widgets();
    restore_brightness_overlay_state();
    /* 布局重建可能让温度/电量/POD 等辅助槽位引用失效，这里再补一轮常用脏位，
     * 让后续 router 走正常刷新路径，把派生视图状态也一起拉齐。 */
    bus_requeue_dirty(DIRTY_DIVE_PROFILE | DIRTY_SYSTEM | DIRTY_GAS_SUPPLY);
}

void screen_rebuild_tileview(void)
{
    bool force_card_home = s_enter_card_home_after_layout_rebuild;
    /* 这个入口比 screen_rebuild_layout() 更重：
     * 它会销毁并重建右侧整个 tileview、菜单层和弹层。
     * 用于页面顺序、页面数量、页面类型发生变化的场景。 */
    lv_disp_t *disp = lv_disp_get_default();
    if (disp) lv_disp_enable_invalidation(disp, true);
    screen_mark_tiles_layout_dirty();
    uint8_t saved_dash_page = ui_state_get_dash_page();
    screen_page_restore_snapshot_t saved_page_snapshot =
        screen_capture_page_restore_snapshot(saved_dash_page);
    ui_state_t saved_state = ui_state_get_state();
    ui_state_t saved_sub_parent = ui_state_get_sub_parent();
    uint8_t saved_sub_idx = ui_state_get_sub_menu_idx();
    uint8_t saved_sub_depth = ui_state_get_sub_history_depth();
    uint8_t saved_info_idx = ui_state_get_menu_info_idx();
    uint8_t saved_setup_idx = ui_state_get_menu_setup_idx();
    uint8_t saved_menu_idx = (saved_state == UI_INFO) ? saved_info_idx
                             : (saved_state == UI_SETUP) ? saved_setup_idx
                             : (saved_sub_parent == UI_INFO) ? saved_info_idx
                             : (saved_sub_parent == UI_SETUP) ? saved_setup_idx
                             : 0;
    /* 先保存“用户当时在哪一页、处于什么状态”，重建后尽量回到原上下文，
     * 避免 APP 下发布局或设置变化后把用户强行踢回首页。 */
    screen_log_restore_snapshot("capture",
                                saved_state,
                                &saved_page_snapshot,
                                saved_page_snapshot.display_pos,
                                saved_state);

    memset(s_tile_objs, 0, sizeof(s_tile_objs));
    memset(g_card_custom_objs, 0, sizeof(g_card_custom_objs));
    g_card_custom_obj_count = 0;
    for (uint8_t i = 0; i < PAGE_ID_COUNT; i++)
    {
        page_t *page = page_get_by_id(i);
        if (page) page->tile_obj = NULL;
    }
    if (s_right_cont)
    {
        /* 右侧容器一旦删除，里面所有 tile / submenu / modal 对象都全部失效。
         * 所以必须先把全局缓存引用清掉，杜绝后续误用悬空指针。 */
        lv_obj_del(s_right_cont);
        s_right_cont = NULL;
        s_tileview   = NULL;
        reset_transient_ui_refs();
    }
    if (s_dot_cont)
    {
        lv_obj_del(s_dot_cont);
        s_dot_cont = NULL;
        for (uint8_t i = 0; i < DASH_PAGE_COUNT; i++) s_scroll_dots[i] = NULL;
    }
    right_panel_create();
    wall_create();
    submenu_view_create(s_right_cont,
                        s_cached_right_w > 0 ? s_cached_right_w :
                        ui_content_w_get(),
                        ui_content_h_get());
    modal_view_create(s_right_cont,
                      s_cached_right_w > 0 ? s_cached_right_w :
                      ui_content_w_get(),
                      ui_content_h_get());
    restore_brightness_overlay_state();
    if (force_card_home)
    {
        s_enter_card_home_after_layout_rebuild = false;
        screen_force_card_home_after_layout_rebuild();
        screen_log_restore_snapshot("force_card1_after_ui45",
                                    saved_state,
                                    &saved_page_snapshot,
                                    PAGE_POS_DYNAMIC_FIRST,
                                    UI_DASH);
    }
    else if (saved_state == UI_INFO)
    {
        if (screen_restore_info_state(saved_menu_idx))
        {
            screen_log_restore_snapshot("restore_info",
                                        saved_state,
                                        &saved_page_snapshot,
                                        PAGE_POS_INFO,
                                        UI_INFO);
        }
        else
        {
            uint8_t restored_dash_page = screen_restore_dash_page_or_default(&saved_page_snapshot);

            ui_state_set_state(UI_DASH);
            ui_state_set_dash_page(restored_dash_page);
            if ((restored_dash_page < page_count()) && s_tile_objs[restored_dash_page])
            {
                ui_go_to_page(restored_dash_page);
            }
            screen_log_restore_snapshot("restore_info_fallback_dash",
                                        saved_state,
                                        &saved_page_snapshot,
                                        restored_dash_page,
                                        UI_DASH);
        }
    }
    else if (saved_state == UI_SETUP)
    {
        uint8_t setup_pos = page_setup_display_pos();

        if (screen_restore_setup_state(saved_menu_idx))
        {
            screen_log_restore_snapshot("restore_setup",
                                        saved_state,
                                        &saved_page_snapshot,
                                        setup_pos,
                                        UI_SETUP);
        }
        else
        {
            uint8_t restored_dash_page = screen_restore_dash_page_or_default(&saved_page_snapshot);

            ui_state_set_state(UI_DASH);
            ui_state_set_dash_page(restored_dash_page);
            if ((restored_dash_page < page_count()) && s_tile_objs[restored_dash_page])
            {
                ui_go_to_page(restored_dash_page);
            }
            screen_log_restore_snapshot("restore_setup_fallback_dash",
                                        saved_state,
                                        &saved_page_snapshot,
                                        restored_dash_page,
                                        UI_DASH);
        }
    }
    else if (saved_state == UI_SUB_MENU)
    {
        uint8_t parent_menu_idx = (saved_sub_parent == UI_INFO) ? saved_info_idx
                                  : (saved_sub_parent == UI_SETUP) ? saved_setup_idx
                                  : 0U;

        if (saved_sub_depth == 0U &&
            screen_restore_submenu_state(saved_sub_parent, parent_menu_idx, saved_sub_idx))
        {
            uint8_t restored_pos = (saved_sub_parent == UI_INFO) ? PAGE_POS_INFO : page_setup_display_pos();
            screen_log_restore_snapshot((saved_sub_parent == UI_INFO) ? "restore_sub_info" : "restore_sub_setup",
                                        saved_state,
                                        &saved_page_snapshot,
                                        restored_pos,
                                        UI_SUB_MENU);
        }
        else if (screen_restore_menu_parent_state(saved_sub_parent, parent_menu_idx))
        {
            screen_log_restore_snapshot("restore_sub_nested_parent",
                                        saved_state,
                                        &saved_page_snapshot,
                                        (saved_sub_parent == UI_INFO) ? PAGE_POS_INFO : page_setup_display_pos(),
                                        saved_sub_parent);
        }
        else
        {
            uint8_t restored_dash_page = screen_restore_dash_page_or_default(&saved_page_snapshot);

            ui_state_set_sub_history_depth(0U);
            ui_state_set_sub_item_count(0U);
            ui_state_set_sub_menu_idx(0U);
            ui_state_set_state(UI_DASH);
            ui_state_set_dash_page(restored_dash_page);
            if ((restored_dash_page < page_count()) && s_tile_objs[restored_dash_page])
            {
                ui_go_to_page(restored_dash_page);
            }
            screen_log_restore_snapshot("restore_sub_fallback_dash",
                                        saved_state,
                                        &saved_page_snapshot,
                                        restored_dash_page,
                                        UI_DASH);
        }
    }
    else
    {
        /* 如果旧页已经不合法（例如卡片数量变了），则回退到默认动态页。 */
        uint8_t restored_dash_page = screen_restore_dash_page_or_default(&saved_page_snapshot);

        ui_state_set_state(UI_DASH);
        ui_state_set_dash_page(restored_dash_page);
        if ((restored_dash_page < page_count()) && s_tile_objs[restored_dash_page])
        {
            ui_go_to_page(restored_dash_page);
        }
        if (restored_dash_page < PAGE_POS_DYNAMIC_FIRST || restored_dash_page >= page_setup_display_pos())
        {
            ui_state_set_dash_page(PAGE_POS_DYNAMIC_FIRST);
            if (s_tile_objs[PAGE_POS_DYNAMIC_FIRST])
            {
                ui_go_to_page(PAGE_POS_DYNAMIC_FIRST);
            }
            restored_dash_page = PAGE_POS_DYNAMIC_FIRST;
        }
        screen_log_restore_snapshot("restore_dash_by_identity",
                                    saved_state,
                                    &saved_page_snapshot,
                                    restored_dash_page,
                                    UI_DASH);
    }
    if (force_card_home)
    {
        /*
         * 物理意义：
         * 最终钳位放在这里，保证任何恢复分支都不能把首页改回 INFO / SETUP。
         * 0x45 后首页必须是卡片第一页，最终显示态与状态机都要一致。
         */
        ui_state_set_sub_history_depth(0U);
        ui_state_set_sub_item_count(0U);
        ui_state_set_sub_menu_idx(0U);
        ui_state_set_sub_parent(UI_DASH);
        ui_state_set_menu_info_idx(0U);
        ui_state_set_menu_setup_idx(0U);
        ui_state_set_state(UI_DASH);
        ui_state_set_dash_page(PAGE_POS_DYNAMIC_FIRST);
        if (s_tile_objs[PAGE_POS_DYNAMIC_FIRST])
        {
            ui_go_to_page(PAGE_POS_DYNAMIC_FIRST);
        }
        screen_log_restore_snapshot("force_card1_final_clamp",
                                    saved_state,
                                    &saved_page_snapshot,
                                    PAGE_POS_DYNAMIC_FIRST,
                                    UI_DASH);
    }
    uint8_t active_idx = 0;
    if (ui_state_get_dash_page() >= PAGE_POS_DYNAMIC_FIRST && ui_state_get_dash_page() < page_setup_display_pos())
    {
        for (uint8_t pos = PAGE_POS_DYNAMIC_FIRST; pos < ui_state_get_dash_page(); pos++)
        {
            uint8_t page_id = page_id_at(pos);
            if (page_id != PAGE_ID_UNUSED)
            {
                active_idx++;
            }
        }
    }
    screen_update_scroll_dots(active_idx, ui_state_get_state() == UI_DASH || ui_state_get_state() == UI_EDIT_GAS);
}

void screen_rebuild_full(void)
{
    /* Full rebuild = 先重建页面集合，再重建布局。
     * 顺序不能反：因为 layout 依赖 tileview/右侧容器已经重新创建完成。 */
    bool force_card_home = s_enter_card_home_after_layout_rebuild;
    screen_rebuild_tileview();
    screen_rebuild_layout();

    /*
     * 物理意义：
     * tileview 和 layout 是两段独立的重建流程。前者负责页面树，后者负责几何和组件挂载。
     * 0x45 属于“结构级布局替换”，如果只在 tileview 阶段钳位首页，layout 阶段的补刷
     * 仍可能把最终可见态拉回旧 dash_page。
     * 所以这里再补一次最终钳位，保证整次重建事务结束时，状态机和可见页都停在新布局首页。
     */
    if (force_card_home)
    {
        screen_force_card_home_after_layout_rebuild();
    }
}
