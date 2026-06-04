/*
 * 文件: src/app_ui/ui/screen/layout_view.c
 * 作用: 该文件属于屏幕或页面编排模块，负责整屏布局、分页切换、覆盖层、编辑态显示或页面注册管理。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "../core/ui_engine.h"
#include "../core/data.h"
#include "../core/ui_vm.h"
#include "../core/vm/ui_vm_dashboard.h"
#include "layout_view.h"
#include "../comp/comp_style.h"
#include "../comp/comp_view.h"

#include <stdio.h>

#define COMP_GAP  0

static lv_obj_t *s_left_bat_lbl = NULL;
static lv_obj_t *s_left_prj_lbl = NULL;

static bool layout_label_is_valid(lv_obj_t **obj_ref)
{
    /* 句柄失效后直接置空，避免后续继续操作已经被 LVGL 删除的对象。 */
    if (obj_ref == NULL || *obj_ref == NULL)
    {
        return false;
    }

    if (!lv_obj_is_valid(*obj_ref))
    {
        *obj_ref = NULL;
        return false;
    }

    return true;
}

bool safe_zone_in_danger(void)
{
    /* 这个判断用于检测安全区偏移是否已经超过屏幕容忍范围。 */
    /* safe zone 是整个 UI 的“可安全显示矩形”。
     * 只要偏移过大，哪怕局部布局算法本身没错，最终也可能出现：
     * - 内容超出物理屏边界
     * - 遮罩裁切到有效显示区
     * 所以这里是 ui_apply_config() 的第一道几何防线。 */
    uint16_t safe_w = ui_safe_zone_w_get();
    uint16_t safe_h = ui_safe_zone_h_get();
    int16_t max_offset_x = (int16_t)((PHYSICAL_W - safe_w) / 2);
    int16_t max_offset_y = (int16_t)((PHYSICAL_H - safe_h) / 2);

    if (ui_offset_x_get() < -max_offset_x || ui_offset_x_get() > max_offset_x)
    {
        return true;
    }
    if (ui_offset_y_get() < -max_offset_y || ui_offset_y_get() > max_offset_y)
    {
        return true;
    }

    if (ui_mask_enabled_get())
    {
        int16_t bottom_edge = (int16_t)(PHYSICAL_H / 2 + safe_h / 2 + ui_offset_y_get());
        if (bottom_edge > PHYSICAL_H - MASK_EDGE_GUARD)
        {
            return true;
        }
    }

    return false;
}

void calc_layout_rect(int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h,
                           int16_t anchor_offset_x, int16_t anchor_offset_y)
{
    /* 根据屏幕中心和锚点偏移，计算当前安全区矩形。 */
    int16_t center_x = (int16_t)(PHYSICAL_W / 2) + anchor_offset_x;
    int16_t center_y = (int16_t)(PHYSICAL_H / 2) + anchor_offset_y;

    *out_x = center_x - (int16_t)(ui_safe_zone_w_get() / 2);
    *out_y = center_y - (int16_t)(ui_safe_zone_h_get() / 2);
    *out_w = ui_safe_zone_w_get();
    *out_h = ui_safe_zone_h_get();
}

void calc_tech_layout(int16_t *out_lx, int16_t *out_ly,
                           uint16_t *out_lw, uint16_t *out_lh,
                           int16_t *out_rx, int16_t *out_ry,
                           uint16_t *out_rw, uint16_t *out_rh)
{
    /* TECH 布局把屏幕分为左锚点和右内容区两部分。 */
    uint16_t gap = ui_panel_gap_px_get();
    uint16_t anchor_w = ui_anchor_w_get();
    uint16_t anchor_h = ui_anchor_h_get();
    uint16_t content_w = ui_content_w_get();
    uint16_t content_h = ui_content_h_get();

    if (ui_layout_is_vertical_split())
    {
        if (ui_layout_order_get() == ORDER_NORMAL)
        {
            *out_lx = 0;
            *out_rx = (int16_t)(anchor_w + gap);
        }
        else
        {
            *out_lx = (int16_t)(content_w + gap);
            *out_rx = 0;
        }
        *out_ly = 0;
        *out_ry = 0;
    }
    else
    {
        *out_lx = 0;
        *out_rx = 0;
        if (ui_layout_order_get() == ORDER_NORMAL)
        {
            *out_ly = 0;
            *out_ry = (int16_t)(anchor_h + gap);
        }
        else
        {
            *out_ly = (int16_t)(content_h + gap);
            *out_ry = 0;
        }
    }

    *out_lw = anchor_w;
    *out_lh = anchor_h;
    *out_rw = content_w;
    *out_rh = content_h;
}

void calc_classic_layout(int16_t *out_top_x, int16_t *out_top_y,
                              uint16_t *out_top_w, uint16_t *out_top_h,
                              int16_t *out_bot_x, int16_t *out_bot_y,
                              uint16_t *out_bot_w, uint16_t *out_bot_h)
{
    /* CLASSIC 布局按上下区域堆叠，顶部高度由各功能块高度累加得到。 */
    uint16_t gap = ui_panel_gap_px_get();
    uint16_t top_h = 0;

    top_h += (uint16_t)(ui_depth_h_u_get() * BASE_U);
    top_h += (uint16_t)(ui_ndl_h_u_get() * BASE_U + gap);
    top_h += (uint16_t)(ui_pod_h_u_get() * BASE_U + gap);
    top_h += (uint16_t)(ui_batt_h_u_get() * BASE_U + gap);
    top_h += (uint16_t)(ui_gas_h_u_get() * BASE_U + gap);
    top_h += (uint16_t)(ui_time_h_u_get() * BASE_U);

    if (top_h < MIN_CLASSIC_TOP_H)
    {
        top_h = MIN_CLASSIC_TOP_H;
    }

    uint16_t bottom_h = (ui_safe_zone_h_get() > top_h + gap)
                        ? (ui_safe_zone_h_get() - top_h - gap)
                        : MIN_CLASSIC_TOP_H;

    if (ui_layout_order_get() == ORDER_NORMAL)
    {
        *out_top_x = 0;
        *out_bot_x = 0;
        *out_top_y = 0;
        *out_bot_y = (int16_t)(top_h + gap);
    }
    else
    {
        *out_top_x = 0;
        *out_bot_x = 0;
        *out_top_y = (int16_t)(bottom_h + gap);
        *out_bot_y = 0;
    }

    *out_top_w = ui_safe_zone_w_get();
    *out_top_h = top_h;
    *out_bot_w = ui_safe_zone_w_get();
    *out_bot_h = bottom_h;
}

void calc_widget_cell(uint16_t parent_w, uint16_t parent_h,
                           uint8_t row, uint8_t col,
                           uint8_t w_span, uint8_t h_span,
                           int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h)
{
    /* 自定义网格按当前布局方向拆分：side=5x6，top/bottom=7x4。 */
    uint8_t grid_cols = ui_custom_grid_cols_get();
    uint8_t grid_rows = ui_custom_grid_rows_get();
    uint16_t unit_w = (grid_cols > 0U) ? (uint16_t)(parent_w / grid_cols) : parent_w;
    uint16_t unit_h = (grid_rows > 0U) ? (uint16_t)(parent_h / grid_rows) : parent_h;

    *out_x = (int16_t)(col * unit_w);
    *out_y = (int16_t)(row * unit_h);
    *out_w = w_span * unit_w;
    *out_h = h_span * unit_h;

    if (*out_x + *out_w > parent_w)
    {
        *out_w = parent_w - *out_x;
    }
    if (*out_y + *out_h > parent_h)
    {
        *out_h = parent_h - *out_y;
    }
}

void calc_tissue_bars(uint16_t total_w, uint16_t bar_max_h,
                           int16_t out_x[16], uint16_t out_w[16])
{
    /* 组织柱图固定拆成 16 根条柱，每根条柱占等宽区域。 */
    uint16_t col_w = total_w / 16;
    for (uint8_t i = 0; i < 16; i++)
    {
        out_x[i] = (int16_t)(i * col_w);
        out_w[i] = col_w;
    }
    (void)bar_max_h;
}

void render_dynamic_menu(lv_obj_t *parent_card,
                              const menu_item_cfg_t *items,
                              uint8_t item_count,
                              int start_y,
                              lv_obj_t **out_item_handles)
{
    /* 动态菜单按 item_cfg 逐项创建，标题、徽标和边框样式都来自配置表。 */
    if (!parent_card || !items || item_count == 0) return;

    int right_canvas_w = (int)ui_content_w_get();
    int item_w = right_canvas_w - 15;

    int current_y = start_y;
    for (uint8_t i = 0; i < item_count; i++)
    {
        const menu_item_cfg_t *item_cfg = &items[i];
        int item_h = (int)((item_cfg->height_u > 0U) ? item_cfg->height_u : (uint8_t)(ui_menu_item_h_px_get() / BASE_U))
                     * BASE_U;
        int gap_y = (int)ui_menu_gap_px_get();

        lv_obj_t *item = lv_obj_create(parent_card);
        lv_obj_remove_style_all(item);
        lv_obj_set_pos(item, 0, current_y);
        lv_obj_set_size(item, item_w, item_h);
        lv_obj_set_style_bg_color(item, BLACK, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(item, DARK, 0);
        lv_obj_set_style_border_width(item, CARD_DEBUG_BORDERS ? item_cfg->border_width : 0, LV_PART_MAIN);
        lv_obj_set_style_radius(item, 0, 0);
        lv_obj_set_style_pad_all(item, 0, LV_PART_MAIN);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

        if (item_cfg->title_text)
        {
            /* 每个条目都可以带标题文本。 */
            lv_obj_t *title_lbl = lv_label_create(item);
            lv_label_set_text(title_lbl, item_cfg->title_text);
            lv_obj_set_style_text_font(title_lbl, get_font(item_cfg->title_font_id), 0);
            lv_obj_set_style_text_color(title_lbl, GREEN, 0);
            lv_obj_set_size(title_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 12, 0);
            lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_DOT);
        }

        if (item_cfg->value_badge)
        {
            /* 徽标区用于显示当前值或状态摘要。 */
            lv_obj_t *badge_lbl = lv_label_create(item);
            lv_label_set_text(badge_lbl, item_cfg->value_badge);
            lv_obj_set_style_text_font(badge_lbl, get_font(item_cfg->value_font_id), 0);
            lv_obj_set_style_text_color(badge_lbl, LIGHT, 0);
            lv_obj_set_size(badge_lbl, 80, 28);
            lv_obj_align(badge_lbl, LV_ALIGN_RIGHT_MID, -12, 0);
            lv_obj_set_style_text_align(badge_lbl, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_long_mode(badge_lbl, LV_LABEL_LONG_DOT);
        }

        if (out_item_handles)
        {
            out_item_handles[i] = item;
        }

        current_y += item_h + gap_y;
    }
}

void render_card_title(lv_obj_t *parent_card, const char *title_text)
{
    /* 卡片标题统一用同一套位置、尺寸和省略规则，避免各卡片样式漂移。 */
    uint16_t right_w = ui_content_w_get();

    lv_obj_t *lbl = lv_label_create(parent_card);
    lv_obj_remove_style_all(lbl);
    lv_obj_set_pos(lbl, 16, 8);
    lv_obj_set_size(lbl, right_w - 32, 40);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(lbl, title_text);
    lv_obj_set_style_text_font(lbl, get_font(FONT_ID_TITLE), 0);
    lv_obj_set_style_text_color(lbl, LIGHT, 0);

    lv_obj_t *line = lv_obj_create(parent_card);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, right_w - 32, 2);
    lv_obj_set_pos(line, 16, 48);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(line, DARK, 0);
}

void calc_widget_grid(uint16_t parent_w, uint16_t parent_h,
                           uint8_t row, uint8_t col,
                           uint8_t span_w, uint8_t span_h,
                           int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h)
{
    uint8_t grid_cols = ui_custom_grid_cols_get();
    uint8_t grid_rows = ui_custom_grid_rows_get();
    uint16_t cell_w = (grid_cols > 0U) ? (uint16_t)(parent_w / grid_cols) : parent_w;
    uint16_t cell_h = (parent_h > CARD_TITLE_H && grid_rows > 0U)
                      ? ((parent_h - CARD_TITLE_H) / grid_rows)
                      : 60;

    *out_x = (int16_t)(col * cell_w + COMP_GAP);
    *out_y = (int16_t)(CARD_TITLE_H + row * cell_h + COMP_GAP);
    *out_w = (uint16_t)(span_w * cell_w - COMP_GAP * 2);
    *out_h = (uint16_t)(span_h * cell_h - COMP_GAP * 2);

    if (*out_x + *out_w > (int16_t)parent_w)
    {
        *out_w = (uint16_t)((int16_t)parent_w - *out_x);
    }
    if (*out_y + *out_h > (int16_t)parent_h)
    {
        *out_h = (uint16_t)((int16_t)parent_h - *out_y);
    }
}

static void add_left_anchor_sep_line(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w)
{
    lv_obj_t *line;

    if (!parent) return;

    line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, w, 1);
    lv_obj_set_pos(line, x, y);
    lv_obj_set_style_bg_color(line, GREEN, 0);
    lv_obj_set_style_bg_opa(line, 140, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
}

static void add_left_anchor_sep_vline(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t h)
{
    lv_obj_t *line;

    if (!parent) return;

    line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, 1, h);
    lv_obj_set_pos(line, x, y);
    lv_obj_set_style_bg_color(line, GREEN, 0);
    lv_obj_set_style_bg_opa(line, 140, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
}

static void add_left_anchor_sep_boundary(uint16_t boundaries[LEFT_MAX_WIDGETS * 2],
                                         uint8_t *count,
                                         uint16_t y,
                                         uint16_t anchor_h)
{
    if (count == NULL)
    {
        return;
    }

    if (y == 0U || y >= anchor_h)
    {
        return;
    }

    for (uint8_t i = 0U; i < *count; i++)
    {
        if (boundaries[i] == y)
        {
            return;
        }
    }

    if (*count < (LEFT_MAX_WIDGETS * 2U))
    {
        boundaries[*count] = y;
        (*count)++;
    }
}

void refresh_left_aux_slots(void)
{
    ui_vm_left_aux_t vm;

    ui_vm_left_aux_update(&vm);

    if (layout_label_is_valid(&s_left_bat_lbl))
    {
        lv_label_set_text(s_left_bat_lbl, vm.battery_temp_text);
    }

    if (layout_label_is_valid(&s_left_prj_lbl))
    {
        lv_label_set_text(s_left_prj_lbl, vm.project_temp_text);
    }
}

void render_left_anchor_grid(lv_obj_t *left_anchor)
{
    if (!left_anchor) return;

    /* 固定栏按 g_sys_config.left_widgets[] 动态渲染。
     * 0x02 协议下 APP 直接发送当前方向实际坐标：side=2x7，top/bottom=7x2。 */
    g_left_anchor_obj = left_anchor;
    s_left_bat_lbl = NULL;
    s_left_prj_lbl = NULL;

    uint16_t sep_boundaries[LEFT_MAX_WIDGETS * 2];
    uint16_t sep_x_boundaries[LEFT_MAX_WIDGETS * 2];
    uint8_t sep_boundary_count = 0U;
    uint8_t sep_x_boundary_count = 0U;
    const uint16_t anchor_w = ui_anchor_w_get();
    const uint16_t anchor_h = ui_anchor_h_get();
    const uint8_t grid_cols = ui_fixed_grid_cols_get();
    const uint8_t grid_rows = ui_fixed_grid_rows_get();
    const uint16_t cell_w = (grid_cols > 0U) ? (uint16_t)(anchor_w / grid_cols) : LEFT_CELL_W;
    const uint16_t cell_h = (grid_rows > 0U) ? (uint16_t)(anchor_h / grid_rows) : LEFT_CELL_H;
    const bool horizontal_anchor = !ui_layout_is_vertical_split();

    for (uint8_t i = 0; i < ui_left_widget_count_get() && i < LEFT_MAX_WIDGETS; i++)
    {
        const grid_widget_t *cfg = ui_left_widget_get(i);
        if (cfg == NULL)
        {
            continue;
        }
        if (cfg->widget_id == COMP_EMPTY) continue;

        const comp_style_t *style = comp_get_style(cfg->widget_id);
        uint8_t span_w = (style != NULL) ? style->span_w : 1;
        uint8_t span_h = (style != NULL) ? style->span_h : 1;
        uint8_t origin_col = cfg->x;
        uint8_t origin_row = cfg->y;

        if (origin_col >= grid_cols || origin_row >= grid_rows ||
            (uint16_t)origin_col + span_w > grid_cols ||
            (uint16_t)origin_row + span_h > grid_rows)
        {
            printf("[LAYOUT] skip fixed widget id=%u pos=%u,%u span=%u,%u grid=%u,%u\r\n",
                   (unsigned)cfg->widget_id,
                   (unsigned)origin_col,
                   (unsigned)origin_row,
                   (unsigned)span_w,
                   (unsigned)span_h,
                   (unsigned)grid_cols,
                   (unsigned)grid_rows);
            continue;
        }

        int16_t  abs_x = (int16_t)(origin_col * cell_w);
        int16_t  abs_y = (int16_t)(origin_row * cell_h);
        uint16_t abs_w = span_w * cell_w;
        uint16_t abs_h = span_h * cell_h;

        if (horizontal_anchor)
        {
            add_left_anchor_sep_boundary(sep_x_boundaries, &sep_x_boundary_count, (uint16_t)(origin_col * cell_w), anchor_w);
            add_left_anchor_sep_boundary(sep_x_boundaries, &sep_x_boundary_count, (uint16_t)((origin_col + span_w) * cell_w), anchor_w);
        }
        else
        {
            add_left_anchor_sep_boundary(sep_boundaries, &sep_boundary_count, (uint16_t)(origin_row * cell_h), anchor_h);
            add_left_anchor_sep_boundary(sep_boundaries, &sep_boundary_count, (uint16_t)((origin_row + span_h) * cell_h), anchor_h);
        }

        render_widget_by_id(left_anchor, cfg->widget_id,
                            abs_x, abs_y, abs_w, abs_h,
                            span_w, span_h, (font_id_t)255);
    }

    for (uint8_t i = 0U; i < sep_boundary_count; i++)
    {
        add_left_anchor_sep_line(left_anchor, 0, (lv_coord_t)sep_boundaries[i], (lv_coord_t)anchor_w);
    }
    for (uint8_t i = 0U; i < sep_x_boundary_count; i++)
    {
        add_left_anchor_sep_vline(left_anchor, (lv_coord_t)sep_x_boundaries[i], 0, (lv_coord_t)anchor_h);
    }
}

static void render_custom_card_widgets(lv_obj_t *card_custom, uint8_t custom_card_idx)
{
    if (!card_custom ||
            custom_card_idx >= MAX_CUSTOM_CARDS)
    {
        return;
    }

    uint16_t parent_w = lv_obj_get_width(card_custom);
    uint16_t parent_h = lv_obj_get_height(card_custom);
    uint8_t count = ui_custom_card_widget_count_get(custom_card_idx);
    uint16_t fallback_w = ui_content_w_get();
    uint8_t grid_cols = ui_custom_grid_cols_get();
    uint8_t grid_rows = ui_custom_grid_rows_get();

    if (parent_w == 0 || parent_w > ui_safe_zone_w_get())
    {
        parent_w = fallback_w;
    }
    if (parent_h == 0 || parent_h > ui_safe_zone_h_get())
    {
        parent_h = ui_content_h_get();
    }

    if (count > MAX_5F_WIDGETS)
    {
        count = MAX_5F_WIDGETS;
    }

    /* 每次重建自定义卡都先 clean，再重画标题和所有 widget。
     * 这是一种“整卡重绘”策略，简单直接，能避免局部布局变更后残留旧对象。 */
    lv_obj_clean(card_custom);
    render_card_title(card_custom, ui_custom_card_title_get(custom_card_idx));

    if (count == 0U)
    {
        return;
    }

    for (uint8_t i = 0; i < count; i++)
    {
        const grid_widget_t *widget = ui_custom_card_widget_get(custom_card_idx, i);
        if (widget == NULL)
        {
            continue;
        }
        comp_id_t w_id = widget->widget_id;
        uint8_t c = widget->x;
        uint8_t r = widget->y;

        if (w_id == COMP_EMPTY) continue;

        const comp_style_t *style = comp_get_style(w_id);
        uint8_t span_w = (style != NULL) ? style->span_w : 1;
        uint8_t span_h = (style != NULL) ? style->span_h : 1;

        if (c >= grid_cols || r >= grid_rows ||
            (uint16_t)c + span_w > grid_cols ||
            (uint16_t)r + span_h > grid_rows)
        {
            printf("[LAYOUT] skip custom widget id=%u pos=%u,%u span=%u,%u grid=%u,%u\r\n",
                   (unsigned)w_id,
                   (unsigned)c,
                   (unsigned)r,
                   (unsigned)span_w,
                   (unsigned)span_h,
                   (unsigned)grid_cols,
                   (unsigned)grid_rows);
            continue;
        }

        int16_t abs_x, abs_y;
        uint16_t abs_w, abs_h;
        calc_widget_grid(parent_w, parent_h,
                              r, c, span_w, span_h,
                              &abs_x, &abs_y, &abs_w, &abs_h);

        /* 真正的组件创建统一下沉到 comp_view 工厂，
         * layout_view 只负责“算位置、给尺寸、按配置摆放”。 */
        render_widget_by_id(card_custom, w_id, abs_x, abs_y, abs_w, abs_h,
                            span_w, span_h, (font_id_t)255);
    }
}

void render_5f_custom_grid(lv_obj_t *card_custom, lv_obj_t *left_anchor, uint8_t custom_card_idx)
{
    /* 这里除了渲染当前卡片，还顺手把容器句柄登记到全局数组。
     * 后续 comp_update / alarm_view 才能跨页面找到这些组件进行同步刷新或告警联动。 */
    g_left_anchor_obj = left_anchor;
    if (custom_card_idx < MAX_CUSTOM_CARDS)
    {
        g_card_custom_objs[custom_card_idx] = card_custom;
        if (g_card_custom_obj_count < (custom_card_idx + 1))
        {
            g_card_custom_obj_count = custom_card_idx + 1;
        }
    }

    render_custom_card_widgets(card_custom, custom_card_idx);
}

void grid_5f_rebuild_all(void)
{
    /* 自定义卡全部重建通常发生在布局参数、页面顺序或组件配置变化之后。 */
    for (uint8_t i = 0; i < g_card_custom_obj_count && i < MAX_CUSTOM_CARDS; i++)
    {
        if (g_card_custom_objs[i] != NULL)
        {
            render_custom_card_widgets(g_card_custom_objs[i], i);
        }
    }
}

lv_obj_t* render_widget(lv_obj_t *parent,
                             const comp_pos_t *pos,
                             uint16_t cell_w, uint16_t cell_h,
                             uint16_t title_h)
{
    if (!parent || !pos) return NULL;
    if (pos->widget_id == COMP_EMPTY) return NULL;

    const comp_style_t *style = comp_get_style(pos->widget_id);
    if (!style)
    {
        lv_obj_t *comp = lv_obj_create(parent);
        lv_obj_remove_style_all(comp);
        int16_t ax = (int16_t)(pos->x * cell_w);
        int16_t ay = (int16_t)(pos->y * cell_h) + title_h;
        lv_obj_set_pos(comp, ax, ay);
        lv_obj_set_size(comp, cell_w, cell_h);
        return comp;
    }

    int16_t  abs_x = (int16_t)(pos->x * cell_w);
    int16_t  abs_y = (int16_t)(pos->y * cell_h) + title_h;
    uint16_t abs_w = (uint16_t)(style->span_w * cell_w);
    uint16_t abs_h = (uint16_t)(style->span_h * cell_h);

    return render_widget_by_id(parent, pos->widget_id,
                               abs_x, abs_y, abs_w, abs_h,
                               style->span_w, style->span_h,
                               (font_id_t)255);
}
