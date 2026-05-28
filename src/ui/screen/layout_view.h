/*
 * 文件: src/app_ui/ui/screen/layout_view.h
 * 作用: 该文件属于屏幕或页面编排模块，负责整屏布局、分页切换、覆盖层、编辑态显示或页面注册管理。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef LAYOUT_VIEW_H
#define LAYOUT_VIEW_H

#include "../core/ui_defs.h"
#include "../core/ui_types.h"
#include "../comp/comp_style_types.h"
#include "page_registry_types.h"

#ifdef __cplusplus
extern "C" {
#endif

bool safe_zone_in_danger(void);

void calc_layout_rect(int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h,
                           int16_t anchor_offset_x, int16_t anchor_offset_y);

void calc_tech_layout(int16_t *out_lx, int16_t *out_ly,
                           uint16_t *out_lw, uint16_t *out_lh,
                           int16_t *out_rx, int16_t *out_ry,
                           uint16_t *out_rw, uint16_t *out_rh);

void calc_classic_layout(int16_t *out_top_x, int16_t *out_top_y,
                              uint16_t *out_top_w, uint16_t *out_top_h,
                              int16_t *out_bot_x, int16_t *out_bot_y,
                              uint16_t *out_bot_w, uint16_t *out_bot_h);

void calc_widget_cell(uint16_t parent_w, uint16_t parent_h,
                           uint8_t row, uint8_t col,
                           uint8_t w_span, uint8_t h_span,
                           int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h);

void calc_tissue_bars(uint16_t total_w, uint16_t bar_max_h,
                           int16_t out_x[16], uint16_t out_w[16]);

void calc_widget_grid(uint16_t parent_w, uint16_t parent_h,
                           uint8_t row, uint8_t col,
                           uint8_t span_w, uint8_t span_h,
                           int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h);

void render_card_title(lv_obj_t *parent_card, const char *title_text);

void render_dynamic_menu(lv_obj_t *parent_card,
                              const menu_item_cfg_t *items,
                              uint8_t item_count,
                              int start_y,
                              lv_obj_t **out_item_handles);

void render_5f_custom_grid(lv_obj_t *card_custom,
                                lv_obj_t *left_anchor_obj,
                                uint8_t custom_card_idx);

void grid_5f_rebuild_all(void);

void render_left_anchor_grid(lv_obj_t *left_anchor);
void refresh_left_aux_slots(void);

lv_obj_t *render_widget(lv_obj_t *parent,
                             const comp_pos_t *pos,
                             uint16_t cell_w, uint16_t cell_h,
                             uint16_t title_h);

#ifdef __cplusplus
}
#endif

#endif /* LAYOUT_VIEW_H */
