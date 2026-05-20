#include "arex_screen.h"
#include "arex_layout_view.h"
#include "arex_widget_update.h"
#include "arex_widget_view.h"
#include "arex_ui_engine.h"
#include "arex_data.h"
#include "arex_ui_state.h"
#include "arex_card_registry.h"
#include "arex_callbacks.h"
#include "arex_modal_view.h"
#include "arex_submenu_view.h"
#include "cards/card_compass.h"
#include "fonts/arex_fonts.h"
#include <stdio.h>
#include <string.h>

/* =========================================================
 * 内部句柄
 * ========================================================= */
static lv_obj_t *s_scr;
static lv_obj_t *s_safe_zone;        /* 安全区容器(绝对坐标原点) */
static lv_obj_t *s_left_anchor;      /* 左侧锚点 (10U 沙盒) */
static lv_obj_t *s_right_cont;       /* clip container */
static lv_obj_t *s_tileview;
static lv_obj_t *s_tile_objs[AREX_CARD_COUNT];

/* 灯光控制状态（LIGHT CONTROL 子菜单全局共享） */
/* 问题4修复：灯光硬件默认开启，UI 初始值必须匹配硬件状态 */
bool g_light_power_state = false;

/* Wall indicators */
static lv_obj_t *s_wall_top;
static lv_obj_t *s_wall_bottom;
static lv_obj_t *s_wall_text_top,    *s_wall_blocks_top;
static lv_obj_t *s_wall_text_bottom, *s_wall_blocks_bottom;

/* Scroll dots */
static lv_obj_t *s_dot_cont;  /* dots 容器（父对象 s_safe_zone，可定位到间隙中间） */
static lv_obj_t *s_scroll_dots[AREX_DASH_CARD_COUNT];

static lv_obj_t *s_brightness_overlay;
static bool s_software_brightness_enabled = true;

/* INFO / SETUP list objects */
static lv_obj_t *s_info_list;
static lv_obj_t *s_setup_list;

/* Inline value edit (MOD PO2 pattern) */
static lv_timer_t *s_edit_flash_timer;
static lv_obj_t   *s_edit_flash_badge;
static lv_obj_t   *s_edit_flash_val_lbl;
static bool        s_edit_flash_on;

/* 排版缓存 (避免每次重算) */
static uint16_t s_cached_right_w = 0;

/* Forward declarations for static functions */
static void wall_create(void);
static void reset_transient_ui_refs(void);
static void edit_flash_stop(void);
static void restore_brightness_overlay_state(void);

/* =========================================================
 * 样式 (静态初始化一次)
 * ========================================================= */
static lv_style_t s_style_screen;
static lv_style_t s_style_panel;
static lv_style_t s_style_anchor_bg;
static lv_style_t s_style_label_huge;
static lv_style_t s_style_label_med;
static lv_style_t s_style_label_small;
static lv_style_t s_style_title_zone;
static lv_style_t s_style_val_zone;
static lv_style_t s_style_menu_item;
static lv_style_t s_style_menu_item_active;
static lv_style_t s_style_sep_line;
static bool       s_styles_inited = false;

static void reset_transient_ui_refs(void)
{
    s_wall_top = NULL;
    s_wall_bottom = NULL;
    s_wall_text_top = NULL;
    s_wall_blocks_top = NULL;
    s_wall_text_bottom = NULL;
    s_wall_blocks_bottom = NULL;
    arex_modal_view_reset();
    arex_submenu_view_reset();
    s_info_list = NULL;
    s_setup_list = NULL;
    s_edit_flash_badge = NULL;
    s_edit_flash_val_lbl = NULL;
    edit_flash_stop();

    g_ui.sub_history_depth = 0;
    g_ui.sub_item_count = 0;
    g_ui.sub_menu_idx = 0;
    g_ui.gas_modal_from_submenu = false;
    g_ui.edit_ctx.active = false;
}

static void restore_brightness_overlay_state(void)
{
    if (s_scr == NULL)
    {
        return;
    }

    /* 亮度遮罩挂在根屏 s_scr 上，布局翻转/重建右侧容器时不会被删除。
     * 这里必须保留对象引用并在重建后立即按当前档位重放一次，
     * 否则 APP 下发布局后会出现旧遮罩残留或前后亮度现象不一致。 */
    arex_apply_software_brightness(g_sys_config.brightness);
}

static void styles_init(void)
{
    if (s_styles_inited) return;
    s_styles_inited = true;

    lv_style_init(&s_style_screen);
    lv_style_set_bg_color(&s_style_screen, AREX_BG);
    lv_style_set_bg_opa(&s_style_screen, LV_OPA_COVER);
    lv_style_set_pad_all(&s_style_screen, 0);
    lv_style_set_border_width(&s_style_screen, 0);

    lv_style_init(&s_style_anchor_bg);
    lv_style_set_bg_color(&s_style_anchor_bg, AREX_BLACK);
    lv_style_set_bg_opa(&s_style_anchor_bg, LV_OPA_COVER);
    lv_style_set_border_color(&s_style_anchor_bg, AREX_DARK);
    lv_style_set_border_width(&s_style_anchor_bg, AREX_DEBUG_BORDERS ? 1 : 0);
    lv_style_set_pad_all(&s_style_anchor_bg, 0);     /* 必须显式清零，否则 LVGL 默认 padding 会偏移所有子组件坐标 */
    lv_style_set_radius(&s_style_anchor_bg, 0);

    lv_style_init(&s_style_panel);
    lv_style_set_bg_color(&s_style_panel, AREX_BLACK);
    lv_style_set_bg_opa(&s_style_panel, LV_OPA_COVER);
    lv_style_set_pad_all(&s_style_panel, 0);
    lv_style_set_border_width(&s_style_panel, 0);

    lv_style_init(&s_style_label_huge);
    lv_style_set_text_color(&s_style_label_huge, AREX_GREEN);
    lv_style_set_text_font(&s_style_label_huge, arex_get_font(AREX_FONT_ID_HUGE));

    lv_style_init(&s_style_label_med);
    lv_style_set_text_color(&s_style_label_med, AREX_GREEN);
    lv_style_set_text_font(&s_style_label_med, arex_get_font(AREX_FONT_ID_MEDIUM));

    lv_style_init(&s_style_label_small);
    lv_style_set_text_color(&s_style_label_small, AREX_LIGHT);
    lv_style_set_text_font(&s_style_label_small, arex_get_font(AREX_FONT_ID_SMALL));

    lv_style_init(&s_style_title_zone);
    lv_style_set_text_color(&s_style_title_zone, AREX_LIGHT);
    lv_style_set_text_font(&s_style_title_zone, arex_get_font(AREX_FONT_ID_SMALL));
    lv_style_set_bg_opa(&s_style_title_zone, LV_OPA_TRANSP);

    lv_style_init(&s_style_val_zone);
    lv_style_set_text_color(&s_style_val_zone, AREX_GREEN);
    lv_style_set_bg_opa(&s_style_val_zone, LV_OPA_TRANSP);

    lv_style_init(&s_style_sep_line);
    lv_style_set_bg_color(&s_style_sep_line, AREX_DARK);
    lv_style_set_bg_opa(&s_style_sep_line, LV_OPA_50);
    lv_style_set_border_width(&s_style_sep_line, 0);

    lv_style_init(&s_style_menu_item);
    lv_style_set_bg_color(&s_style_menu_item, AREX_BLACK);
    lv_style_set_bg_opa(&s_style_menu_item, LV_OPA_COVER);
    lv_style_set_text_color(&s_style_menu_item, AREX_GREEN);
    lv_style_set_border_color(&s_style_menu_item, AREX_DARK);
    lv_style_set_border_width(&s_style_menu_item, AREX_INNER_BORDER_W);
    lv_style_set_pad_all(&s_style_menu_item, 12);

    lv_style_init(&s_style_menu_item_active);
    lv_style_set_bg_color(&s_style_menu_item_active, AREX_GREEN);
    lv_style_set_bg_opa(&s_style_menu_item_active, LV_OPA_COVER);
    lv_style_set_text_color(&s_style_menu_item_active, AREX_BLACK);
    lv_style_set_border_color(&s_style_menu_item_active, AREX_GREEN);
}

/* =========================================================
 * 辅助函数
 * ========================================================= */

/* 清空 ascent/NDL widget 句柄数组（在任何网格渲染之前调用）
 * arex_screen_rebuild_layout() 和 left_anchor_create() 入口各调用一次，
 * 确保数组从干净状态开始，两侧网格渲染函数均以追加模式运行。 */
/* =========================================================
 * 统一重置 UI 渲染状态（防止悬空指针访问）
 * 调用链：arex_screen_rebuild_layout -> clear_widget_arrays
 * ========================================================= */
static void clear_widget_arrays(void)
{
    /* 重置渲染计数器和 SystemData 静态句柄 */
    arex_reset_widget_render_state();
}

/* =========================================================
 * 左侧锚点：绝对坐标重建
 *
 * 核心铁律：所有子组件以 s_left_anchor 左上角为原点 (0,0)
 * 不使用任何 LV_FLEX / LV_GRID，完全通过数学累加 current_y
 *
 * Tech 模式下：
 *   - DEPTH: (0, cur_y), 宽 160px, 高 h_depth*10
 *   - NDL/TTS 双拼行: cur_y += h_depth*10 + gap
 *     NDL: (0, cur_y), 宽 80px, 高 h_ndl*10
 *     TTS: (80, cur_y), 宽 80px, 高 h_ndl*10
 *   - 以此类推...
 *
 * Classic 模式下：使用同样逻辑，宽度扩展为 safe_zone_w
 * ========================================================= */
/* =========================================================
 * 左侧锚点重建 (配置变更时调用)
 *
 * 2x7 绝对网格版本：直接调用网格渲染引擎重建。
 * ========================================================= */
static void left_anchor_rebuild(uint8_t comp_count)
{
    (void)comp_count;
    if (!s_left_anchor || !s_safe_zone) return;

    /* 重建锚点容器尺寸（布局顺序可能变化） */
    uint16_t anchor_w = AREX_LEFT_ANCHOR_W;
    uint16_t anchor_h = g_sys_config.safe_zone_h;
    lv_obj_set_size(s_left_anchor, anchor_w, anchor_h);

    /* 清除所有子对象 */
    lv_obj_clean(s_left_anchor);

    /* 重新调用 2x7 绝对网格渲染引擎 */
    arex_render_left_anchor_grid(s_left_anchor);
}

/* =========================================================
 * 左侧锚点创建 (首次初始化)
 * ========================================================= */
/* =========================================================
 * 左侧锚点创建 (2x7 绝对网格版本)
 *
 * 废弃 current_y 累加排版，改为 2 列(80px) x 7 行(60px) 绝对网格矩阵。
 * 所有组件通过 arex_render_left_anchor_grid() 使用 render_widget_by_id 工厂渲染
 * 并注入 arex_widget_id_t 烙印，供 arex_widget_set_value() 定位更新。
 * SystemData 已作为 g_sys_config.left_widgets[6] 参与网格排版，不再独立渲染。
 * ========================================================= */
static void left_anchor_create(void)
{
    /* 1. 创建锚点容器 */
    s_left_anchor = lv_obj_create(s_safe_zone);
    lv_obj_set_size(s_left_anchor, AREX_LEFT_ANCHOR_W, g_sys_config.safe_zone_h);
    if (g_sys_config.layout_order == AREX_ORDER_NORMAL)
    {
        lv_obj_set_pos(s_left_anchor, 0, 0);
    }
    else
    {
        uint16_t panel_gap = g_sys_config.panel_gap_u * AREX_BASE_U;
        uint16_t right_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W - panel_gap;
        lv_obj_set_pos(s_left_anchor, (lv_coord_t)(right_w + panel_gap), 0);
    }
    lv_obj_add_style(s_left_anchor, &s_style_anchor_bg, 0);
    lv_obj_set_style_pad_all(s_left_anchor, 0, 0);
    lv_obj_set_scrollbar_mode(s_left_anchor, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(s_left_anchor, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_clean(s_left_anchor);

    /* 清空 widget 句柄数组（首次创建时重置） */
    clear_widget_arrays();

    /* 2. 调用 2x7 绝对网格渲染引擎（带 sudu 速率图标） */
    arex_render_left_anchor_grid(s_left_anchor);
}

/* =========================================================
 * 右侧区域: Safe Zone 动态排版
 *
 * Safe Zone 内部布局 (绝对坐标):
 *
 *  [s_left_anchor] | [s_right_cont]
 *  (Tech 模式)
 *
 * Safe Zone 坐标原点 (0,0) 在 s_safe_zone 左上角。
 * 所有子组件的坐标以此为基准计算。
 * ========================================================= */
static void safe_zone_reposition(void)
{
    if (!s_safe_zone) return;

    /* 1. 安全区容器定位 */
    lv_obj_set_size(s_safe_zone, g_sys_config.safe_zone_w, g_sys_config.safe_zone_h);
    lv_obj_align(s_safe_zone, LV_ALIGN_CENTER,
                 g_sys_config.offset_x, g_sys_config.offset_y);

    /* 2. 计算左右分界线 */
    uint16_t panel_gap = g_sys_config.panel_gap_u * AREX_BASE_U;
    uint16_t right_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W - panel_gap;

    /* 3. 定位左侧锚点 */
    if (g_sys_config.layout_order == AREX_ORDER_NORMAL)
    {
        lv_obj_set_pos(s_left_anchor, 0, 0);
        lv_obj_set_pos(s_right_cont, (lv_coord_t)(AREX_LEFT_ANCHOR_W + panel_gap), 0);
    }
    else
    {
        /* 翻转: 右侧容器放左侧，左侧锚点放右侧 */
        lv_obj_set_pos(s_right_cont, 0, 0);
        lv_obj_set_pos(s_left_anchor, (lv_coord_t)right_w + panel_gap, 0);
    }

    /* 固定区在左右互换后仍要保持自己的层级和重绘优先级，防止分隔线被后续容器覆盖。 */
    if (s_left_anchor)
    {
        lv_obj_move_foreground(s_left_anchor);
        lv_obj_invalidate(s_left_anchor);
    }

    /* 4. 设置右侧容器尺寸 */
    lv_obj_set_size(s_right_cont, right_w, g_sys_config.safe_zone_h);

    s_cached_right_w = right_w;

    /* 5. 重新定位 scroll dots（处理所有位置模式） */
    if (s_dot_cont)
    {
        lv_coord_t dot_x, dot_y;
        uint16_t dot_cont_h = arex_visible_dash_count() * 14;

        printf("[SAFE_ZONE] reposition dots: position=%d, visible_dash=%u, dot_cont_h=%u, safe_zone=%ux%u\r\n",
               g_sys_config.dots_position, arex_visible_dash_count(), dot_cont_h,
               g_sys_config.safe_zone_w, g_sys_config.safe_zone_h);

        /* 更新容器大小以匹配实际显示数量 */
        lv_obj_set_size(s_dot_cont, 10, (lv_coord_t)dot_cont_h);

        if (g_sys_config.dots_position == AREX_DOTS_LEFT)
        {
            /* 左侧：间隙中间 */
            lv_coord_t gap_center_x;
            lv_coord_t gap_center_y = (lv_coord_t)(g_sys_config.safe_zone_h / 2);
            if (g_sys_config.layout_order == AREX_ORDER_NORMAL)
            {
                gap_center_x = (lv_coord_t)(AREX_LEFT_ANCHOR_W + panel_gap / 2);
            }
            else
            {
                gap_center_x = (lv_coord_t)(right_w + panel_gap / 2);
            }
            dot_x = gap_center_x - 5;
            dot_y = gap_center_y - (lv_coord_t)(dot_cont_h / 2);
        }
        else if (g_sys_config.dots_position == AREX_DOTS_RIGHT)
        {
            /* 右侧：相对于右侧容器的右边缘 */
            lv_coord_t right_cont_x = (g_sys_config.layout_order == AREX_ORDER_NORMAL)
                                      ? (lv_coord_t)(AREX_LEFT_ANCHOR_W + panel_gap)
                                      : 0;
            dot_x = right_cont_x + (lv_coord_t)right_w - 18;
            dot_y = (lv_coord_t)(g_sys_config.safe_zone_h / 2 - (int)dot_cont_h / 2);
        }
        else if (g_sys_config.dots_position == AREX_DOTS_BOTTOM)
        {
            /* 底部：水平居中 */
            dot_x = (lv_coord_t)(g_sys_config.safe_zone_w / 2 - 5);
            dot_y = (lv_coord_t)(g_sys_config.safe_zone_h - 18);
        }
        else
        {
            /* AREX_DOTS_NONE: 隐藏 dots */
            dot_x = -1000;  /* 移出可见区域 */
            dot_y = -1000;
        }
        lv_obj_set_pos(s_dot_cont, dot_x, dot_y);

        /* 更新每个 dot 的绝对位置（使用绝对定位） */
        for (uint8_t i = 0; i < AREX_DASH_CARD_COUNT; i++)
        {
            if (s_scroll_dots[i])
            {
                lv_obj_set_pos(s_scroll_dots[i], 2, (lv_coord_t)(i * 14));
                /* 超出 visible_dash 的 dot 隐藏 */
                if (i >= arex_visible_dash_count())
                {
                    lv_obj_add_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
                }
                else
                {
                    lv_obj_clear_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
                }
            }
        }

        /* 调用 arex_screen_update_scroll_dots() 根据 UI 状态更新可见性 */
        /* 计算逻辑索引 */
        uint8_t active_idx = 0;
        if (g_ui.dash_card >= CARD_POS_DYNAMIC_FIRST && g_ui.dash_card < arex_setup_display_pos())
        {
            for (uint8_t pos = CARD_POS_DYNAMIC_FIRST; pos < g_ui.dash_card; pos++)
            {
                uint8_t card_id = g_sys_card_order(pos);
                if (card_id != CARD_ID_UNUSED && card_id != CARD_ID_BLANK)
                {
                    active_idx++;
                }
            }
        }
        bool show_dots = (g_ui.state == UI_DASH || g_ui.state == UI_EDIT_GAS);
        arex_screen_update_scroll_dots(active_idx, show_dots);
    }
}

/* =========================================================
 * 右侧区域: clip container + tileview
 * ========================================================= */
static void right_panel_create(void)
{
    /* 计算右侧容器宽度 */
    uint16_t panel_gap = g_sys_config.panel_gap_u * AREX_BASE_U;
    uint16_t right_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W - panel_gap;
    s_cached_right_w = right_w;

    /* 创建右侧容器，根据 layout_order 决定左右位置 */
    s_right_cont = lv_obj_create(s_safe_zone);
    lv_obj_set_size(s_right_cont, right_w, g_sys_config.safe_zone_h);
    if (g_sys_config.layout_order == AREX_ORDER_NORMAL)
    {
        lv_obj_set_pos(s_right_cont, (lv_coord_t)(AREX_LEFT_ANCHOR_W + panel_gap), 0);
    }
    else
    {
        lv_obj_set_pos(s_right_cont, 0, 0);
    }
    lv_obj_set_style_bg_color(s_right_cont, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(s_right_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_right_cont, 0, 0);
    lv_obj_set_style_border_width(s_right_cont, 0, 0);
    lv_obj_clear_flag(s_right_cont, LV_OBJ_FLAG_SCROLLABLE);

    /* Tileview */
    s_tileview = lv_tileview_create(s_right_cont);
    lv_obj_set_size(s_tileview, right_w, g_sys_config.safe_zone_h);
    lv_obj_set_pos(s_tileview, 0, 0);
    lv_obj_set_style_bg_color(s_tileview, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(s_tileview, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_tileview, 0, 0);
    lv_obj_set_scrollbar_mode(s_tileview, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(s_tileview, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建 tiles */
    memset(s_tile_objs, 0, sizeof(s_tile_objs));
    uint8_t count = arex_card_count();
    for (uint8_t i = 0; i < count; i++)
    {
        arex_card_t *card = arex_card_get(i);
        if (!card) continue;

        lv_obj_t *tile = lv_tileview_add_tile(s_tileview, 0, i,
                                              LV_DIR_TOP | LV_DIR_BOTTOM);
        lv_obj_set_style_bg_color(tile, AREX_BLACK, 0);
        lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(tile, 0, 0);
        lv_obj_set_scrollbar_mode(tile, LV_SCROLLBAR_MODE_OFF);
        lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
        s_tile_objs[i] = tile;
        card->tile_obj = tile;

        switch (card->engine_type)
        {
        case CARD_ENGINE_GRID:
        {
            /* 获取当前 tile 对应的 custom_card_slot 索引 */
            uint8_t storage_pos = arex_card_storage_pos(i);
            uint8_t custom_card_idx = (storage_pos < AREX_CARD_COUNT)
                                      ? g_sys_config.custom_card_slot[storage_pos]
                                      : 0xFF;
            if (custom_card_idx < AREX_MAX_CUSTOM_CARDS)
            {
                arex_render_5f_custom_grid(tile, g_left_anchor_obj, custom_card_idx);
            }
            break;
        }
        case CARD_ENGINE_MENU:
        case CARD_ENGINE_CUSTOM:
        default:
            if (card->create_cb) card->create_cb(tile);
            break;
        }
    }

    /* Scroll dots - 父对象为 s_safe_zone，可定位到间隙中间 */
    s_dot_cont = lv_obj_create(s_safe_zone);
    /* 容器高度根据实际显示数量计算 */
    uint16_t dot_cont_h = arex_visible_dash_count() * 14;
    lv_obj_set_size(s_dot_cont, 10, dot_cont_h);
    lv_obj_set_style_bg_opa(s_dot_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_dot_cont, 0, 0);
    lv_obj_set_style_pad_all(s_dot_cont, 0, 0);
    /* 不使用 flex 布局，改用绝对定位，避免隐藏元素占用空间 */
    lv_obj_set_scrollbar_mode(s_dot_cont, LV_SCROLLBAR_MODE_OFF);

    /* Dots 位置跟随配置 - 使用绝对坐标（相对于 s_safe_zone） */
    if (g_sys_config.dots_position == AREX_DOTS_LEFT)
    {
        /* 放在左侧固定区和右侧卡片区的间隙中间 */
        lv_coord_t gap_center_x;
        lv_coord_t gap_center_y = (lv_coord_t)(g_sys_config.safe_zone_h / 2);

        if (g_sys_config.layout_order == AREX_ORDER_NORMAL)
        {
            /* NORMAL 布局：间隙中间 X = AREX_LEFT_ANCHOR_W + panel_gap / 2 */
            gap_center_x = (lv_coord_t)(AREX_LEFT_ANCHOR_W + panel_gap / 2);
        }
        else
        {
            /* FLIPPED 布局：间隙中间 X = right_w + panel_gap / 2 */
            gap_center_x = (lv_coord_t)(right_w + panel_gap / 2);
        }
        lv_obj_set_pos(s_dot_cont, gap_center_x - 5, gap_center_y - dot_cont_h / 2);
    }
    else if (g_sys_config.dots_position == AREX_DOTS_RIGHT)
    {
        /* 右侧：相对于右侧容器的右边缘 */
        lv_coord_t right_cont_x = (g_sys_config.layout_order == AREX_ORDER_NORMAL)
                                  ? (lv_coord_t)(AREX_LEFT_ANCHOR_W + panel_gap)
                                  : 0;
        lv_coord_t dots_x = right_cont_x + right_w - 18;
        lv_coord_t dots_y = (lv_coord_t)(g_sys_config.safe_zone_h / 2 - dot_cont_h / 2);
        lv_obj_set_pos(s_dot_cont, dots_x, dots_y);
    }
    else if (g_sys_config.dots_position == AREX_DOTS_BOTTOM)
    {
        /* 底部：水平居中 */
        lv_coord_t dots_x = (lv_coord_t)(g_sys_config.safe_zone_w / 2 - 5);
        lv_coord_t dots_y = (lv_coord_t)(g_sys_config.safe_zone_h - 18);
        lv_obj_set_pos(s_dot_cont, dots_x, dots_y);
    }
    /* AREX_DOTS_NONE 时不显示 dot_cont */

    /* 使用绝对定位创建 dots，避免隐藏时仍占用空间的问题 */
    for (uint8_t i = 0; i < AREX_DASH_CARD_COUNT; i++)
    {
        s_scroll_dots[i] = lv_obj_create(s_dot_cont);
        lv_obj_set_size(s_scroll_dots[i], 6, 6);
        lv_obj_set_pos(s_scroll_dots[i], 2, (lv_coord_t)(i * 14));  /* 绝对定位，垂直均匀分布 */
        lv_obj_set_style_radius(s_scroll_dots[i], 0, 0);
        lv_obj_set_style_bg_color(s_scroll_dots[i], AREX_DARK, 0);
        lv_obj_set_style_bg_opa(s_scroll_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_scroll_dots[i], 0, 0);
        lv_obj_set_style_pad_all(s_scroll_dots[i], 0, 0);
        lv_obj_set_style_shadow_width(s_scroll_dots[i], 0, 0);
        lv_obj_clear_flag(s_scroll_dots[i], LV_OBJ_FLAG_SCROLLABLE);
        if (g_sys_config.dots_position == AREX_DOTS_NONE)
            lv_obj_add_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
        /* 超出 visible_dash 的 dot 也隐藏 */
        if (i >= arex_visible_dash_count())
            lv_obj_add_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* 只在 DASH/EDIT 状态才显示 dots，INFO/SETUP 菜单不显示 */
    bool show_dots = (g_ui.state == UI_DASH || g_ui.state == UI_EDIT_GAS);
    arex_screen_update_scroll_dots(0, show_dots);
}

/* =========================================================
 * Safe Zone 容器重建 (配置变更后调用)
 * 不重建 cards，只重建布局框架
 * ========================================================= */
void arex_screen_rebuild_layout(void)
{
    printf("[REBUILD_LAYOUT] Enter: visible_dash=%u, dots_pos=%d, layout_order=%d\r\n",
           arex_visible_dash_count(), g_sys_config.dots_position, g_sys_config.layout_order);

    /* 【问题四修复】必须在清空对象前重新启用 invalidation
     * 因为 arex_ui_timer_cb() 中禁用了 invalidation 以优化刷屏性能
     * 任何涉及删除 LVGL 对象的代码都应该假设 invalidation 可能被禁用。 */
    lv_disp_t *disp = lv_disp_get_default();
    if (disp) lv_disp_enable_invalidation(disp, true);

    /* 1. 必须在清空对象前，把指针全部洗白！断绝悬空指针！ */
    clear_widget_arrays();

    /* 2. 清空左侧锚点 */
    if (s_left_anchor)
    {
        lv_obj_clean(s_left_anchor);
    }

    /* 3. 重建左侧锚点排版（2x7 绝对网格版本） */
    if (s_left_anchor)
    {
        left_anchor_rebuild(0);
    }

    /* 4. 重建所有自定义网格卡片 */
    arex_5f_grid_rebuild_all();

    /* 5. 重建 Safe Zone 内部定位（包括 dots 位置和可见性） */
    safe_zone_reposition();

    /* 5.1 重建后立即把当前数据灌入全部 widget 实例，避免多实例场景下部分组件长时间停留在 "--" */
    arex_screen_refresh_all_widgets();

    /* 5.2 APP 下发布局/翻转后，亮度现象必须与默认布局保持一致。 */
    restore_brightness_overlay_state();

    /* 6. 强制把所有常规数据的脏标记置 1
     * 因为新建 Label 里面全是 "--"，必须让定时器在下一帧把真实数据刷进去！ */
    g_sensor_data.dirty_mask |= (DIRTY_DEPTH | DIRTY_BATT | DIRTY_TEMP | DIRTY_POD);
}


/* =========================================================
 * Tileview 重建 (卡片顺序变更时调用)
 * ========================================================= */
void arex_screen_rebuild_tileview(void)
{
    /* 【问题四修复】必须在删除对象前重新启用 invalidation
     * 因为 arex_ui_timer_cb() 中禁用了 invalidation 以优化刷屏性能
     * 任何涉及删除 LVGL 对象的代码都应该假设 invalidation 可能被禁用。 */
    lv_disp_t *disp = lv_disp_get_default();
    if (disp) lv_disp_enable_invalidation(disp, true);

    uint8_t count = arex_card_count();

    /* 【问题二修复】保存当前焦点位置和状态机上下文 */
    uint8_t saved_dash_card = g_ui.dash_card;
    arex_ui_state_t saved_state = g_ui.state;
    uint8_t saved_menu_idx = (saved_state == UI_INFO) ? g_ui.menu_info_idx
                             : (saved_state == UI_SETUP) ? g_ui.menu_setup_idx
                             : 0;

    memset(s_tile_objs, 0, sizeof(s_tile_objs));
    memset(g_card_custom_objs, 0, sizeof(g_card_custom_objs));
    g_card_custom_obj_count = 0;
    for (uint8_t i = 0; i < count; i++)
    {
        arex_card_t *card = arex_card_get_by_id(i);
        if (card) card->tile_obj = NULL;
    }

    if (s_right_cont)
    {
        lv_obj_del(s_right_cont);
        s_right_cont = NULL;
        s_tileview   = NULL;
        reset_transient_ui_refs();
    }

    /* 单独删除 s_dot_cont（父对象 s_safe_zone，不会随 s_right_cont 删除） */
    if (s_dot_cont)
    {
        lv_obj_del(s_dot_cont);
        s_dot_cont = NULL;
        for (uint8_t i = 0; i < AREX_DASH_CARD_COUNT; i++)
            s_scroll_dots[i] = NULL;
    }

    right_panel_create();
    /* right_panel_create() 只重建右侧 tileview / dots
     * tileview 删除后，挂在 s_right_cont 上的 wall / submenu / modal 句柄也已失效
     * 必须同步重建，否则后续状态机路径会把 NULL 传进 lv_obj_add_flag/clear_flag。 */
    wall_create();
    arex_submenu_view_create(s_right_cont,
                             s_cached_right_w > 0 ? s_cached_right_w :
                             (uint16_t)(g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W -
                                        g_sys_config.panel_gap_u * AREX_BASE_U),
                             g_sys_config.safe_zone_h);
    arex_modal_view_create(s_right_cont,
                           s_cached_right_w > 0 ? s_cached_right_w :
                           (uint16_t)(g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W -
                                      g_sys_config.panel_gap_u * AREX_BASE_U),
                           g_sys_config.safe_zone_h);
    restore_brightness_overlay_state();

    /* 【问题二修复】恢复 tile 焦点到保存的位置
     * 注意：g_ui.dash_card 已经在外部保存了，这里使用 saved_dash_card */
    if (s_tileview && saved_dash_card < AREX_CARD_COUNT && s_tile_objs[saved_dash_card])
    {
        lv_obj_set_tile(s_tileview, s_tile_objs[saved_dash_card], LV_ANIM_OFF);
    }

    /* 【问题X修复】恢复 UI 状态机
     * 布局重建后只恢复 tile 位置，但没有恢复状态机
     * 导致 g_ui.state 不同步，滑动行为异常，左侧指示器显示错误 */
    if (AREX_ENABLE_INFO_MENU && saved_dash_card == CARD_POS_INFO)
    {
        g_ui.state = UI_INFO;
        g_ui.menu_info_idx = saved_menu_idx;
    }
    else if (saved_dash_card == arex_setup_display_pos())
    {
        g_ui.state = UI_SETUP;
        g_ui.menu_setup_idx = saved_menu_idx;
    }
    else
    {
        g_ui.state = UI_DASH;
        if (saved_dash_card < CARD_POS_DYNAMIC_FIRST || saved_dash_card >= arex_setup_display_pos())
        {
            g_ui.dash_card = CARD_POS_DYNAMIC_FIRST;
            if (s_tileview && s_tile_objs[CARD_POS_DYNAMIC_FIRST])
            {
                lv_obj_set_tile(s_tileview, s_tile_objs[CARD_POS_DYNAMIC_FIRST], LV_ANIM_OFF);
            }
        }
    }

    {
        /* 计算逻辑索引：从 DYNAMIC_FIRST 到当前卡片之间有多少个有效卡*/
        uint8_t active_idx = 0;
        if (g_ui.dash_card >= CARD_POS_DYNAMIC_FIRST && g_ui.dash_card < arex_setup_display_pos())
        {
            for (uint8_t pos = CARD_POS_DYNAMIC_FIRST; pos < g_ui.dash_card; pos++)
            {
                uint8_t card_id = g_sys_card_order(pos);
                if (card_id != CARD_ID_UNUSED && card_id != CARD_ID_BLANK)
                {
                    active_idx++;
                }
            }
        }
        /* INFO/SETUP 菜单不显示 dots，只更新活跃索引 */
        bool show_dots = (g_ui.state == UI_DASH || g_ui.state == UI_EDIT_GAS);
        arex_screen_update_scroll_dots(active_idx, show_dots);
    }
}

void arex_screen_rebuild_full(void)
{
    /* 完整重建入口
     * 1. card_order/custom_card_slot/custom_cards 变化时必须重建 tileview
     * 2. tileview 重建后，safe zone / left anchor / dots 会随之按当前 g_sys_config 重建
     * 这样可以保证恢复默认布局后，Dive Menu 对应页面结构和运行时配置完全一致。 */
    arex_screen_rebuild_tileview();
    arex_screen_rebuild_layout();
}

/* =========================================================
 * Wall indicator
 * ========================================================= */
static lv_obj_t *make_wall(lv_obj_t *parent, lv_coord_t y)
{
    lv_obj_t *w = lv_obj_create(parent);
    lv_coord_t wall_h = g_sys_config.h_tissues_chart * AREX_BASE_U;  /* 90px = 9U */
    lv_coord_t wall_w = (s_cached_right_w > 0) ? s_cached_right_w
                        : (lv_coord_t)(g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W);
    lv_obj_set_size(w, wall_w, wall_h);
    lv_obj_set_pos(w, 0, y);
    lv_obj_set_style_bg_color(w, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(w, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(w, AREX_DARK, 0);
    lv_obj_set_style_border_width(w, AREX_INNER_BORDER_W, 0);
    lv_obj_set_style_border_side(w, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_all(w, 0, 0);
    lv_obj_add_flag(w, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(w, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *txt = lv_label_create(w);
    lv_obj_set_style_text_color(txt, AREX_GREEN, 0);
    lv_obj_set_style_text_font(txt, arex_get_font(AREX_FONT_ID_TITLE), 0);
    lv_obj_set_width(txt, wall_w);
    lv_obj_set_style_text_align(txt, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(txt, 0, 12);
    lv_label_set_text(txt, "");

    lv_obj_t *blk = lv_label_create(w);
    lv_obj_set_style_text_color(blk, AREX_GREEN, 0);
    /* Wall blocks 必须使用 Courier 内置字体以支持 ■ (U+25A0) 方块字符 */
    lv_obj_set_style_text_font(blk, &lv_font_courier_28, 0);
    lv_obj_set_width(blk, wall_w);
    lv_obj_set_style_text_align(blk, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(blk, 0, 50);
    lv_label_set_text(blk, "");

    return w;
}

static void wall_create(void)
{
    lv_coord_t wall_h = g_sys_config.h_tissues_chart * AREX_BASE_U;  /* 90px = 9U */
    lv_coord_t wall_y_bottom = (g_sys_config.safe_zone_h > wall_h)
                               ? (lv_coord_t)(g_sys_config.safe_zone_h - wall_h)
                               : (lv_coord_t)(g_sys_config.safe_zone_h);

    s_wall_top    = make_wall(s_right_cont, 0);
    s_wall_bottom = make_wall(s_right_cont, wall_y_bottom);
    s_wall_text_top      = lv_obj_get_child(s_wall_top, 0);
    s_wall_blocks_top    = lv_obj_get_child(s_wall_top, 1);
    s_wall_text_bottom   = lv_obj_get_child(s_wall_bottom, 0);
    s_wall_blocks_bottom = lv_obj_get_child(s_wall_bottom, 1);
}

/* =========================================================
 * arex_screen_create - 公开入口
 * ========================================================= */
void arex_screen_create(void)
{
    styles_init();

    s_scr = lv_obj_create(NULL);
    lv_obj_add_style(s_scr, &s_style_screen, 0);

    /* 安全区容器(相对 s_scr 居中定位) */
    s_safe_zone = lv_obj_create(s_scr);
    lv_obj_set_size(s_safe_zone, g_sys_config.safe_zone_w, g_sys_config.safe_zone_h);
    lv_obj_align(s_safe_zone, LV_ALIGN_CENTER,
                 g_sys_config.offset_x, g_sys_config.offset_y);
    lv_obj_set_style_bg_opa(s_safe_zone, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_safe_zone, 0, 0);
    lv_obj_set_style_pad_all(s_safe_zone, 0, 0);
    lv_obj_clear_flag(s_safe_zone, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_border_color(s_safe_zone, AREX_DARK, 0);
    lv_obj_set_style_border_width(s_safe_zone, AREX_DEBUG_BORDERS ? 1 : 0, 0);

    left_anchor_create();
    right_panel_create();
    wall_create();
    arex_submenu_view_create(s_right_cont,
                             s_cached_right_w > 0 ? s_cached_right_w :
                             (uint16_t)(g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W -
                                        g_sys_config.panel_gap_u * AREX_BASE_U),
                             g_sys_config.safe_zone_h);
    arex_modal_view_create(s_right_cont,
                           s_cached_right_w > 0 ? s_cached_right_w :
                           (uint16_t)(g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W -
                                      g_sys_config.panel_gap_u * AREX_BASE_U),
                           g_sys_config.safe_zone_h);
    restore_brightness_overlay_state();

    lv_scr_load(s_scr);
}

/* =========================================================
 * Tileview 导航
 * ========================================================= */
void arex_screen_scroll_to_card(uint8_t tile_pos)
{
    /* 【问题三修复】s_tileview 可能为 NULL（布局重建期间） */
    if (!s_tileview) return;

    if (tile_pos >= arex_card_count())
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

    lv_obj_set_tile(s_tileview, tile, AREX_TILE_ANIM_ENABLED ? LV_ANIM_ON : LV_ANIM_OFF);

    /* 首屏/重建后首次进卡时，当前 tile 可能没有后续脏数据驱动刷新
     * 这里主动补一次当前可见页的布局和重绘，避免必须等用户旋钮交互后才完整显示。 */
    lv_obj_update_layout(tile);
    lv_obj_invalidate(tile);

    if (g_sys_card_order(tile_pos) == CARD_ID_COMPASS)
    {
        card_compass_refresh_heading(true);
    }

    /* SETUP（最后一页）不显示 dots，只有 DASH 动态卡片才更新 */
    if (tile_pos >= CARD_POS_DYNAMIC_FIRST && tile_pos < arex_setup_display_pos())
    {
        /* 计算逻辑索引：从 DYNAMIC_FIRST 到 tile_pos 之间有多少个有效卡片 */
        uint8_t active_idx = 0;
        for (uint8_t pos = CARD_POS_DYNAMIC_FIRST; pos < tile_pos; pos++)
        {
            uint8_t card_id = g_sys_card_order(pos);
            if (card_id != CARD_ID_UNUSED && card_id != CARD_ID_BLANK)
            {
                active_idx++;
            }
        }
        arex_screen_update_scroll_dots(active_idx, true);
    }
    else
    {
        arex_screen_update_scroll_dots(0, false);
    }
}

/* =========================================================
 * 左侧面板刷新 (使用 arex_widget_set_value API)
 *
 * 彻底委托 arex_widget_set_value()，通过 user_data 烙印自动定位
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
 * 左侧面板刷新 - 数据源自动推导引擎
 *
 * 铁律：只读 g_sys_config.left_widgets[] 数组，根据 widget_id 路由到对应的 g_sensor_data 字段
 * 每次修改布局（调整 g_left_widgets[] 顺序/类型）后，此函数自动同步，无需手动维护
 * ========================================================= */
/* =========================================================
 * 全屏组件数据同步接口
 *
 * 同时刷新左侧锚点和右侧 5F 自定义网格的所有组件
 * 内部调用 arex_widget_sync_data() 路由分发器，实现全量数据自动对齐
 * ========================================================= */
void arex_screen_refresh_all_widgets(void)
{
    /* 1. 同步左侧固定区配置 */
    for (uint8_t i = 0; i < g_sys_config.left_widget_count; i++)
    {
        if (g_sys_config.left_widgets[i].widget_id != WIDGET_EMPTY)
        {
            arex_widget_sync_data(g_sys_config.left_widgets[i].widget_id);
        }
    }

    /* 2. 同步右侧全部自定义卡片配置 */
    for (uint8_t card_idx = 0;
            card_idx < g_sys_config.custom_card_count && card_idx < AREX_MAX_CUSTOM_CARDS;
            card_idx++)
    {
        for (uint8_t i = 0; i < g_sys_config.custom_cards[card_idx].widget_count; i++)
        {
            if (g_sys_config.custom_cards[card_idx].widgets[i].widget_id != WIDGET_EMPTY)
            {
                arex_widget_sync_data(g_sys_config.custom_cards[card_idx].widgets[i].widget_id);
            }
        }
    }
}

/* =========================================================
 * 兼容旧接口：仅刷新左侧面板
 * 内部委托 arex_widget_sync_data()，消除冗余 switch-case
 * ========================================================= */
void arex_screen_refresh_left_panel(void)
{
    for (uint8_t i = 0; i < g_sys_config.left_widget_count; i++)
    {
        if (g_sys_config.left_widgets[i].widget_id != WIDGET_EMPTY)
        {
            arex_widget_sync_data(g_sys_config.left_widgets[i].widget_id);
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

void arex_screen_show_wall(wall_side_t side, uint8_t charge, const char *text)
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

void arex_screen_hide_walls(void)
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

void arex_screen_hide_walls_snap(void)
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
 * Scroll dots
 * ========================================================= */
void arex_screen_update_scroll_dots(uint8_t active_idx, bool visible)
{
    bool in_dash_or_edit = (g_ui.state == UI_DASH || g_ui.state == UI_EDIT_GAS);
    bool dots_enabled = (g_sys_config.dots_position != AREX_DOTS_NONE);
    uint8_t visible_dash = arex_visible_dash_count();

    printf("[DOTS] update: active=%u, visible=%d, state=%d, dots_pos=%d, visible_dash=%u, dot_cont_children=%d\r\n",
           active_idx, visible, g_ui.state, g_sys_config.dots_position, visible_dash,
           s_dot_cont ? lv_obj_get_child_cnt(s_dot_cont) : -1);

    for (uint8_t i = 0; i < AREX_DASH_CARD_COUNT; i++)
    {
        if (!s_scroll_dots[i])
        {
            if (i < visible_dash)
            {
                printf("[DOTS] WARN: s_scroll_dots[%u] is NULL but visible_dash=%u!\r\n", i, visible_dash);
            }
            continue;
        }
        bool show = visible && in_dash_or_edit && dots_enabled && (i < visible_dash);
        if (!show)
        {
            lv_obj_add_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
        if (i == active_idx)
        {
            lv_obj_set_style_bg_color(s_scroll_dots[i], AREX_GREEN, 0);
            lv_obj_set_style_shadow_width(s_scroll_dots[i], 8, 0);
            lv_obj_set_style_shadow_color(s_scroll_dots[i], AREX_GREEN, 0);
            lv_obj_set_style_shadow_opa(s_scroll_dots[i], 255, 0);
        }
        else
        {
            lv_obj_set_style_bg_color(s_scroll_dots[i], AREX_DARK, 0);
            lv_obj_set_style_shadow_width(s_scroll_dots[i], 0, 0);
            lv_obj_set_style_shadow_opa(s_scroll_dots[i], 0, 0);
        }
    }
}

/* =========================================================
 * Info / Setup list
 * ========================================================= */
void arex_screen_set_info_selection(uint8_t idx)
{
    if (!s_info_list) return;
    uint32_t cnt = lv_obj_get_child_cnt(s_info_list);
    for (uint32_t i = 0; i < cnt; i++)
    {
        lv_obj_t *item = lv_obj_get_child(s_info_list, i);
        lv_obj_t *lbl  = lv_obj_get_child(item, 0);
        if (i == idx)
        {
            lv_obj_set_style_bg_color(item, AREX_GREEN, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, AREX_BLACK, 0);
        }
        else
        {
            lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
        }
    }
}

uint8_t arex_screen_info_item_count(void)
{
    if (!s_info_list) return 0;
    return (uint8_t)lv_obj_get_child_cnt(s_info_list);
}

void arex_screen_set_setup_selection(uint8_t idx)
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
            lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(item, AREX_GREEN, 0);
            lv_obj_set_style_border_width(item, AREX_INNER_BORDER_W + 2, 0);
            if (lbl)
            {
                lv_obj_set_style_text_color(lbl, AREX_LIGHT, 0);
                lv_obj_set_style_text_font(lbl, arex_get_font(AREX_FONT_ID_MEDIUM), 0);
            }
            if (badge)
            {
                lv_obj_set_style_text_color(badge, AREX_LIGHT, 0);
                lv_obj_set_style_text_font(badge, arex_get_font(AREX_FONT_ID_TITLE), 0);
            }
        }
        else
        {
            lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(item, AREX_DARK, 0);
            lv_obj_set_style_border_width(item, AREX_INNER_BORDER_W, 0);
            if (lbl)
            {
                lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
                lv_obj_set_style_text_font(lbl, arex_get_font(AREX_FONT_ID_TITLE), 0);
            }
            if (badge)
            {
                lv_obj_set_style_text_color(badge, AREX_LIGHT, 0);
                lv_obj_set_style_text_font(badge, arex_get_font(AREX_FONT_ID_SMALL), 0);
            }
        }
    }
}

uint8_t arex_screen_setup_item_count(void)
{
    if (!s_setup_list) return 0;
    return (uint8_t)lv_obj_get_child_cnt(s_setup_list);
}

void arex_screen_register_info_list(lv_obj_t *list)
{
    s_info_list  = list;
}
void arex_screen_register_setup_list(lv_obj_t *list)
{
    s_setup_list = list;
}

/* =========================================================
 * Setup badge update
 * ========================================================= */
void arex_screen_update_setup_badge(uint8_t item_idx, const char *value)
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
void arex_screen_refresh_compass_target(void)
{
    arex_card_t *c = arex_card_get_by_id(CARD_ID_COMPASS);
    if (c && c->update_cb) c->update_cb();
}

void arex_screen_refresh_gas_menu(void)
{
    arex_card_t *c = arex_card_get_by_id(CARD_ID_GAS);
    if (c && c->update_cb) c->update_cb();
}

void arex_screen_refresh_setup_menu(void)
{
    arex_card_t *c = arex_card_get_by_id(CARD_ID_SETUP);
    if (c && c->update_cb) c->update_cb();
}

static void edit_flash_timer_cb(lv_timer_t *t)
{
    (void)t;
    s_edit_flash_on = !s_edit_flash_on;
    if (s_edit_flash_val_lbl)
    {
        /* 文字颜色在绿/暗绿之间闪烁，无背景色切换 */
        lv_color_t fg = s_edit_flash_on ? AREX_GREEN : AREX_DARK;
        lv_obj_set_style_text_color(s_edit_flash_val_lbl, fg, 0);
    }
}

static void edit_flash_stop(void)
{
    if (s_edit_flash_timer)
    {
        lv_timer_del(s_edit_flash_timer);
        s_edit_flash_timer = NULL;
    }
    s_edit_flash_badge   = NULL;
    s_edit_flash_val_lbl = NULL;
}

static void edit_flash_start(void)
{
    if (s_edit_flash_timer)
    {
        lv_timer_del(s_edit_flash_timer);
        s_edit_flash_timer = NULL;
    }
    s_edit_flash_on = true;
    s_edit_flash_timer = lv_timer_create(edit_flash_timer_cb, 350, NULL);
}

static void edit_value_cleanup(lv_obj_t *item);

static void format_edit_value_text(char *buf, size_t buf_size, float value, uint8_t decimals)
{
    if (decimals == 0)
    {
        snprintf(buf, buf_size, "%.0f ^v", (double)value);
    }
    else
    {
        snprintf(buf, buf_size, "%.1f ^v", (double)value);
    }
}

static void format_edit_committed_text(char *buf,
                                       size_t buf_size,
                                       arex_submenu_setting_kind_t kind,
                                       float value)
{
    switch (kind)
    {
    case AREX_SUBMENU_SETTING_MOD_PPO2:
        snprintf(buf, buf_size, "MOD PO2: %.1f", (double)value);
        break;
    case AREX_SUBMENU_SETTING_DEPTH_ALARM:
        snprintf(buf, buf_size, "DEPTH ALARM: %.0fm", (double)value);
        break;
    case AREX_SUBMENU_SETTING_TIME_ALARM:
        snprintf(buf, buf_size, "TIME ALARM: %.0fmin", (double)value);
        break;
    default:
        snprintf(buf, buf_size, "%.1f", (double)value);
        break;
    }
}

static void dispatch_edit_setting_callback(arex_submenu_setting_kind_t kind, float value)
{
    switch (kind)
    {
    case AREX_SUBMENU_SETTING_MOD_PPO2:
        arex_ui_on_mod_ppo2_set(value);
        break;
    case AREX_SUBMENU_SETTING_DEPTH_ALARM:
        arex_ui_on_depth_alarm_set((uint16_t)(value + 0.5f));
        break;
    case AREX_SUBMENU_SETTING_TIME_ALARM:
        arex_ui_on_time_alarm_set((uint16_t)(value + 0.5f));
        break;
    default:
        break;
    }
}

void arex_screen_refresh_edit_value(void)
{
    if (!g_ui.edit_ctx.active || !s_edit_flash_val_lbl || !arex_submenu_view_get_list()) return;
    static float last_drawn = -9999.f;
    float cur = g_ui.edit_ctx.value;
    if (cur == last_drawn) return;   /* dirty check：值未变则跳过，不触发重绘 */
    last_drawn = cur;
    char buf[16];
    format_edit_value_text(buf, sizeof(buf), cur, g_ui.edit_ctx.decimals);
    lv_label_set_text(s_edit_flash_val_lbl, buf);
}

void arex_screen_begin_edit_value(uint8_t item_idx, const arex_submenu_edit_spec_t *spec)
{
    lv_obj_t *submenu_list = arex_submenu_view_get_list();
    if (!submenu_list || !spec) return;

    g_ui.edit_ctx.value      = spec->value;
    g_ui.edit_ctx.original   = spec->value;
    g_ui.edit_ctx.min        = spec->min;
    g_ui.edit_ctx.max        = spec->max;
    g_ui.edit_ctx.step       = spec->step;
    g_ui.edit_ctx.setting_kind = spec->kind;
    g_ui.edit_ctx.setting_arg = spec->arg;
    g_ui.edit_ctx.decimals   = spec->decimals;
    g_ui.edit_ctx.item_index = item_idx;
    g_ui.edit_ctx.active     = true;
    lv_snprintf(g_ui.edit_ctx.label, sizeof(g_ui.edit_ctx.label), "%s", spec->label);
    g_ui.state = UI_EDIT_VALUE;

    lv_obj_t *item = lv_obj_get_child(submenu_list, item_idx);
    if (!item)
    {
        g_ui.edit_ctx.active = false;
        g_ui.state = UI_SUB_MENU;
        return;
    }

    /* 从选中态切换到编辑态：绿底→黑底绿框，title 文字恢复绿色 */
    lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(item, AREX_GREEN, 0);
    lv_obj_set_style_border_width(item, 2, 0);

    /* 复用 child 0 作为左侧标签，恢复绿色文字 */
    lv_obj_t *prefix_lbl = lv_obj_get_child(item, 0);
    if (prefix_lbl)
    {
        lv_label_set_text(prefix_lbl, g_ui.edit_ctx.label);
        lv_obj_set_style_text_color(prefix_lbl, AREX_GREEN, 0);
        lv_obj_set_size(prefix_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(prefix_lbl, LV_ALIGN_LEFT_MID, 12, 0);
    }

    /* child 1 badge label（CONSERVATISM 等无 badge 时为 NULL），恢复颜色 */
    lv_obj_t *old_badge = lv_obj_get_child(item, 1);
    if (old_badge) lv_obj_set_style_text_color(old_badge, AREX_GREEN, 0);

    /* 创建右侧数值 + 箭头 label，透明背景，靠右居中 */
    lv_obj_t *val_lbl = lv_label_create(item);
    lv_obj_set_style_text_color(val_lbl, AREX_GREEN, 0);
    lv_obj_set_style_text_font(val_lbl, arex_get_font(AREX_FONT_ID_TITLE), 0);
    lv_obj_set_style_bg_opa(val_lbl, LV_OPA_TRANSP, 0);
    lv_obj_set_size(val_lbl, 120, LV_SIZE_CONTENT);
    lv_obj_align(val_lbl, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_set_style_text_align(val_lbl, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(val_lbl, LV_LABEL_LONG_DOT);
    char buf[16];
    format_edit_value_text(buf, sizeof(buf), spec->value, spec->decimals);
    lv_label_set_text(val_lbl, buf);

    s_edit_flash_badge    = val_lbl;   /* 复用指针用于闪烁 */
    s_edit_flash_val_lbl  = val_lbl;

    edit_flash_start();
}

static void edit_value_cleanup(lv_obj_t *item)
{
    if (!item) return;
    edit_flash_stop();
    lv_obj_set_style_border_color(item, AREX_DARK, 0);
    uint32_t cnt = lv_obj_get_child_cnt(item);
    if (cnt > 2) lv_obj_del(lv_obj_get_child(item, 2));
    cnt = lv_obj_get_child_cnt(item);
    if (cnt > 1) lv_obj_del(lv_obj_get_child(item, 1));
    lv_obj_set_layout(item, 0);
    lv_obj_update_layout(item);
}

void arex_screen_commit_edit_value(void)
{
    lv_obj_t *submenu_list = arex_submenu_view_get_list();
    if (!submenu_list)
    {
        edit_flash_stop();
        g_ui.edit_ctx.active = false;
        return;
    }
    lv_obj_t *item = lv_obj_get_child(submenu_list, g_ui.edit_ctx.item_index);
    if (!item)
    {
        edit_flash_stop();
        g_ui.edit_ctx.active = false;
        return;
    }
    edit_value_cleanup(item);
    lv_obj_t *lbl = lv_obj_get_child(item, 0);
    if (lbl)
    {
        char buf[32];
        format_edit_committed_text(buf,
                                   sizeof(buf),
                                   g_ui.edit_ctx.setting_kind,
                                   g_ui.edit_ctx.value);
        lv_label_set_text(lbl, buf);
        lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
    }
    arex_submenu_apply_edit_value(g_ui.edit_ctx.setting_kind,
                                  g_ui.edit_ctx.setting_arg,
                                  g_ui.edit_ctx.value);
    dispatch_edit_setting_callback(g_ui.edit_ctx.setting_kind, g_ui.edit_ctx.value);
    g_ui.edit_ctx.active = false;
    arex_screen_set_submenu_selection(g_ui.sub_menu_idx);
}

void arex_screen_cancel_edit_value(void)
{
    lv_obj_t *submenu_list = arex_submenu_view_get_list();
    if (!submenu_list)
    {
        edit_flash_stop();
        return;
    }
    lv_obj_t *item = lv_obj_get_child(submenu_list, g_ui.edit_ctx.item_index);
    if (!item)
    {
        edit_flash_stop();
        return;
    }
    edit_value_cleanup(item);
    lv_obj_t *lbl = lv_obj_get_child(item, 0);
    if (lbl)
    {
        char buf[32];
        format_edit_committed_text(buf,
                                   sizeof(buf),
                                   g_ui.edit_ctx.setting_kind,
                                   g_ui.edit_ctx.original);
        lv_label_set_text(lbl, buf);
        lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
    }
    arex_screen_set_submenu_selection(g_ui.sub_menu_idx);
}

/* =========================================================
 * Card title helper
 * ========================================================= */
lv_obj_t *arex_screen_make_card_title(lv_obj_t *parent, const char *text)
{
    uint16_t right_w_fallback = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                                - g_sys_config.panel_gap_u * AREX_BASE_U;
    uint16_t right_w = (s_cached_right_w > 0) ? s_cached_right_w : right_w_fallback;

    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_style_text_color(lbl, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(lbl, arex_get_font(AREX_FONT_ID_TITLE), 0);
    lv_obj_set_pos(lbl, 16, 8);
    lv_obj_set_size(lbl, right_w - 32, 40);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(lbl, text);

    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_set_size(line, right_w - 32, 2);
    lv_obj_set_pos(line, 16, 48);
    lv_obj_set_style_bg_color(line, AREX_DARK, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    lv_obj_set_style_radius(line, 0, 0);

    return lbl;
}

/* =========================================================
 * Software brightness overlay
 * ========================================================= */
void arex_apply_software_brightness(uint8_t level)
{
    /* 当前正式策略：面板固定在安全硬件亮度，UI 侧只做温和遮罩。
     * 低档首先保证可读，避免再次出现 “LOW 基本看不见” 的问题。 */
    static const lv_opa_t brightness_opa[6] = {120, 150, 185, 215, 238, 255};
    lv_opa_t opa = brightness_opa[(level < 6) ? level : 0];
    lv_opa_t overlay_opa = (lv_opa_t)(255 - opa);

    if (s_scr == NULL)
    {
        return;
    }

    if (s_brightness_overlay == NULL)
    {
        s_brightness_overlay = lv_obj_create(s_scr);
        lv_obj_remove_style_all(s_brightness_overlay);
        lv_obj_set_size(s_brightness_overlay, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(s_brightness_overlay, lv_color_black(), 0);
        lv_obj_set_style_border_width(s_brightness_overlay, 0, 0);
        lv_obj_clear_flag(s_brightness_overlay, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(s_brightness_overlay, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_clear_flag(s_brightness_overlay, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_move_foreground(s_brightness_overlay);
    }

    if (!s_software_brightness_enabled || overlay_opa == 0)
    {
        lv_obj_add_flag(s_brightness_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_set_style_bg_opa(s_brightness_overlay, overlay_opa, 0);
        lv_obj_clear_flag(s_brightness_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_brightness_overlay);
    }

    printf("[BRIGHTNESS] Level: %d (OPA: %d overlay=%d)\n", level, opa, overlay_opa);
}

void arex_set_software_brightness_enabled(bool enabled)
{
    s_software_brightness_enabled = enabled;

    if (s_brightness_overlay == NULL)
    {
        return;
    }

    if (enabled)
    {
        lv_obj_clear_flag(s_brightness_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(s_brightness_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

lv_obj_t *arex_get_safe_zone(void)
{
    return s_safe_zone;
}
