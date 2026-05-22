#ifndef AREX_LAYOUT_VIEW_H
#define AREX_LAYOUT_VIEW_H

#include "../core/arex_ui_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

bool arex_safe_zone_in_danger(void);

void arex_calc_layout_rect(int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h,
                           int16_t anchor_offset_x, int16_t anchor_offset_y);

void arex_calc_tech_layout(int16_t *out_lx, int16_t *out_ly,
                           uint16_t *out_lw, uint16_t *out_lh,
                           int16_t *out_rx, int16_t *out_ry,
                           uint16_t *out_rw, uint16_t *out_rh);

void arex_calc_classic_layout(int16_t *out_top_x, int16_t *out_top_y,
                              uint16_t *out_top_w, uint16_t *out_top_h,
                              int16_t *out_bot_x, int16_t *out_bot_y,
                              uint16_t *out_bot_w, uint16_t *out_bot_h);

void arex_calc_widget_cell(uint16_t parent_w, uint16_t parent_h,
                           uint8_t row, uint8_t col,
                           uint8_t w_span, uint8_t h_span,
                           int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h);

void arex_calc_tissue_bars(uint16_t total_w, uint16_t bar_max_h,
                           int16_t out_x[16], uint16_t out_w[16]);

void arex_calc_widget_grid(uint16_t parent_w, uint16_t parent_h,
                           uint8_t row, uint8_t col,
                           uint8_t span_w, uint8_t span_h,
                           int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h);

void arex_render_card_title(lv_obj_t *parent_card, const char *title_text);

void arex_render_dynamic_menu(lv_obj_t *parent_card,
                              const arex_menu_item_cfg_t *items,
                              uint8_t item_count,
                              int start_y,
                              lv_obj_t **out_item_handles);

void arex_render_5f_custom_grid(lv_obj_t *card_custom,
                                lv_obj_t *left_anchor_obj,
                                uint8_t custom_card_idx);

void arex_5f_grid_rebuild_all(void);

void arex_render_left_anchor_grid(lv_obj_t *left_anchor);
void arex_refresh_left_aux_slots(void);

lv_obj_t *arex_render_widget(lv_obj_t *parent,
                             const comp_pos_t *pos,
                             uint16_t cell_w, uint16_t cell_h,
                             uint16_t title_h);

#ifdef __cplusplus
}
#endif

#endif /* AREX_LAYOUT_VIEW_H */
