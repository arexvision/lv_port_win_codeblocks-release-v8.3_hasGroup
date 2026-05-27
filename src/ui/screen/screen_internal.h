#ifndef SCREEN_INTERNAL_H
#define SCREEN_INTERNAL_H

#include "screen.h"

#ifdef __cplusplus
extern "C" {
#endif

extern lv_obj_t *s_scr;
extern lv_obj_t *s_safe_zone;
extern lv_obj_t *s_left_anchor;
extern lv_obj_t *s_right_cont;
extern lv_obj_t *s_tileview;
extern lv_obj_t *s_tile_objs[PAGE_COUNT];
extern lv_obj_t *s_wall_top;
extern lv_obj_t *s_wall_bottom;
extern lv_obj_t *s_wall_text_top;
extern lv_obj_t *s_wall_blocks_top;
extern lv_obj_t *s_wall_text_bottom;
extern lv_obj_t *s_wall_blocks_bottom;
extern lv_obj_t *s_dot_cont;
extern lv_obj_t *s_scroll_dots[DASH_PAGE_COUNT];
extern lv_obj_t *s_brightness_overlay;
extern lv_timer_t *s_edit_flash_timer;
extern lv_obj_t *s_edit_flash_badge;
extern lv_obj_t *s_edit_flash_val_lbl;
extern bool s_edit_flash_on;
extern bool s_software_brightness_enabled;
extern uint16_t s_cached_right_w;
extern lv_style_t s_style_screen;
extern lv_style_t s_style_panel;
extern lv_style_t s_style_anchor_bg;
extern lv_style_t s_style_label_huge;
extern lv_style_t s_style_label_med;
extern lv_style_t s_style_label_small;
extern lv_style_t s_style_title_zone;
extern lv_style_t s_style_val_zone;
extern lv_style_t s_style_menu_item;
extern lv_style_t s_style_menu_item_active;
extern lv_style_t s_style_sep_line;

void reset_transient_ui_refs(void);
void restore_brightness_overlay_state(void);
void edit_flash_stop(void);
void wall_create(void);
void render_left_anchor_grid(lv_obj_t *left_anchor);
void render_5f_custom_grid(lv_obj_t *card_custom,
                           lv_obj_t *left_anchor_obj,
                           uint8_t custom_card_idx);
void grid_5f_rebuild_all(void);
uint8_t page_visible_dash_count(void);

#ifdef __cplusplus
}
#endif

#endif /* SCREEN_INTERNAL_H */
