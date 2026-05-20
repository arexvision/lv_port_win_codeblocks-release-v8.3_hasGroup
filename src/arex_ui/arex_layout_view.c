#include "arex_ui_engine.h"

#define WIDGET_GAP  0

void arex_calc_widget_grid(uint16_t parent_w, uint16_t parent_h,
                           uint8_t row, uint8_t col,
                           uint8_t span_w, uint8_t span_h,
                           int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h)
{
    uint16_t cell_w = parent_w / 5;
    uint16_t cell_h = (parent_h > AREX_CARD_TITLE_H)
                      ? ((parent_h - AREX_CARD_TITLE_H) / AREX_WIDGET_ROWS)
                      : 60;

    *out_x = (int16_t)(col * cell_w + WIDGET_GAP);
    *out_y = (int16_t)(AREX_CARD_TITLE_H + row * cell_h + WIDGET_GAP);
    *out_w = (uint16_t)(span_w * cell_w - WIDGET_GAP * 2);
    *out_h = (uint16_t)(span_h * cell_h - WIDGET_GAP * 2);

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
    lv_obj_set_style_bg_color(line, AREX_GREEN, 0);
    lv_obj_set_style_bg_opa(line, 140, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
}

static arex_grid_widget_t *arex_left_find_widget_at_cell(uint8_t col, uint8_t row)
{
    for (uint8_t i = 0; i < g_sys_config.left_widget_count && i < AREX_LEFT_MAX_WIDGETS; i++)
    {
        arex_grid_widget_t *cfg = &g_sys_config.left_widgets[i];
        if (cfg->widget_id == WIDGET_EMPTY)
        {
            continue;
        }

        const arex_widget_style_t *style = arex_get_widget_style(cfg->widget_id);
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

    const uint16_t cell_w = AREX_LEFT_CELL_W;
    const uint16_t cell_h = AREX_LEFT_CELL_H;

    for (uint8_t i = 0; i < g_sys_config.left_widget_count && i < AREX_LEFT_MAX_WIDGETS; i++)
    {
        arex_grid_widget_t *cfg = &g_sys_config.left_widgets[i];
        if (cfg->widget_id == WIDGET_EMPTY) continue;

        const arex_widget_style_t *style = arex_get_widget_style(cfg->widget_id);
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
            custom_card_idx >= AREX_MAX_CUSTOM_CARDS)
    {
        return;
    }

    uint16_t parent_w = lv_obj_get_width(card_custom);
    uint16_t parent_h = lv_obj_get_height(card_custom);
    uint8_t count = g_sys_config.custom_cards[custom_card_idx].widget_count;
    uint16_t fallback_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                          - (g_sys_config.panel_gap_u * AREX_BASE_U);

    if (parent_w == 0 || parent_w > g_sys_config.safe_zone_w)
    {
        parent_w = fallback_w;
    }
    if (parent_h == 0 || parent_h > g_sys_config.safe_zone_h)
    {
        parent_h = g_sys_config.safe_zone_h;
    }

    if (count > AREX_5F_MAX_WIDGETS)
    {
        count = AREX_5F_MAX_WIDGETS;
    }

    lv_obj_clean(card_custom);
    arex_render_card_title(card_custom, "CUSTOM WIDGETS");

    for (uint8_t i = 0; i < count; i++)
    {
        arex_grid_widget_t *widget = &g_sys_config.custom_cards[custom_card_idx].widgets[i];
        arex_widget_id_t w_id = widget->widget_id;
        uint8_t c = widget->x;
        uint8_t r = widget->y;

        if (w_id == WIDGET_EMPTY) continue;
        if (r >= AREX_WIDGET_ROWS || c >= AREX_WIDGET_COLS) continue;

        const arex_widget_style_t *style = arex_get_widget_style(w_id);
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
    if (custom_card_idx < AREX_MAX_CUSTOM_CARDS)
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
    for (uint8_t i = 0; i < g_card_custom_obj_count && i < AREX_MAX_CUSTOM_CARDS; i++)
    {
        if (g_card_custom_objs[i] != NULL)
        {
            render_custom_card_widgets(g_card_custom_objs[i], i);
        }
    }
}

lv_obj_t* arex_render_widget(lv_obj_t *parent,
                             const arex_widget_pos_t *pos,
                             uint16_t cell_w, uint16_t cell_h,
                             uint16_t title_h)
{
    if (!parent || !pos) return NULL;
    if (pos->widget_id == WIDGET_EMPTY) return NULL;

    const arex_widget_style_t *style = arex_get_widget_style(pos->widget_id);
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
