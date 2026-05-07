#include "arex_screen.h"
#include "arex_ui_engine.h"
#include "arex_data.h"
#include "arex_ui_state.h"
#include "arex_card_registry.h"
#include "fonts/arex_fonts.h"

#include <stdio.h>
#include <string.h>

/* =========================================================
 * 内部句柄
 * ========================================================= */
static lv_obj_t *s_scr;
static lv_obj_t *s_safe_zone;        /* 安全区容器 (绝对坐标原点) */
static lv_obj_t *s_left_anchor;      /* 左侧锚点 (10U 沙盒) */
static lv_obj_t *s_right_cont;       /* clip container */
static lv_obj_t *s_tileview;
static lv_obj_t *s_tile_objs[AREX_CARD_COUNT];

/* 灯光控制状态（供 LIGHT CONTROL 子菜单全局共享） */
bool g_light_power_state = false;
static lv_obj_t *s_light_status_lbl = NULL;  /* LIGHT ON/OFF 状态标签 */

/* Wall indicators */
static lv_obj_t *s_wall_top;
static lv_obj_t *s_wall_bottom;
static lv_obj_t *s_wall_text_top,    *s_wall_blocks_top;
static lv_obj_t *s_wall_text_bottom, *s_wall_blocks_bottom;

/* Scroll dots */
static lv_obj_t *s_dot_cont;  /* dots 容器（父对象为 s_safe_zone，可定位到间隙中间） */
static lv_obj_t *s_scroll_dots[AREX_DASH_CARD_COUNT];

/* Modal overlay */
static lv_obj_t *s_modal;
static lv_obj_t *s_modal_box;

/* Sub-menu layer */
static lv_obj_t *s_submenu_layer;
static lv_obj_t *s_submenu_title;
static lv_obj_t *s_submenu_list;

/* INFO / SETUP list objects */
static lv_obj_t *s_info_list;
static lv_obj_t *s_setup_list;

/* 排版缓存 (避免每次重算) */
static uint16_t s_cached_right_w = 0;

/* Forward declarations for static functions */
static void wall_create(void);
static void modal_create(void);
static void submenu_layer_create(void);

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
 * 在 arex_screen_rebuild_layout() 和 left_anchor_create() 入口各调用一次，
 * 确保数组从干净状态开始，两侧网格渲染函数均以追加模式运行。 */
/* =========================================================
 * 统一重置 UI 渲染状态（防止悬空指针访问）
 * 调用链：arex_screen_rebuild_layout → clear_widget_arrays
 * ========================================================= */
static void clear_widget_arrays(void)
{
    /* 重置速率图标阵列 */
    memset(s_img_ascent_rate, 0, sizeof(s_img_ascent_rate));
    s_ascent_icon_count = 0;

    /* 重置 NDL 状态机 */
    memset(s_ndl_handles, 0, sizeof(s_ndl_handles));
    s_ndl_handle_count = 0;

    /* 重置渲染计数器和 SystemData 静态句柄 */
    arex_reset_widget_render_state();
}

/* =========================================================
 * 左侧锚点：绝对坐标重建
 *
 * 核心铁律：所有子组件以 s_left_anchor 左上角为原点 (0,0)，
 * 不使用任何 LV_FLEX / LV_GRID，完全通过数学累加 current_y。
 *
 * Tech 模式下：
 *   - DEPTH: (0, cur_y), 宽=160px, 高=h_depth*10
 *   - NDL/TTS 双拼行: cur_y += h_depth*10 + gap
 *     NDL: (0, cur_y), 宽=80px, 高=h_ndl*10
 *     TTS: (80, cur_y), 宽=80px, 高=h_ndl*10
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
 * 废弃 current_y 累加排版，改为 2列(80px) x 7行(60px) 绝对网格矩阵。
 * 所有组件通过 arex_render_left_anchor_grid() 使用 render_widget_by_id 工厂渲染，
 * 并注入 arex_widget_id_t 烙印供 arex_widget_set_value() 定位更新。
 * SystemData 已作为 g_sys_config.left_widgets[6] 参与网格排版，不再独立渲染。
 * ========================================================= */
static void left_anchor_create(void)
{
    /* 1. 创建锚点容器 */
    s_left_anchor = lv_obj_create(s_safe_zone);
    lv_obj_set_size(s_left_anchor, AREX_LEFT_ANCHOR_W, g_sys_config.safe_zone_h);
    if (g_sys_config.layout_order == AREX_ORDER_NORMAL) {
        lv_obj_set_pos(s_left_anchor, 0, 0);
    } else {
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
 * Safe Zone 坐标原点为 (0,0) 在 s_safe_zone 左上角。
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
    if (g_sys_config.layout_order == AREX_ORDER_NORMAL) {
        lv_obj_set_pos(s_left_anchor, 0, 0);
        lv_obj_set_pos(s_right_cont, (lv_coord_t)(AREX_LEFT_ANCHOR_W + panel_gap), 0);
    } else {
        /* 翻转: 右侧容器放左边, 左侧锚点放右边 */
        lv_obj_set_pos(s_right_cont, 0, 0);
        lv_obj_set_pos(s_left_anchor, (lv_coord_t)right_w + panel_gap, 0);
    }

    /* 4. 设置右侧容器尺寸 */
    lv_obj_set_size(s_right_cont, right_w, g_sys_config.safe_zone_h);

    s_cached_right_w = right_w;

    /* 5. 重新定位 scroll dots（如果存在且位置模式为 LEFT） */
    if (s_dot_cont && g_sys_config.dots_position == AREX_DOTS_LEFT) {
        lv_coord_t gap_center_x;
        uint16_t dot_cont_h = arex_visible_dash_count() * 14;
        lv_coord_t gap_center_y = (lv_coord_t)(g_sys_config.safe_zone_h / 2);

        if (g_sys_config.layout_order == AREX_ORDER_NORMAL) {
            /* NORMAL 布局：间隙中间 X = AREX_LEFT_ANCHOR_W + panel_gap / 2 */
            gap_center_x = (lv_coord_t)(AREX_LEFT_ANCHOR_W + panel_gap / 2);
        } else {
            /* FLIPPED 布局：间隙中间 X = right_w + panel_gap / 2 */
            gap_center_x = (lv_coord_t)(right_w + panel_gap / 2);
        }
        lv_obj_set_pos(s_dot_cont, gap_center_x - 5, gap_center_y - dot_cont_h / 2);
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

    /* 创建右侧容器 — 根据 layout_order 决定左右位置 */
    s_right_cont = lv_obj_create(s_safe_zone);
    lv_obj_set_size(s_right_cont, right_w, g_sys_config.safe_zone_h);
    if (g_sys_config.layout_order == AREX_ORDER_NORMAL) {
        lv_obj_set_pos(s_right_cont, (lv_coord_t)(AREX_LEFT_ANCHOR_W + panel_gap), 0);
    } else {
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
    for (uint8_t i = 0; i < count; i++) {
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

        switch (card->engine_type) {
            case CARD_ENGINE_GRID: {
                /* 获取此 tile 对应的 custom_card_slot 索引 */
                uint8_t storage_pos = arex_card_storage_pos(i);
                uint8_t custom_card_idx = (storage_pos < AREX_CARD_COUNT)
                    ? g_sys_config.custom_card_slot[storage_pos]
                    : 0xFF;
                if (custom_card_idx < AREX_MAX_CUSTOM_CARDS) {
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
    uint16_t dot_cont_h = arex_visible_dash_count() * 14;
    lv_obj_set_size(s_dot_cont, 10, dot_cont_h);
    lv_obj_set_style_bg_opa(s_dot_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_dot_cont, 0, 0);
    lv_obj_set_style_pad_all(s_dot_cont, 0, 0);
    lv_obj_set_flex_flow(s_dot_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_dot_cont, LV_FLEX_ALIGN_SPACE_EVENLY,
                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(s_dot_cont, LV_SCROLLBAR_MODE_OFF);

    /* Dots 位置跟随配置 - 使用绝对坐标（相对于 s_safe_zone） */
    if (g_sys_config.dots_position == AREX_DOTS_LEFT) {
        /* 放在左侧固定区和右侧卡片区的间隙中间 */
        lv_coord_t gap_center_x;
        lv_coord_t gap_center_y = (lv_coord_t)(g_sys_config.safe_zone_h / 2);

        if (g_sys_config.layout_order == AREX_ORDER_NORMAL) {
            /* NORMAL 布局：间隙中间 X = AREX_LEFT_ANCHOR_W + panel_gap / 2 */
            gap_center_x = (lv_coord_t)(AREX_LEFT_ANCHOR_W + panel_gap / 2);
        } else {
            /* FLIPPED 布局：间隙中间 X = right_w + panel_gap / 2 */
            gap_center_x = (lv_coord_t)(right_w + panel_gap / 2);
        }
        lv_obj_set_pos(s_dot_cont, gap_center_x - 5, gap_center_y - dot_cont_h / 2);
    } else if (g_sys_config.dots_position == AREX_DOTS_RIGHT) {
        /* 右侧：相对于右侧容器的右边缘 */
        lv_coord_t right_cont_x = (g_sys_config.layout_order == AREX_ORDER_NORMAL)
            ? (lv_coord_t)(AREX_LEFT_ANCHOR_W + panel_gap)
            : 0;
        lv_coord_t dots_x = right_cont_x + right_w - 18;
        lv_coord_t dots_y = (lv_coord_t)(g_sys_config.safe_zone_h / 2 - dot_cont_h / 2);
        lv_obj_set_pos(s_dot_cont, dots_x, dots_y);
    } else if (g_sys_config.dots_position == AREX_DOTS_BOTTOM) {
        /* 底部：水平居中 */
        lv_coord_t dots_x = (lv_coord_t)(g_sys_config.safe_zone_w / 2 - 5);
        lv_coord_t dots_y = (lv_coord_t)(g_sys_config.safe_zone_h - 18);
        lv_obj_set_pos(s_dot_cont, dots_x, dots_y);
    }
    /* AREX_DOTS_NONE 时不显示 dot_cont */

    for (uint8_t i = 0; i < AREX_DASH_CARD_COUNT; i++) {
        s_scroll_dots[i] = lv_obj_create(s_dot_cont);
        lv_obj_set_size(s_scroll_dots[i], 6, 6);
        lv_obj_set_style_radius(s_scroll_dots[i], 0, 0);
        lv_obj_set_style_bg_color(s_scroll_dots[i], AREX_DARK, 0);
        lv_obj_set_style_bg_opa(s_scroll_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_scroll_dots[i], 0, 0);
        lv_obj_set_style_pad_all(s_scroll_dots[i], 0, 0);
        lv_obj_set_style_shadow_width(s_scroll_dots[i], 0, 0);
        lv_obj_clear_flag(s_scroll_dots[i], LV_OBJ_FLAG_SCROLLABLE);
        if (g_sys_config.dots_position == AREX_DOTS_NONE)
            lv_obj_add_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
    }

    arex_screen_update_scroll_dots(0, true);
}

/* =========================================================
 * Safe Zone 容器重建 (配置变更后调用)
 * 不重建 cards，只重建布局框架
 * ========================================================= */
void arex_screen_rebuild_layout(void)
{
    /* 【问题四修复】必须在清空对象前重新启用 invalidation
     * 因为 arex_ui_timer_cb() 中禁用了 invalidation 以优化刷屏性能，
     * 任何涉及删除 LVGL 对象的代码都应该假设 invalidation 可能被禁用。 */
    lv_disp_t *disp = lv_disp_get_default();
    if (disp) lv_disp_enable_invalidation(disp, true);

    /* 1. 必须在清空对象前，把指针全部洗白！断绝悬空指针！ */
    clear_widget_arrays();

    /* 2. 清空左侧锚点（拆房子） */
    if (s_left_anchor) {
        lv_obj_clean(s_left_anchor);
    }

    /* 3. 重建左侧锚点排版（2x7 绝对网格版本，建房子） */
    if (s_left_anchor) {
        left_anchor_rebuild(0);
    }

    /* 4. 重建所有自定义网格卡片 */
    arex_5f_grid_rebuild_all();

    /* 5. 重建 Safe Zone 内部定位 */
    safe_zone_reposition();

    /* 6. 重建滚动指示器 */
    for (uint8_t i = 0; i < AREX_DASH_CARD_COUNT; i++) {
        if (s_scroll_dots[i]) {
            if (g_sys_config.dots_position == AREX_DOTS_NONE)
                lv_obj_add_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_clear_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* 7. 强制把所有常规数据的脏标记置 1！
     * 因为新建的 Label 里面全是 "--"，必须让定时器在下一帧把真实数据刷进去！ */
    g_sensor_data.dirty_mask |= (DIRTY_DEPTH | DIRTY_BATT | DIRTY_TEMP | DIRTY_POD);
}


/* =========================================================
 * Tileview 重建 (卡片顺序变更时调用)
 * ========================================================= */
void arex_screen_rebuild_tileview(void)
{
    /* 【问题四修复】必须在删除对象前重新启用 invalidation
     * 因为 arex_ui_timer_cb() 中禁用了 invalidation 以优化刷屏性能，
     * 任何涉及删除 LVGL 对象的代码都应该假设 invalidation 可能被禁用。 */
    lv_disp_t *disp = lv_disp_get_default();
    if (disp) lv_disp_enable_invalidation(disp, true);

    uint8_t count = arex_card_count();

    /* 【问题二修复】保存当前焦点位置（使用 g_ui.dash_card，它已经保存了当前卡片位置） */
    uint8_t saved_dash_card = g_ui.dash_card;

    memset(s_tile_objs, 0, sizeof(s_tile_objs));
    memset(g_card_custom_objs, 0, sizeof(g_card_custom_objs));
    g_card_custom_obj_count = 0;
    for (uint8_t i = 0; i < count; i++) {
        arex_card_t *card = arex_card_get_by_id(i);
        if (card) card->tile_obj = NULL;
    }

    if (s_right_cont) {
        lv_obj_del(s_right_cont);
        s_right_cont = NULL;
        s_tileview   = NULL;
        s_wall_top = NULL;
        s_wall_bottom = NULL;
        s_wall_text_top = NULL;
        s_wall_blocks_top = NULL;
        s_wall_text_bottom = NULL;
        s_wall_blocks_bottom = NULL;
        s_modal = NULL;
        s_modal_box = NULL;
        s_submenu_layer = NULL;
        s_submenu_title = NULL;
        s_submenu_list = NULL;
    }

    /* 单独删除 s_dot_cont（父对象为 s_safe_zone，不会随 s_right_cont 删除） */
    if (s_dot_cont) {
        lv_obj_del(s_dot_cont);
        s_dot_cont = NULL;
        for (uint8_t i = 0; i < AREX_DASH_CARD_COUNT; i++)
            s_scroll_dots[i] = NULL;
    }

    right_panel_create();
    /* 【问题一修复】不再重复调用 wall_create/submenu_layer_create/modal_create
     * 因为它们已经在 right_panel_create() 内部被正确调用了 */

    /* 【问题二修复】恢复 tile 焦点到保存的位置
     * 注意：g_ui.dash_card 已经在外部保存了，这里使用 saved_dash_card */
    if (s_tileview && saved_dash_card < AREX_CARD_COUNT && s_tile_objs[saved_dash_card]) {
        lv_obj_set_tile(s_tileview, s_tile_objs[saved_dash_card], LV_ANIM_OFF);
    }

    {
        /* 计算逻辑索引：从 DYNAMIC_FIRST 到当前卡片之间有多少个有效卡片 */
        uint8_t active_idx = 0;
        if (g_ui.dash_card >= CARD_POS_DYNAMIC_FIRST && g_ui.dash_card < arex_setup_display_pos()) 
        {
            for (uint8_t pos = CARD_POS_DYNAMIC_FIRST; pos < g_ui.dash_card; pos++)
            {
                if (g_sys_card_order(pos) != CARD_ID_UNUSED)
                {
                    active_idx++;
                }
            }
        }
        arex_screen_update_scroll_dots(active_idx, true);
    }
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
 * Modal overlay
 * ========================================================= */
static void modal_create(void)
{
    s_modal = lv_obj_create(s_right_cont);
    uint16_t right_w_fallback = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                                - g_sys_config.panel_gap_u * AREX_BASE_U;
    uint16_t sub_w = s_cached_right_w > 0 ? s_cached_right_w : right_w_fallback;
    lv_obj_set_size(s_modal, sub_w,
                     g_sys_config.safe_zone_h);
    lv_obj_set_pos(s_modal, 0, 0);
    lv_obj_set_style_bg_color(s_modal, lv_color_make(0,0,0), 0);
    lv_obj_set_style_bg_opa(s_modal, 242, 0);
    lv_obj_set_style_border_width(s_modal, 0, 0);
    lv_obj_add_flag(s_modal, LV_OBJ_FLAG_HIDDEN);

    s_modal_box = lv_obj_create(s_modal);
    lv_obj_set_size(s_modal_box, 400, 260);
    lv_obj_center(s_modal_box);
    lv_obj_set_style_bg_color(s_modal_box, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(s_modal_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_modal_box, AREX_GREEN, 0);
    lv_obj_set_style_border_width(s_modal_box, 4, 0);
    lv_obj_set_style_radius(s_modal_box, 0, 0);
    lv_obj_set_style_pad_all(s_modal_box, 30, 0);
}

/* =========================================================
 * Sub-menu layer
 * ========================================================= */
static void submenu_layer_create(void)
{
    uint16_t right_w_fallback = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                                - g_sys_config.panel_gap_u * AREX_BASE_U;
    uint16_t sub_w = s_cached_right_w > 0 ? s_cached_right_w : right_w_fallback;

    s_submenu_layer = lv_obj_create(s_right_cont);
    lv_obj_set_size(s_submenu_layer, sub_w, g_sys_config.safe_zone_h);
    lv_obj_set_pos(s_submenu_layer, sub_w, 0);
    lv_obj_set_style_bg_color(s_submenu_layer, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(s_submenu_layer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_submenu_layer, 0, 0);
    lv_obj_set_style_pad_all(s_submenu_layer, 0, 0);
    lv_obj_clear_flag(s_submenu_layer, LV_OBJ_FLAG_SCROLLABLE);

    s_submenu_title = lv_label_create(s_submenu_layer);
    lv_obj_set_style_text_color(s_submenu_title, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(s_submenu_title, arex_get_font(AREX_FONT_ID_TITLE), 0);
    lv_obj_set_pos(s_submenu_title, 16, 8);
    lv_label_set_text(s_submenu_title, "> SUB MENU");

    lv_obj_t *title_line = lv_obj_create(s_submenu_layer);
    lv_obj_set_size(title_line, sub_w - 32, 2);
    lv_obj_set_pos(title_line, 16, AREX_CARD_TITLE_H - 2);
    lv_obj_set_style_bg_color(title_line, AREX_DARK, 0);
    lv_obj_set_style_bg_opa(title_line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(title_line, 0, 0);
    lv_obj_set_style_pad_all(title_line, 0, 0);

    s_submenu_list = lv_obj_create(s_submenu_layer);
    lv_obj_set_size(s_submenu_list, sub_w - 15, g_sys_config.safe_zone_h - AREX_CARD_TITLE_H - 10);
    lv_obj_set_pos(s_submenu_list, 0, AREX_CARD_TITLE_H);
    lv_obj_set_style_bg_opa(s_submenu_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_submenu_list, 0, 0);
    lv_obj_set_style_pad_all(s_submenu_list, 0, 0);
    lv_obj_clear_flag(s_submenu_list, LV_OBJ_FLAG_SCROLLABLE);
}

/* =========================================================
 * arex_screen_create — 公开入口
 * ========================================================= */
void arex_screen_create(void)
{
    styles_init();

    s_scr = lv_obj_create(NULL);
    lv_obj_add_style(s_scr, &s_style_screen, 0);

    /* 安全区容器 (相对于 s_scr 居中定位) */
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
    submenu_layer_create();
    modal_create();

    lv_scr_load(s_scr);
}

/* =========================================================
 * Tileview 导航
 * ========================================================= */
void arex_screen_scroll_to_card(uint8_t tile_pos)
{
    /* 【问题三修复】s_tileview 可能为 NULL（布局重建期间） */
    if (!s_tileview) return;

    if (tile_pos >= arex_card_count()) {
        return;
    }
    lv_obj_t *tile = s_tile_objs[tile_pos];
    if (!tile) {
        return;
    }

    if (tile_pos == 0) {
        lv_anim_del(s_tileview, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_obj_set_y(s_tileview, 0);
    }

    lv_obj_set_tile(s_tileview, tile, AREX_TILE_ANIM_ENABLED ? LV_ANIM_ON : LV_ANIM_OFF);

    /* SETUP(最后一页) 不显示 dots，只有 DASH 动态卡片才更新 */
    if (tile_pos >= CARD_POS_DYNAMIC_FIRST && tile_pos < arex_setup_display_pos()) 
    {
        /* 计算逻辑索引：从 DYNAMIC_FIRST 到 tile_pos 之间有多少个有效卡片 */
        uint8_t active_idx = 0;
        for (uint8_t pos = CARD_POS_DYNAMIC_FIRST; pos < tile_pos; pos++)
        {
            if (g_sys_card_order(pos) != CARD_ID_UNUSED) {
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
 * 彻底委托给 arex_widget_set_value()，通过 user_data 烙印自动定位
 * 左侧锚点 + 5F 网格中所有打了烙印的组件并更新数值。
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
 * 左侧面板刷新 — 数据源自动推导引擎
 *
 * 铁律：只读 g_sys_config.left_widgets[] 数组，根据 widget_id 路由到对应的 g_sensor_data 字段。
 * 每次修改布局（调整 g_left_widgets[] 顺序/类型）后，此函数自动同步，无需手动维护。
 * ========================================================= */
/* =========================================================
 * 全屏组件数据同步接口
 *
 * 同时刷新左侧锚点和右侧 5F 自定义网格的所有组件。
 * 内部调用 arex_widget_sync_data() 路由分发器，实现全量数据自动对齐。
 * ========================================================= */
void arex_screen_refresh_all_widgets(void)
{
    /* 1. 同步左侧固定区配置 */
    for (uint8_t i = 0; i < g_sys_config.left_widget_count; i++) {
        if (g_sys_config.left_widgets[i].widget_id != WIDGET_EMPTY) {
            arex_widget_sync_data(g_sys_config.left_widgets[i].widget_id);
        }
    }

    /* 2. 同步右侧全部自定义卡片配置 */
    for (uint8_t card_idx = 0;
         card_idx < g_sys_config.custom_card_count && card_idx < AREX_MAX_CUSTOM_CARDS;
         card_idx++) {
        for (uint8_t i = 0; i < g_sys_config.custom_cards[card_idx].widget_count; i++) {
            if (g_sys_config.custom_cards[card_idx].widgets[i].widget_id != WIDGET_EMPTY) {
                arex_widget_sync_data(g_sys_config.custom_cards[card_idx].widgets[i].widget_id);
            }
        }
    }
}

/* =========================================================
 * 兼容旧接口：仅刷新左侧面板
 * 内部委托给 arex_widget_sync_data()，消除冗余 switch-case
 * ========================================================= */
void arex_screen_refresh_left_panel(void)
{
    for (uint8_t i = 0; i < g_sys_config.left_widget_count; i++) {
        if (g_sys_config.left_widgets[i].widget_id != WIDGET_EMPTY) {
            arex_widget_sync_data(g_sys_config.left_widgets[i].widget_id);
        }
    }
}

/* =========================================================
 * Wall indicators
 * ========================================================= */
static const char *charge_blocks[] = {
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

    for (uint8_t i = 0; i < AREX_DASH_CARD_COUNT; i++) {
        if (!s_scroll_dots[i]) continue;
        bool show = visible && in_dash_or_edit && dots_enabled && (i < visible_dash);
        if (!show) {
            lv_obj_add_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
        if (i == active_idx) {
            lv_obj_set_style_bg_color(s_scroll_dots[i], AREX_GREEN, 0);
            lv_obj_set_style_shadow_width(s_scroll_dots[i], 8, 0);
            lv_obj_set_style_shadow_color(s_scroll_dots[i], AREX_GREEN, 0);
            lv_obj_set_style_shadow_opa(s_scroll_dots[i], 255, 0);
        } else {
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
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *item = lv_obj_get_child(s_info_list, i);
        lv_obj_t *lbl  = lv_obj_get_child(item, 0);
        if (i == idx) {
            lv_obj_set_style_bg_color(item, AREX_GREEN, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, AREX_BLACK, 0);
        } else {
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
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *item  = lv_obj_get_child(s_setup_list, i);
        lv_obj_t *lbl   = lv_obj_get_child(item, 0);
        lv_obj_t *badge = lv_obj_get_child(item, 1);
        if (i == idx) {
            lv_obj_set_style_bg_color(item, AREX_GREEN, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            if (lbl)   lv_obj_set_style_text_color(lbl,   AREX_BLACK, 0);
            if (badge) lv_obj_set_style_text_color(badge, AREX_BLACK, 0);
        } else {
            lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            if (lbl)   lv_obj_set_style_text_color(lbl,   AREX_GREEN, 0);
            if (badge) lv_obj_set_style_text_color(badge, AREX_LIGHT, 0);
        }
    }
}

uint8_t arex_screen_setup_item_count(void)
{
    if (!s_setup_list) return 0;
    return (uint8_t)lv_obj_get_child_cnt(s_setup_list);
}

void arex_screen_register_info_list(lv_obj_t *list)  { s_info_list  = list; }
void arex_screen_register_setup_list(lv_obj_t *list) { s_setup_list = list; }

/* =========================================================
 * Sub-menu layer
 * ========================================================= */
static void submenu_slide_in(void)
{
    uint16_t right_w_fallback = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                                - g_sys_config.panel_gap_u * AREX_BASE_U;
    uint16_t slide_w = s_cached_right_w > 0 ? s_cached_right_w : right_w_fallback;

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
    uint16_t right_w_fallback = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                                - g_sys_config.panel_gap_u * AREX_BASE_U;
    uint16_t slide_w = s_cached_right_w > 0 ? s_cached_right_w : right_w_fallback;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_submenu_layer);
    lv_anim_set_values(&a, 0, slide_w);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_time(&a, 250);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_start(&a);
}

static void submenu_populate(const char *title, const char **items, uint8_t count)
{
    lv_label_set_text(s_submenu_title, title);
    lv_obj_clean(s_submenu_list);
    s_light_status_lbl = NULL;  /* 重置 LIGHT 状态标签 */

    /* right_w 从缓存读取，fallback = safe_zone_w - left_anchor_w - panel_gap */
    uint16_t right_w = (s_cached_right_w > 0)
        ? s_cached_right_w
        : (g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W - g_sys_config.panel_gap_u * AREX_BASE_U);
    uint16_t sub_w = right_w;
    int item_h = (int)(g_sys_config.h_menu_item * AREX_BASE_U);  /* 5U=50px */
    int item_w = (int)sub_w - 15;
    int gap_y  = (int)(g_sys_config.gap_menu * AREX_BASE_U);   /* 1U=10px */
    int current_y = 0;

    for (uint8_t i = 0; i < count; i++) {
        lv_obj_t *item = lv_obj_create(s_submenu_list);
        lv_obj_set_pos(item, 0, current_y);
        lv_obj_set_size(item, item_w, item_h);
        lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(item, AREX_DARK, 0);
        lv_obj_set_style_border_width(item, AREX_INNER_BORDER_W, LV_PART_MAIN);
        lv_obj_set_style_radius(item, 0, 0);
        lv_obj_set_style_pad_all(item, 0, LV_PART_MAIN);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

        /* LIGHT CONTROL 特殊布局: LIGHT 左, ON/OFF 右 */
        if (strcmp(title, "LIGHT CONTROL") == 0 && i == 0) {
            /* "LIGHT" 标签在左侧 */
            lv_obj_t *lbl_light = lv_label_create(item);
            lv_obj_set_style_text_color(lbl_light, AREX_GREEN, 0);
            lv_obj_set_style_text_font(lbl_light, arex_get_font(AREX_FONT_ID_TITLE), 0);
            lv_obj_set_size(lbl_light, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align(lbl_light, LV_ALIGN_LEFT_MID, 12, 0);
            lv_obj_set_style_text_align(lbl_light, LV_TEXT_ALIGN_LEFT, 0);
            lv_label_set_text(lbl_light, "LIGHT");

            /* "ON"/"OFF" 标签在右侧 */
            lv_obj_t *lbl_status = lv_label_create(item);
            lv_obj_set_style_text_color(lbl_status, g_light_power_state ? AREX_GREEN : AREX_LIGHT, 0);
            lv_obj_set_style_text_font(lbl_status, arex_get_font(AREX_FONT_ID_TITLE), 0);
            lv_obj_set_size(lbl_status, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align(lbl_status, LV_ALIGN_RIGHT_MID, -12, 0);
            lv_obj_set_style_text_align(lbl_status, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(lbl_status, g_light_power_state ? "ON" : "OFF");

            /* 保存状态标签引用，用于点击时更新 */
            s_light_status_lbl = lbl_status;
            current_y += item_h + gap_y;
            continue;
        }

        /* 普通菜单项 */
        lv_obj_t *lbl = lv_label_create(item);
        lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
        lv_obj_set_style_text_font(lbl, arex_get_font(AREX_FONT_ID_TITLE), 0);
        lv_obj_set_size(lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 12, 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_LEFT, 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_label_set_text(lbl, items[i]);

        current_y += item_h + gap_y;
    }
    arex_screen_set_submenu_selection(0);
}

void arex_screen_set_submenu_selection(uint8_t idx)
{
    uint32_t cnt = lv_obj_get_child_cnt(s_submenu_list);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *item = lv_obj_get_child(s_submenu_list, i);
        lv_obj_t *lbl  = lv_obj_get_child(item, 0);
        /* 正在编辑的 item 由 begin_edit_value 单独管理，不参与选中态刷新 */
        if (g_ui.edit_ctx.active && (uint8_t)i == g_ui.edit_ctx.item_index) continue;
        if (i == idx) {
            lv_obj_add_state(item, LV_STATE_FOCUSED);  // HOTFIX: Clear LVGL states to fix bold residue.
            lv_obj_set_style_bg_color(item, AREX_GREEN, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, AREX_BLACK, 0);
            /* LIGHT CONTROL 特殊处理：第二列（ON/OFF）也变黑色 */
            lv_obj_t *lbl2 = lv_obj_get_child(item, 1);
            if (lbl2) lv_obj_set_style_text_color(lbl2, AREX_BLACK, 0);
        } else {
            lv_obj_clear_state(item, LV_STATE_FOCUSED | LV_STATE_EDITED | LV_STATE_CHECKED);  // HOTFIX: Clear LVGL states to fix bold residue.
            if (lbl) lv_obj_clear_state(lbl, LV_STATE_ANY);  // HOTFIX: Clear LVGL states to fix bold residue.
            lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
            /* LIGHT CONTROL 特殊处理：第二列（ON/OFF）恢复状态色 */
            lv_obj_t *lbl2 = lv_obj_get_child(item, 1);
            if (lbl2) lv_obj_set_style_text_color(lbl2, g_light_power_state ? AREX_GREEN : AREX_LIGHT, 0);
        }
    }
}

/* INFO sub-menu */
static const char *s_info_titles[] = {
    "> LAST DIVE", "> DIVE PLAN", "> TISSUE & TOX", "> GAS & CALC", "> SENSOR & DEVICE"
};

static char s_info_str[5][5][32];
static const char *s_info_dyn[5][6];

// HOTFIX: Removed soft BACK buttons.
static void build_info_submenu(uint8_t idx)
{
    uint8_t n = 0;
    switch (idx) {
        case 0:
            snprintf(s_info_str[0][0], 32, "MAX DEPTH: %dm", (int)g_sensor_data.depth);
            snprintf(s_info_str[0][1], 32, "DIVE TIME: %dm", (int)(g_sensor_data.dive_time_s / 60));
            s_info_dyn[0][n++] = s_info_str[0][0];
            s_info_dyn[0][n++] = s_info_str[0][1];
            s_info_dyn[0][n++] = "SURFACE INT: 2h 10m";
            break;
        case 1:
            s_info_dyn[1][n++] = "VIEW PROFILE";
            s_info_dyn[1][n++] = "RECALCULATE";
            break;
        case 2:
            snprintf(s_info_str[2][0], 32, "GF: %d/%d", 30, 70);
            snprintf(s_info_str[2][1], 32, "CNS: %d%%", g_sensor_data.cns_pct);
            snprintf(s_info_str[2][2], 32, "OTU: %d", g_sensor_data.otu);
            s_info_dyn[2][n++] = "VIEW BAR GRAPH";
            s_info_dyn[2][n++] = s_info_str[2][0];
            s_info_dyn[2][n++] = s_info_str[2][1];
            s_info_dyn[2][n++] = s_info_str[2][2];
            break;
        case 3:
            snprintf(s_info_str[3][0], 32, "GAS 1: %s", g_sensor_data.gas_name);
            s_info_dyn[3][n++] = s_info_str[3][0];
            s_info_dyn[3][n++] = "ALGO: ZHL-16C";
            break;
        case 4:
            if (g_sensor_data.pod1_bar <= 0.0f)
                snprintf(s_info_str[4][0], 32, "POD 1: -- BAR");
            else
                snprintf(s_info_str[4][0], 32, "POD 1: %.0f BAR", g_sensor_data.pod1_bar);
            if (g_sensor_data.pod2_bar <= 0.0f)
                snprintf(s_info_str[4][1], 32, "POD 2: -- BAR");
            else
                snprintf(s_info_str[4][1], 32, "POD 2: %.0f BAR", g_sensor_data.pod2_bar);
            snprintf(s_info_str[4][2], 32, "BATTERY: %.0f%%", g_sensor_data.battery_pct);
            snprintf(s_info_str[4][3], 32, "TEMP: 24C");
            s_info_dyn[4][n++] = s_info_str[4][0];
            s_info_dyn[4][n++] = s_info_str[4][1];
            s_info_dyn[4][n++] = s_info_str[4][2];
            s_info_dyn[4][n++] = s_info_str[4][3];
            break;
        default:
            break;
    }
    s_info_dyn[idx][n] = NULL;
}

void arex_screen_open_info_submenu(uint8_t item_idx)
{
    if (item_idx >= 5) return;
    build_info_submenu(item_idx);
    uint8_t count = 0;
    while (count < 6 && s_info_dyn[item_idx][count]) count++;
    submenu_populate(s_info_titles[item_idx], s_info_dyn[item_idx], count);
    g_ui.sub_item_count = count;
    g_ui.sub_menu_idx   = 0;
    g_ui.sub_parent     = UI_INFO;
    g_ui.state          = UI_SUB_MENU;
    submenu_slide_in();
}


/* SETUP sub-menu */
// Remove 'SELECT ' prefix
static const char *s_setup_sub[][6] = {
    { "AIR", "NX 32", "TX 18/45", "O2 100%", NULL },
    { "LOW (GF 40/85)", "MED (GF 30/70)", "HIGH (GF 20/65)", NULL },
    { "800", "1200", "1600", "2400", NULL },
    { "START CALIBRATION", NULL },
    { "LIGHT ON/OFF", "RED COLOR", "GREEN COLOR", "BLUE COLOR", "WHITE COLOR", NULL },
    { "VERSION: " AREX_SYSTEM_VERSION, "MODE SETUP", "DIVE MENU", "AI SETUP", "ALERTS SETUP", "DISPLAY" },
};

static const char *s_setup_titles[] = {
    "GAS SWITCH", "CONSERATISM", "BRIGHTNESS", "COMPASS CAL", "LIGHT CONTROL", "SYSTEMS SETUP"
};

static const char *s_nested_red[]    = { "10%", "30%", "50%", "70%", "100%", NULL };
static const char *s_nested_green[]  = { "10%", "30%", "50%", "70%", "100%", NULL };
static const char *s_nested_blue[]   = { "10%", "30%", "50%", "70%", "100%", NULL };
static const char *s_nested_white[]  = { "10%", "30%", "50%", "70%", "100%", NULL };

void arex_screen_open_setup_submenu(uint8_t item_idx)
{
    if (item_idx >= 6) return;
    uint8_t count = 0;
    while (count < 6 && s_setup_sub[item_idx][count]) count++;
    submenu_populate(s_setup_titles[item_idx], s_setup_sub[item_idx], count);
    g_ui.sub_item_count = count;
    g_ui.sub_menu_idx   = 0;
    g_ui.sub_parent     = UI_SETUP;
    g_ui.state          = UI_SUB_MENU;
    submenu_slide_in();
}

/* Nested sub-menus */
static const char *s_nested_mode_setup[]   = { "AIR", "NITROX", "3 GAS NX", "GAUGE", NULL };
static const char *s_nested_ai_setup[]     = { "PAIR T1", "PAIR T2", "GTR MODE: ON", NULL };
static const char *s_nested_alerts_setup[] = { "DEPTH ALARM: 40m", "TIME ALARM: 60m", "LOW NDL: 5m", "TEST VIBRATION", NULL };
static const char *s_nested_display_sys[]  = { "UNITS: METRIC", "DATE & CLOCK", "LOG RATE: 10s", "BLUETOOTH: OFF", "RESET DEFAULTS", NULL };

static char s_modppo2_str[20];
static const char *s_nested_dive_setup[5];

static void build_nested_dive_setup(void)
{
    extern arex_sensor_data_t g_sensor_data;
    (void)g_sensor_data;
    snprintf(s_modppo2_str, sizeof(s_modppo2_str), "MOD PO2: %.1f", 1.4f);
    s_nested_dive_setup[0] = "SALINITY: FRESH";
    s_nested_dive_setup[1] = s_modppo2_str;
    s_nested_dive_setup[2] = "SAFETY STOP: 3 MIN";
    s_nested_dive_setup[3] = "ALTITUDE: AUTO";
    s_nested_dive_setup[4] = NULL;
}

static const char **nested_items_for(const char *title, uint8_t *out_count)
{
    const char **tbl = NULL;
    if      (strcmp(title, "MODE SETUP")    == 0) tbl = s_nested_mode_setup;
    else if (strcmp(title, "DIVE MENU")    == 0) { build_nested_dive_setup(); tbl = s_nested_dive_setup; }
    else if (strcmp(title, "AI SETUP")      == 0) tbl = s_nested_ai_setup;
    else if (strcmp(title, "ALERTS SETUP")  == 0) tbl = s_nested_alerts_setup;
    else if (strcmp(title, "DISPLAY") == 0) tbl = s_nested_display_sys;
    else if (strcmp(title, "RED")    == 0) tbl = s_nested_red;
    else if (strcmp(title, "GREEN")  == 0) tbl = s_nested_green;
    else if (strcmp(title, "BLUE")   == 0) tbl = s_nested_blue;
    else if (strcmp(title, "WHITE")  == 0) tbl = s_nested_white;

    if (tbl && out_count) {
        *out_count = 0;
        while (*out_count < 8 && tbl[*out_count]) (*out_count)++;
    }
    return tbl;
}

static void submenu_history_push(void)
{
    if (g_ui.sub_history_depth >= AREX_SUB_HISTORY_MAX) return;
    arex_sub_history_t *h = &g_ui.sub_history[g_ui.sub_history_depth];
    const char *cur_title = lv_label_get_text(s_submenu_title);
    lv_snprintf(h->title, sizeof(h->title), "%s", cur_title ? cur_title : "");
    h->idx = g_ui.sub_menu_idx;
    g_ui.sub_history_depth++;
}

void arex_screen_open_nested_submenu(const char *title, const char **items, uint8_t count)
{
    submenu_history_push();
    char full_title[40];
    lv_snprintf(full_title, sizeof(full_title), "> %s", title);
    submenu_populate(full_title, items, count);
    g_ui.sub_item_count = count;
    g_ui.sub_menu_idx   = 0;
    g_ui.state = UI_SUB_MENU;
}

void arex_screen_handle_submenu_select(uint8_t item_idx)
{
    if (item_idx >= g_ui.sub_item_count) return;
    lv_obj_t *item = lv_obj_get_child(s_submenu_list, item_idx);
    lv_obj_t *lbl  = item ? lv_obj_get_child(item, 0) : NULL;
    if (!lbl) return;
    const char *text = lv_label_get_text(lbl);
    if (!text) return;

    const char *raw_title = lv_label_get_text(s_submenu_title);
    char cur_title[40] = {0};
    if (raw_title) {
        const char *p = raw_title;
        if (p[0] == '>' && p[1] == ' ') p += 2;
        lv_snprintf(cur_title, sizeof(cur_title), "%s", p);
    }

    if (strcmp(text, "< BACK") == 0) {
        arex_screen_close_submenu();
        return;
    }

    // HOTFIX: Block action for Info items.
    if (strcmp(cur_title, "LAST DIVE") == 0 ||
        strcmp(cur_title, "TISSUE & TOX") == 0 ||
        strcmp(cur_title, "GAS & CALC") == 0 ||
        strcmp(cur_title, "SENSOR & DEVICE") == 0) {
        return;
    }

    /* LIGHT CONTROL 颜色选项处理（必须在通用 > 处理之前） */
    if (strcmp(cur_title, "LIGHT CONTROL") == 0 && strstr(text, "COLOR") != NULL) {
        /* 从 "RED COLOR >" 提取颜色名 */
        char color_name[20] = {0};
        if (strncmp(text, "RED", 3) == 0) strcpy(color_name, "RED");
        else if (strncmp(text, "GREEN", 5) == 0) strcpy(color_name, "GREEN");
        else if (strncmp(text, "BLUE", 4) == 0) strcpy(color_name, "BLUE");
        else if (strncmp(text, "WHITE", 5) == 0) strcpy(color_name, "WHITE");

        /* 通过 nested_items_for 获取颜色亮度选项（专门的二级嵌套菜单） */
        uint8_t ncnt = 0;
        const char **color_items = nested_items_for(color_name, &ncnt);
        if (color_items && ncnt > 0) {
            arex_screen_open_nested_submenu(color_name, color_items, ncnt);
        }
        return;
    }

    /* LIGHT CONTROL 开关处理（第一项，点击切换 ON/OFF 状态） */
    if (strcmp(cur_title, "LIGHT CONTROL") == 0 && item_idx == 0) {
        extern void arex_bus_set_light_power(bool on);
        g_light_power_state = !g_light_power_state;
        arex_bus_set_light_power(g_light_power_state);

        /* 更新状态标签 */
        if (s_light_status_lbl) {
            lv_label_set_text(s_light_status_lbl, g_light_power_state ? "ON" : "OFF");
        }
        /* 重新设置选中态，确保 ON/OFF 变黑色 */
        arex_screen_set_submenu_selection(g_ui.sub_menu_idx);
        return;
    }

    if (text[strlen(text) - 1] == '>') {
        char nested_name[40] = {0};
        size_t len = strlen(text);
        size_t copy_len = (len >= 2) ? len - 2 : 0;
        if (copy_len >= sizeof(nested_name)) copy_len = sizeof(nested_name) - 1;
        memcpy(nested_name, text, copy_len);
        while (copy_len > 0 && nested_name[copy_len - 1] == ' ') {
            nested_name[--copy_len] = '\0';
        }
        uint8_t ncnt = 0;
        const char **nitems = nested_items_for(nested_name, &ncnt);
        if (nitems && ncnt > 0) {
            arex_screen_open_nested_submenu(nested_name, nitems, ncnt);
        }
        return;
    }

    if (strcmp(cur_title, "GAS SWITCH") == 0) {
        const char *gas_name = text;
        if (strncmp(text, "SELECT ", 7) == 0) gas_name = text + 7;
        extern const char *AREX_GAS_NAMES[4];
        for (uint8_t i = 0; i < 4; i++) {
            if (strcmp(AREX_GAS_NAMES[i], gas_name) == 0) {
                // HOTFIX: Route gas switch to safety modal.
                g_ui.gas_cursor = i;
                g_ui.gas_modal_from_submenu = true;  // HOTFIX: Route GAS modal exit based on context.
                arex_screen_show_modal_gas();
                g_ui.state = UI_MODAL_GAS;
                return;
            }
        }
        return;
    }

    if (strcmp(cur_title, "CONSERVATISM") == 0) {
        if (strcmp(text, "< BACK") != 0) {
            /* 解析 conservatism 级别：LOW/MED/HIGH */
            uint8_t level = 1;  /* 默认 MED */
            if (strncmp(text, "LOW", 3) == 0) level = 0;
            else if (strncmp(text, "MED", 3) == 0) level = 1;
            else if (strncmp(text, "HIGH", 4) == 0) level = 2;

            /* 调用外部业务层回调（weak 实现会调用内部 bus） */
            extern void arex_ui_on_conservatism_set(uint8_t level);
            arex_ui_on_conservatism_set(level);

            arex_screen_refresh_setup_menu();
        }
        arex_screen_update_setup_badge(1, text);
        arex_screen_close_submenu();
        return;
    }

    if (strcmp(cur_title, "BRIGHTNESS") == 0) {
        if (strcmp(text, "< BACK") != 0) {
            /* 设置亮度并更新 badge */
            if (strcmp(text, "LOW") == 0) {
                g_sys_config.brightness = 0;
            } else if (strcmp(text, "MED") == 0) {
                g_sys_config.brightness = 1;
            } else if (strcmp(text, "HIGH") == 0) {
                g_sys_config.brightness = 2;
            } else if (strcmp(text, "MAX") == 0) {
                g_sys_config.brightness = 3;
            }
            /* 实际设置屏幕亮度（模拟器版本） */
            arex_set_brightness(g_sys_config.brightness);
        }
        arex_screen_update_setup_badge(2, text);
        arex_screen_close_submenu();
        return;
    }

    /* 颜色亮度调节子菜单处理（RED/GREEN/BLUE/WHITE） */
    if (strcmp(cur_title, "RED") == 0 || strcmp(cur_title, "GREEN") == 0 ||
        strcmp(cur_title, "BLUE") == 0 || strcmp(cur_title, "WHITE") == 0) {
        /* 用户选择了亮度百分比 */
        if (strcmp(text, "< BACK") != 0) {
            /* 回调给业务层处理 */
            extern void arex_ui_on_light_color_set(const char *color, const char *level);
            arex_ui_on_light_color_set(cur_title, text);

            /* 选中亮度后自动开灯 */
            if (!g_light_power_state) {
                g_light_power_state = true;
                extern void arex_bus_set_light_power(bool on);
                arex_bus_set_light_power(true);
            }
        }
        /* 直接关闭子菜单返回上级，不弹窗 */
        arex_screen_close_submenu();
        return;
    }

    if (strcmp(cur_title, "DIVE MENU") == 0) {
        if (strncmp(text, "MOD PO2:", 8) == 0 || strncmp(text, "MOD PO2 ", 8) == 0) {
            arex_screen_begin_edit_value(item_idx, 1.4f, 1.0f, 1.6f, 0.1f);
            return;
        }
        arex_screen_show_modal_act(text);
        return;
    }

    arex_screen_show_modal_act(text);
}

void arex_screen_close_submenu(void)
{
    if (g_ui.sub_history_depth > 0) {
        g_ui.sub_history_depth--;
        arex_sub_history_t *h = &g_ui.sub_history[g_ui.sub_history_depth];
        const char *prev_title = h->title;
        if (prev_title[0] == '>' && prev_title[1] == ' ') prev_title += 2;

        bool found = false;
        for (uint8_t i = 0; i < 5 && !found; i++) {
            const char *setup_title_stripped = s_setup_titles[i];
            if (setup_title_stripped[0] == '>' && setup_title_stripped[1] == ' ')
                setup_title_stripped += 2;
            if (strcmp(prev_title, setup_title_stripped) == 0) {
                uint8_t cnt = 0;
                while (cnt < 6 && s_setup_sub[i][cnt]) cnt++;
                submenu_populate(s_setup_titles[i], s_setup_sub[i], cnt);
                g_ui.sub_item_count = cnt;
                g_ui.sub_menu_idx   = h->idx;
                arex_screen_set_submenu_selection(h->idx);
                found = true;
            }
        }
        if (!found) {
            uint8_t ncnt = 0;
            const char **nitems = nested_items_for(prev_title, &ncnt);
            if (nitems && ncnt > 0) {
                char full_title[40];
                lv_snprintf(full_title, sizeof(full_title), "> %s", prev_title);
                submenu_populate(full_title, nitems, ncnt);
                g_ui.sub_item_count = ncnt;
                g_ui.sub_menu_idx   = h->idx;
                arex_screen_set_submenu_selection(h->idx);
                found = true;
            }
        }
        g_ui.state = UI_SUB_MENU;
        return;
    }
    submenu_slide_out();
    g_ui.state = g_ui.sub_parent;
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
 * Modal
 * ========================================================= */
static void modal_act_timer_cb(lv_timer_t *t)
{
    (void)t;
    arex_screen_hide_modal();
    if (g_ui.state == UI_MODAL_ACT) {
        g_ui.state = UI_SUB_MENU;
    }
    lv_timer_del(t);
}

static void modal_set_content(const char *title, const char *body, const char *hint)
{
    lv_obj_clean(s_modal_box);

    lv_obj_t *t = lv_label_create(s_modal_box);
    lv_obj_set_style_text_color(t, AREX_GREEN, 0);
    lv_obj_set_style_text_font(t, arex_get_font(AREX_FONT_ID_TITLE), 0);
    lv_label_set_text(t, title);
    lv_obj_set_pos(t, 0, 0);

    lv_obj_t *b = lv_label_create(s_modal_box);
    lv_obj_set_style_text_color(b, AREX_GREEN, 0);
    lv_obj_set_style_text_font(b, arex_get_font(AREX_FONT_ID_MEDIUM), 0);
    lv_label_set_text(b, body);
    lv_obj_set_pos(b, 0, 40);

    lv_obj_t *h = lv_label_create(s_modal_box);
    lv_obj_set_style_text_color(h, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(h, arex_get_font(AREX_FONT_ID_SMALL), 0);
    lv_label_set_text(h, hint);
    lv_obj_set_pos(h, 0, 100);
}

void arex_screen_show_modal_act(const char *action_text)
{
    modal_set_content("ACTION", action_text ? action_text : "", "[ ESC TO BACK ]");
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
    g_ui.state = UI_MODAL_ACT;
    lv_timer_create(modal_act_timer_cb, 1000, NULL);
}

void arex_screen_show_modal_gas(void)
{
    uint8_t ci = g_ui.gas_cursor;
    extern const char *AREX_GAS_NAMES[4];
    extern const uint8_t AREX_GAS_MOD_M[4];
    char body[32];
    snprintf(body, sizeof(body), "%s\nMOD: %dm", AREX_GAS_NAMES[ci], AREX_GAS_MOD_M[ci]);

    const char *hint = (g_sensor_data.depth > AREX_GAS_MOD_M[ci])
        ? "[ FATAL: OVER MOD ]"
        : "[ ENTER CONFIRM ]  [ ESC CANCEL ]";

    modal_set_content("CONFIRM GAS", body, hint);
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}

void arex_screen_show_modal_compass(void)
{
    modal_set_content("CLEAR TARGET?", "REMOVE HEADING MARKER?",
                      "[ ENTER CONFIRM ]  [ ESC CANCEL ]");
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}

void arex_screen_pulse_modal(void)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_modal_box);
    lv_anim_set_values(&a, 0, 6);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_time(&a, 80);
    lv_anim_set_playback_time(&a, 80);
    lv_anim_set_repeat_count(&a, 2);
    lv_anim_start(&a);
}

void arex_screen_hide_modal(void)
{
    lv_obj_add_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
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

/* =========================================================
 * Inline value edit (MOD PO2 pattern)
 * ========================================================= */
static lv_timer_t *s_edit_flash_timer;
static lv_obj_t   *s_edit_flash_badge;
static lv_obj_t   *s_edit_flash_val_lbl;
static bool        s_edit_flash_on;

static void edit_flash_timer_cb(lv_timer_t *t)
{
    (void)t;
    s_edit_flash_on = !s_edit_flash_on;
    if (s_edit_flash_val_lbl) {
        /* 文字颜色在绿/暗绿之间闪烁，无背景色切换 */
        lv_color_t fg = s_edit_flash_on ? AREX_GREEN : AREX_DARK;
        lv_obj_set_style_text_color(s_edit_flash_val_lbl, fg, 0);
    }
}

static void edit_flash_stop(void)
{
    if (s_edit_flash_timer) {
        lv_timer_del(s_edit_flash_timer);
        s_edit_flash_timer = NULL;
    }
    s_edit_flash_badge   = NULL;
    s_edit_flash_val_lbl = NULL;
}

static void edit_flash_start(void)
{
    if (s_edit_flash_timer) {
        lv_timer_del(s_edit_flash_timer);
        s_edit_flash_timer = NULL;
    }
    s_edit_flash_on = true;
    s_edit_flash_timer = lv_timer_create(edit_flash_timer_cb, 350, NULL);
}

static void edit_value_cleanup(lv_obj_t *item);

void arex_screen_refresh_edit_value(void)
{
    if (!g_ui.edit_ctx.active || !s_edit_flash_val_lbl) return;
    static float last_drawn = -9999.f;
    float cur = g_ui.edit_ctx.value;
    if (cur == last_drawn) return;   /* dirty check：值未变则跳过，不触发重绘 */
    last_drawn = cur;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f ^v", cur);
    lv_label_set_text(s_edit_flash_val_lbl, buf);
}

void arex_screen_begin_edit_value(uint8_t item_idx, float value,
                                   float min, float max, float step)
{
    g_ui.edit_ctx.value      = value;
    g_ui.edit_ctx.original   = value;
    g_ui.edit_ctx.min        = min;
    g_ui.edit_ctx.max        = max;
    g_ui.edit_ctx.step       = step;
    g_ui.edit_ctx.item_index = item_idx;
    g_ui.edit_ctx.active     = true;
    g_ui.state = UI_EDIT_VALUE;

    lv_obj_t *item = lv_obj_get_child(s_submenu_list, item_idx);
    if (!item) return;

    /* 从选中态切换到编辑态：绿底→黑底绿框，title 文字恢复绿色 */
    lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(item, AREX_GREEN, 0);
    lv_obj_set_style_border_width(item, 2, 0);

    /* 复用 child 0 作为左侧标签，恢复绿色文字 */
    lv_obj_t *prefix_lbl = lv_obj_get_child(item, 0);
    if (prefix_lbl) {
        lv_label_set_text(prefix_lbl, "MOD PO2:");
        lv_obj_set_style_text_color(prefix_lbl, AREX_GREEN, 0);
        lv_obj_set_size(prefix_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(prefix_lbl, LV_ALIGN_LEFT_MID, 12, 0);
    }

    /* child 1 是 badge label（CONSERVATISM 等无 badge 时为 NULL），恢复颜色 */
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
    snprintf(buf, sizeof(buf), "%.1f ^v", value);
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
    lv_obj_t *item = lv_obj_get_child(s_submenu_list, g_ui.edit_ctx.item_index);
    if (!item) return;
    edit_value_cleanup(item);
    lv_obj_t *lbl = lv_obj_get_child(item, 0);
    if (lbl) {
        char buf[32];
        snprintf(buf, sizeof(buf), "MOD PO2: %.1f", g_ui.edit_ctx.value);
        lv_label_set_text(lbl, buf);
        lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
    }
    arex_screen_set_submenu_selection(g_ui.sub_menu_idx);
}

void arex_screen_cancel_edit_value(void)
{
    arex_screen_commit_edit_value();
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
 * Light control callbacks (供业务层对接)
 *
 * 以下两个函数是 UI 层与业务层/硬件层的对接入口：
 *   - arex_bus_set_light_power()  : 由业务层实现，控制灯光开关
 *   - arex_ui_on_light_color_set(): 由业务层实现，设置颜色亮度
 * ========================================================= */

/**
 * 灯光开关控制回调
 *
 * 调用时机：当用户在 SETUP > LIGHT CONTROL > LIGHT ON/OFF 点击时触发
 * 调用方向：arex_screen.c -> 业务层
 *
 * @param on  true=开灯, false=关灯
 *
 * 【业务层对接方式】
 * 在业务层重新定义此函数，控制 GPIO：
 *
 *   void arex_bus_set_light_power(bool on) {
 *       if (on) {
 *           GPIO_SetBits(PORT_LIGHT_EN, PIN_LIGHT_EN);
 *       } else {
 *           GPIO_ResetBits(PORT_LIGHT_EN, PIN_LIGHT_EN);
 *       }
 *   }
 */
#ifdef PC_SIMULATOR
#else
__attribute__((weak))    //这个在真机需要打开，这个是用来弱定义的
#endif
void arex_bus_set_light_power(bool on)
{
    /* TODO: 业务层实现
     *
     * 示例伪代码：
     *   extern void hw_light_set_power(bool state);
     *   hw_light_set_power(on);
     *
     * 此处仅打印日志供调试
     */
    printf("[LIGHT] Power: %s\n", on ? "ON" : "OFF");
}

/**
 * 灯光颜色亮度设置回调
 *
 * 调用时机：当用户在 SETUP > LIGHT CONTROL > [COLOR] > [LEVEL] 点击时触发
 * 调用方向：arex_screen.c -> 业务层
 *
 * @param color  颜色名称: "RED", "GREEN", "BLUE", "WHITE"
 * @param level 亮度级别: "10%", "30%", "50%", "70%", "100%"
 *
 * 【业务层对接方式】
 * 在业务层实现此函数，处理 RGBW PWM 控制：
 *
 *   void arex_ui_on_light_color_set(const char *color, const char *level) {
 *       uint8_t duty = 0;
 *       if (strncmp(level, "10", 2) == 0) duty = 25;
 *       else if (strncmp(level, "30", 2) == 0) duty = 76;
 *       else if (strncmp(level, "50", 2) == 0) duty = 127;
 *       else if (strncmp(level, "70", 2) == 0) duty = 178;
 *       else duty = 255;
 *       if (strncmp(color, "RED", 3) == 0) set_pwm(CH_RED, duty);
 *       else if (strncmp(color, "GREEN", 5) == 0) set_pwm(CH_GREEN, duty);
 *       else if (strncmp(color, "BLUE", 4) == 0) set_pwm(CH_BLUE, duty);
 *       else if (strncmp(color, "WHITE", 5) == 0) set_pwm(CH_WHITE, duty);
 *   }
 */
#ifdef PC_SIMULATOR
#else
__attribute__((weak))    //这个在真机需要打开，这个是用来弱定义的
#endif
void arex_ui_on_light_color_set(const char *color, const char *level)
{
    /* TODO: 业务层实现
     *
     * 此处仅打印日志供调试
     */
    printf("[LIGHT] Color: %s, Level: %s\n", color, level);
}

/**
 * 屏幕亮度设置回调
 *
 * 调用时机：当用户在 SETUP > BRIGHTNESS 选择 LOW/MED/HIGH/MAX 时触发
 * 调用方向：arex_screen.c -> 业务层
 *
 * @param level 亮度级别: 0=LOW, 1=MED, 2=HIGH, 3=MAX
 *
 * 【业务层对接方式】
 * 在业务层实现此函数，控制屏幕背光 PWM：
 *
 *   void arex_set_brightness(uint8_t level) {
 *       static const uint8_t brightness_map[4] = {25, 76, 178, 255};
 *       uint8_t duty = brightness_map[level & 0x03];
 *       set_pwm(BACKLIGHT_CHANNEL, duty);
 *   }
 */
#ifdef PC_SIMULATOR
#else
__attribute__((weak))    //这个在真机需要打开，这个是用来弱定义的
#endif
void arex_set_brightness(uint8_t level)
{
    /* 亮度级别到 OPA 映射: LOW=40%, MED=70%, HIGH=90%, MAX=100% */
    static const lv_opa_t brightness_opa[4] = {102, 178, 229, 255};
    lv_opa_t opa = brightness_opa[level & 0x03];

    /* 通过调整所有面板的全局透明度来模拟亮度效果 */
    lv_disp_t *disp = lv_disp_get_default();
    if (disp) {
        /* 实际真机上应该直接控制背光 PWM，这里用透明度模拟 */
        printf("[BRIGHTNESS] Level: %d (OPA: %d)\n", level, opa);
    }

    /* TODO: 业务层实现
     *
     * 示例伪代码：
     *   extern void hw_backlight_set_duty(uint8_t duty);
     *   hw_backlight_set_duty(brightness_map[level]);
     *
     * 真机实现示例（STM32 PWM）：
     *   static const uint16_t brightness_duty[4] = {100, 200, 400, 1023};
     *   __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, brightness_duty[level & 0x03]);
     */
}

/**
 * CONSERVATISM 保守度设置回调
 *
 * 调用时机：当用户在 SETUP > CONSERVATISM 选择 LOW/MED/HIGH 时触发
 * 调用方向：arex_screen.c -> 业务层
 *
 * @param level 保守度级别: 0=LOW, 1=MED, 2=HIGH
 *
 * 【业务层对接方式】
 * 在业务层实现此函数，控制减压算法的保守度参数：
 *
 *   void arex_ui_on_conservatism_set(uint8_t level) {
 *       g_sys_config.conservatism = level;
 *       recalculate_deco_plan();  // 重新计算减压计划
 *   }
 */
#ifdef PC_SIMULATOR
#else
__attribute__((weak))
#endif
void arex_ui_on_conservatism_set(uint8_t level)
{
    /* 调用内部 bus 更新系统配置 */
    extern void arex_bus_set_conservatism(uint8_t level);
    arex_bus_set_conservatism(level);
}

/* 获取 Safe Zone 容器对象（供告警横幅使用） */
lv_obj_t *arex_get_safe_zone(void)
{
    return s_safe_zone;
}
