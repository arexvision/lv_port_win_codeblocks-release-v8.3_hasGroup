#include "../core/ui_engine.h"
#include "layout_view.h"
#include "../comp/comp_style.h"
#include "../comp/comp_view.h"

#include <stdio.h>

#define COMP_GAP  0

static lv_obj_t *s_left_bat_lbl = NULL;
static lv_obj_t *s_left_prj_lbl = NULL;

bool arex_safe_zone_in_danger(void)
{
    int16_t max_offset_x = (int16_t)((PHYSICAL_W - g_sys_config.safe_zone_w) / 2);
    int16_t max_offset_y = (int16_t)((PHYSICAL_H - g_sys_config.safe_zone_h) / 2);

    if (g_sys_config.offset_x < -max_offset_x || g_sys_config.offset_x > max_offset_x)
    {
        return true;
    }
    if (g_sys_config.offset_y < -max_offset_y || g_sys_config.offset_y > max_offset_y)
    {
        return true;
    }

    if (g_sys_config.mask_enabled)
    {
        int16_t bottom_edge = (int16_t)(PHYSICAL_H / 2 + g_sys_config.safe_zone_h / 2 + g_sys_config.offset_y);
        if (bottom_edge > PHYSICAL_H - MASK_EDGE_GUARD)
        {
            return true;
        }
    }

    return false;
}

void arex_calc_layout_rect(int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h,
                           int16_t anchor_offset_x, int16_t anchor_offset_y)
{
    int16_t center_x = (int16_t)(PHYSICAL_W / 2) + anchor_offset_x;
    int16_t center_y = (int16_t)(PHYSICAL_H / 2) + anchor_offset_y;

    *out_x = center_x - (int16_t)(g_sys_config.safe_zone_w / 2);
    *out_y = center_y - (int16_t)(g_sys_config.safe_zone_h / 2);
    *out_w = g_sys_config.safe_zone_w;
    *out_h = g_sys_config.safe_zone_h;
}

void arex_calc_tech_layout(int16_t *out_lx, int16_t *out_ly,
                           uint16_t *out_lw, uint16_t *out_lh,
                           int16_t *out_rx, int16_t *out_ry,
                           uint16_t *out_rw, uint16_t *out_rh)
{
    uint16_t gap = g_sys_config.gap_u * BASE_U;

    if (g_sys_config.layout_order == ORDER_NORMAL)
    {
        *out_lx = 0;
        *out_rx = (int16_t)(LEFT_ANCHOR_W + gap);
    }
    else
    {
        *out_lx = (int16_t)(g_sys_config.safe_zone_w - LEFT_ANCHOR_W - gap);
        *out_rx = 0;
    }

    *out_ly = 0;
    *out_ry = 0;
    *out_lw = LEFT_ANCHOR_W;
    *out_lh = g_sys_config.safe_zone_h;
    *out_rw = g_sys_config.safe_zone_w - LEFT_ANCHOR_W - gap;
    *out_rh = g_sys_config.safe_zone_h;
}

void arex_calc_classic_layout(int16_t *out_top_x, int16_t *out_top_y,
                              uint16_t *out_top_w, uint16_t *out_top_h,
                              int16_t *out_bot_x, int16_t *out_bot_y,
                              uint16_t *out_bot_w, uint16_t *out_bot_h)
{
    uint16_t gap = g_sys_config.gap_u * BASE_U;
    uint16_t top_h = 0;

    top_h += g_sys_config.h_depth * BASE_U;
    top_h += g_sys_config.h_ndl * BASE_U + gap;
    top_h += g_sys_config.h_pod * BASE_U + gap;
    top_h += g_sys_config.h_batt * BASE_U + gap;
    top_h += g_sys_config.h_gas * BASE_U + gap;
    top_h += g_sys_config.h_time * BASE_U;

    if (top_h < MIN_CLASSIC_TOP_H)
    {
        top_h = MIN_CLASSIC_TOP_H;
    }

    uint16_t bottom_h = (g_sys_config.safe_zone_h > top_h + gap)
                        ? (g_sys_config.safe_zone_h - top_h - gap)
                        : MIN_CLASSIC_TOP_H;

    if (g_sys_config.layout_order == ORDER_NORMAL)
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

    *out_top_w = g_sys_config.safe_zone_w;
    *out_top_h = top_h;
    *out_bot_w = g_sys_config.safe_zone_w;
    *out_bot_h = bottom_h;
}

void arex_calc_widget_cell(uint16_t parent_w, uint16_t parent_h,
                           uint8_t row, uint8_t col,
                           uint8_t w_span, uint8_t h_span,
                           int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h)
{
    uint16_t unit_w = parent_w / COMP_GRID_COLS;
    uint16_t unit_h = parent_h / COMP_GRID_ROWS;

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

void arex_calc_tissue_bars(uint16_t total_w, uint16_t bar_max_h,
                           int16_t out_x[16], uint16_t out_w[16])
{
    uint16_t col_w = total_w / 16;
    for (uint8_t i = 0; i < 16; i++)
    {
        out_x[i] = (int16_t)(i * col_w);
        out_w[i] = col_w;
    }
    (void)bar_max_h;
}

void arex_render_dynamic_menu(lv_obj_t *parent_card,
                              const arex_menu_item_cfg_t *items,
                              uint8_t item_count,
                              int start_y,
                              lv_obj_t **out_item_handles)
{
    if (!parent_card || !items || item_count == 0) return;

    int right_canvas_w = g_sys_config.safe_zone_w - LEFT_ANCHOR_W
                         - ((int)g_sys_config.gap_u * BASE_U);
    int item_w = right_canvas_w - 15;

    int current_y = start_y;
    for (uint8_t i = 0; i < item_count; i++)
    {
        const arex_menu_item_cfg_t *item_cfg = &items[i];
        int item_h = (int)(item_cfg->height_u > 0 ? item_cfg->height_u : g_sys_config.h_menu_item)
                     * BASE_U;
        int gap_y = (int)g_sys_config.gap_menu * BASE_U;

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
            lv_obj_t *title_lbl = lv_label_create(item);
            lv_label_set_text(title_lbl, item_cfg->title_text);
            lv_obj_set_style_text_font(title_lbl, arex_get_font(item_cfg->title_font_id), 0);
            lv_obj_set_style_text_color(title_lbl, GREEN, 0);
            lv_obj_set_size(title_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 12, 0);
            lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_DOT);
        }

        if (item_cfg->value_badge)
        {
            lv_obj_t *badge_lbl = lv_label_create(item);
            lv_label_set_text(badge_lbl, item_cfg->value_badge);
            lv_obj_set_style_text_font(badge_lbl, arex_get_font(item_cfg->value_font_id), 0);
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

void arex_render_card_title(lv_obj_t *parent_card, const char *title_text)
{
    uint16_t right_w = g_sys_config.safe_zone_w - LEFT_ANCHOR_W
                       - ((int)g_sys_config.gap_u * BASE_U);

    lv_obj_t *lbl = lv_label_create(parent_card);
    lv_obj_remove_style_all(lbl);
    lv_obj_set_pos(lbl, 16, 8);
    lv_obj_set_size(lbl, right_w - 32, 40);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(lbl, title_text);
    lv_obj_set_style_text_font(lbl, arex_get_font(FONT_ID_TITLE), 0);
    lv_obj_set_style_text_color(lbl, LIGHT, 0);

    lv_obj_t *line = lv_obj_create(parent_card);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, right_w - 32, 2);
    lv_obj_set_pos(line, 16, 48);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(line, DARK, 0);
}

void arex_calc_widget_grid(uint16_t parent_w, uint16_t parent_h,
                           uint8_t row, uint8_t col,
                           uint8_t span_w, uint8_t span_h,
                           int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h)
{
    uint16_t cell_w = parent_w / 5;
    uint16_t cell_h = (parent_h > CARD_TITLE_H)
                      ? ((parent_h - CARD_TITLE_H) / COMP_GRID_ROWS)
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

static void arex_add_left_anchor_sep_line(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w)
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

static void arex_format_left_temp_text(char *buf, size_t len, float temp_c, bool valid)
{
    if (!valid)
    {
        snprintf(buf, len, "--");
        return;
    }

    snprintf(buf, len, "%.1fC", (double)temp_c);
}

void arex_refresh_left_aux_slots(void)
{
    char buf[16];

    if (s_left_bat_lbl)
    {
        arex_format_left_temp_text(buf, sizeof(buf),
                                   g_sensor_data.bat_temperature_c,
                                   g_sensor_data.bat_temperature_valid);
        lv_label_set_text(s_left_bat_lbl, buf);
    }

    if (s_left_prj_lbl)
    {
        arex_format_left_temp_text(buf, sizeof(buf),
                                   g_sensor_data.prj_temperature_c,
                                   g_sensor_data.prj_temperature_valid);
        lv_label_set_text(s_left_prj_lbl, buf);
    }
}

static lv_obj_t *arex_create_left_aux_slot(lv_obj_t *parent,
                                           int16_t abs_x,
                                           int16_t abs_y,
                                           const char *title,
                                           bool is_bat)
{
    lv_obj_t *obj = lv_obj_create(parent);
    if (!obj) return NULL;

    lv_obj_set_pos(obj, abs_x, abs_y);
    lv_obj_set_size(obj, AREX_LEFT_CELL_W, AREX_LEFT_CELL_H);
    lv_obj_set_style_bg_color(obj, BLACK, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, DARK, 0);
    lv_obj_set_style_border_width(obj, DEBUG_BORDERS ? 1 : 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 2, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *title_lbl = lv_label_create(obj);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_font(title_lbl, arex_get_font(FONT_ID_SMALL), 0);
    lv_obj_set_style_text_color(title_lbl, LIGHT, 0);
    lv_obj_align(title_lbl, LV_ALIGN_TOP_LEFT, 4, 4);

    lv_obj_t *val_lbl = lv_label_create(obj);
    lv_obj_set_style_text_font(val_lbl, arex_get_font(FONT_ID_MEDIUM), 0);
    lv_obj_set_style_text_color(val_lbl, GREEN, 0);
    lv_obj_align(val_lbl, LV_ALIGN_BOTTOM_RIGHT, -2, -4);

    if (is_bat)
    {
        s_left_bat_lbl = val_lbl;
    }
    else
    {
        s_left_prj_lbl = val_lbl;
    }

    arex_refresh_left_aux_slots();
    return obj;
}

static arex_grid_widget_t *arex_left_find_widget_at_cell(uint8_t col, uint8_t row)
{
    for (uint8_t i = 0; i < g_sys_config.left_widget_count && i < LEFT_MAX_WIDGETS; i++)
    {
        arex_grid_widget_t *cfg = &g_sys_config.left_widgets[i];
        if (cfg->widget_id == COMP_EMPTY)
        {
            continue;
        }

        const comp_style_t *style = comp_get_style(cfg->widget_id);
        uint8_t span_w = (style != NULL) ? style->span_w : 1;
        uint8_t span_h = (style != NULL) ? style->span_h : 1;
        if (col >= cfg->x && col < (uint8_t)(cfg->x + span_w) &&
                row >= cfg->y && row < (uint8_t)(cfg->y + span_h))
        {
            return cfg;
        }
    }

    return NULL;
}

void arex_render_left_anchor_grid(lv_obj_t *left_anchor)
{
    if (!left_anchor) return;

    g_left_anchor_obj = left_anchor;
    s_left_bat_lbl = NULL;
    s_left_prj_lbl = NULL;

    const uint16_t cell_w = AREX_LEFT_CELL_W;
    const uint16_t cell_h = AREX_LEFT_CELL_H;

    for (uint8_t i = 0; i < g_sys_config.left_widget_count && i < LEFT_MAX_WIDGETS; i++)
    {
        arex_grid_widget_t *cfg = &g_sys_config.left_widgets[i];
        if (cfg->widget_id == COMP_EMPTY) continue;

        const comp_style_t *style = comp_get_style(cfg->widget_id);
        uint8_t span_w = (style != NULL) ? style->span_w : 1;
        uint8_t span_h = (style != NULL) ? style->span_h : 1;

        int16_t  abs_x = (int16_t)(cfg->x * cell_w);
        int16_t  abs_y = (int16_t)(cfg->y * cell_h);
        uint16_t abs_w = span_w * cell_w;
        uint16_t abs_h = span_h * cell_h;

        render_widget_by_id(left_anchor, cfg->widget_id,
                            abs_x, abs_y, abs_w, abs_h,
                            span_w, span_h, (arex_font_id_t)255);
    }

    if (arex_left_find_widget_at_cell(0, 5) == NULL)
    {
        (void)arex_create_left_aux_slot(left_anchor, 0, (int16_t)(5 * cell_h), "BAT", true);
    }
    if (arex_left_find_widget_at_cell(1, 5) == NULL)
    {
        (void)arex_create_left_aux_slot(left_anchor, (int16_t)cell_w, (int16_t)(5 * cell_h), "PRJ", false);
    }

    for (uint8_t row = 1; row < AREX_LEFT_ROWS; row++)
    {
        uint8_t seg_start = 0xFF;

        for (uint8_t col = 0; col < AREX_LEFT_COLS; col++)
        {
            arex_grid_widget_t *top_cfg = arex_left_find_widget_at_cell(col, (uint8_t)(row - 1));
            arex_grid_widget_t *bottom_cfg = arex_left_find_widget_at_cell(col, row);
            bool draw_seg = (top_cfg != NULL && bottom_cfg != NULL && top_cfg != bottom_cfg);

            if (draw_seg)
            {
                if (seg_start == 0xFF)
                {
                    seg_start = col;
                }
            }
            else if (seg_start != 0xFF)
            {
                arex_add_left_anchor_sep_line(left_anchor,
                                              (lv_coord_t)(seg_start * cell_w),
                                              (lv_coord_t)(row * cell_h),
                                              (lv_coord_t)((col - seg_start) * cell_w));
                seg_start = 0xFF;
            }
        }

        if (seg_start != 0xFF)
        {
            arex_add_left_anchor_sep_line(left_anchor,
                                          (lv_coord_t)(seg_start * cell_w),
                                          (lv_coord_t)(row * cell_h),
                                          (lv_coord_t)((AREX_LEFT_COLS - seg_start) * cell_w));
        }
    }
}

static void render_custom_card_widgets(lv_obj_t *card_custom, uint8_t custom_card_idx)
{
    if (!card_custom || custom_card_idx >= g_sys_config.custom_card_count ||
            custom_card_idx >= MAX_CUSTOM_CARDS)
    {
        return;
    }

    uint16_t parent_w = lv_obj_get_width(card_custom);
    uint16_t parent_h = lv_obj_get_height(card_custom);
    uint8_t count = g_sys_config.custom_cards[custom_card_idx].widget_count;
    uint16_t fallback_w = g_sys_config.safe_zone_w - LEFT_ANCHOR_W
                          - (g_sys_config.panel_gap_u * BASE_U);

    if (parent_w == 0 || parent_w > g_sys_config.safe_zone_w)
    {
        parent_w = fallback_w;
    }
    if (parent_h == 0 || parent_h > g_sys_config.safe_zone_h)
    {
        parent_h = g_sys_config.safe_zone_h;
    }

    if (count > MAX_5F_WIDGETS)
    {
        count = MAX_5F_WIDGETS;
    }

    lv_obj_clean(card_custom);
    arex_render_card_title(card_custom, "CUSTOM WIDGETS");

    for (uint8_t i = 0; i < count; i++)
    {
        arex_grid_widget_t *widget = &g_sys_config.custom_cards[custom_card_idx].widgets[i];
        comp_id_t w_id = widget->widget_id;
        uint8_t c = widget->x;
        uint8_t r = widget->y;

        if (w_id == COMP_EMPTY) continue;
        if (r >= COMP_GRID_ROWS || c >= COMP_GRID_COLS) continue;

        const comp_style_t *style = comp_get_style(w_id);
        uint8_t span_w = (style != NULL) ? style->span_w : 1;
        uint8_t span_h = (style != NULL) ? style->span_h : 1;

        int16_t abs_x, abs_y;
        uint16_t abs_w, abs_h;
        arex_calc_widget_grid(parent_w, parent_h,
                              r, c, span_w, span_h,
                              &abs_x, &abs_y, &abs_w, &abs_h);

        render_widget_by_id(card_custom, w_id, abs_x, abs_y, abs_w, abs_h,
                            span_w, span_h, (arex_font_id_t)255);
    }
}

void arex_render_5f_custom_grid(lv_obj_t *card_custom, lv_obj_t *left_anchor, uint8_t custom_card_idx)
{
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

void arex_5f_grid_rebuild_all(void)
{
    for (uint8_t i = 0; i < g_card_custom_obj_count && i < MAX_CUSTOM_CARDS; i++)
    {
        if (g_card_custom_objs[i] != NULL)
        {
            render_custom_card_widgets(g_card_custom_objs[i], i);
        }
    }
}

lv_obj_t* arex_render_widget(lv_obj_t *parent,
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
                               (arex_font_id_t)255);
}
