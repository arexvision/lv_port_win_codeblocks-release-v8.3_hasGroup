#include "arex_ui_engine.h"

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
