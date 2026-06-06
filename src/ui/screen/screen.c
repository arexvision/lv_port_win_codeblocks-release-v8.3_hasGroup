/*
 * 文件: src/app_ui/ui/screen/screen.c
 * 作用: 该文件属于屏幕或页面编排模块，负责整屏布局、分页切换、覆盖层、编辑态显示或页面注册管理。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

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
bool g_light_power_state = true;
light_mode_t g_light_mode_state = LIGHT_MODE_ALWAYS;

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
static void menu_list_ensure_visible(lv_obj_t *list, uint8_t idx);

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
    /* 布局重建前先把所有“临时可失效引用”清空，防止旧对象残留。 */
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

    /* 子菜单/编辑态相关状态也一起回到初始值，确保下一次进入页面时完全干净。 */
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
    /* 只初始化一次，避免重复创建样式对象。 */
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
    /* 先建样式，再建对象，最后一次性切屏，避免中间状态闪烁。 */
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

    /* 左锚点负责承载 10U 沙盒与左侧固定信息。 */
    left_anchor_create();
    /* 右侧面板负责承载 tileview、菜单和弹层。 */
    right_panel_create();
    /* 壁面提示用于边界菜单切换的“蓄力确认”反馈。 */
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

    /* 所有对象准备完成后再加载屏幕，避免用户看到半成品布局。 */
    lv_scr_load(s_scr);
}

/* =========================================================
 * Tileview 导航
 * ========================================================= */
void screen_scroll_to_page(uint8_t tile_pos)
{
    /* 【问题三修复】s_tileview 可能为 NULL（布局重建期间） */
    if (!s_tileview) return;

    /* tile_pos 代表显示顺序，不是底层 card_order 的原始索引。 */
    /* screen 层所有翻页逻辑都基于“显示序号”运作，
     * 真正的页面身份需要再通过 page_registry 做一层转换。 */
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
        /* 首屏需要清掉残留动画，保证返回 INFO 页时位置绝对归零。 */
        lv_anim_del(s_tileview, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_obj_set_y(s_tileview, 0);
    }

    lv_obj_set_tile(s_tileview, tile, TILE_ANIM_ENABLED ? LV_ANIM_ON : LV_ANIM_OFF);
    /* 到这里 tileview 只是切到了目标 tile，并不代表 tile 内所有内容都已经完成重排。
     * 对于刚重建完或首次进入的页面，主动补 layout/invalidate 可以减少首帧残缺。 */

    /* 首屏/重建后首次进卡时，当前 tile 可能没有后续脏数据驱动刷新
     * 这里主动补一次当前可见页的布局和重绘，避免必须等用户旋钮交互后才完整显示。 */
    lv_obj_update_layout(tile);
    lv_obj_invalidate(tile);

    /* 罗盘页进入时需要立刻补一次航向刷新，避免显示滞后。 */
    if (page_id_at(tile_pos) == PAGE_ID_COMPASS)
    {
        card_compass_refresh_heading(true);
    }

    /* SETUP（最后一页）不显示 dots，只有 DASH 动态页面才更新 */
    if (tile_pos >= PAGE_POS_DYNAMIC_FIRST && tile_pos < page_setup_display_pos())
    {
        /* 根据当前可见动态页数量重新计算小圆点高亮位置。 */
        /* 计算逻辑索引：从 DYNAMIC_FIRST 到 tile_pos 之间有多少个有效页面 */
        uint8_t active_idx = 0;
        for (uint8_t pos = PAGE_POS_DYNAMIC_FIRST; pos < tile_pos; pos++)
        {
            uint8_t page_id = page_id_at(pos);
            if (page_id != PAGE_ID_UNUSED)
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
    /* 注意这里不直接保存每个组件的 label 句柄，而是按配置遍历 widget_id。
     * 好处是布局变了、卡片顺序变了，刷新逻辑不用跟着大改。 */
    for (uint8_t i = 0; i < ui_left_widget_count_get(); i++)
    {
        const grid_widget_t *widget = ui_left_widget_get(i);
        if (widget && widget->widget_id != COMP_EMPTY)
        {
            comp_sync_data(widget->widget_id);
        }
    }

    /* 2. 同步右侧全部自定义卡片配置 */
    /* 右侧 5F 卡片也是同样思路：刷新跟着“配置里有哪些组件”走，而不是跟着“某个页固定有哪些控件”走。 */
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
    /* wall 是一种“边界蓄力反馈”：
     * 用户继续朝边界方向旋转时，不立刻进菜单，而是先显示 1~3 格充能提示。
     * 这样潜水中能显著降低误入菜单的概率。 */

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
static void menu_list_scroll_item_to_view(lv_obj_t *list, lv_obj_t *item, lv_anim_enable_t anim)
{
    lv_coord_t visible_h;
    lv_coord_t item_y;
    lv_coord_t item_h;
    lv_coord_t scroll_y;
    lv_coord_t target_y;
    lv_coord_t margin = MENU_LIST_EDGE_PAD_PX;

    lv_obj_update_layout(list);
    visible_h = lv_obj_get_height(list);
    item_y = lv_obj_get_y(item);
    item_h = lv_obj_get_height(item);
    scroll_y = lv_obj_get_scroll_y(list);
    target_y = scroll_y;
    if (visible_h <= item_h + margin * 2) margin = 0;

    if (item_y - margin < scroll_y) target_y = item_y - margin;
    else if (item_y + item_h + margin > scroll_y + visible_h) target_y = item_y + item_h + margin - visible_h;
    if (target_y < 0) target_y = 0;

    lv_obj_scroll_to_y(list, target_y, anim);
}

static void menu_list_ensure_visible(lv_obj_t *list, uint8_t idx)
{
    lv_obj_t *item;

    if (!list || !lv_obj_is_valid(list)) return;

    item = lv_obj_get_child(list, idx);
    if (!item) return;

    menu_list_scroll_item_to_view(list, item, MENU_LIST_SCROLL_ANIM_ENABLED ? LV_ANIM_ON : LV_ANIM_OFF);
}

void screen_set_info_selection(uint8_t idx)
{
    if (!s_info_list || !lv_obj_is_valid(s_info_list))
    {
        s_info_list = NULL;
        return;
    }
    /* 这里不依赖 LVGL 自带 list 焦点态，而是手工重绘选中样式。
     * 这样可以保持项目自定义的字体、边框和颜色规则完全一致。 */
    uint32_t cnt = lv_obj_get_child_cnt(s_info_list);
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
    menu_list_ensure_visible(s_info_list, idx);
}

uint8_t screen_info_item_count(void)
{
    if (!s_info_list || !lv_obj_is_valid(s_info_list))
    {
        s_info_list = NULL;
        return 0;
    }
    return (uint8_t)lv_obj_get_child_cnt(s_info_list);
}

void screen_set_setup_selection(uint8_t idx)
{
    if (!s_setup_list || !lv_obj_is_valid(s_setup_list))
    {
        s_setup_list = NULL;
        return;
    }
    /* SETUP 比 INFO 多一个 badge 列，所以选中态要同时处理主标题和右侧状态文本。 */
    uint32_t cnt = lv_obj_get_child_cnt(s_setup_list);
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
    menu_list_ensure_visible(s_setup_list, idx);
}

uint8_t screen_setup_item_count(void)
{
    if (!s_setup_list || !lv_obj_is_valid(s_setup_list))
    {
        s_setup_list = NULL;
        return 0;
    }
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
    if (!s_setup_list || !lv_obj_is_valid(s_setup_list))
    {
        s_setup_list = NULL;
        return;
    }
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
    uint16_t right_w_fallback = ui_content_w_get();
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

