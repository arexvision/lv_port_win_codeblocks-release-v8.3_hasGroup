#include "screen.h"
#include "screen_layout.h"
#include "screen_dots.h"
#include "screen_overlay.h"
#include "layout_view.h"
#include "../comp/comp_update.h"
#include "../comp/comp_view.h"
#include "../core/ui_engine.h"
#include "../core/data.h"
#include "../core/ui_vm.h"
#include "../core/vm/ui_vm_system_view_types.h"
#include "../core/ui_state.h"
#include "page_registry.h"
#include "../core/callbacks.h"
#include "../views/modal_view.h"
#include "../views/submenu_types.h"
#include "../views/submenu_model.h"
#include "../views/submenu_view.h"
#include "../cards/card_compass.h"
#include "../fonts/fonts.h"
#include <stdio.h>
#include <string.h>

/* =========================================================
 * 内部句柄
 * ========================================================= */
lv_obj_t *s_scr;
lv_obj_t *s_safe_zone;        /* 安全区容器(绝对坐标原点) */
lv_obj_t *s_left_anchor;      /* 左侧锚点 (10U 沙盒) */
lv_obj_t *s_right_cont;       /* clip container */
lv_obj_t *s_tileview;
lv_obj_t *s_tile_objs[PAGE_COUNT];

/* 灯光控制状态（LIGHT CONTROL 子菜单全局共享） */
/* 问题4修复：灯光硬件默认开启，UI 初始值必须匹配硬件状态 */
bool g_light_power_state = false;

/* Wall indicators */
lv_obj_t *s_wall_top;
lv_obj_t *s_wall_bottom;
lv_obj_t *s_wall_text_top,    *s_wall_blocks_top;
lv_obj_t *s_wall_text_bottom, *s_wall_blocks_bottom;

/* Scroll dots */
lv_obj_t *s_dot_cont;  /* dots 容器（父对象 s_safe_zone，可定位到间隙中间） */
lv_obj_t *s_scroll_dots[DASH_PAGE_COUNT];

lv_obj_t *s_brightness_overlay;
bool s_software_brightness_enabled = true;

/* INFO / SETUP list objects */
lv_obj_t *s_info_list;
lv_obj_t *s_setup_list;

/* 排版缓存 (避免每次重算) */
uint16_t s_cached_right_w = 0;

/* Forward declarations for static functions */
void wall_create(void);
void reset_transient_ui_refs(void);
void edit_flash_stop(void);
void restore_brightness_overlay_state(void);

/* =========================================================
 * 样式 (静态初始化一次)
 * ========================================================= */
lv_style_t s_style_screen;
lv_style_t s_style_panel;
lv_style_t s_style_anchor_bg;
lv_style_t s_style_label_huge;
lv_style_t s_style_label_med;
lv_style_t s_style_label_small;
lv_style_t s_style_title_zone;
lv_style_t s_style_val_zone;
lv_style_t s_style_menu_item;
lv_style_t s_style_menu_item_active;
lv_style_t s_style_sep_line;
static bool       s_styles_inited = false;

void reset_transient_ui_refs(void)
{
    s_wall_top = NULL;
    s_wall_bottom = NULL;
    s_wall_text_top = NULL;
    s_wall_blocks_top = NULL;
    s_wall_text_bottom = NULL;
    s_wall_blocks_bottom = NULL;
    modal_view_reset();
    submenu_view_reset();
    s_info_list = NULL;
    s_setup_list = NULL;
    s_edit_flash_badge = NULL;
    s_edit_flash_val_lbl = NULL;
    edit_flash_stop();

    ui_state_set_sub_history_depth(0U);
    ui_state_set_sub_item_count(0U);
    ui_state_set_sub_menu_idx(0U);
    ui_state_set_gas_modal_from_submenu(false);
    ui_state_set_edit_active(false);
}

void restore_brightness_overlay_state(void)
{
    ui_vm_brightness_t vm;

    if (s_scr == NULL)
    {
        return;
    }

    /* 亮度遮罩挂在根屏 s_scr 上，布局翻转/重建右侧容器时不会被删除。
     * 这里必须保留对象引用并在重建后立即按当前档位重放一次，
     * 否则 APP 下发布局后会出现旧遮罩残留或前后亮度现象不一致。 */
    ui_vm_brightness_update(&vm);
    apply_software_brightness(vm.brightness_level);
}

static void styles_init(void)
{
    if (s_styles_inited) return;
    s_styles_inited = true;

    lv_style_init(&s_style_screen);
    lv_style_set_bg_color(&s_style_screen, BG);
    lv_style_set_bg_opa(&s_style_screen, LV_OPA_COVER);
    lv_style_set_pad_all(&s_style_screen, 0);
    lv_style_set_border_width(&s_style_screen, 0);

    lv_style_init(&s_style_anchor_bg);
    lv_style_set_bg_color(&s_style_anchor_bg, BLACK);
    lv_style_set_bg_opa(&s_style_anchor_bg, LV_OPA_COVER);
    lv_style_set_border_color(&s_style_anchor_bg, DARK);
    lv_style_set_border_width(&s_style_anchor_bg, DEBUG_BORDERS ? 1 : 0);
    lv_style_set_pad_all(&s_style_anchor_bg, 0);     /* 必须显式清零，否则 LVGL 默认 padding 会偏移所有子组件坐标 */
    lv_style_set_radius(&s_style_anchor_bg, 0);

    lv_style_init(&s_style_panel);
    lv_style_set_bg_color(&s_style_panel, BLACK);
    lv_style_set_bg_opa(&s_style_panel, LV_OPA_COVER);
    lv_style_set_pad_all(&s_style_panel, 0);
    lv_style_set_border_width(&s_style_panel, 0);

    lv_style_init(&s_style_label_huge);
    lv_style_set_text_color(&s_style_label_huge, GREEN);
    lv_style_set_text_font(&s_style_label_huge, get_font(FONT_ID_HUGE));

    lv_style_init(&s_style_label_med);
    lv_style_set_text_color(&s_style_label_med, GREEN);
    lv_style_set_text_font(&s_style_label_med, get_font(FONT_ID_MEDIUM));

    lv_style_init(&s_style_label_small);
    lv_style_set_text_color(&s_style_label_small, LIGHT);
    lv_style_set_text_font(&s_style_label_small, get_font(FONT_ID_SMALL));

    lv_style_init(&s_style_title_zone);
    lv_style_set_text_color(&s_style_title_zone, LIGHT);
    lv_style_set_text_font(&s_style_title_zone, get_font(FONT_ID_SMALL));
    lv_style_set_bg_opa(&s_style_title_zone, LV_OPA_TRANSP);

    lv_style_init(&s_style_val_zone);
    lv_style_set_text_color(&s_style_val_zone, GREEN);
    lv_style_set_bg_opa(&s_style_val_zone, LV_OPA_TRANSP);

    lv_style_init(&s_style_sep_line);
    lv_style_set_bg_color(&s_style_sep_line, DARK);
    lv_style_set_bg_opa(&s_style_sep_line, LV_OPA_50);
    lv_style_set_border_width(&s_style_sep_line, 0);

    lv_style_init(&s_style_menu_item);
    lv_style_set_bg_color(&s_style_menu_item, BLACK);
    lv_style_set_bg_opa(&s_style_menu_item, LV_OPA_COVER);
    lv_style_set_text_color(&s_style_menu_item, GREEN);
    lv_style_set_border_color(&s_style_menu_item, DARK);
    lv_style_set_border_width(&s_style_menu_item, INNER_BORDER_W);
    lv_style_set_pad_all(&s_style_menu_item, 12);

    lv_style_init(&s_style_menu_item_active);
    lv_style_set_bg_color(&s_style_menu_item_active, GREEN);
    lv_style_set_bg_opa(&s_style_menu_item_active, LV_OPA_COVER);
    lv_style_set_text_color(&s_style_menu_item_active, BLACK);
    lv_style_set_border_color(&s_style_menu_item_active, GREEN);
}

/* =========================================================
 * 辅助函数
 * ========================================================= */

/* 清空 ascent/NDL widget 句柄数组（在任何网格渲染之前调用）
 * screen_rebuild_layout() 和 left_anchor_create() 入口各调用一次，
 * 确保数组从干净状态开始，两侧网格渲染函数均以追加模式运行。 */
/* =========================================================
 * 统一重置 UI 渲染状态（防止悬空指针访问）
 * 调用链：screen_rebuild_layout -> clear_widget_arrays
 * ========================================================= */
/* =========================================================
 * screen_create - 公开入口
 * ========================================================= */
void screen_create(void)
{
    styles_init();

    s_scr = lv_obj_create(NULL);
    lv_obj_add_style(s_scr, &s_style_screen, 0);

    /* 安全区容器(相对 s_scr 居中定位) */
    s_safe_zone = lv_obj_create(s_scr);
    lv_obj_set_size(s_safe_zone, ui_safe_zone_w_get(), ui_safe_zone_h_get());
    lv_obj_align(s_safe_zone, LV_ALIGN_CENTER,
                 ui_offset_x_get(), ui_offset_y_get());
    lv_obj_set_style_bg_opa(s_safe_zone, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_safe_zone, 0, 0);
    lv_obj_set_style_pad_all(s_safe_zone, 0, 0);
    lv_obj_clear_flag(s_safe_zone, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_border_color(s_safe_zone, DARK, 0);
    lv_obj_set_style_border_width(s_safe_zone, DEBUG_BORDERS ? 1 : 0, 0);

    left_anchor_create();
    right_panel_create();
    wall_create();
    submenu_view_create(s_right_cont,
                             s_cached_right_w > 0 ? s_cached_right_w :
                             (uint16_t)(ui_safe_zone_w_get() - LEFT_ANCHOR_W -
                                        ui_panel_gap_px_get()),
                             ui_safe_zone_h_get());
    modal_view_create(s_right_cont,
                           s_cached_right_w > 0 ? s_cached_right_w :
                           (uint16_t)(ui_safe_zone_w_get() - LEFT_ANCHOR_W -
                                      ui_panel_gap_px_get()),
                           ui_safe_zone_h_get());
    restore_brightness_overlay_state();

    lv_scr_load(s_scr);
}

/* =========================================================
 * Tileview 导航
 * ========================================================= */
void screen_scroll_to_page(uint8_t tile_pos)
{
    /* 【问题三修复】s_tileview 可能为 NULL（布局重建期间） */
    if (!s_tileview) return;

    if (tile_pos >= page_count())
    {
        return;
    }
    lv_obj_t *tile = s_tile_objs[tile_pos];
    if (!tile)
    {
        return;
    }

    if (tile_pos == 0)
    {
        lv_anim_del(s_tileview, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_obj_set_y(s_tileview, 0);
    }

    lv_obj_set_tile(s_tileview, tile, TILE_ANIM_ENABLED ? LV_ANIM_ON : LV_ANIM_OFF);

    /* 首屏/重建后首次进卡时，当前 tile 可能没有后续脏数据驱动刷新
     * 这里主动补一次当前可见页的布局和重绘，避免必须等用户旋钮交互后才完整显示。 */
    lv_obj_update_layout(tile);
    lv_obj_invalidate(tile);

    if (g_sys_page_order(tile_pos) == PAGE_ID_COMPASS)
    {
        card_compass_refresh_heading(true);
    }

    /* SETUP（最后一页）不显示 dots，只有 DASH 动态页面才更新 */
    if (tile_pos >= PAGE_POS_DYNAMIC_FIRST && tile_pos < page_setup_display_pos())
    {
        /* 计算逻辑索引：从 DYNAMIC_FIRST 到 tile_pos 之间有多少个有效页面 */
        uint8_t active_idx = 0;
        for (uint8_t pos = PAGE_POS_DYNAMIC_FIRST; pos < tile_pos; pos++)
        {
            uint8_t page_id = g_sys_page_order(pos);
            if (page_id != PAGE_ID_UNUSED && page_id != PAGE_ID_BLANK)
            {
                active_idx++;
            }
        }
        screen_update_scroll_dots(active_idx, true);
    }
    else
    {
        screen_update_scroll_dots(0, false);
    }
}

/* =========================================================
 * 左侧面板刷新 (使用 comp_set_value API)
 *
 * 彻底委托 comp_set_value()，通过 user_data 烙印自动定位
 * 左侧锚点 + 5F 网格中所有打了烙印的组件并更新数值
 * 不再直接操作 s_lbl_* 句柄，彻底解耦！
 *
 * 2x7 网格布局:
 *   Row 0: NDL      | (2x1)
 *   Row 1-2: DEPTH  | (2x2, 带 sudu 速率图标)
 *   Row 3: POD1     | POD2   (各 1x1)
 *   Row 4: TIME     | (2x1, 潜水时间 MM:SS)
 *   Row 5: GAS      | (2x1, 当前气体名称)
 *   Row 6: SYS      | (2x1, 系统时间)
 * ========================================================= */
/* =========================================================
 * 左侧面板刷新
 *
 * 布局层只读取 ui_state / 配置查询接口，组件展示数据通过 comp_sync_data -> VM 链路更新。
 * 调整左侧 widget 顺序或类型后，这里不需要手工改每个组件的数据绑定。
 * ========================================================= */
/* =========================================================
 * 全屏组件数据同步接口
 *
 * 同时刷新左侧锚点和右侧 5F 自定义网格的所有组件
 * 内部调用 comp_sync_data() 路由分发器，实现全量数据自动对齐
 * ========================================================= */
void screen_refresh_all_widgets(void)
{
    /* 1. 同步左侧固定区配置 */
    for (uint8_t i = 0; i < ui_left_widget_count_get(); i++)
    {
        const grid_widget_t *widget = ui_left_widget_get(i);
        if (widget && widget->widget_id != COMP_EMPTY)
        {
            comp_sync_data(widget->widget_id);
        }
    }

    /* 2. 同步右侧全部自定义卡片配置 */
    for (uint8_t page_idx = 0;
            page_idx < ui_custom_card_count_get() && page_idx < MAX_CUSTOM_CARDS;
            page_idx++)
    {
        for (uint8_t i = 0; i < ui_custom_card_widget_count_get(page_idx); i++)
        {
            const grid_widget_t *widget = ui_custom_card_widget_get(page_idx, i);
            if (widget && widget->widget_id != COMP_EMPTY)
            {
                comp_sync_data(widget->widget_id);
            }
        }
    }
}

/* =========================================================
 * 兼容旧接口：仅刷新左侧面板
 * 内部委托 comp_sync_data()，消除冗余 switch-case
 * ========================================================= */
void screen_refresh_left_panel(void)
{
    for (uint8_t i = 0; i < ui_left_widget_count_get(); i++)
    {
        const grid_widget_t *widget = ui_left_widget_get(i);
        if (widget && widget->widget_id != COMP_EMPTY)
        {
            comp_sync_data(widget->widget_id);
        }
    }
}

/* =========================================================
 * Wall indicators
 * ========================================================= */
static const char *charge_blocks[] =
{
    "[   ]   [   ]   [   ]",
    "[ \xE2\x96\xA0 ]   [   ]   [   ]",
    "[ \xE2\x96\xA0 ]   [ \xE2\x96\xA0 ]   [   ]",
    "[ \xE2\x96\xA0 ]   [ \xE2\x96\xA0 ]   [ \xE2\x96\xA0 ]",
};

static void wall_nudge_tileview(lv_coord_t offset_y)
{
    /* 【问题三修复】s_tileview 可能为 NULL（布局重建期间） */
    if (!s_tileview) return;

    lv_anim_del(s_tileview, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_coord_t cur_y = lv_obj_get_y(s_tileview);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_tileview);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&a, cur_y, offset_y);
    lv_anim_set_time(&a, 350);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

void screen_show_wall(wall_side_t side, uint8_t charge, const char *text)
{
    /* 【问题三修复】s_tileview 可能为 NULL（布局重建期间） */
    if (!s_tileview) return;

    if (charge > 3) charge = 3;

    lv_obj_t *wall    = (side == WALL_TOP) ? s_wall_top    : s_wall_bottom;
    lv_obj_t *txt_lbl = (side == WALL_TOP) ? s_wall_text_top    : s_wall_text_bottom;
    lv_obj_t *blk_lbl = (side == WALL_TOP) ? s_wall_blocks_top  : s_wall_blocks_bottom;
    if (!wall || !txt_lbl || !blk_lbl) return;

    lv_label_set_text(txt_lbl, text);
    lv_label_set_text(blk_lbl, charge_blocks[charge]);
    lv_obj_clear_flag(wall, LV_OBJ_FLAG_HIDDEN);

    lv_coord_t nudge = (lv_coord_t)(charge * 20);
    wall_nudge_tileview(side == WALL_TOP ? nudge : -nudge);
}

void screen_hide_walls(void)
{
    /* 【问题三修复】s_tileview 可能为 NULL（布局重建期间） */
    if (!s_tileview) return;
    if (!s_wall_top || !s_wall_bottom) return;

    lv_obj_add_flag(s_wall_top,    LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_wall_bottom, LV_OBJ_FLAG_HIDDEN);
    lv_coord_t cur_y = lv_obj_get_y(s_tileview);
    if (cur_y == 0) return;
    lv_anim_del(s_tileview, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_tileview);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&a, cur_y, 0);
    lv_anim_set_time(&a, 350);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

void screen_hide_walls_snap(void)
{
    /* 【问题三修复】s_tileview 可能为 NULL（布局重建期间） */
    if (!s_tileview) return;
    if (!s_wall_top || !s_wall_bottom) return;

    lv_obj_add_flag(s_wall_top,    LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_wall_bottom, LV_OBJ_FLAG_HIDDEN);
    lv_anim_del(s_tileview, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_obj_set_y(s_tileview, 0);
}

/* =========================================================
 * Info / Setup list
 * ========================================================= */
void screen_set_info_selection(uint8_t idx)
{
    if (!s_info_list) return;
    uint32_t cnt = lv_obj_get_child_cnt(s_info_list);
    for (uint32_t i = 0; i < cnt; i++)
    {
        lv_obj_t *item = lv_obj_get_child(s_info_list, i);
        lv_obj_t *lbl  = lv_obj_get_child(item, 0);
        if (i == idx)
        {
            lv_obj_set_style_bg_color(item, BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(item, GREEN, 0);
            lv_obj_set_style_border_width(item, INNER_BORDER_W + 2, 0);
            if (lbl)
            {
                lv_obj_set_style_text_color(lbl, LIGHT, 0);
                lv_obj_set_style_text_font(lbl, get_font(FONT_ID_MEDIUM), 0);
            }
        }
        else
        {
            lv_obj_set_style_bg_color(item, BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(item, DARK, 0);
            lv_obj_set_style_border_width(item, INNER_BORDER_W, 0);
            if (lbl)
            {
                lv_obj_set_style_text_color(lbl, GREEN, 0);
                lv_obj_set_style_text_font(lbl, get_font(FONT_ID_TITLE), 0);
            }
        }
    }
}

uint8_t screen_info_item_count(void)
{
    if (!s_info_list) return 0;
    return (uint8_t)lv_obj_get_child_cnt(s_info_list);
}

void screen_set_setup_selection(uint8_t idx)
{
    if (!s_setup_list) return;
    uint32_t cnt = lv_obj_get_child_cnt(s_setup_list);
    for (uint32_t i = 0; i < cnt; i++)
    {
        lv_obj_t *item  = lv_obj_get_child(s_setup_list, i);
        lv_obj_t *lbl   = lv_obj_get_child(item, 0);
        lv_obj_t *badge = lv_obj_get_child(item, 1);
        if (i == idx)
        {
            lv_obj_set_style_bg_color(item, BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(item, GREEN, 0);
            lv_obj_set_style_border_width(item, INNER_BORDER_W + 2, 0);
            if (lbl)
            {
                lv_obj_set_style_text_color(lbl, LIGHT, 0);
                lv_obj_set_style_text_font(lbl, get_font(FONT_ID_MEDIUM), 0);
            }
            if (badge)
            {
                lv_obj_set_style_text_color(badge, LIGHT, 0);
                lv_obj_set_style_text_font(badge, get_font(FONT_ID_TITLE), 0);
            }
        }
        else
        {
            lv_obj_set_style_bg_color(item, BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(item, DARK, 0);
            lv_obj_set_style_border_width(item, INNER_BORDER_W, 0);
            if (lbl)
            {
                lv_obj_set_style_text_color(lbl, GREEN, 0);
                lv_obj_set_style_text_font(lbl, get_font(FONT_ID_TITLE), 0);
            }
            if (badge)
            {
                lv_obj_set_style_text_color(badge, LIGHT, 0);
                lv_obj_set_style_text_font(badge, get_font(FONT_ID_SMALL), 0);
            }
        }
    }
}

uint8_t screen_setup_item_count(void)
{
    if (!s_setup_list) return 0;
    return (uint8_t)lv_obj_get_child_cnt(s_setup_list);
}

void screen_register_info_list(lv_obj_t *list)
{
    s_info_list  = list;
}
void screen_register_setup_list(lv_obj_t *list)
{
    s_setup_list = list;
}

/* =========================================================
 * Setup badge update
 * ========================================================= */
void screen_update_setup_badge(uint8_t item_idx, const char *value)
{
    if (!s_setup_list) return;
    lv_obj_t *item = lv_obj_get_child(s_setup_list, item_idx);
    if (!item) return;
    lv_obj_t *badge = lv_obj_get_child(item, 1);
    if (!badge) return;
    lv_label_set_text(badge, value ? value : "");
}

/* =========================================================
 * Compass / Gas / Edit callbacks
 * ========================================================= */
void screen_refresh_compass_target(void)
{
    page_t *c = page_get_by_id(PAGE_ID_COMPASS);
    if (c && c->update_cb) c->update_cb();
}

void screen_refresh_gas_menu(void)
{
    page_t *c = page_get_by_id(PAGE_ID_GAS);
    if (c && c->update_cb) c->update_cb();
}

void screen_refresh_setup_menu(void)
{
    page_t *c = page_get_by_id(PAGE_ID_SETUP);
    if (c && c->update_cb) c->update_cb();
}


/* =========================================================
 * Card title helper
 * ========================================================= */
lv_obj_t *screen_make_card_title(lv_obj_t *parent, const char *text)
{
    uint16_t right_w_fallback = ui_safe_zone_w_get() - LEFT_ANCHOR_W
                                - ui_panel_gap_px_get();
    uint16_t right_w = (s_cached_right_w > 0) ? s_cached_right_w : right_w_fallback;

    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_style_text_color(lbl, LIGHT, 0);
    lv_obj_set_style_text_font(lbl, get_font(FONT_ID_TITLE), 0);
    lv_obj_set_pos(lbl, 16, 8);
    lv_obj_set_size(lbl, right_w - 32, 40);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(lbl, text);

    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_set_size(line, right_w - 32, 2);
    lv_obj_set_pos(line, 16, 48);
    lv_obj_set_style_bg_color(line, DARK, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    lv_obj_set_style_radius(line, 0, 0);

    return lbl;
}

