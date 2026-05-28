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
    lv_obj_set_size(s_left_anchor, LEFT_ANCHOR_W, ui_safe_zone_h_get());
    lv_obj_clean(s_left_anchor);
    render_left_anchor_grid(s_left_anchor);
}

void left_anchor_create(void)
{
    /* 左锚点是整屏布局的固定参考物，负责承载 2x7 左侧信息网格。 */
    s_left_anchor = lv_obj_create(s_safe_zone);
    lv_obj_set_size(s_left_anchor, LEFT_ANCHOR_W, ui_safe_zone_h_get());
    if (ui_layout_order_get() == ORDER_NORMAL)
    {
        /* 正常布局时左锚点固定贴左。 */
        lv_obj_set_pos(s_left_anchor, 0, 0);
    }
    else
    {
        /* 翻转布局时左锚点移动到右侧。 */
        uint16_t panel_gap = ui_panel_gap_px_get();
        uint16_t right_w = ui_safe_zone_w_get() - LEFT_ANCHOR_W - panel_gap;
        lv_obj_set_pos(s_left_anchor, (lv_coord_t)(right_w + panel_gap), 0);
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
    uint16_t right_w = ui_safe_zone_w_get() - LEFT_ANCHOR_W - panel_gap;

    if (ui_layout_order_get() == ORDER_NORMAL)
    {
        /* 正常模式：左锚点在左，右内容区在右。 */
        lv_obj_set_pos(s_left_anchor, 0, 0);
        lv_obj_set_pos(s_right_cont, (lv_coord_t)(LEFT_ANCHOR_W + panel_gap), 0);
    }
    else
    {
        /* 翻转模式：右内容区在左，左锚点在右。 */
        lv_obj_set_pos(s_right_cont, 0, 0);
        lv_obj_set_pos(s_left_anchor, (lv_coord_t)right_w + panel_gap, 0);
    }

    if (s_left_anchor)
    {
        lv_obj_move_foreground(s_left_anchor);
        lv_obj_invalidate(s_left_anchor);
    }

    lv_obj_set_size(s_right_cont, right_w, ui_safe_zone_h_get());
    s_cached_right_w = right_w;

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
            gap_center_x = (ui_layout_order_get() == ORDER_NORMAL)
                           ? (lv_coord_t)(LEFT_ANCHOR_W + panel_gap / 2)
                           : (lv_coord_t)(right_w + panel_gap / 2);
            dot_x = gap_center_x - 5;
            dot_y = gap_center_y - (lv_coord_t)(dot_cont_h / 2);
        }
        else if (ui_dots_position_get() == DOTS_RIGHT)
        {
            lv_coord_t right_cont_x = (ui_layout_order_get() == ORDER_NORMAL)
                                      ? (lv_coord_t)(LEFT_ANCHOR_W + panel_gap)
                                      : 0;
            dot_x = right_cont_x + (lv_coord_t)right_w - 18;
            dot_y = (lv_coord_t)(ui_safe_zone_h_get() / 2U - (int)dot_cont_h / 2);
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
    uint16_t right_w = ui_safe_zone_w_get() - LEFT_ANCHOR_W - panel_gap;
    s_cached_right_w = right_w;

    s_right_cont = lv_obj_create(s_safe_zone);
    lv_obj_set_size(s_right_cont, right_w, ui_safe_zone_h_get());
    if (ui_layout_order_get() == ORDER_NORMAL)
    {
        lv_obj_set_pos(s_right_cont, (lv_coord_t)(LEFT_ANCHOR_W + panel_gap), 0);
    }
    else
    {
        lv_obj_set_pos(s_right_cont, 0, 0);
    }
    lv_obj_set_style_bg_color(s_right_cont, BLACK, 0);
    lv_obj_set_style_bg_opa(s_right_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_right_cont, 0, 0);
    lv_obj_set_style_border_width(s_right_cont, 0, 0);
    lv_obj_clear_flag(s_right_cont, LV_OBJ_FLAG_SCROLLABLE);

    s_tileview = lv_tileview_create(s_right_cont);
    lv_obj_set_size(s_tileview, right_w, ui_safe_zone_h_get());
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
                       ? (lv_coord_t)(LEFT_ANCHOR_W + panel_gap / 2)
                       : (lv_coord_t)(right_w + panel_gap / 2);
        lv_obj_set_pos(s_dot_cont, gap_center_x - 5, gap_center_y - dot_cont_h / 2);
    }
    else if (ui_dots_position_get() == DOTS_RIGHT)
    {
        lv_coord_t right_cont_x = (ui_layout_order_get() == ORDER_NORMAL)
                                  ? (lv_coord_t)(LEFT_ANCHOR_W + panel_gap)
                                  : 0;
        lv_coord_t dots_x = right_cont_x + right_w - 18;
        lv_coord_t dots_y = (lv_coord_t)(ui_safe_zone_h_get() / 2U - dot_cont_h / 2);
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
    lv_coord_t wall_w = (s_cached_right_w > 0) ? s_cached_right_w : (lv_coord_t)(ui_safe_zone_w_get() - LEFT_ANCHOR_W);
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
    lv_coord_t wall_y_bottom = (ui_safe_zone_h_get() > (uint16_t)wall_h)
                               ? (lv_coord_t)(ui_safe_zone_h_get() - (uint16_t)wall_h)
                               : (lv_coord_t)ui_safe_zone_h_get();
    s_wall_top    = make_wall(s_right_cont, 0);
    s_wall_bottom = make_wall(s_right_cont, wall_y_bottom);
    s_wall_text_top      = lv_obj_get_child(s_wall_top, 0);
    s_wall_blocks_top    = lv_obj_get_child(s_wall_top, 1);
    s_wall_text_bottom   = lv_obj_get_child(s_wall_bottom, 0);
    s_wall_blocks_bottom = lv_obj_get_child(s_wall_bottom, 1);
}

void screen_rebuild_layout(void)
{
    printf("[REBUILD_LAYOUT] Enter: visible_dash=%u, dots_pos=%d, layout_order=%d\r\n",
           page_visible_dash_count(), ui_dots_position_get(), ui_layout_order_get());
    /* 这个入口只重建“空间关系”和“组件挂载关系”，不重建 tileview 页面集合。
     * 适用于安全区尺寸、左右翻转、dots 位置、左侧组件布局等几何类变化。 */
    lv_disp_t *disp = lv_disp_get_default();
    if (disp) lv_disp_enable_invalidation(disp, true);
    clear_widget_arrays();
    if (s_left_anchor) lv_obj_clean(s_left_anchor);
    if (s_left_anchor) left_anchor_rebuild(0);
    grid_5f_rebuild_all();
    safe_zone_reposition();
    /* 重建后的新对象还没有数据，必须主动补一次全量同步。 */
    screen_refresh_all_widgets();
    restore_brightness_overlay_state();
    /* 布局重建可能让温度/电量/POD 等辅助槽位引用失效，这里再补一轮常用脏位，
     * 让后续 router 走正常刷新路径，把派生视图状态也一起拉齐。 */
    bus_requeue_dirty(DIRTY_DEPTH | DIRTY_BATT | DIRTY_TEMP | DIRTY_POD);
}

void screen_rebuild_tileview(void)
{
    /* 这个入口比 screen_rebuild_layout() 更重：
     * 它会销毁并重建右侧整个 tileview、菜单层和弹层。
     * 用于页面顺序、页面数量、页面类型发生变化的场景。 */
    lv_disp_t *disp = lv_disp_get_default();
    if (disp) lv_disp_enable_invalidation(disp, true);
    uint8_t count = page_count();
    uint8_t saved_dash_page = ui_state_get_dash_page();
    ui_state_t saved_state = ui_state_get_state();
    uint8_t saved_menu_idx = (saved_state == UI_INFO) ? ui_state_get_menu_info_idx()
                             : (saved_state == UI_SETUP) ? ui_state_get_menu_setup_idx()
                             : 0;
    /* 先保存“用户当时在哪一页、处于什么状态”，重建后尽量回到原上下文，
     * 避免 APP 下发布局或设置变化后把用户强行踢回首页。 */

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
                        (uint16_t)(ui_safe_zone_w_get() - LEFT_ANCHOR_W - ui_panel_gap_px_get()),
                        ui_safe_zone_h_get());
    modal_view_create(s_right_cont,
                      s_cached_right_w > 0 ? s_cached_right_w :
                      (uint16_t)(ui_safe_zone_w_get() - LEFT_ANCHOR_W - ui_panel_gap_px_get()),
                      ui_safe_zone_h_get());
    restore_brightness_overlay_state();
    if (s_tileview && saved_dash_page < PAGE_COUNT && s_tile_objs[saved_dash_page])
    {
        /* 尽量恢复重建前用户正在看的那一页。 */
        lv_obj_set_tile(s_tileview, s_tile_objs[saved_dash_page], LV_ANIM_OFF);
    }
    if (ENABLE_INFO_MENU && saved_dash_page == PAGE_POS_INFO)
    {
        ui_state_set_state(UI_INFO);
        ui_state_set_menu_info_idx(saved_menu_idx);
    }
    else if (saved_dash_page == page_setup_display_pos())
    {
        ui_state_set_state(UI_SETUP);
        ui_state_set_menu_setup_idx(saved_menu_idx);
    }
    else
    {
        /* 如果旧页已经不合法（例如卡片数量变了），则回退到默认动态页。 */
        ui_state_set_state(UI_DASH);
        if (saved_dash_page < PAGE_POS_DYNAMIC_FIRST || saved_dash_page >= page_setup_display_pos())
        {
            ui_state_set_dash_page(PAGE_POS_DYNAMIC_FIRST);
            if (s_tileview && s_tile_objs[PAGE_POS_DYNAMIC_FIRST])
            {
                lv_obj_set_tile(s_tileview, s_tile_objs[PAGE_POS_DYNAMIC_FIRST], LV_ANIM_OFF);
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

void screen_rebuild_full(void)
{
    /* Full rebuild = 先重建页面集合，再重建布局。
     * 顺序不能反：因为 layout 依赖 tileview/右侧容器已经重新创建完成。 */
    screen_rebuild_tileview();
    screen_rebuild_layout();
}
